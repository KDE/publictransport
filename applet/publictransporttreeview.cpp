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

#include "publictransporttreeview.h"

#include "htmldelegate.h"
#include "departuremodel.h"

#include <Plasma/FrameSvg>
#include <Plasma/Theme>
#include <KColorScheme>

#include <QPainter>
#include <QApplication>
#include <QScrollBar>

HeaderView::HeaderView( Qt::Orientation orientation, QWidget* parent )
	    : QHeaderView( orientation, parent ) {
    setAutoFillBackground( false );
    setAttribute( Qt::WA_NoSystemBackground );
}

void HeaderView::paintEvent( QPaintEvent* e ) {
    Plasma::FrameSvg svg;
    if ( Plasma::Theme::defaultTheme()->currentThemeHasImage("widgets/frame") )
	svg.setImagePath( "widgets/frame" );
    else
	svg.setImagePath( "widgets/tooltip" );
    svg.setElementPrefix( "raised" );
    svg.resizeFrame( rect().size() );
    svg.setEnabledBorders( Plasma::FrameSvg::TopBorder |
	    Plasma::FrameSvg::BottomBorder | Plasma::FrameSvg::LeftBorder |
	    Plasma::FrameSvg::RightBorder );

    QPixmap pix( rect().size() );
    pix.fill( Qt::transparent );
    QPainter p( &pix );
    svg.paintFrame( &p, rect().topLeft() );
    p.setCompositionMode( QPainter::CompositionMode_DestinationIn );
    p.fillRect( rect(), QColor(0, 0, 0, 160) );
    p.end();

    QPainter painter( viewport() );
    painter.setRenderHints( QPainter::SmoothPixmapTransform | QPainter::Antialiasing );
    painter.drawPixmap( rect(), pix );
    painter.end();

    QHeaderView::paintEvent( e );
}

void HeaderView::paintSection( QPainter* painter, const QRect& rect,
			int logicalIndex ) const {
    QString text = model()->headerData( logicalIndex, orientation() ).toString();
    painter->setPen( palette().color(QPalette::Text) );
    painter->setRenderHints( QPainter::SmoothPixmapTransform |
				QPainter::Antialiasing );

    // Get the state of the section
    QStyleOptionHeader opt;
    initStyleOption( &opt );

    // Setup the style options structure
    opt.rect = rect;
    opt.section = logicalIndex;
    opt.text = text;

    QVariant textAlignment = model()->headerData( logicalIndex, orientation(),
						    Qt::TextAlignmentRole );
    opt.textAlignment = textAlignment.isValid()
	    ? Qt::Alignment(textAlignment.toInt()) : defaultAlignment();

    if ( isSortIndicatorShown() && sortIndicatorSection() == logicalIndex ) {
	opt.sortIndicator = (sortIndicatorOrder() == Qt::AscendingOrder)
			    ? QStyleOptionHeader::SortDown : QStyleOptionHeader::SortUp;
    }

    QStyleOptionHeader optArrow = opt;
    optArrow.rect = style()->subElementRect( QStyle::SE_HeaderArrow, &opt, this );
    style()->drawPrimitive( QStyle::PE_IndicatorHeaderArrow, &optArrow, painter, this );

    QStyleOptionHeader optLabel = opt;
    optLabel.rect = style()->subElementRect( QStyle::SE_HeaderLabel, &opt, this );
    style()->drawControl( QStyle::CE_HeaderLabel, &optLabel, painter, this );

    if ( resizeMode(logicalIndex) == QHeaderView::Interactive
		&& visualIndex(logicalIndex) < count() - hiddenSectionCount() - 1 ) {
	QStyleOption optSplitter = opt;
	int w = style()->pixelMetric( QStyle::PM_SplitterWidth );
	optSplitter.palette = QApplication::palette();
	optSplitter.rect = QRect( rect.right() - w, 0, w, rect.height() );
	if ( orientation() == Qt::Horizontal )
	    optSplitter.state |= QStyle::State_Horizontal;
	style()->drawControl( QStyle::CE_Splitter, &optSplitter, painter, this );
    }
}

TreeView::TreeView( QStyle* style ) : QTreeView() {
    // Set plasma style (like it's done in Plasma::TreeView)
    setAttribute( Qt::WA_NoSystemBackground );
    setFrameStyle( QFrame::NoFrame );
    verticalScrollBar()->setStyle( style );
    horizontalScrollBar()->setStyle( style );

    // Create fade tiles
    m_topFadeTile = createFadeTile( Qt::transparent, Qt::black );
    m_bottomFadeTile = createFadeTile( Qt::black, Qt::transparent );
}

QPixmap TreeView::createFadeTile( const QColor& start, const QColor& end ) {
    // This code is copied from the folder view applet :)
    QPixmap fadeTile( 256, fadeHeight );
    fadeTile.fill( Qt::transparent );
    QLinearGradient g( 0, 0, 0, fadeHeight );
    g.setColorAt( 0, start );
    g.setColorAt( 1, end );
    QPainter p( &fadeTile );
    p.setCompositionMode( QPainter::CompositionMode_Source );
    p.fillRect( 0, 0, 256, fadeHeight, g );
    p.end();
    return fadeTile;
}

void TreeView::scrollContentsBy( int dx, int dy ) {
    setUpdatesEnabled( false );
    QTreeView::scrollContentsBy( dx, dy );
    setUpdatesEnabled( true );

    const QRect cr = viewport()->contentsRect();
    int addYBottom = dy > 0 ? dy : 0;
    int addYTop = dy < 0 ? -dy : 0;
    const QRect topFadeRect( cr.x(), cr.y(), cr.width(), fadeHeight + addYTop );
    const QRect bottomFadeRect( cr.bottomLeft() - QPoint(0, fadeHeight + addYBottom + 1),
				QSize(cr.width(), fadeHeight + addYBottom) );

    QRegion updateRegion;
    if ( !horizontalScrollBar()->isVisible() )
	updateRegion = updateRegion.united( bottomFadeRect );
    if ( isHeaderHidden() )
	updateRegion = updateRegion.united( topFadeRect );
    viewport()->update( updateRegion );
}

void TreeView::drawRowBackground( QPainter* painter,
				  const QStyleOptionViewItem& options,
				  const QModelIndex& index ) const {
    QModelIndex topLevelParent = index;
    while ( topLevelParent.parent().isValid() )
	topLevelParent = topLevelParent.parent();
    QBrush bgBrush = alternatingRowColors() && topLevelParent.row() & 1
	    ? options.palette.alternateBase() : options.palette.base();
    painter->fillRect( options.rect, bgBrush );

    if ( topLevelParent.data(DrawAlarmBackground).toBool() ) {
	QColor alarmColor = KColorScheme( QPalette::Active )
		.background( KColorScheme::NegativeBackground ).color();
	QLinearGradient bgGradient( 0, 0, 1, 0 );
	bgGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
	bgGradient.setColorAt( 0, Qt::transparent );
	bgGradient.setColorAt( 0.3, alarmColor );
	bgGradient.setColorAt( 0.7, alarmColor );
	bgGradient.setColorAt( 1, Qt::transparent );
	painter->fillRect( options.rect, QBrush(bgGradient) );
    }

    QVariant vr = topLevelParent.data( JourneyRatingRole );
    if ( vr.isValid() ) {
	qreal rating = vr.toReal();
	QColor ratingColor;
	if ( rating >= 0 && rating <= 0.2 ) {
	    ratingColor = KColorScheme( QPalette::Active )
		    .background( KColorScheme::PositiveBackground ).color();
	    ratingColor.setAlphaF( (0.2 - rating) * 5 );
	} else if ( rating >= 0.8 && rating <= 1.0 ) {
	    ratingColor = KColorScheme( QPalette::Active )
		    .background( KColorScheme::NegativeBackground ).color();
	    ratingColor.setAlphaF( (rating - 0.8) * 5 );
	} else
	    return;

	QLinearGradient bgGradient( 0, 0, 1, 0 );
	bgGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
	bgGradient.setColorAt( 0, Qt::transparent );
	bgGradient.setColorAt( 0.3, ratingColor );
	bgGradient.setColorAt( 0.7, ratingColor );
	bgGradient.setColorAt( 1, Qt::transparent );
	painter->fillRect( options.rect, QBrush(bgGradient) );
    }
}

void TreeView::drawRow( QPainter* painter, const QStyleOptionViewItem& options,
			const QModelIndex& index ) const {
    const QRect cr = viewport()->contentsRect();
    const QRect topFadeRect( cr.x(), cr.y(), cr.width(), fadeHeight );
    const QRect bottomFadeRect( cr.bottomLeft() - QPoint(0, fadeHeight),
				QSize(cr.width(), fadeHeight) );
    int scrollValue = verticalScrollBar()->value();

    QStyleOptionViewItem optNoAlternateBase = options;
    optNoAlternateBase.palette.setBrush( QPalette::All, QPalette::AlternateBase,
					    options.palette.base() );

    if ( (scrollValue < verticalScrollBar()->maximum() &&
	    !horizontalScrollBar()->isVisible()
	    && bottomFadeRect.intersects(options.rect)) ||
	    (scrollValue > 0 && isHeaderHidden()
	    && topFadeRect.intersects(options.rect)) )
    {
	QStyleOptionViewItem opt = options;
	opt.rect.moveTopLeft( QPoint(0, 0) );
	optNoAlternateBase.rect.moveTopLeft( QPoint(0, 0) );

	// Draw row into pixmap
	QPixmap pixmap( options.rect.size() );
	pixmap.fill( Qt::transparent );
	QPainter p( &pixmap );
	drawRowBackground( &p, opt, index );
	QTreeView::drawRow( &p, optNoAlternateBase, index );

	// Fade out parts of the row that intersect with the fade rect
	p.setCompositionMode( QPainter::CompositionMode_DestinationIn );
	if ( bottomFadeRect.intersects(options.rect) ) {
	    p.drawTiledPixmap( 0, cr.height() - fadeHeight - options.rect.top() + 1,
				pixmap.width(), fadeHeight, m_bottomFadeTile );
	} else {
	    p.drawTiledPixmap( 0, -options.rect.top(),
				pixmap.width(), fadeHeight, m_topFadeTile );
	}
	p.end();

	// Draw faded row
	painter->drawPixmap( options.rect.topLeft(), pixmap );
    } else {
	drawRowBackground( painter, options, index );
	QTreeView::drawRow( painter, optNoAlternateBase, index );
    }
}

#include "publictransporttreeview.h"