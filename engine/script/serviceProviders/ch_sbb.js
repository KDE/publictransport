/** Service provider www.sbb.ch (switzerland).
 * © 2012, Friedrich Pülz */

include("base_hafas.js");

// Create instance of Hafas
var hafas = Hafas({
    baseUrl: "http://fahrplan.sbb.ch",
    productBits: 16 });

hafas.stopSuggestions.parserGeoPosition.options.fixJson = true;

function features() {
    return hafas.stopSuggestions.features.concat(
            hafas.timetable.features,
//             hafas.timetable.additionalData.features,
            hafas.journeys.features );
}

var getStopSuggestions = hafas.stopSuggestions.get;
var getTimetable = hafas.timetable.get;
var getJourneys = hafas.journeys.get;

// The mobile HTML pages do not contain any route data URLs,
// the whole desktop HTML for departures/arrivals would need to be
// downloaded and parsed (no route data document needed then)
// var getAdditionalData = hafas.timetable.additionalData.get;

hafas.otherVehicleFromString = function( string ) {
    string = string.toLowerCase();
    if ( string == "nbu" ) {
        return PublicTransport.Bus;
    } else if ( string == "ntr" || string == "tra" ) {
        return PublicTransport.Tram;
    } else if ( string == "oic" ) {
        return PublicTransport.IntercityTrain;
    } else if ( string == "wb" || string == "ire" ) {
        return PublicTransport.RegionalExpressTrain;
    } else if ( string == "d" || string == "x" /*InterConnex*/ ) {
        return PublicTransport.InterregionalTrain;
    }
};
