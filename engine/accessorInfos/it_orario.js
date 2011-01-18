/** Accessor for travelplanner.cup2000.it (Regione Emilia-Romagna, Italia)
* © 2010, Friedrich Pülz */

function usedTimetableInformations() {
    return [ /*'Delay', 'DelayReason', 'Platform', 'JourneyNews',*/ 'TypeOfVehicle', 'Operator',
	     'StopID', /*'Pricing', 'Changes',*/ 'RouteStops'/*, 'RoutePlatformsDeparture',
	     'RoutePlatformsArrival', 'RouteTimesDeparture', 'RoutePlatformsArrival',
	     'RouteTransportLines'*/ ];
}

function parseTimetable( html ) {
    var returnValue = new Array;
	// Dates are set from today, not the requested date. They need to be adjusted by X days,
	// where X is the difference in days between today and the requested date.
	returnValue.push( 'dates need adjustment' );
	
    // Find block of departures
    var str = helper.extractBlock( html,
		'<tbody class="ivuVAlignTop">', '</tbody>' );

    // Initialize regular expressions (compile them only once)
    var departuresRegExp = /<tr>([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
    var typeOfVehicleRegExp = /<a[^>]*?><img src="\/JourneyPlanner\/img\/products\/([^_]*?)_/i;
    var operatorRegExp = /<img src="\/JourneyPlanner\/img\/operators\/[^"]*?".*?title="([^"]*?)"[^\/]*?\/>/i;
    var targetRegExp = /<span class="ivuBold">\s*<a[^>]*?>([\s\S]*?)<\/a>\s*<\/span>/ig;
    var paragraphRegExp = /<p[^>]*?>([\s\S]*?)<\/p>/i;
    var routeBlocksRegExp = /\r?\n\s*(-|<span class="ivuBold"[^>]*?>&rarr;<\/span>)\s*\r?\n/gi;
    var routeBlockEndOfExactRouteMarkerRegExp = /<span class="ivuBold"[^>]*?>&rarr;<\/span>/i;
//     var timeCompleteRegExp = /(\d{2}:\d{2})/i;
//     var delayRegExp = /<span class="prognosis">approx.&nbsp;(\d{2}:\d{2})<\/span>/i;

    // Go through all departure blocks
    while ( (departureRowArr = departuresRegExp.exec(str)) ) {
		departureRow = departureRowArr[1];

		// Get column contents
		var columns = new Array;
		while ( (col = columnsRegExp.exec(departureRow)) )
			columns.push( col[1] );
		columnsRegExp.lastIndex = 0;

		if ( columns.length < 5 ) {
			helper.error("Too less columns found (" + columns.length + ") in a departure row", departureRow);
			continue;
		}

		// Initialize result variables with defaults
		var time, typeOfVehicle = "", transportLine, targetString, platformString = "", operator = "",
			delay = -1, delayReason = "", journeyNews = "",
			routeStops = new Array, routeTimes = new Array, exactRouteStops = 0;

		// Parse time column
		time = helper.matchTime( helper.trim(columns[0]), "hh:mm" );
		if ( time.length != 2 ) {
			helper.error("Unexcepted string in time column", columns[0]);
			continue;
		}

		// Parse type of vehicle column
		if ( (typeOfVehicleArr = typeOfVehicleRegExp.exec(columns[1])) != null ) {
			typeOfVehicle = typeOfVehicleArr[1];
		} else {
			helper.error("Unexcepted string in type of vehicle column", columns[1]);
			continue;
		}
			
		// Parse operator column
		if ( (operatorArr = operatorRegExp.exec(columns[2])) != null )
			operator = operatorArr[1];
			
		// Parse transport line column
		transportLine = helper.trim( helper.stripTags(columns[3]) );
		if ( transportLine.substring(0, 4) == "Lin " )
			transportLine = helper.trim( transportLine.substring(4) );

		// Parse route column ..
		//  .. target
		if ( (targetString = targetRegExp.exec(columns[4])) == null ) {
			helper.error("Unexcepted string in target column", columns[4]);
			continue;
		}
		targetString = helper.trim( targetString[1] );

		// .. route
		var routeColumn = columns[4].substring( targetRegExp.lastIndex );
		targetRegExp.lastIndex = 0;
		var routeArr = paragraphRegExp.exec( routeColumn );
		if ( routeArr != null ) {
			var route = helper.trim( routeArr[1] );
			var routeBlocks = route.split( routeBlocksRegExp );

			if ( !routeBlockEndOfExactRouteMarkerRegExp.test(route) ) {
				exactRouteStops = routeBlocks.length;
			} else {
				while ( (splitter = routeBlocksRegExp.exec(route)) ) {
					++exactRouteStops;
					if ( routeBlockEndOfExactRouteMarkerRegExp.test(splitter) )
					break;
				}
			}

			for ( var n = 0; n < routeBlocks.length; ++n ) {
				var lines = helper.splitSkipEmptyParts( routeBlocks[n], "\n" );
				if ( lines.count < 4 )
					continue;

				routeStops.push( lines[1] );
				routeTimes.push( lines[3] );
			}
		}
	//
	// 	// Parse platform column if any
	// 	if ( platformCol != -1 )
	// 	    platformString = helper.trim( helper.stripTags(columns[platformCol]) );

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
		timetableData.set( 'Operator', operator );
	// 	timetableData.set( 'Platform', platformString );
	// 	timetableData.set( 'Delay', delay );
	// 	timetableData.set( 'DelayReason', delayReason );
	// 	timetableData.set( 'JourneyNews', journeyNews );
		timetableData.set( 'RouteStops', routeStops );
		timetableData.set( 'RouteTimes', routeTimes );
		timetableData.set( 'RouteExactStops', exactRouteStops );
		result.addData( timetableData );
    }

    return returnValue;
}

function parsePossibleStops( html ) {
    // Find block of stops
    var pos = html.search( /<select class="error".*?name="input"[^>]*?>/i );
    if ( pos == -1 ) {
		helper.error("Stop suggestion element not found!", html);
		return;
	}
    var end = html.indexOf( '</select>', pos + 1 );
    var str = html.substr( pos, end - pos );

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
