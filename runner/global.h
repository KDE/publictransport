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

#ifndef GLOBAL_HEADER
#define GLOBAL_HEADER

/** @file
* This file contains enumerations and Global used by the public transport applet.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#include <qnamespace.h>
#include <QTime>
#include <KIcon>
#include "kdeversion.h"

/** Different config modes for the time of the first departure. */
enum FirstDepartureConfigMode {
	RelativeToCurrentTime = 0, /**< Uses the current date and time and adds an offset. */
	AtCustomTime = 1 /**< Uses a custom time, but the current date. */
};

/** The type of the vehicle used for a public transport line.
* The numbers here must match the ones in the data engine! */
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

	Spacecraft = 300, /**< A spacecraft. */
};

/** The type of the delay of a departure / arrival. */
enum DelayType {
	DelayUnknown = 0, /**< No information about delay available */
	OnSchedule = 1, /**< Vehicle will depart / arrive on schedule */
	Delayed = 2 /**< Vehicle will depart / arrive with delay */
};

/** The position of the decoration. */
enum DecorationPosition {
	DecorationLeft, /**< Show the decoration on the left side. */
	DecorationRight /**< Show the decoration on the right side. */
};

/** Indicates what is saved in a model item's data. */
enum ModelDataRoles {
	SortRole = Qt::UserRole, /**< Used to store sorting data */
	ServiceProviderDataRole = Qt::UserRole + 8, /**< For the service provider combo box */
	DepartureInfoRole = Qt::UserRole + 10, /**< Used to store the departure */
	LocationCodeRole = Qt::UserRole + 12, /**< Used to store the location code (country code or other) in the location model. */
	TimetableItemHashRole = Qt::UserRole + 13, /**< Used to store a hash for the current timetable item in the model. */
	ServiceProviderIdRole = Qt::UserRole + 14, /** Used to store the service provider ID. */

	FormattedTextRole = Qt::UserRole + 500, /**< Used to store formatted text.
	* The text of an item should not contain html tags, if used in a combo box. */
	DecorationPositionRole = Qt::UserRole + 501,
	DrawAlarmBackgroundRole = Qt::UserRole + 502,
	AlarmColorIntensityRole = Qt::UserRole + 503,
	JourneyRatingRole = Qt::UserRole + 504, /**< Stores a value between 0 and 1.
	* 0 for the journey with the biggest duration, 1 for the smallest duration. */
	LinesPerRowRole = Qt::UserRole + 505, /**< Used to change the number of lines for a row. */
	IconSizeRole = Qt::UserRole + 506 /**< Used to set a specific icon size for an element. */
};

/** @class Global
  * @brief Contains global static methods. */
class Global {
public:
	static QColor textColorOnSchedule();
	static QColor textColorDelayed();

	static KIcon putIconIntoBiggerSizeIcon( const KIcon &icon,
			const QSize &iconSize, const QSize &resultingSize = QSize(32, 32) );

	/** Create an "international" icon with some flag icons */
	static KIcon internationalIcon();

	/** Creates an icon that has another icon as overlay on the bottom right. */
	static KIcon makeOverlayIcon( const KIcon &icon, const KIcon &overlayIcon,
			const QSize &overlaySize = QSize(10, 10),
			int iconExtend = 16 );

	/** Creates an icon that has another icon as overlay on the bottom right. */
	static KIcon makeOverlayIcon( const KIcon &icon, const QString &overlayIconName,
			const QSize &overlaySize = QSize(10, 10),
			int iconExtend = 16 );

	/** Creates an icon that has other icons as overlay on the bottom. */
	static KIcon makeOverlayIcon( const KIcon &icon, const QList<KIcon> &overlayIconsBottom, const QSize &overlaySize = QSize(10, 10), int iconExtend = 16 );

	/** Gets an icon for the given type of vehicle. */
	static KIcon vehicleTypeToIcon( const VehicleType &vehicleType,
			const QString &overlayIcon = QString() );

	/** Gets an icon containing the icons of all vehicle types in the given list. */
	static KIcon iconFromVehicleTypeList( const QList<VehicleType> &vehicleTypes, int extend = 32 );

	/** Gets a string like "25 minutes". */
	static QString durationString( int seconds );
};

#endif // Multiple inclusion guard
