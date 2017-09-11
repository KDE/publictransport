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

// Header
#include "serviceproviderglobal.h"

// Own includes
#include "config.h"

// KDE includes
#include <KDebug>
#include <KGlobal>

#include <KStandardDirs>
#include <KLocalizedString>

// Qt includes
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QMimeDatabase>
#include <QMimeType>

QList< Enums::ServiceProviderType > ServiceProviderGlobal::availableProviderTypes()
{
    QList< Enums::ServiceProviderType > types;
    types << Enums::GtfsProvider;
    return types;
}

bool ServiceProviderGlobal::isProviderTypeAvailable( Enums::ServiceProviderType type )
{
    switch ( type ) {
    case Enums::GtfsProvider:
#ifdef BUILD_PROVIDER_TYPE_GTFS
        return true;
#else
        return false;
#endif

    case Enums::InvalidProvider:
    default:
        return false;
    }
}

QString ServiceProviderGlobal::defaultProviderForLocation( const QString &location )
{
    QStringList locationProviders;
    const QString subDirectory = installationSubDirectory();
    foreach ( const QString &pattern, filePatterns() ) {
        locationProviders << KGlobal::dirs()->findAllResources(
                "data", subDirectory + location + '_' + pattern );
    }
    if ( locationProviders.isEmpty() ) {
        qWarning() << "Couldn't find any providers for location" << location;
        return QString();
    } else {
        qDebug() << "Following service providers were found ...";
        foreach (QString location, locationProviders) {
            qDebug() << location;
        }
    }

    // Simply return first found provider as default provider
    qDebug() << "Default Provider = " << locationProviders.first();
    return locationProviders.first();
}

QString ServiceProviderGlobal::cacheFileName()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Char('/')
            + "plasma_engine_publictransport/" + QLatin1String("datacache");
}

QSharedPointer< KConfig > ServiceProviderGlobal::cache()
{
    return QSharedPointer< KConfig >( new KConfig(cacheFileName(), KConfig::SimpleConfig) );
}

void ServiceProviderGlobal::cleanupCache( const QSharedPointer< KConfig > &_cache )
{
    QSharedPointer< KConfig > cache = _cache.isNull() ? ServiceProviderGlobal::cache() : _cache;
    const QStringList installedProviderPaths = ServiceProviderGlobal::installedProviders();
    QStringList installedProviderIDs;
    foreach ( const QString &installedProviderPath, installedProviderPaths ) {
        installedProviderIDs << idFromFileName( installedProviderPath );
    }

    foreach ( const QString &group, cache->groupList() ) {
        if ( group != QLatin1String("script") && group != QLatin1String("gtfs") &&
             !installedProviderIDs.contains(group) )
        {
            // Found a group for a provider that is no longer installed
            kDebug() << "Cleanup cache data for no longer installed provider" << group;
            clearCache( group, cache, false );
        }
    }
    cache->sync();
}

void ServiceProviderGlobal::clearCache( const QString &providerId,
                                        const QSharedPointer< KConfig > &_cache, bool syncCache )
{
    QSharedPointer< KConfig > cache = _cache.isNull() ? ServiceProviderGlobal::cache() : _cache;
    if ( !cache->hasGroup(providerId) ) {
        // No data cached for the provider
        return;
    }

    // Remove all data for the provider from the cache
    cache->deleteGroup( providerId );

    // Remove provider from "usingProviders" lists for included script files
    KConfigGroup globalScriptGroup = cache->group( "script" );
    const QStringList globalScriptGroupNames = globalScriptGroup.groupList();
    foreach ( const QString &globalScriptGroupName, globalScriptGroupNames ) {
        if ( !globalScriptGroupName.startsWith(QLatin1String("include_")) ) {
            continue;
        }

        // Check if the provider to remove from the cache is listed as using the current include
        KConfigGroup includeFileGroup = globalScriptGroup.group( globalScriptGroupName );
        QStringList usingProviders = includeFileGroup.readEntry( "usingProviders", QStringList() );
        if ( usingProviders.contains(providerId) ) {
            // Remove provider from the list of providers using the current include script file
            usingProviders.removeOne( providerId );
            includeFileGroup.writeEntry( "usingProviders", usingProviders );
        }
    }

    if ( syncCache ) {
        cache->sync();
    }
}

QString ServiceProviderGlobal::idFromFileName( const QString &serviceProviderFileName )
{
    // Get the service provider substring from the XML filename, ie. "/path/to/xml/<id>.pts/xml"
    return QFileInfo( serviceProviderFileName ).baseName();
}

QString ServiceProviderGlobal::fileNameFromId( const QString &serviceProviderId )
{
    const QString subDirectory = installationSubDirectory();
    foreach ( const QString &extension, fileExtensions() ) {
        const QString fileName = QStandardPaths::locate(
                QStandardPaths::GenericDataLocation, subDirectory + serviceProviderId + '.' + extension );
        if ( !fileName.isEmpty() ) {
            return fileName;
        }
    }

    // File not found
    kDebug() << "No service provider plugin found with this ID:" << serviceProviderId;
    return QString();
}

Enums::ServiceProviderType ServiceProviderGlobal::typeFromString(
        const QString &serviceProviderType )
{
    QString s = serviceProviderType.toLower();
    if ( s == QLatin1String("script") ||    // DEPRECATED
         s == QLatin1String("html") )       // DEPRECATED
    {
        return Enums::InvalidProvider;
    } else if ( s == QLatin1String("gtfs") ) {
        return Enums::GtfsProvider;
    } else {
        return Enums::InvalidProvider;
    }
}

QString ServiceProviderGlobal::typeToString( Enums::ServiceProviderType type )
{
    switch ( type ) {
        case Enums::GtfsProvider:
            return "gtfs";
        case Enums::InvalidProvider:
        default:
            return "invalid";
    }
}

QString ServiceProviderGlobal::typeName( Enums::ServiceProviderType type, ProviderTypeNameOptions options )
{
    QString name;
    switch ( type ) {
        case Enums::GtfsProvider:
            name = i18nc("@info/plain Name of a service provider plugin type", "GTFS");
            break;
        case Enums::InvalidProvider:
        default:
            qWarning() << "Invalid provider type" << type;
            return i18nc("@info/plain Name of the invalid service provider plugin type", "Invalid");
    }

    // Append "(unsupported)" if the engine gets build without support for the provider type
    if ( options == AppendHintForUnsupportedProviderTypes && !isProviderTypeAvailable(type) ) {
        name += ' ' + i18nc("@info/plain Gets appended to service provider plugin type names, "
                            "if the engine gets build without support for that type",
                            "(unsupported)");
    }
    return name;
}

QStringList ServiceProviderGlobal::filePatterns()
{
    QMimeDatabase db;
    const QMimeType mimeType = db.mimeTypeForName("application/x-publictransport-serviceprovider");
    if (!mimeType.isValid() ) {
        qWarning() << "The application/x-publictransport-serviceprovider mime type was not found!";
        qWarning() << "No provider plugins will get loaded.";
        kDebug() << "Solution: Make sure 'serviceproviderplugin.xml' is installed correctly "
                    "and run kbuildsycoca4.";
        return QStringList();
    } else {
        return mimeType.globPatterns();
    }
}

QStringList ServiceProviderGlobal::fileExtensions()
{
    QStringList extensions = filePatterns();
    for ( QStringList::Iterator it = extensions.begin(); it != extensions.end(); ++it ) {
        const int pos = it->lastIndexOf( '.' );
        if ( pos == -1 || pos == it->length() - 1 ) {
            qWarning() << "Could not extract file extension from mime type pattern!\n"
                          "Check the \"application/x-publictransport-serviceprovider\" mime type.";
            continue;
        }

        // Cut away everything but the file name extension
        *it = it->mid( pos + 1 );
    }
    return extensions;
}

QStringList ServiceProviderGlobal::installedProviders()
{
    QStringList providers;
    const QString subDirectory = installationSubDirectory();
    foreach ( const QString &pattern, filePatterns() ) {
        providers << KGlobal::dirs()->findAllResources( "data", subDirectory + pattern );
    }
    return providers;
}

bool ServiceProviderGlobal::isProviderInstalled( const QString &providerId )
{
    const QString subDirectory = installationSubDirectory();
    foreach ( const QString &extension, fileExtensions() ) {
        const QString providerFile = QStandardPaths::locate(
                QStandardPaths::GenericDataLocation, subDirectory + providerId + '.' + extension );
        if ( !providerFile.isEmpty() ) {
            // Found the provider plugin source file in an installation directory
            return true;
        }
    }

    // Provider is not installed
    return false;
}

bool ServiceProviderGlobal::isSourceFileModified( const QString &providerId,
                                                  const QSharedPointer<KConfig> &cache )
{
    // Check if the script file was modified since the cache was last updated
    const KConfigGroup group = cache->group( providerId );
    const QDateTime modifiedTime = group.readEntry( "modifiedTime", QDateTime() );
    const QString fileName = fileNameFromId( providerId );
    return fileName.isEmpty() ? true : QFileInfo(fileName).lastModified() != modifiedTime;
}

QString ServiceProviderGlobal::featureName( Enums::ProviderFeature feature )
{
    switch ( feature ) {
    case Enums::ProvidesDepartures:
        return i18nc("@info/plain A short string indicating support for departure from a stop",
                     "Departures");
    case Enums::ProvidesArrivals:
        return i18nc("@info/plain A short string indicating support for arrivals to a stop.",
                     "Arrivals");
    case Enums::ProvidesJourneys:
        return i18nc("@info/plain A short string indicating support for journeys",
                     "Journey search");
    case Enums::ProvidesAdditionalData:
        return i18nc("@info/plain A short string indicating support for additional data",
                     "Get additional data later");
    case Enums::ProvidesDelays:
        return i18nc("@info/plain A short string indicating that delay information can be provided",
                     "Delays");
    case Enums::ProvidesNews:
        return i18nc("@info/plain A short string indicating that news about timetable items can be provided",
                     "News");
    case Enums::ProvidesPlatform:
        return i18nc("@info/plain A short string indicating that platform information can be provided",
                     "Platform");
    case Enums::ProvidesStopSuggestions:
        return i18nc("@info/plain A short string indicating support for stop suggestions",
                     "Stop suggestions");
    case Enums::ProvidesStopsByGeoPosition:
        return i18nc("@info/plain A short string indicating support for querying stops by geo position",
                     "Stops by geolocation");
    case Enums::ProvidesStopID:
        return i18nc("@info/plain A short string indicating that stop IDs can be provided",
                     "Stop ID");
    case Enums::ProvidesStopGeoPosition:
        return i18nc("@info/plain A short string indicating that stop geographical positions can be provided",
                     "Stop geolocation");
    case Enums::ProvidesPricing:
        return i18nc("@info/plain A short string indicating that pricing information can be provided",
                     "Pricing");
    case Enums::ProvidesRouteInformation:
        return i18nc("@info/plain A short string indicating that route information can be provided",
                     "Route information");
    case Enums::ProvidesMoreJourneys:
        return i18nc("@info/plain A short string indicating that earlier later journeys can be "
                     "provided for existing journey data sources",
                     "Get earlier/later journeys");
    default:
        qWarning() << "Unexpected feature value" << feature;
        return QString();
    }
}

QStringList ServiceProviderGlobal::featureNames( const QList<Enums::ProviderFeature> &features )
{
    QStringList names;
    foreach ( Enums::ProviderFeature feature, features ) {
        names << featureName( feature );
    }
    return names;
}

QStringList ServiceProviderGlobal::featureStrings( const QList<Enums::ProviderFeature> &features )
{
    QStringList names;
    foreach ( Enums::ProviderFeature feature, features ) {
        names << Enums::toString( feature );
    }
    return names;
}

QList< Enums::ProviderFeature > ServiceProviderGlobal::featuresFromFeatureStrings(
        const QStringList &featureNames, bool *ok )
{
    QList< Enums::ProviderFeature > features;
    if ( ok ) *ok = true;
    foreach ( const QString &featureName, featureNames ) {
        Enums::ProviderFeature feature = Enums::stringToFeature( featureName.toAscii().data() );
        if ( feature == Enums::InvalidProviderFeature ) {
            if ( ok ) *ok = false;
        } else {
            features << feature;
        }
    }
    return features;
}
