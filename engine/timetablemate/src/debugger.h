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

#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <QScriptEngineAgent>
#include <QScriptContextInfo>
#include <QScriptValue>
#include <QQueue>

typedef QQueue< QScriptContextInfo > BacktraceQueue;

class QScriptEngine;
class Debugger;

/**
 * @brief Represents a breakpoint.
 *
 * Can be used as a simple breakpoint as well as a more advanced one with a condition, which can
 * be written in JavaScript and gets executed in the current engines context if the breakpoint
 * gets reached. Breakpoints can be enabled/disabled manually. If a maximum hit count is reached
 * the breakpoint gets disabled.
 **/
class Breakpoint {
    friend class Debugger;
public:
    /**
     * @brief Create a new breakpoint at @p lineNumber.
     *
     * @param lineNumber The line number where to interrupt execution.
     * @param enabled Whether or not the breakpoint should be enabled initially.
     * @param maxHitCount The maximum number of hits for this breakpoint or -1 for infinite hits.
     *   If the maximum hit count gets reached, the breakpoint gets disabled.
     **/
    Breakpoint( int lineNumber = -1, bool enabled = true, int maxHitCount = -1 ) {
        m_lineNumber = lineNumber;
        m_enabled = enabled;
        m_hitCount = 0;
        m_maxHitCount = maxHitCount;
    };

    /** @brief Create a one-time breakpoint at @p lineNumber. */
    static Breakpoint oneTimeBreakpoint( int lineNumber ) {
        return Breakpoint( lineNumber, true, 1 );
    };

    /** @brief Whether or not this breakpoint is valid. */
    bool isValid() const { return m_lineNumber > 0; };

    /** @brief The line number of this breakpoint. */
    int lineNumber() const { return m_lineNumber; };

    /** @brief Whether or not this breakpoint is currently enabled. */
    bool isEnabled() const { return m_enabled; };

    /** @brief The number of hits of this breakpoint since the last call of reset(). */
    int hitCount() const { return m_hitCount; };

    /** @brief The maximum number of hits, the breakpoint gets disabled after the last hit. */
    int maximumHitCount() const { return m_maxHitCount; };

    /** @brief The condition of this breakpoint. */
    QString condition() const { return m_condition; };

    /**
     * @brief Set the condition for this breakpoint to @p condition.
     *
     * If @p condition is an empty string, the breakpoint has no condition.
     * Otherwise @p condition gets evaluated in the current engines context if the breakpoint gets
     * reached. The evaluation should return a boolean QScriptValue.
     * Every occurance of "%HITS" in @p condition gets replaced by the number of hits of this
     * breakpoint, see hitCount(). This makes it possible to eg. create a breakpoint which only
     * breaks after the ten first hits with a condition like @verbatim%HITS >= 10@endverbatim.
     **/
    void setCondition( const QString &condition = QString() );

    /** @brief Get the result of the last condition evaluation. */
    QScriptValue lastConditionResult() const { return m_lastConditionResult; };

    /** @brief Reset the hit count. */
    void reset() { m_hitCount = 0; };

    /** @brief Enable/disable this breakpoint. */
    void setEnabled( bool enabled = true ) { m_enabled = enabled; };

    /** @brief Set the maximum number of hits to @p maximumHitCount. */
    void setMaximumHitCount( int maximumHitCount ) { m_maxHitCount = maximumHitCount; };

protected:
    /** @brief Gets called by Debugger if this breakpoint was reached. */
    void reached();

    /** @brief Gets called by Debugger to test if the condition is satisfied. */
    bool testCondition( QScriptEngine *engine );

private:
    int m_lineNumber;
    bool m_enabled;
    int m_hitCount;
    int m_maxHitCount;
    QString m_condition;
    QScriptValue m_lastConditionResult;
};

/**
 * @brief A QScriptEngineAgent that acts as a debugger.
 *
 * Provides common debugger functionality to control script execution like interrupting a running
 * script (debugInterrupt()), continuing after an interrupt (debugContinue()), executing a script
 * step by step (debugStepInto(), debugStepOver(), debugStepOut()), executing until a specific
 * line number (debugRunUntilLineNumber(), eg. until the current cursor position) or aborting
 * script execution.
 *
 * The position at which a script got interrupted can be retrieved using lineNumber() and
 * columnNumber().
 *
 * Breakpoints can be added/removed using addBreakpoint(), removeBreakpoint(), toggleBreakpoint(),
 * setBreakpoint(), removeAllBreakpoints(). The signals breakpointAdded() / breakpointRemoved()
 * are then emitted. If a breakpoint is reached the breakpointReached() signal gets emitted and
 * the hit count of the breakpoint gets increased.
 * Breakpoints can have a maximum hit count, after which they will be deleted. (TODO deleted -> disabled)
 * They can also have a condition written in JavaScript, which gets executed in the current script
 * context and should return a boolean. If the condition returns true, the breakpoint matches.
 * To get a list of all line numbers which have a breakpoint, use breakpoints().
 * @warning This class tries to check if the script can break at a given line number in the
 *   canBreakAt() function, but this does not always work. For example, if the script should be
 *   interrupted at a multiline statement, it must be set at the first line of the statement.
 *   Otherwise the breakpoint will never be hit.
 *
 * @note Line numbers are expected to begin with 1 for the first line like in QScript-classes,
 *   rather than being zero-based like line numbers in KTextEditor.
 **/
class Debugger : public QObject, public QScriptEngineAgent {
    Q_OBJECT
    friend class Breakpoint;

public:
    /** @brief States of a breakpoint in a specific line, returned by breakpointState().. */
    enum BreakpointState {
        NoBreakpoint = 0, /**< No breakpoint in the specific line. */
        EnabledBreakpoint, /**< There is an enabled breakpoint in the specific line. */
        DisabledBreakpoint /**< There is a disabled breakpoint in the specific line. */
    };

    /** @brief Execution types. */
    enum ExecutionType {
        ExecuteNotRunning = 0, /**< Script is not running. */
        ExecuteRun, /**< Script is running, will be interrupted on breakpoints
                * or uncaught exceptions. */
        ExecuteInterrupt, /**< Script is running, will be interrupted at the next statement. */
        ExecuteStepInto, /**< Script is running, will be interrupted at the next statement. */
        ExecuteStepOver, /**< Script is running, will be interrupted at the next statement
                * in the same context. */
        ExecuteStepOut /**< Script is running, will be interrupted at the next statement
                * in the parent context. */
//         ExecuteStepOutOfContext // TODO break after leaving a context (ie. a code block)
    };

    /** @brief Changes between two backtrace queues, returned by compareBacktraces(). */
    enum BacktraceChange {
        NoBacktraceChange = 0, /**< No change between the two backtrace queues found. */
        EnteredFunction, /**< A function was entered after the first backtrace. */
        ExitedFunction /**< A function was exited after the first backtrace. */
    };

    /** @brief Hints returned canBreakAt(). */
    enum NextEvaluatableLineHint {
        FoundEvaluatableLine, /**< The tested line is evaluatable. */
        CannotFindNextEvaluatableLine, /**< Cannot find an evaluatable line near the tested line. */
        NextEvaluatableLineAbove, /**< The tested line is not evaluatable, the line above should
                * be tested next. */
        NextEvaluatableLineBelow, /**< The tested line is not evaluatable, the line below should
                * be tested next. */
    };

    /** @brief Creates a new Debugger instance. */
    Debugger( QScriptEngine* engine = 0 );

    /** @brief Destructor. */
    virtual ~Debugger();

    /** @brief Whether or not script execution is currently interrupted. */
    bool isInterrupted() const { return m_executionType != ExecuteRun; };

    /** @brief Get the state of the breakpoint at @p lineNumber or NoBreakpoint if there is none. */
    BreakpointState breakpointState( int lineNumber ) const {
        return !m_breakpoints.contains(lineNumber) ? NoBreakpoint
                : (m_breakpoints[lineNumber].isEnabled() ? EnabledBreakpoint : DisabledBreakpoint);
    };

    /** @brief Get the current backtrace as list of QScriptContextInfo objects. */
    BacktraceQueue backtrace() const { return m_lastBacktrace; };

    /** @brief Compares @p backtrace with @p oldBacktrace. */
    BacktraceChange compareBacktraces( const BacktraceQueue &backtrace,
                                       const BacktraceQueue &oldBacktrace ) const;

    /** @brief Get a list of line numbers with breakpoints. */
    QList<int> breakpoints() const { return m_breakpoints.keys(); };

    /** @brief Remove the given @p breakpoint. */
    void removeBreakpoint( const Breakpoint &breakpoint );

    /** @brief Remove the breakpoint at @p lineNumber. */
    void removeBreakpoint( int lineNumber );

    /** @brief Add the given @p breakpoint, existing breakpoints at the same line are overwritten. */
    bool addBreakpoint( const Breakpoint &breakpoint );

    /** @brief Adds/removes a breakpoint at @p lineNumber. */
    Breakpoint setBreakpoint( int lineNumber, bool enable = true );

    /** @brief Toggle breakpoint at @p lineNumber. */
    Breakpoint toggleBreakpoint( int lineNumber ) {
        BreakpointState breakpoint = breakpointState( lineNumber );
        return setBreakpoint( lineNumber, breakpoint == NoBreakpoint );
    };

    /** @brief Get the breakpoint at @p lineNumber or an invalid breakpoint. */
    Breakpoint breakpointAt( int lineNumber ) const {
            return m_breakpoints.contains(lineNumber) ? m_breakpoints[lineNumber] : Breakpoint(); };

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
    int currentFunctionStartLineNumber() const { return m_currentFunctionLineNumber; };

    /** @brief The current line number. */
    int lineNumber() const { return m_lineNumber; };

    /** @brief The current column number. */
    int columnNumber() const { return m_columnNumber; };

signals:
    /** @brief The script finished and is no longer running */
    void scriptFinished();

    /** @brief Script execution just started. */
    void scriptStarted();

    /** @see isInterrupted() */
    void positionChanged( int lineNumber, int columnNumber );

    /** @brief . */
    void positionAboutToChanged( int oldLineNumber, int oldColumnNumber,
                                 int lineNumber, int columnNumber );

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
    void backtraceChanged( const BacktraceQueue &backtrace, Debugger::BacktraceChange change );

    /** @brief The script send a @p debugString from @p context using the print() function. */
    void output( const QString &debugString, QScriptContext *context );

public slots:
    void checkExecution();

    /** @brief Continue script execution until the next statement. */
    void debugStepInto();

    /** @brief Continue script execution until the next statement in the same context. */
    void debugStepOver();

    /** @brief Continue script execution until the current function gets left. */
    void debugStepOut();

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

    void slotOutput( const QString &outputString, QScriptContext *context ) {
            emit output(outputString, context); };

public:
//     virtual bool supportsExtension( Extension extension ) const { return true; };
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

private:
    BacktraceQueue buildBacktrace() const;
    int currentFunctionLineNumber() const;

    int m_lineNumber;
    int m_columnNumber;
    QHash< int, Breakpoint > m_breakpoints;
    BacktraceQueue m_lastBacktrace;

    ExecutionType m_executionType;
    QScriptContext *m_lastContext;
    QScriptContext *m_interruptContext;
    bool m_backtraceCleanedup;

    int m_currentFunctionLineNumber;
    int m_interruptFunctionLineNumber;

    QStringList m_scriptLines;
    bool m_running;
};

#endif // Multiple inclusion guard
