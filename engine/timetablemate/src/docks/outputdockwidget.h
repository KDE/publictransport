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

#ifndef OUTPUTDOCKWIDGET_H
#define OUTPUTDOCKWIDGET_H

#include "abstractdockwidget.h"

class Project;
class ProjectModel;
namespace Debugger {
    class Debugger;
};
class QScriptContextInfo;
class QPlainTextEdit;

class OutputDockWidget : public AbstractDockWidget {
    Q_OBJECT
public:
    explicit OutputDockWidget( ProjectModel *projectModel, KActionMenu *showDocksAction,
                               QWidget *parent = 0 );

    virtual KIcon icon() const { return KIcon("system-run"); };
    virtual Qt::DockWidgetArea defaultDockArea() const { return Qt::BottomDockWidgetArea; };
    QPlainTextEdit *outputWidget() const { return m_outputWidget; };

public slots:
    void appendHtml( const QString &html );

protected slots:
    /** @brief The script produced output at @p context. */
    void scriptOutput( const QString &outputString, const QScriptContextInfo &contextInfo );

    void scriptErrorReceived( const QString &errorMessage, const QScriptContextInfo &contextInfo,
                              const QString &failedParseText );

    void activeProjectAboutToChange( Project *project, Project *previousProject );

private:
    QPlainTextEdit *m_outputWidget;
    ProjectModel *m_projectModel;
};

#endif // Multiple inclusion guard
