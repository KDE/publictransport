/** Service provider www.sbb.ch (switzerland).
 * © 2012, Friedrich Pülz */

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
    return [ 'Arrivals', 'Delay', 'DelayReason', 'Platform', 'JourneyNews', 'TypeOfVehicle',
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

function getTimetable( values ) {
    var url = "http://fahrplan.sbb.ch/bin/bhftafel.exe/dn?" +
            "input=" + values.stop + "!" +
            "&boardType=" + (values.dataType == "arrivals" ? "arr" : "dep") +
            "&time=" + values.dateTime.getHours() + ":" + values.dateTime.getMinutes() +
            "&disableEquivs=no&maxJourneys=" + values.maxCount + "&start=yes";

    var request = network.createRequest( url );
//     request.readyRead.connect( parseTimetableIterative );
    request.finished.connect( parseTimetable );
    network.get( request );
}

function getJourneys( values ) {
    var url = "http://fahrplan.sbb.ch/bin/query.exe/dn" +
            "?S=" + values.originStop + "!" +
            "&Z=" + values.targetStop + "!" +
            "&date=" + helper.formatDateTime(values.dateTime, "dd.MM.yy") +
            "&time=" + helper.formatDateTime(values.dateTime, "hh:mm") +
            "&maxJourneys=" + values.maxCount +
            "&REQ0HafasSearchForw=" + (values.dataType == "arrivals" ? "0" : "1") +
            "&start=yes&sortConnections=minDeparture&jumpToDetails=yes&HWAI=JS!ajax=yes!&" +
            "HWAI=CONNECTION$C0-0!id=C0-0!HwaiConId=C0-0!HwaiDetailStatus=details!HwaiConnectionNumber=!;" +
            "CONNECTION$C0-1!id=C0-1!HwaiConId=C0-1!HwaiDetailStatus=details!HwaiConnectionNumber=!;" +
            "CONNECTION$C0-2!id=C0-2!HwaiConId=C0-2!HwaiDetailStatus=details!HwaiConnectionNumber=!;" +
            "CONNECTION$C0-3!id=C0-3!HwaiConId=C0-3!HwaiDetailStatus=details!HwaiConnectionNumber=!;" +
            "CONNECTION$C0-0!id=C0-0!HwaiConId=C0-0!HwaiDetailStatus=details!HwaiConnectionNumber=1!;" +
            "CONNECTION$C0-1!id=C0-1!HwaiConId=C0-1!HwaiDetailStatus=details!HwaiConnectionNumber=2!;" +
            "CONNECTION$C0-2!id=C0-2!HwaiConId=C0-2!HwaiDetailStatus=details!HwaiConnectionNumber=3!;" +
            "CONNECTION$C0-3!id=C0-3!HwaiConId=C0-3!HwaiDetailStatus=details!HwaiConnectionNumber=4!;" +
            "CONNECTION$C1-0!id=C1-0!HwaiConId=C1-0!HwaiDetailStatus=details!HwaiConnectionNumber=5!;" +
            "CONNECTION$C1-1!id=C1-1!HwaiConId=C1-1!HwaiDetailStatus=details!HwaiConnectionNumber=6!;" +
            "CONNECTION$C1-2!id=C1-2!HwaiConId=C1-2!HwaiDetailStatus=details!HwaiConnectionNumber=7!;" +
            "CONNECTION$C2-0!id=C2-0!HwaiConId=C2-0!HwaiDetailStatus=details!HwaiConnectionNumber=8!;" +
            "CONNECTION$C2-1!id=C2-1!HwaiConId=C2-1!HwaiDetailStatus=details!HwaiConnectionNumber=9!;" +
            "CONNECTION$C2-2!id=C2-2!HwaiConId=C2-2!HwaiDetailStatus=details!HwaiConnectionNumber=10!;";

    // Get three journey documents
    for ( i = 0; i < 3; ++i ) {
        // Download and parse journey document
        var html = network.getSynchronous( url );
        if ( network.lastDownloadAborted ) {
            break;
        }
        parseJourneys( html );

        // Directly publish journeys parsed from the current document
        result.publish();

        // Search for another URL to a document with later journeys
        var urlRegExp = /<a .*?href="([^"]*?)"[^>]*?>Sp&#228;tere Verbindungen[^<]*?<\/a>/i;
        var urlLater = urlRegExp.exec( html );
        if ( urlLater ) {
            // Set next download URL, need to decode "&amp;" to "&"
            url = urlLater[1].replace( /&amp;/gi, "&" );
            print( "Next journey URL: " + url );
        } else {
            // No URL to later journeys found
            if ( i == 0 ) {
                print( "No URL to later journeys found" );
                print( html );
            }
            break;
        }
    }
}

function typeOfVehicleFromString( string ) {
    string = string.toLowerCase();
    if ( string == "s" ) {
        return "interurbantrain";
    } else if ( string == "u" ) {
        return "subway";
    } else if ( string == "nbu" ) {
        return "bus";
    } else if ( string == "ntr" || string == "tra" ) {
        return "tram";
    } else if ( string == "ice" || string == "rj" ) { // RailJet
        return "highspeedtrain";
    } else if ( string == "oic" || string == "ic" ||
		 string == "en" ||  // EuroNight
		 string == "ec" || // EuroCity
		 string == "cnl" ) { // City Night Line
        return "intercitytrain";
    } else if ( string == "r" ) {
        return "regionaltrain";
    } else if ( string == "re" || string == "rexpresstrain" ||
	         string == "wb" || string == "ire" )
    {
        return "regionalexpresstrain";
    } else if ( string == "d" ) {
        return "interregionaltrain";
    } else {
        return string;
    }
}

function parseTimetable( html ) {
    var returnValue = new Array;
    // Dates are set from today, not the requested date. They need to be adjusted by X days,
    // where X is the difference in days between today and the requested date.
    returnValue.push( 'dates need adjustment' );

    // Find block of departures
    var str = /<table [^>]*class="hfs_stboard"[^>]*>([\s\S]*?)<\/table>/i.exec(html);
    if ( str == null ) {
        helper.error( "The table containing departures wasn't found!", html );
        return;
    }
    str = str[1];

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
        } else if ( colMeaning == "In Richtung" || colMeaning == "Aus Richtung" ) {
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

    var departuresRegExp = /<tr class="zebra-row-\d">([\s\S]*?)<\/tr>(?:\s*<tr class="zebra-row-\d">[\s\S]*?<div [^>]*?class="messageHolder"[^>]*>[\s\S]*?<div [^>]*?class="text"[^>]*>\s*<span [^>]*?class="bold">([\s\S]*?)<\/span>[\s\S]*?<span [^>]*?class="him">([\s\S]*?)<\/span>[\s\S]*?<\/tr>)?/ig;
    var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
    var typeOfVehicleRegExp = /<img [^>]*src="[^"]*?\/products\/([^_]*?)\d?_pic\.\w{3,4}"/i;
    var transportLineRegExp = /<a href="http:\/\/fahrplan.sbb.ch\/bin\/traininfo.exe[^>]*?>\s*(?:<img[^>]*>\s*)?(?:<span[^>]*>\s*)?([^<]*?)(?:<\/span>\s*)?<\/a>/i;
    var targetRegExp = /<a href="http:\/\/fahrplan.sbb.ch[^>]*?>([^<]*?)<\/a>\s*<\/span>[\s\S]*?<br>/i;
    var platformRegExp = /<strong>([^<]*?)<\/strong>/i;
    var delayRegExp = /ca.&nbsp;\+(\d+)&nbsp;Min\./i;
    var routeBlocksRegExp = /\r?\n\s*(-|&#149;)\s*\r?\n/ig;
    var routeBlockEndOfExactRouteMarkerRegExp = /&#149;/;
    var routeStopRegExp = /<a href="[^"]*?">([^<]*?)<\/a>&nbsp;(\d{2}:\d{2})/i;

    // Go through all departure blocks
    var dateTime = new Date();
    while ( (departureArr = departuresRegExp.exec(str)) ) {
        departureBlock = departureArr[1];

        // Get column contents
        var columns = new Array;
        while ( (col = columnsRegExp.exec(departureBlock)) ) {
            columns.push( col[1] );
        }

        if ( columns.length < 3 ) {
            helper.error("Too less columns (" + columns.length + ") found in a departure row!",
                         departureBlock);
            continue; // Not all columns where not found
        }

        // Initialize result variables with defaults
        var departure = { RouteStops: new Array, RouteTimes: new Array, RouteExactStops: 0 };

        // Parse time column
        var timeString = helper.trim( helper.stripTags(columns[timeCol]) );
        var time = helper.matchTime( timeString, "hh:mm" );
        if ( time.error ) {
            helper.error("Unexpected string in time column!", columns[timeCol]);
            continue;
        }
        dateTime.setHours( time.hour, time.minute, 0 );
        departure.DepartureDateTime = dateTime;

        // Parse type of vehicle column
        var typeOfVehicle = typeOfVehicleRegExp.exec( columns[typeOfVehicleCol] );
        if ( typeOfVehicle == null ) {
            helper.error("Unexpected string in type of vehicle column!", columns[typeOfVehicleCol]);
            continue;
        }
        departure.TypeOfVehicle = typeOfVehicleFromString( helper.trim(typeOfVehicle[1]) );

        // Parse transport line column
        var transportLine = transportLineRegExp.exec( columns[typeOfVehicleCol] );
        if ( transportLine == null ) {
            helper.error("Unexpected string in transport line column!", columns[typeOfVehicleCol]);
            continue;
        }
        departure.TransportLine = helper.trim( transportLine[1] );

        // Parse target column
        var targetString = targetRegExp.exec( columns[targetCol] );
        if ( targetString == null ) {
            helper.error("Unexpected string in target column!", columns[targetCol]);
            continue;
        }
        departure.Target = helper.trim( targetString[1] );

        // Parse platform column
        if ( platformCol != -1 ) {
            departure.Platform = helper.trim( columns[platformCol] );
        }

        // Parse delay column
        if ( delayCol != -1 ) {
            var delayString = helper.trim( helper.stripTags(columns[delayCol]) );
            if ( delayString.length != 0 ) {
                if ( (delayArr = delayRegExp.exec(delayString)) ) {
                    departure.Delay = parseInt( delayArr[1] );
                    print("parsed delay: " + departure.Delay + " minutes");
                } else {
                    var prognosisTime = helper.matchTime( delayString, "hh:mm" );
                    if ( prognosisTime.length == 2 ) {
                        // Prognosis time found
                        var delayResult = helper.duration( timeString,
                                helper.formatTime(prognosisTime[0], prognosisTime[1], "hh:mm"),
                                "hh:mm" );
                        if ( delayResult < 0 ) {
                            helper.error("Unexpected string in prognosis column!", columns[delayCol]);
                        } else {
                            print("parsed delay: " + delayResult + " minutes");
                            departure.Delay = delayResult;
                        }
                    } else {
                        departure.JourneyNews = delayString;
                    }
                }
            } else {
                departure.JourneyNews = delayString;
            }
        }

        var delayReason = "";
        if ( departureArr.length >= 4 && departureArr[2] && departureArr[3] ) {
            departure.DelayReason = helper.trim( departureArr[2].replace("&nbsp;", ' ') );
            var _journeyNews = helper.trim( departureArr[3].replace("&nbsp;", ' ') );
            print("Additional information: " + departure.DelayReason + ", " + _journeyNews);
            if ( _journeyNews.length > 0 ) {
                if ( departure.JourneyNews.length > 0 ) {
                    departure.JourneyNews += "<br/>";
                }
                departure.JourneyNews += _journeyNews;
            }
        }

        // Parse route from target column
        var routeStops = new Array;
        var routeTimes = new Array;
        var exactRouteStops = 0;
        var route = columns[targetCol].substr( targetRegExp.lastIndex );
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
            var routeStopArr = routeStopRegExp.exec( routeBlocks[n] );
            if ( routeStopArr == null ) {
                continue;
            }

            departure.RouteStops.push( helper.trim(routeStopArr[1]) );
            departure.RouteTimes.push( helper.trim(routeStopArr[2]) );
        }

        if ( departure.RouteStops.length == 0 ) {
            helper.error("Did not find any route stops", routeBlocks[n]);
        }

        // Add departure
        result.addData( departure );
    }

    return returnValue;
}

function parseJourneyDetails( details, resultObject ) {
    // 	    var exactRouteStops = 0;
    // Initialize regular expressions (only used once)
    var meaningsRegExp = /<tr>[\s\S]*?(<th[\s\S]*?)<\/tr>/ig;
    var columnMeaningsRegExp = /<th [^>]*?id="([^-]+)-[^\"]*"[^>]*?>[\s\S]*?<\/th>/ig;

    if ( (meanings = meaningsRegExp.exec(details)) == null ) {
        helper.error("Header row of details table not found !", details);
        return;
    }
    meanings = meanings[1];
//     print(" meaningsRegExp.lastIndex = " + meaningsRegExp.lastIndex);
//     print(" \n\n meanings: " + meanings + "\n\n");
    details = details.substr( meaningsRegExp.lastIndex ); // Cut the header row

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
    var detailsBlockRegExp = /<tr[^>]*?>([\s\S]*?)<\/tr>\s*?<tr[^>]*?>([\s\S]*?)<\/tr>/ig;
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
            routeVehicleType = typeOfVehicleFromString( helper.trim(routeVehicleType[1]) );
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
	if ( routeVehicleType != "" ) {
	    resultObject.routeVehicleTypes.push( routeVehicleType );
	}
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
    // Find block of journeys, may fail if there are multiple tables with the class "hfs_overview".
    // Don't extract the string until the next closing </table> tag, because the table itself
    // contains multiple other tables. Extract until some uniquely identifyable element after
    // the journey table (<div class="legend">).
    var str = /<table [^>]*class="hfs_overview"[^>]*>([\s\S]*?)<div class="legend">/i.exec(html);
    if ( str == null ) {
        helper.error( "The table containing journeys wasn't found!", html );
        return;
    }

    var pos = html.indexOf( '<table summary="Details - Verbindung 1' );
    var detailsHtml = pos != -1 ? html.substr( pos ) : '';

// 	print("PARSE JOURNEYS, total: " + html.length + ", length: " + str.length + ", details length: " + detailsHtml.length);

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
        } else if ( colMeaning == "products" ) {
            typesOfVehicleCol = i;
        } else if ( colMeaning == "location" ) {
            if ( colspan == 2 ) {
                // The location column has colspan=2, but the second column contains the stop name
                locationCol = i + 1;
            } else {
                print( "The website layout has changed, may not work correctly any longer. " +
                       "The 'location' column has colspan!=2, expected is colspan==2." );
                locationCol = i;
            }
        } else if ( colMeaning == "changes" ) {
            changesCol = i;
        } else if ( colMeaning == "duration" ) {
            durationCol = i;
        } else if ( colMeaning == "capacity" ) {
            capacityCol = i;
        }

        i += colspan;
    }

    if ( timeCol == -1 || typesOfVehicleCol == -1 || durationCol == -1 || locationCol == -1 ) {
        helper.error("Required columns not found (time, duration, types of vehicle, location)",
                meanings);
        return;
    }

    // Initialize regular expressions (compile them only once)
    var journeysRegExp = /<tr [^>]*id="trOverviewDep([^\"]*)"[^>]*>([\s\S]*?)<\/tr>[\s\S]*?<tr [^>]*id="trOverviewArr[^\"]*"[^>]*>([\s\S]*?)<\/tr>/ig
    var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
    var dateRegExp = /([0-9]{2}\.[0-9]{2}\.[0-9]{2})/i;
    var durationRegExp = /(\d{1,2}:\d{2})/i;
    // 	var journeyDetailsRegExp = /<div id="[^"]*?" class="detailContainer[^"]*?">\s*?<table class="result">([\s\S]*?)<\/table>/ig;

    // Go through all journey blocks
    var dateTime = new Date();
    while ( (journeyResult = journeysRegExp.exec(str)) ) {
        journeyID = journeyResult[1]; // Can be used to get the Details element (id="tr{journeyID}")
        journeyRow1 = journeyResult[2];
        journeyRow2 = journeyResult[3];
//         print( "Parse journey " + journeyID );

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

        // Initialize result variable with defaults
        var journey = { TypesOfVehicleInJourney: new Array };

        // Parse location column
        journey.StartStopName = helper.trim( helper.stripTags(columnsRow1[locationCol]) );
        journey.TargetStopName = helper.trim( helper.stripTags(columnsRow2[locationCol - 1]) );

        // Parse departure date column
        var departureDate = dateRegExp.exec( columnsRow1[dateCol] );
        if ( departureDate == null ) {
            helper.error("Unexpected string in departure date column!", columnsRow1[dateCol]);
            continue;
        }
        departureDate = helper.matchDate( departureDate, "dd.MM.yy" );
//         journey.DepartureDate = departureDate[1] + "";

        // Parse departure time column
        var departureTime = helper.matchTime( columnsRow1[timeCol + 1], "hh:mm" );
        if ( departureTime.error ) {
            helper.error("Unexpected string in departure time column!", columnsRow1[timeCol + 1]);
            continue;
        }
//         dateTime.setHours( departureTime.hour, departureTime.minute, 0 );
//         journey.DepartureTime = departureTime.hour + ":" + departureTime.minute;
        journey.DepartureDateTime = new Date( // TODO
                departureDate.getFullYear(), departureDate.getMonth(), departureDate.getDate(),
                departureTime.hour, departureTime.minute, 0 );

        // Parse arrival date column
        var arrivalDate = dateRegExp.exec( columnsRow2[dateCol - 1] );
        if ( !arrivalDate ) {
            arrivalDate = departureDate;
        } else {
            arrivalDate = helper.matchDate( arrivalDate[1], "dd.MM.yy" );
        }

        // Parse arrival time column
        var arrivalTime = helper.matchTime( columnsRow2[timeCol], "hh:mm" );
        if ( arrivalTime.error ) {
            helper.error("Unexpected string in arrival time column!", columnsRow2[timeCol]);
            continue;
        }
//         dateTime.setHours( arrivalTime.hour, arrivalTime.minute, 0 );
//         journey.ArrivalDateTime = dateTime;
//         journey.ArrivalTime = arrivalTime.hour + ":" + arrivalTime.minute;
        journey.ArrivalDateTime = new Date( // TODO
                arrivalDate.getFullYear(), arrivalDate.getMonth(), arrivalDate.getDate(),
                arrivalTime.hour, arrivalTime.minute, 0 );

        // Parse duration column
        var duration = null;
        duration = durationRegExp.exec( columnsRow1[durationCol] );
        if ( duration != null ) {
            journey.Duration = duration[1];
        }

        // Parse types of vehicles (products) column
        var types = helper.trim( columnsRow1[typesOfVehicleCol] );
        types = types.split( /,/i );
        for ( var n = 0; n < types.length; ++n ) {
	    var type = typeOfVehicleFromString( helper.trim(types[n]).replace(/\d$/, '') );
	    if ( type.length != 0 ) {
	      journey.TypesOfVehicleInJourney.push( type );
	    }
        }

        // Parse changes column
        journey.Changes = helper.trim( columnsRow1[changesCol] );

        // Parse details if available
        var detailsResults = new detailsResultsObject();
        if ( detailsCol != -1 ) {
            var detailsColRegExp = /<h2[^>]*>Verbindung\s+([0-9]+)<\/h2>/i;
            var res1 = detailsColRegExp.exec( columnsRow1[detailsCol] );
            if ( res1 != null ) {
                var journeyNrStr = res1[1];
                var journeyNrRegExp = /([0-9]+)/i;
                var res = journeyNrRegExp.exec( journeyNrStr );
                if ( res != null ) {
                    var journeyNr = parseInt( res[1] );
                    var detailsRegExp = new RegExp( "<table [^>]*summary=\"Details - Verbindung " +
                            journeyNr + "\"[^>]*>([\\s\\S]*?)</table>", "i" );
                    var details = detailsRegExp.exec( detailsHtml );
                    if ( details == null ) {
                        helper.error( "Details for journey item " + journeyNr + " not found!",
                                        detailsHtml );
                        continue;
                    }
                    details = details[1];

                    parseJourneyDetails( details, detailsResults );
                }

                if ( typeof(detailsResults.routeStops) != 'undefined'
                    && detailsResults.routeStops.length > 0 )
                {
                    if ( typeof(journey.StartStopName) != 'string'
                        || journey.StartStopName.length == 0 )
                    {
                        journey.StartStopName = detailsResults.routeStops[0];
                    }
                    if ( typeof(journey.TargetStopName) != 'string'
                        || journey.TargetStopName.length == 0 )
                    {
                        journey.TargetStopName = detailsResults.routeStops[
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

        journey.JourneyNews = detailsResults.journeyNews;
        journey.RouteStops = detailsResults.routeStops;
        journey.RoutePlatformsDeparture = detailsResults.routePlatformsDeparture;
        journey.RoutePlatformsArrival = detailsResults.routePlatformsArrival;
        journey.RouteTimesDeparture = detailsResults.routeTimesDeparture;
        journey.RouteTimesArrival = detailsResults.routeTimesArrival;
        journey.RouteTimesDepartureDelay = detailsResults.routeTimesDepartureDelay;
        journey.RouteTimesArrivalDelay = detailsResults.routeTimesArrivalDelay;
        journey.RouteTypesOfVehicles = detailsResults.routeVehicleTypes;
        journey.RouteTransportLines = detailsResults.routeTransportLines;

        // Add journey
        result.addData( journey );
    }
}

function getStopSuggestions( values  ) {
    var url = "http://fahrplan.sbb.ch/bin/ajax-getstop.exe/dn?" +
            "REQ0JourneyStopsS0A=1&REQ0JourneyStopsS0G=" + values.stop;
    var json = network.getSynchronous( url );

    if ( !network.lastDownloadAborted ) {
        // Find all stop suggestions
        var stopRegExp = /{"value":"([^"]*?)","id":"[^"]*?@L=([0-9]+)@[^"]*?"[^}]*?"weight":"(\d+)"[^}]*?}/ig;
        while ( (stop = stopRegExp.exec(json)) ) {
            result.addData( {StopName: stop[1], StopID: stop[2], StopWeight: stop[3]} );
        }
        return result.hasData();
    } else {
        return false;
    }
}
