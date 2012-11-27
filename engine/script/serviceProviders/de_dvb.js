/** Service provider www.dvb.de (Dresden, germany).
* © 2010, Friedrich Pülz */


function features() {
    return [ PublicTransport.ProvidesArrivals ];
}

function getTimetable( values ) {
    var timestamp = Math.round( (new Date()).getTime() / 1000 ); // in seconds since 1970-01-01...
    var url = "http://widgets.vvo-online.de/abfahrtsmonitor/Abfahrten.do" +
	"?hst=" + values.stop + "&vz=0" +
	"&time=" + timestamp + "&iso=1";

    var request = network.createRequest( url );
    request.finished.connect( parseTimetable );
    network.get( request );
}

function parseTimetable( json, hasError, errorString ) {
    if ( hasError ) {
        throw Error( errorString );
    }

    // Initialize regular expressions (compile them only once)
    var departureRegExp = /\["([^"]*)","([^"]*)","([^"]*)"\]/ig;

    // Go through all departures
    var now = new Date();
    while ( (departure = departureRegExp.exec(json)) ) {
        var minutes = parseInt( departure[3] );
        var timeString = helper.addMinsToTime( now.getHours() + ":" + now.getMinutes(), minutes,
                                        "h:m" );
        var time = helper.matchTime( timeString, "h:m" );
        if ( time.error ) {
                helper.error("Unexpected string in time column!", departure[3]);
                continue;
        }
        var dateTime = new Date();
	dateTime.setHours( time.hour );
	dateTime.setMinutes( time.minute );

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
        result.addData( {TransportLine: departure[1],
			 Target: departure[2],
			 TypeOfVehicle: vehicleType,
			 DepartureDateTime: dateTime} );
    }
}

function getStopSuggestions( values  ) {
    var timestamp = Math.round( (new Date()).getTime() / 1000 ); // in seconds since 1970-01-01...
    var url = "http://widgets.vvo-online.de/abfahrtsmonitor/Haltestelle.do?hst=" + values.stop
            + "?&vz=0&time=" + timestamp + "&iso=1";
    var json = network.getSynchronous( url );

    if ( !network.lastDownloadAborted ) {
        // Initialize regular expressions (compile them only once)
        var stopRangeRegExp = /\[\[\["[^"]*"\]\],\[(.*)\]\]$/i;
        var stopRegExp = /\["([^"]*)","([^"]*)","([^"]*)"\]/ig;

        // Get range of all stops
        var range = stopRangeRegExp.exec( json );
        if ( range == null ) {
	    helper.error("Stop range not found", json);
	    return false;
        }
        range = range[1];

        // Go through all stops
        while ( (stop = stopRegExp.exec(range)) ) {
            // Add stop
            result.addData( {StopName: stop[1], StopCity: stop[2], StopID: stop[3]} );
        }
    }

    return result.hasData();
}
