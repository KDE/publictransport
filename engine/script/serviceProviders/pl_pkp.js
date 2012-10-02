/** Service provider rozklad.sitkol.pl (poland).
 * © 2012, Friedrich Pülz */

include("base_hafas.js");

// Create instance of Hafas
var hafas = Hafas({
    baseUrl: "http://rozklad-pkp.pl",
//     xmlDateFormat: "dd-MM-yyyy",
    productBits: 7,
    language: "p"
});

function features() {
    return hafas.stopSuggestions.features.concat(
            hafas.timetable.features,
            hafas.journeys.features );
}
 
var getStopSuggestions = hafas.stopSuggestions.get;
var getTimetable = hafas.timetable.get;
var getJourneys = hafas.journeys.get;

hafas.otherVehicleFromString = function( string ) {
    string = string.toLowerCase();
    if ( string == "skw" || string == "skm" ||
         string == "ar" || string == "n" ||
         string == "kw" || string == "ks" ||
         string == "km" || string == "e" ||
         string == "db" )
    {
        return PublicTransport.RegionalTrain;
    } else if ( string == "tlk" ) {
        return PublicTransport.RegionalExpressTrain;
    }
};
