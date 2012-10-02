/** Service provider http://www.fahrinfo-berlin.de (BVG, Berlin).
 * © 2012, Friedrich Pülz */

include("base_hafas.js");

// Create instance of Hafas
var hafas = Hafas({
    baseUrl: "http://reiseauskunft.insa.de",
    productBits: 8 });

function features() {
    return hafas.stopSuggestions.features.concat(
	    hafas.timetable.features,
	    hafas.journeys.features );
}
 
var getStopSuggestions = hafas.stopSuggestions.get;
var getTimetable = hafas.timetable.get;
var getJourneys = hafas.journeys.get;
