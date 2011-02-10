/** Accessor for www.sbb.ch (swiss).
 * © 2010, Friedrich Pülz */

Array.prototype.contains = function( element ) {
	for ( var i = 0; i < this.length; i++ ) {
		if ( this[i] == element ) {
			return true;
		}
	}
	return false;
}

function detailsResultsObject() {
	this.journeyNews = '';
	this.routeStops = new Array;
	this.routePlatformsDeparture = new Array;
	this.routePlatformsArrival = new Array;
	this.routeVehicleTypes = new Array;
	this.routeTransportLines = new Array;
	this.routeTimesDeparture = new Array;
	this.routeTimesArrival = new Array;
	this.routeTimesDepartureDelay = new Array;
	this.routeTimesArrivalDelay = new Array;
}

function usedTimetableInformations() {
	return [ 'Delay', 'Platform', 'JourneyNews', 'TypeOfVehicle',
			'StopID', 'Changes', 'RouteStops',
			'RoutePlatformsDeparture', 'RoutePlatformsArrival',
			'RouteTimesDeparture', 'RouteTimesArrival',
			'RouteTransportLines', 'StopID'/*,
			'TypesOfVehicleInJourney', 'RouteTransportLines'*/
	];
}

function getUrlForLaterJourneyResults( html ) {
	var urlRegExp = /<a href="([^"]*?)"[^>]*?>Sp&#228;tere Verbindungen[^<]*?<\/a>/i;
	var urlLater = urlRegExp.exec( html );
	if ( urlLater != null ) {
		return urlLater[1];
	} else {
		return '';
	}
}

function getUrlForDetailedJourneyResults( html ) {
	var urlRegExp = /<form name="tp_results_form" action="([^#]*?)#focus"[^>]*?>/i;
	var urlLater = urlRegExp.exec( html );
	if ( urlLater != null ) {
		return urlLater[1] + "&guiVCtrl_connection_detailsOut_add_selection&guiVCtrl_connection_detailsOut_select_C0-0&guiVCtrl_connection_detailsOut_select_C0-1&guiVCtrl_connection_detailsOut_select_C0-2&guiVCtrl_connection_detailsOut_select_C0-3&guiVCtrl_connection_detailsOut_select_C0-4&guiVCtrl_connection_detailsOut_select_C1-0&guiVCtrl_connection_detailsOut_select_C1-1&guiVCtrl_connection_detailsOut_select_C1-2&guiVCtrl_connection_detailsOut_select_C1-3&guiVCtrl_connection_detailsOut_select_C1-4&guiVCtrl_connection_detailsOut_select_C2-0&guiVCtrl_connection_detailsOut_select_C2-1&guiVCtrl_connection_detailsOut_select_C2-2&guiVCtrl_connection_detailsOut_select_C2-3&guiVCtrl_connection_detailsOut_select_C2-4";
	} else {
		return '';
	}
}

function parseTimetable( html ) {
    var returnValue = new Array;
	// Dates are set from today, not the requested date. They need to be adjusted by X days,
	// where X is the difference in days between today and the requested date.
	returnValue.push( 'dates need adjustment' );
	
	// Find block of departures
	var str = helper.extractBlock( html,
			'<table cellspacing="0" class="hafas-content hafas-sq-content">', '</table>' );

	// Initialize regular expressions (only used once)
	var meaningsRegExp = /<tr>[\s\S]*?(<th[\s\S]*?)<\/tr>/i;
	var columnMeaningsRegExp = /<th[^>]*?>([\s\S]*?)<\/th>/ig;

	if ( (meanings = meaningsRegExp.exec(str)) == null ) {
		helper.error( "Couldn't find result table header!", str );
		return;
	}
	meanings = meanings[1];

	var timeCol = -1, delayCol = -1, typeOfVehicleCol = -1, targetCol = -1, platformCol = -1;
	var i = 0;
	while ( (colMeaning = columnMeaningsRegExp.exec(meanings)) ) {
		colMeaning = helper.trim( colMeaning[1] );

		if ( colMeaning == "Uhrzeit" ) {
			timeCol = i;
		} else if ( colMeaning == "Prognose" ) {
			delayCol = i;
		} else if ( colMeaning == "Fahrt" ) {
			typeOfVehicleCol = i;
			++i; // colspan="2" for this column
		} else if ( colMeaning == "In Richtung" ) {
			targetCol = i;
		} else if ( colMeaning == "Gleis/Haltestelle" ) {
			platformCol = i;
		}
		// 	    else if ( colMeaning == "Belegung" )

		++i;
	}

	if ( timeCol == -1 || typeOfVehicleCol == -1 || targetCol == -1 ) {
		helper.error( "Didn't find all required columns in the header!", meanings );
		return;
	}

	// Initialize regular expressions (compile them only once)
	var departuresRegExp = /<tr class="zebra-row-\d">([\s\S]*?)<\/tr>/ig;
	var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
	var typeOfVehicleRegExp = /<img src="[^"]*?\/products\/([^_]*?)\d?_pic\.\w{3,4}"/i;
	var transportLineRegExp = /<a href="http:\/\/fahrplan.sbb.ch\/bin\/traininfo.exe[^>]*?>([^<]*?)<\/a>/i;
	var targetRegExp = /<a href="http:\/\/fahrplan.sbb.ch[^>]*?>([^<]*?)<\/a>\s*<\/span>[\s\S]*?<br>/i;
	var platformRegExp = /<strong>([^<]*?)<\/strong>/i;
	var delayRegExp = /ca.&nbsp;\+(\d)&nbsp;Min\./i;

	var routeBlocksRegExp = /\r?\n\s*(-|&#149;)\s*\r?\n/ig;
	var routeBlockEndOfExactRouteMarkerRegExp = /&#149;/;
	var routeStopRegExp = /<a href="[^"]*?">([^<]*?)<\/a>&nbsp;(\d{2}:\d{2})/i;

	// Go through all departure blocks
	while ( (departure = departuresRegExp.exec(str)) ) {
		departure = departure[1];

		// Get column contents
		var columns = new Array;
		while ( (col = columnsRegExp.exec(departure)) ) {
			columns.push( col[1] );
		}

		if ( columns.length < 3 ) {
			helper.error("Too less columns (" + columns.length + ") found in a departure row!", departure);
			continue; // Not all columns where not found
		}

		// Parse time column
		var time = helper.matchTime( columns[timeCol], "hh:mm" );
		if ( time.length != 2 ) {
			helper.error("Unexpected string in time column!", columns[timeCol]);
			continue;
		}

		// Parse type of vehicle column
		var typeOfVehicle = typeOfVehicleRegExp.exec( columns[typeOfVehicleCol] );
		if ( typeOfVehicle == null ) {
			helper.error("Unexpected string in type of vehicle column!", columns[typeOfVehicleCol]);
			continue;
		}
		typeOfVehicle = helper.trim( typeOfVehicle[1] );
		
		var vehicle = typeOfVehicle.toLowerCase();
		if ( vehicle == "nbu" ) {
			typeOfVehicle = "bus";
		} else if ( vehicle == "s1" ) {
			typeOfVehicle = "s";
		} else if ( vehicle == "ntr" || vehicle == "tra" ) {
			typeOfVehicle = "tram";
		}

		// Parse transport line column
		var transportLine = transportLineRegExp.exec( columns[typeOfVehicleCol + 1] );
		if ( transportLine == null ) {
			helper.error("Unexpected string in transport line column!", columns[typeOfVehicleCol]);
			continue;
		}
		transportLine = helper.trim( transportLine[1] );

		// Parse target column
		var targetString = targetRegExp.exec( columns[targetCol] );
		if ( targetString == null ) {
			helper.error("Unexpected string in target column!", columns[targetCol]);
			continue;
		}
		targetString = helper.trim( targetString[1] );

		// Parse platform column
		var platformString = '';
		if ( platformCol != -1 ) {
			platformString = helper.trim( columns[platformCol] );
		}

		// Parse delay column
		var delay = -1;
		if ( delayCol != -1 ) {
			var delayResult = delayRegExp.exec( columns[delayCol] );
			if ( delayResult != null )
				delay = parseInt( delayResult[1] );
		}

		// Parse route from target column
		var routeStops = new Array;
		var routeTimes = new Array;
		var exactRouteStops = 0;
		var route = columns[targetCol].substr( targetRegExp.lastIndex );
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
			var routeStopArr = routeStopRegExp.exec( routeBlocks[n] );
			if ( routeStopArr == null ) {
				helper.error("Unexpected string in route block " + n + "!", routeBlocks[n]);
				continue; // Unexcepted string in route block
			}

				routeStops.push( helper.trim(routeStopArr[1]) );
			routeTimes.push( helper.trim(routeStopArr[2]) );
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
		timetableData.set( 'RouteStops', routeStops );
		timetableData.set( 'RouteTimes', routeTimes );
		timetableData.set( 'RouteExactStops', exactRouteStops );
		result.addData( timetableData );
	}
    return returnValue;
}

function parseJourneyDetails( details, resultObject ) {
	// 	    var exactRouteStops = 0;
	// Initialize regular expressions (only used once)
	var meaningsRegExp = /<tr>[\s\S]*?(<th[\s\S]*?)<\/tr>/ig;
	var columnMeaningsRegExp = /<th [\s\S]*?id="([^"]*?)-[^\"]*"[^>]*?>[\s\S]*?<\/th>/ig;

	if ( (meanings = meaningsRegExp.exec(details)) == null ) {
		helper.error("Header row of details table not found !", details);
		return;
	}
	meanings = meanings[1];
	details = details.substr( meaningsRegExp.lastIndex );

	var posNews = details.indexOf( '<table cellspacing="0" class="hafas-content him-content">' );
	var journeyNews = details.substr( posNews );
	if ( journeyNews != "" ) {
		resultObject.journeyNews = helper.trim( helper.stripTags(journeyNews) );
	}

	var timeCol = -1, dateCol = -1, stopsCol = -1, typeOfVehicleCol = -1,
			platformCol = -1, remarksCol = -1;
	var i = 1;
	while ( (colMeaning = columnMeaningsRegExp.exec(meanings)) ) {
		colMeaning = helper.trim( colMeaning[1] );
		if ( colMeaning.indexOf("time") != -1 ) {
			timeCol = i;
			++i; // colspan="2" for this column
		} else if ( colMeaning.indexOf("date") != -1 ) {
			dateCol = i;
		} else if ( colMeaning.indexOf("stops") != -1 ) {
			stopsCol = i;
			++i; // colspan="2" for this column
		} else if ( colMeaning.indexOf("platform") != -1 ) {
			platformCol = i;
		} else if ( colMeaning.indexOf("remarks") != -1 ) {
			remarksCol = i;
		} else if ( colMeaning.indexOf("products") != -1 ) {
			typeOfVehicleCol = i;
		} /*else if ( colMeaning == "capacity" ) {
			capacityCol = i;
		}*/
		++i;
	}

	if ( timeCol == -1 || typeOfVehicleCol == -1 || stopsCol == -1 ) {
		helper.error("Required columns not found (time, stop name, type of vehicle)!", meanings);
		return;
	}

	var lastStop = null, lastArrivalTime = '';
	var detailsBlockRegExp = /<tr>([\s\S]*?)<\/tr>\s*?<tr>([\s\S]*?)<\/tr>/ig;
	var columnsContentsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
	var timeCompleteRegExp = /(\d{2}:\d{2})/i;

	var cellIsVerticallySpanned = true; // The first "details" cell in vertically spanned
	while ( (detailsBlock = detailsBlockRegExp.exec(details)) ) {
		detailsBlockRow1 = detailsBlock[1];
		detailsBlockRow2 = detailsBlock[2];

		var columnsRow1 = new Array;
		while ( (col = columnsContentsRegExp.exec(detailsBlockRow1)) ) {
			columnsRow1.push( col[1] );
		}
		if ( columnsRow1.length == 0 ) {
			continue;
		}

		var columnsRow2 = new Array;
		if ( cellIsVerticallySpanned ) {
			columnsRow2.push( columnsRow1[0] ); // Add vertically spanned "details" cell
			cellIsVerticallySpanned = false; // Following "details" cells aren't vertically spanned
		}
		while ( (col = columnsContentsRegExp.exec(detailsBlockRow2)) ) {
			columnsRow2.push( col[1] );
		}

		if ( columnsRow1.count < 3 || columnsRow2.count < 1 ) {
			break;
		}

		var routeStop = helper.trim( helper.stripTags(columnsRow1[stopsCol + 1]) );

		// Search for the vehicle type in the transport line column
		// (including html entities, ie. '&#0245;' or '&uuml;')
		var routeVehicleType = /(([abcdefghijklmnopqrstuvwxyz]|&#\d{2,5};|&\w{2,5};)+)/i.exec(
			helper.trim(helper.stripTags(columnsRow1[typeOfVehicleCol])) );
		if ( routeVehicleType != null ) {
			routeVehicleType = routeVehicleType[1];
			routeVehicleType = helper.trim( routeVehicleType );
		}

		var routePlatformDeparture = helper.trim( helper.stripTags(columnsRow1[platformCol]) );
		var routePlatformArrival = helper.trim( helper.stripTags(columnsRow2[platformCol]) );
		var routeTransportLine = helper.trim( helper.stripTags(columnsRow1[typeOfVehicleCol]) );

		// If there's no departure time given, use the last arrival
		// as departure (needed eg. for vehicle type Feet).
		var routeTimeDeparture = timeCompleteRegExp.exec( columnsRow1[timeCol + 1] );
		if ( routeTimeDeparture != null ) {
			routeTimeDeparture = routeTimeDeparture[1];
		} else if ( lastArrivalTime != '' ) {
			routeTimeDeparture = lastArrivalTime;
		} else {
			// TODO: subtract time in remarksCol...
			routeTimeDeparture = '';
		}

		/*
		 *		var delayDeparture = delayRegExp.exec( columnsRow1[3] );
		 *		if ( delayDeparture != null )
		 *			delayDeparture = delayDeparture[1];
		 *		else if ( delayOnScheduleRegExp.test(columnsRow1[3]) )
		 *			delayDeparture = 0;
		 *		else
		 *			delayDeparture = -1;
		 *
		 *		var delayArrival = delayRegExp.exec( columnsRow2[3] );
		 *		if ( delayArrival != null )
		 *			delayArrival = delayArrival[1];
		 *		else if ( delayOnScheduleRegExp.test(columnsRow2[3]) )
		 *			delayArrival = 0;
		 *		else
		 *			delayArrival = -1;*/

		// If there's no arrival time given try to find the duration
		// in the notes column and add it to the departure time
		var routeTimeArrival = timeCompleteRegExp.exec( columnsRow2[timeCol + 1] );
		if ( routeTimeArrival != null ) {
			routeTimeArrival = routeTimeArrival[1];
		} else {
			var timeArrivalParts = helper.matchTime( columnsRow2[timeCol + 1], "hh:mm" );
			var timeDepartureParts = helper.matchTime( routeTimeDeparture, "hh:mm" );

			if ( timeArrivalParts.length < 2 && timeDepartureParts.length >= 2 ) {
				var durationPart = /(\d*?) Min./i.exec( columnsRow1[remarksCol] );
				if ( durationPart != null && parseInt(durationPart[1]) > 0 ) {
					routeTimeArrival = helper.addMinsToTime( routeTimeDeparture,
															 parseInt(durationPart[1]), "hh:mm" );
				}
			}
		}

		resultObject.routeStops.push( routeStop );
		resultObject.routePlatformsDeparture.push( routePlatformDeparture );
		resultObject.routePlatformsArrival.push( routePlatformArrival );
		resultObject.routeVehicleTypes.push( routeVehicleType );
		resultObject.routeTransportLines.push( routeTransportLine );
		resultObject.routeTimesDeparture.push( routeTimeDeparture );
		// 	    resultObject.routeTimesDepartureDelay.push( delayDeparture );
		resultObject.routeTimesArrival.push( routeTimeArrival );
		// 	    resultObject.routeTimesArrivalDelay.push( delayArrival );

		lastStop = helper.trim( helper.stripTags(columnsRow2[stopsCol + 1]) );
		lastArrivalTime = lastArrivalTime == null ? '' : lastArrivalTime;
	}

	if ( lastStop != null ) {
		resultObject.routeStops.push( helper.trim(lastStop) );
	}

	return true;
}

function parseJourneys( html ) {

	// Find block of journeys
	var str = helper.extractBlock( html, // TODO
								   '<table cellspacing="0" class="hafas-content hafas-tp-result-overview">', 'here Verbindungen</a>' );

	var pos = html.indexOf( '<table summary="Details - Verbindung 1' );
	var detailsHtml = pos != -1 ? html.substr( pos ) : '';

// 	println("PARSE JOURNEYS, total: " + html.length + ", length: " + str.length + ", details length: " + detailsHtml.length);

	// Initialize regular expressions (only used once)
	var meaningsRegExp = /<tr>[\s\S]*?(<th[\s\S]*?)<\/tr>/i;
	var columnMeaningsRegExp = /<th id="([^"]*)?"([^>]*?)>[\s\S]*?<\/th>/ig;
	var colspanRegExp = /colspan="(\d+)"/i;

	if ( (meanings = meaningsRegExp.exec(str)) == null ) {
		helper.error("Didn't find a header row in the result table!", str);
		return;
	}
	meanings = meanings[1];

	var timeCol = -1, dateCol = -1, delayCol = -1, typesOfVehicleCol = -1, locationCol = -1,
			changesCol = -1, durationCol = -1, detailsCol = -1, capacityCol = -1;
	var i = 0;

	while ( (colMeaningArr = columnMeaningsRegExp.exec(meanings)) ) {
		colMeaning = helper.trim( colMeaningArr[1] );
		var colspan = colspanRegExp.exec( colMeaningArr[2] );
		if ( colspan != null ) {
			colspan = parseInt( colspan[1] );
			if ( colspan <= 0 ) {
				colspan = 1;
			}
		} else {
			colspan = 1;
		}

		if ( colMeaning == "time" ) {
			timeCol = i;
		} else if ( colMeaning == "date" ) {
			dateCol = i;
		} else if ( colMeaning == "details" ) {
			detailsCol = i;
			// 	    } else if ( colMeaning == "" ) {
				// 		delayCol = i;
		} else if ( colMeaning == "products" ) {
			typesOfVehicleCol = i;
		} else if ( colMeaning == "location" ) {
			locationCol = i;
		} else if ( colMeaning == "changes" ) {
			changesCol = i;
		} else if ( colMeaning == "duration" ) {
			durationCol = i;
		} else if ( colMeaning == "capacity" ) {
			capacityCol = i;
		}

		i += colspan;
	}

	if ( timeCol == -1 || typesOfVehicleCol == -1 || durationCol == -1 ) {
		helper.error("Required columns not found (time, duration, types of vehicle)", meanings);
		return;
	}

	// Initialize regular expressions (compile them only once)
	//     var journeysRegExp = /<tr class="zebra-row-\d">([\s\S]*?)<\/tr>[\s\S]*?<tr class="zebra-row-\d">([\s\S]*?)<\/tr>/ig
	var journeysRegExp = /<tr class="[^\"]*" id="trOverviewDep([^\"]*)">([\s\S]*?)<\/tr>[\s\S]*?<tr class="[^\"]*" id="trOverviewArr[^\"]*">([\s\S]*?)<\/tr>/ig
	var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
	var dateRegExp = /([0-9]{2}\.[0-9]{2}\.[0-9]{2})/i;
	var durationRegExp = /(\d{1,2}:\d{2})/i;
	// 	var journeyDetailsRegExp = /<div id="[^"]*?" class="detailContainer[^"]*?">\s*?<table class="result">([\s\S]*?)<\/table>/ig;

	// Go through all journey blocks
	while ( (journey = journeysRegExp.exec(str)) ) {
		journeyID = journey[1]; // Can be used to get the Details element (id="tr{journeyID}")
		journeyRow1 = journey[2];
		journeyRow2 = journey[3];
		println("  >> parse journey \"" + journeyID + "\"...");

		// Get column contents
		var columnsRow1 = new Array;
		while ( (col = columnsRegExp.exec(journeyRow1)) ) {
			columnsRow1.push( col[1] );
		}
		columnsRegExp.lastIndex = 0;

		var columnsRow2 = new Array;
		while ( (col = columnsRegExp.exec(journeyRow2)) ) {
			columnsRow2.push( col[1] );
		}

		// Parse location column
		var startStopName = helper.trim( helper.stripTags(columnsRow1[locationCol]) );
		var targetStopName = helper.trim( helper.stripTags(columnsRow2[locationCol - 1]) );

		// Parse departure date column
		var departureDate = dateRegExp.exec( columnsRow1[dateCol] );
		if ( departureDate == null ) {
			helper.error("Unexpected string in departure date column!", columnsRow1[dateCol]);
			continue; // Unexcepted string in departure date column
		}
		departureDate = departureDate[1];

		// Parse departure time column
		var departureTime = helper.matchTime( columnsRow1[timeCol + 1], "hh:mm" );
		if ( departureTime.length != 2 ) {
			helper.error("Unexpected string in departure time column!", columnsRow1[timeCol]);
			continue; // Unexpected string in departure time column
		}

		// Parse arrival date column
		var arrivalDate = dateRegExp.exec( columnsRow2[dateCol - 1] );
		if ( arrivalDate == null ) {
			arrivalDate = departureDate;
		} else {
			arrivalDate = arrivalDate[1];
		}

		// Parse arrival time column
		var arrivalTime = helper.matchTime( columnsRow2[timeCol], "hh:mm" );
		if ( arrivalTime.length != 2 ) {
			helper.error("Unexpected string in arrival time column!", columnsRow2[timeCol]);
			continue; // Unexpected string in arrival time column
		}

		// Parse duration column
		var duration = null;
		duration = durationRegExp.exec( columnsRow1[durationCol] );
		if ( duration != null ) {
			duration = duration[1];
		}

		// Parse types of vehicles (products) column
		var typesOfVehicleInJourney = new Array;
		var types = helper.trim( columnsRow1[typesOfVehicleCol] );
		types = types.split( /,/i );
		for ( var n = 0; n < types.length; ++n ) {
			typesOfVehicleInJourney.push( helper.trim(types[n]).replace(/\d$/, '') );
		}

		// Parse changes column
		var changes = helper.trim( columnsRow1[changesCol] );

		// Parse details if available
		var detailsResults = new detailsResultsObject();
		println( "detailsCol = " + detailsCol );
		if ( detailsCol != -1 ) {
			var detailsColRegExp = /<h2[^>]*>Verbindung\s+([0-9]+)<\/h2>/i;
			//   /<a .*title=\"[^\"]*([0-9]+)/i;
			//   println("regexp created, run on '" + columnsRow1[detailsCol] + "'");
			var res1 = detailsColRegExp.exec( columnsRow1[detailsCol] );
// 			println("regexp executed");
			if ( res1 != null ) {
				// var journeyNrStr = helper.trim( columnsRow1[detailsCol] );

// 				println( res1[1] );
				var journeyNrStr = res1[1];

				var journeyNrRegExp = /([0-9]+)/i;
				var res = journeyNrRegExp.exec( journeyNrStr );
				// Go through all stop options
				if ( res != null ) {
					// 			  var journeyNr = helper.trim( helper.stripTags(columnsRow1[detailsCol]) );
					var journeyNr = parseInt( res[1] );
					var details = helper.extractBlock( detailsHtml,
							'<table summary="Details - Verbindung ' + journeyNr +
							'" cellspacing="0" width="100%" class="hac_detail">', '</table>' );
					// 			println( "    detailsHtml = " + detailsHtml );
					// 			var details = helper.extractBlock( detailsHtml,
					// 				'<table summary="Details - Verbindung ' + journeyNr +
					// 				'" cellspacing="0" width="100%" class="hac_detail">', '</table>' );
					parseJourneyDetails( details, detailsResults );
// 					println("TEST 2 " + detailsResults.routeStops.length);
				}

				if ( typeof(detailsResults.routeStops) != 'undefined'
					&& detailsResults.routeStops.length > 0 )
				{
					if ( typeof(startStopName) != 'string' || startStopName.length == 0 ) {
						startStopName = detailsResults.routeStops[0];
					}
					if ( typeof(targetStopName) != 'string' || targetStopName.length == 0 ) {
						targetStopName = detailsResults.routeStops[
								detailsResults.routeStops.length - 1 ];
					}
				}
			} else { 
				helper.error("Heading not found in details column", columnsRow1[detailsCol]);
			}
		}

		// Parse capacity
		if ( capacityCol != -1 ) {
			var capacityFirstClass = '';
			var capacitySecondClass = '';
			var capacityHtml = helper.trim( columnsRow1[capacityCol] );
			var capacityRegExp = /[1-2]\.\s+<img.*title="([^\"]+)"/ig;
			var capacity;
			if ( capacity = capacityRegExp.exec(capacityHtml) ) {
				capacityFirstClass = capacity[1].split( /,/i );
			}
			if ( capacity = capacityRegExp.exec(capacityHtml) ) {
				capacitySecondClass = capacity[1].split( /,/i );
			}

			if ( capacityFirstClass.length > 0 && capacitySecondClass.length > 0 ) {
				var capacityRemarks = "1. " + capacityFirstClass + ", 2. " + capacitySecondClass;

				// There's no special timetableData item like 'Capacity'.
				// Plus: There is capacity information for each "sub-journey".
				// Such a value could be added in the publictransport data engine
				// and then be visualized in the routes item in the applet.
				if ( typeof(detailsResults.journeyNews) == 'string'
					&& detailsResults.journeyNews.length > 0 )
				{
					detailsResults.journeyNews += ", " + capacityRemarks;
				} else {
					detailsResults.journeyNews = capacityRemarks;
				}
			}
		}

		// Add journey
		timetableData.clear();
		timetableData.set( 'TypesOfVehicleInJourney', typesOfVehicleInJourney );
		timetableData.set( 'StartStopName', startStopName );
		timetableData.set( 'TargetStopName', targetStopName );
		timetableData.set( 'DepartureDate', departureDate );
		timetableData.set( 'DepartureHour', departureTime[0] );
		timetableData.set( 'DepartureMinute', departureTime[1] );
		timetableData.set( 'ArrivalDate', arrivalDate );
		timetableData.set( 'ArrivalHour', arrivalTime[0] );
		timetableData.set( 'ArrivalMinute', arrivalTime[1] );
		timetableData.set( 'Duration', duration );
		timetableData.set( 'Changes', changes );
		timetableData.set( 'JourneyNews', detailsResults.journeyNews );
		timetableData.set( 'RouteStops', detailsResults.routeStops );
		timetableData.set( 'RoutePlatformsDeparture', detailsResults.routePlatformsDeparture );
		timetableData.set( 'RoutePlatformsArrival', detailsResults.routePlatformsArrival );
		timetableData.set( 'RouteTimesDeparture', detailsResults.routeTimesDeparture );
		timetableData.set( 'RouteTimesArrival', detailsResults.routeTimesArrival );
		timetableData.set( 'RouteTimesDepartureDelay', detailsResults.routeTimesDepartureDelay );
		timetableData.set( 'RouteTimesArrivalDelay', detailsResults.routeTimesArrivalDelay );
		timetableData.set( 'RouteTypesOfVehicles', detailsResults.routeVehicleTypes );
		timetableData.set( 'RouteTransportLines', detailsResults.routeTransportLines );
		result.addData( timetableData );
	}
}

function parsePossibleStops( html ) {
	if ( parsePossibleStops_1(html) ) {
		return true;
	} else if ( parsePossibleStops_2(html) ) {
		return true;
	} else {
		return false;
	}
}

function parsePossibleStops_1( html ) {
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

function parsePossibleStops_2( html ) {
	// Find block of stops
	var pos = html.search( /<select name="REQ0JourneyStopsZ0K"[^>]*?>/i );
	if ( pos == -1 ) {
		return;
	}
	var end = html.indexOf( '</select>', pos + 1 );
	var str = html.substr( pos, end - pos );

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
