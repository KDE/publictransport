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
#include "accessorinfoxmlwriter.h"

// PublicTransport engine includes
#include <engine/timetableaccessor_info.h>
#include <engine/global.h>

// KDE includes
#include <KLocalizedString>
#include <KLocale>
#include <KDebug>
#include <KGlobal>

// Qt includes
#include <QFile>
#include <QFileInfo>

bool AccessorInfoXmlWriter::write( QIODevice *device, const TimetableAccessor *accessor )
{
    Q_ASSERT( device );

    bool closeAfterRead; // Only close after reading if it wasn't open before
    if( ( closeAfterRead = !device->isOpen() ) && !device->open( QIODevice::WriteOnly ) ) {
        return false;
    }
    setDevice( device );
    setAutoFormatting( true );

    const TimetableAccessorInfo *info = accessor->info();

    writeStartDocument();
    writeStartElement( "accessorInfo" );
    writeAttribute( "fileVersion", info->fileVersion() );
    writeAttribute( "version", info->version() );
    writeAttribute( "type", info->accessorType() == ScriptedAccessor ? "Script" : "Unknown" );

    bool enUsed = false;
    for( QHash<QString, QString>::const_iterator it = info->names().constBegin();
         it != info->names().constEnd(); ++it )
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
    for( QHash<QString, QString>::const_iterator it = info->descriptions().constBegin();
         it != info->descriptions().constEnd(); ++it )
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

    writeStartElement( "author" );
    writeTextElement( "fullname", info->author() );
    writeTextElement( "short", info->shortAuthor() );
    writeTextElement( "email", info->email() );
    writeEndElement();

    if( info->useSeparateCityValue() ) {
        writeTextElement( "useSeperateCityValue", info->useSeparateCityValue() ? "true" : "false" );
    }
    if( info->onlyUseCitiesInList() ) {
        writeTextElement( "onlyUseCitiesInList", info->onlyUseCitiesInList() ? "true" : "false" );
    }
    if( !info->url().isEmpty() ) {
        writeTextElement( "url", info->url() );
    }
    if( !info->shortUrl().isEmpty() ) {
        writeTextElement( "shortUrl", info->shortUrl() );
    }
    if( !info->credit().isEmpty() ) {
        writeTextElement( "credit", info->credit() );
    }
    if( info->defaultVehicleType() != Unknown ) {
        writeTextElement( "defaultVehicleType",
                          Global::vehicleTypeToString(info->defaultVehicleType()) );
    }
    if( info->minFetchWait() > 2 ) {
        writeTextElement( "minFetchWait", QString::number( info->minFetchWait() ) );
    }
    if( !info->fallbackCharset().isEmpty() ) {
        writeTextElement( "fallbackCharset", info->fallbackCharset() );
    }
    if( !info->charsetForUrlEncoding().isEmpty() ) {
        writeTextElement( "charsetForUrlEncoding", info->charsetForUrlEncoding() );
    }
    if( !info->scriptFileName().isEmpty() ) {
        // Write script file name without path (scripts are expected to be in the same path as the XML)
        writeStartElement( QLatin1String("script") );
        writeAttribute( QLatin1String("extensions"), info->scriptExtensions().join(",") );
        writeCharacters( QFileInfo(info->scriptFileName()).fileName() );
        writeEndElement(); // script
    }
    if( !info->cities().isEmpty() ) {
        writeStartElement( "cities" );
        foreach( const QString & city, info->cities() ) {
            writeStartElement( "city" );
            const QString lowerCity = city.toLower();
            if( info->cityNameToValueReplacementHash().contains( lowerCity ) ) {
                writeAttribute( "replaceWith", info->cityNameToValueReplacementHash()[lowerCity] );
            }
            writeCharacters( city );
            writeEndElement(); // city
        }
        writeEndElement(); // cities
    }

    // Write changelog
    if( !info->changelog().isEmpty() ) {
        writeStartElement( "changelog" );
        foreach( const ChangelogEntry &entry, info->changelog() ) {
            writeStartElement( "entry" );
            if( !entry.author.isEmpty() && entry.author != info->shortAuthor() ) {
                writeAttribute( "author", entry.author );
            }
            writeAttribute( "since", entry.version );
            if( !entry.engineVersion.isEmpty() ) {
                writeAttribute( "releasedWith", entry.engineVersion );
            }
            writeCharacters( entry.description );
            writeEndElement(); // entry
        }
        writeEndElement(); // changelog
    }

    // Write samples
    if ( !info->sampleStopNames().isEmpty() || !info->sampleCity().isEmpty() ) {
        writeStartElement( "samples" );
        foreach ( const QString &stop, info->sampleStopNames() ) {
            writeTextElement( "stop", stop );
        }
        if ( !info->sampleCity().isEmpty() ) {
            writeTextElement( "city", info->sampleCity() );
        }
        writeEndElement(); // samples
    } else {
        kDebug() << "NO SAMPLES TO WRITE";
    }

    writeEndElement(); // accessorInfo
    writeEndDocument();
    return true;
}
