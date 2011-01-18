/** Accessor for imhd.sk.
* © 2010, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'TypeOfVehicle' ];
}

function parseTimetable( html ) {
    // First parse types of vehicle
    var typesOfVehicle = new Array;
    var pos = html.indexOf( '<table class="tab" border="1" bordercolor="#000000" cellpadding="5" cellspacing="0" align="center"><tr><th>Line</th><th>Type</th><th>Terminus</th>' );
    if ( pos == -1 ) {
		helper.error( "The table containing vehicle types wasn't found!", html );
		return;
    }
    var end = html.indexOf( '</table>', pos + 1 );
    var str = html.substr( pos, end - pos );

    // Initialize regular expressions (compile them only once)
    var typeOfVehicleRowRegExp = /<tr>([\s\S]*?)<\/tr>/ig;
    var columnsRegExp1 = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
    var transportLineRegExp1 = /<center><b>(.*?)<\/b><\/center>/i;
    var typeOfVehicleRegExp = /<center>([^<]*?)<\/center>/i;

    // Go through all type of vehicle blocks
    while ( (typeOfVehicleRow = typeOfVehicleRowRegExp.exec(str)) ) {
		typeOfVehicleRow = typeOfVehicleRow[1];

		// Get column contents
		var columns = new Array;
		while ( (column = columnsRegExp1.exec(typeOfVehicleRow)) ) {
			column = column[1];
			columns.push( column );
		}
		
		if ( columns.length == 0 ) {
			continue; // The header row (only with <th>)
		} else if ( columns.length < 3 ) {
			helper.error( "Found a (vehicle type) row with too less columns (" + columns.length + ")!", typeOfVehicleRow );
			continue; // Too less columns
		}

		// Parse transport line column
		var transportLine = transportLineRegExp1.exec( columns[0] );
		if ( transportLine == null ) {
			helper.error( "Unexcepted string in (vehicle type) transport line column!", columns[0] );
			continue;
		}
		transportLine = helper.trim( helper.stripTags(transportLine[1]) );

		// Parse type of vehicle column
		var typeOfVehicle = typeOfVehicleRegExp.exec( columns[1] );
		if ( typeOfVehicle == null ) {
			helper.error( "Unexcepted string in (vehicle type) type of vehicle column!", columns[1] );
			continue;
		}
		typeOfVehicle = helper.trim( typeOfVehicle[1] );

		typesOfVehicle[ transportLine ] = typeOfVehicle;
    }


    // Find block of departures
    pos = html.indexOf( '<table class="tab" border="1" bordercolor="#000000" cellpadding="5" cellspacing="0" align="center"><tr><th>Time</th><th>Line</th><th>Terminus</th></tr>' );
    if ( pos == -1 ) {
		helper.error( "The table containing departures wasn't found!", html );
		return;
    }
    end = html.indexOf( '</table>', pos + 1 );
    str = html.substr( pos, end - pos );

    // Initialize regular expressions (compile them only once)
    var departuresRegExp = /<tr>([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
    var transportLineRegExp = /<center><b><span[^>]*?>(N?[0-9]+)<\/span><\/b><\/center>/i;

    // Go through all departure blocks
    while ( (departure = departuresRegExp.exec(str)) ) {
		departure = departure[1];

		// Get column contents
		var columns = new Array;
		while ( (column = columnsRegExp.exec(departure)) ) {
			column = column[1];
			columns.push( column );
		}
		if ( columns.length == 0 ) {
			continue; // The header row (only with <th>)
		} else if ( columns.length < 3 ) {
			helper.error( "Found a (departure) row with too less columns (" + columns.length + ")", departure );
			continue; // Too less columns
		}

		// Parse time column
		var time = helper.matchTime( columns[0], "h:mm" );
		if ( time.length != 2 ) {
			helper.error( "Unexpected string in time column!", columns[0] );
			continue;
		}

		// Parse transport line column
		var transportLine = transportLineRegExp.exec( columns[1] );
		if ( transportLine == null ) {
			helper.error( "Unexpected string in transport line column!", columns[1] );
			continue;
		}
		transportLine = helper.trim( transportLine[1] );

		var typeOfVehicle = typesOfVehicle[ transportLine ];
		if ( typeOfVehicle == null )
			typeOfVehicle = "";

		// Parse target column
		var targetString = helper.trim( columns[2] );

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
    
function parsePossibleStops( html ) {
    // Find block of stops
    var stopBlockRegExp = /<select name="k"[^>]*?><option value="[^"]*?">[^<]*?<\/option>([\s\S]*?)<\/select>/i;

    var str = stopBlockRegExp.exec( html );
    if ( str == null ) {
		helper.error("Didn't find stop suggestions element!", html);
		return false; // Unexcepted string
	}
    str = helper.trim( str[1] );

    // Initialize regular expressions (compile them only once)
    var stopRegExp = /<option value="[^"]*?">([^<]*?)<\/option>/ig;

    // Go through all stop options
    while ( (stop = stopRegExp.exec(str)) ) {
		var stopName = stop[1];
		
		// Add stop
		timetableData.clear();
		timetableData.set( 'StopName', stopName );
		result.addData( timetableData );
    }
    
    return result.hasData();
}
