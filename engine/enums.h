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

#include <vector>

#ifndef ENUMS_HEADER
#define ENUMS_HEADER

/** Enumeration of implemented service providers with IDs as values.
* Use availableServiceProviders to loop over all available service providers.
* If you implemented support for a new service provider, add a value here and
* also add it to availableServiceProvidersArray. */
enum ServiceProvider {
    NoServiceProvider = -1, /**< @internal No service provider, invalid value */

// Germany (0 .. 50? in germany.)
    DB = 1, /**< Service provider for Germany. */
    RMV = 2, /**< Service provider for the Rhine-Main region in germany. */
    VVS = 3, /**< Service provider for Stuttgart in germany. */
    VRN = 4, /**< Service provider for the Rhine-Neckar region in germany. */
    BVG = 5, /**< Service provider for Berlin in germany. */
    DVB = 6, /**< Service provider for Dresden in germany. */
    NASA = 7, /**< Service provider for Saxony-Anhalt in germany. */
    Fahrplaner = 8, /**< Service provider for Lower Saxony / Bremen in Germany. */

    SBB = 20, /**< Service provider for Switzerland. */

    OEBB = 30, /**< Service provider for Autstria. */

    // Slovakia (1000 .. 1009)
    IMHD = 1000, /**< Service provider for Bratislava. */

    // Czech (1010 .. 1019)
    IDNES = 1010,  /**< Service provider for Czechia. */

    // Poland (1020 .. 1029)
    PKP = 1020 /**< Service provider for Poland. */
};

/** An array containing all available service providers. */
const ServiceProvider availableServiceProvidersArray[] = { Fahrplaner, RMV, VVS, VRN, BVG, DVB, NASA, DB, SBB, OEBB, IMHD, IDNES, PKP };

/** A std::vector used to loop through all available service providers. */
const std::vector<ServiceProvider> availableServiceProviders( availableServiceProvidersArray, availableServiceProvidersArray + sizeof(availableServiceProvidersArray) / sizeof(int) );


/** Different types of information. */
enum TimetableInformation {
    Nothing = 0, /**< No usable information. */

    DepartureDate = 1, /**< The date of the departure. */
    DepartureHour = 2, /**< The hour of the departure. */
    DepartureMinute = 3, /**< The minute of the departure. */
    TypeOfVehicle = 4, /**< The type of vehicle. */
    TransportLine = 5, /**< The name of the public transport line, e.g. "4", "6S", "S 5", "RB 24122". */
    Target = 6, /**< The target of a journey / of a public transport line. */
    Platform = 7, /**< The platform at which the vehicle departs / arrives. */
    Delay = 8, /**< The delay of a public transport vehicle. */
    DelayReason = 9, /**< The reason of a delay. */
    JourneyNews = 10,  /**< Can contain delay / delay reason / other news */
    JourneyNewsOther = 11,  /**< Other news (not delay / delay reason) */
    DepartureHourPrognosis = 12, /**< The prognosis for the departure hour, which is the departure hour plus the delay. */
    DepartureMinutePrognosis = 13, /**< The prognosis for the departure minute, which is the departure minute plus the delay. */

    Duration = 20, /**< The duration of a journey. */
    StartStopName = 21, /**< The name of the starting stop of a journey. */
    StartStopID = 22, /**< The ID of the starting stop of a journey. */
    TargetStopName = 23, /**< The name of the target stop of a journey. */
    TargetStopID = 24, /**< The ID of the target stop of a journey. */
    ArrivalDate = 25, /**< The date of the arrival. */
    ArrivalHour = 26, /**<The hour of the arrival. */
    ArrivalMinute = 27, /**< The minute of the arrival. */
    Changes = 28, /**< The number of changes between different vehicles in a journey. */
    TypesOfVehicleInJourney = 29, /**< A list of vehicle types used in a journey. */
    Pricing = 30, /**< Information about the pricing of a journey. */

    NoMatchOnSchedule = 50, /**< Vehicle is expected to depart on schedule, no regexp-matched string is needed for this info */

    StopName = 100, /**< The name of a stop / station. */
    StopID = 101 /**< The ID of a stop / station. */
};

/** Different modes for parsing documents. */
enum ParseDocumentMode {
    ParseForDeparturesArrivals, /**< Parsing for departures or arrivals. */
    ParseForJourneys /**< Parsing for journeys. */
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

    TrainRegional = 10, /**< A regional train. */
    TrainRegionalExpress = 11, /**< A regional express train. */
    TrainInterregio = 12, /**< An inter-regional train. */
    TrainIntercityEurocity = 13, /**< An intercity / eurocity train. */
    TrainIntercityExpress = 14 /**< An intercity express. */
};

/** The type of services for a public transport line. */
enum LineService {
    NoLineService = 0x00, /**< The public transport line has no special services. */

    NightLine = 0x01, /**< The public transport line is a night line. */
    ExpressLine = 0x02 /**< The public transport line is an express line. */
};
// Q_DECLARE_FLAGS( LineServices, LineService ); // Gives a compiler error here.. but not in departureinfo.h

#endif // ENUMS_HEADER