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
* @brief This file contains structures describing timetable data requests.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef REQUEST_HEADER
#define REQUEST_HEADER

// Own includes
#include "config.h"
#include "enums.h"

// Qt includes
#include <QScriptValue>

/**
 * @brief Stores information about a request to the publictransport data engine.
 *
 * All values other than @p sourceName can be derived (parsed) from it.
 **/
struct AbstractRequest {
    /**
     * @brief The requesting source name.
     * Other values in AbstractRequest are derived from this string.
     **/
    QString sourceName;

    /** @brief The date and time to get results for. */
    QDateTime dateTime;

    /** @brief The stop name of the request. */
    QString stop;

    /** @brief The maximum number of result items, eg. departures or stop suggestions. */
    int maxCount;

    /**
     * @brief The city to get stop suggestions for.
     * This is only needed if @ref ServiceProvider::useSeparateCityValue returns @c true.
     **/
    QString city;

    /** @brief Describes what should be retrieved with the request, eg. departures or a a stop ID. */
    ParseDocumentMode parseMode;

    QString parseModeName() const;
    static QString parseModeName( ParseDocumentMode parseMode );

    AbstractRequest( const QString &sourceName = QString(),
                     ParseDocumentMode parseMode = ParseInvalid )
            : sourceName(sourceName), maxCount(parseMode == ParseForStopSuggestions ? 200 : 20),
              parseMode(parseMode)
    {
    };

    AbstractRequest( const QString &sourceName, const QString &stop, const QDateTime &dateTime,
            int maxCount, const QString &city = QString(),
            ParseDocumentMode parseMode = ParseForStopSuggestions )
            : sourceName(sourceName), dateTime(dateTime), stop(stop), maxCount(maxCount),
              city(city), parseMode(parseMode)
    {
    };

    AbstractRequest( const AbstractRequest &info )
            : sourceName(info.sourceName), dateTime(info.dateTime), stop(info.stop),
              maxCount(info.maxCount), city(info.city), parseMode(info.parseMode)
    {
    };

    virtual ~AbstractRequest() {};

    virtual AbstractRequest *clone() const = 0;

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    virtual QScriptValue toScriptValue( QScriptEngine *engine ) const = 0;

    /** @brief Get the name of the script function that is associated with this request. */
    virtual QString functionName() const = 0;
#endif
};

struct StopSuggestionRequest : public AbstractRequest {
    StopSuggestionRequest( const QString &sourceName = QString(),
                           ParseDocumentMode parseMode = ParseForStopSuggestions )
        : AbstractRequest(sourceName, parseMode) {};
    StopSuggestionRequest( const QString &sourceName, const QString &stop,
                           int maxCount, const QString &city = QString(),
                           ParseDocumentMode parseMode = ParseForStopSuggestions )
        : AbstractRequest(sourceName, stop, QDateTime(), maxCount, city, parseMode) {};
    StopSuggestionRequest( const StopSuggestionRequest &info ) : AbstractRequest(info) {};

    virtual AbstractRequest *clone() const
    {
        return new StopSuggestionRequest( sourceName, stop, maxCount, city, parseMode );
    };

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    virtual QScriptValue toScriptValue( QScriptEngine *engine ) const;

    /** @brief Get the name of the script function that is associated with this request. */
    virtual QString functionName() const;
#endif
};

struct StopSuggestionFromGeoPositionRequest : public StopSuggestionRequest {
    /** @brief The longitude of the stop. */
    float longitude;

    /** @brief The latitude of the stop. */
    float latitude;

    /** @brief Maximal distance in meters from the position at longitude/latitude. */
    int distance;

    StopSuggestionFromGeoPositionRequest( const QString &sourceName = QString(),
                                          ParseDocumentMode parseMode = ParseForStopSuggestions )
        : StopSuggestionRequest(sourceName, parseMode),
          longitude(0.0), latitude(0.0), distance(500) {};
    StopSuggestionFromGeoPositionRequest( const QString &sourceName,
                                          float longitude, float latitude, int maxCount = 200,
                                          int distance = 500,
                                          ParseDocumentMode parseMode = ParseForStopSuggestions )
        : StopSuggestionRequest(sourceName, QString(), maxCount, QString(), parseMode),
          longitude(longitude), latitude(latitude), distance(distance) {};
    StopSuggestionFromGeoPositionRequest( const StopSuggestionFromGeoPositionRequest &request )
            : StopSuggestionRequest(request),
              longitude(request.longitude), latitude(request.latitude), distance(request.distance) {};

    virtual AbstractRequest *clone() const
    {
        return new StopSuggestionFromGeoPositionRequest( sourceName, longitude, latitude,
                                                         maxCount, distance, parseMode );
    };

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    virtual QScriptValue toScriptValue( QScriptEngine *engine ) const;
#endif
};

struct DepartureRequest : public AbstractRequest {
    DepartureRequest( const QString &sourceName = QString(),
                      ParseDocumentMode parseMode = ParseForDepartures )
        : AbstractRequest(sourceName, parseMode) {};
    DepartureRequest( const QString &sourceName, const QString &stop,
                      const QDateTime &dateTime, int maxCount,
                      const QString &city = QString(),
                      ParseDocumentMode parseMode = ParseForDepartures )
        : AbstractRequest(sourceName, stop, dateTime, maxCount, city, parseMode) {};
    DepartureRequest( const DepartureRequest &info ) : AbstractRequest(info) {};

    virtual AbstractRequest *clone() const
    {
        return new DepartureRequest( sourceName, stop, dateTime, maxCount, city, parseMode );
    };

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    virtual QScriptValue toScriptValue( QScriptEngine *engine ) const;

    /** @brief Get the name of the script function that is associated with this request. */
    virtual QString functionName() const;
#endif
};

struct ArrivalRequest : public DepartureRequest {
    ArrivalRequest( const QString &sourceName = QString(),
                    ParseDocumentMode parseMode = ParseForArrivals )
        : DepartureRequest(sourceName, parseMode) {};
    ArrivalRequest( const QString &sourceName, const QString &stop,
                    const QDateTime &dateTime, int maxCount,
                    const QString &city = QString(),
                    ParseDocumentMode parseMode = ParseForArrivals )
        : DepartureRequest(sourceName, stop, dateTime, maxCount, city, parseMode) {};

    virtual AbstractRequest *clone() const
    {
        return new ArrivalRequest( sourceName, stop, dateTime, maxCount, city, parseMode );
    };

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    virtual QScriptValue toScriptValue( QScriptEngine *engine ) const;
#endif
};

/**
 * @brief Stores information about a request for journeys to the publictransport data engine.
 *
 * All values other than @p sourceName are derived (parsed) from it or represent the current state
 * of the request, eg. @p roundTrips stores the number of requests sent to a server to get
 * enough data (each round trip adds some data).
 **/
struct JourneyRequest : public AbstractRequest {
    /** @brief The target stop name of the request. */
    QString targetStop;

    /** @brief Specifies the URL to use to download the journey source document. */
    QString urlToUse;

    /** @brief Current number of round trips used to fulfil the journey request. */
    int roundTrips;

    JourneyRequest( const QString &sourceName = QString(),
                    ParseDocumentMode parseMode = ParseForJourneysByDepartureTime )
            : AbstractRequest(sourceName, parseMode), roundTrips(0)
    {
    };

    JourneyRequest( const QString &sourceName, const QString &startStop,
            const QString &targetStop, const QDateTime &dateTime, int maxCount,
            const QString &urlToUse, const QString &city = QString(),
            ParseDocumentMode parseMode = ParseForJourneysByDepartureTime )
        : AbstractRequest(sourceName, startStop, dateTime, maxCount, city, parseMode),
          targetStop(targetStop), urlToUse(urlToUse), roundTrips(0)
    {
    };

    JourneyRequest( const JourneyRequest &info ) : AbstractRequest(info)
    {
        targetStop = info.targetStop;
        urlToUse = info.urlToUse;
        roundTrips = info.roundTrips;
    };

    virtual JourneyRequest *clone() const
    {
        return new JourneyRequest( sourceName, stop, targetStop, dateTime, maxCount, urlToUse,
                                   city, parseMode );
    };

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    virtual QScriptValue toScriptValue( QScriptEngine *engine ) const;

    /** @brief Get the name of the script function that is associated with this request. */
    virtual QString functionName() const;
#endif
};

// TODO Add another abstraction without unneded values in AbstractRequest
struct AdditionalDataRequest : public AbstractRequest {
    int itemNumber;
    QString transportLine;
    QString target;

    AdditionalDataRequest( const QString &sourceName = QString(),
                           ParseDocumentMode parseMode = ParseForAdditionalData )
        : AbstractRequest(sourceName, parseMode) {};
    AdditionalDataRequest( const QString &sourceName, int itemNumber, const QString &stop,
                           const QDateTime &dateTime, const QString &transportLine,
                           const QString &target, const QString &city = QString(),
                           ParseDocumentMode parseMode = ParseForAdditionalData )
        : AbstractRequest(sourceName, stop, dateTime, 1, city, parseMode),
          itemNumber(itemNumber), transportLine(transportLine), target(target) {};
    AdditionalDataRequest( const AdditionalDataRequest &other )
        : AbstractRequest(other), itemNumber(other.itemNumber),
          transportLine(other.transportLine), target(other.target) {};

    virtual AbstractRequest *clone() const { return new AdditionalDataRequest(*this); };

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    virtual QScriptValue toScriptValue( QScriptEngine *engine ) const;

    /** @brief Get the name of the script function that is associated with this request. */
    virtual QString functionName() const;
#endif
};

#endif // Multiple inclusion guard
