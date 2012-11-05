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
 * @brief Contains structures and enumerations to be used with Debugger.
 *
 * @author Friedrich Pülz <fpuelz@gmx.de>
 **/

#ifndef DEBUGGERSTRUCTURES_H
#define DEBUGGERSTRUCTURES_H

// Qt includes
#include <QStringList>
#include <QScriptValue>
#include <QScriptContextInfo>
#include <QStack>
#include <QVariant>
#include <QDebug>

class QModelIndex;
class QScriptContextInfo;
class QScriptEngine;

namespace Debugger {

class BreakpointModel;

class DebuggerAgent;
class BacktraceModel;

/** @brief States of the debugger. */
enum DebuggerState {
    NotRunning = 0, /**< Script is not running. */
    Running, /**< Script is running. */
    Interrupted, /**< Script is interrupted. */
    Aborting /**< Scirpt is currently being aborted. */
};

/** @brief Debug mode used for function arguments. */
enum DebugFlag {
    NeverInterrupt          = 0x0000,

    InterruptAtStart        = 0x0001,
    InterruptOnExceptions   = 0x0002,
    InterruptOnBreakpoints  = 0x0004,

    InterruptOnExceptionsAndBreakpoints = InterruptOnExceptions | InterruptOnBreakpoints,
    DefaultDebugFlags = NeverInterrupt
};
Q_DECLARE_FLAGS( DebugFlags, DebugFlag );

/** @brief Execution control types. */
enum ExecutionControl {
    ExecuteRun = 0, /**< Run script, will be interrupted on breakpoints
            * or uncaught exceptions. */
    ExecuteContinue = ExecuteRun, /**< Continue execution, will be interrupted on breakpoints
            * or uncaught exceptions. */
    ExecuteInterrupt, /**< Interrupt execution at the next statement. */
    ExecuteAbort, /**< Abort debugging. */
    ExecuteAbortInjectedProgram, /**< Abort execution of injected code, but not of the main script. */

    ExecuteStepInto, /**< Interrupted execution at the next statement. */
    ExecuteStepOver, /**< Interrupted execution at the next statement in the same context. */
    ExecuteStepOut, /**< Interrupted execution at the next statement in the parent context. */
//         ExecuteStepOutOfContext // TODO break after leaving a context (ie. a code block)?
    ExecuteRunInjectedProgram, /**< Execute a program injected using evaluateInContext(). */
    ExecuteStepIntoInjectedProgram /**< Execute a program injected using evaluateInContext()
            * and interrupted it at the next statement. */
};

/** @brief Job types. */
enum JobType {
    LoadScript, /**< A job of type LoadScriptJob. */
    EvaluateInContext, /**< A job of type EvaluateInContextJob. */
    ExecuteConsoleCommand, /**< A job of type ExecuteConsoleCommandJob. */
    CallScriptFunction, /**< A job of type CallScriptFunctionJob. */
    TestFeatures, /**< A job of type TestFeaturesJob. */
    TimetableDataRequest /**< A job of type TimetableDataRequestJob. */
};

/** @brief Types of script errors. */
enum ScriptErrorType {
    InitializingScript = -1,
    NoScriptError = 0,
    ScriptLoadFailed,
    ScriptParseError,
    ScriptRunError
};

/** @brief Hints returned Debugger::canBreakAt(). */
enum NextEvaluatableLineHint {
    FoundEvaluatableLine, /**< The tested line is evaluatable. */
    CannotFindNextEvaluatableLine, /**< Cannot find an evaluatable line near the tested line. */
    NextEvaluatableLineAbove, /**< The tested line is not evaluatable, the line above should
            * be tested next. */
    NextEvaluatableLineBelow /**< The tested line is not evaluatable, the line below should
            * be tested next. */
};

/**
 * @brief Information about how script execution stopped.
 *
 * These flags get used externally by DebuggerAgent, Debugger only uses them internally and emits
 * Debugger::stopped() when the script is completely finished, ie. has stopped and has no running
 * network requests. DebuggerAgent::stopped() gets emitted when evaluation in the engine has
 * stopped, but may be waiting for a network request to continue evaluation with the received data.
 **/
enum ScriptStoppedFlag {
    ScriptStopped               = 0x00, /**< Script evaluation has stopped in the engine,
                                           * this flag is always set (value 0x0). */
    ScriptWasAborted            = 0x01, /**< The script was stopped because of an abort, eg.
                                           * because of a call to abortDebugger(). */
    ScriptHasRunningRequests    = 0x02  /**< The script engine stopped (paused) evaluation and is
            * waiting for a network request. It will continue automatically when the request has
            * finished (when the signal is connected to a slot in the script). */
};
Q_DECLARE_FLAGS( ScriptStoppedFlags, ScriptStoppedFlag );

/** @brief Contains information about the result of an evaluation. */
struct EvaluationResult {
    /** @brief Creates a new EvaluationResult object. */
    EvaluationResult( const QString &returnValue = QString() )
            : error(false), errorLineNumber(-1), returnValue(returnValue) {};

    bool error; /**< Whether or not there was an error. */
    int errorLineNumber; /**< The line number, where the error happened, if error is true. */
    QString errorMessage; /**< An error message, if error is true. */
    QStringList backtrace; /**< A backtrace from where the error happened, if error is true. */
    QString returnValue; /**< The return value of the evaluation as string, if error is false. */
};

/** @brief Represents one frame of a backtrace. */
class Frame {
    friend class BacktraceModel;
    friend class DebuggerAgent;

public:
    /** @brief Creates an invalid Frame object. */
    Frame() : m_model(0) {};

    void setValuesOf( const Frame &frame );

    /**
     * @brief The BacktraceModel this Frame belongs to or 0 if this Frame was not added to a model.
     * @note BacktraceModel will delete Frame object when they get removed from the model.
     **/
    inline BacktraceModel *model() const { return m_model; };

    QModelIndex index();

    /** @brief A context string, eg. QScriptContextInfo::function(), if available. */
    inline QString contextString() const { return m_contextString; };

    /** @brief The QScriptContextInfo object of this frame. */
    inline QScriptContextInfo contextInfo() const { return m_contextInfo; };

    /** @see QScriptContextInfo::lineNumber() */
    inline int lineNumber() const { return m_contextInfo.lineNumber(); };

    /** @see QScriptContextInfo::fileName() */
    inline QString fileName() const { return m_contextInfo.fileName(); };

protected:
    /** @brief Creates a new Frame object. */
    Frame( const QScriptContextInfo &contextInfo, bool global = false, BacktraceModel *model = 0 )
            : m_model(model)
    {
        setContextInfo( contextInfo, global );
    };

    void setContextInfo( const QScriptContextInfo &contextInfo, bool global = false );
    inline void setModel( BacktraceModel *model ) { m_model = model; };

private:
    QString m_contextString;
    QScriptContextInfo m_contextInfo;
    BacktraceModel *m_model;
};

/** @brief A stack of frames, ie. a backtrace. */
typedef QStack< Frame* > FrameStack;

/**
 * @brief Represents a breakpoint.
 *
 * Can be used as a simple breakpoint as well as a more advanced one with a condition, which can
 * be written in JavaScript and gets executed in the current engines context if the breakpoint
 * gets reached. Breakpoints can be enabled/disabled manually. If a maximum hit count is reached
 * the breakpoint gets disabled.
 *
 * This class is thread safe.
 **/
class Breakpoint {
    friend class DebuggerAgent;
    friend class BreakpointModel;
public:
    /** @brief States of a breakpoint in a specific line, returned by breakpointState().. */
    enum State {
        NoBreakpoint = 0, /**< No breakpoint in the specific line. */
        EnabledBreakpoint, /**< There is an enabled breakpoint in the specific line. */
        DisabledBreakpoint /**< There is a disabled breakpoint in the specific line. */
    };

    /**
     * @brief Create a new breakpoint at @p lineNumber.
     *
     * @param fileName The name of the file in which this breakpoint should be added.
     * @param lineNumber The line number where to interrupt execution.
     * @param enabled Whether or not the breakpoint should be enabled initially.
     * @param maxHitCount The maximum number of hits for this breakpoint or -1 for infinite hits.
     *   If the maximum hit count gets reached, the breakpoint gets disabled.
     **/
    Breakpoint( const QString &fileName = QString(), int lineNumber = -1,
                bool enabled = true, int maxHitCount = -1 )
        : m_model(0), m_fileName(fileName), m_lineNumber(lineNumber), m_enabled(enabled),
          m_hitCount(0), m_maxHitCount(maxHitCount)
    {
    };

    Breakpoint( const Breakpoint &breakpoint ) : m_model(0)
    { setValuesOf(breakpoint); };

    /** @brief Destructor. */
    virtual ~Breakpoint() {};

    /** @brief Create a one-time breakpoint in @p fileName at @p lineNumber. */
    static Breakpoint *createOneTimeBreakpoint( const QString &fileName, int lineNumber ) {
        return new Breakpoint( fileName, lineNumber, true, 1 );
    };

    /** @brief Whether or not this breakpoint is valid. */
    bool isValid() const { return m_lineNumber > 0; };

    /** @brief The name of the file of this breakpoint. */
    QString fileName() const { return m_fileName; };

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

    BreakpointModel *model() const { return m_model; };

    /** @brief Reset the hit count. */
    void reset();

    /** @brief Enable/disable this breakpoint. */
    void setEnabled( bool enabled = true );

    /** @brief Set the maximum number of hits to @p maximumHitCount. */
    void setMaximumHitCount( int maximumHitCount );

    void setValuesOf( const Breakpoint &breakpoint );

    /** @brief Compares line number and file name, which unambiguously identify a breakpoint. */
    inline bool operator ==( const Breakpoint &other ) const {
        return m_lineNumber == other.m_lineNumber && m_fileName == other.m_fileName;
    };

protected:
    /** @brief Gets called by Debugger if this breakpoint was reached. */
    void reached();

    /**
     * @brief Gets called by Debugger to test if the condition is satisfied.
     *
     * If no condition is set, this always returns true.
     * @param agent The debugger agent to use for evaluating the condition.
     * @param error If not 0, this gets set to true, if there was an error with the condition
     *   that needs to be fixed by the user. Should be initialized with false.
     **/
    bool testCondition( DebuggerAgent *agent, bool *error = 0 );

    void setModel( BreakpointModel *model ) { m_model = model; };

private:
    BreakpointModel *m_model;
    QString m_fileName;
    int m_lineNumber;
    bool m_enabled;
    int m_hitCount;
    int m_maxHitCount;
    QString m_condition;
    QScriptValue m_lastConditionResult;
};

/** @brief Contains information about a console command. */
class ConsoleCommand {
public:
    /** @brief Types of console command types. */
    enum Command {
        InvalidCommand = 0, /**< Invalid command. */
        DebugCommand, /**< Debug command, steps into execution of script code in the current
                * context and interrupts in the new command context. */
        HelpCommand, /**< Provides information about the console or about a specific command,
                * if the command is given as argument. */
        ClearCommand, /**< Clears the console history. */
        LineNumberCommand, /**< Retrieves the current line number of script execution. */
        DebuggerControlCommand, /**< Controls the debugger, eg. interrupt it. */
        BreakpointCommand /**< Add/remove/change breakpoints or get information about
                * existing breakpoints. */
    };

    ConsoleCommand() : m_command(InvalidCommand) {};

    ConsoleCommand( Command command, const QStringList &arguments = QStringList() )
            : m_command(command), m_arguments(arguments) {};

    ConsoleCommand( const QString &name, const QStringList &arguments = QStringList() )
            : m_command(commandFromName(name)), m_arguments(arguments) {};

    static ConsoleCommand fromString( const QString &str );

    bool isValid() const;
    bool commandExecutesScriptCode() const { return m_command == DebugCommand; };

    Command command() const { return m_command; };
    QStringList arguments() const { return m_arguments; };
    QString argument( int i = 0 ) const {
            return i < 0 || i >= m_arguments.count() ? QString() : m_arguments[i].trimmed(); };
    QString description() const;
    QString syntax() const;
    QString toString() const;

    /** Normally true. If false, the command is NOT executed in Debugger::runCommand() */
    bool getsExecutedAutomatically() const;

    static Command commandFromName( const QString &name );
    static QString commandToName( Command command );
    static QStringList availableCommands();
    static QStringList defaultCompletions();
    static QString commandDescription( Command command );
    static QString commandSyntax( Command command );
    static bool getsCommandExecutedAutomatically( Command command );

private:
    const Command m_command;
    const QStringList m_arguments;
};

/* Functions for nicer debug output */
inline QDebug &operator <<( QDebug debug, DebugFlag debugMode )
{
    switch ( debugMode ) {
    case NeverInterrupt:
        return debug << "NeverInterrupt";
    case InterruptAtStart:
        return debug << "InterruptAtStart";
    case InterruptOnExceptions:
        return debug << "InterruptOnExceptions";
    case InterruptOnBreakpoints:
        return debug << "InterruptOnBreakpoints";
    default:
        return debug << "DebugMode unknown" << static_cast<int>(debugMode);
    }
}

inline QDebug &operator <<( QDebug debug, Breakpoint::State state )
{
    switch ( state ) {
    case Breakpoint::NoBreakpoint:
        return debug << "NoBreakpoint";
    case Breakpoint::EnabledBreakpoint:
        return debug << "EnabledBreakpoint";
    case Breakpoint::DisabledBreakpoint:
        return debug << "DisabledBreakpoint";
    default:
        return debug << "DebugMode unknown" << static_cast<int>(state);
    }
}

inline QDebug &operator <<( QDebug debug, DebuggerState state )
{
    switch ( state ) {
    case NotRunning:
        return debug << "NotRunning";
    case Running:
        return debug << "Running";
    case Interrupted:
        return debug << "Interrupted";
    case Aborting:
        return debug << "Aborting";
    default:
        return debug << "DebuggerState unknown" << static_cast<int>(state);
    }
}

inline QDebug &operator <<( QDebug debug, ConsoleCommand::Command command )
{
    switch ( command ) {
    case ConsoleCommand::InvalidCommand:
        return debug << "InvalidCommand";
    case ConsoleCommand::DebugCommand:
        return debug << "DebugCommand";
    case ConsoleCommand::HelpCommand:
        return debug << "HelpCommand";
    case ConsoleCommand::ClearCommand:
        return debug << "ClearCommand";
    case ConsoleCommand::LineNumberCommand:
        return debug << "LineNumberCommand";
    case ConsoleCommand::DebuggerControlCommand:
        return debug << "DebuggerControlCommand";
    case ConsoleCommand::BreakpointCommand:
        return debug << "BreakpointCommand";
    default:
        return debug << "Command unknown" << static_cast<int>(command);
    }
}

inline QDebug &operator <<( QDebug debug, ExecutionControl executeControlType )
{
    switch ( executeControlType ) {
    case ExecuteRun: // case ExecuteContinue: (same value)
        return debug << "ExecuteRun / ExecuteContinue";
    case ExecuteInterrupt:
        return debug << "ExecuteInterrupt";
    case ExecuteAbort:
        return debug << "ExecuteAbort";
    case ExecuteStepInto:
        return debug << "ExecuteStepInto";
    case ExecuteStepOver:
        return debug << "ExecuteStepOver";
    case ExecuteStepOut:
        return debug << "ExecuteStepOut";
    case ExecuteRunInjectedProgram:
        return debug << "ExecuteRunInjectedProgram";
    case ExecuteStepIntoInjectedProgram:
        return debug << "ExecuteStepIntoInjectedProgram";
    default:
        return debug << "Execution control type unknown" << static_cast<int>(executeControlType);
    }
}

}; // namespace Debugger

// Make Frame and FrameStack known to the meta object system
Q_DECLARE_METATYPE( Debugger::Frame );
Q_DECLARE_METATYPE( Debugger::FrameStack );

#endif // Multiple inclusion guard
