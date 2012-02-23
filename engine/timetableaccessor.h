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
* @brief This file contains the base class for all accessors used by the public transport data engine.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TIMETABLEACCESSOR_HEADER
#define TIMETABLEACCESSOR_HEADER

// Own includes
#include "enums.h"
#include "departureinfo.h"

// KDE includes
#include <KUrl> // Member variable

// Qt includes
#include <QHash>
#include <QStringList>
#include <QScriptValue>

class QScriptEngine;
class KUrl;
class KJob;

class ChangelogEntry;
class StopInfo;
class DepartureInfo;
class JourneyInfo;
class PublicTransportInfo;
class TimetableAccessorInfo;

/**
 * @brief Stores information about a request to the publictransport data engine.
 *
 * All values other than @p sourceName are derived (parsed) from it or represent the current state
 * of the request.
 **/
struct RequestInfo {
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

    RequestInfo()
    {
        this->parseMode = ParseInvalid;
        this->maxCount = -1;
        this->useDifferentUrl = false;
        this->parseMode = ParseForStopSuggestions;
    };

    RequestInfo( const QString &sourceName, const QString &stop, const QDateTime &dateTime,
            int maxCount, const QString &dataType = "departures", bool useDifferentUrl = false,
            const QString &city = QString(),
            ParseDocumentMode parseMode = ParseForStopSuggestions )
            : sourceName(sourceName), dateTime(dateTime), stop(stop), maxCount(maxCount),
              dataType(dataType), useDifferentUrl(useDifferentUrl), city(city), parseMode(parseMode)
    {
    };

    RequestInfo( const RequestInfo &info )
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

    virtual ~RequestInfo() {};

    virtual RequestInfo *clone() const
    {
        return new RequestInfo( sourceName, stop, dateTime, maxCount, dataType, useDifferentUrl,
                                city, parseMode );
    };

    virtual QScriptValue toScriptValue( QScriptEngine *engine ) const;
};

typedef RequestInfo StopSuggestionRequestInfo;

struct DepartureRequestInfo : public RequestInfo {
    DepartureRequestInfo() : RequestInfo() { parseMode = ParseForDeparturesArrivals; };
    DepartureRequestInfo( const QString &sourceName, const QString &stop,
                          const QDateTime &dateTime, int maxCount,
                          const QString &dataType = "departures",
                          bool useDifferentUrl = false, const QString &city = QString(),
                          ParseDocumentMode parseMode = ParseForDeparturesArrivals )
        : RequestInfo(sourceName, stop, dateTime, maxCount, dataType,
                      useDifferentUrl, city, parseMode) {};
    DepartureRequestInfo( const DepartureRequestInfo &info ) : RequestInfo(info) {};

    virtual QScriptValue toScriptValue( QScriptEngine *engine ) const;
};

/**
 * @brief Stores information about a request for journeys to the publictransport data engine.
 *
 * All values other than @p sourceName are derived (parsed) from it or represent the current state
 * of the request, eg. @p roundTrips stores the number of requests sent to a server to get
 * enough data (each round trip adds some data).
 **/
struct JourneyRequestInfo : public RequestInfo {
    /** @brief The target stop name of the request. */
    QString targetStop;

    /** @brief Specifies the URL to use to download the journey source document. */
    QString urlToUse;

    /** @brief Current number of round trips used to fulfil the journey request. */
    int roundTrips;

    JourneyRequestInfo() : RequestInfo()
    {
        this->roundTrips = 0;
        this->parseMode = ParseForJourneys;
    };

    JourneyRequestInfo( const QString &sourceName, const QString &startStop,
            const QString &targetStop, const QDateTime &dateTime, int maxCount,
            const QString &urlToUse, const QString &dataType = "journeys",
            bool useDifferentUrl = false, const QString &city = QString(),
            ParseDocumentMode parseMode = ParseForJourneys )
        : RequestInfo(sourceName, startStop, dateTime, maxCount, dataType,
                      useDifferentUrl, city, parseMode),
          targetStop(targetStop), urlToUse(urlToUse), roundTrips(0)
    {
    };

    JourneyRequestInfo( const JourneyRequestInfo &info ) : RequestInfo(info)
    {
        targetStop = info.targetStop;
        urlToUse = info.urlToUse;
        roundTrips = info.roundTrips;
    };

    virtual JourneyRequestInfo *clone() const
    {
        return new JourneyRequestInfo( sourceName, stop, targetStop, dateTime, maxCount, urlToUse,
                                       dataType, useDifferentUrl, city, parseMode );
    };

    virtual QScriptValue toScriptValue( QScriptEngine *engine ) const;
};

/**
 * @brief Gets timetable information for public transport from different service providers.
 *
 * The easiest way to implement support for a new service provider is to add an XML file describing
 * the service provider and a script to parse timetable documents.
 * If that's not enough a new class can be derived from TimetableAccessor or it's derivates. These
 * functions should then be overwritten:
 *   - serviceProvider()
 *   - country(), cities()
 *   - rawUrl(), parseDocument()
 **/
class TimetableAccessor : public QObject {
    Q_OBJECT
    Q_PROPERTY( QString serviceProvider READ serviceProvider )
    Q_PROPERTY( QStringList features READ features )
    Q_PROPERTY( QStringList featuresLocalized READ featuresLocalized )
    Q_PROPERTY( QString country READ country )
    Q_PROPERTY( QStringList cities READ cities )
    Q_PROPERTY( QString credit READ credit )
    Q_PROPERTY( int minFetchWait READ minFetchWait )
    Q_PROPERTY( bool useSeparateCityValue READ useSeparateCityValue )
    Q_PROPERTY( bool onlyUseCitiesInList READ onlyUseCitiesInList )

public:
    /**
     * @brief Constructs a new TimetableAccessor object. You should use getSpecificAccessor()
     *   to get an accessor that can download and parse documents from the given service provider.
     **/
    explicit TimetableAccessor( TimetableAccessorInfo *info = 0 );

    /** @brief Destructor. */
    virtual ~TimetableAccessor();

    /**
     * @brief Gets a timetable accessor that is able to parse results from the given service provider.
     *
     * @param serviceProvider The ID of the service provider to get an accessor for.
     *   The ID starts with a country code, followed by an underscore and it's name.
     *   If it's empty, the default service provider for the users country will
     *   be used, if there is any.
     **/
    static TimetableAccessor *getSpecificAccessor( const QString &serviceProvider = QString() );

    /** @brief Gets the AccessorType enumerable for the given string. */
    static AccessorType accessorTypeFromString( const QString &sAccessorType );

    /** @brief Gets the service provider ID for the given accessor XML file name. */
    static QString serviceProviderIdFromFileName( const QString &accessorXmlFileName );

    /** @brief Gets the file path of the default service provider XML for the given @p location. */
    static QString defaultServiceProviderForLocation( const QString &location,
                                                      const QStringList &dirs = QStringList() );

    /** @brief Gets the service provider ID the accessor is designed for. */
    virtual QString serviceProvider() const;

    /** @brief Gets the minimum seconds to wait between two data-fetches from the service provider. */
    virtual int minFetchWait() const;

    /** @brief Gets a list of features that this accessor supports. */
    virtual QStringList features() const;

    /** @brief Gets a list of short localized strings describing the supported features. */
    QStringList featuresLocalized() const;

    /** @brief Gets a list of features that this accessor supports through a script. */
    virtual QStringList scriptFeatures() const { return QStringList(); };

    /** @brief The country for which the accessor returns results. */
    virtual QString country() const;

    /** @brief A list of cities for which the accessor returns results. */
    virtual QStringList cities() const;

    QString credit() const;

    const TimetableAccessorInfo *info() const { return m_info; };

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
     * @brief Requests a list of journeys.
     *
     * When the journey list is completely received @ref journeyListReceived() gets emitted.
     *
     * @param requestInfo Information about the journey request.
     **/
    virtual void requestJourneys( const JourneyRequestInfo &requestInfo );

    /**
     * @brief Requests a list of stop suggestions.
     *
     * When the stop list is completely received @ref stopListReceived gets emitted.
     *
     * @param requestInfo Information about the stop suggestion request.
     **/
    virtual void requestStopSuggestions( const StopSuggestionRequestInfo &requestInfo );

    /** @brief Gets the information object used by this accessor. */
    const TimetableAccessorInfo &timetableAccessorInfo() const { return *m_info; };

    /** @brief Whether or not the city should be put into the "raw" url. */
    virtual bool useSeparateCityValue() const;

    /**
     * @brief Whether or not cities may be chosen freely.
     *
     * @return true if only cities in the list returned by cities()  are valid.
     * @return false (default) if cities may be chosen freely, but may be invalid.
     **/
    virtual bool onlyUseCitiesInList() const;

    /**
     * @brief Encodes the url in @p str using the charset in @p charset. Then it is percent encoded.
     *
     * @see charsetForUrlEncoding()
     **/
    static QString toPercentEncoding( const QString &str, const QByteArray &charset );

protected:
    /**
     * @brief Gets the charset used to encode urls before percent-encoding them. Normally
     *   this charset is UTF-8. But that doesn't work for sites that require parameters
     *   in the url (..&param=x) to be encoded in that specific charset.
     *
     * @see TimetableAccessor::toPercentEncoding()
     **/
    virtual QByteArray charsetForUrlEncoding() const;


    QString m_curCity; /**< @brief Stores the currently used city. */
    TimetableAccessorInfo *m_info; /**< @brief Stores service provider specific information that is
                                     * used to parse the documents from the service provider. */

signals:
    /**
     * @brief Emitted when a new departure or arrival list has been received and parsed.
     *
     * @param accessor The accessor that was used to download and parse the departures/arrivals.
     * @param requestUrl The url used to request the information.
     * @param journeys A list of departures / arrivals that were received.
     * @param requestInfo Information about the request for the just received departure/arrival list.
     * @see TimetableAccessor::useSeperateCityValue()
     **/
    void departureListReceived( TimetableAccessor *accessor, const QUrl &requestUrl,
            const DepartureInfoList &journeys, const GlobalTimetableInfo &globalInfo,
            const DepartureRequestInfo &requestInfo );

    /**
     * @brief Emitted when a new journey list has been received and parsed.
     *
     * @param accessor The accessor that was used to download and parse the journeys.
     * @param requestUrl The url used to request the information.
     * @param journeys A list of journeys that were received.
     * @param requestInfo Information about the request for the just received journey list.
     * @see TimetableAccessor::useSeperateCityValue()
     **/
    void journeyListReceived( TimetableAccessor *accessor, const QUrl &requestUrl,
            const JourneyInfoList &journeys, const GlobalTimetableInfo &globalInfo,
            const JourneyRequestInfo &requestInfo );

    /**
     * @brief Emitted when a list of stop names has been received and parsed.
     *
     * @param accessor The accessor that was used to download and parse the stops.
     * @param requestUrl The url used to request the information.
     * @param stops A pointer to a list of @ref StopInfo objects.
     * @param requestInfo Information about the request for the just received stop list.
     * @see TimetableAccessor::useSeperateCityValue()
     **/
    void stopListReceived( TimetableAccessor *accessor, const QUrl &requestUrl,
            const StopInfoList &stops, const StopSuggestionRequestInfo &requestInfo );

    /**
     * @brief Emitted when a session key has been received and parsed.
     *
     * @param accessor The accessor that was used to download and parse the session key.
     * @param sessionKey The parsed session key.
     **/
    void sessionKeyReceived( TimetableAccessor *accessor, const QString &sessionKey );

    /**
     * @brief Emitted when an error occurred while parsing.
     *
     * @param accessor The accessor that was used to download and parse information
     *   from the service provider.
     * @param errorCode The error code or NoError if there was no error.
     * @param errorString If @p errorCode isn't NoError this contains a
     *   description of the error.
     * @param requestUrl The url used to request the information.
     * @param requestInfo Information about the request that resulted in the error.
     * @see TimetableAccessor::useSeperateCityValue()
     **/
    void errorParsing( TimetableAccessor *accessor, ErrorCode errorCode, const QString &errorString,
            const QUrl &requestUrl, const RequestInfo *requestInfo );

    void forceUpdate();

protected:
    struct JobInfos {
        // Mainly for QHash
        JobInfos() : requestInfo(0) {
        };

        JobInfos( const KUrl &url, RequestInfo *requestInfo ) : requestInfo(requestInfo) {
            this->url = url;
        };

        ~JobInfos()
        {
            delete requestInfo;
        };

        KUrl url;
        RequestInfo *requestInfo;
    };

private:
    static QString gethex( ushort decimal );

    // Stores information about currently running download jobs
    QHash< KJob*, JobInfos > m_jobInfos;

    bool m_idAlreadyRequested;
};

#endif // TIMETABLEACCESSOR_HEADER
