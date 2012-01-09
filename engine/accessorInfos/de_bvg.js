
// This function returns a list of all features supported by this script.
function usedTimetableInformations() {
	return [];
}

// This function parses a given HTML document for departure/arrival data.
function parseTimetable( html ) {
	// Find result table
	var pos = html.search( /<h2 class=\"ivuHeadline\">\s*Ist-Abfahrtzeiten - Übersicht/i );
	if ( pos == -1 ) {
// 		helper.error("Result table not found!", html);
		str = html;
	} else {
		str = html.substr( pos );
	}

	var dateRegExp = /<td class="ivuTableLabel">\s*Datum:\s*<\/td>\s*<td>\s*([\s\S]*?)\s*<\/td>/i;
	var date = dateRegExp.exec( html );
	if ( date == null ) {
		var now = new Date();
		date = [ now.getDate(), now.getMonth() + 1, now.getFullYear() ];
	} else {
		date = helper.matchDate( helper.stripTags(date[1]), "dd.MM.yyyy" );
	}

	// Initialize regular expressions
	var departuresRegExp = /<tr[^>]*?>([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
	var typeOfVehicleRegExp = /<img src="[^"]*" class="ivuTDProductPicture" alt="[^"]*"\s*class="ivuTDProductPicture" \/>(\w{1,10})\s*((?:\w*\s*)?[0-9]+)/i; // Matches type of vehicle [0] and transport line [1]
	
    var timeCol = 0, typeOfVehicleCol = 1, transportLineCol = 1, targetCol = 2;
	
	// Go through all departure blocks
	while ( (departureRow = departuresRegExp.exec(str)) ) {
		// This gets the current departure row
		departureRow = departureRow[1];
		
		// Get column contents
		var columns = new Array;
		while ( (col = columnsRegExp.exec(departureRow)) ) {
			columns.push( col[1] );
		}
		columnsRegExp.lastIndex = 0;
		
		if ( columns.length < 3 ) {
			if ( departureRow.indexOf("<th") == -1 && departureRow.indexOf("ivuTableFootRow") == -1 ) {
				helper.error("Too less columns in a departure row found (" + columns.length + ") " + departureRow);
			}
			continue;
		}
		
		// Parse time column
		time = helper.matchTime( helper.trim(helper.stripTags(columns[timeCol])), "hh:mm" );
		if ( time.length != 2 ) {
			helper.error("Unexpected string in time column", columns[timeCol]);
			continue;
		}
	
		// Parse type of vehicle column
		var typeOfVehicle = typeOfVehicleRegExp.exec(columns[typeOfVehicleCol]);
		if ( typeOfVehicle != null ) {
			transportLine = typeOfVehicle[2];
			typeOfVehicle = typeOfVehicle[1];
		} else {
			helper.error("Unexpected string in type of vehicle column", columns[typeOfVehicleCol]);
			typeOfVehicle = "Unknown";
			transportLine = "Unknown";
		}
		
		// Parse target column
		var targetString = helper.trim( helper.stripTags(columns[targetCol]) );
		
		// Add departure to the result set
		timetableData.clear();
		timetableData.set( 'DepartureHour', time[0] );
		timetableData.set( 'DepartureMinute', time[1] );
		timetableData.set( 'DepartureDate', date );
		timetableData.set( 'TransportLine', transportLine );
		timetableData.set( 'TypeOfVehicle', typeOfVehicle );
		timetableData.set( 'Target', targetString );
		result.addData( timetableData );
	}
}

// This function parses a given HTML document for stop suggestions.
function parseStopSuggestions( html ) {
    // Find block of stops
    var stopRangeRegExp = /<select [^>]*class="ivuSelectBox"[^>]*>\s*([\s\S]*?)\s*<\/select>/i;
	var range = stopRangeRegExp.exec( html );
	if ( range == null ) {
		helper.error( "Stop range not found", html );
		return false;
	}
	range = range[1];

    // Initialize regular expressions (compile them only once)
    var stopRegExp = /<option value="[^"]*">\s*([^<]*)\s*<\/option>/ig;

    // Go through all stop options
    while ( (stop = stopRegExp.exec(range)) ) {
		var stopName = stop[1];

		// Add stop
		timetableData.clear();
		timetableData.set( 'StopName', stopName );
		result.addData( timetableData );
    }

    return result.hasData();
}
