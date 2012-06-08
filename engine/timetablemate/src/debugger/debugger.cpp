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
#include "debugger.h"

// Own includes
#include "debuggerjobs.h"
#include "variablemodel.h"
#include "backtracemodel.h"
#include "breakpointmodel.h"
#include "timetabledatarequestjob.h"

// PublicTransport engine includes
#include <engine/timetableaccessor_info.h>
#include <engine/timetableaccessor_script.h>
#include <engine/script_thread.h>

// KDE includes
#include <KDebug>
#include <KLocalizedString>
#include <ThreadWeaver/WeaverInterface>
#include <ThreadWeaver/DependencyPolicy>
#include <ThreadWeaver/Weaver>
#include <ThreadWeaver/ResourceRestrictionPolicy>
#include <ThreadWeaver/Thread>
#include <threadweaver/JobSequence.h>
#include <threadweaver/DebuggingAids.h>

// Qt includes
#include <QScriptEngine>
#include <QScriptContextInfo>
#include <QTimer>
#include <QWaitCondition>
#include <QApplication>

namespace Debugger {

Debugger::Debugger( QObject *parent )
        : QObject( parent ), m_state(Initializing), m_weaver(new ThreadWeaver::Weaver(this)),
          m_loadScriptJob(0), m_engineMutex(new QMutex()),
          m_engine(new QScriptEngine(this)), m_script(0),
          m_debugger(new DebuggerAgent(m_engine, m_engineMutex)), m_lastScriptError(NoScriptError),
          m_info(0), m_scriptNetwork(0), m_scriptHelper(0), m_scriptResult(0), m_scriptStorage(0),
          m_variableModel(new VariableModel(this)),
          m_backtraceModel(new BacktraceModel(this)),
          m_breakpointModel(new BreakpointModel(this)),
          m_jobSequence(0), m_debuggerRestrictionPolicy(0), m_evaluateInContextRestrictionPolicy(0),
          m_running(false), m_runData(0)
{
    qRegisterMetaType<QScriptContextInfo>( "QScriptContextInfo" );
    qRegisterMetaType<EvaluationResult>( "EvaluationResult" );
    qRegisterMetaType<Frame>( "Frame" );
    qRegisterMetaType<FrameStack>( "FrameStack" );
    qRegisterMetaType<Breakpoint>( "Breakpoint" );
    qRegisterMetaType<ConsoleCommand>( "ConsoleCommand" );
    qRegisterMetaType<DebuggerState>( "DebuggerState" );

    // Forward signals of the debugger agent
    connect( m_debugger, SIGNAL(positionChanged(int,int,int,int)),
             this, SIGNAL(positionChanged(int,int,int,int)) );
    connect( m_debugger, SIGNAL(stateChanged(DebuggerState,DebuggerState)),
             this, SIGNAL(stateChanged(DebuggerState,DebuggerState)) );
    connect( m_debugger, SIGNAL(aborted()), this, SIGNAL(aborted()) );
    connect( m_debugger, SIGNAL(interrupted()), this, SLOT(slotInterrupted()) );
    connect( m_debugger, SIGNAL(continued(bool)), this, SLOT(slotContinued(bool)) );
    connect( m_debugger, SIGNAL(started()), this, SLOT(slotStarted()), Qt::QueuedConnection );
    connect( m_debugger, SIGNAL(stopped()), this, SLOT(slotStopped()), Qt::QueuedConnection );
    connect( m_debugger, SIGNAL(exception(int,QString)),
             this, SIGNAL(exception(int,QString)) );
    connect( m_debugger, SIGNAL(output(QString,QScriptContextInfo)),
             this, SIGNAL(output(QString,QScriptContextInfo)) );
    connect( m_debugger, SIGNAL(informationMessage(QString)),
             this, SIGNAL(informationMessage(QString)) );
    connect( m_debugger, SIGNAL(errorMessage(QString)),
             this, SIGNAL(errorMessage(QString)) );
    connect( m_debugger, SIGNAL(breakpointReached(Breakpoint)),
             this, SIGNAL(breakpointReached(Breakpoint)) );

    // Connect models
    connect( m_debugger, SIGNAL(variablesChanged(VariableChange)),
             m_variableModel, SLOT(applyChange(VariableChange)) );
    connect( m_debugger, SIGNAL(backtraceChanged(BacktraceChange)),
             m_backtraceModel, SLOT(applyChange(BacktraceChange)) );
    connect( m_breakpointModel, SIGNAL(breakpointAdded(Breakpoint)),
             this, SIGNAL(breakpointAdded(Breakpoint)) );
    connect( m_breakpointModel, SIGNAL(breakpointAboutToBeRemoved(Breakpoint)),
             this, SIGNAL(breakpointAboutToBeRemoved(Breakpoint)) );
    connect( m_debugger, SIGNAL(breakpointsChanged(BreakpointChange)),
             m_breakpointModel, SLOT(applyChange(BreakpointChange)) );
    connect( m_debugger, SIGNAL(breakpointReached(Breakpoint)),
             m_breakpointModel, SLOT(updateBreakpoint(Breakpoint)) );
    connect( m_breakpointModel, SIGNAL(breakpointModified(Breakpoint)),
             m_debugger, SLOT(updateBreakpoint(Breakpoint)) );
    connect( m_breakpointModel, SIGNAL(breakpointAdded(Breakpoint)),
             m_debugger, SLOT(addBreakpoint(Breakpoint)) );
    connect( m_breakpointModel, SIGNAL(breakpointAboutToBeRemoved(Breakpoint)),
             m_debugger, SLOT(removeBreakpoint(Breakpoint)) );

    // Setup script engine
    m_engine->setProcessEventsInterval( 100 );

    // Attach the debugger agent
    m_engine->setAgent( m_debugger );

    // Interrupt at first statement
    m_debugger->setExecutionControlType( ExecuteInterrupt );

    ThreadWeaver::setDebugLevel( true, 1 );
}

Debugger::~Debugger()
{
    m_debugger->abortDebugger();
    m_weaver->requestAbort();

    if ( !m_engineMutex->tryLock(500) ) {
        m_engineMutex->unlockInline();
    }
    m_weaver->finish();

    QApplication::processEvents( QEventLoop::AllEvents );

    // FIXME This can cause a crash if a queue policy is still assigned to jobs
    // (eg. jobs that weren't deleted when they were done).
    delete m_weaver;
    delete m_debuggerRestrictionPolicy;
    delete m_evaluateInContextRestrictionPolicy;

    delete m_engineMutex;
    delete m_runData;
}

void Debugger::slotStarted()
{
    if ( !m_running ) {
        // Script execution gets started
        m_running = true;
        m_runData = new ScriptRunData();
        emit started();
    } else {
        Q_ASSERT( m_runData );
        // Was waiting for a signal, script execution restarts now
        const int waitingTime = m_runData->wokeUpFromSignal();
        emit wokeUpFromSignal( waitingTime );
    }
}

void Debugger::slotStopped()
{
    if ( m_running ) {
        if ( m_runningJobs.isEmpty() ) {
            // No more running jobs
            m_running = false;
        } else if ( m_runningJobs.count() == 1 ) {
            // Only one job still in the list, check if it is finished
            // (slotStopped() was called before the job was destroyed)
            ThreadWeaver::Job *job = qobject_cast< ThreadWeaver::Job* >( m_runningJobs.first() );
            if ( job && job->isFinished() ) {
                m_running = false;
            }
        }

        Q_ASSERT( m_runData );
        if ( m_running ) {
            // Script execution gets suspended, waiting for a signal
            m_runData->waitingForSignal();
            emit waitingForSignal();
        } else {
            // Script execution has finished
            emit stopped( *m_runData );
            delete m_runData;
            m_runData = 0;
        }
    }
}

void Debugger::slotInterrupted()
{
    Q_ASSERT( m_runData );
    m_runData->interrupted();
    emit interrupted();
}

void Debugger::slotContinued( bool willInterruptAfterNextStatement )
{
    Q_ASSERT( m_runData );
    m_runData->continued();
    emit continued( willInterruptAfterNextStatement );
}

void Debugger::jobStarted( ThreadWeaver::Job *job )
{
    m_runningJobs << job;
}

void Debugger::jobDestroyed( QObject *object )
{
    m_runningJobs.removeOne( object );
}

void Debugger::createScriptObjects( const TimetableAccessorInfo *info )
{
    // Create objects for the script, they get inserted into the engine in LoadScriptJob
    if ( !m_scriptHelper ) {
        m_scriptHelper = new Helper( info->serviceProvider(), m_engine );
        connect( m_scriptHelper, SIGNAL(errorReceived(QString,QScriptContextInfo,QString)),
                 this, SIGNAL(scriptErrorReceived(QString,QScriptContextInfo,QString)) );
    }

    if ( !m_scriptResult ) {
        m_scriptResult = new ResultObject( m_engine );
//         connect( m_scriptResult, SIGNAL(publish()), this, SLOT(publish()) ); TODO
    } else {
        m_scriptResult->clear();
    }

    if ( !m_scriptNetwork ) {
        m_scriptNetwork = new Network( info->fallbackCharset(), m_engine );
    } else {
        m_scriptNetwork->clear();
    }

    if ( !m_scriptStorage ) {
        m_scriptStorage = new Storage( info->serviceProvider() );
    }
}

void Debugger::removeAllBreakpoints()
{
    m_breakpointModel->clear();
}

void Debugger::executeCommand( const ConsoleCommand &command )
{
    kDebug() << "executeCommand(...)";
    if ( command.command() == ConsoleCommand::ClearCommand ) {
        kWarning() << "The clear command is not implemented in Debugger/DebuggerAgent, "
                      "should be implemented in the console to clear it's history";
    } else {
        enqueueJob( createExecuteConsoleCommandJob(command) );
    }
}

void Debugger::evaluateInContext( const QString &program, const QString &contextName )
{
    kDebug() << "evaluateInContext(...)";
    enqueueJob( createEvaluateInContextJob(program, contextName) );
    return;
}

void Debugger::loadScript( const QString &program, const TimetableAccessorInfo *info )
{
    if ( m_loadScriptJob ) {
        kDebug() << "Script already gets loaded, please wait...";
        return;
    }
    if ( m_script && m_script->sourceCode() == program ) {
        kDebug() << "Script code unchanged";
        return;
    }

    m_state = ScriptModified;
    m_script = new QScriptProgram( program, info->scriptFileName() );
    m_info = info;
    m_lastScriptError = NoScriptError;
    enqueueNewLoadScriptJob();
}

LoadScriptJob *Debugger::enqueueNewLoadScriptJob()
{
    Q_ASSERT( m_info );
    createScriptObjects( m_info );
    m_loadScriptJob = new LoadScriptJob( m_debugger, *m_info, m_engineMutex, m_script,
                                         m_scriptHelper, m_scriptResult, m_scriptNetwork,
                                         m_scriptStorage, ResultObject::staticMetaObject, this );
    enqueueJob( m_loadScriptJob );
    return m_loadScriptJob;
}

TimetableDataRequestJob *Debugger::createTimetableDataRequestJob(
        const RequestInfo *request, DebugFlags debugFlags )
{
    Q_ASSERT( m_info );
    return new TimetableDataRequestJob( m_debugger, *m_info, m_engineMutex,
                                        request, debugFlags, this );
}

ExecuteConsoleCommandJob *Debugger::createExecuteConsoleCommandJob( const ConsoleCommand &command )
{
    Q_ASSERT( m_info );
    return new ExecuteConsoleCommandJob( m_debugger, *m_info, m_engineMutex, command, this );
}

EvaluateInContextJob *Debugger::createEvaluateInContextJob( const QString &program,
                                                            const QString &context )
{
    Q_ASSERT( m_info );
    return new EvaluateInContextJob( m_debugger, *m_info, m_engineMutex, program, context, this );
}

CallScriptFunctionJob *Debugger::createCallScriptFunctionJob( const QString &functionName,
        const QVariantList &arguments, DebugFlags debugFlags )
{
    Q_ASSERT( m_info );
    return new CallScriptFunctionJob( m_debugger, *m_info, m_engineMutex, functionName, arguments,
                                      debugFlags, this );
}

TestUsedTimetableInformationsJob *Debugger::createTestUsedTimetableInformationsJob(
        DebugFlags debugFlags )
{
    Q_ASSERT( m_info );
    return new TestUsedTimetableInformationsJob( m_debugger, *m_info, m_engineMutex,
                                                 debugFlags, this );
}

bool Debugger::canEvaluate( const QString &program ) const
{
    QMutexLocker engineLocker( m_engineMutex );
    return m_engine->canEvaluate( program );
}

bool Debugger::requestTimetableData( const RequestInfo *requestInfo, DebugFlags debugFlags )
{
    if ( m_lastScriptError != NoScriptError ) {
        kDebug() << "Script could not be loaded correctly";
        emit requestTimetableDataResult( QSharedPointer< RequestInfo >(requestInfo->clone()),
                false, i18nc("@info", "Script could not be loaded correctly") );
        return false;
    }

    TimetableDataRequestJob *runScriptJob = createTimetableDataRequestJob( requestInfo, debugFlags );
    enqueueJob( runScriptJob );
    return true;
}

void Debugger::connectJob( DebuggerJob *debuggerJob )
{
    const DebuggerJob::Type type = debuggerJob->type();
    switch ( type ) {
    case DebuggerJob::LoadScript:
        connect( debuggerJob, SIGNAL(done(ThreadWeaver::Job*)),
                 this, SLOT(loadScriptJobDone(ThreadWeaver::Job*)) );
        break;
    case DebuggerJob::TimetableDataRequest:
        connect( debuggerJob, SIGNAL(done(ThreadWeaver::Job*)),
                 this, SLOT(timetableDataRequestJobDone(ThreadWeaver::Job*)) );
        break;
    case DebuggerJob::ExecuteConsoleCommand:
        connect( debuggerJob, SIGNAL(done(ThreadWeaver::Job*)),
                 this, SLOT(executeConsoleCommandJobDone(ThreadWeaver::Job*)) );
        break;
    case DebuggerJob::EvaluateInContext:
        connect( debuggerJob, SIGNAL(done(ThreadWeaver::Job*)),
                 this, SLOT(evaluateInContextJobDone(ThreadWeaver::Job*)) );
        break;
    case DebuggerJob::CallScriptFunction:
        connect( debuggerJob, SIGNAL(done(ThreadWeaver::Job*)),
                 this, SLOT(callScriptFunctionJobDone(ThreadWeaver::Job*)) );
        break;
    case DebuggerJob::TestUsedTimetableInformations:
        connect( debuggerJob, SIGNAL(done(ThreadWeaver::Job*)),
                 this, SLOT(testUsedTimetableInformationsJobDone(ThreadWeaver::Job*)) );
        break;
    default:
        kWarning() << "Unknown job type";
        break;
    }
}

bool Debugger::enqueueJob( DebuggerJob *debuggerJob, bool doConnectJob )
{
    if ( doConnectJob ) {
        connectJob( debuggerJob );
    }
    connect( debuggerJob, SIGNAL(started(ThreadWeaver::Job*)),
             this, SLOT(jobStarted(ThreadWeaver::Job*)) );
    // Do not connect to done() because it may have been deleted
    // in another slot connected to that signal (and the slot would not be called)
    connect( debuggerJob, SIGNAL(destroyed(QObject*)),
             this, SLOT(jobDestroyed(QObject*)) );

    // Check job type to decide how to enqueue the new job
    switch ( debuggerJob->type() ) {
    case DebuggerJob::LoadScript:
        // A new version of the script can only be loaded into the engine, when no other job
        // currently accesses the engine (with an old version of the script loaded).
        assignDebuggerQueuePolicy( debuggerJob );
        m_weaver->enqueue( debuggerJob );
        break;
    case DebuggerJob::ExecuteConsoleCommand: {
        // Console commands need different queue policies
        ExecuteConsoleCommandJob *consoleJob =
                qobject_cast< ExecuteConsoleCommandJob* >( debuggerJob );
        if ( consoleJob->command().commandExecutesScriptCode() ) {
            // Console command executes script code, assign evaluate in context restriction policy,
            // ie. allow only one job at a time to run script code in the script context
            // of another job
            assignEvaluateInContextQueuePolicy( debuggerJob );
            runAfterScriptIsLoaded( debuggerJob );
        } else if ( consoleJob->command().command() == ConsoleCommand::BreakpointCommand ) {
            // First load the script in the debugger because it needs to have the code available
            // to check if execution can be interrupted at a given line number
            runAfterScriptIsLoaded( debuggerJob );
        } else {
            // Console command does not execute script code, no queue policies needed
            m_weaver->enqueue( debuggerJob );
        }
    } break;
    case DebuggerJob::EvaluateInContext:
        // Evaluate in context means that script code gets executed in the script context of
        // another job. Use restriction policy to only allow one job at a time
        // to evaluate injected code
        assignEvaluateInContextQueuePolicy( debuggerJob );
        runAfterScriptIsLoaded( debuggerJob );
        break;
    case DebuggerJob::TimetableDataRequest:
    case DebuggerJob::CallScriptFunction:
    case DebuggerJob::TestUsedTimetableInformations:
    default:
        // Other jobs that access the engine and need the script to be loaded
        assignDebuggerQueuePolicy( debuggerJob );
        runAfterScriptIsLoaded( debuggerJob );
        break;
    }
    return true;
}

void Debugger::assignDebuggerQueuePolicy( DebuggerJob *job )
{
    // Assign a resource restriction policy,
    // that ensures that only one job at a time uses the debugger
    if ( !m_debuggerRestrictionPolicy ) {
        m_debuggerRestrictionPolicy = new ThreadWeaver::ResourceRestrictionPolicy( 1 );
    }
    job->assignQueuePolicy( m_debuggerRestrictionPolicy );
}

void Debugger::assignEvaluateInContextQueuePolicy( DebuggerJob *job )
{
    if ( !m_evaluateInContextRestrictionPolicy ) {
        m_evaluateInContextRestrictionPolicy = new ThreadWeaver::ResourceRestrictionPolicy( 1 );
    }
    job->assignQueuePolicy( m_evaluateInContextRestrictionPolicy );
}

void Debugger::runAfterScriptIsLoaded( ThreadWeaver::Job *dependendJob )
{
    if ( m_state != ScriptLoaded ) {
        // If the script is not loaded (still in initializing mode, error or modified)
        // create a job to load it and make the run script job dependend
        if ( !m_loadScriptJob ) {
            m_loadScriptJob = enqueueNewLoadScriptJob();
        }

        // Do not enqueue the dependend job, only add the LoadScriptJob as dependency
        ThreadWeaver::DependencyPolicy::instance().addDependency( dependendJob, m_loadScriptJob );
        m_loadScriptJob->assignQueuePolicy( &ThreadWeaver::DependencyPolicy::instance() );
        dependendJob->assignQueuePolicy( &ThreadWeaver::DependencyPolicy::instance() );
    }

    m_weaver->enqueue( dependendJob );
}

void Debugger::loadScriptJobDone( ThreadWeaver::Job *job )
{
    Q_ASSERT( m_loadScriptJob == job );
    if ( job->success() ) {
        m_state = ScriptLoaded;
        m_lastScriptError = NoScriptError;
        m_lastScriptErrorString.clear();
    } else {
        m_state = ScriptError;
        m_lastScriptError = ScriptLoadFailed;
        m_lastScriptErrorString = m_loadScriptJob->explanation();
    }

    ThreadWeaver::DependencyPolicy::instance().free( m_loadScriptJob );
    delete m_loadScriptJob;
    m_loadScriptJob = 0;

    emit loadScriptResult( m_lastScriptError, m_lastScriptErrorString );
}

void Debugger::timetableDataRequestJobDone( ThreadWeaver::Job *job )
{
    TimetableDataRequestJob *runScriptJob = qobject_cast< TimetableDataRequestJob* >( job );
    Q_ASSERT( runScriptJob );

    emit requestTimetableDataResult( runScriptJob->request(), runScriptJob->success(),
                                     runScriptJob->explanation(), runScriptJob->timetableData(),
                                     runScriptJob->returnValue() );
    kDebug() << "Scipt run done" << runScriptJob->returnValue().toString() << job->success();
    delete runScriptJob;
}

void Debugger::executeConsoleCommandJobDone( ThreadWeaver::Job *job )
{
    ExecuteConsoleCommandJob *consoleCommandJob = qobject_cast< ExecuteConsoleCommandJob* >( job );
    Q_ASSERT( consoleCommandJob );
    emit commandExecutionResult( consoleCommandJob->returnValue(), !consoleCommandJob->success() );
    kDebug() << "Console command done" << consoleCommandJob->returnValue() << job->success();
    delete job;
}

void Debugger::evaluateInContextJobDone( ThreadWeaver::Job *job )
{
    EvaluateInContextJob *evaluateInContextJob = qobject_cast< EvaluateInContextJob* >( job );
    Q_ASSERT( evaluateInContextJob );

    emit evaluationResult( evaluateInContextJob->result() );
    kDebug() << "Evaluate in context done" << evaluateInContextJob->result().returnValue.toString()
             << job->success();
    delete job;
}

void Debugger::callScriptFunctionJobDone( ThreadWeaver::Job *job )
{
    CallScriptFunctionJob *callFunctionJob = qobject_cast< CallScriptFunctionJob* >( job );
    Q_ASSERT( callFunctionJob );

    emit callScriptFunctionResult( callFunctionJob->functionName(), callFunctionJob->returnValue() );
    kDebug() << "Call script function done" << callFunctionJob->functionName()
             << callFunctionJob->returnValue().toString() << job->success();
    delete job;
}

void Debugger::testUsedTimetableInformationsJobDone( ThreadWeaver::Job *job )
{
    TestUsedTimetableInformationsJob *testJob =
            qobject_cast< TestUsedTimetableInformationsJob* >( job );
    Q_ASSERT( testJob );

    emit testUsedTimetableInformationsResult( testJob->timetableInformations() );
    kDebug() << "Call script function done" << testJob->functionName()
             << testJob->returnValue().toString() << job->success();
    delete job;
}

}; // namespace Debugger
