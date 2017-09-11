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
#include "request.h"

QString AbstractRequest::parseModeName() const
{
    return parseModeName( m_parseMode );
}

QString AbstractRequest::parseModeName( ParseDocumentMode parseMode )
{
    switch ( parseMode ) {
    case ParseForDepartures:
        return "departures";
    case ParseForArrivals:
        return "arrivals";
    case ParseForJourneysByDepartureTime:
    case ParseForJourneysByArrivalTime:
        return "journeys";
    case ParseForStopSuggestions:
        return "stopSuggestions";
    case ParseInvalid:
        return "invalid";
    default:
        return "unknown";
    }
}

QString DepartureRequest::argumentsString() const
{
    return QString("{stop: \"%1\", stopIsId: \"%2\", city: \"%3\", count: %4, "
                   "dateTime: %5, dataType: %6}")
            .arg(m_stopId.isEmpty() ? m_stop : m_stopId)
            .arg(!m_stopId.isEmpty()).arg(m_city).arg(m_count)
            .arg(m_dateTime.toString(Qt::SystemLocaleShortDate), parseModeName());
}

QString ArrivalRequest::argumentsString() const
{
    return DepartureRequest::argumentsString();
}

QString StopSuggestionRequest::argumentsString() const
{
    return QString("{stop: \"%1\", city: \"%2\", count: %3}").arg(m_stop, m_city).arg(m_count);
}

QString StopsByGeoPositionRequest::argumentsString() const
{
    return QString("{longitude: %1, longitude: %2, distance: %3, count: %4}")
            .arg(m_longitude).arg(m_latitude).arg(m_distance).arg(m_count);
}

QString AdditionalDataRequest::argumentsString() const
{
    return QString("{dataType: %1, transportLine: \"%2\", "
                   "target: \"%3\", dateTime: %4, routeDataUrl: %5}")
            .arg(parseModeName(), m_transportLine, m_target,
                 m_dateTime.toString(Qt::SystemLocaleShortDate), m_routeDataUrl);
}

QString JourneyRequest::argumentsString() const
{
    return QString("{city: \"%1\", count: %2, "
                   "originStop: \"%3\", originStopIsId: \"%4\", "
                   "targetStop: \"%5\", targetStopIsId: \"%6\", dateTime: %7}")
            .arg(m_city).arg(m_count)
            .arg(m_stopId.isEmpty() ? m_stop : m_stopId)
            .arg(!m_stopId.isEmpty())
            .arg(m_targetStopId.isEmpty() ? m_targetStop : m_targetStopId)
            .arg(!m_targetStopId.isEmpty())
            .arg(m_dateTime.toString(Qt::SystemLocaleShortDate));
}

QString MoreItemsRequest::argumentsString() const
{
    Q_ASSERT( m_request );
    return Enums::toString(m_direction) + ": " + m_request->argumentsString();
}
