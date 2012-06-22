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
* @brief This file contains enumerations used by the public transport data engine.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef ENUMS_HEADER
#define ENUMS_HEADER

#include <QDebug>
#include <QDate>

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

/**
 * @brief Error codes.
 **/
enum ErrorCode {
    NoError = 0, /**< There were no error. */

    ErrorDownloadFailed = 1, /**< Download error occurred. */
    ErrorParsingFailed = 2, /**< Parsing downloaded data failed. */
    ErrorNeedsImport = 3 /**< An import step needs to be performed, before using the accessor.
            * This is currently only used for GTFS accessors, which need to import the GTFS feed
            * before being usable. */
};

/**
 * @brief Different types of timetable information.
 *
 * In scripts the enumerable names can be used as property names of a result object, eg.:
 * @code
 * result.addData({ DepartureDateTime: new Date(),
 *                  TypeOfVehicle: "Bus",
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
    JourneyNews = 11,  /**< Can contain delay / delay reason / other news */
    JourneyNewsOther = 12, /**< Other news (not delay / delay reason) */

    /**
     * Contains a link to an html page with journey news. The url of the service provider is
     * prepended, if a relative path has been matched (starting with "/").
     **/
    JourneyNewsLink = 13,

    Operator = 16, /**< The company that is responsible for the journey. */
    Status = 20, /**< The current status of the departure / arrival. Currently only used for planes. */

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
     * Can contain list elements of type QTime (ie. QtScript Date) or QString in format "hh:mm".
     **/
    RouteTimes = 24,

    /**
     * A list of departure times of the journey. If @ref RouteStops and @ref RouteTimesDeparture
     * are both set, the latter should contain one elements less (because the last stop has no
     * departure, only an arrival time). Elements with equal indices should be associated (the
     * times at which the vehicle departs from the stops).
     * Can contain list elements of type QTime (ie. QtScript Date) or QString in format "hh:mm".
     **/
    RouteTimesDeparture = 25,

    /**
     * A list of arrival times of the journey. If @ref RouteStops and @ref RouteTimesArrival are
     * both set, the latter should contain one elements less (because the first stop has no
     * arrival, only a departure time). Elements with equal indices should be associated (the
     * times at which the vehicle arrives at the stops).
     * Can contain list elements of type QTime (ie. QtScript Date) or QString in format "hh:mm".
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
    StopCountryCode = 204 /**< The country code of the country in which the stop is. */
};

/** @brief Stores information about a departure/arrival/journey/stop suggestion. */
typedef QHash<TimetableInformation, QVariant> TimetableData;

/** @brief Different modes for parsing documents. */
enum ParseDocumentMode {
    ParseInvalid = 0, /**< Invalid value, only used to mark as invalid. */
    ParseForDeparturesArrivals = 1, /**< Parsing for departures or arrivals. */
    ParseForJourneys, /**< Parsing for journeys. */
    ParseForStopSuggestions, /**< Parsing for stop suggestions. */
    ParseForStopIdThenDepartures /**< Parsing for a stop ID, to be used to get departures/arrivals. */
};

/** @brief The type of a service provider. */
enum ServiceProviderType {
    InvalidProvider, /**< @internal Invalid value. */

    /**
     * Uses a script to request and parse documents. Scripts can make use of
     * several helper objects to download documents (GET or POST), store found departures/
     * journeys/stop suggestions, cache values, parse HTML, notify about errors, etc.
     * QtScript extensions can be used, eg. qt.xml to parse XML documents.
     **/
    ScriptedProvider,

    /**
     * The accessor uses a DB filled with GTFS data.
     **/
    GtfsProvider
};

/**
 * @brief The type of the vehicle used for a public transport line.
 *
 * Scripts can use the names (case insensitive) of these enumerables as vehicle types.
 * @code
 * result.addData({ TypeOfVehicle: "Bus" });
 * result.addData({ TypeOfVehicle: "traM" });
 * result.addData({ TypeOfVehicle: "RegionalExpressTrain" });
 * @endcode
 **/
enum VehicleType {
    Unknown = 0, /**< The type of the vehicle is unknown. */

    Tram = 1, /**< @image html hi16-app-vehicle_type_tram.png A tram / streetcar. */
    Bus = 2, /**< @image html hi16-app-vehicle_type_bus.png A bus. */
    Subway = 3, /**< @image html hi16-app-vehicle_type_subway.png A subway. */

    /** @image html hi16-app-vehicle_type_train_interurban.png An interurban train. */
    InterurbanTrain = 4,

    Metro = 5, /**< @image html hi16-app-vehicle_type_metro.png A metro. */

    /**
     * @image html hi16-app-vehicle_type_trolleybus.png
     * A trolleybus (also known as trolley bus, trolley coach, trackless trolley, trackless tram
     * or trolley) is an electric bus that draws its electricity from overhead wires (generally
     * suspended from roadside posts) using spring-loaded trolley poles.
     **/
    TrolleyBus = 6,

    /**
     * @image html hi16-app-vehicle_type_train_regional.png
     * A regional train. Stops at many small stations, slow.
     **/
    RegionalTrain = 10,

    /**
     * @image html hi16-app-vehicle_type_train_regional.png
     * A regional express train. Stops at less small stations than RegionalTrain but is faster.
     **/
    RegionalExpressTrain = 11,

    /**
     * @image html hi16-app-vehicle_type_train_interregional.png
     * An inter-regional train. Higher distances and faster than RegionalTrain and
     * RegionalExpressTrain.
     **/
    InterregionalTrain = 12,

    /**
     * @image html hi16-app-vehicle_type_train_intercity.png
     * An intercity / eurocity train. Connects cities.
     **/
    IntercityTrain = 13,

    /**
     * @image html hi16-app-vehicle_type_train_highspeed.png
     * A highspeed train, eg. an ICE (intercity express) or TGV.
     * Trains at > 250 km/h, high distances.
     **/
    HighSpeedTrain = 14,

    /**
     * @image html hi16-app-vehicle_type_feet.png
     * By feet, ie. no vehicle. Used for journeys, eg. from platform A to platform B when changing
     * the vehicle.
     **/
    Feet = 50,

    /** @image html hi16-app-vehicle_type_ferry.png A ferry. */
    Ferry = 100,

    /** @image html hi16-app-vehicle_type_ferry.png A ship, but not a ferry. */
    Ship = 101,

    /** @image html hi16-app-vehicle_type_plane.png An aeroplane. */
    Plane = 200,

    /** A spacecraft. @todo Currently unused. */
    Spacecraft = 300
};

/** @brief The type of services for a public transport line. */
enum LineService {
    NoLineService = 0x00, /**< The public transport line has no special services. */

    NightLine = 0x01, /**< The public transport line is a night line. */
    ExpressLine = 0x02 /**< The public transport line is an express line. */
};
// Q_DECLARE_FLAGS( LineServices, LineService ); // Gives a compiler error here.. but not in departureinfo.h TODO #include <QMetaType>

/* Functions for nicer debug output */
inline QDebug &operator <<( QDebug debug, ParseDocumentMode parseDocumentMode )
{
    switch ( parseDocumentMode ) {
    case ParseForDeparturesArrivals:
        return debug << "ParseForDeparturesArrivals";
    case ParseForJourneys:
        return debug << "ParseForJourneys";
    case ParseForStopSuggestions:
        return debug << "ParseForStopSuggestions";
    case ParseForStopIdThenDepartures:
        return debug << "ParseForStopIdThenDepartures";

    default:
        return debug << "ParseDocumentMode unknown" << static_cast<int>(parseDocumentMode);
    }
}

inline QDebug &operator <<( QDebug debug, TimetableInformation timetableInformation )
{
    switch ( timetableInformation ) {
    case Nothing:
        return debug << "Nothing";
    case DepartureDateTime:
        return debug << "DepartureDateTime";
    case DepartureDate:
        return debug << "DepartureDate";
    case DepartureTime:
        return debug << "DepartureTime";
    case TypeOfVehicle:
        return debug << "TypeOfVehicle";
    case TransportLine:
        return debug << "TransportLine";
    case Target:
        return debug << "Target";
    case TargetShortened:
        return debug << "TargetShortened";
    case Platform:
        return debug << "Platform";
    case Delay:
        return debug << "Delay";
    case DelayReason:
        return debug << "DelayReason";
    case JourneyNews:
        return debug << "JourneyNews";
    case JourneyNewsOther:
        return debug << "JourneyNewsOther";
    case JourneyNewsLink:
        return debug << "JourneyNewsLink";
    case Operator:
        return debug << "Operator";
    case Status:
        return debug << "Status";
    case Duration:
        return debug << "Duration";
    case StartStopName:
        return debug << "StartStopName";
    case StartStopID:
        return debug << "StartStopID";
    case StopWeight:
        return debug << "StopWeight";
    case StopCity:
        return debug << "StopCity";
    case StopCountryCode:
        return debug << "StopCountryCode";
    case TargetStopName:
        return debug << "TargetStopName";
    case TargetStopID:
        return debug << "TargetStopID";
    case ArrivalDateTime:
        return debug << "ArrivalDateTime";
    case ArrivalDate:
        return debug << "ArrivalDate";
    case ArrivalTime:
        return debug << "ArrivalTime";
    case Changes:
        return debug << "Changes";
    case TypesOfVehicleInJourney:
        return debug << "TypesOfVehicleInJourney";
    case Pricing:
        return debug << "Pricing";
    case IsNightLine:
        return debug << "IsNightline";
    case StopName:
        return debug << "StopName";
    case StopID:
        return debug << "StopID";
    case RouteStops:
        return debug << "RouteStops";
    case RouteStopsShortened:
        return debug << "RouteStopsShortened";
    case RouteTimes:
        return debug << "RouteTimes";
    case RouteTimesDeparture:
        return debug << "RouteTimesDeparture";
    case RouteTimesArrival:
        return debug << "RouteTimesArrival";
    case RouteExactStops:
        return debug << "RouteExactStops";
    case RouteTypesOfVehicles:
        return debug << "RouteTypesOfVehicles";
    case RouteTransportLines:
        return debug << "RouteTransportLines";
    case RoutePlatformsDeparture:
        return debug << "RoutePlatformsDeparture";
    case RoutePlatformsArrival:
        return debug << "RoutePlatformsArrival";
    case RouteTimesDepartureDelay:
        return debug << "RouteTimesDepartureDelay";
    case RouteTimesArrivalDelay:
        return debug << "RouteTimesArrivalDelay";

    default:
        return debug << "TimetableInformation unknown" << static_cast<int>(timetableInformation);
    }
}

#endif // ENUMS_HEADER
