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
#include "config.h"
#include "global.h"
#include "serviceproviderglobal.h"

// KDE includes
#include <KStandardDirs>
#include <KConfig>
#include <KConfigGroup>
#include <KDebug>
#include <KLocalizedString>

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
#include <QMutex>
#include <QFileInfo>
#include <QBuffer>

// Other includes
#include <zlib.h>

namespace Scripting {

NetworkRequest::NetworkRequest( QObject *parent )
        : QObject(parent), m_mutex(new QMutex(QMutex::Recursive)), m_network(0),
          m_isFinished(false), m_request(0), m_reply(0)
{
}

NetworkRequest::NetworkRequest( const QString &url, const QString &userUrl,
                                Network *network, QObject* parent )
        : QObject(parent), m_mutex(new QMutex(QMutex::Recursive)), m_url(url),
          m_userUrl(userUrl.isEmpty() ? url : userUrl), m_network(network),
          m_isFinished(false), m_request(new QNetworkRequest(url)), m_reply(0)
{
}

NetworkRequest::~NetworkRequest()
{
    abort();

    delete m_request;
    delete m_mutex;
}

void NetworkRequest::abort()
{
    const bool timedOut = qobject_cast< QTimer* >( sender() );
    if ( !isRunning() ) {
        if ( timedOut ) {
            kDebug() << "Timeout, but request already finished";
        }
        return;
    }

    // isRunning() returned true => m_reply != 0
    m_mutex->lockInline();
    disconnect( m_reply, 0, this, 0 );
    m_reply->abort();
    m_reply->deleteLater();
    m_reply = 0;
    m_mutex->unlockInline();

    emit aborted( timedOut );
    emit finished( QByteArray(), true, i18nc("@info/plain", "The request was aborted") );
}

void NetworkRequest::slotReadyRead()
{
    m_mutex->lockInline();
    if ( !m_reply ) {
        // Prevent crashes on exit
        m_mutex->unlockInline();
        kWarning() << "Reply object already deleted, aborted?";
        return;
    }

    // Read all data, decode it and give it to the script
    QByteArray data = m_reply->readAll();
    m_data.append( data );

    if ( data.isEmpty() ) {
        kWarning() << "Error downloading" << m_url << m_reply->errorString();
    }
    m_mutex->unlockInline();

    emit readyRead( data );
}

QByteArray gzipDecompress( QByteArray compressData )
{
    // Strip header and trailer
    compressData.remove( 0, 10 );
    compressData.chop( 12 );

    // FIXME Decompress in one chunk, because otherwise it fails with error -3 "distance too far back",
    // estimate size of uncompressed data, expect that the data was compressed at best to 20% size,
    // limit to 512KB
    const int chunkSize = qMin( int(compressData.size() / 0.2), 512 * 1024 );
    kDebug() << "Chunk size:" << chunkSize;
    if ( chunkSize == 512 * 1024 ) {
        kWarning() << "Maximum chunk size for decompression reached, may fail";
    }

    unsigned char buffer[ chunkSize ];
    z_stream stream;
    stream.next_in = (Bytef*)(compressData.data());
    stream.avail_in = compressData.size();
    stream.total_in = 0;
    stream.total_out = 0;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    if ( inflateInit2(&stream, -8) != Z_OK ) {
        kDebug() << "cmpr_stream error!";
        return QByteArray();
    }

    QByteArray uncompressed;
    do {
        stream.next_out = buffer;
        stream.avail_out = chunkSize;
        int status = inflate( &stream, Z_SYNC_FLUSH );
        if ( status == Z_OK || status == Z_STREAM_END ) {
            uncompressed.append( QByteArray::fromRawData(
                    (char*)buffer, chunkSize - stream.avail_out) );
            if ( status == Z_STREAM_END ) {
                break;
            }
        } else {
            kWarning() << "Error while decompressing" << status << stream.msg;
            if ( status == Z_NEED_DICT || status == Z_DATA_ERROR || status == Z_MEM_ERROR ) {
                inflateEnd( &stream );
            }
            return QByteArray();
        }
    } while( stream.avail_out == 0 );
    inflateEnd( &stream );
    return uncompressed;
}

void NetworkRequest::slotFinished()
{
    m_mutex->lockInline();
    if ( !m_reply ) {
        // Prevent crashes on exit
        m_mutex->unlockInline();
        kWarning() << "Reply object already deleted, aborted?";
        return;
    }

    if ( m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid() ) {
        if ( m_redirectUrl.isValid() ) {
            kWarning() << "Only one redirection allowed, from" << m_url << "to" << m_redirectUrl;
            kWarning() << "New redirection to"
                       << m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        } else {
            m_redirectUrl = m_reply->attribute( QNetworkRequest::RedirectionTargetAttribute ).toUrl();
            DEBUG_NETWORK("Redirection to" << m_redirectUrl);

            // Delete the reply
            m_reply->deleteLater();
            m_reply = 0;

            delete m_request;
            m_request = new QNetworkRequest( m_redirectUrl );
            const QUrl redirectUrl = m_redirectUrl;
            m_mutex->unlockInline();

            emit redirected( redirectUrl );
            return;
        }
    }

    const int size = m_reply->size();
    const int statusCode = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // Read all data, decode it and give it to the script
    m_data.append( m_reply->readAll() );

    if ( m_data.isEmpty() ) {
        kWarning() << "Error downloading" << m_url
                   << (m_reply ? m_reply->errorString() : "Reply already deleted");
    }

    // Check if the data is compressed and was not decompressed by QNetworkAccessManager
    if ( m_data.length() >= 2 && quint8(m_data[0]) == 0x1f && quint8(m_data[1]) == 0x8b ) {
        // Data is compressed, uncompress it
        // NOTE qUncompress() did not work here, "invalid zip"
        m_data = gzipDecompress( m_data );
        DEBUG_NETWORK("Uncompressed data from" << size << "Bytes to" << m_data.size()
                << "Bytes, ratio:" << (100 - (m_data.isEmpty() ? 100 : qRound(100 * size / m_data.size()))) << '%');
    }

    DEBUG_NETWORK("Request finished" << m_reply->url());
    if ( m_reply->url().isEmpty() ) {
        kWarning() << "Empty URL in QNetworkReply!";
    }
    const bool hasError = m_reply->error() != QNetworkReply::NoError;
    const QString errorString = m_reply->errorString();
    m_reply->deleteLater();
    m_reply = 0;

    m_isFinished = true;
    const QByteArray data = m_data;
    m_mutex->unlockInline();

    emit finished( data, hasError, errorString, statusCode, size );
}

void NetworkRequest::started( QNetworkReply* reply, int timeout )
{
    m_mutex->lockInline();
    if ( !m_network ) {
        kWarning() << "Can't decode, no m_network given...";
        m_mutex->unlockInline();
        return;
    }
    m_data.clear();
    m_reply = reply;

    if ( timeout > 0 ) {
        QTimer::singleShot( timeout, this, SLOT(abort()) );
    }

    // Connect to signals of reply only when the associated signals of this class are connected
    if ( receivers(SIGNAL(readyRead(QByteArray))) > 0 ) {
        connect( m_reply, SIGNAL(readyRead()), this, SLOT(slotReadyRead()) );
    }
    connect( m_reply, SIGNAL(finished()), this, SLOT(slotFinished()) );
    m_mutex->unlockInline();

    emit started();
}

bool NetworkRequest::isValid() const
{
    QMutexLocker locker( m_mutex );
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
    QMutexLocker locker( m_mutex );
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

QByteArray NetworkRequest::postDataByteArray() const
{
    QMutexLocker locker( m_mutex );
    return m_postData;
}

void NetworkRequest::setPostData( const QString& postData, const QString& charset )
{
    if ( !isValid() ) {
        return;
    }
    if ( isRunning() ) {
        kDebug() << "Cannot set POST data for an already running request!";
        return;
    }

    QByteArray baCharset = getCharset( charset );
    QTextCodec *codec = QTextCodec::codecForName( baCharset );
    QMutexLocker locker( m_mutex );
    if ( codec ) {
        m_request->setHeader( QNetworkRequest::ContentTypeHeader, baCharset );
        m_postData = codec->fromUnicode( postData );
    } else {
        kDebug() << "Codec" << baCharset << "couldn't be found to encode the data "
                "to post, now using UTF-8";
        m_request->setHeader( QNetworkRequest::ContentTypeHeader, "utf8" );
        m_postData = postData.toUtf8();
    }
}

QString NetworkRequest::header( const QString &header, const QString& charset ) const
{
    if ( !isValid() ) {
        return QString();
    }
    QByteArray baCharset = getCharset( charset );
    QTextCodec *codec = QTextCodec::codecForName( baCharset );

    QMutexLocker locker( m_mutex );
    return m_request->rawHeader( codec ? codec->fromUnicode(header) : header.toUtf8() );
}

void NetworkRequest::setHeader( const QString& header, const QString& value,
                                const QString& charset )
{
    if ( !isValid() ) {
        return;
    }
    if ( isRunning() ) {
        kDebug() << "Cannot set headers for an already running request!";
        return;
    }

    QByteArray baCharset = getCharset( charset );
    QTextCodec *codec = QTextCodec::codecForName( baCharset );
    QMutexLocker locker( m_mutex );
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
    QMutexLocker locker( m_mutex );
    return m_request;
}

Network::Network( const QByteArray &fallbackCharset, QObject* parent )
        : QObject(parent), m_mutex(new QMutex(QMutex::Recursive)),
          m_fallbackCharset(fallbackCharset), m_manager(new QNetworkAccessManager(this)),
          m_quit(false), m_lastDownloadAborted(false)
{
    qRegisterMetaType< NetworkRequest* >( "NetworkRequest*" );
    qRegisterMetaType< NetworkRequest::Ptr >( "NetworkRequest::Ptr" );
}

Network::~Network()
{
    // Quit event loop of possibly running synchronous requests
    m_mutex->lock();
    m_quit = true;
    m_mutex->unlock();

    const QList< NetworkRequest::Ptr > _runningRequests = runningRequests();
    if ( !_runningRequests.isEmpty() ) {
        kWarning() << "Deleting Network object with" << _runningRequests.count()
                   << "running requests";
        foreach ( const NetworkRequest::Ptr &request, _runningRequests ) {
            request->abort();
//             request->deleteLater();
        }
    }

    delete m_mutex;
}

NetworkRequest* Network::createRequest( const QString& url, const QString &userUrl )
{
    NetworkRequest *request = new NetworkRequest( url, userUrl, this, parent() );
    NetworkRequest::Ptr requestPtr( request );
    m_requests << requestPtr;
    connect( request, SIGNAL(started()), this, SLOT(slotRequestStarted()) );
    connect( request, SIGNAL(finished(QByteArray,bool,QString,int,int)),
             this, SLOT(slotRequestFinished(QByteArray,bool,QString,int,int)) );
    connect( request, SIGNAL(aborted()), this, SLOT(slotRequestAborted()) );
    connect( request, SIGNAL(redirected(QUrl)), this, SLOT(slotRequestRedirected(QUrl)) );
    return request;
}

NetworkRequest::Ptr Network::getSharedRequest( NetworkRequest *request ) const
{
    foreach ( const NetworkRequest::Ptr &sharedRequest, m_requests ) {
        if ( sharedRequest.data() == request ) {
            return sharedRequest;
        }
    }
    return NetworkRequest::Ptr();
}

void Network::slotRequestStarted()
{
    NetworkRequest *request = qobject_cast< NetworkRequest* >( sender() );
    NetworkRequest::Ptr sharedRequest = getSharedRequest( request );
    Q_ASSERT( sharedRequest ); // This slot should only be connected to signals of NetworkRequest

    m_mutex->lockInline();
    m_lastDownloadAborted = false;
    m_mutex->unlockInline();

    emit requestStarted( sharedRequest );
}

void Network::slotRequestFinished( const QByteArray &data, bool error, const QString &errorString, 
                                   int statusCode, int size )
{
    NetworkRequest *request = qobject_cast< NetworkRequest* >( sender() );
    NetworkRequest::Ptr sharedRequest = getSharedRequest( request );
    Q_ASSERT( sharedRequest ); // This slot should only be connected to signals of NetworkRequest

    // Remove finished request from request list and add it to the list of finished requests
    // to not have it deleted here. The script might try to use the request object later,
    // eg. because of connected slots, and will crash if the request object was already deleted.
    // This way NetworkRequest objects stay alive at least as long as the Network object does.
    m_mutex->lockInline();
    const QDateTime timestamp = QDateTime::currentDateTime();
    m_requests.removeOne( sharedRequest );
    m_finishedRequests << sharedRequest;
    m_mutex->unlockInline();

    emit requestFinished( sharedRequest, data, error, errorString, timestamp, statusCode, size );

    if ( !hasRunningRequests() ) {
        emit allRequestsFinished();
    }
}

void Network::slotRequestRedirected( const QUrl &newUrl )
{
    NetworkRequest *request = qobject_cast< NetworkRequest* >( sender() );
    NetworkRequest::Ptr sharedRequest = getSharedRequest( request );
    Q_ASSERT( sharedRequest ); // This slot should only be connected to signals of NetworkRequest

    m_mutex->lockInline();
    QNetworkReply *reply = m_manager->get( *request->request() );
    m_lastUrl = newUrl.toString();
    m_mutex->unlockInline();

    request->started( reply );

    DEBUG_NETWORK("Redirected to" << newUrl);
    emit requestRedirected( sharedRequest, newUrl );
}

void Network::slotRequestAborted()
{
    m_mutex->lockInline();
    if ( m_quit ) {
        m_mutex->unlockInline();
        return;
    }

    NetworkRequest *request = qobject_cast< NetworkRequest* >( sender() );
    NetworkRequest::Ptr sharedRequest = getSharedRequest( request );
    if ( !sharedRequest ) {
        kWarning() << "Network object already deleted?";
    }
//     Q_ASSERT( sharedRequest ); // This slot should only be connected to signals of NetworkRequest

    DEBUG_NETWORK("Aborted" << request->url());
    m_lastDownloadAborted = true;
    m_mutex->unlockInline();

    emit requestAborted( sharedRequest );
}

bool Network::checkRequest( NetworkRequest* request )
{
    // Wrong argument type from script or no argument
    if ( !request ) {
        kDebug() << "Need a NetworkRequest object as argument, create it with "
                    "'network.createRequest(url)'";
        return false;
    }

    // The same request can not be executed more than once at a time
    if ( request->isRunning() ) {
        kDebug() << "Request is currently running" << request->url();
        return false;
    } else if ( request->isFinished() ) {
        kDebug() << "Request is already finished" << request->url();
        return false;
    }

    return request->isValid();
}

void Network::get( NetworkRequest* request, int timeout )
{
    if ( !checkRequest(request) ) {
        return;
    }

    // Create a get request
    m_mutex->lockInline();
    QNetworkReply *reply = m_manager->get( *request->request() );
    m_lastUrl = request->url();
    m_lastUserUrl = request->userUrl();
    m_mutex->unlockInline();

    request->started( reply, timeout );
}

void Network::head( NetworkRequest* request, int timeout )
{
    if ( !checkRequest(request) ) {
        return;
    }

    // Create a head request
    m_mutex->lockInline();
    QNetworkReply *reply = m_manager->head( *request->request() );
    m_lastUrl = request->url();
    m_lastUserUrl = request->userUrl();
    m_mutex->unlockInline();

    request->started( reply, timeout );
}

void Network::post( NetworkRequest* request, int timeout )
{
    if ( !checkRequest(request) ) {
        return;
    }

    // Create a head request
    m_mutex->lockInline();
    QNetworkReply *reply = m_manager->post( *request->request(), request->postDataByteArray() );
    m_lastUrl = request->url();
    m_lastUserUrl = request->userUrl();
    m_mutex->unlockInline();

    request->started( reply, timeout );
}

void Network::abortAllRequests()
{
    const QList< NetworkRequest::Ptr > requests = runningRequests();
    DEBUG_NETWORK("Abort" << requests.count() << "request(s)");
    for ( int i = requests.count() - 1; i >= 0; --i ) {
        // Calling abort automatically removes the aborted request from m_createdRequests
        requests[i]->abort();
    }
}

QByteArray Network::getSynchronous( const QString &url, const QString &userUrl, int timeout )
{
    // Create a get request
    QNetworkRequest request( url );
    DEBUG_NETWORK("Start synchronous request" << url);

    m_mutex->lockInline();
    QNetworkReply *reply = m_manager->get( request );
    m_lastUrl = url;
    m_lastUserUrl = userUrl.isEmpty() ? url : userUrl;
    m_lastDownloadAborted = false;
    m_mutex->unlockInline();

    emit synchronousRequestStarted( url );
    const QTime start = QTime::currentTime();

    int redirectCount = 0;
    const int maxRedirections = 3;
    forever {
        // Use an event loop to wait for execution of the request,
        // ie. make netAccess.download() synchronous for scripts
        QEventLoop eventLoop;
        connect( reply, SIGNAL(finished()), &eventLoop, SLOT(quit()) );
        connect( this, SIGNAL(allRequestsFinished()), &eventLoop, SLOT(quit()) );
        if ( timeout > 0 ) {
            QTimer::singleShot( timeout, &eventLoop, SLOT(quit()) );
        }
        if ( !reply->isFinished() ) {
            eventLoop.exec( QEventLoop::ExcludeUserInputEvents );
        }

        m_mutex->lock();
        const bool quit = m_quit || m_lastDownloadAborted || reply->isRunning();
        m_mutex->unlock();

        // Check if the timeout occured before the request finished
        if ( quit ) {
            DEBUG_NETWORK("Cancelled, destroyed or timeout while downloading" << url);
            emit synchronousRequestFinished( url, QByteArray(), true );
            return QByteArray();
        }

        if ( reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid() ) {
            ++redirectCount;
            if ( redirectCount > maxRedirections ) {
                reply->deleteLater();
                emit synchronousRequestFinished( url, QByteArray("Too many redirections"), true,
                                                 200, start.msecsTo(QTime::currentTime()) );
                return QByteArray();
            }

            // Request the redirection target
            const QUrl redirectUrl =
                    reply->attribute( QNetworkRequest::RedirectionTargetAttribute ).toUrl();
            request.setUrl( redirectUrl );
            DEBUG_NETWORK("Redirected to" << redirectUrl);

            m_mutex->lock();
            delete reply;
            reply = m_manager->get( request );
            m_lastUrl = redirectUrl.toString();
            m_mutex->unlock();

            emit synchronousRequestRedirected( redirectUrl.toEncoded() );
        } else {
            // No (more) redirection
            break;
        }
    }

    const int time = start.msecsTo( QTime::currentTime() );
    const int statusCode = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
    DEBUG_NETWORK("Waited" << (time / 1000.0) << "seconds for download of"
                  << url << "Status:" << statusCode);

    // Read all data, decode it and give it to the script
    QByteArray data = reply->readAll();
    reply->deleteLater();
    if ( data.isEmpty() ) {
        kWarning() << "Error downloading" << url << reply->errorString();
        emit synchronousRequestFinished( url, QByteArray(), true, statusCode, time );
        return QByteArray();
    } else {
        QMutexLocker locker( m_mutex );
        emit synchronousRequestFinished( url, data, false, statusCode, time, data.size() );
        return data;
    }
}

ResultObject::ResultObject( QObject* parent )
        : QObject(parent), m_mutex(new QMutex()), m_features(DefaultFeatures), m_hints(NoHint)
{
}

ResultObject::~ResultObject()
{
    delete m_mutex;
}

bool ResultObject::isHintGiven( ResultObject::Hint hint ) const
{
    QMutexLocker locker( m_mutex );
    return m_hints.testFlag( hint );
}

void ResultObject::giveHint( ResultObject::Hint hint, bool enable )
{
    QMutexLocker locker( m_mutex );
    if ( enable ) {
        // Remove incompatible flags of hint if any
        if ( hint == CityNamesAreLeft && m_hints.testFlag(CityNamesAreRight) ) {
            m_hints &= ~CityNamesAreRight;
        } else if ( hint == CityNamesAreRight && m_hints.testFlag(CityNamesAreLeft) ) {
            m_hints &= ~CityNamesAreLeft;
        }
        m_hints |= hint;
    } else {
        m_hints &= ~hint;
    }
}

bool ResultObject::isFeatureEnabled( ResultObject::Feature feature ) const
{
    QMutexLocker locker( m_mutex );
    return m_features.testFlag( feature );
}

void ResultObject::enableFeature( ResultObject::Feature feature, bool enable )
{
    QMutexLocker locker( m_mutex );
    if ( enable ) {
        m_features |= feature;
    } else {
        m_features &= ~feature;
    }
}

void ResultObject::dataList( const QList< TimetableData > &dataList,
                             PublicTransportInfoList *infoList, ParseDocumentMode parseMode,
                             Enums::VehicleType defaultVehicleType, const GlobalTimetableInfo *globalInfo,
                             ResultObject::Features features, ResultObject::Hints hints )
{                               // TODO Use features and hints
    Q_UNUSED( features );
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
        if ( !timetableData.contains(Enums::TypeOfVehicle) ||
             timetableData[Enums::TypeOfVehicle].toString().isEmpty() )
        {
            timetableData[ Enums::TypeOfVehicle ] = static_cast<int>( defaultVehicleType );
        }

        if ( parseMode != ParseForStopSuggestions ) {
            QDateTime dateTime = timetableData[ Enums::DepartureDateTime ].toDateTime();
            QDate departureDate = timetableData[ Enums::DepartureDate ].toDate();
            QTime departureTime = timetableData[ Enums::DepartureTime ].toTime();

            if ( !dateTime.isValid() && !departureTime.isValid() ) {
                kDebug() << "No departure time given!" << timetableData[Enums::DepartureTime];
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
                timetableData[ Enums::DepartureDateTime ] = dateTime;
            }

            if ( dayAdjustment != 0 ) {
                dateTime.setDate( dateTime.date().addDays(dayAdjustment) );
                timetableData[ Enums::DepartureDateTime ] = dateTime;
            }
            curDate = dateTime.date();
            lastTime = departureTime;
        }

        // Create info object for the timetable data
        PublicTransportInfoPtr info;
        if ( parseMode == ParseForJourneysByDepartureTime ||
             parseMode == ParseForJourneysByArrivalTime )
        {
            info = QSharedPointer<JourneyInfo>( new JourneyInfo(timetableData) );
        } else if ( parseMode == ParseForDepartures || parseMode == ParseForArrivals ) {
            info = QSharedPointer<DepartureInfo>( new DepartureInfo(timetableData) );
        } else if ( parseMode == ParseForStopSuggestions ) {
            info = QSharedPointer<StopInfo>( new StopInfo(timetableData) );
        }

        if ( !info->isValid() ) {
            info.clear();
            continue;
        }

        // Find word to remove from beginning/end of stop names, if not already found
        // TODO Use hint from the data engine..
        if ( features.testFlag(AutoRemoveCityFromStopNames) &&
             removeFirstWord.isEmpty() && removeLastWord.isEmpty() )
        {
            // First count the first/last word of the target stop name
            const QString target = info->value( Enums::Target ).toString();
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
            if ( info->contains(Enums::RouteStops) ) {
                QStringList stops = info->value( Enums::RouteStops ).toStringList();
                QString target = info->value( Enums::Target ).toString();

                // TODO Break if 70% or at least three of the route stop names
                // begin/end with the same word
//                 int minCount = qMax( 3, int(stops.count() * 0.7) );
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
    if ( features.testFlag(AutoRemoveCityFromStopNames) ) {
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
                QString target = info->value( Enums::Target ).toString();
                if ( target.startsWith(removeFirstWord) ) {
                    target = target.mid( removeFirstWord.length() + 1 );
                    info->insert( Enums::TargetShortened, target );
                }

                QStringList stops = info->value( Enums::RouteStops ).toStringList();
                for ( int i = 0; i < stops.count(); ++i ) {
                    if ( stops[i].startsWith(removeFirstWord) ) {
                        stops[i] = stops[i].mid( removeFirstWord.length() + 1 );
                    }
                }
                info->insert( Enums::RouteStopsShortened, stops );
            }
        } else if ( !removeLastWord.isEmpty() ) {
            // Remove removeLastWord from all stop names
            for ( int i = 0; i < infoList->count(); ++i ) {
                QSharedPointer<PublicTransportInfo> info = infoList->at( i );
                QString target = info->value( Enums::Target ).toString();
                if ( target.endsWith(removeLastWord) ) {
                    target = target.left( target.length() - removeLastWord.length() );
                    info->insert( Enums::TargetShortened, target );
                }

                QStringList stops = info->value( Enums::RouteStops ).toStringList();
                for ( int i = 0; i < stops.count(); ++i ) {
                    if ( stops[i].endsWith(removeLastWord) ) {
                        stops[i] = stops[i].left( stops[i].length() - removeLastWord.length() );
                    }
                }
                info->insert( Enums::RouteStopsShortened, stops );
            }
        }
    }
}

QString Helper::decodeHtmlEntities( const QString& html )
{
    return Global::decodeHtmlEntities( html );
}

QString Helper::encodeHtmlEntities( const QString &html )
{
    return Global::encodeHtmlEntities( html );
}

QString Helper::decodeHtml( const QByteArray &document, const QByteArray &fallbackCharset )
{
    return Global::decodeHtml( document, fallbackCharset );
}

QString Helper::decode( const QByteArray &document, const QByteArray &charset )
{
    return Global::decode( document, charset );
}

Helper::Helper( const QString &serviceProviderId, QObject *parent )
        : QObject(parent), m_mutex(new QMutex()), m_serviceProviderId(serviceProviderId),
          m_errorMessageRepetition(0)
{
}

Helper::~Helper()
{
    emitRepeatedMessageWarning();
    delete m_mutex;
}

void Helper::emitRepeatedMessageWarning()
{
    QMutexLocker locker( m_mutex );
    if ( m_errorMessageRepetition > 0 ) {
        kDebug() << "(Last error message repeated" << m_errorMessageRepetition << "times)";
        m_errorMessageRepetition = 0;
        emit messageReceived( i18nc("@info/plain", "Last error message repeated %1 times",
                                    m_errorMessageRepetition), QScriptContextInfo() );
    }
}

void Helper::messageReceived( const QString& message, const QString &failedParseText,
                              ErrorSeverity severity )
{
    m_mutex->lock();
    if ( message == m_lastErrorMessage ) {
        ++m_errorMessageRepetition;
        m_mutex->unlock();
        return;
    }
    m_lastErrorMessage = message;
    QScriptContextInfo info( context()->parentContext() );
    const QString serviceProviderId = m_serviceProviderId;
    m_mutex->unlock();

    emitRepeatedMessageWarning();
    emit messageReceived( message, info, failedParseText, severity );

    // Output debug message and a maximum count of 200 characters of the text where the parsing failed
    QString shortParseText = failedParseText.trimmed().left(350);
    int diff = failedParseText.length() - shortParseText.length();
    if ( diff > 0 ) {
        shortParseText.append(QString("... <%1 more chars>").arg(diff));
    }
    shortParseText = shortParseText.replace('\n', "\n    "); // Indent

#ifdef ENABLE_DEBUG_SCRIPT_ERROR
    DEBUG_SCRIPT_ERROR( QString("Error in %1-script, function %2(), file %3, line %4")
            .arg(m_serviceProviderId)
            .arg(info.functionName().isEmpty() ? "[anonymous]" : info.functionName())
            .arg(QFileInfo(info.fileName()).fileName())
            .arg(info.lineNumber()) );
    DEBUG_SCRIPT_ERROR( message );
    if ( !shortParseText.isEmpty() ) {
        DEBUG_SCRIPT_ERROR( QString("The text of the document where parsing failed is: \"%1\"")
                            .arg(shortParseText) );
    }
#endif

    // Log the complete message to the log file.
    QString logFileName = KGlobal::dirs()->saveLocation( "data", "plasma_engine_publictransport" );
    logFileName.append( "serviceproviders.log" );

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

        logFile.write( QString("%1 (%2, in function %3(), file %4, line %5):\n   \"%6\"\n   Failed while reading this text: \"%7\"\n-------------------------------------\n\n")
                .arg(serviceProviderId)
                .arg(QDateTime::currentDateTime().toString())
                .arg(info.functionName().isEmpty() ? "[anonymous]" : info.functionName())
                .arg(QFileInfo(info.fileName()).fileName())
                .arg(info.lineNumber())
                .arg(message)
                .arg(failedParseText.trimmed()).toUtf8() );
        logFile.close();
    }
}

void ResultObject::addData( const QVariantMap& map )
{
    m_mutex->lockInline();
    TimetableData data;
    for ( QVariantMap::ConstIterator it = map.constBegin(); it != map.constEnd(); ++it ) {
        bool ok;
        Enums::TimetableInformation info =
                static_cast< Enums::TimetableInformation >( it.key().toInt(&ok) );
        if ( !ok || info == Enums::Nothing ) {
            info = Global::timetableInformationFromString( it.key() );
        }
        const QVariant value = it.value();
        if ( info == Enums::Nothing ) {
            kDebug() << "Unknown timetable information" << it.key() << "with value" << value;
            const QString message = i18nc("@info/plain", "Invalid timetable information \"%1\" "
                                          "with value \"%2\"", it.key(), value.toString());
            const int count = m_timetableData.count();
            m_mutex->unlockInline();
            emit invalidDataReceived( info, message, context()->parentContext(), count, map );
            m_mutex->lockInline();
            continue;
        } else if ( value.isNull() ) {
            // Null value received, simply leave the data empty
            continue;
        } else if ( !value.isValid() ) {
            kDebug() << "Value for" << info << "is invalid or null" << value;
            const QString message = i18nc("@info/plain", "Invalid value received for \"%1\"",
                                          it.key());
            const int count = m_timetableData.count();
            m_mutex->unlockInline();
            emit invalidDataReceived( info, message, context()->parentContext(), count, map );
            m_mutex->lockInline();
            continue;
        } else if ( info == Enums::TypeOfVehicle &&
                    static_cast<Enums::VehicleType>(value.toInt()) == Enums::InvalidVehicleType &&
                    Global::vehicleTypeFromString(value.toString()) == Enums::InvalidVehicleType )
        {
            kDebug() << "Invalid type of vehicle value" << value;
            const QString message = i18nc("@info/plain",
                    "Invalid type of vehicle received: \"%1\"", value.toString());
            const int count = m_timetableData.count();
            m_mutex->unlockInline();
            emit invalidDataReceived( info, message, context()->parentContext(), count, map );
            m_mutex->lockInline();
        } else if ( info == Enums::TypesOfVehicleInJourney || info == Enums::RouteTypesOfVehicles ) {
            const QVariantList types = value.toList();
            foreach ( const QVariant &type, types ) {
                if ( static_cast<Enums::VehicleType>(type.toInt()) == Enums::InvalidVehicleType &&
                     Global::vehicleTypeFromString(type.toString()) == Enums::InvalidVehicleType )
                {
                    kDebug() << "Invalid type of vehicle value in"
                             << Global::timetableInformationToString(info) << value;
                    const QString message = i18nc("@info/plain",
                            "Invalid type of vehicle received in \"%1\": \"%2\"",
                            Global::timetableInformationToString(info), type.toString());
                    const int count = m_timetableData.count();
                    m_mutex->unlockInline();
                    emit invalidDataReceived( info, message, context()->parentContext(),
                                              count, map );
                    m_mutex->lockInline();
                }
            }
        }

        if ( m_features.testFlag(AutoDecodeHtmlEntities) ) {
            if ( value.canConvert(QVariant::String) &&
                 (info == Enums::StopName || info == Enums::Target || info == Enums::StartStopName ||
                  info == Enums::TargetStopName || info == Enums::Operator ||
                  info == Enums::TransportLine || info == Enums::Platform ||
                  info == Enums::DelayReason || info == Enums::Status || info == Enums::Pricing) )
            {
                // Decode HTML entities in string values
                data[ info ] = Global::decodeHtmlEntities( value.toString() ).trimmed();
            } else if ( value.canConvert(QVariant::StringList) &&
                 (info == Enums::RouteStops || info == Enums::RoutePlatformsDeparture ||
                  info == Enums::RoutePlatformsArrival) )
            {
                // Decode HTML entities in string list values
                QStringList stops = value.toStringList();
                for ( QStringList::Iterator it = stops.begin(); it != stops.end(); ++it ) {
                    *it = Helper::trim( Global::decodeHtmlEntities(*it) );
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
        m_mutex->unlockInline();
        emit publish();
    } else {
        m_mutex->unlockInline();
    }
}

QString Helper::trim( const QString& str )
{
    return QString(str).trimmed()
                       .replace( QRegExp("^(&nbsp;)+|(&nbsp;)+$", Qt::CaseInsensitive), QString() )
                       .trimmed();
}

QString Helper::simplify( const QString &str )
{
    return QString(str).replace( QRegExp("(&nbsp;)+", Qt::CaseInsensitive), QString() )
                       .simplified();
}

QString Helper::stripTags( const QString& str )
{
    const QString attributePattern = "\\w+(?:\\s*=\\s*(?:\"[^\"]*\"|'[^']*'|[^\"'>\\s]+))?";
    QRegExp rx( QString("<\\/?\\w+(?:\\s+%1)*(?:\\s*/)?>").arg(attributePattern) );
    rx.setMinimal( true );
    return QString( str ).remove( rx );
}

QString Helper::camelCase( const QString& str )
{
    QString ret = str.toLower();
    QRegExp rx( "(^\\w)|\\W(\\w)" );
    int pos = 0;
    while ( (pos = rx.indexIn(ret, pos)) != -1 ) {
        if ( rx.pos(2) == -1 || rx.pos(2) >= ret.length() ) {
            ret[ rx.pos(1) ] = ret[ rx.pos(1) ].toUpper();
        } else {
            ret[ rx.pos(2) ] = ret[ rx.pos(2) ].toUpper();
        }
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
            DEBUG_SCRIPT_HELPER("Couldn't match time in" << str << pattern);
        }
    } else {
        ret.insert( "error", true );
        DEBUG_SCRIPT_HELPER("Couldn't match time in" << str << pattern);
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
        }
    }
    if ( !date.isValid() ) {
        DEBUG_SCRIPT_HELPER("Couldn't match date in" << str << pattern);
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
        DEBUG_SCRIPT_HELPER("Couldn't parse the given time" << sTime << format);
        return "";
    }
    return time.addSecs( minsToAdd * 60 ).toString( format );
}

QString Helper::addDaysToDate( const QString& sDate, int daysToAdd, const QString& format )
{
    QDate date = QDate::fromString( sDate, format ).addDays( daysToAdd );
    if ( !date.isValid() ) {
        DEBUG_SCRIPT_HELPER("Couldn't parse the given date" << sDate << format);
        return sDate;
    }
    return date.toString( format );
}

QDateTime Helper::addDaysToDate( const QDateTime& dateTime, int daysToAdd )
{
    return dateTime.addDays( daysToAdd );
}

QStringList Helper::splitSkipEmptyParts( const QString& str, const QString& sep )
{
    return str.split( sep, QString::SkipEmptyParts );
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
    const QString contentsRegExpPattern = options.value( "contentsRegExp", QString() ).toString();
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
                           // TODO TEST does this need a "\\s*" before the ">"?
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
            const int valueCap = attributeRegExp.cap(2).isEmpty() ?
                    (attributeRegExp.cap(3).isEmpty() ? 4 : 3) : 2;
            foundAttributes.insert( attributeRegExp.cap(1), attributeRegExp.cap(valueCap) );
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

        // Search for new opening HTML tags (with same tag name) before the closing HTML tag
        int endPosition = htmlTagRegExp.pos() + htmlTagRegExp.matchedLength();
        if ( !attributesMatch ) {
            position = endPosition;
            continue;
        }
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
                    position = endPosition;
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

        // Match contents, only use regular expression if one was given in the options argument
        if ( !contentsRegExpPattern.isEmpty() ) {
            if ( contentsRegExp.indexIn(tagContents) == -1 ) {
                if ( debug ) {
                    kDebug() << "Did not match tag contents" << tagContents.left(500);
                }
                position = endPosition;
                continue;
            } else {
                // Use first matched group as contents string, if any
                // Otherwise use the whole match as contents string
                tagContents = contentsRegExp.cap( contentsRegExp.captureCount() <= 1 ? 0 : 1 );
            }
        } else {
            // No regexp pattern for contents, use complete contents, but trimmed
            tagContents = tagContents.trimmed();
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
        if ( foundTags.isEmpty() ) {
            kDebug() << "Found no" << tagName << "HTML tags in HTML" << str;
        } else {
            kDebug() << "Found" << foundTags.count() << tagName << "HTML tags";
        }
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
    const bool debug = options.value( "debug", false ).toBool();

    const QVariantList foundTags = findHtmlTags( str, tagName, options );
    QVariantMap foundTagsMap;
    foreach ( const QVariant &foundTag, foundTags ) {
        QString name = getTagName( foundTag.toMap(), namePosition["type"].toString(),
                namePositionRegExpPattern,
                namePositionIsAttribute ? namePosition["name"].toString() : QString() );
        if ( name.isEmpty() ) {
            if ( debug ) {
                kDebug() << "Empty name in" << str;
            }
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
    if ( !foundTagsMap.contains("names") ) {
        foundTagsMap[ "names" ] = QVariant::fromValue<QStringList>( foundTagsMap.keys() );
    } else if ( debug ) {
        kDebug() << "A tag with the name 'names' was found. Normally a property 'names' gets "
                    "added to the object returned by this functionm, which lists all found "
                    "names in a list.";
    }
    return foundTagsMap;
}

const char* Storage::LIFETIME_ENTRYNAME_SUFFIX = "__expires__";

class StoragePrivate {
public:
    StoragePrivate( const QString &serviceProvider )
            : readWriteLock(new QReadWriteLock), readWriteLockPersistent(new QReadWriteLock),
              serviceProvider(serviceProvider), lastLifetimeCheck(0), config(0) {
    };

    ~StoragePrivate() {
        delete readWriteLock;
        delete readWriteLockPersistent;
        delete config;
    };

    void readPersistentData() {
        // Delete already read config object
        if ( config ) {
            delete config;
        }

        config = new KConfig( ServiceProviderGlobal::cacheFileName(), KConfig::SimpleConfig );
    };

    KConfigGroup persistentGroup() {
        if ( !config ) {
            readPersistentData();
        }
        return config->group( serviceProvider ).group( QLatin1String("storage") );
    };

    quint16 checkLength( const QByteArray &data ) {
        if ( data.length() > 65535 ) {
            kDebug() << "Data is too long, only 65535 bytes are supported" << data.length();
        }

        return static_cast<quint16>( data.length() );
    };

    QReadWriteLock *readWriteLock;
    QReadWriteLock *readWriteLockPersistent;
    QVariantMap data;
    const QString serviceProvider;
    uint lastLifetimeCheck; // as time_t
    KConfig *config;
    KConfigGroup lastPersistentGroup;
};

Storage::Storage( const QString &serviceProviderId, QObject *parent )
        : QObject(parent), d(new StoragePrivate(serviceProviderId))
{
    // Delete persistently stored data which lifetime has expired
    checkLifetime();
}

Storage::~Storage()
{
    delete d;
}

void Storage::write( const QVariantMap& data )
{
    for ( QVariantMap::ConstIterator it = data.constBegin(); it != data.constEnd(); ++it ) {
        write( it.key(), it.value() );
    }
}

void Storage::write( const QString& name, const QVariant& data )
{
    QWriteLocker locker( d->readWriteLock );
    d->data.insert( name, data );
}

QVariantMap Storage::read()
{
    QReadLocker locker( d->readWriteLock );
    return d->data;
}

QVariant Storage::read( const QString& name, const QVariant& defaultData )
{
    QReadLocker locker( d->readWriteLock );
    return d->data.contains(name) ? d->data[name] : defaultData;
}

void Storage::remove( const QString& name )
{
    QWriteLocker locker( d->readWriteLock );
    d->data.remove( name );
}

void Storage::clear()
{
    QWriteLocker locker( d->readWriteLock );
    d->data.clear();
}

int Storage::lifetime( const QString& name )
{
    QWriteLocker locker( d->readWriteLockPersistent );
    return lifetime( name, d->persistentGroup() );
}

int Storage::lifetime( const QString& name, const KConfigGroup& group )
{
    QReadLocker locker( d->readWriteLockPersistent );
    const uint lifetimeTime_t = group.readEntry( name + LIFETIME_ENTRYNAME_SUFFIX, 0 );
    return QDateTime::currentDateTime().daysTo( QDateTime::fromTime_t(lifetimeTime_t) );
}

void Storage::checkLifetime()
{
    QWriteLocker locker( d->readWriteLockPersistent );
    if ( QDateTime::currentDateTime().toTime_t() - d->lastLifetimeCheck <
         MIN_LIFETIME_CHECK_INTERVAL * 60 )
    {
        // Last lifetime check was less than 15 minutes ago
        return;
    }

    // Try to load script features from a cache file
    KConfigGroup group = d->persistentGroup();
    QMap< QString, QString > data = group.entryMap();
    for ( QMap< QString, QString >::ConstIterator it = data.constBegin();
          it != data.constEnd(); ++it )
    {
        if ( it.key().endsWith(LIFETIME_ENTRYNAME_SUFFIX) ) {
            // Do not check lifetime of entries which store the lifetime of the real data entries
            continue;
        }
        const int remainingLifetime = lifetime( it.key(), group );
        if ( remainingLifetime <= 0 ) {
            // Lifetime has expired
            kDebug() << "Lifetime of storage data" << it.key() << "for" << d->serviceProvider
                     << "has expired" << remainingLifetime;
            removePersistent( it.key(), group );
        }
    }

    d->lastLifetimeCheck = QDateTime::currentDateTime().toTime_t();
}

bool Storage::hasData( const QString &name ) const
{
    QReadLocker locker( d->readWriteLock );
    return d->data.contains( name );
}

bool Storage::hasPersistentData( const QString &name ) const
{
    QReadLocker locker( d->readWriteLockPersistent );
    KConfigGroup group = d->persistentGroup();
    return group.hasKey( name );
}

QByteArray Storage::encodeData( const QVariant &data ) const
{
    // Store the type of the variant, because it is needed to read the QVariant with the correct
    // type using KConfigGroup::readEntry()
    const uchar type = static_cast<uchar>( data.type() );
    if ( type >= QVariant::LastCoreType ) {
        kDebug() << "Invalid data type, only QVariant core types are supported" << data.type();
        return QByteArray();
    }

    // Write type into the first byte
    QByteArray encodedData;
    encodedData[0] = type;

    // Write data
    if ( data.canConvert(QVariant::ByteArray) ) {
        encodedData += data.toByteArray();
    } else if ( data.canConvert(QVariant::String) ) {
        encodedData += data.toString().toUtf8();
    } else {
        switch ( data.type() ) {
        case QVariant::StringList:
        case QVariant::List: {
            // Lists are stored like this (one entry after the other):
            // "<2 Bytes: Value length><Value>"
            const QVariantList list = data.toList();
            foreach ( const QVariant &item, list ) {
                // Encode current list item
                QByteArray encodedItem = encodeData( item );

                // Construct a QByteArray which contains the length in two bytes (0 .. 65535)
                const quint16 length = d->checkLength( encodedItem );
                const QByteArray baLength( (const char*)&length, sizeof(length) );

                // Use 2 bytes for the length of the data, append data
                encodedData += baLength;
                encodedData += encodedItem;
            }
            break;
        }
        case QVariant::Map: {
            // Maps are stored like this (one entry after the other):
            // "<2 Bytes: Key length><Key><2 Bytes: Value length><Value>"
            const QVariantMap map = data.toMap();
            for ( QVariantMap::ConstIterator it = map.constBegin(); it != map.constEnd(); ++it ) {
                // Encode current key and value
                QByteArray encodedKey = it.key().toUtf8();
                QByteArray encodedValue = encodeData( it.value() );

                // Construct QByteArrays which contain the lengths in two bytes each (0 .. 65535)
                const quint16 lengthKey = d->checkLength( encodedKey );
                const quint16 lengthValue = d->checkLength( encodedValue );
                const QByteArray baLengthKey( (const char*)&lengthKey, sizeof(lengthKey) );
                const QByteArray baLengthValue( (const char*)&lengthValue, sizeof(lengthValue) );

                // Use 2 bytes for the length of the key, append key
                encodedData += baLengthKey;
                encodedData += encodedKey;

                // Use 2 bytes for the length of the value, append value
                encodedData += baLengthValue;
                encodedData += encodedValue;
            }
            break;
        }
        default:
            kDebug() << "Cannot convert from type" << data.type();
            return QByteArray();
        }
    }

    return encodedData;
}

QVariant Storage::decodeData( const QByteArray &data ) const
{
    const QVariant::Type type = static_cast<QVariant::Type>( data[0] );
    if ( type >= QVariant::LastCoreType ) {
        kDebug() << "Invalid encoding for data" << data;
        return QVariant();
    }

    const QByteArray encodedValue = data.mid( 1 );
    QVariant value( type );
    value.setValue( encodedValue );
    if ( value.canConvert( type ) ) {
        value.convert( type );
        return value;
    } else {
        switch ( type ) {
        case QVariant::Date:
            // QVariant::toString() uses Qt::ISODate to convert QDate to string
            return QDate::fromString( value.toString(), Qt::ISODate );
        case QVariant::Time:
            return QTime::fromString( value.toString() );
        case QVariant::DateTime:
            // QVariant::toString() uses Qt::ISODate to convert QDateTime to string
            return QDateTime::fromString( value.toString(), Qt::ISODate );
        case QVariant::StringList:
        case QVariant::List: {
            // Lists are stored like this (one entry after the other):
            // "<2 Bytes: Value length><Value>"
            QVariantList decoded;
            QByteArray encoded = value.toByteArray();
            int pos = 0;
            while ( pos + 2 < encoded.length() ) {
                const quint16 length = *reinterpret_cast<quint16*>( encoded.mid(pos, 2).data() );
                if ( pos + 2 + length > encoded.length() ) {
                    kDebug() << "Invalid list data" << encoded;
                    return QVariant();
                }
                const QByteArray encodedValue = encoded.mid( pos + 2, length );
                const QVariant decodedValue = decodeData( encodedValue );
                decoded << decodedValue;

                pos += 2 + length;
            }
            return decoded;
        }
        case QVariant::Map: {
            // Maps are stored like this (one entry after the other):
            // "<2 Bytes: Key length><Key><2 Bytes: Value length><Value>"
            QVariantMap decoded;
            const QByteArray encoded = value.toByteArray();
            int pos = 0;
            while ( pos + 4 < encoded.length() ) {
                // Decode two bytes to quint16, this is the key length
                const quint16 keyLength = *reinterpret_cast<quint16*>( encoded.mid(pos, 2).data() );
                if ( pos + 4 + keyLength > encoded.length() ) { // + 4 => 2 Bytes for keyLength + 2 Bytes for valueLength
                    kDebug() << "Invalid map data" << encoded;
                    return QVariant();
                }

                // Extract key, starting after the two bytes for the key length
                const QString key = encoded.mid( pos + 2, keyLength );
                pos += 2 + keyLength;

                // Decode two bytes to quint16, this is the value length
                const quint16 valueLength = *reinterpret_cast<quint16*>( encoded.mid(pos, 2).data() );
                if ( pos + 2 + valueLength > encoded.length() ) {
                    kDebug() << "Invalid map data" << encoded;
                    return QVariant();
                }

                // Extract and decode value, starting after the two bytes for the value length
                const QByteArray encodedValue = encoded.mid( pos + 2, valueLength );
                const QVariant decodedValue = decodeData( encodedValue );
                pos += 2 + valueLength;

                // Insert decoded value into the result map
                decoded.insert( key, decodedValue );
            }
            return decoded;
        }
        default:
            kDebug() << "Cannot convert to type" << type;
            return QVariant();
        }
    }
}

void Storage::writePersistent( const QVariantMap& data, uint lifetime )
{
    QWriteLocker locker( d->readWriteLockPersistent );
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
    QWriteLocker locker( d->readWriteLockPersistent );
    KConfigGroup group = d->persistentGroup();
    group.writeEntry( name + LIFETIME_ENTRYNAME_SUFFIX,
                      QDateTime::currentDateTime().addDays(lifetime).toTime_t() );
    group.writeEntry( name, encodeData(data) );
}

QVariant Storage::readPersistent( const QString& name, const QVariant& defaultData )
{
    // Try to load script features from a cache file
    QWriteLocker locker( d->readWriteLockPersistent );
    if ( defaultData.isValid() ) {
        return d->persistentGroup().readEntry( name, defaultData );
    } else {
        return decodeData( d->persistentGroup().readEntry(name, QByteArray()) );
    }
}

void Storage::removePersistent( const QString& name, KConfigGroup& group )
{
    QWriteLocker locker( d->readWriteLockPersistent );
    group.deleteEntry( name + LIFETIME_ENTRYNAME_SUFFIX );
    group.deleteEntry( name );
}

void Storage::removePersistent( const QString& name )
{
    // Try to load script features from a cache file
    QWriteLocker locker( d->readWriteLockPersistent );
    KConfigGroup group = d->persistentGroup();
    removePersistent( name, group );
}

void Storage::clearPersistent()
{
    // Try to load script features from a cache file
    QWriteLocker locker( d->readWriteLockPersistent );
    d->persistentGroup().deleteGroup();
}

QString Network::lastUrl() const
{
    QMutexLocker locker( m_mutex );
    return m_lastUrl;
}

QString Network::lastUserUrl() const
{
    QMutexLocker locker( m_mutex );
    return m_lastUserUrl;
}

void Network::clear()
{
    QMutexLocker locker( m_mutex );
    m_lastUrl.clear();
    m_lastUserUrl.clear();
}

bool Network::lastDownloadAborted() const
{
    QMutexLocker locker( m_mutex );
    return m_lastDownloadAborted;
}

bool Network::hasRunningRequests() const
{
    QMutexLocker locker( m_mutex );
    foreach ( const NetworkRequest::Ptr &request, m_requests ) {
        if ( request->isRunning() ) {
            // Found a running request
            return true;
        }
    }
    // No running request found
    return false;
}

QList< NetworkRequest::Ptr > Network::runningRequests() const
{
    QMutexLocker locker( m_mutex );
    QList< NetworkRequest::Ptr > requests;
    foreach ( const NetworkRequest::Ptr &request, m_requests ) {
        if ( request->isRunning() ) {
            requests << request;
        }
    }
    return requests;
}

int Network::runningRequestCount() const
{
    return runningRequests().count();
}

QByteArray Network::fallbackCharset() const
{
    QMutexLocker locker( m_mutex );
    return m_fallbackCharset;
}

QList< TimetableData > ResultObject::data() const
{
    QMutexLocker locker( m_mutex );
    return m_timetableData;
}

QVariant ResultObject::data( int index, Enums::TimetableInformation information ) const
{
    QMutexLocker locker( m_mutex );
    if ( index < 0 || index >= m_timetableData.count() ) {
        context()->throwError( QScriptContext::RangeError, "Index out of range" );
        return QVariant();
    }
    return m_timetableData[ index ][ information ];
}

bool ResultObject::hasData() const
{
    QMutexLocker locker( m_mutex );
    return !m_timetableData.isEmpty();
}

int ResultObject::count() const
{
    QMutexLocker locker( m_mutex );
    return m_timetableData.count();
}

ResultObject::Features ResultObject::features() const
{
    QMutexLocker locker( m_mutex );
    return m_features;
}

ResultObject::Hints ResultObject::hints() const
{
    QMutexLocker locker( m_mutex );
    return m_hints;
}

void ResultObject::clear()
{
    QMutexLocker locker( m_mutex );
    m_timetableData.clear();
}

QScriptValue constructStream( QScriptContext *context, QScriptEngine *engine )
{
    QScriptValue argument = context->argument(0);
    DataStreamPrototype *object;
    if ( argument.instanceOf(context->callee()) ) {
        object = new DataStreamPrototype( qscriptvalue_cast<DataStreamPrototype*>(argument) );
    } else if ( argument.isQObject() && qscriptvalue_cast<QIODevice*>(argument) ) {
        object = new DataStreamPrototype( qscriptvalue_cast<QIODevice*>(argument) );
    } else if ( argument.isVariant() ) {
        const QVariant variant = argument.toVariant();
        object = new DataStreamPrototype( variant.toByteArray() );
    }
    return engine->newQObject( object, QScriptEngine::ScriptOwnership );
}

QScriptValue dataStreamToScript( QScriptEngine *engine, const DataStreamPrototypePtr &stream )
{
    return engine->newQObject( const_cast<DataStreamPrototype*>(stream), QScriptEngine::QtOwnership,
                               QScriptEngine::PreferExistingWrapperObject );
}

void dataStreamFromScript( const QScriptValue &object, DataStreamPrototypePtr &stream )
{
    stream = qobject_cast< DataStreamPrototype* >( object.toQObject() );
}

DataStreamPrototype::DataStreamPrototype( QObject *parent )
        : QObject( parent )
{
}

DataStreamPrototype::DataStreamPrototype( const QByteArray &byteArray, QObject *parent )
        : QObject( parent )
{
    QBuffer *buffer = new QBuffer( const_cast<QByteArray*>(&byteArray), this );
    buffer->open( QIODevice::ReadOnly );
    m_dataStream = QSharedPointer< QDataStream >( new QDataStream(buffer) );
}

DataStreamPrototype::DataStreamPrototype( QIODevice *device, QObject *parent )
        : QObject( parent )
{
    if ( !device->isOpen() ) {
        kWarning() << "Device not opened";
    }
    m_dataStream = QSharedPointer< QDataStream >( new QDataStream(device) );
}

DataStreamPrototype::DataStreamPrototype( DataStreamPrototype *other )
        : QObject(), m_dataStream(other->stream())
{
}

QDataStream *DataStreamPrototype::thisDataStream() const
{
    return m_dataStream.data();
}

qint8 DataStreamPrototype::readInt8()
{
    qint8 i;
    thisDataStream()->operator >>( i );
    return i;
}

quint8 DataStreamPrototype::readUInt8()
{
    quint8 i;
    thisDataStream()->operator >>( i );
    return i;
}

quint16 DataStreamPrototype::readUInt16()
{
    quint16 i;
    thisDataStream()->operator >>( i );
    return i;
}

qint32 DataStreamPrototype::readInt32()
{
    qint32 i;
    thisDataStream()->operator >>( i );
    return i;
}

qint16 DataStreamPrototype::readInt16()
{
    qint16 i;
    thisDataStream()->operator >>( i );
    return i;
}

quint32 DataStreamPrototype::readUInt32()
{
    quint32 i;
    thisDataStream()->operator >>( i );
    return i;
}

QString DataStreamPrototype::readString()
{
    QString string;
    char character;
    while ( thisDataStream()->device()->getChar(&character) && character != 0 ) {
        string.append( character );
    }
    return string;
}

QByteArray DataStreamPrototype::readBytes( uint byteCount )
{
    char *chars = new char[ byteCount ];
    const uint bytesRead = thisDataStream()->readRawData( chars, byteCount );
    if ( bytesRead != byteCount ) {
        kWarning() << "Did not read all requested bytes, read" << bytesRead << "of" << byteCount;
    }
    QByteArray bytes( chars );
    delete[] chars;
    return bytes;
}

QByteArray DataStreamPrototype::readBytesUntilZero()
{
    QByteArray bytes;
    char character;
    while ( thisDataStream()->device()->getChar(&character) && character != 0 ) {
        bytes.append( character );
    }
    return bytes;
}

QString NetworkRequest::url() const
{
    QMutexLocker locker( m_mutex );
    return m_url;
}

QString NetworkRequest::userUrl() const
{
    QMutexLocker locker( m_mutex );
    return m_userUrl;
}

bool NetworkRequest::isRunning() const
{
    QMutexLocker locker( m_mutex );
    return m_reply;
}

bool NetworkRequest::isFinished() const
{
    QMutexLocker locker( m_mutex );
    return m_isFinished;
}

bool NetworkRequest::isRedirected() const
{
    QMutexLocker locker( m_mutex );
    return m_redirectUrl.isValid();
}

QUrl NetworkRequest::redirectedUrl() const
{
    QMutexLocker locker( m_mutex );
    return m_redirectUrl;
}

QString NetworkRequest::postData() const
{
    QMutexLocker locker( m_mutex );
    return m_postData;
}

quint64 NetworkRequest::uncompressedSize() const
{
    QMutexLocker locker( m_mutex );
    return m_uncompressedSize;
}

#include "scripting.moc"

}; // namespace Scripting
