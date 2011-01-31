
// This function returns a list of all features supported by this script.
function usedTimetableInformations() {
	return [];
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
	var departuresRegExp = /<tr><td class="[^"]*" \/><td>\s*([0-9]{2}:[0-9]{2})\s*<\/td>(?:<td \/>)?<td class="[^"]*" style="[^"]*"><div style="[^"]*"><img src="[^"]*" alt="[^"]*" title="([^"]*)" border=[^>]*><\/div><div style="[^"]*">\s*([^<]*)\s*<\/div><\/td><td>\s*([^<]*)\s*<\/td><td>([^<]*)<\/td><td>(?:<span class="hinweis"><p[^>]*><a[^>]*>)?([^<]*)?(?:<\/a><\/p><\/span>)?<\/td><\/tr>/ig;
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
		var platformString = helper.trim( helper.stripTags(departureRow[5]) );
		
		// Parse journey news column
		var journeyNews = helper.trim( helper.stripTags(departureRow[6]) );
		
		// Add departure to the result set
		timetableData.clear();
		timetableData.set( 'DepartureHour', time[0] );
		timetableData.set( 'DepartureMinute', time[1] );
		timetableData.set( 'TransportLine', transportLine );
		timetableData.set( 'TypeOfVehicle', typeOfVehicle );
		timetableData.set( 'Platform', platformString );
		timetableData.set( 'Target', targetString );
		timetableData.set( 'JourneyNewsOther', journeyNews );
		result.addData( timetableData );
	}
}
