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
* @brief This file contains classes describing (timetable data) requests.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef REQUEST_HEADER
#define REQUEST_HEADER

// Own includes
#include "config.h"
#include "enums.h"


/**
 * @brief Stores information about a request to the publictransport data engine.
 *
 * All values other than @p sourceName can be derived (parsed) from the source name.
 **/
class AbstractRequest {
public:
    AbstractRequest( const QString &sourceName = QString(),
                     ParseDocumentMode parseMode = ParseInvalid )
            : m_sourceName(sourceName), m_parseMode(parseMode)
    {
    };

    AbstractRequest( const AbstractRequest &request )
            : m_sourceName(request.m_sourceName), m_parseMode(request.m_parseMode)
    {
    };

    virtual ~AbstractRequest() {};

    virtual AbstractRequest *clone() const = 0;
    virtual QString argumentsString() const = 0;

    /**
     * @brief The requesting source name.
     * Other values in AbstractRequest are derived from this string.
     **/
    QString sourceName() const { return m_sourceName; };

    /** @brief Describes what should be retrieved with the request, eg. departures or a a stop ID. */
    ParseDocumentMode parseMode() const { return m_parseMode; };

    void setParseMode( ParseDocumentMode parseMode ) { m_parseMode = parseMode; };

    QString parseModeName() const;
    static QString parseModeName( ParseDocumentMode parseMode );

protected:
    QString m_sourceName;
    ParseDocumentMode m_parseMode;
};

class AbstractTimetableItemRequest : public AbstractRequest {
public:
    AbstractTimetableItemRequest( const QString &sourceName = QString(),
                                  ParseDocumentMode parseMode = ParseInvalid )
            : AbstractRequest(sourceName, parseMode), m_count(20) {};
    AbstractTimetableItemRequest( const QString &sourceName, const QString &stop,
                                  const QString &stopId, const QDateTime &dateTime, int count,
                                  const QString &city = QString(),
                                  ParseDocumentMode parseMode = ParseInvalid )
            : AbstractRequest(sourceName, parseMode),
              m_dateTime(dateTime), m_stop(stop), m_stopId(stopId), m_count(count), m_city(city) {};
    AbstractTimetableItemRequest( const AbstractTimetableItemRequest &other )
            : AbstractRequest(other), m_dateTime(other.m_dateTime), m_stop(other.m_stop),
              m_stopId(other.m_stopId), m_count(other.m_count), m_city(other.m_city) {};

    virtual ~AbstractTimetableItemRequest() {};

    virtual AbstractRequest *clone() const = 0;
    virtual QString argumentsString() const = 0;


    /** @brief The date and time to get results for. */
    QDateTime dateTime() const { return m_dateTime; };

    /** @brief The stop name of the request. */
    QString stop() const { return m_stop; };

    /** @brief The stop ID of the request. */
    QString stopId() const { return m_stopId; };

    /**
     * @brief The number of timetable items to request, eg. departures or stop suggestions.
     * @note This is just a hint for the provider
     **/
    int count() const { return m_count; };

    /**
     * @brief The city to get stop suggestions for.
     * This is only needed if @ref ServiceProvider::useSeparateCityValue returns @c true.
     **/
    QString city() const { return m_city; };

    void setDateTime( const QDateTime &dateTime ) { m_dateTime = dateTime; };
    void setStop( const QString &stop ) { m_stop = stop; };
    void setStopId( const QString &stopId ) { m_stopId = stopId; };
    void setCity( const QString &city ) { m_city = city; };
    void setCount( int count ) { m_count = count; };

protected:
    /** @brief The date and time to get results for. */
    QDateTime m_dateTime;
    QString m_stop;
    QString m_stopId;
    int m_count;
    QString m_city;
};

class StopSuggestionRequest : public AbstractTimetableItemRequest {
public:
    StopSuggestionRequest( const QString &sourceName = QString(),
                           ParseDocumentMode parseMode = ParseForStopSuggestions )
        : AbstractTimetableItemRequest(sourceName, parseMode) {};
    StopSuggestionRequest( const QString &sourceName, const QString &stop,
                           int count, const QString &city = QString(),
                           ParseDocumentMode parseMode = ParseForStopSuggestions )
        : AbstractTimetableItemRequest(sourceName, stop, QString(), QDateTime(),
                                       count, city, parseMode) {};
    StopSuggestionRequest( const StopSuggestionRequest &other )
        : AbstractTimetableItemRequest(other) {};

    virtual AbstractRequest *clone() const { return new StopSuggestionRequest(*this); };
    virtual QString argumentsString() const;

};

class StopsByGeoPositionRequest : public StopSuggestionRequest {
public:
    StopsByGeoPositionRequest( const QString &sourceName = QString(),
                               ParseDocumentMode parseMode = ParseForStopSuggestions )
            : StopSuggestionRequest(sourceName, parseMode),
              m_longitude(0.0), m_latitude(0.0), m_distance(5000) {};
    StopsByGeoPositionRequest( const QString &sourceName, qreal longitude, qreal latitude,
                               int count = 200, int distance = 5000,
                               ParseDocumentMode parseMode = ParseForStopSuggestions )
            : StopSuggestionRequest(sourceName, QString(), count, QString(), parseMode),
              m_longitude(longitude), m_latitude(latitude), m_distance(distance) {};
    StopsByGeoPositionRequest( const StopsByGeoPositionRequest &request )
            : StopSuggestionRequest(request),
              m_longitude(request.m_longitude), m_latitude(request.m_latitude),
              m_distance(request.m_distance) {};

    virtual AbstractRequest *clone() const { return new StopsByGeoPositionRequest(*this); };
    virtual QString argumentsString() const;

    /** @brief The longitude of the stop. */
    qreal longitude() const { return m_longitude; };

    /** @brief The latitude of the stop. */
    qreal latitude() const { return m_latitude; };

    /** @brief Maximal distance in meters from the position at longitude/latitude. */
    int distance() const { return m_distance; };

    void setLongitude( qreal longitude ) { m_longitude = longitude; };
    void setLatitude( qreal latitude ) { m_latitude = latitude; };
    void setDistance( int distance ) { m_distance = distance; };

protected:
    qreal m_longitude;
    qreal m_latitude;
    int m_distance;
};

class DepartureRequest : public AbstractTimetableItemRequest {
public:
    DepartureRequest( const QString &sourceName = QString(),
                      ParseDocumentMode parseMode = ParseForDepartures )
        : AbstractTimetableItemRequest(sourceName, parseMode) {};
    DepartureRequest( const QString &sourceName, const QString &stop, const QString &stopId,
                      const QDateTime &dateTime, int count, const QString &city = QString(),
                      ParseDocumentMode parseMode = ParseForDepartures )
        : AbstractTimetableItemRequest(sourceName, stop, stopId, dateTime,
                                       count, city, parseMode) {};
    DepartureRequest( const DepartureRequest &info ) : AbstractTimetableItemRequest(info) {};

    virtual AbstractRequest *clone() const { return new DepartureRequest(*this); };
    virtual QString argumentsString() const;

};

class ArrivalRequest : public DepartureRequest {
public:
    ArrivalRequest( const QString &sourceName = QString(),
                    ParseDocumentMode parseMode = ParseForArrivals )
        : DepartureRequest(sourceName, parseMode) {};
    ArrivalRequest( const QString &sourceName, const QString &stop, const QString &stopId,
                    const QDateTime &dateTime, int count, const QString &city = QString(),
                    ParseDocumentMode parseMode = ParseForArrivals )
        : DepartureRequest(sourceName, stop, stopId, dateTime, count, city, parseMode) {};

    virtual AbstractRequest *clone() const { return new ArrivalRequest(*this); };
    virtual QString argumentsString() const;

};

/**
 * @brief Stores information about a request for journeys to the publictransport data engine.
 *
 * All values other than @p sourceName are derived (parsed) from it or represent the current state
 * of the request, eg. @p roundTrips stores the number of requests sent to a server to get
 * enough data (each round trip adds some data).
 **/
class JourneyRequest : public AbstractTimetableItemRequest {
public:
    JourneyRequest( const QString &sourceName = QString(),
                    ParseDocumentMode parseMode = ParseForJourneysByDepartureTime )
            : AbstractTimetableItemRequest(sourceName, parseMode), m_roundTrips(0)
    {
    };

    JourneyRequest( const QString &sourceName, const QString &startStop, const QString &startStopId,
            const QString &targetStop, const QString &targetStopId, const QDateTime &dateTime,
            int count, const QString &urlToUse, const QString &city = QString(),
            ParseDocumentMode parseMode = ParseForJourneysByDepartureTime )
        : AbstractTimetableItemRequest(sourceName, startStop, startStopId, dateTime,
                                       count, city, parseMode),
          m_targetStop(targetStop), m_targetStopId(targetStopId), m_urlToUse(urlToUse),
          m_roundTrips(0)
    {
    };

    JourneyRequest( const JourneyRequest &other )
            : AbstractTimetableItemRequest(other), m_targetStop(other.m_targetStop),
              m_targetStopId(other.m_targetStopId), m_urlToUse(other.m_urlToUse),
              m_roundTrips(other.m_roundTrips)
    {
    };

    virtual JourneyRequest *clone() const { return new JourneyRequest(*this); };
    virtual QString argumentsString() const;


    /** @brief The target stop name of the request. */
    QString targetStop() const { return m_targetStop; };

    /** @brief The ID of the target stop of the request. */
    QString targetStopId() const { return m_targetStopId; };

    /** @brief Specifies the URL to use to download the journey source document. */
    QString urlToUse() const { return m_urlToUse; };

    /** @brief Current number of round trips used to fulfil the journey request. */
    int roundTrips() const { return m_roundTrips; };

    void setTargetStop( const QString &targetStop ) { m_targetStop = targetStop; };

    void setTargetStopId( const QString &targetStopId ) { m_targetStopId = targetStopId; };

protected:
    QString m_targetStop;
    QString m_targetStopId;
    QString m_urlToUse;
    int m_roundTrips;
};

class AdditionalDataRequest : public AbstractTimetableItemRequest {
public:
    AdditionalDataRequest( const QString &sourceName = QString(),
                           ParseDocumentMode parseMode = ParseForAdditionalData )
        : AbstractTimetableItemRequest(sourceName, parseMode) {};
    AdditionalDataRequest( const QString &sourceName, int itemNumber, const QString &stop,
                           const QString &stopId, const QDateTime &dateTime,
                           const QString &transportLine, const QString &target,
                           const QString &city = QString(), const QString &routeDataUrl = QString(),
                           ParseDocumentMode parseMode = ParseForAdditionalData )
        : AbstractTimetableItemRequest(sourceName, stop, stopId, dateTime, 1, city, parseMode),
          m_itemNumber(itemNumber), m_transportLine(transportLine), m_target(target),
          m_routeDataUrl(routeDataUrl) {};
    AdditionalDataRequest( const AdditionalDataRequest &other )
        : AbstractTimetableItemRequest(other), m_itemNumber(other.m_itemNumber),
          m_transportLine(other.m_transportLine), m_target(other.m_target),
          m_routeDataUrl(other.m_routeDataUrl) {};

    virtual AbstractRequest *clone() const { return new AdditionalDataRequest(*this); };
    virtual QString argumentsString() const;

    int itemNumber() const { return m_itemNumber; };
    QString transportLine() const { return m_transportLine; };
    QString target() const { return m_target; };
    QString routeDataUrl() const { return m_routeDataUrl; };

protected:
    int m_itemNumber;
    QString m_transportLine;
    QString m_target;
    QString m_routeDataUrl;
};

class MoreItemsRequest : public AbstractRequest {
public:
    MoreItemsRequest( const QString &sourceName = QString(),
                      const QSharedPointer<AbstractRequest> &request = QSharedPointer<AbstractRequest>(),
                      const QVariantMap &requestData = QVariantMap(),
                      Enums::MoreItemsDirection direction = Enums::LaterItems )
            : AbstractRequest(sourceName, request ? request->parseMode() : ParseInvalid),
              m_request(request), m_requestData(requestData), m_direction(direction)
    {
        Q_ASSERT( request );
    };
    MoreItemsRequest( const MoreItemsRequest &other )
            : AbstractRequest(other), m_request(other.m_request),
              m_requestData(other.m_requestData), m_direction(other.m_direction) {};

    virtual AbstractRequest *clone() const { return new MoreItemsRequest(*this); };
    virtual QString argumentsString() const;

    QSharedPointer< AbstractRequest > request() const { return m_request; };
    QVariantMap requestData() const { return m_requestData; };
    Enums::MoreItemsDirection direction() const { return m_direction; };

protected:
    QSharedPointer< AbstractRequest > m_request;
    QVariantMap m_requestData;
    Enums::MoreItemsDirection m_direction;
};

Q_DECLARE_METATYPE(DepartureRequest)
Q_DECLARE_METATYPE(ArrivalRequest)
Q_DECLARE_METATYPE(JourneyRequest)
Q_DECLARE_METATYPE(StopSuggestionRequest)
Q_DECLARE_METATYPE(StopsByGeoPositionRequest)
Q_DECLARE_METATYPE(AdditionalDataRequest)

#endif // Multiple inclusion guard
