/** Service provider http://www.fahrinfo-berlin.de (BVG, Berlin).
 * © 2012, Friedrich Pülz */

include("base_hafas.js");

// Create instance of Hafas
var hafas = Hafas({
    baseUrl: "http://www.fahrinfo-berlin.de/Fahrinfo",
    productBits: 8,
    programExtension: "bin" });

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

// No departure/arrival data source in XML format available,
// use mobile HTML format instead. It also contains RouteDataUrls
// for each departure/arrival. Data sources for route data are also
// not available in XML format (or text format), also use mobile
// HTML format here:
hafas.timetable.options.format = Hafas.HtmlMobileFormat;
hafas.routeData.options.format = Hafas.HtmlMobileFormat;

// Implement parser for departure/arrival data in mobile HTML format
hafas.timetable.parser.parseHtmlMobile = function( html, hasError, errorString ) {
    if ( hasError ) {
        throw Error( errorString );
    }
    if ( html.length == 0 ) {
        throw Error("Received HTML document is empty");
    }
    html = helper.decode( html, "latin1" );

    // Go through all departure blocks
    var departures = [];
    var departureRow = { position: -1 };
    while ( (departureRow = helper.findFirstHtmlTag(html, "tr",
             {position: departureRow.position + 1,
              attributes: {"class": "ivu_table_bg(?:1|2)"}})).found )
    {
	var columns = helper.findHtmlTags(departureRow.contents, "td" );
	if ( columns.length < 3 ) {
	    print( "Did not find enough columns: " + columns.length );
	    continue;
	}

        var departure = {}
        var routeData = helper.findFirstHtmlTag( columns[1].contents, "a",
                            {attributes: {"href": ""}} );
        departure.RouteDataUrl = routeData.attributes.href
		.replace( /([a-z])l/, "$1ox" )
		.replace( /L=[^&]+/, "" );
	if ( departure.RouteDataUrl.substr(0, 7) != "http://" ) {
	    departure.RouteDataUrl = "http://www.fahrinfo-berlin.de" +
				     departure.RouteDataUrl;
	}

	var transportLine = helper.simplify( helper.stripTags(routeData.contents) );
        departure.TransportLine = transportLine.replace( /^((?:Bus|STR)\s+)/g, "" );

	var matches = /^([a-z]+)/i.exec( transportLine );
	if ( matches != null ) {
	    departure.TypeOfVehicle = hafas.vehicleFromString( matches[1] );
	}

	departure.Target = helper.decodeHtmlEntities(
	    helper.trim(helper.stripTags(columns[2].contents)) );

        var timeString = helper.stripTags( columns[0].contents );
        var time = helper.matchTime( timeString, "hh:mm" );
        if ( time.error ) {
            helper.error( "Could not match time", other );
            continue;
        }
        var dateTime = new Date();
        dateTime.setHours( time.hour, time.minute, 0, 0 );
        departure.DepartureDateTime = dateTime;

        departures.push( departure );
	result.addData( departure );
    }

    return departures;
};

// Implement parser for route data in mobile HTML format
hafas.routeData.parser.parseHtmlMobile = function( html, stop, firstTime ) {
    if ( html.length == 0 ) {
        throw Error("Received HTML document is empty");
    }
    html = helper.decode( html, "latin1" );

    // Go through all departure blocks
    var departureRow = { position: -1 };
    var routeData = { RouteStops: [], RouteTimes: [] };
    var inRange = firstTime == undefined;
    while ( (departureRow = helper.findFirstHtmlTag(html, "tr",
             {position: departureRow.position + 1})).found )
    {
	var columns = helper.findHtmlTags(departureRow.contents, "td" );
	if ( columns.length < 3 ) {
	    print( "Did not find enough columns: " + columns.length );
	    continue;
	}
	var routeStop = helper.decodeHtmlEntities( helper.trim(columns[2].contents) );

	var timeString = helper.trim( columns[1].contents );
	time = helper.matchTime( timeString, "hh:mm" );
	if ( time.error ) {
	    helper.error( "Could not match route time", timeString );
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
