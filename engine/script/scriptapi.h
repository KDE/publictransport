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

/** @file
 * @brief This file contains helper classes to be used from service provider plugin scripts.
 *
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef SCRIPTING_HEADER
#define SCRIPTING_HEADER

// Own includes
#include "enums.h"
#include "departureinfo.h" // TODO for eg. PublicTransportInfoList typedef

// Qt includes
#include <QScriptEngine>
#include <QScriptable> // Base class
#include <QUrl>

class PublicTransportInfo;
class ServiceProviderData;
class KConfigGroup;
class QScriptContextInfo;
class QNetworkRequest;
class QReadWriteLock;
class QNetworkReply;
class QNetworkAccessManager;
class QMutex;

/** @brief Stores information about a departure/arrival/journey/stop suggestion. */
typedef QHash<Enums::TimetableInformation, QVariant> TimetableData;

/** @brief Namespace for classes exposed to scripts. */
namespace ScriptApi {

class Network;

/** @ingroup scriptApi
 * @{ */
/**
 * @brief Represents one asynchronous request, created with Network::createRequest().
 *
 * To get notified about new data, connect to either the finished() or the readyRead() signal.
 **/
class NetworkRequest : public QObject {
    Q_OBJECT
    Q_PROPERTY( QString url READ url )
    Q_PROPERTY( QUrl redirectedUrl READ redirectedUrl )
    Q_PROPERTY( bool isRunning READ isRunning )
    Q_PROPERTY( bool isFinished READ isFinished )
    Q_PROPERTY( bool isRedirected READ isRedirected )
    Q_PROPERTY( QString postData READ postData )
    friend class Network;

public:
    /** @brief Creates an invalid request object. Needed for Q_DECLARE_METATYPE. */
    NetworkRequest( QObject* parent = 0 );

    /** @brief Create a new request object for @p url, managed by @p network. */
    explicit NetworkRequest( const QString &url, const QString &userUrl,
                             Network *network, QObject* parent = 0 );

    /** @brief Destructor. */
    virtual ~NetworkRequest();

    typedef QSharedPointer< NetworkRequest > Ptr;

    /**
     * @brief The URL of this request.
     *
     * @note The URL can not be changed, a request object is only used for @em one request.
     **/
    QString url() const;

    /**
     * @brief An URL for this request that should be shown to users.
     * This can be eg. an URL to an HTML document showing the data that is also available
     * at the requested URL, but eg. in XML format.
     * @see url()
     **/
    QString userUrl() const;

    /**
     * @brief Whether or not the request is currently running.
     **/
    bool isRunning() const;

    /**
     * @brief Whether or not the request is finished (successful or not), ie. was running.
     **/
    bool isFinished() const;

    /** @brief Whether or not this request was redirected. */
    bool isRedirected() const;

    /** @brief Get the redirected URL of this request, if any. */
    QUrl redirectedUrl() const;

    /**
     * @brief Set the data to sent to the server when using Network::post().
     *
     * This function automatically sets the "ContentType" header of the request to the used
     * charset. If you want to use another value for the "ContentType" header than the
     * data is actually encoded in, you can change the header using setHeader() after calling
     * this function.
     * @note If the request is already started, no more POST data can be set and this function
     *   will do nothing.
     *
     * @param postData The data to be POSTed.
     * @param charset The charset to encode @p postData with. If charset is an empty string (the
     *   default) the "ContentType" header gets used if it was set using setHeader(). Otherwise
     *   utf8 gets used.
     * @see isRunning()
     **/
    Q_INVOKABLE void setPostData( const QString &postData, const QString &charset = QString() );

    /** @brief Get the data to sent to the server when using Network::post(). */
    QString postData() const;

    /** @brief Get the @p header decoded using @p charset. */
    Q_INVOKABLE QString header( const QString &header, const QString& charset ) const;

    /**
     * @brief Set the @p header of this request to @p value.
     *
     * @note If the request is already started, no more headers can be set and this function
     *   will do nothing.
     *
     * @param header The name of the header to change.
     * @param value The new value for the @p header.
     * @param charset The charset to encode @p header and @p value with. If charset is an empty
     *   string (the default) the "ContentType" header gets used if it was set using setHeader().
     *   Otherwise utf8 gets used.
     * @see isRunning()
     **/
    Q_INVOKABLE void setHeader( const QString &header, const QString &value,
                                const QString &charset = QString() );

    quint64 uncompressedSize() const;

    /**
     * @brief Set custom @p userData for the request.
     *
     * This data is available to slots connected to the finished() signal.
     **/
    Q_INVOKABLE void setUserData( const QVariant &userData );

    /**
     * @brief Get custom user data stored with setUserData().
     **/
    Q_INVOKABLE QVariant userData() const;

public Q_SLOTS:
    /**
     * @brief Aborts this (running) request.
     *
     * @note If the request has not already been started, it cannot be aborted, of course,
     *   and this function will do nothing.
     **/
    void abort();

Q_SIGNALS:
    /**
     * @brief Emitted when this request was started.
     **/
    void started();

    /**
     * @brief Emitted when this request got aborted or timed out.
     * @param timedOut Whether or not the request was aborted because of a timeout.
     **/
    void aborted( bool timedOut = false );

    /**
     * @brief Emitted when this request has finished.
     *
     * @param data The complete data downloaded for this request.
     * @param error @c True, if there was an error executing the request, @c false otherwise.
     * @param errorString A human readable description of the error if @p error is @c true.
     * @param statusCode The HTTP status code that was received or -1 if there was an error.
     * @param size The size in bytes of the received data.
     * @param url The URL of the request.
     * @param userData Custom data stored for the request, see setUserData().
     **/
    void finished( const QByteArray &data = QByteArray(), bool error = false,
                   const QString &errorString = QString(), int statusCode = -1, int size = 0,
                   const QString &url = QString(), const QVariant &userData = QVariant() );

    /**
     * @brief Emitted when new data is available for this request.
     *
     * @param data New downloaded data for this request.
     **/
    void readyRead( const QByteArray &data );

    void redirected( const QUrl &newUrl );

protected Q_SLOTS:
    void slotFinished();
    void slotReadyRead();

protected:
    void started( QNetworkReply* reply, int timeout = 0 );
    QByteArray postDataByteArray() const;
    QNetworkRequest *request() const;
    QByteArray getCharset( const QString &charset = QString() ) const;
    bool isValid() const;

private:
    QMutex *m_mutex;
    const QString m_url;
    const QString m_userUrl;
    QUrl m_redirectUrl;
    Network *m_network;
    bool m_isFinished;
    QNetworkRequest *m_request;
    QNetworkReply *m_reply;
    QByteArray m_data;
    QByteArray m_postData;
    quint32 m_uncompressedSize;
    QVariant m_userData;
};
/** \} */ // @ingroup scriptApi

/** @ingroup scriptApi
 * @{ */
/**
 * @brief Provides network access to scripts.
 *
 * An instance of the Network class is available for scripts as @b network. It can be used to
 * synchronously or asynchronously download data. It doesn't really matter what method the script
 * uses, because every call of a script function from the data engine happens in it's own thread
 * and won't block anything. For the threads ThreadWeaver gets used with jobs of type ScriptJob.
 *
 * To download a document synchronously simply call getSynchronous() with the URL to download.
 * When the download is finished getSynchronous(this request using the POST method) returns and the script can start parsing the
 * document. There is a default timeout of 30 seconds. If a requests takes more time it gets
 * aborted. To define your own timeout you can give it as argument to getSynchronous().
 * @note downloadSynchronous() is an alias for getSynchronous().
 *
 * @code
 * var document = network.getSynchronous( url );
 * // Parse the downloaded document here
 * @endcode
 * @n
 *
 * To create a network request which should be executed asynchronously use createRequest() with
 * the URL of the request as argument. The script should then connect to either the finished()
 * or readyRead() signal of the network request object. The request gets started by calling
 * get() / download(). To only get the headers use head().
 * The example below shows how to asynchronously request a document from within a script:
 * @code
    var request = network.createRequest( url );
    request.finished.connect( handler );
    network.get( request );

    function handler( document ) {
        // Parse the downloaded document here
    };
   @endcode
 * @n
 *
 * If a script needs to use the POST method to request data use post(). The data to be sent in
 * a POST request can be set using NetworkRequest::setPostData().
 *
 * @note One request object created with createRequest() can @em not be used multiple times in
 *   parallel. To start another request create a new request object.
 * @note There is a global 60 seconds timeout for all network requests to finish.
 **/
class Network : public QObject, public QScriptable {
    Q_OBJECT
    Q_PROPERTY( QString lastUrl READ lastUrl )
    Q_PROPERTY( bool lastDownloadAborted READ lastDownloadAborted )
    Q_PROPERTY( QString fallbackCharset READ fallbackCharset )
    Q_PROPERTY( int runningRequestCount READ runningRequestCount )
    friend class NetworkRequest;

public:
    /** @brief The default timeout in milliseconds for network requests. */
    static const int DEFAULT_TIMEOUT = 30000;

    /** @brief Constructor. */
    explicit Network( const QByteArray &fallbackCharset = QByteArray(), QObject* parent = 0 );

    /** @brief Destructor. */
    virtual ~Network();

    /**
     * @brief Get the last requested URL.
     *
     * The last URL gets updated every time a request gets started, eg. using get(), post(),
     * getSynchronous(), download(), downloadSynchronous(), etc.
     **/
    Q_INVOKABLE QString lastUrl() const;

    /**
     * @brief Get an URL for the last requested URL that should be shown to users.
     **/
    Q_INVOKABLE QString lastUserUrl() const;

    /**
     * @brief Clears the last requested URL.
     **/
    Q_INVOKABLE void clear();

    /**
     * @brief Returns @c true, if the last download was aborted before it was ready.
     *
     * Use lastUrl() to get the URL of the aborted download. Downloads may be aborted eg. by
     * closing plasma.
     **/
    Q_INVOKABLE bool lastDownloadAborted() const;

    /**
     * @brief Download the document at @p url synchronously.
     *
     * @param url The URL to download.
     * @param userUrl An URL for this request that should be shown to users.
     *   This can be eg. an URL to an HTML document showing the data that is also available
     *   at the requested URL, but eg. in XML format.
     * @param timeout Maximum time in milliseconds to wait for the reply to finish.
     *   If smaller than 0, no timeout gets used.
     *   Note that there is a global 60 seconds timeout for all network requests to finish.
     **/
    Q_INVOKABLE QByteArray getSynchronous( const QString &url, const QString &userUrl = QString(),
                                           int timeout = DEFAULT_TIMEOUT );

    /**
     * @brief This is an alias for getSynchronous().
     **/
    Q_INVOKABLE inline QByteArray downloadSynchronous( const QString &url,
                                                       const QString &userUrl = QString(),
                                                       int timeout = DEFAULT_TIMEOUT )
    {
        return getSynchronous(url, userUrl, timeout);
    };

    /**
     * @brief Creates a new NetworkRequest for asynchronous network access.
     *
     * @note Each NetworkRequest object can only be used once for one download.
     * @see get, download, post, head
     **/
    Q_INVOKABLE NetworkRequest *createRequest( const QString &url,
                                               const QString &userUrl = QString() );

    /**
     * @brief Perform the network @p request asynchronously.
     *
     * @param request The NetworkRequest object created with createRequest().
     * @param timeout Maximum time in milliseconds to wait for the reply to finish.
     *   If smaller than 0, no timeout gets used.
     *   Note that there is a global 60 seconds timeout for all network requests to finish.
     **/
    Q_INVOKABLE void get( NetworkRequest *request, int timeout = DEFAULT_TIMEOUT );

    /**
     * @brief Perform the network @p request asynchronously using POST method.
     *
     * @param request The NetworkRequest object created with createRequest().
     * @param timeout Maximum time in milliseconds to wait for the reply to finish.
     *   If smaller than 0, no timeout gets used.
     *   Note that there is a global 60 seconds timeout for all network requests to finish.
     **/
    Q_INVOKABLE void post( NetworkRequest *request, int timeout = DEFAULT_TIMEOUT );

    /**
     * @brief Perform the network @p request asynchronously, but only get headers.
     *
     * @param request The NetworkRequest object created with createRequest().
     * @param timeout Maximum time in milliseconds to wait for the reply to finish.
     *   If smaller than 0, no timeout gets used.
     *   Note that there is a global 60 seconds timeout for all network requests to finish.
     **/
    Q_INVOKABLE void head( NetworkRequest *request, int timeout = DEFAULT_TIMEOUT );

    /**
     * @brief This is an alias for get().
     **/
    Q_INVOKABLE inline void download( NetworkRequest *request, int timeout = DEFAULT_TIMEOUT ) {
        get( request, timeout );
    };

    /**
     * @brief Returns whether or not there are network requests running in the background.
     * @see runningRequests
     **/
    Q_INVOKABLE bool hasRunningRequests() const;

    /**
     * @brief Get a list of all running asynchronous requests.
     *
     * If hasRunningRequests() returns @c false, this will return an empty list.
     * If hasRunningRequests() returns @c true, this @em may return a non-empty list (maybe only
     * a synchronous request is running).
     * @see hasRunningRequests
     **/
    Q_INVOKABLE QList< NetworkRequest::Ptr > runningRequests() const;

    /**
     * @brief Returns the number of currently running requests.
     *
     * @note This includes running synchronous requests. The number of request objects returned
     *   by runningRequests() may be less than the number returned here, because it only returns
     *   running asynchronous requests.
     **/
    Q_INVOKABLE int runningRequestCount() const;

    /**
     * @brief Returns the charset to use for decoding documents, if it cannot be detected.
     *
     * The fallback charset can be selected in the XML file, as \<fallbackCharset\>-tag.
     **/
    Q_INVOKABLE QByteArray fallbackCharset() const;

Q_SIGNALS:
    /**
     * @brief Emitted when an asynchronous request has been started.
     *
     * This signal is @em not emitted if the network gets accessed synchronously.
     * @param request The request that has been started.
     * @see synchronousRequestStarted()
     **/
    void requestStarted( const NetworkRequest::Ptr &request );

    /**
     * @brief Emitted when an asynchronous request has finished.
     *
     * This signal is @em not emitted if the network gets accessed synchronously.
     * @param request The request that has finished.
     * @param data Received data decoded to a string.
     * @param error @c True, if there was an error executing the request, @c false otherwise.
     * @param errorString A human readable description of the error if @p error is @c true.
     * @param timestamp The date and time on which the request was finished.
     * @param statusCode The HTTP status code received.
     * @param size The size in bytes of the received data.
     * @see synchronousRequestFinished()
     **/
    void requestFinished( const NetworkRequest::Ptr &request,
                          const QByteArray &data = QByteArray(),
                          bool error = false, const QString &errorString = QString(),
                          const QDateTime &timestamp = QDateTime(),
                          int statusCode = 200, int size = 0 );

    /** @brief Emitted when an asynchronous @p request was redirect to @p newUrl. */
    void requestRedirected( const NetworkRequest::Ptr &request, const QUrl &newUrl );

    /**
     * @brief Emitted when a synchronous request has been started.
     *
     * This signal is @em not emitted if the network gets accessed asynchronously.
     * @param url The URL of the request that has been started.
     * @see requestStarted()
     **/
    void synchronousRequestStarted( const QString &url );

    /**
     * @brief Emitted when an synchronous request has finished.
     *
     * This signal is @em not emitted if the network gets accessed asynchronously.
     * @param url The URL of the request that has finished.
     * @param data Received data decoded to a string.
     * @param cancelled Whether or not the request has been cancelled, eg. because of a timeout.
     * @param statusCode The HTTP status code if @p cancelled is @c false.
     * @param waitTime The time spent waiting for the download to finish.
     * @param size The size in bytes of the received data.
     * @see requestFinished()
     **/
    void synchronousRequestFinished( const QString &url, const QByteArray &data = QByteArray(),
                                     bool cancelled = false, int statusCode = 200,
                                     int waitTime = 0, int size = 0 );

    /**
     * @brief Emitted when a synchronous request has been redirected.
     *
     * This signal is @em not emitted if the network gets accessed asynchronously.
     * @param url The URL of the request that has been redirected.
     * @see requestRedirected()
     **/
    void synchronousRequestRedirected( const QString &url );

    /**
     * @brief Emitted when all running requests are finished.
     *
     * Emitted when a request finishes and there are no more running requests,
     * ie. hasRunningRequests() returns @c false.
     **/
    void allRequestsFinished();

    /**
     * @brief Emitted when an asynchronous request got aborted.
     *
     * This signal is @em not emitted if the network gets accessed synchronously.
     * @param request The request that was aborted.
     **/
    void requestAborted( const NetworkRequest::Ptr &request );

    /**
     * @brief Emitted to abort all running synchronous requests.
     *
     * This signal gets emitted by abortSynchronousRequests().
     **/
    void doAbortSynchronousRequests();

public Q_SLOTS:
    /**
     * @brief Abort all running requests (synchronous and asynchronous).
     **/
    void abortAllRequests();

    /**
     * @brief Abort all running synchronous requests.
     **/
    void abortSynchronousRequests();

protected Q_SLOTS:
    void slotRequestStarted();
    void slotRequestFinished( const QByteArray &data = QByteArray(), bool error = false,
                              const QString &errorString = QString(), int statusCode = -1,
                              int size = 0 );
    void slotRequestAborted();
    void slotRequestRedirected( const QUrl &newUrl );

protected:
    bool checkRequest( NetworkRequest *request );
    NetworkRequest::Ptr getSharedRequest( NetworkRequest *request ) const;
    void emitSynchronousRequestFinished( const QString &url, const QByteArray &data = QByteArray(),
                                         bool cancelled = false, int statusCode = 200,
                                         int waitTime = 0, int size = 0 );

private:
    QMutex *m_mutex;
    const QByteArray m_fallbackCharset;
    QNetworkAccessManager *m_manager;
    bool m_quit;
    int m_synchronousRequestCount;
    QString m_lastUrl;
    QString m_lastUserUrl;
    bool m_lastDownloadAborted;
    QList< NetworkRequest::Ptr > m_requests;
    QList< NetworkRequest::Ptr > m_finishedRequests;
};
/** \} */ // @ingroup scriptApi

/** @ingroup scriptApi
 * @{ */
/**
 * @brief A helper class for scripts.
 *
 * An instance of this class gets published to scripts as <em>"helper"</em>.
 * Scripts can use it's functions, like here:
 * @code
 * var stripped = helper.stripTags("<div>Test</div>");
 * // stripped == "Test"
 *
 * var timeValues = helper.matchTime( "15:28" );
 * // timeValues == { hour: 15, minutes: 28, error: false }
 *
 * var timeString = helper.formatTime( timeValues[0], timeValues[1] );
 * // timeString == "15:28"
 *
 * var duration = helper.duration("15:20", "15:45");
 * // duration == 25
 *
 * var time2 = helper.addMinsToTime("15:20", duration);
 * // time2 == "15:45"
 *
 * helper.debug("Debug message, eg. something unexpected happened");
 * @endcode
 **/
class Helper : public QObject, protected QScriptable {
    Q_OBJECT
    Q_ENUMS( ErrorSeverity )

public:
    /** @brief The severity of an error. */
    enum ErrorSeverity {
        Information, /**< The message is only an information. */
        Warning, /**< The message is a warning. */
        Fatal /**< The message describes a fatal error. */
    };

    /**
     * @brief Creates a new helper object.
     *
     * @param serviceProviderId The ID of the service provider this Helper object is created for.
     * @param parent The parent object.
     **/
    explicit Helper( const QString &serviceProviderId, QObject* parent = 0 );

    virtual ~Helper();

    /**
     * @brief Prints an information @p message on stdout and logs it in a file.
     *
     * Logs the message with the given @p failedParseText string, eg. the HTML code where parsing
     * failed. The message gets also send to stdout with a short version of the data.
     * The log file is normally located at "~/.kde4/share/apps/plasma_engine_publictransport/serviceproviders.log".
     * If the same message gets sent repeatedly, it gets sent only once.
     *
     * @param message The information message.
     * @param failedParseText The text in the source document to which the message applies.
     **/
    Q_INVOKABLE void information( const QString &message, const QString &failedParseText = QString() ) {
            messageReceived(message, failedParseText, Information); };

    /**
     * @brief Prints a warning @p message on stdout and logs it in a file.
     *
     * Logs the message with the given @p failedParseText string, eg. the HTML code where parsing
     * failed. The message gets also send to stdout with a short version of the data.
     * The log file is normally located at "~/.kde4/share/apps/plasma_engine_publictransport/serviceproviders.log".
     * If the same message gets sent repeatedly, it gets sent only once.
     *
     * @param message The warning message.
     * @param failedParseText The text in the source document to which the message applies.
     **/
    Q_INVOKABLE void warning( const QString &message, const QString &failedParseText = QString() ) {
            messageReceived(message, failedParseText, Warning); };

    /**
     * @brief Prints an error @p message on stdout and logs it in a file.
     *
     * Logs the message with the given @p failedParseText string, eg. the HTML code where parsing
     * failed. The message gets also send to stdout with a short version of the data.
     * The log file is normally located at "~/.kde4/share/apps/plasma_engine_publictransport/serviceproviders.log".
     * If the same message gets sent repeatedly, it gets sent only once.
     *
     * @note If the error is fatal consider using @c throw instead to create an exception and abort
     *   script execution.
     *
     * @param message The error message.
     * @param failedParseText The text in the source document to which the message applies.
     **/
    Q_INVOKABLE void error( const QString &message, const QString &failedParseText = QString() ) {
            messageReceived(message, failedParseText, Fatal); };

    /**
     * @brief Decodes HTML entities in @p html.
     *
     * For example "&nbsp;" gets replaced by " ".
     * HTML entities which include a charcode, eg. "&#100;" are also replaced, in the example
     * by the character for the charcode 100, ie. QChar(100).
     *
     * @param html The string to be decoded.
     * @return @p html with decoded HTML entities.
     **/
    Q_INVOKABLE static QString decodeHtmlEntities( const QString &html );

    /** @brief Encodes HTML entities in @p html, e.g. "<" is replaced by "&lt;". */
    Q_INVOKABLE static QString encodeHtmlEntities( const QString& html );

    /**
     * @brief Decodes the given HTML document.
     *
     * First it tries QTextCodec::codecForHtml().
     * If that doesn't work, it parses the document for the charset in a meta-tag.
     **/
    Q_INVOKABLE static QString decodeHtml( const QByteArray& document,
                                           const QByteArray& fallbackCharset = QByteArray() );

    /** @brief Decode @p document using @p charset. */
    Q_INVOKABLE static QString decode( const QByteArray& document,
                                       const QByteArray& charset = QByteArray() );

    /**
     * @brief Decode @p document using @p charset.
     * This is needed for scripts that use a string as argument. If the @p charset argument gets
     * converted to QByteArray from a script string value automatically, it will be empty.
     **/
    Q_INVOKABLE static QString decode( const QByteArray& document, const QString& charset ) {
        return decode( document, charset.toAscii() );
    };

    /**
     * @brief Trims spaces from the beginning and the end of the given string @p str.
     *
     * @note The HTML entitiy <em>&nbsp;</em> is also trimmed.
     *
     * @param str The string to be trimmed.
     * @return @p str without spaces at the beginning or end.
     **/
    Q_INVOKABLE static QString trim( const QString &str );

    /**
     * @brief Like trim(), additionally removes double whitespace in the middle of @p str.
     *
     * @note All occurrences of the HTML entitiy <em>&nbsp;</em> are also removes.
     *
     * @param str The string to be simplified.
     * @return @p str without whitespace in the middle, at the beginning or end.
     **/
    Q_INVOKABLE static QString simplify( const QString &str );

    /**
     * @brief Removes all HTML tags from str.
     *
     * This function works with attributes which contain a closing tab as strings.
     *
     * @param str The string from which the HTML tags should be removed.
     * @return @p str without HTML tags.
     **/
    Q_INVOKABLE static QString stripTags( const QString &str );

    /**
     * @brief Makes the first letter of each word upper case, all others lower case.
     *
     * @param str The input string.
     * @return @p str in camel case.
     **/
    Q_INVOKABLE static QString camelCase( const QString &str );

    /**
     * @brief Extracts a block from @p str, which begins at the first occurrence of @p beginString
     *   in @p str and end at the first occurrence of @p endString in @p str.
     *
     * @deprecated Does only work with fixed strings. Use eg. findFirstHtmlTag() instead.
     * @bug The returned string includes beginString but not endString.
     *
     * @param str The input string.
     * @param beginString A string to search for in @p str and to use as start position.
     * @param endString A string to search for in @p str and to use as end position.
     * @return The text block in @p str between @p beginString and @p endString.
     **/
    Q_INVOKABLE static QString extractBlock( const QString &str,
            const QString &beginString, const QString &endString );

    /**
     * @brief Get a map with the hour and minute values parsed from @p str using @p format.
     *
     * QVariantMap gets converted to an object in scripts. The result can be used in the script
     * like this:
     * @code
        var time = matchTime( "15:23" );
        if ( !time.error ) {
            var hour = time.hour;
            var minute = time.minute;
        }
       @endcode
     *
     * @param str The string containing the time to be parsed, eg. "08:15".
     * @param format The format of the time string in @p str. Default is "hh:mm".
     * @return A map with two values: 'hour' and 'minute' parsed from @p str. On error it contains
     *   an 'error' value of @c true.
     * @see formatTime
     **/
    Q_INVOKABLE static QVariantMap matchTime( const QString &str, const QString &format = "hh:mm" );

    /**
     * @brief Get a date object parsed from @p str using @p format.
     *
     * @param str The string containing the date to be parsed, eg. "2010-12-01".
     * @param format The format of the time string in @p str. Default is "YY-MM-dd".
     * @return The matched date.
     * @see formatDate
     **/
    Q_INVOKABLE static QDate matchDate( const QString &str, const QString &format = "yyyy-MM-dd" );

    /**
     * @brief Formats the time given by the values @p hour and @p minute
     *   as string in the given @p format.
     *
     * @param hour The hour value of the time.
     * @param minute The minute value of the time.
     * @param format The format of the time string to return. Default is "hh:mm".
     * @return The formatted time string.
     * @see matchTime
     **/
    Q_INVOKABLE static QString formatTime( int hour, int minute, const QString &format = "hh:mm" );

    /**
     * @brief Formats the time given by the values @p hour and @p minute
     *   as string in the given @p format.
     *
     * @param year The year value of the date.
     * @param month The month value of the date.
     * @param day The day value of the date.
     * @param format The format of the date string to return. Default is "yyyy-MM-dd".
     * @return The formatted date string.
     * @see matchTime
     **/
    Q_INVOKABLE static QString formatDate( int year, int month, int day,
                                           const QString &format = "yyyy-MM-dd" );

    /**
     * @brief Formats @p dateTime using @p format.
     **/
    Q_INVOKABLE static QString formatDateTime( const QDateTime &dateTime,
                                               const QString &format = "yyyy-MM-dd" );

    /**
     * @brief Calculates the duration in minutes from the time in @p time1 until @p time2.
     *
     * @param time1 A string with the start time, in the given @p format.
     * @param time2 A string with the end time, in the given @p format.
     * @param format The format of @p time1 and @p time2. Default is "hh:mm".
     * @return The number of minutes from @p time1 until @p time2. If @p time2 is earlier than
     *   @p time1 a negative value gets returned.
     **/
    Q_INVOKABLE static int duration( const QString &time1, const QString &time2,
                                     const QString &format = "hh:mm" );

    /**
     * @brief Adds @p minsToAdd minutes to the given @p time.
     *
     * @param time A string with the base time.
     * @param minsToAdd The number of minutes to add to @p time.
     * @param format The format of @p time. Default is "hh:mm".
     * @return A time string formatted in @p format with the calculated time.
     **/
    Q_INVOKABLE static QString addMinsToTime( const QString &time, int minsToAdd,
                                              const QString &format = "hh:mm" );

    /**
     * @brief Adds @p daysToAdd days to the date in @p dateTime.
     *
     * @param dateTime A string with the base time.
     * @param daysToAdd The number of minutes to add to @p time.
     * @param format The format of @p time. Default is "hh:mm".
     * @return A time string formatted in @p format with the calculated time.
     **/
    Q_INVOKABLE static QDateTime addDaysToDate( const QDateTime &dateTime, int daysToAdd );

    /**
     * @brief Adds @p daysToAdd days to @p date.
     *
     * @param date A string with the base date.
     * @param daysToAdd The number of days to add to @p date.
     * @param format The format of @p date. Default is "yyyy-MM-dd".
     * @return A date string formatted in @p format with the calculated date.
     **/
    Q_INVOKABLE static QString addDaysToDate( const QString &date, int daysToAdd,
                                              const QString &format = "yyyy-MM-dd" );

    /**
     * @brief Splits @p str at @p sep, but skips empty parts.
     *
     * @param string The string to split.
     * @param separator The separator.
     * @return A list of string parts.
     **/
    Q_INVOKABLE static QStringList splitSkipEmptyParts( const QString &string,
                                                        const QString &separator );

    /**
     * @brief Finds the first occurrence of an HTML tag with @p tagName in @p str.
     *
     * @param str The string containing the HTML tag to be found.
     * @param tagName The name of the HTML tag to be found, ie. &lt;tagName&gt;.
     * @param options The same as in findHtmlTags(), "maxCount" will be set to 1.
     *
     * @b Example:
     * @code
     * // This matches the first &lt;table&gt; tag found in html
     * // which has a class attribute with a value that matches
     * // the regular expression pattern "test\\d+",
     * // eg. "test1", "test2", ...
     * var result = helper.findFirstHtmlTag( html, "table",
     *         {attributes: {"class": "test\\d+"}} );
     * @endcode
     *
     * @return A map with properties like in findHtmlTags(). Additionally these properties are
     *   returned:
     *   @li @b found: A boolean, @c true if the tag was found, @c false otherwise.
     * @see findHtmlTags
     **/
    Q_INVOKABLE static QVariantMap findFirstHtmlTag( const QString &str, const QString &tagName,
                                                     const QVariantMap &options = QVariantMap() );

    /**
     * @brief This is an overloaded function which expects a value for "tagName" in @p options.
     * @overload QVariantMap findFirstHtmlTag(const QString&,const QString&,const QVariantMap&)
     **/
    Q_INVOKABLE static inline QVariantMap findFirstHtmlTag(
            const QString &str, const QVariantMap &options = QVariantMap() )
    {
        return findFirstHtmlTag( str, options["tagName"].toString(), options );
    };

    /**
     * @brief Find all occurrences of (top level) HTML tags with @p tagName in @p str.
     *
     * Using this function avoids having to deal with various problems when matching HTML elements:
     * @li Nested HTML elements with the same @p tagName. When simply searching for the first
     *     closing tag after the found opening tag, a nested closing tag gets matched. If you are
     *     sure that there are no nested tags or if you want to only match until the first nested
     *     closing tag set the option "noNesting" in @p options to @c true.
     * @li Matching tags with specific attributes. This function extracts all attributes of a
     *     matched tag. They can have values, which can be put in single/double/no quotation marks.
     *     To only match tags with specific attributes, add them to the "attributes" option in
     *     @p options. Regular expressions can be used to match the attribute name and value
     *     independently. Attribute order does not matter.
     * @li Matching HTML tags correctly. For example a ">" inside an attributes value could cause
     *     problems and have the tag cut off there.
     *
     * @note This function only returns found top level tags. These found tags may contain matching
     *   child tags. You can use this function again on the contents string of a found top level
     *   tag to find its child tags.
     *
     * @b Example:
     * @code
     * // This matches all &lt;div&gt; tags found in html which
     * // have a class attribute with the value "test" and only
     * // numbers as contents, eg. "<div class='test'>42</div>".
     * var result = helper.findHtmlTags( html, "div",
     *         {attributes: {"class": "test"},
     *          contentsRegExp: "\\d+"} );
     * @endcode
     *
     * @param str The string containing the HTML tags to be found.
     * @param tagName The name of the HTML tags to be found, ie. &lt;tagName&gt;.
     * @param options A map with these properties:
     *   @li @b attributes: A map containing all required attributes and it's values. The keys of that
     *     map are the names of required attributes and can be regular expressions. The values
     *     are the values of the required attributes and are also handled as regular expressions.
     *   @li @b contentsRegExp: A regular expression pattern which the contents of found HTML tags
     *     must match. If it does not match, that tag does not get returned as found.
     *     If no parenthesized subexpressions are present in this regular expression, the whole
     *     matching string gets used as contents. If more than one parenthesized subexpressions
     *     are found, only the first one gets used. The regular expression gets matched case
     *     insensitive. By default all content of the HTML tag gets matched.
     *   @li @b position: An integer, where to start the search for tags. If the position is inside
     *     a top-level matching tag, its child tags will be matched and other following top-level
     *     tags. This is 0 by default.
     *   @li @b noContent: A boolean, @c false by default. If @c true, HTML tags without any
     *     content are matched, eg. "br" or "img" tags. Otherwise tags need to be closed to get
     *     matched.
     *   @li @b noNesting: A boolean, @c false by default. If @c true, no checks will be made to
     *     ensure that the first found closing tag belongs to the opening tag. In this case the
     *     found contents always end after the first closing tag after the opening tag, no matter
     *     if the closing tag belongs to a nested tag or not. By setting this to @c true you can
     *     enhance performance.
     *   @li @b maxCount: The maximum number of HTML tags to match or 0 to match any number of HTML tags.
     *   @li @b debug: A boolean, @c false by default. If @c true, more debug output gets generated.
     *
     * @return A list of maps, each map represents one found tag and has these properties:
     *   @li @b contents: A string, the contents of the found tag (if found is @c true).
     *   @li @b position: An integer, the position of the first character of the found tag
     *     (ie. '<') in @p str (if found is @c true).
     *   @li @b endPosition: An integer, the position of the first character after the found end
     *     tag in @p str (if found is @c true).
     *   @li @b attributes: A map containing all found attributes of the tag and it's values (if
     *     found is @c true). The attribute names are the keys of the map, while the attribute
     *     values are the values of the map.
     **/
    Q_INVOKABLE static QVariantList findHtmlTags( const QString &str, const QString &tagName,
                                                  const QVariantMap &options = QVariantMap() );

    /**
     * @brief This is an overloaded function which expects a value for "tagName" in @p options.
     * @overload QVariantMap findHtmlTags(const QString&,const QString&,const QVariantMap&)
     **/
    Q_INVOKABLE static inline QVariantList findHtmlTags(
            const QString &str, const QVariantMap &options = QVariantMap() )
    {
        return findHtmlTags( str, options["tagName"].toString(), options );
    };

    /**
     * @brief Finds all occurrences of HTML tags with @p tagName in @p str.
     *
     * This function uses findHtmlTags() to find the HTML tags and then extracts a name for each
     * found tag from @p str.
     * Instead of returning a list of all matched tags, a map is returned, with the found names as
     * keys and the tag objects (as returned in a list by findHtmlTags()) as values.
     *
     * @b Example:
     * @code
     * // In this example the findNamedHtmlTags() function gets
     * // used in a while loop to find columns (<td>-tags) in rows
     * // (<tr>-tags) and assign names to the found columns. This
     * // makes it easy to parse HTML tables.
     *
     * // Initialize position value
     * var tableRow = { position: -1 };
     *
     * // Use findFirstHtmlTag() with the current position value to
     * // find the next <tr> tag. The return value contains the
     * // updated position and the boolean property "found"
     * while ( (tableRow = helper.findFirstHtmlTag(html, "tr",
     *                     {position: tableRow.position + 1})).found )
     * {
     *     // Find columns, ie. <td> tags in a table row.
     *     // Each column gets a name assigned by findNamedHtmlTags().
     *     // Ambiguous names are resolved by adding/increasing a
     *     // number after the name. The names get extracted from the
     *     // <td>-tags class attributes, by matching the given
     *     // regular expression.
     *     var columns = helper.findNamedHtmlTags( tableRow.contents, "td",
     *           {ambiguousNameResolution: "addNumber",
     *            namePosition: {type: "attribute", name: "class",
     *                           regexp: "([\\w]+)\\d+"}} );
     *
     *     // Check the "names" property of the return value,
     *     // should contain the names of all columns that are needed
     *     if ( !columns.names.contains("time") ) {
     *         // Notify the data engine about an error,
     *         // if a "time" column was expected
     *         helper.error("Didn't find all needed columns! " +
     *                      "Found: " + columns.names, tableRow.contents);
     *         continue;
     *     }
     *
     *     // Now read the contents of the columns
     *     var time = columns["time"].contents;
     *
     *     // Read more and add data to the result set using result.addData()
     * }
     * @endcode
     *
     * @param str The string containing the HTML tag to be found.
     * @param tagName The name of the HTML tag to be found, ie. &lt;tagName&gt;.
     * @param options The same as in findHtmlTags(), but @em additionally these options can be used:
     *     @li @b namePosition: A map with more options, indicating the position of the name of tags:
     *       @li @em type: Can be @em "contents" (ie. use tag contents as name, the default) or
     *         @em "attribute" (ie. use a tag attribute value as name). If @em "attribute" is used
     *         for @em "type", the name of the attribute can be set as @em "name" property.
     *         Additionally a @em "regexp" property can be used to extract a string from the string
     *         that would otherwise be used as name as is.
     *       @li @em ambiguousNameResolution: Can be used to tell what should be done if the same name
     *         was found multiple times. This can currently be one of: @em "addNumber" (adds a
     *         number to the name, ie. "..1", "..2")., @em "replace" (a later match with an already
     *         matched name overwrites the old match, the default).
     *
     * @return A map with the found names as keys and the tag objects as values. @em Additionally
     *   these properties are returned:
     *   @li @b names: A list of all found tag names.
     * @see findHtmlTags
     **/
    Q_INVOKABLE static QVariantMap findNamedHtmlTags( const QString &str, const QString &tagName,
                                                      const QVariantMap &options = QVariantMap() );

signals:
    /**
     * @brief An error was received from the script.
     *
     * @param message The error message.
     * @param failedParseText The text in the source document where parsing failed.
     * @param severity The severity of the error.
     **/
    void messageReceived( const QString &message, const QScriptContextInfo &contextInfo,
                        const QString &failedParseText = QString(),
                        Helper::ErrorSeverity severity = Warning );

private:
    static QString getTagName( const QVariantMap &searchResult, const QString &type = "contents",
            const QString &regExp = QString(), const QString attributeName = QString() );
    void messageReceived( const QString &message, const QString &failedParseText,
                          Helper::ErrorSeverity severity );
    void emitRepeatedMessageWarning();

    QMutex *m_mutex;
    QString m_serviceProviderId;
    QString m_lastErrorMessage;
    int m_errorMessageRepetition;
};
/** \} */ // @ingroup scriptApi

/** @ingroup scriptApi
 * @{ */
/**
 * @brief This class is used by scripts to store results in, eg. departures.
 *
 * An instance of this class gets published to scripts as @b result.
 * Scripts can use it to add items to the result set, ie. departures/arrivals/journeys/
 * stop suggestions. Items can be added from scripts using addData().
 * @code
 * // Add stop suggestion data to result set
 * result.addData({ StopName: "Name" });
 * @endcode
 **/
class ResultObject : public QObject, protected QScriptable {
    Q_OBJECT
    Q_ENUMS( Feature Hint )
    Q_FLAGS( Features Hints )
    Q_PROPERTY( int count READ count )

public:
    /**
     * @brief Used to store enabled features.
     *
     * The meta object of ResultObject gets published to scripts under the name "enum" and contains
     * this enumeration.
     *
     * @see enableFeature()
     * @see isFeatureEnabled()
     **/
    enum Feature {
        NoFeature   = 0x00, /**< No feature is enabled. */
        AutoPublish = 0x01, /**< Automatic publishing of the first few data items. Turn
                * this off if you want to call publish() manually. */
        AutoDecodeHtmlEntities
                    = 0x02, /**< Automatic decoding of HTML entities in strings and string
                * lists. If you are sure, that there are no HTML entities in the strings parsed
                * from the downloaded documents, you can turn this feature off. You can also
                * manually decode HTML entities using Helper::decodeHtmlEntities(). */
        AutoRemoveCityFromStopNames
                    = 0x04, /**< Automatic removing of city names from all stop names, ie.
                * stop names in eg. RouteStops or Target). Scripts can help the data engine with
                * this feature with the hints CityNamesAreLeft or CityNamesAreRight.
                * @code
                * result.giveHint( enum.CityNamesAreLeft );
                * result.giveHint( enum.CityNamesAreRight );
                * @endcode
                * @see Hint */
        AllFeatures = AutoPublish | AutoDecodeHtmlEntities | AutoRemoveCityFromStopNames,
                /**< All available features are enabled. */
        DefaultFeatures = AutoPublish | AutoDecodeHtmlEntities
                /**< The default set of features gets enabled. */
    };
    Q_DECLARE_FLAGS( Features, Feature )

    /**
     * @brief Can be used by scripts to give hints to the data engine.
     *
     * The meta object of ResultObject gets published to scripts as @b enum and contains this
     * enumeration.
     *
     * @see giveHint()
     * @see isHintGiven()
     **/
    enum Hint {
        NoHint              = 0x00, /**< No hints given. */
        DatesNeedAdjustment = 0x01, /**< Dates are set from today, not the requested date. They
                * need to be adjusted by X days, where X is the difference in days between today
                * and the requested date. */
        NoDelaysForStop     = 0x02, /**< Delays are not available for the current stop, although
                * delays are available for other stops. */
        CityNamesAreLeft    = 0x04, /**< City names are most likely on the left of stop names. */
        CityNamesAreRight   = 0x08  /**< City names are most likely on the right of stop names. */
    };
    Q_DECLARE_FLAGS( Hints, Hint )

    /**
     * @brief Creates a new ResultObject instance.
     **/
    ResultObject( QObject* parent = 0 );

    virtual ~ResultObject();

    /**
     * @brief Get the list of stored TimetableData objects.
     *
     * @return The list of stored TimetableData objects.
     **/
    QList< TimetableData > data() const;

    Q_INVOKABLE inline QVariant data( int index, int information ) const {
        return data( index, static_cast<Enums::TimetableInformation>(information) );
    };
    Q_INVOKABLE QVariant data( int index, Enums::TimetableInformation information ) const;

    /**
     * @brief Checks whether or not the list of TimetableData objects is empty.
     *
     * @return @c True, if the list of TimetableData objects isn't empty. @c False, otherwise.
     **/
    Q_INVOKABLE bool hasData() const;

    /**
     * @brief Returns the number of timetable elements currently in the resultset.
     **/
    Q_INVOKABLE int count() const;

    /**
     * @brief Whether or not @p feature is enabled.
     *
     * Script example:
     * @code
     * if ( result.isFeatureEnabled(enum.AutoPublish) ) {
     *    // Do something when the AutoPublish feature is enabled
     * }
     * @endcode
     *
     * By default all features are enabled.
     *
     * @param feature The feature to check. Scripts can access the Feature enumeration
     *   under the name "enum".
     *
     * @see Feature
     **/
    Q_INVOKABLE bool isFeatureEnabled( Feature feature ) const;

    /**
     * @brief Set whether or not @p feature is @p enabled.
     *
     * Script example:
     * @code
     * // Disable the AutoPublish feature
     * result.enableFeature( enum.AutoPublish, false );
     * @endcode
     *
     * By default all features are enabled, disable unneeded features for better performance.
     *
     * @param feature The feature to enable/disable. Scripts can access the Feature enumeration
     *   under the name "enum".
     * @param enable @c True to enable @p feature, @c false to disable it.
     *
     * @see Feature
     **/
    Q_INVOKABLE void enableFeature( Feature feature, bool enable = true );

    /**
     * @brief Test if the given @p hint is set.
     *
     * Script example:
     * @code
     * if ( result.isHintGiven(enum.CityNamesAreLeft) ) {
     *    // Do something when the CityNamesAreLeft hint is given
     * }
     * @endcode
     *
     * By default no hints are set.
     *
     * @param hint The hint to check. Scripts can access the Hint enumeration
     *   under the name "enum".
     */
    Q_INVOKABLE bool isHintGiven( Hint hint ) const;

    /**
     * @brief Set the given @p hint to @p enable.
     *
     * Script example:
     * @code
     * // Remove the CityNamesAreLeft hint
     * result.giveHint( enum.CityNamesAreLeft, false );
     * @endcode
     *
     * By default no hints are set.
     *
     * @param hint The hint to give.
     * @param enable Whether the @p hint should be set or unset.
     */
    Q_INVOKABLE void giveHint( Hint hint, bool enable = true );

    /**
     * @brief Get the currently enabled features.
     *
     * Scripts can access the Features enumeration like @verbatimenum.AutoPublish@endverbatim.
     * By default this equals to DefaultFeatures.
     */
    Features features() const;

    /**
     * @brief Get the currently set hints.
     *
     * Scripts can access the Hints enumeration like @verbatimenum.CityNamesAreLeft@endverbatim.
     * By default this equals to NoHints.
     */
    Hints hints() const;

    static void dataList( const QList< TimetableData > &dataList,
                          PublicTransportInfoList *infoList, ParseDocumentMode parseMode,
                          Enums::VehicleType defaultVehicleType, const GlobalTimetableInfo *globalInfo,
                          ResultObject::Features features, ResultObject::Hints hints );

Q_SIGNALS:
    /**
     * @brief Can be called by scripts to trigger the data engine to publish collected data.
     *
     * This does not need to be called by scripts, the data engine will publish all collected data,
     * when the script returns and all network requests are finished. After the first ten items
     * have been added, this signal is emitted automatically, if the AutoPublish feature is
     * enabled (the default). Use @verbatimresult.enableFeature(AutoPublish, false)@endverbatim to
     * disable this feature.
     *
     * If collecting data takes too long, calling this signal causes the data collected so far
     * to be published immediately. Good reasons to call this signal are eg. because additional
     * documents need to be downloaded or because a very big document gets parsed. Visualizations
     * connected to the data engine will then receive data not completely at once, but step by
     * step.
     *
     * It also means that the first data items are published to visualizations faster. A good idea
     * could be to only call publish() after the first few data items (similar to the AutoPublish
     * feature). That way visualizations get the first dataset very quickly, eg. the data that
     * fits into the current view. Remaining data will then be added after the script is finished.
     *
     * @note Do not call publish() too often, because it causes some overhead. Visualizations
     *   will get notified about the updated data source and process it at whole, ie. not only
     *   newly published items but also the already published items again. Publishing data in
     *   groups of less than ten items will be too much in most situations. But if eg. another
     *   document needs to be downloaded to make more data available, it is a good idea to call
     *   publish() before starting the download (even with less than ten items).
     *   Use count() to see how many items are collected so far.
     *
     * @see Feature
     * @see setFeatureEnabled
     **/
    void publish();

    /**
     * @brief Emitted when invalid data gets received through the addData() method.
     *
     * @param info The TimetableInformation which was invalid in @p map.
     * @param errorMessage An error message explaining why the data for @p info in @p map
     *   is invalid.
     * @param context The script context from which addData() was called.
     * @param index The target index at which the data in @p map will be inserted into this result
     *   object.
     * @param map The argument for addData(), which contained invalid data.
     **/
    void invalidDataReceived( Enums::TimetableInformation info, const QString &errorMessage,
                              const QScriptContextInfo &context,
                              int index, const QVariantMap& map );

public Q_SLOTS:
    /**
     * @brief Clears the list of stored TimetableData objects.
     **/
    void clear();

    /**
     * @brief Add @p timetableItem to the result set.
     *
     * This can be data for departures, arrivals, journeys, stop suggestions or additional data.
     * See Enums::TimetableInformation for a list of the property names to use.
     *
     * @code
     *  result.addData( {DepartureDateTime: new Date(), Target: 'Test'} );
     * @endcode
     *
     * A predefined object can also be added like this:
     * @code
     *  var departure = { DepartureDateTime: new Date() };
     *
     *  // Use "Target" as property name
     *  departure.Target = 'Test';
     *
     *  // Alternative: Use enumerable value
     *  departure[ PublicTransport.Target ] = 'Test';
     *
     *  result.addData( departure );
     * @endcode
     *
     * @param timetableItem A script object with data for a timetable item. Contains data in a set
     *   of properties, eg. the departure date and time for a departure gets stored as
     *   DepartureDateTime. All available properties can be found in Enums::TimetableInformation.
     **/
    void addData( const QVariantMap &timetableItem );

//     TODO
//     /** @brief Whether or not @p info is contained in this TimetableData object. */
//     bool contains( TimetableInformation info ) const { return m_timetableData.contains(info); };

private:
    QList< TimetableData > m_timetableData;

    // Protect data from concurrent access by the script in a separate thread and usage in C++
    QMutex *m_mutex;
    Features m_features;
    Hints m_hints;
};
/** \} */ // @ingroup scriptApi
Q_DECLARE_OPERATORS_FOR_FLAGS( ResultObject::Features )
Q_DECLARE_OPERATORS_FOR_FLAGS( ResultObject::Hints )

class StoragePrivate;
/** @ingroup scriptApi
 * @{ */
/**
 * @brief Used by scripts to store data between calls.
 *
 * An object of this type gets created for each script (not for each call to the script).
 * The data in the storage is therefore shared between calls to a script, but not between
 * scripts for different service providers.
 *
 * This class distinguishes between memory storage and persistent storage, ie. on disk.
 * Both storage types are protected by separate QReadWriteLock's, because scripts may run in
 * parallel.
 *
 * @code
 * // Write a single value and read it again
 * storage.write( "name1", 123 );
 * var name1 = storage.read( "name1" );   // name1 == 123
 *
 * // Write an object with multiple values at once and read it again
 * var object = { value1: 445, value2: "test",
 *                otherValue: new Date() };
 * storage.write( object );
 * var readObject = storage.read();
 * // The object read from storage now contains both the single
 * // value and the values of the object
 * // readObject == { name1: 123, value1: 445, value2: "test",
 * //                 otherValue: new Date() };
 *
 * var value2 = storage.read( "value2" );   // value2 == "test"
 * var other = storage.read( "other", 555 );   // other == 555, the default value
 * storage.remove( "name1" );   // Remove a value from the storage
 * storage.clear();
 * @endcode
 *
 *
 * After the service provider plugin has been reloaded (eg. after a restart), the values stored
 * like shown above are gone. To write data persistently to disk use readPersistent(),
 * writePersistent() and removePersistent(). Persistently stored data has a lifetime which can be
 * specified as argument to writePersistent() and defaults to one week.
 * The maximum lifetime is one month.
 *
 * @code
 * // Write a single value persistently and read it again
 * storage.writePersistent( "name1", 123 );   // Using the default lifetime
 * var name1 = storage.readPersistent( "name1" );   // name1 == 123
 *
 * // Write an object with multiple values at once and read it again
 * var object = { value1: 445, value2: "test", otherValue: new Date() };
 * storage.writePersistent( object );
 * var readObject = storage.readPersistent();
 * // The object read from storage now contains both the single
 * // value and the values of the object
 * // readObject == { name1: 123, value1: 445, value2: "test",
 * //                 otherValue: new Date() };
 *
 * // Using custom lifetimes (in days)
 * // 1. Store value 66 for 30 days as "longNeeded"
 * storage.writePersistent( "longNeeded", 66, 30 );
 * // 2. Lifetime can't be higher than 30 days
 * storage.writePersistent( "veryLongNeeded", 66, 300 );
 *
 * // Check the remaining lifetime of a persistently stored value
 * var lifetimeLongNeeded = storage.lifetime( "longNeeded" ); // Now 30
 * var other = storage.readPersistent( "other", 555 );   // other == 555, default
 * storage.removePersistent( "name1" );   // Remove a value from the storage
 * storage.clearPersistent();
 * @endcode
 *
 * @warning Since the script can run multiple times simultanously in different threads which share
 *   the same Storage object, the stored values are also shared. If you want to store a value for
 *   the current job of the script only (eg. getting departures and remember a value after an
 *   asynchronous request), you should store the value in a global script variable instead.
 *   Otherwise one departure request job might use the value stored by another one, which is
 *   probably not what you want. Scripts can not not access the Storage object of other scripts
 *   (for other service providers).
 **/
class Storage : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Create a new Storage instance.
     *
     * @param serviceProviderId Used to find the right place in the service provider cache file
     *   to read/write persistent data.
     * @param parent The parent QObject.
     **/
    Storage( const QString &serviceProviderId, QObject *parent = 0 );

    /** @brief Destructor. */
    virtual ~Storage();

    /**
     * @brief The maximum lifetime in days for data written to disk.
     * @see writePersistent()
     **/
    static const uint MAX_LIFETIME = 30;

    /**
     * @brief The default lifetime in days for data written to disk.
     * @see writePersistent()
     **/
    static const uint DEFAULT_LIFETIME = 7;

    /**
     * @brief The suffix to use for lifetime data entries.
     *
     * This suffix gets appended to the name with which data gets written persistently to disk
     * to get the name of the data entry storing the associated lifetime information.
     * @see writePersistent()
     **/
    static const char* LIFETIME_ENTRYNAME_SUFFIX;

    /**
     * @brief The minimal interval in minutes to run checkLifetime().
     * @see checkLifetime()
     **/
    static const int MIN_LIFETIME_CHECK_INTERVAL = 15;

    /**
     * @brief Whether or not a data entry with @p name exists in memory.
     **/
    Q_INVOKABLE bool hasData( const QString &name ) const;

    /**
     * @brief Whether or not a data entry with @p name exists in persistent memory.
     **/
    Q_INVOKABLE bool hasPersistentData( const QString &name ) const;

    /**
     * @brief Reads all data stored in memory.
     **/
    Q_INVOKABLE QVariantMap read();

    /**
     * @brief Reads data stored in memory with @p name.
     **/
    Q_INVOKABLE QVariant read( const QString &name, const QVariant& defaultData = QVariant() );

    /**
     * @brief Reads the lifetime remaining for data written using writePersistent() with @p name.
     **/
    Q_INVOKABLE int lifetime( const QString &name );

    /**
     * @brief Reads data stored on disk with @p name.
     *
     * @param name The name of the value to read.
     * @param defaultData The value to return if no stored value was found under @p name.
     *   If you use another default value than the default invalid QVariant, the type must match
     *   the type of the stored value. Otherwise an invalid QVariant gets returned.
     * @see lifetime()
     **/
    Q_INVOKABLE QVariant readPersistent( const QString &name,
                                         const QVariant& defaultData = QVariant() );

    /**
     * @brief Checks the lifetime of all persistent data entries.
     *
     * If the lifetime of a persistent data entry has expired, it gets deleted.
     **/
    void checkLifetime();

public Q_SLOTS:
    /**
     * @brief Stores @p data in memory with @p name.
     **/
    void write( const QString &name, const QVariant &data );

    /**
     * @brief Stores @p data in memory, eg. a script object.
     *
     * @param data The data to write to disk. This can be a script object.
     * @overload
     **/
    void write( const QVariantMap &data );

    /**
     * @brief Removes data stored in memory with @p name.
     **/
    void remove( const QString &name );

    /**
     * @brief Clears all data stored in memory.
     **/
    void clear();

    /**
     * @brief Stores @p data on disk with @p name.
     *
     * @param name A name to access the written data with.
     * @param data The data to write to disk. The type of the data can also be QVariantMap (ie.
     *   script objects) or list types (gets encoded to QByteArray). Length of the data is limited
     *   to 65535 bytes.
     * @param lifetime The lifetime in days of the data. Limited to 30 days and defaults to 7 days.
     *
     * @see lifetime
     **/
    void writePersistent( const QString &name, const QVariant &data,
                          uint lifetime = DEFAULT_LIFETIME );

    /**
     * @brief Stores @p data on disk, eg. a script object.
     *
     * After @p lifetime days have passed, the written data will be deleted automatically.
     * To prevent automatic deletion the data has to be written again.
     *
     * @param data The data to write to disk. This can be a script object.
     * @param lifetime The lifetime in days of each entry in @p data.
     *   Limited to 30 days and defaults to 7 days.
     *
     * @see lifetime
     * @overload
     **/
    void writePersistent( const QVariantMap &data, uint lifetime = DEFAULT_LIFETIME );

    /**
     * @brief Removes data stored on disk with @p name.
     *
     * @note Scripts do not need to remove data written persistently, ie. to disk, because each
     *   data entry has a lifetime, which is currently limited to 30 days and defaults to 7 days.
     **/
    void removePersistent( const QString &name );

    /**
     * @brief Clears all data stored persistently, ie. on disk.
     *
     * @note Scripts do not need to remove data written persistently, ie. to disk, because each
     *   data entry has a lifetime, which is currently limited to 30 days and defaults to 7 days.
     **/
    void clearPersistent();

private:
    int lifetime( const QString &name, const KConfigGroup &group );
    void removePersistent( const QString &name, KConfigGroup &group );
    QByteArray encodeData( const QVariant &data ) const;
    QVariant decodeData( const QByteArray &data ) const;

    StoragePrivate *d;
};
/** \} */ // @ingroup scriptApi

/** @ingroup scriptApi
 * @{ */
/**
 * @brief A data stream class to be used in scripts.
 *
 * This class gets made available to scripts under the name @c DataStream.
 * It can be setup to read from @c data that was received from a network request like here:
 * @code
 * // data is a QByteArray as received from a network request
 * // That data can now be decoded using helper.decode(),
 * // but if it contains binary data DataStream should be used with a QBuffer
 * var buffer = new QBuffer( data );
 * buffer.open( QIODevice.ReadOnly );
 * var stream = new DataStream( buffer );
 * // The stream is now ready
 *
 * // Seek to position 10, if possible
 * if ( 10 < buffer.size() ) {
 *    stream.seek( 10 );
 * }
 *
 * // The current position is available in the 'pos' property
 * var pos = stream.pos;
 *
 * // Skip some bytes
 * stream.skip( 2 ); // Skip two bytes, equal to stream.seek(stream.pos + 2)
 *
 * // Read data
 * var number = stream.readUInt32(); // Read a four byte unsigned integer
 * var smallNumber = stream.readInt8(); // Read a one byte integer
 * var string = stream.readString(); // Read a string, ends at the first found \0
 *
 * // Close the buffer again
 * buffer.close();
 * @endcode
 *
 * This class wraps a QDataStream, because it's read/write operators cannot be used from QtScript
 * otherwise, ie. the operator>>(), operator<<() functions. This class offers some read functions
 * to make this possible, ie readInt8(), readUInt8() or readString(). To read byte data use
 * readBytes().
 **/
class DataStreamPrototype: public QObject, protected QScriptable {
    Q_OBJECT
    Q_PROPERTY( quint64 pos READ pos WRITE seek )
    Q_PROPERTY( bool atEnd READ atEnd )

public:
    DataStreamPrototype( QObject *parent = 0 );
    DataStreamPrototype( const QByteArray &byteArray, QObject *parent = 0 );
    DataStreamPrototype( QIODevice *device, QObject *parent = 0 );
    DataStreamPrototype( DataStreamPrototype *other );

    /** @brief Reads an 8 bit integer. */
    Q_INVOKABLE qint8 readInt8();

    /** @brief Reads an unsigned 8 bit integer. */
    Q_INVOKABLE quint8 readUInt8();

    /** @brief Reads a 16 bit integer. */
    Q_INVOKABLE qint16 readInt16();

    /** @brief Reads an unsigned 16 bit integer. */
    Q_INVOKABLE quint16 readUInt16();

    /** @brief Reads a 32 bit integer. */
    Q_INVOKABLE qint32 readInt32();

    /** @brief Reads an unsigned 32 bit integer. */
    Q_INVOKABLE quint32 readUInt32();

    /** @brief Reads a string until the first \0 character is read. */
    Q_INVOKABLE QString readString();

    /** @brief Reads bytes until the first \0 character is read. */
    Q_INVOKABLE QByteArray readBytesUntilZero();

    /** @brief Reads @p bytes bytes. */
    Q_INVOKABLE QByteArray readBytes( uint bytes );

    /** @brief Whether or not the stream is at it's end. */
    Q_INVOKABLE bool atEnd() const { return m_dataStream->atEnd(); };

    /** @brief Get the current position in the streams device. */
    Q_INVOKABLE quint64 pos() const { return m_dataStream->device()->pos(); };

    /** @brief Seek to @p pos in the streams device. */
    Q_INVOKABLE bool seek( quint64 pos ) const { return m_dataStream->device()->seek(pos); };

    /** @brief Peek data with the given @p maxLength. */
    Q_INVOKABLE QByteArray peek( quint64 maxLength ) const {
        return m_dataStream->device()->peek(maxLength);
    };

    /** @brief Skip @p bytes bytes. */
    Q_INVOKABLE int skip( int bytes ) const { return m_dataStream->skipRawData(bytes); };

    /** @brief Return a human-readable description of the last device error that occured. */
    Q_INVOKABLE QString errorString() const { return m_dataStream->device()->errorString(); };

    /** @brief Get the underlying QDataStream as a QSharedPointer. */
    QSharedPointer< QDataStream > stream() const { return m_dataStream; };

private:
    QDataStream *thisDataStream() const;
    QSharedPointer< QDataStream > m_dataStream;
};
/** \} */ // @ingroup scriptApi

typedef DataStreamPrototype* DataStreamPrototypePtr;
QScriptValue constructStream( QScriptContext *context, QScriptEngine *engine );
QScriptValue dataStreamToScript( QScriptEngine *engine, const DataStreamPrototypePtr &stream );
void dataStreamFromScript( const QScriptValue &object, DataStreamPrototypePtr &stream );

} // namespace ScriptApi

Q_DECLARE_METATYPE(ScriptApi::Helper::ErrorSeverity)
Q_DECLARE_METATYPE(ScriptApi::ResultObject::Hint)
Q_DECLARE_METATYPE(ScriptApi::ResultObject::Hints)
Q_DECLARE_METATYPE(ScriptApi::ResultObject::Feature)
Q_DECLARE_METATYPE(ScriptApi::ResultObject::Features)

Q_DECLARE_METATYPE(ScriptApi::NetworkRequest*)
Q_DECLARE_METATYPE(ScriptApi::NetworkRequest::Ptr)
Q_SCRIPT_DECLARE_QMETAOBJECT(ScriptApi::NetworkRequest, QObject*)

Q_DECLARE_METATYPE(QIODevice*)
Q_DECLARE_METATYPE(QDataStream*)
Q_DECLARE_METATYPE(ScriptApi::DataStreamPrototype*)
Q_SCRIPT_DECLARE_QMETAOBJECT(ScriptApi::DataStreamPrototype, QObject*)

#endif // Multiple inclusion guard
