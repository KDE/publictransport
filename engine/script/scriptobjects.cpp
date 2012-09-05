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
#include "scriptobjects.h"

// Own includes
#include "script_thread.h"

// KDE includes
#include <KLocalizedString>
#include <KDebug>

// Qt includes
#include <QMutex>

ScriptObjects::ScriptObjects()
        : scriptStorage(0), scriptNetwork(0), scriptResult(0), scriptHelper(0)
{
}

void ScriptObjects::clear()
{
    scriptStorage = QSharedPointer< Storage >( 0 );
    scriptHelper = QSharedPointer< Helper >( 0 );
    scriptNetwork = QSharedPointer< Network >( 0 );
    scriptResult = QSharedPointer< ResultObject >( 0 );
}

void ScriptObjects::createObjects( const ServiceProviderData *_data,
                                   const QScriptProgram &_scriptProgram )
{
    if ( _data ) {
        data = *_data;
    }
    if ( !_scriptProgram.isNull() ) {
        scriptProgram = _scriptProgram;
    }
    if ( !scriptStorage ) {
        scriptStorage = QSharedPointer< Storage >( new Storage(data.id()) );
    }
    if ( scriptNetwork.isNull() ) {
        scriptNetwork = QSharedPointer< Network >( new Network(data.fallbackCharset()) );
    }
    if ( scriptResult.isNull() ) {
        scriptResult = QSharedPointer< ResultObject >( new ResultObject() );
    }
    if ( !scriptHelper ) {
        scriptHelper = QSharedPointer< Helper >( new Helper(data.id()) );
    }
}

ScriptObjects ScriptObjects::fromEngine( QScriptEngine *engine,
                                         const QScriptProgram &scriptProgram )
{
    ScriptObjects objects;
    ServiceProviderData *data = qobject_cast< ServiceProviderData* >(
            engine->globalObject().property("provider").toQObject() );
    if ( data ) {
        objects.data = *data;
    }

    objects.scriptHelper =QSharedPointer< Helper >( qobject_cast< Helper* >(
            engine->globalObject().property("helper").toQObject() ) );
    objects.scriptNetwork = QSharedPointer< Network >( qobject_cast< Network* >(
            engine->globalObject().property("network").toQObject() ) );
    objects.scriptResult = QSharedPointer< ResultObject >( qobject_cast< ResultObject* >(
            engine->globalObject().property("result").toQObject() ) );
    objects.scriptStorage = QSharedPointer< Storage >( qobject_cast< Storage* >(
            engine->globalObject().property("storage").toQObject() ) );

    objects.scriptProgram = scriptProgram;
    return objects;
}

bool ScriptObjects::attachToEngine( QScriptEngine *engine )
{
    if ( !isValid() || scriptProgram.isNull() ) {
        kWarning() << "Cannot attach invalid objects"
                   << scriptHelper << scriptNetwork
                   << scriptResult << scriptStorage;
        return false;
    }

    // Register classes for use in the script
    qRegisterMetaType< QIODevice* >( "QIODevice*" );
    qRegisterMetaType< DataStreamPrototype* >( "DataStreamPrototype*" );
    qScriptRegisterMetaType< DataStreamPrototypePtr >( engine,
            dataStreamToScript, dataStreamFromScript );
    qScriptRegisterMetaType< NetworkRequestPtr >( engine,
            networkRequestToScript, networkRequestFromScript );

    const QScriptValue::PropertyFlags flags = QScriptValue::ReadOnly | QScriptValue::Undeletable;

    // Add a 'provider' object
    engine->globalObject().setProperty( "provider", engine->newQObject(&data), flags ); // TEST

    // Add an include() function
    QScriptValue includeFunction = engine->globalObject().property("include");
    if ( !includeFunction.isValid() ) {
        includeFunction = engine->newFunction( include, 1 );
    }
    if ( !scriptProgram.isNull() ) {
        QVariantHash includeData;
        includeData[ scriptProgram.fileName() ] = maxIncludeLine( scriptProgram.sourceCode() );
        includeFunction.setData( qScriptValueFromValue(engine, includeData) );
    }
    engine->globalObject().setProperty( "include", includeFunction, flags );

    QScriptValue streamConstructor = engine->newFunction( constructStream );
    QScriptValue streamMeta = engine->newQMetaObject(
            &DataStreamPrototype::staticMetaObject, streamConstructor );
    engine->globalObject().setProperty( "DataStream", streamMeta, flags );

    // Make the objects available to the script
    engine->globalObject().setProperty( "helper", engine->newQObject(scriptHelper.data()), flags );
    engine->globalObject().setProperty( "network", engine->newQObject(scriptNetwork.data()), flags );
    engine->globalObject().setProperty( "storage", engine->newQObject(scriptStorage.data()), flags );
    engine->globalObject().setProperty( "result", engine->newQObject(scriptResult.data()), flags );
    engine->globalObject().setProperty( "enum",
            engine->newQMetaObject(&ResultObject::staticMetaObject), flags );
    engine->globalObject().setProperty( "PublicTransport",
            engine->newQMetaObject(&Enums::staticMetaObject), flags );

    if ( !engine->globalObject().property("DataStream").isValid() ) {
        DataStreamPrototype *dataStream = new DataStreamPrototype( engine );
        QScriptValue stream = engine->newQObject( dataStream );
        QScriptValue streamConstructor = engine->newFunction( constructStream );
        engine->setDefaultPrototype( qMetaTypeId<DataStreamPrototype*>(), stream );
        engine->globalObject().setProperty( "DataStream", streamConstructor, flags );
    }

    // Import extensions (from XML file, <script extensions="...">)
    lastError.clear();
    foreach ( const QString &extension, data.scriptExtensions() ) {
        if ( !importExtension(engine, extension) ) {
            lastError = i18nc("@info/plain", "Could not import extension %1", extension);
            return false;
        }
    }

    return true;
}
