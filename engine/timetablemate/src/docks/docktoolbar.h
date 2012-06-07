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

#ifndef DOCKTOOLBAR_HEADER
#define DOCKTOOLBAR_HEADER

// KDE includes
#include <QToolBar>
#include <KToggleAction>

// Qt includes
#include <QToolButton>
#include <QPointer>

class KIcon;
class KAction;
class KActionMenu;
class QDockWidget;

/**
 * @brief A tool button, which rotates itself for different dock widget areas.
 *
 * In the bottom dock widget area the tool button gets painted like a normal QToolButton.
 * In the left area it gets rotated -90 degrees, in the right area 90 degrees.
 */
class DockToolButton : public QToolButton
{
    Q_OBJECT
public:
    explicit DockToolButton( Qt::DockWidgetArea area, QWidget *parent = 0 );

    virtual QSize sizeHint() const;
    Qt::Orientation orientation() const;
    Qt::DockWidgetArea area() const { return m_area; };
    void setArea( Qt::DockWidgetArea area );

protected:
    virtual void paintEvent( QPaintEvent *event );

private:
    Qt::DockWidgetArea m_area;
};

/** @brief An action that creates DockToolButton widgets when inserted into a DockToolBar. */
class DockToolButtonAction : public KToggleAction
{
    Q_OBJECT
public:
    explicit DockToolButtonAction( QDockWidget *dockWidget, const KIcon &icon,
                                   const QString &text, QObject *parent );

    /** @brief Get the dock widget associated with this action. */
    inline QDockWidget *dockWidget() const { return m_dockWidget; };

    /** @brief Create a DockToolButton if @p parent is a DockToolBar. */
    virtual QWidget *createWidget( QWidget *parent );

protected:
    virtual void slotToggled( bool checked );

private:
    QDockWidget *m_dockWidget;
};

/**
 * @brief A fixed toolbar, to be filled with DockToolButtonAction's.
 *
 * A DockToolBar is expected to stay in the area given in the constructor.
 * All added DockToolButtonAction's are also added to an exclusive QActionGroup until they get
 * removed from this tool bar again.
 **/
class DockToolBar : public QToolBar
{
    Q_OBJECT
public:
    DockToolBar( Qt::DockWidgetArea area, const QString &objectName, KActionMenu *showDocksAction,
                 QWidget *parent = 0 );

    Qt::DockWidgetArea area() const { return m_area; };
    DockToolButtonAction *actionForDockWidget( QDockWidget *dockWidget );
    QActionGroup *actionGroup() const { return m_group; };

public slots:
    void hideCurrentDock();

protected slots:
    void slotOrientationChanged( Qt::Orientation orientation );

protected:
    /** @brief Reimplemented to add/remove actions to/from the toolbars action group. */
    virtual void actionEvent( QActionEvent *event );

    virtual void contextMenuEvent( QContextMenuEvent *event );

private:
    Qt::ToolBarArea toolBarAreaFromDockWidgetArea( Qt::DockWidgetArea dockWidgetArea );

    Qt::DockWidgetArea m_area;
    QActionGroup *const m_group;
    KActionMenu *const m_showDocksAction;
};

#endif // Multiple inclusion guard
