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
#include "timetableaccessor.h"

// Own includes
#include "timetableaccessor_info.h"
#include "accessorinfoxmlreader.h"
#include "departureinfo.h"

// KDE includes
#include <KIO/NetAccess>
#include <KStandardDirs>
#include <KLocale>
#include <KDebug>
#include <kio/job.h>

// Qt includes
#include <QTextCodec>
#include <QFile>
#include <QTimer>
#include <QScriptEngine>

TimetableAccessor::TimetableAccessor( TimetableAccessorInfo *info )
        : m_info(info ? info : new TimetableAccessorInfo())
{
    m_idAlreadyRequested = false;
}

TimetableAccessor::~TimetableAccessor()
{
    if ( !m_jobInfos.isEmpty() ) {
        // There are still pending requests, data haven't been completely received.
        // The result is, that the data engine won't be able to fill the data source with values.
        // A possible cause is calling Plasma::DataEngineManager::unloadEngine() more often
        // than Plasma::DataEngineManager::loadEngine(), which deletes the whole data engine
        // with all it's currently loaded accessors even if one other visualization is connected
        // to the data engine. The dataUpdated() slot of this other visualization won't be called.
        kDebug() << "Accessor with" << m_jobInfos.count() << "pending requests deleted";
        if ( m_info ) {
            kDebug() << m_info->serviceProvider() << m_jobInfos.count();
        }
    }
    delete m_info;
}

TimetableAccessor* TimetableAccessor::getSpecificAccessor( const QString &serviceProvider )
{
    QString filePath;
    QString country = "international";
    QString sp = serviceProvider;
    if ( sp.isEmpty() ) {
        // No service provider ID given, use the default one for the users country
        country = KGlobal::locale()->country();

        // Try to find the XML filename of the default accessor for [country]
        filePath = defaultServiceProviderForLocation( country );
        if ( filePath.isEmpty() ) {
            return 0;
        }

        // Extract service provider ID from filename
        sp = serviceProviderIdFromFileName( filePath );
        kDebug() << "No service provider ID given, using the default one for country"
                 << country << "which is" << sp;
    } else {
        filePath = KGlobal::dirs()->findResource( "data",
                   QString("plasma_engine_publictransport/accessorInfos/%1.xml").arg(sp) );
        if ( filePath.isEmpty() ) {
            kDebug() << "Couldn't find a service provider information XML named" << sp;
            return NULL;
        }

        // Get country code from filename
        QRegExp rx( "^([^_]+)" );
        if ( rx.indexIn(sp) != -1 && KGlobal::locale()->allCountriesList().contains(rx.cap()) ) {
            country = rx.cap();
        }
    }

    QFile file( filePath );
    AccessorInfoXmlReader reader;
    TimetableAccessor *ret = reader.read( &file, sp, filePath, country );
    if ( !ret ) {
        kDebug() << "Error while reading accessor info xml" << filePath << reader.lineNumber() << reader.errorString();
    }
    return ret;
}

QString TimetableAccessor::defaultServiceProviderForLocation( const QString &location,
                                                              const QStringList &dirs )
{
    // Get the filename of the default accessor for the given location
    const QStringList _dirs = !dirs.isEmpty() ? dirs
            : KGlobal::dirs()->findDirs( "data", "plasma_engine_publictransport/accessorInfos" );
    QString fileName = QString( "%1_default.xml" ).arg( location );
    foreach( const QString &dir, _dirs ) {
        if ( QFile::exists(dir + fileName) ) {
            fileName = dir + fileName;
            break;
        }
    }

    // Get the real filename the "xx_default.xml"-symlink links to
    fileName = KGlobal::dirs()->realFilePath( fileName );
    if ( fileName.isEmpty() ) {
        kDebug() << "Couldn't find the default service provider for location" << location;
    }
    return fileName;
}

QString TimetableAccessor::serviceProviderIdFromFileName( const QString &accessorXmlFileName )
{
    // Cut the service provider substring from the XML filename, ie. "/path/to/xml/<id>.xml"
    const int pos = accessorXmlFileName.lastIndexOf( '/' );
    return accessorXmlFileName.mid( pos + 1, accessorXmlFileName.length() - pos - 5 );
}

AccessorType TimetableAccessor::accessorTypeFromString( const QString &sAccessorType )
{
    QString s = sAccessorType.toLower();
    if ( s == "script" ||  s == "html" ) {
        return ScriptedAccessor;
    } else {
        return NoAccessor;
    }
}

QStringList TimetableAccessor::features() const
{
    QStringList list;
    list << scriptFeatures();
    list.removeDuplicates();
    return list;
}

QStringList TimetableAccessor::featuresLocalized() const
{
    const QStringList featureList = features();
    QStringList featuresl10n;

    if ( featureList.contains( "Arrivals" ) ) {
        featuresl10n << i18nc( "Support for getting arrivals for a stop of public "
                               "transport. This string is used in a feature list, "
                               "should be short.", "Arrivals" );
    }
    if ( featureList.contains( "Autocompletion" ) ) {
        featuresl10n << i18nc( "Autocompletion for names of public transport stops",
                               "Autocompletion" );
    }
    if ( featureList.contains( "JourneySearch" ) ) {
        featuresl10n << i18nc( "Support for getting journeys from one stop to another. "
                               "This string is used in a feature list, should be short.",
                               "Journey search" );
    }
    if ( featureList.contains( "Delay" ) ) {
        featuresl10n << i18nc( "Support for getting delay information. This string is "
                               "used in a feature list, should be short.", "Delay" );
    }
    if ( featureList.contains( "DelayReason" ) ) {
        featuresl10n << i18nc( "Support for getting the reason of a delay. This string "
                               "is used in a feature list, should be short.",
                               "Delay reason" );
    }
    if ( featureList.contains( "Platform" ) ) {
        featuresl10n << i18nc( "Support for getting the information from which platform "
                               "a public transport vehicle departs / at which it "
                               "arrives. This string is used in a feature list, "
                               "should be short.", "Platform" );
    }
    if ( featureList.contains( "JourneyNews" ) ) {
        featuresl10n << i18nc( "Support for getting the news about a journey with public "
                               "transport, such as a platform change. This string is "
                               "used in a feature list, should be short.", "Journey news" );
    }
    if ( featureList.contains( "TypeOfVehicle" ) ) {
        featuresl10n << i18nc( "Support for getting information about the type of "
                               "vehicle of a journey with public transport. This string "
                               "is used in a feature list, should be short.",
                               "Type of vehicle" );
    }
    if ( featureList.contains( "Status" ) ) {
        featuresl10n << i18nc( "Support for getting information about the status of a "
                               "journey with public transport or an aeroplane. This "
                               "string is used in a feature list, should be short.",
                               "Status" );
    }
    if ( featureList.contains( "Operator" ) ) {
        featuresl10n << i18nc( "Support for getting the operator of a journey with public "
                               "transport or an aeroplane. This string is used in a "
                               "feature list, should be short.", "Operator" );
    }
    if ( featureList.contains( "StopID" ) ) {
        featuresl10n << i18nc( "Support for getting the id of a stop of public transport. "
                               "This string is used in a feature list, should be short.",
                               "Stop ID" );
    }
    return featuresl10n;
}

void TimetableAccessor::requestDepartures(
        const DepartureRequestInfo &requestInfo )
{
    kDebug() << "Not implemented";
    return;
}

void TimetableAccessor::requestStopSuggestions(
        const StopSuggestionRequestInfo &requestInfo )
{
    kDebug() << "Not implemented";
    return;
}

void TimetableAccessor::requestJourneys( const JourneyRequestInfo &requestInfo )
{
    kDebug() << "Not implemented";
    return;
}

QString TimetableAccessor::gethex( ushort decimal )
{
    QString hexchars = "0123456789ABCDEFabcdef";
    return QChar( '%' ) + hexchars[decimal >> 4] + hexchars[decimal & 0xF];
}

QString TimetableAccessor::toPercentEncoding( const QString &str, const QByteArray &charset )
{
    QString unreserved = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_.~";
    QString encoded;

    QByteArray ba = QTextCodec::codecForName( charset )->fromUnicode( str );
    for ( int i = 0; i < ba.length(); ++i ) {
        char ch = ba[i];
        if ( unreserved.indexOf(ch) != -1 ) {
            encoded += ch;
        } else if ( ch < 0 ) {
            encoded += gethex( 256 + ch );
        } else {
            encoded += gethex( ch );
        }
    }

    return encoded;
}

QByteArray TimetableAccessor::charsetForUrlEncoding() const
{
    return m_info->charsetForUrlEncoding();
}

QString TimetableAccessor::serviceProvider() const
{
    return m_info->serviceProvider();
}

int TimetableAccessor::minFetchWait() const
{
    return m_info->minFetchWait();
}

QString TimetableAccessor::country() const
{
    return m_info->country();
}

QStringList TimetableAccessor::cities() const
{
    return m_info->cities();
}

QString TimetableAccessor::credit() const
{
    return m_info->credit();
}

bool TimetableAccessor::useSeparateCityValue() const
{
    return m_info->useSeparateCityValue();
}

bool TimetableAccessor::onlyUseCitiesInList() const
{
    return m_info->onlyUseCitiesInList();
}

QScriptValue StopSuggestionRequestInfo::toScriptValue( QScriptEngine *engine ) const
{
    QScriptValue argument = engine->newObject();
    argument.setProperty( QLatin1String("stop"), stop );
    argument.setProperty( QLatin1String("city"), city );
    argument.setProperty( QLatin1String("maxCount"), maxCount );
    return argument;
}

QScriptValue DepartureRequestInfo::toScriptValue( QScriptEngine *engine ) const
{
    QScriptValue argument = engine->newObject();
    argument.setProperty( QLatin1String("stop"), stop );
    argument.setProperty( QLatin1String("city"), city );
    argument.setProperty( QLatin1String("maxCount"), maxCount );
    argument.setProperty( QLatin1String("dateTime"), engine->newDate(dateTime) );
    argument.setProperty( QLatin1String("dataType"), dataType );
    return argument;
}

QScriptValue JourneyRequestInfo::toScriptValue( QScriptEngine *engine ) const
{
    QScriptValue argument = engine->newObject();
    argument.setProperty( QLatin1String("stop"), stop );
    argument.setProperty( QLatin1String("city"), city );
    argument.setProperty( QLatin1String("maxCount"), maxCount );
    argument.setProperty( QLatin1String("originStop"), stop ); // Already in argument as "stop"
    argument.setProperty( QLatin1String("targetStop"), targetStop );
    argument.setProperty( QLatin1String("dateTime"), engine->newDate(dateTime) );
    argument.setProperty( QLatin1String("dataType"), dataType );
    return argument;
}
