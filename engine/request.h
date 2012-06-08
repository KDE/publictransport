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
#include "enums.h"

// Qt includes
#include <QScriptValue>

/**
 * @brief Stores information about a request to the publictransport data engine.
 *
 * All values other than @p sourceName are derived (parsed) from it or represent the current state
 * of the request.
 **/
struct AbstractRequest {
    /**
     * @brief The requesting source name.
     *
     * Other values in AbstractRequest are derived from this string.
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

    /**
     * @brief The city to get stop suggestions for (only needed if
     *   @ref TimetableAccessor::useSeparateCityValue returns true).
     **/
    QString city;

    /** @brief Describes what should be retrieved with the request, eg. departures or a a stop ID. */
    ParseDocumentMode parseMode;

    AbstractRequest()
    {
        this->parseMode = ParseInvalid;
        this->maxCount = -1;
        this->parseMode = ParseForStopSuggestions;
    };

    AbstractRequest( const QString &sourceName, const QString &stop, const QDateTime &dateTime,
            int maxCount, const QString &city = QString(), const QString &dataType = "departures",
            ParseDocumentMode parseMode = ParseForStopSuggestions )
            : sourceName(sourceName), dateTime(dateTime), stop(stop), maxCount(maxCount),
              dataType(dataType), city(city), parseMode(parseMode)
    {
    };

    AbstractRequest( const AbstractRequest &info )
            : sourceName(info.sourceName), dateTime(info.dateTime), stop(info.stop),
              maxCount(info.maxCount), dataType(info.dataType), city(info.city),
              parseMode(info.parseMode)
    {
    };

    virtual ~AbstractRequest() {};

    virtual AbstractRequest *clone() const = 0;
    virtual QScriptValue toScriptValue( QScriptEngine *engine ) const = 0;

    /** @brief Get the name of the script function that is associated with this request. */
    virtual QString functionName() const = 0;
};

struct StopSuggestionRequest : public AbstractRequest {
    StopSuggestionRequest() : AbstractRequest() { parseMode = ParseForStopSuggestions; };
    StopSuggestionRequest( const QString &sourceName, const QString &stop,
                           int maxCount, const QString &city = QString(),
                           ParseDocumentMode parseMode = ParseForStopSuggestions )
        : AbstractRequest(sourceName, stop, QDateTime(), maxCount, city, "departures", parseMode) {};
    StopSuggestionRequest( const StopSuggestionRequest &info ) : AbstractRequest(info) {};

    virtual AbstractRequest *clone() const
    {
        return new StopSuggestionRequest( sourceName, stop, maxCount, city, parseMode );
    };
    virtual QScriptValue toScriptValue( QScriptEngine *engine ) const;

    /** @brief Get the name of the script function that is associated with this request. */
    virtual QString functionName() const;
};

struct DepartureRequest : public AbstractRequest {
    DepartureRequest() : AbstractRequest() { parseMode = ParseForDeparturesArrivals; };
    DepartureRequest( const QString &sourceName, const QString &stop,
                      const QDateTime &dateTime, int maxCount,
                      const QString &city = QString(),
                      const QString &dataType = "departures",
                      ParseDocumentMode parseMode = ParseForDeparturesArrivals )
        : AbstractRequest(sourceName, stop, dateTime, maxCount, city, dataType, parseMode) {};
    DepartureRequest( const DepartureRequest &info ) : AbstractRequest(info) {};

    virtual AbstractRequest *clone() const
    {
        return new DepartureRequest( sourceName, stop, dateTime, maxCount, dataType,
                                         city, parseMode );
    };

    virtual QScriptValue toScriptValue( QScriptEngine *engine ) const;

    /** @brief Get the name of the script function that is associated with this request. */
    virtual QString functionName() const;
};

struct ArrivalRequest : public DepartureRequest {
    ArrivalRequest() : DepartureRequest() {};
    ArrivalRequest( const QString &sourceName, const QString &stop,
                    const QDateTime &dateTime, int maxCount,
                    const QString &city = QString(),
                    const QString &dataType = "arrivals",
                    ParseDocumentMode parseMode = ParseForDeparturesArrivals )
        : DepartureRequest(sourceName, stop, dateTime, maxCount, city, dataType, parseMode) {};
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

    JourneyRequest() : AbstractRequest()
    {
        this->roundTrips = 0;
        this->parseMode = ParseForJourneys;
    };

    JourneyRequest( const QString &sourceName, const QString &startStop,
            const QString &targetStop, const QDateTime &dateTime, int maxCount,
            const QString &urlToUse, const QString &city = QString(),
            const QString &dataType = "journeys",
            ParseDocumentMode parseMode = ParseForJourneys )
        : AbstractRequest(sourceName, startStop, dateTime, maxCount, city, dataType, parseMode),
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
                                   city, dataType, parseMode );
    };

    virtual QScriptValue toScriptValue( QScriptEngine *engine ) const;

    /** @brief Get the name of the script function that is associated with this request. */
    virtual QString functionName() const;
};

#endif // Multiple inclusion guard
