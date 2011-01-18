/** Accessor for hafas.bene-system.com (SNCB, B-Rail).
* © 2010, Friedrich Pülz */

function usedTimetableInformations() {
    return [ /*'Delay', 'DelayReason',*/ 'Platform', /*'JourneyNews',*/ 'TypeOfVehicle',
	     'StopID', /*'Pricing',*/ /*'Changes',*/ 'RouteStops', /*'RoutePlatformsDeparture',
	     'RoutePlatformsArrival', 'RouteTimesDeparture', 'RoutePlatformsArrival',
	     'RouteTransportLines'*/ ];
}

function parseTimetable( html ) {
    // Initialize regular expressions (compile them only once)
    var departuresRegExp = /<tr valign="top" bgcolor="#[^"]*?">([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
    var typeOfVehicleRegExp = /<img src="\/hafas-res\/img\/bs_([^_]*?)_pic/i;
    var targetRegExp = /<span class="bold">[^<]*?<a[^>]*?>([\s\S]*?)<\/a>/i;
    var routeBlocksRegExp = /\r?\n\s*(-|<img[^>]*?>)\s*\r?\n/gi;
    var routeBlockEndOfExactRouteMarkerRegExp = /<img[^>]*?>/i;
//     var delayRegExp = /<span style="[^"]*?">ca.&nbsp;(\d*)&nbsp;Minuten&nbsp;sp&#228;ter<\/span>/i;
//     var delayWithReasonRegExp = /<span><span style="[^"]*?">ca.&nbsp;(\d*)&nbsp;Minuten&nbsp;sp&#228;ter<\/span><\/span>,<br\/><span class="[^"]*?">Grund:\s*?([^<]*)<\/span>/i;
//     var delayOnTimeRegExp = /<span style="[^"]*?">p&#252;nktlich<\/span>/i;
//     var trainCanceledRegExp = /<span class="[^"]*?">Zug f&#228;llt aus<\/span>/i;

    // Go through all departure blocks
    while ( (departureRow = departuresRegExp.exec(html)) ) {
		departureRow = departureRow[1];

		// Get column contents
		var columns = new Array;
		while ( (col = columnsRegExp.exec(departureRow)) )
			columns.push( col[1] );

		if ( columns.length == 0 ) {
			helper.error("No column found in a departure row!", departureRow);
			break; // Not all columns where not found
		} else if ( columns.length < 7 ) {
			helper.error("Didn't find all columns (" + columns.length + " found)!", departureRow);
			break; // Not all columns where not found
		}

		// Initialize result variables with defaults
		var time, typeOfVehicle, transportLine, targetString, platformString = "",
			delay = -1, delayReason = "", journeyNews = "",
			routeStops = new Array, routeTimes = new Array, exactRouteStops = 0;

		// Parse time column
		time = helper.matchTime( helper.trim(helper.stripTags(columns[0])), "hh:mm" );
		if ( time.length != 2 ) {
			helper.error("Unexpected string in time column!", columns[0]);
			continue;
		}
		
		// Parse type of vehicle column
		if ( (typeOfVehicle = typeOfVehicleRegExp.exec(columns[2])) == null ) {
			helper.error("Unexpected string in type of vehicle column!", columns[2]);
			continue; // Unexcepted string in type of vehicle column
		}
		typeOfVehicle = typeOfVehicle[1];

		// Parse transport line column
		transportLine = helper.trim( helper.stripTags(columns[4]) );

		// Parse route column ..
		//  .. target
		if ( (targetString = targetRegExp.exec(columns[6])) == null ) {
			helper.error("Unexpected string in target column!", columns[6]);
			continue; // Unexcepted string in target column
		}
		targetString = helper.trim( targetString[1] );

		// .. route
		var route = columns[6];
		var posRoute = route.indexOf( "<br>" );
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
				if ( lines.count < 4 ) {
					continue;
				}

				routeStops.push( lines[1] );
				routeTimes.push( lines[3] );
			}
		}

		// Parse platform column if any
		if ( columns.length >= 9 ) {
			platformString = helper.trim( helper.stripTags(columns[8]) );
		}
	/*
		// Parse delay column if any
		if ( columnMeanings.contains("ris") ) {
			if ( delayOnTimeRegExp.test(columns["ris"]) ) {
			delay = 0;
			} else if ( (delayArr = delayWithReasonRegExp.exec(columns["ris"])) ) {
			delay = delayArr[1];
			delayReason = delayArr[2];
			} else if ( (delayArr = delayRegExp.exec(columns["ris"])) ) {
			delay = delayArr[1];
			} else if ( trainCanceledRegExp.test(columns["ris"]) ) {
			var infoText = "Zug f&auml;llt aus.";
			if ( journeyNews == "" )
				journeyNews = infoText;
			else
				journeyNews += " " + infoText;
			}
		}*/

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
		timetableData.set( 'JourneyNews', journeyNews );
		timetableData.set( 'RouteStops', routeStops );
		timetableData.set( 'RouteTimes', routeTimes );
		timetableData.set( 'RouteExactStops', exactRouteStops );
		result.addData( timetableData );
    }

    return true;
}

function parsePossibleStops( html ) {
    if ( parsePossibleStops(html) )
	return true;
    else
	return false;
}

function parsePossibleStops( html ) {
    // Find block of stops
    var str = helper.extractBlock( html, '<select name="input">', '</select>' );

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