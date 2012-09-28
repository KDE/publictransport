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

#ifndef VARIABLESDOCKWIDGET_H
#define VARIABLESDOCKWIDGET_H

// Own includes
#include "abstractdockwidget.h"
#include "../debugger/debuggerstructures.h"

class Project;
class ProjectModel;
namespace Debugger {
    class Debugger;
    class VariableModel;
    class VariableFilterProxyModel;
}
class KLineEdit;
class QTreeView;

using namespace Debugger;

/**
 * @brief A QDockWidget that shows a list of variables of the active project for a given stack depth.
 *
 * Gets enabled/disabled automatically when the state of the debugger changes. Only when the
 * debugger is interrupted, a variable list is shown and the dock widget is enabled.
 **/
class VariablesDockWidget : public AbstractDockWidget {
    Q_OBJECT
public:
    explicit VariablesDockWidget( ProjectModel *projectModel, KActionMenu *showDocksAction,
                                  QWidget *parent = 0 );

    virtual KIcon icon() const { return KIcon("debugger"); };
    virtual Qt::DockWidgetArea defaultDockArea() const { return Qt::LeftDockWidgetArea; };
    VariableModel *variableModel() const { return m_variableModel; };
    QTreeView *variablesWidget() const { return m_variablesWidget; };

public slots:
    void setSearchString( const QString &searchString );

protected slots:
    void activeProjectAboutToChange( Project *project, Project *previousProject );
    void debuggerContinued( const QDateTime &timestamp,
                            bool willInterruptAfterNextStatement = false );
    void debuggerStateChanged( DebuggerState newState, DebuggerState oldState );
    void contextMenu( const QPoint &pos );

private:
    void enable();
    void disable();

    ProjectModel *m_projectModel;
    VariableModel *m_variableModel;
    QTreeView *m_variablesWidget;
    KLineEdit *m_searchLine;
    VariableFilterProxyModel *m_proxyModel;
};

#endif // Multiple inclusion guard
