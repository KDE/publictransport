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

#ifndef CONSOLEDOCKWIDGET_H
#define CONSOLEDOCKWIDGET_H

#include "abstractdockwidget.h"

class Project;
class ProjectModel;
namespace Debugger {
    class Debugger;
    struct EvaluationResult;
}
class KLineEdit;
class QToolButton;
class QPlainTextEdit;

using namespace Debugger;

/**
 * @brief A dock widget that shows a console for a TimetableMate project.
 *
 * Can execute ConsoleCommand's asynchronously using ExecuteConsoleCommandJob or evaluate script
 * code using EvaluateInContextJob if no console command was found. Running commands can be aborted
 * using a tool button. Multiline command can be entered by appending a '\' to the end of a line
 * if more lines follow.
 **/
class ConsoleDockWidget : public AbstractDockWidget {
    Q_OBJECT
public:
    /** @brief Different states of the console. */
    enum State {
        WaitingForInput = 0, /**< The console is waiting for user input, ie. new commands. */
        EvaluatingResult /**< The console is evaluating a command. */
    };

    explicit ConsoleDockWidget( ProjectModel *projectModel, KActionMenu *showDocksAction,
                                QWidget *parent = 0 );

    virtual KIcon icon() const { return KIcon("utilities-terminal"); };
    virtual Qt::DockWidgetArea defaultDockArea() const { return Qt::BottomDockWidgetArea; };
    QPlainTextEdit *consoleWidget() const { return m_consoleWidget; };
    KLineEdit *commandLineEdit() const { return m_commandLineEdit; };
    QToolButton *cancelButton() const { return m_cancelButton; };
    ProjectModel *projectModel() const { return m_projectModel; };
    State state() const { return m_state; };

protected slots:
    void commandEntered( const QString &commandString );
    void evaluationResult( const EvaluationResult &evaluationResult );
    void cancelEvaluation();
    void appendToConsole( const QString &text );
    void commandExecutionResult( const QString &text );
    void consoleTextChanged( const QString &consoleText );

    void activeProjectAboutToChange( Project *project, Project *previousProject );

protected:
    virtual bool eventFilter( QObject *source, QEvent *event );

private:
    void setState( State state );
    QString encodeInput( const QString &input ) const;

    QPlainTextEdit *m_consoleWidget;
    KLineEdit *m_commandLineEdit;
    QToolButton *m_cancelButton;
    QStringList m_consoleHistory;
    int m_consoleHistoryIndex;
    ProjectModel *m_projectModel;
    QStringList m_enteredMultilineCommandLines;
    State m_state;
};

#endif // Multiple inclusion guard
