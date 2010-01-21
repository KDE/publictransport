/** Accessor atlas.sk.
* © 2010, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'TypeOfVehicle' ];
}

function parseTimetable( html ) {
    // Find block of departures
    var pos = html.indexOf( '<table class="results" border="0">' );
    if ( pos == -1 )
	return;
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
	if ( columns.length < 4 )
	    continue; // Too less columns

	// Parse time column
	var time = helper.matchTime( columns[0], "h:mm" );
	if ( time.length != 2 )
	    continue; // Unexpected string in time column

	// Parse type of vehicle column
	var typeOfVehicle = typeOfVehicleRegExp.exec( columns[4] );
	if ( typeOfVehicle == null )
	    continue; // Unexcepted string in type of vehicle column
	typeOfVehicle = typeOfVehicle[1];

	// Parse target & transport line column
	var targetAndTransportLine = targetAndTransportLineRegExp.exec( columns[4] );
	if ( targetAndTransportLine == null )
	    continue; // Unexcepted string in target & transport line column
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
    
function parsePossibleStops( html ) {
    // Find block of stops
    var pos = html.search( /<select name="ctl[^"]*?" id="ctl[^"]*?"[^>]*?>/i );
    if ( pos == -1 )
	return;
    var end = html.indexOf( '</select>', pos + 1 );
    var str = html.substr( pos, end - pos );

    // Initialize regular expressions (compile them only once)
    var stopRegExp = /<option (?:selected="selected" )?value="[^"]*?">([^<]*?)<\/option>/ig;

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
