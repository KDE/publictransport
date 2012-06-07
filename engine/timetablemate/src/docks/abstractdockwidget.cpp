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
#include "abstractdockwidget.h"

// Qt includes
#include <QContextMenuEvent>

// KDE includes
#include <KActionMenu>
#include <KMenu>
#include <KDebug>

AbstractDockWidget::AbstractDockWidget( KActionMenu *showDocksAction, QWidget *parent )
        : QDockWidget(parent), m_showDocksAction(showDocksAction)
{
    Q_ASSERT( showDocksAction );
}

AbstractDockWidget::AbstractDockWidget( const QString &title, KActionMenu *showDocksAction,
                                        QWidget *parent )
        : QDockWidget(title, parent), m_showDocksAction(showDocksAction)
{
    Q_ASSERT( showDocksAction );
}

void AbstractDockWidget::contextMenuEvent( QContextMenuEvent *event )
{
    m_showDocksAction->menu()->exec( event->globalPos() );
    event->accept();
}
