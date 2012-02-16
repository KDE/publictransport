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
#include "scripting.h"

// Own includes
#include "timetableaccessor_script.h"
#include "timetableaccessor_info.h"
#include "departureinfo.h"

// KDE includes
#include <KStandardDirs>
#include <KConfig>
#include <KConfigGroup>
#include <KDebug>

// Qt includes
#include <QFile>
#include <QScriptContextInfo>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QTextCodec>
#include <QWaitCondition>
#include <QReadLocker>
#include <QWriteLocker>

NetworkRequest::NetworkRequest( QObject* parent )
        : QObject(parent), m_network(0), m_request(0), m_reply(0)
{
    kDebug() << "Create INVALID request";
}

NetworkRequest::NetworkRequest( const QString& url, Network *network, QObject* parent )
        : QObject(parent), m_url(url), m_network(network),
          m_request(new QNetworkRequest(url)), m_reply(0)
{
    kDebug() << "Create request" << url;
}

NetworkRequest::~NetworkRequest()
{
    kDebug() << "Delete reuqest for" << m_url;
    delete m_request;
    if ( m_reply ) {
        m_reply->deleteLater();
    }
}

void NetworkRequest::slotReadyRead()
{
    // Read all data, decode it and give it to the script
    QByteArray data = m_reply->readAll();
    m_data.append( data );

    QString string;
    if ( data.isEmpty() ) {
        kDebug() << "Error downloading" << m_url << m_reply->errorString();
    } else {
        string = TimetableAccessorScript::decodeHtml( data, m_network->fallbackCharset() );
    }

    emit readyRead( string );
}

void NetworkRequest::slotFinished()
{
    // Read all data, decode it and give it to the script
    QByteArray data = m_reply->readAll();
    m_data.append( data );

    QString string;
    if ( m_data.isEmpty() ) {
        kDebug() << "Error downloading" << m_url << m_reply->errorString();
    } else {
        string = TimetableAccessorScript::decodeHtml( m_data, m_network->fallbackCharset() );
    }
    m_reply->deleteLater();
    m_reply = 0;
    m_data.clear();

    emit finished( string );
}

void NetworkRequest::started( QNetworkReply* reply )
{
    if ( !m_network ) {
        kDebug() << "Can't decode, no m_network given...";
        return;
    }
    kDebug() << "REQUEST STARTED" << reply->isRunning();
    m_data.clear();
    m_reply = reply;

    // Connect to signals of reply only when the associated signals of this class are connected
    if ( receivers(SIGNAL(readyRead(QString))) > 0 ) {
        connect( m_reply, SIGNAL(readyRead()), this, SLOT(slotReadyRead()) );
    }
    if ( receivers(SIGNAL(finished(QString))) > 0 ) {
        connect( m_reply, SIGNAL(finished()), this, SLOT(slotFinished()) );
    }

    connect( m_reply, SIGNAL(finished()), this, SIGNAL(finishedNoDecoding()) );
}

bool NetworkRequest::isValid() const
{
    if ( m_request ) {
        return true;
    } else {
        // Default constructor used, not available to scripts
        kDebug() << "Request is invalid";
        return false;
    }
}

QByteArray NetworkRequest::getCharset( const QString& charset ) const
{
    QByteArray baCharset;
    if ( charset.isEmpty() ) {
        // No charset given, use the one specified in the ContentType header
        baCharset = m_request->header( QNetworkRequest::ContentTypeHeader ).toByteArray();

        // No ContentType header, use utf8
        if ( baCharset.isEmpty() ) {
            baCharset = "utf8";
        }
    } else {
        baCharset = charset.toUtf8();
    }
    return baCharset;
}

QByteArray NetworkRequest::postData() const
{
    return m_postData;
}

void NetworkRequest::setPostData( const QString& postData, const QString& charset )
{
    if ( !isValid() ) {
        return;
    }

    QByteArray baCharset = getCharset( charset );
//     if ( charset.compare(QLatin1String("utf8"), Qt::CaseInsensitive) == 0 ) {
//         m_request->setHeader( QNetworkRequest::ContentTypeHeader, "utf8" );
//         m_postData = postData.toUtf8();
//     } else {
        QTextCodec *codec = QTextCodec::codecForName( baCharset );
        if ( codec ) {
            m_request->setHeader( QNetworkRequest::ContentTypeHeader, baCharset );
            m_postData = codec->fromUnicode( postData );
        } else {
            kDebug() << "Codec" << baCharset << "couldn't be found to encode the data "
                    "to post, now using UTF-8";
            m_request->setHeader( QNetworkRequest::ContentTypeHeader, "utf8" );
            m_postData = postData.toUtf8();
        }
//     }
}

void NetworkRequest::setHeader( const QString& header, const QString& value,
                                const QString& charset )
{
    if ( !isValid() ) {
        return;
    }

    QByteArray baCharset = getCharset( charset );
    QTextCodec *codec = QTextCodec::codecForName( baCharset );
    if ( codec ) {
        m_request->setRawHeader( codec->fromUnicode(header), codec->fromUnicode(value) );
    } else {
        kDebug() << "Codec" << baCharset << "couldn't be found to encode the data "
                "to post, now using UTF-8";
        m_request->setRawHeader( header.toUtf8(), value.toUtf8() );
    }
}

QNetworkRequest* NetworkRequest::request() const
{
    return m_request;
}

    static int networkObjects = 0;
Network::Network( const QByteArray &fallbackCharset, QObject* parent )
        : QObject(parent), m_fallbackCharset(fallbackCharset),
          m_manager(new QNetworkAccessManager(this)), m_quit(false), m_lastDownloadAborted(false)
{
    ++networkObjects;
    kDebug() << "Create Network object" << thread() << networkObjects;
}

Network::~Network()
{
    // Quit event loop of possibly running synchronous requests
    m_mutex.lock();
    m_quit = true;
    m_mutex.unlock();

    emit aborted();

    --networkObjects;
    kDebug() << "DELETE Network object" << thread() << networkObjects;
    if ( !m_runningRequests.isEmpty() ) {
        kDebug() << "Deleting Network object with" << m_runningRequests.count()
                 << "running requests";
        qDeleteAll( m_runningRequests );
    }
}

NetworkRequest* Network::createRequest( const QString& url )
{
    return new NetworkRequest( url, this );
}

void Network::slotRequestFinished()
{
    NetworkRequest *request = qobject_cast< NetworkRequest* >( sender() );
    Q_ASSERT( request ); // This slot should only be connected to signals of NetworkRequest

    kDebug() << "Request finished" << request->url();
    m_runningRequests.removeOne( request );
    emit requestFinished( request );
}

bool Network::checkRequest( NetworkRequest* request )
{
    // Wrong argument type from script or no argument
    if ( !request ) {
        kDebug() << "Need a NetworkRequest object as argument, create it with "
                    "'network.createRequest(url)' or use network.getSynchronous(url, timeout)";
        return false;
    }

    // The same request can not be executed more than once at a time
    if ( request->isRunning() ) {
        kDebug() << "Request is currently running" << request->url();
        return false;
    }

    return request->isValid();
}

void Network::get( NetworkRequest* request )
{
    if ( !checkRequest(request) ) {
        return;
    }

    // Create a get request
    QNetworkReply *reply = m_manager->get( *request->request() );
    request->started( reply );
    m_lastUrl = request->url();
    m_lastDownloadAborted = false; // TODO store in NetworkRequest

    m_runningRequests << request;
    kDebug() << "GET DOCUMENT, now" << m_runningRequests.count() << "running requests" << request->url();
    connect( request, SIGNAL(finishedNoDecoding()), this, SLOT(slotRequestFinished()) );
}

void Network::head( NetworkRequest* request )
{
    if ( !checkRequest(request) ) {
        return;
    }

    // Create a head request
    QNetworkReply *reply = m_manager->head( *request->request() );
    request->started( reply );
    m_runningRequests << request;
    connect( request, SIGNAL(finishedNoDecoding()), this, SLOT(slotRequestFinished()) );
}

void Network::post( NetworkRequest* request )
{
    if ( !checkRequest(request) ) {
        return;
    }

    // Create a head request
    QNetworkReply *reply = m_manager->post( *request->request(), request->postData() );
    request->started( reply );
    m_runningRequests << request;
    connect( request, SIGNAL(finishedNoDecoding()), this, SLOT(slotRequestFinished()) );
}

void Network::abort()
{
    m_lastDownloadAborted = true;
    emit aborted();
}

QString Network::getSynchronous( const QString &url, int timeout )
{
    // Create a get request
    QNetworkRequest request( url );
    QNetworkReply *reply = m_manager->get( request );
    m_lastUrl = url;
    m_lastDownloadAborted = false;
    QTime start = QTime::currentTime();

    // Use an event loop to wait for execution of the request,
    // ie. make netAccess.download() synchronous for scripts
    QEventLoop eventLoop;
    connect( reply, SIGNAL(finished()), &eventLoop, SLOT(quit()) );
    connect( this, SIGNAL(aborted()), &eventLoop, SLOT(quit()) );
    if ( timeout > 0 ) {
        QTimer::singleShot( timeout, &eventLoop, SLOT(quit()) );
    }
    eventLoop.exec();

    m_mutex.lock();
    const bool quit = reply->isRunning() || m_quit || m_lastDownloadAborted;
    m_mutex.unlock();

    // Check if the timeout occured before the request finished
    if ( quit ) {
        kDebug() << "Cancelled, destroyed or timeout while downloading" << url;
        reply->abort();
        reply->deleteLater();
        return QString();
    }

    int time = start.msecsTo( QTime::currentTime() );
    kDebug() << "Waited" << ( time / 1000.0 ) << "seconds for download of" << url;

    // Read all data, decode it and give it to the script
    QByteArray data = reply->readAll();
    reply->deleteLater();
    if ( data.isEmpty() ) {
        kDebug() << "Error downloading" << url << reply->errorString();
        return QString();
    } else {
        return TimetableAccessorScript::decodeHtml( data, m_fallbackCharset );
    }
}

ResultObject::ResultObject( QObject* parent )
        : QObject(parent), m_features(AllFeatures), m_hints(NoHint)
{
}

bool ResultObject::isHintGiven( ResultObject::Hint hint ) const
{
    return m_hints.testFlag( hint );
}

void ResultObject::giveHint( ResultObject::Hint hint, bool enable )
{
    QMutexLocker locker( &m_mutex );
    if ( enable ) {
        m_hints |= hint;
    } else {
        m_hints &= ~hint;
    }
}

bool ResultObject::isFeatureEnabled( ResultObject::Feature feature ) const
{
    return m_features.testFlag( feature );
}

void ResultObject::enableFeature( ResultObject::Feature feature, bool enable )
{
    QMutexLocker locker( &m_mutex );
    if ( enable ) {
        m_features |= feature;
    } else {
        m_features &= ~feature;
    }
}

void ResultObject::dataList( const QList< TimetableData > &dataList,
                             PublicTransportInfoList *infoList, ParseDocumentMode parseMode,
                             VehicleType defaultVehicleType, const GlobalTimetableInfo *globalInfo,
                             ResultObject::Features features, ResultObject::Hints hints )
{                               // TODO TODO TODO TODO FEATURES TODO HINTS TODO TODO TODO TODO
    QDate curDate;
    QTime lastTime;
    int dayAdjustment = hints.testFlag(DatesNeedAdjustment)
            ? QDate::currentDate().daysTo(globalInfo->requestDate) : 0;
    if ( dayAdjustment != 0 ) {
        kDebug() << "Dates get adjusted by" << dayAdjustment << "days";
    }

    // Find words at the beginning/end of target and route stop names that have many
    // occurrences. These words are most likely the city names where the stops are in.
    // But the timetable becomes easier to read and looks nicer, if not each stop name
    // includes the same city name.
    QHash< QString, int > firstWordCounts; // Counts occurrences of words at the beginning
    QHash< QString, int > lastWordCounts; // Counts occurrences of words at the end

    // This is the range of occurrences of one word in stop names,
    // that causes that word to be removed
    const int minWordOccurrence = 10;
    const int maxWordOccurrence = 30;

    // This regular expression gets used to search for word at the end, possibly including
    // a colon before the last word
    QRegExp rxLastWord( ",?\\s+\\S+$" );

    // These strings store the words with the most occurrences in stop names at the beginning/end
    QString removeFirstWord;
    QString removeLastWord;

    // Read timetable data from the script
    for ( int i = 0; i < dataList.count(); ++i ) {
        TimetableData timetableData = dataList[ i ];

        // Set default vehicle type if none is set
        if ( !timetableData.contains(TypeOfVehicle) ||
             timetableData[TypeOfVehicle].toString().isEmpty() )
        {
            timetableData[ TypeOfVehicle ] = static_cast<int>( defaultVehicleType );
        }

        if ( parseMode != ParseForStopSuggestions ) {
            QDateTime dateTime = timetableData[ DepartureDateTime ].toDateTime();
            QDate departureDate = timetableData[ DepartureDate ].toDate();
            QTime departureTime = timetableData[ DepartureTime ].toTime();

            if ( !dateTime.isValid() && !departureTime.isValid() ) {
                kDebug() << "No departure time given!" << timetableData[DepartureTime];
                kDebug() << "Use eg. helper.matchTime() to convert a string to a time object";
            }

            if ( !dateTime.isValid() ) {
                if ( departureDate.isValid() ) {
                    dateTime = QDateTime( departureDate, departureTime );
                } else if ( curDate.isNull() ) {
                    // First departure
                    if ( QTime::currentTime().hour() < 3 && departureTime.hour() > 21 ) {
                        dateTime.setDate( QDate::currentDate().addDays(-1) );
                    } else if ( QTime::currentTime().hour() > 21 && departureTime.hour() < 3 ) {
                        dateTime.setDate( QDate::currentDate().addDays(1) );
                    } else {
                        dateTime.setDate( QDate::currentDate() );
                    }
                } else if ( lastTime.secsTo(departureTime) < -5 * 60 ) {
                    // Time too much ealier than last time, estimate it's tomorrow
                    dateTime.setDate( curDate.addDays(1) );
                } else {
                    dateTime.setDate( curDate );
                }
                timetableData[ DepartureDateTime ] = dateTime;
            }

            if ( dayAdjustment != 0 ) {
                dateTime.setDate( dateTime.date().addDays(dayAdjustment) );
                timetableData[ DepartureDateTime ] = dateTime;
            }
            curDate = dateTime.date();
            lastTime = departureTime;
        }

        // Create info object for the timetable data
        PublicTransportInfoPtr info;
        if ( parseMode == ParseForJourneys ) {
            info = QSharedPointer<JourneyInfo>( new JourneyInfo(timetableData) );
        } else if ( parseMode == ParseForDeparturesArrivals ) {
            info = QSharedPointer<DepartureInfo>( new DepartureInfo(timetableData) );
        } else if ( parseMode == ParseForStopSuggestions ) {
            info = QSharedPointer<StopInfo>( new StopInfo(timetableData) );
        }

        if ( !info->isValid() ) {
//             delete info;
            info.clear();
            continue;
        }

        // Find word to remove from beginning/end of stop names, if not already found
        // TODO Use hint from the data engine..
        if ( removeFirstWord.isEmpty() && removeLastWord.isEmpty() ) {
            // First count the first/last word of the target stop name
            const QString target = info->value( Target ).toString();
            int pos = target.indexOf( ' ' );
            if ( pos > 0 && ++firstWordCounts[target.left(pos)] >= maxWordOccurrence ) {
                removeFirstWord = target.left(pos);
            }
            if ( rxLastWord.indexIn(target) != -1 &&
                 ++lastWordCounts[rxLastWord.cap()] >= maxWordOccurrence )
            {
                removeLastWord = rxLastWord.cap();
            }

            // Check if route stop names are available
            if ( info->contains(RouteStops) ) {
                QStringList stops = info->value( RouteStops ).toStringList();
                QString target = info->value( Target ).toString();

                // Break if 70% or at least three of the route stop names
                // begin/end with the same word
                int minCount = qMax( 3, int(stops.count() * 0.7) );
                foreach ( const QString &stop, stops ) {
                    // Test first word
                    pos = stop.indexOf( ' ' );
                    const QString newFirstWord = stop.left( pos );
                    if ( pos > 0 && ++firstWordCounts[newFirstWord] >= maxWordOccurrence ) {
                        removeFirstWord = newFirstWord;
                        break;
                    }

                    // Test last word
                    if ( rxLastWord.indexIn(stop) != -1 &&
                         ++lastWordCounts[rxLastWord.cap()] >= maxWordOccurrence )
                    {
                        removeLastWord = rxLastWord.cap();
                        break;
                    }
                }
            }
        }

        infoList->append( info );
    }

    // Remove word with most occurrences from beginning/end of stop names
    if ( removeFirstWord.isEmpty() && removeLastWord.isEmpty() ) {
        // If no first/last word with at least maxWordOccurrence occurrences was found,
        // find the word with the most occurrences
        int max = 0;

        // Find word at the beginning with most occurrences
        for ( QHash< QString, int >::ConstIterator it = firstWordCounts.constBegin();
              it != firstWordCounts.constEnd(); ++it )
        {
            if ( it.value() > max ) {
                max = it.value();
                removeFirstWord = it.key();
            }
        }

        // Find word at the end with more occurrences
        for ( QHash< QString, int >::ConstIterator it = lastWordCounts.constBegin();
              it != lastWordCounts.constEnd(); ++it )
        {
            if ( it.value() > max ) {
                max = it.value();
                removeLastWord = it.key();
            }
        }

        if ( max < minWordOccurrence ) {
            // The first/last word with the most occurrences has too few occurrences
            // Do not remove any word
            removeFirstWord.clear();
            removeLastWord.clear();
        } else if ( !removeLastWord.isEmpty() ) {
            // removeLastWord has more occurrences than removeFirstWord
            removeFirstWord.clear();
        }
    }

    if ( !removeFirstWord.isEmpty() ) {
        // Remove removeFirstWord from all stop names
        for ( int i = 0; i < infoList->count(); ++i ) {
            QSharedPointer<PublicTransportInfo> info = infoList->at( i );
            QString target = info->value( Target ).toString();
            if ( target.startsWith(removeFirstWord) ) {
                target = target.mid( removeFirstWord.length() + 1 );
                info->insert( TargetShortened, target );
            }

            QStringList stops = info->value( RouteStops ).toStringList();
            for ( int i = 0; i < stops.count(); ++i ) {
                if ( stops[i].startsWith(removeFirstWord) ) {
                    stops[i] = stops[i].mid( removeFirstWord.length() + 1 );
                }
            }
            info->insert( RouteStopsShortened, stops );
        }
    } else if ( !removeLastWord.isEmpty() ) {
        // Remove removeLastWord from all stop names
        for ( int i = 0; i < infoList->count(); ++i ) {
            QSharedPointer<PublicTransportInfo> info = infoList->at( i );
            QString target = info->value( Target ).toString();
            if ( target.endsWith(removeLastWord) ) {
                target = target.left( target.length() - removeLastWord.length() );
                info->insert( TargetShortened, target );
            }

            QStringList stops = info->value( RouteStops ).toStringList();
            for ( int i = 0; i < stops.count(); ++i ) {
                if ( stops[i].endsWith(removeLastWord) ) {
                    stops[i] = stops[i].left( stops[i].length() - removeLastWord.length() );
                }
            }
            info->insert( RouteStopsShortened, stops );
        }
    }
}

QString Helper::decodeHtmlEntities( const QString& html )
{
    return TimetableAccessorScript::decodeHtmlEntities( html );
}

void Helper::error( const QString& message, const QString &failedParseText )
{
    // Output debug message and a maximum count of 200 characters of the text where the parsing failed
    QString shortParseText = failedParseText.trimmed().left(350);
    int diff = failedParseText.length() - shortParseText.length();
    if ( diff > 0 ) {
        shortParseText.append(QString("... <%1 more chars>").arg(diff));
    }
    shortParseText = shortParseText.replace('\n', "\n    "); // Indent

    QScriptContextInfo info( context()->parentContext() );
    kDebug() << QString("Error in %1-script, function %2(), line %3")
            .arg(m_serviceProviderId).arg(info.functionName()).arg(info.lineNumber());
    kDebug() << message;
    if ( !shortParseText.isEmpty() ) {
        kDebug() << QString("The text of the document where parsing failed is: \"%1\"")
                    .arg(shortParseText);
    }

    // Log the complete message to the log file.
    QString logFileName = KGlobal::dirs()->saveLocation( "data", "plasma_engine_publictransport" );
    logFileName.append( "accessors.log" );

    if ( !logFileName.isEmpty() ) {
        QFile logFile(logFileName);
        if ( logFile.size() > 1024 * 512 ) { // == 0.5 MB
            if ( !logFile.remove() ) {
                kDebug() << "Error: Couldn't delete old log file.";
            } else {
                kDebug() << "Deleted old log file, because it was getting too big.";
            }
        }

        if ( !logFile.open(QIODevice::Append | QIODevice::Text) ) {
            kDebug() << "Couldn't open the log file in append mode" << logFileName << logFile.errorString();
            return;
        }

        logFile.write( QString("%1 (%2, in function %3(), line %4):\n   \"%5\"\n   Failed while reading this text: \"%6\"\n-------------------------------------\n\n")
                .arg(m_serviceProviderId).arg(QDateTime::currentDateTime().toString())
                .arg(info.functionName()).arg(info.lineNumber())
                .arg(message).arg(failedParseText.trimmed()).toUtf8() );
        logFile.close();
    }
}

void ResultObject::addData( const QVariantMap& map )
{
    QMutexLocker locker( &m_mutex );
    TimetableData data;
    for ( QVariantMap::ConstIterator it = map.constBegin(); it != map.constEnd(); ++it ) {
        const TimetableInformation info =
                TimetableAccessor::timetableInformationFromString( it.key() );
        const QVariant value = it.value();
        if ( info == Nothing ) {
            kDebug() << "Unknown timetable information" << it.key() << "with value" << value;
            continue;
        }
        if ( !value.isValid() || value.isNull() ) {
            kDebug() << "Value for" << info << "is invalid or null" << value;
            continue;
        }

        if ( m_features.testFlag(AutoDecodeHtmlEntities) ) {
            if ( value.canConvert(QVariant::String) &&
                 (info == StopName || info == Target || info == StartStopName ||
                  info == TargetStopName || info == Operator || info == TransportLine ||
                  info == Platform || info == DelayReason || info == Status || info == Pricing) )
            {
                // Decode HTML entities in string values
                data[ info ] = TimetableAccessorScript::decodeHtmlEntities( value.toString() ).trimmed();
            } else if ( value.canConvert(QVariant::StringList) &&
                 (info == RouteStops || info == RoutePlatformsDeparture ||
                  info == RoutePlatformsArrival) )
            {
                // Decode HTML entities in string list values
                QStringList stops = value.toStringList();
                for ( QStringList::Iterator it = stops.begin(); it != stops.end(); ++it ) {
                    *it = Helper::trim( TimetableAccessorScript::decodeHtmlEntities(*it) );
                }
                data[ info ] = stops;
            } else {
                // Other values don't need decoding
                data[ info ] = value;
            }
        } else {
            data[ info ] = value;
        }
    }
    m_timetableData << data;

    if ( m_features.testFlag(AutoPublish) && m_timetableData.count() == 10 ) {
        // Publish the first 10 data items automatically
        emit publish();
    }
}

QString Helper::trim( const QString& str )
{
    return QString(str).trimmed()
                       .replace( QRegExp("^(&nbsp;)+|(&nbsp;)+$", Qt::CaseInsensitive), QString() )
                       .trimmed();
}

QString Helper::stripTags( const QString& str )
{
    QRegExp rx( "<\\/?[^>]+>" );
    rx.setMinimal( true );
    return QString( str ).replace( rx, "" );
}

QString Helper::camelCase( const QString& str )
{
    QString ret = str.toLower();
    QRegExp rx( "(^\\w)|\\W(\\w)" );
    int pos = 0;
    while (( pos = rx.indexIn( ret, pos ) ) != -1 ) {
        if ( rx.pos( 2 ) < 0 || rx.pos( 2 ) >= ret.length() ) { // TODO
            break;
        }
        ret[ rx.pos( 2 )] = ret[ rx.pos( 2 )].toUpper();
        pos += rx.matchedLength();
    }
    return ret;
}

QString Helper::extractBlock( const QString& str, const QString& beginString,
                              const QString& endString )
{
    int pos = str.indexOf( beginString );
    if ( pos == -1 ) {
        return "";
    }

    int end = str.indexOf( endString, pos + 1 );
    return str.mid( pos, end - pos );
}

QVariantMap Helper::matchTime( const QString& str, const QString& format )
{
    QString pattern = QRegExp::escape( format );
    pattern = pattern.replace( "hh", "\\d{2}" )
              .replace( "h", "\\d{1,2}" )
              .replace( "mm", "\\d{2}" )
              .replace( "m", "\\d{1,2}" )
              .replace( "AP", "(AM|PM)" )
              .replace( "ap", "(am|pm)" );

    QVariantMap ret;
    QRegExp rx( pattern );
    if ( rx.indexIn(str) != -1 ) {
        QTime time = QTime::fromString( rx.cap(), format );
        ret.insert( "hour", time.hour() );
        ret.insert( "minute", time.minute() );
    } else if ( format != "hh:mm" ) {
        // Try default format if the one specified doesn't work
        QRegExp rx2( "\\d{1,2}:\\d{2}" );
        if ( rx2.indexIn(str) != -1 ) {
            QTime time = QTime::fromString( rx2.cap(), "hh:mm" );
            ret.insert( "hour", time.hour() );
            ret.insert( "minute", time.minute() );
        } else {
            ret.insert( "error", true );
            kDebug() << "Couldn't match time in" << str << pattern;
        }
    } else {
        ret.insert( "error", true );
        kDebug() << "Couldn't match time in" << str << pattern;
    }
    return ret;
}

QDate Helper::matchDate( const QString& str, const QString& format )
{
    QString pattern = QRegExp::escape( format ).replace( "d", "D" );
    pattern = pattern.replace( "DD", "\\d{2}" )
              .replace( "D", "\\d{1,2}" )
              .replace( "MM", "\\d{2}" )
              .replace( "M", "\\d{1,2}" )
              .replace( "yyyy", "\\d{4}" )
              .replace( "yy", "\\d{2}" );

    QRegExp rx( pattern );
    QDate date;
    if ( rx.indexIn(str) != -1 ) {
        date = QDate::fromString( rx.cap(), format );
    } else if ( format != "yyyy-MM-dd" ) {
        // Try default format if the one specified doesn't work
        QRegExp rx2( "\\d{2,4}-\\d{2}-\\d{2}" );
        if ( rx2.indexIn(str) != -1 ) {
            date = QDate::fromString( rx2.cap(), "yyyy-MM-dd" );
        } else {
            kDebug() << "Couldn't match date in" << str << pattern;
        }
    } else {
        kDebug() << "Couldn't match date in" << str << pattern;
    }

    // Adjust date, needed for formats with only two "yy" for year matching
    // A year 12 means 2012, not 1912
    if ( date.year() < 1970 ) {
        date.setDate( date.year() + 100, date.month(), date.day() );
    }
    return date;
}

QString Helper::formatTime( int hour, int minute, const QString& format )
{
    return QTime( hour, minute ).toString( format );
}

QString Helper::formatDate( int year, int month, int day, const QString& format )
{
    return QDate( year, month, day ).toString( format );
}

QString Helper::formatDateTime( const QDateTime& dateTime, const QString& format )
{
    return dateTime.toString( format );
}

int Helper::duration( const QString& sTime1, const QString& sTime2, const QString& format )
{
    QTime time1 = QTime::fromString( sTime1, format );
    QTime time2 = QTime::fromString( sTime2, format );
    if ( !time1.isValid() || !time2.isValid() ) {
        return -1;
    }
    return time1.secsTo( time2 ) / 60;
}

QString Helper::addMinsToTime( const QString& sTime, int minsToAdd, const QString& format )
{
    QTime time = QTime::fromString( sTime, format );
    if ( !time.isValid() ) {
        kDebug() << "Couldn't parse the given time" << sTime << format;
        return "";
    }
    return time.addSecs( minsToAdd * 60 ).toString( format );
}

QString Helper::addDaysToDate( const QString& sDate, int daysToAdd, const QString& format )
{
    QDate date = QDate::fromString( sDate, format ).addDays( daysToAdd );
    if ( !date.isValid() ) {
        kDebug() << "Couldn't parse the given date" << sDate << format;
        return sDate;
    }
    return date.toString( format );
}

QDateTime Helper::addDaysToDate( const QDateTime& dateTime, int daysToAdd )
{
    return dateTime.addDays( daysToAdd );
}

QVariantList Helper::addDaysToDateArray( const QVariantList& values, int daysToAdd )
{
    if ( values.count() != 3 ) {
        kDebug() << "The first argument needs to be a list with three values (year, month, day)";
        return values;
    }

    QDate date( values[0].toInt(), values[1].toInt(), values[2].toInt() );
    date = date.addDays( daysToAdd );
    return QVariantList() << date.year() << date.month() << date.day();
}

QStringList Helper::splitSkipEmptyParts( const QString& str, const QString& sep )
{
    return str.split( sep, QString::SkipEmptyParts );
}

QVariantMap Helper::findTableHeaderPositions( const QString &str, const QVariantMap &options )
{
    QVariantMap headerContainerOptions =
            options.value( "headerContainerOptions", QVariantMap() ).toMap();
    QVariantMap headerOptions =
            options.value( "headerOptions", QVariantMap() ).toMap();
    const bool debug = options.value( "debug", false ).toBool();

    // Ensure some options are present
    if ( !headerContainerOptions.contains("tagName") ) {
        headerContainerOptions[ "tagName" ] = "tr";
    }
    if ( !headerOptions.contains("tagName") ) {
        headerOptions[ "tagName" ] = "th";
    }
    QVariantMap namePosition;
    if ( !headerOptions.contains("namePosition") ) {
        namePosition[ "type" ] = "contents"; // Can be "attribute", "contents"
        headerOptions[ "namePosition" ] = namePosition;
    } else {
        namePosition = headerOptions[ "namePosition" ].toMap();
    }
    const bool namePositionIsAttribute = namePosition["type"].toString().toLower().compare(
            QLatin1String("attribute"), Qt::CaseInsensitive ) == 0;
    const QString namePositionRegExpPattern = namePosition.contains("regexp")
            ? namePosition["regexp"].toString() : QString();

    const QVariantMap headerContainer = findFirstHtmlTag( str, headerContainerOptions );
    if ( !headerContainer["found"].toBool() ) {
        kDebug() << "Did not find a header container row"
                 << QString("<%1..>..<%2..>..</%2>..</%1>")
                    .arg( headerContainerOptions["tagName"].toString() )
                    .arg( headerOptions["tagName"].toString() )
                 << "in" << str;
        return QVariantMap();
    }

    QVariantMap headerPositions;
    int i = 0;
    QStringList requiredHeaders = options[ "required" ].toStringList();
    QStringList optionalHeaders = options[ "optional" ].toStringList();
    const QString headerContainerContents = headerContainer["contents"].toString();
    QVariantMap headerTag;
    while ( (headerTag = findFirstHtmlTag(headerContainerContents, headerOptions))["found"].toBool() )
    {
        headerOptions["position"] = headerTag["position"].toInt() + 1;
        const QString headerName = getTagName( headerTag, namePosition["type"].toString(),
                namePositionRegExpPattern,
                namePositionIsAttribute ? namePosition["name"].toString() : QString() );
        if ( headerName.isEmpty() ) {
            kDebug() << "Empty header name" << str;
            ++i;
            continue;
        }
        const bool foundRequiredHeader = requiredHeaders.contains( headerName, Qt::CaseInsensitive );
        const bool foundOptionalHeader = optionalHeaders.contains( headerName, Qt::CaseInsensitive );
        if ( !foundRequiredHeader && !foundOptionalHeader ) {
            kDebug() << "Unused timetable header found" << headerName;
            ++i;
            continue;
        }

        // Store found column position
        if ( debug ) {
            kDebug() << "Found header" << headerName << "at position" << i;
        }
        headerPositions[ headerName.toLower() ] = i;

        // Remove column from required/optional header list
        if ( foundRequiredHeader ) {
            requiredHeaders.removeOne( headerName );
        }

        ++i;
    }

    if ( !requiredHeaders.isEmpty() ) {
        kDebug() << "Did not find all required headers" << requiredHeaders;
        headerPositions[ "error" ] = true;
    }

    return headerPositions;
}

QVariantMap Helper::findFirstHtmlTag( const QString &str, const QString &tagName,
                                      const QVariantMap &options )
{
    // Set/overwrite maxCount option to match only the first tag using findHtmlTags()
    QVariantMap _options = options;
    _options[ "maxCount" ] = 1;
    QVariantList tags = findHtmlTags( str, tagName, _options );

    // Copy values of first matched tag (if any) to the result object
    QVariantMap result;
    result.insert( "found", !tags.isEmpty() );
    if ( !tags.isEmpty() ) {
        const QVariantMap firstTag = tags.first().toMap();
        result.insert( "contents", firstTag["contents"] );
        result.insert( "position", firstTag["position"] );
        result.insert( "endPosition", firstTag["endPosition"] );
        result.insert( "attributes", firstTag["attributes"] );
        result.insert( "name", firstTag["name"] );
    }
    return result;
}

QVariantList Helper::findHtmlTags( const QString &str, const QString &tagName,
                                   const QVariantMap &options )
{
    const QVariantMap &attributes = options[ "attributes" ].toMap();
    const int maxCount = options.value( "maxCount", 0 ).toInt();
    const bool noContent = options.value( "noContent", false ).toBool();
    const bool noNesting = options.value( "noNesting", false ).toBool();
    const bool debug = options.value( "debug", false ).toBool();
    const QString contentsRegExpPattern = options.value( "contentsRegExp", "\\s*(.*)\\s*" ).toString();
    const QVariantMap namePosition = options[ "namePosition" ].toMap();
    int position = options.value( "position", 0 ).toInt();

    const bool namePositionIsAttribute = namePosition["type"].toString().toLower().compare(
            QLatin1String("attribute"), Qt::CaseInsensitive ) == 0;
    const QString namePositionRegExpPattern = namePosition.contains("regexp")
            ? namePosition["regexp"].toString() : QString();

    // Create regular expression that matches HTML elements with or without attributes.
    // Since QRegExp offers no way to retreive multiple matches of the same capture group
    // the whole attribute string gets matched here and then analyzed in another loop
    // using attributeRegExp.
    // Matching the attributes with all details here is required to prevent eg. having a match
    // end after a ">" character in a string in an attribute.
    const QString attributePattern = "\\w+(?:\\s*=\\s*(?:\"[^\"]*\"|'[^']*'|[^\"'>\\s]+))?";
    QRegExp htmlTagRegExp( noContent
            ? QString("<%1((?:\\s+%2)*)(?:\\s*/)?>").arg(tagName).arg(attributePattern)
            : QString("<%1((?:\\s+%2)*)>").arg(tagName).arg(attributePattern),
//             : QString("<%1((?:\\s+%2)*)>%3</%1\\s*>").arg(tagName).arg(attributePattern)
//                     .arg(contentsRegExpPattern),
            Qt::CaseInsensitive );
    QRegExp htmlCloseTagRegExp( QString("</%1\\s*>").arg(tagName), Qt::CaseInsensitive );
    QRegExp contentsRegExp( contentsRegExpPattern, Qt::CaseInsensitive );
    htmlTagRegExp.setMinimal( true );

    // Match attributes with or without value, with single/double/not quoted value
    QRegExp attributeRegExp( "(\\w+)(?:\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\"'>\\s]+)))?",
                             Qt::CaseInsensitive );

    QVariantList foundTags;
    while ( (foundTags.count() < maxCount || maxCount <= 0) &&
            (position = htmlTagRegExp.indexIn(str, position)) != -1 )
    {
        if ( debug ) {
            kDebug() << "Test match at" << position << htmlTagRegExp.cap().left(500);
        }
        const QString attributeString = htmlTagRegExp.cap( 1 );
        QString tagContents; // = noContent ? QString() : htmlTagRegExp.cap( 2 );

        QVariantMap foundAttributes;
        int attributePos = 0;
        while ( (attributePos = attributeRegExp.indexIn(attributeString, attributePos)) != -1 ) {
            foundAttributes.insert( attributeRegExp.cap(1), attributeRegExp.cap(2) );
            attributePos += attributeRegExp.matchedLength();
        }
        if ( debug ) {
            kDebug() << "Found attributes" << foundAttributes << "in" << attributeString;
        }

        // Test if the attributes match
        bool attributesMatch = true;
        for ( QVariantMap::ConstIterator it = attributes.constBegin();
              it != attributes.constEnd(); ++it )
        {
            if ( !foundAttributes.contains(it.key()) ) {
                // Did not find exact attribute name, try to use it as regular expression pattern
                attributesMatch = false;
                QRegExp attributeNameRegExp( it.key(), Qt::CaseInsensitive );
                foreach ( const QString &attributeName, foundAttributes.keys() ) {
                    if ( attributeNameRegExp.indexIn(attributeName) != -1 ) {
                        // Matched the attribute name
                        attributesMatch = true;
                        break;
                    }
                }

                if ( !attributesMatch ) {
                    if ( debug ) {
                        kDebug() << "Did not find attribute" << it.key();
                    }
                    break;
                }
            }

            // Attribute exists, test it's value
            const QString value = foundAttributes[ it.key() ].toString();
            const QString valueRegExpPattern = it.value().toString();
            if ( !(value.isEmpty() && valueRegExpPattern.isEmpty()) ) {
                QRegExp valueRegExp( valueRegExpPattern, Qt::CaseInsensitive );
                if ( valueRegExp.indexIn(value) == -1 ) {
                    // Attribute value regexp did not matched
                    attributesMatch = false;
                    if ( debug ) {
                        kDebug() << "Value" << value << "did not match pattern" << valueRegExpPattern;
                    }
                    break;
                } else if ( valueRegExp.captureCount() > 0 ) {
                    // Attribute value regexp matched, store captures
                    foundAttributes[ it.key() ] = valueRegExp.capturedTexts();
                }
            }
        }
        if ( !attributesMatch ) {
            position = htmlTagRegExp.pos() + htmlTagRegExp.matchedLength();
            continue;
        }

        // Search for new opening HTML tags (with same tag name) before the closing HTML tag
        int endPosition = htmlTagRegExp.pos() + htmlTagRegExp.matchedLength();
        if ( !noContent ) {
            if ( noNesting ) {
                // "noNesting" option set, simply search for next closing tag, no matter if it is
                // a nested tag or not
                const int posClosing = htmlCloseTagRegExp.indexIn( str, endPosition );
                const int contentsBegin = htmlTagRegExp.pos() + htmlTagRegExp.matchedLength();
                tagContents = str.mid( contentsBegin, posClosing - contentsBegin );
                endPosition = htmlCloseTagRegExp.pos() + htmlCloseTagRegExp.matchedLength();
            } else {
                // Find next closing tag, skipping nested tags.
                // Get string after the opening HTML tag
                const QString rest = str.mid( htmlTagRegExp.pos() + htmlTagRegExp.matchedLength() );

                int posClosing = htmlCloseTagRegExp.indexIn( rest );
                if ( posClosing == -1 ) {
                    position = htmlTagRegExp.pos() + htmlTagRegExp.matchedLength();
                    if ( debug ) {
                        kDebug() << "Closing tag" << tagName << "could not be found";
                    }
                    continue;
                }

                // Search for nested opening tags in between the main opening tag and the
                // next closing tag
                int posOpening = htmlTagRegExp.indexIn( rest.left(posClosing) );
                while ( posOpening != -1 ) {
                    // Found a nested tag, find the next closing tag
                    posClosing = htmlCloseTagRegExp.indexIn( rest,
                            posClosing + htmlCloseTagRegExp.matchedLength() );
                    if ( posClosing == -1 ) {
                        position = htmlTagRegExp.pos() + htmlTagRegExp.matchedLength();
                        if ( debug ) {
                            kDebug() << "Closing tag" << tagName << "could not be found";
                        }
                        break;
                    }

                    // Search for more nested opening tags
                    posOpening = htmlTagRegExp.indexIn( rest.left(posClosing),
                            posOpening + htmlTagRegExp.matchedLength() );
                }

                tagContents = rest.left( posClosing );
                endPosition += htmlCloseTagRegExp.pos() + htmlCloseTagRegExp.matchedLength();
            }
        }

        // Match contents
        if ( contentsRegExp.indexIn(tagContents) == -1 ) {
            if ( debug ) {
                kDebug() << "Did not match tag contents" << tagContents.left(500);
            }
            continue;
        } else {
            // Use first matched group as contents string, if any. Otherwise use the whole match
            // as contents string.
            tagContents = contentsRegExp.cap( contentsRegExp.captureCount() <= 1 ? 0 : 1 );
        }

        // Construct a result object
        QVariantMap result;
        result.insert( "contents", tagContents );
        result.insert( "position", position );
        result.insert( "endPosition", endPosition );
        result.insert( "attributes", foundAttributes );

        // Find name if a "namePosition" option is given
        if ( !namePosition.isEmpty() ) {
            const QString name = getTagName( result, namePosition["type"].toString(),
                    namePositionRegExpPattern,
                    namePositionIsAttribute ? namePosition["name"].toString() : QString() );
            result.insert( "name", name );
        }

        if ( debug ) {
            kDebug() << "Found HTML tag" << tagName << "at" << position << foundAttributes;
        }
        foundTags << result;
        position = endPosition;
    }

    if ( debug ) {
        kDebug() << "Found" << foundTags.count() << tagName << "HTML tags";
    }
    return foundTags;
}

QString Helper::getTagName( const QVariantMap &searchResult, const QString &type,
                            const QString &regExp, const QString attributeName )
{
    const bool namePositionIsAttribute = type.toLower().compare(
            QLatin1String("attribute"), Qt::CaseInsensitive ) == 0;
    QString name = trim( namePositionIsAttribute
            ? searchResult["attributes"].toMap()[ attributeName ].toString()
            : searchResult["contents"].toString() );
    if ( !regExp.isEmpty() ) {
        // Use "regexp" property of namePosition to match the header name
        QRegExp namePositionRegExp( regExp, Qt::CaseInsensitive );
        if ( namePositionRegExp.indexIn(name) != -1 ) {
            name = namePositionRegExp.cap( qMin(1, namePositionRegExp.captureCount()) );
        }
    }
    return name;
}

QVariantMap Helper::findNamedHtmlTags( const QString &str, const QString &tagName,
                                       const QVariantMap &options )
{
    QVariantMap namePosition;
    if ( !options.contains("namePosition") ) {
        namePosition[ "type" ] = "contents"; // Can be "attribute", "contents"
    } else {
        namePosition = options[ "namePosition" ].toMap();
    }
    const bool namePositionIsAttribute = namePosition["type"].toString().toLower().compare(
            QLatin1String("attribute"), Qt::CaseInsensitive ) == 0;
    const QString namePositionRegExpPattern = namePosition.contains("regexp")
            ? namePosition["regexp"].toString() : QString();
    const QString ambiguousNameResolution = options.contains("ambiguousNameResolution")
            ? options["ambiguousNameResolution"].toString().toLower() : "replace";

    const QVariantList foundTags = findHtmlTags( str, tagName, options );
    QVariantMap foundTagsMap;
    foreach ( const QVariant &foundTag, foundTags ) {
        QString name = getTagName( foundTag.toMap(), namePosition["type"].toString(),
                namePositionRegExpPattern,
                namePositionIsAttribute ? namePosition["name"].toString() : QString() );
        if ( name.isEmpty() ) {
            kDebug() << "Empty name in" << str;
            continue;
        }

        // Check if the newly found name was already found
        // and decide what to do based on the "ambiguousNameResolution" option
        if ( ambiguousNameResolution == QLatin1String("addnumber") && foundTagsMap.contains(name) ) {
            QRegExp rx( "(\\d+)$" );
            if ( rx.indexIn(name) != -1 ) {
                name += QString::number( rx.cap(1).toInt() + 1 );
            } else {
                name += "2";
            }
        }
        foundTagsMap[ name ] = foundTag; // TODO Use lists here? The same name could be found multiply times
    }

    // Store list of names in the "names" property, therefore "names" should not be a found tag name
    if ( foundTagsMap.contains("names") ) {
        kDebug() << "A tag with the name 'names' was found. Normally a property 'names' gets "
                    "added to the object returned by this functionm, which lists all found "
                    "names in a list.";
    } else {
        foundTagsMap[ "names" ] = QVariant::fromValue<QStringList>( foundTagsMap.keys() );
    }
    return foundTagsMap;
}

const char* Storage::LIFETIME_ENTRYNAME_SUFFIX = "__expires__";

Storage::Storage( const QString &serviceProvider, QObject* parent )
        : QObject(parent), m_readWriteLock(new QReadWriteLock),
          m_serviceProvider(serviceProvider), m_lastLifetimeCheck(0)
{
    // Delete persistently stored data which lifetime has expired
    checkLifetime();
}

Storage::~Storage()
{
    delete m_readWriteLock;
}

void Storage::write( const QVariantMap& data )
{
    for ( QVariantMap::ConstIterator it = data.constBegin(); it != data.constEnd(); ++it ) {
        write( it.key(), it.value() );
    }
}

void Storage::write( const QString& name, const QVariant& data )
{
    QWriteLocker locker( m_readWriteLock );
    m_data.insert( name, data );
}

QVariantMap Storage::read()
{
    QReadLocker locker( m_readWriteLock );
    return m_data;
}

QVariant Storage::read( const QString& name, const QVariant& defaultData )
{
    QReadLocker locker( m_readWriteLock );
    return m_data.contains(name) ? m_data[name] : defaultData;
}

void Storage::remove( const QString& name )
{
    QWriteLocker locker( m_readWriteLock );
    m_data.remove( name );
}

void Storage::clear()
{
    QWriteLocker locker( m_readWriteLock );
    m_data.clear();
}

int Storage::lifetime( const QString& name )
{
    const QString fileName = KGlobal::dirs()->saveLocation("data",
            "plasma_engine_publictransport/").append( QLatin1String("datacache"));
    KConfig cfg( fileName, KConfig::SimpleConfig );
    KConfigGroup group = cfg.group( m_serviceProvider ).group( QLatin1String("storage") );
    return lifetime( name, group );
}

int Storage::lifetime( const QString& name, const KConfigGroup& group )
{
    QReadLocker locker( m_readWriteLockPersistent );
    const uint lifetimeTime_t = group.readEntry( name + LIFETIME_ENTRYNAME_SUFFIX, 0 );
    return QDateTime::currentDateTime().daysTo( QDateTime::fromTime_t(lifetimeTime_t) );
}

void Storage::checkLifetime()
{
    if ( QDateTime::currentDateTime().toTime_t() - m_lastLifetimeCheck <
         MIN_LIFETIME_CHECK_INTERVAL * 60 )
    {
        // Last lifetime check was less than 15 minutes ago
        return;
    }

    // Try to load script features from a cache file
    // TODO: Use TimetableAccessor::accessorCacheFileName() from GTFS branch
    const QString fileName = KGlobal::dirs()->saveLocation("data",
            "plasma_engine_publictransport/").append( QLatin1String("datacache"));
    KConfig cfg( fileName, KConfig::SimpleConfig );
    KConfigGroup group = cfg.group( m_serviceProvider ).group( QLatin1String("storage") );
    QMap< QString, QString > data = group.entryMap();
    for ( QMap< QString, QString >::ConstIterator it = data.constBegin();
          it != data.constEnd(); ++it )
    {
        const int remainingLifetime = lifetime( it.key(), group );
        if ( remainingLifetime <= 0 ) {
            // Lifetime has expired
            kDebug() << "Lifetime of storage data" << it.key() << "for" << m_serviceProvider
                     << "has expired" << remainingLifetime;
            removePersistent( it.key(), group );
        }
    }

    m_lastLifetimeCheck = QDateTime::currentDateTime().toTime_t();
}

void Storage::writePersistent( const QVariantMap& data, uint lifetime )
{
    QWriteLocker locker( m_readWriteLockPersistent );
    for ( QVariantMap::ConstIterator it = data.constBegin(); it != data.constEnd(); ++it ) {
        writePersistent( it.key(), it.value(), lifetime );
    }
}

void Storage::writePersistent( const QString& name, const QVariant& data, uint lifetime )
{
    if ( lifetime > MAX_LIFETIME ) {
        lifetime = MAX_LIFETIME;
    }

    // Try to load script features from a cache file
    // TODO: Use TimetableAccessor::accessorCacheFileName() from GTFS branch
    const QString fileName = KGlobal::dirs()->saveLocation("data",
            "plasma_engine_publictransport/").append( QLatin1String("datacache"));
    KConfig cfg( fileName, KConfig::SimpleConfig );
    KConfigGroup group = cfg.group( m_serviceProvider ).group( QLatin1String("storage") );

    QWriteLocker locker( m_readWriteLockPersistent );
    group.writeEntry( name + LIFETIME_ENTRYNAME_SUFFIX,
                      QDateTime::currentDateTime().addDays(lifetime).toTime_t() );
    group.writeEntry( name, data );
}

QVariant Storage::readPersistent( const QString& name, const QVariant& defaultData )
{
    // Try to load script features from a cache file
    // TODO: Use TimetableAccessor::accessorCacheFileName() from GTFS branch
    const QString fileName = KGlobal::dirs()->saveLocation("data",
            "plasma_engine_publictransport/").append( QLatin1String("datacache"));
    KConfig cfg( fileName, KConfig::SimpleConfig );
    KConfigGroup grp = cfg.group( m_serviceProvider ).group( QLatin1String("storage") );

    QReadLocker locker( m_readWriteLockPersistent );
    return grp.readEntry( name, defaultData );
}

void Storage::removePersistent( const QString& name, KConfigGroup& group )
{
    QWriteLocker locker( m_readWriteLockPersistent );
    group.deleteEntry( name + LIFETIME_ENTRYNAME_SUFFIX );
    group.deleteEntry( name );
}

void Storage::removePersistent( const QString& name )
{
    // Try to load script features from a cache file
    // TODO: Use TimetableAccessor::accessorCacheFileName() from GTFS branch
    const QString fileName = KGlobal::dirs()->saveLocation("data",
            "plasma_engine_publictransport/").append( QLatin1String("datacache"));
    KConfig cfg( fileName, KConfig::SimpleConfig );
    KConfigGroup group = cfg.group( m_serviceProvider ).group( QLatin1String("storage") );
    removePersistent( name, group );
}

void Storage::clearPersistent()
{
    // Try to load script features from a cache file
    // TODO: Use TimetableAccessor::accessorCacheFileName() from GTFS branch
    const QString fileName = KGlobal::dirs()->saveLocation("data",
            "plasma_engine_publictransport/").append( QLatin1String("datacache"));
    KConfig cfg( fileName, KConfig::SimpleConfig );
    KConfigGroup group = cfg.group( m_serviceProvider ).group( QLatin1String("storage") );
    group.deleteGroup();
}
