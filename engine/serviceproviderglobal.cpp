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

QList< ServiceProviderType > ServiceProviderGlobal::availableProviderTypes()
{
    QList< ServiceProviderType > types;
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    types << ScriptedProvider;
#endif
#ifdef BUILD_PROVIDER_TYPE_GTFS
    types << GtfsProvider;
#endif
    return types;
}

bool ServiceProviderGlobal::isProviderTypeAvailable( ServiceProviderType type )
{
    switch ( type ) {
    case ScriptedProvider:
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        return true;
#else
        return false;
#endif

    case GtfsProvider:
#ifdef BUILD_PROVIDER_TYPE_GTFS
        return true;
#else
        return false;
#endif

    case InvalidProvider:
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
    return QString();
}

ServiceProviderType ServiceProviderGlobal::typeFromString(
        const QString &serviceProviderType )
{
    QString s = serviceProviderType.toLower();
    if ( s == QLatin1String("script") ||
         s == QLatin1String("html") ) // DEPRECATED
    {
        return ScriptedProvider;
    } else if ( s == QLatin1String("gtfs") ) {
        return GtfsProvider;
    } else {
        return InvalidProvider;
    }
}

QString ServiceProviderGlobal::typeToString( ServiceProviderType type )
{
    switch ( type ) {
    case ScriptedProvider:
        return "script";
    case GtfsProvider:
        return "gtfs";
    case InvalidProvider:
    default:
        return "invalid";
    }
}

QString ServiceProviderGlobal::typeName( ServiceProviderType type, ProviderTypeNameOptions options )
{
    QString name;
    switch ( type ) {
    case ScriptedProvider:
        name = i18nc("@info/plain Name of a service provider plugin type", "Scripted");
        break;
    case GtfsProvider:
        name = i18nc("@info/plain Name of a service provider plugin type", "GTFS");
        break;
    case InvalidProvider:
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

QStringList ServiceProviderGlobal::localizeFeatures( const QStringList &features )
{
    QStringList featuresl10n;
    if ( features.contains("Arrivals") ) {
        featuresl10n << i18nc( "Support for getting arrivals for a stop of public "
                               "transport. This string is used in a feature list, "
                               "should be short.", "Arrivals" );
    }
    if ( features.contains("Autocompletion") ) {
        featuresl10n << i18nc( "Autocompletion for names of public transport stops",
                               "Autocompletion" );
    }
    if ( features.contains("JourneySearch") ) {
        featuresl10n << i18nc( "Support for getting journeys from one stop to another. "
                               "This string is used in a feature list, should be short.",
                               "Journey search" );
    }
    if ( features.contains("Delay") ) {
        featuresl10n << i18nc( "Support for getting delay information. This string is "
                               "used in a feature list, should be short.", "Delay" );
    }
    if ( features.contains("DelayReason") ) {
        featuresl10n << i18nc( "Support for getting the reason of a delay. This string "
                               "is used in a feature list, should be short.",
                               "Delay reason" );
    }
    if ( features.contains("Platform") ) {
        featuresl10n << i18nc( "Support for getting the information from which platform "
                               "a public transport vehicle departs / at which it "
                               "arrives. This string is used in a feature list, "
                               "should be short.", "Platform" );
    }
    if ( features.contains("JourneyNews") ) {
        featuresl10n << i18nc( "Support for getting the news about a journey with public "
                               "transport, such as a platform change. This string is "
                               "used in a feature list, should be short.", "Journey news" );
    }
    if ( features.contains("TypeOfVehicle") ) {
        featuresl10n << i18nc( "Support for getting information about the type of "
                               "vehicle of a journey with public transport. This string "
                               "is used in a feature list, should be short.",
                               "Type of vehicle" );
    }
    if ( features.contains("Status") ) {
        featuresl10n << i18nc( "Support for getting information about the status of a "
                               "journey with public transport or an aeroplane. This "
                               "string is used in a feature list, should be short.",
                               "Status" );
    }
    if ( features.contains("Operator") ) {
        featuresl10n << i18nc( "Support for getting the operator of a journey with public "
                               "transport or an aeroplane. This string is used in a "
                               "feature list, should be short.", "Operator" );
    }
    if ( features.contains("StopID") ) {
        featuresl10n << i18nc( "Support for getting the id of a stop of public transport. "
                               "This string is used in a feature list, should be short.",
                               "Stop ID" );
    }
    return featuresl10n;
}
