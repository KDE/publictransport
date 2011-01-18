/** Accessor for gares-en-mouvement.com (france).
* © 2010, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'TypeOfVehicle', 'Operator', 'Platform', 'Delay', 'StopID' ];
}

function parseTimetable( html ) {
    // Find block of departures
    var pos = html.search( /<div id="tvs">/i );
    if ( pos == -1 ) {
		helper.error("Result table not found!", html);
		return;
	}
    var end = html.indexOf( '</table>', pos + 1 );
    var str = html.substr( pos, end - pos );

    var tbody = helper.extractBlock( str, "<tbody>", "</tbody>" );
    
    // Initialize regular expressions (compile them only once)
    var departuresRegExp = /<tr class="[^"]*?">([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
    var typeOfVehicleRegExp = /<img src="[^"]*"\s+alt="([^"]+)"\s*\/>/i;
    var timeRegExp = /(\d{2})<abbr title="Time">h<\/abbr>(\d{2})/i;
    var delayRegExp = /Delay\s*:\s*(\d+)/i;
    
    // Go through all departure blocks
    while ( (departure = departuresRegExp.exec(str)) ) {
		departure = departure[1];
		
		// Get column contents
		var columns = new Array;
		while ( (column = columnsRegExp.exec(departure)) ) {
			column = column[1];
			columns.push( column );
		}
		if ( columns.length < 6 ) {
			helper.error("Too less columns in a departure row found (" + columns.length + ")!", departure);
			continue; // Too less columns
		}
			
		// Parse time column
		var timeValues = timeRegExp.exec( columns[2] );
		if ( timeValues == null || timeValues.length != 3 ) {
			helper.error("Unexpected string in time column!", columns[2]);
			continue; // Unexpected string in time column
		}
		var hour = timeValues[1];
		var minute = timeValues[2];
		
// <img src="images/tvs/30bleu_clair.gif" alt="TGV"/>
		
		// Parse vehicle type column
		var vehicleTypeValues = typeOfVehicleRegExp.exec( columns[0] );
		if ( vehicleTypeValues == null ) { //|| vehicleTypeValues.length < 3 ) {
			helper.error("Unexpected string in type of vehicle column!", columns[0]);
			continue; // Unexpected string in vehicle type column
		}
		var operator = vehicleTypeValues[1];
		var typeOfVehicle = "ICE"; //vehicleTypeValues[2]; TODO
		if ( operator == null )
			operator = "";

		// Parse delay column
		var delay = -1;
		if ( (delayArr = delayRegExp.exec(columns[4])) ) {
			delay = parseInt( delayArr[1] );
			if ( delay == NaN )
				delay = -1; // error while parsing
		}
		
		var transportLine = helper.trim( helper.stripTags(columns[1]) );
		var targetString = helper.camelCase( helper.trim(helper.stripTags(columns[3])) );
		var platformString = helper.trim( helper.stripTags(columns[5]) );
		
		// Add departure
		timetableData.clear();
		timetableData.set( 'TransportLine', transportLine );
		timetableData.set( 'TypeOfVehicle', typeOfVehicle );
		timetableData.set( 'Target', targetString );
		timetableData.set( 'DepartureHour', hour );
		timetableData.set( 'DepartureMinute', minute );
		timetableData.set( 'Delay', delay );
		timetableData.set( 'Operator', operator );
		timetableData.set( 'Platform', platformString );
		result.addData( timetableData );
    }
}

function parsePossibleStops( html ) {
    // Find block of stops
    var pos = html.search( /<div id="liste_gare"[^>]*>/i );
    if ( pos == -1 ) {
		helper.error("Stop list element not found!", html);
		return;
	}
    var str = html.substr( pos );
	
    // Initialize regular expressions (compile them only once)
    var stopRegExp = /<li>\s*<a href="gare.php\?gare=([^"]+)">([^<]+)<\/a>\s*<\/li>/ig;
    
    // Go through all stop options
    while ( (stop = stopRegExp.exec(str)) ) {
		var stopID = stop[1];
		var stopName = stop[2];
		
		// Add stop
		timetableData.clear();
		timetableData.set( 'StopID', stopID );
		timetableData.set( 'StopName', stopName );
		result.addData( timetableData );
    }
    
    return result.hasData();
}
