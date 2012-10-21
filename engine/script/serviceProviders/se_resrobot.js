/** Service provider resrobot.se
* © 2012, Friedrich Pülz */

include("base_hafas.js");

// Create instance of Hafas
var hafas = Hafas({
    baseUrl: "http://reseplanerare.resrobot.se",
    urlDateFormat: "yyyy-MM-dd",
    xmlDateFormat: "yyyy-MM-dd",
    productBits: 14,
    language: "s"
});
 
function features() {
    return hafas.stopSuggestions.features.concat(
            hafas.timetable.features,
//             hafas.timetable.additionalData.features,
            hafas.journeys.features );
}

var getStopSuggestions = hafas.stopSuggestions.get;
var getTimetable = hafas.timetable.get;
// var getAdditionalData = hafas.timetable.additionalData.get;
var getJourneys = hafas.journeys.get;

hafas.otherVehicleFromClass = function( classId ) {
    switch ( classId ) {
    case 1:
    case 2:
        return PublicTransport.IntercityTrain;
    case 4:
    case 16:
    case 1024:
        return PublicTransport.RegionalTrain;
    case 8:
    case 128:
        return PublicTransport.Bus;
    case 32:
        return PublicTransport.Subway;
    case 64:
        return PublicTransport.Tram;
    case 256:
        return PublicTransport.Ferry;
    default:
        return PublicTransport.UnknownVehicleType;
    }
}

hafas.otherVehicleFromString = function( string ) {
    string = string.toLowerCase();
    if ( string == "regional" || string == "skw" || string == "skm" ||
         string == "ar" || string == "n" ||
         string == "kw" || string == "ks" ||
         string == "km" || string == "e" ||
         string == "db" ||
         string == "jft" ) // Future Train
    {
        return PublicTransport.RegionalTrain;
    } else if ( string == "tunnelbana" ) {
	return PublicTransport.Subway;
    }
    print( "Unknown vehicle type abbreviation: " + string + "'" );
};
