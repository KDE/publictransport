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
#include "script/serviceproviderscript.h"
#include "serviceproviderdata.h"
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
#include <QScriptContextInfo>
#include <QFile>
#include <QTimer>
#include <QEventLoop>
#include <QFileInfo>

static int thread_count = 0;
ScriptJob::ScriptJob( QScriptProgram *script, const ServiceProviderData* data,
                      Storage *scriptStorage, QObject* parent )
    : ThreadWeaver::Job( parent ),
      m_engine(0), m_script(script),
      m_scriptStorage(scriptStorage), m_scriptNetwork(0), m_scriptResult(0),
      m_eventLoop(0), m_published(0), m_success(true), m_data(*data)
{
    kDebug() << "Thread count:" << ++thread_count;
    qRegisterMetaType<DepartureRequest>( "DepartureRequest" );
    qRegisterMetaType<ArrivalRequest>( "ArrivalRequest" );
    qRegisterMetaType<JourneyRequest>( "JourneyRequest" );
    qRegisterMetaType<StopSuggestionRequest>( "StopSuggestionRequest" );
    qRegisterMetaType<AdditionalDataRequest>( "AdditionalDataRequest" );
}

ScriptJob::~ScriptJob()
{
    kDebug() << "Thread count:" << --thread_count;
    if ( !m_engine ) {
        return;
    }

    if ( m_eventLoop ) {
        m_eventLoop->quit();
    }

    m_scriptNetwork->abortAllRequests();
    m_engine->abortEvaluation();
    m_engine->deleteLater();
    m_engine = 0;

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
    case ParseForDepartures:
    case ParseForArrivals:
        functionName = ServiceProviderScript::SCRIPT_FUNCTION_GETTIMETABLE;
        break;
    case ParseForJourneysByDepartureTime:
    case ParseForJourneysByArrivalTime:
        functionName = ServiceProviderScript::SCRIPT_FUNCTION_GETJOURNEYS;
        break;
    case ParseForStopSuggestions:
        functionName = ServiceProviderScript::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS;
        break;
    case ParseForAdditionalData:
        functionName = ServiceProviderScript::SCRIPT_FUNCTION_GETADDITIONALDATA;
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

        // Update last download URL
        // TODO Store all URLs
        m_lastUrl = m_scriptNetwork->lastUrl();

        while ( m_scriptNetwork->hasRunningRequests() || m_engine->isEvaluating() ) {
            // Wait for running requests to finish
            m_eventLoop = new QEventLoop();
            ScriptAgent agent( m_engine );
            QTimer::singleShot( 30000, m_eventLoop, SLOT(quit()) );
            connect( this, SIGNAL(destroyed(QObject*)), m_eventLoop, SLOT(quit()) );
            connect( &agent, SIGNAL(scriptFinished()), m_eventLoop, SLOT(quit()) );
            connect( m_scriptNetwork.data(), SIGNAL(allRequestsFinished()),
                     m_eventLoop, SLOT(quit()) );

            kDebug() << "Waiting for script to finish..." << m_scriptNetwork->hasRunningRequests()
                     << m_engine->isEvaluating() << thread();
            m_eventLoop->exec( QEventLoop::ExcludeUserInputEvents );
            delete m_eventLoop;
            m_eventLoop = 0;
        }

        // Inform about script run time
        kDebug() << " > Script finished after" << (time.elapsed() / 1000.0)
                    << "seconds: " << m_data.scriptFileName() << thread() << request()->parseMode;

        // If data for the current job has already been published, do not emit
        // completed with an empty resultset
        if ( m_published == 0 || m_scriptResult->count() > m_published ) {
            const bool couldNeedForcedUpdate = m_published > 0;
            switch ( request()->parseMode ) {
            case ParseForDepartures:
                emit departuresReady( m_scriptResult->data().mid(m_published),
                        m_scriptResult->features(), m_scriptResult->hints(),
                        m_scriptNetwork->lastUrl(), globalInfo,
                        *dynamic_cast<const DepartureRequest*>(request()),
                        couldNeedForcedUpdate );
                break;
            case ParseForArrivals: {
                emit arrivalsReady( m_scriptResult->data().mid(m_published),
                        m_scriptResult->features(), m_scriptResult->hints(),
                        m_scriptNetwork->lastUrl(), globalInfo,
                        *dynamic_cast< const ArrivalRequest* >(request()),
                        couldNeedForcedUpdate );
                break;
            }
            case ParseForJourneysByDepartureTime:
            case ParseForJourneysByArrivalTime:
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

            case ParseForAdditionalData: {
                const QList< TimetableData > data = m_scriptResult->data();
                if ( data.isEmpty() ) {
                    kDebug() << "The script didn't find any new data" << request()->sourceName;
                    m_errorString = i18nc("@info/plain",
                                          "Error while parsing the additional data document.");
                    m_success = false;
                    return;
                } else if ( data.count() > 1 ) {
                    kWarning() << "The script added more than one result set, only the first gets used";
                }
                emit additionalDataReady( data.first(), m_scriptResult->features(),
                        m_scriptResult->hints(), m_scriptNetwork->lastUrl(), globalInfo,
                        *dynamic_cast<const AdditionalDataRequest*>(request()),
                        couldNeedForcedUpdate );
                break;
            }

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
    if ( !ServiceProviderScript::allowedExtensions().contains(extension, Qt::CaseInsensitive) ) {
        if ( engine->availableExtensions().contains(extension, Qt::CaseInsensitive) ) {
            kDebug() << "Extension" << extension << "is not allowed currently";
        } else {
            kDebug() << "Extension" << extension << "could not be found";
            kDebug() << "Available extensions:" << engine->availableExtensions();
        }
        kDebug() << "Allowed extensions:" << ServiceProviderScript::allowedExtensions();
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

QScriptValue include( QScriptContext *context, QScriptEngine *engine )
{
    // Check argument count, include() call in global context?
    if ( context->argumentCount() < 1 ) {
        context->throwError( i18nc("@info/plain", "One argument expected for <icode>include()</icode>") );
        return engine->undefinedValue();
    } else if ( context->parentContext() && context->parentContext()->parentContext() ) {
        context->throwError( i18nc("@info/plain", "<icode>include()</icode> calls must be in global context") );
        return engine->undefinedValue();
    }

    // Check if this include() call is before all other statements
    const QScriptContextInfo contextInfo( context->parentContext() );
    const quint16 maxIncludeLine = context->callee().data().toUInt16();
    if ( contextInfo.lineNumber() > maxIncludeLine ) {
        context->throwError( i18nc("@info/plain", "<icode>include()</icode> calls must be the first statements") );
        return engine->undefinedValue();
    }

    // Get argument and check that it's not pointing to another directory
    const QString fileName = context->argument(0).toString();
    if ( fileName.contains('/') ) {
        context->throwError( i18nc("@info/plain", "Cannot include files from other directories") );
        return engine->undefinedValue();
    }

    // Get path of the main script
    QString path;
    QScriptContext *fileInfoContext = context;
    do {
        path = QFileInfo( QScriptContextInfo(fileInfoContext).fileName() ).path();
        fileInfoContext = fileInfoContext->parentContext();
    } while ( path.isEmpty() || path == QLatin1String(".") );

    // Construct file path to the file to be included and check if the file is already included
    const QString filePath = path + '/' + fileName;
    QStringList includedFiles =
            engine->globalObject().property( "includedFiles" ).toVariant().toStringList();
    if ( includedFiles.contains(filePath) ) {
        kWarning() << "File already included" << filePath;
        return engine->undefinedValue();
    }

    // Try to open the file to be included
    QFile scriptFile( filePath );
    if ( !scriptFile.open(QIODevice::ReadOnly) ) {
        context->throwError( i18nc("@info/plain", "Cannot find file to be included: "
                                   "<filename>%1</filename>", filePath) );
        return engine->undefinedValue();
    }

    // Read the file
    kDebug() << "include(" << filePath << ")";
    QTextStream stream( &scriptFile );
    const QString program = stream.readAll();
    scriptFile.close();

    // Set script context
    QScriptContext *parent = context->parentContext();
    if ( parent ) {
        context->setActivationObject( parent->activationObject() );
        context->setThisObject( parent->thisObject() );
    }

    // Store included files in global property "includedFiles"
    includedFiles << filePath;
    includedFiles.removeDuplicates();
    engine->globalObject().setProperty( "includedFiles", engine->newVariant(includedFiles) );

    // Evaluate script
    return engine->evaluate( program, filePath );
}

quint16 maxIncludeLine( const QString &program )
{
    // Get first lines of script code until the first statement which is not an include() call
    // The regular expression matches in blocks: Multiline comments (needs non-greedy regexp),
    // one line comments, whitespaces & newlines, include("...") calls and semi-colons (needed,
    // because the regexp is non-greedy)
    QRegExp programRegExp( "\\s*(?:\\s*//[^\\n]*\\n|/\\*.*\\*/|[\\s\\n]+|include\\s*\\(\\s*\"[^\"]*\"\\s*\\)|;)+" );
    programRegExp.setMinimal( true );
    int pos = 0, nextPos = 0;
    QString programBegin;
    while ( (pos = programRegExp.indexIn(program, pos)) == nextPos ) {
        const QString capture = programRegExp.cap();
        programBegin.append( capture );
        pos += programRegExp.matchedLength();
        nextPos = pos;
    }
    return programBegin.count( '\n' );
}

bool ScriptJob::loadScript( QScriptProgram *script )
{
    // Create script engine
    kDebug() << "Create QScriptEngine";
    m_engine = new QScriptEngine( /*thread()*/ );
//     m_scriptStorage->mutex.lock();
//     kDebug() << "LOAD qt.core" << thread() << m_parseMode;
//     m_engine->importExtension("qt.core");
    foreach ( const QString &extension, m_data.scriptExtensions() ) {
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

    m_engine->globalObject().setProperty( "provider", m_engine->newQObject(&m_data) ); //parent()) );

    QScriptValue includeFunction = m_engine->globalObject().property("include");
    if ( !includeFunction.isValid() ) {
        includeFunction = m_engine->newFunction( include, 1 );
    }
    includeFunction.setData( maxIncludeLine(script->sourceCode()) );
    m_engine->globalObject().setProperty( "include", includeFunction );

    // Process events every 100ms
//     m_engine->setProcessEventsInterval( 100 );

    // Register NetworkRequest class for use in the script
    qScriptRegisterMetaType< NetworkRequestPtr >( m_engine,
            networkRequestToScript, networkRequestFromScript );

    // Create objects for the script
    Helper *scriptHelper = new Helper( m_data.id(), m_engine );
    m_scriptNetwork = QSharedPointer<Network>( new Network(m_data.fallbackCharset(), m_engine) );
    m_scriptResult = QSharedPointer<ResultObject>( new ResultObject(m_engine) );
    connect( m_scriptResult.data(), SIGNAL(publish()), this, SLOT(publish()) );

    // Make the objects available to the script
    m_engine->globalObject().setProperty( "helper", m_engine->newQObject(scriptHelper) );
    m_engine->globalObject().setProperty( "network", m_engine->newQObject(m_scriptNetwork.data()) );
    m_engine->globalObject().setProperty( "storage", m_engine->newQObject(m_scriptStorage) );
    m_engine->globalObject().setProperty( "result", m_engine->newQObject(m_scriptResult.data()) );
    m_engine->globalObject().setProperty( "enum",
            m_engine->newQMetaObject(&ResultObject::staticMetaObject) );
    m_engine->globalObject().setProperty( "PublicTransport",
            m_engine->newQMetaObject(&Enums::staticMetaObject) );

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
        case ParseForDepartures:
            emit departuresReady( m_scriptResult->data().mid(m_published),
                    m_scriptResult->features(), m_scriptResult->hints(),
                    m_scriptNetwork->lastUrl(), globalInfo,
                    *dynamic_cast<const DepartureRequest*>(request()), couldNeedForcedUpdate );
            break;
        case ParseForArrivals:
            emit arrivalsReady( m_scriptResult->data().mid(m_published),
                    m_scriptResult->features(), m_scriptResult->hints(),
                    m_scriptNetwork->lastUrl(), globalInfo,
                    *dynamic_cast<const ArrivalRequest*>(request()), couldNeedForcedUpdate );
            break;
        case ParseForJourneysByDepartureTime:
        case ParseForJourneysByArrivalTime:
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

DepartureJob::DepartureJob( QScriptProgram* script, const ServiceProviderData* info,
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

class ArrivalJobPrivate {
public:
    ArrivalJobPrivate( const ArrivalRequest &request ) : request(request) {};
    ArrivalRequest request;
};

ArrivalJob::ArrivalJob( QScriptProgram* script, const ServiceProviderData* info,
        Storage* scriptStorage, const ArrivalRequest& request, QObject* parent )
        : ScriptJob(script, info, scriptStorage, parent), d(new ArrivalJobPrivate(request))
{
}

ArrivalJob::~ArrivalJob()
{
    delete d;
}


const AbstractRequest *ArrivalJob::request() const
{
    return &d->request;
}

class JourneyJobPrivate {
public:
    JourneyJobPrivate( const JourneyRequest &request ) : request(request) {};
    JourneyRequest request;
};

JourneyJob::JourneyJob( QScriptProgram* script, const ServiceProviderData* info,
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

StopSuggestionsJob::StopSuggestionsJob( QScriptProgram* script, const ServiceProviderData* info,
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

class AdditionalDataJobPrivate {
public:
    AdditionalDataJobPrivate( const AdditionalDataRequest &request ) : request(request) {};
    AdditionalDataRequest request;
};

AdditionalDataJob::AdditionalDataJob( QScriptProgram *script, const ServiceProviderData *info,
        Storage *scriptStorage, const AdditionalDataRequest &request, QObject *parent )
        : ScriptJob(script, info, scriptStorage, parent), d(new AdditionalDataJobPrivate(request))
{
}

AdditionalDataJob::~AdditionalDataJob()
{
    delete d;
}

const AbstractRequest *AdditionalDataJob::request() const
{
    return &d->request;
}
