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

#ifndef HTMLDELEGATE_HEADER
#define HTMLDELEGATE_HEADER

/** @file
 * @brief Contains the HtmlDelegate.
 *
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#include <QItemDelegate>

class QPainter;
class QTextLayout;
class HtmlDelegatePrivate;
class PublicTransportDelegatePrivate;

/** @class HtmlDelegate
 * @brief A delegate than can display html formatted text.
 *
 * It uses html data in @ref FormattedTextRole (from the enumeration @ref ModelDataRoles
 * in global.h).
 * @ref LocationModel and @ref ServiceProviderModel both use HtmlDelegate and
 * @ref FormattedTextRole.
 *
 * There are some options to control what/how the delegate draws items. It can draw a halo/shadow
 * for text and decoration using @ref DrawShadows. A background is drawn by default, but can be
 * turned off using @ref DontDrawBackground.
 * The options can be set in the constructor or later using @ref setOption and @ref setOptions.
 **/
class HtmlDelegate : public QItemDelegate {
public:
    /**
     * @brief Options to control what/how the delegate draws items.
     **/
    enum Option {
        NoOption = 0x0000, /**< No options for the HtmlDelegate. */
        DrawShadows = 0x0001, /**< Draw shadows/halos for text and decoration. */
        DontDrawBackground = 0x0002, /**< Don't draw a background, just leave it transparent. */
        AlignTextToDecoration = 0x0004 /**< Aligns text always as if a decoration would be drawn. */
    };
    Q_DECLARE_FLAGS( Options, Option )

    /**
     * @brief Creates a new HTML delgate with the given @p options.
     *
     * @param options Options for the delegate. Default is NoOption.
     * @param parent The parent for this delegate. Default is 0.
     **/
    explicit HtmlDelegate( Options options = NoOption, QObject *parent = 0 );

    /** @brief Reimplemented from QItemDelegate. */
    virtual QSize sizeHint( const QStyleOptionViewItem& option, const QModelIndex& index ) const;

    /**
     * @brief Gets the options of the delegate.
     *
     * @return The current options of the delegate.
     **/
    Options options() const;

    /**
     * @brief Enables/disables the given @p option.
     *
     * @param option The option too enable/disable.
     * @param enable True, to enable @p option. False, to disable it.
     **/
    void setOption( Option option, bool enable );

    /**
     * @brief Sets the options of the delegate to @p options.
     *
     * @param options The new options for the delegate.
     **/
    void setOptions( Options options );

protected:
    /** @brief Reimplemented from QItemDelegate. */
    virtual void paint( QPainter* painter, const QStyleOptionViewItem& option,
                        const QModelIndex& index ) const;

    /** @brief Reimplemented from QItemDelegate. */
    virtual void drawDecoration( QPainter* painter, const QStyleOptionViewItem& option,
                                 const QRect& rect, const QPixmap& pixmap ) const;

    /** @brief Reimplemented from QItemDelegate. */
    virtual void drawDisplay( QPainter* painter, const QStyleOptionViewItem& option,
                              const QRect& rect, const QString& text ) const;

    HtmlDelegatePrivate* const d_ptr;

private:
    Q_DECLARE_PRIVATE( HtmlDelegate )
    Q_DISABLE_COPY( HtmlDelegate )
};
Q_DECLARE_OPERATORS_FOR_FLAGS( HtmlDelegate::Options )


/**
 * @brief This delegate is used by the publictransport applet to draw
 *   departures/arrivals/journeys.
 **/
class PublicTransportDelegate : public HtmlDelegate {
public:
    /**
     * @brief Creates a new public transport delgate.
     *
     * @param parent The parent for this delegate. Default is 0.
     **/
    PublicTransportDelegate( QObject *parent = 0 );

protected:
    /** @brief Reimplemented from HtmlDelegate. */
    virtual void paint( QPainter* painter, const QStyleOptionViewItem& option,
                        const QModelIndex& index ) const;
};

#endif // HTMLDELEGATE_HEADER
