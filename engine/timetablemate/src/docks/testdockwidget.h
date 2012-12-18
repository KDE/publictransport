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

#ifndef TESTDOCKWIDGET_H
#define TESTDOCKWIDGET_H

#include "abstractdockwidget.h"

class ProjectModel;
class Project;
class TestModel;
class QTreeView;
class QModelIndex;

/**
 * @brief A QDockWidget that shows the contents of a TestModel.
 **/
class TestDockWidget : public AbstractDockWidget {
    Q_OBJECT
public:
    explicit TestDockWidget( ProjectModel *projectModel, KActionMenu *showDocksAction,
                             QWidget *parent = 0 );
    virtual ~TestDockWidget();

    virtual KIcon icon() const { return KIcon("task-complete"); };
    virtual Qt::DockWidgetArea defaultDockArea() const { return Qt::RightDockWidgetArea; };
    TestModel *testModel() const { return m_testModel; };
    QTreeView *testWidget() const { return m_testWidget; };
    virtual QWidget *mainWidget() const;

signals:
    void clickedTestErrorItem( const QString &fileName, int lineNumber, const QString &errorMessage );

protected slots:
    void activeProjectAboutToChange( Project *project, Project *previousProject );
    void rowsInserted( const QModelIndex &parent, int first, int last );
    void itemClicked( const QModelIndex &index );
    void contextMenu( const QPoint &pos );

private:
    void initModel( TestModel *testModel );

    ProjectModel *m_projectModel;
    TestModel *m_testModel;
    QTreeView *m_testWidget;
};

#endif // Multiple inclusion guard
