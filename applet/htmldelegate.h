/*
 *   Copyright 2009 Friedrich Pülz <fpuelz@gmx.de>
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

class QPainter;
class QTextLayout;

// A delegate than can display html formatted text.
class HtmlDelegate : public QItemDelegate
{
    public:
	enum DataRole {
	    FormattedTextRole = Qt::UserRole + 500, // Used to store formatted text. The text of an item should not contain html tags, if used in a combo box.
	    TextBackgroundRole = Qt::UserRole + 501,
	    DecorationPositionRole = Qt::UserRole + 502,
	    GroupTitleRole = Qt::UserRole + 503,
	    LinesPerRowRole = Qt::UserRole + 504 // Used to change the number of lines for a row
	};

	// Position of the decoration
	enum DecorationPosition {
	    Left, // Show the decoration on the left
	    Right // Show the decoration on the right
	};

	HtmlDelegate();

	virtual QSize sizeHint ( const QStyleOptionViewItem& option, const QModelIndex& index ) const;
	bool alignText() const { return m_alignText; };
	void setAlignText( bool alignText ) { m_alignText = alignText; };

    protected:
	virtual void paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const;

	virtual void drawDecoration( QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect, const QPixmap& pixmap ) const;
	virtual void drawDisplayWithShadow( QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect, const QString& text, bool bigContrastShadow = false ) const;


    private:
	bool m_alignText;
// 	void plainTextToHTMLCharCount( const QString &sHtml, int pos, int plainTextCount, int *htmlPos, int *htmlCount ) const;
// 	void plainTextToHTMLCharPos( const QString &sHtml, int pos, int *htmlPos ) const;

	// From the tasks plasmoid
// 	QSize layoutText( QTextLayout &layout, const QString &text, const QSize &constraints ) const;
	// From the tasks plasmoid
// 	void drawTextLayout( QPainter *painter, const QTextLayout &layout, const QRect &rect, const QStyleOptionViewItem& option ) const;
};

#endif // HTMLDELEGATE_HEADER
