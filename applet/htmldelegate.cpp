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

#include "htmldelegate.h"
#include "global.h"

#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QAbstractItemView>
#include <QTreeView>
#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <qmath.h>

#include <Plasma/PaintUtils>
#include <Plasma/FrameSvg>
#include <KColorScheme>


PublicTransportDelegate::PublicTransportDelegate( QObject *parent )
	: HtmlDelegate( DrawShadows | DontDrawBackground, parent ) {
}

void PublicTransportDelegate::paint( QPainter* painter,
	    const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->setRenderHints( QPainter::SmoothPixmapTransform | QPainter::Antialiasing );

    if ( index.data(DrawBackgroundGradientRole).isValid() ) {
	QPixmap pixmap( option.rect.width(), option.rect.height() / 2 );
	pixmap.fill( Qt::transparent );
	QPainter p( &pixmap );

	QLinearGradient bgGradient( 0, 0, 0, 1 );
	bgGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
	bgGradient.setColorAt( 0, QColor(72, 72, 72, 0) );
	bgGradient.setColorAt( 1, QColor(72, 72, 72, 80) );
	p.fillRect( 0, 0, option.rect.width(), option.rect.height() / 2, bgGradient );

	// Fade out left and right
	QLinearGradient alphaGradient1( 0, 0, 1, 0 );
	QLinearGradient alphaGradient2( 1, 0, 0, 0 );
	alphaGradient1.setCoordinateMode( QGradient::ObjectBoundingMode );
	alphaGradient2.setCoordinateMode( QGradient::ObjectBoundingMode );
	alphaGradient1.setColorAt( 0, QColor(0, 0, 0, 0) );
	alphaGradient1.setColorAt( 1, QColor(0, 0, 0, 255) );
	alphaGradient2.setColorAt( 0, QColor(0, 0, 0, 0) );
	alphaGradient2.setColorAt( 1, QColor(0, 0, 0, 255) );
	p.setCompositionMode( QPainter::CompositionMode_DestinationIn );
	p.fillRect( 0, 0, option.rect.width() / 5, option.rect.height() / 2, alphaGradient1 );
	p.fillRect( option.rect.right() - option.rect.width() / 5 - option.rect.left(), 0,
		    option.rect.width() / 5 + 1, option.rect.height() / 2, alphaGradient2 );
	p.end();

	painter->drawPixmap( option.rect.left(),
			     option.rect.top() + option.rect.height() / 2, pixmap );
    }
    
    HtmlDelegate::paint( painter, option, index );
    
    if ( index.data(TextBackgroundRole).isValid() ) {
	QStringList data = index.data( TextBackgroundRole ).toStringList();
	if ( data.contains("drawFrameForWholeRow")
		    && (option.state.testFlag(QStyle::State_HasFocus)
		    || index.row() > 0) ) {
	    // Draw line above
	    QLinearGradient bgGradient;
	    QRect bgRect = option.state.testFlag( QStyle::State_HasFocus )
		? QRect( QPoint(0, 0), option.rect.size() )
		: QRect( 0, 0, option.rect.width(), 1 );
	    QPixmap pixmap( bgRect.size() );
	    pixmap.fill( Qt::transparent );
	    QPainter p( &pixmap );

	    QColor bgColor1 = option.palette.color( QPalette::Base );
	    QColor bgColor = option.palette.color( QPalette::Text );
	    bgColor.setAlpha( 140 );
	    p.fillRect( bgRect, bgColor );

	    QStyleOptionViewItemV4 opt = option;
	    if ( opt.viewItemPosition == QStyleOptionViewItemV4::Beginning
		 || opt.viewItemPosition == QStyleOptionViewItemV4::OnlyOne )
	    {
		// Fade out left
		p.setCompositionMode( QPainter::CompositionMode_DestinationIn );
		QLinearGradient alphaGradient1( 0, 0, 1, 0 );
		alphaGradient1.setCoordinateMode( QGradient::ObjectBoundingMode );
		alphaGradient1.setColorAt( 0, QColor(0, 0, 0, 0) );
		alphaGradient1.setColorAt( 1, QColor(0, 0, 0, 255) );
		p.fillRect( 0, 0, option.rect.width() / 3,
			    option.rect.height(), alphaGradient1 );
	    }

	    if ( opt.viewItemPosition == QStyleOptionViewItemV4::End
		 || opt.viewItemPosition == QStyleOptionViewItemV4::OnlyOne )
	    {
		// Fade out right
		p.setCompositionMode( QPainter::CompositionMode_DestinationIn );
		QLinearGradient alphaGradient2( 1, 0, 0, 0 );
		alphaGradient2.setCoordinateMode( QGradient::ObjectBoundingMode );
		alphaGradient2.setColorAt( 0, QColor(0, 0, 0, 0) );
		alphaGradient2.setColorAt( 1, QColor(0, 0, 0, 255) );
		p.fillRect( option.rect.right() - option.rect.width() / 3 -
			    option.rect.left(), 0, option.rect.width() / 3 + 1,
			    option.rect.height(), alphaGradient2 );
	    }
	    p.end();

	    painter->drawPixmap( option.rect.topLeft(), pixmap );
	}
    }
}

HtmlDelegate::HtmlDelegate( Options options, QObject *parent )
	    : QItemDelegate( parent ) {
    m_alignText = false;
    m_options = options;
}

void HtmlDelegate::paint( QPainter* painter, const QStyleOptionViewItem& option,
			  const QModelIndex& index ) const {
    painter->setRenderHints( QPainter::SmoothPixmapTransform | QPainter::Antialiasing );

    if ( !m_options.testFlag(DontDrawBackground) ) {
	QStyleOptionViewItemV4 opt = option;
	QStyle *style = QApplication::style();
	style->drawControl( QStyle::CE_ItemViewItem, &opt, painter );
    }
    
    QString formattedText = index.data( FormattedTextRole ).toString();
    QString text = formattedText.isEmpty()
	    ? index.data( Qt::DisplayRole ).toString() : formattedText;
    QSize iconSize = index.data( IconSizeRole ).isValid()
	    ? index.data( IconSizeRole ).toSize() : option.decorationSize;
    if ( iconSize.height() > option.rect.height() )
	iconSize.scale( option.rect.size(), Qt::KeepAspectRatio );
    
    int margin = 4, padding = 2;
    QRect displayRect;
    if ( index.data( Qt::DecorationRole ).isValid()
	    && !index.data( Qt::DecorationRole ).value<QIcon>().isNull() ) {
	QIcon icon = index.data( Qt::DecorationRole ).value<QIcon>();
	QPoint topLeft;

	DecorationPosition decorationPos = index.data(DecorationPositionRole).isValid()
	    ? static_cast<DecorationPosition>(index.data(DecorationPositionRole).toInt())
	    : Left;
	
	if ( decorationPos == Left ) {
	    topLeft = option.rect.topLeft() +
		    QPoint(margin, (option.rect.height() - iconSize.height()) / 2);
	    displayRect = QRect( option.rect.topLeft() +
				 QPoint(margin + iconSize.width() + padding, 0),
				 option.rect.bottomRight() );
	} else { // decorationPos == Right
	    topLeft = option.rect.topRight() + QPoint(-margin - iconSize.width(),
		    (option.rect.height() - iconSize.height()) / 2);
	    displayRect = QRect( option.rect.topLeft(), option.rect.bottomRight() -
				 QPoint(margin + iconSize.width() + padding, 0) );
	}

	QRect decorationRect( topLeft, iconSize );
	drawDecoration( painter, option, decorationRect, icon.pixmap(iconSize) );
    } else { // no decoration
	displayRect = option.rect;
	if ( m_alignText ) // Align text as if an icon would be shown
	    displayRect.adjust( margin + iconSize.width() + padding, 0, 0, 0 );
    }

    QStyleOptionViewItem opt = option;
    QModelIndex topLevelParent = index;
    while ( topLevelParent.parent().isValid() )
	topLevelParent = topLevelParent.parent();
    if ( topLevelParent.data(DrawAlarmBackground).toBool() ) {
	opt.palette.setColor( QPalette::Text, KColorScheme( QPalette::Active )
		.foreground( KColorScheme::NegativeText ).color() );
    }
    drawDisplay( painter, opt, displayRect, text );
    drawFocus( painter, option, displayRect );
}

void HtmlDelegate::drawDecoration( QPainter* painter, const QStyleOptionViewItem& option,
				   const QRect& rect, const QPixmap& pixmap ) const {
    if ( rect.isEmpty() )
	return;
    
    QPixmap bufferPixmap( rect.size() );
    bufferPixmap.fill( Qt::transparent );

    QPainter p( &bufferPixmap );
    QRect rcPixmap = rect.translated( -rect.topLeft() );
    QItemDelegate::drawDecoration( &p, option, rcPixmap, pixmap );
    
//     if ( m_options.testFlag(DrawShadows) )
// 	Plasma::PaintUtils::drawHalo( painter, rect.adjusted(2, 2, -2, -2) );

    painter->drawPixmap( rect.topLeft(), bufferPixmap );
}

void HtmlDelegate::drawDisplay( QPainter* painter, const QStyleOptionViewItem& option,
				const QRect& rect, const QString& text ) const {
    if ( text.isEmpty() || rect.isEmpty() )
	return;

    int margin = 3;
    int lineCount = 0;
    int maxLineCount = qMax( qFloor(rect.height() / option.fontMetrics.lineSpacing()), 1 );
    QRect textRect = rect.adjusted( margin, 0, 0, 0 );
    QColor textColor = option.state.testFlag( QStyle::State_Selected )
	? option.palette.highlightedText().color() : option.palette.text().color();

    bool drawHalos = m_options.testFlag(DrawShadows) && qGray(textColor.rgb()) < 192;
	
    QList< QRect > fadeRects, haloRects;
    int fadeWidth = 30;
    QPixmap pixmap( textRect.size() );
    pixmap.fill( Qt::transparent );
    QPainter p( &pixmap );
    p.setPen( painter->pen() );
    
    QTextDocument document;
    document.setDefaultFont( option.font );
    QTextOption textOption( option.displayAlignment );
    textOption.setTextDirection( option.direction );
    if ( maxLineCount == 1 )
	textOption.setWrapMode( QTextOption::NoWrap );
    else if ( text.contains("<br>") )
	textOption.setWrapMode( QTextOption::ManualWrap );
    else if ( !text.contains(' ') )
	textOption.setWrapMode( QTextOption::WrapAtWordBoundaryOrAnywhere );
    else
	textOption.setWrapMode( QTextOption::WordWrap );
    document.setDefaultTextOption( textOption );

    QString sStyleSheet = QString("body { color:rgba(%1,%2,%3,%4); margin-left: %5px; }")
	.arg( textColor.red() ).arg( textColor.green() ).arg( textColor.blue() )
	.arg( textColor.alpha() ).arg( margin );
    document.setDefaultStyleSheet( sStyleSheet );

    QString sText = text;
    // Here "<br-wrap>" is a special line break that doesn't change wrapping behaviour
    sText.replace( "<br-wrap>", "<br>" );
    if ( sText.indexOf("<body>") == -1 )
	sText = sText.prepend("<body>").append("</body>");

    document.setHtml( sText );
    document.setDocumentMargin( 0 );
    document.setDocumentLayout( 0 ); // This is to prevent a memory leak in setHtml()
    document.documentLayout();

    // Right-aligned text would be aligned far too far on the right
    if ( maxLineCount == 1 && !option.displayAlignment.testFlag(Qt::AlignRight) &&
	    !option.displayAlignment.testFlag(Qt::AlignHCenter) ) {
	document.setPageSize( QSize(99999, textRect.height()) );
    } else
	document.setPageSize( textRect.size() );

    for ( int b = 0; b < document.blockCount(); ++b )
	lineCount += document.findBlockByLineNumber( b ).layout()->lineCount();
    if ( lineCount > maxLineCount )
	lineCount = maxLineCount;

    int textHeight = lineCount * (option.fontMetrics.lineSpacing() + 1);
    QPointF position( 0, (textRect.height() - textHeight) / 2.0f );

    for ( int b = 0; b < document.blockCount(); ++b ) {
	QTextLayout *textLayout = document.findBlockByLineNumber( b ).layout();

	for ( int l = 0; l < textLayout->lineCount(); ++l ) {
	    QTextLine textLine = textLayout->lineAt( l );
	    textLine.draw( &p, position );

	    if ( drawHalos ) {
		QRect haloRect = QStyle::visualRect( textLayout->textOption().textDirection(),
			pixmap.rect(), QRect((textLine.position() + position).toPoint() +
			rect.topLeft(), textLine.naturalTextRect().size().toSize()) );
		if ( haloRect.top() <= textRect.bottom() ) {
		    if ( haloRect.width() > pixmap.width() )
			haloRect.setWidth( pixmap.width() );
		    // Add a halo rect for each drawn text line
		    haloRects << haloRect;
		}
	    }

	    // Add a fade out rect to the list if the line is too long
	    if ( textLine.naturalTextWidth() > textRect.width() - textLine.x() ) {
		int x = int( qMin(textLine.naturalTextWidth(), (qreal)textRect.width()) )
			    - fadeWidth + textLine.x() + position.x();
		int y = int( textLine.position().y() + position.y() );
		QRect r = QStyle::visualRect( textLayout->textOption().textDirection(),
			pixmap.rect(), QRect(x, y, fadeWidth, int(textLine.height()) + 1) );
		fadeRects.append( r );
	    }
	}
    }
    document.setPlainText( QString() ); // This is to prevent a memory leak in setHtml().. But maybe it isn't one?

    // Reduce the alpha in each fade out rect using the alpha gradient
    if ( !fadeRects.isEmpty() ) {
	// (From the tasks plasmoid) Create the alpha gradient for the fade out effect
	QLinearGradient alphaGradient( 0, 0, 1, 0 );
	alphaGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
	if ( option.direction == Qt::LeftToRight ) {
	    alphaGradient.setColorAt( 0, QColor(0, 0, 0, 255) );
	    alphaGradient.setColorAt( 1, QColor(0, 0, 0, 0) );
	} else {
	    alphaGradient.setColorAt( 0, QColor(0, 0, 0, 0) );
	    alphaGradient.setColorAt( 1, QColor(0, 0, 0, 255) );
	}
	
        p.setCompositionMode( QPainter::CompositionMode_DestinationIn );
        foreach ( const QRect &rect, fadeRects )
            p.fillRect( rect, alphaGradient );
    }
    p.end();
    
    if ( m_options.testFlag(DrawShadows) ) {
	if ( drawHalos ) {
	    foreach ( QRect haloRect, haloRects )
		Plasma::PaintUtils::drawHalo( painter, haloRect );
	} else {
	    QImage shadow = pixmap.toImage();
	    Plasma::PaintUtils::shadowBlur( shadow, 2, Qt::black );
	    painter->drawImage( rect.topLeft() + QPoint(1, 2), shadow );
	}
    }

    painter->drawPixmap( rect.topLeft(), pixmap );
}

QSize HtmlDelegate::sizeHint( const QStyleOptionViewItem& option,
			      const QModelIndex& index ) const {
    QSize size = QItemDelegate::sizeHint( option, index );

    if ( index.data(LinesPerRowRole).isValid() ) {
	int lines = qMax( index.data(LinesPerRowRole).toInt(), 1 );
// 	int height = option.fontMetrics.boundingRect( 0, 0, size.width(), 999999,
// 						      0, "AlpfIgj(" ).height();
// 	size.setHeight( (height + option.fontMetrics.leading()) * lines + 4 );
	size.setHeight( lines * (option.fontMetrics.lineSpacing() + 2) );
    }
    else
	size.setHeight( option.fontMetrics.lineSpacing() + 4 );

    return size;
}
