/** Service provider www.db.de (Deutsche Bahn, germany).
  * © 2012, Friedrich Pülz */

include("base_hafas.js");

// Create instance of Hafas
var hafas = Hafas({
    baseUrl: "http://reiseauskunft.bahn.de",
    productBits: 14
});

function features() {
    return hafas.stopSuggestions.features.concat(
            hafas.timetable.features,
            hafas.timetable.additionalData.features,
            hafas.journeys.features );
}

var getStopSuggestions = hafas.stopSuggestions.get;
var getTimetable = hafas.timetable.get;
var getAdditionalData = hafas.timetable.additionalData.get;
var getJourneys = hafas.journeys.get;

// Needed to get route data URLs for additional data
hafas.timetable.parser.parseHtmlMobile = function( html, hasError, errorString ) {
    if ( hasError ) {
        throw Error( errorString );
    }
    if ( html.length == 0 ) {
        throw Error("hafas.parseTimetableMobile(): Received HTML document is empty");
    }
    html = helper.decode( html, "latin1" );

    // Go through all departure blocks
    var departures = [];
    var departureRow = { position: -1 };
    var lineBreakRegExp = /(<br \/>)/i;
    while ( (departureRow = helper.findFirstHtmlTag(html, "div",
             {position: departureRow.position + 1,
              attributes: {"class": "sqdetails(?:Dep|Arr)"}})).found )
    {
        var departure = {}
        var routeData = helper.findFirstHtmlTag( departureRow.contents, "a",
                            {attributes: {"href": ""}} );
        departure.RouteDataUrl = routeData.attributes.href
	      .replace( /\/([a-z])ox\b/, "/$1l" )
	      .replace( /L=[^&]+/, "" );
        departure.TransportLine = helper.simplify( helper.stripTags(routeData.contents)
                    .replace(/^((?:Bus|STR)\s+)/g, "") );

        var other = departureRow.contents.substr( routeData.endPosition + 1 );
        var pos = other.indexOf("&gt;&gt;");
        var lineBreak = lineBreakRegExp.exec( other );
        if ( pos == -1 || !lineBreak ) { //|| pos2 < pos + 8 ) {
            helper.error( "hafas.parseTimetableMobile(): Error while parsing", other );
            continue;
        }
        var pos2 = lineBreak.index;

        departure.Target = helper.decodeHtmlEntities( helper.trim(other.substr(pos + 8, pos2 - pos - 8)) );

        var timeString = helper.stripTags( other.substr(pos2 + lineBreak[1].length) );
        var time = helper.matchTime( timeString, "hh:mm" );
        if ( time.error ) {
            helper.error( "hafas.parseTimetableMobile(): Could not match time", other );
            continue;
        }
        var dateTime = new Date(); // TODO Correct date!
        dateTime.setHours( time.hour, time.minute, 0, 0 );
        departure.DepartureDateTime = dateTime;

        departures.push( departure );
    }

    return departures;
};

// The traininfo.exe XML format does not work, use the text version
hafas.routeData.options.format = Hafas.TextFormat;
hafas.routeData.parser.parseText = function( html, stop, firstTime ) {
    // Find table
    html = helper.decode( html, "latin1" );
    var trainBlock = helper.findFirstHtmlTag( html, "pre" );
    if ( !trainBlock.found ) {
        throw Error("hafas.routeData.parser.parseText(): Result table not found");
    }

    var routeData = { RouteStops: [], RouteTimes: [] };
    var trainRow = { position: -1 };
    var rows = helper.splitSkipEmptyParts( helper.stripTags(trainBlock.contents), "\n" );
    var inRange = firstTime == undefined;
    for ( r = 0; r < rows.length; ++r ) {
        var row = rows[r];
        var columns = helper.splitSkipEmptyParts( row, "|" );
        if ( columns.length < 3 ) {
            // Most probably only a line of "-" for the textual table visualization,
            // ie. like "+------------------+-----+---+"
            continue;
        }
        if ( helper.trim(columns[1]) == "Station" || helper.trim(columns[1]) == "Bahnhof" ) {
            // Table header
            continue;
        }

        var routeStop = helper.decodeHtmlEntities( helper.trim(helper.stripTags(columns[1])) );
	var arrival = helper.trim( columns[2] );
	var departure = helper.trim( columns[3] );
	var timeString = departure.length < 5 ? arrival : departure;
	time = helper.matchTime( timeString, "hh:mm" );
	if ( time.error ) {
	    helper.error( "hafas.parseTrainInfoText: Could not match route time", timeString );
	    continue;
	}
	timeValue = new Date( firstTime.getFullYear(), firstTime.getMonth(),
			       firstTime.getDate(), time.hour, time.minute, 0, 0 );

        if ( inRange || routeStop == stop || firstTime.getTime() <= timeValue.getTime() ) {
            inRange = true;
            routeData.RouteStops.push( routeStop );
            routeData.RouteTimes.push( timeValue );
        }
    }
    return routeData;
};
