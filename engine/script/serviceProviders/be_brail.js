/** Service provider hafas.bene-system.com (SNCB, B-Rail).
* © 2012, Friedrich Pülz */

include("base_hafas.js");

// Create instance of Hafas
var hafas = Hafas({
    baseUrl: "http://hari.b-rail.be/hafas",
    xmlDateFormat: "dd/MM/yy",
    productBits: 11 });

// Arrivals are not working in XML format (wrong "no trains in result", with other formats it's ok),
// stop suggestions cannot be requested by position, disable these features
hafas.timetable.removeFeature( PublicTransport.ProvidesArrivals );
hafas.stopSuggestions.removeFeature( PublicTransport.ProvidesStopSuggestionsByPosition );

function features() {
    return hafas.stopSuggestions.features.concat(
            hafas.timetable.features,
            hafas.timetable.additionalData.features,
            hafas.journeys.features );
}

var getStopSuggestions = hafas.stopSuggestions.get;
var getTimetable = hafas.timetable.get;
var getJourneys = hafas.journeys.get;

// traininfo.exe-documents may be missing time information
// for some route stops for some reason
var getAdditionalData = hafas.timetable.additionalData.get;

// Needed to get traininfo.exe-URLs for additional data
hafas.timetable.parser.parseHtmlMobile = function( html ) {
    if ( html.length == 0 ) {
        throw Error("Received HTML document is empty");
    }
    html = helper.decode( html, "latin1" );

    // Get departure date
    var date;
    var globalData = helper.findFirstHtmlTag( html, "p", {attributes: {"class": "qs"}} );
    if ( globalData.found ) {
	var matches = /0?(\d?\d)\/0?(\d?\d)\/0?(\d?\d)/i.exec( globalData.contents );
	if ( matches == null ) {
	    helper.error("Date not found");
	    date = new Date();
	} else {
	    date = new Date( 2000 + parseInt(matches[3]), parseInt(matches[2]) - 1,
			     parseInt(matches[1]), 0, 0, 0 );
	}
    } else {
	helper.error("Date not found");
	date = new Date();
    }
    
    // Go through all departure blocks
    var departures = [];
    var departureRow = { position: -1 };
    var lineBreakRegExp = /(<br \/>)/i;
    var lastDateTime;
    while ( (departureRow = helper.findFirstHtmlTag(html, "p",
             {position: departureRow.position + 1,
              attributes: {"class": "journey"}})).found )
    {
        var departure = {}
        var routeData = helper.findFirstHtmlTag( departureRow.contents, "a",
                            {attributes: {"href": ""}} );
        departure.RouteDataUrl = routeData.attributes.href
		.replace( /\/([a-z])ox\b/, "/$1l" )
		.replace( /L=[^&]+/, "" );
        departure.TransportLine = helper.simplify( helper.stripTags(routeData.contents)
                    .replace(/^((?:Bus|STR)\s+)/ig, "") );

        var other = departureRow.contents.substr( routeData.endPosition + 1 );
        var pos = other.indexOf("&gt;&gt;");
        var lineBreak = lineBreakRegExp.exec( other );
        if ( pos == -1 || !lineBreak ) { //|| pos2 < pos + 8 ) {
            helper.error( "Error while parsing", other );
            continue;
        }
        var pos2 = lineBreak.index;

        departure.Target = helper.decodeHtmlEntities( 
		helper.trim(other.substr(pos + 8, pos2 - pos - 8)) );

        var timeString = helper.stripTags( other.substr(pos2 + lineBreak[1].length) );
        var time = helper.matchTime( timeString, "hh:mm" );
        if ( time.error ) {
            helper.error( "Could not match time", other );
            continue;
        }
        
        
        departure.DepartureDateTime = new Date( 
		date.getFullYear(), date.getMonth(), date.getDate(), 
		time.hour, time.minute, 0, 0 );
	
	// TEST Fix date correction with departures of two (or more) days
	if ( lastDateTime != undefined ) {
	    var diff = departure.DepartureDateTime.getTime() - lastDateTime.getTime();
	    if ( -diff / (1000 * 60 * 60) > 20 ) {
		// Add one day to the date
		var millisecondsInOneDay = 24 * 60 * 60 * 1000;
		date.setTime( date.getTime() + millisecondsInOneDay );
		departure.DepartureDateTime.setTime( 
			departure.DepartureDateTime.getTime() + millisecondsInOneDay );
	    }
	}
	lastDateTime = departure.DepartureDateTime;

        departures.push( departure );
    }

    return departures;
};
