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
#include "debuggeragent.h"

// Own includes
#include "backtracemodel.h"
#include "breakpointmodel.h"
#include "variablemodel.h"

// PublicTransport engine includes
#include <engine/script/scripting.h>

// KDE includes
#include <KDebug>
#include <KLocalizedString>

// Qt includes
#include <QScriptEngine>
#include <QScriptContextInfo>
#include <QScriptValueIterator>
#include <QTimer>
#include <QMutex>
#include <QWaitCondition>
#include <QEventLoop>
#include <QFileInfo> // Only used for debugging

namespace Debugger {

// In milliseconds
const int DebuggerAgent::CHECK_RUNNING_INTERVAL = 1000;
const int DebuggerAgent::CHECK_RUNNING_WHILE_INTERRUPTED_INTERVAL = 5000;

QScriptValue debugPrintFunction( QScriptContext *context, QScriptEngine *engine )
{
    QString result;
    for ( int i = 0; i < context->argumentCount(); ++i ) {
        if ( i > 0 ) {
            result.append(" ");
        }
        result.append( context->argument(i).toString() );
    }

    QScriptValue calleeData = context->callee().data();
    DebuggerAgent *debugger = qobject_cast<DebuggerAgent*>( calleeData.toQObject() );
    debugger->slotOutput( result, QScriptContextInfo(context->parentContext()) );
    return engine->undefinedValue();
};

DebuggerAgent::DebuggerAgent( QScriptEngine *engine, QMutex *engineMutex, bool mutexIsLocked )
        : QObject(engine), QScriptEngineAgent(engine),
          m_mutex(new QMutex(QMutex::Recursive)), m_interruptWaiter(new QWaitCondition()),
          m_interruptMutex(new QMutex()), m_engineMutex(engineMutex),
          m_checkRunningTimer(new QTimer(this)), m_lastRunAborted(false), m_runUntilLineNumber(-1),
          m_debugFlags(DefaultDebugFlags), m_currentContext(0), m_interruptContext(0)
{
    qRegisterMetaType< VariableChange >( "VariableChange" );
    qRegisterMetaType< BacktraceChange >( "BacktraceChange" );
    qRegisterMetaType< BreakpointChange >( "BreakpointChange" );
    qRegisterMetaType< Breakpoint >( "Breakpoint" );

    m_mutex->lockInline();
    m_lineNumber = -1;
    m_columnNumber = -1;
    m_currentScriptId = -1;
    m_state = NotRunning;
    m_injectedScriptState = InjectedScriptNotRunning;
    m_injectedScriptId = -1;
    m_executionControl = ExecuteRun;
    m_previousExecutionControl = ExecuteRun;
    m_hasUncaughtException = false;
    m_uncaughtExceptionLineNumber = -1;
    m_interruptFunctionLevel = -2;
    m_functionDepth = 0;
    m_mutex->unlockInline();

    // Install custom print function (overwriting the builtin print function)
    if ( !mutexIsLocked ) {
        m_engineMutex->lockInline();
    }
    QScriptValue printFunction = engine->newFunction( debugPrintFunction );
    printFunction.setData( engine->newQObject(this) );
    QScriptValue::PropertyFlags flags = QScriptValue::ReadOnly | QScriptValue::Undeletable;
    engine->globalObject().setProperty( "print", printFunction, flags );
    if ( !mutexIsLocked ) {
        m_engineMutex->unlockInline();
    }
}

DebuggerAgent::~DebuggerAgent()
{
    abortDebugger();

    delete m_mutex;
    delete m_interruptMutex;
    delete m_interruptWaiter;
}

bool DebuggerAgent::isInterrupted() const
{
    QMutexLocker locker( m_mutex );
    if ( m_state == Interrupted ) {
        return true;
    }

    const bool canLock = m_interruptMutex->tryLock();
    if ( canLock ) {
        m_interruptMutex->unlockInline();
    }
    return !canLock;
}

bool DebuggerAgent::isRunning() const
{
    QMutexLocker locker( m_mutex );
    return m_state == Running;
}

bool DebuggerAgent::isAborting() const
{
    QMutexLocker locker( m_mutex );
    return m_state == Aborting;
}

bool DebuggerAgent::wasLastRunAborted() const
{
    QMutexLocker locker( m_mutex );
    return m_lastRunAborted;
}

void DebuggerAgent::finish() const
{
    m_mutex->lockInline();
    if ( m_state != NotRunning ) {
        DEBUGGER_EVENT("Wait until script execution finishes...");
        QEventLoop loop;
        connect( this, SIGNAL(stopped(QDateTime,bool,bool,int,QString,QStringList)),
                 &loop, SLOT(quit()) );
        m_mutex->unlockInline();

        loop.exec();
        DEBUGGER_EVENT("...Script execution has finished");
    } else {
        m_mutex->unlockInline();
    }
}

NextEvaluatableLineHint DebuggerAgent::canBreakAt( int lineNumber, const QStringList &programLines )
{
    kDebug() << "canBreakAt(" << lineNumber << "), code lines:" << programLines.count();
    const int scriptLineCount = programLines.count();
    if ( lineNumber < 1 || lineNumber > scriptLineCount ) {
        return CannotFindNextEvaluatableLine;
    }

    QString line = programLines[ lineNumber - 1 ].trimmed();
    if ( line.isEmpty() || line.startsWith("//") ) {
        return NextEvaluatableLineBelow;
    } else if ( line.startsWith("/**") ) {
        return NextEvaluatableLineAbove;
    }

    // Test if the line can be evaluated
    // If not, try if appending more lines makes the text evaluatable (multiline statement)
    for ( int lines = 1; lines < 20 && lineNumber + lines <= scriptLineCount; ++lines ) {
        QScriptSyntaxCheckResult result = QScriptEngine::checkSyntax( line );
        if ( result.state() == QScriptSyntaxCheckResult::Valid ) {
            return FoundEvaluatableLine;
        }
        line.append( '\n' + programLines[lineNumber - 1 + lines] );
    }

    return NextEvaluatableLineAbove;
}

int DebuggerAgent::getNextBreakableLineNumber( int lineNumber, const QStringList &programLines )
{
    kDebug() << "getNextBreakableLineNumber(" << lineNumber << "), code lines:" << programLines.count();
    for ( int distance = 0; distance < 15; ++distance ) {
        const int lineNumber1 = lineNumber + distance;
        if ( canBreakAt(lineNumber1, programLines) == FoundEvaluatableLine ) {
            return lineNumber1;
        }

        const int lineNumber2 = lineNumber - distance;
        if ( lineNumber1 != lineNumber2 &&
             canBreakAt(lineNumber2, programLines) == FoundEvaluatableLine )
        {
            return lineNumber2;
        }
    }

    return -1;
}

NextEvaluatableLineHint DebuggerAgent::canBreakAt( const QString &fileName, int lineNumber ) const
{
    kDebug() << "canBreakAt(" << QFileInfo(fileName).fileName() << "," << lineNumber << ")";
    QMutexLocker locker( m_mutex );
    return canBreakAt( lineNumber, m_scriptLines[fileName] );
}

int DebuggerAgent::getNextBreakableLineNumber( const QString &fileName, int lineNumber ) const
{
    kDebug() << "getNextBreakableLineNumber(" << QFileInfo(fileName).fileName() << ","
             << lineNumber << ")";
    QMutexLocker locker( m_mutex );
    return getNextBreakableLineNumber( lineNumber, m_scriptLines[fileName] );
}

QString DebuggerAgent::stateToString( DebuggerState state )
{
    switch ( state ) {
    case NotRunning:
        return i18nc("@info/plain Debugger state", "Not running");
    case Running:
        return i18nc("@info/plain Debugger state", "Running");
    case Interrupted:
        return i18nc("@info/plain Debugger state", "Interrupted");
    case Aborting:
        return i18nc("@info/plain Debugger state", "Aborting");
    default:
        return "Unknown";
    }
}

bool DebuggerAgent::executeCommand( const ConsoleCommand &command, QString *returnValue )
{
    if ( !command.isValid() ) {
        return false;
    }

    switch ( command.command() ) {
    case ConsoleCommand::HelpCommand:
        if ( returnValue ) {
            if ( !command.arguments().isEmpty() ) {
                // "help" command with at least one argument
                const ConsoleCommand::Command commandType =
                        ConsoleCommand::commandFromName( command.argument() );
                *returnValue = i18nc("@info", "Command <emphasis>%1</emphasis>: %2<nl />"
                                              "Syntax: %3",
                        command.argument(), ConsoleCommand::commandDescription(commandType),
                        ConsoleCommand::commandSyntax(commandType));
            } else {
                // "help" command without arguments
                *returnValue = i18nc("@info", "Type a command beginning with a point ('.') or "
                        "JavaScript code. Available commands: %1<nl />"
                        "Use <emphasis>.help</emphasis> with an argument to get more information "
                        "about individual commands<nl />"
                        "Syntax: %2", ConsoleCommand::availableCommands().join(", "),
                        ConsoleCommand::commandSyntax(command.command()) );
            }
        }
        return true;
    case ConsoleCommand::ClearCommand:
        kWarning() << "ClearCommand needs to be implemented outside of DebuggerAgent";
        return true;
    case ConsoleCommand::LineNumberCommand:
        if ( returnValue ) {
            *returnValue = QString::number( lineNumber() );
        }
        return true;
    case ConsoleCommand::BreakpointCommand: {
        bool ok;
        int lineNumber = command.argument().toInt( &ok );
        if ( !ok ) {
            *returnValue = i18nc("@info", "Invalid argument '%1', expected a line number",
                                 command.argument());
            return false;
        }

        // TODO: Add argument to control breakpoints in external scripts
        m_mutex->lock();
        const QString mainScriptFileName = m_mainScriptFileName;
        m_mutex->unlock();

        lineNumber = getNextBreakableLineNumber( mainScriptFileName, lineNumber );
        ok = lineNumber >= 0;
        if ( !ok ) {
            *returnValue = i18nc("@info", "Cannot interrupt script execution at line %1",
                                 lineNumber);
            return false;
        }

        // TODO Use filename from a new argument
        const QHash< uint, Breakpoint > &breakpoints = breakpointsForFile( mainScriptFileName );
        const bool breakpointExists = breakpoints.contains( lineNumber );
        kDebug() << "Breakpoint exists" << breakpointExists;
        Breakpoint breakpoint = breakpointExists ? breakpoints[lineNumber]
                : Breakpoint( mainScriptFileName, lineNumber );
        if ( command.arguments().count() == 1 ) {
            // Only ".break <lineNumber>", no command to execute
            // Return information about the breakpoint
            if ( breakpointExists ) {
                *returnValue = i18nc("@info", "Breakpoint at line %1: %2 hits, %3, %4",
                                     lineNumber, breakpoint.hitCount(),
                                     breakpoint.isEnabled() ? i18nc("@info", "enabled")
                                                            : i18nc("@info", "disabled"),
                                     breakpoint.condition().isEmpty()
                                        ? i18nc("@info", "No condition")
                                        : i18nc("@info", "Condition: %1", breakpoint.condition())
                                    );
            } else {
                *returnValue = i18nc("@info", "No breakpoint found at line %1", lineNumber);
            }
            return breakpointExists;
        }

        // More than one argument given, ie. more than ".break <lineNumber> ..."
        const QString &argument = command.arguments().count() == 1
                ? QString() : command.argument( 1 );
        bool errorNotFound = false;
        QRegExp maxhitRegExp( "^maxhits(?:=|:)(\\d+)$", Qt::CaseInsensitive );
        if ( command.arguments().count() == 1 || argument.compare(QLatin1String("add")) == 0 ) {
            emit breakpointsChanged( BreakpointChange(AddBreakpoint, breakpoint) );
            *returnValue = ok ? i18nc("@info", "Breakpoint added at line %1", lineNumber)
                              : i18nc("@info", "Cannot add breakpoint at line %1", lineNumber);
        } else if ( argument.compare(QLatin1String("remove")) == 0 ) {
            if ( !breakpointExists ) {
                errorNotFound = true;
            } else {
                emit breakpointsChanged( BreakpointChange(RemoveBreakpoint, breakpoint) );
                *returnValue = ok ? i18nc("@info", "Breakpoint at line %1 removed", lineNumber)
                                  : i18nc("@info", "Cannot remove breakpoint at line %1", lineNumber);
            }
        } else if ( argument.compare(QLatin1String("toggle")) == 0 ) {
            if ( !breakpointExists ) {
                errorNotFound = true;
            } else {
                breakpoint.setEnabled( !breakpoint.isEnabled() );
                emit breakpointsChanged( BreakpointChange(UpdateBreakpoint, breakpoint) );
                *returnValue = ok ? i18nc("@info", "Breakpoint toggled at line %1", lineNumber)
                                  : i18nc("@info", "Cannot toggle breakpoint at line %1", lineNumber);
            }
        } else if ( argument.compare(QLatin1String("enable")) == 0 ) {
            if ( !breakpointExists ) {
                errorNotFound = true;
            } else {
                breakpoint.setEnabled();
                emit breakpointsChanged( BreakpointChange(UpdateBreakpoint, breakpoint) );
                *returnValue = ok ? i18nc("@info", "Breakpoint enabled at line %1", lineNumber)
                                  : i18nc("@info", "Cannot enable breakpoint at line %1", lineNumber);
            }
        } else if ( argument.compare(QLatin1String("disable")) == 0 ) {
            if ( !breakpointExists ) {
                errorNotFound = true;
            } else {
                breakpoint.setEnabled( false );
                emit breakpointsChanged( BreakpointChange(UpdateBreakpoint, breakpoint) );
                *returnValue = ok ? i18nc("@info", "Breakpoint disabled at line %1", lineNumber)
                                  : i18nc("@info", "Cannot disable breakpoint at line %1", lineNumber);
            }
        } else if ( argument.compare(QLatin1String("reset")) == 0 ) {
            if ( !breakpointExists ) {
                errorNotFound = true;
            } else {
                breakpoint.reset();
                emit breakpointsChanged( BreakpointChange(UpdateBreakpoint, breakpoint) );
                *returnValue = ok ? i18nc("@info", "Breakpoint reset at line %1", lineNumber)
                                  : i18nc("@info", "Cannot reset breakpoint at line %1", lineNumber);
            }
        } else if ( argument.compare(QLatin1String("condition")) == 0 ) {
            if ( !breakpointExists ) {
                errorNotFound = true;
            } else if ( command.arguments().count() < 3 ) {
                // Needs at least 3 arguments: ".break <lineNumber> condition <conditionCode>"
                *returnValue = i18nc("@info", "Condition code missing");
            } else {
                breakpoint.setCondition( QStringList(command.arguments().mid(2)).join(" ") );
                emit breakpointsChanged( BreakpointChange(UpdateBreakpoint, breakpoint) );
                *returnValue = ok ? i18nc("@info", "Breakpoint condition set to <emphasis>%1"
                                                   "</emphasis> at line %2",
                                          breakpoint.condition(), lineNumber)
                                  : i18nc("@info", "Cannot set breakpoint condition to <emphasis>%1"
                                                   "</emphasis> at line %1",
                                          breakpoint.condition(), lineNumber);
            }
        } else if ( maxhitRegExp.indexIn(argument) != -1 ) {
            if ( !breakpointExists ) {
                errorNotFound = true;
            } else {
                breakpoint.setMaximumHitCount( maxhitRegExp.cap(1).toInt() );
                emit breakpointsChanged( BreakpointChange(UpdateBreakpoint, breakpoint) );
                *returnValue = ok ? i18nc("@info", "Breakpoint changed at line %1", lineNumber)
                                  : i18nc("@info", "Cannot change breakpoint at line %1", lineNumber);
            }
        } else {
            kDebug() << "Unexcepted argument:" << argument;
            ok = false;
            if ( returnValue ) {
                *returnValue = i18nc("@info", "Unexcepted argument: %1<nl />Excepted: "
                                     "<emphasis>add</emphasis> (default), "
                                     "<emphasis>remove</emphasis>, "
                                     "<emphasis>toggle</emphasis>, "
                                     "<emphasis>enable</emphasis>, "
                                     "<emphasis>disable</emphasis>, "
                                     "<emphasis>reset</emphasis>, "
                                     "<emphasis>condition=&lt;conditionCode&gt;</emphasis>, "
                                     "<emphasis>maxhits=&lt;number&gt;</emphasis>",
                                     argument);
            }
        }

        if ( errorNotFound ) {
            ok = false;
            if ( returnValue ) {
                *returnValue = i18nc("@info", "No breakpoint found at line %1",
                                        lineNumber );
            }
        }
        return ok;
    }
    case ConsoleCommand::DebuggerControlCommand: {
        const QString argument = command.argument();
        if ( argument == QLatin1String("status") && returnValue ) {
            m_mutex->lockInline();
            *returnValue = i18nc("@info", "Debugger status: %1", stateToString(m_state));
            if ( m_state != NotRunning ) {
                *returnValue += ", " + i18nc("@info", "line %1", m_lineNumber);
            }
            if ( m_hasUncaughtException ) {
                *returnValue += ", " + i18nc("@info",
                        "uncaught exception in line %1: <message>%2</message>",
                        m_uncaughtExceptionLineNumber, m_uncaughtException.toString());
            }
            m_mutex->unlockInline();
            return true;
        } else {
            ConsoleCommandExecutionControl executionControl =
                    consoleCommandExecutionControlFromString( argument );
            if ( executionControl != InvalidControlExecution ) {
                QString errorMessage;
                const bool ok = debugControl( executionControl, command.argument(1), &errorMessage );
                if ( returnValue ) {
                    if ( ok ) {
                        *returnValue = i18nc("@info", "Command successfully executed");
                    } else {
                        *returnValue = i18nc("@info",
                                "Cannot execute command: <message>%1</message>", errorMessage);
                    }
                }
                return ok;
            } else if ( returnValue ) {
                *returnValue = i18nc("@info", "Unexcepted argument <emphasis>%1</emphasis><nl />"
                                    "Expected one of these: "
                                    "<emphasis>status</emphasis>, "
                                    "<emphasis>continue</emphasis>, "
                                    "<emphasis>interrupt</emphasis>, "
                                    "<emphasis>abort</emphasis>, "
                                    "<emphasis>stepinto &lt;count = 1&gt;</emphasis>, "
                                    "<emphasis>stepover &lt;count = 1&gt;</emphasis>, "
                                    "<emphasis>stepout &lt;count = 1&gt;</emphasis>, "
                                    "<emphasis>rununtil &lt;lineNumber&gt;</emphasis>",
                                    command.argument());
                return false;
            }
        }
    }
    case ConsoleCommand::DebugCommand: {
        bool error;
        int errorLineNumber;
        QString errorMessage;
        QStringList backtrace;

        QScriptValue result = evaluateInContext( command.arguments().join(" "),
                i18nc("@info/plain", "Console Debug Command"), &error, &errorLineNumber,
                &errorMessage, &backtrace, InterruptAtStart );

        if ( error ) {
            if ( returnValue ) {
                *returnValue = i18nc("@info", "Error: <message>%1</message><nl />"
                                              "Backtrace: <message>%2</message>",
                                     errorMessage, backtrace.join("<br />"));
            }
        } else if ( returnValue ) {
            *returnValue = result.toString();
        }
        return !error;
    }

    default:
        kDebug() << "Command execution not implemented" << command.command();
        return false;
    }
}

void DebuggerAgent::continueToDoSomething()
{
    QMutexLocker locker( m_mutex );
    if ( m_state != Interrupted ) {
        kDebug() << "Debugger is not interrupted" << m_state;
        return;
    }

    // Wake from interrupt, then emit doSomething() and directly interrupt again
    m_executionControl = ExecuteInterrupt;
    m_interruptWaiter->wakeAll();
}

QScriptValue DebuggerAgent::evaluateInContext( const QString &program, const QString &contextName,
        bool *hadUncaughtException, int *errorLineNumber, QString *errorMessage,
        QStringList *backtrace, DebugFlags debugFlags )
{
    // Use new context for program evaluation
    QScriptContext *context = engine()->pushContext();

    // Store current execution type/debug flags, to restore it later
    m_mutex->lockInline();
    const ExecutionControl executionType = m_executionControl;
    const DebugFlags oldDebugFlags = m_debugFlags;
    m_debugFlags = debugFlags;
    m_mutex->unlockInline();

    QTimer timer;
    if ( debugFlags.testFlag(InterruptAtStart) ) {
        debugStepIntoInjectedProgram();
    } else {
        debugRunInjectedProgram();

        // Start a countdown, if evaluation does not finish within this countdown, it gets aborted
        timer.setSingleShot( true );
        connect( &timer, SIGNAL(timeout()), this, SLOT(cancelInjectedCodeExecution()) );
        timer.start( 3000 ); // 3 seconds time for the evaluation
    }

    // Evaluate program
    QScriptValue result = engine()->evaluate( program,
            contextName.isEmpty() ? "<Injected Code>" : contextName );
    timer.stop(); // Stop cancel timeout

    // Restore previous execution type (if not interrupted)
    if ( debugFlags.testFlag(InterruptAtStart) ) {
        m_mutex->lockInline();
        m_executionControl = executionType;
        m_mutex->unlockInline();
    }

    // Restore previous debug flags
    if ( debugFlags != oldDebugFlags ) {
        m_mutex->lockInline();
        m_debugFlags = oldDebugFlags;
        m_mutex->unlockInline();
    }

    DEBUGGER_EVENT("Evaluate-in-context result" << result.toString() << program);
    if ( hadUncaughtException ) {
        *hadUncaughtException = engine()->hasUncaughtException();
    }
    if ( errorLineNumber ) {
        *errorLineNumber = engine()->uncaughtExceptionLineNumber();
    }
    if ( errorMessage ) {
        *errorMessage = engine()->uncaughtException().toString();
    }
    if ( backtrace ) {
        *backtrace = engine()->uncaughtExceptionBacktrace();
    }
    if ( engine()->hasUncaughtException() ) {
        kDebug() << "Uncaught exception in program:"
                 << engine()->uncaughtExceptionBacktrace();
        engine()->clearExceptions();
    }

    engine()->popContext();

    // Transfer values from evaluation context to script context
    QScriptValueIterator it( context->activationObject() );
    QScriptValue scriptContext = engine()->currentContext()->activationObject();
    if ( it.hasNext() ) {
        it.next();
        scriptContext.setProperty( it.name(), it.value() );
    }

    return result;
}

void DebuggerAgent::cancelInjectedCodeExecution()
{
    QMutexLocker locker( m_mutex );
    if ( m_injectedScriptState == InjectedScriptEvaluating ) {
        DEBUGGER_EVENT("Evaluation did not finish in time or was cancelled");
        m_executionControl = ExecuteAbortInjectedProgram;
    } else {
        // Is not running injected code
        checkHasExited();
    }

    wakeFromInterrupt();
}

void DebuggerAgent::addBreakpoint( const Breakpoint &breakpoint )
{
    QHash< uint, Breakpoint > &breakpoints = breakpointsForFile( breakpoint.fileName() );
    breakpoints.insert( breakpoint.lineNumber(), breakpoint );
}

void DebuggerAgent::removeBreakpoint( const Breakpoint &breakpoint )
{
    QHash< uint, Breakpoint > &breakpoints = breakpointsForFile( breakpoint.fileName() );
    breakpoints.remove( breakpoint.lineNumber() );
}

bool DebuggerAgent::debugControl( DebuggerAgent::ConsoleCommandExecutionControl controlType,
                                  const QVariant &argument, QString *errorMessage )
{
    switch ( controlType ) {
    case ControlExecutionContinue:
        if ( !isInterrupted() ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not interrupted!");
            }
            return false;
        }
        debugContinue();
        break;
    case ControlExecutionInterrupt:
        if ( !isRunning() ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not running! Start the debugger first.");
            }
            return false;
        }
        debugInterrupt();
        break;
    case ControlExecutionAbort:
        if ( !isRunning() && !isInterrupted() ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not running or interrupted! Start the debugger first.");
            }
            return false;
        }
        abortDebugger();
        break;
    case ControlExecutionStepInto:
        if ( !isInterrupted() ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not interrupted!");
            }
            return false;
        }
        debugStepInto( qMax(0, argument.toInt() - 1) );
        break;
    case ControlExecutionStepOver:
        if ( !isInterrupted() ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not interrupted!");
            }
            return false;
        }
        debugStepOver( qMax(0, argument.toInt() - 1) );
        break;
    case ControlExecutionStepOut:
        if ( !isInterrupted() ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not interrupted!");
            }
            return false;
        }
        debugStepOut( qMax(0, argument.toInt() - 1) );
        break;
    case ControlExecutionRunUntil: {
        bool ok;
        int lineNumber = argument.toInt( &ok );

        m_mutex->lockInline();
        const int scriptLineCount = m_scriptLines[m_mainScriptFileName].count();
        const QString mainScriptFileName = m_mainScriptFileName;
        m_mutex->unlockInline();

        if ( !argument.isValid() || !ok ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Invalid argument '%1', expected line number!",
                                      argument.toString() );
            }
            return false;
        } else if ( lineNumber < 1 || lineNumber > scriptLineCount ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Invalid line number %1! Must be between 1 and %2",
                                      lineNumber, scriptLineCount);
            }
            return false;
        }

        // TODO: Add argument to run until a line in an external script
        lineNumber = getNextBreakableLineNumber( mainScriptFileName, lineNumber );
        if ( lineNumber < 0 ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Cannot interrupt script execution at line %!",
                                      lineNumber);
            }
            return false;
        }

        debugRunUntilLineNumber( mainScriptFileName, lineNumber );
        break;
    }
    case InvalidControlExecution:
        kDebug() << "Invalid control execution type";
        break;
    default:
        kDebug() << "Unknown Debugger::ExecutionControl" << controlType;
        if ( errorMessage ) {
            *errorMessage = i18nc("@info", "Debugger command %1 not implemented!", controlType );
        }
        return false;
    }

    return true;
}

DebuggerAgent::ConsoleCommandExecutionControl DebuggerAgent::consoleCommandExecutionControlFromString( const QString &str )
{
    const QString _str = str.trimmed().toLower();
    if ( _str == QLatin1String("continue") ) {
        return ControlExecutionContinue;
    } else if ( _str == QLatin1String("interrupt") ) {
        return ControlExecutionInterrupt;
    } else if ( _str == QLatin1String("abort") ) {
        return ControlExecutionAbort;
    } else if ( _str == QLatin1String("stepinto") ) {
        return ControlExecutionStepInto;
    } else if ( _str == QLatin1String("stepover") ) {
        return ControlExecutionStepOver;
    } else if ( _str == QLatin1String("stepout") ) {
        return ControlExecutionStepOut;
    } else if ( _str == QLatin1String("rununtil") ) {
        return ControlExecutionRunUntil;
    } else {
        return InvalidControlExecution;
    }
}

void DebuggerAgent::setExecutionControlType( ExecutionControl executionType )
{
    QMutexLocker locker( m_mutex );
    m_executionControl = executionType;
    m_repeatExecutionTypeCount = 0; // If execution type is repeatable, ie. stepInto/stepOver/stepOut
}

void DebuggerAgent::abortDebugger()
{
    DEBUGGER_CONTROL("abortDebugger");
    QMutexLocker locker( m_mutex );
    const DebuggerState state = m_state;
    switch ( state ) {
    case Aborting:
        DEBUGGER_EVENT("Is already aborting");
        return;
    case NotRunning:
        if ( m_injectedScriptState == InjectedScriptEvaluating ) {
            cancelInjectedCodeExecution();
        }
        return;
    case Running:
    case Interrupted:
    default:
        DEBUGGER_EVENT("Abort");
        m_lastRunAborted = true;
        m_executionControl = ExecuteAbort;
        setState( Aborting );
        engine()->abortEvaluation();
        break;
    }

    wakeFromInterrupt( state );
}

void DebuggerAgent::debugInterrupt()
{
    QMutexLocker locker( m_mutex );
    DEBUGGER_CONTROL( "debugInterrupt" );
    if ( m_state != Running ) {
        kDebug() << "Debugger is not running" << m_state;
        return;
    }

    m_executionControl = ExecuteInterrupt;
}

void DebuggerAgent::debugContinue()
{
    QMutexLocker locker( m_mutex );
    DEBUGGER_CONTROL( "debugContinue" );
    if ( m_state != Interrupted ) {
        kDebug() << "Debugger is not interrupted" << m_state;
        return;
    }

    m_executionControl = ExecuteRun;
    m_interruptWaiter->wakeAll();
}

void DebuggerAgent::debugStepInto( int repeat )
{
    DEBUGGER_CONTROL2( "debugStepInto", repeat );
    QMutexLocker locker( m_mutex );
    if ( m_state != Interrupted ) {
        kDebug() << "Debugger is not interrupted" << m_state;
        return;
    }

    // Wake from interrupt and run until the next statement
    m_repeatExecutionTypeCount = repeat;
    m_executionControl = ExecuteStepInto;
    m_interruptWaiter->wakeAll();
}

void DebuggerAgent::debugStepOver( int repeat )
{
    DEBUGGER_CONTROL2( "debugStepOver", repeat );
    QMutexLocker locker( m_mutex );
    if ( m_state != Interrupted ) {
        kDebug() << "Debugger is not interrupted" << m_state;
        return;
    }

    // Wake from interrupt and run until the next statement in the function level
    m_interruptFunctionLevel = 0;
    m_repeatExecutionTypeCount = repeat;
    m_executionControl = ExecuteStepOver;
    m_interruptWaiter->wakeAll();
}

void DebuggerAgent::debugStepOut( int repeat )
{
    DEBUGGER_CONTROL2( "debugStepOut", repeat );
    QMutexLocker locker( m_mutex );
    if ( m_state != Interrupted) {
        kDebug() << "Debugger is not interrupted" << m_state;
        return;
    }

    // Wake from interrupt and run until the current function gets exited
    m_interruptFunctionLevel = 0;
    m_repeatExecutionTypeCount = repeat;
    m_executionControl = ExecuteStepOut;
    m_interruptWaiter->wakeAll();
}

void DebuggerAgent::debugRunUntilLineNumber( const QString &fileName, int lineNumber )
{
    DEBUGGER_CONTROL3( "debugRunUntilLineNumber", QFileInfo(fileName).fileName(), lineNumber );
    int runUntilLineNumber = getNextBreakableLineNumber( fileName, lineNumber );

    QMutexLocker locker( m_mutex );
    m_runUntilLineNumber = runUntilLineNumber;
    if ( runUntilLineNumber != -1 ) {
        m_executionControl = ExecuteRun;
        wakeFromInterrupt();
    }
}

void DebuggerAgent::debugRunInjectedProgram()
{
    QMutexLocker locker( m_mutex );
    DEBUGGER_CONTROL("debugRunInjectedProgram");
    m_repeatExecutionTypeCount = 0;
    m_injectedScriptState = InjectedScriptInitializing; // Update in next scriptLoad()
    m_previousExecutionControl = m_executionControl;
    m_executionControl = ExecuteRunInjectedProgram;
    // Do not wake from interrupt here, because the script should stay interrupted,
    // while the injected program runs in another thread
}

void DebuggerAgent::debugStepIntoInjectedProgram()
{
    QMutexLocker locker( m_mutex );
    DEBUGGER_CONTROL("debugStepIntoInjectedProgram");
    m_repeatExecutionTypeCount = 0;
    m_injectedScriptState = InjectedScriptInitializing; // Update in next scriptLoad()
    m_previousExecutionControl = m_executionControl;
    m_executionControl = ExecuteStepIntoInjectedProgram;
    // Do not wake from interrupt here, because the script should stay interrupted,
    // while the injected program runs in another thread
}

DebuggerState DebuggerAgent::state() const
{
    QMutexLocker locker( m_mutex );
    return m_state;
}

QString DebuggerAgent::mainScriptFileName() const
{
    QMutexLocker locker( m_mutex );
    return m_mainScriptFileName;
}

void DebuggerAgent::setMainScriptFileName( const QString &mainScriptFileName )
{
    QMutexLocker locker( m_mutex );
    m_mainScriptFileName = mainScriptFileName;
}

void DebuggerAgent::setScriptText( const QString &fileName, const QString &program )
{
    QMutexLocker locker( m_mutex );
    m_scriptLines[ fileName ] = program.split( '\n', QString::KeepEmptyParts );
}

void DebuggerAgent::scriptLoad( qint64 id, const QString &program, const QString &fileName,
                                int baseLineNumber )
{
    Q_UNUSED( baseLineNumber );
    if ( id != -1 ) {
        QMutexLocker locker( m_mutex );
        m_scriptIdToFileName.insert( id, fileName );
        m_currentScriptId = id;
        if ( m_injectedScriptState == InjectedScriptInitializing ) {
            // The new script is code that should be executed in the current scripts context
            // while the main script is interrupted
            m_injectedScriptId = id;
            m_injectedScriptState = InjectedScriptEvaluating;
        } else if ( m_executionControl != ExecuteRunInjectedProgram &&
                    m_executionControl != ExecuteStepIntoInjectedProgram )
        {
            DEBUGGER_EVENT( "Load script" << QFileInfo(fileName).fileName() << "with id" << id );
            setScriptText( fileName, program );
        }
    }
}

void DebuggerAgent::scriptUnload( qint64 id )
{
    Q_UNUSED( id );
    QMutexLocker locker( m_mutex );
    if ( m_injectedScriptState == InjectedScriptUpdateVariablesInParentContext ) {
        m_injectedScriptState = InjectedScriptNotRunning;
        QTimer::singleShot( 0, this, SLOT(wakeFromInterrupt()) );
    }
    DEBUGGER_EVENT( "Unload script" << m_scriptIdToFileName[id] << "with id" << id );
    m_scriptIdToFileName.remove( id );
    if ( m_currentScriptId == id ) {
        m_currentScriptId = -1;
    }
}

void DebuggerAgent::wakeFromInterrupt( DebuggerState unmodifiedState )
{
    QMutexLocker locker( m_mutex );
    if ( unmodifiedState == Interrupted ) {
        m_interruptWaiter->wakeAll();
    }
}

void DebuggerAgent::contextPush()
{
}

void DebuggerAgent::contextPop()
{
}

void DebuggerAgent::functionEntry( qint64 scriptId )
{
    if ( scriptId != -1 ) {
        QMutexLocker locker( m_mutex );
        ++m_functionDepth;
        if ( m_interruptFunctionLevel >= -1 &&
            (m_executionControl == ExecuteStepOver || m_executionControl == ExecuteStepOut) )
        {
            ++m_interruptFunctionLevel;
        }

        emit variablesChanged( VariableChange(PushVariableStack) );
        emit backtraceChanged( BacktraceChange(PushBacktraceFrame) );
    }
}


void DebuggerAgent::functionExit( qint64 scriptId, const QScriptValue& returnValue )
{
    if ( scriptId != -1 ) {
        emit variablesChanged( VariableChange(PopVariableStack) );
        emit backtraceChanged( BacktraceChange(PopBacktraceFrame) );
    }

    QMutexLocker locker( m_mutex );
    if ( scriptId != -1 && m_injectedScriptId == scriptId ) {
        DEBUGGER_EVENT( "Evaluation in context finished with" << returnValue.toString() );
        m_executionControl = m_previousExecutionControl;
        emit evaluationInContextFinished( returnValue );

        // Interrupts again if it was interrupted before,
        // but variables can be updated in the script context
        m_injectedScriptState = InjectedScriptUpdateVariablesInParentContext;
    } else {
        if ( scriptId != -1 && m_interruptFunctionLevel >= 0 ) {
            if ( m_executionControl == ExecuteStepOver ) {
                // m_interruptFunctionLevel may be 0 here, if the script exits in one function,
                // waiting for a signal to continue script execution at a connected slot
                --m_interruptFunctionLevel;
            } else if ( m_executionControl == ExecuteStepOut ) {
                --m_interruptFunctionLevel;
            }
        }
    }

    if ( scriptId != -1 ) {
        if ( m_executionControl == ExecuteAbort || m_state == Aborting ) {
            // Do nothing when aborting, changing m_functionDepth when aborting
            // leads to problems when starting the script again
            return;
        }

        --m_functionDepth;
        const int functionDepth = m_functionDepth;

        if ( functionDepth == 0 ) {
            // Engine mutex is still locked here to protect the engine while it is executing,
            // unlock after execution has ended here
            shutdown();
        }
    }
}

ExecutionControl DebuggerAgent::applyExecutionControl( ExecutionControl executionControl,
                                                       QScriptContext *currentContext )
{
    QMutexLocker locker( m_mutex );
    Q_UNUSED( currentContext );
    switch ( executionControl ) {
        case ExecuteStepInto:
        case ExecuteStepIntoInjectedProgram:
            // Decrease repeation counter, if it is at 0 interrupt
            if ( m_repeatExecutionTypeCount > 0 ) {
                --m_repeatExecutionTypeCount;
            } else if ( m_repeatExecutionTypeCount == 0 ) {
                if ( m_executionControl != ExecuteAbort &&
                     m_executionControl != ExecuteAbortInjectedProgram )
                {
                    m_executionControl = executionControl = ExecuteInterrupt;
                }
            }
            break;
        case ExecuteStepOver:
            if ( m_interruptFunctionLevel == 0 ) {
                if ( m_repeatExecutionTypeCount > 0 ) {
                    --m_repeatExecutionTypeCount;
                } else if ( m_repeatExecutionTypeCount == 0 ) {
                    m_interruptFunctionLevel = -2;
                    if ( m_executionControl != ExecuteAbort &&
                         m_executionControl != ExecuteAbortInjectedProgram )
                    {
                        m_executionControl = executionControl = ExecuteInterrupt;
                    }
                }
            }
            break;
        case ExecuteStepOut:
            if ( m_interruptFunctionLevel == -1 ) {
                if ( m_repeatExecutionTypeCount > 0 ) {
                    --m_repeatExecutionTypeCount;
                } else if ( m_repeatExecutionTypeCount == 0 ) {
                    m_interruptFunctionLevel = -2;
                    if ( m_executionControl != ExecuteAbort &&
                         m_executionControl != ExecuteAbortInjectedProgram )
                    {
                        m_executionControl = executionControl = ExecuteInterrupt;
                    }
                }
            }
            break;
        case ExecuteRun:
//         case ExecuteNotRunning:
        case ExecuteInterrupt:
        case ExecuteRunInjectedProgram:
        default:
            break;
    }
    return executionControl;
}

void DebuggerAgent::emitChanges() {
    QMutexLocker engineLocker( m_engineMutex );
    QMutexLocker locker( m_mutex );

    QScriptContext *context = engine()->currentContext();
    m_currentContext = context;
    if ( context ) {
        // Construct backtrace/variable change objects
        // The Frame object is already created and added to the model with empty values
        // (in DebuggerAgent::functionEntry())
        const bool isGlobal = context->thisObject().equals( engine()->globalObject() );
        const Frame frame( QScriptContextInfo(context), isGlobal );
        const BacktraceChange backtraceChange( UpdateBacktraceFrame, frame );
        const VariableChange variableChange = VariableChange::fromContext( context );

        // Emit changes
        emit variablesChanged( variableChange );
        emit backtraceChanged( backtraceChange );
    }
}

void DebuggerAgent::positionChange( qint64 scriptId, int lineNumber, int columnNumber )
{
    Q_UNUSED( scriptId );

    // Lock the engine if not already locked (should normally be locked before script execution,
    // but it may get unlocked before the script is really done, eg. waiting idle for network
    // requests to finish)
    m_engineMutex->tryLockInline(); // Try to have the engine locked here
    QScriptContext *currentContext = engine()->currentContext();
    // Unlock now, maybe trying to lock above was successful or the engine was already locked
    m_engineMutex->unlockInline();

    // Lock member variables and initialize
    m_mutex->lockInline();
    ExecutionControl executionControl = m_executionControl;
    const bool isAborting = executionControl == ExecuteAbort || m_state == Aborting;
    DEBUGGER_EVENT_POS_CHANGED("Position changed to line" << lineNumber << "colum" << columnNumber
        << "in file" << m_scriptIdToFileName[scriptId] << "- Execution type:" << executionControl);
    const bool injectedProgram = m_injectedScriptState == InjectedScriptEvaluating;
    if ( !injectedProgram && m_state == NotRunning ) {
        // Execution has just started
        m_mutex->unlockInline();
        fireup();
        m_mutex->lockInline();
    }

    const int oldLineNumber = m_lineNumber;
    const int oldColumnNumber = m_columnNumber;
    DebuggerState state = m_state;

    // Decide if execution should be interrupted (breakpoints, execution control value, eg. step into).
    if ( !injectedProgram && !isAborting ) {
        // Update current execution position,
        // before emitting breakpointReached() and positionChanged()
        m_currentScriptId = scriptId;
        m_lineNumber = lineNumber;
        m_columnNumber = columnNumber;

        // Check if execution should be interrupted at the current line, because of a
        // runUntilLineNumber command
        if ( m_runUntilLineNumber == lineNumber ) {
            if ( executionControl != ExecuteAbort &&
                 executionControl != ExecuteAbortInjectedProgram )
            {
                m_executionControl = executionControl = ExecuteInterrupt;
                m_runUntilLineNumber = -1;
            }
        }

        // Check for breakpoints at the current line
        if ( m_debugFlags.testFlag(InterruptOnBreakpoints) ) {
            Breakpoint *breakpoint = 0;
            bool conditionError = false;
            if ( findActiveBreakpoint(lineNumber, breakpoint, &conditionError) ) {
                // Reached a breakpoint, applyBreakpoints() may have written
                // a new value in m_executionControl (ExecuteInterrupt)
                if ( executionControl != ExecuteAbort &&
                    executionControl != ExecuteAbortInjectedProgram )
                {
                    m_executionControl = executionControl = ExecuteInterrupt;
                }
                m_mutex->unlockInline();

                emit breakpointReached( *breakpoint );
            } else if ( conditionError ) {
                // There was an error with the condition of the breakpoint
                // Interrupt to let the user fix the condition code
                m_executionControl = executionControl = ExecuteInterrupt;
                m_mutex->unlockInline();
            } else {
                // No breakpoint reached
                m_executionControl = executionControl =
                        applyExecutionControl( executionControl, currentContext );
                m_mutex->unlockInline();
            }
        } else {
            m_executionControl = executionControl =
                    applyExecutionControl( executionControl, currentContext );
            m_mutex->unlockInline();
        }

        if ( executionControl == ExecuteInterrupt ) {
            emit positionChanged( lineNumber, columnNumber, oldLineNumber, oldColumnNumber );

            // Interrupt script execution
            DEBUGGER_EVENT("Interrupt now");
            doInterrupt( injectedProgram );

            // Script execution continued, update values
            m_mutex->lockInline();
            executionControl = m_executionControl;
            state = m_state;
            m_mutex->unlockInline();
        }
    } else {
        // Do not update execution position or check for breakpoints,
        // if in an injected program or aborting
        m_mutex->unlockInline();
    }

    // Check if debugging should be aborted
    if ( executionControl == ExecuteAbort ) {
        m_mutex->lockInline();
        m_injectedScriptId = -1;
        m_injectedScriptState = InjectedScriptNotRunning;
        m_mutex->unlockInline();

        // TODO
        const bool locked = m_engineMutex->tryLock( 250 );
        engine()->abortEvaluation();
        if ( !locked ) {
            kWarning() << "Could not lock the engine";
        }

        shutdown();
    } else if ( executionControl == ExecuteAbortInjectedProgram ) {
        // Restore member variables
        m_mutex->lockInline();
        DEBUGGER_EVENT("Abort injected program");
        m_injectedScriptState = InjectedScriptAborting; // Handled in doInterrupt
        m_executionControl = m_previousExecutionControl;
        m_mutex->unlockInline();

        // TEST
        // Interrupt execution of injected script code,
        // it should be aborted by terminating the executing thread
        doInterrupt( true );
    } else if ( state != NotRunning && state != Aborting ) {
        // Protect further script execution
        m_engineMutex->lockInline();
    }
}

QScriptContextInfo DebuggerAgent::contextInfo() {
    if ( m_engineMutex->tryLock(200) ) {
        const QScriptContextInfo info( engine()->currentContext() );
        m_engineMutex->unlockInline();

        return info;
    } else {
        kDebug() << "Engine is locked (not interrupted), cannot get context info";
        return QScriptContextInfo();
    }
}

bool DebuggerAgent::findActiveBreakpoint( int lineNumber, Breakpoint *&foundBreakpoint,
                                          bool *conditionError )
{
    QMutexLocker locker( m_mutex );
    if ( m_currentScriptId == -1 ) {
        kDebug() << "scriptId == -1";
        return false;
    }

    // Test for a breakpoint at the new line number
    QHash< uint, Breakpoint > &breakpoints = currentBreakpoints();
    if ( !breakpoints.contains(lineNumber) ) {
        // No breakpoint at the current execution position
        return false;
    }

    Breakpoint &breakpoint = breakpoints[ lineNumber ];
    if ( !breakpoint.isValid() ) {
        // No breakpoint for the current file found
        return false;
    } else if ( breakpoint.isEnabled() ) {
        // Found a breakpoint, test breakpoint condition if any
        if ( !breakpoint.condition().isEmpty() ) {
            if ( !breakpoint.testCondition(this, conditionError) ) {
                // Breakpoint reached but it's condition was not satisfied
                return false;
            }
        }

        // The found breakpoint is enabled
        DEBUGGER_EVENT( "Breakpoint reached:" << lineNumber << breakpoint.fileName() );
        breakpoint.reached(); // Increase hit count, etc.

        // Condition satisfied or no condition, active breakpoint found
        foundBreakpoint = &breakpoint;
        return true;
    } else {
        DEBUGGER_EVENT( "Breakpoint at" << lineNumber << "reached but it is disabled"
                        << breakpoint.fileName() );
        return false;
    }
}

void DebuggerAgent::setState( DebuggerState newState )
{
    QMutexLocker locker( m_mutex );
    if ( newState == m_state ) {
        return;
    }

    const DebuggerState oldState = m_state;
    DEBUGGER_STATE_CHANGE( oldState, newState );
    m_state = newState;
    emit stateChanged( newState, oldState );
}

void DebuggerAgent::doInterrupt( bool injectedProgram ) {
    m_mutex->lockInline();
    DEBUGGER_EVENT("Interrupt");
    if ( m_injectedScriptState == InjectedScriptAborting ) {
        DEBUGGER_EVENT("Abort evaluation of injected script");
        m_injectedScriptState = InjectedScriptNotRunning;
        m_injectedScriptId = -1;
        m_mutex->unlockInline();

        // Emit signal to inform that the evaluation was aborted
        emit evaluationInContextAborted( i18nc("@info", "Evaluation did not finish in time. "
                                               "Maybe there is an infinite loop?") );
    } else {
        m_checkRunningTimer->start( CHECK_RUNNING_WHILE_INTERRUPTED_INTERVAL );
        setState( Interrupted );
        const QDateTime timestamp = QDateTime::currentDateTime();
        const int lineNumber = m_lineNumber;
        const QString fileName = m_scriptIdToFileName[ m_currentScriptId ];
        m_mutex->unlockInline();

        // Emit changes in the backtrace/variables
        emitChanges();

        if ( !injectedProgram ) {
            emit interrupted( lineNumber, fileName, timestamp );
        }
    }

    forever {
        // Wait here until the debugger gets continued
        m_interruptMutex->lockInline();
        m_interruptWaiter->wait( m_interruptMutex/*, 500*/ );
        m_interruptMutex->unlockInline();

        // Continued, update the execution control value, which might have changed
        m_mutex->lockInline();
        ExecutionControl executionControl = m_executionControl;
        DEBUGGER_EVENT("Woke up from interrupt, to do now:" << executionControl);
        m_mutex->unlockInline();

        bool continueExecution = false;
        switch ( executionControl ) {
        case ExecuteAbort: {
            // Continued to be aborted
            const bool locked = m_engineMutex->tryLock( 250 );
            engine()->abortEvaluation();
            if ( !locked ) {
                kWarning() << "Could not lock the engine";
            }

            // Shut the debugger down
            shutdown();
            return;
        }
        case ExecuteAbortInjectedProgram:
            // Restore member variables
            m_mutex->lockInline();
            DEBUGGER_EVENT("Abort injected program");
            m_injectedScriptState = InjectedScriptAborting; // Handled in doInterrupt
            m_executionControl = m_previousExecutionControl;
            m_mutex->unlockInline();

            // Interrupt execution of injected script code,
            // it should be aborted by terminating the executing thread
            doInterrupt( true );
            return;

        // Check if execution should be interrupted again, ie. if it was just woken to do something
        case ExecuteInterrupt:
            m_mutex->lockInline();
            DEBUGGER_EVENT("Still interrupted");

            // A hint was set in functionExit() to trigger a call to emitChanges() after script
            // code was evaluated in the script context (in another thread) and might have changed
            // variables. In scriptUnload() a 0-timer gets started to call wakeFromInterrupt() so
            // that the variables in the script context and in the script execution thread are
            // available here. TODO
            if ( m_injectedScriptState == InjectedScriptUpdateVariablesInParentContext ) {
                m_injectedScriptId = -1;
            }
            m_mutex->unlockInline();

            // Update variables/backtrace
            emitChanges();

            // Let directly connected slots be executed in this agent's thread,
            // while execution is interrupted
            emit doSomething();
            break;

        default:
            // Continue script execution
            continueExecution = true;
            break;
        }

        if ( continueExecution ) {
            break;
        }
    }

    m_mutex->lockInline();
    ExecutionControl executionControl = m_executionControl;
    m_checkRunningTimer->start( CHECK_RUNNING_INTERVAL );
    const QDateTime timestamp = QDateTime::currentDateTime();
    m_mutex->unlockInline();

    setState( Running );

    if ( executionControl != ExecuteRunInjectedProgram ) {
        emit continued( timestamp,
                        executionControl != ExecuteContinue && executionControl != ExecuteRun );
    }
}

int DebuggerAgent::currentFunctionLineNumber() const
{
    QMutexLocker locker( m_mutex );
    if ( !m_currentContext ) {
        return -1;
    }
    QScriptContext *context = m_currentContext;
    while ( context ) {
        if ( context->thisObject().isFunction() ) {
            return QScriptContextInfo( context ).lineNumber();
        }
        context = context->parentContext();
    }
    return -1;
}

// TODO Remove?
void DebuggerAgent::checkExecution()
{
    checkHasExited();
}

bool DebuggerAgent::checkHasExited()
{
    QMutexLocker locker( m_mutex );
    if ( m_state == NotRunning ) {
        return true;
    } else if ( isInterrupted() ) {
        // If script execution is interrupted it is not finished
        return false;
    }

    bool isEvaluating;
    if ( m_engineMutex->tryLock(500) ) {
        isEvaluating = engine()->isEvaluating();
        m_engineMutex->unlockInline();
    } else {
        kWarning() << "Cannot lock the engine";
        if ( m_state == Aborting ) {
            engine()->abortEvaluation();
        }
        shutdown();
        return false;
    }

    if ( m_state != NotRunning && !isEvaluating ) {
        shutdown();
        return true;
    } else {
        return false;
    }
}

void DebuggerAgent::fireup()
{
    QMutexLocker locker( m_mutex );
    DEBUGGER_EVENT("Execution started");
    // First store start time
    const QDateTime timestamp = QDateTime::currentDateTime();

    setState( Running );
    m_lastRunAborted = false;
    m_hasUncaughtException = false;
    m_uncaughtExceptionLineNumber = -1;
    m_checkRunningTimer->start( CHECK_RUNNING_INTERVAL );

    emit started( timestamp );
}

void DebuggerAgent::shutdown()
{
    QMutexLocker locker( m_mutex );
    m_checkRunningTimer->stop();
    if ( m_state == NotRunning ) {
        kDebug() << "Not running";
        return;
    }
    DEBUGGER_EVENT("Execution stopped");

    // First store end time
    const QDateTime timestamp = QDateTime::currentDateTime();

    m_functionDepth = 0;
    const bool isPositionChanged = m_lineNumber != -1 || m_columnNumber != -1;

    // Context will be invalid
    m_currentContext = 0;

    const DebuggerState oldState = m_state;

    // Engine mutex is still locked here
    if ( oldState == Aborting ) {
        if ( engine()->isEvaluating() ) {
            kDebug() << "Still evaluating, abort";
            engine()->abortEvaluation();
        }

        DEBUGGER_EVENT("Was aborted");
        m_engineMutex->unlockInline();
        emit aborted();
        m_engineMutex->lockInline();
    }

    engine()->clearExceptions();

    setState( NotRunning );
    m_engineMutex->unlockInline();

    if ( isPositionChanged ) {
        const int oldLineNumber = m_lineNumber;
        const int oldColumnNumber = m_columnNumber;
        m_lineNumber = -1;
        m_columnNumber = -1;

        emit positionChanged( -1, -1, oldLineNumber, oldColumnNumber );
    }

    Scripting::Network *scriptNetwork = qobject_cast< Scripting::Network* >(
            engine()->globalObject().property("network").toQObject() );
    bool hasRunningRequests = scriptNetwork ? scriptNetwork->hasRunningRequests() : false;
    const int uncaughtExceptionLineNumber = m_uncaughtExceptionLineNumber;
    const QString uncaughtException = m_uncaughtException.toString();
    const QStringList backtrace = m_uncaughtExceptionBacktrace;

    emit stopped( timestamp, oldState == Aborting, hasRunningRequests,
                  uncaughtExceptionLineNumber, uncaughtException, backtrace );

    // Restore locked state of the engine mutex after execution ends
    // (needs to be locked before execution starts with eg. QScriptEngine::evaluate())
    m_engineMutex->lockInline();
}

void DebuggerAgent::exceptionCatch( qint64 scriptId, const QScriptValue &exception )
{
    kDebug() << scriptId << exception.toString();
}

void DebuggerAgent::exceptionThrow( qint64 scriptId, const QScriptValue &exceptionValue,
                                    bool hasHandler )
{
    Q_UNUSED( scriptId );
    if ( !hasHandler ) {
        m_mutex->lockInline();
        if ( m_injectedScriptState == InjectedScriptEvaluating ) {
            // Exception was thrown from injected code
            m_mutex->unlockInline();
        } else {
            int uncaughtExceptionLineNumber = m_lineNumber;
            m_hasUncaughtException = true;
            m_uncaughtExceptionLineNumber = uncaughtExceptionLineNumber;
            m_uncaughtException = exceptionValue;
            m_uncaughtExceptionBacktrace = engine()->uncaughtExceptionBacktrace();
            const DebugFlags debugFlags = m_debugFlags;
            const QString fileName = m_scriptIdToFileName[scriptId];
            DEBUGGER_EVENT("Uncatched exception in" << QFileInfo(fileName).fileName()
                           << "line" << uncaughtExceptionLineNumber << exceptionValue.toString());
            m_mutex->unlockInline();

            emit exception( uncaughtExceptionLineNumber, exceptionValue.toString(), fileName );

            if ( debugFlags.testFlag(InterruptOnExceptions) ) {
                // Interrupt at the exception
                doInterrupt();
            }

            abortDebugger();
        }
    }
}

QVariant DebuggerAgent::extension( QScriptEngineAgent::Extension extension,
                                   const QVariant &argument )
{
    return QScriptEngineAgent::extension( extension, argument );
}

QString DebuggerAgent::currentSourceFile() const
{
    QMutexLocker locker( m_mutex );
    return m_currentScriptId == -1 ? QString() : m_scriptIdToFileName[m_currentScriptId];
}

int DebuggerAgent::lineNumber() const
{
    QMutexLocker locker( m_mutex );
    return m_lineNumber;
}

int DebuggerAgent::columnNumber() const
{
    QMutexLocker locker( m_mutex );
    return m_columnNumber;
}

bool DebuggerAgent::hasUncaughtException() const
{
    QMutexLocker locker( m_mutex );
    return m_hasUncaughtException;
}

int DebuggerAgent::uncaughtExceptionLineNumber() const
{
    QMutexLocker locker( m_mutex );
    return m_uncaughtExceptionLineNumber;
}

QScriptValue DebuggerAgent::uncaughtException() const
{
    QMutexLocker locker( m_mutex );
    return m_uncaughtException;
}

ExecutionControl DebuggerAgent::currentExecutionControlType() const
{
    QMutexLocker locker( m_mutex );
    return m_executionControl;
}

DebugFlags DebuggerAgent::debugFlags() const
{
    QMutexLocker locker( m_mutex );
    return m_debugFlags;
}

void DebuggerAgent::setDebugFlags( DebugFlags debugFlags )
{
    QMutexLocker locker( m_mutex );
    m_debugFlags = debugFlags;
}

void DebuggerAgent::slotOutput( const QString &outputString, const QScriptContextInfo &contextInfo )
{
    emit output( outputString, contextInfo );
}

}; // namespace Debugger
