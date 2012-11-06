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

#ifndef BACKTRACEDOCKWIDGET_H
#define BACKTRACEDOCKWIDGET_H

// Own includes
#include "abstractdockwidget.h"
#include "../debugger/debuggerstructures.h"

namespace Debugger
{
    class BacktraceModel;
}

class Project;
class ProjectModel;
class KComboBox;
class QTreeView;
class QUrl;
class QModelIndex;
class QStandardItemModel;
using namespace Debugger;

/**
 * @brief A QDockWidget that shows a backtrace for the active project.
 *
 * Gets enabled/disabled automatically when the state of the debugger changes. Only when the
 * debugger is interrupted, a backtrace is shown and the dock widget is enabled.
 *
 * Connect the activeFrameDepthChanged(int) signal to a switchToVariableStack(int) slot of a
 * VariableModel to have it automatically use the stack depth selected in the backtrace widget.
 **/
class BacktraceDockWidget : public AbstractDockWidget {
    Q_OBJECT
public:
    explicit BacktraceDockWidget( ProjectModel *projectModel, KActionMenu *showDocksAction,
                                  QWidget *parent = 0 );

    virtual KIcon icon() const { return KIcon("view-list-text"); };
    virtual Qt::DockWidgetArea defaultDockArea() const { return Qt::BottomDockWidgetArea; };
    ProjectModel *projectModel() const { return m_projectModel; };
    QTreeView *backtraceWidget() const { return m_backtraceWidget; };

signals:
    void activeFrameDepthChanged( int depth );

protected slots:
    /** @brief An item in the backtrace widget was clicked. */
    void clickedBacktraceItem( const QModelIndex &index );

    void activeProjectAboutToChange( Project *project, Project *previousProject );
    void debuggerContinued( const QDateTime &timestamp, bool willInterruptAfterNextStatement );
    void debuggerStateChanged( DebuggerState newState, DebuggerState oldState );

private:
    void enable();
    void disable();

    ProjectModel *const m_projectModel;
    QTreeView *const m_backtraceWidget;
};

#endif // Multiple inclusion guard
