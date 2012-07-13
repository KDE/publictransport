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
#include "serviceproviderdatawriter.h"

// PublicTransport engine includes
#include <engine/serviceprovider.h>
#include <engine/serviceproviderdata.h>
#include <engine/global.h>

// KDE includes
#include <KLocalizedString>
#include <KLocale>
#include <KDebug>
#include <KGlobal>

// Qt includes
#include <QFile>
#include <QFileInfo>

bool ServiceProviderDataWriter::write( QIODevice *device, const ServiceProvider *provider,
                                       const QString &comments )
{
    Q_ASSERT( device );

    bool closeAfterRead; // Only close after reading if it wasn't open before
    if( ( closeAfterRead = !device->isOpen() ) && !device->open( QIODevice::WriteOnly ) ) {
        return false;
    }
    setDevice( device );
    setAutoFormatting( true );

    const ServiceProviderData *data = provider->data();

    writeStartDocument();
    writeStartElement( "serviceProvider" );
    writeAttribute( "fileVersion", data->fileFormatVersion() );
    writeAttribute( "version", data->version() );
    writeAttribute( "type", data->typeString() );

    bool enUsed = false;
    for( QHash<QString, QString>::const_iterator it = data->names().constBegin();
         it != data->names().constEnd(); ++it )
    {
        QString lang = it.key() == QLatin1String("en_US") ? "en" : it.key();
        if( lang == QLatin1String("en") ) {
            if( enUsed ) {
                continue;
            }
            enUsed = true;
        }

        writeStartElement( "name" );
        writeAttribute( "lang", lang );
        writeCharacters( it.value() );
        writeEndElement();
    }

    enUsed = false;
    for( QHash<QString, QString>::const_iterator it = data->descriptions().constBegin();
         it != data->descriptions().constEnd(); ++it )
    {
        QString lang = it.key() == QLatin1String("en_US") ? "en" : it.key();
        if( lang == QLatin1String("en") ) {
            if( enUsed ) {
                continue;
            }
            enUsed = true;
        }

        writeStartElement( "description" );
        writeAttribute( "lang", lang );
        writeCharacters( it.value() );
        writeEndElement();
    }

    if ( !data->notes().isEmpty() ) {
        writeTextElement( "notes", data->notes() );
    }

    writeStartElement( "author" );
    writeTextElement( "fullname", data->author() );
    writeTextElement( "short", data->shortAuthor() );
    writeTextElement( "email", data->email() );
    writeEndElement();

    if( data->useSeparateCityValue() ) {
        writeTextElement( "useSeperateCityValue", data->useSeparateCityValue() ? "true" : "false" );
    }
    if( data->onlyUseCitiesInList() ) {
        writeTextElement( "onlyUseCitiesInList", data->onlyUseCitiesInList() ? "true" : "false" );
    }
    if( !data->url().isEmpty() ) {
        writeTextElement( "url", data->url() );
    }
    if( !data->shortUrl().isEmpty() ) {
        writeTextElement( "shortUrl", data->shortUrl() );
    }
    if( !data->credit().isEmpty() ) {
        writeTextElement( "credit", data->credit() );
    }
    if( data->defaultVehicleType() != Enums::UnknownVehicleType ) {
        writeTextElement( "defaultVehicleType",
                          Global::vehicleTypeToString(data->defaultVehicleType()) );
    }
    if( data->minFetchWait() > 2 ) {
        writeTextElement( "minFetchWait", QString::number( data->minFetchWait() ) );
    }
    if( !data->fallbackCharset().isEmpty() ) {
        writeTextElement( "fallbackCharset", data->fallbackCharset() );
    }
    if( !data->charsetForUrlEncoding().isEmpty() ) {
        writeTextElement( "charsetForUrlEncoding", data->charsetForUrlEncoding() );
    }

    // Write type specific data
    if( !data->scriptFileName().isEmpty() ) {
        // Write script file name without path (scripts are expected to be in the same path as the XML)
        writeStartElement( QLatin1String("script") );
        if ( !data->scriptExtensions().isEmpty() ) {
            writeAttribute( QLatin1String("extensions"), data->scriptExtensions().join(",") );
        }
        writeCharacters( QFileInfo(data->scriptFileName()).fileName() );
        writeEndElement(); // script
    }
    if ( !data->feedUrl().isEmpty() ) {
        writeTextElement( "feedUrl", data->feedUrl() );
    }
    if ( !data->realtimeTripUpdateUrl().isEmpty() ) {
        writeTextElement( "realtimeTripUpdateUrl", data->realtimeTripUpdateUrl() );
    }
    if ( !data->realtimeAlertsUrl().isEmpty() ) {
        writeTextElement( "realtimeAlertsUrl", data->realtimeAlertsUrl() );
    }
    if ( !data->timeZone().isEmpty() ) {
        writeTextElement( "timeZone", data->timeZone() );
    }

    if( !data->cities().isEmpty() ) {
        writeStartElement( "cities" );
        foreach( const QString & city, data->cities() ) {
            writeStartElement( "city" );
            const QString lowerCity = city.toLower();
            if( data->cityNameToValueReplacementHash().contains( lowerCity ) ) {
                writeAttribute( "replaceWith", data->cityNameToValueReplacementHash()[lowerCity] );
            }
            writeCharacters( city );
            writeEndElement(); // city
        }
        writeEndElement(); // cities
    }

    // Write changelog
    if( !data->changelog().isEmpty() ) {
        writeStartElement( "changelog" );
        foreach( const ChangelogEntry &entry, data->changelog() ) {
            writeStartElement( "entry" );
            if( !entry.author.isEmpty() && entry.author != data->shortAuthor() ) {
                writeAttribute( "author", entry.author );
            }
            writeAttribute( "version", entry.version );
            if( !entry.engineVersion.isEmpty() ) {
                writeAttribute( "engineVersion", entry.engineVersion );
            }
            writeCharacters( entry.description );
            writeEndElement(); // entry
        }
        writeEndElement(); // changelog
    }

    // Write samples
    if ( !data->sampleStopNames().isEmpty() || !data->sampleCity().isEmpty() ) {
        writeStartElement( "samples" );
        foreach ( const QString &stop, data->sampleStopNames() ) {
            writeTextElement( "stop", stop );
        }
        if ( !data->sampleCity().isEmpty() ) {
            writeTextElement( "city", data->sampleCity() );
        }
        writeEndElement(); // samples
    }

    writeEndElement(); // serviceProvider

    if ( !comments.isEmpty() ) {
        writeComment( comments );
    }
    writeEndDocument();

    return true;
}
