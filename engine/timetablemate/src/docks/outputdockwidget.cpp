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
    setWidget( m_outputWidget );

    connect( projectModel, SIGNAL(activeProjectAboutToChange(Project*,Project*)),
             this, SLOT(activeProjectAboutToChange(Project*,Project*)) );
    if ( projectModel->activeProject() ) {
        Debugger::Debugger *debugger = projectModel->activeProject()->debugger();
        connect( debugger, SIGNAL(output(QString,QScriptContextInfo)),
                 this, SLOT(scriptOutput(QString,QScriptContextInfo)) );
        connect( debugger, SIGNAL(scriptErrorReceived(QString,QScriptContextInfo,QString)),
                 this, SLOT(scriptErrorReceived(QString,QScriptContextInfo,QString)) );
    }
}

void OutputDockWidget::activeProjectAboutToChange( Project *project, Project *previousProject )
{
    if ( previousProject ) {
        disconnect( previousProject->debugger(), SIGNAL(output(QString,QScriptContextInfo)),
                    this, SLOT(scriptOutput(QString,QScriptContextInfo)) );
        disconnect( previousProject->debugger(), SIGNAL(scriptErrorReceived(QString,QScriptContextInfo,QString)),
                    this, SLOT(scriptErrorReceived(QString,QScriptContextInfo,QString)) );
    }

    if ( project ) {
        connect( project->debugger(), SIGNAL(output(QString,QScriptContextInfo)),
                 this, SLOT(scriptOutput(QString,QScriptContextInfo)) );
        connect( project->debugger(), SIGNAL(scriptErrorReceived(QString,QScriptContextInfo,QString)),
                 this, SLOT(scriptErrorReceived(QString,QScriptContextInfo,QString)) );
    }
}

void OutputDockWidget::appendHtml( const QString &html )
{
    m_outputWidget->appendHtml( html );
}

void OutputDockWidget::scriptOutput( const QString &outputString,
                                     const QScriptContextInfo &contextInfo )
{
    appendHtml( i18nc("@info", "<emphasis strong='1'>Line %1:</emphasis> <message>%2</message>",
                      contextInfo.lineNumber(), outputString) );
}

void OutputDockWidget::scriptErrorReceived( const QString &errorMessage,
                                            const QScriptContextInfo &contextInfo,
                                            const QString &failedParseText )
{
    Q_UNUSED( failedParseText );
    appendHtml( i18nc("@info", "<emphasis strong='1'>Error in line %1:</emphasis> <message>%2</message>",
                      contextInfo.lineNumber(), errorMessage) );
}
