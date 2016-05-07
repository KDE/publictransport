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
#include "breakpointdockwidget.h"

// Own includes
#include "../debugger/debugger.h"
#include "../debugger/debuggerstructures.h"
#include "../debugger/breakpointmodel.h"
#include "../project.h"
#include "../projectmodel.h"
#include "../tabs/scripttab.h"

// KDE includes
#include <KToggleAction>
#include <KLocalizedString>
#include <KDebug>
#include <KTextEditor/View>
#include <KTextEditor/Document>

// Qt includes
#include <QTreeView>
#include <QStandardItemModel>
#include <QApplication>
#include <QPainter>
#include <QMenu>
#include <QHeaderView>

BreakpointDockWidget::BreakpointDockWidget( ProjectModel *projectModel,
                                            KActionMenu *showDocksAction, QWidget *parent )
        : AbstractDockWidget( i18nc("@title:window Dock title", "Breakpoints"),
                             showDocksAction, parent ),
          m_projectModel(projectModel), m_breakpointWidget(new QTreeView(this))
{
    setObjectName( "breakpoints" );

    setWhatsThis( i18nc("@info:whatsthis", "<title>Shows a list of all breakpoints</title>"
                        "<para>Breakpoints can be enabled/disabled here and it's condition can "
                        "be edited.</para>") );

    // Create a widget to visualize the model
    m_breakpointWidget->setAllColumnsShowFocus( true );
    m_breakpointWidget->setRootIsDecorated( false );
    CheckboxDelegate *delegate = new CheckboxDelegate( m_breakpointWidget, this );
    m_breakpointWidget->setItemDelegateForColumn( 0, delegate );
    connect( delegate, SIGNAL(checkedStateChanged(QModelIndex,bool)),
             this, SLOT(checkedStateChanged(QModelIndex,bool)) );
    m_breakpointWidget->setContextMenuPolicy( Qt::CustomContextMenu );
    connect( m_breakpointWidget, SIGNAL(customContextMenuRequested(QPoint)),
             this, SLOT(contextMenu(QPoint)) );

    m_breakpointWidget->setMinimumSize( 150, 100 );
    setWidget( m_breakpointWidget );

    connect( projectModel, SIGNAL(activeProjectAboutToChange(Project*,Project*)),
             this, SLOT(activeProjectAboutToChange(Project*,Project*)) );
    connect( m_breakpointWidget, SIGNAL(clicked(QModelIndex)),
             this, SLOT(clickedBreakpointItem(QModelIndex)) );

    activeProjectAboutToChange( projectModel->activeProject(), 0 );
}

void BreakpointDockWidget::activeProjectAboutToChange( Project *project, Project *previousProject )
{
    Q_UNUSED( previousProject );

    QAbstractItemModel *oldModel = m_breakpointWidget->model();
    if ( project ) {
        setEnabled( true );
        Debugger::BreakpointModel *breakpointModel = project->debugger()->breakpointModel();
        m_breakpointWidget->setModel( breakpointModel );

        // Initialize header
        QHeaderView *header = m_breakpointWidget->header();
        header->setDefaultSectionSize( 150 );
        header->setResizeMode( static_cast<int>(BreakpointModel::EnableColumn),
                               QHeaderView::ResizeToContents );
        header->setResizeMode( static_cast<int>(BreakpointModel::SourceColumn),
                               QHeaderView::Interactive );
        header->setResizeMode( static_cast<int>(BreakpointModel::ConditionColumn),
                               QHeaderView::Interactive );
        header->setResizeMode( static_cast<int>(BreakpointModel::HitCountColumn),
                               QHeaderView::ResizeToContents );
        header->setResizeMode( static_cast<int>(BreakpointModel::LastConditionResultColumn),
                               QHeaderView::Stretch );
    } else {
        setEnabled( false );
        m_breakpointWidget->setModel( new BreakpointModel(this) );
    }

    if ( oldModel && oldModel->QObject::parent() == this ) {
        delete oldModel;
    }
}

Debugger::BreakpointModel *BreakpointDockWidget::model() const
{
    if ( m_projectModel->activeProject() ) {
        return m_projectModel->activeProject()->debugger()->breakpointModel();
    } else {
        qWarning() << "No breakpoint model available";
        return 0;
    }
}

void BreakpointDockWidget::contextMenu( const QPoint &pos )
{
    const QModelIndex index = m_breakpointWidget->indexAt( pos );
    if ( !index.isValid() ) {
        return;
    }

    Debugger::BreakpointModel *breakpointModel = model();
    if ( !breakpointModel ) {
        return;
    }

    Breakpoint *breakpoint = breakpointModel->breakpointFromIndex( index );

    QPointer<QMenu> menu( new QMenu(this) );
    KToggleAction *enableAction = new KToggleAction( i18nc("@info/plain", "Enable"), menu );
    QAction *resetAction = new QAction( KIcon("edit-undo"), i18nc("@info/plain", "Reset"), menu );
    QAction *deleteAction = new QAction( KIcon("edit-delete"), i18nc("@info/plain", "Delete"), menu );
    enableAction->setChecked( breakpoint->isEnabled() );
    menu->addActions( QList<QAction*>() << enableAction << resetAction << deleteAction );

    QAction *action = menu->exec( m_breakpointWidget->mapToGlobal(pos) );
    if ( action == deleteAction ) {
        breakpointModel->removeBreakpoint( breakpoint );
    } else if ( action == enableAction ) {
        breakpoint->setEnabled( enableAction->isChecked() );
    } else if ( action == resetAction ) {
        breakpoint->reset();
    }
    delete menu.data();
}

void BreakpointDockWidget::checkedStateChanged( const QModelIndex &index, bool checked )
{
    Debugger::BreakpointModel *breakpointModel = model();
    if ( !index.isValid() || !breakpointModel ) {
        return;
    }

    Debugger::Breakpoint *breakpoint = breakpointModel->breakpointFromIndex( index );
    if ( breakpoint ) {
        breakpoint->setEnabled( checked );
    }
}

void BreakpointDockWidget::clickedBreakpointItem( const QModelIndex &index )
{
    Debugger::BreakpointModel *breakpointModel = model();
    if ( !index.isValid() || !breakpointModel ) {
        return;
    }

    const Debugger::Breakpoint *breakpoint = breakpointModel->breakpointFromIndex( index );

    // Set cursor to breakpoint position
    Project *project = m_projectModel->activeProject();
    if ( !project ) {
        kDebug() << "TODO";
        return;
    }
    ScriptTab *tab = project->scriptTab();
    if ( tab ) {
        tab->goToLine( breakpoint->lineNumber() );
    }
}

QWidget *BreakpointDockWidget::mainWidget() const {
    return m_breakpointWidget;
}
