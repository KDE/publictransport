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
#include <QApplication>
#include <QNetworkAccessManager>

ScriptObjects::ScriptObjects()
        : storage(0), network(0), result(0), helper(0)
{
}

ScriptObjects::ScriptObjects( const ScriptObjects &other )
        : storage(other.storage), network(other.network), result(other.result),
          helper(other.helper), lastError(other.lastError)
{
}

ScriptData::ScriptData()
{
}

ScriptData::ScriptData( const ScriptData &scriptData )
        : provider(scriptData.provider), program(scriptData.program)
{
}

ScriptData::ScriptData( const ServiceProviderData *data,
                        const QSharedPointer< QScriptProgram > &scriptProgram )
        : provider(data ? *data : ServiceProviderData()), program(scriptProgram)
{
}

void ScriptObjects::clear()
{
    storage = QSharedPointer< Storage >( 0 );
    helper = QSharedPointer< Helper >( 0 );
    network = QSharedPointer< Network >( 0 );
    result = QSharedPointer< ResultObject >( 0 );
}

void ScriptObjects::createObjects( const ServiceProviderData *data,
                                   const QSharedPointer< QScriptProgram > &scriptProgram )
{
    createObjects( ScriptData(data, scriptProgram) );
}

void ScriptObjects::createObjects( const ScriptData &data )
{
    if ( !storage ) {
        storage = QSharedPointer< Storage >( new Storage(data.provider.id()) );
    }
    if ( !network ) {
        network = QSharedPointer< Network >( new Network(data.provider.fallbackCharset()) );
    }
    if ( !result ) {
        result = QSharedPointer< ResultObject >( new ResultObject() );
    }
    if ( !helper ) {
        helper = QSharedPointer< Helper >( new Helper(data.provider.id()) );
    }
}

ScriptData ScriptData::fromEngine( QScriptEngine *engine,
                                   const QSharedPointer< QScriptProgram > &scriptProgram )
{
    ServiceProviderData *data = qobject_cast< ServiceProviderData* >(
            engine->globalObject().property("provider").toQObject() );
    return ScriptData( data, scriptProgram );
}

ScriptObjects ScriptObjects::fromEngine( QScriptEngine *engine )
{
    ScriptObjects objects;
    objects.helper = QSharedPointer< Helper >( qobject_cast< Helper* >(
            engine->globalObject().property("helper").toQObject() ) );
    objects.network = QSharedPointer< Network >( qobject_cast< Network* >(
            engine->globalObject().property("network").toQObject() ) );
    objects.result = QSharedPointer< ResultObject >( qobject_cast< ResultObject* >(
            engine->globalObject().property("result").toQObject() ) );
    objects.storage = QSharedPointer< Storage >( qobject_cast< Storage* >(
            engine->globalObject().property("storage").toQObject() ) );
    return objects;
}

void ScriptObjects::moveToThread( QThread *thread )
{
    if ( helper ) {
        helper->moveToThread( thread );
    }
    if ( network ) {
        network->moveToThread( thread );
    }
    if ( result ) {
        result->moveToThread( thread );
    }
    if ( storage ) {
        storage->moveToThread( thread );
    }
}

QThread *ScriptObjects::currentThread() const
{
    return helper ? helper->thread() : 0;
}

bool ScriptObjects::attachToEngine( QScriptEngine *engine, const ScriptData &data )
{
    if ( !isValid() ) {
        kDebug() << "Attaching invalid objects" << helper << network << result << storage;
    } else if ( !data.program ) {
        kDebug() << "Attaching invalid data";
    }

    if ( engine->isEvaluating() ) {
        kWarning() << "Cannot attach objects while evaluating";
        return false;
    }

    // Register classes for use in the script
    qScriptRegisterMetaType< DataStreamPrototypePtr >( engine,
            dataStreamToScript, dataStreamFromScript );
    qScriptRegisterMetaType< NetworkRequestPtr >( engine,
            networkRequestToScript, networkRequestFromScript );

    const QScriptValue::PropertyFlags flags = QScriptValue::ReadOnly | QScriptValue::Undeletable;

    // Add a 'provider' object, clone a new ServiceProviderData object
    // for the case that the ScriptData instance gets deleted
    engine->globalObject().setProperty( "provider",
                                        engine->newQObject(data.provider.clone(engine)), flags );

    // Add an include() function
    QScriptValue includeFunction = engine->globalObject().property("include");
    if ( !includeFunction.isValid() ) {
        includeFunction = engine->newFunction( include, 1 );
    }
    if ( data.program ) {
        QVariantHash includeData;
        includeData[ data.program->fileName() ] = maxIncludeLine( data.program->sourceCode() );
        includeFunction.setData( qScriptValueFromValue(engine, includeData) );
    }
    engine->globalObject().setProperty( "include", includeFunction, flags );

    QScriptValue streamConstructor = engine->newFunction( constructStream );
    QScriptValue streamMeta = engine->newQMetaObject(
            &DataStreamPrototype::staticMetaObject, streamConstructor );
    engine->globalObject().setProperty( "DataStream", streamMeta, flags );

    // Make the objects available to the script
    engine->globalObject().setProperty( "helper", helper.isNull()
            ? engine->undefinedValue() : engine->newQObject(helper.data()), flags );
    engine->globalObject().setProperty( "network", network.isNull()
            ? engine->undefinedValue() : engine->newQObject(network.data()), flags );
    engine->globalObject().setProperty( "storage", storage.isNull()
            ? engine->undefinedValue() : engine->newQObject(storage.data()), flags );
    engine->globalObject().setProperty( "result", result.isNull()
            ? engine->undefinedValue() : engine->newQObject(result.data()), flags );
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
    foreach ( const QString &extension, data.provider.scriptExtensions() ) {
        if ( !importExtension(engine, extension) ) {
            lastError = i18nc("@info/plain", "Could not import extension %1", extension);
            return false;
        }
    }

    return true;
}
