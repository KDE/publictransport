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
#include "script_thread.h"

// Own includes
#include "scripting.h"
#include "timetableaccessor_script.h"
#include "timetableaccessor_info.h"
#include "request.h"

// KDE includes
#include <ThreadWeaver/Thread>
#include <ThreadWeaver/Weaver>
#include <KLocalizedString>
#include <KDebug>

// Qt includes
#include <QScriptEngine>
#include <QScriptEngineAgent>
#include <QScriptValueIterator>
#include <QFile>
#include <QTimer>
#include <QEventLoop>

static int thread_count = 0;
ScriptJob::ScriptJob( QScriptProgram *script, const TimetableAccessorInfo* info,
                      Storage *scriptStorage, QObject* parent )
    : ThreadWeaver::Job( parent ),
      m_engine(0), m_script(script),
      m_scriptStorage(scriptStorage), m_scriptNetwork(0), m_scriptResult(0),
      m_published(0), m_success(true),
      m_info( *info )
{
    ++thread_count; kDebug() << "Thread count:" << thread_count;
    qRegisterMetaType<DepartureRequest>( "DepartureRequest" );
    qRegisterMetaType<JourneyRequest>( "JourneyRequest" );
    qRegisterMetaType<StopSuggestionRequest>( "StopSuggestionRequest" );
}

ScriptJob::~ScriptJob()
{
    --thread_count; kDebug() << "Thread count:" << thread_count;
    if ( !m_engine ) {
        return;
    }

    m_scriptNetwork->abortAllRequests();
    m_engine->abortEvaluation();
    m_engine->deleteLater();

    m_scriptNetwork.clear();
    m_scriptResult.clear();
//     m_scriptNetwork->deleteLater();
//     m_scriptResult->deleteLater();
}


ScriptAgent::ScriptAgent( QScriptEngine* engine, QObject *parent )
        : QObject(parent), QScriptEngineAgent(engine)
{
}

void ScriptAgent::functionExit( qint64 scriptId, const QScriptValue& returnValue )
{
    Q_UNUSED( scriptId );
    Q_UNUSED( returnValue );
    QTimer::singleShot( 250, this, SLOT(checkExecution()) );
}

void ScriptAgent::checkExecution()
{
    if ( !engine()->isEvaluating() ) {
        emit scriptFinished();
    }
}

void ScriptJob::run()
{
//                 GlobalTimetableInfo globalInfo;
//                 globalInfo.requestDate = QDate::currentDate();
// //                 ResultObject obj;
//     m_scriptResult = QSharedPointer<ResultObject>( new ResultObject(thread()) );
//                 QVariantMap item;
//                 for ( int i = 0; i < 100; ++i ) {
//                     item.insert("StopName", "Test Haltestelle " + QString::number(i) + " mit Speicherfresser-Namen für mehr offensichtlichen Verbrauch :)#+125$%4&&%$" );
//                     item.insert("StopID", QString::number(100 * i) );
//                     item.insert("StopWeight", 100 );
//                     m_scriptResult->addData( item );
//                 }
//                 emit stopSuggestionsReady( m_scriptResult->data(),
//                         m_scriptResult->features(), m_scriptResult->hints(),
//                         QString(), globalInfo,
//                         *dynamic_cast<const StopSuggestionRequest*>(request()),
//                         true );
//                 m_scriptResult.clear();
//                 delete m_scriptResult.data();
//                 return;

    if ( !loadScript(m_script) ) {
        kDebug() << "Script could not be loaded correctly";
        return;
    }
    kDebug() << "Run script job";
    kDebug() << "JOB:" << request()->stop << request()->dateTime << request()->parseMode;
//     emit begin( m_sourceName );

    // Store start time of the script
    QTime time;
    time.start();

    // Add call to the appropriate function
    QString functionName;
    QScriptValueList arguments = QScriptValueList() << request()->toScriptValue( m_engine );
    kDebug() << "Stop" << request()->toScriptValue( m_engine ).property("stop").toString();
    switch ( request()->parseMode ) {
    case ParseForDeparturesArrivals:
        functionName = TimetableAccessorScript::SCRIPT_FUNCTION_GETTIMETABLE;
        break;
    case ParseForJourneys: {
        functionName = TimetableAccessorScript::SCRIPT_FUNCTION_GETJOURNEYS;
        break;
    } case ParseForStopSuggestions:
        functionName = TimetableAccessorScript::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS;
        break;
    default:
        kDebug() << "Parse mode unsupported:" << request()->parseMode;
        break;
    }

    if ( functionName.isEmpty() ) {
        // This should never happen, therefore no i18n
        m_errorString = "Unknown parse mode";
        m_success = false;
    } else {
        kDebug() << "Call script function" << functionName;
        if ( m_scriptNetwork.isNull() ) {
            kDebug() << "Deleted ------------------------------------------------";
            m_engine->deleteLater();
            m_engine = 0;
            m_scriptNetwork.clear();
            m_scriptResult.clear();
            m_success = false;
            return;
        }

        // Call script function
        QScriptValue function = m_engine->globalObject().property( functionName );
        if ( !function.isFunction() ) {
            kDebug() << "Did not find" << functionName << "function in the script!";
        }

        QScriptValue result = function.call( QScriptValue(), arguments );
        if ( m_engine->hasUncaughtException() ) {
            kDebug() << "Error in the script when calling function" << functionName
                        << m_engine->uncaughtExceptionLineNumber()
                        << m_engine->uncaughtException().toString();
            kDebug() << "Backtrace:" << m_engine->uncaughtExceptionBacktrace().join("\n");
            m_errorString = i18nc("@info/plain", "Error in the script when calling function '%1': "
                    "<message>%2</message>.", functionName, m_engine->uncaughtException().toString());
            m_engine->deleteLater();
            m_engine = 0;
            m_scriptNetwork.clear();
            m_scriptResult.clear();
//             m_scriptNetwork->deleteLater();
//             m_scriptNetwork = 0;
//             m_scriptResult->deleteLater();
//             m_scriptResult = 0;
            m_success = false;
            return;
        }

        GlobalTimetableInfo globalInfo;
        globalInfo.requestDate = QDate::currentDate();
        globalInfo.delayInfoAvailable =
                !m_scriptResult->isHintGiven( ResultObject::NoDelaysForStop );

        while ( m_scriptNetwork->hasRunningRequests() || m_engine->isEvaluating() ) {
            // Wait for running requests to finish
            QEventLoop eventLoop;
            ScriptAgent agent( m_engine );
            QTimer::singleShot( 30000, &eventLoop, SLOT(quit()) );
            connect( this, SIGNAL(destroyed(QObject*)), &eventLoop, SLOT(quit()) );
            connect( &agent, SIGNAL(scriptFinished()), &eventLoop, SLOT(quit()) );
            connect( m_scriptNetwork.data(), SIGNAL(allRequestsFinished()),
                     &eventLoop, SLOT(quit()) );

            kDebug() << "Waiting for script to finish..." << thread();
            eventLoop.exec();
        }

        // Inform about script run time
        kDebug() << " > Script finished after" << (time.elapsed() / 1000.0)
                    << "seconds: " << m_info.scriptFileName() << thread() << request()->parseMode;

        // If data for the current job has already been published, do not emit
        // completed with an empty resultset
        if ( m_published == 0 || m_scriptResult->count() > m_published ) {
            const bool couldNeedForcedUpdate = m_published > 0;
//             emitReady(
            switch ( request()->parseMode ) {
            case ParseForDeparturesArrivals: {
                const DepartureRequest *departureRequest =
                        dynamic_cast< const DepartureRequest* >( request() );
                if ( departureRequest ) {
                    emit departuresReady( m_scriptResult->data().mid(m_published),
                            m_scriptResult->features(), m_scriptResult->hints(),
                            m_scriptNetwork->lastUrl(), globalInfo,
                            *departureRequest, couldNeedForcedUpdate );
                } else {
                    emit arrivalsReady( m_scriptResult->data().mid(m_published),
                            m_scriptResult->features(), m_scriptResult->hints(),
                            m_scriptNetwork->lastUrl(), globalInfo,
                            *dynamic_cast<const ArrivalRequest*>(request()),
                            couldNeedForcedUpdate );
                }
                break;
            }
            case ParseForJourneys:
                emit journeysReady( m_scriptResult->data().mid(m_published),
                        m_scriptResult->features(), m_scriptResult->hints(),
                        m_scriptNetwork->lastUrl(), globalInfo,
                        *dynamic_cast<const JourneyRequest*>(request()),
                        couldNeedForcedUpdate );
                break;
            case ParseForStopSuggestions:
                emit stopSuggestionsReady( m_scriptResult->data().mid(m_published),
                        m_scriptResult->features(), m_scriptResult->hints(),
                        m_scriptNetwork->lastUrl(), globalInfo,
                        *dynamic_cast<const StopSuggestionRequest*>(request()),
                        couldNeedForcedUpdate );
                break;

            default:
                kDebug() << "Parse mode unsupported:" << request()->parseMode;
                break;
            }
//             emit dataReady( m_scriptResult->data().mid(m_published),
//                             m_scriptResult->features(), m_scriptResult->hints(),
//                             m_scriptNetwork->lastUrl(), globalInfo,
//                             request(), couldNeedForcedUpdate );
        }
//         emit end( m_sourceName );

        // Cleanup
        m_scriptResult->clear();
        m_scriptStorage->checkLifetime();

        if ( m_engine->hasUncaughtException() ) {
            kDebug() << "Error in the script when calling function" << functionName
                        << m_engine->uncaughtExceptionLineNumber()
                        << m_engine->uncaughtException().toString();
            kDebug() << "Backtrace:" << m_engine->uncaughtExceptionBacktrace().join("\n");
            m_errorString = i18nc("@info/plain", "Error in the script when calling function '%1': "
                    "<message>%2</message>.", functionName, m_engine->uncaughtException().toString());
            m_engine->deleteLater();
            m_engine = 0;
            m_scriptNetwork.clear();
//             m_scriptNetwork->deleteLater();
//             m_scriptNetwork = 0;
//             m_scriptResult->deleteLater();
//             m_scriptResult = 0;
            m_success = false;
            return;
        }
    }
}

QScriptValue networkRequestToScript( QScriptEngine *engine, const NetworkRequestPtr &request )
{
    return engine->newQObject( const_cast<NetworkRequest*>(request), QScriptEngine::QtOwnership,
                               QScriptEngine::PreferExistingWrapperObject );
}

void networkRequestFromScript( const QScriptValue &object, NetworkRequestPtr &request )
{
    request = qobject_cast<NetworkRequest*>( object.toQObject() );
}

bool importExtension( QScriptEngine *engine, const QString &extension )
{
    if ( !TimetableAccessorScript::allowedExtensions().contains(extension, Qt::CaseInsensitive) ) {
        if ( engine->availableExtensions().contains(extension, Qt::CaseInsensitive) ) {
            kDebug() << "Extension" << extension << "is not allowed currently";
        } else {
            kDebug() << "Extension" << extension << "could not be found";
            kDebug() << "Available extensions:" << engine->availableExtensions();
        }
        kDebug() << "Allowed extensions:" << TimetableAccessorScript::allowedExtensions();
        return false;
    } else {
        kDebug() << "Import extension" << extension << engine << engine->thread();

        // FIXME: This crashes, if it gets called simultanously from multiple threads
        // (with separate script engines)...
        if ( engine->importExtension(extension).isUndefined() ) {
            return true;
        } else {
            if ( engine->hasUncaughtException() ) {
                kDebug() << "Could not import extension" << extension
                         << engine->uncaughtExceptionLineNumber()
                         << engine->uncaughtException().toString();
                kDebug() << "Backtrace:" << engine->uncaughtExceptionBacktrace().join("\n");
            }
            return false;
        }
    }
}

bool ScriptJob::loadScript( QScriptProgram *script )
{
    // Create script engine
    kDebug() << "Create QScriptEngine";
    m_engine = new QScriptEngine( thread() );
//     m_scriptStorage->mutex.lock();
//     kDebug() << "LOAD qt.core" << thread() << m_parseMode;
//     m_engine->importExtension("qt.core");
    foreach ( const QString &extension, m_info.scriptExtensions() ) {
        if ( !importExtension(m_engine, extension) ) {
            m_errorString = i18nc("@info/plain", "Could not load script extension "
                                  "<resource>%1</resource>.", extension);
            m_success = false;
            m_engine->deleteLater();
            m_engine = 0;
            return false;
        }
    }
//     m_scriptStorage->mutex.unlock();

    m_engine->globalObject().setProperty( "accessor", m_engine->newQObject(&m_info) ); //parent()) );

    // Add "importExtension()" function to import extensions
    // Importing Kross not from the GUI thread causes some warnings about pixmaps being used
    // outside the GUI thread...
//     m_engine->globalObject().setProperty( "importExtension",
//                                           m_engine->newFunction(importExtension, 1) );

    // Process events every 100ms
//     m_engine->setProcessEventsInterval( 100 );

    // Register NetworkRequest class for use in the script
    qScriptRegisterMetaType< NetworkRequestPtr >( m_engine,
            networkRequestToScript, networkRequestFromScript );

    // Create objects for the script
    Helper *scriptHelper = new Helper( m_info.serviceProvider(), m_engine );
    m_scriptNetwork = QSharedPointer<Network>( new Network(m_info.fallbackCharset(), thread()) );
    m_scriptResult = QSharedPointer<ResultObject>( new ResultObject(thread()) );
    connect( m_scriptResult.data(), SIGNAL(publish()), this, SLOT(publish()) );

    // Make the objects available to the script
    m_engine->globalObject().setProperty( "helper", m_engine->newQObject(scriptHelper) );
    m_engine->globalObject().setProperty( "network", m_engine->newQObject(m_scriptNetwork.data()) );
    m_engine->globalObject().setProperty( "storage", m_engine->newQObject(m_scriptStorage) );
    m_engine->globalObject().setProperty( "result", m_engine->newQObject(m_scriptResult.data()) );
    m_engine->globalObject().setProperty( "enum",
            m_engine->newQMetaObject(&ResultObject::staticMetaObject) );

    // Load the script program
    m_engine->evaluate( *script );
    if ( m_engine->hasUncaughtException() ) {
        kDebug() << "Error in the script" << m_engine->uncaughtExceptionLineNumber()
                 << m_engine->uncaughtException().toString();
        kDebug() << "Backtrace:" << m_engine->uncaughtExceptionBacktrace().join("\n");
        m_errorString = i18nc("@info/plain", "Error in the script: "
                "<message>%1</message>.", m_engine->uncaughtException().toString());
        m_engine->deleteLater();
        m_engine = 0;
        m_scriptNetwork.clear();
        m_scriptResult.clear();
//         m_scriptNetwork->deleteLater();
//         m_scriptNetwork = 0;
//         m_scriptResult->deleteLater();
//         m_scriptResult = 0;
        m_success = false;
        return false;
    } else {
        return true;
    }
}

void ScriptJob::publish()
{
    qDebug() << "PUBLISH" << request()->parseMode << m_scriptResult->count() << "'ED" << m_published << m_scriptResult << m_scriptNetwork << thread();
    // Only publish, if there is data which is not already published
    if ( m_scriptResult->count() > m_published ) {
        GlobalTimetableInfo globalInfo;
        QList< TimetableData > data = m_scriptResult->data().mid( m_published );
        kDebug() << "Publish" << data.count() << "items" << request()->sourceName;
        const bool couldNeedForcedUpdate = m_published > 0;
//         emit dataReady( data, m_scriptResult->features(), m_scriptResult->hints(),
//                         m_scriptNetwork->lastUrl(), globalInfo, request(),
//                         couldNeedForcedUpdate );
        switch ( request()->parseMode ) {
        case ParseForDeparturesArrivals: {
            const DepartureRequest *departureRequest =
                    dynamic_cast< const DepartureRequest* >( request() );
            if ( departureRequest ) {
                emit departuresReady( m_scriptResult->data().mid(m_published),
                        m_scriptResult->features(), m_scriptResult->hints(),
                        m_scriptNetwork->lastUrl(), globalInfo,
                        *departureRequest, couldNeedForcedUpdate );
            } else {
                emit arrivalsReady( m_scriptResult->data().mid(m_published),
                        m_scriptResult->features(), m_scriptResult->hints(),
                        m_scriptNetwork->lastUrl(), globalInfo,
                        *dynamic_cast<const ArrivalRequest*>(request()),
                        couldNeedForcedUpdate );
            }
            break;
        }
        case ParseForJourneys:
            emit journeysReady( m_scriptResult->data().mid(m_published),
                    m_scriptResult->features(), m_scriptResult->hints(),
                    m_scriptNetwork->lastUrl(), globalInfo,
                    *dynamic_cast<const JourneyRequest*>(request()),
                    couldNeedForcedUpdate );
            break;
        case ParseForStopSuggestions:
            emit stopSuggestionsReady( m_scriptResult->data().mid(m_published),
                    m_scriptResult->features(), m_scriptResult->hints(),
                    m_scriptNetwork->lastUrl(), globalInfo,
                    *dynamic_cast<const StopSuggestionRequest*>(request()),
                    couldNeedForcedUpdate );
            break;

        default:
            kDebug() << "Parse mode unsupported:" << request()->parseMode;
            break;
        }
        m_published += data.count();
    }
}

class DepartureJobPrivate {
public:
    DepartureJobPrivate( const DepartureRequest &request ) : request(request) {};
    DepartureRequest request;
};

DepartureJob::DepartureJob( QScriptProgram* script, const TimetableAccessorInfo* info,
        Storage* scriptStorage, const DepartureRequest& request, QObject* parent )
        : ScriptJob(script, info, scriptStorage, parent), d(new DepartureJobPrivate(request))
{
}

DepartureJob::~DepartureJob()
{
    delete d;
}


const AbstractRequest *DepartureJob::request() const
{
    return &d->request;
}

class JourneyJobPrivate {
public:
    JourneyJobPrivate( const JourneyRequest &request ) : request(request) {};
    JourneyRequest request;
};

JourneyJob::JourneyJob( QScriptProgram* script, const TimetableAccessorInfo* info,
        Storage* scriptStorage, const JourneyRequest& request, QObject* parent )
        : ScriptJob(script, info, scriptStorage, parent), d(new JourneyJobPrivate(request))
{
}

JourneyJob::~JourneyJob()
{
    delete d;
}

const AbstractRequest *JourneyJob::request() const
{
    return &d->request;
}

class StopSuggestionsJobPrivate {
public:
    StopSuggestionsJobPrivate( const StopSuggestionRequest &request ) : request(request) {};
    StopSuggestionRequest request;
};

StopSuggestionsJob::StopSuggestionsJob( QScriptProgram* script, const TimetableAccessorInfo* info,
        Storage* scriptStorage, const StopSuggestionRequest& request, QObject* parent )
        : ScriptJob(script, info, scriptStorage, parent), d(new StopSuggestionsJobPrivate(request))
{
}

StopSuggestionsJob::~StopSuggestionsJob()
{
    delete d;
}

const AbstractRequest *StopSuggestionsJob::request() const
{
    return &d->request;
}
