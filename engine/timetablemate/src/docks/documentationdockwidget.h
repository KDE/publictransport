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

#ifndef DOCUMENTATIONDOCKWIDGET_H
#define DOCUMENTATIONDOCKWIDGET_H

#include "abstractdockwidget.h"

class KActionMenu;
class KWebView;
class KComboBox;
class QUrl;

class DocumentationDockWidget : public AbstractDockWidget {
    Q_OBJECT
public:
    explicit DocumentationDockWidget( KActionMenu *showDocksAction, QWidget *parent = 0 );

    virtual KIcon icon() const { return KIcon("documentation"); };
    virtual Qt::DockWidgetArea defaultDockArea() const { return Qt::RightDockWidgetArea; };
    KComboBox *documentationChooser() const { return m_documentationChooser; };
    KWebView *documentationWidget() const { return m_documentationWidget; };

protected slots:
    void documentationChosen( int index );
    void documentationUrlChanged( const QUrl &url );

private:
    KComboBox *m_documentationChooser;
    KWebView *m_documentationWidget;
};

#endif // Multiple inclusion guard
