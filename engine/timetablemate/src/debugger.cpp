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

namespace Debugger {

Debugger::Debugger( QObject *parent )
        : QObject( parent ), m_state(Initializing), m_weaver(new ThreadWeaver::Weaver(this)),
          m_loadScriptJob(0), m_engineMutex(new QMutex()),
          m_engine(0), m_script(0), m_debugger(0), m_info(0), m_scriptNetwork(0),
          m_scriptHelper(0), m_scriptResult(0), m_scriptStorage(0)
{
    // Maximally two parallel threads, ie. one thread processing the script and may be interrupted
    // while in another thread a console command gets executed or something gets evaluated in
    // the current script context
    m_weaver->setMaximumNumberOfThreads( 2 );

    m_lastScriptError = NoScriptError;
    qRegisterMetaType<QScriptContextInfo>( "QScriptContextInfo" );
    qRegisterMetaType<EvaluationResult>( "EvaluationResult" );
    qRegisterMetaType<Frame>( "Frame" );
    qRegisterMetaType<FrameStack>( "FrameStack" );
    qRegisterMetaType<Variable>( "Variable" );
    qRegisterMetaType<Variables>( "Variables" );
    qRegisterMetaType<Breakpoint>( "Breakpoint" );
    qRegisterMetaType<ConsoleCommand>( "ConsoleCommand" );
    qRegisterMetaType<BacktraceChange>( "BacktraceChange" );

    // Create script engine
    m_engine = new QScriptEngine();
    m_engine->setProcessEventsInterval( 100 );

    // Create debugger agent and forward signals
    m_debugger = new DebuggerAgent( m_engine, m_engineMutex );
    connect( m_debugger, SIGNAL(positionChanged(int,int,int,int)),
             this, SIGNAL(positionChanged(int,int,int,int)) );
    connect( m_debugger, SIGNAL(interrupted()), this, SIGNAL(interrupted()) );
    connect( m_debugger, SIGNAL(continued()), this, SIGNAL(continued()) );
    connect( m_debugger, SIGNAL(started()), this, SIGNAL(started()) );
    connect( m_debugger, SIGNAL(stopped()), this, SIGNAL(stopped()) );
    connect( m_debugger, SIGNAL(exception(int,QString)),
             this, SIGNAL(exception(int,QString)) );
    connect( m_debugger, SIGNAL(backtraceChanged(FrameStack,BacktraceChange)),
             this, SIGNAL(backtraceChanged(FrameStack,BacktraceChange)) );
    connect( m_debugger, SIGNAL(breakpointReached(Breakpoint)),
             this, SIGNAL(breakpointReached(Breakpoint)) );
    connect( m_debugger, SIGNAL(breakpointAdded(Breakpoint)),
             this, SIGNAL(breakpointAdded(Breakpoint)) );
    connect( m_debugger, SIGNAL(breakpointRemoved(Breakpoint)),
             this, SIGNAL(breakpointRemoved(Breakpoint)) );
    connect( m_debugger, SIGNAL(output(QString,QScriptContextInfo)),
             this, SIGNAL(output(QString,QScriptContextInfo)) );

    // Attach the debugger
    m_engine->setAgent( m_debugger );

    // Interrupt at first statement
    m_debugger->setExecutionControlType( ExecuteInterrupt );

    ThreadWeaver::setDebugLevel( true, 1 );
}

Debugger::~Debugger()
{
    m_debugger->abortDebugger();
    m_weaver->requestAbort();
    delete m_engineMutex;
}

void Debugger::loadScriptObjects( const TimetableAccessorInfo *info )
{
    QMutexLocker engineLocker( m_engineMutex );

    // Register NetworkRequest class for use in the script
    qScriptRegisterMetaType< NetworkRequestPtr >( m_engine,
            networkRequestToScript, networkRequestFromScript );

    // Create objects for the script
    if ( !m_scriptHelper ) {
        m_scriptHelper = new Helper( info->serviceProvider(), m_engine );
//         connect( m_scriptHelper, SIGNAL(errorReceived(QString,QString)),
//                  this, SLOT(scriptErrorReceived(QString,QString)) );
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

    // Expose objects to the script engine
    if ( !m_engine->globalObject().property("accessor").isValid() ) {
        m_engine->globalObject().setProperty( "accessor", m_engine->newQObject(
                const_cast<TimetableAccessorInfo*>(info)) ); // info has only readonly properties
    }
    if ( !m_engine->globalObject().property("helper").isValid() ) {
        m_engine->globalObject().setProperty( "helper", m_engine->newQObject(m_scriptHelper) );
    }
    if ( !m_engine->globalObject().property("network").isValid() ) {
        m_engine->globalObject().setProperty( "network", m_engine->newQObject(m_scriptNetwork) );
    }
    if ( !m_engine->globalObject().property("storage").isValid() ) {
        m_engine->globalObject().setProperty( "storage", m_engine->newQObject(m_scriptStorage) );
    }
    if ( !m_engine->globalObject().property("result").isValid() ) {
        m_engine->globalObject().setProperty( "result", m_engine->newQObject(m_scriptResult) );
    }
    if ( !m_engine->globalObject().property("enum").isValid() ) {
        m_engine->globalObject().setProperty( "enum",
                m_engine->newQMetaObject(&ResultObject::staticMetaObject) );
    }

    // Import extensions (from XML file, <script extensions="...">)
    foreach ( const QString &extension, info->scriptExtensions() ) {
        if ( !importExtension(m_engine, extension) ) {
            m_lastScriptError = ScriptLoadFailed;
            return;
        }
    }
}

void Debugger::executeCommand( const ConsoleCommand &command )
{
    kDebug() << "executeCommand(...)";
    kDebug() << "DebugCommand?" << (command.command() == ConsoleCommand::DebugCommand) << m_debugger->isInterrupted();
    if ( command.command() == ConsoleCommand::DebugCommand ) {
        if ( m_debugger->isInterrupted() ) {
            emit commandExecutionResult( "Debugger is already running!", true );
        } else {
            kDebug() << "THREADWEAVER TODO";
            ExecuteConsoleCommandJob *consoleCommandJob = createExecuteConsoleCommandJob( command ); // TODO QScriptProgram as argument to createLoadScriptJob()?
            connect( consoleCommandJob, SIGNAL(done(ThreadWeaver::Job*)),
                     this, SLOT(executeConsoleCommandJobDone(ThreadWeaver::Job*)) );
            m_weaver->enqueue( consoleCommandJob );
        }
    } else {
        kDebug() << "THREADWEAVER";
        ExecuteConsoleCommandJob *consoleCommandJob = createExecuteConsoleCommandJob( command ); // TODO QScriptProgram as argument to createLoadScriptJob()?
        connect( consoleCommandJob, SIGNAL(done(ThreadWeaver::Job*)),
                 this, SLOT(executeConsoleCommandJobDone(ThreadWeaver::Job*)) );
        m_weaver->enqueue( consoleCommandJob );
    }

//     TODO Wake from interrupt done in jobs?
}

void Debugger::evaluateInContext( const QString &program, const QString &contextName ) // TODO , bool interruptAtStart )
{
    kDebug() << "evaluateInContext(...)";
    EvaluateInContextJob *evaluateInContextJob = createEvaluateInContextJob( program, contextName );
    connect( evaluateInContextJob, SIGNAL(done(ThreadWeaver::Job*)),
             this, SLOT(evaluateInContextJobDone(ThreadWeaver::Job*)) );
    runAfterScriptIsLoaded( evaluateInContextJob );
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
    m_loadScriptJob = createLoadScriptJob(); // TODO QScriptProgram as argument to createLoadScriptJob()?
    LoadScriptJob *loadScriptJob = m_loadScriptJob;

    loadScriptObjects( info );

    connect( loadScriptJob, SIGNAL(done(ThreadWeaver::Job*)),
             this, SLOT(loadScriptJobDone(ThreadWeaver::Job*)) );
    m_weaver->enqueue( loadScriptJob );
}

LoadScriptJob *Debugger::createLoadScriptJob()
{
    return new LoadScriptJob( m_debugger, *m_info, m_engineMutex, m_script, this );
}

ProcessTimetableDataRequestJob *Debugger::createProcessTimetableDataRequestJob(
        const RequestInfo *request, DebugMode debugMode )
{
    return new ProcessTimetableDataRequestJob( m_debugger, *m_info, m_engineMutex, request,
                                               debugMode, this );
}

ExecuteConsoleCommandJob *Debugger::createExecuteConsoleCommandJob( const ConsoleCommand &command )
{
    return new ExecuteConsoleCommandJob( m_debugger, *m_info, m_engineMutex, command, this );
}

EvaluateInContextJob *Debugger::createEvaluateInContextJob( const QString &program,
                                                                   const QString &context )
{
    return new EvaluateInContextJob( m_debugger, *m_info, m_engineMutex, program, context, this );
}

bool Debugger::callFunction( const QString &functionToRun, const RequestInfo *requestInfo )
{
    if ( m_lastScriptError != NoScriptError ) {
        kDebug() << "Script could not be loaded correctly";
        return false;
    }
    if ( m_debugger->isRunning() ) {
        kDebug() << "Already executing script";
        return false;
    }

//     TODO
    ProcessTimetableDataRequestJob *runScriptJob = createProcessTimetableDataRequestJob(
            requestInfo, InterruptAtStart );
    connect( runScriptJob, SIGNAL(done(ThreadWeaver::Job*)),
             this, SLOT(processTimetableDataRequestJobDone(ThreadWeaver::Job*)) );
    runAfterScriptIsLoaded( runScriptJob );
    return true;
}

void Debugger::runAfterScriptIsLoaded( ThreadWeaver::Job *dependendJob )
{
    if ( m_state != ScriptLoaded ) {
        // If the script is not loaded (still in initializing mode, error or modified)
        // create a job to load it and make the run script job dependend
        if ( !m_loadScriptJob ) {
            kDebug() << "XXX Script not loaded, create LoadScriptJob";
            ThreadWeaver::debug( 0, "Script not loaded, create LoadScriptJob\n" );
            m_loadScriptJob = createLoadScriptJob();
            connect( m_loadScriptJob, SIGNAL(done(ThreadWeaver::Job*)),
                     this, SLOT(loadScriptJobDone(ThreadWeaver::Job*)) );
            m_loadScriptJob->assignQueuePolicy( &ThreadWeaver::DependencyPolicy::instance() );
            m_weaver->enqueue( m_loadScriptJob );
        }
//         TEST
        // Do not enqueue the dependend job, only add the LoadScriptJob as dependency
        kDebug() << "Script not loaded, add dependency";
//         TODO THIS DOES NOT WORK...
        ThreadWeaver::debug( 0, "Script not loaded, run job after LoadScriptJob has finished\n" );

//         m_loadScriptJob->assignQueuePolicy( &ThreadWeaver::DependencyPolicy::instance() );
        dependendJob->assignQueuePolicy( &ThreadWeaver::DependencyPolicy::instance() );

        ThreadWeaver::DependencyPolicy::instance().addDependency( dependendJob, m_loadScriptJob );
        kDebug() << "TEST" << ThreadWeaver::DependencyPolicy::instance().getDependencies( m_loadScriptJob ).count();

        m_weaver->enqueue( dependendJob );

//         ThreadWeaver::JobSequence *jobSequence = new ThreadWeaver::JobSequence( this );
//         jobSequence->addJob( m_loadScriptJob );
//         jobSequence->addJob( dependendJob );
//         connect( jobSequence, SIGNAL(done(ThreadWeaver::Job*)),
//                  this, SLOT(jobSequenceDone(ThreadWeaver::Job*)));
//         m_weaver->enqueue( jobSequence );
    } else {
        // Script is already loaded, just enqueue the job
        m_weaver->enqueue( dependendJob );
    }
}

void Debugger::jobSequenceDone( ThreadWeaver::Job *jobSequence )
{
    ThreadWeaver::debug( 0, "A job sequence is done (LoadScriptJob, X)\n" );
    delete jobSequence;
}

void Debugger::loadScriptJobDone( ThreadWeaver::Job *job )
{
    Q_ASSERT( m_loadScriptJob == job );
    m_state = job->success() ? ScriptLoaded : ScriptError;
    kDebug() << "TEST\n\n----------------------------------------------------\n";
    ThreadWeaver::DependencyPolicy::instance().dumpJobDependencies();
    ThreadWeaver::DependencyPolicy::instance().free( m_loadScriptJob );
    ThreadWeaver::DependencyPolicy::instance().dumpJobDependencies();
    kDebug() << "TEST" << ThreadWeaver::DependencyPolicy::instance().getDependencies( m_loadScriptJob ).count();
    m_loadScriptJob = 0;
    kDebug() << "Scipt loaded" << job->success();
    ThreadWeaver::debug( 0, QString("Script loaded: %1\n").arg(job->success() ? "Success" : "Failed").toUtf8() );
//     delete job; TODO
}

void Debugger::processTimetableDataRequestJobDone( ThreadWeaver::Job *job )
{
    ProcessTimetableDataRequestJob *runScriptJob = qobject_cast< ProcessTimetableDataRequestJob* >( job );
    Q_ASSERT( runScriptJob );

    emit functionCallResult( runScriptJob->timetableData(), runScriptJob->returnValue() );
    kDebug() << "Scipt run done" << runScriptJob->returnValue().toString() << job->success();
    delete job;
}

void Debugger::executeConsoleCommandJobDone( ThreadWeaver::Job *job )
{
    ExecuteConsoleCommandJob *consoleCommandJob = qobject_cast< ExecuteConsoleCommandJob* >( job );
    Q_ASSERT( consoleCommandJob );
//     QString returnValue;
//     bool error = !m_debugger->executeCommand( command, &returnValue );
    emit commandExecutionResult( consoleCommandJob->returnValue(), !consoleCommandJob->success() );
    kDebug() << "Console command done" << consoleCommandJob->returnValue() << job->success();
    delete job;
}

void Debugger::evaluateInContextJobDone( ThreadWeaver::Job *job )
{
    EvaluateInContextJob *evaluateInContextJob = qobject_cast< EvaluateInContextJob* >( job );
    Q_ASSERT( evaluateInContextJob );

    emit evaluationResult( evaluateInContextJob->result() );
    kDebug() << "Evaluate in context done" << evaluateInContextJob->result().returnValue.toString() << job->success();
    delete job;
}

}; // namespace Debugger
