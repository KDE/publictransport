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
#include "popuphandler.h"

// KDE includes
#include <KStandardAction>
#include <KLocalizedString>
#include <KAction>
#include <KGlobalSettings>
#include <KLineEdit>
#include <KDebug>

// Qt includes
#include <QCoreApplication>
#include <QWidget>
#include <QToolButton>
#include <QEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QMenu>

namespace PublicTransport {

/** @brief Private class of PopupHandler. */
class PopupHandlerPrivate
{
    Q_DECLARE_PUBLIC( PopupHandler )

public:
    enum ResizeMode {
        NoResize = 0,
        TopResize,
        TopRightResize,
        RightResize,
        BottomRightResize,
        BottomResize
    };

    PopupHandlerPrivate( QWidget *popup, QWidget *popupTrigger, PopupHandler::PopupFlags flags,
                         PopupHandler::Position position, PopupHandler *q )
            : popup(popup), popupTrigger(popupTrigger), flags(flags), position(position),
              resizeMode(NoResize), closeButton(0), q_ptr(q)
    {
        // Detect free popup position if wanted
        if ( position == PublicTransport::PopupHandler::AutoPosition ) {
            KLineEdit *lineEdit = qobject_cast< KLineEdit* >( popupTrigger );
            if ( lineEdit && lineEdit->completionMode() != KGlobalSettings::CompletionNone ) {
                // The trigger widget is a KLineEdit with completion enabled,
                // put the popup above the KLineEdit
                this->position = PopupHandler::AboveWidget;
            } else {
                // No other popup on bottom detected
                this->position = PopupHandler::BelowWidget;
            }
        }

        // Create close button if wanted
        if ( flags.testFlag(PopupHandler::ShowCloseButton) ) {
            KAction *closeAction = KStandardAction::close( popup, SLOT(hide()), q );
            closeAction->setToolTip( i18nc("@info:tooltip", "Close Map") );
            closeButton = new QToolButton( popup );
            closeButton->setDefaultAction( closeAction );
            closeButton->setToolButtonStyle( Qt::ToolButtonIconOnly );
            closeButton->setAutoRaise( true );
            closeButton->setFixedSize( 24, 24 );
            closeButton->setCursor( QCursor(Qt::ArrowCursor) );
        }

        // Use the trigger widget as parent of the popup
        popup->setParent( popupTrigger );

        // Use trigger widget as focus proxy
        popup->setFocusProxy( popupTrigger );
        popup->setFocusPolicy( Qt::ClickFocus );

        // Make the popup widget a tooltip window, that deletes itself and gets hover events
        popup->setWindowFlags( Qt::ToolTip );
        popup->setAttribute( Qt::WA_DeleteOnClose );
        popup->setAttribute( Qt::WA_Hover );

        // Hide the popup initially
        popup->hide();

        // Install an event filter to get notified when the popup widget
        // gets shown or hidden
        popup->installEventFilter( q );
    };

    ResizeMode resizeModeFromPos( const QRect &widgetRect, const QPoint &pos ) const {
        const int resizeArea = 5;
        const int cornerSize = 25;
        if ( !widgetRect.contains(pos) ) {
            return NoResize;
        } else if ( pos.x() < widgetRect.right() - resizeArea &&
                    (position == PopupHandler::BelowWidget ? pos.y() < widgetRect.bottom() - resizeArea
                                                      : pos.y() > widgetRect.top() + resizeArea) )
        {
            return NoResize;
        } else if ( pos.x() < widgetRect.right() - cornerSize ) {
            return position == PopupHandler::BelowWidget ? BottomResize : TopResize;
        } else if ( position == PopupHandler::BelowWidget ? pos.y() < widgetRect.bottom() - cornerSize
                                                     : pos.y() > widgetRect.top() + cornerSize )
        {
            return RightResize;
        } else {
            return position == PopupHandler::BelowWidget ? BottomRightResize : TopRightResize;
        }
    };

    Qt::CursorShape cursorForResizeMode( ResizeMode resizeMode ) const {
         switch ( resizeMode ) {
        case TopResize:
        case BottomResize:
            return Qt::SizeVerCursor;
        case RightResize:
            return Qt::SizeHorCursor;
        case TopRightResize:
            return Qt::SizeBDiagCursor;
        case BottomRightResize:
            return Qt::SizeFDiagCursor;
        case NoResize:
        default:
            return Qt::ArrowCursor;
        }
    };

    bool isResizing() const { return resizeMode != NoResize; };

    bool isResizingHorizontally() const {
        return resizeMode == RightResize ||
               resizeMode == TopRightResize || resizeMode == BottomRightResize;
    };

    bool isResizingVertically() const {
        return resizeMode == TopResize || resizeMode == BottomResize ||
               resizeMode == TopRightResize || resizeMode == BottomRightResize;
    };

    bool isResizingTopEdge() const {
        return resizeMode == TopResize || resizeMode == TopRightResize;
    };

    bool isResizingBottomEdge() const {
        return resizeMode == BottomResize || resizeMode == BottomRightResize;
    };

    QWidget *popup;
    QWidget *popupTrigger;
    PopupHandler::PopupFlags flags;
    PopupHandler::Position position;

    ResizeMode resizeMode;
    QToolButton *closeButton;

protected:
    PopupHandler* const q_ptr;
};

PopupHandler::PopupHandler( QWidget *popup, QWidget *popupTrigger,
                            PopupFlags flags, Position position, QObject *parent )
        : QObject( parent ),
          d_ptr(new PopupHandlerPrivate(popup, popupTrigger, flags, position, this))
{
}

PopupHandler::~PopupHandler()
{
    delete d_ptr;
}

PopupHandler *PopupHandler::installPopup( QWidget *popup, QWidget *popupTrigger,
                                          PopupFlags flags, Position position )
{
    return new PopupHandler( popup, popupTrigger, flags, position, popup );
}

QToolButton *PopupHandler::closeButton() const
{
    Q_D( const PopupHandler );
    return d->closeButton;
}

bool PopupHandler::eventFilter( QObject *object, QEvent *event )
{
    Q_D( PopupHandler );

    const QEvent::Type type = event->type();
    if ( object == d->popup && type == QEvent::Hide ) {
        // Popup gets hidden
        qApp->removeEventFilter( this );
        emit popupHidden();
        return false;
    } else if ( object == d->popup && type == QEvent::Show ) {
        // Popup gets shown, update geometry
        QPoint position = d->popupTrigger->mapToGlobal( d->popupTrigger->pos() );

        // Try to use the width of the trigger widget, if it fits into the desktop
        const int width = qMin( KGlobalSettings::desktopGeometry(d->popup).width() -
                                d->popupTrigger->width(), d->popupTrigger->width() );
        const int height = qMin( 225, position.y() );

        switch ( d->position ) {
        case PopupHandler::BelowWidget:
            position.setY( position.y() + d->popupTrigger->height() );
            break;
        case PopupHandler::AboveWidget:
            position.setY( position.y() - height );
            break;
        default:
            kWarning() << "Unexpected position value" << d->position;
            break;
        }
        d->popup->setGeometry( position.x(), position.y(), width, height );


        // Install a global event filter to hide the popup again on certain events
        qApp->installEventFilter( this );

        // Notify connected objects
        emit popupShown();
        return false;
    } else if ( object == d->popup && type == QEvent::Resize && d->closeButton ) {
        // The popup widget was resized, update close button position if any
        const QSize size = dynamic_cast< QResizeEvent* >( event )->size();
        const int distance = d->flags.testFlag(AllowPopupResizing) ? 4 : 2;
        const QRect rect( QPoint(size.width() - d->closeButton->width() - distance, distance),
                          size );
        d->closeButton->setGeometry( rect );
    } else if ( object == d->popup && d->flags.testFlag(AllowPopupResizing) ) {
        // Handle popup resizing if enabled
        switch ( event->type() ) {
        case QEvent::MouseButtonPress: {
            d->resizeMode = d->resizeModeFromPos( QRect(d->popup->pos(), d->popup->size()),
                    dynamic_cast<QMouseEvent*>(event)->globalPos() );
            if ( d->resizeMode != PopupHandlerPrivate::NoResize ) {
                d->popup->grabMouse();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease:
            if ( d->resizeMode != PopupHandlerPrivate::NoResize ) {
                d->resizeMode = PopupHandlerPrivate::NoResize;
                d->popup->releaseMouse();
                return true;
            }
            break;
        case QEvent::MouseMove:
            if ( d->resizeMode == PopupHandlerPrivate::NoResize ) {
                PopupHandlerPrivate::ResizeMode resize =
                        d->resizeModeFromPos( QRect(d->popup->pos(), d->popup->size()),
                                                dynamic_cast<QMouseEvent*>(event)->globalPos() );
                if ( resize != PopupHandlerPrivate::NoResize ) {
                    d->popup->setCursor( d->cursorForResizeMode(resize) );
                    return true;
                }
            } else {
                // Calculate dx/dy to move edges to the current cursor position
                const QRect desktop = KGlobalSettings::desktopGeometry( d->popup );
                const QPoint mouse = dynamic_cast<QMouseEvent*>(event)->globalPos();
                const QSize minSize = d->popup->minimumSize();
                QRect geometry = d->popup->geometry();
                if ( d->isResizingHorizontally() ) {
                    geometry.setRight( qBound(geometry.left() + minSize.width(), mouse.x() + 2,
                                              desktop.right()) );

                    // Snap to width of the trigger widget
                    const int snapSize = 10;
                    if ( qAbs(geometry.width() - d->popupTrigger->width()) < snapSize ) {
                        geometry.setRight( geometry.left() + d->popupTrigger->width() );
                    }
                }

                if ( d->isResizingTopEdge() ) {
                    geometry.setTop( qBound(desktop.top(), mouse.y() - 2,
                                            geometry.bottom() - minSize.height()) );
                } else if ( d->isResizingBottomEdge() ) {
                    geometry.setBottom( qBound(geometry.top() + minSize.height(), mouse.y() + 2,
                                               desktop.bottom()) );
                }
                d->popup->setGeometry( geometry );
                return true;
            }
            break;
        default:
            break;
        }
    }

    // Hide the popup on certain events
    if ( !d->popup->isVisible() ) {
        return false;
    } else if ( qobject_cast<QWidget*>(object) ) {
        // The findChildren() is needed for the marble popup menu to work as expected
        QWidget *widget = qobject_cast< QWidget* >( object );
        if ( (widget == d->popup || d->popup->isAncestorOf(widget) ||
              d->popup->findChildren<QWidget*>().contains(widget)) &&
             (widget->underMouse() || qobject_cast<QMenu*>(object)) )
        {
            // Do not hide the map on events for the map (or one of it's children)
            // while it is hovered, instead let the map widget handle the events
            // Also do not hide the map if the context menu of the map gets shown
            return false;
        } else if ( widget == d->popupTrigger && (type == QEvent::Move || type == QEvent::Resize)) {
            // Hide map if this popup trigger gets moved or resized
            d->popup->hide();
            return false;
        } else if ( (widget->windowFlags() & Qt::Window) && type == QEvent::Move &&
                    widget == d->popupTrigger->window() )
        {
            kDebug() << "TEEEST" << widget << d->popupTrigger->window();
            // Hide map if the window of the trigger widget gets moved
            d->popup->hide();
            return false;
        } else if ( (type == QEvent::MouseButtonPress || (type == QEvent::KeyPress &&
                     dynamic_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)) &&
                    !d->popup->isAncestorOf(widget) &&
                    !d->popup->findChildren<QWidget*>().contains(widget) )
        {
            // Hide map if the mouse or the escape key gets pressed on a widget,
            // which is no child of this. Accept the event and filter it out,
            // ie. do not send it to the widget
            event->accept();
            d->popup->hide();
            return true;
        }
    }

    switch ( type ) {
    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
    case QEvent::MouseButtonPress:
        d->popup->hide();
        break;
    default:
        break;
    }

    return false;
}

}; // namespace PublicTransport
