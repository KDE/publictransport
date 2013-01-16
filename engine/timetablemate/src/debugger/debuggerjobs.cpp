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
#include "debuggerjobs.h"

// Own includes
#include "debugger.h"
#include "debuggeragent.h"
#include "breakpointmodel.h"
#include "variablemodel.h"
#include "backtracemodel.h"

// PublicTransport engine includes
#include <engine/script/serviceproviderscript.h> // For ServiceProviderScript::SCRIPT_FUNCTION_...

// KDE includes
#include <KLocalizedString>
#include <KDebug>
#include <ThreadWeaver/DependencyPolicy>
#include <ThreadWeaver/Thread>

// Qt includes
#include <QMutex>
#include <QSemaphore>
#include <QTimer>
#include <QEventLoop>
#include <QFile>
#include <QApplication>

namespace Debugger {

DebuggerJob::DebuggerJob( const ScriptData &scriptData, const QString &useCase, QObject *parent )
        : ThreadWeaver::Job(parent), m_agent(0), m_data(scriptData), m_debugger(0),
          m_success(true), m_aborted(false), m_mutex(new QMutex(QMutex::Recursive)),
          m_waitLoop(0), m_engineSemaphore(new QSemaphore(1)),
          m_useCase(useCase), m_quit(false), m_scriptObjectTargetThread(0), m_currentLoop(0),
          m_stoppedSignalWasProcessed(true)
{
    Q_ASSERT_X( scriptData.isValid(), "DebuggerJob constructor",
                "ScriptData contains invalid data, no provider data and/or script code loaded" );

    QMutexLocker locker( m_mutex );
    if ( m_useCase.isEmpty() ) {
        QTimer::singleShot( 0, this, SLOT(setDefaultUseCase()) );
    }
}

DebuggerJob::DebuggerJob( DebuggerAgent *debugger, QSemaphore *engineSemaphore,
                          const ScriptData &scriptData, const ScriptObjects &objects,
                          const QString &useCase, QObject *parent )
        : ThreadWeaver::Job(parent), m_agent(debugger), m_data(scriptData), m_objects(objects),
          m_success(true), m_aborted(false), m_mutex(new QMutex(QMutex::Recursive)),
          m_waitLoop(0), m_engineSemaphore(engineSemaphore),
          m_useCase(useCase), m_quit(false), m_scriptObjectTargetThread(0),
          m_stoppedSignalWasProcessed(true)
{
    Q_ASSERT_X( scriptData.isValid(), "DebuggerJob constructor",
                "ScriptData contains invalid data, no provider data and/or script code loaded" );
    Q_ASSERT_X( objects.isValid(), "DebuggerJob constructor",
                "ScriptObjects contains invalid objects, use ScriptObjects::createObjects()" );

    QMutexLocker locker( m_mutex );
    if ( m_useCase.isEmpty() ) {
        QTimer::singleShot( 0, this, SLOT(setDefaultUseCase()) );
    }
}

DebuggerJob::~DebuggerJob()
{
    if ( !isFinished() ) {
        requestAbort();
    }

    m_mutex->lockInline();
    if ( m_agent ) {
        DEBUG_JOBS("Wait for the debugger to finish");
        m_agent->finish();
    }
    m_mutex->unlockInline();

    delete m_mutex;
    delete m_engineSemaphore;
}

void DebuggerJob::connectScriptObjects( bool doConnect )
{
    QMutexLocker locker( m_mutex );
    if ( doConnect ) {
        connect( m_objects.helper.data(), SIGNAL(messageReceived(QString,QScriptContextInfo,QString,Helper::ErrorSeverity)),
                 this, SIGNAL(scriptMessageReceived(QString,QScriptContextInfo,QString,Helper::ErrorSeverity)) );
    } else {
        disconnect( m_objects.helper.data(), SIGNAL(messageReceived(QString,QScriptContextInfo,QString,Helper::ErrorSeverity)),
                    this, SIGNAL(scriptMessageReceived(QString,QScriptContextInfo,QString,Helper::ErrorSeverity)) );
    }
}

void DebuggerJob::disconnectScriptObjects()
{
    QMutexLocker locker( m_mutex );
    // Disconnect all signals of script objects from this job
    // to prevent crashes after execution was aborted
    if ( !m_objects.helper.isNull() ) {
        disconnect( m_objects.helper.data(), 0, this, 0 );
    }
    if ( !m_objects.network.isNull() ) {
        disconnect( m_objects.network.data(), 0, this, 0 );
    }
    if ( !m_objects.result.isNull() ) {
        disconnect( m_objects.result.data(), 0, this, 0 );
    }

    m_objects.clear();
    m_agent = 0;
}

LoadScriptJob::~LoadScriptJob()
{
}

EvaluateInContextJob::~EvaluateInContextJob()
{
}

ExecuteConsoleCommandJob::~ExecuteConsoleCommandJob()
{
}

void DebuggerJob::setDefaultUseCase()
{
    QMutexLocker locker( m_mutex );
    if ( m_useCase.isEmpty() ) {
        // Do not change use case, if it was already set using setUseCase()
        m_useCase = defaultUseCase();
    }
}

bool DebuggerJob::success() const
{
    QMutexLocker locker( m_mutex );
    return m_success;
}

ScriptObjects DebuggerJob::objects() const
{
    QMutexLocker locker( m_mutex );
    return m_objects;
}

void DebuggerJob::requestAbort()
{
    QMutexLocker locker( m_mutex );
    if ( m_quit || !m_agent ) {
        DEBUG_JOBS("Is already aborting/finished");
        return;
    }

    DEBUG_JOBS("Request abort");
    m_quit = true;
    if ( m_currentLoop ) {
        QEventLoop *loop = m_currentLoop;
        m_currentLoop = 0;
        loop->quit();
    }

    if ( !isFinished() && m_objects.network->hasRunningRequests() ) {
        m_objects.network->abortAllRequests();
    }

    if ( m_agent ) {
        m_agent->abortDebugger();
    }
}

QString DebuggerJob::typeToString( JobType type )
{
    switch ( type ) {
    case LoadScript:
        return "LoadScriptJob";
    case EvaluateInContext:
        return "EvaluateInContextJob";
    case ExecuteConsoleCommand:
        return "ExecuteConsoleCommandJob";
    case CallScriptFunction:
        return "CallScriptFunctionJob";
    case TestFeatures:
        return "TestFeaturesJob";
    case TimetableDataRequest:
        return "TimetableDataRequestJob";
    default:
        return QString("Unknown job type %1").arg(type);
    }
}

QString EvaluateInContextJob::toString() const
{
    QMutexLocker locker( m_mutex );
    QString programString = m_program;
    if ( programString.length() > 100 ) {
        programString = programString.left(100) + "...";
    }
    return QString("%1 (%2)").arg( DebuggerJob::toString() ).arg( programString );
}

QString ExecuteConsoleCommandJob::toString() const
{
    QMutexLocker locker( m_mutex );
    return QString("%1 (%2)").arg( DebuggerJob::toString() ).arg( m_command.toString() );
}

void DebuggerJob::run()
{
#ifdef DEBUG_JOB_START_END
    kDebug() << "\nDebuggerJob::run(): Start" << toString();
#endif

    debuggerRun();
    disconnectScriptObjects();

#ifdef DEBUG_JOB_START_END
    debugJobEnd();
#endif
}

#ifdef DEBUG_JOB_START_END
void DebuggerJob::debugJobEnd() const
{
    QMutexLocker locker( m_mutex );
    kDebug() << "DebuggerJob::run():" << (m_success ? "Success" : "Fail") << toString();
}
#endif

void DebuggerJob::attachAgent( DebuggerAgent *agent )
{
    // Initialize and connect the new agent.
    // The agent lives in the thread of this job, while the job itself lives in the GUI thread.
    // Therefore the connections below are queued connections.
    m_mutex->lock();
    m_agent = agent;
    agent->setMainScriptFileName( m_data.provider.scriptFileName() );
    agent->setDebugFlags( NeverInterrupt );
    Debugger *debugger = m_debugger;
    m_mutex->unlock();

    connectAgent( agent );

    // Add existing breakpoints
    if ( debugger ) {
        BreakpointModel *breakpointModel = debugger->breakpointModel();
        for ( int i = 0; i < breakpointModel->rowCount(); ++i ) {
            agent->addBreakpoint( *breakpointModel->breakpointFromRow(i) );
        }
    }
}

void DebuggerJob::connectAgent( DebuggerAgent *agent, bool doConnect, bool connectModels )
{
    QMutexLocker locker( m_mutex );
    if ( doConnect ) {
        connect( agent, SIGNAL(started(QDateTime)), this, SIGNAL(started(QDateTime)) );
        connect( agent, SIGNAL(stopped(QDateTime,bool,bool,int,QString,QStringList)),
                 this, SLOT(slotStopped(QDateTime,bool,bool,int,QString,QStringList)) );
        connect( agent, SIGNAL(aborted()), this, SIGNAL(aborted()) );
        connect( agent, SIGNAL(positionChanged(int,int,int,int)),
                 this, SIGNAL(positionChanged(int,int,int,int)) );
        connect( agent, SIGNAL(stateChanged(DebuggerState,DebuggerState)),
                 this, SIGNAL(stateChanged(DebuggerState,DebuggerState)) );
        connect( agent, SIGNAL(breakpointReached(Breakpoint)),
                 this, SIGNAL(breakpointReached(Breakpoint)) );
        connect( agent, SIGNAL(exception(int,QString,QString)),
                 this, SIGNAL(exception(int,QString,QString)) );
        connect( agent, SIGNAL(interrupted(int,QString,QDateTime)),
                 this, SIGNAL(interrupted(int,QString,QDateTime)) );
        connect( agent, SIGNAL(continued(QDateTime,bool)), this, SIGNAL(continued(QDateTime,bool)) );
        connect( agent, SIGNAL(output(QString,QScriptContextInfo)),
                 this, SIGNAL(output(QString,QScriptContextInfo)) );
        connect( agent, SIGNAL(informationMessage(QString)),
                 this, SIGNAL(informationMessage(QString)) );
        connect( agent, SIGNAL(errorMessage(QString)),
                 this, SIGNAL(errorMessage(QString)) );
        connect( agent, SIGNAL(evaluationInContextFinished(QScriptValue)),
                 this, SIGNAL(evaluationInContextFinished(QScriptValue)) );
        connect( agent, SIGNAL(evaluationInContextAborted(QString)),
                 this, SIGNAL(evaluationInContextAborted(QString)) );
        if ( connectModels ) {
            connect( agent, SIGNAL(variablesChanged(VariableChange)),
                    this, SIGNAL(variablesChanged(VariableChange)) );
            connect( agent, SIGNAL(backtraceChanged(BacktraceChange)),
                    this, SIGNAL(backtraceChanged(BacktraceChange)) );
            connect( agent, SIGNAL(breakpointReached(Breakpoint)),
                    this, SIGNAL(breakpointReached(Breakpoint)) );
        }
    } else {
        disconnect( agent, SIGNAL(started(QDateTime)), this, SIGNAL(started(QDateTime)) );
        disconnect( agent, SIGNAL(stopped(QDateTime,bool,bool,int,QString,QStringList)),
                    this, SLOT(slotStopped(QDateTime,bool,bool,int,QString,QStringList)) );
        disconnect( agent, SIGNAL(aborted()), this, SIGNAL(aborted()) );
        disconnect( agent, SIGNAL(positionChanged(int,int,int,int)),
                    this, SIGNAL(positionChanged(int,int,int,int)) );
        disconnect( agent, SIGNAL(stateChanged(DebuggerState,DebuggerState)),
                    this, SIGNAL(stateChanged(DebuggerState,DebuggerState)) );
        disconnect( agent, SIGNAL(breakpointReached(Breakpoint)),
                    this, SIGNAL(breakpointReached(Breakpoint)) );
        disconnect( agent, SIGNAL(exception(int,QString,QString)),
                    this, SIGNAL(exception(int,QString,QString)) );
        disconnect( agent, SIGNAL(interrupted(int,QString,QDateTime)),
                    this, SIGNAL(interrupted(int,QString,QDateTime)) );
        disconnect( agent, SIGNAL(continued(QDateTime,bool)), this, SIGNAL(continued(QDateTime,bool)) );
        disconnect( agent, SIGNAL(output(QString,QScriptContextInfo)),
                    this, SIGNAL(output(QString,QScriptContextInfo)) );
        disconnect( agent, SIGNAL(informationMessage(QString)),
                    this, SIGNAL(informationMessage(QString)) );
        disconnect( agent, SIGNAL(errorMessage(QString)),
                    this, SIGNAL(errorMessage(QString)) );
        disconnect( agent, SIGNAL(evaluationInContextFinished(QScriptValue)),
                    this, SIGNAL(evaluationInContextFinished(QScriptValue)) );
        disconnect( agent, SIGNAL(evaluationInContextAborted(QString)),
                    this, SIGNAL(evaluationInContextAborted(QString)) );
        if ( connectModels ) {
            disconnect( agent, SIGNAL(variablesChanged(VariableChange)),
                        this, SIGNAL(variablesChanged(VariableChange)) );
            disconnect( agent, SIGNAL(backtraceChanged(BacktraceChange)),
                        this, SIGNAL(backtraceChanged(BacktraceChange)) );
            disconnect( agent, SIGNAL(breakpointReached(Breakpoint)),
                        this, SIGNAL(breakpointReached(Breakpoint)) );
        }
    }
}

void DebuggerJob::slotStopped( const QDateTime &timestamp, bool aborted, bool hasRunningRequests,
                               int uncaughtExceptionLineNumber, const QString &uncaughtException,
                               const QStringList &backtrace )
{
    ScriptStoppedFlags flags = aborted ? ScriptWasAborted : ScriptStopped;
    if ( hasRunningRequests ) {
        flags |= ScriptHasRunningRequests;
    }
    emit stopped( timestamp, flags, uncaughtExceptionLineNumber, uncaughtException, backtrace );
}

DebuggerAgent *DebuggerJob::createAgent() {
    // m_engineSemaphore is expected to be locked here
    // Create engine and agent and connect them
    m_mutex->lock();
    QScriptEngine *engine = new QScriptEngine();
    DebuggerAgent *agent = new DebuggerAgent( engine, m_engineSemaphore, true );
    m_mutex->unlock();

    engine->setAgent( agent );
    attachAgent( agent );

    // Initialize the script
    QMutexLocker locker( m_mutex );
    const ScriptData data = m_data;
    m_objects.createObjects( data );
    if ( !m_objects.attachToEngine(engine, data) ) {
        kDebug() << "Cannot attach script objects to engine" << m_objects.lastError;
        engine->setAgent( 0 );
        delete agent;
        delete engine;
        m_explanation = m_objects.lastError;
        m_success = false;
        return 0;
    }
    Q_ASSERT( m_objects.isValid() );

    // Make this job responsive while running the script
    engine->setProcessEventsInterval( 50 );
    return agent;
}

void DebuggerJob::destroyAgent()
{
    // m_engineSemaphore is expected to be locked here
    m_mutex->lock();
    DebuggerAgent *agent = m_agent;
    QScriptEngine *engine = agent->engine();
    m_mutex->unlock();

    m_mutex->lock();
    if ( !m_stoppedSignalWasProcessed ) {
        kWarning() << "No stopped() signal was processed, aborted?";
        setJobDone( true );
        m_agent->engine()->abortEvaluation();
    }
    DEBUG_JOBS( "Destroying agent" << m_data.provider.id() << agent << engine );
    if ( engine->isEvaluating() ) {
        kWarning() << "Still evaluating..." << engine;
    }
    m_agent = 0;
    engine->setAgent( 0 );
    m_objects.clear();
    delete agent;
    engine->deleteLater();
    m_mutex->unlock();
}

void DebuggerJob::setJobDone( bool done )
{
    // The stopped() signal was processed in a connected Debugger instance
    QMutexLocker locker( m_mutex );
    m_stoppedSignalWasProcessed = done;
    if ( done && m_waitLoop ) {
        m_waitLoop->exit();
    }
}

bool DebuggerJob::waitFor( QObject *sender, const char *signal, WaitForType type )
{
    // The called function returned, but asynchronous network requests may have been started.
    // Wait for all network requests to finish, because slots in the script may get called
    const int finishWaitTime = 1000;
    int finishWaitCounter = 0;

    m_mutex->lockInline();
    bool success = m_success;
    bool quit = m_quit;
    if ( !success || quit ) {
        m_mutex->unlockInline();
        return true;
    }

    DebuggerAgent *agent = m_agent;
    ScriptObjects objects = m_objects;
    bool stoppedSignalWasProcessed = m_stoppedSignalWasProcessed;
    m_mutex->unlockInline();

    while ( !agent->wasLastRunAborted() && finishWaitCounter < 50 && sender ) {
        if ( (type == WaitForNetwork && !objects.network->hasRunningRequests()) ||
             (type == WaitForScriptFinish &&
              !agent->engine()->isEvaluating() && stoppedSignalWasProcessed) ||
             (type == WaitForInjectedScriptFinish && !agent->isInjectedScriptEvaluating()) ||
             (type == WaitForInterrupt && agent->isInterrupted()) ||
             (type == WaitForNothing && finishWaitCounter > 0) )
        {
            break;
        }

        QEventLoop loop;
        connect( sender, signal, &loop, SLOT(quit()) );
        QTimer::singleShot( finishWaitTime, &loop, SLOT(quit()) );

        // Store a pointer to the event loop, to be able to quit it from the destructor
        m_mutex->lockInline();
        m_currentLoop = &loop;
        m_mutex->unlockInline();

        // Engine continues execution here / waits for a signal
        loop.exec();
        qApp->processEvents();

        m_mutex->lockInline();
        if ( !m_currentLoop || m_quit ) {
            m_mutex->unlockInline();
            // Job was aborted
            destroyAgent();
            return false;
        }
        m_currentLoop = 0;
        stoppedSignalWasProcessed = m_stoppedSignalWasProcessed;
        m_mutex->unlockInline();

        ++finishWaitCounter;
    }

    if ( finishWaitCounter >= 50 ) {
        DEBUG_JOBS("Timeout");
        if ( type == WaitForScriptFinish ) {
            // Script not finished
            DEBUG_JOBS("Script not finished, abort");
            agent->engine()->abortEvaluation();
        } else if ( type == WaitForNetwork ) {
            objects.network->abortAllRequests();
        }
    }
    return finishWaitCounter < 50;
}

void LoadScriptJob::debuggerRun()
{
    m_mutex->lockInline();
    QScriptSyntaxCheckResult syntax = QScriptEngine::checkSyntax( m_data.program->sourceCode() );
    if ( syntax.state() == QScriptSyntaxCheckResult::Error ) {
        DEBUG_JOBS("Syntax error at" << syntax.errorLineNumber() << syntax.errorColumnNumber()
                   << syntax.errorMessage());
        if ( syntax.errorColumnNumber() < 0 ) {
            m_explanation = i18nc("@info/plain", "Syntax error at line %1: <message>%2</message>",
                                  syntax.errorLineNumber(), syntax.errorMessage());
        } else {
            m_explanation = i18nc("@info/plain", "Syntax error at line %1, column %2: <message>%3</message>",
                                  syntax.errorLineNumber(), syntax.errorColumnNumber(),
                                  syntax.errorMessage());
        }
        m_success = false;
        m_mutex->unlockInline();
        return;
    }

    // Create new engine and agent
    m_mutex->unlockInline();
    m_engineSemaphore->acquire();
    DebuggerAgent *agent = createAgent();
    m_mutex->lockInline();
    if ( !agent || m_quit ) {
        m_mutex->unlockInline();
        if ( agent ) {
            destroyAgent();
        }
        m_engineSemaphore->release();
        return;
    }
    const ScriptData data = m_data;
    const ScriptObjects objects = m_objects;
    const DebugFlags debugFlags = m_debugFlags;
    m_mutex->unlockInline();

    agent->setDebugFlags( debugFlags );
    agent->setExecutionControlType( debugFlags.testFlag(InterruptAtStart)
                                    ? ExecuteInterrupt : ExecuteRun );
    agent->engine()->evaluate( *data.program );
    Q_ASSERT_X( !objects.network->hasRunningRequests() || !agent->engine()->isEvaluating(),
                "LoadScriptJob::debuggerRun()",
                "Evaluating the script should not start any asynchronous requests, bad script" );

    // Wait for the stopped signal
    const bool aborted = agent->wasLastRunAborted();
    if ( aborted ) {
        kDebug() << "Aborted" << m_data.provider.id();
        m_stoppedSignalWasProcessed = true; // Do not crash on assert in destroyAgent()
        destroyAgent();
        QMutexLocker locker( m_mutex );
        m_explanation = i18nc("@info/plain", "Was aborted");
        m_success = false;
        m_aborted = true;
        m_engineSemaphore->release();
        return;
    }

    m_mutex->lock();
    if ( !m_stoppedSignalWasProcessed ) {
        // Wait for the stopped() signal to be processed by the connected Debugger instance
        m_waitLoop = new QEventLoop();
        m_mutex->unlock();
        m_waitLoop->exec();

        // The stopped() signal was processed
        m_mutex->lock();
        delete m_waitLoop;
        m_waitLoop = 0;
        Q_ASSERT( m_stoppedSignalWasProcessed );
        m_mutex->unlock();
    } else {
        m_mutex->unlock();
    }


    // Check that the getTimetable() function is implemented in the script
    const QString functionName = ServiceProviderScript::SCRIPT_FUNCTION_GETTIMETABLE;
    const QScriptValue function = agent->engine()->globalObject().property( functionName );
    if ( !function.isFunction() ) {
        QMutexLocker locker( m_mutex );
        DEBUG_JOBS("Did not find" << functionName << "function in the script!");
        m_explanation = i18nc("@info/plain", "Did not find required '%1' function in the script.",
                              functionName);
        m_success = false;
    }

    const QStringList functions = QStringList()
            << ServiceProviderScript::SCRIPT_FUNCTION_FEATURES
            << ServiceProviderScript::SCRIPT_FUNCTION_GETTIMETABLE
            << ServiceProviderScript::SCRIPT_FUNCTION_GETJOURNEYS
            << ServiceProviderScript::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS
            << ServiceProviderScript::SCRIPT_FUNCTION_GETADDITIONALDATA;
    QStringList globalFunctions;
    foreach ( const QString &function, functions ) {
        const QScriptValue value = agent->engine()->globalObject().property( function );
        if ( value.isFunction() ) {
            globalFunctions << function;
        }
    }
    const QStringList includedFiles =
            agent->engine()->globalObject().property( "includedFiles" ).toVariant().toStringList();
    m_engineSemaphore->release();

    m_mutex->lock();
    if ( !m_success || m_quit ) {
        m_success = false;
    } else if ( agent->hasUncaughtException() ) {
        m_engineSemaphore->acquire();
        const QString uncaughtException = agent->uncaughtException().toString();
        DEBUG_JOBS("Error in the script" << agent->uncaughtExceptionLineNumber()
                   << uncaughtException);
        m_engineSemaphore->release();

        m_includedFiles = includedFiles;
        m_globalFunctions = globalFunctions;
        m_explanation = i18nc("@info/plain", "Error in the script: "
                              "<message>%1</message>.", uncaughtException);
        m_success = false;
    } else if ( agent->wasLastRunAborted() ) {
        // Script was aborted
        m_explanation = i18nc("@info/plain", "Aborted");
        m_success = false;
    } else {
        m_includedFiles = includedFiles;
        m_globalFunctions = globalFunctions;
        m_success = true;
    }
    m_mutex->unlock();

    m_engineSemaphore->acquire();
    destroyAgent();
    m_engineSemaphore->release();
}

bool ExecuteConsoleCommandJob::runInForeignAgent( DebuggerAgent *agent )
{
    // Execute the command
    m_mutex->lockInline();
    const ConsoleCommand command = m_command;
    m_mutex->unlockInline();

    QString returnValue;
    bool success = agent->executeCommand( command, &returnValue );

    QMutexLocker locker( m_mutex );
    m_success = success;
    m_returnValue = returnValue;
    return true;
}

bool ForeignAgentJob::waitForFinish( DebuggerAgent *agent, const ScriptObjects &objects )
{
    // Wait for the evaluation to finish,
    // does not include waiting for running asynchronous network requests to finish
    if ( !waitFor(agent, SIGNAL(evaluationInContextFinished(QScriptValue)),
                  WaitForInjectedScriptFinish) )
    {
        return false;
    }

    // The called function returned, but asynchronous network requests may have been started.
    // Wait for all network requests to finish, because slots in the script may get called
    if ( !waitFor(objects.network.data(), SIGNAL(allRequestsFinished()), WaitForNetwork) ) {
        return false;
    }

    // Wait for the evaluation to finish after having received network data,
    // ie. wait until execution gets interrupted again in the parent thread, if any
    if ( m_parentJob &&
         !waitFor(agent, SIGNAL(interrupted(int,QString,QDateTime)), WaitForInterrupt) )
    {
        return false;
    }

    return true;
}

void ForeignAgentJob::debuggerRun()
{
    m_mutex->lockInline();
    const ScriptData data = m_data;
    DebuggerAgent *agent = m_agent;
    QScriptEngine *engine = 0;
    ScriptObjects objects = m_objects;
    m_success = true;
    const QPointer< DebuggerJob > parentJob = m_parentJob;
    m_mutex->unlockInline();

    const bool useExistingEngine = agent;
    QThread *scriptObjectThread = 0;
    DebugFlags previousDebugFlags = NeverInterrupt;
    ExecutionControl previousExecutionControl = ExecuteInterrupt;
    if ( useExistingEngine ) {
        engine = agent->engine();

        // Let the running job move script objects to this thread
        QEventLoop loop;
        connect( parentJob, SIGNAL(movedScriptObjectsToThread(QThread*)),
                 &loop, SLOT(quit()) );
        scriptObjectThread = objects.currentThread();
        parentJob->moveScriptObjectsToThread( QThread::currentThread() );
        loop.exec();

        // Do not interrupt in injected script code,
        // store previous debug flags set by the parent job
        previousDebugFlags = agent->debugFlags();
        previousExecutionControl = agent->currentExecutionControlType();
        agent->setDebugFlags( NeverInterrupt );
        agent->setExecutionControlType( ExecuteRun );

        connectAgent( agent );
    } else {
        // Create new engine and agent
        m_engineSemaphore->acquire();
        m_mutex->lockInline();
        agent = createAgent();
        objects = m_objects;
        m_mutex->unlockInline();

        if ( !agent ) {
            m_engineSemaphore->release();
            return;
        }
        engine = agent->engine();

        // Load script
        engine->evaluate( *data.program );
        Q_ASSERT_X( !engine->isEvaluating(), "CallScriptFunctionJob::debuggerRun()",
                    "Evaluating the script should not start any asynchronous requests, bad script" );
        m_engineSemaphore->release();
    }

    m_engineSemaphore->acquire();
    connect( agent, SIGNAL(evaluationInContextAborted(QString)),
             this, SLOT(evaluationAborted(QString)) );

    // Start the job in the foreign agent, implemented by derived classes
    // and wait until it has finished (this includes waiting for running network requests)
    if ( !runInForeignAgent(agent) || !waitForFinish(agent, objects) ) {
        destroyAgent();
        m_engineSemaphore->release();
        return;
    }

    disconnect( agent, SIGNAL(evaluationInContextAborted(QString)),
                this, SLOT(evaluationAborted(QString)) );

    // Handle uncaught exceptions
    if ( agent->hasUncaughtException() ) {
        handleUncaughtException( agent );
    }
    m_engineSemaphore->release();

    // Cleanup
    if ( useExistingEngine ) {
        QMutexLocker locker( m_mutex );
        // Move script objects back to the parent thread
        DEBUG_JOBS("Move script objects back to the parent thread");
        m_engineSemaphore->acquire();
        objects.moveToThread( scriptObjectThread );
        connectAgent( agent, false );
        m_engineSemaphore->release();

        // Restore previous debug flags, interrupts were disabled for this job
        agent->setDebugFlags( previousDebugFlags );
        agent->setExecutionControlType( previousExecutionControl );
        m_agent = 0;
        m_objects.clear();
        m_engineSemaphore = 0; // Do not delete, used by the other thread

        parentJob->gotScriptObjectsBack();
    } else {
        m_engineSemaphore->acquire();
        destroyAgent();
        m_engineSemaphore->release();
    }
}

void EvaluateInContextJob::handleUncaughtException( DebuggerAgent *agent )
{
    QMutexLocker locker( m_mutex );
    const QString program = m_program;
    const QString context = m_context;
    EvaluationResult result = m_result;
    handleError( agent->engine(), i18nc("@info/plain", "Error in the script when evaluating '%1' "
                 "with code <icode>%2</icode>: <message>%3</message>",
                 context, program, agent->uncaughtException().toString()),
                 &result );
    m_result = result;
}

bool EvaluateInContextJob::runInForeignAgent( DebuggerAgent *agent )
{
    m_mutex->lockInline();
    const QString program = m_program;
    const QString context = m_context;
    m_result.error = false;
    m_mutex->unlockInline();

    // m_engineSemaphore is locked
    // Check syntax of the program to run
    DEBUG_JOBS("Evaluate in context" << context << program);
    QScriptSyntaxCheckResult syntax = QScriptEngine::checkSyntax( program );
    if ( syntax.state() == QScriptSyntaxCheckResult::Error ) {
        DEBUG_JOBS("Error in script code:" << syntax.errorLineNumber() << syntax.errorMessage());

        QMutexLocker locker( m_mutex );
        m_explanation = syntax.errorMessage().isEmpty() ? i18nc("@info/plain", "Syntax error")
                : i18nc("@info/plain", "Syntax error: <message>%1</message>.", syntax.errorMessage());
        m_success = false;
        m_result.error = true;
        m_result.errorLineNumber = syntax.errorLineNumber();
        m_result.errorMessage = m_explanation;
        return false;
    }

    // Evaluate script code
    EvaluationResult result;
    result.returnValue = agent->evaluateInContext( /*'{' + */program/* + '}'*/, context,
            &(result.error), &(result.errorLineNumber),
            &(result.errorMessage), &(result.backtrace), NeverInterrupt ).toString();
    DEBUG_JOBS("Evaluate in context result:" << result.returnValue);

    QMutexLocker locker( m_mutex );
    m_result = result;
    return true;
}

void DebuggerJob::evaluationAborted( const QString &errorMessage )
{
    QMutexLocker locker( m_mutex );
    m_explanation = errorMessage;
    m_success = false;
    _evaluationAborted( errorMessage );
    DebuggerAgent *agent = m_agent;

    agent->abortDebugger();
}

void EvaluateInContextJob::_evaluationAborted( const QString &errorMessage )
{
    QMutexLocker locker( m_mutex );
    m_result.error = true;
    m_result.errorMessage = errorMessage;
}

void DebuggerJob::handleError( QScriptEngine *engine, const QString &message,
                               EvaluationResult *result )
{
    // m_engineSemaphore is locked
    handleError( engine->uncaughtExceptionLineNumber(), engine->uncaughtException().toString(),
                 engine->uncaughtExceptionBacktrace(), message, result );
}

void DebuggerJob::handleError( int uncaughtExceptionLineNumber, const QString &uncaughtException,
                               const QStringList &backtrace, const QString &message,
                               EvaluationResult *result )
{
    if ( result ) {
        result->error = true;
        result->errorMessage = uncaughtException;
        result->errorLineNumber = uncaughtExceptionLineNumber;
        result->backtrace = backtrace;
    }
    DEBUG_JOBS((!message.isEmpty() ? message : "Script error")
               << uncaughtExceptionLineNumber << uncaughtException << backtrace);

    QMutexLocker locker( m_mutex );
    m_explanation = !message.isEmpty() ? message
            : i18nc("@info/plain", "Error in the script: <message>%1</message>.",
                    uncaughtException);
    m_success = false;
}

void DebuggerJob::attachDebugger( Debugger *debugger )
{
    // Connect models
    BreakpointModel *breakpointModel = debugger->breakpointModel();
    connect( this, SIGNAL(variablesChanged(VariableChange)),
             debugger->variableModel(), SLOT(applyChange(VariableChange)) );
    connect( this, SIGNAL(backtraceChanged(BacktraceChange)),
             debugger->backtraceModel(), SLOT(applyChange(BacktraceChange)) );
    connect( this, SIGNAL(breakpointsChanged(BreakpointChange)),
             breakpointModel, SLOT(applyChange(BreakpointChange)) );
    connect( this, SIGNAL(breakpointReached(Breakpoint)),
             breakpointModel, SLOT(updateBreakpoint(Breakpoint)) );
    connect( breakpointModel, SIGNAL(breakpointModified(Breakpoint)),
             this, SLOT(updateBreakpoint(Breakpoint)) );
    connect( breakpointModel, SIGNAL(breakpointAdded(Breakpoint)),
             this, SLOT(addBreakpoint(Breakpoint)) );
    connect( breakpointModel, SIGNAL(breakpointAboutToBeRemoved(Breakpoint)),
             this, SLOT(removeBreakpoint(Breakpoint)) );

    QMutexLocker locker( m_mutex );
    m_debugger = debugger;
}

const ConsoleCommand ExecuteConsoleCommandJob::command() const
{
    QMutexLocker locker( m_mutex );
    return m_command;
}

QVariant ExecuteConsoleCommandJob::returnValue() const
{
    QMutexLocker locker( m_mutex );
    return m_returnValue;
}

EvaluationResult EvaluateInContextJob::result() const
{
    QMutexLocker locker( m_mutex );
    return m_result;
}

QVariant EvaluateInContextJob::returnValue() const
{
    QMutexLocker locker( m_mutex );
    return m_result.returnValue;
}

QString LoadScriptJob::defaultUseCase() const
{
    return i18nc("@info", "Load script");
}

QString EvaluateInContextJob::defaultUseCase() const
{
    QMutexLocker locker( m_mutex );
    return i18nc("@info", "Evaluate <icode>%1</icode>", m_program.left(50));
}

QString ExecuteConsoleCommandJob::defaultUseCase() const
{
    QMutexLocker locker( m_mutex );
    return i18nc("@info", "Execute command <icode>%1</icode>", m_command.toString());
}

QStringList LoadScriptJob::globalFunctions() const
{
    QMutexLocker locker( m_mutex );
    return m_globalFunctions;
}

QStringList LoadScriptJob::includedFiles() const
{
    QMutexLocker locker( m_mutex );
    return m_includedFiles;
}

DebugFlags LoadScriptJob::debugFlags() const
{
    QMutexLocker locker( m_mutex );
    return m_debugFlags;
}

void DebuggerJob::setUseCase( const QString &useCase )
{
    QMutexLocker locker( m_mutex );
    m_useCase = useCase;
}

QString DebuggerJob::useCase() const
{
    QMutexLocker locker( m_mutex );
    return m_useCase;
}

QString DebuggerJob::toString() const
{
    QMutexLocker locker( m_mutex );
    return typeToString( type() ) + ", " + m_data.provider.id();
}

DebuggerAgent *DebuggerJob::debuggerAgent() const
{
    QMutexLocker locker( m_mutex );
    return m_agent;
}

QSemaphore *DebuggerJob::engineSemaphore() const
{
    QMutexLocker locker( m_mutex );
    return m_engineSemaphore;
}

bool DebuggerJob::wasAborted() const
{
    QMutexLocker locker( m_mutex );
    return m_aborted;
}

QString DebuggerJob::explanation() const
{
    QMutexLocker locker( m_mutex );
    return m_explanation;
}

const ServiceProviderData DebuggerJob::providerData() const
{
    QMutexLocker locker( m_mutex );
    return m_data.provider;
}

void DebuggerJob::debugStepInto( int repeat )
{
    QMutexLocker locker( m_mutex );
    if ( m_agent ) {
        m_agent->debugStepInto( repeat );
    } else {
        kWarning() << "Debugger already deleted";
    }
}

void DebuggerJob::abortDebugger()
{
    QMutexLocker locker( m_mutex );
    if ( m_agent ) {
        m_agent->abortDebugger();
    } else {
        kWarning() << "Debugger already deleted";
    }
}

void DebuggerJob::addBreakpoint( const Breakpoint &breakpoint )
{
    QMutexLocker locker( m_mutex );
    if ( m_agent ) {
        m_agent->addBreakpoint( breakpoint );
    } else {
        kWarning() << "Debugger already deleted";
    }
}

void DebuggerJob::checkExecution()
{
    QMutexLocker locker( m_mutex );
    if ( m_agent ) {
        m_agent->checkExecution();
    }
}

void DebuggerJob::debugContinue()
{
    QMutexLocker locker( m_mutex );
    if ( m_agent ) {
        m_agent->debugContinue();
    } else {
        kWarning() << "Debugger already deleted";
    }
}

void DebuggerJob::debugInterrupt()
{
    QMutexLocker locker( m_mutex );
    if ( m_agent ) {
        m_agent->debugInterrupt();
    } else {
        kWarning() << "Debugger already deleted";
    }
}

void DebuggerJob::debugRunUntilLineNumber( const QString &fileName, int lineNumber )
{
    QMutexLocker locker( m_mutex );
    if ( m_agent ) {
        m_agent->debugRunUntilLineNumber( fileName, lineNumber );
    } else {
        kWarning() << "Debugger already deleted";
    }
}

void DebuggerJob::debugStepOut( int repeat )
{
    QMutexLocker locker( m_mutex );
    if ( m_agent ) {
        m_agent->debugStepOut( repeat );
    } else {
        kWarning() << "Debugger already deleted";
    }
}

void DebuggerJob::debugStepOver( int repeat )
{
    QMutexLocker locker( m_mutex );
    if ( m_agent ) {
        m_agent->debugStepOver( repeat );
    } else {
        kWarning() << "Debugger already deleted";
    }
}

void DebuggerJob::removeBreakpoint( const Breakpoint &breakpoint )
{
    QMutexLocker locker( m_mutex );
    if ( m_agent ) {
        m_agent->removeBreakpoint( breakpoint );
    } else {
        kWarning() << "Debugger already deleted";
    }
}

void DebuggerJob::slotOutput( const QString &outputString, const QScriptContextInfo &contextInfo )
{
    QMutexLocker locker( m_mutex );
    if ( m_agent ) {
        m_agent->slotOutput( outputString, contextInfo );
    }
}

NextEvaluatableLineHint DebuggerJob::canBreakAt( const QString &fileName, int lineNumber ) const
{
    QMutexLocker locker( m_mutex );
    return !m_agent ? CannotFindNextEvaluatableLine
                       : m_agent->canBreakAt(fileName, lineNumber);
}

int DebuggerJob::getNextBreakableLineNumber( const QString &fileName, int lineNumber ) const
{
    QMutexLocker locker( m_mutex );
    return !m_agent ? -1 : m_agent->getNextBreakableLineNumber(fileName, lineNumber);
}

void DebuggerJob::finish() const
{
    DEBUG_JOBS("Wait for the job to finish...");
    m_mutex->lockInline();
    if ( m_agent ) {
        m_agent->finish();
    }

    if ( !isFinished() ) {
        QEventLoop loop;
        connect( this, SIGNAL(done(ThreadWeaver::Job*)), &loop, SLOT(quit()) );
        m_mutex->unlockInline();

        loop.exec();
    } else {
        m_mutex->unlockInline();
    }
    DEBUG_JOBS("...job finished");
}

bool DebuggerJob::isAborting() const
{
    QMutexLocker locker( m_mutex );
    return !m_agent ? false : m_agent->isAborting();
}

bool DebuggerJob::isInterrupted() const
{
    QMutexLocker locker( m_mutex );
    return !m_agent ? false : m_agent->isInterrupted();
}

bool DebuggerJob::isRunning() const
{
    QMutexLocker locker( m_mutex );
    return !m_agent ? false : m_agent->isRunning();
}

bool DebuggerJob::isEvaluating() const
{
    QMutexLocker locker( m_mutex );
    if ( !m_agent ) {
        return false;
    }

    m_engineSemaphore->acquire();
    const bool result = !m_agent || !m_agent->engine() ? false : m_agent->engine()->isEvaluating();
    m_engineSemaphore->release();
    return result;
}

bool DebuggerJob::canEvaluate( const QString &program ) const
{
    QMutexLocker locker( m_mutex );
    m_engineSemaphore->acquire();
    const bool result = m_agent->engine()->canEvaluate( program );
    m_engineSemaphore->release();
    return result;
}

DebuggerState DebuggerJob::state() const
{
    QMutexLocker locker( m_mutex );
    return !m_agent ? NotRunning : m_agent->state();
}

QString DebuggerJob::currentSourceFile() const
{
    QMutexLocker locker( m_mutex );
    return !m_agent ? QString() : m_agent->currentSourceFile();
}

int DebuggerJob::lineNumber() const
{
    QMutexLocker locker( m_mutex );
    return !m_agent ? -1 : m_agent->lineNumber();
}

int DebuggerJob::columnNumber() const
{
    QMutexLocker locker( m_mutex );
    return !m_agent ? -1 : m_agent->columnNumber();
}

bool DebuggerJob::hasUncaughtException() const
{
    QMutexLocker locker( m_mutex );
    return !m_agent ? false : m_agent->hasUncaughtException();
}

int DebuggerJob::uncaughtExceptionLineNumber() const
{
    QMutexLocker locker( m_mutex );
    return !m_agent ? -1 : m_agent->uncaughtExceptionLineNumber();
}

QScriptValue DebuggerJob::uncaughtException() const
{
    QMutexLocker locker( m_mutex );
    return !m_agent ? QScriptValue() : m_agent->uncaughtException();
}

ForeignAgentJob::ForeignAgentJob( const ScriptData &scriptData, const QString &useCase,
                                  DebuggerJob *parentJob, QObject *parent )
        : DebuggerJob(scriptData, useCase, parent), m_parentJob(QPointer<DebuggerJob>(parentJob))
{
    if ( parentJob ) {
        QMutexLocker locker( m_mutex );
        if ( m_engineSemaphore ) {
            delete m_engineSemaphore;
        }
        m_engineSemaphore = parentJob->engineSemaphore();
        m_agent = parentJob->debuggerAgent();
        m_objects = parentJob->objects();
    }
}

ExecuteConsoleCommandJob::ExecuteConsoleCommandJob( const ScriptData &data,
        const ConsoleCommand &command, const QString &useCase, DebuggerJob *parentJob,
        QObject *parent )
        : ForeignAgentJob(data, useCase, parentJob, parent), m_command(command)
{
}

EvaluateInContextJob::EvaluateInContextJob( const ScriptData &data, const QString &program,
        const QString &context, const QString &useCase, DebuggerJob *parentJob, QObject *parent )
        : ForeignAgentJob(data, useCase, parentJob, parent),
          m_program(program), m_context(QString(context).replace('\n', ' '))
{
}

void DebuggerJob::moveScriptObjectsToThread( QThread *thread )
{
    QMutexLocker locker( m_mutex );
    m_scriptObjectTargetThread = thread;
    // The agent lives in the thread that gets executed by this job,
    // connect directly to do something in the thread of the agent
    connect( m_agent, SIGNAL(doSomething()), this, SLOT(doSomething()), Qt::DirectConnection );
    m_agent->continueToDoSomething();
}

void DebuggerJob::doSomething()
{
    QMutexLocker locker( m_mutex );
    Q_ASSERT( QThread::currentThread() == m_agent->thread() );
    disconnect( m_agent, SIGNAL(doSomething()), this, SLOT(doSomething()) );
    QThread *targetThread = m_scriptObjectTargetThread;
    if ( !targetThread ) {
        // Nothing to do
        return;
    }

    // Move script objects to thread
    m_engineSemaphore->acquire();
    connectScriptObjects( false );
    connectAgent( m_agent, false, false );

    Q_ASSERT( m_objects.isValid() );
    m_objects.moveToThread( targetThread );
    m_scriptObjectTargetThread = 0;
    m_engineSemaphore->release();

    emit movedScriptObjectsToThread( targetThread );
}

void DebuggerJob::gotScriptObjectsBack()
{
    QMutexLocker locker( m_mutex );
    connectScriptObjects();
    connectAgent( m_agent, true, false );

    // Continue to update variables
    m_agent->continueToDoSomething();
}

} // namespace Debugger
