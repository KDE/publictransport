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
#include "debuggeragent.h"
#include "debuggerstructures.h"

// PublicTransport engine includes
#include <engine/enums.h>

namespace ThreadWeaver
{
    class Job;
    class WeaverInterface;
};
namespace Scripting {
    class Network;
    class Storage;
    class ResultObject;
    class Helper;
};
struct RequestInfo;
class TimetableAccessorInfo;
class QMutex;
class QWaitCondition;
class QScriptProgram;
class QScriptEngine;
class QScriptContextInfo;

using namespace Scripting;

/** @brief The Debugger namespace contains classes and enumerations used for debugging scripts. */
namespace Debugger {
class EvaluateInContextJob;
class ExecuteConsoleCommandJob;
class ProcessTimetableDataRequestJob;
class LoadScriptJob;

/**
 * @brief Manages debugging of QtScript code using DebuggerAgent and ThreadWeaver.
 *
 * DebuggerAgent is a specialized QScriptEngineAgent used to control execution of QtScript code.
 * It is ensured, that only one evaluation is running in the engine at a time. If the evaluation
 * of the main script is interrupted, code can be run inside the current context of the script in
 * another thread. This can be used for eg. console commands or breakpoint condition evaluation.
 *
 * Functions that need to access the script engine (eg. to evaluate script code) return values
 * asynchronously using signals. For example callFunction() returns values in the
 * functionCallResult() signal, executeCommand() uses the commandExecutionResult() signal and
 * evaluateInContext() uses the evaluationResult() signal.
 * Other information about the script engine gets cached and is provided thread safe via the
 * methods lineNumber(), coloumnNumber(), hasUncaughtException(), uncaughtExceptionLineNumber(),
 * uncaughtException(), backtrace(), variables().
 *
 * @warning If the script is running, ie. not interrupted, the engine mutex is locked to protect
 *   QScriptEngine from simultaneous calls. So you should first check the return value of
 *   isInterrupted() or use QMutex::tryLock() and eg. create a thread job object to do the work
 *   when the engine is free again.
 *
 * This class is not thread safe, but DebuggerAgent is. ThreadWeaver jobs are communicating with
 * DebuggerAgent, not this class. This class only manages thread jobs and asynchronous
 * communication. Many functions only foward to the according functions in DebuggerAgent.
 **/
class Debugger : public QObject {
    Q_OBJECT
    friend class DebuggerAgent;
    friend class LoadScriptJob;
    friend class ProcessTimetableDataRequestJob;
    friend class EvaluateInContextJob;
    friend class ExecuteConsoleCommandJob;

public:
    enum ScriptErrorType {
        NoScriptError = 0,
        ScriptLoadFailed,
        ScriptParseError,
        ScriptRunError
    };

    enum ScriptState {
        Initializing = 0,
        ScriptLoaded,
        ScriptError,
        ScriptModified,
    };


    explicit Debugger( QObject *parent = 0 );
    virtual ~Debugger();

    /** @brief Gets the current state of the debugger. */
    inline DebuggerState state() const { return m_debugger->state(); };

    /** @brief Whether or not the debugger is currently interrupted. */
    inline bool isInterrupted() const { return m_debugger->isInterrupted(); };

    /** @brief Gets the current state of the script. */
    inline ScriptState scriptState() const { return m_state; };

    /** @brief Gets the current execution line number. */
    inline int lineNumber() const { return m_debugger->lineNumber(); };

    /** @brief Gets the current execution column number. */
    inline int columnNumber() const { return m_debugger->columnNumber(); };

    /** @brief Whether or not there was an uncaught exception. */
    inline bool hasUncaughtException() const { return m_debugger->hasUncaughtException(); };

    /** @brief The line number of an uncaught exception, if any. */
    inline int uncaughtExceptionLineNumber() const { return m_debugger->uncaughtExceptionLineNumber(); };

    /** @brief The QScriptValue of an uncaught exception, if any. */
    inline QScriptValue uncaughtException() const { return m_debugger->uncaughtException(); };

    /** @brief Get the current backtrace as list of QScriptContextInfo objects. */
    inline FrameStack backtrace() const { return m_debugger->backtrace(); };

    /** @brief Compares @p backtrace with @p oldBacktrace. */
    inline BacktraceChange compareBacktraces( const FrameStack &backtrace,
                                                             const FrameStack &oldBacktrace ) const {
            return m_debugger->compareBacktraces(backtrace, oldBacktrace); };

    /**
     * @brief TODO
     *
     * @param frame frame==0 current frame, frame==1 previous frame, ...
     **/
    inline Variables variables( int frame = 0 ) { return m_debugger->variables(frame); };

    /** @brief Get a list of line numbers with breakpoints. */
    inline QList< uint > breakpointLines() const { return m_debugger->breakpointLines(); };

    /** @brief Remove the given @p breakpoint. */
    inline bool removeBreakpoint( const Breakpoint &breakpoint ) {
            return m_debugger->removeBreakpoint(breakpoint); };

    /** @brief Remove the breakpoint at @p lineNumber. */
    inline bool removeBreakpoint( int lineNumber ) {
            return m_debugger->removeBreakpoint(lineNumber); };

    /** @brief Add the given @p breakpoint, existing breakpoints at the same line are overwritten. */
    inline bool addBreakpoint( const Breakpoint &breakpoint ) {
            return m_debugger->addBreakpoint(breakpoint); };

    /** @brief Adds/removes a breakpoint at @p lineNumber. */
    inline Breakpoint setBreakpoint( int lineNumber, bool enable = true ) {
            return m_debugger->setBreakpoint(lineNumber, enable); };

    /** @brief Toggle breakpoint at @p lineNumber. */
    inline Breakpoint toggleBreakpoint( int lineNumber ) const {
            return m_debugger->toggleBreakpoint(lineNumber); };

    /** @brief Get the breakpoint at @p lineNumber or an invalid breakpoint. */
    inline Breakpoint breakpointAt( int lineNumber ) const {
            return m_debugger->breakpointAt(lineNumber); };

    /** @brief Get the state of the breakpoint at @p lineNumber or NoBreakpoint if there is none. */
    inline Breakpoint::State breakpointState( int lineNumber ) const {
            return m_debugger->breakpointState(lineNumber); };

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

    /**
     * @brief Creates a new job to call @p functionToRun with @p requestInfo.
     *
     * The return value only informs about errors while creating a ProcessTimetableDataRequestJob.
     * The return value of the script function call can be retrieved by connecting to the
     * functionCallResult() signal.
     *
     * @param functionToRun The name of the global script function to call.
     * @param requestInfo A pointer to a RequestInfo object used to create the argument for the
     *   function to call.
     * @return False if the ProcessTimetableDataRequestJob could not be created.
     **/
    bool callFunction( const QString &functionToRun, const RequestInfo *requestInfo );

    /**
     * @brief Creates a new job to execute @p command.
     *
     * The return value of the console @p command can be retrieved by connecting to the
     * commandExecutionResult() signal.
     * If the debugger is already running you cannot execute ConsoleCommand::DebugCommand commands.
     * In that case this function only signals an error message via commandExecutionResult() with
     * the error argument true. Otherwise a ProcessTimetableDataRequestJob gets created and
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

signals:
    /** @brief Script execution just started. */
    void started();

    /** @brief The script finished and is no longer running */
    void stopped();

    /** @brief The current execution position changed to @p lineNumber, @p columnNumber. */
    void positionChanged( int lineNumber, int columnNumber, int oldLineNumber, int oldColumnNumber );

    /** @brief A new @p breakpoint was added. */
    void breakpointAdded( const Breakpoint &breakpoint );

    /** @brief @p breakpoint was removed. */
    void breakpointRemoved( const Breakpoint &breakpoint );

    /** @brief Reached @p breakpoint and increased it's hit count. */
    void breakpointReached( const Breakpoint &breakpoint );

    /** @brief An uncaught exception occured at @p lineNumber. */
    void exception( int lineNumber, const QString &errorMessage );

    /** @brief Script execution was just interrupted. */
    void interrupted();

    /** @brief Script execution was just continued after begin interrupted. */
    void continued();

    /** @brief There was a @p change in the @p backtrace. */
    void backtraceChanged( const FrameStack &backtrace, BacktraceChange change );

    /** @brief The script send a @p debugString using the print() function. */
    void output( const QString &debugString, const QScriptContextInfo &contextInfo );

    /**
     * @brief An ExecuteConsoleCommandJob created in executeCommand() is done.
     *
     * @param returnValue The string returned by the console command.
     * @param error True, if there was an error while executing the console command (@p returnValue
     *   contains an error message in that case). False otherwise.
     **/
    void commandExecutionResult( const QString &returnValue, bool error = false );

    /**
     * @brief An EvaluateInContextJob created in evaluateInContext() is done.
     *
     * @param result An EvaluationResult object containing information about the result of the
     *   evaluation, ie. the return value and error information.
     **/
    void evaluationResult( const EvaluationResult &result );

    /**
     * @brief The ProcessTimetableDataRequestJob created in callFunction() is done.
     *
     * @param timetableData A list of timetable data objects, ie. departures / journeys /
     *   stop suggestions.
     * @param returnValue The value, returned by the script.
     **/
    void functionCallResult( const QList< TimetableData > &timetableData,
                            const QScriptValue &returnValue );

public slots:
    /** @brief Load script code @p program. */
    void loadScript( const QString &program, const TimetableAccessorInfo *info );

    /** @brief Continue script execution until the next statement. */
    inline void debugStepInto( int repeat = 0 ) { m_debugger->debugStepInto(repeat); };

    /** @brief Continue script execution until the next statement in the same context. */
    inline void debugStepOver( int repeat = 0 ) { m_debugger->debugStepOver(repeat); };

    /** @brief Continue script execution until the current function gets left. */
    inline void debugStepOut( int repeat = 0 ) { m_debugger->debugStepOut(repeat); };

    /** @brief Continue script execution until @p lineNumber is reached. */
    inline void debugRunUntilLineNumber( int lineNumber ) {
            m_debugger->debugRunUntilLineNumber(lineNumber); };

    /** @brief Interrupt script execution. */
    inline void debugInterrupt() { m_debugger->debugInterrupt(); };

    /** @brief Continue script execution, only interrupt on breakpoints or uncaught exceptions. */
    inline void debugContinue() { m_debugger->debugContinue(); };

    /** @brief Abort script execution. */
    inline void abortDebugger() { m_debugger->abortDebugger(); };

    /** @brief Remove all breakpoints, for each removed breakpoint breakpointRemoved() gets emitted. */
    inline void removeAllBreakpoints() { m_debugger->removeAllBreakpoints(); };

protected slots:
    /** @brief A LoadScriptJob is done, started by loadScript(). */
    void loadScriptJobDone( ThreadWeaver::Job *job );

    /** @brief A ProcessTimetableDataRequestJob is done, started by callFunction(). */
    void processTimetableDataRequestJobDone( ThreadWeaver::Job *job );

    /** @brief An ExecuteConsoleCommandJob is done, started by executeCommand(). */
    void executeConsoleCommandJobDone( ThreadWeaver::Job *job );

    /** @brief A EvaluateInContextJob is done, started by evaluateInContext(). */
    void evaluateInContextJobDone( ThreadWeaver::Job *job );

    // TODO Remove
    void jobSequenceDone( ThreadWeaver::Job *jobSequence );

private:
    void runAfterScriptIsLoaded( ThreadWeaver::Job *dependendJob );
    LoadScriptJob *createLoadScriptJob();
    ProcessTimetableDataRequestJob *createProcessTimetableDataRequestJob( const RequestInfo *request,
                                                        DebugMode debugMode = InterruptOnExceptions );
    ExecuteConsoleCommandJob *createExecuteConsoleCommandJob( const ConsoleCommand &command );
    EvaluateInContextJob *createEvaluateInContextJob( const QString &program,
                                                      const QString &context );

    void loadScriptObjects( const TimetableAccessorInfo *info );

    ScriptState m_state;
    ThreadWeaver::WeaverInterface *m_weaver;
    LoadScriptJob *m_loadScriptJob;
    QMutex *m_engineMutex; // Global mutex to protect the script engine

    QScriptEngine *m_engine; // Can be accessed simultanously by multiple threads, protected by m_engineMutex
    QScriptProgram *m_script;
    DebuggerAgent *m_debugger; // Is thread safe
    ScriptErrorType m_lastScriptError;

    const TimetableAccessorInfo *m_info;
    Network *m_scriptNetwork;
    Helper *m_scriptHelper;
    ResultObject *m_scriptResult;
    Storage *m_scriptStorage;
};

}; // namespace Debugger

#endif // Multiple inclusion guard
