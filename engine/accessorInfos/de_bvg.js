
// This function returns a list of all features supported by this script.
function usedTimetableInformations() {
    return [];
}

function getTimetable( stop, dateTime, maxCount, dataType, city ) {
    var url = "http://www.fahrinfo-berlin.de/IstAbfahrtzeiten/index;ref=3?" +
            "input=" + stop + "&submit=Anzeigen";

    var request = network.createRequest( url );
//     request.readyRead.connect( parseTimetableIterative );
    request.finished.connect( parseTimetable );
    network.get( request );
}

// This function parses a given HTML document for departure/arrival data.
function parseTimetable( html ) {
    // Find result table
    var pos = html.search( /<h2 class=\"ivuHeadline\">\s*Ist-Abfahrtzeiten - Übersicht/i );
    if ( pos == -1 ) {
// 		helper.error("Result table not found!", html);
        str = html;
    } else {
        str = html.substr( pos );
    }

    var dateRegExp = /<input .*?value="([^"]*)" name="date" id="home_date"[^>]*?>/i;
//     var dateRegExp = /<td class="ivuTableLabel">\s*Datum:\s*<\/td>\s*<td>\s*([\s\S]*?)\s*<\/td>/i;
    var date = dateRegExp.exec( html );
    if ( date == null ) {
        var now = new Date();
        date = [ now.getDate(), now.getMonth() + 1, now.getFullYear() ];
    } else {
        date = helper.matchDate( helper.stripTags(date[1]), "dd.MM.yy" );
    }

    // Initialize regular expressions
    var departuresRegExp = /<tr[^>]*?>([\s\S]*?)<\/tr>/ig;
    var columnsRegExp = /<td[^>]*?>([\s\S]*?)<\/td>/ig;
    var typeOfVehicleRegExp = /<img src="[^"]*" class="ivuTDProductPicture" alt="[^"]*"\s*class="ivuTDProductPicture" \/>(\w{1,10})\s*((?:\w*\s*)?[0-9]+)/i; // Matches type of vehicle [0] and transport line [1]

    var timeCol = 0, typeOfVehicleCol = 1, transportLineCol = 1, targetCol = 2;

    // Go through all departure blocks
    while ( (departureRow = departuresRegExp.exec(str)) ) {
        // This gets the current departure row
        departureRow = departureRow[1];

        // Get column contents
        var columns = new Array;
        while ( (col = columnsRegExp.exec(departureRow)) ) {
            columns.push( col[1] );
        }
        columnsRegExp.lastIndex = 0;

        if ( columns.length < 3 ) {
            if ( departureRow.indexOf("<th") == -1 && departureRow.indexOf("ivuTableFootRow") == -1 ) {
                helper.error("Too less columns in a departure row found (" + columns.length + ") " + departureRow);
            }
            continue;
        }

        var departure = {};

        // Parse time column
        time = helper.matchTime( helper.trim(helper.stripTags(columns[timeCol])), "hh:mm" );
        if ( time.error ) {
            helper.error("Unexpected string in time column", columns[timeCol]);
            continue;
        }
        departure.DepartureDateTime = new Date( date[2], date[1], date[0],
                                                time.hour, time.minute, 0, 0 );
        print( departure.DepartureDateTime );

        // Parse type of vehicle column
        var typeOfVehicle = typeOfVehicleRegExp.exec(columns[typeOfVehicleCol]);
        if ( typeOfVehicle != null ) {
            departure.TransportLine = typeOfVehicle[2];
            departure.TypeOfVehicle = typeOfVehicle[1];
        } else {
            helper.error("Unexpected string in type of vehicle column", columns[typeOfVehicleCol]);
            departure.TypeOfVehicle = "Unknown";
            departure.TransportLine = "Unknown";
        }

        // Parse target column
        departure.Target = helper.trim( helper.stripTags(columns[targetCol]) );

        // Add departure to the result set
        result.addData( departure );
    }
}

function getStopSuggestions( stop, maxCount, city  ) {
    var url = "http://www.fahrinfo-berlin.de/IstAbfahrtzeiten/index;ref=3?input=" + stop +
              "&submit=Anzeigen";
    var html = network.getSynchronous( url );

    if ( !network.lastDownloadAborted() ) {
        // Find block of stops
        var stopRangeRegExp = /<select [^>]*class="ivuSelectBox"[^>]*>\s*([\s\S]*?)\s*<\/select>/i;
        var range = stopRangeRegExp.exec( html );
        if ( range == null ) {
            helper.error( "Stop range not found", html );
            return false;
        }
        range = range[1];

        // Initialize regular expressions (compile them only once)
        var stopRegExp = /<option value="[^"]*">\s*([^<]*)\s*<\/option>/ig;

        // Go through all stop options
        while ( (stop = stopRegExp.exec(range)) ) {
            result.addData({ StopName: helper.trim(stop[1]) });
        }

        return result.hasData();
    } else {
        return false;
    }
}
