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
 * @brief Contains the a debugger QScriptEngineAgent.
 *
 * @author Friedrich Pülz <fpuelz@gmx.de>
 **/

#ifndef DEBUGGER_H
#define DEBUGGER_H

// Own includes
#include "debug_config.h"
#include "debuggerstructures.h"

// Qt includes
#include <QScriptEngineAgent>

struct ScriptObjects;

class QMutex;
class QTimer;
class QWaitCondition;
class QScriptContext;
class QScriptContextInfo;

namespace Debugger {

struct VariableChange;
struct BacktraceChange;
struct BreakpointChange;

/**
 * @brief A QScriptEngineAgent that acts as a debugger.
 *
 * @warning This is an internal class. Do not use this class directly (there is no public
 *   contructor). Instead use Debugger, which manages threads to run and control scripts.
 *   Debugger uses this class internally through thread jobs, ie. classes derived from DebuggerJob.
 *   It can safely be used in multiple threads simultanously. Debugger simply forwards/connects
 *   many functions/signals to this class (through functions/signals of DebuggerJob objects).
 *
 * DebuggerAgent provides common debugger functionality to control script execution like
 * interrupting a running script (debugInterrupt()), continuing after an interrupt
 * (debugContinue()), executing a script step by step (debugStepInto(), debugStepOver(),
 * debugStepOut()), executing until a specific line number (debugRunUntilLineNumber(), eg. until
 * the current cursor position), aborting script execution and managing breakpoints.
 * Interrupts are handled using a QWaitCondition.
 *
 * The position at which a script got interrupted can be retrieved using lineNumber() and
 * columnNumber().
 *
 * Breakpoints can be added/removed using addBreakpoint(), removeBreakpoint() and
 * updateBreakpoint(). If a breakpoint is reached the breakpointReached() signal gets emitted and
 * the hit count of the breakpoint gets increased. These functions are used by BreakpointModel to
 * synchronize breakpoints in this class with those in the breakpoint model. Normally
 * BreakpointModel should be used to alter breakpoints, but if the BreakpointModel should not be
 * notified about changes use the breakpoint function of this class.
 * @warning This class tries to check if the script can break at a given line number in the
 *   canBreakAt() function, but this does not always work. For example, if the script should be
 *   interrupted at a multiline statement, it must be set at the first line of the statement.
 *   Otherwise the breakpoint will never be hit.
 *
 * Console commands can be executed using executeCommand(). To evaluate script code in the context
 * of a currently interrupted script use evaluateInContext().
 *
 * @note Line numbers are expected to begin with 1 for the first line like in QScript-classes by
 *   default, rather than being zero-based like line numbers in KTextEditor.
 *
 * This class is thread safe. There is a mutex to protect member variables and a global mutex
 * to protect access to the QScriptEngine.
 *
 * @see Debugger
 **/
class DebuggerAgent : public QObject, public QScriptEngineAgent {
    Q_OBJECT
    friend class Breakpoint;
    friend class Frame;
    friend class DebuggerJob;

public:
    static const int CHECK_RUNNING_INTERVAL;
    static const int CHECK_RUNNING_WHILE_INTERRUPTED_INTERVAL;

    /** @brief Destructor. */
    virtual ~DebuggerAgent();

    static QString stateToString( DebuggerState state );

    /** @brief Gets the current state of the debugger. */
    DebuggerState state() const;

    /** @brief Whether or not script execution is currently interrupted. */
    bool isInterrupted() const;

    /** @brief Whether or not the script currently gets executed or is interrupted. */
    bool isRunning() const;

    /** @brief Whether or not the script currently gets aborted. */
    bool isAborting() const;

    /**
     * @brief Blocks until the debugger has been completely shut down.
     *
     * If the debugger is not running, this function returns immediately.
     * This function should be called before starting another execution to ensure that the debugger
     * state stays clean. Otherwise there may be crashes and unexpected behaviour.
     **/
    void finish() const;

    QString mainScriptFileName() const;
    void setMainScriptFileName( const QString &mainScriptFileName );

    /**
     * @brief Whether or not the last script execution was aborted.
     * Returns only true, if the debugger currently is @em not running and refers to the last
     * script execution.
     * @see abortDebugger()
     **/
    bool wasLastRunAborted() const;

    /**
     * @brief Checks whether script execution can be interrupted at @p lineNumber.
     *
     * Empty lines or lines with '//' at the beginning are not executable and script execution
     * can not be interrupted there, for example.
     *
     * If the line at @p lineNumber is not evaluatable, the line and the following line are tested
     * together. Up to 25 following lines are currently used to test if there is an evaluatable
     * multiline statement starting at @p lineNumber.
     *
     * @warning This does not always work. The breakpoint may always be skipped although this
     *   function says it could break there.
     **/
    NextEvaluatableLineHint canBreakAt( const QString &fileName, int lineNumber ) const;

    /**
     * @brief Get the first executable line number bigger than or equal to @p lineNumber.
     *
     * This function uses canBreakAt() to check whether or not script execution can be interrupted.
     * If not, the line number gets increased and again checked, etc.
     * If no such line number could be found -1 gets returned.
     **/
    int getNextBreakableLineNumber( const QString &fileName, int lineNumber ) const;

    static NextEvaluatableLineHint canBreakAt( int lineNumber, const QStringList &programLines );
    static int getNextBreakableLineNumber( int lineNumber, const QStringList &programLines );
    inline static NextEvaluatableLineHint canBreakAt( int lineNumber, const QString &program )
    {
        return canBreakAt(lineNumber, program.split('\n', QString::KeepEmptyParts));
    };
    inline static int getNextBreakableLineNumber( int lineNumber, const QString &program )
    {
        return getNextBreakableLineNumber(lineNumber, program.split('\n', QString::KeepEmptyParts));
    };

    /** @brief The name of the currently executed source file. */
    QString currentSourceFile() const;

    /** @brief The current line number. */
    int lineNumber() const;

    /** @brief The current column number. */
    int columnNumber() const;

    QScriptContextInfo contextInfo();

    /** @brief Whether or not an uncaught exception was thrown in the script. */
    bool hasUncaughtException() const;
    int uncaughtExceptionLineNumber() const;
    QScriptValue uncaughtException() const;

    /** @brief Evaluate @p program in the context of an interrupted script. */
    QScriptValue evaluateInContext( const QString &program, const QString &contextName,
                                    bool *hadUncaughtException = 0, int *errorLineNumber = 0,
                                    QString *errorMessage = 0, QStringList *backtrace = 0,
                                    DebugFlags debugFlags = DefaultDebugFlags );

    /** @brief Executes @p command and puts the return value into @p returnValue. */
    bool executeCommand( const ConsoleCommand &command, QString *returnValue = 0 );

    ExecutionControl currentExecutionControlType() const;
    void setExecutionControlType( ExecutionControl executionType );

    DebugFlags debugFlags() const;
    void setDebugFlags( DebugFlags debugFlags );

    bool checkHasExited();

    /**
     * @brief Get a pointer to the mutex used to protect access to the script engine.
     *
     * The engine mutex is locked while the script engine is evaluating, gets unlocked after
     * every line of code and while the debugger is interrupted. Gets locked/unlocked from
     * different threads.
     * This always returns the same pointer for the livetime of this DebuggerAgent.
     **/
    QMutex *engineMutex() const { return m_engineMutex; };

    /**
     * @brief Continue an interrupted script to emit doSomething() and directly interrupts again.
     * @see doSomething()
     **/
    void continueToDoSomething();

signals:
    /**
     * @brief Emitted when interrupting directly after waking from interrupt.
     *
     * Can be used together with continueToDoSomething() to execute code in the thread of this
     * DebuggerAgent, the connection to this signal needs to be direct (Qt::DirectConnection).
     **/
    void doSomething();

    /** @brief Script execution just started. */
    void started( const QDateTime &timestamp );

    /** @brief The script finished and is no longer running */
    void stopped( const QDateTime &timestamp, bool aborted = false,
                  bool hasRunningRequests = false, int uncaughtExceptionLineNumber = -1,
                  const QString &uncaughtException = QString(),
                  const QStringList &backtrace = QStringList() );

    /** @see isInterrupted() */
    void positionChanged( int lineNumber, int columnNumber, int oldLineNumber, int oldColumnNumber );

    /** @brief The state of the debugger has changed from @p oldState to @p newState. */
    void stateChanged( DebuggerState newState, DebuggerState oldState );

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

    /**
     * @brief Evaluation of script code in the context of a running script has finished.
     * This signal gets emitted after calling evaluateInContext().
     *
     * @p returnValue The value that was returned from the code that was evaluated.
     **/
    void evaluationInContextFinished( const QScriptValue &returnValue = QScriptValue() );

    void evaluationInContextAborted( const QString &errorMessage );

    /** @brief Variables have changed according to @p change. */
    void variablesChanged( const VariableChange &change );

    /** @brief The backtrace has changed according to @p change. */
    void backtraceChanged( const BacktraceChange &change );

    /** @brief Breakpoints have changed according to @p change. */
    void breakpointsChanged( const BreakpointChange &change );

public slots:
    /**
     * @brief Continue script execution until the next statement.
     * @param repeat The number of repetitions of the step into action.
     *   Use 0 to only execute the action once, ie. no repetitions, the default.
     **/
    void debugStepInto( int repeat = 0 );

    /**
     * @brief Continue script execution until the next statement in the same context.
     * @param repeat The number of repetitions of the step over action.
     *   Use 0 to only execute the action once, ie. no repetitions, the default.
     **/
    void debugStepOver( int repeat = 0 );

    /**
     * @brief Continue script execution until the current function gets left.
     * @param repeat The number of repetitions of the step out action.
     *   Use 0 to only execute the action once, ie. no repetitions, the default.
     **/
    void debugStepOut( int repeat = 0 );

    /** @brief Continue script execution until @p lineNumber is reached. */
    void debugRunUntilLineNumber( const QString &fileName, int lineNumber );

    /** @brief Interrupt script execution. */
    void debugInterrupt();

    /** @brief Continue script execution, only interrupt on breakpoints or uncaught exceptions. */
    void debugContinue();

    /** @brief Abort script execution. */
    void abortDebugger();

    void addBreakpoint( const Breakpoint &breakpoint );
    void updateBreakpoint( const Breakpoint &breakpoint ) { addBreakpoint(breakpoint); };
    void removeBreakpoint( const Breakpoint &breakpoint );

    void slotOutput( const QString &outputString, const QScriptContextInfo &contextInfo );

    void checkExecution();

protected slots:
    void debugRunInjectedProgram();
    void debugStepIntoInjectedProgram();
    void cancelInjectedCodeExecution();
    void wakeFromInterrupt( DebuggerState unmodifiedState );
    inline void wakeFromInterrupt() { wakeFromInterrupt(m_state); };

public: // QScriptAgent-Implementation
    virtual void scriptLoad( qint64 id, const QString &program, const QString &fileName, int baseLineNumber );
    virtual void scriptUnload( qint64 id );
    virtual void positionChange( qint64 scriptId, int lineNumber, int columnNumber );
    virtual void contextPush();
    virtual void contextPop();
    virtual void exceptionCatch( qint64 scriptId, const QScriptValue &exception );
    virtual void exceptionThrow( qint64 scriptId, const QScriptValue &exception, bool hasHandler );
    virtual QVariant extension( Extension extension, const QVariant &argument = QVariant() );
    virtual void functionEntry( qint64 scriptId );
    virtual void functionExit( qint64 scriptId, const QScriptValue& returnValue );

    void setScriptText( const QString &fileName, const QString &program );

protected:
    /** @brief Creates a new Debugger instance. */
    DebuggerAgent( QScriptEngine *engine, QMutex *engineMutex, bool mutexIsLocked = false );

private:
    enum ConsoleCommandExecutionControl {
        InvalidControlExecution = 0,
        ControlExecutionContinue,
        ControlExecutionInterrupt,
        ControlExecutionAbort,
        ControlExecutionStepInto,
        ControlExecutionStepOver,
        ControlExecutionStepOut,
        ControlExecutionRunUntil
    };

    enum InjectedScriptState {
        InjectedScriptNotRunning = 0,
        InjectedScriptEvaluating,
        InjectedScriptAborting,
        InjectedScriptInitializing,
        InjectedScriptUpdateVariablesInParentContext
    };

    ExecutionControl applyExecutionControl( ExecutionControl executionControl,
                                            QScriptContext *currentContext );

    // Looks for breakpoints at the current execution position (tests conditions, enabled/disabled)
    bool findActiveBreakpoint( int lineNumber, Breakpoint *&foundBreakpoint, bool *conditionError );

    int currentFunctionLineNumber() const;
    void fireup();
    void shutdown();

    void doInterrupt( bool injectedProgram = false );

    // Emit changes in the backtrace and the variables in the current context
    // Is called by doInterrupt() to have models updated
    void emitChanges();

    bool debugControl( ConsoleCommandExecutionControl controlType,
                       const QVariant &argument = QVariant(), QString *errorMessage = 0 );
    void setState( DebuggerState newState );
    static ConsoleCommandExecutionControl consoleCommandExecutionControlFromString( const QString &str );

    inline QHash< uint, Breakpoint > &currentBreakpoints() {
        return breakpointsForFile( m_scriptIdToFileName[m_currentScriptId] );
    };
    inline QHash< uint, Breakpoint > &breakpointsForFile( const QString &fileName ) {
        return m_breakpoints[ fileName ];
    };

    QMutex *const m_mutex; // Protect member variables, make Debugger class thread safe
    QWaitCondition *const m_interruptWaiter; // Waits on interrupts, wake up to continue script execution based on m_executionType
    QMutex *const m_interruptMutex;
    QMutex *const m_engineMutex; // Is locked while the script engine is evaluating,
            // gets unlocked after every line of code and while the debugger is interrupted
    QTimer *const m_checkRunningTimer;

    int m_lineNumber;
    int m_columnNumber;
    bool m_lastRunAborted;
    bool m_hasUncaughtException;
    int m_uncaughtExceptionLineNumber;
    QScriptValue m_uncaughtException;
    QStringList m_uncaughtExceptionBacktrace;
    QHash< QString, QHash< uint, Breakpoint > > m_breakpoints; // Outer key: filename, inner key: line number
    int m_runUntilLineNumber; // -1 => "run until line number" not active

    DebuggerState m_state;
    DebugFlags m_debugFlags;
    InjectedScriptState m_injectedScriptState;
    ExecutionControl m_executionControl;
    ExecutionControl m_previousExecutionControl;
    int m_repeatExecutionTypeCount;
    QScriptContext *m_currentContext;
    QScriptContext *m_interruptContext;
    qint64 m_injectedScriptId;
    qint64 m_currentScriptId;

    int m_interruptFunctionLevel;
    int m_functionDepth;

    QHash< qint64, QString > m_scriptIdToFileName;
    QString m_mainScriptFileName;
    QHash< QString, QStringList > m_scriptLines;
};

}; // namespace Debugger

#endif // Multiple inclusion guard
