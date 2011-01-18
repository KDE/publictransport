/** Accessor for www.dvb.de (Dresden, germany).
* © 2010, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'TypeOfVehicle' ];
}

function parseTimetable( html ) {
    // Find block of departures
    var pos = html.indexOf( '<table class="full">' );
    if ( pos == -1 ) {
		helper.error("Result table not found (<table class=\"full\">)!", html);
		return;
	}
    var end = html.indexOf( '</table>', pos + 1 );
    var str = html.substr( pos, end - pos );

    // Initialize regular expressions (compile them only once)
    var departuresRegExp = /<tr class="[^"]*?">([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*>([\s\S]*?)<\/td>/ig;
    var typeOfVehicleRegExp = /<img src="\/images\/design\/pikto_([^\.]*?)\./i;
    var transportLineRegExp = /(\w*\s*\d+)/i;

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
			helper.error("Too less columns (" + columns.length + ") found in a departure row!", departure);
			continue; // Too less columns
		}

		// Parse time column
		var time = helper.matchTime( columns[0], "hh:mm" );
		if ( time.length != 2 ) {
			helper.error("Unexpected string in time column!", columns[0]);
			continue; // Unexpected string in time column
		}

		// Parse type of vehicle column
		var typeOfVehicle = typeOfVehicleRegExp.exec( columns[1] );
		if ( typeOfVehicle == null ) {
			helper.error("Unexpected string in type of vehicle column!", columns[1]);
			continue; // Unexcepted string in type of vehicle column
		}
		typeOfVehicle = typeOfVehicle[1];

		// Parse transport line column
		var transportLine = transportLineRegExp.exec( columns[2] );
		if ( transportLine == null ) {
			helper.error("Unexpected string in transport line column!", columns[2]);
			continue; // Unexcepted string in transport line column
		}
		transportLine = transportLine[1];

		// Parse target column
		var targetString = helper.trim( helper.stripTags(columns[3]) );

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
