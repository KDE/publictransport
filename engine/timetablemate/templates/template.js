/** Accessor for <Service Provider>
* © 2010, <Author>
*
* Use this template to create your custom accessor script. Each script also needs
* an XML-File which references the script, eg. <script>template.js</script>. */

// This function isn't needed.
// You can add it to parse a given HTML document (with journeys) for an url
// that gives later results. When this function is used requesting journey
// lists is done in multiple round trips.
function getUrlForLaterJourneyResults( html ) {
    return "www.parsed.url.com";
}
    
// This function isn't needed.
// You can add it to parse a given HTML document (with journeys) for an url
// that enables details for the journeys. When this function is used
// requesting journey lists is done in multiple round trips.
function getUrlForDetailedJourneyResults( html ) {
    return "www.parsed.url.com";
}

// This function parses a given HTML document for timetable infos.
// The infos are added to the result-array as shown below.
function parseTimetable( html ) {
    // Add departure
    // A list of all available key-strings to use in the result-array is at
    //    /publictransport-data-engine-sources/enums.h.
    // As you can see, not all values are needed.
    this.result.push( { 'TransportLine' : transportLine, // needed
			'TypeOfVehicle' : typeOfVehicle,
			'Target' : targetString, // needed
			'DepartureHour' : hour, // needed
			'DepartureMinute' : minute, // needed
			'Platform' : platformString,
			'Delay' : delay, // in minutes
			'DelayReason' : delayReason,
			'RouteStops' : routeStops,
			'RouteTimes' : routeTimes,
			'RouteExactStops' : exactRouteStops } );
}
    
// This function parses a given HTML document for journey infos.
// The infos are added to the result-array as shown below.
function parseJourneys( html ) {
    this.result = new Array();

    // Add journey
    // A list of all available key-strings to use in the result-array is at
    //    /publictransport-data-engine-sources/enums.h.
    // As you can see, not all values are needed.
    this.result.push( { 'TypesOfVehicleInJourney' : typesOfVehicleInJourney,
			'StartStopName' : startStopName, // needed
			'TargetStopName' : targetStopName, // needed
			'DepartureDate' : departureDate,
			'DepartureHour' : departureHour, // needed
			'DepartureMinute' : departureMinute, // needed
			'ArrivalDate' : arrivalDate,
			'ArrivalHour' : arrivalHour, // needed
			'ArrivalMinute' : arrivalMinute, // needed
			'Duration' : duration, // in minutes or hh:mm
			'Changes' : changes,
			'Pricing' : pricing,
			'Delay' : delay, // in minutes
			'DelayReason' : delayReason,
			'RouteStops' : routeStops,
			'RoutePlatformsDeparture' : routePlatformsDeparture,
			'RoutePlatformsArrival' : routePlatformsArrival,
			'RouteTimesDeparture' : routeTimesDeparture,
			'RouteTimesArrival' : routeTimesArrival,
			'RouteTypesOfVehicles' : routeVehicleTypes,
			'RouteTransportLines' : routeTransportLines } );
}
    
// This function parses a given HTML document for stop suggestions.
// The infos are added to the result-array as shown below.
// You can also see an implementation that works for stop suggestions
// being shown in a list with <select>.
function parsePossibleStops( html ) {
    // Find block of stops, you may need to add some attributes
    // to the <select>-tag
    var str = extractBlock( html, '<select>', '</select>' );

    // Initialize regular expressions (compile them only once)
    var stopRegExp = /<option>([^<]*?)<\/option>/ig;

    // Go through all stop options
    while ( (stop = stopRegExp.exec(str)) ) {
	// Add stop
	// You can also use 'StopID' if there are id's used for stops by
	// the provider. But 'StopName' should always be set.
	this.result.push( { 'StopName' : stop[1] } );
    }
}
