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
* This file contains enumerations and Global used by the public transport runner.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#include <qnamespace.h>
#include <QTime>
#include <KIcon>
#include "kdeversion.h"
#include <publictransporthelper/enums.h>

/** The type of the delay of a departure / arrival. */
enum DelayType {
	DelayUnknown = 0, /**< No information about delay available */
	OnSchedule = 1, /**< Vehicle will depart / arrive on schedule */
	Delayed = 2 /**< Vehicle will depart / arrive with delay */
};

/** @class Global
  * @brief Contains global static methods. */
class Global {
public:
	static QColor textColorOnSchedule();
	static QColor textColorDelayed();

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
