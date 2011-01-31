
function usedTimetableInformations() {
    return [ 'StopID', 'StopWeight' ];
}

// This function parses a given HTML document for departure/arrival data.
function parseTimetable( html ) {
	// Find block of departures
// 	var str = helper.extractBlock( html, '<table departure_table>', '</table>' );
	var str = html;
	
	// Initialize regular expressions
// 	var departuresRegExp = /<tr>([\s\S]*?)<\/tr>/ig;
	var departuresRegExp = /<td class="nowrap">\s*<span style="[^"]*">\s*(Str|Bus|RE|IC|ICE|RB|)\s*([^<]*)\s*<\/span>\s*<\/td>\s*<td class="nowrap">\s*<span style="[^"]*">\s*([^<]*)\s*(?:<br \/>\s*<img .+ \/>&nbsp;\s*<span class="him">\s*<span class="bold">.*<\/span>.*<\/span>\s*)?<\/span>\s*<\/td>\s*<td>\s*<span style="[^"]*">&nbsp;([0-9]{2}:[0-9]{2})&nbsp;<\/span><\/td>\s*<\/tr>/ig;
	
	// Go through all departure blocks
	while ( (departureRow = departuresRegExp.exec(str)) ) {
		// Parse time column
		time = helper.matchTime( helper.trim(helper.stripTags(departureRow[4])), "hh:mm" );
		if ( time.length != 2 ) {
			helper.error("Unexpected string in time column", columns[timeCol]);
			continue;
		}

		// Parse type of vehicle column
		var typeOfVehicle = helper.trim( helper.stripTags(departureRow[1]) );
		
		// Parse transport line column
		var transportLine = helper.trim( helper.stripTags(departureRow[2]) );
		
		// Parse target column
		var targetString = helper.trim( helper.stripTags(departureRow[3]) );
		
		// Add departure to the result set
		timetableData.clear();
		timetableData.set( 'DepartureHour', time[0] );
		timetableData.set( 'DepartureMinute', time[1] );
		timetableData.set( 'TransportLine', transportLine );
		timetableData.set( 'TypeOfVehicle', typeOfVehicle );
		timetableData.set( 'Target', targetString );
		result.addData( timetableData );
	}
}

// This function parses a given HTML document for stop suggestions.
function parsePossibleStops( html ) {
    // Initialize regular expressions (compile them only once)
    var stopRegExp = /{"value":"([^"]*?)","id":"[^"]*?@L=([0-9]+)@[^"]*?"[^}]*?"weight":"(\d+)"[^}]*?}/ig;

    // Go through all stop options
    while ( (stop = stopRegExp.exec(html)) ) {
		var stopName = stop[1];
		var stopID = stop[2];
		var stopWeight = stop[3];
		
		// Add stop
		timetableData.clear();
		timetableData.set( 'StopName', stopName );
		timetableData.set( 'StopID', stopID );
		timetableData.set( 'StopWeight', stopWeight );
		result.addData( timetableData );
    }

    return result.hasData();
}

