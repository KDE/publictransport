/** Service provider hafas.bene-system.com (SNCB, B-Rail).
* © 2012, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'Arrivals', 'Platform',  'TypeOfVehicle', 'StopID', 'RouteStops' ];
}

function getTimetable( values ) {
    var url = "http://hafas.bene-system.com//bin/stboard.exe/dn?rt=1&L=b-rail" +
            "&input=" + values.stop + "!" +
            "&boardType=" + (values.dataType == "arrivals" ? "arr" : "dep") +
            "&date=" + helper.formatDateTime(values.dateTime, "dd.MM.yy") +
            "&time=" + helper.formatDateTime(values.dateTime, "hh:mm") +
            "&maxJourneys=" + values.maxCount +
            "&disableEquivs=no&start=yes&productsFilter=111111111";

    var request = network.createRequest( url );
    request.finished.connect( parseTimetable );
    network.get( request );
}

function typeOfVehicleFromString( string ) {
    if ( string == "s" || string == "dpn" ) {
	return "interurbantrain";
    } else if ( string == "str" ) {
	return "tram";
    } else if ( string == "u" ) {
	return "subway";
    } else if ( string == "ice" ) {
	return "highspeedtrain";
    } else if ( string == "ic" || string == "ict" || string == "ec" ) { // Eurocity
	return "intercitytrain";
    } else if ( string == "re" ) {
	return "regionalexpresstrain";
    } else if ( string == "ir" ) {
	return "interregionaltrain";
    } else {
	return string;
    }
}

function parseTimetable( html ) {
    // Initialize regular expressions (compile them only once)
    var targetRegExp = /<span class="bold">[^<]*?<a[^>]*?>([\s\S]*?)<\/a>/i;
    var routeBlocksRegExp = /\r?\n\s*(-|<img[^>]*?>)\s*\r?\n/gi;
    var routeBlockEndOfExactRouteMarkerRegExp = /<img[^>]*?>/i;

    // Go through all departure blocks
    var dateTime = new Date();
    var departureRow = { position: -1 };
    while ( (departureRow = helper.findFirstHtmlTag(html, "tr",
            { attributes: {"valign": "top", "bgcolor": "#.*"},
              position: departureRow.position + 1 })).found )
    {
        var columns = helper.findHtmlTags( departureRow.contents, "td" );

        if ( columns.length == 0 ) {
            helper.error("No column found in a departure row!", departureRow.contents);
            break; // Not all columns where not found
        } else if ( columns.length < 7 ) {
            helper.error("Didn't find all columns (" + columns.length + " found)!", departureRow.contents);
            break; // Not all columns where not found
        }

        // Initialize
        var departure = { RouteStops: new Array, RouteTimes: new Array, RouteExactStops: 0 };

        // Parse time column
        var time = helper.matchTime( helper.trim(helper.stripTags(columns[0].contents)), "hh:mm" );
        if ( time.error ) {
            helper.error("Unexpected string in time column!", columns[0].contents);
            continue;
        }
        dateTime.setHours( time.hour, time.minute, 0 );
        departure.DepartureDateTime = dateTime;

        // Parse type of vehicle column
        var typeOfVehicle = helper.findFirstHtmlTag(columns[2].contents, "img",
                    {noContent: true, attributes: {"src": "/hafas-res/img/(?:bs_)?([^_]+)_pic"}})
        if ( !typeOfVehicle.found ) {
            helper.error("Unexpected string in type of vehicle column!", columns[2].contents);
            continue;
        }
        var vehicle = typeOfVehicle.attributes["src"][1]; // First matched subexpression of value regexp for attribute "src"
        departure.TypeOfVehicle = typeOfVehicleFromString( vehicle );
        if ( departure.TypeOfVehicle == undefined ||
             (typeof(departure.TypeOfVehicle) == 'string' && departure.TypeOfVehicle.length == 0) )
        {
            print( "Matched vehicle type string not handled in map: " + vehicle );
            departure.TypeOfVehicle = vehicle;
        }

        // Parse transport line column
        departure.TransportLine = helper.trim( helper.stripTags(columns[4].contents) );

        // Parse route column ... target
        if ( (targetString = targetRegExp.exec(columns[6].contents)) == null ) {
            helper.error("Unexpected string in target column!", columns[6].contents);
            continue;
        }
        departure.Target = helper.trim( targetString[1] );

        // .. route
        var route = columns[6].contents;
        var posRoute = route.indexOf( "<br>" );
        if ( posRoute != -1 ) {
            route = route.substr( posRoute + 4 );
            var routeBlocks = route.split( routeBlocksRegExp );

            if ( !routeBlockEndOfExactRouteMarkerRegExp.test(route) )
                departure.RouteExactStops = routeBlocks.length;
            else {
                while ( (splitter = routeBlocksRegExp.exec(route)) ) {
                    ++departure.RouteExactStops;
                    if ( routeBlockEndOfExactRouteMarkerRegExp.test(splitter) )
                        break;
                }
            }

            for ( var n = 0; n < routeBlocks.length; ++n ) {
                var lines = helper.splitSkipEmptyParts( routeBlocks[n], "\n" );
                if ( lines.count < 4 ) {
                    helper.error( "Too few lines (" + lines.count + ") in route string!", routeBlocks[n] );
                    continue;
                }

                if ( typeof(lines[1]) != "undefined" && typeof(lines[3]) != "undefined" ) {
                    departure.RouteStops.push( lines[1] );
                    departure.RouteTimes.push( lines[3] );
                }
            }
        }

        // Parse platform column if any
        if ( columns.length >= 9 ) {
            departure.Platform = helper.trim( helper.stripTags(columns[8].contents) );
        }
/*
        // Parse delay column if any
        if ( columnMeanings.contains("ris") ) {
            if ( delayOnTimeRegExp.test(columns["ris"].contents) ) {
                delay = 0;
            } else if ( (delayArr = delayWithReasonRegExp.exec(columns["ris"].contents)) ) {
                delay = delayArr[1];
                delayReason = delayArr[2];
            } else if ( (delayArr = delayRegExp.exec(columns["ris"].contents)) ) {
                delay = delayArr[1];
            } else if ( trainCanceledRegExp.test(columns["ris"].contents) ) {
                var infoText = "Zug f&auml;llt aus.";
                if ( journeyNews == "" )
                        journeyNews = infoText;
                else
                        journeyNews += " " + infoText;
            }
        }*/

        // Add departure
        result.addData( departure );
    }

    return true;
}

function getStopSuggestions( values  ) {
    // Download suggestions and parse them using a regular expression
    var html = network.getSynchronous( "http://hafas.bene-system.com//bin/stboard.exe/dn?" +
            "rt=1&L=b-rail&input=" + values.stop + "?&disableEquivs=no&start=yes&productsFilter=111111111" );
    if ( network.lastDownloadAborted ) {
        return false;
    }

    // Find all stop suggestions in the HTML list of <option> tags
    var stop = { position: -1 };
    while ( (stop = helper.findFirstHtmlTag(html, "option",
            {attributes: {"value": "(.+)#([0-9]+)"}, position: stop.position + 1})).found )
    {
        result.addData( {StopName: stop.attributes["value"][1],
                         StopId: stop.attributes["value"][2]} );
    }
    return result.hasData();
}
