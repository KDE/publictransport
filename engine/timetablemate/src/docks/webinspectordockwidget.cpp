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
#include "webinspectordockwidget.h"

// Own includes
#include "../projectmodel.h"
#include "../project.h"
#include "../tabs/webtab.h"

// KDE includes
#include <KLocalizedString>

// Qt includes
#include <QWebInspector>
#include <QLabel>

WebInspectorDockWidget::WebInspectorDockWidget( KActionMenu *showDocksAction, QWidget *parent )
        : AbstractDockWidget( i18nc("@window:title Dock title", "Web Inspector"),
                             showDocksAction, parent ),
          m_webInspector(0), m_placeholder(0)
{
    setObjectName( "webinspector" );
    setWhatsThis( i18nc("@info:whatsthis", "<title>Inspect web elements</title>"
                        "<para>Provides a web inspector.</para>") );
    m_placeholder = new QLabel( i18nc("@info/plain", "Open a web tab to show the inspector."),
                                this );
    setWidget( m_placeholder );
}

void WebInspectorDockWidget::setWebTab( WebTab *webTab )
{
    // Hide previous inspector widget
    if ( m_webInspector ) {
        m_webInspector->hide();
        setWidget( m_placeholder );
        m_placeholder->show();
    }

    // Add new inspector widget
    if ( webTab ) {
        m_placeholder->hide();
        m_webInspector = webTab->webInspector();
        setWidget( m_webInspector );
        m_webInspector->show();
    }
}

QWidget *WebInspectorDockWidget::mainWidget() const {
    return m_webInspector;
}
