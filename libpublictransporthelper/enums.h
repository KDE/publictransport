/*
 *   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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

/** @file
 * @brief This file contains enumerations used by the public transport helper library.
 *
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#include "publictransporthelper_export.h"
#include <QDebug>
#include <qnamespace.h>

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

/** @brief The type of services for a public transport line. */
enum LineService {
    NoLineService = 0, /**< The public transport line has no special services */

    NightLine = 1, /**< The public transport line is a night line */
    ExpressLine = 2 /**< The public transport line is an express line */
};

/** @brief The type of the delay of a departure / arrival. */
enum DelayType {
    DelayUnknown = 0, /**< No information about delay available */
    OnSchedule = 1, /**< Vehicle will depart / arrive on schedule */
    Delayed = 2 /**< Vehicle will depart / arrive with delay */
};

/** @brief Types of filters, ie. what to filter.
 * @ingroup filterSystem */
enum FilterType {
    InvalidFilter = 0, /**< An invalid filter. */

    FilterByVehicleType, /**< Filter by vehicle type. */
    FilterByTransportLine, /**< Filter by transport line string. */
    FilterByTransportLineNumber, /**< Filter by transport line number. */
    FilterByTarget, /**< Filter by target/origin. */
    FilterByDelay, /**< Filter by delay. */
    FilterByVia, /**< Filter by intermediate stops.
            * (Like FilterByNextStop, but for all intermediate stops, not only the first). */
    FilterByNextStop, /**< Filter by next intermediate stop after the home stop (previous before
            * the home stop for arrivals). Like FilterByVia, but only for the second (last)
            * intermediate stop. */
    FilterByDeparture, /**< Filter by departure/arrival time. */
    FilterByDayOfWeek /**< Filter by the day of week of the departure date.
            * Values are expected to be of type QVariantList with integers like the ones returned
            * by QDate::dayOfWeek(). Can be used with FilterIsOneOf and FilterIsNotOneOf. */
};

/** @brief Variants of filters, eg. equals / doesn't equal.
 * @ingroup filterSystem */
enum FilterVariant {
    FilterNoVariant = 0, /**< Used for parameters, eg. as initial variant to use
            * the first available filter variant. */

    FilterContains = 1,
    FilterDoesNotContain = 2,
    FilterDoesntContain = FilterDoesNotContain, // DEPRECATED
    FilterEquals = 3,
    FilterDoesNotEqual = 4,
    FilterDoesntEqual = FilterDoesNotEqual, // DEPRECATED
    FilterMatchesRegExp = 5,
    FilterDoesNotMatchRegExp = 6,
    FilterDoesntMatchRegExp = FilterDoesNotMatchRegExp, // DEPRECATED

    FilterIsOneOf = 7,
    FilterIsNotOneOf = 8,
    FilterIsntOneOf = FilterIsNotOneOf, // DEPRECATED

    FilterGreaterThan = 9,
    FilterLessThan = 10
};

/** @brief The action to be executed for filters, ie. show or hide matching items.
 * @ingroup filterSystem */
enum FilterAction {
    // ShowAll = 0, /**< Show all targets / origins */ TODO Remove this, filters are globally enabled/disabled
    ShowMatching = 0, /**< Show only targets / origins that are in the list of filter targets / origins */
    HideMatching = 1 /**< Hide targets / origins that are in the list of filter targets / origins */
};

/**
 * @brief Contains keys for different stop settings.
 *
 * Indices beginning at UserSetting may be used to store custom data.
 *
 * For each enumerable a data type is defined, which can be used for example in
 * @ref StopSettings::get. If you want to get the value of eg. <em>TimeOfFirstDepartureSetting</em>
 * you can write something like this:
 * @code
 * StopSettings stopSettings;
 * stopSettings.set( TimeOfFirstDepartureSetting, QTime(12, 0) );
 * QTime time = stopSettings.get<QTime>( TimeOfFirstDepartureSetting );
 * QTime time2 = stopSettings[ TimeOfFirstDepartureSetting ].toTime();
 * @endcode
 * The last two lines do the same thing and <em>time</em> and <em>time2</em> are equal. You can
 * use the function you prefer to get the value. The data type <em>QTime</em> can be retrieved
 * from the documentation for the enumerable <em>TimeOfFirstDepartureSetting</em> (At the end
 * in parantheses).
 **/
enum PUBLICTRANSPORTHELPER_EXPORT StopSetting {
    NoSetting = 0, /**< Don't use any setting. */

    LocationSetting = 1, /**< The location of the stop, eg. a country (QString). */
    ServiceProviderSetting = 2, /**< The ID of the service provider of the stop (QString). */
    CitySetting = 3, /**< The city of the stop, if used by the service provider (QString). */
    StopNameSetting = 4, /**< The stop name (Stop). */

    FilterConfigurationSetting = 10, /**< The filter configuration to be used for the stop (QString). */
    AlarmTimeSetting = 11, /** The time in minutes before the departure
            * at which the alarm should be fired (int). */
    FirstDepartureConfigModeSetting = 12, /**< The config mode for the time
            * of the first departure (enum FirstDepartureConfigMode, int). */
    TimeOffsetOfFirstDepartureSetting = 13, /**< The offset in minutes from the current time
            * until the first departure (int). */
    TimeOfFirstDepartureSetting = 14, /**< A custom time for the first departure (QTime). */

    UserSetting = 100 /**< The first index to be used for custom data (QVariant). Widgets
            * to edit custom settings can be created for the @ref StopSettingsDialog
            * using a @ref StopSettingsWidgetFactory. */
};

/** @brief Different config modes for the time of the first departure. */
enum PUBLICTRANSPORTHELPER_EXPORT FirstDepartureConfigMode {
    RelativeToCurrentTime = 0, /**< Uses the current date and time and adds an offset. */
    AtCustomTime = 1 /**< Uses a custom time, but the current date. */
};

/** @brief Indicates what is saved in a model item's data. */
enum PUBLICTRANSPORTHELPER_EXPORT ModelDataRoles {
    SortRole = Qt::UserRole, /**< Used to store sorting data */
    ServiceProviderDataRole = Qt::UserRole + 8, /**< For the service provider combo box */
    DepartureInfoRole = Qt::UserRole + 10, /**< Used to store the departure */
    LocationCodeRole = Qt::UserRole + 12, /**< Used to store the location code
            * (country code or other) in the location model. */
    TimetableItemHashRole = Qt::UserRole + 13, /**< Used to store a hash for the current
            * timetable item in the model. */
    ServiceProviderIdRole = Qt::UserRole + 14, /**< Used to store the service provider ID. */
    FilterSettingsRole = Qt::UserRole + 15, /**< Used to store FilterSettings objects. */

    // Additional data roles used by DepartureModel / JourneyModel.
    FormattedTextRole = Qt::UserRole + 500, /**< Used to store formatted text.
            * The text of an item should not contain html tags, if used in a combo box. */
    DecorationPositionRole = Qt::UserRole + 501,
    DrawAlarmBackgroundRole = Qt::UserRole + 502,
    AlarmColorIntensityRole = Qt::UserRole + 503,
    JourneyRatingRole = Qt::UserRole + 504, /**< Stores a value between 0 and 1.
            * 0 for the journey with the biggest duration, 1 for the smallest duration. */
    LinesPerRowRole = Qt::UserRole + 505, /**< Used to change the number of lines for a row. */
    IconSizeRole = Qt::UserRole + 506, /**< Used to set a specific icon size for an element. */
    IsLeavingSoonRole = Qt::UserRole + 507, /**< Whether or not a departure/arrival is leaving soon. */
    GroupColorRole = Qt::UserRole + 508 /**< Departures can be grouped, visualized by colors.
            * These group colors can be stored in this role. */
};

/** @brief The position of the decoration. */
enum PUBLICTRANSPORTHELPER_EXPORT DecorationPosition {
    DecorationLeft, /**< Show the decoration on the left side. */
    DecorationRight /**< Show the decoration on the right side. */
};

/**
 * @brief A more general vehicle type than @ref VehicleType.
 *
 * For example @ref VehicleType has special enumerables for different types of trains,
 * where @ref GerenalVehicleType combines them into one enumerable.
 */
enum PUBLICTRANSPORTHELPER_EXPORT GeneralVehicleType {
    UnknownVehicle = 0,
    LocalPublicTransport = 1,
    Train = 2,
    WaterVehicle = 3,
    AirVehicle = 4
};

/**
 * @brief The type of the vehicle used for a public transport line.
 *
 * The numbers here match the ones used by the data engine.
 */
enum PUBLICTRANSPORTHELPER_EXPORT VehicleType {
    Unknown = 0, /**< The type of the vehicle is unknown. */

    Tram = 1, /**< A tram / streetcar. */
    Bus = 2, /**< A bus. */
    Subway = 3, /**< A subway. */
    InterurbanTrain = 4, /**< An interurban train. */
    Metro = 5, /**< A metro. */
    TrolleyBus = 6, /**< A trolleybus (also known as trolley bus, trolley coach,
            * trackless trolley, trackless tram or trolley) is an electric bus that draws its
            * electricity from overhead wires (generally suspended from roadside posts)
            * using spring-loaded trolley poles. */

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

    Spacecraft = 300, /**< A spacecraft. */

    TrainInterurban = InterurbanTrain, /**< An interurban train. @deprecated Use InterurbanTrain instead. */
    TrainRegional = RegionalTrain, /**< A regional train. @deprecated Use RegionalTrain instead. */
    TrainRegionalExpress = RegionalExpressTrain, /**< A regional express train. @deprecated Use RegionalExpressTrain instead. */
    TrainInterregio = InterregionalTrain, /**< An inter-regional train. @deprecated Use InterregionalTrain instead. */
    TrainIntercityEurocity = IntercityTrain, /**< An intercity / eurocity train. @deprecated Use IntercityTrain instead. */
    TrainIntercityExpress = HighSpeedTrain /**< An intercity express. @deprecated Use HighSpeedTrain instead. */
};

inline QDebug& operator<<( QDebug debug, FilterType filterType )
{
    switch ( filterType ) {
        case InvalidFilter:
            return debug << "InvalidFilter";
        case FilterByVehicleType:
            return debug << "FilterByVehicleType";
        case FilterByTransportLine:
            return debug << "FilterByTransportLine";
        case FilterByTransportLineNumber:
            return debug << "FilterByTransportLineNumber";
        case FilterByTarget:
            return debug << "FilterByTarget";
        case FilterByDelay:
            return debug << "FilterByDelay";
        case FilterByVia:
            return debug << "FilterByVia";
        case FilterByNextStop:
            return debug << "FilterByNextStop";
        case FilterByDeparture:
            return debug << "FilterByDeparture";
        case FilterByDayOfWeek:
            return debug << "FilterByDayOfWeek";
        default:
            return debug << "Unknown filter type: " << filterType;
    }
};

inline QDebug& operator<<( QDebug debug, FilterVariant filterVariant )
{
    switch ( filterVariant ) {
        case FilterNoVariant:
            return debug << "FilterNoVariant";
        case FilterContains:
            return debug << "FilterContains";
        case FilterDoesntContain:
            return debug << "FilterDoesntContain";
        case FilterEquals:
            return debug << "FilterEquals";
        case FilterDoesntEqual:
            return debug << "FilterDoesntEqual";
        case FilterMatchesRegExp:
            return debug << "FilterMatchesRegExp";
        case FilterDoesntMatchRegExp:
            return debug << "FilterDoesntMatchRegExp";
        case FilterIsOneOf:
            return debug << "FilterIsOneOf";
        case FilterIsntOneOf:
            return debug << "FilterIsntOneOf";
        case FilterGreaterThan:
            return debug << "FilterGreaterThan";
        case FilterLessThan:
            return debug << "FilterLessThan";
        default:
            return debug << "Unknown filter variant: " << filterVariant;
    }
};

}; // namespace Timetable

#endif // Multiple inclusion guard
