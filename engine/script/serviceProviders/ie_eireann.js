/** Service provider plugin for ie_eireann
  * © 2012, fpuelz */

include("base_hafas.js");

// Create instance of Hafas
var hafas = Hafas({
    baseUrl: "http://buseireann.fahrinfo.ivu.de/Fahrinfo",
    programExtension: "bin",
    xmlDateFormat: "dd-MM-yyyy",
    productBits: 16 });

// Arrivals are not working in XML format (wrong "no trains in result", with other formats it's ok)
hafas.timetable.removeFeature( PublicTransport.ProvidesArrivals );

function features() {
    return hafas.stopSuggestions.features.concat(
            hafas.timetable.features,
            hafas.journeys.features );
}

var getTimetable = hafas.timetable.get;
var getStopSuggestions = hafas.stopSuggestions.get;
var getJourneys = hafas.journeys.get;
