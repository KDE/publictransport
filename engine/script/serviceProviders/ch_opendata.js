/** Service provider plugin for transport.opendata.ch
  * © 2012, Friedrich Pülz */

var baseUrl = "http://transport.opendata.ch/v1/";

function features() {
    return [ PublicTransport.ProvidesStopID,
             PublicTransport.ProvidesStopPosition,
             PublicTransport.ProvidesStopsByGeoPosition];
}

function getTimetable( values ) {
    // Construct an URL from the given values
    // Cannot tell date or time nor can arrivals be requested
    var url = baseUrl + "stationboard" +
            "?station=" + values.stop + "!" + // TODO "&id=" + values.stopID
            "&limit=" + values.count;

    // Create and connect a NetworkRequest object for the URL
    var request = network.createRequest( url );
    request.finished.connect( parseTimetable );

    // Start the download, parseTimetable() will be called when it is finished
    network.get( request );
}

function parseTimetable( jsonDocument ) {
    // Decode the received QByteArray and parse it's JSON content
    jsonDocument = helper.decode( jsonDocument );
    var json = JSON.parse( jsonDocument );
    checkForErrors( json );

    for ( i = 0; i < json.stationboard.length; ++i ) {
        var jsonDeparture = json.stationboard[ i ];
        var departure = {
            TypeOfVehicle: vehicleFromCategory(jsonDeparture.category),
            TransportLine: jsonDeparture.number,
            Operator: jsonDeparture.operator,
            Target: jsonDeparture.to
        };

        var stop = jsonDeparture.stop;
        var departureInfo = getTimeInfo( stop.departure, stop.prognosis.departure );
        departure.DepartureDateTime = departureInfo.prognosis;
        departure.Delay = departureInfo.delay;

        var platform = getPlatformInfo( stop.platform, stop.prognosis.platform );
        if ( platform.prognosis != undefined ) {
            departure.Platform = platform.prognosis;
            departure.JourneyNews = appendText( departure.JourneyNews, platform.news );
        }

        result.addData( departure );
    }
}

function getStopSuggestions( values  ) {
    // Construct an URL from the given values
    var url = baseUrl + "locations";

    // Check if stops are requested from a geo position
    var byGeoPosition = values.stop == undefined &&
            values.longitude != undefined && values.latitude != undefined;
    url += byGeoPosition
            ? ("?x=" + values.latitude + "&y=" + values.longitude)
            : ("?query=" + values.stop);

    // Download the document synchronously and decode it
    var jsonDocument = network.getSynchronous( url );
    jsonDocument = helper.decode( jsonDocument );

    // Check if the download was completed successfully
    if ( !network.lastDownloadAborted ) {
        // Find all stop suggestions
        var json = JSON.parse( jsonDocument );
        checkForErrors( json );
        for ( i = 0; i < json.stations.length; ++i ) {
            // 'stop' contains these unused information:
            // stop.type: Type of the stop, 'station'/'poi'/'address'/'refine'
            // stop.distance: If requested from a geo position, in meters
            var stop = json.stations[ i ];
            result.addData( {StopName: stop.name, StopID: stop.id,
                             StopWeight: stop.score,
                             // This assumes stop.coordinate.type == "WGS84"
                             StopLongitude: stop.coordinate.x,
                             StopLatitude: stop.coordinate.y} );
        }
        return result.hasData();
    } else {
        return false;
    }
}

function getJourneys( values ) {
    // Construct an URL from the given values
    var url = baseUrl + "connections" +
            "?from=" + values.originStop +
            "&to=" + values.targetStop +
            "&date=" + helper.formatDateTime(values.dateTime, "yyyy-MM-dd") +
            "&time=" + helper.formatDateTime(values.dateTime, "hh:mm") +
            "&isArrivalTime=" + (values.dataType == "arrivals" ? "1" : "0") +
            "&limit=" + Math.min(6, values.count); // Can be 1-6

    // Create a NetworkRequest object for the URL
    var request = network.createRequest( url );

    // Connect to the finished signal,
    // an alternative is the readyRead signal to parse iteratively
    request.finished.connect( parseJourneys );

    // Start the download,
    // the parseTimetable() function will be called when it is finished
    network.get( request );
}

function parseJourneys( jsonDocument ) {
    // Decode the received QByteArray and parse it's JSON content
    jsonDocument = helper.decode( jsonDocument );
    var json = JSON.parse( jsonDocument );
    checkForErrors( json );

    for ( i = 0; i < json.connections.length; ++i ) {
        var connection = json.connections[ i ];
        var journey = {
            StartStopName: connection.from.station.name,
            TargetStopName: connection.to.station.name,
            TypesOfVehicleInJourney: vehiclesFromCategories(connection.products),
            Changes: connection.transfers, // Not in the API documentation
            RouteStops: new Array, RouteNews: new Array,
            RouteTimesDeparture: new Array, RouteTimesArrival: new Array,
            RouteTimesDepartureDelay: new Array, RouteTimesArrivalDelay: new Array,
            RoutePlatformsDeparture: new Array, RoutePlatformsArrival: new Array,
            RouteTypesOfVehicles: new Array, RouteTransportLines: new Array,
            RouteSubJourneys: new Array
        };

        // Get departure/arrival time and delay
        var departure = getTimeInfo( connection.from.departure,
                                     connection.from.prognosis.departure );
        var arrival = getTimeInfo( connection.to.arrival,
                                   connection.to.prognosis.arrival );
        journey.DepartureDateTime = departure.prognosis;
        journey.ArrivalDateTime = arrival.prognosis;
        journey.Delay = departure.delay;

        // Get departure/arrival platform and warn about changes in JourneyNews
        var departurePlatform = getPlatformInfo( connection.from.platform,
                                                 connection.from.prognosis.platform );
        var arrivalPlatform = getPlatformInfo( connection.to.platform,
                                               connection.to.prognosis.platform );
        if ( departurePlatform.prognosis != undefined ) {
            journey.JourneyNews = appendText( journey.JourneyNews, departurePlatform.news );
        }
        if ( arrivalPlatform.prognosis != undefined ) {
            journey.JourneyNews = appendText( journey.JourneyNews, arrivalPlatform.news );
        }

        // A 'section' is a 'sub journey'
        if ( connection.sections != undefined && connection.sections.length > 0 ) {
            // Found data about sub journeys
            // (the "sections" property is not mentioned in the API documentation)
            var section;
            for ( s = 0; s < connection.sections.length; ++s ) {
                section = connection.sections[ s ];
                journey.RouteStops.push( section.departure.station.name );

                var departure = getTimeInfo( section.departure.departure,
                                              section.departure.prognosis.departure );
                if ( departure.prognosis != undefined ) {
                    journey.RouteTimesDeparture.push( departure.prognosis );
                    journey.RouteTimesDepartureDelay.push( departure.delay );
                }

                var arrival = getTimeInfo( section.arrival.arrival,
                                            section.arrival.prognosis.arrival );
                if ( arrival.prognosis != undefined ) {
                    journey.RouteTimesArrival.push( arrival.prognosis );
                    journey.RouteTimesArrivalDelay.push( arrival.delay );
                }

                var departurePlatform = getPlatformInfo( section.departure.platform,
                        section.departure.prognosis.platform );
                var arrivalPlatform = getPlatformInfo( section.arrival.platform,
                        section.arrival.prognosis.platform );
                journey.RoutePlatformsDeparture.push( departurePlatform.prognosis );
                journey.RoutePlatformsArrival.push( arrivalPlatform.prognosis );

                var sectionVehicle = section.walk ? PublicTransport.Footway
                        : vehicleFromCategory(section.journey.category);
                journey.RouteTypesOfVehicles.push( sectionVehicle );
                journey.RouteTransportLines.push( section.journey ? section.journey.number : "" );

                if ( section.journey ) {
                    var intermediateStopCount = section.journey.passList.length - 2;
                    var subJourney = {
                        RouteStops: new Array(intermediateStopCount),
                        RouteNews: new Array(intermediateStopCount),
                        RoutePlatformsDeparture: new Array(intermediateStopCount),
                        RoutePlatformsArrival: new Array(intermediateStopCount),
                        RouteTimesDeparture: new Array(intermediateStopCount),
                        RouteTimesArrival: new Array(intermediateStopCount),
                        RouteTimesDepartureDelay: new Array(intermediateStopCount),
                        RouteTimesArrivalDelay: new Array(intermediateStopCount)
                    };

                    // Exclude first and last passed stops, only use intermediate stops here.
                    for ( j = 1; j < section.journey.passList.length - 1; ++j ) {
                        var pass = section.journey.passList[ j ];
                        var routeNews = "";
                        subJourney.RouteStops[ j - 1 ] = pass.station.name;

                        var passDeparture = getTimeInfo( pass.departure, pass.prognosis.departure );
                        var passArrival = getTimeInfo( pass.arrival, pass.prognosis.arrival );
                        subJourney.RouteTimesDeparture[ j - 1 ] = passDeparture.prognosis;
                        subJourney.RouteTimesDepartureDelay[ j - 1 ] = passDeparture.delay;
                        subJourney.RouteTimesArrival[ j - 1 ] = passArrival.prognosis;
                        subJourney.RouteTimesArrivalDelay[ j - 1 ] = passArrival.delay;

                        var passPlatform = getPlatformInfo( pass.platform, pass.prognosis.platform );
                        subJourney.RoutePlatformsDeparture[ j - 1 ] = passPlatform.prognosis;
                        subJourney.RoutePlatformsArrival[ j - 1 ] = passPlatform.prognosis;
                        routeNews = appendText( routeNews, passPlatform.news );

                        subJourney.RouteNews[ j - 1 ] = routeNews;
                    }
                    journey.RouteSubJourneys.push( subJourney );
                } else {
                    journey.RouteSubJourneys.push( {} );
                }
            }

            // Add last route stop (variable 'section' contains the last section now)
            journey.RouteStops.push( section.arrival.station.name );
        }

        journey.JourneyNews += appendText( journey.JourneyNews,
                                            connection.service.regular );
        journey.JourneyNews += appendText( journey.JourneyNews,
                                            connection.service.irregular );

        result.addData( journey );
    }
}

// Read time string in ISO date format as used by transport.opendata.ch
function readTime( timeString ) {
    return QDateTime.fromString( timeString + "", Qt.ISODate );
}

function getPlatformInfo( scheduledPlatform, prognosisPlatform ) {
    var platformInfo = { scheduled: scheduledPlatform,
                         prognosis: scheduledPlatform, news: "" };
    if ( typeof(prognosisPlatform) == 'string' && prognosisPlatform.length > 0 ) {
        platformInfo.prognosis = prognosisPlatform;
        if ( prognosisPlatform != scheduledPlatform &&
             typeof(scheduledPlatform) == 'string' && scheduledPlatform.length > 0 )
        {
            platformInfo.news = "Gleisänderung von " + scheduledPlatform +
                                " nach " + prognosisPlatform; // TODO i18n
        }
    }
    return platformInfo;
}

function getTimeInfo( scheduledTime, prognosisTime ) {
    scheduledTime = readTime( scheduledTime );
    var timeInfo = { scheduled: scheduledTime, prognosis: scheduledTime, delay: -1 };
    if ( prognosisTime != undefined ) {
        if ( prognosisTime != scheduledTime ) {
            timeInfo.prognosis = readTime( prognosisTime );
            timeInfo.delay = (timeInfo.prognosis.getTime() - scheduledTime.getTime()) / (1000 * 60);
        } else {
            // On schedule
            timeInfo.delay = 0;
        }
    }
    return timeInfo;
}

function checkForErrors( json ) {
    if ( json.errors != undefined && json.errors.length > 0 ) {
        throw Error( json.errors[0] );
    }
}

function appendText( text, appendText ) {
    if ( text == undefined ) {
        return appendText;
    } else if ( appendText == undefined ) {
        return text;
    }
    if ( text.length > 0 ) {
        text += ", ";
    }
    return text + appendText;
}

function vehiclesFromCategories( categories ) {
    var vehicles = [];
    for ( n = 0; n < categories.length; ++n ) {
        var vehicle = vehicleFromCategory( categories[n] );
        vehicles.push( vehicle );
    }
    return vehicles;
}

function vehicleFromCategory( category ) {
    category = category.toLowerCase();

    // Remove any numbers that are appended
    regExpResult = /([^\d]+)/.exec( category );
    if ( regExpResult ) {
        category = regExpResult[ 0 ];
    }

    switch ( category ) {
    case "bus":
    case "nfb": // "Niederflurbus"
        return PublicTransport.Bus;
    case "tro":
        return PublicTransport.TrolleyBus;
    case "tram":
        return PublicTransport.Tram;
    case "s":
        return PublicTransport.InterurbanTrain;
    case "u":
        return PublicTransport.Subway;
    case "ice": // InterCityExpress, Germany
    case "tgv": // Train à grande vitesse, France
    case "rj": // RailJet
        return PublicTransport.HighSpeedTrain;
    case "ic":
    case "icn": // "InterCity-Neigezug"
    case "ec": // EuroCity
        return PublicTransport.IntercityTrain;
    case "re":
        return PublicTransport.RegionalExpressTrain;
    case "rb":
    case "r":
        return PublicTransport.RegionalTrain;
    case "ir":
        return PublicTransport.InterregionalTrain;
    case "ferry":
        return PublicTransport.Ferry;
    case "feet":
        return PublicTransport.Feet;

    default:
        helper.warning( "Unknown vehicle type category: " + category );
        return PublicTransport.UnknownVehicleType;
    }
}
