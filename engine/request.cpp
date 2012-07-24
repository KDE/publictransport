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

// Own includes
#include "script/serviceproviderscript.h"

// Qt includes
#include <QScriptEngine>

QString AbstractRequest::parseModeName() const
{
    return parseModeName( parseMode );
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
    value.setProperty( QLatin1String("stop"), stop );
    value.setProperty( QLatin1String("city"), city );
    value.setProperty( QLatin1String("maxCount"), maxCount );
    return value;
}

QScriptValue DepartureRequest::toScriptValue( QScriptEngine *engine ) const
{
    QScriptValue value = engine->newObject();
    value.setProperty( QLatin1String("stop"), stop );
    value.setProperty( QLatin1String("city"), city );
    value.setProperty( QLatin1String("maxCount"), maxCount );
    value.setProperty( QLatin1String("dateTime"), engine->newDate(dateTime) );
    value.setProperty( QLatin1String("dataType"), parseModeName() );
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
    value.setProperty( QLatin1String("stop"), stop );
    value.setProperty( QLatin1String("city"), city );
    value.setProperty( QLatin1String("maxCount"), maxCount );
    value.setProperty( QLatin1String("originStop"), stop ); // Already in argument as "stop"
    value.setProperty( QLatin1String("targetStop"), targetStop );
    value.setProperty( QLatin1String("dateTime"), engine->newDate(dateTime) );
    return value;
}

QScriptValue AdditionalDataRequest::toScriptValue( QScriptEngine *engine ) const
{
    QScriptValue value = engine->newObject();
    value.setProperty( QLatin1String("stop"), stop );
    value.setProperty( QLatin1String("city"), city );
    value.setProperty( QLatin1String("dataType"),
            sourceName.startsWith(QLatin1String("Arrivals"), Qt::CaseInsensitive)
            ? parseModeName(ParseForArrivals) : parseModeName(ParseForDepartures));
    value.setProperty( QLatin1String("transportLine"), transportLine );
    value.setProperty( QLatin1String("target"), target );
    value.setProperty( QLatin1String("dateTime"), engine->newDate(dateTime) );
    return value;
}

#endif // BUILD_PROVIDER_TYPE_SCRIPT
