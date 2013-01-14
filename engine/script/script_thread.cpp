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
#include "config.h"
#include "scriptapi.h"
#include "script/serviceproviderscript.h"
#include "serviceproviderdata.h"
#include "request.h"
#include "serviceproviderglobal.h"

// KDE includes
#include <ThreadWeaver/Thread>
#include <ThreadWeaver/Weaver>
#include <KLocalizedString>
#include <KDebug>
#include <KGlobal>
#include <KStandardDirs>
#include <KLocale>

// Qt includes
#include <QScriptEngine>
#include <QScriptEngineAgent>
#include <QScriptValueIterator>
#include <QScriptContextInfo>
#include <QFile>
#include <QTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QApplication>

ScriptJob::ScriptJob( const ScriptData &data, const QSharedPointer< Storage > &scriptStorage,
                      QObject* parent )
    : ThreadWeaver::Job(parent), m_engine(0), m_mutex(new QMutex(QMutex::Recursive)),
      m_data(data), m_eventLoop(0), m_published(0), m_quit(false), m_success(true)
{
    Q_ASSERT_X( data.isValid(), "ScriptJob constructor", "Needs valid script data" );

    // Use global storage, may contain non-persistent data from earlier requests.
    // Does not need to live in this thread, it does not create any new QObjects.
    QMutexLocker locker( m_mutex );
    m_objects.storage = scriptStorage;

    qRegisterMetaType<DepartureRequest>( "DepartureRequest" );
    qRegisterMetaType<ArrivalRequest>( "ArrivalRequest" );
    qRegisterMetaType<JourneyRequest>( "JourneyRequest" );
    qRegisterMetaType<StopSuggestionRequest>( "StopSuggestionRequest" );
    qRegisterMetaType<StopsByGeoPositionRequest>( "StopsByGeoPositionRequest" );
    qRegisterMetaType<AdditionalDataRequest>( "AdditionalDataRequest" );
}

ScriptJob::~ScriptJob()
{
    // Abort, if still running
    requestAbort();

    // Do some cleanup if not done already
    cleanup();

    delete m_mutex;
}

void ScriptJob::requestAbort()
{
    m_mutex->lock();
    if ( !m_engine ) {
        // Is already finished
        m_mutex->unlock();
        return;
    } else if ( m_quit ) {
        // Is already aborting, wait for the script engine to get destroyed
        QEventLoop loop;
        connect( m_engine, SIGNAL(destroyed(QObject*)), &loop, SLOT(quit()) );
        m_mutex->unlock();

        // Run the event loop, waiting for the engine to get destroyed or a 1 second timeout
        QTimer::singleShot( 1000, &loop, SLOT(quit()) );
        loop.exec();
        return;
    }

    // Is still running, remember to abort
    m_quit = true;

    // Abort running network requests, if any
    if ( m_objects.network->hasRunningRequests() ) {
        m_objects.network->abortAllRequests();
    }

    // Abort script evaluation
    if ( m_engine ) {
        m_engine->abortEvaluation();
    }

    // Wake waiting event loops
    if ( m_eventLoop ) {
        QEventLoop *loop = m_eventLoop;
        m_eventLoop = 0;
        loop->quit();
    }
    m_mutex->unlock();

    // Wait until signals are processed
    qApp->processEvents();
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
    m_mutex->lock();
    if ( !loadScript(m_data.program) ) {
        kDebug() << "Script could not be loaded correctly";
        m_mutex->unlock();
        return;
    }

    QScriptEngine *engine = m_engine;
    ScriptObjects objects = m_objects;
    const QString scriptFileName = m_data.provider.scriptFileName();
    const QScriptValueList arguments = QScriptValueList() << request()->toScriptValue( engine );
    const ParseDocumentMode parseMode = request()->parseMode();
    m_mutex->unlock();

    // Add call to the appropriate function
    QString functionName;
    switch ( parseMode ) {
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
        kDebug() << "Parse mode unsupported:" << parseMode;
        break;
    }

    if ( functionName.isEmpty() ) {
        // This should never happen, therefore no i18n
        handleError( "Unknown parse mode" );
        return;
    }

    // Check if the script function is implemented
    QScriptValue function = engine->globalObject().property( functionName );
    if ( !function.isFunction() ) {
        handleError( i18nc("@info/plain", "Function <icode>%1</icode> not implemented by "
                           "the script <filename>%1</filename>", functionName, scriptFileName) );
        return;
    }

    // Store start time of the script
    QElapsedTimer timer;
    timer.start();

    // Call script function
    QScriptValue result = function.call( QScriptValue(), arguments );
    if ( engine->hasUncaughtException() ) {
        // TODO Get filename where the exception occured, maybe use ScriptAgent for that
        handleError( i18nc("@info/plain", "Error in script function <icode>%1</icode>, "
                           "line %2: <message>%3</message>.",
                           functionName, engine->uncaughtExceptionLineNumber(),
                           engine->uncaughtException().toString()) );
        return;
    }

    // Inform about script run time
    DEBUG_ENGINE_JOBS( "Script finished in" << (timer.elapsed() / 1000.0)
                       << "seconds: " << scriptFileName << parseMode );

    GlobalTimetableInfo globalInfo;
    globalInfo.requestDate = QDate::currentDate();
    globalInfo.delayInfoAvailable = !objects.result->isHintGiven( ResultObject::NoDelaysForStop );

    // The called function returned, but asynchronous network requests may have been started.
    // Slots in the script may be connected to those requests and start script execution again.
    // In the execution new network requests may get started and so on.
    //
    // Wait until all network requests are finished and the script is not evaluating at the same
    // time. Define a global timeout, each waitFor() call subtracts the elapsed time.
    //
    // NOTE Network requests by default use a 30 seconds timeout, the global timeout should
    //   be bigger. Synchronous requests that were started by signals of running asynchronous
    //   requests are accounted as script execution time here. Synchronous requests that were
    //   started before are already done.
    const int startTimeout = 60000;
    int timeout = startTimeout;
    while ( objects.network->hasRunningRequests() || engine->isEvaluating() ) {
        // Wait until all asynchronous network requests are finished
        if ( !waitFor(objects.network.data(), SIGNAL(allRequestsFinished()),
                      WaitForNetwork, &timeout) )
        {
            if ( timeout <= 0 ) {
                // Timeout expired while waiting for network requests to finish,
                // abort all requests that are still running
                objects.network->abortAllRequests();

                QMutexLocker locker( m_mutex );
                m_success = false;
                m_errorString = i18nc("@info", "Timeout expired after %1 while waiting for "
                                      "network requests to finish",
                                      KGlobal::locale()->prettyFormatDuration(startTimeout));
            }

            cleanup();
            return;
        }

        // Wait for script execution to finish, ie. slots connected to the last finished request(s)
        ScriptAgent agent( engine );
        if ( !waitFor(&agent, SIGNAL(scriptFinished()), WaitForScriptFinish, &timeout) ) {
            if ( timeout <= 0 ) {
                // Timeout expired while waiting for script execution to finish, abort evaluation
                engine->abortEvaluation();

                QMutexLocker locker( m_mutex );
                m_success = false;
                m_errorString = i18nc("@info", "Timeout expired after %1 while waiting for script "
                                      "execution to finish",
                                      KGlobal::locale()->prettyFormatDuration(startTimeout));
            }

            cleanup();
            return;
        }

        // Check for new exceptions after waiting for script execution to finish (again)
        if ( engine->hasUncaughtException() ) {
            // TODO Get filename where the exception occured, maybe use ScriptAgent for that
            handleError( i18nc("@info/plain", "Error in script function <icode>%1</icode>, "
                                "line %2: <message>%3</message>.",
                                functionName, engine->uncaughtExceptionLineNumber(),
                                engine->uncaughtException().toString()) );
            return;
        }
    }

    // Publish remaining items, if any
    publish();

    // Cleanup
    cleanup();
}

void ScriptJob::cleanup()
{
    QMutexLocker locker( m_mutex );
    if ( !m_objects.storage.isNull() ) {
        m_objects.storage->checkLifetime();
    }
    if ( !m_objects.result.isNull() ) {
        disconnect( m_objects.result.data(), 0, this, 0 );
    }
    if ( m_engine ) {
        m_engine->deleteLater();
        m_engine = 0;
    }
}

void ScriptJob::handleError( const QString &errorMessage )
{
    QMutexLocker locker( m_mutex );
    kDebug() << "Error:" << errorMessage;
    kDebug() << "Backtrace:" << m_engine->uncaughtExceptionBacktrace().join("\n");
    m_errorString = errorMessage;
    m_success = false;
    cleanup();
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

bool ScriptJob::waitFor( QObject *sender, const char *signal, WaitForType type, int *timeout )
{
    if ( *timeout <= 0 ) {
        return false;
    }

    // Do not wait if the job was aborted or failed
    QMutexLocker locker( m_mutex );
    if ( !m_success || m_quit ) {
        return false;
    }

    QScriptEngine *engine = m_engine;
    ScriptObjects objects = m_objects;

    // Test if the target condition is already met
    if ( (type == WaitForNetwork && objects.network->hasRunningRequests()) ||
         (type == WaitForScriptFinish && engine->isEvaluating()) )
    {
        // Not finished yet, wait for the given signal,
        // that should get emitted when the target condition is met
        QEventLoop loop;
        connect( sender, signal, &loop, SLOT(quit()) );

        // Add a timeout to not wait forever (eg. because of an infinite loop in the script).
        // Use a QTimer variable to be able to check if the timeout caused the event loop to quit
        // rather than the given signal
        QTimer timer;
        timer.setSingleShot( true );
        connect( &timer, SIGNAL(timeout()), &loop, SLOT(quit()) );

        // Store a pointer to the event loop, to be able to quit it on abort.
        // Then start the timeout.
        m_eventLoop = &loop;
        QElapsedTimer elapsedTimer;
        elapsedTimer.start();
        timer.start( *timeout );
        m_mutex->unlock();

        // Start the event loop waiting for the given signal / timeout.
        // QScriptEngine continues execution here or network requests continue to get handled and
        // may call script slots on finish.
        loop.exec();

        // Test if the timeout has expired, ie. if the timer is no longer running (single shot)
        m_mutex->lock();
        const bool timeoutExpired = !timer.isActive();

        // Update remaining time of the timeout
        if ( timeoutExpired ) {
            *timeout = 0;
        } else {
            *timeout -= elapsedTimer.elapsed();
        }

        // Test if the job was aborted while waiting
        if ( !m_eventLoop || m_quit ) {
            // Job was aborted
            m_eventLoop = 0;
            return false;
        }
        m_eventLoop = 0;

        // Generate errors if the timeout has expired and abort network requests / script execution
        if ( timeoutExpired ) {
            return false;
        }
    }

    // The target condition was met in time or was already met
    return true;
}

QScriptValue include( QScriptContext *context, QScriptEngine *engine )
{
    // Check argument count, include() call in global context?
    const QScriptContextInfo contextInfo( context->parentContext() );
    if ( context->argumentCount() < 1 ) {
        context->throwError( i18nc("@info/plain", "One argument expected for <icode>include()</icode>") );
        return engine->undefinedValue();
    } else if ( context->parentContext() && context->parentContext()->parentContext() ) {
        QScriptContext *parentContext = context->parentContext()->parentContext();
        bool error = false;
        while ( parentContext ) {
            const QScriptContextInfo parentContextInfo( parentContext );
            if ( !parentContextInfo.fileName().isEmpty() &&
                 parentContextInfo.fileName() == contextInfo.fileName() )
            {
                // Parent context is in the same file, error
                error = true;
                break;
            }
            parentContext = parentContext->parentContext();
        }

        if ( error ) {
            context->throwError( i18nc("@info/plain", "<icode>include()</icode> calls must be in global context") );
            return engine->undefinedValue();
        }
    }

    // Check if this include() call is before all other statements
    QVariantHash includeData = context->callee().data().toVariant().toHash();
    if ( includeData.contains(contextInfo.fileName()) ) {
        const quint16 maxIncludeLine = includeData[ contextInfo.fileName() ].toInt();
        if ( contextInfo.lineNumber() > maxIncludeLine ) {
            context->throwError( i18nc("@info/plain", "<icode>include()</icode> calls must be the first statements") );
            return engine->undefinedValue();
        }
    }

    // Get argument and check that it's not pointing to another directory
    const QString fileName = context->argument(0).toString();
    if ( fileName.contains('/') ) {
        context->throwError( i18nc("@info/plain", "Cannot include files from other directories") );
        return engine->undefinedValue();
    }

    // Find the script to be included
    const QString subDirectory = ServiceProviderGlobal::installationSubDirectory();
    const QString filePath = KGlobal::dirs()->findResource( "data", subDirectory + fileName );

    // Check if the script was already included
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
                                   "<filename>%1</filename>", fileName) );
        return engine->undefinedValue();
    }

    // Read the file
    QTextStream stream( &scriptFile );
    const QString program = stream.readAll();
    scriptFile.close();

    if ( !includeData.contains(scriptFile.fileName()) ) {
        includeData[ scriptFile.fileName() ] = maxIncludeLine( program );

        QScriptValue includeFunction = engine->globalObject().property("include");
        Q_ASSERT( includeFunction.isValid() );
        includeFunction.setData( qScriptValueFromValue(engine, includeData) );
        engine->globalObject().setProperty( "include", includeFunction,
                                            QScriptValue::KeepExistingFlags );
    }

    // Set script context
    QScriptContext *parent = context->parentContext();
    if ( parent ) {
        context->setActivationObject( parent->activationObject() );
        context->setThisObject( parent->thisObject() );
    }

    // Store included files in global property "includedFiles"
    includedFiles << filePath;
    includedFiles.removeDuplicates();
    QScriptValue::PropertyFlags flags = QScriptValue::ReadOnly | QScriptValue::Undeletable;
    engine->globalObject().setProperty( "includedFiles", engine->newVariant(includedFiles), flags );

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

bool ScriptJob::loadScript( const QSharedPointer< QScriptProgram > &script )
{
    if ( !script ) {
        kDebug() << "Invalid script data";
        return false;
    }

    // Create script engine
    QMutexLocker locker( m_mutex );
    m_engine = new QScriptEngine();
    foreach ( const QString &extension, m_data.provider.scriptExtensions() ) {
        if ( !importExtension(m_engine, extension) ) {
            m_errorString = i18nc("@info/plain", "Could not load script extension "
                                  "<resource>%1</resource>.", extension);
            m_success = false;
            cleanup();
            return false;
        }
    }

    // Create and attach script objects.
    // The Storage object is already created and will not be replaced by a new instance.
    // It lives in the GUI thread and gets used in all thread jobs to not erase non-persistently
    // stored data after each request.
    m_objects.createObjects( m_data );
    m_objects.attachToEngine( m_engine, m_data );

    // Connect the publish() signal directly (the result object lives in the thread that gets
    // run by this job, this job itself lives in the GUI thread). Connect directly to ensure
    // the script objects and the request are still valid (prevent crashes).
    connect( m_objects.result.data(), SIGNAL(publish()), this, SLOT(publish()),
             Qt::DirectConnection );

    // Load the script program
    m_mutex->unlock();
    m_engine->evaluate( *script );
    m_mutex->lock();

    if ( m_engine->hasUncaughtException() ) {
        kDebug() << "Error in the script" << m_engine->uncaughtExceptionLineNumber()
                 << m_engine->uncaughtException().toString();
        kDebug() << "Backtrace:" << m_engine->uncaughtExceptionBacktrace().join("\n");
        m_errorString = i18nc("@info/plain", "Error in script, line %1: <message>%2</message>.",
                              m_engine->uncaughtExceptionLineNumber(),
                              m_engine->uncaughtException().toString());
        m_success = false;
        cleanup();
        return false;
    } else {
        return true;
    }
}

bool ScriptJob::hasDataToBePublished() const
{
    QMutexLocker locker( m_mutex );
    return !m_objects.isValid() ? false : m_objects.result->count() > m_published;
}

void ScriptJob::publish()
{
    // This slot gets run in the thread of this job
    // Only publish, if there is data which is not already published
    if ( hasDataToBePublished() ) {
        m_mutex->lock();
        GlobalTimetableInfo globalInfo;
        const QList< TimetableData > data = m_objects.result->data().mid( m_published );
        const ResultObject::Features features = m_objects.result->features();
        const ResultObject::Hints hints = m_objects.result->hints();
        const QString lastUserUrl = m_objects.network->lastUserUrl();
        const bool couldNeedForcedUpdate = m_published > 0;
        const MoreItemsRequest *moreItemsRequest = dynamic_cast<const MoreItemsRequest*>(request());
        const AbstractRequest *_request =
                moreItemsRequest ? moreItemsRequest->request().data() : request();
        const ParseDocumentMode parseMode = request()->parseMode();

        m_lastUrl = m_objects.network->lastUrl(); // TODO Store all URLs
        m_lastUserUrl = m_objects.network->lastUserUrl();
        m_published += data.count();

        // Unlock mutex after copying the request object, then emit it with an xxxReady() signal
        switch ( parseMode ) {
        case ParseForDepartures: {
            Q_ASSERT(dynamic_cast<const DepartureRequest*>(_request));
            const DepartureRequest departureRequest =
                    *dynamic_cast<const DepartureRequest*>(_request);
            m_mutex->unlock();
            emit departuresReady( data, features, hints, lastUserUrl, globalInfo,
                                  departureRequest, couldNeedForcedUpdate );
            break;
        }
        case ParseForArrivals: {
            Q_ASSERT(dynamic_cast<const ArrivalRequest*>(_request));
            const ArrivalRequest arrivalRequest = *dynamic_cast<const ArrivalRequest*>(_request);
            m_mutex->unlock();
            emit arrivalsReady( data, features, hints, lastUserUrl, globalInfo,
                                arrivalRequest, couldNeedForcedUpdate );
            break;
        }
        case ParseForJourneysByDepartureTime:
        case ParseForJourneysByArrivalTime: {
            Q_ASSERT(dynamic_cast<const JourneyRequest*>(_request));
            const JourneyRequest journeyRequest = *dynamic_cast<const JourneyRequest*>(_request);
            m_mutex->unlock();
            emit journeysReady( data, features, hints, lastUserUrl, globalInfo,
                                journeyRequest, couldNeedForcedUpdate );
            break;
        }
        case ParseForStopSuggestions: {
            Q_ASSERT(dynamic_cast<const StopSuggestionRequest*>(_request));
            const StopSuggestionRequest stopSuggestionRequest =
                    *dynamic_cast< const StopSuggestionRequest* >( _request );
            m_mutex->unlock();
            emit stopSuggestionsReady( data, features, hints, lastUserUrl, globalInfo,
                                       stopSuggestionRequest, couldNeedForcedUpdate );
            break;
        }
        case ParseForAdditionalData: {
            if ( data.count() > 1 ) {
                kWarning() << "The script added more than one result in an additional data request";
                kDebug() << "All received additional data for item"
                         << dynamic_cast<const AdditionalDataRequest*>(_request)->itemNumber()
                         << ':' << data;
                m_errorString = i18nc("@info/plain", "The script added more than one result in "
                                      "an additional data request.");
                m_success = false;
                cleanup();
                m_mutex->unlock();
                return;
            }

            // Additional data gets requested per timetable item, only one result expected
            const TimetableData additionalData = data.first();
            if ( additionalData.isEmpty() ) {
                kWarning() << "Did not find any additional data.";
                m_errorString = i18nc("@info/plain", "TODO."); // TODO
                m_success = false;
                cleanup();
                m_mutex->unlock();
                return;
            }

            Q_ASSERT(dynamic_cast<const AdditionalDataRequest*>(_request));
            const AdditionalDataRequest additionalDataRequest =
                    *dynamic_cast< const AdditionalDataRequest* >( _request );
            m_mutex->unlock();

            emit additionalDataReady( additionalData, features, hints, lastUserUrl, globalInfo,
                                      additionalDataRequest, couldNeedForcedUpdate );
            break;
        }

        default:
            m_mutex->unlock();
            kDebug() << "Parse mode unsupported:" << parseMode;
            break;
        }
    }
}

class DepartureJobPrivate {
public:
    DepartureJobPrivate( const DepartureRequest &request ) : request(request) {};
    DepartureRequest request;
};

DepartureJob::DepartureJob( const ScriptData &data, const QSharedPointer< Storage > &scriptStorage,
                            const DepartureRequest& request, QObject* parent )
        : ScriptJob(data, scriptStorage, parent), d(new DepartureJobPrivate(request))
{
}

DepartureJob::~DepartureJob()
{
    delete d;
}


const AbstractRequest *DepartureJob::request() const
{
    QMutexLocker locker( m_mutex );
    return &d->request;
}

QString ScriptJob::sourceName() const
{
    QMutexLocker locker( m_mutex );
    return request()->sourceName();
}

const AbstractRequest *ScriptJob::cloneRequest() const
{
    QMutexLocker locker( m_mutex );
    return request()->clone();
}

class ArrivalJobPrivate {
public:
    ArrivalJobPrivate( const ArrivalRequest &request ) : request(request) {};
    ArrivalRequest request;
};

ArrivalJob::ArrivalJob( const ScriptData &data, const QSharedPointer< Storage > &scriptStorage,
                        const ArrivalRequest& request, QObject* parent )
        : ScriptJob(data, scriptStorage, parent), d(new ArrivalJobPrivate(request))
{
}

ArrivalJob::~ArrivalJob()
{
    delete d;
}


const AbstractRequest *ArrivalJob::request() const
{
    QMutexLocker locker( m_mutex );
    return &d->request;
}

class JourneyJobPrivate {
public:
    JourneyJobPrivate( const JourneyRequest &request ) : request(request) {};
    JourneyRequest request;
};

JourneyJob::JourneyJob( const ScriptData &data, const QSharedPointer< Storage > &scriptStorage,
                        const JourneyRequest& request, QObject* parent )
        : ScriptJob(data, scriptStorage, parent), d(new JourneyJobPrivate(request))
{
}

JourneyJob::~JourneyJob()
{
    delete d;
}

const AbstractRequest *JourneyJob::request() const
{
    QMutexLocker locker( m_mutex );
    return &d->request;
}

class StopSuggestionsJobPrivate {
public:
    StopSuggestionsJobPrivate( const StopSuggestionRequest &request ) : request(request) {};
    StopSuggestionRequest request;
};

StopSuggestionsJob::StopSuggestionsJob( const ScriptData &data,
                                        const QSharedPointer< Storage > &scriptStorage,
                                        const StopSuggestionRequest& request, QObject* parent )
        : ScriptJob(data, scriptStorage, parent), d(new StopSuggestionsJobPrivate(request))
{
}

StopSuggestionsJob::~StopSuggestionsJob()
{
    delete d;
}

const AbstractRequest *StopSuggestionsJob::request() const
{
    QMutexLocker locker( m_mutex );
    return &d->request;
}

class StopsByGeoPositionJobPrivate {
public:
    StopsByGeoPositionJobPrivate( const StopsByGeoPositionRequest &request )
            : request(request) {};
    StopsByGeoPositionRequest request;
};

StopsByGeoPositionJob::StopsByGeoPositionJob(
        const ScriptData &data, const QSharedPointer< Storage > &scriptStorage,
        const StopsByGeoPositionRequest& request, QObject* parent )
        : ScriptJob(data, scriptStorage, parent), d(new StopsByGeoPositionJobPrivate(request))
{
}

StopsByGeoPositionJob::~StopsByGeoPositionJob()
{
    delete d;
}

const AbstractRequest *StopsByGeoPositionJob::request() const
{
    QMutexLocker locker( m_mutex );
    return &d->request;
}

class AdditionalDataJobPrivate {
public:
    AdditionalDataJobPrivate( const AdditionalDataRequest &request ) : request(request) {};
    AdditionalDataRequest request;
};

AdditionalDataJob::AdditionalDataJob( const ScriptData &data,
                                      const QSharedPointer< Storage > &scriptStorage,
                                      const AdditionalDataRequest &request, QObject *parent )
        : ScriptJob(data, scriptStorage, parent), d(new AdditionalDataJobPrivate(request))
{
}

AdditionalDataJob::~AdditionalDataJob()
{
    delete d;
}

const AbstractRequest *AdditionalDataJob::request() const
{
    QMutexLocker locker( m_mutex );
    return &d->request;
}

class MoreItemsJobPrivate {
public:
    MoreItemsJobPrivate( const MoreItemsRequest &request ) : request(request) {};
    MoreItemsRequest request;
};

MoreItemsJob::MoreItemsJob( const ScriptData &data, const QSharedPointer< Storage > &scriptStorage,
                            const MoreItemsRequest &request, QObject *parent )
        : ScriptJob(data, scriptStorage, parent), d(new MoreItemsJobPrivate(request))
{
}

MoreItemsJob::~MoreItemsJob()
{
    delete d;
}

const AbstractRequest *MoreItemsJob::request() const
{
    QMutexLocker locker( m_mutex );
    return &d->request;
}

bool ScriptJob::success() const
{
    QMutexLocker locker( m_mutex );
    return m_success;
}

int ScriptJob::publishedItems() const
{
    QMutexLocker locker( m_mutex );
    return m_published;
}
QString ScriptJob::errorString() const
{
    QMutexLocker locker( m_mutex );
    return m_errorString;
}

QString ScriptJob::lastDownloadUrl() const
{
    QMutexLocker locker( m_mutex );
    return m_lastUrl;
}

QString ScriptJob::lastUserUrl() const
{
    QMutexLocker locker( m_mutex );
    return m_lastUserUrl;
}
