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

#ifndef SERVICEPROVIDERGLOBAL_HEADER
#define SERVICEPROVIDERGLOBAL_HEADER

// Own includes
#include "enums.h"

// Qt includes
#include <QStringList>

class KConfigGroup;

/** @brief Provides global data/functions for service providers. */
class ServiceProviderGlobal {
public:
    /** @brief Get the ServiceProviderType enumerable for the given string. */
    static ServiceProviderType typeFromString( const QString &serviceProviderType );

    static QString typeToString( ServiceProviderType type );

    /** @brief Get the service provider ID for the given service provider plugin file name. */
    static QString idFromFileName( const QString &serviceProviderPluginFileName );

    /** @brief Get the service provider plugin file name for the given service provider ID. */
    static QString fileNameFromId( const QString &serviceProviderId );

    static bool isSourceFileModified( const QString &providerId,
                                      const QString &fileName = QString() );

    /** @brief Get the file path of the default service provider XML for the given @p location. */
    static QString defaultProviderForLocation( const QString &location,
                                               const QStringList &dirs = QStringList() );

    /**
     * @brief Get the name of the cache file for provider plugin information.
     *
     * The cache file can be used by provider plugins to store information about themselves that
     * might take some time to get if not stored. For example a network request might be needed to
     * get the information.
     * KConfig gets used to read from / write to the cache file.
     *
     * @note Each provider should only write to it's group, the name of that group is the provider
     *   ID. Classes derived from ServiceProvider should write their own data to a subgroup.
     **/
    static QString cacheFileName();

    /** @brief Get the sub directory for service provider plugins for the data engine. */
    static QString installationSubDirectory() {
            return "plasma_engine_publictransport/serviceProviders/"; };

    /**
     * @brief Get all patterns for service provider plugin files for the data engine.
     * The patterns are retrieved from the "application-x-publictransport-serviceprovider" mime type.
     **/
    static QStringList filePatterns();

    /**
     * @brief Get all extensions for service provider plugin files for the data engine.
     * The file name extensions are retrieved from the "application-x-publictransport-serviceprovider"
     * mime type file patterns.
     **/
    static QStringList fileExtensions();

    /** @brief Get the file names of all installed service providers. */
    static QStringList installedProviders();
};

#endif // Multiple inclusion guard
