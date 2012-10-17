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
#include <KGlobal>
#include <KStandardDirs>
#include <KDebug>
#include <KMimeType>
#include <KConfigGroup>
#include <KLocalizedString>

// Qt includes
#include <QFile>
#include <QFileInfo>

QList< Enums::ServiceProviderType > ServiceProviderGlobal::availableProviderTypes()
{
    QList< Enums::ServiceProviderType > types;
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    types << Enums::ScriptedProvider;
#endif
#ifdef BUILD_PROVIDER_TYPE_GTFS
    types << Enums::GtfsProvider;
#endif
    return types;
}

bool ServiceProviderGlobal::isProviderTypeAvailable( Enums::ServiceProviderType type )
{
    switch ( type ) {
    case Enums::ScriptedProvider:
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        return true;
#else
        return false;
#endif

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

QString ServiceProviderGlobal::defaultProviderForLocation( const QString &location,
                                                           const QStringList &dirs )
{
    // Get the filename of the default service provider plugin for the given location
    const QStringList _dirs = !dirs.isEmpty() ? dirs
            : KGlobal::dirs()->findDirs( "data", installationSubDirectory() );
    QString filePath = QString( "%1_default" ).arg( location );
    foreach( const QString &dir, _dirs ) {
        foreach ( const QString &extension, fileExtensions() ) {
            if ( QFile::exists(dir + filePath + '.' + extension) ) {
                filePath = dir + filePath + '.' + extension;
                break;
            }
        }
    }

    // Get the real filename the "xx_default.pts/xml"-symlink links to
    filePath = QFileInfo( filePath ).canonicalFilePath();
    if ( filePath.isEmpty() ) {
        kDebug() << "Couldn't find the default service provider for location" << location;
    }
    return filePath;
}

QString ServiceProviderGlobal::cacheFileName()
{
    return KGlobal::dirs()->saveLocation("data", "plasma_engine_publictransport/")
            .append( QLatin1String("datacache") );
}

QSharedPointer< KConfig > ServiceProviderGlobal::cache()
{
    return QSharedPointer< KConfig >( new KConfig(cacheFileName(), KConfig::SimpleConfig) );
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
        const QString fileName = KGlobal::dirs()->findResource(
                "data", subDirectory + serviceProviderId + '.' + extension );
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
    if ( s == QLatin1String("script") ||
         s == QLatin1String("html") ) // DEPRECATED
    {
        return Enums::ScriptedProvider;
    } else if ( s == QLatin1String("gtfs") ) {
        return Enums::GtfsProvider;
    } else {
        return Enums::InvalidProvider;
    }
}

QString ServiceProviderGlobal::typeToString( Enums::ServiceProviderType type )
{
    switch ( type ) {
    case Enums::ScriptedProvider:
        return "script";
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
    case Enums::ScriptedProvider:
        name = i18nc("@info/plain Name of a service provider plugin type", "Scripted");
        break;
    case Enums::GtfsProvider:
        name = i18nc("@info/plain Name of a service provider plugin type", "GTFS");
        break;
    case Enums::InvalidProvider:
    default:
        kWarning() << "Invalid provider type" << type;
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
    const KMimeType::Ptr mimeType =
            KMimeType::mimeType("application/x-publictransport-serviceprovider");
    return mimeType.isNull() ? QStringList() : mimeType->patterns();
}

QStringList ServiceProviderGlobal::fileExtensions()
{
    QStringList extensions = filePatterns();
    for ( QStringList::Iterator it = extensions.begin(); it != extensions.end(); ++it ) {
        const int pos = it->lastIndexOf( '.' );
        if ( pos == -1 || pos == it->length() - 1 ) {
            kWarning() << "Could not extract file extension from mime type pattern!\n"
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

        // Remove symlinks to default providers from the list
        for ( int i = providers.count() - 1; i >= 0; --i ) {
            const QString &provider = providers[i];
            const QFileInfo fileInfo( provider );
            if ( fileInfo.isSymLink() && fileInfo.baseName().endsWith(QLatin1String("_default")) ) {
                providers.removeAt( i );
            }
        }
    }
    return providers;
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
                     "Stops by geolocial position");
    case Enums::ProvidesStopID:
        return i18nc("@info/plain A short string indicating that stop IDs can be provided",
                     "Stop ID");
    case Enums::ProvidesStopGeoPosition:
        return i18nc("@info/plain A short string indicating that stop geological positions can be provided",
                     "Geological stop position");
    case Enums::ProvidesPricing:
        return i18nc("@info/plain A short string indicating that pricing information can be provided",
                     "Pricing");
    case Enums::ProvidesRouteInformation:
        return i18nc("@info/plain A short string indicating that route information can be provided",
                     "Route information");
    default:
        kWarning() << "Unexpected feature value" << feature;
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
