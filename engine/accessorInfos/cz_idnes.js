/** Accessor for idnes.cz.
* © 2011, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'TypeOfVehicle', 'StopName' ];
}

function parseTimetable( html ) {
    // Find block of departures
    var pos = html.indexOf( '<table class="results" border="0">' );
    if ( pos == -1 ) {
		helper.error("Result table not found!", html);
		return;
	}
    var end = html.indexOf( '</table>', pos + 1 );
    var str = html.substr( pos, end - pos );
	
    // Initialize regular expressions (compile them only once)
    var departuresRegExp = /<tr class="[^"]*">([\s\S]*?<td[^>]*>[\s\S]*?)<\/tr>/ig; // Finds rows with <td> elements in it (no <th>'s)
    var columnsRegExp = /<td[^>]*>([\s\S]*?)<\/td>/ig;
    var infoRegExp = /<img [^>]*?alt="([^"]*?)"[^>]*\/>\s*<a href="[^"]*?" title="[^\(]*?\([^>]*>>\s([^\)]*?)\)"[^>]*>([^<]*?)<\/a>/i; // Matches type of vehicle [1], target [2], transport line [3]

	var timeCol = 0, infoCol = 4;
	
    // Go through all departure blocks
    while ( (departure = departuresRegExp.exec(str)) ) {
		departure = departure[1];

		// Get column contents
		var columns = new Array;
		while ( (column = columnsRegExp.exec(departure)) ) {
			column = column[1];
			columns.push( column );
		}
		if ( columns.length < 5 ) {
			helper.error("Too less columns found in a row in the result table (" + columns.length + ")!", departure);
			continue;
		}

		// Parse time column
		var time = helper.matchTime( columns[timeCol], "h:mm" );
		if ( time.length != 2 ) {
			helper.error("Unexpected string in time column!", columns[timeCol]);
			continue;
		}

		// Parse info column
		var info = infoRegExp.exec( columns[infoCol] );
		if ( info == null ) {
			helper.error("Unexpected string in info column!", columns[infoCol]);
			continue;
		}
		typeOfVehicle = helper.trim( info[1] );
		targetString = helper.trim( info[2] );
		transportLine = helper.trim( info[3] );

		// Add departure
		timetableData.clear();
		timetableData.set( 'TransportLine', transportLine );
		timetableData.set( 'TypeOfVehicle', typeOfVehicle );
		timetableData.set( 'Target', targetString );
		timetableData.set( 'DepartureHour', time[0] );
		timetableData.set( 'DepartureMinute', time[1] );
		result.addData( timetableData );
    }
}

function parseStopSuggestions( json ) {
    // Initialize regular expressions (compile them only once)
    var stopRegExp = /\{"__type":"CHAPS.IDOS3.TTWS.TimetableObjectInfo",(?:"[^"]*":[^,]*,)*?"oItem":\{([^\}]+)\}[^\}]*\}/ig;
	var stopNameRegExp = /"sName":"([^"]+)"/i;
// 	var stopIdRegExp = /"iItem":([0-9]+)/i;

    // Go through all stop options
    while ( (oItem = stopRegExp.exec(json)) ) {
		var oItem = oItem[1];
		
		var stopName = stopNameRegExp.exec(oItem);
		if ( stopName == null ) {
			helper.error("Unexpected string in stop suggestion document, didn't find the stop name!", oItem);
			continue;
		}
		stopName = stopName[1];
		
// 		var stopId = stopIdRegExp.exec(oItem);
// 		if ( stopId == null ) {
// 			stopId = "";
// 			helper.error("Unexpected string in stop suggestion document, didn't find the stop ID!", oItem);
// 		} else {
// 			stopId = stopId[1];
// 		}
		
		// Add stop
		timetableData.clear();
		timetableData.set( 'StopName', stopName );
// 		if ( stopId.length > 0 ) {
// 			timetableData.set( 'StopID', stopId );
// 		}
		result.addData( timetableData );
    }

    return result.hasData();
}

function parseSessionKey( html ) {
	var sessionKey = /var\s+sIDOSKey\s*=\s*'([^']+)';/i.exec( html );
	if ( sessionKey == null ) {
		helper.error("Session key not found! Maybe the website layout changed.", html);
		return "";
	}
	
	return sessionKey[1];
}
