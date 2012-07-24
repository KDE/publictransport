/*
 *   Copyright 2012 Friedrich Pülz <fpuelz@gmx.de>
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
 * @brief This file contains classes for color groups used by the applet.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef COLORGROUPS_HEADER
#define COLORGROUPS_HEADER

// Own includes
#include "settings.h"

// libpublictransporthelper includes
#include <departureinfo.h>

/**
 * @brief This class contains a static method to generate color groups from departures.
 **/
class ColorGroups {
public:
    /**
     * @brief Generates a list of color group settings from the given departure @p infoList.
     *
     * The given departure @p infoList get grouped by direction. Each group gets a color assigned.
     **/
    static ColorGroupSettingsList generateColorGroupSettingsFrom(
            const QList< DepartureInfo >& infoList,
            DepartureArrivalListType departureArrivalListType );
};

#endif // Multiple inclusion guard
