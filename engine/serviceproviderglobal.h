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

class KConfig;
class KConfigGroup;

/** @brief Provides static functions for service providers. */
class ServiceProviderGlobal {
public:
    /** @brief Options for the provider type name. */
    enum ProviderTypeNameOptions {
        ProviderTypeNameWithoutUnsupportedHint, /**< Only use the provider type name. */
        AppendHintForUnsupportedProviderTypes /**< Append a hint to the provider type name
                * if the engine was build without support for that provider type. */
    };

    /**
     * @brief Get a list of available service provider types.
     * The engine can be built without support for some provider types.
     * @note InvalidProvider is never contained in the list.
     * @see isProviderTypeAvailable()
     **/
    static QList< Enums::ServiceProviderType > availableProviderTypes();

    /**
     * @brief Whether or not the given provider @p type is available.
     * The engine can be built without support for some provider types.
     * @note If @p type is InvalidProvider this always returns false.
     * @see availableProviderTypes()
     **/
    static bool isProviderTypeAvailable( Enums::ServiceProviderType type );

    /** @brief Get the ServiceProviderType enumerable for the given string. */
    static Enums::ServiceProviderType typeFromString( const QString &serviceProviderType );

    /** @brief Get a string for the given @p type, not to be displayed to users, all lower case. */
    static QString typeToString( Enums::ServiceProviderType type );

    /** @brief Get the name for the given @p type, translated, to be displayed to users. */
    static QString typeName( Enums::ServiceProviderType type,
                             ProviderTypeNameOptions options = AppendHintForUnsupportedProviderTypes );

    /** @brief Get a localized name for @p feature. */
    static QString featureName( Enums::ProviderFeature feature );

    /** @brief Get a list of localized names for @p features. */
    static QStringList featureNames( const QList<Enums::ProviderFeature> &features );

    /** @brief Get a list of strings for @p features, using Enums::toString(). */
    static QStringList featureStrings( const QList<Enums::ProviderFeature> &features );

    /** @brief Get a list of provider feature enumerables from a list of feature strings. */
    static QList<Enums::ProviderFeature> featuresFromFeatureStrings(
            const QStringList &featureNames, bool *ok = 0 );

    /** @brief Get the service provider ID for the given service provider plugin file name. */
    static QString idFromFileName( const QString &serviceProviderPluginFileName );

    /** @brief Get the service provider plugin file name for the given service provider ID. */
    static QString fileNameFromId( const QString &serviceProviderId );

    /** @brief Get the file path of the default service provider XML for the given @p location. */
    static QString defaultProviderForLocation( const QString &location,
                                               const QStringList &dirs = QStringList() );

    /**
     * @brief Whether or not the provider source file (.pts) was modified.
     *
     * @param providerId The ID of the provider to test for modifications.
     * @param cache A shared pointer to the provider cache, see cache().
     * @return @c True, if the provider source file was modified since the last provider test
     *   for @p providerId, @c false otherwise.
     **/
    static bool isSourceFileModified( const QString &providerId,
                                      const QSharedPointer<KConfig> &cache );

    /**
     * @brief Get a shared pointer to the cache object for provider plugin information.
     *
     * The cache can be used by provider plugins to store information about themselves that
     * might take some time to get if not stored. For example a network request might be needed to
     * get the information.
     * KConfig gets used to read from / write to the cache file.
     *
     * @note Each provider should only write to it's group, the name of that group is the provider
     *   ID. Classes derived from ServiceProvider should write their own data to a subgroup.
     * @see cacheFileName()
     **/
    static QSharedPointer<KConfig> cache();

    /**
     * @brief Get the name of the cache file.
     * @see cache()
     **/
    static QString cacheFileName();

    /** @brief Cleanup the cache from old entries for no longer installed providers. */
    static void cleanupCache( const QSharedPointer<KConfig> &cache = QSharedPointer<KConfig>() );

    /**
     * @brief Clear all values for the provider with the given @p providerId from the @p cache.
     *
     * Should be called when a provider was uninstalled.
     **/
    static void clearCache( const QString &providerId,
                            const QSharedPointer<KConfig> &cache = QSharedPointer<KConfig>(),
                            bool syncCache = true );

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

    /**
     * @brief Get the file paths of all installed service provider plugins.
     * If invalid provider plugins are installed, they also get returned here.
     **/
    static QStringList installedProviders();

    /**
     * @brief Wether or not the provider with the given @p providerId is installed.
     **/
    static bool isProviderInstalled( const QString &providerId );
};

#endif // Multiple inclusion guard
