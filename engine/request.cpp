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

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    // Own includes
    #include "script/serviceproviderscript.h"

    // Qt includes
    #include <QScriptEngine>
#endif

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
    return QString("{stop: \"%1\", city: \"%2\", maxCount: %3, dateTime: %4, dataType: %5}")
            .arg(m_stop, m_city).arg(m_maxCount)
            .arg(m_dateTime.toString(Qt::SystemLocaleShortDate), parseModeName());
}

QString ArrivalRequest::argumentsString() const
{
    return DepartureRequest::argumentsString();
}

QString StopSuggestionRequest::argumentsString() const
{
    return QString("{stop: \"%1\", city: \"%2\", maxCount: %3}").arg(m_stop, m_city).arg(m_maxCount);
}

QString StopsByGeoPositionRequest::argumentsString() const
{
    return QString("{longitude: %1, longitude: %2, distance: %3, maxCount: %4}")
            .arg(m_longitude).arg(m_latitude).arg(m_distance).arg(m_maxCount);
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
    return QString("{stop: \"%1\", city: \"%2\", maxCount: %3, originStop: \"%4\", "
                   "targetStop: \"%5\", dateTime: %6}")
            .arg(m_stop, m_city).arg(m_maxCount)
            .arg(m_stop, m_targetStop, m_dateTime.toString(Qt::SystemLocaleShortDate));
}

QString MoreItemsRequest::argumentsString() const
{
    Q_ASSERT( m_request );
    return Enums::toString(m_direction) + ": " + m_request->argumentsString();
}

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
QString StopSuggestionRequest::functionName() const
{
    return ServiceProviderScript::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS;
}

QString DepartureRequest::functionName() const
{ // Same name for ArrivalRequest
    return ServiceProviderScript::SCRIPT_FUNCTION_GETTIMETABLE;
}

QString JourneyRequest::functionName() const
{
    return ServiceProviderScript::SCRIPT_FUNCTION_GETJOURNEYS;
}

QString AdditionalDataRequest::functionName() const
{
    return ServiceProviderScript::SCRIPT_FUNCTION_GETADDITIONALDATA;
}

QScriptValue StopSuggestionRequest::toScriptValue( QScriptEngine *engine ) const
{
    QScriptValue value = engine->newObject();
    value.setProperty( QLatin1String("stop"), m_stop );
    value.setProperty( QLatin1String("city"), m_city );
    value.setProperty( QLatin1String("maxCount"), m_maxCount );
    return value;
}

QScriptValue StopsByGeoPositionRequest::toScriptValue( QScriptEngine *engine ) const
{
    QScriptValue value = engine->newObject();
    value.setProperty( QLatin1String("longitude"), m_longitude );
    value.setProperty( QLatin1String("latitude"), m_latitude );
    value.setProperty( QLatin1String("distance"), m_distance );
    value.setProperty( QLatin1String("maxCount"), m_maxCount );
    return value;
}

QScriptValue DepartureRequest::toScriptValue( QScriptEngine *engine ) const
{
    QScriptValue value = engine->newObject();
    value.setProperty( QLatin1String("stop"), m_stop );
    value.setProperty( QLatin1String("city"), m_city );
    value.setProperty( QLatin1String("maxCount"), m_maxCount );
    value.setProperty( QLatin1String("dateTime"), engine->newDate(m_dateTime) );
    value.setProperty( QLatin1String("dataType"), parseModeName() );
    value.setProperty( QLatin1String("moreItemsDirection"), Enums::RequestedItems );
    return value;
}

QScriptValue ArrivalRequest::toScriptValue( QScriptEngine *engine ) const
{
    QScriptValue value = DepartureRequest::toScriptValue( engine );
    value.setProperty( QLatin1String("dataType"), parseModeName() );
    return value;
}

QScriptValue JourneyRequest::toScriptValue( QScriptEngine *engine ) const
{
    QScriptValue value = engine->newObject();
    value.setProperty( QLatin1String("stop"), m_stop );
    value.setProperty( QLatin1String("city"), m_city );
    value.setProperty( QLatin1String("maxCount"), m_maxCount );
    value.setProperty( QLatin1String("originStop"), m_stop ); // Already in argument as "stop"
    value.setProperty( QLatin1String("targetStop"), m_targetStop );
    value.setProperty( QLatin1String("dateTime"), engine->newDate(m_dateTime) );
    value.setProperty( QLatin1String("moreItemsDirection"), Enums::RequestedItems );
    return value;
}

QScriptValue AdditionalDataRequest::toScriptValue( QScriptEngine *engine ) const
{
    QScriptValue value = engine->newObject();
//     value.setProperty( QLatin1String("stop"), stop );
//     value.setProperty( QLatin1String("city"), city );
    value.setProperty( QLatin1String("dataType"),
            m_sourceName.startsWith(QLatin1String("Arrivals"), Qt::CaseInsensitive)
            ? parseModeName(ParseForArrivals) : parseModeName(ParseForDepartures));
    value.setProperty( QLatin1String("transportLine"), m_transportLine );
    value.setProperty( QLatin1String("target"), m_target );
    value.setProperty( QLatin1String("dateTime"), engine->newDate(m_dateTime) );
    value.setProperty( QLatin1String("routeDataUrl"), m_routeDataUrl );
    return value;
}

QScriptValue MoreItemsRequest::toScriptValue( QScriptEngine *engine ) const
{
    Q_ASSERT( m_request );
    QScriptValue value = m_request->toScriptValue( engine );
    QScriptValue data = engine->newObject();
    for ( QVariantHash::ConstIterator it = m_requestData.constBegin();
          it != m_requestData.constEnd(); ++it )
    {
        data.setProperty( it.key(), engine->toScriptValue(it.value()) );
    }
    value.setProperty( QLatin1String("requestData"), data ); //engine->toScriptValue(requestData) );
    value.setProperty( QLatin1String("moreItemsDirection"), m_direction );
    return value;
}

#endif // BUILD_PROVIDER_TYPE_SCRIPT
