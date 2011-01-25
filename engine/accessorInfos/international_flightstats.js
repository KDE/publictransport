/** Accessor for www.flightstats.com (international).
* © 2011, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'TypeOfVehicle', 'StopID', 'Status' ];
}

function parseTimetable( html ) {
    // Find block of departures
    var pos = html.search( /<table [^>]*class="tableListingTable"[^>]*>/i );
    if ( pos == -1 ) {
		helper.error("Result table not found!", html);
		return;
	}
    var end = html.indexOf( '</table>', pos + 1 );
    var str = html.substr( pos, end - pos );

    // Initialize regular expressions (compile them only once)
    var departuresRegExp = /<tr[^>]*>([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*>([\s\S]*?)<\/td>/ig;
	var flightNumberRegExp = /onclick="populateDynDropmenuFlight\('\d*','([^']+)','(\d+)',/i;
	var linkRegExp = /<a href="([^"]+)"/i
	
	var targetCol = 0, flightNumberCol = 1, airlineCol = 3, departureCol = 4, statusCol = 7;
	
    // Go through all departure blocks
    while ( (departure = departuresRegExp.exec(str)) ) {
		departure = departure[1];

		// Get column contents
		var columns = new Array;
		while ( (column = columnsRegExp.exec(departure)) ) {
			column = column[1];
			columns.push( column );
		}
		if ( columns.length < 8 ) {
			helper.error("Too less columns (" + columns.length + ") found in a departure row!", departure);
			continue; // Too less columns
		}
		
		// Initialize some values
		var delay = -1; // unknown delay

		// Parse time column
		var time = helper.matchTime( columns[departureCol], "hh:mm AP" ); // TODO Test
		if ( time.length != 2 ) {
			helper.error("Unexpected string in time column!", columns[departureCol]);
			continue; // Unexpected string in time column
		}

		// Parse flight number column
		var flightNumber = flightNumberRegExp.exec( columns[flightNumberCol] );
		if ( flightNumber == null ) {
			helper.error("Unexpected string in flight number column!", columns[flightNumberCol]);
			continue; // Unexcepted string in flight number column
		}
// 		var airlineCode = flightNumber[1]; // TODO isn't used currently
		var flightNumber = flightNumber[2];
		
		var journeyNewsLink = linkRegExp.exec( columns[flightNumberCol] );
		if ( journeyNewsLink != null ) {
			journeyNewsLink = journeyNewsLink[0];
		}

		// Parse target column
		var targetString = helper.trim( helper.stripTags(columns[targetCol]) );

		// Parse airline column
		var airline = helper.trim( helper.stripTags(columns[airlineCol]) );
		
		// Parse status column (status + maybe delay info)
		var status = helper.trim( helper.stripTags(columns[statusCol]) );
		pos = status.indexOf( "On-time" );
		if ( pos != -1 ) {
			delay = 0;
			
			// Cut "On-time" from the status string
			status = helper.trim( status.substr(0, pos - 1) );
		}

		// Add departure
		timetableData.clear();
		timetableData.set( 'Target', targetString );
		timetableData.set( 'FlightNumber', flightNumber );
		timetableData.set( 'Operator', airline );
		if ( journeyNewsLink != null ) {
			timetableData.set( 'journeyNewsLink', journeyNewsLink );
		}
		timetableData.set( 'Status', status );
		timetableData.set( 'Delay', delay );
		timetableData.set( 'DepartureHour', time[0] );
		timetableData.set( 'DepartureMinute', time[1] );
		result.addData( timetableData );
    }
}

function parsePossibleStops( html ) {
    // Initialize regular expressions (compile them only once)
    var stopRegExp = /<airport\s+code="([^"]*)"\s+name="([^"]*)"\s+city="([^"]*)"\s+countryCode="([^"]*)"[^>]*\/>/ig;
	
    // Go through all stop options
    while ( (stop = stopRegExp.exec(html)) ) {
		var stopID = stop[1];
		var stopName = stop[2];
		var stopCity = stop[3];
		var stopCountry = stop[4];
		
		// Add stop
		timetableData.clear();
		timetableData.set( 'StopID', stopID );
		timetableData.set( 'StopName', stopName );
		timetableData.set( 'StopCity', stopCity );
		timetableData.set( 'StopCountryCode', stopCountry );
		result.addData( timetableData );
    }

    return result.hasData();
}
