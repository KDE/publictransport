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
#include "consoledockwidget.h"

// Own includes
#include "../debugger/debuggerstructures.h"
#include "../debugger/debugger.h"
#include "../projectmodel.h"
#include "../project.h"

// Public Transport engine includes
#include <engine/timetableaccessor.h>
#include <engine/global.h>

// KDE includes
#include <KLocalizedString>
#include <KGlobalSettings>
#include <KLineEdit>
#include <KDebug>

// Qt includes
#include <QPlainTextEdit>
#include <QBoxLayout>
#include <QToolButton>

ConsoleDockWidget::ConsoleDockWidget( ProjectModel *projectModel, KActionMenu *showDocksAction,
                                      QWidget *parent )
        : AbstractDockWidget( i18nc("@window:title Dock title", "Console"), showDocksAction, parent ),
          m_consoleWidget(0), m_commandLineEdit(0), m_cancelButton(0), m_consoleHistoryIndex(-1),
          m_projectModel(projectModel), m_state(WaitingForInput)
{
    setObjectName( "console" );

    setWhatsThis( i18nc("@info:whatsthis", "<title>A debugger console</title>"
                        "<para>Provides some commands (type <icode>.help</icode>) and can "
                        "execute script code in the current script context. If values are altered "
                        "by a console command they are also altered in a running script.</para>") );

    QWidget *container = new QWidget( this );
    container->setMinimumSize( 150, 100 );
    m_consoleWidget = new QPlainTextEdit( container );
    m_consoleWidget->setReadOnly( true );
    m_consoleWidget->setFont( KGlobalSettings::fixedFont() );
    m_commandLineEdit = new KLineEdit( container );
    m_commandLineEdit->setFont( KGlobalSettings::fixedFont() );
    m_commandLineEdit->setClickMessage( i18nc("@info/plain", "Enter a command, eg. '.help'") );
    m_cancelButton = new QToolButton( container );
    m_cancelButton->setToolButtonStyle( Qt::ToolButtonIconOnly );
    m_cancelButton->setToolTip( i18nc("@info:tooltip", "Cancel evaluation and abort debugger") );
    m_cancelButton->setIcon( KIcon("process-stop") );
    m_cancelButton->hide();

    QHBoxLayout *inputBarLayout = new QHBoxLayout();
    inputBarLayout->setContentsMargins( 0, 0, 0, 0 );
    inputBarLayout->addWidget( m_commandLineEdit );
    inputBarLayout->addWidget( m_cancelButton );

    QVBoxLayout *consoleLayout = new QVBoxLayout( container );
    consoleLayout->setSpacing( 0 );
    consoleLayout->setContentsMargins( 0, 0, 0, 0 );
    consoleLayout->addWidget( m_consoleWidget );
    consoleLayout->addLayout( inputBarLayout );
    setWidget( container );
    connect( m_commandLineEdit, SIGNAL(returnPressed(QString)),
             this, SLOT(commandEntered(QString)) );
    connect( m_cancelButton, SIGNAL(clicked(bool)), this, SLOT(cancelEvaluation()) );

    m_consoleHistoryIndex = -1;
    m_commandLineEdit->installEventFilter( this );
    KCompletion *completion = m_commandLineEdit->completionObject();
    completion->setItems( Debugger::ConsoleCommand::defaultCompletions() );

    connect( projectModel, SIGNAL(activeProjectAboutToChange(Project*,Project*)),
             this, SLOT(activeProjectAboutToChange(Project*,Project*)) );

    setState( WaitingForInput );
}

void ConsoleDockWidget::activeProjectAboutToChange( Project *project, Project *previousProject )
{
    if ( previousProject ) {
        disconnect( previousProject->debugger(), SIGNAL(evaluationResult(EvaluationResult)),
                    this, SLOT(evaluationResult(EvaluationResult)) );
        disconnect( previousProject->debugger(), SIGNAL(commandExecutionResult(QString)),
                    this, SLOT(commandExecutionResult(QString)) );
    }

    if ( project ) {
        connect( project->debugger(), SIGNAL(evaluationResult(EvaluationResult)),
                 this, SLOT(evaluationResult(EvaluationResult)) );
        connect( project->debugger(), SIGNAL(commandExecutionResult(QString)),
                 this, SLOT(commandExecutionResult(QString)) );
    }
}

void ConsoleDockWidget::setState( ConsoleDockWidget::State state )
{
    switch ( state ) {
    case WaitingForInput:
        m_cancelButton->hide();
        m_commandLineEdit->clear();
        m_commandLineEdit->setEnabled( true );
        m_commandLineEdit->setFocus();
        break;
    case EvaluatingResult:
        m_cancelButton->show();
        m_commandLineEdit->setEnabled( false );
        break;
    }
    m_state = state;
}

QString ConsoleDockWidget::encodeInput( const QString &input ) const
{
    return QString(input).replace( QLatin1String("<"), QLatin1String("&lt;") )
                         .replace( QLatin1String(">"), QLatin1String("&gt;") );
}

void ConsoleDockWidget::commandEntered( const QString &_commandString )
{
    if ( _commandString.isEmpty() ) {
        kDebug() << "No command given";
        return;
    }

    Project *project = m_projectModel->activeProject();
    if ( !project ) {
        kDebug() << "No active project";
        return;
    }

    setState( EvaluatingResult );

    // Check if the command string ends with a '\'. If it does, add the command to the current
    // multiline command and wait for more input. If not, check if there are entered multiline
    // command lines.
    QString commandString = _commandString;
    if ( commandString.endsWith('\\') ) {
        const QString correctedCommandString = commandString.left( commandString.length() - 1 );
        if ( m_enteredMultilineCommandLines.isEmpty() ) {
            // Write first line of a multiline command to console
            appendToConsole( QString("<b> &gt; %1</b>").arg(encodeInput(correctedCommandString)) );
        } else {
            // Write following line of a multiline command to console
            appendToConsole( QString("<b>   %1</b>").arg(encodeInput(correctedCommandString)) );
        }
        m_enteredMultilineCommandLines << correctedCommandString;
        setState( WaitingForInput );
        return;
    } else if ( !m_enteredMultilineCommandLines.isEmpty() ) {
        // Write last line of a multiline command to console
        appendToConsole( QString("<b>   %1</b>").arg(encodeInput(commandString)) );

        // Prepend already entered lines
        commandString.prepend( m_enteredMultilineCommandLines.join("\n") + '\n' );
        m_enteredMultilineCommandLines.clear();
    } else {
        // Write executed command to console
        appendToConsole( QString("<b> &gt; %1</b>").arg(encodeInput(commandString)) );
    }

    // Store executed command in history (use up/down keys)
    if ( m_consoleHistory.isEmpty() || m_consoleHistory.first() != commandString ) {
        m_consoleHistory.prepend( commandString );
    }
    m_consoleHistoryIndex = -1;

    // Add executed command to completion object
    KCompletion *completion = m_commandLineEdit->completionObject();
    if ( !completion->items().contains(commandString) ) {
        completion->addItem( commandString );
    }

    // Check if commandString contains a command of the form ".<command> ..."
    project->debugger()->loadScript( project->scriptText(), project->accessor()->info() );
    Debugger::ConsoleCommand command = Debugger::ConsoleCommand::fromString( commandString );
    if ( command.isValid() )  {
        if ( command.command() == ConsoleCommand::ClearCommand ) {
            // The clear command cannot be executed in the debugger,
            // simply clear the console history here
            m_consoleWidget->clear();
            setState( WaitingForInput );
        } else {
            // Execute the command
            project->debugger()->executeCommand( command );
        }
    } else {
        // No command given, execute string as script code
        project->debugger()->evaluateInContext( commandString,
                i18nc("@info/plain", "Console Command (%1)", commandString) );
    }
}

void ConsoleDockWidget::evaluationResult( const EvaluationResult &result )
{
    if ( result.error ) {
        if ( result.backtrace.isEmpty() ) {
            appendToConsole( i18nc("@info", "Error: <message>%1</message>", result.errorMessage) );
        } else {
            appendToConsole( i18nc("@info", "Error: <message>%1</message><nl />"
                                   "Backtrace: <message>%2</message>",
                                   result.errorMessage, result.backtrace.join("<br />")) );
        }
    } else {
        m_consoleWidget->appendHtml( result.returnValue.toString() );
    }

    setState( WaitingForInput );
}

void ConsoleDockWidget::commandExecutionResult( const QString &text )
{
    appendToConsole( text );
    setState( WaitingForInput );
}

void ConsoleDockWidget::cancelEvaluation()
{
    if ( m_projectModel->activeProject() ) {
        m_projectModel->activeProject()->debugger()->abortDebugger();
        appendToConsole( i18nc("@info", "(Debugger aborted)") );
    }
}

void ConsoleDockWidget::appendToConsole( const QString &text )
{
    if ( !text.isEmpty() ) {
        m_consoleWidget->appendHtml( text );
    }
}

bool ConsoleDockWidget::eventFilter( QObject *source, QEvent *event )
{
    if ( source == m_commandLineEdit && event->type() == QEvent::KeyPress ) {
        QKeyEvent *keyEvent = dynamic_cast<QKeyEvent*>( event );
        if ( keyEvent ) {
            switch ( keyEvent->key() ) {
            case Qt::Key_Up:
                if ( m_consoleHistoryIndex + 1 < m_consoleHistory.count() ) {
                    ++m_consoleHistoryIndex;
                    m_commandLineEdit->setText( m_consoleHistory[m_consoleHistoryIndex] );
                    return true;
                }
                break;
            case Qt::Key_Down:
                if ( m_consoleHistoryIndex >= 0 ) {
                    --m_consoleHistoryIndex;
                    if ( m_consoleHistoryIndex == -1 ) {
                        m_commandLineEdit->clear();
                    } else {
                        m_commandLineEdit->setText( m_consoleHistory[m_consoleHistoryIndex] );
                    }
                    return true;
                }
                break;
            }
        }
    }

    return QObject::eventFilter( source, event );
}
