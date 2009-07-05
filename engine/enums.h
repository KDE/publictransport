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
#include <vector>

#ifndef ENUMS_HEADER
#define ENUMS_HEADER

// Enumeration of implemented service providers with IDs as values.
// Use availableServiceProviders to loop over all available service providers.
// If you implemented support for a new service provider, add a value here and also add it to availableServiceProvidersArray.
enum ServiceProvider
{
    NoServiceProvider = -1,

// Germany (0 .. 50?)
    DB = 1, // Germany
    RMV = 2, // Rhine-Main
    VVS = 3, // Stuttgart
    VRN = 4, // Rhine-Neckar
    BVG = 5, // Berlin
    DVB = 6, // Dresden
    NASA = 7, // Saxony-Anhalt
    Fahrplaner = 8, // Lower saxony/Bremen

    SBB = 20, // Switzerland

    OEBB = 30, // Autstria

    // Slovakia (1000 .. 1009)
    IMHD = 1000, // Bratislava

    // Czech (1010 .. 1019)
    IDNES = 1010,

    // Poland (1020 .. 1029)
    PKP = 1020
};
const ServiceProvider availableServiceProvidersArray[] = { Fahrplaner, RMV, VVS, VRN, BVG, DVB, NASA, DB, SBB, OEBB, IMHD, IDNES, PKP };
const std::vector<ServiceProvider> availableServiceProviders( availableServiceProvidersArray, availableServiceProvidersArray + sizeof(availableServiceProvidersArray) / sizeof(int) );

enum TimetableInformation
{
    Nothing = 0,

    DepartureHour = 1,
    DepartureMinute = 2,
    TypeOfVehicle = 3,
    TransportLine = 4,
    Direction = 5,
    Platform = 6,
    Delay = 7,
    DelayReason = 8,
    JourneyNews = 9, // Can contain delay / delay reason / other news
    JourneyNewsOther = 10, // Can contain other news (not delay / delay reason)
    DepartureHourPrognosis = 11,
    DepartureMinutePrognosis = 12,

    NoMatchOnSchedule = 50, // Vehicle is expected to depart on schedule, no regexp-matched string is needed for this info

    StopName = 100,
    StopID = 101
};

enum AccessorType
{
    NoAccessor,
    HTML,
    XML
};

// The type of the vehicle used for a public transport line.
enum VehicleType
{
    Unknown = 0,
    Tram = 1,
    Bus = 2,
    Subway = 3,
    TrainInterurban = 4,

    TrainRegional = 10,
    TrainRegionalExpress = 11,
    TrainInterregio = 12,
    TrainIntercityEurocity = 13,
    TrainIntercityExpress = 14
};

// The type of services for a public transport line.
enum LineService
{
    NoLineService = 0,

    NightLine = 1,
    ExpressLine = 2
};
// Q_DECLARE_FLAGS( LineServices, LineService ); // Gives a compiler error here.. but not in departureinfo.h

#endif // ENUMS_HEADER