/** Accessor for www.db.de (Deutsche Bahn, germany).
  * © 2010, Friedrich Pülz */

Array.prototype.contains = function( element ) {
    for ( var i = 0; i < this.length; i++ ) {
	if ( this[i] == element )
	    return true;
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
    return [ 'Delay', 'DelayReason', 'Platform', 'JourneyNews', 'TypeOfVehicle',
	     'StopID', 'Pricing', 'Changes', 'RouteStops', 'RoutePlatformsDeparture',
	     'RoutePlatformsArrival', 'RouteTimesDeparture', 'RoutePlatformsArrival',
	     'RouteTransportLines' ];
}

function getUrlForLaterJourneyResults( html ) {
    var urlRegExp = /<a href="([^"]*?)" class="arrowlink arrowlinkbottom" >Sp&#228;ter<\/a>/i;
    var urlLater = urlRegExp.exec( html );
    if ( urlLater != null )
		return 'http://reiseauskunft.bahn.de' + urlLater[1];
    else
		return null;
}

function parseTimetable( html ) {
    var returnValue = new Array;
    if ( html.indexOf('<p class="red">F&#252;r diese Haltestelle sind keine aktuellen Informationen verf&#252;gbar.</p>') != -1
		|| html.indexOf('<p>Aktuelle Informationen zu Ihrer Reise sind nur 120 Minuten im Voraus m&#246;glich.</p>') != -1 ) 
	{
		returnValue.push( 'no delays' ); // No delay information available
	}
	
	// Dates are set from today, not the requested date. They need to be adjusted by X days,
	// where X is the difference in days between today and the requested date.
	returnValue.push( 'dates need adjustment' );
	
	var now = new Date();
	var currentDate = [ now.getDate(), now.getMonth() + 1, now.getFullYear() ];

    // Find block of departures
    var str = helper.extractBlock( html, '<table class="result stboard dep">', '</table>' );
    if ( str.length == 0 ) {
		str = helper.extractBlock( html, '<table class="result stboard arr">', '</table>' );
    }

    // Initialize regular expressions (compile them only once)
    var departuresRegExp = /<tr>([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td class="([^"]*?)">([\s\S]*?)<\/td>/ig;
    var typeOfVehicleRegExp = /<img src="[^"]*?\/img\/([^_]*?)_/i;
    var targetRegExp = /<span class="bold">[^<]*?<a[^>]*?>([\s\S]*?)<\/a>/i;
    var routeBlocksRegExp = /\r?\n\s*(-|<img[^>]*?>)\s*\r?\n/gi;
    var routeBlockEndOfExactRouteMarkerRegExp = /<img[^>]*?>/i;
    var platformRegExp = /<strong>([^<]*?)<\/strong>/i; //<br \/>\s*[^<]*
    var delayRegExp = /<span style="[^"]*?">ca.&nbsp;(\d*)&nbsp;Minuten&nbsp;sp&#228;ter<\/span>/i;
    var delayWithReasonRegExp = /<span><span style="[^"]*?">ca.&nbsp;(\d*)&nbsp;Minuten&nbsp;sp&#228;ter<\/span><\/span>,<br\/><span class="[^"]*?">Grund:\s*?([^<]*)<\/span>/i;
    var delayOnTimeRegExp = /<span style="[^"]*?">p&#252;nktlich<\/span>/i;
    var trainCanceledRegExp = /<span class="[^"]*?">Zug f&#228;llt aus<\/span>/i;
    
// println("DE_DB: Go through all departure blocks");
	
    // Go through all departure blocks
	var lastHour = 0;
	var departureNumber = 0;
    while ( (departureRow = departuresRegExp.exec(str)) ) {
		departureRow = departureRow[1];
		++departureNumber;
// println("DE_DB: Next departure Row " + departureNumber);

		// Get column contents
		var columns = new Array;
		var columnMeanings = new Array;
		var iTrain = 1;
		while ( (col = columnsRegExp.exec(departureRow)) ) {
			var meaning = col[1];
			
			if ( meaning == "train" ) {
				meaning += iTrain;
				++iTrain;
			}
			columns[ meaning ] = col[2];
			columnMeanings.push( meaning );
		}
		
		if ( !columnMeanings.contains("time") || !columnMeanings.contains("route")
			|| !columnMeanings.contains("train1") || !columnMeanings.contains("train2") ) 
		{
			if ( departureRow.indexOf("<th") == -1 ) {
				// There's only an error, if this isn't the header row of the table
				helper.error("Didn't find all columns in a row of the result table!", departureRow == null ? "" : departureRow);
			}
			continue; // Not all columns where not found
		}
			
		// Initialize result variables with defaults
		var time = 0, typeOfVehicle = "", transportLine = "", targetString = "", platformString = "",
			delay = -1, delayReason = "", journeyNews = "",
			routeStops = new Array, routeTimes = new Array, exactRouteStops = 0;
			
		// Parse time column
		time = helper.matchTime( columns["time"], "hh:mm" );
		if ( time.length != 2 ) {
			helper.error("Unexpected string in time column", columns["time"] == null ? "" : columns["time"]);
			continue;
		}
		
		// TODO
		// If 0 o'clock is passed between the last departure time and the current one, increase the date by one day.
		if ( time[0] < lastHour || (departureNumber == 1 && time[0] == 0 && now.getHours() == 23) ) {
			currentDate = helper.addDaysToDate( currentDate, 1 );
		}
		lastHour = time[0];
		
		// Parse type of vehicle column
		if ( (typeOfVehicle = typeOfVehicleRegExp.exec(columns["train1"])) == null ) {
			helper.error("Unexpected string in type of vehicle column", columns["train1"] == null ? "" : columns["train1"]);
			continue; // Unexcepted string in type of vehicle column
		}
		typeOfVehicle = typeOfVehicle[1];
		
		// Parse transport line column
		transportLine = helper.trim( helper.stripTags(columns["train2"]) );

		// Parse route column ..
		//  .. target
		if ( (targetString = targetRegExp.exec(columns["route"])) == null ) {
			helper.error("Unexpected string in target column", columns["route"] == null ? "" : columns["route"]);
			continue; // Unexcepted string in target column
		}
		targetString = helper.trim( targetString[1] );
		
		// .. route
		var route = columns["route"];
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
				if ( lines.count < 2 )
					continue;

				routeStops.push( lines[0] );
				routeTimes.push( lines[1] );
			}
		}
		
		// Parse platform column if any
		if ( columnMeanings.contains("platform") ) {
			var platformArr = platformRegExp.exec( columns["platform"] );
			if ( platformArr != null )
				platformString = helper.trim( platformArr[0] );
		}
		
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
		}
		
		// Add departure
		timetableData.clear();
		timetableData.set( 'TransportLine', transportLine );
		timetableData.set( 'TypeOfVehicle', typeOfVehicle );
		timetableData.set( 'Target', targetString );
		timetableData.set( 'DepartureDate', currentDate );
		timetableData.set( 'DepartureHour', time[0] );
		timetableData.set( 'DepartureMinute', time[1] );
		timetableData.set( 'Platform', platformString );
		timetableData.set( 'Delay', delay );
		timetableData.set( 'DelayReason', delayReason );
		timetableData.set( 'JourneyNews', journeyNews );
		timetableData.set( 'RouteStops', routeStops );
		timetableData.set( 'RouteTimes', routeTimes );
		timetableData.set( 'RouteExactStops', exactRouteStops );
		result.addData( timetableData );
    }

    return returnValue;
}

function parseJourneys( code ) {
    var useNewParsingCode = false;
    if ( useNewParsingCode ) {
		println( "--------- PARSE CODE FROM DB AFTER THEIR HALF-CLOSED SPECIFICATION ---------" );
		println( "Received code: " + code );
		
		// Should always start with "BAHN_MNB.fm = {"
		if ( code.indexOf("BAHN_MNB.fm = {") != 0 ) {
			helper.error( "The received code doesn't start with \"BAHN_MNB.fm = {\"", code == null ? "" : code );
		} else {
			code = code.substr( 15 );
			
			var infoRegExp = /"(S|Z|E|fl)":\s*("[^"]*"|\[[^\]]*\])/ig;
			var journeyListRegExp = /\{([^\}]*)\}/ig
			var journeyRegExp = /"([^"]*)":\s*("[^"]*"|\d*)/ig;
			
			println( "SEARCH..." );
			var startName = "", targetName = "", errorText = "", journeys = "";
			while ( (info = infoRegExp.exec(code)) ) {
				if ( info.length < 3 )
					continue;
				
				var infoType = info[1];
				var infoText = info[2];
				
				println( "FOUND: ''" + infoType + "'' : ''" + infoText.substr(0,100).replace(/\n/g, "\n     ") + "...''" );
				
				if ( infoType == "S" ) {
					startName = infoText;
				} else if ( infoType == "Z" ) {
					targetName = infoText;
				} else if ( infoType == "E" ) {
					errorText = infoText;
				} else if ( infoType == "fl" ) {
					// Parse journey list
					while ( (journeyList = journeyListRegExp.exec(infoText)) ) {
						var journeyInfos = journeyList[1];
						println( "FOUND JOURNEY INFOS: ''" + journeyInfos.replace(/\n/g, "\n     ") + "''" );
						
						var departureTime = "", departureDelay = -1, departureDelayReason = "";
						var arrivalTime = "", arrivalDelay = -1, arrivalDelayReason = "";
						var changes = -1, duration = "", platform = "", vehiclesShort = "", vehiclesLong = "";
						// Parse journey
						while ( (journey = journeyRegExp.exec(journeyInfos)) ) {
							if ( info.journey < 3 )
								continue;
							
							var journeyInfo = journey[1];
							var journeyText = journey[2];
							var journeyNumber = parseInt( journeyText );
							
							if ( journeyInfo == "ab" )
								departureTime = journeyText;
							else if ( journeyInfo == "abps" )
								departureDelay = journeyNumber;
							else if ( journeyInfo == "abpm" )
								departureDelayReason = journeyText;
							else if ( journeyInfo == "an" )
								arrivalTime = journeyText;
							else if ( journeyInfo == "anps" )
								arrivalDelay = journeyNumber;
							else if ( journeyInfo == "anpm" )
								arrivalDelayReason = journeyText;
							else if ( journeyInfo == "u" )
								changes = journeyNumber;
							else if ( journeyInfo == "d" )
								duration = journeyText;
							else if ( journeyInfo == "g" )
								platform = journeyText;
							else if ( journeyInfo == "ps" )
								vehiclesShort = journeyText;
							else if ( journeyInfo == "pl" )
								vehiclesLong = journeyText;
						}
						journeyRegExp.lastIndex = 0; // Reset regexp
						// All journey infos parsed here.

						// Parse duration (in "hh:mm" format with double quotes) to number of minutes
						if ( duration.length < 6 || duration.length > 7 ) {
							helper.error( "Duration has wrong format, ie. not \"hh:mm\" or \"h:mm\" with double quotes.", duration );
							continue;
						}
						
						var pos = duration.indexOf( ':', 0 );
						println( "DUR Hours: " + duration.substr(1, pos-1) + ", DUR Mins: " + duration.substr(pos+1, 2) );
						var durationMinutes = parseInt(duration.substr(1, pos-1)) * 60 + parseInt(duration.substr(pos+1, 2));
						
						// Parse times
						println( "PARSE DEP TIME " + departureTime );
						var departureTimeInfos = helper.matchTime( departureTime, "hh:mm" );
						if ( departureTimeInfos.length != 2 )
							continue;
						
						println( "PARSE ARR TIME " + arrivalTime );
						var arrivalTimeInfos = helper.matchTime( arrivalTime, "hh:mm" );
						if ( arrivalTimeInfos.length != 2 )
							continue;

						// Add journey
						timetableData.clear();
						var typesOfVehicleInJourney = vehiclesLong.split(/,\s*/);
						println( "TypesOfVehicleInJourney: " + typesOfVehicleInJourney );
						println( "StartStopName: " + startName );
						println( "TargetStopName: " + targetName );
						println( "Departure Time: " + departureTimeInfos[0] + ":" + departureTimeInfos[1] );
						println( "Arrival Time: " + arrivalTimeInfos[0] + ":" + arrivalTimeInfos[1] );
						println( "Duration: " + durationMinutes + " Minutes" );
						println( "Changes: " + changes );
						timetableData.set( 'TypesOfVehicleInJourney', typesOfVehicleInJourney ); //vehiclesLong );
				// 	    timetableData.set( 'RouteTypesOfVehicles', detailsResults.routeVehicleTypes );
						timetableData.set( 'StartStopName', startName );
						timetableData.set( 'TargetStopName', targetName );
					// 	  timetableData.set( 'DepartureDate', departureDate );
						timetableData.set( 'DepartureHour', departureTimeInfos[0] );
						timetableData.set( 'DepartureMinute', departureTimeInfos[1] );
					// 	  timetableData.set( 'ArrivalDate', arrivalDate );
						timetableData.set( 'ArrivalHour', arrivalTimeInfos[0] );
						timetableData.set( 'ArrivalMinute', arrivalTimeInfos[1] );
						timetableData.set( 'Duration', durationMinutes );
						timetableData.set( 'Changes', changes );
				// 	    timetableData.set( 'Pricing', pricing );
				// 	    timetableData.set( 'JourneyNews', detailsResults.journeyNews );
				// 	    timetableData.set( 'RouteStops', detailsResults.routeStops );
				// 	    timetableData.set( 'RoutePlatformsDeparture', detailsResults.routePlatformsDeparture );
				// 	    timetableData.set( 'RoutePlatformsArrival', detailsResults.routePlatformsArrival );
				// 	    timetableData.set( 'RouteTimesDeparture', detailsResults.routeTimesDeparture );
				// 	    timetableData.set( 'RouteTimesArrival', detailsResults.routeTimesArrival );
				// 	    timetableData.set( 'RouteTimesDepartureDelay', detailsResults.routeTimesDepartureDelay );
				// 	    timetableData.set( 'RouteTimesArrivalDelay', detailsResults.routeTimesArrivalDelay );
				// 	    timetableData.set( 'RouteTypesOfVehicles', detailsResults.routeVehicleTypes );
				// 	    timetableData.set( 'RouteTransportLines', detailsResults.routeTransportLines );
						result.addData( timetableData );
						
						println( "JOURNEY ADDED.\n\n\n" );
					}
				}
			}
      }
      println("END");
      return;
    }


    if ( code.indexOf("BAHN_MNB.fm = {") != -1 ) {
		println("Warning: Using old html parsing code, but got data in new format");
    }

    //    OLD HTML PARSING CODE
    var html = code;

    // Find block of journeys
    var str = helper.extractBlock( html, '<table class="result" cellspacing="0">',
			    '<div id="hafasContentEnd">' );

    // Initialize regular expressions (compile them only once)
    var journeysRegExp = /<tr class="\s*?firstrow">([\s\S]*?)<\/tr>[\s\S]*?<tr class="\s*?last">([\s\S]*?)<\/tr>/ig
    var nextJourneyBeginRegExp = /<tr class="\s*?firstrow">/i;
    var columnsRegExp = /<td class="([^"]*?)"[^>]*?>([\s\S]*?)<\/td>/ig;
    var startStopNameRegExp = /<div class="resultDep">\s*([^<]*?)\s*<\/div>/i;
    var dateRegExp = /([0-9]{2}\.[0-9]{2}\.[0-9]{2})/i;
    var durationRegExp = /(\d{1,2}:\d{2})/i;
    var vehicleTypesRegExp = /<span[^>]*?><a[^>]*?>(?:[^<]*?<img src="[^"]*?"[^>]*?>)?([^<]*?)<\/a><\/span>/i;
    var journeyDetailsRegExp = /<div id="[^"]*?" class="detailContainer[^"]*?">\s*?<table class="result">([\s\S]*?)<\/table>/ig;

    // Go through all journey blocks
    while ( (journey = journeysRegExp.exec(str)) ) {
	journeyRow1 = journey[1];
	journeyRow2 = journey[2];

	// Get column contents
	var columns = new Array;
	var columnMeanings = new Array;
	while ( (col = columnsRegExp.exec(journeyRow1)) ) {
	    var meaning = helper.trim(col[1].replace(/(lastrow|button-inside|tablebutton)/gi, '')) + 'Row1';
	    columns[ meaning ] = col[2];
	    columnMeanings.push( meaning );
	}
	columnsRegExp.lastIndex = 0;

	while ( (col = columnsRegExp.exec(journeyRow2)) ) {
	    var meaning = helper.trim(col[1]) + 'Row2';
	    columns[ meaning ] = col[2];
	    columnMeanings.push( meaning );
	}

	if ( !columnMeanings.contains("timeRow1")
		|| !columnMeanings.contains("durationRow1")
		|| !columnMeanings.contains("station firstRow1")
		|| !columnMeanings.contains("station stationDestRow2") )
	    continue; // Not all columns where not found

	// Initialize result variables with defaults
	var startStopName, targetStopName, departureDate, departureTime,
	    arrivalDate, arrivalTime, typesOfVehicleInJourney = new Array,
	    duration = "", pricing = "", changes;
	    
	// Parse start stop column
	startStopName = startStopNameRegExp.exec( columns["station firstRow1"] );
	if ( startStopName == null )
	    continue; // Unexcepted string in start stop column
	startStopName = startStopName[1];

	// Parse target stop column
	targetStopName = helper.trim( columns["station stationDestRow2"] );

	// Parse departure date column
	if ( (departureDate = dateRegExp.exec(columns["dateRow1"])) == null )
	    continue; // Unexcepted string in departure date column
	departureDate = departureDate[1];

	// Parse departure time column
	departureTime = helper.matchTime( columns["timeRow1"], "hh:mm" );
	if ( departureTime.length != 2 )
	    continue;

	// Parse arrival date column
	if ( (arrivalDate = dateRegExp.exec(columns["dateRow2"])) == null )
	    continue; // Unexcepted string in arrival date column
	arrivalDate = arrivalDate[1];

	// Parse arrival time column
	arrivalTime = helper.matchTime( columns["timeRow2"], "hh:mm" );
	if ( arrivalTime.length != 2 )
	    continue;

	// Parse duration column
	if ( columnMeanings.contains("durationRow1") ) {
	    duration = durationRegExp.exec( columns["durationRow1"] );
	    if ( duration != null )
		duration = duration[1];
	}

	// Parse types of vehicles (products) column
	if ( columnMeanings.contains("productsRow1") ) {
	    var types = vehicleTypesRegExp.exec( columns["productsRow1"] );
	    if ( types != null ) {
		types = types[1];
		types = types.split( /,/i );
		for ( var n = 0; n < types.length; ++n )
		    typesOfVehicleInJourney.push( helper.trim(types[n]) );
	    } else {
		// This is changed, test it TODO
		types = helper.stripTags( columns["productsRow1"] ).split( /,/i );
		for ( var n = 0; n < types.length; ++n )
		    typesOfVehicleInJourney.push( helper.trim(types[n]) );
	    }
	}

	// Parse changes column
	changes = helper.trim( columns["changesRow1"] );

	// Parse pricing column
	if ( columnMeanings.contains("fareStdRow1") ) {
	    pricing = helper.trim( helper.stripTags(columns["fareStdRow1"]) );
	    pricing = pricing.replace( /(Zur&nbsp;Buchung|Zur&nbsp;Reservierung|\(Res. Info\)|Preisauskunft nicht m&#246;glich)/ig, '' );
	}

	// Parse details if available
	var detailsResults = new detailsResultsObject();
	var str2 = str.substr( journeysRegExp.lastIndex );
	var details = journeyDetailsRegExp.exec( str2 );
	// Make sure that the details block belongs to the just parsed journey
	var beginOfNextJourney = str2.search( nextJourneyBeginRegExp );
	if ( details != null && (beginOfNextJourney < 0
		|| journeyDetailsRegExp.lastIndex <= beginOfNextJourney) ) {
	    details = details[1];
	    parseJourneyDetails( details, detailsResults );
	}
	journeyDetailsRegExp.lastIndex = 0; // start from beginning next time

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
	timetableData.set( 'Pricing', pricing );
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


function parseJourneyDetails( details, resultObject ) {
    // 	    var exactRouteStops = 0;
    var journeyNews = null;
    var journeyNewsRegExp = /<tr>\s*?<td colspan="8" class="red bold"[^>]*?>([\s\S]*?)<\/td>\s*?<\/tr>/i;
    if ( (journeyNews = journeyNewsRegExp.exec(details)) != null ) {
		journeyNews = helper.trim( journeyNews[1] );
		if ( journeyNews != '' )
			resultObject.journeyNews = journeyNews;
    }

    var headerEndRegExp = /<tr>\s*?<th>[\s\S]*?<\/tr>/ig;
    if ( headerEndRegExp.exec(details) == null )
		return;
    var str = details.substr( headerEndRegExp.lastIndex );

    var lastStop = null, lastArrivalTime = '';
    var detailsBlockRegExp = /<tr>([\s\S]*?)<\/tr>\s*?<tr class="last">([\s\S]*?)<\/tr>/ig;
    var columnsContentsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
//     var timeRegExp = /(\d{2}):(\d{2})/i;
    var timeCompleteRegExp = /(\d{2}:\d{2})/i;
    var delayRegExp = /<span[^>]*?>ca.&nbsp;\+(\d+?)&nbsp;Min.&nbsp;<\/span>/i;
    var delayOnScheduleRegExp = /<span[^>]*?>p&#252;nktlich<\/span>/i;
    var trainCanceledRegExp = /<span[^>]*?>Zug f&#228;llt aus<\/span>/i;

    while ( (detailsBlock = detailsBlockRegExp.exec(str)) ) {
	detailsBlockRow1 = detailsBlock[1];
	detailsBlockRow2 = detailsBlock[2];

	var columnsRow1 = new Array;
	var columnsRow2 = new Array;
	while ( (col = columnsContentsRegExp.exec(detailsBlockRow1)) )
	    columnsRow1.push( col[1] );
	while ( (col = columnsContentsRegExp.exec(detailsBlockRow2)) )
	    columnsRow2.push( col[1] );

	if ( columnsRow1.count < 4 )
	    break;

	var routeStop = helper.trim( columnsRow1[0] );

	// Search for the vehicle type in the transport line column
	// (including html entities, ie. '&#0245;' or '&uuml;')
	var routeVehicleType = /(([abcdefghijklmnopqrstuvwxyz]|&#\d{2,5};|&\w{2,5};)+)/i.exec(
		helper.stripTags(columnsRow1[5]) );
	if ( routeVehicleType != null )
	    routeVehicleType = helper.trim( routeVehicleType[1] );

	var routePlatformDeparture = helper.trim( helper.stripTags(columnsRow1[4]) );
	var routePlatformArrival = helper.trim( helper.stripTags(columnsRow2[4]) );

	var routeTransportLine;
	var separatorPos = columnsRow1[5].search( /<br \/>/i );
	if ( separatorPos <= 0 )
	    routeTransportLine = helper.trim( helper.stripTags(columnsRow1[5]) );
	else {
	    // When the line of the vehicle changes during the journey,
	    // both lines are written into the column, separated by <br />
	    var line1 = columnsRow1[5].substr( 0, separatorPos );
	    var line2 = columnsRow1[5].substr( separatorPos );
	    routeTransportLine = helper.trim( helper.stripTags(line1) ) + ", "
		    + helper.trim( helper.stripTags(line2) );
	}

	// Infos about canceled trains are given in the
	// departure times column for some reason...
	if ( trainCanceledRegExp.test(columnsRow1[3]) ) {
	    var infoText = "Zug '" + routeTransportLine + "' f&auml;llt aus.";
	    if ( resultObject.journeyNews == null )
		resultObject.journeyNews = infoText;
	    else
		resultObject.journeyNews += " " + infoText;

	    routeTransportLine += " <span style='color:red;'>Zug f&auml;llt aus</span>";
	}

	// If there's no departure time given, use the last arrival
	// as departure (eg. when vehicle type is Feet).
	var timeDeparture = timeCompleteRegExp.exec( columnsRow1[3] );
	if ( timeDeparture != null )
	    timeDeparture = timeDeparture[1];
	else
	    timeDeparture = lastArrivalTime;

	var delayDeparture = delayRegExp.exec( columnsRow1[3] );
	if ( delayDeparture != null )
	    delayDeparture = delayDeparture[1];
	else if ( delayOnScheduleRegExp.test(columnsRow1[3]) )
	    delayDeparture = 0;
	else
	    delayDeparture = -1;

	var delayArrival = delayRegExp.exec( columnsRow2[3] );
	if ( delayArrival != null )
	    delayArrival = delayArrival[1];
	else if ( delayOnScheduleRegExp.test(columnsRow2[3]) )
	    delayArrival = 0;
	else
	    delayArrival = -1;

	// If there's no arrival time given try to find the duration
	// in the notes column and add it to the departure time (used for footways)
	var timeArrival = timeCompleteRegExp.exec( columnsRow2[3] );
	if ( timeArrival != null )
	    timeArrival = timeArrival[1];
	else {
	    var timeArrivalParts = helper.matchTime( columnsRow2[3], "hh:mm" );
	    var timeDepartureParts = helper.matchTime( timeDeparture, "hh:mm" );
	    
	    if ( timeArrivalParts.length < 2 && timeDepartureParts.length >= 2 ) {
		var durationPart = /(\d*?) Min./i.exec( columnsRow1[6] );
		if ( durationPart != null && parseInt(durationPart[1]) > 0 )
		    timeArrival = helper.addMinsToTime( timeDeparture,
				    parseInt(durationPart[1]), "hh:mm" );
	    }
	}

	resultObject.routeStops.push( routeStop );
	resultObject.routePlatformsDeparture.push( routePlatformDeparture );
	resultObject.routePlatformsArrival.push( routePlatformArrival );
	resultObject.routeVehicleTypes.push( routeVehicleType );
	resultObject.routeTransportLines.push( routeTransportLine );
	resultObject.routeTimesDeparture.push( timeDeparture );
	resultObject.routeTimesDepartureDelay.push( delayDeparture );
	resultObject.routeTimesArrival.push( timeArrival );
	resultObject.routeTimesArrivalDelay.push( delayArrival );

	lastStop = columnsRow2[0];
	lastArrivalTime = timeArrival;
    }

    if ( lastStop != null )
		resultObject.routeStops.push( helper.trim(lastStop) );

    return true;
}

function parsePossibleStops( html ) {
    if ( parsePossibleStops_1(html) )
		return true;
    else if ( parsePossibleStops_2(html) )
		return true;
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

function parsePossibleStops_2( html ) {
    // Find block of stops
    var pos = html.search( /<select class="locInput[^"]*?" name="REQ0JourneyStopsZ0K" id="REQ0JourneyStopsZ0K"[^>]*?>/i );
    if ( pos == -1 )
		return;
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
