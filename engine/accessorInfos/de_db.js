/** Accessor for www.db.de (Deutsche Bahn, germany).
  * © 2012, Friedrich Pülz */

Array.prototype.contains = function( element ) {
    for ( var i = 0; i < this.length; i++ ) {
        if ( this[i] == element )
            return true;
    }
    return false;
}

function usedTimetableInformations() {
    return [ 'Arrivals', 'Delay', 'DelayReason', 'Platform', 'JourneyNews',
            'StopID', 'Pricing', 'Changes', 'RouteStops', 'RouteTimes',
            'RoutePlatformsDeparture', 'RoutePlatformsArrival',
            'RouteTimesDeparture', 'RouteTimesArrival',
            'RouteTypesOfVehicles', 'RouteTransportLines' ];
}

var requestDateTime;
function getTimetable( stop, dateTime, maxCount, dataType, city ) {
    // Store request date and time to know it in parseTimetable
    requestDateTime = dateTime;
    print( "write requestDateTime: " + requestDateTime );

    var url = "http://reiseauskunft.bahn.de/bin/bhftafel.exe/dn?rt=1" +
            "&input=" + stop + "!" +
            "&boardType=" + (dataType == "arrivals" ? "arr" : "dep") +
            "&date=" + helper.formatDateTime(dateTime, "dd.MM.yy") +
            "&time=" + helper.formatDateTime(dateTime, "hh:mm") +
//             "&date=" + qDateTime.toString("dd.MM.yy") +
//             "&time=" + qDateTime.toString("hh:mm") +
            "&maxJourneys=" + maxCount +
            "&disableEquivs=no&start=yes&productsFilter=111111111";

    var request = network.createRequest( url );
//     request.readyRead.connect( parseTimetableIterative );
    request.finished.connect( parseTimetable );
    network.get( request );
}

function getJourneys( originStop, targetStop, dateTime, maxCount, dataType, city ) {
//     for ( var i = 0; i < 100; ++i ) {
//         result.addData({ DepartureDateTime: new Date(), ArrivalDateTime: new Date(), StartStopName: "Fieti" + i, TargetStopName: "Annika" });
//         for ( var n = 0; n < 100000000; ++n ) {
//             var v = 666;
//         }
//         result.publish();
//     }
//     return;

//     var qDateTime = new QDateTime( dateTime );
    var url = "http://reiseauskunft.bahn.de/bin/query.exe/dn" +
            "?S=" + originStop + "!" +
            "&Z=" + targetStop + "!" +
            "&date=" + helper.formatDateTime(dateTime, "dd.MM.yy") +
            "&time=" + helper.formatDateTime(dateTime, "hh:mm") +
//             "&date=" + qDateTime.toString("dd.MM.yy") +
//             "&time=" + qDateTime.toString("hh:mm") +
            "&maxJourneys=" + maxCount +
            "&REQ0HafasSearchForw=" + (dataType == "arrivals" ? "0" : "1") +
            "&start=yes&sortConnections=minDeparture&HWAI=JS!ajax=yes!&HWAI=CONNECTION$C0-0!id=C0-0!HwaiDetailStatus=details!;CONNECTION$C0-1!id=C0-1!HwaiDetailStatus=details!;CONNECTION$C0-2!id=C0-2!HwaiDetailStatus=details!;CONNECTION$C0-3!id=C0-3!HwaiDetailStatus=details!;CONNECTION$C0-4!id=C0-4!HwaiDetailStatus=details!;CONNECTION$C1-0!id=C1-0!HwaiDetailStatus=details!;CONNECTION$C1-1!id=C1-1!HwaiDetailStatus=details!;CONNECTION$C1-2!id=C1-2!HwaiDetailStatus=details!;CONNECTION$C1-3!id=C1-3!HwaiDetailStatus=details!;CONNECTION$C1-4!id=C1-4!HwaiDetailStatus=details!;CONNECTION$C2-0!id=C2-0!HwaiDetailStatus=details!;CONNECTION$C2-1!id=C2-1!HwaiDetailStatus=details!;CONNECTION$C2-2!id=C2-2!HwaiDetailStatus=details!;CONNECTION$C2-3!id=C2-3!HwaiDetailStatus=details!;CONNECTION$C2-4!id=C2-4!HwaiDetailStatus=details!;";

    // Get three journey documents
    for ( i = 0; i < 3; ++i ) {
        // Download and parse journey document
        var html = network.getSynchronous( url );
        if ( network.lastDownloadAborted() ) {
            break;
        }
        parseJourneys( html );

        // Directly publish journeys parsed from the current document
        result.publish();

        // Search for another URL to a document with later journeys
        var urlRegExp = /<a href="([^"]*?)" class="arrowlink arrowlinkbottom"[^>]*>Sp&#228;ter<\/a>/i;
        var urlLater = urlRegExp.exec( html );
        if ( urlLater ) {
            // Set next download URL, need to decode "&amp;" to "&"
            url = 'http://reiseauskunft.bahn.de' + urlLater[1].replace( /&amp;/gi, "&" );
        } else {
            // No URL to later journeys found
            break;
        }
    }
}

function getStopSuggestions( stop, maxCount, city  ) {
    var url = "http://reiseauskunft.bahn.de/bin/ajax-getstop.exe/dn?REQ0JourneyStopsS0A=1" +
            "&REQ0JourneyStopsS0G=" + stop;
    var json = network.getSynchronous( url );

    if ( !network.lastDownloadAborted() ) {
        // Find all stop suggestions
        var stopRegExp = /{"value":"([^"]*?)","id":"[^"]*?@L=([0-9]+)@[^"]*?"[^}]*?"weight":"(\d+)"[^}]*?}/ig;
        while ( (stop = stopRegExp.exec(json)) ) {
            result.addData({ StopName: stop[1], StopID: stop[2], StopWeight: stop[3] });
        }
        return result.hasData();
    } else {
        return false;
    }
}

function parseTimetable( html ) {
    print( "read in parseTimetable -> requestDateTime: " + requestDateTime );
    var returnValue = new Array;
    if ( html.indexOf('<p class="red">F&#252;r diese Haltestelle sind keine aktuellen Informationen verf&#252;gbar.</p>') != -1
        || html.indexOf('<p>Aktuelle Informationen zu Ihrer Reise sind nur 120 Minuten im Voraus m&#246;glich.</p>') != -1 )
    {
        returnValue.push( 'no delays' ); // No delay information available
    }

    // Dates are set from today, not the requested date. They need to be adjusted by X days,
    // where X is the difference in days between today and the requested date.
    returnValue.push( 'dates need adjustment' );

    // Find block of departures
    var departureBlock = helper.findFirstHtmlTag( html, "table",
            {attributes: {"class": "result\\s+stboard\\s+(dep|arr)"}} );
    if ( !departureBlock.found ) {
        helper.error("Result table not found", html);
        return;
    }

    // Initialize regular expressions (compile them only once)
    var typeOfVehicleRegExp = /<img src="[^"]*?\/img\/([^_]*?)_/i;
    var targetRegExp = /<span class="bold">[^<]*?<a[^>]*?>([\s\S]*?)<\/a>/i;
    var routeBlocksRegExp = /\r?\n\s*(-|<img[^>]*?>)\s*\r?\n/gi;
    var routeBlockEndOfExactRouteMarkerRegExp = /<img[^>]*?>/i;
    var platformRegExp = /<strong>([^<]*?)<\/strong>/i;
    var delayRegExp = /<span style="[^"]*?">ca.&nbsp;(\d*)&nbsp;Minuten&nbsp;sp&#228;ter<\/span>/i;
    var delayWithReasonRegExp = /<span><span style="[^"]*?">ca.&nbsp;(\d*)&nbsp;Minuten&nbsp;sp&#228;ter<\/span><\/span>,<br\/><span class="[^"]*?">Grund:\s*?([^<]*)<\/span>/i;
    var delayOnTimeRegExp = /<span style="[^"]*?">p&#252;nktlich<\/span>/i;
    var trainCanceledRegExp = /<span class="[^"]*?">Zug f&#228;llt aus<\/span>/i;

    // Store values to get the correct date of departures
    var now = new Date();
    var dateTime = now;
    var lastHour = 0;

    // Go through all departure blocks
    var departureRow = { position: -1 };
    while ( (departureRow = helper.findFirstHtmlTag(departureBlock.contents, "tr",
                            {position: departureRow.position + 1})).found )
    {
        // Find columns, ie. <td> tags in the current departure row
        var columns = helper.findNamedHtmlTags( departureRow.contents, "td",
                {attributes: {"class": ""}, ambiguousNameResolution: "addNumber",
                 namePosition: {type: "attribute", name: "class", regexp: "([^\\s]*)(?:\\s+\\w+)?"}} );

        // Check that all needed columns are found
        if ( !columns.names.contains("time") || !columns.names.contains("route") ||
             !columns.names.contains("train") || !columns.names.contains("train2") )
        {
            if ( departureRow.contents.indexOf("<th") == -1 ) {
                // There's only an error, if this isn't the header row of the table
                helper.error("Didn't find all columns in a row of the result table! " +
                             "Found: " + columns.names, departureRow.contents);
            }
            continue; // Not all columns where not found
        }

        // Initialize result variables with defaults
        var departure = { RouteStops: new Array, RouteTimes: new Array, RouteExactStops: 0 };

        // Parse time column
        var time = QTime.fromString( columns["time"].contents, "hh:mm" );
        if ( !time.isValid() ) {
            helper.error("Unexpected string in time column", columns["time"].contents);
            continue;
        }
        // Update time of the dateTime object
        dateTime.setHours( time.hour(), time.minute(), 0 );

        // TODO
        // If 0 o'clock is passed between the last departure time and the current one, increase the date by one day.
// 		print( "Time: " + time[0] + ":" + time[1] + ", !result.hasData(): " + !result.hasData() + ", now: " + now.getHours() );
        if ( dateTime.getHours() < lastHour - 3 ||
            (!result.hasData() && dateTime.getHours() < now.getHours() - 3) )
        {
            print( "Add one day to departure date: " + dateTime.toDateString() );
            dateTime = helper.addDaysToDate( dateTime, 1 );
//             dateTime = new QDateTime( dateTime ).addDays( 1 );
            print( "+1 => " + dateTime.toDateString()
                + ", because of A: " + (dateTime.getHours() < (lastHour - 3 + 24) % 24)
                + ", B: " + (!result.hasData() && dateTime.getHours() < now.getHours() - 3)
                + ", time is: " + dateTime.toTimeString());
        }
        lastHour = dateTime.getHours();
        departure.DepartureDateTime = dateTime;

        // Parse type of vehicle column
        if ( !(typeOfVehicle = typeOfVehicleRegExp.exec(columns["train"].contents)) ) {
            helper.error("Unexpected string in type of vehicle column", columns["train"].contents);
            continue; // Unexcepted string in type of vehicle column
        }
        departure.TypeOfVehicle = typeOfVehicle[1];

        // Parse transport line column
        departure.TransportLine = helper.trim( helper.stripTags(columns["train2"].contents) );

        // Parse route column:
        // > Target information
        if ( !(targetString = targetRegExp.exec(columns["route"].contents)) ) {
            helper.error("Unexpected string in target column", columns["route"].contents);
            continue; // Unexcepted string in target column
        }
        departure.Target = helper.trim( targetString[1] );

        // > Route information
        var route = columns["route"].contents;
        var posRoute = route.indexOf( "<br />" );
        if ( posRoute != -1 ) {
            route = route.substr( posRoute + 6 );
            var routeBlocks = route.split( routeBlocksRegExp );

            if ( !routeBlockEndOfExactRouteMarkerRegExp.test(route) ) {
                departure.RouteExactStops = routeBlocks.length;
            } else {
                while ( (splitter = routeBlocksRegExp.exec(route)) ) {
                    ++departure.RouteExactStops;
                    if ( routeBlockEndOfExactRouteMarkerRegExp.test(splitter) )
                        break;
                }
            }

            for ( var n = 0; n < routeBlocks.length; ++n ) {
                if ( helper.trim(routeBlocks[n]).length <= 1 )
                    continue; // Current routeBlock is the matched split part

                var lines = helper.splitSkipEmptyParts( routeBlocks[n], "\n" );
                if ( lines.count < 2 )
                    continue;

                departure.RouteStops.push( lines[0] );
                departure.RouteTimes.push( lines[1] );
            }
        }

        // Parse platform column if any
        if ( columns.names.contains("platform") ) {
            if ( (platformArr = platformRegExp.exec(columns["platform"].contents)) )
                departure.Platform = helper.trim( platformArr[0] );
        }

        // Parse delay column if any
        if ( columns.names.contains("ris") ) {
            if ( delayOnTimeRegExp.test(columns["ris"].contents) ) {
                departure.Delay = 0;
            } else if ( (delayArr = delayWithReasonRegExp.exec(columns["ris"].contents)) ) {
                departure.Delay = delayArr[1];
                departure.DelayReason = delayArr[2];
            } else if ( (delayArr = delayRegExp.exec(columns["ris"].contents)) ) {
                departure.Delay = delayArr[1];
            } else if ( trainCanceledRegExp.test(columns["ris"].contents) ) {
                var infoText = "Zug f&auml;llt aus.";
                if ( departure.JourneyNews.length == 0 ) {
                    departure.JourneyNews = infoText;
                } else {
                    departure.JourneyNews += " " + infoText;
                }
            }
        } else {
            departure.Delay = -1; // Unknown delay
        }

        // Add departure
        result.addData( departure );

//         if ( result.count() % 10 == 0 ) {
//             result.publish( result );
//         }
    }

    return returnValue;
}

function parseJourneys( html ) {
    // Find block of journeys
    var journeyBlockContents = helper.extractBlock( html, '<table class="result" cellspacing="0">',
                '<div id="hafasContentEnd">' );
    if ( journeyBlockContents.length < 10 ) {
        helper.error("Did not find the result table", html);
    }

    // Initialize regular expressions (compile them only once)
    var nextJourneyBeginRegExp = /<tr class="\s*?firstrow">/i;
    var startStopNameRegExp = /<div class="resultDep">\s*([^<]*?)\s*<\/div>/i;
    var timeRegExp = /(\d{1,2}):(\d{2})/i;
    var dateRegExp = /(\d{2}).(\d{2}).(\d{2,4})/;
    var vehicleTypesRegExp = /<span[^>]*?><a[^>]*?>(?:[^<]*?<img src="[^"]*?"[^>]*?>)?([^<]*?)<\/a><\/span>/i;
    var journeyDetailsRegExp = /<div id="[^"]*?" class="detailContainer[^"]*?">\s*?<table class="result">([\s\S]*?)<\/table>/ig;

    // Go through all journey blocks (each block consists of two table rows (<tr> elements)
    var journeyRow1, journeyRow2 = { endPosition: -1 };
    while ( (journeyRow1 = helper.findFirstHtmlTag(journeyBlockContents, "tr",
                {attributes: {"class": "firstrow"}, position: journeyRow2.endPosition + 1})).found &&
            (journeyRow2 = helper.findFirstHtmlTag(journeyBlockContents, "tr",
                {attributes: {"class": "last"}, position: journeyRow1.endPosition + 1})).found )
    {
        // Find columns, ie. <td> tags in the current journey rows
        var columnsRow1 = helper.findNamedHtmlTags( journeyRow1.contents, "td",
                {attributes: {"class": ""}, ambiguousNameResolution: "addNumber",
                 namePosition: {type: "attribute", name: "class", regexp: "([^\\s]*)(?:\\s+\\w+)?"}} );
        var columnsRow2 = helper.findNamedHtmlTags( journeyRow2.contents, "td",
                {attributes: {"class": ""}, ambiguousNameResolution: "addNumber",
                 namePosition: {type: "attribute", name: "class", regexp: "([^\\s]*)(?:\\s+\\w+)?"}} );

        if ( !columnsRow1.names.contains("time") || !columnsRow1.names.contains("duration") ||
             !columnsRow1.names.contains("station") || !columnsRow2.names.contains("station") )
        {
            helper.error("Did not find all required columns",
                         journeyRow1.contents + "  -  " + journeyRow2.contents);
            continue;
        }

        // Initialize result variables with defaults
        var journey = { TypesOfVehicleInJourney: new Array, JourneyNews: '', RouteStops: new Array,
                        RoutePlatformsDeparture: new Array, RoutePlatformsArrival: new Array,
                        RouteTypesOfVehicles: new Array, RouteTransportLines: new Array,
                        RouteTimesDeparture: new Array, RouteTimesArrival: new Array,
                        RouteTimesDepartureDelay: new Array, RouteTimesArrivalDelay: new Array };

        // Parse start/target stop column
        if ( !(startStopName = startStopNameRegExp.exec(columnsRow1["station"].contents)) ) {
            helper.error("Unexcepted string in start stop column", columnsRow1["station"].contents);
            continue;
        }
        journey.StartStopName = startStopName[1];
        journey.TargetStopName = helper.trim( columnsRow2["station"] );

        // Parse departure/arrival date column TODO
//         journey.DepartureDateTime = helper.matchDate( columns["dateRow1"], "dd.MM.yy" );
//         journey.ArrivalDateTime = helper.matchDate( columns["dateRow2"], "dd.MM.yy" );
        if ( !(departureDate = dateRegExp.exec(columnsRow1["date"].contents)) ) {
            helper.error("Unexcepted string in departure date column", columnsRow1["date"].contents);
            continue;
        }
        var departureYear = parseInt( departureDate[3] );
        journey.DepartureDate = new Date( departureYear < 100 ? departureYear + 2000 : departureYear,
                                          parseInt(departureDate[2]) - 1, // month is zero based
                                          parseInt(departureDate[1]) );

        if ( !(arrivalDate = dateRegExp.exec(columnsRow2["date"].contents)) ) {
            helper.error("Unexcepted string in arrival date column", columnsRow2["date"].contents);
            continue;
        }
        var arrivalYear = parseInt( arrivalDate[3] );
        journey.ArrivalDate = new Date( arrivalYear < 100 ? arrivalYear + 2000 : arrivalYear,
                                        parseInt(arrivalDate[2]) - 1, // month is zero based
                                        parseInt(arrivalDate[1]) );

        // Parse departure time column // TODO parse delay
//         var departureTime = columns["timeRow1"];
//         var pos = departureTime.indexOf("\n");
//         if ( pos > 0 ) {
//             // Cut until line break
//             departureTime = departureTime.substr( 0, pos );
//         }
//         journey.DepartureTime = QTime.fromString( helper.trim(departureTime), "hh:mm" );
        if ( !(departureTime = timeRegExp.exec(columnsRow1["time"].contents)) ) {
//         if ( !journey.DepartureTime.isValid() ) {
            helper.error("Unexcepted string in departure time column", columnsRow1["time"].contents);
            continue;
        }
        journey.DepartureTime = new QTime( departureTime[1], departureTime[2] );

        if ( !(arrivalTime = timeRegExp.exec(columnsRow2["time"].contents)) ) {
//         if ( !journey.DepartureTime.isValid() ) {
            helper.error("Unexcepted string in arrival time column", columnsRow2["time"].contents);
            continue;
        }
        journey.ArrivalTime = new QTime( arrivalTime[1], arrivalTime[2] );

//         // Parse arrival time column // TODO parse delay
//         var arrivalTime = columns["timeRow2"];
//         var pos = arrivalTime.indexOf("\n");
//         if ( pos > 0 ) {
//             // Cut until line break
//             arrivalTime = arrivalTime.substr( 0, pos );
//         }
//         journey.ArrivalTime = QTime.fromString( helper.trim(arrivalTime), "hh:mm" );
//         if ( !journey.ArrivalTime.isValid() ) {
//             helper.error("Unexcepted string in arrival time column", columns["timeRow2"]);
//             continue;
//         }

        // Parse duration column
        if ( columnsRow1.names.contains("duration") ) {
            if ( (duration = timeRegExp.exec(columnsRow1["duration"].contents)) ) {
                // Duration is a string in format "hh:mm"
                journey.Duration = duration[1] * 60 + duration[2];
            }
        }

        // Parse types of vehicles (products) column
        if ( columnsRow1.names.contains("products") ) {
            var types = vehicleTypesRegExp.exec( columnsRow1["products"].contents );
            if ( types ) {
                types = types[1];
                types = types.split( /,/i );
                for ( var n = 0; n < types.length; ++n ) {
                    journey.TypesOfVehicleInJourney.push( helper.trim(types[n]) );
                }
            } else {
                // This is changed, test it TODO
                types = helper.stripTags( columnsRow1["products"].contents ).split( /,/i );
                for ( var n = 0; n < types.length; ++n ) {
                    journey.TypesOfVehicleInJourney.push( helper.trim(types[n]) );
                }
            }
        }

        // Parse changes column
        journey.Changes = parseInt( helper.trim(columnsRow1["changes"].contents) );

        // Parse pricing column
        if ( columnsRow1.names.contains("fareStd") ) {
            journey.Pricing = helper.trim( helper.stripTags(columnsRow1["fareStd"].contents) );
            journey.Pricing = journey.Pricing.replace( /(Zur&nbsp;Buchung|Zur&nbsp;Reservierung|\(Res. Info\)|Preisauskunft nicht m&#246;glich)/ig, '' );
        }

        // Parse details if available
        var str = journeyBlockContents.substr( journeyRow2.endPosition );
        var details = journeyDetailsRegExp.exec( str );
        // Make sure that the details block belongs to the just parsed journey
        var beginOfNextJourney = str.search( nextJourneyBeginRegExp );
        if ( details &&
             (beginOfNextJourney < 0 || journeyDetailsRegExp.lastIndex <= beginOfNextJourney) )
        {
            parseJourneyDetails( details[1], journey );
        }
        journeyDetailsRegExp.lastIndex = 0; // start from beginning next time

        // Add journey
        result.addData( journey );
    }
}

function parseJourneyDetails( details, resultObject ) {
    var journeyNews = null;
    var journeyNewsRegExp = /<tr>\s*?<td colspan="8" class="red bold"[^>]*?>([\s\S]*?)<\/td>\s*?<\/tr>/i;
    if ( (journeyNews = journeyNewsRegExp.exec(details)) != null ) {
        journeyNews = helper.trim( journeyNews[1] );
        if ( journeyNews != '' ) {
            resultObject.JourneyNews = journeyNews;
        }
    }

    var headerEndRegExp = /<tr>\s*?<th>[\s\S]*?<\/tr>/ig;
    if ( !headerEndRegExp.exec(details) ) {
        return;
    }
    var str = details.substr( headerEndRegExp.lastIndex );

    var lastStop = null, lastArrivalTime = '';
    var detailsBlockRegExp = /<tr>([\s\S]*?)<\/tr>\s*?<tr class="last">([\s\S]*?)<\/tr>/ig;
    var columnsContentsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
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
            if ( resultObject.JourneyNews == null )
                resultObject.JourneyNews = infoText;
            else
                resultObject.JourneyNews += " " + infoText;

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
        if ( timeArrival )
            timeArrival = timeArrival[1];
        else {
            var timeArrivalParts = helper.matchTime( columnsRow2[3], "hh:mm" );
            var timeDepartureParts = helper.matchTime( timeDeparture, "hh:mm" );

            if ( !timeArrivalParts.error && !timeDepartureParts.error ) {
                var durationPart = /(\d*?) Min./i.exec( columnsRow1[6] );
                if ( durationPart && parseInt(durationPart[1]) > 0 ) {
                    timeArrival = helper.addMinsToTime( timeDeparture,
                            parseInt(durationPart[1]), "hh:mm" );
                }
            }
        }

        resultObject.RouteStops.push( routeStop );
        resultObject.RoutePlatformsDeparture.push( routePlatformDeparture );
        resultObject.RoutePlatformsArrival.push( routePlatformArrival );
        resultObject.RouteTypesOfVehicles.push( routeVehicleType );
        resultObject.RouteTransportLines.push( routeTransportLine );
        resultObject.RouteTimesDeparture.push( timeDeparture );
        resultObject.RouteTimesDepartureDelay.push( delayDeparture );
        resultObject.RouteTimesArrival.push( timeArrival );
        resultObject.RouteTimesArrivalDelay.push( delayArrival );

        lastStop = columnsRow2[0];
        lastArrivalTime = timeArrival;
    }

    if ( lastStop != null ) {
        resultObject.RouteStops.push( helper.trim(lastStop) );
    }
    return true;
}
