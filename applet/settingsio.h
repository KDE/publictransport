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

/** @file
 * @brief This file contains classes for synchronizing settings with widgets in the UI.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef SETTINGSIO_HEADER
#define SETTINGSIO_HEADER

// Own includes
#include "settings.h"

// KDE includes
#include <KConfigGroup>

namespace Plasma {
    class DataEngine;
}

/**
 * @brief Contains static methods to read/write settings.
 *
 * Stop and filter settings are stored globally for all PublicTransport applets.
 **/
class SettingsIO {
public:
    /** @brief These flags describe what settings have changed. */
    enum ChangedFlag {
        NothingChanged          = 0x0000, /**< Nothing has changed. */
        IsChanged               = 0x0001, /**< This flag is set if something has changed.
                * If another change flag is set (except for NothingChanged), this flag is also
                * set. This flag also gets set for changes not covered by the other change flags. */
        ChangedServiceProvider  = 0x0002, /**< Service provider settings have been changed
                                            * (stop name, service provider, ...). */ // TODO
        ChangedDepartureArrivalListType
                                = 0x0004, /**< Changed from showing departures to arrivals
                                            * or vice versa. */
        ChangedStopSettings     = 0x0008, /**< Stop settings have been changed. This flag also gets
                * set if only eg. the favorite/recent journey searches have been changed.
                * Use ChangedCurrentStopSettings to check if timetable data needs to be requested
                * from the data engine again with the changed settings. */
        ChangedCurrentStopSettings
                                = 0x0010, /**< Stop settings of the current stop
                * have been changed, that require timetable data to be requested from the data
                * engine again. If this flag is set, the current timetable data may not represent
                * correct results any longer for the changed stop settings.
                * Stop settings that do not require a new timetable data request are unaffected. */
        ChangedCurrentJourneySearchLists
                                = 0x0020, /**< The list of favorite and/or recent journey
                                            * searches has been changed for the current stop. */
        ChangedCurrentStop      = 0x0040, /**< The current stop has been changed. */
        ChangedFilterSettings   = 0x0080, /**< Filter settings have been changed. */
        ChangedLinesPerRow      = 0x0100, /**< The lines per row setting has been changed. */
        ChangedAlarmSettings    = 0x0200, /**< Alarm settings have been changed. This does not
                                            * include AlarmTimeSetting in stop settings. */
        ChangedColorization     = 0x0400, /**< Colorization of departures has been toggled. */
        ChangedColorGroupSettings
                                = 0x0800  /**< Color group settings have been changed. */
    };
    Q_DECLARE_FLAGS( ChangedFlags, ChangedFlag );

    /** @brief Read settings from @p cg and @p cgGlobal. */
    static Settings readSettings( KConfigGroup cg, KConfigGroup cgGlobal,
                                  Plasma::DataEngine *publicTransportEngine = 0 );

    /**
     * @brief Write changed @p settings to @p cg and @p cgGlobal.
     *
     * @p oldSettings is used to see which settings have been changed.
     *
     * @returns What settings have been changed.
     * @see ChangedFlags */
    static ChangedFlags writeSettings( const Settings &settings, const Settings &oldSettings,
                                       KConfigGroup cg, KConfigGroup cgGlobal );

    /**
     * @brief Decodes journey search items from @p data.
     *
     * @param data Journey search items encoded using encodeJourneySearchItems.
     * @return The list of journey search items decoded from @p data.
     * @see encodeJourneySearchItems
     **/
    static QList<JourneySearchItem> decodeJourneySearchItems( QByteArray *data );

    /**
     * @brief Encodes @p journeySearches into a QByteArray.
     *
     * @param journeySearches Journey search items to encode.
     * @return @p journeySearches encoded in a QByteArray.
     * @see decodeJourneySearchItems
     **/
    static QByteArray encodeJourneySearchItems( const QList<JourneySearchItem> &journeySearches );

    /** @brief Read filter configuration from @p cgGlobal. */
    static FilterSettings readFilterConfig( const KConfigGroup &cgGlobal );

    /**
     * @brief Write filter configuration @p filterSettings to @p cgGlobal.
     *
     * This function only writes settings that have changed compared to @p oldFilterSettings.
     **/
    static bool writeFilterConfig( const FilterSettings &filterSettings,
                                   const FilterSettings &oldFilterSettings, KConfigGroup cgGlobal );

    /** @brief Write filter configuration @p filterSettings to @p cgGlobal. */
    static void writeFilterConfig( const FilterSettings &filterSettings, KConfigGroup cgGlobal );
};
Q_DECLARE_OPERATORS_FOR_FLAGS( SettingsIO::ChangedFlags );

#endif // Multiple inclusion guard
