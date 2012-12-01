/** Service provider oebb.at.
* © 2012, Friedrich Pülz */

include("base_hafas.js");

// Setup hafas options// Create instance of Hafas
var hafas = Hafas({
    baseUrl: "http://fahrplan.oebb.at",
    productBits: 16 });

function features() {
    return hafas.stopSuggestions.features.concat(
            hafas.timetable.features,
            hafas.timetable.additionalData.features,
            hafas.journeys.features );
}

var getStopSuggestions = hafas.stopSuggestions.get;
var getTimetable = hafas.timetable.get;
function getAdditionalData( values ) {
    return hafas.timetable.additionalData.get( values,
	    {encodeUrlQuery: false, additionalUrlParameters: "&mobile="} );
};
var getJourneys = hafas.journeys.get;

hafas.timetable.parser.parseHtmlMobile = function( html ) {
    if ( html.length == 0 ) {
	throw Error("hafas.parseTimetableMobile(): Received HTML document is empty");
    }
    html = helper.decode( html, "latin1" );

    // Find block of departures
    var departureBlock = helper.findFirstHtmlTag( html, "div",
            {attributes: {"class": "HFS_mobile"}, debug: true} );
    if ( !departureBlock.found ) {
        throw Error("hafas.parseTimetableMobile(): Result table not found");
    }
    html = departureBlock.contents;

    // Go through all departure blocks
    var departures = [];
    var departureRow = { position: -1 };
    var lineBreakRegExp = /(<br \/>)/i;
    var vehicleTypeRegExp = /\/img\/vs_oebb\/(^_)+_pic\./i;
    while ( (departureRow = helper.findFirstHtmlTag(html, "div",
			    {position: departureRow.position + 1,
			      attributes: {"onclick" :"window.location='(.*)'"}})).found )
    {
	var departure = {}
	var routeData = helper.findFirstHtmlTag( departureRow.contents, "a",
						  {attributes: {"href": ""}} );
	departure.RouteDataUrl = routeData.attributes.href
		.replace( /\/([a-z])ox\b/, "/$1l" )
		+ "&L=vs_java3"; // It's available in XML format
	departure.TransportLine = helper.simplify( helper.stripTags(routeData.contents)
				  .replace(/^((?:Bus|STR)\s+)/g, "") );

	var other = departureRow.contents.substr( routeData.endPosition );
	var pos = other.indexOf("&#xBB;");
	var lineBreak = lineBreakRegExp.exec( other );
	if ( pos == -1 || !lineBreak ) {
	    helper.error( "hafas.parseTimetableMobile(): Target not found", other );
	    continue;
	}
	var pos2 = lineBreak.index;
	departure.Target = helper.decodeHtmlEntities( helper.trim(other.substr(pos + 2, pos2 - pos - 2)) );

	var timeString = helper.stripTags( other.substr(pos2 + lineBreak[1].length) );
	var time = helper.matchTime( timeString, "hh:mm" );
	if ( time.error ) {
	    helper.error( "hafas.parseTimetableMobile(): Could not match time", other );
	    continue;
	}
	var dateTime = new Date();
	dateTime.setHours( time.hour, time.minute, 0, 0 );
	departure.DepartureDateTime = dateTime;

	departures.push( departure );
    }

    return departures;
};

hafas.otherVehicleFromString = function( string ) {
    string = string.toLowerCase();
    if ( string == "s" || string == "sbahn" ||
         string == "cat" ) // "City Airport Train", non-stop connection between airport "Wien-Schwechat" and the city centry of Wien
    {
	return PublicTransport.InterurbanTrain;
    } else if ( string == "str" || string == "ntr" || string == "lkb" ) {
	return PublicTransport.Tram;
    } else if ( string == "nmg" || string == "nmp" || string == "hmp" ||
		 string == "hog" )
    {
	return PublicTransport.Bus;
    } else if ( string == "oic" || string == "oec" || string == "ec" ) { // Eurocity
	return PublicTransport.IntercityTrain;
    } else if ( string == "rex" || string == "ez" ) { // "ErlebnisZug"
	return PublicTransport.RegionalExpressTrain;
    } else if ( string == "ir" ||
	         string == "wb" ) // "WestBahn" (Wien – Salzburg – Freilassing)
    {
	return PublicTransport.InterregionalTrain;
    } else {
	return string;
    }
};
