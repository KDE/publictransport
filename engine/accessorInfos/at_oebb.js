/** Accessor oebb.at.
* © 2012, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'Arrivals', 'TypeOfVehicle', 'JourneyNews', 'Platform', 'Delay', 'StopID', 'StopWeight' ];
}

function getTimetable( values ) {
    var url = "http://fahrplan.oebb.at/bin/stboard.exe/dn?ld=oebb&sqView=2&start=Abfahrtstafel&" +
            "boardType=" + (values.dataType == "arrivals" ? "arr" : "dep") +
            "&time=" + values.dateTime.getHours() + ":" + values.dateTime.getMinutes() +
            "&maxJourneys=" + values.maxCount +
            "&dateBegin=&dateEnd=&selectDate=&productsFilter=0&editStation=yes&dirInput=&input=" +
            values.stop;

    var request = network.createRequest( url );
    request.finished.connect( parseTimetable );
    network.get( request );
}

function parseTimetable( html ) {
    // Find block of departures
    var str = helper.findFirstHtmlTag( html, "table", {attributes: {"class": "resultTable"}} );
    if ( !str.found ) {
        helper.error( "The table containing departures wasn't found!", html );
        return;
    }
    str = str.contents;

    if ( headers.error /*|| timestamp > headers.timestamp*/ ) {
        // Find column positions of specific headers using a helper function
        headers = helper.findTableHeaderPositions( str,
            { required: ["time", "product", "timetable"], // Names of required headers
              optional: ["prognosis", "platform"], // Names of optional headers
              headerOptions: { // Options of header tags (by default "th" tags inside "tr")
                attributes: {"class": ""}, // Header tags require a class attribute
                // Use first word in "class" attribute of the header tag as header name
                namePosition: {type: "attribute", name: "class", regexp: "([^\\s]*)(?:\\s+\\w+)?"}
              }
            });
        if ( headers.error ) {
            helper.error( "Did not find all required columns", str );
            return;
        }
    }

    // Initialize regular expressions (compile them only once)
    // Matches transport line [1] and type of vehicle [2]
    var typeOfVehicleAndTransportLineRegExp = /<img [^>]*src="\/img\/vs_oebb\/([^_]*)_pic[^"]*" [^>]*alt="([^"]*)"[^>]*>/i;
    var targetRegExp = /<strong>[\s\S]*?<a[^>]*?>([^<]*)<\/a>[\s\S]*?<\/strong>/i;
    var delayRegExp = /<span[^>]*?>ca.&nbsp;\+(\d+?)&nbsp;Min.&nbsp;<\/span>/i;
    var delayOnScheduleRegExp = /<span[^>]*?>p&#252;nktlich<\/span>/i;
    var routeBlocksRegExp = /\r?\n\s*(-|&#8226;)\s*\r?\n/gi;
    var routeBlockEndOfExactRouteMarker = "&#8226;";

    // This map gets used to replace matched vehicle type strings with strings the
    // PublicTransport data engine understands
    var vehicleTypeMap = { "nmg": "bus", "hog": "bus", "str": "tram", "ntr": "tram",
                           "s": "interurban", "r": "regional", "u": "subway",
                           "oec": "intercity", "oic": "intercity",
                           "rj": "highspeed train", // RailJet
                           "rex": "regional express", // local train stopping at few stations, semi fast
                           "ez": "regional express" // "ErlebnisZug", local train stopping at few stations, semi fast
                         };

    // Go through all departure rows, ie. <tr> tags
    var dateTime = new Date();
    var departureRow = { position: -1 };
    while ( (departureRow = helper.findFirstHtmlTag(str, "tr",
            { attributes: {"class": ".*"}, position: departureRow.position + 1 })).found )
    {
        // Find columns, ie. <td> tags in the current departure row
        var columns = helper.findHtmlTags( departureRow.contents, "td" );
        for ( var key in headers ) {
            if ( headers[key] >= columns.length ) {
                helper.error( "Column for header '" + key + "' at " + headers[key] +
                              " not found in current row with " + columns.length + " columns!",
                              departureRow.contents );
                columns = false;
                break;
            }
        }
        if ( !columns ) {
            continue;
        }

        // Initialize
        var departure = { RouteStops: new Array, RouteTimes: new Array, RouteExactStops: 0 };

        // Parse time column
        var time = helper.matchTime( columns[headers.time].contents, "hh:mm" );
        if ( time.error ) {
            helper.error( "Unexpected string in time column!", columns[headers.time].contents );
            continue;
        }
        dateTime.setHours( time.hour, time.minute, 0 );
        departure.DepartureDateTime = dateTime;

        // Parse type of vehicle column
        var info = typeOfVehicleAndTransportLineRegExp.exec( columns[headers.product].contents );
        if ( info == null ) {
            helper.error( "Unexpected string in type of vehicle column!", columns[headers.product].contents );
            continue;
        }
        departure.TransportLine = helper.trim( info[2] );

        // Use vehicle type match to lookup a correct vehicle type string in vehicleTypeMap
        var vehicle = helper.trim( info[1].toLowerCase() );
        departure.TypeOfVehicle = vehicleTypeMap[ vehicle ];
        if ( departure.TypeOfVehicle == undefined ||
             (typeof(departure.TypeOfVehicle) == 'string' && departure.TypeOfVehicle.length == 0) )
        {
            print( "Matched vehicle type string not handled in map: " + vehicle );
            departure.TypeOfVehicle = vehicle;
        }

        // Parse target column
        var target = targetRegExp.exec( columns[headers.timetable].contents );
        if ( target == null ) {
            helper.error( "Unexpected string in target column!", columns[headers.timetable].contents );
            continue;
        }
        departure.Target = helper.stripTags( target[1] );

        // Parse route from target column
        var route = columns[headers.timetable].contents;
        var posRoute = route.indexOf( "<br />" );
        if ( posRoute != -1 ) {
            route = route.substr( posRoute + 6 );
            var routeBlocks = route.split( routeBlocksRegExp );

            // Get the number of exact route stops by searching for the position of the
            // "end-of-route-marker"
            if ( route.indexOf(routeBlockEndOfExactRouteMarker) == -1 ) {
                departure.RouteExactStops = routeBlocks.length;
            } else {
                while ( (splitter = routeBlocksRegExp.exec(route)) ) {
                    ++departure.RouteExactStops;
                    if ( splitter.indexOf(routeBlockEndOfExactRouteMarker) != -1 )
                        break;
                }
            }

            // Go through all route blocks
            for ( var n = 0; n < routeBlocks.length; ++n ) {
                var lines = helper.splitSkipEmptyParts( routeBlocks[n], "\n" );
                if ( lines.count < 4 ) {
                    helper.error( "Too few lines (" + lines.count + ") in route string!", routeBlocks[n] );
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
        var journeyNews = helper.findFirstHtmlTag( columns[headers.timetable].contents, "div",
                {attributes: {"class": "journeyMessageHIM"}} );
        if ( journeyNews.found ) {
            departure.JourneyNews = helper.stripTags( journeyNews.contents );
        }

        // Parse platform from platform column
        if ( headers.platform != -1 ) {
            departure.Platform = helper.trim( columns[headers.platform].contents );
        }

        // Parse delay information from delay column
        if ( headers.prognosis != -1 ) {
            var delayDeparture = delayRegExp.exec( columns[headers.prognosis].contents );
            if ( delayDeparture != null ) {
                departure.Delay = parseInt( delayDeparture[1] );
            } else if ( delayOnScheduleRegExp.test(columns[headers.prognosis].contents) ) {
                departure.Delay = 0;
            } else {
                departure.Delay = -1;
            }
        }

        // Add departure
        result.addData( departure );
    }
}

function getStopSuggestions( values  ) {
    // Download suggestions and parse them using a regular expression
    var json = network.getSynchronous( "http://fahrplan.oebb.at/bin/ajax-getstop.exe/dn?" +
            "REQ0JourneyStopsS0A=1&REQ0JourneyStopsS0G=" + values.stop + "?" );
    if ( network.lastDownloadAborted() ) {
        return false;
    }

    // Find all stop suggestions in the JSON list of stops
    // Each element in the list is an object as matched by stopRegExp
    var stopRegExp = /\{"value":"([^"]*)","id":"[^"]*?@L=([^@]*)@[^"]*",.*?"weight":"([0-9]+)"[^\}]*\}/ig;
    while ( (stop = stopRegExp.exec(json)) ) {
        result.addData({ StopName: stop[1], StopID: stop[2], StopWeight: stop[3] });
    }
    return result.hasData();
}
