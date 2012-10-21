/** Service provider rejseplanen.dk
* © 2010, Friedrich Pülz */

include("base_hafas.js");

// Create instance of Hafas
var hafas = Hafas({
    baseUrl: "http://www.rejseplanen.dk",
    productBits: 11 });

function features() {
    return hafas.stopSuggestions.features.concat(
            hafas.timetable.features,
            hafas.journeys.features );
}
 
var getStopSuggestions = hafas.stopSuggestions.get;
var getTimetable = hafas.timetable.get;
var getJourneys = hafas.journeys.get;

hafas.otherVehicleFromString = function( string ) {
    switch ( string.toLowerCase() ) {
    case "togbus":
        return PublicTransport.Bus;
    default:
        return PublicTransport.UnknownVehicleType;
    }
}

hafas.otherVehicleFromClass = function( classId ) {
    switch ( classId ) {
    case 1: // IC
    case 2: // ICL
        return PublicTransport.IntercityTrain;
    case 4: // RX, RA
        return PublicTransport.RegionalTrain;
    case 32:
        return PublicTransport.Bus;
// Unknown classes:
//     case 8:
//     case 16:
//     case 64:
//     case 128:
//     case 256:
//     case 1024:
// Vehicle types without known classes:
//         return PublicTransport.Subway;
//         return PublicTransport.Tram;
//         return PublicTransport.Ferry;
    default:
        return PublicTransport.UnknownVehicleType;
    }
}
