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

#ifndef SCRIPTOBJECTS_HEADER
#define SCRIPTOBJECTS_HEADER

#include "serviceproviderdata.h"
#include "scriptapi.h"

using namespace ScriptApi;

class QMutex;
class QScriptEngine;

struct ScriptData {
    /** @brief Read script data from the engine's global object. */
    static ScriptData fromEngine( QScriptEngine *engine,
                                  const QSharedPointer< QScriptProgram > &scriptProgram =
                                        QSharedPointer< QScriptProgram >() );

    ScriptData();
    ScriptData( const ScriptData &scriptData );
    ScriptData( const ServiceProviderData *data,
                const QSharedPointer< QScriptProgram > &scriptProgram );

    bool isValid() const { return provider.isValid() && program; };

    ServiceProviderData provider;
    QSharedPointer< QScriptProgram > program;
};
Q_DECLARE_METATYPE( ScriptData )

struct ScriptObjects {
    /** @brief Read script objects from the engine's global object. */
    static ScriptObjects fromEngine( QScriptEngine *engine );

    ScriptObjects();
    ScriptObjects( const ScriptObjects &other );

    void clear();
    void createObjects( const ServiceProviderData *data,
                        const QSharedPointer< QScriptProgram > &scriptProgram );
    void createObjects( const ScriptData &data = ScriptData() );
    bool attachToEngine( QScriptEngine *engine, const ScriptData &data );
    void moveToThread( QThread *thread );
    QThread *currentThread() const;

    bool isValid() const {
        return storage && network && result && helper;
    };

    QSharedPointer< Storage > storage;
    QSharedPointer< Network > network;
    QSharedPointer< ResultObject > result;
    QSharedPointer< Helper > helper;
    QString lastError;
};
Q_DECLARE_METATYPE( ScriptObjects )

#endif // Multiple inclusion guard
