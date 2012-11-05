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
#include "debug_config.h"
#include "debuggerjobs.h"

// PublicTransport engine includes
#include <engine/enums.h> // For TimetableData

// Qt includes
#include <QPointer>

struct AbstractRequest;
class ServiceProviderData;
namespace Scripting {
    class Network;
    class Storage;
    class ResultObject;
    class Helper;
    class DataStreamPrototype;
};
namespace ThreadWeaver {
    class Job;
    class WeaverInterface;
    class JobSequence;
    class ResourceRestrictionPolicy;
    class Thread;
};
typedef QSharedPointer< ThreadWeaver::WeaverInterface > WeaverInterfacePointer;

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
    enum State {
        Initializing = 0,
        LoadingScript,
        ScriptLoaded, /**< The script has evaluated, ie. loaded, but no function has been called.
                * LoadScriptJob does not use this and directly uses Finished instead. */
        Running,
        WaitingForSignal,
        Interrupted,
        Finished
    };

    DebuggerJob *job() const { return m_job; };

    bool isExecuting() const { return m_state != Initializing && m_state != Finished; };
    bool isScriptLoaded() const { return m_state >= ScriptLoaded; };
    bool isWaitingForSignal() const { return m_state == WaitingForSignal; };

    /** @brief The time in milliseconds spent for script execution. */
    int executionTime() const {
        return qMax( 0, m_executionTime - m_signalWaitingTime -
                        m_interruptTime - m_synchronousDownloadTime );
    };

    /** @brief The time in milliseconds spent waiting for signals. */
    int signalWaitingTime() const { return m_signalWaitingTime; };

    int synchronousDownloadTime() const { return m_synchronousDownloadTime; };

    /** @brief The time in milliseconds in which the script was interrupted. */
    int interruptTime() const { return m_interruptTime; };

    int asynchronousDownloadSize() const { return m_asynchronousDownloadSize; };
    int synchronousDownloadSize() const { return m_synchronousDownloadSize; };
    int totalDownloadSize() const { return m_asynchronousDownloadSize + m_synchronousDownloadSize; };

protected:
    /** @brief Constructor, directly starts the execution timer. */
    ScriptRunData( DebuggerJob *job = 0 );

    void executionStarted( const QDateTime &timestamp );

    void executionStopped( const QDateTime &timestamp );

    /** @brief Call this method when script execution gets suspended to wait for a signal. */
    void waitingForSignal( const QDateTime &timestamp );

    /**
     * @brief Call this method when script execution gets continued after waiting for a signal.
     * @returns The time in milliseconds spent waiting for the last signal.
     **/
    int wokeUpFromSignal( const QDateTime &timestamp );

    /** @brief Call this method when script execution gets interrupted. */
    void interrupted( const QDateTime &timestamp );

    /** @brief Call this method when script execution gets continued after being interrupted. */
    void continued( const QDateTime &timestamp );

    void asynchronousDownloadFinished( const QDateTime &timestamp, int size ) {
        Q_UNUSED( timestamp )
        m_asynchronousDownloadSize += size;
    };

    void synchronousDownloadFinished( int waitingTime, int size ) {
        m_synchronousDownloadTime += waitingTime;
        m_synchronousDownloadSize += size;
    };

private:
    QPointer<DebuggerJob> m_job;
    QDateTime m_executionStartTimestamp;
    QDateTime m_waitForSignalTimestamp;
    QDateTime m_interruptTimestamp;
    int m_executionTime;
    int m_signalWaitingTime;
    int m_interruptTime;
    int m_synchronousDownloadTime;
    int m_asynchronousDownloadSize;
    int m_synchronousDownloadSize;
    State m_state;
};

/**
 * @brief Manages multithreaded debugging of QtScript code.
 *
 * ThreadWeaver is used with jobs derived from DebuggerJob and some restriction policies to
 * synchronize the jobs. It is ensured, that only one evaluation is running in the engine
 * at a time. If the evaluation of the script is interrupted, code can be run inside the current
 * context of the script in another thread. This can be used for eg. console commands or
 * breakpoint condition evaluation.
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
 * This class is not thread safe, but DebuggerAgent is. DebuggerJob instances are communicating
 * with DebuggerAgent, not this class. This class manages the thread jobs and synchronizes them.
 * Some functions only foward to the according functions in the currently running DebuggerJob.
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
    explicit Debugger( const WeaverInterfacePointer &weaver, QObject *parent = 0 );

    /** @brief Destructor. */
    virtual ~Debugger();

    bool hasRunningJobs() const;
    QStack< QPointer<DebuggerJob> > runningJobs() const;

    /** @brief Get the current state of the debugger. */
    inline DebuggerState state() const {
        return !hasRunningJobs() ? NotRunning : currentJob()->state();
    };

    /** @brief Whether or not the debugger is currently running. */
    inline bool isRunning() const {
        return !hasRunningJobs() ? false : currentJob()->isRunning();
    };

    /** @brief Whether or not the debugger is currently interrupted. */
    inline bool isInterrupted() const {
        return !hasRunningJobs() ? false : currentJob()->isInterrupted();
    };

    /** @brief Get the current state of the script. */
    ScriptState scriptState() const;

    /** @brief Get the type of the last script error, if scriptState() returns ScriptError. */
    ScriptErrorType lastScriptError() const;

    /** @brief Get a descriptive string for the last script error, if scriptState() returns ScriptError. */
    QString lastScriptErrorString() const;

    /** @brief The name of the currently executed source file. */
    inline QString currentSourceFile() const {
        return !hasRunningJobs() ? QString() : currentJob()->currentSourceFile();
    };

    /** @brief Get the current execution line number. */
    inline int lineNumber() const { return !hasRunningJobs() ? -1 : currentJob()->lineNumber(); };

    /** @brief Get the current execution column number. */
    inline int columnNumber() const { return !hasRunningJobs() ? -1 : currentJob()->columnNumber(); };

    /** @brief Whether or not there was an uncaught exception. */
    inline bool hasUncaughtException() const {
        return !hasRunningJobs() ? false : currentJob()->hasUncaughtException();
    };

    /** @brief The line number of an uncaught exception, if any. */
    inline int uncaughtExceptionLineNumber() const {
        return !hasRunningJobs() ? -1 : currentJob()->uncaughtExceptionLineNumber();
    };

    /** @brief The QScriptValue of an uncaught exception, if any. */
    inline QScriptValue uncaughtException() const {
        return !hasRunningJobs() ? QScriptValue() : currentJob()->uncaughtException();
    };

    /** @brief Get a pointer to the VariableModel used by this debugger. */
    VariableModel *variableModel() const;

    /** @brief Get a pointer to the BacktraceModel used by this debugger. */
    BacktraceModel *backtraceModel() const;

    /** @brief Get a pointer to the BreakpointModel used by this debugger. */
    BreakpointModel *breakpointModel() const;

    /**
     * @brief Load script code @p program.
     * @return @c True if a LoadScriptJob was started or is already running,
     *   @c false if the script was already loaded and did not change.
     **/
    LoadScriptJob *loadScript( const QString &program, const ServiceProviderData *data,
                               DebugFlags debugFlags = DefaultDebugFlags );

    bool isLoadScriptJobRunning() const;

    /**
     * @brief Checks whether script execution can be interrupted at @p lineNumber.
     * @see DebuggerAgent::canBreakAt(int)
     **/
    inline NextEvaluatableLineHint canBreakAt( const QString &fileName, int lineNumber ) const {
            return !hasRunningJobs() ? CannotFindNextEvaluatableLine
                    : currentJob()->canBreakAt(fileName, lineNumber); };

    /**
     * @brief Get the first executable line number bigger than or equal to @p lineNumber.
     * @see DebuggerAgent::getNextBreakableLineNumber(int)
     **/
    inline int getNextBreakableLineNumber( const QString &fileName, int lineNumber ) const {
            return !hasRunningJobs() ? lineNumber
                    : currentJob()->getNextBreakableLineNumber(fileName, lineNumber); };

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
                               const QString &useCase = QString(),
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
     * @return True, if @p debuggerJob was successfully enqueued. False, otherwise.
     **/
    bool enqueueJob( DebuggerJob *debuggerJob );

    /** @brief Create a new LoadScriptJob. */
    LoadScriptJob *getLoadScriptJob( const QString &program, const ServiceProviderData *data,
                                     DebugFlags debugFlags = NeverInterrupt );

    /** @brief Create a new TimetableDataRequestJob from @p request. */
    TimetableDataRequestJob *createTimetableDataRequestJob(
            const AbstractRequest *request, const QString &useCase = QString(),
            DebugFlags debugFlags = DefaultDebugFlags );

    /** @brief Create a new ExecuteConsoleCommandJob from @p command. */
    ExecuteConsoleCommandJob *createExecuteConsoleCommandJob( const ConsoleCommand &command,
                                                              const QString &useCase = QString() );

    /**
     * @brief Create a new EvaluateInContextJob that executes @p program.
     *
     * @param program The script program to execute in the current context.
     * @p contextName A name for the new context in which @p program gets executed. This name gets
     *   shown in backtraces.
     **/
    EvaluateInContextJob *createEvaluateInContextJob( const QString &program,
            const QString &contextName, const QString &useCase = QString() );

    CallScriptFunctionJob *createCallScriptFunctionJob( const QString &functionName,
            const QVariantList &arguments = QVariantList(), const QString &useCase = QString(),
            DebugFlags debugFlags = DefaultDebugFlags );

    TestFeaturesJob *createTestFeaturesJob( const QString &useCase = QString(),
                                            DebugFlags debugFlags = DefaultDebugFlags );

    WeaverInterfacePointer weaver() const;

    /**
     * @brief Blocks until the debugger has been completely shut down.
     *
     * If the debugger is not running, this function returns immediately.
     * This function should be called before starting another execution to ensure that the debugger
     * state stays clean. Otherwise there may be crashes and unexpected behaviour.
     **/
    void finish();

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
    void interrupted( int lineNumber, const QString &fileName, const QDateTime &timestamp );

    /** @brief Script execution was just aborted. */
    void aborted();

    /** @brief Script execution was just continued after begin interrupted. */
    void continued( const QDateTime &timestamp, bool willInterruptAfterNextStatement = false );

    /** @brief The script send a @p debugString using the print() function. */
    void output( const QString &debugString, const QScriptContextInfo &contextInfo );

    void informationMessage( const QString &message );
    void errorMessage( const QString &message );

    /** @brief A job of the given @p type has been started. */
    void jobStarted( JobType type, const QString &useCase, const QString &objectName );

    /** @brief A job of the given @p type is done. */
    void jobDone( JobType type, const QString &useCase, const QString &objectName,
                  const DebuggerJobResult &result );

    /**
     * @brief A LoadScriptJob is done.
     *
     * The LoadScriptJob was created in loadScript().
     *
     * @param returnValue The string returned by the console command.
     * @param error True, if there was an error while executing the console command (@p returnValue
     *   contains an error message in that case). False otherwise.
     * @param globalFunctions A list of global functions used by the PublicTransport engine that
     *   are implemented by the script.
     * @param includedFiles A list of file paths to all included script files.
     **/
    void loadScriptResult( ScriptErrorType lastScriptError,
                           const QString &lastScriptErrorString,
                           const QStringList &globalFunctions,
                           const QStringList &includedFiles );

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
                             const QVariant &returnValue = QVariant() );

    void callScriptFunctionResult( const QString &functionName,
                                   const QVariant &returnValue = QVariant() );

    void testFeaturesResult( const QList<Enums::ProviderFeature> features );

    void scriptMessageReceived( const QString &errorMessage, const QScriptContextInfo &contextInfo,
                              const QString &failedParseText,
                              Helper::ErrorSeverity severity = Helper::Information );

public slots:
    /** @brief Continue script execution until the next statement. */
    inline void debugStepInto( int repeat = 0 ) {
        if ( hasRunningJobs() ) currentJob()->debugStepInto(repeat);
    };

    /** @brief Continue script execution until the next statement in the same level. */
    inline void debugStepOver( int repeat = 0 ) {
        if ( hasRunningJobs() ) currentJob()->debugStepOver(repeat);
    };

    /** @brief Continue script execution until the current function gets left. */
    inline void debugStepOut( int repeat = 0 ) {
        if ( hasRunningJobs() ) currentJob()->debugStepOut(repeat);
    };

    /** @brief Continue script execution until @p lineNumber is reached. */
    void debugRunUntilLineNumber( const QString &fileName, int lineNumber ) {
        if ( hasRunningJobs() ) currentJob()->debugRunUntilLineNumber(fileName, lineNumber); };

    /** @brief Interrupt script execution. */
    inline void debugInterrupt() { if ( hasRunningJobs() ) currentJob()->debugInterrupt(); };

    /** @brief Continue script execution, only interrupt on breakpoints or uncaught exceptions. */
    inline void debugContinue() { if ( hasRunningJobs() ) currentJob()->debugContinue(); };

    /** @brief Abort script execution. */
    inline void abortDebugger() { if ( hasRunningJobs() ) currentJob()->abortDebugger(); };

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

    void slotJobStarted( ThreadWeaver::Job *job );
    void slotJobDone( ThreadWeaver::Job *job );

    void slotStarted( const QDateTime &timestamp );
    void slotStopped( const QDateTime &timestamp, ScriptStoppedFlags flags = ScriptStopped,
                      int uncaughtExceptionLineNumber = -1,
                      const QString &uncaughtException = QString() );
    void slotInterrupted( int lineNumber, const QString &fileName, const QDateTime &timestamp );
    void slotContinued( const QDateTime &timestamp, bool willInterruptAfterNextStatement = false );
    void slotAborted();
    void asynchronousRequestWaitFinished( const QDateTime &timestamp, int statusCode, int size );
    void synchronousRequestWaitFinished( int statusCode, int waitingTime, int size );

    void timeout();

private:
    void initialize();
    void runAfterScriptIsLoaded( ThreadWeaver::Job *dependendJob );
    void assignDebuggerQueuePolicy( DebuggerJob *job );
    void assignEvaluateInContextQueuePolicy( DebuggerJob *job );
    LoadScriptJob *createLoadScriptJob( DebugFlags debugFlags = NeverInterrupt );
    void connectJob( DebuggerJob *debuggerJob );

    // Timeout for network requests, leave some time, many tests may be running in parallel
    void startTimeout( int milliseconds = 10000 );
    void stopTimeout();

    const QPointer<DebuggerJob> currentJob() const;
    inline bool isEvaluating() const {
        return hasRunningJobs() ? currentJob()->isEvaluating() : false;
    };

    ScriptState m_state;
    WeaverInterfacePointer m_weaver;
    QMutex *m_mutex;
    LoadScriptJob *m_loadScriptJob;
    QStack< QPointer<DebuggerJob> > m_runningJobs;

    ScriptErrorType m_lastScriptError;
    QString m_lastScriptErrorString;
    ScriptData m_data;

    VariableModel *const m_variableModel;
    BacktraceModel *const m_backtraceModel;
    BreakpointModel *const m_breakpointModel;

    // Restricts access to the debugger to one job at a time
    // (currently assigned to LoadScriptJob and TimetableDataRequestJob)
    ThreadWeaver::ResourceRestrictionPolicy *m_debuggerRestrictionPolicy;

    // Restricts (short) access to the debugger in the current script context when the debugger
    // is currently interrupted to one job at a time
    // (currently assigned to EvaluateInContextJob and ExecuteConsoleCommandJob, if the command
    //  is executing script code)
    ThreadWeaver::ResourceRestrictionPolicy *m_evaluateInContextRestrictionPolicy;

    bool m_running;
    ScriptRunData *m_runData;
    QTimer *m_timeout;
};

}; // namespace Debugger

#endif // Multiple inclusion guard
