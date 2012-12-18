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

#ifndef NETWORKMONITORDOCKWIDGET_H
#define NETWORKMONITORDOCKWIDGET_H

#include "abstractdockwidget.h"

class NetworkMonitorFilterModel;
class ProjectModel;
class Project;
class AbstractTab;
class QTreeView;

/**
 * @brief A dock widget that shows requests of a QNetworkAccessManager.
 *
 * Requests of the QNetworkAccessManager of the currently active projects web tab are shown, if any.
 * A NetworkMonitorFilterModel gets used to filter by the type of the contents that get requested,
 * eg. HTML, XML, Images, CSS, etc.
 **/
class NetworkMonitorDockWidget : public AbstractDockWidget {
    Q_OBJECT
public:
    explicit NetworkMonitorDockWidget( ProjectModel *projectModel, KActionMenu *showDocksAction,
                                       QWidget *parent = 0 );

    virtual KIcon icon() const { return KIcon("utilities-system-monitor"); };
    virtual Qt::DockWidgetArea defaultDockArea() const { return Qt::BottomDockWidgetArea; };
    NetworkMonitorFilterModel *filterModel() const { return m_filterModel; };
    virtual QWidget *mainWidget() const;

protected slots:
    void activeProjectAboutToChange( Project *project, Project *previousProject );
    void tabOpenRequest( AbstractTab *tab );
    void tabClosed( QObject *tab );
    void contextMenu( const QPoint &pos );

private:
    void initModel();

    QTreeView *m_widget;
    NetworkMonitorFilterModel *m_filterModel;
};

#endif // Multiple inclusion guard
