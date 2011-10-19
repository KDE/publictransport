/*
 *   Copyright 2010 Friedrich Pülz <fpuelz@gmx.de>
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
        datesNeedAdjustment = false;
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
     * @brief Whether or not dates are set from today instead of the requested date.
     *
     * If this is true, all dates need to be adjusted by X days, where X is the difference in days
     * between today and the requested date.
     **/
    bool datesNeedAdjustment;

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
 * @brief Places where the session key should be put in requests.
 **/
enum SessionKeyPlace {
    PutNowhere = 0, /**< Don't place the session key anywhere. */
    PutIntoCustomHeader /**< Place the session key in a custom header, which name should be given
            * as session key <em>data</em>. */
};

/**
 * @brief Different types of timetable information.
 *
 * In scripts the enumerable names can be used as strings, eg.:
 * @code
 * timetableData.set("TypeOfVehicle", "Bus"); // Set the type of vehicle
 * timetableData.set("Delay", 5); // Set a delay of 5 minutes
 * @endcode
 */
enum TimetableInformation {
    Nothing = 0, /**< No usable information. */

    // Departures / arrival / journey information
    DepartureDate = 1, /**< The date of the departure/arrival/journey. */
    DepartureTime = 2, /**< The time of the departure/arrival/journey, can include seconds. TODO */
    DepartureHour = 3, /**< The hour of the departure/arrival/journey. */
    DepartureMinute = 4, /**< The minute of the departure/arrival/journey. */

    TypeOfVehicle = 5, /**< The type of vehicle. */
    TransportLine = 6, /**< The name of the public transport line, e.g. "4", "6S", "S 5", "RB 24122". */
    FlightNumber = TransportLine, /**< Same as TransportLine, used for flights. */
    Target = 7, /**< The target of a journey / of a public transport line. */
    Platform = 8, /**< The platform at which the vehicle departs / arrives. */
    Delay = 9, /**< The delay of a public transport vehicle. */
    DelayReason = 10, /**< The reason of a delay. */
    JourneyNews = 11,  /**< Can contain delay / delay reason / other news */
    JourneyNewsOther = 12,  /**< Other news (not delay / delay reason) */
    JourneyNewsLink = 13,  /**< Contains a link to an html page with journey news. The url of the accessor is prepended, if a relative path has been matched (starting with "/"). */
    DepartureHourPrognosis = 14, /**< The prognosis for the departure hour, which is the departure hour plus the delay. */
    DepartureMinutePrognosis = 15, /**< The prognosis for the departure minute, which is the departure minute plus the delay. */
    Operator = 16, /**< The company that is responsible for the journey (in GTFS this is called the 'agency'). */
    DepartureAMorPM = 17, /**< Used to match the string "am" or "pm" for the departure time. */
    DepartureAMorPMPrognosis = 18, /**< Used to match the string "am" or "pm" for the prognosis departure time. */
    ArrivalAMorPM = 19, /**< Used to match the string "am" or "pm" for the arrival time. */
    Status = 20, /**< The current status of the departure / arrival. Currently only used for planes. */
    DepartureYear = 21, /**< The year of the departure, to be used when the year is separated from the date. */
    RouteStops = 22, /**< A list of stops of the departure / arrival to it's destination stop or a list of stops of the journey from it's start to it's destination stop. If @ref RouteStops and @ref RouteTimes are both set, they should contain the same number of elements. And elements with equal indices should be associated (the times at which the vehicle is at the stops). For journeys @ref RouteTimesDeparture and @ref RouteTimesArrival should be used instead of @ref RouteTimes. */
    RouteTimes = 23, /**< A list of times of the departure / arrival to it's destination stop. If @ref RouteStops and @ref RouteTimes are both set, they should contain the same number of elements. And elements with equal indices should be associated (the times at which the vehicle is at the stops). */
    RouteTimesDeparture = 24, /**< A list of departure times of the journey. If @ref RouteStops and @ref RouteTimesDeparture are both set, the latter should contain one elements less (because the last stop has no departure, only an arrival time). Elements with equal indices should be associated (the times at which the vehicle departs from the stops). */
    RouteTimesArrival = 25, /**< A list of arrival times of the journey. If @ref RouteStops and @ref RouteTimesArrival are both set, the latter should contain one elements less (because the first stop has no arrival, only a departure time). Elements with equal indices should be associated (the times at which the vehicle arrives at the stops). */
    RouteExactStops = 26, /**< The number of exact route stops. The route stop list isn't complete from the last exact route stop. */
    RouteTypesOfVehicles = 27, /**< The types of vehicles used for each "sub-journey" of a journey. */
    RouteTransportLines = 28, /**< The transport lines used for each "sub-journey" of a journey. */
    RoutePlatformsDeparture = 29, /**< The platforms of departures used for each "sub-journey" of a journey. */
    RoutePlatformsArrival = 30, /**< The platforms of arrivals used for each "sub-journey" of a journey. */
    RouteTimesDepartureDelay = 31, /**< A list of delays in minutes for each departure time of a route (RouteTimesDeparture). */
    RouteTimesArrivalDelay = 32, /**< A list of delays in minutes for each arrival time of a route (RouteTimesArrival). */

    // Journey information
    Duration = 50, /**< The duration of a journey. */
    StartStopName = 51, /**< The name of the starting stop of a journey. */
    StartStopID = 52, /**< The ID of the starting stop of a journey. */
    TargetStopName = 53, /**< The name of the target stop of a journey. */
    TargetStopID = 54, /**< The ID of the target stop of a journey. */
    ArrivalDate = 55, /**< The date of the arrival. */
    ArrivalTime = 56, /**< The time of the arrival, can include seconds. */
    ArrivalHour = 57, /**<The hour of the arrival. */
    ArrivalMinute = 58, /**< The minute of the arrival. */
    Changes = 59, /**< The number of changes between different vehicles in a journey. */
    TypesOfVehicleInJourney = 60, /**< A list of vehicle types used in a journey. */
    Pricing = 61, /**< Information about the pricing of a journey. */

    // Special information
    NoMatchOnSchedule = 100, /**< Vehicle is expected to depart on schedule, no regexp-matched string is needed for this info */
    IsNightLine = 101, /**< The transport line is a nightline. TODO Use this info in the data engine */

    // Stop suggestion information
    StopName = 200, /**< The name of a stop/station. */
    StopID = 201, /**< The ID of a stop/station. */
    StopWeight = 202, /**< The weight of a stop suggestion. */
    StopCity = 203, /**< The city in which a stop is. */
    StopCountryCode = 204 /**< The country code of the country in which the stop is. */
};

/** @brief Different modes for parsing documents. */
enum ParseDocumentMode {
    ParseInvalid = 0, /**< Invalid value, only used to mark as invalid. */
    ParseForDeparturesArrivals = 1, /**< Parsing for departures or arrivals. */
    ParseForJourneys, /**< Parsing for journeys. */
    ParseForStopSuggestions, /**< Parsing for stop suggestions. */
    ParseForSessionKeyThenStopSuggestions, /**< Parsing for a session key, to be used to get stop suggestions. */
    ParseForSessionKeyThenDepartures, /**< Parsing for a session key, to be used to get departures/arrivals. */
    ParseForStopIdThenDepartures /**< Parsing for a stop ID, to be used to get departures/arrivals. */
};

/** @brief The type of an accessor. */
enum AccessorType {
    NoAccessor, /**< @internal Invalid value */
    ScriptedAccessor, /**< The accessor parses documents using a script. */
    XmlAccessor, /**< The accessor parses XML documents. */
    GtfsAccessor /**< The accessor uses a DB filled with GTFS data. */
};

/** @brief The type of the vehicle used for a public transport line. */
enum VehicleType {
    Unknown = 0, /**< The type of the vehicle is unknown. */

    Tram = 1, /**< A tram / streetcar. */
    Bus = 2, /**< A bus. */
    Subway = 3, /**< A subway. */
    TrainInterurban = 4, /**< An interurban train. @deprecated Use InterurbanTrain instead. */
    InterurbanTrain = 4, /**< An interurban train. */
    Metro = 5, /**< A metro. */
    TrolleyBus = 6, /**< A trolleybus (also known as trolley bus, trolley coach,
            * trackless trolley, trackless tram or trolley) is an electric bus that draws its
            * electricity from overhead wires (generally suspended from roadside posts)
            * using spring-loaded trolley poles. */

    TrainRegional = 10, /**< A regional train. @deprecated Use RegionalTrain instead. */
    TrainRegionalExpress = 11, /**< A regional express train. @deprecated Use RegionalExpressTrain instead. */
    TrainInterregio = 12, /**< An inter-regional train. @deprecated Use InterregionalTrain instead. */
    TrainIntercityEurocity = 13, /**< An intercity / eurocity train. @deprecated Use IntercityTrain instead. */
    TrainIntercityExpress = 14, /**< An intercity express. @deprecated Use HighSpeedTrain instead. */

    RegionalTrain = 10, /**< A regional train. Stops at many small stations, slow. */
    RegionalExpressTrain = 11, /**< A regional express train. Stops at less small stations than
            * RegionalTrain but it faster. */
    InterregionalTrain = 12, /**< An inter-regional train. Higher distances and faster than
            * RegionalTrain and RegionalExpressTrain. */
    IntercityTrain = 13, /**< An intercity / eurocity train. Connects cities. */
    HighSpeedTrain = 14, /**< A highspeed train, eg. an intercity express (ICE).
            * Trains at > 250 km/h, high distances. */

    Feet = 50, /**< By feet, ie. no vehicle. Used for journeys, eg. from platform A to
            * platform B when changing the vehicle. */

    Ferry = 100, /**< A ferry. */
    Ship = 101, /**< A ship. */

    Plane = 200, /**< An aeroplane. */

    Spacecraft = 300, /**< A spacecraft. */ // TODO: add to applet
};

/** @brief The type of services for a public transport line. */
enum LineService {
    NoLineService = 0x00, /**< The public transport line has no special services. */

    NightLine = 0x01, /**< The public transport line is a night line. */
    ExpressLine = 0x02 /**< The public transport line is an express line. */
};
// Q_DECLARE_FLAGS( LineServices, LineService ); // Gives a compiler error here.. but not in departureinfo.h

/** @brief What calculation should be done to get a missing value. */
enum CalculateMissingValue {
    CalculateDelayFromDepartureAndPrognosis,
    CalculateDepartureDate,
    CalculateArrivalDateFromDepartureDate,
    CalculateDurationFromDepartureAndArrival,
    CalculateArrivalFromDepartureAndDuration,
    CalculateDepartureFromArrivalAndDuration
};

/** @class Global
  * @brief Contains global static methods. */
class Global
{
public:
    /** Gets an icon for the given type of vehicle. */
    static QString vehicleTypeToIcon( const VehicleType &vehicleType );

    /** Gets the name of the given type of vehicle. */
    static QString vehicleTypeToString( const VehicleType &vehicleType, bool plural = false );
};

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
    case ParseForSessionKeyThenDepartures:
        return debug << "ParseForSessionKeyThenDepartures";
    case ParseForSessionKeyThenStopSuggestions:
        return debug << "ParseForSessionKeyThenStopSuggestions";
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
    case DepartureDate:
        return debug << "DepartureDate";
    case DepartureTime:
        return debug << "DepartureTime";
    case DepartureHour:
        return debug << "DepartureHour";
    case DepartureMinute:
        return debug << "DepartureMinute";
    case TypeOfVehicle:
        return debug << "TypeOfVehicle";
    case TransportLine:
        return debug << "TransportLine";
    case Target:
        return debug << "Target";
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
    case DepartureHourPrognosis:
        return debug << "DepartureHourPrognosis";
    case DepartureMinutePrognosis:
        return debug << "DepartureMinutePrognosis";
    case Operator:
        return debug << "Operator";
    case DepartureAMorPM:
        return debug << "DepartureAMorPM";
    case DepartureAMorPMPrognosis:
        return debug << "DepartureAMorPMPrognosis";
    case ArrivalAMorPM:
        return debug << "ArrivalAMorPM";
    case Status:
        return debug << "Status";
    case DepartureYear:
        return debug << "DepartureYear";
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
    case ArrivalDate:
        return debug << "ArrivalDate";
    case ArrivalTime:
        return debug << "ArrivalTime";
    case ArrivalHour:
        return debug << "ArrivalHour";
    case ArrivalMinute:
        return debug << "ArrivalMinute";
    case Changes:
        return debug << "Changes";
    case TypesOfVehicleInJourney:
        return debug << "TypesOfVehicleInJourney";
    case Pricing:
        return debug << "Pricing";
    case NoMatchOnSchedule:
        return debug << "NoMatchOnSchedule";
    case IsNightLine:
        return debug << "IsNightline";
    case StopName:
        return debug << "StopName";
    case StopID:
        return debug << "StopID";
    case RouteStops:
        return debug << "RouteStops";
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
};

#endif // ENUMS_HEADER
