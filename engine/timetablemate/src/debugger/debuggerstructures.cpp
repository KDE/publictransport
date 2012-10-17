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
#include "debuggerstructures.h"

// Own includes
#include "backtracemodel.h"
#include "breakpointmodel.h"
#include "debuggeragent.h"

// KDE includes
#include <KDebug>
#include <KLocalizedString>

// Qt includes
#include <QScriptContext>
#include <QScriptEngine>
#include <QMutex>

namespace Debugger {

void Frame::setValuesOf( const Frame &frame )
{
    m_contextInfo = frame.contextInfo();
    m_contextString = frame.contextString();
    if ( m_model ) {
        m_model->frameChanged( this );
    }
}

void Frame::setContextInfo( const QScriptContextInfo &info, bool global )
{
    QString contextString = info.functionName();
    if ( contextString.isEmpty() ) {
        contextString = global ? "<global>" : "<anonymous>";
    }

    m_contextInfo = info;
    m_contextString = contextString;
    if ( m_model ) {
        m_model->frameChanged( this );
    }
}

QModelIndex Frame::index()
{
    return m_model->indexFromFrame( this );
}

void Breakpoint::setCondition( const QString &condition )
{
    // Clear last condition result
    m_lastConditionResult = QScriptValue();

    // Set new condition and inform the model of this breakpoint
    m_condition = condition;
    if ( m_model ) {
        m_model->slotBreakpointChanged( this );
    }
}

bool Breakpoint::testCondition( DebuggerAgent *agent, bool *error )
{
    if ( m_condition.isEmpty() ) {
        return true; // No condition, always satisfied
    }

    // Replace '%HITS' with the current number of hits
    const QString condition =
            m_condition.replace( QLatin1String("%HITS"), QString::number(m_hitCount) );

    bool uncaughtException;
    int errorLineNumber;
    QString errorMessage;
    QStringList backtrace;
    agent->engineMutex()->lockInline();
    m_lastConditionResult = agent->evaluateInContext(
            condition, QString("Breakpoint Condition at %1").arg(m_lineNumber),
            &uncaughtException, &errorLineNumber, &errorMessage, &backtrace );
    agent->engineMutex()->unlockInline();

    // Check result value of condition evaluation
    kDebug() << "Breakpoint condition result" << m_lastConditionResult.toString() << condition;
    if ( uncaughtException ) {
        kDebug() << "Uncaught exception in breakpoint condition:" << errorMessage << backtrace;
        agent->errorMessage( i18nc("@info", "Uncaught exception in breakpoint condition at line %1: "
                                   "<message>%2</message><nl />",
                                   agent->lineNumber(), errorMessage) );
    } else if ( !m_lastConditionResult.isBool() ) {
        kDebug() << "Breakpoint conditions should return a boolean";
        agent->errorMessage( i18nc("@info", "The condition code of breakpoint at line %1 did "
                                   "not return a boolean, return value was: <icode>%2</icode>",
                                   agent->lineNumber(), m_lastConditionResult.toString()) );
    } else {
        return m_lastConditionResult.toBool();
    }

    // There was an error (uncaught exception or wrong return type)
    if ( error ) {
        *error = true;
    }
    return false;
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

    if ( m_model ) {
        m_model->slotBreakpointChanged( this );
    } else {
        kDebug() << "No model given";
    }
}

ConsoleCommand ConsoleCommand::fromString( const QString &str )
{
    if ( str.isEmpty() ) {
        return ConsoleCommand( InvalidCommand );
    }

    const QStringList words = str.split( ' ', QString::SkipEmptyParts );
    QString commandName = words.first().trimmed().toLower();
    if ( commandName.startsWith('.') ) {
        commandName = commandName.mid( 1 ); // Cut the '.'
        return ConsoleCommand( commandName, words.mid(1) );
    }

    return ConsoleCommand( InvalidCommand );
}

QString ConsoleCommand::toString() const
{
    const QString commandName = commandToName( m_command );
    QString string;
    if ( !commandName.isEmpty() ) {
        string = '.' + commandName;
    }
    foreach ( const QString &argument, m_arguments ) {
        if ( !string.isEmpty() ) {
            string += ' ';
        }
        string += argument;
    }
    return string;
}

bool ConsoleCommand::isValid() const
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

QString ConsoleCommand::description() const
{
    return commandDescription( m_command );
}

QString ConsoleCommand::syntax() const
{
    return commandSyntax( m_command );
}

bool ConsoleCommand::getsExecutedAutomatically() const
{
    return getsCommandExecutedAutomatically( m_command );
}

QStringList ConsoleCommand::availableCommands()
{
    return QStringList() << ".help" << ".clear" << ".debugger" << ".debug" << ".break" << ".line"
                         << ".currentline";
}

QStringList ConsoleCommand::defaultCompletions()
{
    return QStringList() << ".help" << ".help debug" << ".help debugger" << ".help line"
            << ".help currentline" << ".help clear" << ".help break" << ".debugger status"
            << ".debugger stepInto" << ".debugger stepOver" << ".debugger stepOut"
            << ".debugger continue" << ".debugger interrupt" << ".debugger abort"
            << ".debugger runUntil" << ".debug" << ".line" << ".currentline" << ".clear"
            << ".break";
}

bool ConsoleCommand::getsCommandExecutedAutomatically( Command command )
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

QString ConsoleCommand::commandSyntax( ConsoleCommand::Command command )
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
                     "<emphasis>status</emphasis>, "
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

QString ConsoleCommand::commandDescription( Command command )
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

ConsoleCommand::Command ConsoleCommand::commandFromName( const QString &name )
{
    const QString _name = name.trimmed().toLower();
    if ( _name == QLatin1String("help") ) {
        return HelpCommand;
    } else if ( name == QLatin1String("clear") ) {
        return ClearCommand;
    } else if ( name == QLatin1String("line") || name == QLatin1String("currentline") ) {
        return LineNumberCommand;
    } else if ( name == QLatin1String("debugger") ) {
        return DebuggerControlCommand;
    } else if ( name == QLatin1String("debug") ) {
        return DebugCommand;
    } else if ( name == QLatin1String("break") ) {
        return BreakpointCommand;
    } else {
        return InvalidCommand;
    }
}

QString ConsoleCommand::commandToName( ConsoleCommand::Command command )
{
    switch ( command ) {
    case HelpCommand:
        return "help";
    case ClearCommand:
        return "clear";
    case LineNumberCommand:
        return "line";
    case DebuggerControlCommand:
        return "debugger";
    case DebugCommand:
        return "debug";
    case BreakpointCommand:
        return "break";
    case InvalidCommand:
    default:
        return QString();
    }
}

void Breakpoint::reset()
{
    m_hitCount = 0;
    m_lastConditionResult = QScriptValue();
    if ( m_model ) {
        m_model->slotBreakpointChanged( this );
    }
}

void Breakpoint::setEnabled( bool enabled )
{
    m_enabled = enabled;
    if ( m_model ) {
        m_model->slotBreakpointChanged( this );
    }
}

void Breakpoint::setMaximumHitCount( int maximumHitCount )
{
    m_maxHitCount = maximumHitCount;
    if ( m_model ) {
        m_model->slotBreakpointChanged( this );
    }
}

void Breakpoint::setValuesOf( const Breakpoint &breakpoint )
{
    m_fileName = breakpoint.m_fileName;
    m_lineNumber = breakpoint.m_lineNumber;
    m_enabled = breakpoint.m_enabled;
    m_hitCount = breakpoint.m_hitCount;
    m_maxHitCount = breakpoint.m_maxHitCount;
    m_condition = breakpoint.m_condition;
    m_lastConditionResult = breakpoint.m_lastConditionResult;
    if ( m_model ) {
        m_model->slotBreakpointChanged( this );
    }
}

}; // namespace Debugger
