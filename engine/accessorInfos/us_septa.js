/** Accessor for septa.org (Southeastern Pennsylvania, USA)
* © 2010, Friedrich Pülz */

function usedTimetableInformations() {
    return [ /*'Delay', 'DelayReason',*/ 'Platform', /*'JourneyNews',*/ 'TypeOfVehicle',
	     'StopID', /*'Pricing', 'Changes',*/ 'RouteStops'/*, 'RoutePlatformsDeparture',
	     'RoutePlatformsArrival', 'RouteTimesDeparture', 'RoutePlatformsArrival',
	     'RouteTransportLines'*/ ];
}

function parseTimetable( html ) {
    // Find block of departures
    var str = helper.extractBlock( html,
	    '<table cellspacing="0" class="hafasResult" style="width: 100%;">',
	    '</table>' );
	    
    // Initialize regular expressions (only used once)
    var meaningsRegExp = /<tr>[\s\S]*?(<th[\s\S]*?)<\/tr>/i;
    var columnMeaningsRegExp = /<th[^>]*?>([\s\S]*?)<\/th>/ig;

    if ( (meanings = meaningsRegExp.exec(str)) == null )
	return;
    meanings = meanings[1];

    var timeCol = -1, delayCol = -1, typeOfVehicleCol = -1, targetCol = -1, platformCol = -1;
    var i = 0;
    while ( (colMeaning = columnMeaningsRegExp.exec(meanings)) ) {
	colMeaning = helper.trim( helper.stripTags(colMeaning[1]) );

	if ( colMeaning == "Time" )
	    timeCol = i;
	else if ( colMeaning == "Train" )
	    typeOfVehicleCol = i;
	else if ( colMeaning == "Timetable" )
	    targetCol = i;
	else if ( colMeaning == "Platform/Station" )
	    platformCol = i;

	++i;
    }
    if ( timeCol == -1 || typeOfVehicleCol == -1 || targetCol == -1 )
	return;

    // Initialize regular expressions (compile them only once)
    var departuresRegExp = /<tr class="depboard-[^>]*?>([\s\S]*?)<\/tr>/ig;
    var rowsRegExp = /<tr>([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
    var typeOfVehicleRegExp = /^\s*?(\D+)/i;
    var targetRegExp = /<span class="bold">[^<]*?<a[^>]*?>([\s\S]*?)<\/a>/i;
    var routeBlocksRegExp = /\r?\n\s*(-|&#8226;)\s*\r?\n/gi;
    var routeBlockEndOfExactRouteMarkerRegExp = /&#8226;/i;
    var timeCompleteRegExp = /(\d{2}:\d{2})/i;
    var delayRegExp = /<span class="prognosis">approx.&nbsp;(\d{2}:\d{2})<\/span>/i;

    // Go through all departure blocks
    while ( (departureRowArr = departuresRegExp.exec(str)) ) {
	departureRow = departureRowArr[1];

	// Get column contents
	var columns = new Array;
	while ( (col = columnsRegExp.exec(departureRow)) )
	    columns.push( col[1] );
	columnsRegExp.lastIndex = 0;

	// Initialize result variables with defaults
	var time, typeOfVehicle = "", transportLine, targetString, platformString = "",
	    delay = -1, delayReason = "", journeyNews = "",
	    routeStops = new Array, routeTimes = new Array, exactRouteStops = 0;

	// Parse time column
	time = helper.matchTime( helper.trim(helper.stripTags(columns[timeCol])), "hh:mm AP" );
	if ( time.length != 2 )
	    continue;

	// Parse transport line column
	transportLine = helper.trim( helper.stripTags(columns[typeOfVehicleCol]) );
	println( transportLine );

	// Parse type of vehicle column
	if ( (typeOfVehicle = typeOfVehicleRegExp.exec(transportLine)) == null )
	    continue; // Unexcepted string in type of vehicle column
	typeOfVehicle = typeOfVehicle[1];

	// Parse route column ..
	//  .. target
	if ( (targetString = targetRegExp.exec(columns[targetCol])) == null )
	    continue; // Unexcepted string in target column
	targetString = helper.trim( targetString[1] );

	// .. route
	var route = columns[targetCol];
	var posRoute = route.indexOf( "<br />" );
	if ( posRoute != -1 ) {
	    route = route.substr( posRoute + 6 );
	    var routeBlocks = route.split( routeBlocksRegExp );
	    
	    if ( !routeBlockEndOfExactRouteMarkerRegExp.test(route) )
		exactRouteStops = routeBlocks.length;
	    else {
		while ( (splitter = routeBlocksRegExp.exec(route)) ) {
		    ++exactRouteStops;
		    if ( routeBlockEndOfExactRouteMarkerRegExp.test(splitter) )
			break;
		}
	    }
	    
	    for ( var n = 0; n < routeBlocks.length; ++n ) {
		var lines = helper.splitSkipEmptyParts( routeBlocks[n], "\n" );
		if ( lines.count < 2 )
		    continue;
		
		routeTime = helper.matchTime( lines[3], "hh:mm AP" );
		
		routeStops.push( lines[1] );
		routeTimes.push( helper.formatTime(routeTime[0], routeTime[1]) );
	    }
	}

	// Parse platform column if any
	if ( platformCol != -1 )
	    platformString = helper.trim( helper.stripTags(columns[platformCol]) );

	// Parse delay column if any
// 	if ( delayCol != -1 ) {
// 	    delayArr = delayRegExp.exec( columns[delayCol] );
// 	    if ( delayArr )
// 		delay = helper.duration( time[0] + ":" + time[1], delayArr[1] );
// 	}

	// Add departure
	timetableData.clear();
	timetableData.set( 'TransportLine', transportLine );
	timetableData.set( 'TypeOfVehicle', typeOfVehicle );
	timetableData.set( 'Target', targetString );
	timetableData.set( 'DepartureHour', time[0] );
	timetableData.set( 'DepartureMinute', time[1] );
	timetableData.set( 'Platform', platformString );
// 	timetableData.set( 'Delay', delay );
// 	timetableData.set( 'DelayReason', delayReason );
// 	timetableData.set( 'JourneyNews', journeyNews );
	timetableData.set( 'RouteStops', routeStops );
	timetableData.set( 'RouteTimes', routeTimes );
	timetableData.set( 'RouteExactStops', exactRouteStops );
	result.addData( timetableData );
    }

    return true;
}

function parsePossibleStops( html ) {
    // Find block of stops
    var str = helper.extractBlock( html, '<select class="error" name="input">',
				   '</select>' );

    // Initialize regular expressions (compile them only once)
    var stopRegExp = /<option value="[^"]+?#([0-9]+)">([^<]*?)<\/option>/ig;

    // Go through all stop options
    while ( (stop = stopRegExp.exec(str)) ) {
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