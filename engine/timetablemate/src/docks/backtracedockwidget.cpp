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
#include "backtracedockwidget.h"

// Own includes
#include "../project.h"
#include "../projectmodel.h"
#include "../debugger/debugger.h"
#include "../debugger/backtracemodel.h"
#include "../tabs/scripttab.h"

// KDE includes
#include <KLocalizedString>
#include <KTextEditor/View>
#include <KTextEditor/Document>

// Qt includes
#include <QTreeView>

BacktraceDockWidget::BacktraceDockWidget( ProjectModel *projectModel, KActionMenu *showDocksAction,
                                          QWidget *parent )
        : AbstractDockWidget( i18nc("@window:title Dock title", "Backtrace"),
                             showDocksAction, parent ),
          m_projectModel(projectModel), m_backtraceWidget(new QTreeView(this))
{
    setObjectName( "backtrace" );
    disable(); // Gets only enabled while the debugger is running

    setWhatsThis( i18nc("@info:whatsthis", "<title>Shows a backtrace</title>"
                        "<para>When the debugger is interrupted a backtrace gets shown here. "
                        "For each script function that gets entered another frame gets added "
                        "to the backtrace. You can click on backtrace frames to show variables "
                        "of that frame in the variables dock.</para>") );

    m_backtraceWidget->setAllColumnsShowFocus( true );
    m_backtraceWidget->setRootIsDecorated( false );
    m_backtraceWidget->setEditTriggers( QAbstractItemView::NoEditTriggers );
    m_backtraceWidget->setMinimumSize( 150, 100 );
    setWidget( m_backtraceWidget );
    connect( m_backtraceWidget, SIGNAL(clicked(QModelIndex)),
             this, SLOT(clickedBacktraceItem(QModelIndex)) );

    if ( projectModel->activeProject() ) {
        m_backtraceWidget->setModel( projectModel->activeProject()->debugger()->backtraceModel() );
        connect( projectModel->activeProject()->debugger(), SIGNAL(continued(bool)),
                 this, SLOT(debuggerContinued(bool)) );
        connect( projectModel->activeProject()->debugger(), SIGNAL(stateChanged(DebuggerState,DebuggerState)),
                 this, SLOT(debuggerStateChanged(DebuggerState,DebuggerState)) );
    }
    connect( projectModel, SIGNAL(activeProjectAboutToChange(Project*,Project*)),
             this, SLOT(activeProjectAboutToChange(Project*,Project*)) );
}

void BacktraceDockWidget::activeProjectAboutToChange( Project *project, Project *previousProject )
{
    if ( previousProject ) {
        disconnect( previousProject->debugger(), SIGNAL(continued(QDateTime,bool)),
                    this, SLOT(debuggerContinued(QDateTime,bool)) );
        disconnect( previousProject->debugger(), SIGNAL(stateChanged(DebuggerState,DebuggerState)),
                    this, SLOT(debuggerStateChanged(DebuggerState,DebuggerState)) );
    }

    if ( project ) {
        m_backtraceWidget->setModel( project->debugger()->backtraceModel() );
        connect( project->debugger(), SIGNAL(continued(QDateTime,bool)),
                 this, SLOT(debuggerContinued(QDateTime,bool)) );
        connect( project->debugger(), SIGNAL(stateChanged(DebuggerState,DebuggerState)),
                 this, SLOT(debuggerStateChanged(DebuggerState,DebuggerState)) );
    }
}

void BacktraceDockWidget::enable()
{
    if ( isEnabled() ) {
        return;
    }

    Project *project = m_projectModel->activeProject();
    if ( project ) {
        setEnabled( true );
        setToolTip( QString() );
        m_backtraceWidget->setModel( project->debugger()->backtraceModel() );
    }
}

void BacktraceDockWidget::disable()
{
    if ( !isEnabled() ) {
        return;
    }

    setEnabled( false );
    setToolTip( i18nc("@info:tooltip", "Shows a backtrace when the debugger is interrupted") );
    if ( m_backtraceWidget ) {
        m_backtraceWidget->setModel( 0 );
    }
}

void BacktraceDockWidget::debuggerStateChanged( DebuggerState newState, DebuggerState oldState )
{
    switch ( newState ) {
    case Debugger::NotRunning:
    case Debugger::Aborting:
        disable();
        break;
    case Debugger::Running:
        // State changes from Interrupted to Running are handles in debuggerContinued
        if ( oldState != Debugger::Interrupted ) {
            disable();
        }
        break;
    case Debugger::Interrupted:
        enable();
        break;
    }
}

void BacktraceDockWidget::debuggerContinued( const QDateTime &timestamp,
                                             bool willInterruptAfterNextStatement )
{
    Q_UNUSED( timestamp );
    if ( !willInterruptAfterNextStatement ) {
        disable();
    }
}

void BacktraceDockWidget::clickedBacktraceItem( const QModelIndex &index )
{
    if ( !index.isValid() ) {
        return;
    }

    Project *project = m_projectModel->activeProject();
    if ( !project ) {
        kDebug() << "TODO";
        return;
    }

    Frame *frame = project->debugger()->backtraceModel()->frameFromIndex( index );
    ScriptTab *tab;
    if ( frame->fileName() == project->scriptFileName() || frame->fileName().isEmpty() ) {
        tab = project->showScriptTab( parentWidget() );
    } else {
        tab = project->showExternalScriptTab( frame->fileName(), parentWidget() );
    }

    // Set cursor position to the current execution position in frame
    if ( tab ) {
        tab->goToLine( frame->lineNumber() );
    }

    const int depth = index.row();
    emit activeFrameDepthChanged( depth );
}
