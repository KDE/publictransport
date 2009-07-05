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

#ifndef ENUMS_HEADER
#define ENUMS_HEADER

#include <qnamespace.h>
#include <KLocalizedString>

// Different states of alarm
enum AlarmState {
    NoAlarm, // No alarm is set
    AlarmPending, // An alarm is set and pending
    AlarmFired // An alarm has been fired
};

// Indicates the information that an item displays (a cell in the tree view)
enum ItemInformation {
    LineNameItem, // The item contains the line name
    TargetItem, // The item contains the target / origin
    DepartureItem, // The item contains the departure / arrival time
    PlatformLabelItem, // The item contains platform label
    PlatformItem, // The item contains platform
    JourneyNewsLabelItem, // The item contains the journey news label
    JourneyNewsItem, // The item contains the journey news
    DelayLabelItem, // The item contains delay label
    DelayReasonItem, // The item contains the reason of delay
    DelayItem // The item contains the delay
};

// Types of data sources
enum DataType {
    Departures = 0, // The data source contains a list of departures
    Arrivals = 1 // The data source contains a list of arrivals
};

// Indicates what is saved in a model item's data
enum ModelDataRoles {
    SortRole = Qt::UserRole, // Used to store sorting data
    AlarmTimerRole = Qt::UserRole + 4, // Used to store the alarm timer
    VehicleTypeRole = Qt::UserRole + 5, // Used to store the vehicle type
    OriginalBackgroundColorRole = Qt::UserRole + 6, // Used to store the original background color
    ServiceProviderDataRole = Qt::UserRole + 7, // For the service provider combo box
    RemainingMinutesRole = Qt::UserRole + 8, // Used to store an int with the remaining minutes until the predicted departure / arrival (= departure / arrival + delay)
    DepartureInfoRole = Qt::UserRole + 9
};

// The type of the vehicle used for a public transport line.
// The numbers here must match the ones in the data engine!
enum VehicleType {
    Unknown = 0, // The vehicle type is unknown
    Tram = 1, // The vehicle is a tram
    Bus = 2, // The vehicle is a bus
    Subway = 3, // The vehicle is a subway
    TrainInterurban = 4, // The vehicle is an interurban train

    TrainRegional = 10, // The vehicle is a regional train
    TrainRegionalExpress = 11, // The vehicle is a region express
    TrainInterregio = 12, // The vehicle is an interregional train
    TrainIntercityEurocity = 13, // The vehicle is a intercity / eurocity train
    TrainIntercityExpress = 14 // The vehicle is an intercity express (ICE, TGV?, ...?)
};

// The type of services for a public transport line.
enum LineService {
    NoLineService = 0, // The public transport line has no special services

    NightLine = 1, // The public transport line is a night line
    ExpressLine = 2 // The public transport line is an express line
};

// The type of filtering by target / origin
enum FilterType {
    ShowAll = 0, // Show all targets / origins
    ShowMatching = 1, // Show only targets / origins that are in the list of filter targets / origins
    HideMatching = 2 // Hide targets / origins that are in the list of filter targets / origins
};

// The type of the delay of a departure / arrival
enum DelayType {
    DelayUnknown = 0, // No information about delay available
    OnSchedule = 1, // Vehicle will depart / arrive on schedule
    Delayed = 2 // Vehicle will depart / arrive with delay
};

#endif // ENUMS_HEADER