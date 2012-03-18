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

// PublicTransport engine includes
#include <engine/scripting.h>
#include <engine/timetableaccessor_script.h>
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
          m_mutex(new QMutex()), m_interruptWaiter(new QWaitCondition()),
          m_interruptMutex(new QMutex()), m_engineMutex(engineMutex),
          m_checkRunningTimer(new QTimer(this)), m_currentContext(0), m_interruptContext(0)
{
    m_mutex->lockInline();
    m_backtraceCleanedup = false;
    m_lineNumber = -1;
    m_columnNumber = -1;
    m_state = NotRunning;
    m_executionControl = ExecuteRun;
    m_currentFunctionLineNumber = -1;
    m_interruptFunctionLineNumber = -1;
    m_hasUncaughtException = false;
    m_uncaughtExceptionLineNumber = -1;
    m_injectedCodeContextLevel = -1;
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
    return m_state != NotRunning; // TODO
}

NextEvaluatableLineHint DebuggerAgent::canBreakAt( int lineNumber ) const
{
    kDebug() << "canBreakAt(" << lineNumber << ")";
    m_mutex->lockInline();
    const int scriptLineCount = m_scriptLines.count();
    const QStringList scriptLines = m_scriptLines;
    m_mutex->unlockInline();

    if ( lineNumber < 1 || lineNumber > scriptLineCount ) {
        return CannotFindNextEvaluatableLine;
    }

    QString line = scriptLines[ lineNumber - 1 ].trimmed();
    if ( line.isEmpty() || line.startsWith("//") ) {
        return NextEvaluatableLineBelow;
    }

    // Test if the line can be evaluated
    // If not, try if appending more lines makes the text evaluatable (multiline statement)
    if ( m_engineMutex->tryLock(100) ) {
        for ( int lines = 1; lines < 20 && lineNumber + lines <= scriptLines.count(); ++lines ) {
            QScriptSyntaxCheckResult result = engine()->checkSyntax( line );
            if ( result.state() == QScriptSyntaxCheckResult::Valid ) {
                m_engineMutex->unlockInline();
                return FoundEvaluatableLine;
            }
            line.append( '\n' + scriptLines[lineNumber - 1 + lines] );
        }

        m_engineMutex->unlockInline();
        return NextEvaluatableLineAbove;
    } else {
        kDebug() << "Could not lock engine.. It is most probably running";
        return CannotFindNextEvaluatableLine;
    }
}

int DebuggerAgent::getNextBreakableLineNumber( int lineNumber ) const
{
    kDebug() << "getNextBreakableLineNumber(" << lineNumber << ")";
    // Use lastHint to ensure, that the direction isn't changed
    NextEvaluatableLineHint lastHint = CannotFindNextEvaluatableLine;
    int count = 0;
    while ( lineNumber < m_scriptLines.count() && count < 15 ) {
        NextEvaluatableLineHint hint = canBreakAt( lineNumber );
        switch ( hint ) {
        case NextEvaluatableLineAbove:
            lineNumber += lastHint == NextEvaluatableLineBelow ? 1 : -1;
            break;
        case NextEvaluatableLineBelow:
            lineNumber += lastHint == NextEvaluatableLineAbove ? -1 : 1;
            break;
        case FoundEvaluatableLine:
            return lineNumber;
        case CannotFindNextEvaluatableLine:
            return -1;
        }

        lastHint = hint;
        ++count;
    }

    return -1;
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
                *returnValue = i18nc("@info", "Available commands: %1<nl />"
                        "Use <emphasis>.help</emphasis> with an argument to get more information about "
                        "individual commands<nl />"
                        "Syntax: %2", ConsoleCommand::availableCommands().join(", "),
                        ConsoleCommand::commandSyntax(command.command()) );
            }
        }
        return true;
    case ConsoleCommand::ClearCommand:
        return true;
    case ConsoleCommand::LineNumberCommand:
        if ( returnValue ) {
            *returnValue = QString::number( lineNumber() );
        }
        return true;
    case ConsoleCommand::BreakpointCommand: {
        bool ok, /*add = true,*/ breakpointExisted = false;
//         int enable = 0; TODO
        int lineNumber = command.argument().toInt( &ok );
        if ( !ok ) {
            return false;
        }

        lineNumber = getNextBreakableLineNumber( lineNumber );
        ok = lineNumber >= 0;
        if ( !ok ) {
            return false;
        }

        Breakpoint breakpoint = breakpointAt( lineNumber );
        if ( breakpoint.isValid() ) {
            breakpointExisted = true;
        } else {
            breakpoint = Breakpoint( lineNumber );
        }
        if ( command.arguments().count() == 1 ) {
            return ok;
        }

        // More than one argument given, ie. more than ".break <lineNumber> ..."
        const QString &argument = command.arguments().count() == 1
                ? QString() : command.argument( 1 );
        bool errorNotFound = false;
        QRegExp maxhitRegExp( "^maxhits(?:=|:)(\\d+)$", Qt::CaseInsensitive );
        if ( command.arguments().count() == 1 || argument.compare(QLatin1String("add")) == 0 ) {
            ok = addBreakpoint( breakpoint );
            *returnValue = ok ? i18nc("@info", "Breakpoint added at line %1", lineNumber)
                              : i18nc("@info", "Cannot add breakpoint at line %1", lineNumber);
        } else if ( argument.compare(QLatin1String("remove")) == 0 ) {
            if ( !breakpointExisted ) {
                errorNotFound = true;
            } else {
                ok = removeBreakpoint( breakpoint );
                *returnValue = ok ? i18nc("@info", "Breakpoint at line %1 removed", lineNumber)
                                  : i18nc("@info", "Cannot remove breakpoint at line %1", lineNumber);
            }
        } else if ( argument.compare(QLatin1String("toggle")) == 0 ) {
            if ( !breakpointExisted ) {
                errorNotFound = true;
            } else {
                breakpoint.setEnabled( !breakpoint.isEnabled() );
                ok = addBreakpoint( breakpoint );
                *returnValue = ok ? i18nc("@info", "Breakpoint toggled at line %1", lineNumber)
                                  : i18nc("@info", "Cannot toggle breakpoint at line %1", lineNumber);
            }
        } else if ( argument.compare(QLatin1String("enable")) == 0 ) {
            if ( !breakpointExisted ) {
                errorNotFound = true;
            } else {
                breakpoint.setEnabled();
                ok = addBreakpoint( breakpoint );
                *returnValue = ok ? i18nc("@info", "Breakpoint enabled at line %1", lineNumber)
                                  : i18nc("@info", "Cannot enable breakpoint at line %1", lineNumber);
            }
        } else if ( argument.compare(QLatin1String("disable")) == 0 ) {
            if ( !breakpointExisted ) {
                errorNotFound = true;
            } else {
                breakpoint.setEnabled( false );
                ok = addBreakpoint( breakpoint );
                *returnValue = ok ? i18nc("@info", "Breakpoint disabled at line %1", lineNumber)
                                  : i18nc("@info", "Cannot disable breakpoint at line %1", lineNumber);
            }
        } else if ( argument.compare(QLatin1String("reset")) == 0 ) {
            if ( !breakpointExisted ) {
                errorNotFound = true;
            } else {
                breakpoint.reset();
                ok = addBreakpoint( breakpoint );
                *returnValue = ok ? i18nc("@info", "Breakpoint reset at line %1", lineNumber)
                                  : i18nc("@info", "Cannot reset breakpoint at line %1", lineNumber);
            }
        } else if ( argument.compare(QLatin1String("condition")) == 0 ) {
            if ( !breakpointExisted ) {
                errorNotFound = true;
            } else if ( command.arguments().count() < 3 ) {
                // Needs at least 3 arguments: ".break <lineNumber> condition <conditionCode>"
                ok = false;
                *returnValue = i18nc("@info", "Condition code missing");
            } else {
                breakpoint.setCondition( QStringList(command.arguments().mid(2)).join(" ") );
                ok = addBreakpoint( breakpoint );
                *returnValue = ok ? i18nc("@info", "Breakpoint condition set to <emphasis>%1"
                                                   "</emphasis> at line %2",
                                          breakpoint.condition(), lineNumber)
                                  : i18nc("@info", "Cannot set breakpoint condition to <emphasis>%1"
                                                   "</emphasis> at line %1",
                                          breakpoint.condition(), lineNumber);
            }
        } else if ( maxhitRegExp.indexIn(argument) != -1 ) {
            if ( !breakpointExisted ) {
                errorNotFound = true;
            } else {
                breakpoint.setMaximumHitCount( maxhitRegExp.cap(1).toInt() );
                ok = addBreakpoint( breakpoint );
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
            ConsoleCommandExecutionControl executionControl = consoleCommandExecutionControlFromString( argument );
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
                &errorMessage, &backtrace, true ); // TODO interrupt at start, store in job
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

QScriptValue DebuggerAgent::evaluateInContext( const QString &program, const QString &contextName,
        bool *hadUncaughtException, int *errorLineNumber, QString *errorMessage,
        QStringList *backtrace, bool interruptAtStart )
{
    // Use new context for program evaluation
    if ( !m_engineMutex->tryLock(100) ) {
        kDebug() << "Cannot lock engine, it is most probably running, ie. not interrupted!";
        if ( hadUncaughtException ) {
            *hadUncaughtException = true;
        }
        if ( errorMessage ) {
            *errorMessage = "Cannot lock engine, it is most probably running, ie. not interrupted!"; // TODO
        }
        return QScriptValue();
    }
    QScriptContext *context = engine()->pushContext();
    m_engineMutex->unlockInline();

    // Store current execution type, to restore it later
    m_mutex->lockInline();
    const ExecutionControl executionType = m_executionControl;
    const int lineNumber = m_lineNumber;
    m_mutex->unlockInline();

    // Evaluating may block if script execution is currently interrupted,
    // this makes sure it runs over the given program and returns to where it was before
    // TODO
    if ( interruptAtStart ) {
//         m_interruptWaiter
        /*QTimer::singleShot( 0, this, SLOT(*/debugStepIntoInjectedProgram()/*) )*/;
    } else {
        /*QTimer::singleShot( 0, this, SLOT(*/debugRunInjectedProgram()/*) )*/;
    }

    // Start a countdown, if evaluation does not finish within this countdown, it gets aborted
    QTimer timer;
    timer.setSingleShot( true );
    connect( &timer, SIGNAL(timeout()), this, SLOT(cancelInjectedCodeExecution()) );
    timer.start( 2500 );

    // Evaluate program
    kDebug() << "Evaluate program" << program;
    m_engineMutex->lockInline();
    QScriptValue result = engine()->evaluate( program,
            contextName.isEmpty() ? "<Injected Code>" : contextName, lineNumber );
    m_engineMutex->unlockInline();
    kDebug() << "Done";

    timer.stop(); // Stop cancel timeout

    // Restore previous execution type (if not interrupted)
    if ( !interruptAtStart ) {
        m_mutex->lockInline();
        m_executionControl = executionType;
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
    kDebug() << "Evaluation did not finish in time, what now? ;)";
}

Breakpoint DebuggerAgent::setBreakpoint( int lineNumber, bool enable )
{
    kDebug() << "setBreakpoint(" << lineNumber << "," << enable << ")";
    Breakpoint breakpoint;
    if ( lineNumber < 0 ) {
        return breakpoint;
    }

    // Find a valid breakpoint line number near lineNumber (may be lineNumber itself)
    lineNumber = getNextBreakableLineNumber( lineNumber );

    m_mutex->lockInline();
    const bool hasBreakpoint = m_breakpoints.contains( lineNumber );
    if ( hasBreakpoint && !enable ) {
        kDebug() << "Remove breakpoint at line" << lineNumber;
        breakpoint = m_breakpoints[ lineNumber ];
        m_breakpoints.remove( lineNumber );
        m_mutex->unlockInline();
        emit breakpointRemoved( breakpoint );
    } else if ( !hasBreakpoint && enable ) {
        kDebug() << "Add breakpoint at line" << lineNumber;
        breakpoint = Breakpoint( lineNumber, enable );
        m_breakpoints.insert( lineNumber, breakpoint );
        m_mutex->unlockInline();
        emit breakpointAdded( breakpoint );
    } else {
        m_mutex->unlockInline();
    }

    return breakpoint;
}

bool DebuggerAgent::addBreakpoint( const Breakpoint &breakpoint )
{
    kDebug() << "addBreakpoint(" << breakpoint.lineNumber() << ")";
    if ( !breakpoint.isValid() ) {
        kDebug() << "Breakpoint is invalid" << breakpoint.lineNumber() << breakpoint.condition();
        return false;
    }
    if ( canBreakAt(breakpoint.lineNumber()) != FoundEvaluatableLine ) {
        kDebug() << "Cannot add breakpoint at" << breakpoint.lineNumber() << breakpoint.condition();
        return false;
    }

    m_mutex->lockInline();
    if ( m_breakpoints.contains(breakpoint.lineNumber()) ) {
        Breakpoint foundBreakpoint = m_breakpoints[ breakpoint.lineNumber() ];
        m_mutex->unlockInline();

        emit breakpointRemoved( foundBreakpoint );
        m_mutex->lockInline();
    }

    m_breakpoints.insert( breakpoint.lineNumber(), breakpoint );
    m_mutex->unlockInline();

    emit breakpointAdded( breakpoint );
    return true;
}

void DebuggerAgent::removeAllBreakpoints()
{
    kDebug() << "removeAllBreakpoints()";
    m_mutex->lockInline();
    QHash< uint, Breakpoint > breakpoints = m_breakpoints;
    m_breakpoints.clear();
    m_mutex->unlockInline();

    foreach ( const Breakpoint &breakpoint, breakpoints ) {
        kDebug() << "Remove breakpoint at line" << breakpoint.lineNumber();
        breakpoints.remove( breakpoint.lineNumber() );

        emit breakpointRemoved( breakpoint );
    }
}

bool DebuggerAgent::removeBreakpoint( int lineNumber )
{
    kDebug() << "removeBreakpoint(" << lineNumber << ")";
    lineNumber = getNextBreakableLineNumber( lineNumber );

    m_mutex->lockInline();
    if ( m_breakpoints.contains(lineNumber) ) {
        kDebug() << "Remove breakpoint at line" << lineNumber;
        const Breakpoint breakpoint = m_breakpoints[ lineNumber ];
        m_breakpoints.remove( lineNumber );
        m_mutex->unlockInline();
        emit breakpointRemoved( breakpoint );
        return true;
    }

    m_mutex->unlockInline();
    return false;
}

bool DebuggerAgent::removeBreakpoint( const Breakpoint &breakpoint )
{
    return removeBreakpoint( breakpoint.lineNumber() );
}

QList< uint > DebuggerAgent::breakpointLines() const
{
    return m_breakpoints.keys();
}

bool DebuggerAgent::debugControl( DebuggerAgent::ConsoleCommandExecutionControl controlType, const QVariant &argument,
                             QString *errorMessage )
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
        if ( !isInterrupted() /*&& m_executionType != ExecuteNotRunning*/ ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not interrupted!");
            }
            return false;
        }
        debugStepInto( argument.isValid() ? argument.toInt() : 0 );
        break;
    case ControlExecutionStepOver:
        if ( !isInterrupted() /*&& m_executionType != ExecuteNotRunning*/ ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not interrupted!");
            }
            return false;
        }
        debugStepOver( argument.isValid() ? argument.toInt() : 0 );
        break;
    case ControlExecutionStepOut:
        if ( !isInterrupted() /*&& m_executionType != ExecuteNotRunning*/ ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not interrupted!");
            }
            return false;
        }
        debugStepOut( argument.isValid() ? argument.toInt() : 0 );
        break;
    case ControlExecutionRunUntil: {
        bool ok;
        const int lineNumber = argument.toInt( &ok );
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
    m_repeatExecutionTypeCount = 0; // If execution type is repeatable, ie. ZstepInto/stepOver/stepOut
}

void DebuggerAgent::abortDebugger()
{
    kDebug() << "abortDebugger()";
    m_mutex->lockInline();
    if ( m_state == NotRunning ) {
        m_mutex->unlockInline();

        kDebug() << "Not running";
        checkHasExited();
    } else {
        m_executionControl = ExecuteAbort;
        m_mutex->unlockInline();
    }

    m_interruptWaiter->wakeAll();
}

void DebuggerAgent::debugInterrupt()
{
    kDebug() << "debugInterrupt()";
    QMutexLocker locker( m_mutex );
    if ( m_state == Interrupted ) {
        kDebug() << "Already interrupted";
        return;
    }/* else if ( m_state == NotRunning ) {
        kDebug() << "Not running";
        return;
    }*/
    m_executionControl = ExecuteInterrupt;
}

void DebuggerAgent::debugContinue()
{
    kDebug() << "debugContinue()";
    m_mutex->lockInline();
    if ( m_state == NotRunning ) {
        m_mutex->unlockInline();
        kDebug() << "Debugger is not running";
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
    if ( m_state == NotRunning ) {
        kDebug() << "Debugger is not running";
        return;
    }
    kDebug() << "debugStepInto()";
    m_repeatExecutionTypeCount = repeat;
    m_executionControl = ExecuteStepInto;
    m_interruptWaiter->wakeAll();
}

void DebuggerAgent::debugStepOver( int repeat )
{
    kDebug() << "debugStepOver(" << repeat << ")";
    if ( currentFunctionStartLineNumber() == -1 ) {
        // Not currently in a function, use step into. Otherwise this would equal debugContinue()
        debugStepInto( repeat );
    } else {
        QMutexLocker locker( m_mutex );
        m_mutex->lockInline();
        if ( m_state == NotRunning ) {
//             m_mutex->unlockInline();
            kDebug() << "Debugger is not interrupted or is not running";
            return;
        }
//         m_mutex->unlockInline();

//         m_engineMutex->lockInline();
//         QScriptContext *currentContext = engine()->currentContext();
//         m_engineMutex->unlockInline();

        const int lineNumber = currentFunctionLineNumber();
//         QMutexLocker locker( m_mutex );
        m_interruptContext = m_currentContext;
        m_repeatExecutionTypeCount = repeat;
        m_interruptFunctionLineNumber = lineNumber;
        m_executionControl = ExecuteStepOver;
        m_interruptWaiter->wakeAll();
    }
}

void DebuggerAgent::debugStepOut( int repeat )
{
    kDebug() << "debugStepOut(" << repeat << ")";
    QMutexLocker locker( m_mutex );
    const int lineNumber = currentFunctionLineNumber();
    if ( m_state == NotRunning) {
        kDebug() << "Debugger is not running";
        return;
    }

    m_repeatExecutionTypeCount = repeat;
    m_interruptFunctionLevel = 0;
//     m_interruptContext = engine()->currentContext()->parentContext();
//     m_interruptFunctionLineNumber = lineNumber;
    m_executionControl = ExecuteStepOut;
    m_interruptWaiter->wakeAll();
}

void DebuggerAgent::debugRunUntilLineNumber( int lineNumber )
{
    kDebug() << "debugRunUntilLineNumber(" << lineNumber << ")";
    addBreakpoint( Breakpoint::oneTimeBreakpoint(lineNumber) );

    QMutexLocker locker( m_mutex );
    m_executionControl = ExecuteRun;
    m_interruptWaiter->wakeAll();
}

void DebuggerAgent::debugRunInjectedProgram()
{
    QMutexLocker locker( m_mutex );
    kDebug() << "debugRunInjectedProgram()";
    m_backtraceCleanedup = false;
    m_repeatExecutionTypeCount = 0;
    m_injectedCodeContextLevel = 0;
//     m_interruptFunctionLevel = 0; // TODO
//     m_interruptContext = engine()->currentContext()->parentContext();
//     m_interruptFunctionLineNumber = currentFunctionLineNumber();
    m_executionControl = ExecuteRunInjectedProgram;
    m_interruptWaiter->wakeAll();
}

void DebuggerAgent::debugStepIntoInjectedProgram()
{
    QMutexLocker locker( m_mutex );
    kDebug() << "debugStepIntoInjectedProgram()";
    m_backtraceCleanedup = false;
    m_repeatExecutionTypeCount = 0;
    m_injectedCodeContextLevel = 0;
//     m_interruptFunctionLevel = 0;
//     m_interruptContext = engine()->currentContext()->parentContext();
//     m_interruptFunctionLineNumber = currentFunctionLineNumber();
    m_executionControl = ExecuteStepIntoInjectedProgram;
    m_interruptWaiter->wakeAll();
}

void DebuggerAgent::scriptLoad( qint64 id, const QString &program, const QString &fileName, int baseLineNumber )
{
    // TODO ONLY FOR THE MAIN SCRIPT FILE, eg. not for fileName == "Console Command"
    kDebug() << id /*<< program*/ << fileName << baseLineNumber;
    if ( id != -1 ) {
        QMutexLocker locker( m_mutex );
        kDebug() << "Load new script program" << id << fileName;
        m_scriptLines = program.split( '\n', QString::KeepEmptyParts );
    }
}

void DebuggerAgent::scriptUnload( qint64 id )
{
    kDebug() << "***** UNLOAD ******" << id;
    // TODO
}

void DebuggerAgent::contextPush()
{
    // TODO
//     kDebug() << "push";
}

void DebuggerAgent::contextPop()
{
    // TODO
//     kDebug() << "pop";
}

ExecutionControl DebuggerAgent::applyExecutionControl( ExecutionControl executionControl )
{
    switch ( executionControl ) {
        case ExecuteStepInto:
        case ExecuteStepIntoInjectedProgram:
            // Decrease repeation counter, if it is at 0 interrupt
            if ( m_repeatExecutionTypeCount > 0 ) {
                --m_repeatExecutionTypeCount;
            } else if ( m_repeatExecutionTypeCount == 0 ) {
                if ( m_executionControl != ExecuteAbort ) {
                    m_executionControl = ExecuteInterrupt;
                    executionControl = ExecuteInterrupt;
                }
            }
            break;
        case ExecuteStepOver:
            if ( m_currentContext == m_interruptContext ) {
                kDebug() << "Interrupt after step over";
                // Decrease repeation counter, if it is at 0 interrupt
                if ( m_repeatExecutionTypeCount > 0 ) {
                    --m_repeatExecutionTypeCount;
                } else if ( m_repeatExecutionTypeCount == 0 ) {
                    if ( m_executionControl != ExecuteAbort ) {
                        m_executionControl = ExecuteInterrupt;
                        executionControl = ExecuteInterrupt;
                    }
                    m_interruptContext = 0;
//                     m_interruptFunctionLineNumber = -1; TODO
                }
            } else {
                kDebug() << "Step over" << m_lineNumber;
            }
            break;
        case ExecuteStepOut: // Done in contextPop.. TODO
        case ExecuteRun:
//         case ExecuteNotRunning:
        case ExecuteInterrupt:
        case ExecuteRunInjectedProgram:
        default:
            break;
    }
    return executionControl;
}

void DebuggerAgent::positionChange( qint64 scriptId, int lineNumber, int columnNumber )
{
    Q_UNUSED( scriptId );

    // The engine should be locked here, before a call to eg. evaluate()
//     ThreadWeaver::debug( 10, QString("Position changed to script %1 line %2, column %3\n")
//                              .arg(scriptId).arg(lineNumber).arg(columnNumber).toUtf8() );

    // Lock the engine if not already locked (should normally be locked before script execution,
    // but currently it may get unlocked before the script is really done, eg. waiting idle for
    // network requests to finish)
    m_engineMutex->tryLockInline();
//     Q_ASSERT( !m_engineMutex->tryLock() ); // TODO Fails eg. after a network request
    DEBUGGER_DEBUG("Engine unlocked --------------------------------");
    QScriptContext *currentContext = engine()->currentContext();
    m_engineMutex->unlockInline();

    // Lock member variables and initialize
    m_mutex->lockInline();
    ExecutionControl executionControl = m_executionControl;
    DEBUGGER_DEBUG("Execution type:" << executionControl << "  line" << lineNumber << "col" << columnNumber);
    const bool injectedProgram = executionControl == ExecuteRunInjectedProgram;
    if ( !injectedProgram && m_state == NotRunning ) {
        // Execution has just started
        m_mutex->unlockInline();
        fireup(); // Updates m_state
        m_mutex->lockInline();
    }
    m_currentContext = currentContext;
    const int oldLineNumber = m_lineNumber;
    const int oldColumnNumber = m_columnNumber;
    DebuggerState state = m_state;

    // Decide if execution should be interrupted (breakpoints, execution control value, eg. step into).
    if ( !injectedProgram ) {
        // Update current execution position,
        // before emitting breakpointReached() and positionChanged()
        m_lineNumber = lineNumber;
        m_columnNumber = columnNumber;

        // Check for breakpoints at the current line
        const Breakpoint *breakpoint = findActiveBreakpoint( lineNumber );
        if ( breakpoint ) {
            // Reached a breakpoint, applyBreakpoints() may have written
            // a new value in m_executionControl (ExecuteInterrupt)
            if ( executionControl != ExecuteAbort ) {
                m_executionControl = executionControl = ExecuteInterrupt;
                cleanupBacktrace();
            }
            m_mutex->unlockInline();

            emit breakpointReached( *breakpoint );
        } else {
            // No breakpoint reached
            m_executionControl = executionControl = applyExecutionControl( executionControl );
            if ( executionControl == ExecuteInterrupt ) {
                cleanupBacktrace();
            }
            m_mutex->unlockInline();
        }

        if ( executionControl == ExecuteInterrupt ) {
            // TODO Enable/disable emit positionChanged/cleanupBacktrace even if not interrupted?
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
        bool locked = m_engineMutex->tryLock( 250 );
        if ( !locked ) {
            kDebug() << "Call QScriptEngine::abortEvaluation() without having lock";
        }
        engine()->abortEvaluation();
        if ( locked ) {
            m_engineMutex->unlockInline();
        }

        shutdown();
    } else if ( state != NotRunning ) {
        // Protect further script execution
        m_engineMutex->lockInline();
        DEBUGGER_DEBUG("Engine locked --------------------------------");
    }
}

Breakpoint *DebuggerAgent::findActiveBreakpoint( int lineNumber )
{
    // Test for a breakpoint at the new line number
    if ( m_breakpoints.contains(lineNumber) ) {
        // Found a breakpoint
        Breakpoint &breakpoint = m_breakpoints[ lineNumber ];

        if ( breakpoint.isEnabled() ) {
            // The found breakpoint is enabled
            kDebug() << "Breakpoint reached:" << lineNumber;
            breakpoint.reached(); // Increase hit count, etc.

            // Test breakpoint condition if any,
            // unlock m_mutex while m_engineMutex could be locked longer
            if ( !breakpoint.condition().isEmpty() ) {
                m_mutex->unlockInline();
                m_engineMutex->lockInline();
                const bool conditionSatisfied = breakpoint.testCondition( engine() );
                m_engineMutex->unlockInline();
                m_mutex->lockInline();

                if ( !conditionSatisfied ) {
                    kDebug() << "Breakpoint at" << lineNumber << "reached but it's condition"
                             << breakpoint.condition() << "did not match";
                    return 0;
                }
            }

            // Condition satisfied or no condition, active breakpoint found
            return &breakpoint;
        } else {
            kDebug() << "Breakpoint at" << lineNumber << "reached but it is disabled";
            return 0;
        }
    } else {
        // No breakpoint at the current execution position
        return 0;
    }
}

void DebuggerAgent::doInterrupt( bool injectedProgram ) {
    // Unlock engine while being interrupted
    // The engine should be locked here, ie. before starting an evaluation, that
    // got interrupted here
//         Q_ASSERT( !m_engineMutex->tryLock() );
//         m_engineMutex->unlockInline();

    kDebug() << "emit interrupted() **************************** " << thread();

    m_mutex->lockInline();
    m_checkRunningTimer->start( CHECK_RUNNING_WHILE_INTERRUPTED_INTERVAL );
    m_state = Interrupted;
    ExecutionControl executionControl = m_executionControl;
    m_mutex->unlockInline();

    if ( !injectedProgram ) {
        emit interrupted();
    }

    forever {
        m_interruptMutex->lockInline();
        kDebug() << "wait" << thread();
        m_interruptWaiter->wait( m_interruptMutex/*, 500*/ );
        m_interruptMutex->unlockInline();

        m_mutex->lockInline();
        executionControl = m_executionControl;
        m_mutex->unlockInline();

        if ( executionControl == ExecuteAbort ) {
            m_engineMutex->lockInline();
            engine()->abortEvaluation();
            m_engineMutex->unlockInline();

            shutdown();
            return;
        }
        if ( executionControl == ExecuteInterrupt ) {
            kDebug() << "Still interrupted";
        } else {
            kDebug() << "ready" << thread();
            break;
        }
    }
    kDebug() << "Zzzzz... The debugger just woke up..." << thread();

    m_mutex->lockInline();
    kDebug() << "Execution type D continued:" << m_executionControl;
    m_state = Running;
    m_checkRunningTimer->start( CHECK_RUNNING_INTERVAL );
    if ( executionControl != ExecuteRunInjectedProgram ) {
        m_mutex->unlockInline();
        kDebug() << "emit continued()";
        emit continued();
    } else {
        kDebug() << m_backtraceCleanedup;
        m_backtraceCleanedup = false;
        cleanupBacktrace();
        m_mutex->unlockInline();
    }
//         m_interruptMutex->unlockInline();
}

void DebuggerAgent::cleanupBacktrace()
{
    if ( !m_backtraceCleanedup || m_lastBacktrace.isEmpty() ) {
        m_backtraceCleanedup = true;
//         const int oldFunctionLineNumber = m_currentFunctionLineNumber;
        m_currentFunctionLineNumber = currentFunctionLineNumber();
        const FrameStack newBacktrace = buildBacktrace();
        const FrameStack oldBacktrace = m_lastBacktrace;
        m_lastBacktrace = newBacktrace;
        BacktraceChange change = compareBacktraces( newBacktrace, oldBacktrace );

        if ( change != NoBacktraceChange ) {
            m_mutex->unlockInline();
            emit backtraceChanged( newBacktrace, change );
            m_mutex->lockInline();
        }

//         switch ( change ) {
//         case EnteredFunction:
// //             kDebug() << "Entered function";
// //             ++m_interruptFunctionLevel;
//             break;
//         case ExitedFunction:
// //             kDebug() << "Exited function";
// // //             --m_interruptFunctionLevel; TODO
// //             if ( m_executionControl == ExecuteStepOut && m_interruptFunctionLevel < 0 ) {
// // //                 if ( oldFunctionLineNumber == m_interruptFunctionLineNumber ) {
// //                 kDebug() << "Interrupt at return";
// //                 m_interruptContext = 0;
// //                 m_interruptFunctionLineNumber = -1;
// //                 m_executionControl = ExecuteInterrupt;
// // //                 }
// //             }
//             break;
//         case NoBacktraceChange:
//         default:
//             break;
//         }
    }
}

Variables DebuggerAgent::variables( int frame )
{
    m_mutex->lockInline();
    QScriptContext *context = m_currentContext;
    m_mutex->unlockInline();

    while ( frame > 0 && context->parentContext() ) {
        context = context->parentContext();
        --frame;
    }
    if ( frame > 0 ) {
        kDebug() << "Could not step up higher in the backtrace" << frame;
    }
    return buildVariables( context->activationObject() );
}

QString DebuggerAgent::variableValueTooltip( const QString &completeValueString,
                                             bool encodeHtml, const QChar &endCharacter ) const
{
    if ( completeValueString.isEmpty() ) {
        return QString();
    }

    QString tooltip = completeValueString.left( 1000 );
    if ( encodeHtml ) {
        if ( !endCharacter.isNull() ) {
            tooltip += endCharacter; // Add end character (eg. a quotation mark), which got cut off
        }
        tooltip = Global::encodeHtmlEntities( tooltip );
    }
    if ( tooltip.length() < completeValueString.length() ) {
        tooltip.prepend( i18nc("@info Always plural",
                         "<emphasis strong='1'>First %1 characters:</emphasis><nl />", 1000) )
               .append( QLatin1String("...") );
    }
    return tooltip.prepend( QLatin1String("<p>") ).append( QLatin1String("</p>") );
}

QPair<KColorScheme::BackgroundRole, KColorScheme::ForegroundRole> checkTimetableInformation(
        TimetableInformation info, const QVariant &value )
{
    bool correct = value.isValid();
    if ( correct ) {
        switch ( info ) {
        case DepartureDateTime:
        case ArrivalDateTime:
            correct = value.toDateTime().isValid();
            break;
        case DepartureDate:
        case ArrivalDate:
            correct = value.toDate().isValid();
            break;
        case DepartureTime:
        case ArrivalTime:
            correct = value.toTime().isValid();
            break;
        case TypeOfVehicle:
            correct = PublicTransportInfo::getVehicleTypeFromString( value.toString() ) != Unknown;
            break;
        case TransportLine:
        case Target:
        case TargetShortened:
        case Platform:
        case DelayReason:
        case JourneyNews:
        case JourneyNewsOther:
        case JourneyNewsLink:
        case Operator:
        case Status:
        case StartStopName:
        case StartStopID:
        case StopCity:
        case StopCountryCode:
        case TargetStopName:
        case TargetStopID:
        case Pricing:
        case StopName:
        case StopID:
            correct = !value.toString().trimmed().isEmpty();
            break;
        case Delay:
            correct = value.canConvert( QVariant::Int ) && value.toInt() >= -1;
            break;
        case Duration:
        case StopWeight:
        case Changes:
        case RouteExactStops:
            correct = value.canConvert( QVariant::Int ) && value.toInt() >= 0;
            break;
        case TypesOfVehicleInJourney:
        case RouteTimes:
        case RouteTimesDeparture:
        case RouteTimesArrival:
        case RouteTypesOfVehicles:
        case RouteTimesDepartureDelay:
        case RouteTimesArrivalDelay:
            correct = !value.toList().isEmpty();
            break;
        case IsNightLine:
            correct = value.canConvert( QVariant::Bool );
            break;
        case RouteStops:
        case RouteStopsShortened:
        case RouteTransportLines:
        case RoutePlatformsDeparture:
        case RoutePlatformsArrival:
            correct = !value.toStringList().isEmpty();
            break;

        default:
            correct = true;;
        }
    }

    const KColorScheme scheme( QPalette::Active );
    return QPair<KColorScheme::BackgroundRole, KColorScheme::ForegroundRole>(
            correct ? KColorScheme::PositiveBackground : KColorScheme::NegativeBackground,
            correct ? KColorScheme::PositiveText : KColorScheme::NegativeText );
}

Variables DebuggerAgent::buildVariables( const QScriptValue &value, bool onlyImportantObjects ) const
{
    QScriptValueIterator it( value );
    Variables variables;
    const KColorScheme scheme( QPalette::Active );
    while ( it.hasNext() ) {
        it.next();
        if ( (onlyImportantObjects && !it.value().isQObject()) ||
             it.value().isError() || it.flags().testFlag(QScriptValue::SkipInEnumeration) ||
             it.name() == "NaN" || it.name() == "undefined" || it.name() == "Infinity" )
        {
            continue;
        }

        Variable variable;
        variable.isHelperObject = it.name() == "helper" || it.name() == "network" ||
                it.name() == "storage" || it.name() == "result" || it.name() == "accessor";
        if ( onlyImportantObjects && !variable.isHelperObject ) {
            continue;
        }

        QString valueString;
        bool encodeValue = false;
        QChar endCharacter;
        if ( it.value().isArray() ) {
            valueString = QString("[%0]").arg( it.value().toVariant().toStringList().join(", ") );
            endCharacter = ']';
        } else if ( it.value().isString() ) {
            valueString = QString("\"%1\"").arg( it.value().toString() );
            encodeValue = true;
            endCharacter = '\"';
        } else if ( it.value().isRegExp() ) {
            valueString = QString("/%1/%2").arg( it.value().toRegExp().pattern() )
                    .arg( it.value().toRegExp().caseSensitivity() == Qt::CaseSensitive ? "" : "i" );
            encodeValue = true;
        } else if ( it.value().isFunction() ) {
            valueString = QString("function %1()").arg( it.name() ); // it.value() is the function definition
        } else {
            valueString = it.value().toString();
        }

        const QString completeValueString = valueString;
        const int cutPos = valueString.indexOf( '\n' );
        if ( cutPos != -1 ) {
            valueString = valueString.left( cutPos ) + " ...";
        }

        variable.name = it.name();
        variable.value = it.value().toVariant();
        variable.description = variableValueTooltip( completeValueString, encodeValue, endCharacter );

        if ( it.value().isRegExp() ) {
            variable.icon = KIcon("code-variable");
            variable.type = Variable::RegExp;
        } else if ( it.value().isFunction() ) {
            variable.icon = KIcon("code-function");
            variable.type = Variable::Function;
        } else if ( it.value().isArray() || it.value().isBool() || it.value().isBoolean() ||
                    it.value().isDate() || it.value().isNull() || it.value().isNumber() ||
                    it.value().isString() || it.value().isUndefined() )
        {
            variable.icon = KIcon("code-variable");
            if ( it.value().isDate() ) {
                variable.type = Variable::Date;
            } else if ( it.value().isNumber() ) {
                variable.type = Variable::Number;
            } else if ( it.value().isNull() || it.value().isUndefined() ) {
                variable.type = Variable::Null;
                variable.backgroundRole = KColorScheme::NegativeBackground;
                variable.foregroundRole = KColorScheme::NegativeText;
            } else if ( it.value().isArray() ) {
                variable.type = Variable::Array;
            } else if ( it.value().isBool() ) {
                variable.type = Variable::Boolean;
            } else if ( it.value().isString() ) {
                variable.type = Variable::String;
            }
        } else if ( it.value().isObject() || it.value().isQObject() || it.value().isQMetaObject() ) {
            variable.icon = KIcon("code-class");
            variable.type = Variable::Object;
        } else if ( it.value().isError() ) {
            variable.icon = KIcon("dialog-error");
            variable.type = Variable::Error;
            variable.backgroundRole = KColorScheme::NegativeBackground;
            variable.foregroundRole = KColorScheme::NegativeText;
        } else {
            variable.icon = KIcon("code-context");
        }

        variable.sorting = 9999;
        if ( !it.value().isQObject() && !it.value().isQMetaObject() ) {
            // Sort to the end
            variable.sorting = 10000;
        } else if ( it.name() == TimetableAccessorScript::SCRIPT_FUNCTION_GETTIMETABLE ||
                    it.name() == TimetableAccessorScript::SCRIPT_FUNCTION_GETJOURNEYS ||
                    it.name() == TimetableAccessorScript::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS ||
                    it.name() == TimetableAccessorScript::SCRIPT_FUNCTION_USEDTIMETABLEINFORMATIONS )
        {
            // Sort to the beginning
            variable.sorting = 0;
        } else if ( variable.isHelperObject ) {
            variable.sorting = 1;
        }

        if ( it.name() == "result" ) {
            // Add special items for the "result" script object, which is an exposed ResultObject object
            ResultObject *result = qobject_cast< ResultObject* >( it.value().toQObject() );
            Q_ASSERT( result );
            const QString valueString = i18ncp("@info/plain", "%1 result", "%1 results",
                                               result->count());
            variable.value = valueString;
            variable.description = valueString;

            Variable dataItem( Variable::Special, i18nc("@info/plain", "Data"), valueString,
                               KIcon("documentinfo") );

            int i = 1;
            QList<TimetableInformation> shortInfoTypes = QList<TimetableInformation>()
                    << Target << TargetStopName << DepartureDateTime << DepartureTime << StopName;
            foreach ( const TimetableData &data, result->data() ) {
                QString shortInfo;
                foreach ( TimetableInformation infoType, shortInfoTypes) {
                    if ( data.contains(infoType) ) {
                        shortInfo = data[ infoType ].toString();
                        break;
                    }
                }
                Variable resultItem( Variable::Special, i18nc("@info/plain", "Result %1", i),
                        QString("<%1>").arg(shortInfo), KIcon("code-class") );
                resultItem.sorting = i;
                for ( TimetableData::ConstIterator it = data.constBegin();
                      it != data.constEnd(); ++it )
                {
                    QString valueString;
                    const bool isList = it.value().isValid() &&
                                        it.value().canConvert( QVariant::List );
                    if ( isList ) {
                        const QVariantList list = it.value().toList();
                        QStringList stringList;
                        int count = 0;
                        for ( int i = 0; i < list.count(); ++i ) {
                            const QString str = list[i].toString();
                            count += str.length();
                            if ( count > 100 ) {
                                stringList << "...";
                                break;
                            }
                            stringList << str;
                        }
                        valueString = '[' + stringList.join(", ") + ']';
                    } else {
                        valueString = it.value().toString();
                    }
                    Variable timetableInformationItem( Variable::Special,
                            Global::timetableInformationToString(it.key()),
                            valueString, KIcon("code-variable") );
                    QPair<KColorScheme::BackgroundRole, KColorScheme::ForegroundRole> colors =
                            checkTimetableInformation( it.key(), it.value() );
                    timetableInformationItem.backgroundRole = colors.first;
                    timetableInformationItem.foregroundRole = colors.second;

                    if ( isList ) {
                        const QVariantList list = it.value().toList();
                        for ( int i = 0; i < list.count(); ++i ) {
                            Variable listItem( Variable::String, QString::number(i + 1),
                                              list[i].toString(), KIcon("code-variable") );
                            listItem.sorting = i;
                            timetableInformationItem.children << listItem;
                        }
                    }
                    resultItem.children << timetableInformationItem;
                }
                ++i;
                dataItem.children << resultItem;
            }
            variable.children << dataItem;
        } else if ( it.name() == "network" ) {
            // Add special items for the "network" script object, which is an exposed Network object
            Network *network = qobject_cast< Network* >( it.value().toQObject() );
            Q_ASSERT( network );
            const QString valueString = i18ncp("@info/plain", "%1 request", "%1 requests",
                                               network->runningRequests().count());
            variable.value = valueString;
            variable.description = valueString;

            Variable requestsItem( Variable::Special, i18nc("@info/plain", "Running Requests"),
                                   valueString, KIcon("documentinfo") );
            int i = 1;
            foreach ( NetworkRequest *networkRequest, network->runningRequests() ) {
                Variable requestItem( Variable::Special, i18nc("@info/plain", "Request %1", i),
                                      networkRequest->url(), KIcon("code-class") );
                requestItem.sorting = i;
                requestsItem.children << requestItem;
                ++i;
            }
            variable.children << requestsItem;
        } else if ( it.name() == "storage" ) {
            // Add special items for the "storage" script object, which is an exposed Storage object
            Storage *storage = qobject_cast< Storage* >( it.value().toQObject() );
            Q_ASSERT( storage );
            const QVariantMap memory = storage->read();
            const QString valueString = i18ncp("@info/plain", "%1 value", "%1 values",
                                               memory.count());
            variable.value = valueString;
            variable.description = valueString;

            Variable memoryItem( Variable::Special, i18nc("@info/plain", "Memory"),
                                 valueString, KIcon("documentinfo") );
            int i = 1;
            for ( QVariantMap::ConstIterator it = memory.constBegin();
                  it != memory.constEnd(); ++it )
            {
                Variable valueItem( Variable::Special, it.key(),
                        variableValueTooltip(it.value().toString(), encodeValue, endCharacter),
                        KIcon("code-variable") );
                valueItem.sorting = i;
                memoryItem.children << valueItem;
                ++i;
            }
            variable.children << memoryItem;
        } else if ( it.name() == "helper" ) {
            const QString valueString = i18nc("@info/plain", "Offers helper functions to scripts");
            variable.value = valueString;
            variable.description = valueString;
        } else if ( it.name() == "accessor" ) {
            const QString valueString = i18nc("@info/plain", "Exposes accessor information to "
                                              "scripts, which got read from the XML file");
            variable.value = valueString;
            variable.description = valueString;
        }

        // Recursively add children, not for functions, max 1000 children
        if ( variable.children.count() < 1000 && !it.value().isFunction() ) {
            variable.children << buildVariables( it.value() );
        }

        variables << variable;
    }
    return variables;
}

FrameStack DebuggerAgent::backtrace() const
{
    QMutexLocker locker( m_mutex );
    return m_backtraceCleanedup ? m_lastBacktrace : buildBacktrace();
}

FrameStack DebuggerAgent::buildBacktrace() const
{
    int depth = 0;
    QScriptContext *context = m_currentContext;
    FrameStack backtrace;
    while ( context ) {
        const QScriptContextInfo info( context );
        QString contextString = info.functionName();
        if ( contextString.isEmpty() ) {
            contextString = context->thisObject().equals( m_globalObject )
                    ? "<global>" : "<anonymous>";
        }
        backtrace.push( Frame(info.fileName(), contextString, info.lineNumber(), depth++) );
        context = context->parentContext();
    }
    return backtrace;
}

int DebuggerAgent::currentFunctionLineNumber() const
{
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

void DebuggerAgent::functionEntry( qint64 scriptId )
{
    if ( scriptId != -1 ) {
        QMutexLocker locker( m_mutex );
        m_backtraceCleanedup = false;
    }

    QMutexLocker locker( m_mutex );
    ++m_interruptFunctionLevel;
}

void DebuggerAgent::functionExit( qint64 scriptId, const QScriptValue& returnValue )
{
//     kDebug() << engine()->isEvaluating() << scriptId << returnValue.toString() << "-*** ------------------------------***-";
    if ( scriptId != -1 ) {
        m_mutex->lockInline();
        m_backtraceCleanedup = false;
        m_mutex->unlockInline();

        checkExecution();
    }

    m_mutex->lockInline();
    bool interrupt = false;
    bool injectedCodeFinished = false;
    if ( m_interruptFunctionLevel >= 0 && m_executionControl == ExecuteStepOut ) {
        --m_interruptFunctionLevel;
        interrupt = m_interruptFunctionLevel == -1;
        kDebug() << "New step out function level" << m_interruptFunctionLevel << interrupt;
    }
    if ( m_injectedCodeContextLevel >= 0 ) {
        --m_injectedCodeContextLevel;
        injectedCodeFinished = m_injectedCodeContextLevel < 0 &&
                m_executionControl == ExecuteRunInjectedProgram;
    }
    m_mutex->unlockInline();

    if ( injectedCodeFinished ) {
        kDebug() << returnValue.toString() << "EVALUATION IN CONTEXT FINISHED";
        emit evaluationInContextFinished( returnValue );
    }

    if ( interrupt ) {
        kDebug() << "Interrupt now";
        doInterrupt();
    }
}

BacktraceChange DebuggerAgent::compareBacktraces( const FrameStack &backtrace,
                                                       const FrameStack &oldBacktrace ) const
{
    if ( backtrace.count() > oldBacktrace.count() ) {
        return EnteredFunction;
    } else if ( backtrace.count() < oldBacktrace.count() ) {
        return ExitedFunction;
    } else {
        return NoBacktraceChange;
    }
}

void DebuggerAgent::checkExecution()
{
    checkHasExited();
}

bool DebuggerAgent::checkHasExited()
{
//     m_engineMutex->lockInline(); TEST
    bool isEvaluating;
    if ( m_engineMutex->tryLock(100) ) {
        isEvaluating = engine()->isEvaluating();
        m_engineMutex->unlockInline();
    } else {
//         kDebug() << "Cannot lock the engine";
        return false;
    }
//     const QScriptContextInfo functionInfo( engine()->currentContext() );
//     m_engineMutex->unlockInline(); TEST

    if ( isInterrupted() ) {
        return false;
    }

    m_mutex->lockInline();
    if ( m_state != NotRunning && !isEvaluating ) {
        m_mutex->unlockInline();
        shutdown();
        kDebug() << "CHECK EXECUTION    FINISHED YES";
        return true;
    } else {
        m_mutex->unlockInline();
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
    m_engineMutex->unlockInline();

    m_state = Running;
    m_hasUncaughtException = false;
    m_uncaughtExceptionLineNumber = -1;
    kDebug() << "FIREUP" << m_lineNumber << m_columnNumber;
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

    kDebug() << "SHUTDOWN" << m_lineNumber << m_columnNumber;
    const bool isPositionChanged = m_lineNumber != -1 || m_columnNumber != -1;
    m_state = NotRunning;

    if ( !m_lastBacktrace.isEmpty() ) {
        const FrameStack oldBacktrace = m_lastBacktrace;
        m_lastBacktrace.clear();
        BacktraceChange change = compareBacktraces( m_lastBacktrace, oldBacktrace );
        m_mutex->unlockInline();

        emit backtraceChanged( FrameStack(), change );
    } else {
        m_mutex->unlockInline();
    }

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

void DebuggerAgent::exceptionThrow( qint64 scriptId, const QScriptValue &exceptionValue, bool hasHandler )
{
    if ( !hasHandler ) {
        QScriptEngine *_engine = engine();
//         m_engineMutex->lockInline(); // TODO should already be locked? unlock for emit?
        const int uncaughtExceptionLineNumber = _engine->uncaughtExceptionLineNumber();
        m_engineMutex->unlockInline();

        m_mutex->lockInline();
        m_hasUncaughtException = true;
        m_uncaughtExceptionLineNumber = uncaughtExceptionLineNumber;
        m_uncaughtException = exceptionValue;
        m_mutex->unlockInline();

        kDebug() << "Uncatched exception in" << uncaughtExceptionLineNumber << exceptionValue.toString();
        debugInterrupt();
        emit exception( uncaughtExceptionLineNumber, exceptionValue.toString() );

        m_engineMutex->lockInline();
        engine()->clearExceptions();
//         m_engineMutex->unlockInline();
    }
}

QVariant DebuggerAgent::extension( QScriptEngineAgent::Extension extension, const QVariant &argument )
{
    kDebug() << extension << argument.toString();
    return QScriptEngineAgent::extension( extension, argument );
}

int DebuggerAgent::currentFunctionStartLineNumber() const
{
    QMutexLocker locker( m_mutex );
    return m_currentFunctionLineNumber;
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

Breakpoint DebuggerAgent::toggleBreakpoint( int lineNumber )
{
    kDebug() << "toggleBreakpoint(" << lineNumber << ")";
    Breakpoint::State breakpoint = breakpointState( lineNumber );
    return setBreakpoint( lineNumber, breakpoint == Breakpoint::NoBreakpoint );
}

Breakpoint DebuggerAgent::breakpointAt( int lineNumber ) const
{
    kDebug() << "breakpointAt(" << lineNumber << ")";
    QMutexLocker locker( m_mutex );
    return m_breakpoints.contains( lineNumber ) ? m_breakpoints[lineNumber] : Breakpoint();
}

Breakpoint::State DebuggerAgent::breakpointState( int lineNumber ) const
{
    kDebug() << "breakpointState(" << lineNumber << ")";
    QMutexLocker locker( m_mutex );
    return !m_breakpoints.contains( lineNumber ) ? Breakpoint::NoBreakpoint
           : ( m_breakpoints[lineNumber].isEnabled() ? Breakpoint::EnabledBreakpoint
                                                     : Breakpoint::DisabledBreakpoint );
}

ExecutionControl DebuggerAgent::currentExecutionControlType() const
{
    QMutexLocker locker( m_mutex );
    return m_executionControl;
}

void DebuggerAgent::slotOutput( const QString &outputString, const QScriptContextInfo &contextInfo )
{
    emit output( outputString, contextInfo );
}

}; // namespace Debugger
