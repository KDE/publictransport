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

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

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
	
	static QString translateFilterKey( const QString &key );
	static QString untranslateFilterKey( const QString &translatedKey );
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
