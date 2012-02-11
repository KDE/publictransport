/** Accessor for www.dvb.de (Dresden, germany).
* © 2010, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'TypeOfVehicle', 'StopID' ];
}

function parseTimetable( json ) {
    // Initialize regular expressions (compile them only once)
    var departureRegExp = /\["([^"]*)","([^"]*)","([^"]*)"\]/ig;

    // Go through all departures
    var now = new Date();
    while ( (departure = departureRegExp.exec(json)) ) {
        var minutes = parseInt( departure[3] );
        var timeString = helper.addMinsToTime( now.getHours() + ":" + now.getMinutes(), minutes,
                                        "h:mm" );
        var time = helper.matchTime( timeString, "hh:mm" );
        if ( time.length != 2 ) {
                helper.error("Unexpected string in time column!", departure[3]);
                continue;
        }

        var vehicleType = "Unknown";
        var line = departure[1];
        if ( line[0] == "S" ) {
                vehicleType = "InterurbanTrain";
        } else {
                var lineNr = parseInt( line );
                if ( lineNr > 0 ) {
                        vehicleType = lineNr <= 20 ? "Tram" : "Bus";
                }
        }

        // Add departure
        timetableData.clear();
        timetableData.set( 'TransportLine', departure[1] );
        timetableData.set( 'Target', departure[2] );
        timetableData.set( 'TypeOfVehicle', vehicleType );
        timetableData.set( 'DepartureHour', time[0] );
        timetableData.set( 'DepartureMinute', time[1] );
        result.addData( timetableData );
    }
}

function getStopSuggestions( stop, maxCount, city  ) {
    var timestamp = Math.round( (new Date()).getTime() / 1000 ); // in seconds since 1970-01-01...
    var url = "http://widgets.vvo-online.de/abfahrtsmonitor/Abfahrten.do?hst=" + stop
            + "&vz=0&time=" + timestamp + "&iso=1";
    var json = network.getSynchronous( url );

    if ( !network.lastDownloadAborted() ) {
        // Initialize regular expressions (compile them only once)
        var stopRangeRegExp = /\[\[\["[^"]*"\]\],\[(.*)\]\]$/i;
        var stopRegExp = /\["([^"]*)","[^"]*","([^"]*)"\]/ig;

        // Get range of all stops
        var range = stopRangeRegExp.exec( json );
        if ( range == null ) {
                helper.error("Stop range not found", json);
                return false;
        }
        range = range[1];

        // Go through all stops
        while ( (stop = stopRegExp.exec(range)) ) {
            var stopName = stop[1];
            var stopID = stop[2];

            // Add stop
            timetableData.clear();
            timetableData.set( 'StopName', stopName );
            timetableData.set( 'StopID', stopID );
            result.addData( timetableData );
        }
    }

    return result.hasData();
}
