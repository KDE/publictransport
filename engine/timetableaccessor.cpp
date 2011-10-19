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
#include "accessorinfoxmlreader.h"
#include "timetableaccessor_generaltransitfeed.h"
#include "timetableaccessor_script.h"
#include "timetableaccessor_xml.h"

// Qt includes
#include <QTextCodec>
#include <QFile>

// KDE includes
#include <KIO/NetAccess>
#include <KStandardDirs>
#include <KLocale>
#include <KDebug>

TimetableAccessor::TimetableAccessor( TimetableAccessorInfo *info ) : m_info(info)
{
    m_idAlreadyRequested = false;
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
    if ( !m_info->sessionKeyUrl().isEmpty() && m_sessionKey.isEmpty()
        && m_sessionKeyGetTime.elapsed() > 500 )
    {
        kDebug() << "Request a session key";
        requestSessionKey( ParseForSessionKeyThenDepartures, m_info->sessionKeyUrl(), &requestInfo );
        return;
    }

    if ( !m_idAlreadyRequested &&
        m_info->attributesForDepatures().contains(QLatin1String("requestStopIdFirst")) &&
        m_info->attributesForDepatures()[QLatin1String("requestStopIdFirst")] == "true" )
    {
        kDebug() << "Request a stop ID";
        m_idAlreadyRequested = true;
        StopSuggestionRequestInfo newRequestInfo = requestInfo;
        newRequestInfo.parseMode = ParseForStopIdThenDepartures;
        requestStopSuggestions( newRequestInfo );
        return;
    }
    m_idAlreadyRequested = false;

    KUrl url = getUrl( requestInfo );
    KIO::StoredTransferJob *job; // = KIO::storedGet( url, KIO::NoReload, KIO::HideProgressInfo );
    if ( m_info->attributesForDepatures()[QLatin1String("method")]
            .compare(QLatin1String("post"), Qt::CaseInsensitive) != 0 )
    {
        job = KIO::storedGet( url, KIO::NoReload, KIO::HideProgressInfo );
    } else if ( m_info->attributesForDepatures().contains(QLatin1String("data")) ) {
        QString sData = m_info->attributesForDepatures()[QLatin1String("data")];
        sData.replace( QLatin1String("{city}"), requestInfo.city );
        sData.replace( QLatin1String("{stop}"), requestInfo.stop );

        QString sDataType;
        if ( requestInfo.dataType == "arrivals" ) {
            sDataType = "arr";
        } else if ( requestInfo.dataType == "departures" || requestInfo.dataType == "journeys" ) {
            sDataType = "dep";
        }

        QString sCity = m_info->mapCityNameToValue( requestInfo.city );
        QString sStop = requestInfo.stop;

        // Encode city and stop
        if ( charsetForUrlEncoding().isEmpty() ) {
            sCity = QString::fromAscii( QUrl::toPercentEncoding(sCity) );
            sStop = QString::fromAscii( QUrl::toPercentEncoding(sStop) );
        } else {
            sCity = toPercentEncoding( sCity, charsetForUrlEncoding() );
            sStop = toPercentEncoding( sStop, charsetForUrlEncoding() );
        }

        // Construct the data
        QByteArray data;
        if ( useSeparateCityValue() ) {
            sData = sData.replace( "{city}", sCity );
        }
        sData = sData.replace( "{time}", requestInfo.dateTime.time().toString("hh:mm") )
                .replace( "{timestamp}", QString::number(requestInfo.dateTime.toTime_t()) )
                .replace( "{maxCount}", QString::number(requestInfo.maxCount).toLatin1() )
                .replace( "{stop}", sStop.toLatin1() )
                .replace( "{dataType}", sDataType.toLatin1() );

        QRegExp rx = QRegExp( "\\{date:([^\\}]*)\\}", Qt::CaseInsensitive );
        if ( rx.indexIn(sData) != -1 ) {
            sData.replace( rx, requestInfo.dateTime.date().toString(rx.cap(1)) );
        }

        job =  KIO::storedHttpPost( QByteArray(), url, KIO::HideProgressInfo );
        if ( m_info->attributesForDepatures().contains(QLatin1String("contenttype")) ) {
            job->addMetaData( "content-type", QString("Content-Type: %1")
                    .arg(m_info->attributesForDepatures()[QLatin1String("contenttype")]) );
        }
        if ( m_info->attributesForDepatures().contains(QLatin1String("charset")) ) {
            QString sCodec = m_info->attributesForDepatures()[QLatin1String("charset")];
            job->addMetaData( "Charsets", sCodec );
            QTextCodec *codec = QTextCodec::codecForName( sCodec.toUtf8() );
            if ( !codec ) {
                job->setData( sData.toUtf8() );
            } else {
                kDebug() << "Codec:" << sCodec << "couldn't be found to encode the data "
                        "to post, now using UTF-8";
                job->setData( codec->fromUnicode(sData) );
            }
        } else {
            // No charset specified, use UTF8
            job->setData( sData.toUtf8() );
        }

        if ( m_info->attributesForDepatures().contains(QLatin1String("accept")) ) {
            job->addMetaData( "accept", m_info->attributesForDepatures()[QLatin1String("accept")] );
        }
    } else {
        kDebug() << "No \"data\" attribute given in the <departures>-tag in"
                 << m_info->fileName() << "but method is \"post\".";
        return;
    }

    // Add the session key
    switch ( m_info->sessionKeyPlace() ) {
    case PutIntoCustomHeader:
        kDebug() << "Using custom HTTP header" << QString("%1: %2").arg(m_info->sessionKeyData()).arg(m_sessionKey);
        job->addMetaData( "customHTTPHeader", QString("%1: %2").arg(m_info->sessionKeyData()).arg(m_sessionKey) );
        break;

    case PutNowhere:
    default:
        // Don't use a session key
        break;
    }

    RequestInfo *newRequestInfo = requestInfo.clone();
    newRequestInfo->parseMode = newRequestInfo->maxCount == -1
            ? ParseForStopSuggestions : ParseForDeparturesArrivals;
    m_jobInfos.insert( job, JobInfos(url, newRequestInfo) );

    connect( job, SIGNAL(result(KJob*)), this, SLOT(result(KJob*)) );
}

void TimetableAccessor::requestSessionKey( ParseDocumentMode parseMode,
        const KUrl &url, const RequestInfo *requestInfo )
{
    Q_UNUSED( parseMode );
    KIO::StoredTransferJob *job = KIO::storedGet( url, KIO::NoReload, KIO::HideProgressInfo );
    m_jobInfos.insert( job, JobInfos(url, requestInfo->clone()) );
//     TODO
    connect( job, SIGNAL(result(KJob*)), this, SLOT(result(KJob*)) );
}

void TimetableAccessor::clearSessionKey()
{
    m_sessionKey.clear();
}

void TimetableAccessor::requestStopSuggestions( const StopSuggestionRequestInfo &requestInfo )
{
    if ( !m_info->sessionKeyUrl().isEmpty() && m_sessionKey.isEmpty()
        && m_sessionKeyGetTime.elapsed() > 500 )
    {
        kDebug() << "Request a session key";
        requestSessionKey( ParseForSessionKeyThenStopSuggestions, m_info->sessionKeyUrl(),
                           &requestInfo );
        return;
    }

    if ( hasSpecialUrlForStopSuggestions() ) {
        KUrl url = getStopSuggestionsUrl( requestInfo );
        // TODO Use post-stuff also for (departures.. done) / journeys
        KIO::StoredTransferJob *job;
        if ( m_info->attributesForStopSuggestions()[QLatin1String("method")]
                .compare(QLatin1String("post"), Qt::CaseInsensitive) != 0 )
        {
            job = KIO::storedGet( url, KIO::NoReload, KIO::HideProgressInfo );
        } else if ( m_info->attributesForStopSuggestions().contains(QLatin1String("data")) ) {
            QString sData = m_info->attributesForStopSuggestions()[QLatin1String("data")];
            sData.replace( QLatin1String("{city}"), requestInfo.city );
            sData.replace( QLatin1String("{stop}"), requestInfo.stop );
            sData.replace( "{timestamp}", QString::number(requestInfo.dateTime.toTime_t()) );

            job =  KIO::storedHttpPost( QByteArray(), url, KIO::HideProgressInfo );
            if ( m_info->attributesForStopSuggestions().contains(QLatin1String("contenttype")) ) {
                job->addMetaData( "content-type", QString("Content-Type: %1")
                        .arg(m_info->attributesForStopSuggestions()[QLatin1String("contenttype")]) );
            }
            if ( m_info->attributesForStopSuggestions().contains(QLatin1String("acceptcharset")) ) {
                QString sCodec = m_info->attributesForStopSuggestions()[QLatin1String("acceptcharset")];
                job->addMetaData( "Charsets", sCodec );
            }
            if ( m_info->attributesForStopSuggestions().contains(QLatin1String("charset")) ) {
                QString sCodec = m_info->attributesForStopSuggestions()[QLatin1String("charset")];
//                 job->addMetaData( "Charsets", sCodec );
//                 job->addMetaData( "Charsets", "ISO-8859-1,utf-8" );
                QTextCodec *codec = QTextCodec::codecForName( sCodec.toUtf8() );
                if ( !codec ) {
                    kDebug() << "Codec:" << sCodec << "couldn't be found to encode the data "
                            "to post, now using UTF-8";
                    job->setData( sData.toUtf8() );
                } else {
                    job->setData( codec->fromUnicode(sData) );
                }

                kDebug() << "Post this data" << sData;
            } else {
                // No codec specified, use UTF8
                job->setData( sData.toUtf8() );
            }
            if ( m_info->attributesForStopSuggestions().contains(QLatin1String("accept")) ) {
                job->addMetaData( "accept", m_info->attributesForStopSuggestions()[QLatin1String("accept")] );
            }
        } else {
            kDebug() << "No \"data\" attribute given in the <stopSuggestions>-tag in"
                     << m_info->fileName() << "but method is \"post\".";
            return;
        }
        if ( requestInfo.parseMode == ParseForStopIdThenDepartures ) {
            m_jobInfos.insert( job, JobInfos(url, requestInfo.clone()) );
        } else {
            RequestInfo *newRequestInfo = requestInfo.clone();
            newRequestInfo->parseMode = ParseForStopSuggestions;
            m_jobInfos.insert( job, JobInfos(url, newRequestInfo) );
        }

        // Add the session key
        switch ( m_info->sessionKeyPlace() ) {
        case PutIntoCustomHeader:
            kDebug() << "Using custom HTTP header" << QString("%1: %2").arg(m_info->sessionKeyData()).arg(m_sessionKey);
            job->addMetaData( "customHTTPHeader", QString("%1: %2").arg(m_info->sessionKeyData()).arg(m_sessionKey) );
            break;

        case PutNowhere:
        default:
            // Don't use a session key
            break;
        }

        connect( job, SIGNAL(result(KJob*)), this, SLOT(result(KJob*)) );
    } else {
        DepartureRequestInfo newRequestInfo = requestInfo;
        newRequestInfo.maxCount = -1;
        newRequestInfo.dataType = "departures";
        requestDepartures( newRequestInfo );
    }
}

void TimetableAccessor::requestJourneys( const JourneyRequestInfo &requestInfo )
{
    // Creating a kioslave
    KUrl url = !requestInfo.urlToUse.isEmpty() ? KUrl(requestInfo.urlToUse)
            : getJourneyUrl(requestInfo);
    KIO::StoredTransferJob *job = KIO::storedGet( url, KIO::NoReload, KIO::HideProgressInfo );
    connect( job, SIGNAL(result(KJob*)), this, SLOT(result(KJob*)) );

    m_jobInfos.insert( job, JobInfos(url, requestInfo.clone()) );
}

void TimetableAccessor::result( KJob* job )
{
    JobInfos jobInfo = m_jobInfos.take( job );
    KIO::StoredTransferJob *storedJob = static_cast< KIO::StoredTransferJob* >( job );
    QByteArray document = storedJob->data();

    QList< PublicTransportInfo* > dataList;
    QList< StopInfo* > stopList;
    ParseDocumentMode parseDocumentMode = jobInfo.requestInfo->parseMode;
    GlobalTimetableInfo globalInfo;
    globalInfo.requestDate = jobInfo.requestInfo->dateTime.date();
    kDebug() << "Finished:" << parseDocumentMode << jobInfo.requestInfo->sourceName << jobInfo.url;

    if ( storedJob->error() != 0 ) {
        kDebug() << "Error in job:" << storedJob->error() << storedJob->errorString();
        emit errorParsing( this, ErrorDownloadFailed, storedJob->errorString(), jobInfo.url,
                           jobInfo.requestInfo.data() );
    }

    if ( parseDocumentMode == ParseForStopSuggestions ) {
        // A stop suggestion request has finished
        if ( parseDocumentPossibleStops(document, &stopList) ) {
            emit stopListReceived( this, jobInfo.url, stopList,
                                   jobInfo.requestInfo.data() );
        } else {
            kDebug() << "Error parsing for stop suggestions" << jobInfo.requestInfo->sourceName;
            emit errorParsing( this, ErrorParsingFailed,
                               i18n("Error while parsing the timetable document."),
                               jobInfo.url, jobInfo.requestInfo.data() );
        }

        return;
    } else if ( parseDocumentMode == ParseForStopIdThenDepartures ) {
        kDebug() << "MODE IS CURRENTLY" << parseDocumentMode;
        if ( parseDocumentPossibleStops(document, &stopList) ) {
            if ( stopList.isEmpty() ) {
                kDebug() << "No stop suggestions got to get an ID to use to get departures";
            } else {
                // Use the ID of the first suggested stop to get departures
                if ( !stopList.first()->contains(StopID) ) {
                    kDebug() << "No stop ID found for the given stop name, "
                                "now requesting departures using the stop name";
                    DepartureRequestInfo *newRequestInfo =
                            static_cast<DepartureRequestInfo*>( jobInfo.requestInfo.data() );
                    requestDepartures( *newRequestInfo );
                } else {
                    DepartureRequestInfo *newRequestInfo =
                            static_cast<DepartureRequestInfo*>( jobInfo.requestInfo.data() );
                    newRequestInfo->stop = stopList.first()->id();
                    requestDepartures( *newRequestInfo );
                }
                return;
            }
        } else {
            kDebug() << "Error parsing for stop suggestions to get an ID to use to get departure"
                     << jobInfo.requestInfo->sourceName;
            emit errorParsing( this, ErrorParsingFailed,
                               i18n("Error while parsing the timetable document."),
                               jobInfo.url, jobInfo.requestInfo.data() );
        }
    } else if ( parseDocumentMode == ParseForSessionKeyThenStopSuggestions ||
                parseDocumentMode == ParseForSessionKeyThenDepartures )
    {
        if ( !(m_sessionKey = parseDocumentForSessionKey(document)).isEmpty() ) {
            emit sessionKeyReceived( this, m_sessionKey );

            if ( !m_sessionKey.isEmpty() ) {
                // Now request stop suggestions using the session key
                if ( parseDocumentMode == ParseForSessionKeyThenStopSuggestions ) {
                    requestStopSuggestions( *jobInfo.requestInfo.data() );
                } else if ( parseDocumentMode == ParseForSessionKeyThenDepartures ) {
                    requestDepartures( *jobInfo.requestInfo.data() );
                }
                m_sessionKeyGetTime.start();

                // Clear the session key after a timeout
                QTimer::singleShot( 5 * 60000, this, SLOT(clearSessionKey()) );
            }
        } else {
            kDebug() << "Error getting a session key" << jobInfo.requestInfo->sourceName;
        }
        return;
    }

    m_curCity = jobInfo.requestInfo->city;
//     kDebug() << "usedDifferentUrl" << jobInfo.usedDifferentUrl;
    if ( !jobInfo.requestInfo->useDifferentUrl ) {
        QString sNextUrl;
        if ( parseDocumentMode == ParseForJourneys ) {
            const JourneyRequestInfo *journeyRequestInfo =
                    static_cast<JourneyRequestInfo*>( jobInfo.requestInfo.data() );
            if ( journeyRequestInfo->roundTrips < 2 ) {
                sNextUrl = parseDocumentForLaterJourneysUrl( document );
            } else if ( journeyRequestInfo->roundTrips == 2 ) {
                sNextUrl = parseDocumentForDetailedJourneysUrl( document );
            }
        }
//         kDebug() << "Parse results" << parseDocumentMode;

        // Try to parse the document
        if ( parseDocument(document, &dataList, &globalInfo, parseDocumentMode) ) {
            if ( parseDocumentMode == ParseForDeparturesArrivals ) {
                QList<DepartureInfo*> departures;
                foreach( PublicTransportInfo *info, dataList ) {
                    departures << dynamic_cast< DepartureInfo* >( info );
                }
                emit departureListReceived( this, jobInfo.url, departures, globalInfo,
                                            jobInfo.requestInfo.data() );
            } else if ( parseDocumentMode == ParseForJourneys ) {
                QList<JourneyInfo*> journeys;
                foreach( PublicTransportInfo *info, dataList ) {
                    journeys << dynamic_cast< JourneyInfo* >( info );
                }
                emit journeyListReceived( this, jobInfo.url, journeys, globalInfo,
                                          jobInfo.requestInfo.data() );
            }
            // Parsing has failed, try to parse stop suggestions.
            // First request departures using a different url if that is a special
            // url for stop suggestions.
        } else if ( hasSpecialUrlForStopSuggestions() ) {
            DepartureRequestInfo *newRequestInfo =
                    static_cast<DepartureRequestInfo*>( jobInfo.requestInfo.data() );
            newRequestInfo->city = m_curCity;
            newRequestInfo->useDifferentUrl = true;
            requestDepartures( *newRequestInfo );
            // Parse for stop suggestions
        } else if ( parseDocumentPossibleStops(document, &stopList) ) {
            kDebug() << "Stop suggestion list received" << parseDocumentMode;
            emit stopListReceived( this, jobInfo.url, stopList, jobInfo.requestInfo.data() );
        } else { // All parsing has failed
            emit errorParsing( this, ErrorParsingFailed, i18n("Error while parsing."),
                               jobInfo.url, jobInfo.requestInfo.data() );
        }

        if ( parseDocumentMode == ParseForJourneys ) {
            if ( !sNextUrl.isNull() && !sNextUrl.isEmpty() ) {
                kDebug() << "Request parsed url:" << sNextUrl;
                JourneyRequestInfo *newRequestInfo =
                        static_cast<JourneyRequestInfo*>( jobInfo.requestInfo.data() );
                ++newRequestInfo->roundTrips;
                newRequestInfo->urlToUse = sNextUrl;
                requestJourneys( *newRequestInfo );
            }
        }
        // Used a different url for requesting data, the data contains stop suggestions
    } else if ( parseDocumentPossibleStops(document, &stopList) ) {
        emit stopListReceived( this, jobInfo.url, stopList, jobInfo.requestInfo.data() );
    } else {
        kDebug() << "Error parsing for stop suggestions from different url" << jobInfo.requestInfo->sourceName;
        emit errorParsing( this, ErrorParsingFailed,
                           i18n("Error while parsing the stop suggestions document."),
                           jobInfo.url, jobInfo.requestInfo.data() );
    }
}

KUrl TimetableAccessor::getUrl( const DepartureRequestInfo &requestInfo ) const
{
    QString sRawUrl = requestInfo.useDifferentUrl ? stopSuggestionsRawUrl() : departuresRawUrl();
    QString sDataType;
    QString sCity = requestInfo.city.toLower(), sStop = requestInfo.stop.toLower();
    if ( requestInfo.dataType == "arrivals" ) {
        sDataType = "arr";
    } else if ( requestInfo.dataType == "departures" || requestInfo.dataType == "journeys" ) {
        sDataType = "dep";
    }

    sCity = timetableAccessorInfo().mapCityNameToValue( sCity );

    // Encode city and stop
    if ( charsetForUrlEncoding().isEmpty() ) {
        sCity = QString::fromAscii( QUrl::toPercentEncoding( sCity ) );
        sStop = QString::fromAscii( QUrl::toPercentEncoding( sStop ) );
    } else {
        sCity = toPercentEncoding( sCity, charsetForUrlEncoding() );
        sStop = toPercentEncoding( sStop, charsetForUrlEncoding() );
    }

    // Construct the url from the "raw" url by replacing values
    if ( useSeparateCityValue() ) {
        sRawUrl = sRawUrl.replace( "{city}", sCity );
    }
    sRawUrl = sRawUrl.replace( "{time}", requestInfo.dateTime.time().toString("hh:mm") )
              .replace( "{timestamp}", QString::number(requestInfo.dateTime.toTime_t()) )
              .replace( "{maxCount}", QString("%1").arg(requestInfo.maxCount) )
              .replace( "{stop}", sStop )
              .replace( "{dataType}", sDataType )
              .replace( "{sessionKey}", m_sessionKey );

    QRegExp rx = QRegExp( "\\{date:([^\\}]*)\\}", Qt::CaseInsensitive );
    if ( rx.indexIn( sRawUrl ) != -1 ) {
        sRawUrl.replace( rx, requestInfo.dateTime.date().toString(rx.cap(1)) );
    }

    return KUrl( sRawUrl );
}

KUrl TimetableAccessor::getStopSuggestionsUrl( const StopSuggestionRequestInfo &requestInfo )
{
    QString sRawUrl = stopSuggestionsRawUrl();
    QString sCity = requestInfo.city.toLower(), sStop = requestInfo.stop.toLower();

    // Encode stop
    if ( charsetForUrlEncoding().isEmpty() ) {
        sCity = QString::fromAscii( QUrl::toPercentEncoding( sCity ) );
        sStop = QString::fromAscii( QUrl::toPercentEncoding( sStop ) );
    } else {
        sCity = toPercentEncoding( sCity, charsetForUrlEncoding() );
        sStop = toPercentEncoding( sStop, charsetForUrlEncoding() );
    }

    if ( useSeparateCityValue() ) {
        sRawUrl = sRawUrl.replace( "{city}", sCity );
    }
    sRawUrl = sRawUrl.replace( "{stop}", sStop );
//     sRawUrl = sRawUrl.replace( "{timestamp}", QString::number(QDateTime::currentDateTime().toMSecsSinceEpoch()) );
    return KUrl( sRawUrl );
}

KUrl TimetableAccessor::getJourneyUrl( const JourneyRequestInfo &requestInfo ) const
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

    sCity = timetableAccessorInfo().mapCityNameToValue( sCity );

    // Encode city and stop
    if ( charsetForUrlEncoding().isEmpty() ) {
        sCity = QString::fromAscii( QUrl::toPercentEncoding( sCity ) );
        sStartStopName = QString::fromAscii( QUrl::toPercentEncoding( sStartStopName ) );
        sTargetStopName = QString::fromAscii( QUrl::toPercentEncoding( sTargetStopName ) );
    } else {
        sCity = toPercentEncoding( sCity, charsetForUrlEncoding() );
        sStartStopName = toPercentEncoding( sStartStopName, charsetForUrlEncoding() );
        sTargetStopName = toPercentEncoding( sTargetStopName, charsetForUrlEncoding() );
    }

    // Construct the url from the "raw" url by replacing values
    if ( useSeparateCityValue() ) {
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

bool TimetableAccessor::parseDocument( const QByteArray &document,
                                       QList<PublicTransportInfo*> *journeys,
                                       GlobalTimetableInfo *globalInfo,
                                       ParseDocumentMode parseDocumentMode )
{
    Q_UNUSED( document );
    Q_UNUSED( journeys );
    Q_UNUSED( globalInfo );
    Q_UNUSED( parseDocumentMode );
    return false;
}

bool TimetableAccessor::parseDocumentPossibleStops( const QByteArray &document,
        QList<StopInfo*> *stops )
{
    Q_UNUSED( document );
    Q_UNUSED( stops );
    return false;
}

QString TimetableAccessor::departuresRawUrl() const
{
    return m_info->departureRawUrl();
}

QString TimetableAccessor::stopSuggestionsRawUrl() const
{
    return m_info->stopSuggestionsRawUrl();
}

QByteArray TimetableAccessor::charsetForUrlEncoding() const
{
    return m_info->charsetForUrlEncoding();
}
