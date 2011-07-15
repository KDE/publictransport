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

#ifndef TIMETABLE_GLOBAL_HEADER
#define TIMETABLE_GLOBAL_HEADER

/** @file
* @brief This file contains enumerations and Global used by the public transport helper library.
* 
* @author Friedrich Pülz <fpuelz@gmx.de> */

#include "publictransporthelper_export.h"
#include "enums.h"

#include <qnamespace.h>
#include <KIcon>
#include "kdeversion.h"
#include <QTime>
#include <QDebug>

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
    FilterByNextStop, /**< Filter by next intermediate stop.
            * (Like FilterByVia, but only for the first intermediate stop). */
    FilterByDeparture, /**< Filter by departure/arrival time. */
    FilterByDayOfWeek /**< Filter by the day of week of the departure date. */
};

/** @brief Variants of filters, eg. equals / doesn't equal.
 * @ingroup filterSystem */
enum FilterVariant {
    FilterNoVariant = 0, /**< Used for parameters, eg. as initial variant to use
            * the first available filter variant. */

    FilterContains = 1,
    FilterDoesntContain = 2,
    FilterEquals = 3,
    FilterDoesntEqual = 4,
    FilterMatchesRegExp = 5,
    FilterDoesntMatchRegExp = 6,

    FilterIsOneOf = 7,
    FilterIsntOneOf = 8,

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

/** @class Global
  * @brief Contains global static methods. */
class PUBLICTRANSPORTHELPER_EXPORT Global {
public:
	static GeneralVehicleType generalVehicleType( VehicleType vehicleType );
	
	/** Create an "international" icon with some flag icons */
	static KIcon internationalIcon();

	/** Gets an icon for the given type of vehicle. */
	static KIcon vehicleTypeToIcon( const VehicleType &vehicleType/*,
									const QString &overlayIcon = QString()*/ );

	/** Gets an icon containing the icons of all vehicle types in the given list. */
	static KIcon iconFromVehicleTypeList( const QList<VehicleType> &vehicleTypes, int extend = 32 );

	/** Gets the name of the given type of vehicle. */
	static QString vehicleTypeToString( const VehicleType &vehicleType, bool plural = false );

	/** Gets a string like "25 minutes". */
	static QString durationString( int seconds );

    static QColor textColorOnSchedule();
    static QColor textColorDelayed();
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

/** @mainpage
 * 
 * The <em>publictransporthelper</em> library can be used by Plasma applets / runners for 
 * configuration of stops to be used with the <em>publictransport</em> data engine.
 * To simply show a dialog to let the user edit @ref StopSettings the @ref StopSettingsDialog
 * can be used. That dialog can also be used to only select a service provider.
 * 
 * The @ref StopWidget shows an overview of a @ref StopSettings object and a button to change the
 * settings using a @ref StopSettingsDialog. @ref StopListWidget can be used to let the user 
 * edit more than one stop. It shows a button to add new stops and buttons beside the stops
 * to remove them.
 * 
 * If you only want to use a location-/service provider-combobox like the one used inside
 * @ref StopSettingsDialog, you can use @ref LocationModel / @ref ServiceProviderModel for your
 * comboboxes.
 * 
 * The library also offers a @ref VehicleType enumeration exactly like the one used inside the
 * publictransport data engine. This can be used to know what the vehicle type numbers returned
 * by the data engine are used for.
 * 
 * To use the library in your applet/runner/application you need to tell the linker about it.
 * Using CMake this can look like this:
 * @code
 * target_link_libraries( YourApp publictransporthelper )
 * @endcode
 * 
 * You can then include the headers, eg. for the @ref StopSettingsDialog
 * @code
 * #include <publictransporthelper/stopsettingsdialog.h>
 * @endcode
 **/

#endif // Multiple inclusion guard
