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

/** @file
* @brief This file contains the base class for all accessors used by the public transport data engine.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TIMETABLEACCESSOR_HEADER
#define TIMETABLEACCESSOR_HEADER

#include "enums.h"

#include <KUrl>
#include <QHash>
#include <QStringList>

class TimetableAccessorInfo;
class PublicTransportInfo;
class DepartureInfo;
class JourneyInfo;
class StopInfo;
class KJob;

/**
 * @brief Stores information about a request to the Public Transport data engine.
 *
 * All values other than @p sourceName are derived (parsed) from it or represent the current state
 * of the request.
 **/
class RequestInfo : public QObject {
    Q_OBJECT

public:
    /**
     * @brief The requesting source name.
     *
     * Other values in RequestInfo are derived from this string.
     **/
    QString sourceName;

    /** @brief The date and time to get results for. */
    QDateTime dateTime;

    /** @brief The stop name of the request. */
    QString stop;

    /** @brief The maximum number of result items, eg. departures or stop suggestions. */
    int maxCount;

    /** @brief Like parseMode, but distinguishes between "arrivals" and "departures". */
    QString dataType;

    /** @brief True, if another source URL than the default one was used. TODO */
    bool useDifferentUrl;

    /**
     * @brief The city to get stop suggestions for (only needed if
     *   @ref TimetableAccessor::useSeparateCityValue returns true).
     **/
    QString city;

    /** @brief Describes what should be retrieved with the request, eg. departures or a a stop ID. */
    ParseDocumentMode parseMode;

    /** @brief Creates an invalid RequestInfo object with the given @p parent. */
    RequestInfo( QObject *parent = 0 ) : QObject( parent )
    {
        this->parseMode = ParseInvalid;
        this->maxCount = -1;
        this->useDifferentUrl = false;
    };

    /**
     * @brief Creates a RequestInfo object.
     *
     * @param sourceName The name of the data source for which the request was triggered.
     * @param stop The stop name parsed from @p sourceName.
     * @param dateTime The date and time where requested data should begin,
     *   parsed from @p sourceName.
     * @param maxCount The maximum count of items (eg. departures) that should be returned.
     *   Some service providers may return some more items. For GTFS accessors this number is
     *   never excessed.
     * @param dataType The type of data that gets requested. Can be "departures", "arrivals",
     *   "journeys", "journeysDep" (equals "journeys", departing from the home stop),
     *   "journeysArr" (arriving at the home stop) or "stopSuggestions". Default is "departures".
     * @param useDifferentUrl Whether or not a different URL should be used, eg. use the stop
     *   suggestions URL instead of the departures URL. Default is false.
     * @param city The city parsed from @p sourceName, if needed by the service provider.
     *   Default is an empty string.
     * @param parseMode What should be parsed from source documents.
     *   Default is ParseForDeparturesArrivals.
     * @param parent The parent QObject.
     **/
    RequestInfo( const QString &sourceName, const QString &stop, const QDateTime &dateTime,
            int maxCount, const QString &dataType = "departures", bool useDifferentUrl = false,
            const QString &city = QString(),
            ParseDocumentMode parseMode = ParseForDeparturesArrivals, QObject *parent = 0 )
            : QObject(parent), sourceName(sourceName), dateTime(dateTime), stop(stop),
              maxCount(maxCount), dataType(dataType), useDifferentUrl(useDifferentUrl),
              city(city), parseMode(parseMode)
    {
    };

    /** @brief Copy constructor. */
    RequestInfo( const RequestInfo &info, QObject *parent = 0 ) : QObject(parent)
    {
        sourceName = info.sourceName;
        dateTime = info.dateTime;
        stop = info.stop;
        maxCount = info.maxCount;
        dataType = info.dataType;
        useDifferentUrl = info.useDifferentUrl;
        city = info.city;
        parseMode = info.parseMode;
    };

    /** @brief Creates a new RequestInfo object, which is a clone of this one. */
    virtual RequestInfo *clone() const
    {
        return new RequestInfo( sourceName, stop, dateTime, maxCount, dataType, useDifferentUrl,
                                city, parseMode );
    };
};

/** @brief Stores information about a request for stop suggestions to the Public Transport data engine. */
typedef RequestInfo StopSuggestionRequestInfo;

/** @brief Stores information about a request for departures/arrivals to the Public Transport data engine. */
typedef RequestInfo DepartureRequestInfo;

/**
 * @brief Stores information about a request for journeys to the Public Transport data engine.
 *
 * All values other than @p sourceName are derived (parsed) from it or represent the current state
 * of the request, eg. @p roundTrips stores the number of requests sent to a server to get
 * enough data (each round trip adds some data).
 **/
class JourneyRequestInfo : public RequestInfo {
    Q_OBJECT

public:
    /** @brief The target stop name of the request. */
    QString targetStop;

    /** @brief Specifies the URL to use to download the journey source document. */
    QString urlToUse;

    /** @brief Current number of round trips used to fulfil the journey request. */
    int roundTrips;

    /** @brief Creates an invalid JourneyRequestInfo object. */
    JourneyRequestInfo( QObject *parent = 0 ) : RequestInfo(parent)
    {
        this->roundTrips = 0;
    };

    /** @brief Creates a JourneyRequestInfo object. */
    JourneyRequestInfo( const QString &sourceName, const QString &startStop,
            const QString &targetStop, const QDateTime &dateTime, int maxCount,
            const QString &urlToUse, const QString &dataType = "journeys",
            bool useDifferentUrl = false, const QString &city = QString(),
            ParseDocumentMode parseMode = ParseForJourneys, QObject *parent = 0  )
        : RequestInfo(sourceName, startStop, dateTime, maxCount, dataType,
                      useDifferentUrl, city, parseMode, parent),
          targetStop(targetStop), urlToUse(urlToUse), roundTrips(0)
    {
    };

    /** @brief Copy constructor. */
    JourneyRequestInfo( const JourneyRequestInfo &info, QObject *parent = 0 )
            : RequestInfo(info, parent)
    {
        targetStop = info.targetStop;
        urlToUse = info.urlToUse;
        roundTrips = info.roundTrips;
    };

    /** @brief Creates a new JourneyRequestInfo object, which is a clone of this one. */
    virtual JourneyRequestInfo *clone() const
    {
        return new JourneyRequestInfo( sourceName, stop, targetStop, dateTime, maxCount, urlToUse,
                                       dataType, useDifferentUrl, city, parseMode );
    };
};

/**
 * @brief Gets timetable data for public transport from different service providers.
 *
 * Use info() to get a pointer to a TimetableAccessorInfo object with information about the service
 * provider. The information in that object is read from an XML file.
 *
 * To add support for a new timetable accessor type you can derive from this class and overwrite
 * functions, eg. requestDepartures() to start a request for departures or parseDocument() to
 * parse a requested document.
 **/
class TimetableAccessor : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs a new TimetableAccessor object.
     *
     * You should use createAccessor() to get an accessor for a given service provider ID.
     *
     * @param info An object containing information about the service provider.
     **/
    explicit TimetableAccessor( TimetableAccessorInfo *info );

    /**
     * @brief Simple destructor.
     *
     * The TimetableAccessorInfo object given in the constructor gets deleted here.
     **/
    virtual ~TimetableAccessor();

    /**
     * @brief Gets a timetable accessor that is able to parse results from the given service provider.
     *
     * @param serviceProvider The ID of the service provider to get an accessor for.
     *   The ID starts with a country code, followed by an underscore and it's name.
     *   If it's empty, the default service provider for the users country will
     *   be used, if there is any.
     **/
    static TimetableAccessor *createAccessor( const QString &serviceProvider = QString() );

    /**
     * @brief Reads the XML file for the given @p serviceProvider.
     *
     * @param serviceProvider The ID of the service provider which XML file should be read.
     *   The ID starts with a country code, followed by an underscore and it's name.
     *   If it's empty, the default service provider for the users country will
     *   be used, if there is any.
     **/
    static TimetableAccessorInfo *readAccessorInfo( const QString &serviceProvider = QString() );

    /** @brief Gets the AccessorType enumerable for the given string. */
    static AccessorType accessorTypeFromString( const QString &accessorType );

    /** @brief Gets the name for the given AccessorType enumerable. */
    static QString accessorTypeName( AccessorType accessorType );

    /** @brief Gets the TimetableInformation enumerable for the given string. */
    static TimetableInformation timetableInformationFromString( const QString &sTimetableInformation );

    /** @brief Gets the VehicleType enumerable for the given string. */
    static VehicleType vehicleTypeFromString( QString sVehicleType );

    /**
     * @brief Gets the name of the cache file for accessor infomation.
     *
     * The cache file can be used by accessors to store information about themselves that might
     * take some time to get if not stored. For example a network request might be needed to get
     * the information.
     * The cache file can also be used to store other information for the accessor, eg. the last
     * modified time of a file that gets only downloaded again, if it's last modified time is
     * higher.
     * KConfig is used to read from / write to the file.
     **/
    static QString accessorCacheFileName();

    /** @brief Gets the service provider ID for the given accessor XML file name. */
    static QString serviceProviderIdFromFileName( const QString &accessorXmlFileName );

    /** @brief Gets the file path of the default service provider XML for the given @p location. */
    static QString defaultServiceProviderForLocation( const QString &location,
                                                      const QStringList &dirs = QStringList() );

    /**
     * @brief Gets the type of this accessor.
     *
     * Derived classes need to overwrite this and return the appropriate type.
     **/
    virtual AccessorType type() const = 0;

    /** @brief Gets a list of features that this accessor supports. */
    virtual QStringList features() const;

    /** @brief Gets a list of short localized strings describing the supported features. */
    QStringList featuresLocalized() const;

    /** @brief Gets a list of features that this accessor supports through a script. */
    virtual QStringList scriptFeatures() const { return QStringList(); };

    /**
     * @brief Gets the TimetableAccessorInfo object for this accessor.
     *
     * This info object contains all information read from the accessors XML file, like it's name,
     * description, an URL to the home page of the service provider, etc.
     **/
    inline const TimetableAccessorInfo *info() const { return m_info; };

    /**
     * @brief Requests a list of departures/arrivals.
     *
     * When the departure/arrival list is completely received @ref departureListReceived gets
     * emitted.
     *
     * @param requestInfo Information about the departure/arrival request.
     **/
    virtual void requestDepartures( const DepartureRequestInfo &requestInfo );

    /**
     * @brief Requests a session key. May be needed for some service providers to work properly.
     *
     * When the session key has been received @ref sessionKeyReceived is emitted.
     *
     * @param parseMode Describes what should be done when the session key is available.
     * @param url The url to use to get the session key.
     * @param requestInfo Information about a request, that gets executed when the session key
     *   is known. If for example requestInfo has information about a departure request, after
     *   getting the session key, @p requestDepartures gets called with this requestInfo object.
     **/
    virtual void requestSessionKey( ParseDocumentMode parseMode, const KUrl &url,
            const RequestInfo *requestInfo );

    /**
     * @brief Requests a list of stop suggestions.
     *
     * When the stop list is completely received @ref stopListReceived gets emitted.
     *
     * @param requestInfo Information about the stop suggestion request.
     **/
    virtual void requestStopSuggestions( const StopSuggestionRequestInfo &requestInfo );

    /**
     * @brief Requests a list of journeys.
     *
     * When the journey list is completely received @ref journeyListReceived() gets emitted.
     *
     * @param requestInfo Information about the journey request.
     **/
    virtual void requestJourneys( const JourneyRequestInfo &requestInfo );

    /**
     * @brief Whether or not a special URL for stop suggestions is available.
     *
     * If there is a stop suggestions URL it can be retrieved using
     * TimetableAccessor::stopSuggestionsRawUrl and this function returns true. Otherwise the
     * URL for departures/arrivals gets used directly and returns stop suggestions if the given
     * stop name is ambiguous.
     *
     * @see TimetableAccessorInfo::stopSuggestionsRawUrl
     **/
    bool hasSpecialUrlForStopSuggestions() const;

    /**
     * @brief Encodes the url in @p str using the charset in @p charset. Then it is percent encoded.
     *
     * @see TimetableAccessorInfo::charsetForUrlEncoding()
     **/
    static QString toPercentEncoding( const QString &str, const QByteArray &charset );

protected:
    /**
     * @brief Constructs an URL to a document containing a departure/arrival list
     *
     * Uses the template "raw" URL for departures and replaces placeholders with the needed
     * information.
     *
     * @param requestInfo Information about the departure/arrival request to get a source URL for.
     **/
    virtual KUrl departureUrl( const DepartureRequestInfo &requestInfo ) const;

    /**
     * @brief Constructs an URL to a document containing a journey list.
     *
     * Uses the template "raw" URL for journeys and replaces placeholder with the needed
     * information.
     *
     * @param requestInfo Information about the journey request to get a source URL for.
     **/
    virtual KUrl journeyUrl( const JourneyRequestInfo &requestInfo ) const;

    /**
     * @brief Constructs an URL to a document containing stop suggestions.
     *
     * Uses the template "raw" URL for stop suggestions and replaces placeholders with the
     * needed information.
     *
     * @param requestInfo Information about the stop suggestions request to get a source URL for.
     **/
    virtual KUrl stopSuggestionsUrl( const StopSuggestionRequestInfo &requestInfo );

    QString m_curCity; /**< @brief Stores the currently used city. */
    TimetableAccessorInfo *m_info; /**< @brief Stores service provider specific information that is
                                     * used to parse the documents from the service provider. */

signals:
    /**
     * @brief Emitted when a new departure or arrival list is available.
     *
     * This signal gets emitted in response to a request started using requestDepartures.
     * The accessor may need some time to fulfil the request, eg. scripted accessors need to
     * download source documents and parse them to make the departure data available.
     * GTFS accessors operate on local databases and are very fast.
     *
     * @param accessor The accessor that was used to make the departure/arrival data available.
     * @param requestUrl The url used to request the departure/arrival data.
     * @param journeys A list of departures/arrivals that were received.
     * @param globalInfo Information for all departures/arrivals.
     * @param requestInfo Information about the request for the received departure/arrival list.
     * @see TimetableAccessor::useSeperateCityValue()
     **/
    void departureListReceived( TimetableAccessor *accessor, const QUrl &requestUrl,
            const QList<DepartureInfo*> &journeys, const GlobalTimetableInfo &globalInfo,
            const RequestInfo *requestInfo );

    /**
     * @brief Emitted when a new journey list is available.
     *
     * This signal gets emitted in response to a request started using requestJourneys.
     * The accessor may need some time to fulfil the request, eg. scripted accessors need to
     * download source documents and parse them to make the journey data available.
     * GTFS accessors operate on local databases and are very fast.
     *
     * @param accessor The accessor that was used to make the journey data available.
     * @param requestUrl The url used to request the journey data.
     * @param journeys A list of journeys that were received.
     * @param globalInfo Information for all journeys.
     * @param requestInfo Information about the request for the just received journey list.
     * @see TimetableAccessor::useSeperateCityValue()
     **/
    void journeyListReceived( TimetableAccessor *accessor, const QUrl &requestUrl,
            const QList<JourneyInfo*> &journeys, const GlobalTimetableInfo &globalInfo,
            const RequestInfo *requestInfo );

    /**
     * @brief Emitted when a list of stop suggestions is available.
     *
     * This signal gets emitted in response to a request started using requestStopSuggestions.
     * May also be emitted in response to calls to requestDepartures or requestJourneys, if the
     * given stop name was ambiguous.
     * The accessor may need some time to fulfil the request, eg. scripted accessors need to
     * download source documents and parse them to make the departure data available. But most
     * scripted accessors use small JSON sources and are fast enough to make the stop suggestions
     * available while typing in a stop name without too much delay.
     * GTFS accessors operate on local databases and are very fast.
     *
     * @param accessor The accessor that was used to make the stop suggestions available.
     * @param requestUrl The url used to request the stop suggestions.
     * @param stops A pointer to a list of @ref StopInfo objects.
     * @param requestInfo Information about the request for the just received stop list.
     * @see TimetableAccessor::useSeperateCityValue()
     **/
    void stopListReceived( TimetableAccessor *accessor, const QUrl &requestUrl,
            const QList<StopInfo*> &stops, const RequestInfo *requestInfo );

    /**
     * @brief Emitted when a session key is available.
     *
     * @param accessor The accessor that was used to make the session key available.
     * @param sessionKey The session key.
     **/
    void sessionKeyReceived( TimetableAccessor *accessor, const QString &sessionKey );

    /**
     * @brief Emitted when an error occurred while parsing.
     *
     * @param accessor The accessor that was used to make timetable data available.
     * @param errorCode The error code or NoError if there was no error.
     * @param errorString If @p errorCode isn't NoError this contains a description of the error.
     * @param requestUrl The url used to request the timetable data.
     * @param requestInfo Information about the request that resulted in the error.
     * @see TimetableAccessor::useSeperateCityValue()
     **/
    void errorParsing( TimetableAccessor *accessor, ErrorCode errorCode, const QString &errorString,
            const QUrl &requestUrl, const RequestInfo *requestInfo );

    /**
     * @brief Reports progress of the accessor in performing an action.
     *
     * @param accessor The accessor that emitted this signal.
     * @param progress A value between 0 (just started) and 1 (completed) indicating progress.
     * @param jobDescription A description of what is currently being done.
     * @param requestUrl The URL to the document that gets downloaded/parsed.
     * @param requestInfo Information about the request that produced the progress report.
     **/
    void progress( TimetableAccessor *accessor, qreal progress, const QString &jobDescription,
            const QUrl &requestUrl, const RequestInfo *requestInfo );

protected slots:
    /**
     * @brief Clears the session key.
     *
     * This gets called from time to time by a timer, to prevent using expired session keys.
     **/
    void clearSessionKey() {};

private:
    static QString gethex( ushort decimal );
};

#endif // TIMETABLEACCESSOR_HEADER
