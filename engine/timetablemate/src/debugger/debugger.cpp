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
#include <engine/serviceproviderdata.h>
#include <engine/request.h>

// KDE includes
#include <KDebug>
#include <KLocalizedString>
#include <ThreadWeaver/Weaver>
#include <ThreadWeaver/WeaverInterface>
#include <ThreadWeaver/DependencyPolicy>
#include <ThreadWeaver/ResourceRestrictionPolicy>
#include <ThreadWeaver/Thread>
#include <KGlobal>
#include <KLocale>
#include <threadweaver/DebuggingAids.h>

// Qt includes
#include <QScriptContextInfo>
#include <QTimer>
#include <QEventLoop>
#include <QApplication>

namespace Debugger {

Debugger::Debugger( const WeaverInterfacePointer &weaver, QObject *parent )
        : QObject(parent), m_state(Initializing), m_weaver(weaver),
          m_mutex(new QMutex(QMutex::Recursive)),
          m_loadScriptJob(0), m_lastScriptError(InitializingScript),
          m_variableModel(new VariableModel(this)),
          m_backtraceModel(new BacktraceModel(this)),
          m_breakpointModel(new BreakpointModel(this)),
          m_debuggerRestrictionPolicy(0), m_evaluateInContextRestrictionPolicy(0),
          m_running(false), m_runData(0), m_timeout(0)
{
    initialize();
}

Debugger::Debugger( QObject *parent )
        : QObject(parent), m_state(Initializing), m_weaver(new ThreadWeaver::Weaver()),//ThreadWeaver::Weaver::instance()),
          m_mutex(new QMutex(QMutex::Recursive)),
          m_loadScriptJob(0), m_lastScriptError(InitializingScript),
          m_variableModel(new VariableModel(this)),
          m_backtraceModel(new BacktraceModel(this)),
          m_breakpointModel(new BreakpointModel(this)),
          m_debuggerRestrictionPolicy(0), m_evaluateInContextRestrictionPolicy(0),
          m_running(false), m_runData(0), m_timeout(0)
{
    initialize();
}

void Debugger::initialize()
{
    QMutexLocker locker( m_mutex );
    qRegisterMetaType< QScriptContextInfo >( "QScriptContextInfo" );
    qRegisterMetaType< EvaluationResult >( "EvaluationResult" );
    qRegisterMetaType< Frame >( "Frame" );
    qRegisterMetaType< FrameStack >( "FrameStack" );
    qRegisterMetaType< Breakpoint >( "Breakpoint" );
    qRegisterMetaType< ConsoleCommand >( "ConsoleCommand" );
    qRegisterMetaType< DebuggerState >( "DebuggerState" );
    qRegisterMetaType< ScriptStoppedFlags >( "ScriptStoppedFlags" );
    qRegisterMetaType< NetworkRequest* >( "NetworkRequest*" );
    qRegisterMetaType< NetworkRequest::Ptr >( "NetworkRequest::Ptr" );
    qRegisterMetaType< QIODevice* >( "QIODevice*" );
    qRegisterMetaType< DataStreamPrototype* >( "DataStreamPrototype*" );

    connect( m_breakpointModel, SIGNAL(breakpointAdded(Breakpoint)),
             this, SIGNAL(breakpointAdded(Breakpoint)) );
    connect( m_breakpointModel, SIGNAL(breakpointAboutToBeRemoved(Breakpoint)),
             this, SIGNAL(breakpointAboutToBeRemoved(Breakpoint)) );
}

Debugger::~Debugger()
{
    // Abort all running jobs, wait for them to finish and then delete them.
    // When there are undeleted jobs with queue policies assigned, deleting m_weaver would crash
    m_mutex->lock();
    if ( hasRunningJobs() ) {
        m_weaver->requestAbort();
        m_weaver->finish();
        qApp->processEvents();
    }
    foreach ( const QPointer<DebuggerJob> &job, m_runningJobs ) {
        if ( job ) {
            job->finish();
            if ( job ) {
                QEventLoop loop;
                connect( job, SIGNAL(destroyed(QObject*)), &loop, SLOT(quit()) );
                loop.exec();
                delete job.data();
            }
        }
    }
    m_runningJobs.clear();

    delete m_debuggerRestrictionPolicy;
    delete m_evaluateInContextRestrictionPolicy;
    delete m_runData;
    m_runData = 0;
    m_mutex->unlock();

    delete m_mutex;
}

void Debugger::finish()
{
    QMutexLocker locker( m_mutex );
    if ( hasRunningJobs() ) {
        m_weaver->finish();
        qApp->processEvents();
    }
    while ( hasRunningJobs() ) {
        const QPointer< DebuggerJob > job = currentJob();
        Q_ASSERT( job );
        job->finish();

        if ( job && m_runningJobs.contains(job) ) {
            QEventLoop loop;
            connect( job, SIGNAL(destroyed(QObject*)), &loop, SLOT(quit()) );
            loop.exec();

            if ( m_runningJobs.contains(job) ) {
                kDebug() << "Manually remove done job";
                m_runningJobs.remove( m_runningJobs.indexOf(job) );
            }
        }
    }
    qApp->processEvents();
}

void Debugger::slotStarted( const QDateTime &timestamp )
{
    stopTimeout();
    QMutexLocker locker( m_mutex );
    if ( !m_runData ) {
        kWarning() << "ScriptRunData already deleted / not yet created";
        return;
    }
    DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_runData->job(), "Debugger::slotStarted(): Execution started/continued" );

    if ( m_runData->isWaitingForSignal() ) {
        // Was waiting for a signal, script execution continues
        DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_runData->job(), "Debugger::slotStarted(): Woke up from signal" );
        const int waitingTime = m_runData->wokeUpFromSignal( timestamp );
        emit wokeUpFromSignal( waitingTime );
    } else if ( m_runData->isScriptLoaded() || !m_runData->isExecuting() ) {
        // The current job started executing the script
        DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_runData->job(), "Debugger::slotStarted(): Start execution timer" );
        m_runData->executionStarted( timestamp );
    } else {
        DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_runData->job(), "Debugger::slotStarted(): Is already executing and not waiting for a signal" );
    }
}

void Debugger::slotStopped( const QDateTime &timestamp, ScriptStoppedFlags flags,
                            int uncaughtExceptionLineNumber, const QString &uncaughtException )
{
    Q_UNUSED( uncaughtExceptionLineNumber );
    Q_UNUSED( uncaughtException );
    QMutexLocker locker( m_mutex );
    if ( !m_runData ) {
        kWarning() << "ScriptRunData already deleted";
        return;
    }
    DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_runData->job(), "Debugger::slotStopped(): Execution stopped" );
    Q_ASSERT( m_runData );
    if ( flags.testFlag(ScriptWasAborted) ) {
        DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_runData->job(), "Debugger::slotStopped(): "
                                   "Execution was aborted" );
        m_runData->executionStopped( timestamp );
    } else if ( flags.testFlag(ScriptHasRunningRequests) ) {
        // Script execution gets suspended, waiting for a signal
        DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_runData->job(), "Debugger::slotStopped(): "
                                   "Script execution gets suspended, waiting for a signal" );
        startTimeout();
        m_runData->waitingForSignal( timestamp );
        emit waitingForSignal();
        return;
    } else {
        DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_runData->job(), "Debugger::slotStopped(): "
                                   "No running requests, job should get stopped now" );
        m_runData->executionStopped( timestamp );
    }

    Q_ASSERT( m_runData->job() );
    m_runData->job()->setJobDone();
}

void Debugger::slotAborted()
{
    QMutexLocker locker( m_mutex );
    foreach ( const QPointer<DebuggerJob> &job, m_runningJobs ) {
        job->requestAbort();
    }

    stopTimeout();
    emit aborted();
}

void Debugger::slotInterrupted( int lineNumber, const QString &fileName,
                                const QDateTime &timestamp )
{
    QMutexLocker locker( m_mutex );
    if ( !m_runData ) {
        kWarning() << "No ScriptRunData available";
        return;
    }
    m_runData->interrupted( timestamp );

    emit interrupted( lineNumber, fileName, timestamp );
}

void Debugger::slotContinued( const QDateTime &timestamp, bool willInterruptAfterNextStatement )
{
    QMutexLocker locker( m_mutex );
    if ( !m_runData ) {
        kWarning() << "No ScriptRunData available";
        return;
    }
    m_runData->continued( timestamp );

    emit continued( timestamp, willInterruptAfterNextStatement );
}

void Debugger::slotJobStarted( ThreadWeaver::Job *job )
{
    DebuggerJob *debuggerJob = qobject_cast< DebuggerJob* >( job );
    Q_ASSERT( debuggerJob );
    m_mutex->lock();
    m_runningJobs.push( QPointer<DebuggerJob>(debuggerJob) );
    DEBUGGER_JOB_SYNCHRONIZATION_JOB( debuggerJob, QString("Debugger::slotJobStarted(): Job started, now %1 jobs running")
                                            .arg(m_runningJobs.count()) );
    m_mutex->unlock();
    ExecuteConsoleCommandJob *consoleCommandJob = qobject_cast< ExecuteConsoleCommandJob* >( job );
    EvaluateInContextJob *evaluateInContextJob = qobject_cast< EvaluateInContextJob* >( job );
    emit jobStarted( debuggerJob->type(), debuggerJob->useCase(), debuggerJob->objectName() );

    stopTimeout();
    if ( !evaluateInContextJob && !consoleCommandJob ) {
        // Script execution gets started
        DEBUGGER_JOB_SYNCHRONIZATION_JOB( debuggerJob, "Debugger::slotJobStarted(): Start script execution, was not running" );

        m_mutex->lock();
        m_running = true;
        m_runData = new ScriptRunData( debuggerJob );
        m_mutex->unlock();

        emit started();
    }
}

void Debugger::slotJobDone( ThreadWeaver::Job *job )
{
    DebuggerJob *debuggerJob = qobject_cast< DebuggerJob* >( job );
    Q_ASSERT( debuggerJob );
    if ( !hasRunningJobs() ) {
        kWarning() << "Job done signal received, but no running jobs, exiting?";
        job->deleteLater();
        return;
    }

    m_mutex->lock();
    if ( debuggerJob != m_runningJobs.top() ) {
        kWarning() << "Unknown job done" << debuggerJob;
        kDebug() << "Current job is" << m_runningJobs.top();
        m_mutex->unlock();
        return;
    }
    m_runningJobs.pop();

    DEBUGGER_JOB_SYNCHRONIZATION_JOB( debuggerJob, QString("Debugger::slotJobDone(): Job done, now %1 running")
                                            .arg(m_runningJobs.count()) );
    ExecuteConsoleCommandJob *consoleCommandJob = qobject_cast< ExecuteConsoleCommandJob* >( job );
    EvaluateInContextJob *evaluateInContextJob = qobject_cast< EvaluateInContextJob* >( job );

    if ( job == m_loadScriptJob ) {
        DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_loadScriptJob, "LoadScriptJob is done" );
        m_loadScriptJob = 0;
    }
    m_mutex->unlock();
    stopTimeout();

    switch ( debuggerJob->type() ) {
    case LoadScript:
        loadScriptJobDone( debuggerJob );
        break;
    case TimetableDataRequest:
        timetableDataRequestJobDone( debuggerJob );
        break;
    case ExecuteConsoleCommand:
        executeConsoleCommandJobDone( debuggerJob );
        break;
    case EvaluateInContext:
        evaluateInContextJobDone( debuggerJob );
        break;
    case CallScriptFunction:
        callScriptFunctionJobDone( debuggerJob );
        break;
    case TestFeatures:
        testFeaturesJobDone( debuggerJob );
        break;
    default:
        kWarning() << "Unknown job type" << debuggerJob->type();
        break;
    }

    DebuggerJobResult result( debuggerJob->success(), debuggerJob->wasAborted(),
                              debuggerJob->returnValue(), debuggerJob->explanation() );
    CallScriptFunctionJob *callFunctionJob = qobject_cast< CallScriptFunctionJob* >( job );
    TimetableDataRequestJob *requestJob = qobject_cast< TimetableDataRequestJob* >( job );
    if ( callFunctionJob ) {
        result.messages = callFunctionJob->additionalMessages();

        if ( requestJob ) {
            result.request = requestJob->request();
            result.resultData = requestJob->timetableData();
        } else {
            TestFeaturesJob *featuresJob = qobject_cast< TestFeaturesJob* >( job );
            if ( featuresJob ) {
                TimetableData featuresData;
                QVariantList features;
                foreach ( Enums::ProviderFeature feature, featuresJob->features() ) {
                    features << feature;
                }

                // Insert as "Nothing", it's no TimetableInformation normally used by the engine
                featuresData.insert( Enums::Nothing, features );
                result.resultData << featuresData;
            }
        }
    }

    // To not crash with queued connections, no pointer to the job gets emitted here,
    // as it may already be deleted when the signal gets received, copy all data
    emit jobDone( debuggerJob->type(), debuggerJob->useCase(), debuggerJob->objectName(), result );

    if ( !evaluateInContextJob && !consoleCommandJob ) {
        // Script execution has finished
        stopTimeout();

        m_mutex->lock();
        m_running = false;
        if ( !m_runData ) {
            kWarning() << "ScriptRunData already deleted";
            m_mutex->unlock();

            emit stopped( ScriptRunData() );
        } else {
            if ( m_runData->isExecuting() ) {
                DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_runData->job(), "Debugger::slotJobDone(): Stopped() "
                                           "signal not correctly received for job" );
                m_runData->executionStopped( QDateTime::currentDateTime() );
            }
            const ScriptRunData runData = *m_runData;
            delete m_runData;
            m_runData = 0;
            m_mutex->unlock();

            emit stopped( runData );
        }
    }

    job->deleteLater();
}

void Debugger::startTimeout( int milliseconds )
{
    QMutexLocker locker( m_mutex );
    if ( !m_timeout ) {
        m_timeout = new QTimer( this );
        connect( m_timeout, SIGNAL(timeout()), this, SLOT(timeout()) );
    }
    m_timeout->start( milliseconds );
}

void Debugger::stopTimeout()
{
    QMutexLocker locker( m_mutex );
    if ( m_timeout ) {
        m_timeout->deleteLater();
        m_timeout = 0;
    }
}

void Debugger::timeout()
{
    QMutexLocker locker( m_mutex );
    if ( m_timeout ) {
        kDebug() << "Timeout, execution took longer than" << m_timeout->interval() << "ms";
        emit errorMessage( i18nc("@info/plain", "Execution timed out after %1",
                                 KGlobal::locale()->prettyFormatDuration(m_timeout->interval())) );
        m_timeout->deleteLater();
        m_timeout = 0;
    } else {
        kDebug() << "Timeout, execution took too long";
        emit errorMessage( i18nc("@info/plain", "Execution timed out") );
    }

    abortDebugger();
}

void Debugger::removeAllBreakpoints()
{
    QMutexLocker locker( m_mutex );
    m_breakpointModel->clear();
}

void Debugger::executeCommand( const ConsoleCommand &command )
{
    if ( command.command() == ConsoleCommand::ClearCommand ) {
        kWarning() << "The clear command is not implemented in Debugger/DebuggerAgent, "
                      "should be implemented in the console to clear it's history";
    } else {
        enqueueJob( createExecuteConsoleCommandJob(command) );
    }
}

void Debugger::evaluateInContext( const QString &program, const QString &contextName )
{
    enqueueJob( createEvaluateInContextJob(program, contextName) );
}

LoadScriptJob *Debugger::createLoadScriptJob( DebugFlags debugFlags )
{
    QMutexLocker locker( m_mutex );
    return new LoadScriptJob( m_data, QString(), debugFlags, this );
}

bool Debugger::isLoadScriptJobRunning() const
{
    QMutexLocker locker( m_mutex );
    return m_loadScriptJob && !m_loadScriptJob->isFinished();
}

LoadScriptJob *Debugger::getLoadScriptJob( const QString &program, const ServiceProviderData *data,
                                           DebugFlags debugFlags )
{
    QMutexLocker locker( m_mutex );
    if ( isLoadScriptJobRunning() ) {
        // Script already gets loaded, return the running job
        return m_loadScriptJob;
    }

    if ( m_lastScriptError == NoScriptError &&
         m_data.program && m_data.program->sourceCode() == program &&
         (!m_data.provider.isValid() || m_data.provider == *data) )
    {
        // Script code and provider data unchanged
        return 0;
    }

    // The script was modified or not loaded before
    m_state = ScriptModified;
    m_data = ScriptData( data, QSharedPointer< QScriptProgram >(
                new QScriptProgram(program, data->scriptFileName())) );
    m_lastScriptError = InitializingScript;
    return createLoadScriptJob( debugFlags );
}

LoadScriptJob *Debugger::loadScript( const QString &program, const ServiceProviderData *data,
                                     DebugFlags debugFlags )
{
    if ( isLoadScriptJobRunning() ) {
        // Script already gets loaded, return the running job
        QMutexLocker locker( m_mutex );
        return m_loadScriptJob;
    }

    LoadScriptJob *loadScriptJob = getLoadScriptJob( program, data, debugFlags );
    if ( !loadScriptJob ) {
        return 0;
    }

    enqueueJob( loadScriptJob );
    return loadScriptJob;
}

TimetableDataRequestJob *Debugger::createTimetableDataRequestJob(
        const AbstractRequest *request, const QString &useCase, DebugFlags debugFlags )
{
    QMutexLocker locker( m_mutex );
    return new TimetableDataRequestJob( m_data, request, useCase, debugFlags, this );
}

ExecuteConsoleCommandJob *Debugger::createExecuteConsoleCommandJob( const ConsoleCommand &command,
                                                                    const QString &useCase )
{
    QMutexLocker locker( m_mutex );
    return new ExecuteConsoleCommandJob( m_data, command, useCase,
                                         hasRunningJobs() ? currentJob() : 0, this );
}

EvaluateInContextJob *Debugger::createEvaluateInContextJob( const QString &program,
                                                            const QString &context,
                                                            const QString &useCase )
{
    QMutexLocker locker( m_mutex );
    return new EvaluateInContextJob( m_data, program, context, useCase,
                                     hasRunningJobs() ? currentJob() : 0, this );
}

CallScriptFunctionJob *Debugger::createCallScriptFunctionJob( const QString &functionName,
        const QVariantList &arguments, const QString &useCase, DebugFlags debugFlags )
{
    QMutexLocker locker( m_mutex );
    return new CallScriptFunctionJob( m_data, functionName, arguments, useCase, debugFlags, this );
}

TestFeaturesJob *Debugger::createTestFeaturesJob( const QString &useCase, DebugFlags debugFlags )
{
    QMutexLocker locker( m_mutex );
    return new TestFeaturesJob( m_data, useCase, debugFlags, this );
}

bool Debugger::canEvaluate( const QString &program ) const
{
    if ( !hasRunningJobs() ) {
        kWarning() << "Not running";
        return false;
    }

    return currentJob()->canEvaluate( program );
}

bool Debugger::requestTimetableData( const AbstractRequest *request, const QString &useCase,
                                     DebugFlags debugFlags )
{
    QMutexLocker locker( m_mutex );
    if ( m_lastScriptError != NoScriptError && m_lastScriptError != InitializingScript ) {
        kWarning() << "Script could not be loaded correctly";
        emit requestTimetableDataResult( QSharedPointer< AbstractRequest >(request->clone()),
                false, i18nc("@info", "Script could not be loaded correctly") );
        return false;
    }

    TimetableDataRequestJob *runScriptJob =
            createTimetableDataRequestJob( request, useCase, debugFlags );
    enqueueJob( runScriptJob );
    return true;
}

void Debugger::connectJob( DebuggerJob *debuggerJob )
{
    connect( debuggerJob, SIGNAL(started(ThreadWeaver::Job*)),
             this, SLOT(slotJobStarted(ThreadWeaver::Job*)) );
    connect( debuggerJob, SIGNAL(done(ThreadWeaver::Job*)),
             this, SLOT(slotJobDone(ThreadWeaver::Job*)) );

    switch ( debuggerJob->type() ) {
    case TimetableDataRequest:
    case CallScriptFunction:
    case TestFeatures:
        connect( qobject_cast<CallScriptFunctionJob*>(debuggerJob), SIGNAL(asynchronousRequestWaitFinished(QDateTime,int,int)),
                 this, SLOT(asynchronousRequestWaitFinished(QDateTime,int,int)) );
        connect( qobject_cast<CallScriptFunctionJob*>(debuggerJob), SIGNAL(synchronousRequestWaitFinished(int,int,int)),
                 this, SLOT(synchronousRequestWaitFinished(int,int,int)) );
        break;
    default:
        break;
    }

    // Connect signals of the job, the job lives in the same thread as this debugger
    // (the GUI thread), the connections are direct. The job itself emits these signals after
    // receiving the associated signals of the used DebuggerAgent instance using queued connections.
    connect( debuggerJob, SIGNAL(positionChanged(int,int,int,int)),
             this, SIGNAL(positionChanged(int,int,int,int)) );
    connect( debuggerJob, SIGNAL(stateChanged(DebuggerState,DebuggerState)),
             this, SIGNAL(stateChanged(DebuggerState,DebuggerState)) );
    connect( debuggerJob, SIGNAL(aborted()), this, SLOT(slotAborted()) );
    connect( debuggerJob, SIGNAL(interrupted(int,QString,QDateTime)),
             this, SLOT(slotInterrupted(int,QString,QDateTime)) );
    connect( debuggerJob, SIGNAL(continued(QDateTime,bool)), this, SLOT(slotContinued(QDateTime,bool)) );
    connect( debuggerJob, SIGNAL(started(QDateTime)), this, SLOT(slotStarted(QDateTime)) );
    connect( debuggerJob, SIGNAL(stopped(QDateTime,ScriptStoppedFlags,int,QString)),
             this, SLOT(slotStopped(QDateTime,ScriptStoppedFlags,int,QString)) );
    connect( debuggerJob, SIGNAL(exception(int,QString,QString)),
             this, SIGNAL(exception(int,QString,QString)) );
    connect( debuggerJob, SIGNAL(output(QString,QScriptContextInfo)),
             this, SIGNAL(output(QString,QScriptContextInfo)) );
    connect( debuggerJob, SIGNAL(informationMessage(QString)),
             this, SIGNAL(informationMessage(QString)) );
    connect( debuggerJob, SIGNAL(errorMessage(QString)),
             this, SIGNAL(errorMessage(QString)) );
    connect( debuggerJob, SIGNAL(breakpointReached(Breakpoint)),
             this, SIGNAL(breakpointReached(Breakpoint)) );
    connect( debuggerJob, SIGNAL(scriptMessageReceived(QString,QScriptContextInfo,QString,Helper::ErrorSeverity)),
             this, SIGNAL(scriptMessageReceived(QString,QScriptContextInfo,QString,Helper::ErrorSeverity)) );

    debuggerJob->attachDebugger( this );
}

bool Debugger::enqueueJob( DebuggerJob *debuggerJob )
{
    QMutexLocker locker( m_mutex );
    if ( m_loadScriptJob == debuggerJob ) {
        kDebug() << "Gets loaded, wait...";
        return true;
    }

    connectJob( debuggerJob );

    // Mark job as not done, wait until the stopped() signal of the job was processed
    // in slotStopped() before marking it as done, allowing it to destroy the agent/engine then
    debuggerJob->setJobDone( false );

    // Check job type to decide how to enqueue the new job
    switch ( debuggerJob->type() ) {
    case LoadScript:
        // Allow only one LoadScriptJob in all Debugger instances at a time
        static ThreadWeaver::ResourceRestrictionPolicy *loadScriptRestrictionPolicy =
                new ThreadWeaver::ResourceRestrictionPolicy(1);
        debuggerJob->assignQueuePolicy( loadScriptRestrictionPolicy );

        // A new version of the script can only be loaded into the engine, when no other job
        // currently accesses the engine (with an old version of the script loaded).
        assignDebuggerQueuePolicy( debuggerJob );

        if ( m_loadScriptJob ) {
            // A LoadScriptJob exists already, maybe still running, most probably another
            // LoadScriptJob gets enqueued because the provider has changed.
            // Add the running LoadScriptJob as dependency for the new one.
            ThreadWeaver::DependencyPolicy::instance().addDependency( debuggerJob, m_loadScriptJob );
            m_loadScriptJob->assignQueuePolicy( &ThreadWeaver::DependencyPolicy::instance() );
            debuggerJob->assignQueuePolicy( &ThreadWeaver::DependencyPolicy::instance() );
        }
        m_loadScriptJob = qobject_cast< LoadScriptJob* >( debuggerJob );
        Q_ASSERT( m_loadScriptJob );
        m_weaver->enqueue( debuggerJob );
        break;
    case ExecuteConsoleCommand: {
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
    case EvaluateInContext:
        // Evaluate in context means that script code gets executed in the script context of
        // another job. Use restriction policy to only allow one job at a time to evaluate
        // injected code. Do not wait for the script to be loaded, it is already loaded in the
        // running job or gets loaded automatically by the EvaluateInContextJob.
        assignEvaluateInContextQueuePolicy( debuggerJob );
        m_weaver->enqueue( debuggerJob );
        break;
    case TimetableDataRequest:
    case CallScriptFunction:
    case TestFeatures:
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
    QMutexLocker locker( m_mutex );
    if ( !m_debuggerRestrictionPolicy ) {
        m_debuggerRestrictionPolicy = new ThreadWeaver::ResourceRestrictionPolicy( 1 );
    }
    job->assignQueuePolicy( m_debuggerRestrictionPolicy );
}

void Debugger::assignEvaluateInContextQueuePolicy( DebuggerJob *job )
{
    QMutexLocker locker( m_mutex );
    if ( !m_evaluateInContextRestrictionPolicy ) {
        m_evaluateInContextRestrictionPolicy = new ThreadWeaver::ResourceRestrictionPolicy( 1 );
    }
    job->assignQueuePolicy( m_evaluateInContextRestrictionPolicy );
}

void Debugger::runAfterScriptIsLoaded( ThreadWeaver::Job *dependendJob )
{
    QMutexLocker locker( m_mutex );
    if ( m_state != ScriptLoaded ) {
        // If the script is not loaded (still in initializing mode, error or modified)
        // create a job to load it and make the run script job dependend
        if ( !isLoadScriptJobRunning() ) {
            TimetableDataRequestJob *job = qobject_cast< TimetableDataRequestJob* >( dependendJob );
            enqueueJob( createLoadScriptJob(job ? job->debugFlags() : NeverInterrupt) );
        }

        // Add the LoadScriptJob as dependency
        ThreadWeaver::DependencyPolicy::instance().addDependency( dependendJob, m_loadScriptJob );
        m_loadScriptJob->assignQueuePolicy( &ThreadWeaver::DependencyPolicy::instance() );
        dependendJob->assignQueuePolicy( &ThreadWeaver::DependencyPolicy::instance() );
    }

    m_weaver->enqueue( dependendJob );
}

void Debugger::loadScriptJobDone( ThreadWeaver::Job *job )
{
    LoadScriptJob *loadScriptJob = qobject_cast< LoadScriptJob* >( job );
    Q_ASSERT( loadScriptJob );
    QMutexLocker locker( m_mutex );
    if ( loadScriptJob->success() ) {

        m_state = ScriptLoaded;
        m_lastScriptError = NoScriptError;
        m_lastScriptErrorString.clear();
    } else {
        m_state = ScriptError;
        m_lastScriptError = ScriptLoadFailed;
        m_lastScriptErrorString = loadScriptJob->explanation();
    }

    const QStringList globalFunctions = loadScriptJob->globalFunctions();
    const QStringList includedFiles = loadScriptJob->includedFiles();
    ThreadWeaver::DependencyPolicy::instance().free( loadScriptJob );

    emit loadScriptResult( m_lastScriptError, m_lastScriptErrorString,
                           globalFunctions, includedFiles );
}

void Debugger::timetableDataRequestJobDone( ThreadWeaver::Job *job )
{
    // To not crash with queued connections, no pointer to the job gets emitted here
    TimetableDataRequestJob *runScriptJob = qobject_cast< TimetableDataRequestJob* >( job );
    Q_ASSERT( runScriptJob );
    emit requestTimetableDataResult( runScriptJob->request(), runScriptJob->success(),
                                     runScriptJob->explanation(), runScriptJob->timetableData(),
                                     runScriptJob->returnValue() );
}

void Debugger::executeConsoleCommandJobDone( ThreadWeaver::Job *job )
{
    // To not crash with queued connections, no pointer to the job gets emitted here
    ExecuteConsoleCommandJob *consoleCommandJob = qobject_cast< ExecuteConsoleCommandJob* >( job );
    Q_ASSERT( consoleCommandJob );
    emit commandExecutionResult( consoleCommandJob->returnValue().toString(),
                                 !consoleCommandJob->success() );
}

void Debugger::evaluateInContextJobDone( ThreadWeaver::Job *job )
{
    // To not crash with queued connections, no pointer to the job gets emitted here
    EvaluateInContextJob *evaluateInContextJob = qobject_cast< EvaluateInContextJob* >( job );
    Q_ASSERT( evaluateInContextJob );
    emit evaluationResult( evaluateInContextJob->result() );
}

void Debugger::callScriptFunctionJobDone( ThreadWeaver::Job *job )
{
    // To not crash with queued connections, no pointer to the job gets emitted here
    CallScriptFunctionJob *callFunctionJob = qobject_cast< CallScriptFunctionJob* >( job );
    Q_ASSERT( callFunctionJob );
    emit callScriptFunctionResult( callFunctionJob->functionName(), callFunctionJob->returnValue() );
}

void Debugger::testFeaturesJobDone( ThreadWeaver::Job *job )
{
    // To not crash with queued connections, no pointer to the job gets emitted here
    TestFeaturesJob *testJob = qobject_cast< TestFeaturesJob* >( job );
    Q_ASSERT( testJob );
    emit testFeaturesResult( testJob->features() );
}

void Debugger::asynchronousRequestWaitFinished( const QDateTime &timestamp,
                                                int statusCode, int size )
{
    Q_UNUSED( statusCode )
    QMutexLocker locker( m_mutex );
    if ( m_runData ) {
        m_runData->asynchronousDownloadFinished( timestamp, size );
    } else {
        kWarning() << "ScriptRunData object already deleted";
    }
}

void Debugger::synchronousRequestWaitFinished( int statusCode, int waitingTime, int size )
{
    Q_UNUSED( statusCode )
    if ( m_runData ) {
        m_runData->synchronousDownloadFinished( waitingTime, size );
    } else {
        kWarning() << "ScriptRunData object already deleted";
    }
}

void ScriptRunData::executionStopped( const QDateTime &timestamp )
{
    if ( m_state != Running && m_state != LoadingScript ) {
        DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_job, "ERROR: Is not executing!" );
        return;
    } else if ( m_job->type() != LoadScript && !isScriptLoaded() ) {
        m_state = ScriptLoaded;
        DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_job, "New state: Script Loaded" );
    } else {
        if ( m_state == WaitingForSignal ) {
            DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_job, "Error: Was still waiting for a signal" );
        }
        m_state = Finished;
        DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_job, "New state: Finished" );
    }
    m_executionTime += m_executionStartTimestamp.msecsTo( timestamp );
    m_executionStartTimestamp = QDateTime();
}

void ScriptRunData::executionStarted( const QDateTime &timestamp )
{
    /*if ( m_state == Finished ) { // m_executionTime != 0 ) {
        DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_job, "ERROR: Execution was already stopped" );
        return;
    } else*/ if ( m_state == Running || m_state == LoadingScript ) {
        DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_job, "ERROR: Is already executing!" );
        kDebug() << "Error state:" << m_state;
        return;
    }
    if ( m_job->type() != LoadScript && !isScriptLoaded() ) {
        m_state = LoadingScript;
        DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_job, "New state: LoadingScript" );
    } else {
        m_state = Running;
        DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_job, "New state: Running" );
    }
    m_executionStartTimestamp = timestamp;
}

ScriptRunData::ScriptRunData( DebuggerJob *job )
        : m_job(job), m_executionTime(0), m_signalWaitingTime(0), m_interruptTime(0),
          m_synchronousDownloadTime(0), m_asynchronousDownloadSize(0),
          m_synchronousDownloadSize(0), m_state(Initializing)
{
    DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_job, "New state: Initializing" );
}

void ScriptRunData::waitingForSignal( const QDateTime &timestamp )
{
    switch( m_state ) {
    case WaitingForSignal:
        qWarning() << "ERROR: Is already waiting for a signal!";
        return;
    case Finished:
        qWarning() << "ERROR: Is already finished!";
        return;
    case Initializing:
        qWarning() << "ERROR: Was not started!";
        return;
    default:
        break;
    }
    m_state = WaitingForSignal;
    DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_job, "New state: WaitingForSignal" );
    m_waitForSignalTimestamp = timestamp;
}

int ScriptRunData::wokeUpFromSignal( const QDateTime &timestamp )
{
    if( m_state != WaitingForSignal ) {
        qWarning() << "ERROR: Is not waiting for a signal!";
        return 0;
    }
    m_state = Running;
    DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_job, "New state: Running" );
    const int waitingTime = m_waitForSignalTimestamp.msecsTo( timestamp );
    m_waitForSignalTimestamp = QDateTime();
    m_signalWaitingTime += waitingTime;
    return waitingTime;
}

void ScriptRunData::interrupted( const QDateTime &timestamp )
{
    Q_ASSERT( m_state == Running || m_state == LoadingScript );
    m_state = Interrupted;
    m_interruptTimestamp = timestamp;
    DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_job, "New state: Interrupted" );
}

void ScriptRunData::continued( const QDateTime &timestamp )
{
    Q_ASSERT( m_state == Interrupted );
    m_state = Running;
    m_interruptTime += m_interruptTimestamp.msecsTo( timestamp );
    m_interruptTimestamp = QDateTime();
    DEBUGGER_JOB_SYNCHRONIZATION_JOB( m_job, "New state: Running" );
}

bool Debugger::hasRunningJobs() const
{
    QMutexLocker locker( m_mutex );
    return !m_runningJobs.isEmpty();
}

QStack< QPointer< DebuggerJob > > Debugger::runningJobs() const
{
    QMutexLocker locker( m_mutex );
    return m_runningJobs;
}

Debugger::ScriptState Debugger::scriptState() const
{
    QMutexLocker locker( m_mutex );
    return m_state;
}

ScriptErrorType Debugger::lastScriptError() const
{
    QMutexLocker locker( m_mutex );
    return m_lastScriptError;
}

QString Debugger::lastScriptErrorString() const
{
    QMutexLocker locker( m_mutex );
    return m_lastScriptErrorString;
}

VariableModel *Debugger::variableModel() const
{
    QMutexLocker locker( m_mutex );
    return m_variableModel;
}

BacktraceModel *Debugger::backtraceModel() const
{
    QMutexLocker locker( m_mutex );
    return m_backtraceModel;
}

BreakpointModel *Debugger::breakpointModel() const
{
    QMutexLocker locker( m_mutex );
    return m_breakpointModel;
}

WeaverInterfacePointer Debugger::weaver() const
{
    QMutexLocker locker( m_mutex );
    return m_weaver;
}

const QPointer< DebuggerJob > Debugger::currentJob() const
{
    QMutexLocker locker( m_mutex );
    QPointer<DebuggerJob> job = m_runningJobs.top();
    if ( ( job->type() == EvaluateInContext || job->type() == ExecuteConsoleCommand ) &&
            m_runningJobs.count() > 1 ) {
        job = m_runningJobs[ m_runningJobs.count() - 2 ];
    }
    return job;
}

} // namespace Debugger
