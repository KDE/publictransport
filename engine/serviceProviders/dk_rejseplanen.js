/** Service provider rejseplanen.dk
* © 2010, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'Delay', /*'DelayReason',*/ 'Platform', /*'JourneyNews',*/ 'TypeOfVehicle',
	     'StopID', /*'Pricing',*/ /*'Changes',*/ 'RouteStops', /*'RoutePlatformsDeparture',
	     'RoutePlatformsArrival', 'RouteTimesDeparture', 'RoutePlatformsArrival',
	     'RouteTransportLines'*/ ];
}

function getTimetable( values ) {
    var url = "http://www.rejseplanen.dk/bin/stboard.exe/en?ml=m&";
//     var url = "http://www.rejseplanen.dk/bin/stboard.exe/en?rt=1" +
    var data = "input=" + values.stop + "!" +
	"&boardType=" + (values.dataType == "arrivals" ? "arr" : "dep") +
	"&date=" + helper.formatDateTime(values.dateTime, "dd.MM.yy") +
	"&time=" + helper.formatDateTime(values.dateTime, "hh:mm") +
	"&disableEquivs=no" +
	"&maxJourneys=" + values.maxCount +
	"&start=Show"; //&productsFilter=111111111";
// 	boardType=dep&input=Oslovej+%2F+Ringvejen&inputRef=&dirInput=&dirInputRef=&selectDate=period&dateBegin=04.05.2012&dateEnd=04.05.2012&time=09%3A12&REQProduct_list_1=00000111100&REQProduct_list_2=11111000000&REQProduct_list_3=00000000001&maxJourneys=7&maxStops=0&start.x=732&start.y=438&start=Show
    var request = network.createRequest( url );
    request.finished.connect( parseTimetable );
    request.setPostData( data, "utf-8" );
//     request.setHeader( "Accept", "application/json, text/javascript, */*", "utf-8" );
//     request.setHeader( "Content-Type", "application/json; charset=utf-8", "utf-8" );
//     request.setHeader( "idoskey", sessionKey, "utf-8" );
    network.post( request );
}

function parseTimetable( html ) {
    // Find block of departures
//     var str = helper.extractBlock( html,
// 	    '<table id="hafasSqResults" class="hafasResult fullwidth" cellspacing="0">',
// 	    '<td id="hafasResultsInfoBlock">' );
    var resultTable = helper.findFirstHtmlTag( html, "table",
	{attributes: {"id": "hafasSqResults",
		      "class": "hafasResult"}} );
    if ( !resultTable.found ) {
	helper.error("Could not find result table.", html);
	return;
    }
    var str = resultTable.contents;

    // Initialize regular expressions (only used once)
    var meaningsRegExp = /<tr>[\s\S]*?(<th[\s\S]*?)<\/tr>/i;
    var columnMeaningsRegExp = /<th[^>]*?>([\s\S]*?)<\/th>/ig;

    if ( (meanings = meaningsRegExp.exec(str)) == null ) {
	helper.error("Couldn't read column meanings! Website layout may have changed.", str);
	return;
    }
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
	else if ( colMeaning == "To" || colMeaning == "From" )
	    targetCol = i;
	else if ( colMeaning == "Platform" )
	    platformCol = i;

	++i;
    }
    if ( timeCol == -1 || typeOfVehicleCol == -1 || targetCol == -1 ) {
	helper.error("Couldn't find time, target and type of vehicle columns! Website layout may have changed.", str);
	return;
    }

    // Initialize regular expressions (compile them only once)
    var departuresRegExp = /<tr id="sqToggleDetails\d+"[^>]*?>([\s\S]*?)<\/tr>[\s\S]*?<table class="hafasResult fullwidth hafasSqResultsDetails"[^>]*?>([\s\S]*?)<\/table>/ig;
    var rowsRegExp = /<tr>([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
    var typeOfVehicleRegExp = /^\s*?(\D+)/i;
    var targetRegExp = /<span class="bold">[^<]*?(?:<span class="italic">[^<]*?)?<a[^>]*?>([\s\S]*?)<\/a>/i;
    var timeCompleteRegExp = /(\d{2}:\d{2})/i;
    var delayRegExp = /<span class="prognosis">approx.&nbsp;(\d{2}:\d{2})<\/span>/i;

    // Go through all departure blocks
    var dateTime = new Date();
    while ( (departureRowArr = departuresRegExp.exec(str)) ) {
	departureRow = departureRowArr[1];
	routeBlock = departureRowArr[2];

	// Get column contents
	var columns = new Array;
	while ( (col = columnsRegExp.exec(departureRow)) )
	    columns.push( col[1] );
	columnsRegExp.lastIndex = 0;

        var departure = { RouteStops: new Array, RouteTimes: new Array, RouteExactStops: 0 };

	// Parse time column
        var timeString = helper.trim( helper.trim(helper.stripTags(columns[timeCol])) );
        var time = helper.matchTime( timeString, "hh:mm" );
        if ( time.error ) {
            helper.error("Unexpected string in time column!", columns[timeCol]);
            continue;
        }
        dateTime.setHours( time.hour, time.minute, 0 );
        departure.DepartureDateTime = dateTime;

	// Parse transport line column
	departure.TransportLine = helper.trim( helper.stripTags(columns[typeOfVehicleCol]) );

	// Parse type of vehicle column
	if ( (typeOfVehicle = typeOfVehicleRegExp.exec(departure.TransportLine)) == null ) {
	    helper.error("Unexpected string in type of vehicle column: \"" +
		departure.TransportLine + "\"! Website layout may have changed.",
		departure.TransportLine);
	    continue; // Unexcepted string in type of vehicle column
	}
	departure.TypeOfVehicle = helper.trim( typeOfVehicle[1] );

	// Parse route column ..
	//  .. target
	if ( (targetResult = targetRegExp.exec(columns[targetCol])) == null ) {
	    helper.error("Unexpected string in target column: \"" +
		columns[targetCol] + "\"! Website layout may have changed.",
		columns[targetCol]);
	    continue; // Unexcepted string in target column
	}
	departure.Target = helper.trim( targetResult[1] );

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

	    departure.RouteTimes.push( routeTime );
	    departure.RouteStops.push( helper.stripTags(routeCols[1]) );
	}

	// Parse platform column if any
	if ( platformCol != -1 )
	    departure.Platform = helper.trim( helper.stripTags(columns[platformCol]) );

	// Parse delay column if any
	if ( delayCol != -1 ) {
	    delayArr = delayRegExp.exec( columns[delayCol] );
	    if ( delayArr )
		departure.Delay = helper.duration( helper.formatTime(time[0], time[1]), delayArr[1] );
	}

	// Add departure
	result.addData( departure );
    }

    return true;
}

function getStopSuggestions( values ) {
    var url = "http://www.rejseplanen.dk/bin/ajax-getstop.exe/mn?start=1&iER=yes&getstop=1" +
	"&REQ0JourneyStopsS0A=" + values.maxCount +
	"&S=" + values.stop + "?&js=true";
    var data = network.getSynchronous( url );

    if ( network.lastDownloadAborted ) {
	return false;
    }

    // Initialize regular expressions (compile them only once)
    var stopRegExp = /{"value":"([^"]*?)","id":"[^"]*?@L=([0-9]+)@[^"]*?"[^}]*?"weight":"(\d+)"[^}]*?}/ig;

    // Find stops
    while ( (stop = stopRegExp.exec(data)) ) {
	// Add stop
	result.addData( {StopName: stop[1], StopID: stop[2], StopWeight: stop[3]} );
    }

    return result.hasData();
}
