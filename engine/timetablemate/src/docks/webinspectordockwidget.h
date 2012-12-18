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

#ifndef WEBINSPECTORDOCKWIDGET_H
#define WEBINSPECTORDOCKWIDGET_H

#include "abstractdockwidget.h"

class ProjectModel;
class Project;
class AbstractTab;
class WebTab;
class QWebPage;
class QWebInspector;

/**
 * @brief A dock widget that shows QWebInspector widgets of WebTab's.
 *
 * If no inspector widget is shown, a label gets shown with the hint to open a web tab to show
 * the inspector.
 **/
class WebInspectorDockWidget : public AbstractDockWidget {
    Q_OBJECT
public:
    explicit WebInspectorDockWidget( KActionMenu *showDocksAction, QWidget *parent = 0 );

    virtual KIcon icon() const { return KIcon("webkit"); };
    virtual Qt::DockWidgetArea defaultDockArea() const { return Qt::BottomDockWidgetArea; };
    QWebInspector *webInspector() const { return m_webInspector; };
    void setWebTab( WebTab *webTab );
    virtual QWidget *mainWidget() const;

private:
    QWebInspector *m_webInspector;
    QWidget *m_placeholder;
};

#endif // Multiple inclusion guard
