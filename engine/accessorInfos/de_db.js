/** Accessor for www.db.de (Deutsche Bahn, germany).
  * © 2010, Friedrich Pülz */

function stripTags( html ) {
    return html.replace( /<\/?[^>]+?>/g, '' );
}

function trim( str ) {
    return str.replace( /^\s+|\s+$/g, '' ).replace( /^(&nbsp;)+|(&nbsp;)+$/g, '' );
}

function extractBlock( str, beginString, endString ) {
    var pos = str.indexOf( beginString );
    if ( pos == -1 )
	return "";
    
    var end = str.indexOf( endString, pos + 1 );
    return str.substr( pos, end - pos );
}

Array.prototype.contains = function( element ) {
    for ( var i = 0; i < this.length; i++ ) {
	if ( this[i] == element )
	    return true;
    }
    return false;
}

function detailsResultsObject() {
    this.journeyNews = null;
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


var Parser = {
    result: null,

    getUrlForLaterJourneyResults : function( html ) {
	var urlRegExp = /<a href="([^"]*?)" class="arrowlink arrowlinkbottom" >Sp&#228;ter<\/a>/i;
	var urlLater = urlRegExp.exec( html );
	if ( urlLater != null )
	    this.result = 'http://reiseauskunft.bahn.de' + urlLater[1];
	else
	    this.result = null;
    },

//     getUrlForDetailedJourneyResults : function( html ) {
// 	var urlRegExp = /<a id="showAllDetails" class="flaparrowlink " href="([^"]*?)">Details f&#252;r alle anzeigen<\/a>/i;
// 	var urlDetails = urlRegExp.exec( html );
// 	if ( urlDetails != null )
// 	    this.result = urlDetails[1];
// 	else
// 	    this.result = null;
//     },
    
    parseTimetable : function( html ) {
	this.result = new Array();
	
	// Find block of departures
	var str = extractBlock( html, '<table class="result stboard dep">', '</table>' );

	// Initialize regular expressions (compile them only once)
	var departuresRegExp = /<tr>([\s\S]*?)<\/tr>/ig;
	var columnsRegExp = /<td class="([^"]*?)">([\s\S]*?)<\/td>/ig;
	var timeRegExp = /(\d{2}):(\d{2})/i;
	var typeOfVehicleRegExp = /<img src="[^"]*?\/img\/([^_]*?)_/i;
	var targetRegExp = /<span class="bold">[^<]*?<a[^>]*?>([\s\S]*?)<\/a>/i;
	var routeBlocksRegExp = /\r?\n\s*(-|<img[^>]*?>)\s*\r?\n/gi;
	var routeBlockEndOfExactRouteMarkerRegExp = /<img[^>]*?>/i;
	var platformRegExp = /<strong>([^<]*?)<\/strong>/i; //<br \/>\s*[^<]*
	var delayRegExp = /<span><span style="[^"]*?">ca.&nbsp;(\d*)&nbsp;Minuten&nbsp;sp&#228;ter<\/span><\/span>/i;
	var delayWithReasonRegExp = /<span><span style="[^"]*?">ca.&nbsp;(\d*)&nbsp;Minuten&nbsp;sp&#228;ter<\/span><\/span>,<br\/><span class="[^"]*?">Grund:\s*?([^<]*)<\/span>/i;
	var delayOnTimeRegExp = /<span><span style="[^"]*?">p&#252;nktlich<\/span><\/span>/i;

	// Go through all departure blocks
	while ( (departure = departuresRegExp.exec(str)) ) {
	    departure = departure[1];

	    // Get column contents
	    var columns = new Array;
	    var columnMeanings = new Array;
	    var iTrain = 1;
	    while ( (col = columnsRegExp.exec(departure)) ) {
		var meaning = col[1];
		
		if ( meaning == "train" ) {
		    meaning += iTrain;
		    ++iTrain;
		}
		columns[ meaning ] = col[2];
		columnMeanings.push( meaning );
	    }
	    
	    if ( !columnMeanings.contains("time")
		    || !columnMeanings.contains("route")
		    || !columnMeanings.contains("train1")
		    || !columnMeanings.contains("train2") )
		continue; // Not all columns where not found
		
	    // Parse time column
	    var timeValues = timeRegExp.exec( columns["time"] );
	    if ( timeValues == null || timeValues.length != 3 )
		continue; // Unexpected string in time column
	    var hour = timeValues[1];
	    var minute = timeValues[2];
	    
	    // Parse type of vehicle column
	    var typeOfVehicle = typeOfVehicleRegExp.exec( columns["train1"] );
	    if ( typeOfVehicle == null )
		continue; // Unexcepted string in type of vehicle column
	    typeOfVehicle = typeOfVehicle[1];
	    
	    // Parse transport line column
	    var transportLine = trim( stripTags(columns["train2"]) );
	    
	    // Parse route column ..
	    //  .. target
	    var targetString = targetRegExp.exec( columns["route"] );
	    if ( targetString == null )
		continue; // Unexcepted string in target column
	    targetString = trim( targetString[1] );

	    // .. route
	    var routeStops = new Array;
	    var routeTimes = new Array;
	    var exactRouteStops = 0;
	    var route = columns["route"];
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
		    var lines = routeBlocks[n].split( /\n/ );
		    var stop, time;
		    var j = 0;
		    for ( var lineIndex = 0; lineIndex < lines.length; ++lineIndex ) {
			line = trim( stripTags(lines[lineIndex]) );
			if ( line != "" ) {
			    ++j;
			    if ( j == 1 )
				stop = line;
			    else {
				time = line;
				break;
			    }
			}
		    }
		    if ( j != 2 )
			continue; // Missing stop or time information
		    
		    routeStops.push( stop );
		    routeTimes.push( time );
		}
	    }

	    // Parse platform column if any
	    var platformString = "";
	    if ( columnMeanings.contains("platform") ) {
		var platformArr = platformRegExp.exec( columns["platform"] );
		if ( platformArr != null )
		    platformString = trim( platformArr[0] );
	    }

	    // Parse delay column if any
	    var delay = -1;
	    var delayReason = "";
	    if ( columnMeanings.contains("ris") ) {
		if ( delayOnTimeRegExp.test(columns["ris"]) ) {
		    delay = 0;
		} else if ( (delayArr = delayWithReasonRegExp.exec(columns["ris"])) ) {
		    delay = delayArr[1];
		    delayReason = delayArr[2];
		} else if ( (delayArr = delayRegExp.exec(columns["ris"])) ) {
		    delay = delayArr[1];
		}
	    }
	    
	    // Add departure
	    this.result.push( { 'TransportLine' : transportLine,
                                'TypeOfVehicle' : typeOfVehicle,
                                       'Target' : targetString,
                                'DepartureHour' : hour,
                              'DepartureMinute' : minute,
                                     'Platform' : platformString,
                                        'Delay' : delay,
                                  'DelayReason' : delayReason,
                                   'RouteStops' : routeStops,
                                   'RouteTimes' : routeTimes,
                              'RouteExactStops' : exactRouteStops } );
	}
    },

    parseJourneyDetails : function( details, resultObject ) {
	// 	    var exactRouteStops = 0;
	var journeyNews = null;
	var journeyNewsRegExp = /<tr>\s*?<td colspan="8" class="red bold"[^>]*?>([\s\S]*?)<\/td>\s*?<\/tr>/i;
	if ( (journeyNews = journeyNewsRegExp.exec(details)) != null )
	    resultObject.journeyNews = journeyNews[1];
	
	var headerEndRegExp = /<tr>\s*?<th>[\s\S]*?<\/tr>/ig;
	if ( headerEndRegExp.exec(details) == null )
	    return;
	var str = details.substr( headerEndRegExp.lastIndex );
	
	var lastStop = null, lastArrivalTime = '';
	var detailsBlockRegExp = /<tr>([\s\S]*?)<\/tr>\s*?<tr class="last">([\s\S]*?)<\/tr>/ig;
	var columnsContentsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
	var timeRegExp = /(\d{2}):(\d{2})/i;
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

	    var routeStop = trim( columnsRow1[0] );

	    // Search for the vehicle type in the transport line column
	    // (including html entities, ie. '&#0245;' or '&uuml;')
	    var routeVehicleType = /(([abcdefghijklmnopqrstuvwxyz]|&#\d{2,5};|&\w{2,5};)+)/i.exec( stripTags(columnsRow1[5]) );
	    if ( routeVehicleType != null ) {
		routeVehicleType = routeVehicleType[1];
		routeVehicleType = trim( routeVehicleType );
	    }
	    
	    var routePlatformDeparture = trim( stripTags(columnsRow1[4]) );
	    var routePlatformArrival = trim( stripTags(columnsRow2[4]) );

	    var routeTransportLine;
	    var separatorPos = columnsRow1[5].search( /<br \/>/i );
	    if ( separatorPos <= 0 )
		routeTransportLine = trim( stripTags(columnsRow1[5]) );
	    else {
		// When the line of the vehicle changes during the journey,
		// both lines are written into the column, separated by <br />
		var line1 = columnsRow1[5].substr( 0, separatorPos );
		var line2 = columnsRow1[5].substr( separatorPos );
		routeTransportLine = trim( stripTags(line1) ) + ", " + trim( stripTags(line2) );
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
	    // in the notes column and add it to the departure time
	    var timeArrival = timeCompleteRegExp.exec( columnsRow2[3] );
	    if ( timeArrival != null )
		timeArrival = timeArrival[1];
	    else {
		var timeArrivalParts = timeRegExp.exec( columnsRow2[3] );
		var timeDepartureParts = timeRegExp.exec( timeDeparture );
		if ( timeArrivalParts == null && timeDepartureParts != null ) {
		    var durationPart = /(\d*?) Min./i.exec( columnsRow1[6] );

		    if ( durationPart != null && parseInt(durationPart[1]) > 0 ) {
			durationPart = parseInt( durationPart[1] );

			var hour = parseInt( timeDepartureParts[1] );
			var minute = parseInt( timeDepartureParts[2] ) + durationPart;
			while ( minute >= 60 ) {
			    ++hour;
			    minute -= 60;
			}
			while ( hour >= 24 ) {
			    // Add one day to date
			    hour -= 24;
			}

			if ( hour.length == 1 )
			    hour = '0' + hour;
			if ( minute.length == 1 )
			    minute = '0' + minute;

			timeArrival = hour + ':' + minute;
		    }
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

	if ( lastStop != null ) {
	    resultObject.routeStops.push( trim(lastStop) );
	    // 		    routeTimes.push( trim(lastTime) );
	}

	return true;
    },

    parseJourneys : function( html ) {
	this.result = new Array();

	// Find block of journeys
	var str = extractBlock( html, '<table class="result" cellspacing="0">',
				'<div id="hafasContentEnd">' );

	// Initialize regular expressions (compile them only once)
	var journeysRegExp = /<tr class="\s*?firstrow">([\s\S]*?)<\/tr>[\s\S]*?<tr class="\s*?last">([\s\S]*?)<\/tr>/ig
	var nextJourneyBeginRegExp = /<tr class="\s*?firstrow">/i;
	var columnsRegExp = /<td class="([^"]*?)"[^>]*?>([\s\S]*?)<\/td>/ig;
	var startStopNameRegExp = /<div class="resultDep">\s*([^<]*?)\s*<\/div>/i;
	var dateRegExp = /([0-9]{2}\.[0-9]{2}\.[0-9]{2})/i;
	var timeRegExp = /(\d{2}):(\d{2})/i;
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
		var meaning = trim(col[1].replace(/(lastrow|button-inside|tablebutton)/gi, '')) + 'Row1';
		columns[ meaning ] = col[2];
		columnMeanings.push( meaning );
	    }
	    while ( (col = columnsRegExp.exec(journeyRow2)) ) {
		var meaning = trim(col[1]) + 'Row2';
		columns[ meaning ] = col[2];
		columnMeanings.push( meaning );
	    }

	    if ( !columnMeanings.contains("timeRow1")
		    || !columnMeanings.contains("durationRow1")
		    || !columnMeanings.contains("station firstRow1")
		    || !columnMeanings.contains("station stationDestRow2") )
		continue; // Not all columns where not found

	    // Parse start stop column
	    var startStopName = startStopNameRegExp.exec( columns["station firstRow1"] );
	    if ( startStopName == null )
		continue; // Unexcepted string in start stop column
	    startStopName = startStopName[1];

	    // Parse target stop column
	    var targetStopName = trim( columns["station stationDestRow2"] );

	    // Parse departure date column
	    var departureDate = dateRegExp.exec( columns["dateRow1"] );
	    if ( departureDate == null )
		continue; // Unexcepted string in departure date column
	    departureDate = departureDate[1];

	    // Parse departure time column
	    var departureTimeValues = timeRegExp.exec( columns["timeRow1"] );
	    if ( departureTimeValues == null || departureTimeValues.length != 3 )
		continue; // Unexpected string in departure time column
	    var departureHour = departureTimeValues[1];
	    var departureMinute = departureTimeValues[2];

	    // Parse arrival date column
	    var arrivalDate = dateRegExp.exec( columns["dateRow2"] );
	    if ( arrivalDate == null )
		continue; // Unexcepted string in arrival date column
		arrivalDate = arrivalDate[1];

	    // Parse arrival time column
	    var arrivalTimeValues = timeRegExp.exec( columns["timeRow2"] );
	    if ( arrivalTimeValues == null || arrivalTimeValues.length != 3 )
		continue; // Unexpected string in arrival time column
	    var arrivalHour = arrivalTimeValues[1];
	    var arrivalMinute = arrivalTimeValues[2];

	    // Parse duration column
	    var duration = null;
	    if ( columnMeanings.contains("durationRow1") ) {
		duration = durationRegExp.exec( columns["durationRow1"] );
		if ( duration != null )
		    duration = duration[1];
	    }

	    // Parse types of vehicles (products) column
	    var typesOfVehicleInJourney = new Array;
	    if ( columnMeanings.contains("productsRow1") ) {
		var types = vehicleTypesRegExp.exec( columns["productsRow1"] );
		if ( types != null ) {
		    types = types[1];
		    types = types.split( /,/i );
		    for ( var n = 0; n < types.length; ++n )
			typesOfVehicleInJourney.push( trim(types[n]) );
		}
	    }

	    // Parse changes column
	    var changes = trim( columns["changesRow1"] );

	    // Parse pricing column
	    var pricing;
	    if ( columnMeanings.contains("fareStdRow1") ) {
		pricing = trim( stripTags(columns["fareStdRow1"]) );
		pricing = pricing.replace( /(Zur&nbsp;Buchung|Zur&nbsp;Reservierung|\(Res. Info\)|Preisauskunft nicht m&#246;glich)/ig, '' );
	    }

	    // Parse details if available
	    var detailsResults = new detailsResultsObject();
	    var str2 = str.substr( journeysRegExp.lastIndex );
	    var details = journeyDetailsRegExp.exec( str2 );
	    var beginOfNextJourney = str2.search( nextJourneyBeginRegExp );
	    if ( details != null && (beginOfNextJourney < 0
		    || journeyDetailsRegExp.lastIndex <= beginOfNextJourney) ) {
		details = details[1];
		this.parseJourneyDetails( details, detailsResults );
	    }
	    journeyDetailsRegExp.lastIndex = 0; // start from beginning next time

	    // Add journey
	    this.result.push( { 'TypesOfVehicleInJourney' : typesOfVehicleInJourney,
					  'StartStopName' : startStopName,
			                 'TargetStopName' : targetStopName,
			                  'DepartureDate' : departureDate,
			                  'DepartureHour' : departureHour,
			                'DepartureMinute' : departureMinute,
			                    'ArrivalDate' : arrivalDate,
			                    'ArrivalHour' : arrivalHour,
			                  'ArrivalMinute' : arrivalMinute,
			                       'Duration' : duration,
			                        'Changes' : changes,
			                        'Pricing' : pricing,
// 			                          'Delay' : delay,
// 			                    'DelayReason' : delayReason } );
	                                    'JourneyNews' : detailsResults.journeyNews,
			                     'RouteStops' : detailsResults.routeStops,
			        'RoutePlatformsDeparture' : detailsResults.routePlatformsDeparture,
			          'RoutePlatformsArrival' : detailsResults.routePlatformsArrival,
				    'RouteTimesDeparture' : detailsResults.routeTimesDeparture,
			              'RouteTimesArrival' : detailsResults.routeTimesArrival,
			       'RouteTimesDepartureDelay' : detailsResults.routeTimesDepartureDelay,
			         'RouteTimesArrivalDelay' : detailsResults.routeTimesArrivalDelay,
			           'RouteTypesOfVehicles' : detailsResults.routeVehicleTypes,
			            'RouteTransportLines' : detailsResults.routeTransportLines } );
// 			                'RouteExactStops' : exactRouteStops } );
	}
    },
    
    parsePossibleStops : function( html ) {
	this.result = new Array();
	
	this.parsePossibleStops_1( html );
	this.parsePossibleStops_2( html );
    },
    
    parsePossibleStops_1 : function( html ) {
	// Find block of stops
	var str = extractBlock( html, '<select class="error" id="rplc0" name="input">', '</select>' );

	// Initialize regular expressions (compile them only once)
	var stopRegExp = /<option value="[^"]+?#([0-9]+)">([^<]*?)<\/option>/ig;

	// Go through all stop options
	while ( (stop = stopRegExp.exec(str)) ) {
	    var stopID = stop[1];
	    var stopName = stop[2];

	    // Add stop
	    this.result.push( { 'StopName' : stopName,
			          'StopID' : stopID } );
	}
    },

    parsePossibleStops_2 : function( html ) {
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
	    this.result.push( { 'StopName' : stopName } );
	}
    }
}
