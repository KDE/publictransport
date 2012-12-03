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

    // Delete the script engine
    m_mutex->lock();
    if ( m_engine ) {
        m_engine->abortEvaluation();
        m_engine->deleteLater();
        m_engine = 0;
    }
    m_mutex->unlock();

    delete m_mutex;
}

void ScriptJob::requestAbort()
{
    QMutexLocker locker( m_mutex );
    if ( m_quit || !m_engine ) {
        // Is already aborting/finished
        return;
    }

    m_quit = true;
    if ( m_eventLoop ) {
        QEventLoop *loop = m_eventLoop;
        m_eventLoop = 0;
        loop->quit();
    }

    if ( !isFinished() && m_objects.network->hasRunningRequests() ) {
        m_objects.network->abortAllRequests();
    }
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
    if ( !loadScript(&m_data.program) ) {
        kDebug() << "Script could not be loaded correctly";
        m_mutex->unlock();
        return;
    }

    QScriptEngine *engine = m_engine;
    ScriptObjects objects = m_objects;
    m_mutex->unlock();

    // Store start time of the script
    QElapsedTimer timer;
    timer.start();

    // Add call to the appropriate function
    QString functionName;
    QScriptValueList arguments = QScriptValueList() << request()->toScriptValue( engine );
    switch ( request()->parseMode() ) {
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
        kDebug() << "Parse mode unsupported:" << request()->parseMode();
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
                           "the script", functionName) );
        return;
    }

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

    GlobalTimetableInfo globalInfo;
    globalInfo.requestDate = QDate::currentDate();
    globalInfo.delayInfoAvailable = !objects.result->isHintGiven( ResultObject::NoDelaysForStop );

    // The called function returned, but asynchronous network requests may have been started.
    // Wait for all network requests to finish, because slots in the script may get called
    if ( !waitFor(objects.network.data(), SIGNAL(allRequestsFinished()), WaitForNetwork) ) {
        return;
    }

    // Wait for script execution to finish
    ScriptAgent agent( engine );
    if ( !waitFor(&agent, SIGNAL(scriptFinished()), WaitForScriptFinish) ) {
        return;
    }

    // Update last download URL
    QMutexLocker locker( m_mutex );
    m_lastUrl = objects.network->lastUrl(); // TODO Store all URLs
    m_lastUserUrl = objects.network->lastUserUrl();

    // Inform about script run time
    DEBUG_ENGINE_JOBS( "Script finished in" << (timer.elapsed() / 1000.0)
            << "seconds: " << m_data.provider.scriptFileName() << request()->parseMode() );

    // If data for the current job has already been published, do not emit
    // xxxReady() with an empty resultset
    if ( m_published == 0 || m_objects.result->count() > m_published ) {
        const bool couldNeedForcedUpdate = m_published > 0;
        const MoreItemsRequest *moreItemsRequest =
                dynamic_cast< const MoreItemsRequest* >( request() );
        const AbstractRequest *_request =
                moreItemsRequest ? moreItemsRequest->request().data() : request();
        switch ( _request->parseMode() ) {
        case ParseForDepartures:
            emit departuresReady( m_objects.result->data().mid(m_published),
                    m_objects.result->features(), m_objects.result->hints(),
                    m_objects.network->lastUserUrl(), globalInfo,
                    *dynamic_cast<const DepartureRequest*>(_request),
                    couldNeedForcedUpdate );
            break;
        case ParseForArrivals: {
            emit arrivalsReady( m_objects.result->data().mid(m_published),
                    m_objects.result->features(), m_objects.result->hints(),
                    m_objects.network->lastUserUrl(), globalInfo,
                    *dynamic_cast< const ArrivalRequest* >(_request),
                    couldNeedForcedUpdate );
            break;
        }
        case ParseForJourneysByDepartureTime:
        case ParseForJourneysByArrivalTime:
            emit journeysReady( m_objects.result->data().mid(m_published),
                    m_objects.result->features(), m_objects.result->hints(),
                    m_objects.network->lastUserUrl(), globalInfo,
                    *dynamic_cast<const JourneyRequest*>(_request),
                    couldNeedForcedUpdate );
            break;
        case ParseForStopSuggestions:
            emit stopSuggestionsReady( m_objects.result->data().mid(m_published),
                    m_objects.result->features(), m_objects.result->hints(),
                    m_objects.network->lastUserUrl(), globalInfo,
                    *dynamic_cast<const StopSuggestionRequest*>(_request),
                    couldNeedForcedUpdate );
            break;

        case ParseForAdditionalData: {
            const QList< TimetableData > data = m_objects.result->data();
            if ( data.isEmpty() ) {
                handleError( i18nc("@info/plain", "Did not find any additional data.") );
                return;
            } else if ( data.count() > 1 ) {
                kWarning() << "The script added more than one result set, only the first gets used";
            }
            emit additionalDataReady( data.first(), m_objects.result->features(),
                    m_objects.result->hints(), m_objects.network->lastUserUrl(), globalInfo,
                    *dynamic_cast<const AdditionalDataRequest*>(_request),
                    couldNeedForcedUpdate );
            break;
        }

        default:
            kDebug() << "Parse mode unsupported:" << _request->parseMode();
            break;
        }
    }

    // Check for exceptions
    if ( m_engine->hasUncaughtException() ) {
        // TODO Get filename where the exception occured, maybe use ScriptAgent for that
        handleError( i18nc("@info/plain", "Error in script function <icode>%1</icode>, "
                            "line %2: <message>%3</message>.",
                            functionName, m_engine->uncaughtExceptionLineNumber(),
                            m_engine->uncaughtException().toString()) );
        return;
    }

    // Cleanup
    m_engine->deleteLater();
    m_engine = 0;
    m_objects.storage->checkLifetime();
    m_objects.clear();
}

void ScriptJob::handleError( const QString &errorMessage )
{
    QMutexLocker locker( m_mutex );
    kDebug() << "Error:" << errorMessage;
    kDebug() << "Backtrace:" << m_engine->uncaughtExceptionBacktrace().join("\n");
    m_errorString = errorMessage;
    m_engine->deleteLater();
    m_objects.clear();
    m_engine = 0;
    m_success = false;
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

bool ScriptJob::waitFor( QObject *sender, const char *signal, WaitForType type )
{
    // The called function returned, but asynchronous network requests may have been started.
    // Wait for all network requests to finish, because slots in the script may get called
    const int finishWaitTime = 100;
    int finishWaitCounter = 0;

    m_mutex->lockInline();
    bool success = m_success;
    bool quit = m_quit;
    if ( !success || quit ) {
        m_mutex->unlockInline();
        return true;
    }

    QScriptEngine *engine = m_engine;
    ScriptObjects objects = m_objects;
    m_mutex->unlockInline();

    while ( finishWaitCounter < 50 && sender ) {
        if ( (type == WaitForNetwork && !objects.network->hasRunningRequests()) ||
             (type == WaitForScriptFinish && !engine->isEvaluating()) ||
             (type == WaitForNothing && finishWaitCounter > 0) )
        {
            break;
        }

        QEventLoop loop;
        connect( sender, signal, &loop, SLOT(quit()) );
        QTimer::singleShot( finishWaitTime, &loop, SLOT(quit()) );

        // Store a pointer to the event loop, to be able to quit it from the destructor
        m_mutex->lockInline();
        m_eventLoop = &loop;
        m_mutex->unlockInline();

        // Engine continues execution here / waits for a signal
        loop.exec();

        QMutexLocker locker( m_mutex );
        if ( !m_eventLoop || m_quit ) {
            // Job was aborted
            m_engine = 0;
            m_objects.clear();
            engine->deleteLater();
            return false;
        }
        m_eventLoop = 0;

        ++finishWaitCounter;
    }

    if ( finishWaitCounter >= 50 && type == WaitForScriptFinish ) {
        // Script not finished
        engine->abortEvaluation();
    }
    return finishWaitCounter < 50;
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

bool ScriptJob::loadScript( QScriptProgram *script )
{
    // Create script engine
    QMutexLocker locker( m_mutex );
    m_engine = new QScriptEngine();
    foreach ( const QString &extension, m_data.provider.scriptExtensions() ) {
        if ( !importExtension(m_engine, extension) ) {
            m_errorString = i18nc("@info/plain", "Could not load script extension "
                                  "<resource>%1</resource>.", extension);
            m_success = false;
            m_engine->deleteLater();
            m_engine = 0;
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
    m_engine->evaluate( *script );
    if ( m_engine->hasUncaughtException() ) {
        kDebug() << "Error in the script" << m_engine->uncaughtExceptionLineNumber()
                 << m_engine->uncaughtException().toString();
        kDebug() << "Backtrace:" << m_engine->uncaughtExceptionBacktrace().join("\n");
        m_errorString = i18nc("@info/plain", "Error in script, line %1: <message>%2</message>.",
                              m_engine->uncaughtExceptionLineNumber(),
                              m_engine->uncaughtException().toString());
        m_engine->deleteLater();
        m_engine = 0;
        m_success = false;
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
    QMutexLocker locker( m_mutex );
    if ( hasDataToBePublished() ) {
        GlobalTimetableInfo globalInfo;
        QList< TimetableData > data = m_objects.result->data().mid( m_published );
        const bool couldNeedForcedUpdate = m_published > 0;
        const MoreItemsRequest *moreItemsRequest =
                dynamic_cast< const MoreItemsRequest* >( request() );
        const AbstractRequest *childRequest =
                moreItemsRequest ? moreItemsRequest->request().data() : request();
        switch ( request()->parseMode() ) {
        case ParseForDepartures:
            emit departuresReady( m_objects.result->data().mid(m_published),
                    m_objects.result->features(), m_objects.result->hints(),
                    m_objects.network->lastUserUrl(), globalInfo,
                    *dynamic_cast<const DepartureRequest*>(childRequest), couldNeedForcedUpdate );
            break;
        case ParseForArrivals:
            emit arrivalsReady( m_objects.result->data().mid(m_published),
                    m_objects.result->features(), m_objects.result->hints(),
                    m_objects.network->lastUserUrl(), globalInfo,
                    *dynamic_cast<const ArrivalRequest*>(childRequest), couldNeedForcedUpdate );
            break;
        case ParseForJourneysByDepartureTime:
        case ParseForJourneysByArrivalTime: {
            emit journeysReady( m_objects.result->data().mid(m_published),
                    m_objects.result->features(), m_objects.result->hints(),
                    m_objects.network->lastUserUrl(), globalInfo,
                    *dynamic_cast<const JourneyRequest*>(childRequest), couldNeedForcedUpdate );
        } break;
        case ParseForStopSuggestions:
            emit stopSuggestionsReady( m_objects.result->data().mid(m_published),
                    m_objects.result->features(), m_objects.result->hints(),
                    m_objects.network->lastUserUrl(), globalInfo,
                    *dynamic_cast<const StopSuggestionRequest*>(childRequest),
                    couldNeedForcedUpdate );
            break;
        case ParseForAdditionalData: {
            // Additional data gets requested per timetable item, only one result expected
            if ( m_objects.result->data().isEmpty() ) {
                kWarning() << "Got no additional data";
                return;
            }
            const TimetableData additionalData = m_objects.result->data().first();
            if ( additionalData.isEmpty() ) {
                kWarning() << "Did not find any additional data.";
                return;
            }
            emit additionalDataReady( data.first(), m_objects.result->features(),
                    m_objects.result->hints(), m_objects.network->lastUserUrl(), globalInfo,
                    *dynamic_cast<const AdditionalDataRequest*>(childRequest),
                    couldNeedForcedUpdate );
            break;
        }

        default:
            kDebug() << "Parse mode unsupported:" << request()->parseMode();
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
