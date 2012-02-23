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
    if ( !breakpoint.isValid() || !canBreakAt(breakpoint.lineNumber()) ) {
        kDebug() << "Breakpoint is invalid" << breakpoint.lineNumber() << breakpoint.condition();
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

void Debugger::removeBreakpoint( int lineNumber )
{
    lineNumber = getNextBreakableLineNumber( lineNumber );
    if ( m_breakpoints.contains(lineNumber) ) {
        kDebug() << "Remove breakpoint at line" << lineNumber;
        const Breakpoint breakpoint = m_breakpoints[ lineNumber ];
        m_breakpoints.remove( lineNumber );
        emit breakpointRemoved( breakpoint );
    }
}

void Debugger::removeBreakpoint( const Breakpoint &breakpoint )
{
    removeBreakpoint( breakpoint.lineNumber() );
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

void Debugger::debugStepInto()
{
    m_executionType = ExecuteStepInto;
}

void Debugger::debugStepOver()
{
    if ( currentFunctionStartLineNumber() == -1 ) {
        // Not currently in a function, use step into. Otherwise this would equal debugContinue()
        debugStepInto();
    } else {
        m_interruptContext = engine()->currentContext();
        m_interruptFunctionLineNumber = currentFunctionLineNumber();
        m_executionType = ExecuteStepOver;
    }
}

void Debugger::debugStepOut()
{
    m_interruptContext = engine()->currentContext();
    m_interruptFunctionLineNumber = currentFunctionLineNumber();
    m_executionType = ExecuteStepOut;
}

void Debugger::debugRunUntilLineNumber( int lineNumber )
{
    addBreakpoint( Breakpoint::oneTimeBreakpoint(lineNumber) );
    m_executionType = ExecuteRun;
}

void Debugger::scriptLoad( qint64 id, const QString &program, const QString &fileName, int baseLineNumber )
{
    // TODO
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
    if ( !m_running ) {
        m_running = true;
        emit scriptStarted();
    }
    emit positionAboutToChanged( m_lineNumber, m_columnNumber, lineNumber, columnNumber );

    switch ( m_executionType ) {
        case ExecuteStepInto:
            m_executionType = ExecuteInterrupt;
            break;
        case ExecuteStepOver:
            if ( engine()->currentContext() == m_interruptContext ) {
                kDebug() << "Interrupt after step over";
                m_interruptContext = 0;
                m_interruptFunctionLineNumber = -1;
                m_executionType = ExecuteInterrupt;
            } else {
                kDebug() << "Step over" << m_lineNumber;
            }
            break;
        case ExecuteStepOut:
            // Done below
        case ExecuteRun:
        case ExecuteNotRunning:
        case ExecuteInterrupt:
        default:
            break;
    }

    const int lastLineNumber = m_lineNumber;
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

    if ( m_executionType == ExecuteInterrupt ) {
        emit interrupted();

        while ( m_executionType == ExecuteInterrupt ) {
            QApplication::processEvents( QEventLoop::AllEvents, 50 );
        }

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
