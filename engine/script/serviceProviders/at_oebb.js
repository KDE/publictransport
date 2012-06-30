/** Service provider oebb.at.
* © 2012, Friedrich Pülz */

Array.prototype.contains = function( element ) {
    for ( var i = 0; i < this.length; i++ ) {
        if ( this[i] == element )
            return true;
    }
    return false;
}

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


function typeOfVehicleFromString( string ) {
    string = string.toLowerCase();
    if ( string == "s" || string == "sbahn" ||
         string == "cat" ) // "City Airport Train", non-stop connection between airport "Wien-Schwechat" and the city centry of Wien
    {
	return "interurbantrain";
    } else if ( string == "str" || string == "ntr" || string == "lkb" ) {
	return "tram";
    } else if ( string == "nmg" || string == "nmp" || string == "hmp" ||
		 string == "hog" )
    {
	return "bus";
    } else if ( string == "u" ) {
	return "subway";
    } else if ( string == "rj" ) { // RailJet
	return "highspeedtrain";
    } else if ( string == "oic" || string == "oec" || string == "ec" ) { // Eurocity
	return "intercitytrain";
    } else if ( string == "r" ) {
	return "regionaltrain";
    } else if ( string == "rex" || string == "ez" ) { // "ErlebnisZug"
	return "regionalexpresstrain";
    } else if ( string == "ir" ||
	         string == "wb" ) // "WestBahn" (Wien – Salzburg – Freilassing)
    {
	return "interregionaltrain";
    } else {
	return string;
    }
}

function parseTimetable( html ) {
    // Find block of departures
    var str = helper.findFirstHtmlTag( html, "table", {attributes: {"class": "resultTable"}} );
    if ( !str.found ) {
        helper.error( "The table containing departures wasn't found!", html );
        return;
    }
    str = str.contents;

    // Initialize regular expressions (compile them only once)
    // Matches transport line [1] and type of vehicle [2]
    var typeOfVehicleAndTransportLineRegExp = /<img [^>]*src="\/img\/vs_oebb\/([^_]*)_pic[^"]*" [^>]*alt="([^"]*)"[^>]*>/i;
    var targetRegExp = /<strong>[\s\S]*?<a[^>]*?>([^<]*)<\/a>[\s\S]*?<\/strong>/i;
    var delayRegExp = /<span[^>]*?>ca.&nbsp;\+(\d+?)&nbsp;Min.&nbsp;<\/span>/i;
    var delayOnScheduleRegExp = /<span[^>]*?>p&#252;nktlich<\/span>/i;
    var routeBlocksRegExp = /\r?\n\s*(-|&#8226;)\s*\r?\n/gi;
    var routeBlockEndOfExactRouteMarker = "&#8226;";

    // Go through all departure rows, ie. <tr> tags
    var dateTime = new Date();
    var departureRow = { position: -1 };
    while ( (departureRow = helper.findFirstHtmlTag(str, "tr",
            { attributes: {"class": ".*"}, position: departureRow.position + 1 })).found )
    {
        // Find columns, ie. <td> tags in the current departure row
        var columns = helper.findNamedHtmlTags( departureRow.contents, "td",
                {attributes: {"class": ""}, ambiguousNameResolution: "addNumber",
                 namePosition: {type: "attribute", name: "class", regexp: "([^\\s]*)(?:\\s+\\w+)?"}} );

        // Check that all needed columns are found
        if ( !columns.names.contains("time") || !columns.names.contains("product") ||
             !columns.names.contains("timetable") )
        {
            if ( departureRow.contents.indexOf("<th") == -1 && columns.names.length != 0 ) {
                // There's only an error, if this isn't the header row of the table
                helper.error("Didn't find all columns in a row of the result table! " +
                             "Found: " + columns.names, departureRow.contents);
            }
            continue; // Not all columns where not found
        }

        // Initialize
        var departure = { RouteStops: new Array, RouteTimes: new Array, RouteExactStops: 0 };

        // Parse time column
        var time = helper.matchTime( columns["time"].contents, "hh:mm" );
        if ( time.error ) {
            helper.error( "Unexpected string in time column!", columns["time"].contents );
            continue;
        }
        dateTime.setHours( time.hour, time.minute, 0 );
        departure.DepartureDateTime = dateTime;

        // Parse type of vehicle column
        var info = typeOfVehicleAndTransportLineRegExp.exec( columns["product"].contents );
        if ( info == null ) {
            helper.error( "Unexpected string in type of vehicle column!", columns["product"].contents );
            continue;
        }
        departure.TransportLine = helper.trim( info[2] );

        // Use vehicle type match to lookup a correct vehicle type string in vehicleTypeMap
        var vehicle = helper.trim( info[1].toLowerCase() );
        departure.TypeOfVehicle = typeOfVehicleFromString( vehicle );

        // Parse target column
        var target = targetRegExp.exec( columns["timetable"].contents );
        if ( target == null ) {
            helper.error( "Unexpected string in target column!", columns["timetable"].contents );
            continue;
        }
        departure.Target = helper.stripTags( target[1] );

        // Parse route from target column
        var route = columns["timetable"].contents;
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
        var journeyNews = helper.findFirstHtmlTag( columns["timetable"].contents, "div",
                {attributes: {"class": "journeyMessageHIM"}} );
        if ( journeyNews.found ) {
            departure.JourneyNews = helper.stripTags( journeyNews.contents );
        }

        // Parse platform from platform column
        if ( columns.names.contains("platform") ) {
            departure.Platform = helper.trim( columns["platform"].contents );
        }

        // Parse delay information from delay column
        if ( columns.names.contains("prognosis") ) {
            var delayDeparture = delayRegExp.exec( columns["prognosis"].contents );
            if ( delayDeparture != null ) {
                departure.Delay = parseInt( delayDeparture[1] );
            } else if ( delayOnScheduleRegExp.test(columns["prognosis"].contents) ) {
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
    if ( network.lastDownloadAborted ) {
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
