/** Accessor oebb.at.
* © 2010, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'TypeOfVehicle', 'JourneyNews', 'Platform', 'Delay', 'StopID', 'StopWeight' ];
}

function parseTimetable( html ) {
    // Find block of departures
	var str = /<table [^>]*class="resultTable"[^>]*>([\s\S]*?)<\/table>/i.exec(html);
	if ( str == null ) {
		helper.error( "The table containing departures wasn't found!", html );
		return;
	}
	str = str[1];
	
	// Initialize regular expressions (only used once)
	var meaningsRegExp = /<tr>[\s\S]*?(<th[\s\S]*?)<\/tr>/i;
	var columnMeaningsRegExp = /<th class="([^"]*)"[^>]*>[^<]*<\/th>/ig;
	if ( (meanings = meaningsRegExp.exec(str)) == null ) {
		helper.error( "Couldn't find result table header!", str );
		return;
	}
	meanings = meanings[1];

	var timeCol = -1, delayCol = -1, typeOfVehicleCol = -1, targetCol = -1, platformCol = -1;
	var i = 0;
	while ( (colMeaning = columnMeaningsRegExp.exec(meanings)) ) {
		colMeaning = helper.trim( colMeaning[1] );

		if ( colMeaning == "time" ) {
			timeCol = i;
		} else if ( colMeaning == "prognosis" ) {
			delayCol = i;
		} else if ( colMeaning == "product" ) {
			typeOfVehicleCol = i;
		} else if ( colMeaning == "timetable" ) {
			targetCol = i;
		} else if ( colMeaning == "platform" ) {
			platformCol = i;
		} else {
			println( "Unknown column found: " + colMeaning );
		}

		++i;
	}

	if ( timeCol == -1 || typeOfVehicleCol == -1 || targetCol == -1 ) {
		helper.error( "Didn't find all required columns in the header!", meanings );
		return;
	}

    // Initialize regular expressions (compile them only once)
    var departuresRegExp = /<tr class="[^"]*?">([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
	// Matches transport line [1] and type of vehicle [2]
    var typeOfVehicleAndTransportLineRegExp = /<img [^>]*src="\/img\/vs_oebb\/([^_]*)_pic[^"]*" [^>]*alt="([^"]*)"[^>]*>/i;
    var targetRegExp = /<strong>[\s\S]*?<a[^>]*>([^<]*)<\/a>[\s\S]*?<\/strong>/i;
	var journeyNewsRegExp = /<div class="journeyMessageHIM"[^>]*>([\s\S]*?)<\/div>/i;
	var delayRegExp = /<span[^>]*?>ca.&nbsp;\+(\d+?)&nbsp;Min.&nbsp;<\/span>/i;
	var delayOnScheduleRegExp = /<span[^>]*?>p&#252;nktlich<\/span>/i;
	
    var routeBlocksRegExp = /\r?\n\s*(-|&#8226;)\s*\r?\n/gi;
    var routeBlockEndOfExactRouteMarkerRegExp = /&#8226;/i;
	
    // Go through all departure blocks
    while ( (departure = departuresRegExp.exec(str)) ) {
		departure = departure[1];

		// Get column contents
		var columns = new Array;
		while ( (column = columnsRegExp.exec(departure)) ) {
			column = column[1];
			columns.push( column );
		}
		
		// Initialize
		var platformString = "", delay = -1, delayReason = "", journeyNews = "";
		var routeStops = new Array, routeTimes = new Array, exactRouteStops = 0;

		// Parse time column
		var time = helper.matchTime( columns[timeCol], "h:mm" );
		if ( time.length != 2 ) {
			helper.error( "Unexpected string in time column!", columns[timeCol] );
			continue; // Unexpected string in time column
		}

		// Parse type of vehicle column
		var typeOfVehicle = typeOfVehicleAndTransportLineRegExp.exec( columns[typeOfVehicleCol] );
		if ( typeOfVehicle == null ) {
			helper.error( "Unexpected string in type of vehicle column!", columns[typeOfVehicleCol] );
			continue; // Unexcepted string in type of vehicle column
		}
		var transportLine = helper.trim( typeOfVehicle[2] );
		typeOfVehicle = typeOfVehicle[1];
		
		var vehicle = typeOfVehicle.toLowerCase();
		if ( vehicle == "rj" ) { // RailJet
			typeOfVehicle = "highspeed train";
		} else if ( vehicle == "oec" || vehicle == "oic" ) {
			typeOfVehicle = "intercity";
		} else if ( vehicle == "rex"    // local train stopping at few stations, semi fast
				 || vehicle == "ez" ) { // "ErlebnisZug", local train stopping at few stations, semi fast
			typeOfVehicle = "regional express";
		}

		// Parse target column
		var target = targetRegExp.exec( columns[targetCol] );
		if ( target == null ) {
			helper.error( "Unexpected string in target column!", columns[targetCol] );
			continue; // Unexcepted string in target column
		}
		var targetString = helper.trim( target[1] );
		
		// Parse route from target column
		var route = columns[targetCol];
		var posRoute = route.indexOf( "<br />" );
		if ( posRoute != -1 ) {
			route = route.substr( posRoute + 6 );
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
				if ( lines.count < 4 ) {
					helper.error( "Too less lines (" + lines.count + ") in route string!", routeBlocks[n] );
					continue;
				}
				
				// lines[0] contains <a href="...">, lines[2] contains </a>
				routeStops.push( lines[1] );
				routeTimes.push( lines[3] );
			}
		}
		
		// Parse journey news from target column
		var journeyNewsMatches = journeyNewsRegExp.exec( columns[targetCol] );
		if ( journeyNewsMatches != null ) {
			journeyNews = helper.stripTags( journeyNewsMatches[1] );
		}
		
		// Parse platform from platform column
		if ( platformCol != -1 ) {
			platformString = helper.trim( columns[platformCol] );
		}

		// Parse delay information from delay column
		if ( delayCol != -1 ) {
			var delayDeparture = delayRegExp.exec( columns[delayCol] );
			if ( delayDeparture != null ) {
				delay = parseInt( delayDeparture[1] );
			} else if ( delayOnScheduleRegExp.test(columns[delayCol]) ) {
				delay = 0;
			} else {
				delay = -1;
			}
		}

		// Add departure
		timetableData.clear();
		timetableData.set( 'TransportLine', transportLine );
		timetableData.set( 'TypeOfVehicle', typeOfVehicle );
		timetableData.set( 'Target', targetString );
		timetableData.set( 'Platform', platformString );
		timetableData.set( 'Delay', delay );
		timetableData.set( 'DelayReason', delayReason );
		timetableData.set( 'JourneyNews', journeyNews );
		timetableData.set( 'DepartureHour', time[0] );
		timetableData.set( 'DepartureMinute', time[1] );
		timetableData.set( 'RouteStops', routeStops );
		timetableData.set( 'RouteTimes', routeTimes );
		timetableData.set( 'RouteExactStops', exactRouteStops );
		result.addData( timetableData );
    }
}

// <regExpRange><![CDATA[(?:SLs.sls=\{"suggestions":\s*\[)(.*)(?:\]\};\s*SLs.showSuggestion\(\);)]]></regExpRange>
// <regExp><![CDATA[(?:\{"value":")([^"]*)(?:","id":"[^"]*@L=)([^@]*)(?:@[^"]*",.*"weight":")([0-9]+)(?:"\})]]></regExp>
// <infos><info>StopName</info> <info>StopID</info> <info>StopWeight</info></infos>
function parsePossibleStops( json ) {
    // Initialize regular expressions (compile them only once) TODO
    var stopRegExp = /\{"value":"([^"]*)","id":"[^"]*@L=([^@]*)@[^"]*",.*"weight":"([0-9]+)"\}/ig;
// 	var stopNameRegExp = /"sName":"([^"]+)"/i;
// 	var stopIdRegExp = /"iItem":([0-9]+)/i;

    // Go through all stop options
    while ( (item = stopRegExp.exec(json)) ) {
		var stopName = item[1];
		var stopId = item[2];
		var stopWeight = item[3];
// 		var stopName = stopNameRegExp.exec(oItem);
// 		if ( stopName == null ) {
// 			helper.error("Unexpected string in stop suggestion document, didn't find the stop name!", oItem);
// 			continue;
// 		}
// 		stopName = stopName[1];
// 		
// 		var stopId = stopIdRegExp.exec(oItem);
// 		if ( stopId == null ) {
// 			stopId = "";
// 			helper.error("Unexpected string in stop suggestion document, didn't find the stop ID!", oItem);
// 		} else {
// 			stopId = stopId[1];
// 		}
		
		// Add stop
		timetableData.clear();
		timetableData.set( 'StopName', stopName );
		if ( stopId.length > 0 ) {
			timetableData.set( 'StopID', stopId );
		}
		timetableData.set( 'StopWeight', stopWeight );
		result.addData( timetableData );
    }

    return result.hasData();
}
