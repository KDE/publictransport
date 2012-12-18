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
#include "variablesdockwidget.h"

// Own includes
#include "../debugger/debugger.h"
#include "../debugger/debuggerstructures.h"
#include "../debugger/backtracemodel.h"
#include "../debugger/variablemodel.h"
#include "../project.h"
#include "../projectmodel.h"

// KDE includes
#include <KLineEdit>
#include <KDebug>
#include <KLocalizedString>
#include <KTemporaryFile>
#include <KMessageBox>
#include <KProcess>

// Qt includes
#include <QTreeView>
#include <QSortFilterProxyModel>
#include <QFormLayout>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QMimeData>

VariablesDockWidget::VariablesDockWidget( ProjectModel *projectModel, KActionMenu *showDocksAction,
                                          QWidget *parent )
        : AbstractDockWidget( i18nc("@window:title Dock title", "Variables"),
                             showDocksAction, parent ),
          m_projectModel(projectModel), m_variableModel(0), m_variablesWidget(new QTreeView(this)),
          m_searchLine(new KLineEdit(this)), m_proxyModel(new VariableFilterProxyModel(this))
{
    setObjectName( "variables" );
    disable(); // Gets only enabled while the debugger is running

    setWhatsThis( i18nc("@info:whatsthis", "<title>Shows variables in the current script context</title>"
                        "<para>When the debugger is interrupted a list of variables gets shown here. "
                        "The variables are taken from the script context, where the interrupt "
                        "happened. You can copy values using the context menu.</para>") );

    m_variablesWidget->setAnimated( true );
    m_variablesWidget->setAllColumnsShowFocus( true );
    m_variablesWidget->setSelectionBehavior( QAbstractItemView::SelectRows );
    m_variablesWidget->setSelectionMode( QAbstractItemView::SingleSelection );
    QSizePolicy sizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    sizePolicy.setVerticalStretch( 1 );
    m_variablesWidget->setSizePolicy( sizePolicy );
    m_variablesWidget->setEditTriggers( QAbstractItemView::DoubleClicked |
                                        QAbstractItemView::EditKeyPressed );
    m_variablesWidget->setContextMenuPolicy( Qt::CustomContextMenu );
    connect( m_variablesWidget, SIGNAL(customContextMenuRequested(QPoint)),
             this, SLOT(contextMenu(QPoint)) );

    m_searchLine->setClickMessage(
            i18nc("@info/plain A KLineEdit click message to filter variables", "Type to search") );
    m_searchLine->setClearButtonShown( true );

    m_proxyModel->setFilterCaseSensitivity( Qt::CaseInsensitive );

    connect( projectModel, SIGNAL(activeProjectAboutToChange(Project*,Project*)),
             this, SLOT(activeProjectAboutToChange(Project*,Project*)) );
    if ( projectModel->activeProject() ) {
        m_variableModel = projectModel->activeProject()->debugger()->variableModel();
        m_proxyModel->setSourceModel( m_variableModel );
        m_variablesWidget->setModel( m_proxyModel );
        connect( projectModel->activeProject()->debugger(), SIGNAL(continued(bool)),
                 this, SLOT(debuggerContinued(bool)) );
        connect( projectModel->activeProject()->debugger(), SIGNAL(stateChanged(DebuggerState,DebuggerState)),
                 this, SLOT(debuggerStateChanged(DebuggerState,DebuggerState)) );
    }

    connect( m_searchLine, SIGNAL(textChanged(QString)),
             m_proxyModel, SLOT(setFilterFixedString(QString)) );

    QWidget *widget = new QWidget( this );
    widget->setMinimumSize( 150, 150 );
    QFormLayout *variablesLayout = new QFormLayout( widget );
    variablesLayout->setContentsMargins( 0, 0, 0, 0 );
    variablesLayout->setVerticalSpacing( 0 );
    variablesLayout->setRowWrapPolicy( QFormLayout::WrapLongRows );
    variablesLayout->addRow( m_searchLine );
    variablesLayout->addRow( m_variablesWidget );
    setWidget( widget );
}

void VariablesDockWidget::contextMenu( const QPoint &pos )
{
    const QModelIndex index = m_proxyModel->mapToSource( m_variablesWidget->indexAt(pos) );
    if ( !index.isValid() ) {
        return;
    }
    const bool isBinaryData =
            m_variableModel->data( index, VariableModel::ContainsBinaryDataRole ).toBool();
    const QVariant data = m_variableModel->data( index, VariableModel::CompleteValueRole );

    QPointer<QMenu> menu( new QMenu(this) );
    QAction *copyAction = new QAction( KIcon("edit-copy"), i18nc("@info/plain", "Copy"), menu );
    QAction *showInOktetaAction = new QAction( KIcon("okteta"),
                                               i18nc("@info/plain", "Show in Okteta"), menu );

    menu->addAction( copyAction );
    if ( isBinaryData ) {
        menu->addAction( showInOktetaAction );
    }

    QAction *action = menu->exec( m_variablesWidget->mapToGlobal(pos) );
    if ( action == copyAction ) {
        if ( isBinaryData ) {
            // Copy binary data (from a QByteArray), can be pasted into Okteta
            QMimeData *mime = new QMimeData();
            mime->setData( "binary/octet-stream", data.toByteArray() );
            QApplication::clipboard()->setMimeData( mime );
        } else {
            // Copy text
            QApplication::clipboard()->setText( data.toString() );
        }
    } else if ( action == showInOktetaAction ) {
        // Write data to a temporary file...
        KTemporaryFile *temporary = new KTemporaryFile(); // No QObject* parent argument...
        temporary->setParent( this );
        if ( !temporary->open() ) {
            KMessageBox::information( this, temporary->errorString() );
        } else {
            if ( isBinaryData ) {
                temporary->write( data.toByteArray() );
            } else {
                temporary->write( data.toString().toAscii() );
            }
            temporary->close();

            // ...and open it in Okteta
            KProcess *okteta = new KProcess( this );
            okteta->setProgram( "okteta", QStringList() << temporary->fileName() );
            connect( okteta, SIGNAL(finished(int)), temporary, SLOT(deleteLater()) );
            okteta->start();
        }
    }

    // TODO Update script value
    delete menu.data();
}

void VariablesDockWidget::setSearchString( const QString &searchString )
{
    m_searchLine->setText( searchString );
}

void VariablesDockWidget::enable()
{
    if ( isEnabled() ) {
        return;
    }

    Project *project = m_projectModel->activeProject();
    if ( project ) {
        setEnabled( true );
        setToolTip( QString() );
        m_variableModel = project->debugger()->variableModel();
        m_proxyModel->setSourceModel( m_variableModel );
        m_variablesWidget->setModel( m_proxyModel );
    }
}

void VariablesDockWidget::disable()
{
    if ( !isEnabled() ) {
        return;
    }

    setEnabled( false );
    setToolTip( i18nc("@info:tooltip", "Shows variables when the debugger is interrupted") );
    m_variableModel = 0;
    if ( m_variablesWidget ) {
        m_variablesWidget->setModel( 0 );
    }
}

void VariablesDockWidget::debuggerStateChanged( DebuggerState newState, DebuggerState oldState )
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

void VariablesDockWidget::debuggerContinued( const QDateTime &timestamp,
                                             bool willInterruptAfterNextStatement )
{
    Q_UNUSED( timestamp );
    if ( !willInterruptAfterNextStatement ) {
        disable();
    }
}

void VariablesDockWidget::activeProjectAboutToChange( Project *project, Project *previousProject )
{
    if ( previousProject ) {
        disconnect( previousProject->debugger(), SIGNAL(continued(QDateTime,bool)),
                    this, SLOT(debuggerContinued(QDateTime,bool)) );
        disconnect( previousProject->debugger(), SIGNAL(stateChanged(DebuggerState,DebuggerState)),
                    this, SLOT(debuggerStateChanged(DebuggerState,DebuggerState)) );
    }

    if ( project ) {
        m_variableModel = project->debugger()->variableModel();
        m_proxyModel->setSourceModel( m_variableModel );
        m_variablesWidget->setModel( m_proxyModel );

        connect( project->debugger(), SIGNAL(continued(QDateTime,bool)),
                 this, SLOT(debuggerContinued(QDateTime,bool)) );
        connect( project->debugger(), SIGNAL(stateChanged(DebuggerState,DebuggerState)),
                 this, SLOT(debuggerStateChanged(DebuggerState,DebuggerState)) );
    }
}

QWidget *VariablesDockWidget::mainWidget() const {
    return m_variablesWidget;
}
