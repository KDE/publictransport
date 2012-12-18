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
#include "outputdockwidget.h"

// Own includes
#include "../debugger/debugger.h"
#include "../projectmodel.h"
#include "../project.h"

// KDE includes
#include <KLocalizedString>

// Qt includes
#include <QPlainTextEdit>
#include <QMenu>
#include <QScriptContextInfo>

OutputDockWidget::OutputDockWidget( ProjectModel *projectModel, KActionMenu *showDocksAction,
                                    QWidget *parent )
        : AbstractDockWidget( i18nc("@window:title Dock title", "Output"), showDocksAction, parent ),
          m_projectModel(projectModel)
{
    setObjectName( "output" );

    setWhatsThis( i18nc("@info:whatsthis", "<title>Script output</title>"
                        "<para>Shows output from the script, eg. when print() or helper.error() "
                        "gets called. Additionaly each time the debugger starts or ends execution "
                        "an information gets added.</para>") );

    m_outputWidget = new QPlainTextEdit( this );
    m_outputWidget->setReadOnly( true );
    m_outputWidget->setMinimumSize( 150, 75 );
    m_outputWidget->setContextMenuPolicy( Qt::CustomContextMenu );
    connect( m_outputWidget, SIGNAL(customContextMenuRequested(QPoint)),
             this, SLOT(showContextMenu(QPoint)) );
    setWidget( m_outputWidget );

    connect( projectModel, SIGNAL(activeProjectAboutToChange(Project*,Project*)),
             this, SLOT(activeProjectAboutToChange(Project*,Project*)) );
    if ( projectModel->activeProject() ) {
        connect( projectModel->activeProject(), SIGNAL(outputAppended(QString)),
                 this, SLOT(outputAppended(QString)) );
        connect( projectModel->activeProject(), SIGNAL(outputCleared()),
                 m_outputWidget, SLOT(clear()) );
    }
}

void OutputDockWidget::showContextMenu( const QPoint &pos )
{
    if ( !m_projectModel->activeProject() ) {
        return;
    }

    QMenu *menu = m_outputWidget->createStandardContextMenu();
    QAction *clearAction = menu->addAction( KIcon("edit-clear-list"), i18nc("@action", "&Clear"),
                                            m_projectModel->activeProject(), SLOT(clearOutput()) );
    clearAction->setEnabled( !m_outputWidget->document()->isEmpty() );
    menu->exec( m_outputWidget->mapToGlobal(pos) );
    delete menu;
}

void OutputDockWidget::activeProjectAboutToChange( Project *project, Project *previousProject )
{
    if ( previousProject ) {
        disconnect( previousProject, SIGNAL(outputAppended(QString)),
                    this, SLOT(outputAppended(QString)) );
        disconnect( previousProject, SIGNAL(outputCleared()), m_outputWidget, SLOT(clear()) );
    }

    if ( project ) {
        setOutput( project->output() );
        connect( project, SIGNAL(outputAppended(QString)), this, SLOT(outputAppended(QString)) );
        connect( project, SIGNAL(outputCleared()), m_outputWidget, SLOT(clear()) );
    } else {
        m_outputWidget->clear();
    }
}

void OutputDockWidget::outputAppended( const QString &html )
{
    m_outputWidget->appendHtml( html );
    m_outputWidget->ensureCursorVisible();
}

void OutputDockWidget::setOutput( const QString &html )
{
    m_outputWidget->document()->setHtml( html );
    m_outputWidget->ensureCursorVisible();
}
QWidget *OutputDockWidget::mainWidget() const {
    return m_outputWidget;
}
