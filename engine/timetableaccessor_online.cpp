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

// Header
#include "timetableaccessor_online.h"

// Own includes
#include "timetableaccessor_info.h"
#include "departureinfo.h"

// KDE includes
#include <KIO/JobClasses>
#include <KIO/Job>
#include <KLocalizedString>
#include <KDebug>

// Qt includes
#include <QTextCodec>
#include <QTimer>

TimetableAccessorOnline::TimetableAccessorOnline( TimetableAccessorInfo *info, QObject *parent )
    : TimetableAccessor(info, parent)
{
    m_stopIdRequested = false;
}

void TimetableAccessorOnline::requestDepartures( const DepartureRequestInfo &requestInfo )
{
    // Test if a session key needs to be requested first
    if ( !m_info->sessionKeyUrl().isEmpty() && m_sessionKey.isEmpty() &&
         m_sessionKeyGetTime.elapsed() > 500 )
    {
        // Session key not already available and no request made in the last 500ms
        kDebug() << "Request a session key";
        RequestInfo *newRequestInfo = requestInfo.clone();
        newRequestInfo->parseMode = ParseForSessionKeyThenDepartures;
        requestSessionKey( newRequestInfo );
        return;
    }

    // Test if a stop ID needs to be requested first
    if ( !m_stopIdRequested &&
         m_info->attributesForDepatures().contains(QLatin1String("requestStopIdFirst")) &&
         m_info->attributesForDepatures()[QLatin1String("requestStopIdFirst")] == "true" )
    {
        // XML attribute "requestStopIdFirst" is present and it's value is "true"
        // for the <departures> tag
        kDebug() << "Request a stop ID";
        m_stopIdRequested = true;
        StopSuggestionRequestInfo newRequestInfo = requestInfo;
        newRequestInfo.parseMode = ParseForStopIdThenDepartures;
        requestStopSuggestions( newRequestInfo );
        return;
    }
    m_stopIdRequested = false;

    // Get a source URL for the request
    KUrl url = departureUrl( requestInfo );
    kDebug() << "Using departure URL" << url;
    KIO::StoredTransferJob *job; // = KIO::storedGet( url, KIO::NoReload, KIO::HideProgressInfo );
    if ( m_info->attributesForDepatures()[QLatin1String("method")]
            .compare(QLatin1String("post"), Qt::CaseInsensitive) != 0 )
    {
        // Use GET to download the source document
        job = KIO::storedGet( url, KIO::NoReload, KIO::HideProgressInfo );
    } else if ( m_info->attributesForDepatures().contains(QLatin1String("data")) ) {
        // XML attribute "method" is present and it's value is "POST" for the <departures> tag
        // The XML attribute "data" is also present and is used as a template string for the data
        // to POST to the server
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
        if ( m_info->charsetForUrlEncoding().isEmpty() ) {
            sCity = QString::fromAscii( QUrl::toPercentEncoding(sCity) );
            sStop = QString::fromAscii( QUrl::toPercentEncoding(sStop) );
        } else {
            sCity = toPercentEncoding( sCity, m_info->charsetForUrlEncoding() );
            sStop = toPercentEncoding( sStop, m_info->charsetForUrlEncoding() );
        }

        // Construct the data
        QByteArray data;
        if ( m_info->useSeparateCityValue() ) {
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

        // Start the POST job and add meta data if special attributes are given
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

void TimetableAccessorOnline::requestJourneys( const JourneyRequestInfo &requestInfo )
{
    // Creating a kioslave
    KUrl url = !requestInfo.urlToUse.isEmpty() ? KUrl(requestInfo.urlToUse)
            : journeyUrl(requestInfo);
    KIO::StoredTransferJob *job = KIO::storedGet( url, KIO::NoReload, KIO::HideProgressInfo );
    connect( job, SIGNAL(result(KJob*)), this, SLOT(result(KJob*)) );

    m_jobInfos.insert( job, JobInfos(url, requestInfo.clone()) );
}

void TimetableAccessorOnline::requestStopSuggestions( const StopSuggestionRequestInfo &requestInfo )
{
    if ( !m_info->sessionKeyUrl().isEmpty() && m_sessionKey.isEmpty() &&
         m_sessionKeyGetTime.elapsed() > 500 )
    {
        kDebug() << "Request a session key";
        RequestInfo *newRequestInfo = requestInfo.clone();
        newRequestInfo->parseMode = ParseForSessionKeyThenStopSuggestions;
        requestSessionKey( newRequestInfo );
        return;
    }

    if ( hasSpecialUrlForStopSuggestions() ) {
        KUrl url = stopSuggestionsUrl( requestInfo );
        // TODO Use post-stuff also for journeys
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

void TimetableAccessorOnline::requestSessionKey( const RequestInfo *requestInfo )
{
    const KUrl url = m_info->sessionKeyUrl();
    KIO::StoredTransferJob *job = KIO::storedGet( url, KIO::NoReload, KIO::HideProgressInfo );
    m_jobInfos.insert( job, JobInfos(url, requestInfo->clone()) );
    connect( job, SIGNAL(result(KJob*)), this, SLOT(result(KJob*)) );
}

void TimetableAccessorOnline::clearSessionKey()
{
    m_sessionKey.clear();
}

KUrl TimetableAccessorOnline::departureUrl( const DepartureRequestInfo& requestInfo ) const
{
    QString sRawUrl = TimetableAccessor::departureUrl( requestInfo ).url();
    sRawUrl.replace( "{sessionKey}", m_sessionKey );
    return KUrl( sRawUrl );
}

void TimetableAccessorOnline::result( KJob* job )
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
        if ( parseDocumentForStopSuggestions(document, &stopList) ) {
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
        if ( parseDocumentForStopSuggestions(document, &stopList) ) {
            if ( stopList.isEmpty() ) {
                kDebug() << "No stop suggestions got to get an ID to use to get departures";
            } else {
                // Use the ID of the first suggested stop to get departures
                if ( !stopList.first()->contains(StopID) ) {
                    kDebug() << "No stop ID found for the given stop name, "
                                "now requesting departures using the stop name";
                    DepartureRequestInfo *newRequestInfo =
                            static_cast<DepartureRequestInfo*>( jobInfo.requestInfo.data() );
                    newRequestInfo->parseMode = ParseForDeparturesArrivals;
                    requestDepartures( *newRequestInfo );
                } else {
                    DepartureRequestInfo *newRequestInfo =
                            static_cast<DepartureRequestInfo*>( jobInfo.requestInfo.data() );
                    newRequestInfo->stop = stopList.first()->id();
                    newRequestInfo->parseMode = ParseForDeparturesArrivals;
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

            // Now request stop suggestions using the session key
            if ( parseDocumentMode == ParseForSessionKeyThenStopSuggestions ) {
                kDebug() << "Request stop suggestions using session key" << m_sessionKey;
                DepartureRequestInfo *newRequestInfo =
                        static_cast<DepartureRequestInfo*>( jobInfo.requestInfo.data() );
                newRequestInfo->parseMode = ParseForStopSuggestions;
                requestStopSuggestions( *newRequestInfo );
            } else if ( parseDocumentMode == ParseForSessionKeyThenDepartures ) {
                kDebug() << "Request departures/arrivals using session key" << m_sessionKey;
                DepartureRequestInfo *newRequestInfo =
                        static_cast<DepartureRequestInfo*>( jobInfo.requestInfo.data() );
                newRequestInfo->parseMode = ParseForDeparturesArrivals;
                requestDepartures( *newRequestInfo );
            }
            m_sessionKeyGetTime.start();

            // Clear the session key after a timeout
            QTimer::singleShot( 5 * 60000, this, SLOT(clearSessionKey()) );
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
        } else if ( parseDocumentForStopSuggestions(document, &stopList) ) {
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
    } else if ( parseDocumentForStopSuggestions(document, &stopList) ) {
        emit stopListReceived( this, jobInfo.url, stopList, jobInfo.requestInfo.data() );
    } else {
        kDebug() << "Error parsing for stop suggestions from different url" << jobInfo.requestInfo->sourceName;
        emit errorParsing( this, ErrorParsingFailed,
                           i18n("Error while parsing the stop suggestions document."),
                           jobInfo.url, jobInfo.requestInfo.data() );
    }
}
