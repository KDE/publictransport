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

// KDE includes
#include <KDebug>
#include <KLocalizedString>

// Qt includes
#include <QScriptContext>
#include <QScriptEngine>

namespace Debugger {

void Breakpoint::setCondition( const QString &condition )
{
    m_condition = condition;
}

// TODO thread-safety? currently engine is expected to be locked
// TODO use evaluateInContext() here?
bool Breakpoint::testCondition( QScriptEngine *engine )
{
    if ( m_condition.isEmpty() ) {
        return true; // No condition, always satisfied
    }

    // Use new context for condition evaluation
//     QMutexLocker locker( m_engineMutex ); // TODO
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
    return QStringList() << "help" << "clear" << "debugger" << "debug" << "break" << "line"
                         << "currentline";
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

}; // namespace Debugger
