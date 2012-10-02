/** Service provider plugin for de_vbb
  * © 2012, fpuelz */

include("base_hafas.js");

// Create instance of Hafas
var hafas = Hafas({
    baseUrl: "http://fahrinfo.vbb.de",
    binDir: "hafas",
    productBits: 7 });

function features() {
    return hafas.stopSuggestions.features.concat(
            hafas.timetable.features,
            hafas.journeys.features );
}
 
var getStopSuggestions = hafas.stopSuggestions.get;
var getTimetable = hafas.timetable.get;
var getJourneys = hafas.journeys.get;
