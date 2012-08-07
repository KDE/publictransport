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

#ifndef POPUPHANDLER_H
#define POPUPHANDLER_H

#include <QObject>

class QToolButton;
class PublicTransportLayer;

/** @brief Namespace for the publictransport helper library. */
namespace PublicTransport {

class PopupHandlerPrivate;

/**
 * @brief Can be used to make a widget a popup widget of another widget.
 *
 * Use installPopup() to turn a widget into a popup widget.
 * The popup widget can be shown on above or below the widget triggering the popup.
 * It can have a close button (ShowCloseButton) and it can be resized (AllowPopupResizing).
 **/
class PopupHandler : public QObject
{
    Q_OBJECT
    Q_ENUMS( PopupFlag Position )
    Q_FLAGS( PopupFlags )

public:
    /** @brief Flags for the popup. */
    enum PopupFlag {
        NoPopupFlags        = 0x00, /**< No special features. */
        ShowCloseButton     = 0x01, /**< Show a close button in the top right corner. */
        AllowPopupResizing  = 0x02, /**< Allow resizing of the popup. */
        DefaultFlags = ShowCloseButton | AllowPopupResizing /**< Default popup flags. */
    };
    Q_DECLARE_FLAGS( PopupFlags, PopupFlag );

    /** @brief The position of the popup relative to the trigger widget. */
    enum Position {
        AutoPosition, /**< Automatically detect a position not used by another popup,
                * eg. a KCompletionBox.
                * @note Other popups installed using installPopup() are currently not detected. */
        AboveWidget, /**< Show the popup above the trigger widget. */
        BelowWidget /**< Show the popup under the trigger widget. */
    };

    /**
     * @brief Install a PopupHandler to use @p popup as popup widget for @p popupTrigger.
     *
     * The popup can be shown/hidden using it's QWidget methods show()/hide(). PopupHandler will
     * automatically update the popup widgets geometry from the geometry of @p popupTrigger when
     * it gets shown. A global event filter gets installed while the popup is shown to get notified
     * when the popup should be hidden again. It checks several events like the activation of some
     * window or a click outside the popup.
     * It also works with widgets that create popup menus or show a dialog (the popup gets hidden
     * before the dialog is shown, ie. a window gets activated, see above).
     *
     * @param popup The widget to be used as popup. The widget gets reparented to @p popupTrigger.
     * @param popupTrigger The widget for which the popup gets installed.
     * @param flags Flags for the popup.
     * @param position Position of the popup relative to @p popupTrigger.
     * @return A pointer to the PopupHandler created to control the popup behavior. The parent
     *   of the PopupHandler is @p popup.
     **/
    static PopupHandler *installPopup( QWidget *popup, QWidget *popupTrigger,
                                       PopupFlags flags = DefaultFlags,
                                       Position position = AutoPosition );

    /** @brief Destructor. */
    virtual ~PopupHandler();

    /** @brief Get the flags used to create this PopupHandler. */
    PopupFlags popupFlags() const;

    /** @brief Get a pointer to the close button if any. */
    QToolButton *closeButton() const;

Q_SIGNALS:
    /** @brief Emitted, when the popup gets shown. */
    void popupShown();

    /** @brief Emitted, when the popup gets hidden. */
    void popupHidden();

protected:
    /** @brief Constructor. */
    explicit PopupHandler( QWidget *popup, QWidget *popupTrigger, PopupFlags flags,
                           Position position, QObject *parent = 0 );

    /** @brief Overwritten to control popup behavior. */
    virtual bool eventFilter( QObject *object, QEvent *event );

private:
    PopupHandlerPrivate* const d_ptr;
    Q_DECLARE_PRIVATE( PopupHandler )
    Q_DISABLE_COPY( PopupHandler )
};
Q_DECLARE_OPERATORS_FOR_FLAGS( PopupHandler::PopupFlags )

}; // namespace PublicTransport

#endif // PUBLICTRANSPORTMAPWIDGET_H
