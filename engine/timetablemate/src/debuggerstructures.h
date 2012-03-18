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

#define DEBUGGER_DEBUG(x) //kDebug() << x

// KDE includes
#include <KIcon>
#include <KColorScheme>

// Qt includes
#include <QStringList>
#include <QScriptValue>
#include <QStack>
#include <QDebug>

class QMutex;
class QWaitCondition;
class QScriptEngine;

namespace Debugger {

class DebuggerAgent;

/** @brief States of the debugger. */
enum DebuggerState {
    NotRunning = 0, /**< Script is not running. */
    Running, /**< Script is running. */
    Interrupted /**< Script is interrupted. */
};

/** @brief Debug mode used for function arguments. */
enum DebugMode {
    InterruptOnExceptions,
    InterruptAtStart
};

/** @brief Execution control types. */
enum ExecutionControl {
    ExecuteRun = 0, /**< Run execution, will be interrupted on breakpoints
            * or uncaught exceptions. */
    ExecuteContinue = ExecuteRun, /**< Continue execution, will be interrupted on breakpoints
            * or uncaught exceptions. */
    ExecuteInterrupt, /**< Interrupt execution at the next statement. */
    ExecuteAbort, /**< Abort debugging at the next statement. */

    ExecuteStepInto, /**< Interrupted execution at the next statement. */
    ExecuteStepOver, /**< Interrupted execution at the next statement in the same context. */
    ExecuteStepOut, /**< Interrupted execution at the next statement in the parent context. */
//         ExecuteStepOutOfContext // TODO break after leaving a context (ie. a code block)?
    ExecuteRunInjectedProgram, /**< Execute a program injected using evaluateInContext(). */
    ExecuteStepIntoInjectedProgram /**< Execute a program injected using evaluateInContext()
            * and interrupted it at the next statement. */
};

/** @brief Changes between two backtrace queues, returned by Debugger::compareBacktraces(). */
enum BacktraceChange {
    NoBacktraceChange = 0, /**< No change between the two backtrace queues found. */
    EnteredFunction, /**< A function was entered after the first backtrace. */
    ExitedFunction /**< A function was exited after the first backtrace. */
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

/** @brief Contains information about the result of an evaluation. */
struct EvaluationResult {
    /** @brief Creates a new EvaluationResult object. */
    EvaluationResult( const QScriptValue &returnValue = QScriptValue() )
            : error(false), errorLineNumber(-1), returnValue(returnValue) {};

    bool error; /**< Whether or not there was an error. */
    int errorLineNumber; /**< The line number, where the error happened, if error is true. */
    QString errorMessage; /**< An error message, if error is true. */
    QStringList backtrace; /**< A backtrace from where the error happened, if error is true. */
    QScriptValue returnValue; /**< The return value of the evaluation, if error is false. */
};

/** @brief Represents one frame of a backtrace. */
struct Frame {
    /** @brief Creates a new Frame object. */
    Frame( const QString &fileName, const QString &function, int lineNumber, int depth )
            : fileName(fileName), function(function), lineNumber(lineNumber),
              functionStartLineNumber(-1), depth(depth)
    {
    };

    /** @brief Creates an invalid Frame object. */
    Frame() : lineNumber(-1), functionStartLineNumber(-1), depth(0) {};

    QString fileName; /**< The file/context name of the frames current context. */
    QString function; /**< The name of the currently executed function, if any. */
    int lineNumber; /**< The line number of the current execution. */
    int functionStartLineNumber; /**< The line number, where the currently executed function
            * starts, if any. */
    int depth; /**< The depth of this frame in the backtrace. */
};
/** @brief A stack of frames, ie. a backtrace. */
typedef QStack< Frame > FrameStack;

/** @brief Represents a variable in a script. */
struct Variable {
    /** @brief Variable types. */
    enum Type {
        Null, /**< Null/undefined. */
        Error, /**< An error. */
        Function, /**< A function. */
        Array, /**< An array / list. */
        Object, /**< An object. */
        Boolean, /**< A boolean. */
        Number, /**< A number. */
        String, /**< A string. */
        RegExp, /**< A regular expression. */
        Date, /**< A date. */
        Special /**< Used for special information objects,
                * generated for some default script objects. */
    };

    /** @brief Creates a new Variable object. */
    Variable( Type type, const QString &name, const QVariant &value, const KIcon &icon = KIcon() )
            : type(type), name(name), value(value), icon(icon) {};

    /** @brief Creates a new Variable object. */
    Variable() {
        backgroundRole = KColorScheme::NormalBackground;
        foregroundRole = KColorScheme::NormalText;
    };

    Type type; /**< The type of the variable. */
    QString name; /**< The name of the variable. */
    QVariant value; /**< The current value of the variable. */
    KIcon icon; /**< An icon for the variable. */
    QString description; /**< A description for the variable, eg. for tooltips. */
    bool isHelperObject; /**< True, if this variable is a helper script object, eg. the 'result',
            * 'network', 'storage' (etc.) script objects. */
    int sorting; /**< Sort value. */
    KColorScheme::BackgroundRole backgroundRole; /**< A background color role, used to highlight
            * wrong values. */
    KColorScheme::ForegroundRole foregroundRole; /**< A foreground color role, used to highlight
            * wrong values. */
    QList< Variable > children; /**< Children of this variable. */
};
typedef QList< Variable > Variables;

/**
 * @brief Represents a breakpoint.
 *
 * Can be used as a simple breakpoint as well as a more advanced one with a condition, which can
 * be written in JavaScript and gets executed in the current engines context if the breakpoint
 * gets reached. Breakpoints can be enabled/disabled manually. If a maximum hit count is reached
 * the breakpoint gets disabled.
 **/
class Breakpoint {
    friend class DebuggerAgent;
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

class ConsoleCommand {
public:
    enum Command {
        InvalidCommand = 0,
        DebugCommand,
        HelpCommand,
        ClearCommand,
        LineNumberCommand,
        DebuggerControlCommand,
        BreakpointCommand
    };

    ConsoleCommand() : m_command(InvalidCommand) {};

    ConsoleCommand( Command command, const QStringList &arguments = QStringList() )
            : m_command(command), m_arguments(arguments) {};

    ConsoleCommand( const QString &name, const QStringList &arguments = QStringList() )
            : m_command(commandFromName(name)), m_arguments(arguments) {};

    static ConsoleCommand fromString( const QString &str );

    bool isValid() const;

    Command command() const { return m_command; };
    QStringList arguments() const { return m_arguments; };
    QString argument( int i = 0 ) const {
            return i < 0 || i >= m_arguments.count() ? QString() : m_arguments[i].trimmed(); };
    QString description() const;
    QString syntax() const;

    /** Normally true. If false, the command is NOT executed in Debugger::runCommand() */
    bool getsExecutedAutomatically() const;

    static Command commandFromName( const QString &name );
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
inline QDebug &operator <<( QDebug debug, DebugMode debugMode )
{
    switch ( debugMode ) {
    case InterruptOnExceptions:
        return debug << "InterruptOnExceptions";
    case InterruptAtStart:
        return debug << "InterruptAtStart";
    default:
        return debug << "DebugMode unknown" << static_cast<int>(debugMode);
    }
}

inline QDebug &operator <<( QDebug debug, Variable::Type type )
{
    switch ( type ) {
    case Variable::Null:
        return debug << "Null";
    case Variable::Error:
        return debug << "Error";
    case Variable::Function:
        return debug << "Function";
    case Variable::Array:
        return debug << "Array";
    case Variable::Object:
        return debug << "Object";
    case Variable::Boolean:
        return debug << "Boolean";
    case Variable::Number:
        return debug << "Number";
    case Variable::String:
        return debug << "String";
    case Variable::RegExp:
        return debug << "RegExp";
    case Variable::Date:
        return debug << "Date";
    case Variable::Special:
        return debug << "Special";
    default:
        return debug << "Variable type unknown" << static_cast<int>(type);
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

#endif // Multiple inclusion guard
