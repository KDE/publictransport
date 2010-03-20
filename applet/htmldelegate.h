/*
 *   Copyright 2010 Friedrich Pülz <fpuelz@gmx.de>
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

// Qt includes
#include <QItemDelegate>
#include <QTextOption>

class QPainter;
class QTextLayout;

/** @class HtmlDelegate
* @brief A delegate than can display html formatted text. */
class HtmlDelegate : public QItemDelegate {
    public:
	enum DataRole {
	    FormattedTextRole = Qt::UserRole + 500, /**< Used to store formatted text. The text of an item should not contain html tags, if used in a combo box. */
	    TextBackgroundRole = Qt::UserRole + 501,
	    DecorationPositionRole = Qt::UserRole + 502,
	    DrawAlarmBackground = Qt::UserRole + 503,
	    LinesPerRowRole = Qt::UserRole + 504, /**< Used to change the number of lines for a row. */
	    IconSizeRole = Qt::UserRole + 505, /**< Used to set a specific icon size for an element. */
	    DrawBackgroundGradientRole = Qt::UserRole + 506 /**< Used to draw a background gradient at the bottom for an element. */
	};

	/** Position of the decoration. */
	enum DecorationPosition {
	    Left, /**< Show the decoration on the left. */
	    Right /**< Show the decoration on the right. */
	};
	
	enum Option {
	    NoOption = 0x0000,
	    DrawShadows = 0x0001,
	    DontDrawBackground = 0x0002
	};
	Q_DECLARE_FLAGS( Options, Option );

	HtmlDelegate( Options options = NoOption, QObject *parent = 0 );

	virtual QSize sizeHint ( const QStyleOptionViewItem& option,
				 const QModelIndex& index ) const;
				 
	bool alignText() const { return m_alignText; };
	void setAlignText( bool alignText ) { m_alignText = alignText; };

	Options options() const { return m_options; };
	void setOption( Option option, bool enable ) {
	    if ( enable )
		m_options |= option;
	    else if ( m_options.testFlag(option) )
		m_options ^= option; };
	void setOptions( Options options ) { m_options = options; };

    protected:
	virtual void paint( QPainter* painter, const QStyleOptionViewItem& option,
			    const QModelIndex& index ) const;

	virtual void drawDecoration( QPainter* painter, const QStyleOptionViewItem& option,
				     const QRect& rect, const QPixmap& pixmap ) const;
	void drawDisplay( QPainter* painter, const QStyleOptionViewItem& option,
			  const QRect& rect, const QString& text ) const;

    private:
	bool m_alignText;
	Options m_options;
};
Q_DECLARE_OPERATORS_FOR_FLAGS( HtmlDelegate::Options );

class PublicTransportDelegate : public HtmlDelegate {
    public:
	PublicTransportDelegate( QObject *parent = 0 );
	
    protected:
	virtual void paint( QPainter* painter, const QStyleOptionViewItem& option,
			    const QModelIndex& index ) const;
};

#endif // HTMLDELEGATE_HEADER
