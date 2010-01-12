/*
 *   Copyright 2009 Friedrich Pülz <fpuelz@gmx.de>
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

/** Different types of information. */
enum TimetableInformation {
    Nothing = 0, /**< No usable information. */

    // Departures / arrival / journey infos
    DepartureDate = 1, /**< The date of the departure. */
    DepartureHour = 2, /**< The hour of the departure. */
    DepartureMinute = 3, /**< The minute of the departure. */
    TypeOfVehicle = 4, /**< The type of vehicle. */
    TransportLine = 5, /**< The name of the public transport line, e.g. "4", "6S", "S 5", "RB 24122". */
    FlightNumber = TransportLine, /**< Same as TransportLine, used for flights. */
    Target = 6, /**< The target of a journey / of a public transport line. */
    Platform = 7, /**< The platform at which the vehicle departs / arrives. */
    Delay = 8, /**< The delay of a public transport vehicle. */
    DelayReason = 9, /**< The reason of a delay. */
    JourneyNews = 10,  /**< Can contain delay / delay reason / other news */
    JourneyNewsOther = 11,  /**< Other news (not delay / delay reason) */
    JourneyNewsLink = 12,  /**< Contains a link to an html page with journey news. The url of the accessor is prepended, if a relative path has been matched (starting with "/"). */
    DepartureHourPrognosis = 13, /**< The prognosis for the departure hour, which is the departure hour plus the delay. */
    DepartureMinutePrognosis = 14, /**< The prognosis for the departure minute, which is the departure minute plus the delay. */
    Operator = 16, /**< The company that is responsible for the journey. */
    DepartureAMorPM = 17, /**< Used to match the string "am" or "pm" for the departure time. */
    DepartureAMorPMPrognosis = 18, /**< Used to match the string "am" or "pm" for the prognosis departure time. */
    ArrivalAMorPM = 19, /**< Used to match the string "am" or "pm" for the arrival time. */
    Status = 20, /**< The current status of the departure / arrival. Currently only used for planes. */
    DepartureYear = 21, /**< The year of the departure, to be used when the year is seperated from the date. */ // TODO: add to timetableInformationToString()
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
    
    // Journey infos
    Duration = 50, /**< The duration of a journey. */
    StartStopName = 51, /**< The name of the starting stop of a journey. */
    StartStopID = 52, /**< The ID of the starting stop of a journey. */
    TargetStopName = 53, /**< The name of the target stop of a journey. */
    TargetStopID = 54, /**< The ID of the target stop of a journey. */
    ArrivalDate = 55, /**< The date of the arrival. */
    ArrivalHour = 56, /**<The hour of the arrival. */
    ArrivalMinute = 57, /**< The minute of the arrival. */
    Changes = 58, /**< The number of changes between different vehicles in a journey. */
    TypesOfVehicleInJourney = 59, /**< A list of vehicle types used in a journey. */
    Pricing = 60, /**< Information about the pricing of a journey. */

    // Special infos
    NoMatchOnSchedule = 100, /**< Vehicle is expected to depart on schedule, no regexp-matched string is needed for this info */

    // Possible stop infos
    StopName = 200, /**< The name of a stop / station. */
    StopID = 201 /**< The ID of a stop / station. */
};

/** Different modes for parsing documents. */
enum ParseDocumentMode {
    ParseForDeparturesArrivals = 1, /**< Parsing for departures or arrivals. */
    ParseForJourneys, /**< Parsing for journeys. */
    ParseForStopSuggestions  /**< Parsing for stop suggestions. */
};

/** The type of an accessor. */
enum AccessorType {
    NoAccessor, /**< @internal Invalid value */
    HTML, /**< The accessor is used to parse HTML documents. */
    XML /**< The accessor is used to parse XML documents. */
};

/** The type of the vehicle used for a public transport line. */
enum VehicleType {
    Unknown = 0, /**< The type of the vehicle is unknown. */

    Tram = 1, /**< A tram / streetcar. */
    Bus = 2, /**< A bus. */
    Subway = 3, /**< A subway. */
    TrainInterurban = 4, /**< An interurban train. */
    Metro = 5, /**< A metro. */
    TrolleyBus = 6, /**< An electric bus. */

    TrainRegional = 10, /**< A regional train. */
    TrainRegionalExpress = 11, /**< A regional express train. */
    TrainInterregio = 12, /**< An inter-regional train. */
    TrainIntercityEurocity = 13, /**< An intercity / eurocity train. */
    TrainIntercityExpress = 14, /**< An intercity express. */
    
    Feet = 50, /**< By feet. */
    
    Ferry = 100, /**< A ferry. */
    Ship = 101, /**< A ship. */

    Plane = 200, /**< An aeroplane. */

    Spacecraft = 300, /**< A spacecraft. */ // TODO: add to applet!
};

/** The type of services for a public transport line. */
enum LineService {
    NoLineService = 0x00, /**< The public transport line has no special services. */

    NightLine = 0x01, /**< The public transport line is a night line. */
    ExpressLine = 0x02 /**< The public transport line is an express line. */
};
// Q_DECLARE_FLAGS( LineServices, LineService ); // Gives a compiler error here.. but not in departureinfo.h

/** What calculation should be done to get a missing value. */
enum CalculateMissingValue {
    CalculateDelayFromDepartureAndPrognosis,
    CalculateDepartureDate,
    CalculateArrivalDateFromDepartureDate,
    CalculateDurationFromDepartureAndArrival,
    CalculateArrivalFromDepartureAndDuration,
    CalculateDepartureFromArrivalAndDuration
};

inline QDebug &operator <<( QDebug debug, ParseDocumentMode parseDocumentMode ) {
    switch ( parseDocumentMode ) {
	case ParseForDeparturesArrivals:
	    return debug << "ParseForDeparturesArrivals";
	case ParseForJourneys:
	    return debug << "ParseForJourneys";
	case ParseForStopSuggestions:
	    return debug << "ParseForStopSuggestions";
	    
	default:
	    return debug << "ParseDocumentMode unknown" << parseDocumentMode;
    }
}

inline QDebug &operator <<( QDebug debug, TimetableInformation timetableInformation ) {
    switch ( timetableInformation ) {
	case Nothing:
	    return debug << "Nothing";
	case DepartureDate:
	    return debug << "DepartureDate";
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
	case TargetStopName:
	    return debug << "TargetStopName";
	case TargetStopID:
	    return debug << "TargetStopID";
	case ArrivalDate:
	    return debug << "ArrivalDate";
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
	    return debug << "TimetableInformation unknown" << timetableInformation;
    }
};

#endif // ENUMS_HEADER