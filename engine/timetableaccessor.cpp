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

// Own includes
#include "timetableaccessor.h"
#include "timetableaccessor_info.h"
#include "timetableaccessor_generaltransitfeed.h"
#include "timetableaccessor_script.h"
#include "timetableaccessor_xml.h"
#include "accessorinfoxmlreader.h"
#include "departureinfo.h"

// KDE includes
#include <KIO/NetAccess>
#include <KIO/Job>
#include <KStandardDirs>
#include <KLocale>
#include <KDebug>

// Qt includes
#include <QTextCodec>
#include <QFile>
#include <QTimer>

TimetableAccessor::TimetableAccessor( TimetableAccessorInfo *info ) : m_info(info)
{
}

TimetableAccessor::~TimetableAccessor()
{
    delete m_info;
}

TimetableAccessor* TimetableAccessor::createAccessor( const QString &serviceProvider )
{
    // Read the XML file for the service provider
    TimetableAccessorInfo *accessorInfo = readAccessorInfo( serviceProvider );
    if ( !accessorInfo ) {
        // There was an error while reading the XML file
        return 0;
    }

    // Create the accessor
    if ( accessorInfo->accessorType() == ScriptedAccessor ) {
        // Ensure a script is specified
        if ( accessorInfo->scriptFileName().isEmpty() ) {
            delete accessorInfo;
            kDebug() << "HTML accessors need a script for parsing";
            return 0;
        }

        // Create the accessor and check for script errors
        TimetableAccessorScript *scriptAccessor = new TimetableAccessorScript( accessorInfo );
        if ( !scriptAccessor->hasScriptErrors() ) {
            return scriptAccessor;
        } else {
            delete scriptAccessor;
            kDebug() << "Couldn't correctly load the script (bad script)";
            return 0;
        }
    } else if ( accessorInfo->accessorType() == XmlAccessor ) {
        // Warn if no script is specified
        if ( accessorInfo->scriptFileName().isEmpty() ) {
            kDebug() << "XML accessors currently use a script to parse stop suggestions, "
                        "but no script file was specified";
        }

        // Create the accessor and check for script errors
        TimetableAccessorXml *xmlAccessor = new TimetableAccessorXml( accessorInfo );
        if ( xmlAccessor->stopSuggestionAccessor() &&
             xmlAccessor->stopSuggestionAccessor()->hasScriptErrors() )
        {
            delete xmlAccessor;
            kDebug() << "Couldn't correctly load the script (bad script)";
            return 0;
        } else {
            return xmlAccessor;
        }
    } else if ( accessorInfo->accessorType() == GtfsAccessor ) {
        // Create the accessor and check for script errors
        return new TimetableAccessorGeneralTransitFeed( accessorInfo );
    }

    delete accessorInfo;
    kDebug() << QString("Accessor type %1 not supported").arg(accessorInfo->accessorType());
    return 0;
}

TimetableAccessorInfo* TimetableAccessor::readAccessorInfo( const QString &serviceProvider )
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
    TimetableAccessorInfo *ret = reader.read( &file, sp, filePath, country );

    if ( !ret ) {
        kDebug() << "Error while reading accessor info xml" << filePath
                 << reader.lineNumber() << reader.errorString();
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

QString TimetableAccessor::accessorCacheFileName()
{
    return KGlobal::dirs()->saveLocation("data", "plasma_engine_publictransport/")
            .append( QLatin1String("datacache") );
}

AccessorType TimetableAccessor::accessorTypeFromString( const QString &accessorType )
{
    QString s = accessorType.toLower();
    if ( s == "script" || s == "html" ) {
        return ScriptedAccessor;
    } else if ( s == "gtfs" ) {
        return GtfsAccessor;
    } else if ( s == "xml" ) {
        return XmlAccessor;
    } else {
        return NoAccessor;
    }
}

QString TimetableAccessor::accessorTypeName( AccessorType accessorType )
{
    switch ( accessorType ) {
        case ScriptedAccessor:
            return i18nc("@info/plain Name of an accessor type", "Scripted");
        case XmlAccessor:
            return i18nc("@info/plain Name of an accessor type", "XML");
        case GtfsAccessor:
            return i18nc("@info/plain Name of an accessor type", "GTFS");

        case NoAccessor:
        default:
            return i18nc("@info/plain Name of an accessor type", "Invalid");
    }
}

VehicleType TimetableAccessor::vehicleTypeFromString( QString sVehicleType )
{
    QString sLower = sVehicleType.toLower();
    if ( sLower == "unknown" ) {
        return Unknown;
    } else if ( sLower == "tram" ) {
        return Tram;
    } else if ( sLower == "bus" ) {
        return Bus;
    } else if ( sLower == "subway" ) {
        return Subway;
    } else if ( sLower == "traininterurban" || sLower == "interurbantrain" ) {
        return InterurbanTrain;
    } else if ( sLower == "metro" ) {
        return Metro;
    } else if ( sLower == "trolleybus" ) {
        return TrolleyBus;
    } else if ( sLower == "trainregional" || sLower == "regionaltrain" ) {
        return RegionalTrain;
    } else if ( sLower == "trainregionalexpress" || sLower == "regionalexpresstrain" ) {
        return RegionalExpressTrain;
    } else if ( sLower == "traininterregio" || sLower == "interregionaltrain" ) {
        return InterregionalTrain;
    } else if ( sLower == "trainintercityeurocity" || sLower == "intercitytrain" ) {
        return IntercityTrain;
    } else if ( sLower == "trainintercityexpress" || sLower == "highspeedtrain" ) {
        return HighSpeedTrain;
    } else if ( sLower == "feet" ) {
        return Feet;
    } else if ( sLower == "ferry" ) {
        return Ferry;
    } else if ( sLower == "ship" ) {
        return Ship;
    } else if ( sLower == "plane" ) {
        return Plane;
    } else {
        return Unknown;
    }
}

TimetableInformation TimetableAccessor::timetableInformationFromString(
    const QString& sTimetableInformation )
{
    QString sInfo = sTimetableInformation.toLower();
    if ( sInfo == "nothing" ) {
        return Nothing;
    } else if ( sInfo == "departuredate" ) {
        return DepartureDate;
    } else if ( sInfo == "departuretime" ) {
        return DepartureTime;
    } else if ( sInfo == "departurehour" ) {
        return DepartureHour;
    } else if ( sInfo == "departureminute" ) {
        return DepartureMinute;
    } else if ( sInfo == "typeofvehicle" ) {
        return TypeOfVehicle;
    } else if ( sInfo == "transportline" ) {
        return TransportLine;
    } else if ( sInfo == "flightnumber" ) {
        return FlightNumber;
    } else if ( sInfo == "target" ) {
        return Target;
    } else if ( sInfo == "platform" ) {
        return Platform;
    } else if ( sInfo == "delay" ) {
        return Delay;
    } else if ( sInfo == "delayreason" ) {
        return DelayReason;
    } else if ( sInfo == "journeynews" ) {
        return JourneyNews;
    } else if ( sInfo == "journeynewsother" ) {
        return JourneyNewsOther;
    } else if ( sInfo == "journeynewslink" ) {
        return JourneyNewsLink;
    } else if ( sInfo == "departurehourprognosis" ) {
        return DepartureHourPrognosis;
    } else if ( sInfo == "departureminuteprognosis" ) {
        return DepartureMinutePrognosis;
    } else if ( sInfo == "status" ) {
        return Status;
    } else if ( sInfo == "departureyear" ) {
        return DepartureYear;
    } else if ( sInfo == "routestops" ) {
        return RouteStops;
    } else if ( sInfo == "routetimes" ) {
        return RouteTimes;
    } else if ( sInfo == "routetimesdeparture" ) {
        return RouteTimesDeparture;
    } else if ( sInfo == "routetimesarrival" ) {
        return RouteTimesArrival;
    } else if ( sInfo == "routeexactstops" ) {
        return RouteExactStops;
    } else if ( sInfo == "routetypesofvehicles" ) {
        return RouteTypesOfVehicles;
    } else if ( sInfo == "routetransportlines" ) {
        return RouteTransportLines;
    } else if ( sInfo == "routeplatformsdeparture" ) {
        return RoutePlatformsDeparture;
    } else if ( sInfo == "routeplatformsarrival" ) {
        return RoutePlatformsArrival;
    } else if ( sInfo == "routetimesdeparturedelay" ) {
        return RouteTimesDepartureDelay;
    } else if ( sInfo == "routetimesarrivaldelay" ) {
        return RouteTimesArrivalDelay;
    } else if ( sInfo == "operator" ) {
        return Operator;
    } else if ( sInfo == "departureamorpm" ) {
        return DepartureAMorPM;
    } else if ( sInfo == "departureamorpmprognosis" ) {
        return DepartureAMorPMPrognosis;
    } else if ( sInfo == "arrivalamorpm" ) {
        return ArrivalAMorPM;
    } else if ( sInfo == "duration" ) {
        return Duration;
    } else if ( sInfo == "startstopname" ) {
        return StartStopName;
    } else if ( sInfo == "startstopid" ) {
        return StartStopID;
    } else if ( sInfo == "targetstopname" ) {
        return TargetStopName;
    } else if ( sInfo == "targetstopid" ) {
        return TargetStopID;
    } else if ( sInfo == "arrivaldate" ) {
        return ArrivalDate;
    } else if ( sInfo == "arrivaltime" ) {
        return ArrivalTime;
    } else if ( sInfo == "arrivalhour" ) {
        return ArrivalHour;
    } else if ( sInfo == "arrivalminute" ) {
        return ArrivalMinute;
    } else if ( sInfo == "changes" ) {
        return Changes;
    } else if ( sInfo == "typesofvehicleinjourney" ) {
        return TypesOfVehicleInJourney;
    } else if ( sInfo == "pricing" ) {
        return Pricing;
    } else if ( sInfo == "isnightline" ) {
        return IsNightLine;
    } else if ( sInfo == "nomatchonschedule" ) {
        return NoMatchOnSchedule;
    } else if ( sInfo == "stopname" ) {
        return StopName;
    } else if ( sInfo == "stopid" ) {
        return StopID;
    } else if ( sInfo == "stopweight" ) {
        return StopWeight;
    } else if ( sInfo == "stopcity" ) {
        return StopCity;
    } else if ( sInfo == "stopcountrycode" ) {
        return StopCountryCode;
    } else {
        kDebug() << sTimetableInformation
        << "is an unknown timetable information value! Assuming value Nothing.";
        return Nothing;
    }
}

QStringList TimetableAccessor::features() const
{
    QStringList list;

    if ( m_info->departureRawUrl().contains( "{dataType}" ) ) {
        list << "Arrivals";
    }

//     if ( m_info->scriptFileName().isEmpty() ) {
//         TimetableAccessorInfoRegExp *infoRegExp = dynamic_cast<TimetableAccessorInfoRegExp*>( m_info );
//         if ( infoRegExp ) {
//             if ( m_info->supportsStopAutocompletion() ) {
//                 list << "Autocompletion";
//             }
//             if ( !infoRegExp->searchJourneys().regExp().isEmpty() ) {
//                 list << "JourneySearch";
//             }
//             if ( m_info->supportsTimetableAccessorInfo( Delay ) ) {
//                 list << "Delay";
//             }
//             if ( m_info->supportsTimetableAccessorInfo( DelayReason ) ) {
//                 list << "DelayReason";
//             }
//             if ( m_info->supportsTimetableAccessorInfo( Platform ) ) {
//                 list << "Platform";
//             }
//             if ( m_info->supportsTimetableAccessorInfo( JourneyNews )
//                     || m_info->supportsTimetableAccessorInfo( JourneyNewsOther )
//                     || m_info->supportsTimetableAccessorInfo( JourneyNewsLink ) ) {
//                 list << "JourneyNews";
//             }
//             if ( m_info->supportsTimetableAccessorInfo( TypeOfVehicle ) ) {
//                 list << "TypeOfVehicle";
//             }
//             if ( m_info->supportsTimetableAccessorInfo( Status ) ) {
//                 list << "Status";
//             }
//             if ( m_info->supportsTimetableAccessorInfo( Operator ) ) {
//                 list << "Operator";
//             }
//             if ( m_info->supportsTimetableAccessorInfo( StopID ) ) {
//                 list << "StopID";
//             }
//         }
//     } else {
        list << scriptFeatures();
//     } TODO

    list.removeDuplicates();
    return list;
}

QStringList TimetableAccessor::featuresLocalized() const
{
    QStringList featuresl10n;
    QStringList featureList = features();

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

void TimetableAccessor::requestDepartures( const DepartureRequestInfo &requestInfo )
{
    Q_UNUSED( requestInfo );
}

void TimetableAccessor::requestSessionKey( ParseDocumentMode parseMode,
        const KUrl &url, const RequestInfo *requestInfo )
{
    Q_UNUSED( parseMode );
    Q_UNUSED( url );
    Q_UNUSED( requestInfo );
}

void TimetableAccessor::requestStopSuggestions( const StopSuggestionRequestInfo &requestInfo )
{
    Q_UNUSED( requestInfo );
}

void TimetableAccessor::requestJourneys( const JourneyRequestInfo &requestInfo )
{
    Q_UNUSED( requestInfo );
}

KUrl TimetableAccessor::departureUrl( const DepartureRequestInfo &requestInfo ) const
{
    QString sRawUrl = requestInfo.useDifferentUrl ? m_info->stopSuggestionsRawUrl()
                                                  : m_info->departureRawUrl();
    QString sDataType;
    QString sCity = requestInfo.city.toLower(), sStop = requestInfo.stop.toLower();
    if ( requestInfo.dataType == "arrivals" ) {
        sDataType = "arr";
    } else if ( requestInfo.dataType == "departures" || requestInfo.dataType == "journeys" ) {
        sDataType = "dep";
    }

    sCity = m_info->mapCityNameToValue( sCity );

    // Encode city and stop
    if ( m_info->charsetForUrlEncoding().isEmpty() ) {
        sCity = QString::fromAscii( QUrl::toPercentEncoding( sCity ) );
        sStop = QString::fromAscii( QUrl::toPercentEncoding( sStop ) );
    } else {
        sCity = toPercentEncoding( sCity, m_info->charsetForUrlEncoding() );
        sStop = toPercentEncoding( sStop, m_info->charsetForUrlEncoding() );
    }

    // Construct the url from the "raw" url by replacing values
    if ( m_info->useSeparateCityValue() ) {
        sRawUrl.replace( "{city}", sCity );
    }
    sRawUrl.replace( "{time}", requestInfo.dateTime.time().toString("hh:mm") )
           .replace( "{timestamp}", QString::number(requestInfo.dateTime.toTime_t()) )
           .replace( "{maxCount}", QString("%1").arg(requestInfo.maxCount) )
           .replace( "{stop}", sStop )
           .replace( "{dataType}", sDataType );

    QRegExp rx = QRegExp( "\\{date:([^\\}]*)\\}", Qt::CaseInsensitive );
    if ( rx.indexIn( sRawUrl ) != -1 ) {
        sRawUrl.replace( rx, requestInfo.dateTime.date().toString(rx.cap(1)) );
    }

    return KUrl( sRawUrl );
}

KUrl TimetableAccessor::stopSuggestionsUrl( const StopSuggestionRequestInfo &requestInfo )
{
    QString sRawUrl = m_info->stopSuggestionsRawUrl();
    QString sCity = requestInfo.city.toLower(), sStop = requestInfo.stop.toLower();

    // Encode stop
    if ( m_info->charsetForUrlEncoding().isEmpty() ) {
        sCity = QString::fromAscii( QUrl::toPercentEncoding( sCity ) );
        sStop = QString::fromAscii( QUrl::toPercentEncoding( sStop ) );
    } else {
        sCity = toPercentEncoding( sCity, m_info->charsetForUrlEncoding() );
        sStop = toPercentEncoding( sStop, m_info->charsetForUrlEncoding() );
    }

    if ( m_info->useSeparateCityValue() ) {
        sRawUrl = sRawUrl.replace( "{city}", sCity );
    }
    sRawUrl = sRawUrl.replace( "{stop}", sStop );
//     sRawUrl = sRawUrl.replace( "{timestamp}", QString::number(QDateTime::currentDateTime().toMSecsSinceEpoch()) );
    return KUrl( sRawUrl );
}

KUrl TimetableAccessor::journeyUrl( const JourneyRequestInfo &requestInfo ) const
{
    QString sRawUrl = m_info->journeyRawUrl();
    QString sTime = requestInfo.dateTime.time().toString( "hh:mm" );
    QString sDataType;
    QString sCity = requestInfo.city.toLower(), sStartStopName = requestInfo.stop.toLower(),
                    sTargetStopName = requestInfo.targetStop.toLower();
    if ( requestInfo.dataType == "arrivals" || requestInfo.dataType == "journeysArr" ) {
        sDataType = "arr";
    } else if ( requestInfo.dataType == "departures" || requestInfo.dataType == "journeysDep" ) {
        sDataType = "dep";
    }

    sCity = m_info->mapCityNameToValue( sCity );

    // Encode city and stop
    if ( m_info->charsetForUrlEncoding().isEmpty() ) {
        sCity = QString::fromAscii( QUrl::toPercentEncoding( sCity ) );
        sStartStopName = QString::fromAscii( QUrl::toPercentEncoding( sStartStopName ) );
        sTargetStopName = QString::fromAscii( QUrl::toPercentEncoding( sTargetStopName ) );
    } else {
        sCity = toPercentEncoding( sCity, m_info->charsetForUrlEncoding() );
        sStartStopName = toPercentEncoding( sStartStopName, m_info->charsetForUrlEncoding() );
        sTargetStopName = toPercentEncoding( sTargetStopName, m_info->charsetForUrlEncoding() );
    }

    // Construct the url from the "raw" url by replacing values
    if ( m_info->useSeparateCityValue() ) {
        sRawUrl = sRawUrl.replace( "{city}", sCity );
    }

    sRawUrl = sRawUrl.replace( "{time}", sTime )
                     .replace( "{maxCount}", QString("%1").arg(requestInfo.maxCount) )
                     .replace( "{startStop}", sStartStopName )
                     .replace( "{targetStop}", sTargetStopName )
                     .replace( "{dataType}", sDataType );

    QRegExp rx = QRegExp( "\\{date:([^\\}]*)\\}", Qt::CaseInsensitive );
    if ( rx.indexIn( sRawUrl ) != -1 ) {
        sRawUrl.replace( rx, requestInfo.dateTime.date().toString( rx.cap( 1 ) ) );
    }

    rx = QRegExp( "\\{dep=([^\\|]*)\\|arr=([^\\}]*)\\}", Qt::CaseInsensitive );
    if ( rx.indexIn( sRawUrl ) != -1 ) {
        if ( sDataType == "arr" ) {
            sRawUrl.replace( rx, rx.cap( 2 ) );
        } else {
            sRawUrl.replace( rx, rx.cap( 1 ) );
        }
    }

    return KUrl( sRawUrl );
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

bool TimetableAccessor::hasSpecialUrlForStopSuggestions() const
{
    return !m_info->stopSuggestionsRawUrl().isEmpty();
}
