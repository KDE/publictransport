/*
 *   Copyright 2013 Friedrich Pülz <fpuelz@gmx.de>
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
* @brief This file contains enumerations used by the public transport data engine.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef ENUMS_HEADER
#define ENUMS_HEADER

#include <QDebug>
#include <QDate>
#include <QMetaType>
#include <QMetaEnum>
#include <kjob.h> // For KJob::UserDefinedError

/** @brief Contains global information about a downloaded timetable that affects all items. */
struct GlobalTimetableInfo {
    GlobalTimetableInfo() {
        delayInfoAvailable = true;
        requestDate = QDate::currentDate();
    };

    /**
     * @brief Whether or not delay information is available.
     *
     *   True, if there may be delay information available. False, if no
     *   delay information is/will be available for the given stop.
     **/
    bool delayInfoAvailable;

    /**
     * @brief The requested date of the first departure/arrival/journey.
     **/
    QDate requestDate;
};

/** @brief General error codes, published in data sources of the PublicTransport data engine. */
enum ErrorCode {
    NoError = 0, /**< There were no error. */

    ErrorDownloadFailed = 1, /**< Download error occurred. */
    ErrorParsingFailed = 2, /**< Parsing downloaded data failed. */
    ErrorNeedsImport = 3 /**< An import step needs to be performed, before using the accessor.
            * This is currently only used for GTFS accessors, which need to import the GTFS feed
            * before being usable. */
};

/** @brief Error codes of the GTFS service. */
enum GtfsServiceError {
    NoGtfsError = KJob::NoError, /**< There was no error. */

    GtfsErrorInvalidProviderId = KJob::UserDefinedError + 1, /**< A provider ID was sent to the
            * GTFS service, for which no installed provider plugin sources were found. */
    GtfsErrorWrongProviderType = KJob::UserDefinedError + 2, /**< A provider ID was sent to the
            * GTFS service, which is associated with a provider plugin of a wrong type,
            * ie. not a GTFS provider. */
    GtfsErrorFeedImportAlreadyRunning = KJob::UserDefinedError + 3, /**< An operation of the GTFS
            * service tried to start a GTFS feed import, but an import is already running
            * for the provider. */
    GtfsErrorFeedImportRequired = KJob::UserDefinedError + 4, /**< The "updateGtfsDatabase"
            * operation of the GTFS service will cancel with this error code, if the GTFS feed
            * was not previously imported successfully. */
    GtfsErrorWrongFeedFormat = KJob::UserDefinedError + 5, /**< The file at the GTFS feed URL has
            * the wrong format, ie. not a zip file. */
    GtfsErrorDownloadFailed = KJob::UserDefinedError + 6, /**< A GTFS feed download failed. */
    GtfsErrorImportFailed = KJob::UserDefinedError + 7, /**< Failed to import the GTFS feed into
            * the database, after it was successfully downloaded. */
    GtfsErrorCannotDeleteDatabase = KJob::UserDefinedError + 8 /**< There was an error while
            * trying to delete the GTFS database. */
};

/**
 * @brief Contains enumerables which are also made available to scripts.
 *
 * Scripts can access the enumerables in this class in the @em PublicTransport script object,
 * eg. @em PublicTransport.ProvidesArrivals.
 * This class only contains enumerables and static functions.
 **/
class Enums : public QObject {
    Q_OBJECT

    // NOTE The ';' at the end is needed for doxygen, otherwise the enumarables get skipped
    Q_ENUMS( TimetableInformation ServiceProviderType ProviderFeature
             VehicleType LineService MoreItemsDirection )
public:
    /**
    * @brief Different types of timetable information.
    *
    * In scripts the enumerable names can be used as property names of a result object, eg.:
    * @code
    * result.addData({ DepartureDateTime: new Date(),
    *                  TypeOfVehicle: PublicTransport.Bus,
    *                  Target: "SomeTarget" });
    * @endcode
    */
    enum TimetableInformation {
        /** No usable information. */
        Nothing = 0,

        // Departures / arrival / journey information
        /**
        * The date and time of the departure. This information is the peferred information for
        * storing the date and time. DepartureDate and DepartureTime get combined to
        * DepartureDateTime if needed.
        **/
        DepartureDateTime = 1,

        /** The date of the departure. */
        DepartureDate = 2,

        /** The time of the departure. */
        DepartureTime = 3,

        /**
        * The type of vehicle, eg. a tram or a bus. Can be the name (matched case insensitively) or
        * the value of an enumerable of the VehicleType enumeration.
        *
        * @warning Do not confuse with VehicleType, which is the name of the enumeration of vehicle
        *   types.
        **/
        TypeOfVehicle = 4,

        /** The name of the public transport line, e.g. "4", "6S", "S 5", "RB 24122". */
        TransportLine = 5,

        FlightNumber = TransportLine, /**< Same as TransportLine, used for flights. */
        Target = 6, /**< The target of a journey / of a public transport line. */

        /**
        * Like Target, but in a shortened form. A word at the beginning/end of the stop name may be
        * removed, if it has many occurrences in stop names. When requesting data from the data
        * engine, the stop name in Target should be used, when showing stop names to the user,
        * TargetShortened should be used.
        **/
        TargetShortened = 7,

        Platform = 8, /**< The platform at which the vehicle departs / arrives. */

        /**
        * The delay of a public transport vehicle in minutes, -1 means that the delay is unknown,
        * 0 means that there is no delay.
        **/
        Delay = 9,

        DelayReason = 10, /**< The reason of a delay. */
        JourneyNews = 11,  /**< An informative message about the departure, arrival or journey,
                * eg. "Train cancelled" or "From platform 4 instead of 3". If only an URL to a
                * website with the news is available, use JourneyNewsUrl. */
        JourneyNewsOther = 12, /**< @deprecated Use JourneyNews instead. */

        /**
         * Contains a link to an HTML page with journey news. The URL of the service provider is
         * prepended, if a relative path has been matched (starting with "/").
         **/
        JourneyNewsUrl = 13, /**< Other news (not delay / delay reason) */
        JourneyNewsLink = JourneyNewsUrl, /**< @deprecated Use JourneyNewsUrl instead. */

        Operator = 16, /**< The company that is responsible for the journey. */
        Status = 20, /**< The current status of the departure / arrival. Currently only used for planes. */

        // Route information
        /**
        * A list of stops of the departure / arrival to it's destination stop or a list of stops of
        * the journey from it's start to it's destination stop. If @ref RouteStops and @ref RouteTimes
        * are both set, they should contain the same number of elements. And elements with equal
        * indices should be associated (the times at which the vehicle is at the stops). For journeys
        * @ref RouteTimesDeparture and @ref RouteTimesArrival should be used instead of
        * @ref RouteTimes.
        **/
        RouteStops = 22,

        /**
        * Like RouteStops, but in a shortened form. Words at the beginning/end of stop names may be
        * removed, if they have many occurrences in stop names. When requesting data from the data
        * engine, the stop names in RouteStops should be used, when showing stop names to the user,
        * RouteStopsShortened should be used.
        **/
        RouteStopsShortened = 23,

        /**
        * A list of times of the departure / arrival to it's destination stop. If @ref RouteStops and
        * @ref RouteTimes are both set, they should contain the same number of elements. And elements
        * with equal indices should be associated (the times at which the vehicle is at the stops).
        * Can contain list elements of type QDateTime (ie. QtScript Date).
        **/
        RouteTimes = 24,

        /**
        * A list of departure times of the journey. If @ref RouteStops and @ref RouteTimesDeparture
        * are both set, the latter should contain one elements less (because the last stop has no
        * departure, only an arrival time). Elements with equal indices should be associated (the
        * times at which the vehicle departs from the stops).
        * Can contain list elements of type QDateTime (ie. QtScript Date).
        **/
        RouteTimesDeparture = 25,

        /**
        * A list of arrival times of the journey. If @ref RouteStops and @ref RouteTimesArrival are
        * both set, the latter should contain one elements less (because the first stop has no
        * arrival, only a departure time). Elements with equal indices should be associated (the
        * times at which the vehicle arrives at the stops).
        * Can contain list elements of type QDateTime (ie. QtScript Date).
        **/
        RouteTimesArrival = 26,

        /**
        * The number of exact route stops. The route stop list isn't complete from the last exact
        * route stop.
        **/
        RouteExactStops = 27,

        /** The types of vehicles used for each "sub-journey" of a journey. */
        RouteTypesOfVehicles = 28,

        /** The transport lines used for each "sub-journey" of a journey. */
        RouteTransportLines = 29,

        /** The platforms of departures used for each "sub-journey" of a journey. */
        RoutePlatformsDeparture = 30,

        /** The platforms of arrivals used for each "sub-journey" of a journey. */
        RoutePlatformsArrival = 31,

        /** A list of delays in minutes for each departure time of a route (RouteTimesDeparture). */
        RouteTimesDepartureDelay = 32,

        /** A list of delays in minutes for each arrival time of a route (RouteTimesArrival). */
        RouteTimesArrivalDelay = 33,

        /** News/notes for each "sub-journey" of a journey. */
        RouteNews = 34,

        /**
         * A hash with route TimetableInformation values for each "sub-journey" of a journey.
         * Contains one element less than RouteStops, one sub-journey between each pair of
         * successive route stops.
         * Can use all TimetableInformation enumerables which begin with "Route", except for
         * RouteTypesOfVehicles, RouteTransportLines and RouteSubJourneys.
         * Each sub-journey represents the exact route of one transport line/vehicle in a journey.
         * @note All sub-journeys are stored without their origin and target stops.
         *   Only intermediate stops are listed here. Therefore all route data in a sub-journey list
         *   needs to have the same number of elements, eg. RouteStops and RouteTimesDeparture.
         **/
        RouteSubJourneys = 35,

        /**
         * An URL to a document, which contains route data for a specific train/vehicle.
         * For HAFAS providers this is a "traininfo.exe"-URL.
         **/
        RouteDataUrl = 40,

        // Journey information
        Duration = 50, /**< The duration of a journey. */
        StartStopName = 51, /**< The name of the starting stop of a journey. */
        StartStopID = 52, /**< The ID of the starting stop of a journey. */
        TargetStopName = 53, /**< The name of the target stop of a journey. */
        TargetStopID = 54, /**< The ID of the target stop of a journey. */

        /**
        * The date and time of the arrival. This information is the peferred information for storing
        * the date and time. ArrivalDate and ArrivalTime get combined to ArrivalDateTime if needed.
        **/
        ArrivalDateTime = 55,

        ArrivalDate = 56, /**< The date of the arrival. */
        ArrivalTime = 57, /**< The time of the arrival. */
        Changes = 58, /**< The number of changes between different vehicles in a journey. */
        TypesOfVehicleInJourney = 59, /**< A list of vehicle types used in a journey. */
        Pricing = 60, /**< Information about the pricing of a journey. */

        // Special information
        IsNightLine = 101, /**< The transport line is a nightline. @todo Currently unused. */

        // Stop suggestion information
        StopName = 200, /**< The name of a stop/station. */
        StopID = 201, /**< The ID of a stop/station. */
        StopWeight = 202, /**< The weight of a stop suggestion. */
        StopCity = 203, /**< The city in which a stop is. */
        StopCountryCode = 204, /**< The country code of the country in which the stop is. */
        StopLongitude = 205, /**< The longitude of the stop. */
        StopLatitude = 206, /**< The latitude of the stop. */

        // Information about the request that was used to get the timetable data
        RequestData = 300 /**< Can be used to store data identifying the request that was used to
                * get the timetable item. This data gets passed back to the provider plugin eg.
                * when requesting later/earlier timetable items. */
    };

    /** @brief Features of a provider. */
    enum ProviderFeature {
        InvalidProviderFeature = -1, /**< @internal */

        ProvidesDepartures, /**< Departure data is provided through the getTimetable()
                * script function. Scripts do not need to specify this enumerable in the features()
                * script function, it is assumed by implementing the getTimetable() script function. */
        ProvidesArrivals, /**< Arrival data is provided through the getTimetable()
                * script function. The argument must have a "dataType" property with the
                * value "arrivals". */
        ProvidesJourneys, /**< Journey data is provided through the getJourneys() script function.
                * Scripts do not need to specify this enumerable in the features()
                * script function, it is assumed by implementing the getJourneys() script function. */
        ProvidesAdditionalData, /**< Additional data for existing timetable data sources is
                * provided through the getAdditionalData() script function. Scripts do not need
                * to specify this enumerable in the features() script function, it is assumed by
                * implementing the getAdditionalData() script function. */
        ProvidesDelays, /**< Realtime delay data is provided for departures/arrivals and/or
                * journeys. The data engine uses this information to determine how often timetable
                * data may need to be updated. */
        ProvidesNews, /**< News are provided for departures/arrivals and/or journeys. */
        ProvidesPlatform, /**< Platform data is provided for departures/arrivals and/or journeys. */
        ProvidesStopSuggestions, /**< Stop suggestions are provided through the
                * getStopSuggestions() script function. Scripts do not need to specify this
                * enumerable in the features() script function, it is assumed by implementing the
                * getStopSuggestions() script function. */
        ProvidesStopID, /**< Stop IDs are provided by the getStopSuggestions() script function. */
        ProvidesStopGeoPosition, /**< Stop positions are provided by the getStopSuggestions() script
                * function, stored as StopLongitude and StopLatitude. */
        ProvidesStopsByGeoPosition, /**< Stop suggestions can also be requested by
                * longitude and latitude. */
        ProvidesPricing, /**< Pricing data is provided for departures/arrivals and/or journeys. */
        ProvidesRouteInformation, /**< Route information is provided for departures/arrivals
                * and/or journeys. */
        ProvidesMoreJourneys /** More journeys (earlier or later) can be provided for existing
                * journey data sources. After requesting more items the data source will contain
                * both the old and the new journeys. */
    };

    /**
    * @brief The type of a service provider.
    * @note The engine may be built without support for some of these types, but at least one type
    *   is available. Check available types using ServiceProviderGlobal::availableProviderTypes()
    *   and ServiceProviderGlobal::isProviderTypeAvailable().
    **/
    enum ServiceProviderType {
        InvalidProvider = 0, /**< @internal Invalid value. */

        /**
        * Uses a script to request and parse documents. Scripts can make use of
        * several helper objects to download documents (GET or POST), store found departures/
        * journeys/stop suggestions, cache values, parse HTML, notify about errors, etc.
        * QtScript extensions can be used, eg. qt.xml to parse XML documents.
        **/
        // ScriptedProvider = 1, # Deprecated; Please use Gtfs service providers

        /**
        * The accessor uses a database filled with GTFS data.
        **/
        GtfsProvider = 2
    };

    /**
    * @brief Types of vehicles.
    *
    * Scripts can access these enumerables in the @em PublicTransport script object,
    * eg. @em PublicTransport.Bus.
    * @code
    * result.addData({ TypeOfVehicle: PublicTransport.Bus, ... });
    * result.addData({ TypeOfVehicle: PublicTransport.Tram, ... });
    * result.addData({ TypeOfVehicle: PublicTransport.RegionalExpressTrain, ... });
    * @endcode
    **/
    enum VehicleType {
        InvalidVehicleType = -1, /**< Invalid vehicle type. */
        UnknownVehicleType = 0, /**< The type of the vehicle is unknown. */

        /**
         * <table style="boder:1;"><tr><td>@image html hi16-app-vehicle_type_tram.png
         * </td><td>
         * A tram / streetcar.
         * </td></tr></table>
         **/
        Tram = 1,

        /**
         * <table style="boder:0;"><tr><td>@image html hi16-app-vehicle_type_bus.png
         * </td><td>
         * A bus.
         * </td></tr></table>
         **/
        Bus = 2,

        /**
         * <table style="boder:0;"><tr><td>@image html hi16-app-vehicle_type_subway.png
         * </td><td>
         * A subway.
         * </td></tr></table>
         **/
        Subway = 3,

        /**
         * <table style="boder:0;"><tr><td>@image html hi16-app-vehicle_type_train_interurban.png
         * </td><td>
         * An interurban train.
         * </td></tr></table>
         **/
        InterurbanTrain = 4,

        /**
         * <table style="boder:0;"><tr><td>@image html hi16-app-vehicle_type_metro.png
         * </td><td>
         * A metro.
         * </td></tr></table>
         **/
        Metro = 5,

        /**
         * <table style="boder:0;"><tr><td>@image html hi16-app-vehicle_type_trolleybus.png
         * </td><td>
         * A trolleybus (also known as trolley bus, trolley coach, trackless trolley, trackless tram
         * or trolley) is an electric bus that draws its electricity from overhead wires (generally
         * suspended from roadside posts) using spring-loaded trolley poles.
         * </td></tr></table>
         **/
        TrolleyBus = 6,

        /**
         * <table style="boder:0;"><tr><td>@image html hi16-app-vehicle_type_train_regional.png
         * </td><td>
         * A regional train. Stops at many small stations, slow.
         * </td></tr></table>
         **/
        RegionalTrain = 10,

        /**
         * <table style="boder:0;"><tr><td>@image html hi16-app-vehicle_type_train_regional.png
         * </td><td>
         * A regional express train. Stops at less small stations than RegionalTrain but is faster.
         * </td></tr></table>
         **/
        RegionalExpressTrain = 11,

        /**
         * <table style="boder:0;"><tr><td>@image html hi16-app-vehicle_type_train_interregional.png
         * </td><td>
         * An inter-regional train. Higher distances and faster than RegionalTrain and
         * RegionalExpressTrain.
         * </td></tr></table>
         **/
        InterregionalTrain = 12,

        /**
         * <table style="boder:0;"><tr><td>@image html hi16-app-vehicle_type_train_intercity.png
         * </td><td>
         * An intercity / eurocity train. Connects cities.
         * </td></tr></table>
         **/
        IntercityTrain = 13,

        /**
         * <table style="boder:0;"><tr><td>@image html hi16-app-vehicle_type_train_highspeed.png
         * </td><td>
         * A highspeed train, eg. an ICE (intercity express) or TGV.
         * Trains at > 250 km/h, high distances.
         * </td></tr></table>
         **/
        HighSpeedTrain = 14,

        /**
         * <table style="boder:0;"><tr><td>@image html hi16-app-vehicle_type_feet.png
         * </td><td>
         * By feet, ie. no vehicle. Used for journeys, eg. from platform A to platform B when changing
         * the vehicle.
         * </td></tr></table>
         **/
        Footway = 50,
        Feet = Footway, /**< @deprecated Use Footway instead. */

        /**
         * <table style="boder:0;"><tr><td>@image html hi16-app-vehicle_type_ferry.png
         * </td><td>
         * A ferry.
         * </td></tr></table>
         **/
        Ferry = 100,

        /**
         * <table style="boder:0;"><tr><td>@image html hi16-app-vehicle_type_ferry.png
         * </td><td>
         * A ship, but not a ferry.
         * </td></tr></table>
         **/
        Ship = 101,

        /**
         * <table style="boder:0;"><tr><td>@image html hi16-app-vehicle_type_plane.png
         * </td><td>
         * An aeroplane.
         * </td></tr></table>
         **/
        Plane = 200,

        /**
         * A spacecraft. @todo Currently unused.
         **/
        Spacecraft = 300
    };

    /** @brief The type of services for a public transport line. */
    enum LineService {
        NoLineService = 0x00, /**< The public transport line has no special services. */

        NightLine = 0x01, /**< The public transport line is a night line. */
        ExpressLine = 0x02 /**< The public transport line is an express line. */
    };
    // Q_DECLARE_FLAGS( LineServices, LineService ) // Gives a compiler error here.. but not in departureinfo.h TODO #include <QMetaType>

    /** @brief Whether to request earlier or later items. */
    enum MoreItemsDirection {
        RequestedItems, /**< Do not request more items, ie. this is the first request. */
        EarlierItems, /**< Request earlier items. */
        LaterItems /**< Request later items. */
    };

    static inline QString toString( TimetableInformation timetableInformation ) {
        const int index = staticMetaObject.indexOfEnumerator("TimetableInformation");
        return QLatin1String(staticMetaObject.enumerator(index).valueToKey(timetableInformation));
    };
    static inline QString toString( ServiceProviderType providerType ) {
        const int index = staticMetaObject.indexOfEnumerator("ServiceProviderType");
        return QLatin1String(staticMetaObject.enumerator(index).valueToKey(providerType));
    };
    static inline QString toString( ProviderFeature feature ) {
        const int index = staticMetaObject.indexOfEnumerator("ProviderFeature");
        return QLatin1String(staticMetaObject.enumerator(index).valueToKey(feature));
    };
    static inline QString toString( VehicleType vehicleType ) {
        const int index = staticMetaObject.indexOfEnumerator("VehicleType");
        return QLatin1String(staticMetaObject.enumerator(index).valueToKey(vehicleType));
    };
    static inline QString toString( LineService lineService ) {
        const int index = staticMetaObject.indexOfEnumerator("LineService");
        return QLatin1String(staticMetaObject.enumerator(index).valueToKey(lineService));
    };
    static inline QString toString( MoreItemsDirection moreItemsDirection ) {
        const int index = staticMetaObject.indexOfEnumerator("MoreItemsDirection");
        return QLatin1String(staticMetaObject.enumerator(index).valueToKey(moreItemsDirection));
    };
    static TimetableInformation stringToTimetableInformation( const char *timetableInformation ) {
        const int index = staticMetaObject.indexOfEnumerator("TimetableInformation");
        const int value = staticMetaObject.enumerator(index).keyToValue(timetableInformation);
        return value == -1 ? Nothing : static_cast< TimetableInformation >( value );
    };
    static ServiceProviderType stringToProviderType( const char *providerType ) {
        const int index = staticMetaObject.indexOfEnumerator("ServiceProviderType");
        const int value = staticMetaObject.enumerator(index).keyToValue(providerType);
        return value == -1 ? InvalidProvider : static_cast< ServiceProviderType >( value );
    };
    static ProviderFeature stringToFeature( const char *feature ) {
        const int index = staticMetaObject.indexOfEnumerator("ProviderFeature");
        const int value = staticMetaObject.enumerator(index).keyToValue(feature);
        return static_cast< ProviderFeature >( value );
    };
    static VehicleType stringToVehicleType( const char *vehicleType ) {
        const int index = staticMetaObject.indexOfEnumerator("VehicleType");
        const int value = staticMetaObject.enumerator(index).keyToValue(vehicleType);
        return value == -1 ? InvalidVehicleType : static_cast< VehicleType >( value );
    };
    static LineService stringToLineService( const char *lineService ) {
        const int index = staticMetaObject.indexOfEnumerator("LineService");
        const int value = staticMetaObject.enumerator(index).keyToValue(lineService);
        return value == -1 ? NoLineService : static_cast< LineService >( value );
    };
    static MoreItemsDirection stringToMoreItemsDirection( const char *moreItemsDirection ) {
        const int index = staticMetaObject.indexOfEnumerator("MoreItemsDirection");
        const int value = staticMetaObject.enumerator(index).keyToValue(moreItemsDirection);
        return value == -1 ? RequestedItems : static_cast< MoreItemsDirection >( value );
    };
};
Q_DECLARE_METATYPE( Enums::ServiceProviderType )
Q_DECLARE_METATYPE( Enums::TimetableInformation )

/** @brief Stores information about a departure/arrival/journey/stop suggestion. */
typedef QHash<Enums::TimetableInformation, QVariant> TimetableData;

/** @brief Different modes for parsing documents. */
enum ParseDocumentMode {
    ParseInvalid = 0, /**< Invalid value, only used to mark as invalid. */
    ParseForDepartures, /**< Parsing for departures. */
    ParseForArrivals, /**< Parsing for arrivals. */
    ParseForJourneysByDepartureTime, /**< Parsing for journeys departing at a given time. */
    ParseForJourneysByArrivalTime, /**< Parsing for journeys arriving at a given time. */
    ParseForStopSuggestions, /**< Parsing for stop suggestions. */
    ParseForAdditionalData /**< Parsing for additional data. */
};
Q_DECLARE_METATYPE( ParseDocumentMode )

/** @brief Enumerables describing for what to wait in ScriptJob/DebuggerJob::waitFor(). */
enum WaitForType {
    WaitForNothing, /**< Wait for the signal only. */
    WaitForNetwork, /**< Wait for all running network requests to finish. */
    WaitForInterrupt, /**< Wait for the next interrupt. */
    WaitForScriptFinish, /**< Wait until script execution is finished. */
    WaitForInjectedScriptFinish /**< Wait until execution of injected script code is finished. */
};

/** @brief Flags for data source updates. */
enum UpdateFlag {
    NoUpdateFlags = 0x00, /**< No update flags. */
    SourceHasConstantTime = 0x01, /**< The data source to update contains timetable items
            * for a constant time. */
    UpdateWasRequestedManually = 0x02, /**< The update was requested manually. */
    DefaultUpdateFlags = NoUpdateFlags /**< Default update flags. */
};
Q_DECLARE_FLAGS( UpdateFlags, UpdateFlag )

/* Functions for nicer debug output */
inline QDebug &operator <<( QDebug debug, ParseDocumentMode parseDocumentMode )
{
    switch ( parseDocumentMode ) {
    case ParseForDepartures:
        return debug << "ParseForDepartures";
    case ParseForArrivals:
        return debug << "ParseForArrivals";
    case ParseForJourneysByDepartureTime:
        return debug << "ParseForJourneysByDepartureTime";
    case ParseForJourneysByArrivalTime:
        return debug << "ParseForJourneysByArrivalTime";
    case ParseForStopSuggestions:
        return debug << "ParseForStopSuggestions";
    case ParseForAdditionalData:
        return debug << "ParseForAdditionalData";

    default:
        return debug << "ParseDocumentMode unknown" << static_cast<int>(parseDocumentMode);
    }
}

inline QDebug &operator <<( QDebug debug, Enums::TimetableInformation timetableInformation )
{
    return debug << Enums::toString( timetableInformation );
}

inline QDebug &operator <<( QDebug debug, Enums::ServiceProviderType providerType )
{
    return debug << Enums::toString( providerType );
}

inline QDebug &operator <<( QDebug debug, Enums::VehicleType vehicleType )
{
    return debug << Enums::toString( vehicleType );
}

inline QDebug &operator <<( QDebug debug, Enums::LineService lineService )
{
    return debug << Enums::toString( lineService );
}

#endif // ENUMS_HEADER
