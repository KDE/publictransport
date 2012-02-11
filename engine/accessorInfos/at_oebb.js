/** Accessor oebb.at.
* © 2012, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'TypeOfVehicle', 'JourneyNews', 'Platform', 'Delay', 'StopID', 'StopWeight' ];
}

function getTimetable( stop, dateTime, maxCount, dataType, city ) {
    var url = "http://fahrplan.oebb.at/bin/stboard.exe/dn?ld=oebb&sqView=2&start=Abfahrtstafel&" +
            "boardType=" + (dataType == "arrivals" ? "arr" : "dep") +
            "&time=" + dateTime.getHours() + ":" + dateTime.getMinutes() +
            "&maxJourneys=" + maxCount +
            "&dateBegin=&dateEnd=&selectDate=&productsFilter=0&editStation=yes&dirInput=&input=" +
            stop;

    var request = network.createRequest( url );
    request.finished.connect( parseTimetable );
    network.get( request );
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
    var dateTime = new Date();
    while ( (departure = departuresRegExp.exec(str)) ) {
        departure = departure[1];

        // Get column contents
        var columns = new Array;
        while ( (column = columnsRegExp.exec(departure)) ) {
            column = column[1];
            columns.push( column );
        }

        // Initialize
        var departure = { RouteStops: new Array, RouteTimes: new Array, RouteExactStops: 0 };

        // Parse time column
        var time = helper.matchTime( columns[timeCol], "hh:mm" );
        if ( time.error ) {
            helper.error( "Unexpected string in time column!", columns[timeCol] );
            continue;
        }
        dateTime.setHours( time.hour, time.minute, 0 );
        departure.DepartureDateTime = dateTime;

        // Parse type of vehicle column
        var info = typeOfVehicleAndTransportLineRegExp.exec( columns[typeOfVehicleCol] );
        if ( info == null ) {
            helper.error( "Unexpected string in type of vehicle column!", columns[typeOfVehicleCol] );
            continue;
        }
        departure.TransportLine = helper.trim( info[2] );

        var vehicle = info[1].toLowerCase();
        if ( vehicle == "rj" ) { // RailJet
            departure.TypeOfVehicle = "highspeed train";
        } else if ( vehicle == "oec" || vehicle == "oic" ) {
            departure.TypeOfVehicle = "intercity";
        } else if ( vehicle == "rex"   // local train stopping at few stations, semi fast
                  || vehicle == "ez" )  // "ErlebnisZug", local train stopping at few stations, semi fast
        {
            departure.TypeOfVehicle = "regional express";
        } else {
            departure.TypeOfVehicle = vehicle;
        }
        departure.TypeOfVehicle = typeOfVehicle;

        // Parse target column
        var target = targetRegExp.exec( columns[targetCol] );
        if ( target == null ) {
            helper.error( "Unexpected string in target column!", columns[targetCol] );
            continue;
        }
        departure.Target = helper.trim( target[1] );

        // Parse route from target column
        var route = columns[targetCol];
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
                var lines = helper.splitSkipEmptyParts( routeBlocks[n], "\n" );
                if ( lines.count < 4 ) {
                    helper.error( "Too less lines (" + lines.count + ") in route string!", routeBlocks[n] );
                    continue;
                }

                // lines[0] contains <a href="...">, lines[2] contains </a>
                if ( typeof(lines[1]) != "undefined" && typeof(lines[3]) != "undefined" ) {
                    departure.RouteStops.push( lines[1] );
                    departure.RouteTimes.push( lines[3] );
                }
            }
        }

        // Parse journey news from target column
        var journeyNewsMatches = journeyNewsRegExp.exec( columns[targetCol] );
        if ( journeyNewsMatches != null ) {
            departure.JourneyNews = helper.stripTags( journeyNewsMatches[1] );
        }

        // Parse platform from platform column
        if ( platformCol != -1 ) {
            departure.Platform = helper.trim( columns[platformCol] );
        }

        // Parse delay information from delay column
        if ( delayCol != -1 ) {
            var delayDeparture = delayRegExp.exec( columns[delayCol] );
            if ( delayDeparture != null ) {
                departure.Delay = parseInt( delayDeparture[1] );
            } else if ( delayOnScheduleRegExp.test(columns[delayCol]) ) {
                departure.Delay = 0;
            } else {
                departure.Delay = -1;
            }
        }

        // Add departure
        result.addData( departure );
    }
}

function getStopSuggestions( stop, maxCount, city  ) {
    var url = "http://fahrplan.oebb.at/bin/ajax-getstop.exe/dn?REQ0JourneyStopsS0A=1&"
            + "REQ0JourneyStopsS0G=" + stop + "?";

    // Download suggestions and parse them using a regular expression
    var json = network.getSynchronous( url );
    if ( !network.lastDownloadAborted() ) {
        // Find all stop suggestions
        var stopRegExp = /\{"value":"([^"]*)","id":"[^"]*?@L=([^@]*)@[^"]*",.*?"weight":"([0-9]+)"[^\}]*\}/ig;
        while ( (stop = stopRegExp.exec(json)) ) {
            result.addData({ StopName: stop[1], StopID: stop[2], StopWeight: stop[3] });
        }
        return result.hasData();
    } else {
        return false;
    }
}
