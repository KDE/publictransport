/*
 *   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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
#include "enums.h"

#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QPainter>
#include <qmath.h>

#include <Plasma/PaintUtils>
#include <Plasma/FrameSvg>
#include <KColorScheme>
#include <KColorUtils>

using namespace Timetable;

PublicTransportDelegate::PublicTransportDelegate( QObject *parent )
        : HtmlDelegate( DrawShadows | DontDrawBackground, parent )
{
}

void PublicTransportDelegate::paint( QPainter* painter, const QStyleOptionViewItem& option,
                                     const QModelIndex& index ) const
{
    painter->setRenderHints( QPainter::SmoothPixmapTransform | QPainter::Antialiasing );

    if ( option.state.testFlag(QStyle::State_HasFocus)
                || option.state.testFlag(QStyle::State_Selected)
                || option.state.testFlag(QStyle::State_MouseOver) ) {
        QColor focusColor = KColorScheme( QPalette::Active, KColorScheme::Selection )
                            .background( KColorScheme::NormalBackground ).color();
        if ( option.state.testFlag(QStyle::State_Selected) ) {
            if ( option.state.testFlag(QStyle::State_MouseOver) ) {
                focusColor.setAlpha( focusColor.alpha() * 0.65f );
            } else {
                focusColor.setAlpha( focusColor.alpha() * 0.55f );
            }
        } else if ( option.state.testFlag( QStyle::State_MouseOver ) ) {
            focusColor.setAlpha( focusColor.alpha() * 0.2f );
        }

        QLinearGradient bgGradient( 0, 0, 1, 0 );
        bgGradient.setCoordinateMode( QGradient::ObjectBoundingMode );

        QStyleOptionViewItemV4 opt = option;
        if ( opt.viewItemPosition == QStyleOptionViewItemV4::Beginning
                    || opt.viewItemPosition == QStyleOptionViewItemV4::OnlyOne ) {
            bgGradient.setColorAt( 0, Qt::transparent );
            bgGradient.setColorAt( 0.1, focusColor );
        } else {
            bgGradient.setColorAt( 0, focusColor );
        }

        if ( opt.viewItemPosition == QStyleOptionViewItemV4::End
                    || opt.viewItemPosition == QStyleOptionViewItemV4::OnlyOne ) {
            bgGradient.setColorAt( 0.6, focusColor );
            bgGradient.setColorAt( 1, Qt::transparent );
        } else {
            bgGradient.setColorAt( 1, focusColor );
        }

        painter->fillRect( option.rect, QBrush(bgGradient) );
    }

    HtmlDelegate::paint( painter, option, index );

    if ( !index.parent().isValid() && index.row() > 0 ) {
        // Draw line above
        QRect lineRect = QRect( option.rect.left(), option.rect.top(), option.rect.width(), 1 );
        QColor lineColor = option.palette.color( QPalette::Text );
        lineColor.setAlpha( 140 );
        QLinearGradient lineGradient( 0, 0, 1, 0 );
        lineGradient.setCoordinateMode( QGradient::ObjectBoundingMode );

        QStyleOptionViewItemV4 opt = option;
        if ( opt.viewItemPosition == QStyleOptionViewItemV4::Beginning
                || opt.viewItemPosition == QStyleOptionViewItemV4::OnlyOne ) {
            lineGradient.setColorAt( 0, option.palette.color( QPalette::Base ) );
            lineGradient.setColorAt( 0.3, lineColor );
        } else {
            lineGradient.setColorAt( 0, lineColor );
        }

        if ( opt.viewItemPosition == QStyleOptionViewItemV4::End
                || opt.viewItemPosition == QStyleOptionViewItemV4::OnlyOne ) {
            lineGradient.setColorAt( 0.7, lineColor );
            lineGradient.setColorAt( 1, option.palette.color( QPalette::Base ) );
        } else {
            lineGradient.setColorAt( 1, lineColor );
        }

        painter->fillRect( lineRect, QBrush( lineGradient ) );
    }
}

class HtmlDelegatePrivate {
public:
    HtmlDelegatePrivate( HtmlDelegate::Options _options ) {
        options = _options;
    };

    HtmlDelegate::Options options;
};

HtmlDelegate::HtmlDelegate( Options options, QObject *parent )
        : QItemDelegate( parent ), d_ptr(new HtmlDelegatePrivate(options))
{
}

void HtmlDelegate::paint( QPainter* painter, const QStyleOptionViewItem& option,
                          const QModelIndex& index ) const
{
    Q_D( const HtmlDelegate );

    painter->setRenderHints( QPainter::SmoothPixmapTransform | QPainter::Antialiasing );

    if ( !d->options.testFlag( DontDrawBackground ) ) {
        QStyleOptionViewItemV4 opt = option;
        QStyle *style = QApplication::style();
        style->drawControl( QStyle::CE_ItemViewItem, &opt, painter );
    }

    QString formattedText = index.data( FormattedTextRole ).toString();
    QString text = formattedText.isEmpty()
                    ? index.data( Qt::DisplayRole ).toString() : formattedText;
    QSize iconSize = index.data( IconSizeRole ).isValid()
                    ? index.data( IconSizeRole ).toSize() : option.decorationSize;
    if ( iconSize.height() > option.rect.height() ) {
        iconSize.scale( option.rect.size(), Qt::KeepAspectRatio );
    }

    int margin = 4, padding = 2;
    QRect displayRect;
    if ( index.data( Qt::DecorationRole ).isValid()
                && !index.data( Qt::DecorationRole ).value<QIcon>().isNull() ) {
        QIcon icon = index.data( Qt::DecorationRole ).value<QIcon>();
        QPoint topLeft;

        DecorationPosition decorationPos = index.data( DecorationPositionRole ).isValid()
                ? static_cast<DecorationPosition>( index.data( DecorationPositionRole ).toInt() )
                : DecorationLeft;

        if ( decorationPos == DecorationLeft ) {
            topLeft = option.rect.topLeft() +
                    QPoint( margin, (option.rect.height() - iconSize.height()) / 2 );
            displayRect = QRect( option.rect.topLeft() +
                    QPoint(margin + iconSize.width() + padding, 0), option.rect.bottomRight() );
        } else { // decorationPos == Right
            topLeft = option.rect.topRight() + QPoint( -margin - iconSize.width(),
                    (option.rect.height() - iconSize.height()) / 2 );
            displayRect = QRect( option.rect.topLeft(), option.rect.bottomRight() -
                    QPoint(margin + iconSize.width() + padding, 0) );
        }

        QRect decorationRect( topLeft, iconSize );
        drawDecoration( painter, option, decorationRect, icon.pixmap( iconSize ) );
    } else { // no decoration
        displayRect = option.rect;
        if ( d->options.testFlag(AlignTextToDecoration) ) { // Align text as if an icon would be shown
            displayRect.adjust( margin + iconSize.width() + padding, 0, 0, 0 );
        }
    }

    QStyleOptionViewItem opt = option;
    QModelIndex topLevelParent = index;
    while ( topLevelParent.parent().isValid() ) {
        topLevelParent = topLevelParent.parent();
    }
    if ( topLevelParent.data( DrawAlarmBackgroundRole ).toBool() ) {
        qreal bias = index.data( AlarmColorIntensityRole ).toReal();
        QColor alarmTextColor = KColorScheme( QPalette::Active )
                                .foreground( KColorScheme::NegativeText ).color();
        QColor color = KColorUtils::mix( opt.palette.color( QPalette::Text ),
                                         alarmTextColor, bias );
        opt.palette.setColor( QPalette::Text, color );
    }
    drawDisplay( painter, opt, displayRect, text );
    drawFocus( painter, option, displayRect );
}

void HtmlDelegate::drawDisplay( QPainter* painter, const QStyleOptionViewItem& option,
                                const QRect& rect, const QString& text ) const {
    Q_D( const HtmlDelegate );

    int margin = 3;
    int lineCount = 0;
    int maxLineCount = qMax( qFloor(rect.height() / option.fontMetrics.lineSpacing()), 1 );
    QRect textRect = rect.adjusted( margin, 0, 0, 0 );
    QColor textColor = option.state.testFlag( QStyle::State_Selected )
            ? option.palette.highlightedText().color() : option.palette.text().color();
    #if KDE_VERSION < KDE_MAKE_VERSION(4,4,0)
        bool drawHalos = false; // No Plasma::PaintUtils::drawHalo() for KDE < 4.4
    #else
        bool drawHalos = d->options.testFlag(DrawShadows) && qGray(textColor.rgb()) < 192;
    #endif

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
    if ( maxLineCount == 1 ) {
        textOption.setWrapMode( QTextOption::NoWrap );
    } else if ( text.contains(QLatin1String("<br>")) ) {
        textOption.setWrapMode( QTextOption::ManualWrap );
    } else if ( !text.contains(QLatin1Char(' ')) ) {
        textOption.setWrapMode( QTextOption::WrapAtWordBoundaryOrAnywhere );
    } else {
        textOption.setWrapMode( QTextOption::WordWrap );
    }
    document.setDefaultTextOption( textOption );

    QString sStyleSheet = QString( "body { color:rgba(%1,%2,%3,%4); margin-left: %5px; }" )
                        .arg( textColor.red() ).arg( textColor.green() ).arg( textColor.blue() )
                        .arg( textColor.alpha() ).arg( margin );
    document.setDefaultStyleSheet( sStyleSheet );

    QString sText = text;
    // Here "<br-wrap>" is a special line break that doesn't change wrapping behaviour
    sText.replace( QLatin1String("<br-wrap>"), QLatin1String("<br>") );
    if ( sText.indexOf(QLatin1String("<body>")) == -1 ) {
        sText = sText.prepend( QLatin1String("<body>") ).append( QLatin1String("</body>") );
    }

    document.setHtml( sText );
    document.setDocumentMargin( 0 );
    document.setDocumentLayout( 0 ); // This is to prevent a memory leak in setHtml(), which maybe isn't one...
    document.documentLayout();

    // Right-aligned text would be aligned far too far on the right
    if ( maxLineCount == 1 && !option.displayAlignment.testFlag( Qt::AlignRight ) &&
                !option.displayAlignment.testFlag( Qt::AlignHCenter ) ) {
        document.setPageSize( QSize( 99999, textRect.height() ) );
    } else {
        document.setPageSize( textRect.size() );
    }

    int blockCount = document.blockCount();
    for ( int b = 0; b < blockCount; ++b ) {
//     TODO    findblockByLineNumber needed?
        lineCount += document.findBlockByNumber( b ).layout()->lineCount();
    }
    if ( lineCount > maxLineCount ) {
        lineCount = maxLineCount;
    }

    int textHeight = lineCount * ( option.fontMetrics.lineSpacing() + 1 );
    QPointF position( 0, (textRect.height() - textHeight) / 2.0f );

    for ( int b = 0; b < blockCount; ++b ) {
        QTextLayout *textLayout = document.findBlockByNumber( b ).layout();
        int lines = textLayout->lineCount();
        for ( int l = 0; l < lines; ++l ) {
            QTextLine textLine = textLayout->lineAt( l );
            textLine.draw( &p, position );

            if ( drawHalos ) {
                QRect haloRect = QStyle::visualRect( textLayout->textOption().textDirection(),
                        pixmap.rect(), QRect((textLine.position() + position).toPoint() +
                        rect.topLeft(), textLine.naturalTextRect().size().toSize()) );
                if ( haloRect.top() <= textRect.bottom() ) {
                    if ( haloRect.width() > pixmap.width() ) {
                        haloRect.setWidth( pixmap.width() );
                    }
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
            alphaGradient.setColorAt( 0, Qt::black );
            alphaGradient.setColorAt( 1, Qt::transparent );
        } else {
            alphaGradient.setColorAt( 0, Qt::transparent );
            alphaGradient.setColorAt( 1, Qt::black );
        }

        p.setCompositionMode( QPainter::CompositionMode_DestinationIn );
        foreach( const QRect &rect, fadeRects ) {
            p.fillRect( rect, alphaGradient );
        }
    }
    p.end();

    if ( d->options.testFlag(HtmlDelegate::DrawShadows) ) {
#if KDE_VERSION >= KDE_MAKE_VERSION(4,4,0)
        if ( drawHalos ) {
            foreach( const QRect &haloRect, haloRects ) {
                Plasma::PaintUtils::drawHalo( painter, haloRect );
            }
        } else
#endif
        {
            QImage shadow = pixmap.toImage();
            Plasma::PaintUtils::shadowBlur( shadow, 3, Qt::black );
            painter->drawImage( rect.topLeft() + QPoint( 1, 2 ), shadow );
        }
    }

    painter->drawPixmap( rect.topLeft(), pixmap );
}

void HtmlDelegate::drawDecoration( QPainter* painter, const QStyleOptionViewItem& option,
                                   const QRect& rect, const QPixmap& pixmap ) const
{
    if ( rect.isEmpty() ) {
        return;
    }

    QPixmap bufferPixmap( rect.size() );
    bufferPixmap.fill( Qt::transparent );

    QPainter p( &bufferPixmap );
    QRect rcPixmap = rect.translated( -rect.topLeft() );
    QItemDelegate::drawDecoration( &p, option, rcPixmap, pixmap );

//     if ( d->options.testFlag(DrawShadows) )
//     Plasma::PaintUtils::drawHalo( painter, rect.adjusted(2, 2, -2, -2) );

    painter->drawPixmap( rect.topLeft(), bufferPixmap );
}

QSize HtmlDelegate::sizeHint( const QStyleOptionViewItem& option,
                              const QModelIndex& index ) const
{
    QSize size = QItemDelegate::sizeHint( option, index );

    if ( index.data(LinesPerRowRole).isValid() ) {
        int lines = qMax( index.data(LinesPerRowRole).toInt(), 1 );
//     int height = option.fontMetrics.boundingRect( 0, 0, size.width(), 999999,
//                               0, "AlpfIgj(" ).height();
//     size.setHeight( (height + option.fontMetrics.leading()) * lines + 4 );
        size.setHeight( lines * (option.fontMetrics.lineSpacing() + 2) );
    } else {
        size.setHeight( option.fontMetrics.lineSpacing() + 4 );
    }

    return size;
}

HtmlDelegate::Options HtmlDelegate::options() const {
    Q_D( const HtmlDelegate );
    return d->options;
}

void HtmlDelegate::setOptions(HtmlDelegate::Options options) {
    Q_D( HtmlDelegate );
    d->options = options;
}

void HtmlDelegate::setOption(HtmlDelegate::Option option, bool enable) {
    Q_D( HtmlDelegate );
    if ( enable ) {
        d->options |= option;
    } else if ( d->options.testFlag(option) ) {
        d->options ^= option;
    }
}
