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
#include "docktoolbar.h"

// KDE includes
#include <KAction>
#include <KActionMenu>
#include <KMenu>
#include <KDebug>

// Qt includes
#include <QBoxLayout>
#include <QDockWidget>
#include <QDropEvent>
#include <QStyleOption>
#include <QStylePainter>
#include <QEvent>

DockToolButtonAction::DockToolButtonAction( QDockWidget *dockWidget, const KIcon &icon,
                                            const QString &text, QObject *parent )
        : KToggleAction(parent), m_dockWidget(dockWidget)
{
    setText( text );
    setIcon( icon );

    // No docking to the top area
    dockWidget->setAllowedAreas(
            Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea );

    connect( dockWidget, SIGNAL(visibilityChanged(bool)), this, SLOT(setChecked(bool)) );
    connect( this, SIGNAL(toggled(bool)), this, SLOT(slotToggled(bool)) );
}

void DockToolButtonAction::slotToggled( bool checked )
{
    KToggleAction::slotToggled( checked );

    const bool wasBlocked = m_dockWidget->blockSignals( true );
    m_dockWidget->setVisible( checked );
    m_dockWidget->blockSignals( wasBlocked );
}

QWidget *DockToolButtonAction::createWidget( QWidget *parent )
{
    DockToolBar *toolBar = qobject_cast< DockToolBar* >( parent );
    if ( !toolBar ) {
        // Only create a DockToolButton when inserting the widget into a DockToolBar
        return 0;
    }

    // Create a DockToolButton when inserting the widget into a DockToolBar
    DockToolButton *toolButton = new DockToolButton( toolBar->area(), parent );
    toolButton->setEnabled( isEnabled() );
    toolButton->setText( text() );
    toolButton->setIcon( icon() );
    toolButton->setCheckable( true );
    toolButton->setChecked( dockWidget()->isVisible() );
    toolButton->setIconSize( toolBar->iconSize() );
    toolButton->setToolButtonStyle( toolBar->toolButtonStyle() );

    // Use vertical title bar with horizontal orientation and vice versa
    if ( toolBar->orientation() == Qt::Horizontal ) {
        dockWidget()->setFeatures( dockWidget()->features() | QDockWidget::DockWidgetVerticalTitleBar );
    } else {
        dockWidget()->setFeatures( dockWidget()->features() & ~QDockWidget::DockWidgetVerticalTitleBar );
    }

    connect( this, SIGNAL(toggled(bool)), toolButton, SLOT(setChecked(bool)) );
    connect( toolButton, SIGNAL(toggled(bool)), this, SLOT(setChecked(bool)) );
    return toolButton;
}

DockToolButton::DockToolButton( Qt::DockWidgetArea area, QWidget *parent )
        : QToolButton(parent), m_area(area)
{
}

void DockToolButton::setArea( Qt::DockWidgetArea area )
{
    m_area = area;
    update();
}

Qt::Orientation DockToolButton::orientation() const
{
    if ( m_area == Qt::LeftDockWidgetArea || m_area == Qt::RightDockWidgetArea ) {
        return Qt::Vertical;
    } else {
        return Qt::Horizontal;
    }
}

QSize DockToolButton::sizeHint() const
{
    ensurePolished();

    QStyleOptionToolButton opt;
    initStyleOption( &opt );

    QFontMetrics fm = fontMetrics();
    const int charWidth = fm.width( QLatin1Char('x') );
    QSize textSize;

    // No text size if we're having icon-only button
    if( toolButtonStyle() != Qt::ToolButtonIconOnly ) {
        textSize = fm.size( Qt::TextShowMnemonic, opt.text );
        textSize.rwidth() += 2 * charWidth;
    }

    const int spacing = 2;
    int iconwidth = 0, iconheight = 0;

    // No icon size if we're drawing text only
    if( toolButtonStyle() != Qt::ToolButtonTextOnly ) {
        if( orientation() == Qt::Horizontal ) {
            iconwidth = opt.iconSize.width();
            iconheight = opt.iconSize.height();
        } else {
            iconwidth = opt.iconSize.height();
            iconheight = opt.iconSize.width();
        }
    }

    int width = 4 + textSize.width() + iconwidth + spacing;
    int height = 4 + qMax( textSize.height(), iconheight ) + spacing;
    if( orientation() == Qt::Vertical ) {
        return QSize( height, width );
    } else {
        return QSize( width, height );
    }
}

void DockToolButton::paintEvent( QPaintEvent *event )
{
    if ( orientation() == Qt::Horizontal ) {
        QToolButton::paintEvent(event);
    } else {
        // Paint button rotated
        QStylePainter painter( this );
        QStyleOptionToolButton option;
        initStyleOption( &option );

        // First draw normal frame and not text/icon
        option.text = QString();
        option.icon = QIcon();
        painter.drawComplexControl( QStyle::CC_ToolButton, option );

        // Rotate the options
        QSize size( option.rect.size() );
        size.transpose();
        option.rect.setSize( size );

        // Rotate the painter
        if( m_area == Qt::LeftDockWidgetArea ) {
            painter.translate( 0, height() );
            painter.rotate( -90 );
        } else {
            painter.translate( width(), 0 );
            painter.rotate( 90 );
        }

        // Paint text and icon
        option.text = text();
        QIcon::Mode iconMode = ( option.state & QStyle::State_MouseOver )
                ? QIcon::Active : QIcon::Normal;
        QPixmap ic = icon().pixmap( option.iconSize, iconMode, QIcon::On );
        QTransform tf;
        if ( m_area == Qt::LeftDockWidgetArea ) {
            tf = tf.rotate( 90 );
        } else {
            tf = tf.rotate(-90);
        }
        option.icon = ic.transformed( tf, Qt::SmoothTransformation );
        painter.drawControl( QStyle::CE_ToolButtonLabel, option );
        painter.end();
    }
}

DockToolBar::DockToolBar( Qt::DockWidgetArea area, const QString &objectName,
                          KActionMenu *showDocksAction, QWidget *parent )
        : QToolBar(parent), m_area(area), m_group(new QActionGroup(this)),
          m_showDocksAction(showDocksAction)
{
    Q_ASSERT( showDocksAction );
    setObjectName( objectName );
    m_group->setExclusive( true );
    setAllowedAreas( toolBarAreaFromDockWidgetArea(area) );
    setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
    setMovable( false );
    setFloatable( false );
    setIconSize( QSize(KIconLoader::SizeSmall, KIconLoader::SizeSmall) );
    connect( this, SIGNAL(orientationChanged(Qt::Orientation)),
             this, SLOT(slotOrientationChanged(Qt::Orientation)) );
}

void DockToolBar::contextMenuEvent( QContextMenuEvent *event )
{
    m_showDocksAction->menu()->exec( event->globalPos() );
    event->accept();
}

void DockToolBar::hideCurrentDock()
{
    if ( m_group->checkedAction() ) {
        m_group->checkedAction()->setChecked( false );
    }
}

void DockToolBar::slotOrientationChanged( Qt::Orientation orientation )
{
    foreach ( QAction *action, actions() ) {
        DockToolButtonAction *dockAction = qobject_cast< DockToolButtonAction* >( action );
        QDockWidget *dockWidget = dockAction->dockWidget();
        if ( orientation == Qt::Horizontal ) {
            dockWidget->setFeatures( dockWidget->features() | QDockWidget::DockWidgetVerticalTitleBar );
        } else {
            dockWidget->setFeatures( dockWidget->features() & ~QDockWidget::DockWidgetVerticalTitleBar );
        }
    }
}

Qt::ToolBarArea DockToolBar::toolBarAreaFromDockWidgetArea( Qt::DockWidgetArea dockWidgetArea )
{
    switch ( dockWidgetArea ) {
    case Qt::LeftDockWidgetArea:
        return Qt::LeftToolBarArea;
    case Qt::RightDockWidgetArea:
        return Qt::RightToolBarArea;
    case Qt::TopDockWidgetArea:
        return Qt::TopToolBarArea;
    case Qt::BottomDockWidgetArea:
        return Qt::BottomToolBarArea;

    case Qt::AllDockWidgetAreas:
        return Qt::AllToolBarAreas;
    case Qt::NoDockWidgetArea:
    default:
        return Qt::NoToolBarArea;
    }
}

DockToolButtonAction *DockToolBar::actionForDockWidget( QDockWidget *dockWidget )
{
    foreach ( QAction *action, actions() ) {
        DockToolButtonAction *dockAction = qobject_cast< DockToolButtonAction* >( action );
        if ( dockAction ) {
            if ( dockAction->dockWidget() == dockWidget ) {
                return dockAction;
            }
        } else {
            qWarning() << "Wrong action type in DockToolBar" << action;
        }
    }

    // No action found for dockWidget
    return 0;
}

void DockToolBar::actionEvent( QActionEvent *event )
{
    DockToolButtonAction *action = qobject_cast< DockToolButtonAction* >( event->action() );
    if ( !action ) {
        return;
    }

    switch ( event->type() ) {
    case QEvent::ActionAdded:
        if ( m_group->checkedAction() && action->isChecked() ) {
            // If there is a checked action and the new action is also checked,
            // uncheck the previously checked action,
            // ie. only allow one QDockWidget per area to be shown
            m_group->checkedAction()->setChecked( false );
        }
        m_group->addAction( action );
        break;
    case QEvent::ActionRemoved:
        m_group->removeAction( action );
        break;
    default:
        break;
    }

    QToolBar::actionEvent( event );
}
