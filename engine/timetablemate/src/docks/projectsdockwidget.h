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

#ifndef PROJECTSDOCKWIDGET_H
#define PROJECTSDOCKWIDGET_H

#include "abstractdockwidget.h"

class Project;
class ProjectModel;
class QModelIndex;
class QTreeView;

class ProjectsDockWidget : public AbstractDockWidget {
    Q_OBJECT
public:
    explicit ProjectsDockWidget( ProjectModel *model, KActionMenu *showDocksAction,
                                 QWidget *parent = 0 );

    virtual KIcon icon() const { return KIcon("project-development"); };
    virtual Qt::DockWidgetArea defaultDockArea() const { return Qt::LeftDockWidgetArea; };
    ProjectModel *model() const { return m_model; };
    QTreeView *projectsWidget() const { return m_projectsWidget; };

protected slots:
    void projectItemDoubleClicked( const QModelIndex &index );
    void projectItemContextMenuRequested( const QPoint &pos );

private:
    ProjectModel *m_model;
    QTreeView *m_projectsWidget;
};

#endif // Multiple inclusion guard
