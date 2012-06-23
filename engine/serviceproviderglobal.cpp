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

// Qt includes
#include <QFile>
#include <QFileInfo>

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
                filePath = dir + filePath;
                break;
            }
        }
    }

    // Get the real filename the "xx_default.pts/xml"-symlink links to
    filePath = KGlobal::dirs()->realFilePath( filePath );
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
    }
    return providers;
}

bool ServiceProviderGlobal::isSourceFileModified( const QString &providerId, const QString &fileName )
{
    // Check if the script file was modified since the cache was last updated
    const KConfig config ( cacheFileName(), KConfig::SimpleConfig );
    const KConfigGroup group = config.group( providerId );
    const QDateTime modifiedTime = group.readEntry ( "modifiedTime", QDateTime() );
    const QString _fileName = !fileName.isEmpty() ? fileName : fileNameFromId(providerId);
    if ( _fileName.isEmpty() ) {
        // Source XML file for the provider not found
        return true;
    } else {
        return QFileInfo ( _fileName ).lastModified() != modifiedTime;
    }
}
