/** Service provider www.fahrplaner.de (germany).
 * © 2012, Friedrich Pülz */

include("base_hafas.js");

// Create instance of Hafas
var hafas = Hafas({
    baseUrl: "http://www.fahrplaner.de",
    binDir: "hafas",
    productBits: 11 });

hafas.stopSuggestions.parserGeoPosition.options.fixJson = true;

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

hafas.otherVehicleFromString = function( string ) {
    switch ( string.toLowerCase() ) {
    case "ag":
    case "as":
        return PublicTransport.RegionalTrain;
    default:
        return PublicTransport.UnknownVehicleType;
    }
};

// Needed to get route data URLs for additional data
hafas.timetable.parser.parseHtmlMobile = function( html ) {
    if ( html.length == 0 ) {
        throw Error("Received HTML document is empty");
    }
    html = helper.decode( html, "latin1" );

    var timeCol = -1, routeDataCol = -1, targetCol = -1;
    var headerColumns = helper.findHtmlTags(html, "th",
             {position: 0, attributes: {"class": "sq"}})
    for ( i = 0; i < headerColumns.length; ++i ) {
        switch ( helper.trim(headerColumns[i].contents).toLowerCase() ) {
            case "zeit": case "time": timeCol = i;
            case "fahrt": case "train": routeDataCol = i;
            case "in richtung": case "timetable": targetCol = i;
        }
    }
    if ( timeCol == -1 || routeDataCol == -1 || targetCol == -1 ) {
        throw Error( "Did not find all required columns" );
    }

    // Go through all departure blocks
    var departures = [];
    var departureRow = { position: -1 };
    while ( (departureRow = helper.findFirstHtmlTag(html, "tr",
             {position: departureRow.position + 1,
              attributes: {"class": "(?:dep|arr)BoardCol(?:1|2)"}})).found )
    {
        var columns = helper.findHtmlTags(departureRow.contents, "td" );
        if ( columns.length < 3 ) {
            print( "Did not find enough columns: " + columns.length );
            continue;
        }

        var departure = {}
        var routeData = helper.findFirstHtmlTag( columns[routeDataCol].contents, "a",
                                                 {attributes: {"href": ""}} );
        departure.RouteDataUrl = routeData.attributes.href
                .replace( /\/([a-z])o\b/, "/$1ox" )
                .replace( /L=[^&]+/, "" );

        var transportLine = helper.stripTags( routeData.contents );
        departure.TransportLine = HafasPrivate.trimTransportLine( transportLine );

        var matches = /^\s*([a-z]+)/i.exec( transportLine );
        if ( matches != null ) {
            departure.TypeOfVehicle = hafas.vehicleFromString( matches[1] );
        }

        var target = helper.findFirstHtmlTag( columns[targetCol].contents, "span",
                                              {attributes: {"class": "bold"}} );
        departure.Target = helper.decodeHtmlEntities(
                helper.trim(helper.stripTags(target.contents)) );

        var timeString = helper.stripTags( columns[timeCol].contents );
        var time = helper.matchTime( timeString, "hh:mm" );
        if ( time.error ) {
            helper.error( "Could not match time", timeString );
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
