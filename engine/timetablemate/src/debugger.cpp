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
#include "debugger.h"

// KDE includes
#include <KDebug>
#include <KLocalizedString>

// Qt includes
#include <QTime>
#include <QTimer>
#include <QScriptEngine>
#include <QScriptContextInfo>
#include <QApplication>

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
    Debugger *debugger = qobject_cast<Debugger*>( calleeData.toQObject() );
    debugger->slotOutput( result, context->parentContext() );
    return engine->undefinedValue();
};

Debugger::Debugger( QScriptEngine *engine )
        : QObject(engine), QScriptEngineAgent(engine), m_lastContext(0), m_interruptContext(0)
{
    m_backtraceCleanedup = false;
    m_running = false;
    m_lineNumber = -1;
    m_columnNumber = -1;
    m_executionType = ExecuteRun;
    m_currentFunctionLineNumber = -1;
    m_interruptFunctionLineNumber = -1;

    engine->setProcessEventsInterval( 100 );

    // Install custom print function (overwriting the builtin print function)
    QScriptValue printFunction = engine->newFunction( debugPrintFunction );
    printFunction.setData( engine->newQObject(this) );
    engine->globalObject().setProperty( "print", printFunction );
}

Debugger::~Debugger()
{
    abortDebugger();
}

Debugger::NextEvaluatableLineHint Debugger::canBreakAt( int lineNumber ) const
{
    if ( lineNumber <= 0 || lineNumber > m_scriptLines.count() ) {
        return CannotFindNextEvaluatableLine;
    }

    QString line = m_scriptLines[ lineNumber - 1 ].trimmed();
    if ( line.isEmpty() || line.startsWith("//") ) {
        return NextEvaluatableLineBelow;
    }

    // Test if the line can be evaluated
    // If not, try if appending more lines makes the text evaluatable (multiline statement)
    for ( int lines = 1; lines < 25; ++lines ) {
        QScriptSyntaxCheckResult result = engine()->checkSyntax( line );
        if ( result.state() == QScriptSyntaxCheckResult::Valid ) {
            return FoundEvaluatableLine;
        }
        line.append( '\n' + m_scriptLines[lineNumber - 1 + lines] );
    }

    return NextEvaluatableLineAbove;
}

int Debugger::getNextBreakableLineNumber( int lineNumber ) const
{
    // Use lastHint to ensure, that the direction isn't changed
    NextEvaluatableLineHint lastHint = CannotFindNextEvaluatableLine;
    int count = 0;
    while ( lineNumber < m_scriptLines.count() && count < 25 ) {
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

void Breakpoint::setCondition( const QString &condition )
{
    m_condition = condition;
}

bool Debugger::executeCommand( const DebuggerCommand &command, QString *returnValue )
{
    if ( !command.isValid() ) {
        return false;
    }

    switch ( command.command() ) {
    case DebuggerCommand::HelpCommand:
        if ( returnValue ) {
            if ( !command.arguments().isEmpty() ) {
                // "help" command with at least one argument
                const DebuggerCommand::Command commandType =
                        DebuggerCommand::commandFromName( command.argument() );
                *returnValue = i18nc("@info", "Command <emphasis>%1</emphasis>: %2<nl />"
                                              "Syntax: %3",
                        command.argument(), DebuggerCommand::commandDescription(commandType),
                        DebuggerCommand::commandSyntax(commandType));
            } else {
                // "help" command without arguments
                *returnValue = i18nc("@info", "Available commands: %1<nl />"
                        "Use <emphasis>.help</emphasis> with an argument to get more information about "
                        "individual commands<nl />"
                        "Syntax: %2", DebuggerCommand::availableCommands().join(", "),
                        DebuggerCommand::commandSyntax(command.command()) );
            }
        }
        return true;
    case DebuggerCommand::ClearCommand:
        return true;
    case DebuggerCommand::LineNumberCommand:
        if ( returnValue ) {
            *returnValue = QString::number( lineNumber() );
        }
        return true;
    case DebuggerCommand::BreakpointCommand: {
        bool ok, add = true, breakpointExisted = false;
        int enable = 0;
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
    case DebuggerCommand::DebuggerControlCommand: {
        ExecutionControl executionControl = executionControlFromString( command.argument() );
        bool ok = executionControl != InvalidControlExecution;
        if ( ok ) {
            QString errorMessage;
            ok = debugControl( executionControl, command.argument(1), &errorMessage );
            if ( returnValue && !ok ) {
                *returnValue = i18nc("@info", "Cannot execute command: <message>%1</message>",
                                     errorMessage);
            }
        } else if ( returnValue ) {
            *returnValue = i18nc("@info", "Unexcepted argument <emphasis>%1</emphasis><nl />"
                                 "Expected one of these: "
                                 "<emphasis>continue</emphasis>, "
                                 "<emphasis>interrupt</emphasis>, "
                                 "<emphasis>abort</emphasis>, "
                                 "<emphasis>stepinto &lt;count = 1&gt;</emphasis>, "
                                 "<emphasis>stepover &lt;count = 1&gt;</emphasis>, "
                                 "<emphasis>stepout &lt;count = 1&gt;</emphasis>, "
                                 "<emphasis>rununtil &lt;lineNumber&gt;</emphasis>",
                                 command.argument());
        }
        return ok;
    }
    case DebuggerCommand::DebugCommand: {
        bool error;
        int errorLineNumber;
        QString errorMessage;
        QStringList backtrace;
        QScriptValue result = evaluateInContext( command.arguments().join(" "),
                i18nc("@info/plain", "Console Debug Command"), &error, &errorLineNumber,
                &errorMessage, &backtrace, true );
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

QScriptValue Debugger::evaluateInContext( const QString &program, const QString &contextName,
        bool *hadUncaughtException, int *errorLineNumber, QString *errorMessage,
        QStringList *backtrace, bool interruptAtStart )
{
    // Use new context for program evaluation
    QScriptContext *context = engine()->pushContext();

    // Store current execution type, to restore it later
    ExecutionType executionType = m_executionType;

    // Evaluating may block if script execution is currently interrupted,
    // this makes sure it runs over the given program and returns to where it was before
    if ( interruptAtStart ) {
        QTimer::singleShot( 0, this, SLOT(debugStepIntoInjectedProgram()) );
    } else {
        QTimer::singleShot( 0, this, SLOT(debugRunInjectedProgram()) );
    }

    // Evaluate program
    QScriptValue result = engine()->evaluate( program,
            contextName.isEmpty() ? "<Injected Code>" : contextName, m_lineNumber );

    // Restore previous execution type (if not interrupted)
    if ( !interruptAtStart ) {
        m_executionType = executionType;
    }

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
    return result;
}

// TODO use evaluateInContext() here?
bool Breakpoint::testCondition( QScriptEngine *engine )
{
    if ( m_condition.isEmpty() ) {
        return true; // No condition, always satisfied
    }

    // Use new context for condition evaluation
    QScriptContext *context = engine->pushContext();

    // Replace '%HITS' with the current number of hits
    const QString condition =
            m_condition.replace( QLatin1String("%HITS"), QString::number(m_hitCount) );

    // Evaluate condition in a try-catch-block
    m_lastConditionResult =
            engine->evaluate( QString("try{%1}catch(err){"
                                      "print('Error in breakpoint condition: ' + err);}")
                                      .arg(condition),
                              QString("Breakpoint Condition at %1").arg(m_lineNumber),
                              m_lineNumber );

    // Check result value of condition evaluation
    kDebug() << "Breakpoint condition result" << m_lastConditionResult.toString() << condition;
    bool conditionSatisfied = false;
    if ( engine->hasUncaughtException() ) {
        kDebug() << "Uncaught exception in breakpoint condition:"
                 << engine->uncaughtExceptionBacktrace();
        engine->clearExceptions();
    } else if ( !m_lastConditionResult.isBool() ) {
        kDebug() << "Breakpoint conditions should return a boolean!";
    } else {
        conditionSatisfied = m_lastConditionResult.toBool();
    }
    engine->popContext();
    return conditionSatisfied;
}

void Breakpoint::reached()
{
    if ( !m_enabled ) {
        return;
    }

    // Increase hit count
    ++m_hitCount;
    if ( m_maxHitCount > 0 && m_hitCount >= m_maxHitCount ) {
        // Maximum hit count reached, disable
        m_enabled = false;
    }
}

Breakpoint Debugger::setBreakpoint( int lineNumber, bool enable )
{
    Breakpoint breakpoint;
    if ( lineNumber < 0 ) {
        return breakpoint;
    }

    // Find a valid breakpoint line number near lineNumber (may be lineNumber itself)
    lineNumber = getNextBreakableLineNumber( lineNumber );
    const bool hasBreakpoint = m_breakpoints.contains( lineNumber );
    if ( hasBreakpoint && !enable ) {
        kDebug() << "Remove breakpoint at line" << lineNumber;
        breakpoint = m_breakpoints[ lineNumber ];
        m_breakpoints.remove( lineNumber );
        emit breakpointRemoved( breakpoint );
    } else if ( !hasBreakpoint && enable ) {
        kDebug() << "Add breakpoint at line" << lineNumber;
        breakpoint = Breakpoint( lineNumber, enable );
        m_breakpoints.insert( lineNumber, breakpoint );
        emit breakpointAdded( breakpoint );
    }

    return breakpoint;
}

bool Debugger::addBreakpoint( const Breakpoint &breakpoint )
{
    if ( !breakpoint.isValid() ) {
        kDebug() << "Breakpoint is invalid" << breakpoint.lineNumber() << breakpoint.condition();
        return false;
    }
    if ( canBreakAt(breakpoint.lineNumber()) != FoundEvaluatableLine ) {
        kDebug() << "Cannot add breakpoint at" << breakpoint.lineNumber() << breakpoint.condition();
        return false;
    }

    if ( m_breakpoints.contains(breakpoint.lineNumber()) ) {
        emit breakpointRemoved( m_breakpoints[breakpoint.lineNumber()] );
    }

    m_breakpoints.insert( breakpoint.lineNumber(), breakpoint );
    emit breakpointAdded( breakpoint );
    return true;
}

void Debugger::removeAllBreakpoints()
{
    foreach ( const Breakpoint &breakpoint, m_breakpoints ) {
        if ( m_breakpoints.contains(breakpoint.lineNumber()) ) {
            kDebug() << "Remove breakpoint at line" << breakpoint.lineNumber();
            m_breakpoints.remove( breakpoint.lineNumber() );
            emit breakpointRemoved( breakpoint );
        }
    }
}

bool Debugger::removeBreakpoint( int lineNumber )
{
    lineNumber = getNextBreakableLineNumber( lineNumber );
    if ( m_breakpoints.contains(lineNumber) ) {
        kDebug() << "Remove breakpoint at line" << lineNumber;
        const Breakpoint breakpoint = m_breakpoints[ lineNumber ];
        m_breakpoints.remove( lineNumber );
        emit breakpointRemoved( breakpoint );
        return true;
    }

    return false;
}

bool Debugger::removeBreakpoint( const Breakpoint &breakpoint )
{
    return removeBreakpoint( breakpoint.lineNumber() );
}

bool Debugger::debugControl( Debugger::ExecutionControl controlType, const QVariant &argument,
                             QString *errorMessage )
{
    switch ( controlType ) {
    case ControlExecutionContinue:
        if ( !isInterrupted() && m_executionType != ExecuteNotRunning ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not interrupted!");
            }
            return false;
        }
        debugContinue();
        break;
    case ControlExecutionInterrupt:
        if ( !m_running ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not running! Start the debugger first.");
            }
            return false;
        }
        debugInterrupt();
        break;
    case ControlExecutionAbort:
        if ( !m_running ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not running! Start the debugger first.");
            }
            return false;
        }
        abortDebugger();
        break;
    case ControlExecutionStepInto:
        if ( !isInterrupted() && m_executionType != ExecuteNotRunning ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not interrupted!");
            }
            return false;
        }
        debugStepInto( argument.isValid() ? argument.toInt() : 1 );
        break;
    case ControlExecutionStepOver:
        if ( !isInterrupted() && m_executionType != ExecuteNotRunning ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not interrupted!");
            }
            return false;
        }
        debugStepOver( argument.isValid() ? argument.toInt() : 1 );
        break;
    case ControlExecutionStepOut:
        if ( !isInterrupted() && m_executionType != ExecuteNotRunning ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Debugger is not interrupted!");
            }
            return false;
        }
        debugStepOut( argument.isValid() ? argument.toInt() : 1 );
        break;
    case ControlExecutionRunUntil: {
        bool ok;
        const int lineNumber = argument.toInt( &ok );
        if ( !argument.isValid() || !ok ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Invalid argument '%1', expected line number!",
                                      argument.toString() );
            }
            return false;
        } else if ( lineNumber < 1 || lineNumber > m_scriptLines.count() ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info", "Invalid line number %1! Must be between 1 and %2",
                                      lineNumber, m_scriptLines.count());
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

Debugger::ExecutionControl Debugger::executionControlFromString( const QString &str )
{
    const QString _str = str.trimmed().toLower();
    if ( _str == "continue" ) {
        return ControlExecutionContinue;
    } else if ( _str == "interrupt" ) {
        return ControlExecutionInterrupt;
    } else if ( _str == "abort" ) {
        return ControlExecutionAbort;
    } else if ( _str == "stepinto" ) {
        return ControlExecutionStepInto;
    } else if ( _str == "stepover" ) {
        return ControlExecutionStepOver;
    } else if ( _str == "stepout" ) {
        return ControlExecutionStepOut;
    } else if ( _str == "rununtil" ) {
        return ControlExecutionRunUntil;
    } else {
        return InvalidControlExecution;
    }
}

void Debugger::abortDebugger()
{
    engine()->abortEvaluation();
    m_executionType = ExecuteNotRunning;
}

void Debugger::debugInterrupt()
{
    m_executionType = ExecuteInterrupt;
}

void Debugger::debugContinue()
{
    engine()->clearExceptions();
    m_executionType = ExecuteRun;
}

void Debugger::debugStepInto( int count )
{
    m_repeatExecutionTypeCount = count;
    kDebug() << m_repeatExecutionTypeCount;
    m_executionType = ExecuteStepInto;
}

void Debugger::debugStepOver( int count )
{
    if ( currentFunctionStartLineNumber() == -1 ) {
        // Not currently in a function, use step into. Otherwise this would equal debugContinue()
        debugStepInto( count );
    } else {
        m_repeatExecutionTypeCount = count;
        m_interruptContext = engine()->currentContext();
        m_interruptFunctionLineNumber = currentFunctionLineNumber();
        m_executionType = ExecuteStepOver;
    }
}

void Debugger::debugStepOut( int count )
{
    m_repeatExecutionTypeCount = count;
    m_interruptContext = engine()->currentContext();
    m_interruptFunctionLineNumber = currentFunctionLineNumber();
    m_executionType = ExecuteStepOut;
}

void Debugger::debugRunUntilLineNumber( int lineNumber )
{
    addBreakpoint( Breakpoint::oneTimeBreakpoint(lineNumber) );
    m_executionType = ExecuteRun;
}

void Debugger::debugRunInjectedProgram()
{
    m_executionType = ExecuteRunInjectedProgram;
}

void Debugger::debugStepIntoInjectedProgram()
{
    m_executionType = ExecuteStepIntoInjectedProgram;
}

void Debugger::scriptLoad( qint64 id, const QString &program, const QString &fileName, int baseLineNumber )
{
    // TODO ONLY FOR THE MAIN SCRIPT FILE, eg. not for fileName == "Console Command"
    kDebug() << id /*<< program*/ << fileName << baseLineNumber;
    if ( id != -1 ) {
        m_scriptLines = program.split( '\n', QString::KeepEmptyParts );
    }
}

void Debugger::scriptUnload( qint64 id )
{
    // TODO
    QScriptEngineAgent::scriptUnload( id );
}

void Debugger::contextPush()
{
    // TODO
}

void Debugger::contextPop()
{
    // TODO
}

void Debugger::positionChange( qint64 scriptId, int lineNumber, int columnNumber )
{
    const bool injectedProgram = m_executionType == ExecuteRunInjectedProgram /*||
                                 m_executionType == ExecuteStepInfoInjectedProgram*/;
    if ( !injectedProgram ) {
        if ( !m_running ) {
            m_running = true;
            emit scriptStarted();
        }
        emit positionAboutToChanged( m_lineNumber, m_columnNumber, lineNumber, columnNumber );
    }

    kDebug() << "m_repeatExecutionTypeCount:" << m_repeatExecutionTypeCount;
    switch ( m_executionType ) {
        case ExecuteStepInto:
        case ExecuteStepIntoInjectedProgram:
            if ( m_repeatExecutionTypeCount > 0 ) { kDebug() << "-- A";
                --m_repeatExecutionTypeCount;
            } else if ( m_repeatExecutionTypeCount == 0 ) {
                m_executionType = ExecuteInterrupt;
            }
            break;
        case ExecuteStepOver:
            if ( engine()->currentContext() == m_interruptContext ) {
                kDebug() << "Interrupt after step over";
                if ( m_repeatExecutionTypeCount > 0 ) { kDebug() << "-- B";
                    --m_repeatExecutionTypeCount;
                } else if ( m_repeatExecutionTypeCount == 0 ) {
                    m_executionType = ExecuteInterrupt;
                    m_interruptContext = 0;
                    m_interruptFunctionLineNumber = -1;
                }
            } else {
                kDebug() << "Step over" << m_lineNumber;
            }
            break;
        case ExecuteStepOut: // Done below
        case ExecuteRun:
        case ExecuteNotRunning:
        case ExecuteInterrupt:
        case ExecuteRunInjectedProgram:
        default:
            break;
    }

    const QScriptContext *oldContext = m_lastContext;
    m_lastContext = engine()->currentContext();

    if ( !m_backtraceCleanedup ) {
        m_backtraceCleanedup = true;

        const int oldFunctionLineNumber = m_currentFunctionLineNumber;
        m_currentFunctionLineNumber = currentFunctionLineNumber();

        const BacktraceQueue oldBacktrace = m_lastBacktrace;
        m_lastBacktrace = buildBacktrace();
        BacktraceChange change = compareBacktraces( m_lastBacktrace, oldBacktrace );
        emit backtraceChanged( m_lastBacktrace, change );

        switch ( change ) {
        case EnteredFunction:
            kDebug() << "Entered function";
            break;
        case ExitedFunction:
            kDebug() << "Exited function";
            if ( m_executionType == ExecuteStepOut ) {
                if ( oldFunctionLineNumber == m_interruptFunctionLineNumber ) {
                    kDebug() << "Interrupt at return";
                    m_interruptContext = 0;
                    m_interruptFunctionLineNumber = -1;
                    m_executionType = ExecuteInterrupt;
                }
            }
            break;
        case NoBacktraceChange:
        default:
            break;
        }
    }

    // Handle breakpoints
    const int lastLineNumber = m_lineNumber;
    if ( !injectedProgram ) {
        m_lineNumber = lineNumber;
        m_columnNumber = columnNumber;

        // Test for a breakpoint at the new line number
        if ( m_breakpoints.contains(lineNumber) ) {
            // Found a breakpoint
            Breakpoint &breakpoint = m_breakpoints[ lineNumber ];
            if ( breakpoint.isEnabled() ) {
                // The found breakpoint is enabled
                kDebug() << "Breakpoint reached:" << lineNumber;
                breakpoint.reached(); // Increase hit count, etc.

                // Test breakpoint condition if any
                if ( breakpoint.testCondition(engine()) ) {
                    // Condition satisfied or no condition, interrupt script
                    m_executionType = ExecuteInterrupt;
                    emit breakpointReached( breakpoint );

                    // Remove breakpoints, if their maximum hit count is reached
                    if ( !breakpoint.isEnabled() ) {
                        m_breakpoints.remove( lineNumber );
                        emit breakpointRemoved( breakpoint );
                    }
                } else {
                    kDebug() << "Breakpoint at" << lineNumber << "reached but it's condition"
                            << breakpoint.condition() << "did not match";
                }
            } else {
                kDebug() << "Breakpoint at" << lineNumber << "reached but it is disabled";
            }
        }

        emit positionChanged( lineNumber, columnNumber );
    }

    if ( m_executionType == ExecuteInterrupt ) {
        emit interrupted();
        while ( m_executionType == ExecuteInterrupt ) {
            QApplication::processEvents( QEventLoop::AllEvents, 200 );
        }

        // TODO May have been deleted here already...
        emit continued();
    }
}

BacktraceQueue Debugger::buildBacktrace() const
{
    BacktraceQueue backtrace;
    QScriptContext *context = engine()->currentContext();
    while ( context ) {
        backtrace << QScriptContextInfo( context );
        context = context->parentContext();
    }
    return backtrace;
}

int Debugger::currentFunctionLineNumber() const
{
    QScriptContext *context = engine()->currentContext();
    while ( context ) {
        if ( context->thisObject().isFunction() ) {
            return QScriptContextInfo( context ).lineNumber();
        }
        context = context->parentContext();
    }
    return -1;
}

void Debugger::functionEntry( qint64 scriptId )
{
    if ( scriptId != -1 ) {
        m_backtraceCleanedup = false;
    }
}

void Debugger::functionExit( qint64 scriptId, const QScriptValue& returnValue )
{
    if ( scriptId != -1 ) {
        m_backtraceCleanedup = false;
    }
    QTimer::singleShot( 250, this, SLOT(checkExecution()) );
}

Debugger::BacktraceChange Debugger::compareBacktraces( const BacktraceQueue &backtrace,
                                                       const BacktraceQueue &oldBacktrace ) const
{
    if ( backtrace.length() > oldBacktrace.length() ) {
        return EnteredFunction;
    } else if ( backtrace.length() < oldBacktrace.length() ) {
        return ExitedFunction;
    } else {
        return NoBacktraceChange;
    }
}

void Debugger::checkExecution()
{
    if ( m_running && !engine()->isEvaluating() ) {
        const QScriptContextInfo functionInfo( engine()->currentContext() );
//         kDebug() << "Exited" << m_lineNumber << m_columnNumber;
        if ( !m_lastBacktrace.isEmpty() ) {
            const BacktraceQueue oldBacktrace = m_lastBacktrace;
            m_lastBacktrace.clear();
            BacktraceChange change = compareBacktraces( m_lastBacktrace, oldBacktrace );
            emit backtraceChanged( m_lastBacktrace, change );
        }
        m_running = false;
        emit scriptFinished();
        emit positionAboutToChanged( m_lineNumber, m_columnNumber, -1, -1 );
        m_lineNumber = -1;
        m_columnNumber = -1;
        emit positionChanged( -1, -1 );
    }
}

void Debugger::exceptionCatch( qint64 scriptId, const QScriptValue &exception )
{
    kDebug() << scriptId << exception.toString();
}

void Debugger::exceptionThrow( qint64 scriptId, const QScriptValue &exceptionValue, bool hasHandler )
{
    if ( !hasHandler ) {
        kDebug() << "Uncatched exception in" << engine()->uncaughtExceptionLineNumber()
                 << exceptionValue.toString();
        debugInterrupt();
        emit exception( engine()->uncaughtExceptionLineNumber(), exceptionValue.toString() );
        engine()->clearExceptions();
    }
}

QVariant Debugger::extension( QScriptEngineAgent::Extension extension, const QVariant &argument )
{
    kDebug() << extension << argument.toString();
    return QScriptEngineAgent::extension( extension, argument );
}

DebuggerCommand DebuggerCommand::fromString( const QString &str )
{
    if ( str.isEmpty() ) {
        return DebuggerCommand( InvalidCommand );
    }

    const QStringList words = str.split( ' ', QString::SkipEmptyParts );
    QString commandName = words.first().trimmed().toLower();
    if ( commandName.startsWith('.') ) {
        commandName = commandName.mid( 1 ); // Cut the '.'
        return DebuggerCommand( commandName, words.mid(1) );
    }

    return DebuggerCommand( InvalidCommand );
}

bool DebuggerCommand::isValid() const
{
    switch ( m_command ) {
    case DebuggerControlCommand:
        // Command accepts 1 - 3 arguments
        return m_arguments.count() >= 1 && m_arguments.count() <= 3;
    case HelpCommand:
        // Command accepts 0 - 1 argument
        return m_arguments.count() <= 1;
    case DebugCommand:
    case BreakpointCommand:
        // Command accepts 1 - * arguments
        return !m_arguments.isEmpty();
    case ClearCommand:
    case LineNumberCommand:
        // Command does not accept arguments
        return m_arguments.isEmpty();

    case InvalidCommand:
        return false;
    default:
        kDebug() << "Command unknown" << m_command;
        return false;
    }
}

QString DebuggerCommand::description() const
{
    return commandDescription( m_command );
}

QString DebuggerCommand::syntax() const
{
    return commandSyntax( m_command );
}

bool DebuggerCommand::getsExecutedAutomatically() const
{
    return getsCommandExecutedAutomatically( m_command );
}

QStringList DebuggerCommand::availableCommands()
{
    return QStringList() << "help" << "clear" << "debugger" << "debug" << "break" << "line"
                         << "currentline";
}

QStringList DebuggerCommand::defaultCompletions()
{
    return QStringList() << ".help" << ".help debug" << ".help debugger" << ".help line"
            << ".help currentline" << ".help clear" << ".help break"
            << ".debugger stepInto" << ".debugger stepOver" << ".debugger stepOut"
            << ".debugger continue" << ".debugger interrupt" << ".debugger abort"
            << ".debugger runUntil" << ".debug" << ".line" << ".currentline" << ".clear"
            << ".break";
}

bool DebuggerCommand::getsCommandExecutedAutomatically( Command command )
{
    switch ( command ) {
    case HelpCommand:
    case LineNumberCommand:
    case DebuggerControlCommand:
    case DebugCommand:
    case BreakpointCommand:
        return true;
    case ClearCommand:
        return false;
    default:
        kDebug() << "Command unknown" << command;
        return false;
    }
}

QString DebuggerCommand::commandSyntax( DebuggerCommand::Command command )
{
    switch ( command ) {
    case HelpCommand:
        return i18nc("@info", "<emphasis>.help</emphasis> or "
                     "<emphasis>.help &lt;command&gt;</emphasis>");
    case ClearCommand:
        return i18nc("@info", "<emphasis>.clear</emphasis>");
    case LineNumberCommand:
        return i18nc("@info", "<emphasis>.line</emphasis> or "
                     "<emphasis>.currentline</emphasis>");
    case DebuggerControlCommand:
        return i18nc("@info", "<emphasis>.debugger &lt;arguments&gt;</emphasis>, "
                     "arguments can be one of "
                     "<emphasis>stepInto &lt;count = 1&gt;?</emphasis>, "
                     "<emphasis>stepOver &lt;count = 1&gt;?</emphasis>, "
                     "<emphasis>stepOut &lt;count = 1&gt;?</emphasis>, "
                     "<emphasis>continue</emphasis>, "
                     "<emphasis>interrupt</emphasis>, "
                     "<emphasis>abort</emphasis>, "
                     "<emphasis>runUntilLineNumber &lt;lineNumber&gt;</emphasis>");
    case DebugCommand:
        return i18nc("@info", "<emphasis>.debug &lt;code-to-execute-in-script-context&gt;</emphasis>");
    case BreakpointCommand:
        return i18nc("@info", "<emphasis>.break &lt;lineNumber&gt; &lt;argument&gt;"
                     "</emphasis>, argument can be one of these: "
                     "<emphasis>remove</emphasis>, "
                     "<emphasis>toggle</emphasis>, "
                     "<emphasis>add</emphasis>, "
                     "<emphasis>enable</emphasis>, "
                     "<emphasis>disable</emphasis>, "
                     "<emphasis>reset</emphasis>, "
                     "<emphasis>condition &lt;conditionCode&gt;</emphasis>, "
                     "<emphasis>maxhits=&lt;number&gt;</emphasis>");
    default:
        kDebug() << "Command unknown" << command;
        return QString();
    }
}

QString DebuggerCommand::commandDescription( Command command )
{
    switch ( command ) {
    case HelpCommand:
        return i18nc("@info", "Show help, one argument can be a command name.");
    case ClearCommand:
        return i18nc("@info", "Clear the current console output.");
    case LineNumberCommand:
        return i18nc("@info", "Returns the current execution line number or -1, if not running.");
    case BreakpointCommand:
        return i18nc("@info", "Add/remove/change a breakpoint at the line given.");
    case DebuggerControlCommand:
        return i18nc("@info", "Control the debugger, expects an argument.");
    case DebugCommand:
        return i18nc("@info", "Execute a command in the script context (eg. calling a script "
                     "function) and interrupts at the first statement in the script (not the "
                     "command). When leaving the <emphasis>.debug</emphasis> away, the command "
                     "gets executed without interruption other than breakpoints or uncaught "
                     "exceptions.");
    default:
        kDebug() << "Command unknown" << command;
        return QString();
    }
}

DebuggerCommand::Command DebuggerCommand::commandFromName( const QString &name )
{
    const QString _name = name.trimmed().toLower();
    if ( _name == "help" ) {
        return HelpCommand;
    } else if ( name == "clear" ) {
        return ClearCommand;
    } else if ( name == "line" || name == "currentline" ) {
        return LineNumberCommand;
    } else if ( name == "debugger" ) {
        return DebuggerControlCommand;
    } else if ( name == "debug" ) {
        return DebugCommand;
    } else if ( name == "break" ) {
        return BreakpointCommand;
    } else {
        return InvalidCommand;
    }
}
