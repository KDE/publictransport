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

#ifndef DOCKS_HEADER
#define DOCKS_HEADER

// KDE includes
#include <KIcon>

// Qt includes
#include <QDockWidget>

class KActionMenu;

/**
 * @brief Base class for TimetableMate dock widgets.
 **/
class AbstractDockWidget : public QDockWidget {
    Q_OBJECT
public:
    explicit AbstractDockWidget( const QString &title, KActionMenu *showDocksAction,
                                 QWidget *parent = 0 );
    explicit AbstractDockWidget( KActionMenu *showDocksAction, QWidget *parent = 0 );

    virtual KIcon icon() const = 0;
    virtual Qt::DockWidgetArea defaultDockArea() const { return Qt::BottomDockWidgetArea; };
    virtual QWidget *mainWidget() const = 0;

protected:
    virtual void contextMenuEvent( QContextMenuEvent *event );

private:
    KActionMenu *const m_showDocksAction;
};

#endif // Multiple inclusion guard
