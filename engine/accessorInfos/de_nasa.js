
// This function returns a list of all features supported by this script.
function usedTimetableInformations() {
	return [ 'Platform', 'StopID' ];
}

// This function parses a given HTML document for departure/arrival data.
function parseTimetable( html ) {
// 	// Find result table
// 	var pos = html.search( /<h2 class=\"ivuHeadline\">\s*Ist-Abfahrtzeiten - Übersicht/i );
// 	if ( pos == -1 ) {
// // 		helper.error("Result table not found!", html);
// 		str = html;
// 	} else {
// 		str = html.substr( pos );
// 	}
	var str = html;

	// Initialize regular expressions
	// TODO: This regexp is copied from the old XML, should be divided to parse each column on it's own
	var departuresRegExp = /<tr class=[^>]*>\s*<td class=[^>]*>([0-9]{2}:[0-9]{2})<\/td>\s*<td class=[^>]*>\s*<a href="\/delfi52\/[^"]*"><img src="\/img52\/([^_]*)_pic.\w{3,4}"[^>]*>\s*([^<]*)\s*<\/a>\s*<\/td>\s*<td class=[^>]*>\s*<span class=[^>]*>\s*<a href=[^>]*>\s*([^<]*)\s*<\/a>\s*<\/span>\s*<br \/>\s*<a href=[^>]*>[\s\S]*?<\/a>[\s\S]*?<\/td>\s*(?:<td class=[^>]*>\s*([^<]*)\s*<br \/>[^<]*<\/td>\s*)?[\s\S]*?<\/tr>/ig;
//     var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;

//     var timeCol = 0, typeOfVehicleCol = 1, transportLineCol = 1, targetCol = 2;
	
	// Go through all departure blocks
	while ( (departureRow = departuresRegExp.exec(str)) ) {
		// This gets the current departure row
// 		departureRow = departureRow[1];
		
// 		// Get column contents
// 		var columns = new Array;
// 		while ( (col = columnsRegExp.exec(departureRow)) ) {
// 			columns.push( col[1] );
// 		}
// 		columnsRegExp.lastIndex = 0;
// 		
// 		if ( columns.length < 3 ) {
// 			if ( departureRow.indexOf("<th") == -1 ) {
// 				helper.error("Too less columns in a departure row found (" + columns.length + ") " + departureRow);
// 			}
// 			continue;
// 		}
	    
		// Parse time column
		time = helper.matchTime( helper.trim(helper.stripTags(departureRow[1])), "hh:mm" );
		if ( time.length != 2 ) {
			helper.error("Unexpected string in time column", columns[timeCol]);
			continue;
		}
		
		// Parse type of vehicle column
		var typeOfVehicle = helper.trim( helper.stripTags(departureRow[2]) );
		
		// Parse transport line column
		var transportLine = helper.trim( helper.stripTags(departureRow[3]) );
		
		// Parse target column
		var targetString = helper.trim( helper.stripTags(departureRow[4]) );
		
		// Parse platform column
		var platformString = departureRow.length >= 6 && typeof(departureRow[5]) != 'undefined'
				? helper.trim( helper.stripTags(departureRow[5]) ) : "";
		
		// Add departure to the result set
		timetableData.clear();
		timetableData.set( 'DepartureHour', time[0] );
		timetableData.set( 'DepartureMinute', time[1] );
		timetableData.set( 'TransportLine', transportLine );
		timetableData.set( 'TypeOfVehicle', typeOfVehicle );
		timetableData.set( 'Platform', platformString );
		timetableData.set( 'Target', targetString );
		result.addData( timetableData );
	}
}

function parsePossibleStops( html ) {
	// Find stop range
	var range = /<select class="error" name="input">\s*([\s\S]*)\s*<\/select>/i.exec( html );
	if ( range == null ) {
		helper.error( "Stop range not found!", html );
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
