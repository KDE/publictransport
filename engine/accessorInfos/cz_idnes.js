/** Accessor for idnes.cz.
* © 2012, Friedrich Pülz */

function usedTimetableInformations() {
    return [ 'TypeOfVehicle', 'StopName' ];
}

function getTimetable( stop, dateTime, maxCount, dataType, city ) {
    var url = "http://jizdnirady.idnes.cz/" + city + "/odjezdy/?" +
            "f=" + stop +
            "&Date=" + helper.formatDateTime(dateTime, "dd.M.yyyy") +
            "&Time=" + helper.formatDateTime(dateTime, "h:mm") +
            "&submit=true&lng=E&isDepTime=true";

    var request = network.createRequest( url );
    request.finished.connect( parseTimetable );
    network.get( request );
}

function parseTimetable( html ) {
    if ( helper.findFirstHtmlTag(html, "a",
         {attributes: {"class": "selectobject", "title": "Input specification"},
          contentsRegExp: "The input is ambiguous, please specify it\."}).found )
    {
        helper.error("No timetable data received, but an error message: Ambiguous input!", html);
        return;
    }

    // Find block of departures
    var departureBlock = helper.findFirstHtmlTag( html, "table", {attributes: {"class": "results"}} );
    if ( !departureBlock.found ) {
        helper.error("Result table not found!", html);
        return;
    }

    // Match date
    var dateTime = helper.findFirstHtmlTag( html, "div", {attributes: {"class": "recapdate"}} );
    dateTime = dateTime.found ? helper.matchDate( dateTime.contents, "d.M.yyyy" ) : new Date();

    // Matches type of vehicle [1], target [2], transport line [3]
    var infoRegExp = /<img [^>]*?alt="([^"]*?)"[^>]*\/>\s*<a href="[^"]*?" title="[^\(]*?\([^>]*>>\s([^\)]*?)\)"[^>]*>([^<]*?)<\/a>/i;

    // Go through all departure blocks
    var timeCol = 0, infoCol = 4, departureRow = { position: -1 };
    while ( (departureRow = helper.findFirstHtmlTag(departureBlock.contents, "tr",
            { attributes: {"class": ""}, position: departureRow.position + 1 })).found )
    {
        // Find columns, ie. <td> tags in the current departure row
        var columns = helper.findHtmlTags( departureRow.contents, "td" );
        if ( !columns || columns.length < 5 ) {
            // Do not send an error message if this is the header row of the table
            if ( departureRow.contents.indexOf("<th") == -1 ) {
                helper.error("Too less columns found in a row in the result table (" +
                             (columns ? columns.length : "[undefined]") + ")!", departureRow.contents);
            }
            continue;
        }

        // Initialize
        var departure = {};

        // Parse time column
        var time = helper.matchTime( columns[timeCol].contents, "h:mm" );
        if ( time.error ) {
            helper.error("Unexpected string in time column!", columns[timeCol].contents);
            continue;
        }
        dateTime.setHours( time.hour, time.minute, 0 );
        departure.DepartureDateTime = dateTime;

        // Parse info column with vehicle type, target and transport line
        var info = infoRegExp.exec( columns[infoCol].contents );
        if ( info == null ) {
            helper.error("Unexpected string in info column!", columns[infoCol].contents);
            continue;
        }
        departure.TypeOfVehicle = helper.trim( info[1] );
        departure.Target = helper.trim( info[2] );
        departure.TransportLine = helper.trim( info[3] );

        // Add departure
        result.addData( departure );
    }
}

function getStopSuggestions( stop, maxCount, city  ) {
    // Try to read session key from cache
    // TODO: Maybe also store the time of the session key request and update the session key from time to time?
    var sessionKey = storage.read( "session-key" );
    if ( typeof(sessionKey) != 'string' || sessionKey.length == 0 ) {
        // Session key not cached, download a document containing a session key
        var html = network.getSynchronous( "http://jizdnirady.idnes.cz/vlakyautobusymhdvse/spojeni/" );
        if ( network.lastDownloadAborted() ) {
            return false;
        }

        // Get the session key and check it
        sessionKey = parseSessionKey( html );
        if ( sessionKey.length == 0 ) {
            return; // Session key not found
        }

        // Write session key into the cache
        storage.write( "session-key", sessionKey );
    }

    // Download suggestions and parse them using a regular expression
    var url = "http://jizdnirady.idnes.cz/AJAXService.asmx/SearchTimetableObjects";
    var request = network.createRequest( url );
    request.finished.connect( parseStopSuggestions );
    request.setPostData( "{\"timestamp\":\"" + (new Date).getTime() + "\"," +
            "\"prefixText\":\"" + stop + "\",\"count\":\"20\"," +
            "\"selectedTT\":\"BMCz\",\"bindElementValue\":\"\",\"iLang\":\"ENGLISH\"," +
            "\"bCoor\":\"false\"}", "utf-8" );
    request.setHeader( "Accept", "application/json, text/javascript, */*", "utf-8" );
    request.setHeader( "Content-Type", "application/json; charset=utf-8", "utf-8" );
    request.setHeader( "idoskey", sessionKey, "utf-8" );
    network.post( request );
}

function parseStopSuggestions( json ) {
    // Initialize regular expressions (compile them only once)
    var stopRegExp = /\{"__type":"CHAPS.IDOS3.TTWS.TimetableObjectInfo",(?:"[^"]*":[^,]*,)*?"oItem":\{([^\}]+)\}[^\}]*\}/ig;
    var stopNameRegExp = /"sName":"([^"]+)"/i;
//     var stopIdRegExp = /"iItem":([0-9]+)/i;

    // Go through all stop options
    while ( (oItem = stopRegExp.exec(json)) ) {
        var oItem = oItem[1];

        var stopName = stopNameRegExp.exec(oItem);
        if ( stopName == null ) {
            helper.error("Unexpected string in stop suggestion document, didn't find the stop name!", oItem);
            continue;
        }
        stopName = stopName[1];

        // Add stop
        result.addData( {'StopName': stopName} );
    }

    return result.hasData();
}

function parseSessionKey( html ) {
    var sessionKey = /var\s+sIDOSKey\s*=\s*'([^']+)';/i.exec( html );
    if ( sessionKey == null ) {
        helper.error("Session key not found!", html);
        return "";
    }

    return sessionKey[1];
}
