
// This function returns a list of all features supported by this script.
function usedTimetableInformations() {
	return [ 'StopID' ];
}

// This function parses a given HTML document for departure/arrival data.
function parseTimetable( html ) {
	// Find block of departures
	if ( html.search(/<td class=\"errormessage\">Your input is ambiguous. Please choose from the selection list.<\/td>/i) != -1 ) {
		helper.error("No data, only stop suggestions in the HTML document");
		return false;
	} else if ( html.search(/<td [^>]*class="errormessage"[^>]*>[^<]*No trains in this space of time[^<]*<\/td>/) != -1 )
		helper.error("No data for the given space of time in the HTML document");
		return true; {
	}
	
	// Find result table
	var resultTableRegExp = /<th[^>]*>Time<\/th>[\s\S]*?<\/tr>([\s\S]*)<\/table>/i;
	var resultTable = resultTableRegExp.exec( html );
	if ( resultTable == null ) {
		helper.error("Result table not found!", html);
		return false;
	}
	var str = resultTable[1];
	
	// Initialize regular expressions
	var departuresRegExp = /<tr[^>]*?>([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
	var targetRegExp = /<span class="[^"]*">\s*<a href="[^"]*">\s*([^<]*)\s*<\/a>\s*<\/span>/i;
	var journeyNewsRegExp = /<span class="rsx">\s*<br>\s*([^<]*)\s*<\/span>/i;
	var typeOfVehicleRegExp = /<a[^>]*>[\S\s]*<img src="\/img\/([^_]*)_pic.[^"]{3,4}"[^>]*>[\S\s]*<\/a>/i;
	
    var timeCol = 0, typeOfVehicleCol = 2, transportLineCol = 4, targetCol = 6, journeyNewsCol = 6;
	
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
		
		if ( columns.length < 7 ) {
			// Happens very often
// 			helper.error("Too less columns in a departure row found (" + columns.length + ") " + departureRow);
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
			typeOfVehicle = typeOfVehicle[1];
			
			var vehicle = typeOfVehicle.toLowerCase();
			// TODO: Check if these are the right train type:
			if ( vehicle == "dpn" || vehicle == "t84" || vehicle == "r84" ) {
				typeOfVehicle = "regional";
			}
		} else {
			helper.error("Unexpected string in type of vehicle column", columns[typeOfVehicleCol]);
			typeOfVehicle = "Unknown";
		}
		
		// Parse target column
		var targetString = targetRegExp.exec(columns[targetCol]);
		if ( targetString != null ) {
			targetString = targetString[1];
		} else {
			helper.error("Unexpected string in target column", columns[targetCol]);
			continue;
		}
		
		var journeyNewsOther = journeyNewsRegExp.exec(columns[targetCol]);
		if ( journeyNewsOther != null ) {
			journeyNewsOther = journeyNewsOther[1];
		}
		
		var transportLine = helper.trim( helper.stripTags(columns[transportLineCol]) );
		
		// Add departure to the result set
		// TODO: Fill in parsed values instead of the sample strings.
		// You can also add other information, use the code completion
		// (Ctrl+Space) for more information.
		timetableData.clear();
		timetableData.set( 'DepartureHour', time[0] );
		timetableData.set( 'DepartureMinute', time[1] );
		timetableData.set( 'TransportLine', transportLine );
		timetableData.set( 'TypeOfVehicle', typeOfVehicle );
		if ( journeyNewsOther != null ) {
			timetableData.set( 'JourneyNewsOther', journeyNewsOther );
		}
		timetableData.set( 'Target', targetString );
		result.addData( timetableData );
	}
}

// This function parses a given HTML document for stop suggestions.
function parsePossibleStops( html ) {
    // Find block of stops
    var stopRangeRegExp = /<td class="result"[^>]*>\s*<img[^>]*>\s*<select name="input"[^>]*>([\s\S]*?)<\/select>/i;
	var range = stopRangeRegExp.exec( html );
	if ( range == null ) {
		helper.error( "Stop range not found", html );
		return false;
	}
	range = range[1];

    // Initialize regular expressions (compile them only once)
    var stopRegExp = /<option value="[^"]+#([0-9]+)">([^<]*)<\/option>/ig;

    // Go through all stop options
    while ( (stop = stopRegExp.exec(range)) ) {
		var stopID = stop[1];
		var stopName = stop[2];

		// Add stop
		timetableData.clear();
		timetableData.set( 'StopName', stopName );
		timetableData.set( 'StopID', stopID );
		result.addData( timetableData );
    }

    return result.hasData();
}

