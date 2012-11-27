/** Service provider www.flightstats.com (international).
* © 2012, Friedrich Pülz */

function features() {
    return [ PublicTransport.ProvidesStopID, PublicTransport.ProvidesArrivals ];
}

function getTimetable( values ) {
    var url = "http://www.flightstats.com/go/FlightStatus/flightStatusByAirport.do" +
            "?airport=" + values.stop +
            "&airportQueryDate=" + helper.formatDateTime(values.dateTime, "yyyy-MM-dd") +
            "&airportQueryTime=" + helper.formatDateTime(values.dateTime, "hh:mm");
            "&airlineToFilter=&airportQueryType=0";
    var request = network.createRequest( url );
    request.finished.connect( parseTimetable );
    network.get( request );
}

function parseTimetable( html ) {
    // Decode document
    html = helper.decode( html, "utf8" );

    // Find block of departures
    var pos = html.search( /<table [^>]*class="tableListingTable"[^>]*>/i );
    if ( pos == -1 ) {
        helper.error("Result table not found!", html);
        return;
    }
    var end = html.indexOf( '</table>', pos + 1 );
    var str = html.substr( pos, end - pos );

    // Initialize regular expressions (compile them only once)
    var departuresRegExp = /<tr[^>]*>([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*>([\s\S]*?)<\/td>/ig;
    var flightNumberRegExp = /<a [^>]*?href='[^>]*?&airline=([^&]+)&flightNumber=(\d+)/i; // matches airline code and flight number
    var linkRegExp = /<a href="([^"]+)"/i

    var targetCol = 0, flightNumberCol = 1, airlineCol = 3, departureCol = 4, statusCol = 7;

    // Go through all departure blocks
    while ( (departureRow = departuresRegExp.exec(str)) ) {
        departureRow = departureRow[1];

        // Get column contents
        var columns = new Array;
        while ( (column = columnsRegExp.exec(departureRow)) ) {
            column = column[1];
            columns.push( column );
        }
        if ( columns.length < 8 ) {
            if ( departureRow.indexOf("<th") == -1 ) {
                helper.error("Too less columns (" + columns.length +
                             ") found in a departure row!", departureRow);
            }
            continue; // Too less columns
        }

        // Initialize some values
        var departure = { Delay: -1, TypeOfVehicle: provider.defaultVehicleType }; // unknown delay

        // Parse time column
        var time = helper.matchTime( columns[departureCol], "hh:mm AP" ); // TODO Test
        if ( time.error ) {
            helper.error("Unexpected string in time column!", columns[departureCol]);
            continue; // Unexpected string in time column
        }
        dateTime = new Date();
        dateTime.setHours( time.hour, time.minute, 0, 0 );
        departure.DepartureDateTime = dateTime;

        // Parse flight number column
        var flightNumber = flightNumberRegExp.exec( columns[flightNumberCol] );
        if ( flightNumber == null ) {
            helper.error("Unexpected string in flight number column!", columns[flightNumberCol]);
            continue; // Unexcepted string in flight number column
        }
        // var airlineCode = flightNumber[1]; // TODO isn't used currently
        departure.FlightNumber = flightNumber[2];

        var journeyNewsLink = linkRegExp.exec( columns[flightNumberCol] );
        if ( journeyNewsLink != null ) {
            departure.JourneyNewsLink = journeyNewsLink[0];
        }

        // Parse columns
        departure.Target = helper.trim( helper.stripTags(columns[targetCol]) );
        departure.Operator = helper.trim( helper.stripTags(columns[airlineCol]) );

        // Parse status column (status + maybe delay info)
        departure.Status = helper.trim( helper.stripTags(columns[statusCol]) );
        pos = departure.Status.indexOf( "On-time" );
        if ( pos != -1 ) {
            departure.Delay = 0;

            // Cut "On-time" from the status string
            departure.Status = helper.trim( departure.Status.substr(0, pos - 1) );
        }

        // Add departure
        result.addData( departure );
    }
}

function getStopSuggestions( values ) {
    var url = "http://www.flightstats.com/go/Suggest/airportSuggest.do" +
            "?desiredResults=" + values.count +
            "&searchSubstring=" + values.stop;
    var document = network.getSynchronous( url );
    if ( network.lastDownloadAborted ) {
        return false;
    }

    // Decode document
    html = helper.decode( document, "utf8" );

    // Initialize regular expressions (compile them only once)
    var stopRegExp = /<airport\s+code="([^"]*)"\s+name="([^"]*)"\s+city="([^"]*)"\s+countryCode="([^"]*)"[^>]*\/>/ig;

    // Go through all stops
    while ( (stop = stopRegExp.exec(html)) ) {
        result.addData( {StopName: stop[2], StopID: stop[1],
                         StopCity: stop[3], StopCountryCode: stop[4]} );
    }

    return result.hasData();
}
