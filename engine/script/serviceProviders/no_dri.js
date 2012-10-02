/** Service provider plugin for no_dri
  * © 2012, fpuelz */

include("base_hafas.js");

// Create instance of Hafas
var hafas = Hafas({
    baseUrl: "http://hafas.websrv05.reiseinfo.no",
    binDir: "/bin/dev/nri/",
    productBits: 8 });

function features() {
    return hafas.stopSuggestions.features.concat(
	    hafas.timetable.features );
}
 
var getStopSuggestions = hafas.stopSuggestions.get;
var getTimetable = hafas.timetable.get;
// var getJourneys = hafas.journeys.get;
