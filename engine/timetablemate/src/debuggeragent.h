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
#include "debuggerstructures.h"

// Qt includes
#include <QScriptEngineAgent>
#include <QScriptValue>

class QScriptContext;
class QScriptContextInfo;

namespace Debugger {

/**
 * @brief A QScriptEngineAgent that acts as a debugger.
 *
 * @warning This is an internal class. Do not use this class directly (there is no public
 *   contructor). Instead use Debugger, which manages threads to run and control scripts.
 *   Debugger uses this class internally, possibly simultanously in multiple threads.
 *   Debugger simply forwards many functions to this class but it hides the public
 *   QScriptEngineAgent interface implementation.
 *
 * DebuggerAgent provides common debugger functionality to control script execution like
 * interrupting a running script (debugInterrupt()), continuing after an interrupt
 * (debugContinue()), executing a script step by step (debugStepInto(), debugStepOver(),
 * debugStepOut()), executing until a specific line number (debugRunUntilLineNumber(), eg. until
 * the current cursor position), aborting script execution and breakpoints.
 * Interrupts are handled using a QWaitCondition.
 *
 * The position at which a script got interrupted can be retrieved using lineNumber() and
 * columnNumber().
 *
 * Breakpoints can be added/removed using addBreakpoint(), removeBreakpoint(), toggleBreakpoint(),
 * setBreakpoint(), removeAllBreakpoints(). The signals breakpointAdded() / breakpointRemoved()
 * are then emitted. If a breakpoint is reached the breakpointReached() signal gets emitted and
 * the hit count of the breakpoint gets increased.
 * Breakpoints can have a maximum hit count, after which they will be disabled. They can also have
 * a condition written in JavaScript, which gets executed in the current script context and should
 * return a boolean. If the condition returns true, the breakpoint matches.
 * To get a list of all line numbers which have a breakpoint, use breakpointLines().
 * @warning This class tries to check if the script can break at a given line number in the
 *   canBreakAt() function, but this does not always work. For example, if the script should be
 *   interrupted at a multiline statement, it must be set at the first line of the statement.
 *   Otherwise the breakpoint will never be hit.
 *
 * @note Line numbers are expected to begin with 1 for the first line like in QScript-classes,
 *   rather than being zero-based like line numbers in KTextEditor.
 *
 * This class is thread safe. There is a mutex to protect member variables and a global mutex
 * to protect the QScriptEngine.
 *
 * @see Debugger
 **/
class DebuggerAgent : public QObject, public QScriptEngineAgent {
    Q_OBJECT
    friend class Debugger;
    friend class Breakpoint;

public:
    static const int CHECK_RUNNING_INTERVAL;
    static const int CHECK_RUNNING_WHILE_INTERRUPTED_INTERVAL;

    /** @brief Destructor. */
    virtual ~DebuggerAgent();

    static QString stateToString( DebuggerState state );

    /** @brief Gets the current state of the debugger. */
    DebuggerState state() const { return m_state; };

    /** @brief Whether or not script execution is currently interrupted. */
    bool isInterrupted() const;

    /** @brief Whether or not the script currently gets executed. */
    bool isRunning() const;

    /** @brief Get the state of the breakpoint at @p lineNumber or NoBreakpoint if there is none. */
    Breakpoint::State breakpointState( int lineNumber ) const;

    Variables variables( int frame = 0 ); // frame==0 current frame, frame==1 previous frame, ...

    /** @brief Get the current backtrace as list of Frame objects, ie. a FrameStack. */
    FrameStack backtrace() const;

    /** @brief Compares @p backtrace with @p oldBacktrace. */
    BacktraceChange compareBacktraces( const FrameStack &backtrace,
                                       const FrameStack &oldBacktrace ) const;

    /** @brief Get a list of line numbers with breakpoints. */
    QList< uint > breakpointLines() const;

    /** @brief Remove the given @p breakpoint. */
    bool removeBreakpoint( const Breakpoint &breakpoint );

    /** @brief Remove the breakpoint at @p lineNumber. */
    bool removeBreakpoint( int lineNumber );

    /** @brief Add the given @p breakpoint, existing breakpoints at the same line are overwritten. */
    bool addBreakpoint( const Breakpoint &breakpoint );

    /** @brief Adds/removes a breakpoint at @p lineNumber. */
    Breakpoint setBreakpoint( int lineNumber, bool enable = true );

    /** @brief Toggle breakpoint at @p lineNumber. */
    Breakpoint toggleBreakpoint( int lineNumber );

    /** @brief Get the breakpoint at @p lineNumber or an invalid breakpoint. */
    Breakpoint breakpointAt( int lineNumber ) const;

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
    NextEvaluatableLineHint canBreakAt( int lineNumber ) const;

    /**
     * @brief Get the first executable line number bigger than or equal to @p lineNumber.
     *
     * This function uses canBreakAt() to check whether or not script execution can be interrupted.
     * If not, the line number gets increased and again checked, etc.
     * If no such line number could be found -1 gets returned.
     **/
    int getNextBreakableLineNumber( int lineNumber ) const;

    /** @brief Get the start line number of the currently executed function. */
    int currentFunctionStartLineNumber() const;

    /** @brief The current line number. */
    int lineNumber() const;

    /** @brief The current column number. */
    int columnNumber() const;

    bool hasUncaughtException() const;
    int uncaughtExceptionLineNumber() const;
    QScriptValue uncaughtException() const;

    QScriptValue evaluateInContext( const QString &program, const QString &contextName,
                                    bool *hadUncaughtException = 0, int *errorLineNumber = 0,
                                    QString *errorMessage = 0, QStringList *backtrace = 0,
                                    bool interruptAtStart = false );

    /** @brief Executes @p command and puts the return value into @p returnValue. */
    bool executeCommand( const ConsoleCommand &command, QString *returnValue = 0 );

    ExecutionControl currentExecutionControlType() const;
    void setExecutionControlType( ExecutionControl executionType );

    bool checkHasExited();

signals:
    /** @brief Script execution just started. */
    void started();

    /** @brief The script finished and is no longer running */
    void stopped();

    /** @see isInterrupted() */
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

    void evaluationInContextFinished( const QScriptValue& returnValue = QScriptValue() );

public slots:
    /** @brief Continue script execution until the next statement. */
    void debugStepInto( int repeat = 0 );

    /** @brief Continue script execution until the next statement in the same context. */
    void debugStepOver( int repeat = 0 );

    /** @brief Continue script execution until the current function gets left. */
    void debugStepOut( int repeat = 0 );

    /** @brief Continue script execution until @p lineNumber is reached. */
    void debugRunUntilLineNumber( int lineNumber );

    /** @brief Interrupt script execution. */
    void debugInterrupt();

    /** @brief Continue script execution, only interrupt on breakpoints or uncaught exceptions. */
    void debugContinue();

    /** @brief Abort script execution. */
    void abortDebugger();

    /** @brief Remove all breakpoints, for each removed breakpoint breakpointRemoved() gets emitted. */
    void removeAllBreakpoints();

    void slotOutput( const QString &outputString, const QScriptContextInfo &contextInfo );

    void checkExecution();

protected slots:
    void debugRunInjectedProgram();
    void debugStepIntoInjectedProgram();
    void cancelInjectedCodeExecution();

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
    /** Overwritten to get noticed when a script might have finished. */
    virtual void functionExit( qint64 scriptId, const QScriptValue& returnValue );

protected:
    /** @brief Creates a new Debugger instance. */
    DebuggerAgent( QScriptEngine *engine, QMutex *engineMutex );

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

    // Expects locked m_mutex
    ExecutionControl applyExecutionControl( ExecutionControl executionControl );

    // Expects locked m_mutex, looks for breakpoints at the current execution position
    // (tests conditions, enabled/disabled)
    Breakpoint *findActiveBreakpoint( int lineNumber );

    // Expects locked m_mutex
    FrameStack buildBacktrace() const;

    // Expects locked m_mutex
    void cleanupBacktrace();

    // Expects locked m_mutex
    int currentFunctionLineNumber() const;

    // Does not need locked m_mutex
    Variables buildVariables( const QScriptValue &value, bool onlyImportantObjects = false ) const;

    // Does not need locked m_mutex
    QString variableValueTooltip( const QString &completeValueString, bool encodeHtml,
                                  const QChar &endCharacter ) const;

    // Excepts unlocked m_mutex
    void fireup();

    // Excepts unlocked m_mutex
    void shutdown();

    // Excepts unlocked m_mutex
    void doInterrupt( bool injectedProgram = false );

    // Excepts unlocked m_mutex
    bool debugControl( ConsoleCommandExecutionControl controlType,
                       const QVariant &argument = QVariant(), QString *errorMessage = 0 );

    static ConsoleCommandExecutionControl consoleCommandExecutionControlFromString( const QString &str );

    QMutex *m_mutex; // Protect member variables, make Debugger class thread safe
    QWaitCondition *m_interruptWaiter; // Waits on interrupts, wake up to continue script execution based on m_executionType
    QMutex *m_interruptMutex;
    QMutex *m_engineMutex; // use carefully, eg. tryLock() instead of lock().. it is blocked while the script is running, until it gets interrupted or finishes
    QTimer *m_checkRunningTimer;

    int m_lineNumber;
    int m_columnNumber;
    bool m_hasUncaughtException;
    int m_uncaughtExceptionLineNumber;
    QScriptValue m_uncaughtException;
    QScriptValue m_globalObject;
    QHash< uint, Breakpoint > m_breakpoints;
    FrameStack m_lastBacktrace;

    DebuggerState m_state;
    ExecutionControl m_executionControl;
    int m_repeatExecutionTypeCount;
    QScriptContext *m_currentContext;
    QScriptContext *m_interruptContext;
    bool m_backtraceCleanedup;
    int m_injectedCodeContextLevel;

    int m_currentFunctionLineNumber;
    int m_interruptFunctionLineNumber;
    int m_interruptFunctionLevel;

    QStringList m_scriptLines;
};

}; // namespace Debugger

#endif // Multiple inclusion guard
