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

#ifndef PUBLICTRANSPORTTREEVIEW_HEADER
#define PUBLICTRANSPORTTREEVIEW_HEADER

#include <QHeaderView>
#include <QTreeView>

/** Plasma like HeaderView. */
class HeaderView : public QHeaderView {
    public:
	HeaderView( Qt::Orientation orientation, QWidget* parent = 0 );

    protected:
	virtual void paintEvent( QPaintEvent *e );
	virtual void paintSection( QPainter *painter, const QRect& rect,
				   int logicalIndex ) const;
};

/** A QTreeView which viewport fades out on bottom (if the horizontal scrollbar
* is hidden) and on top (if the header is hidden).
* @note It doesn't fade out while animating, because the animation is done
* completely private in QTreeView. */
class TreeView : public QTreeView {
    Q_OBJECT
    public:
	static const int fadeHeight = 16;

	TreeView( QStyle *style );

	QString noItemsText() const { return m_noItemsText; };
	void setNoItemsText( const QString &noItemsText );

    protected:
	QPixmap createFadeTile( const QColor &start, const QColor &end );
	virtual void scrollContentsBy( int dx, int dy );
	
	void drawRowBackground( QPainter *painter, const QStyleOptionViewItem& options,
				const QModelIndex& index ) const;
	virtual void drawRow( QPainter *painter, const QStyleOptionViewItem& options,
			      const QModelIndex& index ) const;
	virtual void paintEvent( QPaintEvent* event );

    private:
	QPixmap m_bottomFadeTile, m_topFadeTile;
	int m_current;
	QModelIndex m_firstDrawnIndex;
	QString m_noItemsText;
};

#endif // Multiple inclusion guard