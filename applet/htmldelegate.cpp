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

#include "htmldelegate.h"
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QDebug>
#include <qabstractitemview.h>
#include <QTreeView>
#include <QAbstractTextDocumentLayout>
#include <Plasma/PaintUtils>
#include <QPainter>
#include "global.h"
#include <Plasma/Svg>
#include <plasma/framesvg.h>

HtmlDelegate::HtmlDelegate() : QItemDelegate() {
    m_alignText = false;
}

/*
// TODO: simplify this... (not neede anymore?)
void HtmlDelegate::plainTextToHTMLCharPos( const QString& sHtml, int pos, int* htmlPos ) const
{
    int i = 0;
    bool insideHtmlTag = false;
    *htmlPos = 0;

//     qDebug() << "   HTML " << sHtml << "pos" << pos << "plainTextCount" << plainTextCount;

    // Get pos in html
    if ( pos != 0 )
	while( pos >= 0 && i < sHtml.count() )
	{
	    QChar ch = sHtml[i];
	    if ( ch == '<' )
		insideHtmlTag = true;
	    else if ( pos == 0 )
		break;

	    if ( !insideHtmlTag )
		--pos;
	    ++*htmlPos;
	    ++i;

	    if ( ch == '>' )
		insideHtmlTag = false;
	}

    if ( *htmlPos > 0 )
	--*htmlPos;
}

// TODO: ...and simplify that (not neede anymore?)
void HtmlDelegate::plainTextToHTMLCharCount( const QString &sHtml, int pos, int plainTextCount, int *htmlPos, int *htmlCount ) const
{
    int i = 0;
    bool insideHtmlTag = false;
    *htmlCount = 0;

    plainTextToHTMLCharPos( sHtml, pos, htmlPos );

    i = *htmlPos;
    while( plainTextCount >= 0 && i < sHtml.count() )
    {
	QChar ch = sHtml[i];
	if ( ch == '<' )
	    insideHtmlTag = true;
	else if ( plainTextCount == 0 )
	    break;

	if ( !insideHtmlTag )
	    --plainTextCount;
	++*htmlCount;
	++i;

	if ( ch == '>' )
	    insideHtmlTag = false;
    }

//     qDebug() << "   HTML " << "htmlPos" << *htmlPos << "htmlCount" << *htmlCount;
}*/

void HtmlDelegate::paint ( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const {
    QRect rect = option.rect;

    drawBackground( painter, option, index );
    if ( index.data( TextBackgroundRole ).isValid() ) {
	QStringList data = index.data( TextBackgroundRole ).toStringList();

	Plasma::FrameSvg *svg = new Plasma::FrameSvg(parent());
	svg->setImagePath( "widgets/frame" );
	svg->setElementPrefix( data.at(0) );
	svg->resizeFrame( rect.size() );

	if ( data.contains("drawFrameForWholeRow") ) {
	    svg->setEnabledBorders(Plasma::FrameSvg::TopBorder | Plasma::FrameSvg::BottomBorder);
	    if ( index.column() == 0 )
		svg->setEnabledBorders(svg->enabledBorders() | Plasma::FrameSvg::LeftBorder);
	    if ( index.column() == index.model()->columnCount() - 1 )
		svg->setEnabledBorders(svg->enabledBorders() | Plasma::FrameSvg::RightBorder);
	}

// 	painter->save();
// 	QPixmap bgPixmap( rect.size() );
// 	QPainter p( &bgPixmap );
// // 	painter->setClipRegion( svg->mask().translated(rect.topLeft()) );
// // 	bgPixmap.fill( Qt::transparent );
// // 	p.translate( -rect.topLeft() );
// // 	drawBackground( &p, option, index );
// 	p.fillRect( 0, 0, rect.width(), rect.height(), Qt::red );
//
// 	p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
// 	p.drawPixmap( 0, 0, svg->alphaMask() );
// 	p.end();
// 	painter->drawPixmap( rect, bgPixmap );
// 	painter->restore();

	svg->paintFrame( painter, rect.topLeft() );
    }
//     else
// 	drawBackground( painter, option, index );

//     int size = qMin(rect.width(), rect.height());
    QString formattedText = index.data( FormattedTextRole ).toString();
    QString text = formattedText.isEmpty() ? index.data( Qt::DisplayRole ).toString() : formattedText;
//     qDebug() << index.data( Qt::DecorationRole );

    int margin = 4, padding = 2;
    QRect displayRect;
    if ( index.data( Qt::DecorationRole ).isValid() && !index.data( Qt::DecorationRole ).value<QIcon>().isNull() ) {
	QIcon icon = index.data( Qt::DecorationRole ).value<QIcon>();
	QPoint topLeft;

	DecorationPosition decorationPos;
	if ( index.data( DecorationPositionRole ).isValid() )
	    decorationPos = static_cast<DecorationPosition>( index.data( DecorationPositionRole ).toInt() );
	else
	    decorationPos = Left;
	if ( decorationPos == Left ) {
	    topLeft = option.rect.topLeft() + QPoint(margin, (rect.height() - option.decorationSize.height()) / 2);
	    displayRect = QRect( rect.topLeft() + QPoint(margin + option.decorationSize.width() + padding, 0), rect.bottomRight() );
	}
	else { // decorationPos == Right
	    topLeft = rect.topRight() + QPoint(-margin - option.decorationSize.width(), (rect.height() - option.decorationSize.height()) / 2);
	    displayRect = QRect( rect.topLeft(), rect.bottomRight() - QPoint(margin + option.decorationSize.width() + padding, 0) );
	}

	QRect decorationRect( topLeft, topLeft + QPoint(option.decorationSize.width(), option.decorationSize.height()) );
	drawDecoration( painter, option, decorationRect, icon.pixmap(option.decorationSize) );
    }
    else { // no decoration
	displayRect = rect;
	if ( m_alignText && !index.data( GroupTitleRole ).isValid() )
	    displayRect.adjust(margin + option.decorationSize.width() + padding, 0, 0, 0);
    }

    if ( index.data( GroupTitleRole ).isValid() ) {
// 	painter->setPen( Qt::darkGray );
// 	painter->drawLine(displayRect.bottomLeft() + QPoint(2, -5), displayRect.bottomRight() + QPoint(-displayRect.width() * 0.3f, -5));

	drawDisplayWithShadow( painter, option, displayRect, text, true );
    } else
	drawDisplayWithShadow( painter, option, displayRect, text );

//     painter->save();
//     painter->setRenderHint(QPainter::Antialiasing, true);
//     if (option.state & QStyle::State_Selected)
//     painter->restore();

//     QItemDelegate::paint ( painter, option, index );
}

void HtmlDelegate::drawDecoration ( QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect, const QPixmap& pixmap ) const {
    // Add 2 pixels around the pixmap to have enough space for the shadow
    QRect rcBuffer = rect.adjusted( -2, -2, 2, 2 );
    QPixmap bufferPixmap( rcBuffer.size() );
    bufferPixmap.fill( Qt::transparent );

    QPainter p( &bufferPixmap );
    QRect rcPixmap = rect.translated( -rect.topLeft() ).translated( 2, 2 );
    QItemDelegate::drawDecoration ( &p, option, rcPixmap, pixmap );

    // Draw shadow
    QColor shadowColor;
    if ( qGray(option.palette.foreground().color().rgb()) > 192 )
        shadowColor = Qt::black;
    else
        shadowColor = Qt::white;

    QImage shadow = bufferPixmap.toImage();
    Plasma::PaintUtils::shadowBlur( shadow, 2, shadowColor );

    if ( shadowColor == Qt::white )
        painter->drawImage( rcBuffer.topLeft(), shadow );
    else
        painter->drawImage( rcBuffer.topLeft() + QPoint(1, 2), shadow );

    painter->drawPixmap( rcBuffer.topLeft(), bufferPixmap );
}

void HtmlDelegate::drawDisplayWithShadow( QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect, const QString& text, bool bigContrastShadow ) const {
    painter->setRenderHint(QPainter::Antialiasing);

    int margin = 3;
    int lineCount = 0, maxLineCount = qMax( rect.height() / option.fontMetrics.lineSpacing(), 1 );
    QRect textRect = rect.adjusted(margin, 0, -margin, 0);
    QTextDocument document;
    document.setDefaultFont( option.font );
    QColor textColor = option.palette.foreground().color();
//     qDebug() << "HtmlDelegate::drawDisplay" << text << textColor;
    QTextOption textOption( option.displayAlignment );
    textOption.setTextDirection( option.direction );
    if ( maxLineCount == 1 )
	textOption.setWrapMode( QTextOption::NoWrap );
    else if ( text.contains("<br>") )
	textOption.setWrapMode( QTextOption::ManualWrap );
    else if ( text.count(' ') == 0 )
	textOption.setWrapMode( QTextOption::WrapAtWordBoundaryOrAnywhere );
    else
	textOption.setWrapMode( QTextOption::WordWrap );
    document.setDefaultTextOption( textOption );

    QString sStyleSheet = QString("body { color:rgba(%1,%2,%3,%4); margin-left: %5px; margin-right: %5px; }").arg( textColor.red() ).arg( textColor.green() ).arg( textColor.blue() ).arg( textColor.alpha() ).arg( margin );;
    document.setDefaultStyleSheet( sStyleSheet );

    QString sText = text;
    sText.replace( "<br-wrap>", "<br>" ); // Special line break that doesn't change wrapping behaviour
    if ( sText.indexOf("<body>") == -1 )
	sText = sText.prepend("<body>").append("</body>");

    document.setHtml( sText );
    document.setDocumentMargin( 0 );
    document.documentLayout();

    // From the tasks plasmoid
    // Create the alpha gradient for the fade out effect
    QLinearGradient alphaGradient(0, 0, 1, 0);
    alphaGradient.setCoordinateMode(QGradient::ObjectBoundingMode);
    if ( option.direction == Qt::LeftToRight )
    {
        alphaGradient.setColorAt(0, QColor(0, 0, 0, 255));
        alphaGradient.setColorAt(1, QColor(0, 0, 0, 0));
    } else
    {
        alphaGradient.setColorAt(0, QColor(0, 0, 0, 0));
        alphaGradient.setColorAt(1, QColor(0, 0, 0, 255));
    }
    if ( maxLineCount == 1 && option.displayAlignment != Qt::AlignRight && option.displayAlignment != Qt::AlignCenter && option.displayAlignment != Qt::AlignHCenter ) // Right-aligned text would be aligned far too far on the right ;)
	document.setPageSize( QSize(99999, textRect.height()) );
    else
	document.setPageSize( textRect.size() );

    for ( int b = 0; b < document.blockCount(); ++b )
	lineCount += document.findBlockByLineNumber( b ).layout()->lineCount();
//     qDebug() << lineCount << text << "max=" << maxLineCount;
    if ( lineCount > maxLineCount )
	lineCount = maxLineCount;
//     qDebug() << rect.height() << "/" << option.fontMetrics.lineSpacing() << "=" << rect.height() / option.fontMetrics.lineSpacing() << ", lineCount=" << lineCount;

    int textHeight = lineCount * option.fontMetrics.lineSpacing();
    QPointF position = textHeight < textRect.height() ?
            QPointF(0, (textRect.height() - textHeight) / 2 + (option.fontMetrics.tightBoundingRect("M").height() - option.fontMetrics.xHeight())) : QPointF(0, 0);
    QList<QRect> fadeRects;
    int fadeWidth = 30;

    QPixmap pixmap( textRect.size() );
    pixmap.fill( Qt::transparent );

    QPainter p( &pixmap );
    p.setPen( painter->pen() );

    for ( int b = 0; b < document.blockCount(); ++b )
    {
	QTextLayout *textLayout = document.findBlockByLineNumber( b ).layout();

	for ( int l = 0; l < textLayout->lineCount(); ++l )
	{
	    QTextLine textLine = textLayout->lineAt( l );
// 	    if ( l == maxLineCount - 1 ) // make last line "infinitly" long, but doesn't work when not layouting..
// 		textLine.setLineWidth(9999999); // also doesn't work for right-aligned text!
	    textLine.draw( &p, position );

	    // Add a fade out rect to the list if the line is too long
	    if ( textLine.naturalTextWidth() + 5 > textRect.width() )
	    {
		int x = int(qMin(textLine.naturalTextWidth(), (qreal)textRect.width())) - fadeWidth + 5;
		int y = int(textLine.position().y() + position.y());
		QRect r = QStyle::visualRect(textLayout->textOption().textDirection(), pixmap.rect(),
					    QRect(x, y, fadeWidth, int(textLine.height())));
		fadeRects.append(r);
	    }
	}
    }

    // Reduce the alpha in each fade out rect using the alpha gradient
    if ( !fadeRects.isEmpty() )
    {
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        foreach (const QRect &rect, fadeRects)
            p.fillRect(rect, alphaGradient);
    }
    p.end();

    QColor shadowColor;
    if ( qGray(textColor.rgb()) > 192 )
        shadowColor = Qt::black;
    else if ( bigContrastShadow && qGray(option.palette.background().color().rgb()) > 192 )
	shadowColor = Qt::darkGray;
    else
        shadowColor = Qt::white;

    QImage shadow = pixmap.toImage();
    Plasma::PaintUtils::shadowBlur( shadow, /*bigContrastShadow ? 4 :*/ 2, shadowColor );

    if ( shadowColor == Qt::white )
        painter->drawImage( rect.topLeft(), shadow );
    else
        painter->drawImage( rect.topLeft() + QPoint(1, 2), shadow );

    painter->drawPixmap( rect.topLeft(), pixmap );

//     QItemDelegate::drawDisplay( painter, option, rect, text );
}

QSize HtmlDelegate::sizeHint ( const QStyleOptionViewItem& option, const QModelIndex& index ) const {
    QSize size = QItemDelegate::sizeHint ( option, index );

    if ( index.data( LinesPerRowRole ).isValid() ) {
	int lines = qMax( index.data( LinesPerRowRole ).toInt(), 1 );
	size.setHeight( option.fontMetrics.lineSpacing() * lines + 4 );
    }
    else
	size.setHeight( option.fontMetrics.lineSpacing() + 4 );

    return size;
}

/*
QSize HtmlDelegate::layoutText( QTextLayout& layout, const QString& text, const QSize& constraints ) const
{
    QFontMetrics metrics(layout.font());
    int leading     = metrics.leading();
    int height      = 0;
    int maxWidth    = constraints.width();
    int widthUsed   = 0;
    int lineSpacing = metrics.lineSpacing();
    QTextLine line;

    layout.setText(text);
    layout.beginLayout();
    while ((line = layout.createLine()).isValid()) {
        height += leading;

        // Make the last line that will fit infinitely long.
        // drawTextLayout() will handle this by fading the line out
        // if it won't fit in the contraints.
        if (height + 2 * lineSpacing > constraints.height()) {
            line.setPosition(QPoint(0, height));
            break;
        }

        line.setLineWidth(maxWidth);
        line.setPosition(QPoint(0, height));

        height += int(line.height());
        widthUsed = int(qMax(qreal(widthUsed), line.naturalTextWidth()));
    }
    layout.endLayout();

    return QSize(widthUsed, height);
}

void HtmlDelegate::drawTextLayout ( QPainter* painter, const QTextLayout& layout, const QRect& rect, const QStyleOptionViewItem& option ) const
{
    if (rect.width() < 1 || rect.height() < 1)
        return;

    QColor textColor = option.palette.text().color();
    painter->setPen(QPen(textColor, 1.0));

    // Create the alpha gradient for the fade out effect
    QLinearGradient alphaGradient(0, 0, 1, 0);
    alphaGradient.setCoordinateMode(QGradient::ObjectBoundingMode);
    if ( layout.textOption().textDirection() == Qt::LeftToRight )
    {
        alphaGradient.setColorAt(0, QColor(0, 0, 0, 255));
        alphaGradient.setColorAt(1, QColor(0, 0, 0, 0));
    } else
    {
        alphaGradient.setColorAt(0, QColor(0, 0, 0, 0));
        alphaGradient.setColorAt(1, QColor(0, 0, 0, 255));
    }
//     int lineCount = 0, maxLineCount = rc.height() / option.fontMetrics.lineSpacing();
//     for ( int b = 0; b < document.blockCount(); ++b )
// 	lineCount += document.findBlockByLineNumber( b ).layout()->lineCount();
//     if ( lineCount > maxLineCount )
//     {qDebug() << "restricted to " << maxLineCount;
// 	lineCount = maxLineCount;
//     }
//     qDebug() << rc.height() << "/" << option.fontMetrics.lineSpacing() << "......" << maxLineCount << lineCount;

    QFontMetrics fm(layout.font());
    int textHeight = layout.lineCount() * fm.lineSpacing();
    QPointF position = textHeight < rect.height() ?
            QPointF(0, (rect.height() - textHeight) / 2 + (fm.tightBoundingRect("M").height() - fm.xHeight())) : QPointF(0, 0);
    QList<QRect> fadeRects;
    int fadeWidth = 30;

    QPixmap pixmap( rect.size() );
    pixmap.fill( Qt::transparent );

    QPainter p( &pixmap );
    p.setPen( painter->pen() );

//     for ( int b = 0; b < document.blockCount(); ++b )
//     {
// 	QTextLayout *textLayout = document.findBlockByLineNumber( b ).layout();

// 	qDebug() << textLayout->text() << textLayout->boundingRect().size();
// 	for ( int l = 0; l < textLayout->lineCount(); ++l )
    // Draw each line in the layout
    for ( int i = 0; i < layout.lineCount(); i++ )
    {
	QTextLine textLine = layout.lineAt( i );
	textLine.draw( &p, position );

	// Add a fade out rect to the list if the line is too long
	if ( textLine.naturalTextWidth() > rect.width() )
	{
	    int x = int(qMin(textLine.naturalTextWidth(), (qreal)rect.width())) - fadeWidth;
	    int y = int(textLine.position().y() + position.y());
	    QRect r = QStyle::visualRect(layout.textOption().textDirection(), pixmap.rect(),
					QRect(x, y, fadeWidth, int(textLine.height())));
	    fadeRects.append(r);
	}
    }
//     }

    // Reduce the alpha in each fade out rect using the alpha gradient
    if ( !fadeRects.isEmpty() )
    {
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        foreach (const QRect &rect, fadeRects)
            p.fillRect(rect, alphaGradient);
    }

    p.end();

    QColor shadowColor;
    if ( qGray(textColor.rgb()) > 192 )
        shadowColor = Qt::black;
    else
        shadowColor = Qt::white;

    QImage shadow = pixmap.toImage();
    Plasma::PaintUtils::shadowBlur( shadow, 2, shadowColor );

    if ( shadowColor == Qt::white )
        painter->drawImage( rect.topLeft(), shadow );
    else
        painter->drawImage( rect.topLeft() + QPoint(1, 2), shadow );

    painter->drawPixmap( rect.topLeft(), pixmap );
}*/


