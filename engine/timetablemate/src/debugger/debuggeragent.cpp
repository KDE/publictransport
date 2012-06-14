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
#include <engine/scripting.h>
#include <engine/serviceproviderscript.h>
#include <engine/global.h>

// KDE includes
#include <KDebug>
#include <KLocalizedString>
#include <threadweaver/DebuggingAids.h>

// Qt includes
#include <QScriptEngine>
#include <QScriptContextInfo>
#include <QScriptValueIterator>
#include <QTime>
#include <QTimer>
#include <QMutex>
#include <QWaitCondition>
#include <QApplication>

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

DebuggerAgent::DebuggerAgent( QScriptEngine *engine, QMutex *engineMutex )
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
    m_backtraceCleanedup = true;
    m_lineNumber = -1;
    m_columnNumber = -1;
    m_state = NotRunning;
    m_injectedScriptState = InjectedScriptNotRunning;
    m_injectedScriptId = -1;
    m_executionControl = ExecuteRun;
    m_previousExecutionControl = ExecuteRun;
    m_hasUncaughtException = false;
    m_uncaughtExceptionLineNumber = -1;
    m_interruptFunctionLevel = -2;
    m_functionDepth = 0;
    connect( m_checkRunningTimer, SIGNAL(timeout()), this, SLOT(checkExecution()) );
    m_mutex->unlockInline();

    QMutexLocker engineLocker( m_engineMutex );
    engine->setProcessEventsInterval( 100 );

    // Install custom print function (overwriting the builtin print function)
    QScriptValue printFunction = engine->newFunction( debugPrintFunction );
    printFunction.setData( engine->newQObject(this) );
    engine->globalObject().setProperty( "print", printFunction );

//     kDebug() << "Check execution in 250ms";
//     QTimer::singleShot( 250, this, SLOT(checkExecution()) );
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
    return m_state != NotRunning;
}

bool DebuggerAgent::isLastRunAborted() const
{
    QMutexLocker locker( m_mutex );
    return m_lastRunAborted;
}

NextEvaluatableLineHint DebuggerAgent::canBreakAt( int lineNumber, const QStringList &programLines )
{
    kDebug() << "canBreakAt(" << lineNumber << ")";
    const int scriptLineCount = programLines.count();
    if ( lineNumber < 1 || lineNumber > scriptLineCount ) {
        return CannotFindNextEvaluatableLine;
    }

    QString line = programLines[ lineNumber - 1 ].trimmed();
    if ( line.isEmpty() || line.startsWith("//") ) {
        return NextEvaluatableLineBelow;
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
    kDebug() << "getNextBreakableLineNumber(" << lineNumber << ")";
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

NextEvaluatableLineHint DebuggerAgent::canBreakAt( int lineNumber ) const
{
    QMutexLocker locker( m_mutex );
    return canBreakAt( lineNumber, m_scriptLines );
}

int DebuggerAgent::getNextBreakableLineNumber( int lineNumber ) const
{
    QMutexLocker locker( m_mutex );
    return getNextBreakableLineNumber( lineNumber, m_scriptLines );
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

        lineNumber = getNextBreakableLineNumber( lineNumber );
        ok = lineNumber >= 0;
        if ( !ok ) {
            *returnValue = i18nc("@info", "Cannot interrupt script execution at line %1",
                                 lineNumber);
            return false;
        }

        const bool breakpointExists = m_breakpoints.contains( lineNumber );
        kDebug() << "Breakpoint exists" << breakpointExists;
        Breakpoint breakpoint( breakpointExists ? m_breakpoints[lineNumber] : lineNumber );
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
                if ( returnValue && !ok ) {
                    *returnValue = i18nc("@info", "Cannot execute command: <message>%1</message>",
                                         errorMessage);
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
        return true; // Wait for interrupt
    }

    default:
        kDebug() << "Command execution not implemented" << command.command();
        return false;
    }
}

QVariant DebuggerAgent::evaluateInExternalEngine( const QString &program, const QString &contextName,
                                                  bool *hadUncaughtException, int *errorLineNumber,
                                                  QString *errorMessage, QStringList *backtrace,
                                                  DebugFlags debugFlags )
{
//     TODO One engine per console?
    QScriptEngine engine2;
//     QScriptValue activationObject = engine2.newVariant( context->activationObject().toVariant() );
//     QScriptValue thisObject = engine2.newVariant( context->thisObject().toVariant() );
//     QScriptValue globalObject = engine2.newVariant( engine()->globalObject().toVariant() );
//     m_engineMutex->unlockInline();

//     engine2.setGlobalObject( globalObject );
//     engine2.currentContext()->setActivationObject( activationObject );
//     engine2.currentContext()->setThisObject( thisObject );
    QScriptValue result = engine2.evaluate( program,
            contextName.isEmpty() ? "<Injected Code>" : contextName );

    // Store current execution type, to restore it later
//     m_mutex->lockInline();
//     const ExecutionControl executionType = m_executionControl;
//     m_mutex->unlockInline();

//     QTimer timer;
//     if ( debugMode == InterruptAtStart ) {
//         debugStepIntoInjectedProgram();
//     } else {
//         debugRunInjectedProgram();
//
//         // Start a countdown, if evaluation does not finish within this countdown, it gets aborted
//         timer.setSingleShot( true );
//         connect( &timer, SIGNAL(timeout()), this, SLOT(cancelInjectedCodeExecution()) );
//         timer.start( 3000 ); // 3 seconds time for the evaluation
//     }
//
//     // Evaluate program
//     m_engineMutex->lockInline();
//     QScriptValue result = engine()->evaluate( program,
//             contextName.isEmpty() ? "<Injected Code>" : contextName );
//     m_engineMutex->unlockInline();
//     timer.stop(); // Stop cancel timeout
//
//     // Restore previous execution type (if not interrupted)
//     if ( debugMode != InterruptAtStart ) {
//         m_mutex->lockInline();
//         m_executionControl = executionType;
//         m_mutex->unlockInline();
//     }
//
//     QMutexLocker locker( m_engineMutex );
    kDebug() << "Evaluate-in-context result" << result.toString() << program;
    if ( hadUncaughtException ) {
        *hadUncaughtException = engine2.hasUncaughtException();
    }
    if ( errorLineNumber ) {
        *errorLineNumber = engine2.uncaughtExceptionLineNumber();
    }
    if ( errorMessage ) {
        *errorMessage = engine2.uncaughtException().toString();
    }
    if ( backtrace ) {
        *backtrace = engine2.uncaughtExceptionBacktrace();
    }
    if ( engine2.hasUncaughtException() ) {
        kDebug() << "Uncaught exception in program:"
                 << engine2.uncaughtExceptionBacktrace();
    }

    // Transfer values from evaluation context to script context
//     QScriptValueIterator it( engine2.currentContext()->activationObject() );
//     QScriptValue scriptContext = engine()->currentContext()->activationObject();
//     if ( it.hasNext() ) {
//         it.next();
//         scriptContext.setProperty( it.name(), it.value() );
//     }

    return result.toVariant();
}

QScriptValue DebuggerAgent::evaluateInContext( const QString &program, const QString &contextName,
        bool *hadUncaughtException, int *errorLineNumber, QString *errorMessage,
        QStringList *backtrace, DebugFlags debugFlags )
{
    // Use new context for program evaluation
    if ( !m_engineMutex->tryLock(100) ) {
        kDebug() << "Cannot lock engine, it is most probably running, ie. not interrupted!";
        if ( hadUncaughtException ) {
            *hadUncaughtException = true;
        }
        if ( errorMessage ) {
            *errorMessage = "Cannot lock engine, it is most probably running, ie. not interrupted!";
        }
        return QScriptValue();
    }
    QScriptContext *context = engine()->pushContext();
    m_engineMutex->unlockInline();

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
    m_engineMutex->lockInline();
    QScriptValue result = engine()->evaluate( program,
            contextName.isEmpty() ? "<Injected Code>" : contextName );
    m_engineMutex->unlockInline();
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

    QMutexLocker locker( m_engineMutex );
    kDebug() << "Evaluate-in-context result" << result.toString() << program;
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
    m_mutex->lockInline();
    if ( m_injectedScriptState == InjectedScriptEvaluating ) {
        kDebug() << "Evaluation did not finish in time or was cancelled";
        m_executionControl = ExecuteAbortInjectedProgram;
        m_mutex->unlockInline();
    } else {
        m_mutex->unlockInline();

        kDebug() << "Not running injected code";
        checkHasExited();
    }

    m_interruptWaiter->wakeAll();
}

void DebuggerAgent::addBreakpoint( const Breakpoint &breakpoint )
{
    kDebug() << "Insert breakpoint" << breakpoint.lineNumber() << breakpoint.hitCount();
    m_breakpoints.insert( breakpoint.lineNumber(), breakpoint );
}

void DebuggerAgent::removeBreakpoint( const Breakpoint &breakpoint )
{
    m_breakpoints.remove( breakpoint.lineNumber() );
}

bool DebuggerAgent::debugControl( DebuggerAgent::ConsoleCommandExecutionControl controlType,
                                  const QVariant &argument, QString *errorMessage )
{
    switch ( controlType ) {
    case ControlExecutionContinue:
        if ( !isInterrupted() /*&& m_executionType != ExecuteNotRunning*/ ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not interrupted!");
            }
            return false;
        }
        debugContinue();
        break;
    case ControlExecutionInterrupt:
        m_mutex->lockInline();
        if ( m_state != Running ) {
            m_mutex->unlockInline();
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not running! Start the debugger first.");
            }
            return false;
        }
        m_mutex->unlockInline();
        debugInterrupt();
        break;
    case ControlExecutionAbort:
        m_mutex->lockInline();
        if ( m_state != Running ) {
            m_mutex->unlockInline();
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not running! Start the debugger first.");
            }
            return false;
        }
        m_mutex->unlockInline();
        abortDebugger();
        break;
    case ControlExecutionStepInto:
        if ( !isInterrupted() ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not interrupted!");
            }
            return false;
        }
        debugStepInto( argument.isValid() ? argument.toInt() - 1 : 0 );
        break;
    case ControlExecutionStepOver:
        if ( !isInterrupted() ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not interrupted!");
            }
            return false;
        }
        debugStepOver( argument.isValid() ? argument.toInt() - 1 : 0 );
        break;
    case ControlExecutionStepOut:
        if ( !isInterrupted() ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not interrupted!");
            }
            return false;
        }
        debugStepOut( argument.isValid() ? argument.toInt() - 1 : 0 );
        break;
    case ControlExecutionRunUntil: {
        bool ok;
        int lineNumber = argument.toInt( &ok );
        m_mutex->lockInline();
        const int scriptLineCount = m_scriptLines.count();
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

        lineNumber = getNextBreakableLineNumber( lineNumber );
        if ( lineNumber < 0 ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Cannot interrupt script execution at line %!",
                                      lineNumber);
            }
            return false;
        }

        debugRunUntilLineNumber( lineNumber );
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
    m_mutex->lockInline();
    if ( m_state == Aborting ) {
        kDebug() << "Is already aborting";
        return;
    }
    if ( m_state == NotRunning ) {
        if ( m_injectedScriptState == InjectedScriptEvaluating ) {
            m_mutex->unlockInline();
            cancelInjectedCodeExecution();
        } else {
            m_mutex->unlockInline();

//             // Debugger is not running, check to be sure..
//             checkHasExited();
        }
    } else {
        m_lastRunAborted = true;
        m_executionControl = ExecuteAbort;
        m_mutex->unlockInline();

        setState( Aborting );
    }

    m_interruptWaiter->wakeAll();
}

void DebuggerAgent::debugInterrupt()
{
    QMutexLocker locker( m_mutex );
    if ( m_state == Interrupted ) {
        kDebug() << "Already interrupted";
        return;
    } else if ( m_state == Aborting ) {
        kDebug() << "Aborting";
        return;
    }
        /* else if ( m_state == NotRunning ) {
        kDebug() << "Not running";
        return;
    }*/
    m_executionControl = ExecuteInterrupt;
}

void DebuggerAgent::debugContinue()
{
    m_mutex->lockInline();
    if ( m_state == NotRunning ) {
        m_mutex->unlockInline();
        kDebug() << "Debugger is not running";
        return;
    } else if ( m_state == Aborting ) {
        kDebug() << "Aborting";
        return;
    }
    m_executionControl = ExecuteRun;
    m_interruptWaiter->wakeAll();
    m_mutex->unlockInline();
}

void DebuggerAgent::debugStepInto( int repeat )
{
    kDebug() << "debugStepInto(" << repeat << ")";
    QMutexLocker locker( m_mutex );
    if ( m_state != Interrupted ) {
        kDebug() << "Debugger is not interrupted";
        return;
    }

    // Wake from interrupt and run until the next statement
    m_repeatExecutionTypeCount = repeat;
    m_executionControl = ExecuteStepInto;
    m_interruptWaiter->wakeAll();
}

void DebuggerAgent::debugStepOver( int repeat )
{
    kDebug() << "debugStepOver(" << repeat << ")";
    QMutexLocker locker( m_mutex );
    if ( m_state != Interrupted ) {
        kDebug() << "Debugger is not interrupted";
        return;
    }

    // Wake from interrupt and run until the next statement in the function level
//     m_interruptContext = m_currentContext;
    m_interruptFunctionLevel = 0;
    m_repeatExecutionTypeCount = repeat;
    m_executionControl = ExecuteStepOver;
    kDebug() << "STEP OVER" << m_interruptFunctionLevel;
    m_interruptWaiter->wakeAll();
}

void DebuggerAgent::debugStepOut( int repeat )
{
    kDebug() << "debugStepOut(" << repeat << ")";
    QMutexLocker locker( m_mutex );
    if ( m_state != Interrupted) {
        kDebug() << "Debugger is not interrupted";
        return;
    }

    // Wake from interrupt and run until the current function gets exited
    m_interruptFunctionLevel = 0;
    m_repeatExecutionTypeCount = repeat;
    m_executionControl = ExecuteStepOut;
    m_interruptWaiter->wakeAll();
}

void DebuggerAgent::debugRunUntilLineNumber( int lineNumber )
{
    kDebug() << "debugRunUntilLineNumber(" << lineNumber << ")";
        QMutexLocker locker( m_mutex );
    m_runUntilLineNumber = getNextBreakableLineNumber( lineNumber );

    if ( m_runUntilLineNumber != -1 ) {
        m_executionControl = ExecuteRun;
        m_interruptWaiter->wakeAll();
    }
}

void DebuggerAgent::debugRunInjectedProgram()
{
    QMutexLocker locker( m_mutex );
    kDebug() << "debugRunInjectedProgram()";
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
    kDebug() << "debugStepIntoInjectedProgram()";
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

void DebuggerAgent::setScriptText( const QString &program )
{
    QMutexLocker locker( m_mutex );
    m_scriptLines = program.split( '\n', QString::KeepEmptyParts );
}

void DebuggerAgent::scriptLoad( qint64 id, const QString &program, const QString &fileName,
                                int baseLineNumber )
{
    kDebug() << id /*<< program*/ << fileName << baseLineNumber;
    if ( id != -1 ) {
        m_mutex->lockInline();
        if ( m_injectedScriptState == InjectedScriptInitializing ) {
            // The new script is code that should be executed in the current scripts context
            // while the main script is interrupted
            m_injectedScriptId = id;
            m_injectedScriptState = InjectedScriptEvaluating;
            m_mutex->unlockInline();
        } else if ( m_executionControl != ExecuteRunInjectedProgram &&
                    m_executionControl != ExecuteStepIntoInjectedProgram )
        {
            m_mutex->unlockInline();
            kDebug() << "Load new script program" << id << fileName;
            setScriptText( program );
        }
    }
}

void DebuggerAgent::scriptUnload( qint64 id )
{
    Q_UNUSED( id );
    if ( m_injectedScriptState == InjectedScriptUpdateVariablesInParentContext ) {
        m_injectedScriptState = InjectedScriptNotRunning;
        QTimer::singleShot( 0, this, SLOT(wakeFromInterrupt()) );
    }
}

void DebuggerAgent::wakeFromInterrupt()
{
    m_interruptWaiter->wakeAll();
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
        m_mutex->lockInline();
        ++m_functionDepth;

        m_backtraceCleanedup = false;
        if ( m_interruptFunctionLevel >= -1 &&
            (m_executionControl == ExecuteStepOver || m_executionControl == ExecuteStepOut) )
        {
            ++m_interruptFunctionLevel;
        }
//         kDebug() << "m_functionDepth =" << m_functionDepth;
        m_mutex->unlockInline();

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

    m_mutex->lockInline();
    if ( scriptId != -1 && m_injectedScriptId == scriptId ) {
        kDebug() << "Evaluation in context finished with" << returnValue.toString();
        m_executionControl = m_previousExecutionControl;
        m_mutex->unlockInline();
        emit evaluationInContextFinished( returnValue );

        // Interrupts again if it was interrupted before,
        // but variables can be updated in the script context
        m_mutex->lockInline();
        m_injectedScriptState = InjectedScriptUpdateVariablesInParentContext;
        m_mutex->unlockInline();
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
        m_mutex->unlockInline();
    }

    if ( scriptId != -1 ) {
        m_mutex->lockInline();
        --m_functionDepth;
        const int functionDepth = m_functionDepth;
        m_mutex->unlockInline();

        if ( functionDepth == 0 ) {
            // Engine mutex is still locked here, unlock for checkHasExited()
            m_engineMutex->unlockInline();
            if ( checkHasExited() ) {
                return;
            } else {
                m_engineMutex->lockInline();
            }
        }
    }
}

ExecutionControl DebuggerAgent::applyExecutionControl( ExecutionControl executionControl,
                                                       QScriptContext *currentContext )
{
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
            } else {
                kDebug() << "Step over" << m_lineNumber;
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
    m_engineMutex->lockInline();
    QScriptContext *context = engine()->currentContext();

    m_mutex->lockInline();
    m_currentContext = context;
    m_backtraceCleanedup = true;
    m_mutex->unlockInline();

    if ( context ) {
        emit variablesChanged( VariableChange::fromContext(context) );

        // The Frame object is already created and added to the model with empty values
        // (in DebuggerAgent::functionEntry())
        const bool isGlobal = context->thisObject().equals( engine()->globalObject() );
        const Frame frame( QScriptContextInfo(context), isGlobal );
        emit backtraceChanged( BacktraceChange(UpdateBacktraceFrame, frame) );
    }
    m_engineMutex->unlockInline();
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
    DEBUGGER_DEBUG("Engine unlocked --------------------------------");

    // Lock member variables and initialize
    m_mutex->lockInline();
    ExecutionControl executionControl = m_executionControl;
    DEBUGGER_DEBUG("Execution type:" << executionControl << "  line" << lineNumber << "col" << columnNumber);
    const bool injectedProgram = m_injectedScriptState == InjectedScriptEvaluating;
    if ( !injectedProgram && m_state == NotRunning ) {
        // Execution has just started
        m_mutex->unlockInline();
        fireup(); // Updates m_state
        m_mutex->lockInline();
    }

    const int oldLineNumber = m_lineNumber;
    const int oldColumnNumber = m_columnNumber;
    DebuggerState state = m_state;

    // Decide if execution should be interrupted (breakpoints, execution control value, eg. step into).
    if ( !injectedProgram ) {
        // Update current execution position,
        // before emitting breakpointReached() and positionChanged()
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
        }

        if ( executionControl == ExecuteInterrupt ) {
            emit positionChanged( lineNumber, columnNumber, oldLineNumber, oldColumnNumber );

            // Interrupt script execution
            doInterrupt( injectedProgram );

            // Script execution continued, update values
            m_mutex->lockInline();
            executionControl = m_executionControl;
            state = m_state;
            m_mutex->unlockInline();
        }
    } else {
        // Do not update execution position or check for breakpoints, if in an injected program
        m_mutex->unlockInline();
    }

    DEBUGGER_DEBUG("Execution control type after interrupt:" << executionControl);

    // Check if debugging should be aborted
    if ( executionControl == ExecuteAbort ) {
        m_mutex->lockInline();
        m_injectedScriptId = -1;
        m_injectedScriptState = InjectedScriptNotRunning;
        m_mutex->unlockInline();

        const bool locked = m_engineMutex->tryLock( 250 );
        engine()->abortEvaluation();
        if ( locked ) {
            m_engineMutex->unlockInline();
        }

        kDebug() << "Shutdown";
        shutdown();
    } else if ( executionControl == ExecuteAbortInjectedProgram ) {
        // Restore member variables
        m_mutex->lockInline();
        m_injectedScriptState = InjectedScriptAborting; // Handled in doInterrupt
        m_executionControl = m_previousExecutionControl;
        m_mutex->unlockInline();

        // TEST
        // Interrupt execution of injected script code,
        // it should be aborted by terminating the executing thread
        doInterrupt( true );
    } else if ( state != NotRunning ) {
        // Protect further script execution
        m_engineMutex->lockInline();
        DEBUGGER_DEBUG("Engine locked --------------------------------");
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
    // m_mutex is locked

    // Test for a breakpoint at the new line number
    if ( !m_breakpoints.contains(lineNumber) ) {
        // No breakpoint at the current execution position
        return false;
    }

    Breakpoint &breakpoint = m_breakpoints[ lineNumber ];
    // Found a breakpoint
    if ( breakpoint.isEnabled() ) {
        // Test breakpoint condition if any,
        // unlock m_mutex while m_engineMutex could be locked longer
        if ( !breakpoint.condition().isEmpty() ) {
            m_mutex->unlockInline();
            const bool conditionSatisfied = breakpoint.testCondition( this, conditionError );
            m_mutex->lockInline();

            if ( !conditionSatisfied ) {
//                 kDebug() << "Breakpoint at" << lineNumber << "reached but it's condition"
//                          << breakpoint.condition() << "did not match";
                return false;
            }
        }
        // The found breakpoint is enabled
        kDebug() << "Breakpoint reached:" << lineNumber;
        breakpoint.reached(); // Increase hit count, etc.

        // Condition satisfied or no condition, active breakpoint found
        foundBreakpoint = &breakpoint;
        return true;
    } else {
        kDebug() << "Breakpoint at" << lineNumber << "reached but it is disabled";
        return false;
    }
}

void DebuggerAgent::setState( DebuggerState newState )
{
    m_mutex->lockInline();
    if ( newState == m_state ) {
        kDebug() << "State unchanged";
        m_mutex->unlockInline();
        return;
    }

    const DebuggerState oldState = m_state;
    m_state = newState;
    m_mutex->unlockInline();

    emit stateChanged( newState, oldState );
}

void DebuggerAgent::doInterrupt( bool injectedProgram ) {
    // Unlock engine while being interrupted
    // The engine should be locked here, ie. before starting an evaluation, that
    // got interrupted here
//         Q_ASSERT( !m_engineMutex->tryLock() );
//         m_engineMutex->unlockInline();

    kDebug() << "Interrupt";
    m_mutex->lockInline();
    if ( m_injectedScriptState != InjectedScriptAborting ) {
        m_checkRunningTimer->start( CHECK_RUNNING_WHILE_INTERRUPTED_INTERVAL );
        setState( Interrupted );
        m_mutex->unlockInline();

        // Emit changes in the backtrace/variables
        emitChanges();

        if ( !injectedProgram ) {
            emit interrupted();
        }
    } else {
        kDebug() << "ABORT EVALUATION";
        m_injectedScriptState = InjectedScriptNotRunning;
        m_injectedScriptId = -1;
        m_mutex->unlockInline();

        // Emit signal to inform that the evaluation was aborted
        emit evaluationInContextAborted( i18nc("@info", "Evaluation did not finish in time. "
                                               "Maybe there is an infinite loop?") );
    }

    forever {
        // Wait here until the debugger gets continued
        m_interruptMutex->lockInline();
        kDebug() << "Wait" << thread();
        m_interruptWaiter->wait( m_interruptMutex/*, 500*/ );
        m_interruptMutex->unlockInline();

        // Continued, update the execution control value, which might have changed
        m_mutex->lockInline();
        ExecutionControl executionControl = m_executionControl;
        m_mutex->unlockInline();

        if ( executionControl == ExecuteAbort ) {
            // Continued to be aborted
            const bool locked = m_engineMutex->tryLock( 250 );
            engine()->abortEvaluation();
            if ( locked ) {
                m_engineMutex->unlockInline();
            }

            // Shut the debugger down
            shutdown();
            return;
        } else if ( executionControl == ExecuteAbortInjectedProgram ) {
            // Restore member variables
            m_mutex->lockInline();
            m_injectedScriptState = InjectedScriptAborting; // Handled in doInterrupt
            m_executionControl = m_previousExecutionControl;
            m_mutex->unlockInline();

            // Interrupt execution of injected script code,
            // it should be aborted by terminating the executing thread
            doInterrupt( true );
            return;
        }

        // Check if execution should be interrupted again, ie. if it was just woken to do something
        if ( executionControl == ExecuteInterrupt ) {
            kDebug() << "Still interrupted";
            m_mutex->lockInline();
            // A hint was set in functionExit() to trigger a call to emitChanges() after script
            // code was evaluated in the script context (in another thread) and might have changed
            // variables. In scriptUnload() a 0-timer gets started to call wakeFromInterrupt() so
            // that the variables in the script context and in the script execution thread are
            // available here.
            if ( m_injectedScriptState == InjectedScriptUpdateVariablesInParentContext ) {
                m_injectedScriptId = -1;
                m_mutex->unlockInline();

                // Update after evaluation in context,
                // which might have changed variables in script context
                emitChanges();
            } else {
                m_mutex->unlockInline();
            }
        } else {
            // Continue script execution
            break;
        }
    }

    kDebug() << "Continue";
    m_mutex->lockInline();
    ExecutionControl executionControl = m_executionControl;
    setState( Running );
    m_checkRunningTimer->start( CHECK_RUNNING_INTERVAL );
    m_mutex->unlockInline();

    if ( executionControl != ExecuteRunInjectedProgram ) {
        emit continued( executionControl != ExecuteContinue && executionControl != ExecuteRun );
    }
}

int DebuggerAgent::currentFunctionLineNumber() const
{
    if ( !m_currentContext ) {
        return -1;
    }

//     QMutexLocker engineLocker( m_engineMutex );
    QScriptContext *context = m_currentContext;

    while ( context ) {
        if ( context->thisObject().isFunction() ) {
            return QScriptContextInfo( context ).lineNumber();
        }
        context = context->parentContext();
    }
    return -1;
}

void DebuggerAgent::checkExecution()
{
    checkHasExited();
}

bool DebuggerAgent::checkHasExited()
{
    m_mutex->lockInline();
    DebuggerState state = m_state;
    m_mutex->unlockInline();

    if ( state == NotRunning ) {
        return true;
    } else if ( isInterrupted() ) {
        // If script execution is interrupted it is not finished
        return false;
    }

    bool isEvaluating;
    if ( m_engineMutex->tryLock(100) ) {
        isEvaluating = engine()->isEvaluating();
        m_engineMutex->unlockInline();
    } else {
        kWarning() << "Cannot lock the engine";
        return false;
    }

    if ( state != NotRunning && !isEvaluating ) {
        shutdown();
        return true;
    } else {
        return false;
    }
}

void DebuggerAgent::fireup()
{
    // First lock the engine (m_engineMutex), then the agents member variables (m_mutex),
    // otherwise m_mutex would lock as long as m_engineMutex does
    m_engineMutex->lockInline();
    m_mutex->lockInline();
    m_globalObject = engine()->globalObject();
    m_lastRunAborted = false;
    m_engineMutex->unlockInline();

    setState( Running );
//     m_functionDepth = 0;
    m_hasUncaughtException = false;
    m_uncaughtExceptionLineNumber = -1;
    m_checkRunningTimer->start( CHECK_RUNNING_INTERVAL );
    m_mutex->unlockInline();

    emit started();
}

void DebuggerAgent::shutdown()
{
    m_mutex->lockInline();
    m_checkRunningTimer->stop();
    if ( m_state == NotRunning ) {
        m_mutex->unlockInline();
        kDebug() << "Not running";
        return;
    }

    m_functionDepth = 0;
    const bool isPositionChanged = m_lineNumber != -1 || m_columnNumber != -1;

    // Context will be invalid
    m_currentContext = 0;

    DebuggerState oldState = m_state;
    m_mutex->unlockInline();

    if ( oldState == Aborting ) {
        emit aborted();
    }
    setState( NotRunning );
    emit stopped();

    if ( isPositionChanged ) {
        const int oldLineNumber = m_lineNumber;
        const int oldColumnNumber = m_columnNumber;

        m_mutex->lockInline();
        m_lineNumber = -1;
        m_columnNumber = -1;
        m_mutex->unlockInline();

        emit positionChanged( -1, -1, oldLineNumber, oldColumnNumber );
    }
}

void DebuggerAgent::exceptionCatch( qint64 scriptId, const QScriptValue &exception )
{
    kDebug() << scriptId << exception.toString();
}

void DebuggerAgent::exceptionThrow( qint64 scriptId, const QScriptValue &exceptionValue,
                                    bool hasHandler )
{
    Q_UNUSED( scriptId );
    if ( !hasHandler && m_injectedScriptState != InjectedScriptEvaluating ) {
        QScriptEngine *_engine = engine();
        const int uncaughtExceptionLineNumber = _engine->uncaughtExceptionLineNumber();
        m_engineMutex->unlockInline();

        m_mutex->lockInline();
        m_hasUncaughtException = true;
        m_uncaughtExceptionLineNumber = uncaughtExceptionLineNumber;
        m_uncaughtException = exceptionValue;
        const DebugFlags debugFlags = m_debugFlags;
        m_mutex->unlockInline();

        kDebug() << "Uncatched exception in" << uncaughtExceptionLineNumber
                 << exceptionValue.toString();
        emit exception( uncaughtExceptionLineNumber, exceptionValue.toString() );

        if ( debugFlags.testFlag(InterruptOnExceptions) ) {
            // Interrupt at the exception
            doInterrupt();
        }

        // Continue execution
        m_engineMutex->lockInline();
        engine()->clearExceptions();
    }
}

QVariant DebuggerAgent::extension( QScriptEngineAgent::Extension extension, const QVariant &argument )
{
    kDebug() << extension << argument.toString();
    return QScriptEngineAgent::extension( extension, argument );
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
