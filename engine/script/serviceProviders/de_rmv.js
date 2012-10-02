/** Service provider RMV (Rhein-Main), germany.
  * © 2012, Friedrich Pülz */

include("base_hafas.js");

// Create instance of Hafas
var hafas = Hafas({
    baseUrl: "http://www.rmv.de/auskunft",
    binDir: "bin/jp",
    productBits: 16,
    layout: "vs_rmv.vs_sq",
    charset: "utf-8" // TEST
});
hafas.timetable.options.format = Hafas.XmlResCFormat;

function features() {
    return hafas.stopSuggestions.features.concat(
            hafas.timetable.features,
            hafas.journeys.features );
}

var getStopSuggestions = hafas.stopSuggestions.get;
var getTimetable = hafas.timetable.get;
var getJourneys = hafas.journeys.get;
