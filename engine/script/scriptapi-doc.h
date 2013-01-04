/*
 *   Copyright 2012 Friedrich Pülz <fpuelz@gmx.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @defgroup scriptApi Provider Plugin Script API
 *
 * These classes get exposed to scripts or are used by scripted service provider plugins.
 * Each call to a script from the data engine creates a new thread using ThreadWeaver. Each thread
 * uses it's own QScriptEngine instance to execute the script.
 *
 * Scripts are written in ECMAScript, but they can access Kross to support other languages, ie.
 * Python or Ruby. Kross needs to be imported explicitly. That can be done by adding an
 * @em "extensions" attribute to the @em \<script\> tag in the XML file, like this:
 * @verbatim<script extensions="kross">file.js</script>@endverbatim. Other extensions can also be
 * imported, eg. "qt.core" to use classes provided by the "qt.core" bindings.
 *
 * To use eg. Python code in the script, the following code can then be used in a script:
 * @code
 * // Create Kross action
 * var action = Kross.action( "MyPythonScript" );
 *
 * // Propagate action to the Python script
 * action.addQObject( action, "MyAction" );
 *
 * // Set the interpreter to use, eg. "python", "ruby"
 * action.setInterpreter( "python" );
 *
 * // Set the code to execute and trigger execution
 * action.setCode("import MyAction ; print 'This is Python. name=>',MyAction.interpreter()");
 * action.trigger();
 * @endcode
 * @todo If needed a later version might make this simpler by examining the file type of the script
 *   file and automatically insert the contents of the script file into the setCode() function
 *   like above.
 *
 * @n
 *
 * @section script_exposed Classes Exposed to Scripts
 *
 * Scripts can access some objects that represent classes mentioned here. Only one instance of
 * these classes is available in a script.
 * @li @em ResultObject Stores results, ie. parsed data for departures, arrivals, journeys or
 *   stop suggestions. Available for scripts under the name @b result.
 * @li @em Network Provides network access to scripts and is available for scripts under
 *   the name @b network. This class can create objects of type NetworkRequest using
 *   Network::createRequest() for asynchronous requests. NetworkRequest objects have signals
 *   that scripts can connect to, ie. NetworkRequest::finished() to indicate a finished request.
 * @li @em Storage Stores data between script calls. Can store data in memory or persistently,
 *   ie. on disk. An object of this class is available for scripts under the name @b storage.
 *   The storage object gets shared between calls to the same script (for the same service
 *   provider) and can be use by multiple script instances at the same time.
 * @li @em Helper Provides some helper functions to scripts, available under the name @b helper.
 *
 * @n
 *
 * @section script_functions Script Functions to be Implemented
 *
 * There should be specially named functions in the script, that get called by the data engine.
 * Only the @em getTimetable function @em must be implemented.
 *
 * There is a flexible base script available for providers using the HAFAS API.
 * See @ref examples_script.
 *
 * @subsection script_functions_gettimetable getTimetable( values )
 *   This function @em must be implemented (but that may change).
 *   @note This function gets called to get departures @em or arrivals, depending on the
 *     @em dataType property of the parameter. If arrivals are supported add the
 *     Enums::ProvidesArrivals feature to the return value of the <em>features()</em> script
 *     function.
 *
 *   The only argument contains information about the request with these properties:
 *   @li @em stop: The name/ID of the stop to get departures/arrivals for.
 *   @li @em stopIsId: @c True, if @em stop contains an ID, @c false otherwise.
 *   @li @em dateTime: A Date object with the date and time of the earliest departure/arrival to get.
 *   @li @em dataType: This can be "arrivals" or "departures".
 *   @li @em city: If used, this contains the city name to get departures/arrivals for. Only some
 *     service providers need a separate city value, most are happy with a stop name/stop ID.
 *   @li @em count: The number of departures/arrivals to get.
 *
 * @subsection script_functions_getstopsuggestions getStopSuggestions( values )
 *   Gets called to request stop suggestions. Since it may be called very often it should be fast,
 *   ie. the downloaded data should be as small as possible.
 *   @note There are two types of stop requests:
 *     <em>stop suggestions</em> get requested with the parameters @em stop, @em city and @em count,
 *     while <em>stops by geolocation</em> get requested with @em longitude, @em latitude,
 *     @em distance and @em count and require the Enums::ProvidesStopsByGeoPosition feature to be
 *     returned by the <em>features()</em> script function.
 *
 *   @li @em stop: A part of a stop name to get suggestions for. This is what users can type in, eg.
 *     the beginning of the complete stop name.
 *   @li @em city: If used, this contains the city name to get stop suggestions for. Only some
 *     service providers need a separate city value, most are happy with a part of the stop name.
 *   @li @em longitude: Used together with @em latitude and @em distance for stop requests by
 *     geolocation, which is given by @em longitude and @em latitude.
 *   @li @em latitude: Used together with @em longitude and @em distance for stop requests by
 *     geolocation, which is given by @em longitude and @em latitude.
 *   @li @em distance: The distance in meters around the geolocation given with @em longitude and
 *     @em latitude where to search for stops.
 *   @li @em count: The number of stop suggestions to get.
 *
 * @subsection script_functions_getjourneys getJourneys( values )
 *   Gets called to request journeys (trips from stop A to stop B).
 *   @note Journey requests are also used to get more journeys after a previous request. In this
 *     case the properties @em moreItemsDirection and @em requestData are available and
 *     @em moreItemsDirection does not equal to @em PublicTransport.RequestedItems.
 *     The result currently also needs to include the first received set of journeys.
 *
 *   The only argument contains information about the request with these properties:
 *   @li @em originStop: The name/ID of the start/origin stop, also available as 'stop' property.
 *   @li @em targetStop: The name/ID of the target/destination stop.
 *   @li @em originStopIsId: @c True, if @em originStop contains an ID, @c false otherwise.
 *   @li @em targetStopIsId: @c True, if @em targetStop contains an ID, @c false otherwise.
 *   @li @em dateTime: A Date object with the date and time of the earliest journey to get.
 *   @li @em dataType: This can be "journeys"/"journeysDep" (journeys departing at the given @em dateTime)
 *     or "journeysArr" (journeys arriving at the given @em dateTime).
 *   @li @em city: If used, this contains the city name to get journeys for. Only some
 *     service providers need a separate city value, most are happy with a stop name/stop ID.
 *   @li @em count: The number of journeys to get.
 *   @li @em moreItemsDirection If this is undefined or @em PublicTransport.RequestedItems this
 *     is a normal journey request. Otherwise this is a following journey request to get more
 *     journeys and is @em PublicTransport.EarlierItems or @em PublicTransport.LaterItems.
 *     See Enums::MoreItemsDirection. In this case the field @em requestData is also available.
 *   @li @em requestData This contains data that was stored by the script in the first journeys
 *     request as @em PublicTransport.RequestData. If this is not a following journey request
 *     this property is not used. It can be used to eg. store a request ID.
 *
 * @subsection script_functions_getadditionaldata getAdditionalData( values )
 *   Gets called to request additional data for an already received timetable item (eg. a
 *   departure). The provider itself decides what additional data actually is. For example the
 *   HAFAS base script uses this to get route information for departures/arrivals, which are not
 *   available otherwise. The only argument contains information about the timetable item for which
 *   to get additional data for and has these properties:
 *   @li @em stop: The name of the stop, that was used to get the timetable item.
 *   @li @em city: If used, this contains the city name that was used to get the timetable item.
 *     Only some service providers need a separate city value, most are happy with a stop
 *     name/stop ID.
 *   @li @em dataType: The type of data that was requested to get the timetable item, eg.
 *     "departures", "arrivals", etc.
 *   @li @em dateTime: A Date object with the date and time of the timetable item,
 *     eg. it's departure.
 *   @li @em transportLine: The transport line of the timetable item.
 *   @li @em target: The target of the timetable item.
 *   @li @em routeDataUrl: An URL to a document that contains route information, ie. additional
 *     data. If this is empty it needs to be found out somehow. For example HAFAS providers load
 *     the departure board again but in a different format that includes these URLs (and caches
 *     URLs for multiple timetable items for later use). To do so and to find the correct timetable
 *     item in the other format, the other properties like @em dateTime, @em transportLine and
 *     @em target get used.
 *
 * @subsection script_functions_features features()
 *   Can be implemented to make the data engine aware of supported features.
 *   See Enums::ProviderFeature for a list of available features. Those enumerables are available
 *   to scripts in the @em PublicTransport object.
 *
 *   Some features like @em PublicTransport.ProvidesJourneys are detected automatically
 *   (<em>getJourneys()</em> function implemented?). Others are only used to inform the user,
 *   eg. @em PublicTransport.ProvidesPricing. Some features are also required in the returned
 *   list of this function for those features to actually work,
 *   eg. @em PublicTransport.ProvidesStopGeoPosition.
 *
 * @n
 *
 * @section script_collecting_items Collecting Parsed Items
 * The object @b result (ResultObject) gets used by scripts to collect parsed departures/
 * arrivals/journeys/stop suggestions. It provides a function ResultObject::addData(), which
 * accepts an object with properties that have special names. A simple departure item can be added
 * to the result object like this:
 * @code
 * result.addData({ DepartureDateTime: new Date(),
 *                  VehicleType: PublicTransport.Bus,
 *                  Target: "SomeTarget" });
 * @endcode
 *
 * Another possibility is to assign the properties when they get parsed, like this:
 * @code
 * var departure = {};
 * departure.DepartureDateTime = new Date();
 * departure.VehicleType = PublicTransport.Bus;
 * departure.Target = "SomeTarget";
 * result.addData( departure );
 * @endcode
 *
 * You can also use enumerable values to store data (available in @em PublicTransport):
 * @code
 * var departure = {};
 * departure[ PublicTransport.DepartureDateTime ] = new Date();
 * departure[ PublicTransport.VehicleType ] = PublicTransport.Bus;
 * departure[ PublicTransport.Target ] = "SomeTarget";
 * result.addData( departure );
 * @endcode
 *
 * The names of the properties are important, but upper or lower case does not matter.
 * All entries in the Enums::TimetableInformation enumerable can be used to add information, look
 * there fore more detailed information. This enumerable is a central point of the Public Transport
 * data engine and gets used by all service provider plugin types to store information about results.
 *
 * @n
 * @subsection script_collecting_items_departures Information Types Used for Departures/Arrivals
 * @em DepartureDateTime, @em DepartureDate, @em DepartureTime, @em TypeOfVehicle,
 * @em TransportLine, @em FlightNumber (alias for @em TransportLine), @em Target,
 * @em TargetShortened, @em Platform, @em Delay, @em DelayReason, @em JourneyNews,
 * @em JourneyNewsOther, @em JourneyNewsLink, @em Operator, @em Status, @em RouteStops,
 * @em RouteStopsShortened, @em RouteTimes, @em RouteTimesDeparture, @em RouteTimesArrival,
 * @em RouteExactStops, @em RouteTypesOfVehicles, @em RouteTransportLines,
 * @em RoutePlatformsDeparture, @em RoutePlatformsArrival, @em RouteTimesDepartureDelay,
 * @em RouteTimesArrivalDelay, @em IsNightLine (currently unused).
 * @note At least these information types are needed to form a valid departure/arrival object:
 *   @em DepartureDateTime or @em DepartureTime (the date can be omitted, but that can produce
 *   wrong guessed dates), @em TypeOfVehicle and @em TransportLine.
 * @note When arrivals are requested, @em DepartureDateTime, @em DepartureDate and
 *   @em DepartureTime stand actually for the arrival date/time. The names that start with @em Arrival are
 *   used for journeys only.
 *   @todo This might change, allowing both for arrivals.
 * @see Enums::TimetableInformation
 *
 * @n
 * @subsection script_collecting_items_journeys Information Types Used for Journeys
 * @em DepartureDateTime, @em DepartureDate, @em DepartureTime, @em Duration, @em StartStopName,
 * @em StartStopID, @em TargetStopName, @em TargetStopID, @em ArrivalDateTime, @em ArrivalDate,
 * @em ArrivalTime, @em Changes, @em TypesOfVehicleInJourney, @em Pricing, @em RouteStops,
 * @em RouteStopsShortened, @em RouteTimes, @em RouteTimesDeparture, @em RouteTimesArrival,
 * @em RouteExactStops, @em RouteTypesOfVehicles, @em RouteTransportLines,
 * @em RoutePlatformsDeparture, @em RoutePlatformsArrival, @em RouteTimesDepartureDelay,
 * @em RouteTimesArrivalDelay.
 * @note At least these information types are needed to form a valid journey object:
 *   @em DepartureDateTime or @em DepartureTime, @em ArrivalDateTime or @em ArrivalTime,
 *   @em StartStopName and @em TargetStopName.
 * @see Enums::TimetableInformation
 *
 * @n
 * @subsection script_collecting_items_journeys Information Types Used for Stop Suggestions
 *  @em StopName, @em StopID, @em StopWeight, @em StopCity, @em StopCountryCode.
 * @note Only @em StopName is required to form a valid stop suggestion object.
 * @see Enums::TimetableInformation
 *
 * @n
 * @subsection script_collecting_items_vehicletypes Vehicle Types
 *
 * Vehicle types can be given as enumerable values or names (in @em TypeOfVehicle,
 * @em RouteTypesOfVehicles, @em TypesOfVehicleInJourney), see @ref Enums::VehicleType. @n
 *
 * These are the enumerables of currently supported vehicle types (the names without
 * "PublicTransport." can also be used as vehicle type):@n
 * <table>
 * <tr><td></td><td>PublicTransport.Unknown</td>
 * <tr><td>@image html hi16-app-vehicle_type_tram.png
 * </td><td>PublicTransport.Tram</td>
 * <tr><td>@image html hi16-app-vehicle_type_bus.png
 * </td><td>PublicTransport.Bus</td>
 * <tr><td>@image html hi16-app-vehicle_type_subway.png
 * </td><td>PublicTransport.Subway</td>
 * <tr><td>@image html hi16-app-vehicle_type_train_interurban.png
 * </td><td>PublicTransport.InterurbanTrain</td>
 * <tr><td>@image html hi16-app-vehicle_type_metro.png
 * </td><td>PublicTransport.Metro</td>
 * <tr><td>@image html hi16-app-vehicle_type_trolleybus.png
 * </td><td>PublicTransport.TrolleyBus</td>
 * <tr><td>@image html hi16-app-vehicle_type_train_regional.png
 * </td><td>PublicTransport.RegionalTrain</td>
 * <tr><td>@image html hi16-app-vehicle_type_train_regional.png
 * </td><td>PublicTransport.RegionalExpressTrain</td>
 * <tr><td>@image html hi16-app-vehicle_type_train_interregional.png
 * </td><td>PublicTransport.InterregionalTrain</td>
 * <tr><td>@image html hi16-app-vehicle_type_train_intercity.png
 * </td><td>PublicTransport.IntercityTrain</td>
 * <tr><td>@image html hi16-app-vehicle_type_train_highspeed.png
 * </td><td>PublicTransport.HighSpeedTrain</td>
 * <tr><td>@image html hi16-app-vehicle_type_feet.png
 * </td><td>PublicTransport.Footway" (for journeys to walk from one intermediate stop to the next)</td>
 * <tr><td>@image html hi16-app-vehicle_type_ferry.png
 * </td><td>PublicTransport.Ferry</td>
 * <tr><td>@image html hi16-app-vehicle_type_ferry.png
 * </td><td>PublicTransport.Ship</td>
 * <tr><td>@image html hi16-app-vehicle_type_plane.png
 * </td><td>PublicTransport.Plane</td>
 * </table>
 *
 * @see Enums::TimetableInformation
 * @see Enums::VehicleType
 *
 * @n
 *
 * @section script_advanced Advanced Features
 *
 * Scripts can use some additional features. For example the Storage can be used to store data
 * that would otherwise have to be downloaded and parsed over and over again. Data can be stored
 * in memory (for the current session only) or persistently on disk (using KConfig).
 * @see Storage
 *
 * @n
 *
 * The ResultObject class has additional functions other than ResultObject::addData(). For example
 * the ResultObject::publish() function can be used to tell the data engine to publish the items
 * parsed so far to visualizations. A good use case is to call publish() when a document
 * has been read but for more results another document needs to be downloaded first.
 * @note By default data is automatically published after the first few items to provide
 *   visualizations with data as soon as possible. Use ResultObject::enableFeature() to change
 *   this behaviour.
 *
 * There is also a Hint enumeration to give hints to the data engine. Use ResultObject::giveHint()
 * to give a hint.
 **/
