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

#ifndef BREAKPOINTDOCKWIDGET_H
#define BREAKPOINTDOCKWIDGET_H

#include "abstractdockwidget.h"

class Project;
class ProjectModel;

namespace Debugger {
    class Debugger;
    class Breakpoint;
    class BreakpointModel;
}

namespace KTextEditor {
    class Document;
}
class QStandardItemModel;
class QStandardItem;
class QModelIndex;
class QTreeView;

class BreakpointDockWidget : public AbstractDockWidget {
    Q_OBJECT
public:
    explicit BreakpointDockWidget( ProjectModel *projectModel, KActionMenu *showDocksAction,
                                   QWidget *parent = 0 );

    virtual KIcon icon() const { return KIcon("tools-report-bug"); };
    virtual Qt::DockWidgetArea defaultDockArea() const { return Qt::BottomDockWidgetArea; };
    ProjectModel *projectModel() const { return m_projectModel; };
    QTreeView *breakpointWidget() const { return m_breakpointWidget; };
    virtual QWidget *mainWidget() const;

protected slots:
    /** @brief An item in the breakpoint widget was clicked. */
    void clickedBreakpointItem( const QModelIndex &index );

    void checkedStateChanged( const QModelIndex &index, bool checked );

    void activeProjectAboutToChange( Project *project, Project *previousProject );

    void contextMenu( const QPoint &pos );

private:
    Debugger::BreakpointModel *model() const;

    ProjectModel *m_projectModel;
    QTreeView *m_breakpointWidget;
};

#endif // Multiple inclusion guard
