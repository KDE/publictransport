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

/**
 * @file
 * @brief Contains the main debugger class.
 *
 * @author Friedrich Pülz <fpuelz@gmx.de>
 **/

#ifndef DEBUGGERTHREAD_H
#define DEBUGGERTHREAD_H

// Own includes
#include "debuggeragent.h" // For inline function calls forwarded to DebuggerAgent

// PublicTransport engine includes
#include <engine/enums.h> // For TimetableData

struct AbstractRequest;
class ServiceProviderData;
namespace Scripting {
    class Network;
    class Storage;
    class ResultObject;
    class Helper;
};
namespace ThreadWeaver {
    class Job;
    class WeaverInterface;
    class JobSequence;
    class ResourceRestrictionPolicy;
};
class QMutex;
class QWaitCondition;
class QScriptProgram;
class QScriptEngine;
class QScriptContextInfo;

using namespace Scripting;

/** @brief The Debugger namespace contains classes and enumerations used for debugging scripts. */
namespace Debugger {

class TestFeaturesJob;

class DebuggerJob;
class EvaluateInContextJob;
class ExecuteConsoleCommandJob;
class TimetableDataRequestJob;
class CallScriptFunctionJob;
class LoadScriptJob;
class BacktraceModel;
class BreakpointModel;
class VariableModel;

/** @brief Contains statistics about a script run. */
class ScriptRunData {
    friend class Debugger;
public:
    /** @brief The time in milliseconds spent for script execution. */
    int executionTime() const {
        return m_executionTimer.elapsed() - m_signalWaitingTime - m_interruptTime -
                                            m_synchronousDownloadTime;
    };

    /** @brief The time in milliseconds spent waiting for signals. */
    int signalWaitingTime() const { return m_signalWaitingTime; };

    int synchronousDownloadTime() const { return m_synchronousDownloadTime; };

    /** @brief The time in milliseconds in which the script was interrupted. */
    int interruptTime() const { return m_interruptTime; };

    int asynchronousDownloadSize() const { return m_asynchronousDownloadSize; };
    int synchronousDownloadSize() const { return m_synchronousDownloadSize; };

protected:
    /** @brief Constructor, directly starts the execution timer. */
    ScriptRunData() : m_signalWaitingTime(0), m_interruptTime(0) {
        m_executionTimer.start();
    };

    /** @brief Call this method when script execution gets suspended to wait for a signal. */
    void waitingForSignal() { m_waitForSignalTimer.start(); };

    /**
     * @brief Call this method when script execution gets continued after waiting for a signal.
     * @returns The time in milliseconds spent waiting for the last signal.
     **/
    int wokeUpFromSignal() {
        const int waitingTime = m_waitForSignalTimer.elapsed();
        m_signalWaitingTime += waitingTime;
        return waitingTime;
    };

    /** @brief Call this method when script execution gets interrupted. */
    void interrupted() { m_interruptTimer.start(); };

    /** @brief Call this method when script execution gets continued after being interrupted. */
    void continued() { m_interruptTime += m_interruptTimer.elapsed(); };

    void asynchronousDownloadFinished( int size ) {
        m_asynchronousDownloadSize += size;
    };

    void synchronousDownloadFinished( int waitingTime, int size ) {
        m_synchronousDownloadTime += waitingTime;
        m_synchronousDownloadSize += size;
    };

private:
    QTime m_executionTimer;
    QTime m_waitForSignalTimer;
    QTime m_interruptTimer;
    int m_signalWaitingTime;
    int m_interruptTime;
    int m_synchronousDownloadTime;
    int m_asynchronousDownloadSize;
    int m_synchronousDownloadSize;
};

/**
 * @brief Manages debugging of QtScript code using DebuggerAgent and ThreadWeaver.
 *
 * DebuggerAgent is a specialized QScriptEngineAgent used to control execution of QtScript code.
 * It is ensured, that only one evaluation is running in the engine at a time. If the evaluation
 * of the main script is interrupted, code can be run inside the current context of the script in
 * another thread. This can be used for eg. console commands or breakpoint condition evaluation.
 *
 * Functions that need to access the script engine (eg. to evaluate script code) return values
 * asynchronously using signals. For example requestTimetableData() returns values in the
 * requestTimetableDataResult() signal, executeCommand() uses the commandExecutionResult() signal
 * and evaluateInContext() uses the evaluationResult() signal.
 * Other information about the script engine gets cached and is provided thread safe via the
 * methods lineNumber(), coloumnNumber(), hasUncaughtException(), uncaughtExceptionLineNumber(),
 * uncaughtException(), backtrace(), variables().
 *
 * @warning If the script is running, ie. not interrupted, the engine mutex is locked to protect
 *   QScriptEngine from simultaneous calls. You should first check the return value of
 *   isInterrupted() or use QMutex::tryLock() and eg. create a thread job object to do the work
 *   when the engine is free again.
 *
 * This class is not thread safe, but DebuggerAgent is. ThreadWeaver jobs are communicating with
 * DebuggerAgent, not this class. This class only manages thread jobs and asynchronous
 * communication. Many functions only foward to the according functions in DebuggerAgent.
 *
 * Variables, a backtrace and breakpoints are managed by separate models, accessible using
 * variableModel(), backtraceModel() and breakpointModel(). Variables and frames of the backtrace
 * are automatically kept in sync with the current script context when the debugger gets
 * interrupted.
 *
 * @see BacktraceModel
 * @see VariableModel
 * @see BreakpointModel
 **/
class Debugger : public QObject {
    Q_OBJECT
    friend class DebuggerAgent;
    friend class LoadScriptJob;
    friend class TimetableDataRequestJob;
    friend class EvaluateInContextJob;
    friend class ExecuteConsoleCommandJob;

public:
    /** @brief States of the script. */
    enum ScriptState {
        Initializing = 0,
        ScriptLoaded,
        ScriptError,
        ScriptModified,
    };

    /** @brief Constructor. */
    explicit Debugger( QObject *parent = 0 );

    /** @brief Destructor. */
    virtual ~Debugger();

    /** @brief Get the current state of the debugger. */
    inline DebuggerState state() const { return m_debugger->state(); };

    /** @brief Whether or not the debugger is currently running. */
    inline bool isRunning() const { return m_debugger->isRunning(); };

    /** @brief Whether or not the debugger is currently interrupted. */
    inline bool isInterrupted() const { return m_debugger->isInterrupted(); };

    /** @brief Get the current state of the script. */
    inline ScriptState scriptState() const { return m_state; };

    /** @brief Get the type of the last script error, if scriptState() returns ScriptError. */
    inline ScriptErrorType lastScriptError() const { return m_lastScriptError; };

    /** @brief Get a descriptive string for the last script error, if scriptState() returns ScriptError. */
    inline QString lastScriptErrorString() const { return m_lastScriptErrorString; };

    /** @brief Get the current execution line number. */
    inline int lineNumber() const { return m_debugger->lineNumber(); };

    /** @brief Get the current execution column number. */
    inline int columnNumber() const { return m_debugger->columnNumber(); };

    /** @brief Whether or not there was an uncaught exception. */
    inline bool hasUncaughtException() const { return m_debugger->hasUncaughtException(); };

    /** @brief The line number of an uncaught exception, if any. */
    inline int uncaughtExceptionLineNumber() const { return m_debugger->uncaughtExceptionLineNumber(); };

    /** @brief The QScriptValue of an uncaught exception, if any. */
    inline QScriptValue uncaughtException() const { return m_debugger->uncaughtException(); };

    /** @brief Get a pointer to the VariableModel used by this debugger. */
    inline VariableModel *variableModel() const { return m_variableModel; };

    /** @brief Get a pointer to the BacktraceModel used by this debugger. */
    inline BacktraceModel *backtraceModel() const { return m_backtraceModel; };

    /** @brief Get a pointer to the BreakpointModel used by this debugger. */
    inline BreakpointModel *breakpointModel() const { return m_breakpointModel; };

    /**
     * @brief Checks whether script execution can be interrupted at @p lineNumber.
     * @see DebuggerAgent::canBreakAt(int)
     **/
    inline NextEvaluatableLineHint canBreakAt( int lineNumber ) const {
            return m_debugger->canBreakAt(lineNumber); };

    /**
     * @brief Get the first executable line number bigger than or equal to @p lineNumber.
     * @see DebuggerAgent::getNextBreakableLineNumber(int)
     **/
    inline int getNextBreakableLineNumber( int lineNumber ) const {
            return m_debugger->getNextBreakableLineNumber(lineNumber); };

    /** @brief Whether or not @p program can be evaluated. */
    bool canEvaluate( const QString &program ) const;

    /**
     * @brief Creates a new job to request timetable data.
     *
     * The return value only informs about errors while creating a TimetableDataRequestJob.
     * The return value of the script function call can be retrieved by connecting to the
     * requestTimetableDataResult() signal.
     *
     * @param request A pointer to a request object used to create the argument for the
     *   function to call.
     * @param debugFlags Flags for the debugger.
     * @return False if the TimetableDataRequestJob could not be created.
     **/
    bool requestTimetableData( const AbstractRequest *request,
                               DebugFlags debugFlags = DefaultDebugFlags );

    /**
     * @brief Creates a new job to execute @p command.
     *
     * The return value of the console @p command can be retrieved by connecting to the
     * commandExecutionResult() signal.
     * If the debugger is already running you cannot execute ConsoleCommand::DebugCommand commands.
     * In that case this function only signals an error message via commandExecutionResult() with
     * the error argument true. Otherwise a TimetableDataRequestJob gets created and
     * enqueued.
     *
     * @param command The console command to execute.
     **/
    void executeCommand( const ConsoleCommand &command );

    /**
     * @brief Creates a new job to evaluate @p program in the current script context.
     *
     * The return value of the evaluation can be retrieved by connecting to the evaluationResult()
     * signal. This creates and enqueues an EvaluateInContextJob.
     *
     * @param program The script code to evaluate.
     * @param contextName A name to be used in backtraces for the evaluation context.
     **/
    void evaluateInContext( const QString &program, const QString &contextName = QString() );

    /**
     * @brief Enqueues @p debuggerJob and executes it when all dependencies are fulfilled.
     *
     * @param debuggerJob The job to enqueue.
     * @param connectJob Whether or not the jobs done() signal should be connected to a slot that
     *   emits the associated result signal (eg. loadScriptResult()) and deletes the job.
     *   If this is false, the job will not be deleted. If this is true connectJob() gets called
     *   before enqueueing the job. The default is true.
     * @return True, if @p debuggerJob was successfully enqueued. False, otherwise.
     **/
    bool enqueueJob( DebuggerJob *debuggerJob, bool connectJob = true );

    /**
     * @brief Connects the done() signal of @p debuggerJob to this debugger.
     *
     * The done() signal gets connected to a slot that emits the associated result signal (eg.
     * loadScriptResult()) and deletes the job.
     **/
    void connectJob( DebuggerJob *debuggerJob );

    /** @brief Create a new TimetableDataRequestJob from @p request. */
    TimetableDataRequestJob *createTimetableDataRequestJob(
            const AbstractRequest *request, DebugFlags debugFlags = DefaultDebugFlags );

    /** @brief Create a new ExecuteConsoleCommandJob from @p command. */
    ExecuteConsoleCommandJob *createExecuteConsoleCommandJob( const ConsoleCommand &command );

    /**
     * @brief Create a new EvaluateInContextJob that executes @p program.
     *
     * @param program The script program to execute in the current context.
     * @p contextName A name for the new context in which @p program gets executed. This name gets
     *   shown in backtraces.
     **/
    EvaluateInContextJob *createEvaluateInContextJob( const QString &program,
                                                      const QString &contextName );

    CallScriptFunctionJob *createCallScriptFunctionJob( const QString &functionName,
            const QVariantList &arguments = QVariantList(), DebugFlags debugFlags = DefaultDebugFlags );

    TestFeaturesJob *createTestFeaturesJob( DebugFlags debugFlags = DefaultDebugFlags );

    ThreadWeaver::WeaverInterface *weaver() const { return m_weaver; };

signals:
    /**
     * @brief Script execution just started.
     * @note Is not emitted, if the script was waiting for a signal to continue execution.
     * @see wokeUpFromSignal
     * @see stopped
     **/
    void started();

    /**
     * @brief The script finished and is no longer running.
     * @note Is not emitted, if the script is still waiting for a signal to continue execution.
     * @param scriptRunData Contains statistics about the script run.
     * @see waitingForSignal
     * @see started
     **/
    void stopped( const ScriptRunData &scriptRunData = ScriptRunData() );

    /**
     * @brief Script execution was stopped, but is waiting for a signal to continue execution.
     * @see wokeUpFromSignal
     * @see stopped
     **/
    void waitingForSignal();

    /**
     * @brief Script execution was continued after waiting for a signal.
     * @param time The time in milliseconds since waitingForSignal() was emitted.
     * @see waitingForSignal
     * @see started
     **/
    void wokeUpFromSignal( int time = 0 );

    /** @brief The current execution position changed to @p lineNumber, @p columnNumber. */
    void positionChanged( int lineNumber, int columnNumber, int oldLineNumber, int oldColumnNumber );

    /** @brief The state of the debugger has changed from @p oldState to @p newState. */
    void stateChanged( DebuggerState newState, DebuggerState oldState );

    /** @brief A new @p breakpoint was added. */
    void breakpointAdded( const Breakpoint &breakpoint );

    /** @brief @p breakpoint is about to be removed and deleted. */
    void breakpointAboutToBeRemoved( const Breakpoint &breakpoint );

    /** @brief Reached @p breakpoint and increased it's hit count. */
    void breakpointReached( const Breakpoint &breakpoint );

    /** @brief An uncaught exception occured at @p lineNumber. */
    void exception( int lineNumber, const QString &errorMessage,
                    const QString &fileName = QString() );

    /** @brief Script execution was just interrupted. */
    void interrupted();

    /** @brief Script execution was just aborted. */
    void aborted();

    /** @brief Script execution was just continued after begin interrupted. */
    void continued( bool willInterruptAfterNextStatement = false );

    /** @brief The script send a @p debugString using the print() function. */
    void output( const QString &debugString, const QScriptContextInfo &contextInfo );

    void informationMessage( const QString &message );
    void errorMessage( const QString &message );

    /**
     * @brief A LoadScriptJob is done.
     *
     * The LoadScriptJob was created in loadScript().
     *
     * @param returnValue The string returned by the console command.
     * @param error True, if there was an error while executing the console command (@p returnValue
     *   contains an error message in that case). False otherwise.
     **/
    void loadScriptResult( ScriptErrorType lastScriptError,
                           const QString &lastScriptErrorString );

    /**
     * @brief An ExecuteConsoleCommandJob is done.
     *
     * The ExecuteConsoleCommandJob may have been created in executeCommand() or in
     * createExecuteConsoleCommandJob() (only if the job was also enqueued and connected with
     * the debugger).
     *
     * @param returnValue The string returned by the console command.
     * @param error True, if there was an error while executing the console command (@p returnValue
     *   contains an error message in that case). False otherwise.
     **/
    void commandExecutionResult( const QString &returnValue, bool error = false );

    /**
     * @brief An EvaluateInContextJob is done.
     *
     * The EvaluateInContextJob may have been created in evaluateInContext() or in
     * createEvaluateInContextJob() (only if the job was also enqueued and connected with
     * the debugger).
     *
     * @param result An EvaluationResult object containing information about the result of the
     *   evaluation, ie. the return value and error information.
     **/
    void evaluationResult( const EvaluationResult &result );

    /**
     * @brief A TimetableDataRequestJob is done.
     *
     * The TimetableDataRequestJob may have been created in requestTimetableData() or in
     * createTimetableDataRequestJob() (only if the job was also enqueued and connected with
     * the debugger).
     *
     * @param timetableData A list of timetable data objects, ie. departures / journeys /
     *   stop suggestions.
     * @param returnValue The value, returned by the script.
     **/
    void requestTimetableDataResult( const QSharedPointer< AbstractRequest > &request,
                             bool success, const QString &explanation,
                             const QList< TimetableData > &timetableData = QList< TimetableData >(),
                             const QScriptValue &returnValue = QScriptValue() );

    void callScriptFunctionResult( const QString &functionName,
                                   const QScriptValue &returnValue = QScriptValue() );

    void testFeaturesResult( const QList<Enums::ProviderFeature> features ); // TODO

    void scriptErrorReceived( const QString &errorMessage, const QScriptContextInfo &contextInfo,
                              const QString &failedParseText );

public slots:
    /** @brief Load script code @p program. */
    void loadScript( const QString &program, const ServiceProviderData *data );

    /** @brief Continue script execution until the next statement. */
    inline void debugStepInto( int repeat = 0 ) { m_debugger->debugStepInto(repeat); };

    /** @brief Continue script execution until the next statement in the same level. */
    inline void debugStepOver( int repeat = 0 ) { m_debugger->debugStepOver(repeat); };

    /** @brief Continue script execution until the current function gets left. */
    inline void debugStepOut( int repeat = 0 ) { m_debugger->debugStepOut(repeat); };

    /** @brief Continue script execution until @p lineNumber is reached. */
    void debugRunUntilLineNumber( int lineNumber ) {
            m_debugger->debugRunUntilLineNumber(lineNumber); };

    /** @brief Interrupt script execution. */
    inline void debugInterrupt() { m_debugger->debugInterrupt(); };

    /** @brief Continue script execution, only interrupt on breakpoints or uncaught exceptions. */
    inline void debugContinue() { m_debugger->debugContinue(); };

    /** @brief Abort script execution. */
    inline void abortDebugger() { m_debugger->abortDebugger(); };

    /** @brief Remove all breakpoints, for each removed breakpoint breakpointRemoved() gets emitted. */
    void removeAllBreakpoints();

protected slots:
    /** @brief A LoadScriptJob is done, started by loadScript(). */
    void loadScriptJobDone( ThreadWeaver::Job *job );

    /** @brief A TimetableDataRequestJob is done, started by requestTimetableData(). */
    void timetableDataRequestJobDone( ThreadWeaver::Job *job );

    /** @brief An ExecuteConsoleCommandJob is done, started by executeCommand(). */
    void executeConsoleCommandJobDone( ThreadWeaver::Job *job );

    /** @brief An EvaluateInContextJob is done, started by evaluateInContext(). */
    void evaluateInContextJobDone( ThreadWeaver::Job *job );

    /** @brief A CallScriptFunctionJob is done. */
    void callScriptFunctionJobDone( ThreadWeaver::Job *job );

    /** @brief A TestFeaturesJob is done. */
    void testFeaturesJobDone( ThreadWeaver::Job *job );

    void jobStarted( ThreadWeaver::Job *job );
    void jobDestroyed( QObject *job );

    void slotStarted();
    void slotStopped();
    void slotInterrupted();
    void slotContinued( bool willInterruptAfterNextStatement = false );
    void slotAborted();
    void asynchronousRequestWaitFinished( int size );
    void synchronousRequestWaitFinished( int waitingTime, int size );

    void timeout();

private:
    LoadScriptJob *enqueueNewLoadScriptJob();
    void runAfterScriptIsLoaded( ThreadWeaver::Job *dependendJob );
    void createScriptObjects( const ServiceProviderData *data );
    void assignDebuggerQueuePolicy( DebuggerJob *job );
    void assignEvaluateInContextQueuePolicy( DebuggerJob *job );

    void startTimeout( int milliseconds = 5000 );
    void stopTimeout();

    ScriptState m_state;
    ThreadWeaver::WeaverInterface *const m_weaver;
    LoadScriptJob *m_loadScriptJob;
    QMutex *const m_engineMutex; // Global mutex to protect the script engine

    QScriptEngine *const m_engine; // Can be accessed simultanously by multiple threads, protected by m_engineMutex
    QScriptProgram *m_script;
    DebuggerAgent *const m_debugger; // Is thread safe
    ScriptErrorType m_lastScriptError;
    QString m_lastScriptErrorString;

    const ServiceProviderData *m_data;
    Network *m_scriptNetwork;
    Helper *m_scriptHelper;
    ResultObject *m_scriptResult;
    Storage *m_scriptStorage;

    VariableModel *const m_variableModel;
    BacktraceModel *const m_backtraceModel;
    BreakpointModel *const m_breakpointModel;

    ThreadWeaver::JobSequence *m_jobSequence;

    // Restricts access to the debugger to one job at a time
    // (currently assigned to LoadScriptJob and TimetableDataRequestJob)
    ThreadWeaver::ResourceRestrictionPolicy *m_debuggerRestrictionPolicy;

    // Restricts (short) access to the debugger in the current script context when the debugger
    // is currently interrupted to one job at a time
    // (currently assigned to EvaluateInContextJob and ExecuteConsoleCommandJob, if the command
    //  is executing script code)
    ThreadWeaver::ResourceRestrictionPolicy *m_evaluateInContextRestrictionPolicy;

    QList< QObject* > m_runningJobs;
    bool m_running;
    ScriptRunData *m_runData;
    QTimer *m_timeout;
};

}; // namespace Debugger

#endif // Multiple inclusion guard
