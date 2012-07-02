/** Service provider www.fahrplaner.de (germany).
 * © 2011, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'StopID', 'StopWeight', 'Platform', 'Delay', 'JourneyNews' ];
}

function getTimetable( values ) {
    var url = "TODO";
    var request = network.createRequest( url );
    request.finished.connect( parseTimetable );
    network.get( request );
}

// This function parses a given HTML document for departure/arrival data.
function parseTimetable( html ) {
    // Find block of departures
//     var str = helper.extractBlock( html, '<table departure_table>', '</table>' );
    var str = html;

    var tableRegExp = /<table [^>]*class="hafasResult \s*fullwidth"[^>]*>([\s\S]*?)<\/table>/ig;
    var tableContent = tableRegExp.exec( str );
    if ( !tableContent ) {
        helper.error("Couldn't find results table", str);
        continue;
    }
    str = tableContent[1];

    var tableRowRegExp = /<tr[^>]*>([\s\S]*?)<\/tr>/ig;
    var tableColumnRegExp = /<td[^>]*>([\s\S]*?)<\/td>/ig;
    var delayRegExp = /<span [^>]*class="prognosis"[^>]*>[\s\S]*?<span [^>]*title="([0-9]{2}:[0-9]{2})"[^>]*>[^<]*<\/span>/ig;
    var targetRegExp = /<span [^>]*class="bold"[^>]*>([\s\S]*?)<\/span>/i;
    var routeBlocksRegExp = /\r?\n\s*(-|&#149;)\s*\r?\n/gi;
    var routeBlockEndOfExactRouteMarkerRegExp = /&#149;/i;

    // Go through all departure blocks
    while ( (departureRow = tableRowRegExp.exec(str)) ) {
        var columns = new Array();
        while ( (departureColumn = tableColumnRegExp.exec(departureRow[1])) ) {
            columns.push( departureColumn[1] );
        }
        tableColumnRegExp.lastIndex = 0; // Reset regexp

        if ( columns.length < 4 ) {
            helper.error("Too few columns (min. 4): " + columns.length, departureRow[1]);
            continue;
        }
        var colTime = 0, colDelay = 1, colVehicleType = 2, colTransportLine = 2,
            colTarget = 3, colRoute = 3, colPlatform = 4;

        // Parse time column
        var timeString = helper.trim( helper.stripTags(columns[colTime]) );
        time = helper.matchTime( timeString, "hh:mm" );
        if ( time.length != 2 ) {
            helper.error("Unexpected string in time column", columns[colTime]);
            continue;
        }

        // Parse delay
        delay = delayRegExp.exec( columns[colDelay] );
        if ( delay ) {
            delay = helper.duration( timeString, delay[1] );
        } else {
            delay = -1;
        }

        var vehicleTransportLineString = helper.trim( helper.stripTags(columns[colVehicleType]) );
        var vehicleTypeEnd = vehicleTransportLineString.indexOf(" ");
        if ( vehicleTypeEnd <= 0 ) {
            vehicleTypeEnd = vehicleTransportLineString.search(/\d/i);
        }
        var typeOfVehicle = vehicleTypeEnd <= 0 ? vehicleTransportLineString
                : vehicleTransportLineString.substring(0, vehicleTypeEnd);
        var transportLine = vehicleTypeEnd <= 0 ? vehicleTransportLineString
                : helper.trim(vehicleTransportLineString.substring(vehicleTypeEnd));
        var platform = "";
        if ( columns.length > colPlatform ) {
            platform = helper.trim( helper.stripTags(columns[colPlatform]) );
        }

        var route = helper.trim( columns[colTarget] );
        var targetString = targetRegExp.exec( route );
        if ( targetString ) {
            targetString = helper.trim( helper.stripTags(targetString[1]) );
        } else {
            targetString = "";
            helper.error("Unexpected string in target column", columns[colTarget]);
        }

        var posRoute = route.indexOf( "<br />" );
        var routeStops = new Array;
        var routeTimes = new Array;
        var exactRouteStops = 0;
        if ( posRoute != -1 && route.length > posRoute + 6 ) {
            route = route.substr( posRoute + 6 );
            var routeBlocks = route.split( routeBlocksRegExp );

            if ( !routeBlockEndOfExactRouteMarkerRegExp.test(route) ) {
                exactRouteStops = routeBlocks.length;
            } else {
                while ( (splitter = routeBlocksRegExp.exec(route)) ) {
                    ++exactRouteStops;
                    if ( routeBlockEndOfExactRouteMarkerRegExp.test(splitter) ) {
                        break;
                    }
                }
            }

            for ( var n = 0; n < routeBlocks.length; ++n ) {
                var lines = helper.splitSkipEmptyParts( routeBlocks[n], "\n" );

                if ( lines.count < 4 )
                    continue;

                routeStops.push( lines[1] );
                routeTimes.push( lines[3] );
            }
        }

        // Add departure to the result set
        timetableData.clear();
        timetableData.set( 'DepartureHour', time[0] );
        timetableData.set( 'DepartureMinute', time[1] );
        timetableData.set( 'TransportLine', transportLine );
        timetableData.set( 'TypeOfVehicle', typeOfVehicle );
        timetableData.set( 'Target', targetString );
//         timetableData.set( 'JourneyNews', journeyNews );
        timetableData.set( 'Delay', delay );
        timetableData.set( 'Platform', platform );
        timetableData.set( 'RouteStops', routeStops );
        timetableData.set( 'RouteTimes', routeTimes );
        timetableData.set( 'RouteExactStops', exactRouteStops );
        result.addData( timetableData );
    }
}

// This function parses a given HTML document for stop suggestions.
function parseStopSuggestions( html ) {
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

