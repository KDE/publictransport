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

/** @file
 * @brief This file contains helper classes to be used from service provider plugin scripts.
 *
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef SCRIPTING_HEADER
#define SCRIPTING_HEADER

// Own includes
#include "enums.h"
#include "departureinfo.h" // TODO for eg. PublicTransportInfoList typedef

// Qt includes
#include <QScriptEngine>
#include <QScriptable> // Base class
// #include <QMetaType> // For Q_DECLARE_METATYPE

class PublicTransportInfo;
class ServiceProviderData;
class KConfigGroup;
class QScriptContextInfo;
class QNetworkRequest;
class QReadWriteLock;
class QNetworkReply;
class QNetworkAccessManager;
class QMutex;

/** @brief Stores information about a departure/arrival/journey/stop suggestion. */
typedef QHash<TimetableInformation, QVariant> TimetableData;

/** @brief Namespace for classes exposed to scripts. */
namespace Scripting {

class Network;

/**
 * @defgroup scripting Classes Used for Scripts
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
 * @warning Bindings are not the same between distributions. At least I noticed that QDateTime
 *   is provided in Kubuntu 11.10 when "qt.core" gets imported, but not in openSuse 12.1.
 *   QDateTime values are also always converted to script Date objects.
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
 * @subsection script_functions_gettimetable getTimetable( values )
 *   This function @em must be implemented. It is responsible for downloading and parsing of a
 *   document containing information about departures or arrivals. Departures for all transport
 *   lines and vehicle types should be collected. The argument to this function tells the script
 *   what data it should get. It has the following properties:
 *   @li @em stop: The name/ID of the stop to get departures/arrivals for.
 *   @li @em dateTime: A Date object with the date and time of the earliest departure/arrival to
 *     get.
 *   @li @em maxCount: The maximal number of departures/arrivals to get.
 *   @li @em dataType: This can be "arrivals" or "departures".
 *   @li @em city: If used, this contains the city name to get departures/arrivals for. Only some
 *     service providers need a separate city value, most are happy with a stop name/stop ID.
 *
 * @subsection script_functions_getstopsuggestions getStopSuggestions( values )
 *   This function can be implemented to provide stop suggestions. It is responsible for
 *   downloading and parsing of a document containing the stop suggestions. The argument to this
 *   function tells the script what data it should get (similar to the @em getTimetable function)
 *   and has the following properties:
 *   @li @em stop: A string to get stop suggestions for. This is what users can type in, eg. the
 *     beginning of the complete stop name.
 *   @li @em maxCount: The maximal number of stop suggestions to get.
 *   @li @em city: If used, this contains the city name to get stop suggestions for. Only some
 *     service providers need a separate city value, most are happy with a part of the stop name.
 *
 * @subsection script_functions_getjourneys getJourneys( values )
 *   This function can be implemented to provide journey information. A journey is here a trip from
 *   an origin stop to a target stop. It is responsible for downloading and parsing of a
 *   document containing information about journeys. Journeys for all transport lines and vehicle
 *   types should be collected. The argument to this function tells the script what data it should
 *   get and has the following properties:
 *   @li @em originStop: The name/ID of the start/origin stop, also available as 'stop' property.
 *   @li @em targetStop: The name/ID of the target/destination stop.
 *   @li @em dateTime: A Date object with the date and time of the earliest journey to get.
 *   @li @em maxCount: The maximal number of journeys to get.
 *   @li @em dataType: This can be "journeysDep" (journeys departing at the given @em dateTime)
 *     or "journeysArr" (journeys arriving at the given @em dateTime).
 *   @li @em city: If used, this contains the city name to get journeys for. Only some
 *     service providers need a separate city value, most are happy with a stop name/stop ID.
 *
 * @subsection script_functions_usedtimetableinformations usedTimetableInformations()
 *   Can be implemented to provide information about what information types the script can return.
 *   This can be done by simply returning a list of strings, where each string is the name of a
 *   type of information. Only optional information that the script provides needs to be returned
 *   by this function. This list gets used for the feature list of scripted service provider plugins.
 *   @li @b Arrivals The script can parse arrivals
 *   @li @b Delay The script can parse delay information
 *   @li @b DelayReason The script can also parse a string describing the reason of a delay
 *   @li @b Platform The platform where a departure/arrival happens can be parsed
 *   @li @b JourneyNews Additional information in text form about departures/arrivals/journeys
 *     can be parsed
 *   @li @b StopID Stop IDs can be parsed and used instead of stop names to prevent ambiguities
 *   @li @b Pricing Pricing information can be parsed for journeys
 *   @li @b Changes The number of changes in a journey can be parsed
 *   @li @b RouteStops A list of stop names on the route can be parsed
 *   @li @b RouteTimes A list of times at which the vehicle passes the stops in RouteStops can
 *     be parsed
 *   @li @b RoutePlatformsDeparture A list of platforms in the route where the vehicle departs
 *     can be parsed
 *   @li @b RoutePlatformsArrival A list of platforms in the route where the vehicle arrives
 *     can be parsed
 *   @li @b RouteTimesDeparture A list of times at which the vehicle departs from the stops
 *     in RouteStops can be parsed, eg. for journeys
 *   @li @b RouteTimesArrival A list of times at which the vehicle arrives from the stops
 *     in RouteStops can be parsed, eg. for journeys
 *   @li @b RouteTypesOfVehicles A list of vehicle types used for subtrips in a journey (between
 *     stops in RouteStops)
 *   @li @b RouteTransportLines A list of transport line strings of vehicles used for subtrips
 *     in a journy
 *
 *   @see TimetableInformation
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
 *                  VehicleType: "Bus",
 *                  Target: "SomeTarget" });
 * @endcode
 *
 * Another possibility is to assign the properties when they get parsed, like this:
 * @code
 * var departure = {};
 * departure.departureDateTime = new Date();
 * departure.vehicleType = "Bus";
 * departure.target = "SomeTarget";
 * result.addData( departure );
 * @endcode
 *
 * The names of the properties are important, but upper or lower case does not matter.
 * All entries in the TimetableInformation enumerable can be used to add information, look
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
 *
 * @n
 * @subsection script_collecting_items_journeys Information Types Used for Stop Suggestions
 *  @em StopName, @em StopID, @em StopWeight, @em StopCity, @em StopCountryCode.
 * @note Only @em StopName is required to form a valid stop suggestion object.
 *
 * @n
 * @subsection script_collecting_items_vehicletypes Vehicle Types
 *
 * Vehicle types can be given as strings (in @em TypeOfVehicle, @em RouteTypesOfVehicles,
 * @em TypesOfVehicleInJourney).@n
 *
 * The easiest/safest way to provide information about what type of vehicle gets used is
 * providing it as one of these strings of currently supported vehicle types:@n
 * <table>
 * <tr><td></td><td>"Unknown"</td>
 * <tr><td>@image html hi16-app-vehicle_type_tram.png
 * </td><td>"Tram"</td>
 * <tr><td>@image html hi16-app-vehicle_type_bus.png
 * </td><td>"Bus"</td>
 * <tr><td>@image html hi16-app-vehicle_type_subway.png
 * </td><td>"Subway"</td>
 * <tr><td>@image html hi16-app-vehicle_type_train_interurban.png
 * </td><td>"TrainInterurban"</td>
 * <tr><td>@image html hi16-app-vehicle_type_metro.png
 * </td><td>"Metro"</td>
 * <tr><td>@image html hi16-app-vehicle_type_trolleybus.png
 * </td><td>"TrolleyBus"</td>
 * <tr><td>@image html hi16-app-vehicle_type_train_regional.png
 * </td><td>"RegionalTrain"</td>
 * <tr><td>@image html hi16-app-vehicle_type_train_regional.png
 * </td><td>"RegionalExpressTrain"</td>
 * <tr><td>@image html hi16-app-vehicle_type_train_interregional.png
 * </td><td>"InterregionalTrain"</td>
 * <tr><td>@image html hi16-app-vehicle_type_train_intercity.png
 * </td><td>"IntercityTrain"</td>
 * <tr><td>@image html hi16-app-vehicle_type_train_highspeed.png
 * </td><td>"HighSpeedTrain"</td>
 * <tr><td>@image html hi16-app-vehicle_type_feet.png
 * </td><td>"Feet" (for journeys to walk from one intermediate stop to the next)</td>
 * <tr><td>@image html hi16-app-vehicle_type_ferry.png
 * </td><td>"Ferry"</td>
 * <tr><td>@image html hi16-app-vehicle_type_ferry.png
 * </td><td>"Ship"</td>
 * <tr><td>@image html hi16-app-vehicle_type_plane.png
 * </td><td>"Plane"</td>
 * </table>
 *
 * @see TimetableInformation
 * @see VehicleType
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

/**
 * @brief Represents one asynchronous request, created with Network::createRequest().
 *
 * To get notified about new data, connect to either the finished() or the readyRead() signal.
 *
 * @ingroup scripting
 * @since 0.10
 **/
class NetworkRequest : public QObject {
    Q_OBJECT
    Q_PROPERTY( QString url READ url )
    Q_PROPERTY( bool isRunning READ isRunning )
    Q_PROPERTY( QString postData READ postData )
    friend class Network;

public:
    /** @brief Creates an invalid request object. Needed for Q_DECLARE_METATYPE. */
    NetworkRequest( QObject* parent = 0 );

    /** @brief Create a new request object for @p url, managed by @p network. */
    explicit NetworkRequest( const QString &url, Network *network, QObject* parent = 0 );

    /** @brief Destructor. */
    virtual ~NetworkRequest();

    /**
     * @brief The URL of this request.
     *
     * @note The URL can not be changed, a request object is only used for @em one request.
     * @ingroup scripting
     **/
    QString url() const { return m_url; };

    /**
     * @brief Whether or not the request is currently running.
     * @ingroup scripting
     **/
    bool isRunning() const { return m_reply; };

    /**
     * @brief Whether or not the request is finished (successful or not), ie. was running.
     * @ingroup scripting
     **/
    bool isFinished() const { return m_isFinished; };

    /**
     * @brief Set the data to sent to the server when using Network::post().
     *
     * This function automatically sets the "ContentType" header of the request to the used
     * charset. If you want to use another value for the "ContentType" header than the
     * data is actually encoded in, you can change the header using setHeader() after calling
     * this function.
     * @note If the request is already started, no more POST data can be set and this function
     *   will do nothing.
     *
     * @param postData The data to be POSTed.
     * @param charset The charset to encode @p postData with. If charset is an empty string (the
     *   default) the "ContentType" header gets used if it was set using setHeader(). Otherwise
     *   utf8 gets used.
     * @see isRunning()
     * @ingroup scripting
     **/
    Q_INVOKABLE void setPostData( const QString &postData, const QString &charset = QString() );

    /** @brief Get the data to sent to the server when using Network::post(). */
    QString postData() const { return m_postData; };

    /** @brief Get the @p header decoded using @p charset. */
    Q_INVOKABLE QString header( const QString &header, const QString& charset ) const;

    /**
     * @brief Set the @p header of this request to @p value.
     *
     * @note If the request is already started, no more headers can be set and this function
     *   will do nothing.
     *
     * @param header The name of the header to change.
     * @param value The new value for the @p header.
     * @param charset The charset to encode @p header and @p value with. If charset is an empty
     *   string (the default) the "ContentType" header gets used if it was set using setHeader().
     *   Otherwise utf8 gets used.
     * @see isRunning()
     * @ingroup scripting
     **/
    Q_INVOKABLE void setHeader( const QString &header, const QString &value,
                                const QString &charset = QString() );

public Q_SLOTS:
    /**
     * @brief Aborts this (running) request.
     *
     * @note If the request has not already been started, it cannot be aborted, of course,
     *   and this function will do nothing.
     **/
    Q_INVOKABLE void abort();

Q_SIGNALS:
    /**
     * @brief Emitted when this request was started.
     * @ingroup scripting
     **/
    void started();

    /**
     * @brief Emitted when this request got aborted.
     * @ingroup scripting
     **/
    void aborted();

    /**
     * @brief Emitted when this request has finished.
     *
     * @param data The complete data downloaded for this request.
     * @ingroup scripting
     **/
    void finished( const QString &data = QString() );

    /**
     * @brief Emitted when new data is available for this request.
     *
     * @param data New downloaded data for this request.
     * @ingroup scripting
     **/
    void readyRead( const QString &data );

    // TODO Do the decoding manually in the script, if needed
    void finishedNoDecoding();

protected Q_SLOTS:
    void slotFinished();
    void slotReadyRead();

protected:
    void started( QNetworkReply* reply );
    QByteArray postDataByteArray() const;
    QNetworkRequest *request() const;
    QByteArray getCharset( const QString &charset = QString() ) const;
    bool isValid() const;

private:
    const QString m_url;
    Network *m_network;
    bool m_isFinished;
    QNetworkRequest *m_request;
    QNetworkReply *m_reply;
    QByteArray m_data;
    QByteArray m_postData;
};

/**
 * @brief Provides network access to scripts.
 *
 * An instance of the Network class is available for scripts as @b network. It can be used to
 * synchronously or asynchronously download data. It doesn't really matter what method the script
 * uses, because every call of a script function from the data engine happens in it's own thread
 * and won't block anything. For the threads ThreadWeaver gets used with jobs of type ScriptJob.
 *
 * To download a document synchronously simply call getSynchronous() with the URL to download.
 * When the download is finished getSynchronous(this request using the POST method) returns and the script can start parsing the
 * document. There is a default timeout of 30 seconds. If a requests takes more time it gets
 * aborted. To define your own timeout you can give a second argument to getSynchronous(), which
 * is the timeout in milliseconds.
 * @note downloadSynchronous() is an alias for getSynchronous().
 *
 * @code
 * var document = network.getSynchronous( url );
 * // Parse the downloaded document here
 * @endcode
 * @n
 *
 * To create a network request which should be executed asynchronously use createRequest() with
 * the URL of the request as argument. The script should then connect to either the finished()
 * or readyRead() signal of the network request object. The request gets started by calling
 * get() / download(). To only get the headers use head().
 * The example below shows how to asynchronously request a document from within a script:
 * @code
    var request = network.createRequest( url );
    request.finished.connect( handler );
    network.get( request );

    function handler( document ) {
        // Parse the downloaded document here
    };
   @endcode
 * @n
 *
 * If a script needs to use the POST method to request data use post(). The data to be sent in
 * a POST request can be set using NetworkRequest::setPostData().
 *
 * @note One request object created with createRequest() can not be used multiple times in
 *   parallel. To start another request create a new request object.
 *
 * @ingroup scripting
 * @since 0.10
 **/
class Network : public QObject {
    Q_OBJECT
    Q_PROPERTY( QString lastUrl READ lastUrl )
    Q_PROPERTY( bool lastDownloadAborted READ lastDownloadAborted )
    Q_PROPERTY( QString fallbackCharset READ fallbackCharset )
    Q_PROPERTY( int runningRequestCount READ runningRequestCount )
    friend class NetworkRequest;

public:
    /** @brief The default timeout in milliseconds for network requests. */
    static const int DEFAULT_TIMEOUT = 30000;

    /** @brief Constructor. */
    explicit Network( const QByteArray &fallbackCharset = QByteArray(), QObject* parent = 0 );

    /** @brief Destructor. */
    virtual ~Network();

    /**
     * @brief Get the last requested URL.
     *
     * The last URL gets updated every time a request gets started, eg. using get(), post(),
     * getSynchronous(), download(), downloadSynchronous(), etc.
     **/
    Q_INVOKABLE QString lastUrl() const;

    /**
     * @brief Clears the last requested URL.
     **/
    Q_INVOKABLE void clear();

    /**
     * @brief Returns @c true, if the last download was aborted before it was ready.
     *
     * Use lastUrl() to get the URL of the aborted download. Downloads may be aborted eg. by
     * closing plasma.
     **/
    Q_INVOKABLE bool lastDownloadAborted() const;

    /**
     * @brief Download the document at @p url synchronously.
     *
     * After the request is sent an QEventLoop gets started to wait for the reply to finish.
     * If the @p timeout expires or the abort() slot gets called, the download gets stopped.
     *
     * @param url The URL to download.
     * @param timeout Maximum time in milliseconds to wait for the reply to finish. If smaller than 0,
     *   no timeout gets used.
     * @ingroup scripting
     **/
    Q_INVOKABLE QString getSynchronous( const QString &url, int timeout = DEFAULT_TIMEOUT );

    /**
     * @brief This is an alias for getSynchronous().
     * @ingroup scripting
     **/
    Q_INVOKABLE inline QString downloadSynchronous( const QString &url, int timeout = DEFAULT_TIMEOUT ) {
        return getSynchronous(url, timeout); };

    /**
     * @brief Creates a new NetworkRequest for asynchronous network access.
     *
     * @note Each NetworkRequest object can only be used once for one download.
     *
     * @see get, download, post, head
     * @ingroup scripting
     **/
    Q_INVOKABLE NetworkRequest *createRequest( const QString &url );

    /**
     * @brief Perform the network @p request asynchronously.
     *
     * @param request The NetworkRequest object created with createRequest().
     * @ingroup scripting
     **/
    Q_INVOKABLE void get( NetworkRequest *request );

    /**
     * @brief Perform the network @p request asynchronously using POST method.
     *
     * @param request The NetworkRequest object created with createRequest().
     * @ingroup scripting
     **/
    Q_INVOKABLE void post( NetworkRequest *request );

    /**
     * @brief Perform the network @p request asynchronously, but only get headers.
     *
     * @param request The NetworkRequest object created with createRequest().
     * @ingroup scripting
     **/
    Q_INVOKABLE void head( NetworkRequest *request );

    /**
     * @brief This is an alias for get().
     * @ingroup scripting
     **/
    Q_INVOKABLE inline void download( NetworkRequest *request ) { get(request); };

    /**
     * @brief Returns whether or not there are asynchronous requests running in the background.
     * @see runningRequests
     * @ingroup scripting
     **/
    Q_INVOKABLE bool hasRunningRequests() const;

    /**
     * @brief Returns a list of all NetworkRequest objects, representing all running requests.
     *
     * If hasRunningRequests() returns @c false, this will return an empty list.
     * @see hasRunningRequests
     * @ingroup scripting
     **/
    Q_INVOKABLE QList< NetworkRequest* > runningRequests() const;

    /**
     * @brief Returns the number of currently running requests.
     **/
    Q_INVOKABLE int runningRequestCount() const;

    /**
     * @brief Returns the charset to use for decoding documents, if it cannot be detected.
     *
     * The fallback charset can be selected in the XML file, as \<fallbackCharset\>-tag.
     **/
    Q_INVOKABLE QByteArray fallbackCharset() const;

Q_SIGNALS:
    /**
     * @brief Emitted when an asynchronous request has been started.
     *
     * This signal is @em not emitted if the network gets accessed synchronously.
     * @param request The request that has been started.
     * @ingroup scripting
     **/
    void requestStarted( NetworkRequest *request );

    /**
     * @brief Emitted when an asynchronous request has finished.
     *
     * This signal is @em not emitted if the network gets accessed synchronously.
     * @param request The request that has finished.
     * @param data Received data decoded to a string.
     * @ingroup scripting
     **/
    void requestFinished( NetworkRequest *request, const QString &data = QString() );

    /**
     * @brief Emitted when all requests are finished.
     *
     * This signal gets emitted just after emitting requestFinished(), if there are no more running
     * requests.
     * @ingroup scripting
     **/
    void allRequestsFinished();

    /**
     * @brief Emitted when an asynchronous request got aborted.
     *
     * This signal is @em not emitted if the network gets accessed synchronously.
     * @param request The request that was aborted.
     * @ingroup scripting
     **/
    void requestAborted( NetworkRequest *request );

public Q_SLOTS:
    /**
     * @brief Aborts all running (asynchronous) downloads.
     * @ingroup scripting
     **/
    void abortAllRequests();

protected Q_SLOTS:
    void slotRequestStarted();
    void slotRequestFinished( const QString &data = QString() );
    void slotRequestAborted();

protected:
    bool checkRequest( NetworkRequest *request );

private:
    QMutex *m_mutex;
    const QByteArray m_fallbackCharset;
    QNetworkAccessManager *m_manager;
    bool m_quit;
    QString m_lastUrl;
    bool m_lastDownloadAborted;
    QList< NetworkRequest* > m_runningRequests;
};

/**
 * @brief A helper class for scripts.
 *
 * An instance of this class gets published to scripts as <em>"helper"</em>.
 * Scripts can use it's functions, like here:
 * @code
 * var stripped = helper.stripTags("<div>Test</div>");
 * // stripped == "Test"
 *
 * var timeValues = helper.matchTime( "15:28" );
 * // timeValues == { hour: 15, minutes: 28, error: false }
 *
 * var timeString = helper.formatTime( timeValues[0], timeValues[1] );
 * // timeString == "15:28"
 *
 * var duration = helper.duration("15:20", "15:45");
 * // duration == 25
 *
 * var time2 = helper.addMinsToTime("15:20", duration);
 * // time2 == "15:45"
 *
 * helper.debug("Debug message, eg. something unexpected happened");
 * @endcode
 *
 * @ingroup scripting
 **/
class Helper : public QObject, protected QScriptable {
    Q_OBJECT

public:
    /**
     * @brief Creates a new helper object.
     *
     * @param serviceProviderId The ID of the service provider this Helper object is created for.
     * @param parent The parent object.
     **/
    explicit Helper( const QString &serviceProviderId, QObject* parent = 0 ) : QObject( parent ) {
        m_serviceProviderId = serviceProviderId;
    };

    /**
     * @brief Prints @p message on stdout and logs it in a file.
     *
     * Logs the error message with the given data string, eg. the HTML code where parsing failed.
     * The message gets also send to stdout with a short version of the data
     * The log file is normally located at "~/.kde4/share/apps/plasma_engine_publictransport/serviceproviders.log".
     *
     * @param message The error message.
     * @param failedParseText The text in the source document where parsing failed.
     **/
    Q_INVOKABLE void error( const QString &message, const QString &failedParseText = QString() );

//     /** TODO Implement decode() function for scripts, eg. for version 0.3.1
//      * @brief Decodes HTML entities in @p html.
//      *
//      * For example "&nbsp;" gets replaced by " ".
//      * HTML entities which include a charcode, eg. "&#100;" are also replaced, in the example
//      * by the character for the charcode 100, ie. QChar(100).
//      *
//      * @param html The string to be decoded.
//      * @return @p html with decoded HTML entities.
//      **/
//     Q_INVOKABLE static QString decode( const QString &html );

    /**
     * @brief Decodes HTML entities in @p html.
     *
     * For example "&nbsp;" gets replaced by " ".
     * HTML entities which include a charcode, eg. "&#100;" are also replaced, in the example
     * by the character for the charcode 100, ie. QChar(100).
     *
     * @param html The string to be decoded.
     * @return @p html with decoded HTML entities.
     **/
    Q_INVOKABLE static QString decodeHtmlEntities( const QString &html );

    /**
     * @brief Trims spaces from the beginning and the end of the given string @p str.
     *
     * @note The HTML entitiy <em>&nbsp;</em> is also trimmed.
     *
     * @param str The string to be trimmed.
     * @return @p str without spaces at the beginning or end.
     **/
    Q_INVOKABLE static QString trim( const QString &str );

    /**
     * @brief Removes all HTML tags from str.
     *
     * This function works with attributes which contain a closing tab as strings.
     *
     * @param str The string from which the HTML tags should be removed.
     * @return @p str without HTML tags.
     **/
    Q_INVOKABLE static QString stripTags( const QString &str );

    /**
     * @brief Makes the first letter of each word upper case, all others lower case.
     *
     * @param str The input string.
     * @return @p str in camel case.
     **/
    Q_INVOKABLE static QString camelCase( const QString &str );

    /**
     * @brief Extracts a block from @p str, which begins at the first occurance of @p beginString
     *   in @p str and end at the first occurance of @p endString in @p str.
     *
     * @deprecated Does only work with fixed strings. Use eg. findFirstHtmlTag() instead.
     * @bug The returned string includes beginString but not endString.
     *
     * @param str The input string.
     * @param beginString A string to search for in @p str and to use as start position.
     * @param endString A string to search for in @p str and to use as end position.
     * @return The text block in @p str between @p beginString and @p endString.
     **/
    Q_INVOKABLE static QString extractBlock( const QString &str,
            const QString &beginString, const QString &endString );

    /**
     * @brief Get a map with the hour and minute values parsed from @p str using @p format.
     *
     * QVariantMap gets converted to an object in scripts. The result can be used in the script
     * like this:
     * @code
        var time = matchTime( "15:23" );
        if ( !time.error ) {
            var hour = time.hour;
            var minute = time.minute;
        }
       @endcode
     *
     * @param str The string containing the time to be parsed, eg. "08:15".
     * @param format The format of the time string in @p str. Default is "hh:mm".
     * @return A map with two values: 'hour' and 'minute' parsed from @p str. On error it contains
     *   an 'error' value of @c true.
     * @see formatTime
     **/
    Q_INVOKABLE static QVariantMap matchTime( const QString &str, const QString &format = "hh:mm" );

    /**
     * @brief Get a date object parsed from @p str using @p format.
     *
     * @param str The string containing the date to be parsed, eg. "2010-12-01".
     * @param format The format of the time string in @p str. Default is "YY-MM-dd".
     * @see formatDate
     **/
    Q_INVOKABLE static QDate matchDate( const QString &str, const QString &format = "yyyy-MM-dd" );

    /**
     * @brief Formats the time given by the values @p hour and @p minute
     *   as string in the given @p format.
     *
     * @param hour The hour value of the time.
     * @param minute The minute value of the time.
     * @param format The format of the time string to return. Default is "hh:mm".
     * @return The formatted time string.
     * @see matchTime
     **/
    Q_INVOKABLE static QString formatTime( int hour, int minute, const QString &format = "hh:mm" );

    /**
     * @brief Formats the time given by the values @p hour and @p minute
     *   as string in the given @p format.
     *
     * @param year The year value of the date.
     * @param month The month value of the date.
     * @param day The day value of the date.
     * @param format The format of the date string to return. Default is "yyyy-MM-dd".
     * @return The formatted date string.
     * @see matchTime
     **/
    Q_INVOKABLE static QString formatDate( int year, int month, int day,
                                           const QString &format = "yyyy-MM-dd" );

    /**
     * @brief Formats @p dateTime using @p format.
     **/
    Q_INVOKABLE static QString formatDateTime( const QDateTime &dateTime,
                                               const QString &format = "yyyy-MM-dd" );

    /**
     * @brief Calculates the duration in minutes from the time in @p time1 until @p time2.
     *
     * @param time1 A string with the start time, in the given @p format.
     * @param time2 A string with the end time, in the given @p format.
     * @param format The format of @p time1 and @p time2. Default is "hh:mm".
     * @return The number of minutes from @p time1 until @p time2. If @p time2 is earlier than
     *   @p time1 a negative value gets returned.
     **/
    Q_INVOKABLE static int duration( const QString &time1, const QString &time2,
                                     const QString &format = "hh:mm" );

    /**
     * @brief Adds @p minsToAdd minutes to the given @p time.
     *
     * @param time A string with the base time.
     * @param minsToAdd The number of minutes to add to @p time.
     * @param format The format of @p time. Default is "hh:mm".
     * @return A time string formatted in @p format with the calculated time.
     **/
    Q_INVOKABLE static QString addMinsToTime( const QString &time, int minsToAdd,
                                              const QString &format = "hh:mm" );

    /**
     * @brief Adds @p daysToAdd days to the date in @p dateTime.
     *
     * @param dateTime A string with the base time.
     * @param daysToAdd The number of minutes to add to @p time.
     * @param format The format of @p time. Default is "hh:mm".
     * @return A time string formatted in @p format with the calculated time.
     **/
    Q_INVOKABLE static QDateTime addDaysToDate( const QDateTime &dateTime, int daysToAdd );

    /**
     * @brief Adds @p daysToAdd days to @p date.
     *
     * @param date A string with the base date.
     * @param daysToAdd The number of days to add to @p date.
     * @param format The format of @p date. Default is "yyyy-MM-dd".
     * @return A date string formatted in @p format with the calculated date.
     **/
    Q_INVOKABLE static QString addDaysToDate( const QString &date, int daysToAdd,
                                              const QString &format = "yyyy-MM-dd" );

    /**
     * @brief Splits @p str at @p sep, but skips empty parts.
     *
     * @param string The string to split.
     * @param separator The separator.
     * @return A list of string parts.
     **/
    Q_INVOKABLE static QStringList splitSkipEmptyParts( const QString &string,
                                                        const QString &separator );

    /**
     * @brief Finds the first occurrence of an HTML tag with @p tagName in @p str.
     *
     * @param str The string containing the HTML tag to be found.
     * @param tagName The name of the HTML tag to be found, ie. &lt;tagName&gt;.
     * @param options The same as in findHtmlTags(), "maxCount" will be set to 1.
     *
     * @b Example:
     * @code
     * // This matches the first &lt;table&gt; tag found in html
     * // which has a class attribute with a value that matches
     * // the regular expression pattern "test\\d+",
     * // eg. "test1", "test2", ...
     * var result = helper.findFirstHtmlTag( html, "table",
     *         {attributes: {"class": "test\\d+"}} );
     * @endcode
     *
     * @return A map with properties like in findHtmlTags(). Additionally these properties are
     *   returned:
     *   @li @b found: A boolean, @c true if the tag was found, @c false otherwise.
     * @see findHtmlTags
     **/
    Q_INVOKABLE static QVariantMap findFirstHtmlTag( const QString &str, const QString &tagName,
                                                     const QVariantMap &options = QVariantMap() );

    /**
     * @brief This is an overloaded function which expects a value for "tagName" in @p options.
     * @overload QVariantMap findFirstHtmlTag(const QString&,const QString&,const QVariantMap&)
     **/
    Q_INVOKABLE static inline QVariantMap findFirstHtmlTag(
            const QString &str, const QVariantMap &options = QVariantMap() )
    {
        return findFirstHtmlTag( str, options["tagName"].toString(), options );
    };

    /**
     * @brief Find all occurrences of (top level) HTML tags with @p tagName in @p str.
     *
     * Using this function avoids having to deal with various problems when matching HTML elements:
     * @li Nested HTML elements with the same @p tagName. When simply searching for the first
     *     closing tag after the found opening tag, a nested closing tag gets matched. If you are
     *     sure that there are no nested tags or if you want to only match until the first nested
     *     closing tag set the option "noNesting" in @p options to @c true.
     * @li Matching tags with specific attributes. This function extracts all attributes of a
     *     matched tag. They can have values, which can be put in single/double/no quotation marks.
     *     To only match tags with specific attributes, add them to the "attributes" option in
     *     @p options. Regular expressions can be used to match the attribute name and value
     *     independently. Attribute order does not matter.
     * @li Matching HTML tags correctly. For example a ">" inside an attributes value could cause
     *     problems and have the tag cut off there.
     *
     * @note This function only returns found top level tags. These found tags may contain matching
     *   child tags. You can use this function again on the contents string of a found top level
     *   tag to find its child tags.
     *
     * @b Example:
     * @code
     * // This matches all &lt;div&gt; tags found in html which
     * // have a class attribute with the value "test" and only
     * // numbers as contents, eg. "<div class='test'>42</div>".
     * var result = helper.findHtmlTags( html, "div",
     *         {attributes: {"class": "test"},
     *          contentsRegExp: "\\d+"} );
     * @endcode
     *
     * @param str The string containing the HTML tags to be found.
     * @param tagName The name of the HTML tags to be found, ie. &lt;tagName&gt;.
     * @param options A map with these properties:
     *   @li @b attributes: A map containing all required attributes and it's values. The keys of that
     *     map are the names of required attributes and can be regular expressions. The values
     *     are the values of the required attributes and are also handled as regular expressions.
     *   @li @b contentsRegExp: A regular expression pattern which the contents of found HTML tags
     *     must match. If it does not match, that tag does not get returned as found.
     *     If no parenthesized subexpressions are present in this regular expression, the whole
     *     matching string gets used as contents. If more than one parenthesized subexpressions
     *     are found, only the first one gets used. The regular expression gets matched case
     *     insensitive. By default all content of the HTML tag gets matched.
     *   @li @b position: An integer, where to start the search for tags. If the position is inside
     *     a top-level matching tag, its child tags will be matched and other following top-level
     *     tags. This is 0 by default.
     *   @li @b noContent: A boolean, @c false by default. If @c true, HTML tags without any
     *     content are matched, eg. "br" or "img" tags. Otherwise tags need to be closed to get
     *     matched.
     *   @li @b noNesting: A boolean, @c false by default. If @c true, no checks will be made to
     *     ensure that the first found closing tag belongs to the opening tag. In this case the
     *     found contents always end after the first closing tag after the opening tag, no matter
     *     if the closing tag belongs to a nested tag or not. By setting this to @c true you can
     *     enhance performance.
     *   @li @b maxCount: The maximum number of HTML tags to match or 0 to match any number of HTML tags.
     *   @li @b debug: A boolean, @c false by default. If @c true, more debug output gets generated.
     *
     * @return A list of maps, each map represents one found tag and has these properties:
     *   @li @b contents: A string, the contents of the found tag (if found is @c true).
     *   @li @b position: An integer, the position of the first character of the found tag
     *     (ie. '<') in @p str (if found is @c true).
     *   @li @b endPosition: An integer, the position of the first character after the found end
     *     tag in @p str (if found is @c true).
     *   @li @b attributes: A map containing all found attributes of the tag and it's values (if
     *     found is @c true). The attribute names are the keys of the map, while the attribute
     *     values are the values of the map.
     **/
    Q_INVOKABLE static QVariantList findHtmlTags( const QString &str, const QString &tagName,
                                                  const QVariantMap &options = QVariantMap() );

    /**
     * @brief This is an overloaded function which expects a value for "tagName" in @p options.
     * @overload QVariantMap findHtmlTags(const QString&,const QString&,const QVariantMap&)
     **/
    Q_INVOKABLE static inline QVariantList findHtmlTags(
            const QString &str, const QVariantMap &options = QVariantMap() )
    {
        return findHtmlTags( str, options["tagName"].toString(), options );
    };

    /**
     * @brief Finds all occurrences of HTML tags with @p tagName in @p str.
     *
     * This function uses findHtmlTags() to find the HTML tags and then extracts a name for each
     * found tag from @p str.
     * Instead of returning a list of all matched tags, a map is returned, with the found names as
     * keys and the tag objects (as returned in a list by findHtmlTags()) as values.
     *
     * @b Example:
     * @code
     * // In this example the findNamedHtmlTags() function gets
     * // used in a while loop to find columns (<td>-tags) in rows
     * // (<tr>-tags) and assign names to the found columns. This
     * // makes it easy to parse HTML tables.
     *
     * // Initialize position value
     * var tableRow = { position: -1 };
     *
     * // Use findFirstHtmlTag() with the current position value to
     * // find the next <tr> tag. The return value contains the
     * // updated position and the boolean property "found"
     * while ( (tableRow = helper.findFirstHtmlTag(html, "tr",
     *                     {position: tableRow.position + 1})).found )
     * {
     *     // Find columns, ie. <td> tags in a table row.
     *     // Each column gets a name assigned by findNamedHtmlTags().
     *     // Ambiguous names are resolved by adding/increasing a
     *     // number after the name. The names get extracted from the
     *     // <td>-tags class attributes, by matching the given
     *     // regular expression.
     *     var columns = helper.findNamedHtmlTags( tableRow.contents, "td",
     *           {ambiguousNameResolution: "addNumber",
     *            namePosition: {type: "attribute", name: "class",
     *                           regexp: "([\\w]+)\\d+"}} );
     *
     *     // Check the "names" property of the return value,
     *     // should contain the names of all columns that are needed
     *     if ( !columns.names.contains("time") ) {
     *         // Notify the data engine about an error,
     *         // if a "time" column was expected
     *         helper.error("Didn't find all needed columns! " +
     *                      "Found: " + columns.names, tableRow.contents);
     *         continue;
     *     }
     *
     *     // Now read the contents of the columns
     *     var time = columns["time"].contents;
     *
     *     // Read more and add data to the result set using result.addData()
     * }
     * @endcode
     *
     * @param str The string containing the HTML tag to be found.
     * @param tagName The name of the HTML tag to be found, ie. &lt;tagName&gt;.
     * @param options The same as in findHtmlTags(), but @em additionally these options can be used:
     *     @li @b namePosition: A map with more options, indicating the position of the name of tags:
     *       @li @em type: Can be @em "contents" (ie. use tag contents as name, the default) or
     *         @em "attribute" (ie. use a tag attribute value as name). If @em "attribute" is used
     *         for @em "type", the name of the attribute can be set as @em "name" property.
     *         Additionally a @em "regexp" property can be used to extract a string from the string
     *         that would otherwise be used as name as is.
     *       @li @em ambiguousNameResolution: Can be used to tell what should be done if the same name
     *         was found multiple times. This can currently be one of: @em "addNumber" (adds a
     *         number to the name, ie. "..1", "..2")., @em "replace" (a later match with an already
     *         matched name overwrites the old match, the default).
     *
     * @return A map with the found names as keys and the tag objects as values. @em Additionally
     *   these properties are returned:
     *   @li @b names: A list of all found tag names.
     * @see findHtmlTags
     **/
    Q_INVOKABLE static QVariantMap findNamedHtmlTags( const QString &str, const QString &tagName,
                                                      const QVariantMap &options = QVariantMap() );

signals:
    /**
     * @brief An error was received from the script.
     *
     * @param message The error message.
     * @param failedParseText The text in the source document where parsing failed.
     **/
    void errorReceived( const QString &message, const QScriptContextInfo &contextInfo,
                        const QString &failedParseText = QString() );

private:
    static QString getTagName( const QVariantMap &searchResult, const QString &type = "contents",
            const QString &regExp = QString(), const QString attributeName = QString() );

    QString m_serviceProviderId;
};

/**
 * @brief This class is used by scripts to store results in, eg. departures.
 *
 * An instance of this class gets published to scripts as @b result.
 * Scripts can use it to add items to the result set, ie. departures/arrivals/journeys/
 * stop suggestions. Items can be added from scripts using addData().
 * @code
 * // Add stop suggestion data to result set
 * result.addData({ StopName: "Name" });
 * @endcode
 *
 * @ingroup scripting
 **/
class ResultObject : public QObject, protected QScriptable {
    Q_OBJECT
    Q_ENUMS( Feature Hint )
    Q_FLAGS( Features Hints )
    Q_PROPERTY( int count READ count )

public:
    /**
     * @brief Used to store enabled features.
     *
     * The meta object of ResultObject gets published to scripts under the name "enum" and contains
     * this enumeration.
     *
     * @see enableFeature()
     * @see isFeatureEnabled()
     **/
    enum Feature {
        NoFeature   = 0x00, /**< No feature is enabled. */
        AutoPublish = 0x01, /**< Automatic publishing of the first few data items. Turn
                * this off if you want to call publish() manually. */
        AutoDecodeHtmlEntities
                    = 0x02, /**< Automatic decoding of HTML entities in strings and string
                * lists. If you are sure, that there are no HTML entities in the strings parsed
                * from the downloaded documents, you can turn this feature off. You can also
                * manually decode HTML entities using Helper::decodeHtmlEntities(). */
        AutoRemoveCityFromStopNames
                    = 0x04, /**< Automatic removing of city names from all stop names, ie.
                * stop names in eg. RouteStops or Target). Scripts can help the data engine with
                * this feature with the hints CityNamesAreLeft or CityNamesAreRight.
                * @code
                * result.giveHint( enum.CityNamesAreLeft );
                * result.giveHint( enum.CityNamesAreRight );
                * @endcode
                * @see Hint */
        AllFeatures = AutoPublish | AutoDecodeHtmlEntities | AutoRemoveCityFromStopNames
                /**< All available features are enabled. */
    };
    Q_DECLARE_FLAGS( Features, Feature );

    /**
     * @brief Can be used by scripts to give hints to the data engine.
     *
     * The meta object of ResultObject gets published to scripts as @b enum and contains this
     * enumeration.
     *
     * @see giveHint()
     * @see isHintGiven()
     **/
    enum Hint {
        NoHint              = 0x00, /**< No hints given. */
        DatesNeedAdjustment = 0x01, /**< Dates are set from today, not the requested date. They
                * need to be adjusted by X days, where X is the difference in days between today
                * and the requested date. */
        NoDelaysForStop     = 0x02, /**< Delays are not available for the current stop, although
                * delays are available for other stops. */
        CityNamesAreLeft    = 0x04, /**< City names are most likely on the left of stop names. */
        CityNamesAreRight   = 0x08  /**< City names are most likely on the right of stop names. */
    };
    Q_DECLARE_FLAGS( Hints, Hint );

    /**
     * @brief Creates a new ResultObject instance.
     **/
    ResultObject( QObject* parent = 0 );

    virtual ~ResultObject();

    /**
     * @brief Get the list of stored TimetableData objects.
     *
     * @return The list of stored TimetableData objects.
     **/
    QList< TimetableData > data() const;

    /**
     * @brief Checks whether or not the list of TimetableData objects is empty.
     *
     * @return @c True, if the list of TimetableData objects isn't empty. @c False, otherwise.
     **/
    Q_INVOKABLE bool hasData() const;

    /**
     * @brief Returns the number of timetable elements currently in the resultset.
     **/
    Q_INVOKABLE int count() const;

    /**
     * @brief Whether or not @p feature is enabled.
     *
     * Script example:
     * @code
     * if ( result.isFeatureEnabled(enum.AutoPublish) ) {
     *    // Do something when the AutoPublish feature is enabled
     * }
     * @endcode
     *
     * By default all features are enabled.
     *
     * @param feature The feature to check. Scripts can access the Feature enumeration
     *   under the name "enum".
     *
     * @see Feature
     * @since 0.10
     **/
    Q_INVOKABLE bool isFeatureEnabled( Feature feature ) const;

    /**
     * @brief Set whether or not @p feature is @p enabled.
     *
     * Script example:
     * @code
     * // Disable the AutoPublish feature
     * result.enableFeature( enum.AutoPublish, false );
     * @endcode
     *
     * By default all features are enabled, disable unneeded features for better performance.
     *
     * @param feature The feature to enable/disable. Scripts can access the Feature enumeration
     *   under the name "enum".
     * @param enable @c True to enable @p feature, @c false to disable it.
     *
     * @see Feature
     * @since 0.10
     **/
    Q_INVOKABLE void enableFeature( Feature feature, bool enable = true );

    /**
     * @brief Test if the given @p hint is set.
     *
     * Script example:
     * @code
     * if ( result.isHintGiven(enum.CityNamesAreLeft) ) {
     *    // Do something when the CityNamesAreLeft hint is given
     * }
     * @endcode
     *
     * By default no hints are set.
     *
     * @param hint The hint to check. Scripts can access the Hint enumeration
     *   under the name "enum".
     *
     * @since 0.10
     */
    Q_INVOKABLE bool isHintGiven( Hint hint ) const;

    /**
     * @brief Set the given @p hint to @p enable.
     *
     * Script example:
     * @code
     * // Remove the CityNamesAreLeft hint
     * result.giveHint( enum.CityNamesAreLeft, false );
     * @endcode
     *
     * By default no hints are set.
     *
     * @param hint The hint to give.
     * @param enable Whether the @p hint should be set or unset.
     *
     * @since 0.10
     */
    Q_INVOKABLE void giveHint( Hint hint, bool enable = true );

    /**
     * @brief Get the currently enabled features.
     *
     * Scripts can access the Features enumeration like @verbatimenum.AutoPublish@endverbatim.
     * By default this equals to AllFeatures.
     * @since 0.10
     */
    Features features() const;

    /**
     * @brief Get the currently set hints.
     *
     * Scripts can access the Hints enumeration like @verbatimenum.CityNamesAreLeft@endverbatim.
     * By default this equals to NoHints.
     * @since 0.10
     */
    Hints hints() const;

    static void dataList( const QList< TimetableData > &dataList,
                          PublicTransportInfoList *infoList, ParseDocumentMode parseMode,
                          VehicleType defaultVehicleType, const GlobalTimetableInfo *globalInfo,
                          ResultObject::Features features, ResultObject::Hints hints );

Q_SIGNALS:
    /**
     * @brief Can be called by scripts to trigger the data engine to publish collected data.
     *
     * This does not need to be called by scripts, the data engine will publish all collected data,
     * when the script returns and all network requests are finished. After the first ten items
     * have been added, this signal is emitted automatically, if the AutoPublish feature is
     * enabled (the default). Use @verbatimresult.enableFeature(AutoPublish, false)@endverbatim to
     * disable this feature.
     *
     * If collecting data takes too long, calling this signal causes the data collected so far
     * to be published immediately. Good reasons to call this signal are eg. because additional
     * documents need to be downloaded or because a very big document gets parsed. Visualizations
     * connected to the data engine will then receive data not completely at once, but step by
     * step.
     *
     * It also means that the first data items are published to visualizations faster. A good idea
     * could be to only call publish() after the first few data items (similar to the AutoPublish
     * feature). That way visualizations get the first dataset very quickly, eg. the data that
     * fits into the current view. Remaining data will then be added after the script is finished.
     *
     * @note Do not call publish() too often, because it causes some overhead. Visualizations
     *   will get notified about the updated data source and process it at whole, ie. not only
     *   newly published items but also the already published items again. Publishing data in
     *   groups of less than ten items will be too much in most situations. But if eg. another
     *   document needs to be downloaded to make more data available, it is a good idea to call
     *   publish() before starting the download (even with less than ten items).
     *   Use count() to see how many items are collected so far.
     *
     * @see Feature
     * @see setFeatureEnabled
     * @since 0.10
     **/
    void publish();

    /**
     * @brief Emitted when invalid data gets received through the addData() method.
     *
     * @param info The TimetableInformation which was invalid in @p map.
     * @param errorMessage An error message explaining why the data for @p info in @p map
     *   is invalid.
     * @param context The script context from which addData() was called.
     * @param index The target index at which the data in @p map will be inserted into this result
     *   object.
     * @param map The argument for addData(), which contained invalid data.
     **/
    void invalidDataReceived( TimetableInformation info, const QString &errorMessage,
                              const QScriptContextInfo &context,
                              int index, const QVariantMap& map );

public Q_SLOTS:
    /**
     * @brief Clears the list of stored TimetableData objects.
     * @ingroup scripting
     **/
    void clear();

    /**
     * @brief Adds the data from @p map.
     *
     * This can be used by scripts to add a timetable data object.
     * @code
     *  result.addData({ DepartureDateTime: new Date(), Target: 'Test' });
     * @endcode
     *
     * A predefined object can also be added like this:
     * @code
     *  var departure = { DepartureDateTime: new Date() };
     *  departure.Target = 'Test';
     *  result.addData( departure );
     * @endcode
     *
     * Keys of @p map, ie. properties of the script object are matched case insensitive.
     *
     * @param map A map with all timetable informations as pairs of the information names and
     *   their values.
     **/
    void addData( const QVariantMap &map );

private:
    QList< TimetableData > m_timetableData;

    // Protect data from concurrent access by the script in a separate thread and usage in C++
    QMutex *m_mutex;
    Features m_features;
    Hints m_hints;
};
Q_DECLARE_OPERATORS_FOR_FLAGS( ResultObject::Features );
Q_DECLARE_OPERATORS_FOR_FLAGS( ResultObject::Hints );

class StoragePrivate;
/**
 * @brief Used by scripts to store data between calls.
 *
 * An object of this type gets created for each script (not for each call to the script).
 * The data in the storage is therefore shared between calls to a script, but not between
 * scripts for different service providers.
 *
 * This class distinguishes between memory storage and persistent storage, ie. on disk.
 * Both storage types are protected by separate QReadWriteLock's, because scripts may run in
 * parallel.
 *
 * @code
 * // Write a single value and read it again
 * storage.write( "name1", 123 );
 * var name1 = storage.read( "name1" );   // name1 == 123
 *
 * // Write an object with multiple values at once and read it again
 * var object = { value1: 445, value2: "test",
 *                otherValue: new Date() };
 * storage.write( object );
 * var readObject = storage.read();
 * // The object read from storage now contains both the single
 * // value and the values of the object
 * // readObject == { name1: 123, value1: 445, value2: "test",
 * //                 otherValue: new Date() };
 *
 * var value2 = storage.read( "value2" );   // value2 == "test"
 * var other = storage.read( "other", 555 );   // other == 555, the default value
 * storage.remove( "name1" );   // Remove a value from the storage
 * storage.clear();
 * @endcode
 *
 *
 * After the service provider plugin has been reloaded (eg. after a restart), the values stored
 * like shown above are gone. To write data persistently to disk use readPersistent(),
 * writePersistent() and removePersistent(). Persistently stored data has a lifetime which can be
 * specified as argument to writePersistent() and defaults to one week.
 * The maximum lifetime is one month.
 *
 * @code
 * // Write a single value persistently and read it again
 * storage.writePersistent( "name1", 123 );   // Using the default lifetime
 * var name1 = storage.readPersistent( "name1" );   // name1 == 123
 *
 * // Write an object with multiple values at once and read it again
 * var object = { value1: 445, value2: "test", otherValue: new Date() };
 * storage.writePersistent( object );
 * var readObject = storage.readPersistent();
 * // The object read from storage now contains both the single
 * // value and the values of the object
 * // readObject == { name1: 123, value1: 445, value2: "test",
 * //                 otherValue: new Date() };
 *
 * // Using custom lifetimes (in days)
 * // 1. Store value 66 for 30 days as "longNeeded"
 * storage.writePersistent( "longNeeded", 66, 30 );
 * // 2. Lifetime can't be higher than 30 days
 * storage.writePersistent( "veryLongNeeded", 66, 300 );
 *
 * // Check the remaining lifetime of a persistently stored value
 * var lifetimeLongNeeded = storage.lifetime( "longNeeded" ); // Now 30
 * var other = storage.readPersistent( "other", 555 );   // other == 555, default
 * storage.removePersistent( "name1" );   // Remove a value from the storage
 * storage.clearPersistent();
 * @endcode
 *
 * @warning Since the script can run multiple times simultanously in different threads which share
 *   the same Storage object, the stored values are also shared . If you want to store a value for
 *   the current job of the script only (eg. getting departures and remember a value after an
 *   asynchronous request), you should store the value in a global script variable instead.
 *   Otherwise one departure request job might use the value stored by another one, which is
 *   probably not what you want. Scripts can not not access the Storage object of other scripts
 *   (for other service providers).
 *
 * @ingroup scripting
 * @since 0.10
 **/
class Storage : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Create a new Storage instance.
     *
     * @param serviceProviderId Used to find the right place in the service provider cache file
     *   to read/write persistent data.
     * @param parent The parent QObject.
     **/
    Storage( const QString &serviceProviderId, QObject *parent = 0 );

    /** @brief Destructor. */
    virtual ~Storage();

    /**
     * @brief The maximum lifetime in days for data written to disk.
     * @see writePersistent()
     **/
    static const uint MAX_LIFETIME = 30;

    /**
     * @brief The default lifetime in days for data written to disk.
     * @see writePersistent()
     **/
    static const uint DEFAULT_LIFETIME = 7;

    /**
     * @brief The suffix to use for lifetime data entries.
     *
     * This suffix gets appended to the name with which data gets written persistently to disk
     * to get the name of the data entry storing the associated lifetime information.
     * @see writePersistent()
     **/
    static const char* LIFETIME_ENTRYNAME_SUFFIX;

    /**
     * @brief The minimal interval in minutes to run checkLifetime().
     * @see checkLifetime()
     **/
    static const int MIN_LIFETIME_CHECK_INTERVAL = 15;

    /**
     * @brief Whether or not a data entry with @p name exists in memory.
     * @ingroup scripting
     **/
    Q_INVOKABLE bool hasData( const QString &name ) const;

    /**
     * @brief Whether or not a data entry with @p name exists in persistent memory.
     * @ingroup scripting
     **/
    Q_INVOKABLE bool hasPersistentData( const QString &name ) const;

    /**
     * @brief Reads all data stored in memory.
     * @ingroup scripting
     **/
    Q_INVOKABLE QVariantMap read();

    /**
     * @brief Reads data stored in memory with @p name.
     * @ingroup scripting
     **/
    Q_INVOKABLE QVariant read( const QString &name, const QVariant& defaultData = QVariant() );

    /**
     * @brief Reads the lifetime remaining for data written using writePersistent() with @p name.
     * @ingroup scripting
     **/
    Q_INVOKABLE int lifetime( const QString &name );

    /**
     * @brief Reads data stored on disk with @p name.
     *
     * @param name The name of the value to read.
     * @param defaultData The value to return if no stored value was found under @p name.
     *   If you use another default value than the default invalid QVariant, the type must match
     *   the type of the stored value. Otherwise an invalid QVariant gets returned.
     * @see lifetime()
     * @ingroup scripting
     **/
    Q_INVOKABLE QVariant readPersistent( const QString &name,
                                         const QVariant& defaultData = QVariant() );

    /**
     * @brief Checks the lifetime of all persistent data entries.
     *
     * If the lifetime of a persistent data entry has expired, it gets deleted.
     **/
    void checkLifetime();

public Q_SLOTS:
    /**
     * @brief Stores @p data in memory with @p name.
     * @ingroup scripting
     **/
    void write( const QString &name, const QVariant &data );

    /**
     * @brief Stores @p data in memory, eg. a script object.
     *
     * @param data The data to write to disk. This can be a script object.
     * @overload
     * @ingroup scripting
     **/
    void write( const QVariantMap &data );

    /**
     * @brief Removes data stored in memory with @p name.
     * @ingroup scripting
     **/
    void remove( const QString &name );

    /**
     * @brief Clears all data stored in memory.
     * @ingroup scripting
     **/
    void clear();

    /**
     * @brief Stores @p data on disk with @p name.
     *
     * @param name A name to access the written data with.
     * @param data The data to write to disk. The type of the data can also be QVariantMap (ie.
     *   script objects) or list types (gets encoded to QByteArray). Length of the data is limited
     *   to 65535 bytes.
     * @param lifetime The lifetime in days of the data. Limited to 30 days and defaults to 7 days.
     *
     * @see lifetime
     * @ingroup scripting
     **/
    void writePersistent( const QString &name, const QVariant &data,
                          uint lifetime = DEFAULT_LIFETIME );

    /**
     * @brief Stores @p data on disk, eg. a script object.
     *
     * After @p lifetime days have passed, the written data will be deleted automatically.
     * To prevent automatic deletion the data has to be written again.
     *
     * @param data The data to write to disk. This can be a script object.
     * @param lifetime The lifetime in days of each entry in @p data.
     *   Limited to 30 days and defaults to 7 days.
     *
     * @see lifetime
     * @overload
     * @ingroup scripting
     **/
    void writePersistent( const QVariantMap &data, uint lifetime = DEFAULT_LIFETIME );

    /**
     * @brief Removes data stored on disk with @p name.
     *
     * @note Scripts do not need to remove data written persistently, ie. to disk, because each
     *   data entry has a lifetime, which is currently limited to 30 days and defaults to 7 days.
     * @ingroup scripting
     **/
    void removePersistent( const QString &name );

    /**
     * @brief Clears all data stored persistently, ie. on disk.
     *
     * @note Scripts do not need to remove data written persistently, ie. to disk, because each
     *   data entry has a lifetime, which is currently limited to 30 days and defaults to 7 days.
     * @ingroup scripting
     **/
    void clearPersistent();

private:
    int lifetime( const QString &name, const KConfigGroup &group );
    void removePersistent( const QString &name, KConfigGroup &group );
    QByteArray encodeData( const QVariant &data ) const;
    QVariant decodeData( const QByteArray &data ) const;

    StoragePrivate *d;
};

}; // namespace Scripting

Q_DECLARE_METATYPE(Scripting::NetworkRequest*)
Q_SCRIPT_DECLARE_QMETAOBJECT(Scripting::NetworkRequest, QObject*)

#endif // SCRIPTING_HEADER
