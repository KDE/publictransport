/** Service provider atlas.sk.
* © 2010, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'TypeOfVehicle' ];
}

function parseTimetable( html ) {
    // Find block of departures
    var pos = html.indexOf( '<table class="results" border="0">' );
    if ( pos == -1 ) {
		helper.error( "The table containing departures wasn't found!", html );
		return;
	}
    var end = html.indexOf( '</table>', pos + 1 );
    var str = html.substr( pos, end - pos );

    // Initialize regular expressions (compile them only once)
    var departuresRegExp = /<tr class="[^"]*?">([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
    var typeOfVehicleRegExp = /<img src="[^"]*?" alt="([^"]*?)" title="[^"]*?" \/>/i;
    var targetAndTransportLineRegExp = /<a href="[^"]*?" title="[^\(]*?\([^>]*?>>\s([^\)]*?)\)" style="[^"]*?" onclick="[^"]*?">([^<]*?)<\/a>/i;

    // Go through all departure blocks
    while ( (departure = departuresRegExp.exec(str)) ) {
		departure = departure[1];

		// Get column contents
		var columns = new Array;
		while ( (column = columnsRegExp.exec(departure)) ) {
			column = column[1];
			columns.push( column );
		}
		if ( columns.length < 4 ) {
			if ( departure.indexOf("<th") == -1 ) {
				// There's only an error, if this isn't the header row of the table
				helper.error( "Too less columns (" + columns.length + ") found in result table for a departure!", departure );
			}
			continue; // Too less columns
		}

		// Parse time column
		var time = helper.matchTime( columns[0], "h:mm" );
		if ( time.length != 2 ) {
			helper.error( "Unexpected string in time column!", columns[0] );
			continue; // Unexpected string in time column
		}

		// Parse type of vehicle column
		var typeOfVehicle = typeOfVehicleRegExp.exec( columns[4] );
		if ( typeOfVehicle == null ) {
			helper.error( "Unexpected string in type of vehicle column!", columns[4] );
			continue; // Unexcepted string in type of vehicle column
		}
		typeOfVehicle = typeOfVehicle[1];

		// Parse target & transport line column
		var targetAndTransportLine = targetAndTransportLineRegExp.exec( columns[4] );
		if ( targetAndTransportLine == null ) {
			helper.error( "Unexpected string in target & transport line column!", columns[4] );
			continue; // Unexcepted string in target & transport line column
		}
		var targetString = helper.trim( targetAndTransportLine[1] );
		var transportLine = helper.trim( targetAndTransportLine[2] );

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
    // Initialize regular expressions (compile them only once) TODO
    var stopRegExp = /\{"__type":"CHAPS.IDOS3.TTWS.TimetableObjectInfo",(?:"[^"]*":[^,]*,)*?"oItem":\{([^\}]+)\}[^\}]*\}/ig;
	var stopNameRegExp = /"sName":"([^"]+)"/i;
	var stopIdRegExp = /"iItem":([0-9]+)/i;

    // Go through all stop options
    while ( (oItem = stopRegExp.exec(json)) ) {
		var oItem = oItem[1];

		var stopName = stopNameRegExp.exec(oItem);
		if ( stopName == null ) {
			helper.error("Unexpected string in stop suggestion document, didn't find the stop name!", oItem);
			continue;
		}
		stopName = stopName[1];

		var stopId = stopIdRegExp.exec(oItem);
		if ( stopId == null ) {
			stopId = "";
			helper.error("Unexpected string in stop suggestion document, didn't find the stop ID!", oItem);
		} else {
			stopId = stopId[1];
		}

		// Add stop
		timetableData.clear();
		timetableData.set( 'StopName', stopName );
		if ( stopId.length > 0 ) {
			timetableData.set( 'StopID', stopId );
		}
		result.addData( timetableData );
    }

    return result.hasData();
}

function parseSessionKey( html ) {
	var sessionKey = /var\s+sIDOSKey\s*=\s*'([^']+)';/i.exec( html );
	if ( sessionKey == null ) {
		helper.error("Session key not found!", html);
		return "";
	}

	return sessionKey[1];
}
