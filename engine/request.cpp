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
#include "serviceproviderscript.h"

// Qt includes
#include <QScriptEngine>

QString StopSuggestionRequest::functionName() const
{
    return ServiceProviderScript::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS;
}

QString DepartureRequest::functionName() const
{
    return ServiceProviderScript::SCRIPT_FUNCTION_GETTIMETABLE;
}

QString JourneyRequest::functionName() const
{
    return ServiceProviderScript::SCRIPT_FUNCTION_GETJOURNEYS;
}

QScriptValue StopSuggestionRequest::toScriptValue( QScriptEngine *engine ) const
{
    QScriptValue argument = engine->newObject();
    argument.setProperty( QLatin1String("stop"), stop );
    argument.setProperty( QLatin1String("city"), city );
    argument.setProperty( QLatin1String("maxCount"), maxCount );
    return argument;
}

QScriptValue DepartureRequest::toScriptValue( QScriptEngine *engine ) const
{
    QScriptValue argument = engine->newObject();
    argument.setProperty( QLatin1String("stop"), stop );
    argument.setProperty( QLatin1String("city"), city );
    argument.setProperty( QLatin1String("maxCount"), maxCount );
    argument.setProperty( QLatin1String("dateTime"), engine->newDate(dateTime) );
    argument.setProperty( QLatin1String("dataType"), dataType );
    return argument;
}

QScriptValue JourneyRequest::toScriptValue( QScriptEngine *engine ) const
{
    QScriptValue argument = engine->newObject();
    argument.setProperty( QLatin1String("stop"), stop );
    argument.setProperty( QLatin1String("city"), city );
    argument.setProperty( QLatin1String("maxCount"), maxCount );
    argument.setProperty( QLatin1String("originStop"), stop ); // Already in argument as "stop"
    argument.setProperty( QLatin1String("targetStop"), targetStop );
    argument.setProperty( QLatin1String("dateTime"), engine->newDate(dateTime) );
    argument.setProperty( QLatin1String("dataType"), dataType );
    return argument;
}
