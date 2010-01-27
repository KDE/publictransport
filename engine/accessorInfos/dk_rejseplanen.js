/** Accessor for rejseplanen.dk
* © 2010, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'Delay', /*'DelayReason',*/ 'Platform', /*'JourneyNews',*/ 'TypeOfVehicle',
	     'StopID', /*'Pricing',*/ /*'Changes',*/ 'RouteStops', /*'RoutePlatformsDeparture',
	     'RoutePlatformsArrival', 'RouteTimesDeparture', 'RoutePlatformsArrival',
	     'RouteTransportLines'*/ ];
}

function parseTimetable( html ) {
    // Find block of departures
    var str = helper.extractBlock( html,
	    '<table id="hafasSqResults" class="hafasResult fullwidth" cellspacing="0">',
	    '<td id="hafasResultsInfoBlock">' );
	    
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
	else if ( colMeaning == "Prognosis" )
	    delayCol = i;
	else if ( colMeaning == "With" )
	    typeOfVehicleCol = i;
	else if ( colMeaning == "End station" )
	    targetCol = i;
	else if ( colMeaning == "Platform" )
	    platformCol = i;

	++i;
    }
    if ( timeCol == -1 || typeOfVehicleCol == -1 || targetCol == -1 )
	return;
    
    // Initialize regular expressions (compile them only once)
    var departuresRegExp = /<tr id="sqToggleDetails\d+"[^>]*?>([\s\S]*?)<\/tr>[\s\S]*?<table class="hafasResult fullwidth hafasSqResultsDetails"[^>]*?>([\s\S]*?)<\/table>/ig;
    var rowsRegExp = /<tr>([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
    var typeOfVehicleRegExp = /^\s*?(\D+)/i;
    var targetRegExp = /<span class="bold">[^<]*?(?:<span class="italic">[^<]*?)?<a[^>]*?>([\s\S]*?)<\/a>/i;
    var timeCompleteRegExp = /(\d{2}:\d{2})/i;
    var delayRegExp = /<span class="prognosis">approx.&nbsp;(\d{2}:\d{2})<\/span>/i;

    // Go through all departure blocks
    while ( (departureRowArr = departuresRegExp.exec(str)) ) {
	departureRow = departureRowArr[1];
	routeBlock = departureRowArr[2];

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
	time = helper.matchTime( helper.trim(helper.stripTags(columns[timeCol])), "hh:mm" );
	if ( time.length != 2 )
	    continue;
	
	// Parse transport line column
	transportLine = helper.trim( helper.stripTags(columns[typeOfVehicleCol]) );

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
	// Go through all route rows
	while ( (routeRow = rowsRegExp.exec(routeBlock)) ) {
	    routeRow = routeRow[1];
	    
	    var routeCols = new Array;
	    while ( (col = columnsRegExp.exec(routeRow)) )
		routeCols.push( col[1] );
	    columnsRegExp.lastIndex = 0;

	    var routeTime = timeCompleteRegExp.exec( routeCols[0] );
	    if ( routeTime == null )
		continue;
	    else
		routeTime = routeTime[1];
	    
	    routeTimes.push( routeTime );
	    routeStops.push( helper.stripTags(routeCols[1]) );
	}

	// Parse platform column if any
	if ( platformCol != -1 )
	    platformString = helper.trim( helper.stripTags(columns[platformCol]) );

	// Parse delay column if any
	if ( delayCol != -1 ) {
	    delayArr = delayRegExp.exec( columns[delayCol] );
	    if ( delayArr )
		delay = helper.duration( helper.formatTime(time[0], time[1]), delayArr[1] );
	}

	// Add departure
	timetableData.clear();
	timetableData.set( 'TransportLine', transportLine );
	timetableData.set( 'TypeOfVehicle', typeOfVehicle );
	timetableData.set( 'Target', targetString );
	timetableData.set( 'DepartureHour', time[0] );
	timetableData.set( 'DepartureMinute', time[1] );
	timetableData.set( 'Platform', platformString );
	timetableData.set( 'Delay', delay );
// 	timetableData.set( 'DelayReason', delayReason );
// 	timetableData.set( 'JourneyNews', journeyNews );
	timetableData.set( 'RouteStops', routeStops );
	timetableData.set( 'RouteTimes', routeTimes );
// 	timetableData.set( 'RouteExactStops', exactRouteStops );
	result.addData( timetableData );
    }

    return true;
}


function parsePossibleStops( html ) {
    if ( parsePossibleStops_1(html) )
	return true;
//     else if ( parsePossibleStops_2(html) )
// 	return true;
    else
	return false;
}

function parsePossibleStops_1( html ) {
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
