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

#include "journeysearchlineedit.h"
#include "journeysearchparser.h"

#include <KColorScheme>
#include <QStyle>
#include <QStyleOption>
#include <QTextLayout>
#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <KDebug>

JourneySearchHighlighter::JourneySearchHighlighter( QTextDocument* parent )
        : QSyntaxHighlighter(parent), m_keywords(new JourneySearchKeywords)
{
    m_formatStopName.setFontWeight( QFont::Bold );
    m_formatStopName.setForeground( Qt::darkMagenta );
    m_formatKeyword.setFontWeight( QFont::Bold );
    m_formatKeyword.setForeground( Qt::darkRed );
    m_formatValue.setForeground( Qt::blue );
    m_formatError.setFontItalic( true );
    m_formatError.setForeground( Qt::red );
}

JourneySearchHighlighter::~JourneySearchHighlighter()
{
    delete m_keywords;
}

int JourneySearchHighlighter::highlightKeywords( const QString& text,
        const QStringList& keywords, const QTextCharFormat& format,
        int maxAllowedOccurances, int needsToStartAt )
{
    QTextCharFormat curFormat = format, curKeywordFormat = m_formatKeyword;
    QRegExp expression( QString( "\\b(%1)\\b" ).arg( keywords.join( "|" ) ), Qt::CaseInsensitive );
    int index = text.indexOf( expression );
    int count = 0;
    while ( index >= 0 ) {
        if (( needsToStartAt != -1 && index != needsToStartAt )
                    || count == maxAllowedOccurances ) {
            // The keyword doesn't start at needsToStartAt, if given
            curFormat = m_formatError;
            curKeywordFormat = m_formatError;
        }

        if ( expression.captureCount() >= 3 ) {
            // Matched a keyword with it's value
            setFormat( expression.pos( 2 ), expression.cap( 2 ).length(), curKeywordFormat );
            setFormat( expression.pos( 3 ), expression.cap( 3 ).length(), curFormat );
        } else {
            setFormat( index, expression.matchedLength(), curFormat );
        }

        // Get next keyword
        index = text.indexOf( expression, index + expression.matchedLength() );
        ++count;
    }

    return count;
}

int JourneySearchHighlighter::highlightCombinations( const QString& text,
        const QStringList& keywords, const QStringList& keywordValues,
        const QTextCharFormat& format, int maxAllowedOccurances, int needsToStartAt )
{
    int count = 0;
    foreach( const QString &keyword, keywords ) {
        foreach( const QString &value, keywordValues ) {
            QString str = QString( "(%1) (%2)" ).arg( keyword ).arg( value );
            count += highlightKeywords( text, QStringList() << str, format,
                                        maxAllowedOccurances, needsToStartAt );
        }
    }
    return count;
}

void JourneySearchHighlighter::highlightBlock( const QString& text )
{
    // Highlight keywords
    highlightKeywords( text, QStringList() << m_keywords->toKeywords()
            << m_keywords->fromKeywords(), m_formatKeyword, 1, 0 );
    highlightKeywords( text, QStringList() << m_keywords->arrivalKeywords()
            << m_keywords->departureKeywords(), m_formatKeyword, 1 );
    highlightKeywords( text, m_keywords->timeKeywordsTomorrow(), m_formatKeyword, 1 );

    // Highlight date/time keys and values
    // ("[time]" or "[date]" or "[time], [date]" or "[date], [time]")
    int matched = highlightCombinations( text, m_keywords->timeKeywordsAt(),
            QStringList() << "\\d{2}:\\d{2}(, \\d{2}\\.\\d{2}\\.(\\d{2,4})?)?"
                << "\\d{2}:\\d{2}(, \\d{2}-\\d{2}(-\\d{2,4})?)?"
                << "\\d{2}:\\d{2}(, (\\d{2,4}-)?\\d{2}-\\d{2})?"
                << "\\d{2}\\.\\d{2}\\.(\\d{2,4})?(, \\d{2}:\\d{2})?"
                << "\\d{2}-\\d{2}(-\\d{2,4})?(, \\d{2}:\\d{2})?"
                << "(\\d{2,4}-)?\\d{2}-\\d{2}(, \\d{2}:\\d{2})?",
            m_formatValue, 1 );

    // Highlight relative time keys and values
    highlightCombinations( text, m_keywords->timeKeywordsIn(),
                        QStringList() << m_keywords->relativeTimeString("\\d{1,}"),
                        m_formatValue, matched == 0 ? 1 : 0 );

    // Highlight stop name if it is inside double quotes
    QRegExp expression = QRegExp( "\\s?\"[^\"]*\"\\s?" );
    int index = text.indexOf( expression );
    while ( index >= 0 ) {
        int length = expression.matchedLength();
        setFormat( index, length, m_formatStopName );
        index = text.indexOf( expression, index + length );
    }
}

JourneySearchLineEdit::JourneySearchLineEdit( QWidget* parent ) : KLineEdit( parent )
{
    init();
}

JourneySearchLineEdit::JourneySearchLineEdit( const QString& string, QWidget* parent )
        : KLineEdit( string, parent )
{
    init();
}

void JourneySearchLineEdit::init()
{
    m_hScroll = m_cursor = 0;

    m_doc.setDocumentMargin( 0 );
    m_doc.setDefaultFont( font() );

    // Set the QSyntaxHighlighter to be used
    m_highlighter = new JourneySearchHighlighter( &m_doc );
    m_highlighter->formatStopName().setForeground(
            KColorScheme(QPalette::Active).foreground(KColorScheme::NeutralText) );
    m_highlighter->formatKeyword().setForeground(
            KColorScheme(QPalette::Active).foreground(KColorScheme::PositiveText) );
    m_highlighter->formatValue().setForeground(
            KColorScheme(QPalette::Active).foreground(KColorScheme::PositiveText) );
    m_highlighter->formatError().setForeground(
            KColorScheme(QPalette::Active).foreground(KColorScheme::NegativeText) );

    connect( this, SIGNAL( textChanged( QString ) ), this, SLOT( slotTextChanged( QString ) ) );
}

void JourneySearchLineEdit::slotTextChanged( const QString& )
{
    m_doc.setHtml( text() );
    m_doc.documentLayout();
}

void JourneySearchLineEdit::mouseDoubleClickEvent( QMouseEvent* ev )
{
    if ( ev->button() == Qt::LeftButton ) {
        deselect();
        QRect cr = lineEditContents();
        m_cursor = m_doc.documentLayout()->hitTest(
                ev->posF() - cr.topLeft() + QPoint( m_hScroll, 0 ), Qt::FuzzyHit );

        QTextBlock block = m_doc.findBlockByNumber( 0 );
        if ( block.isValid() ) {
            m_cursor = block.layout()->previousCursorPosition( m_cursor, QTextLayout::SkipWords );
            int end = block.layout()->nextCursorPosition( m_cursor, QTextLayout::SkipWords );

            QString t = text();
            while ( end > m_cursor && t[end-1].isSpace() ) {
                --end;
            }
            moveCursor( end, true );
        }
    } else {
        KLineEdit::mouseDoubleClickEvent( ev );
    }
}

void JourneySearchLineEdit::mousePressEvent( QMouseEvent* ev )
{
    if ( ev->button() == Qt::LeftButton ) {
        // Send clicks on the clear button to KLineEdit
        if ( isClearButtonShown() ) {
            QSize sz = clearButtonUsedSize();
            QRect rect( width() - sz.width(), 0, sz.width(), sz.height() );
            if ( rect.contains( ev->pos() ) ) {
                KLineEdit::mousePressEvent( ev );
                return;
            }
        }

        bool mark = ev->modifiers() & Qt::ShiftModifier;
        QRect cr = lineEditContents();
        m_cursor = m_doc.documentLayout()->hitTest(
                ev->posF() - cr.topLeft() + QPoint( m_hScroll, 0 ), Qt::FuzzyHit );
        moveCursor( m_cursor, mark );
    } else {
        KLineEdit::mousePressEvent( ev );
    }
}

void JourneySearchLineEdit::mouseMoveEvent( QMouseEvent* ev )
{
    if ( ev->buttons().testFlag( Qt::LeftButton ) ) {
        int cursor = 0;
        QRect cr = lineEditContents();
        cursor = m_doc.documentLayout()->hitTest(
                ev->posF() - cr.topLeft() + QPoint( m_hScroll, 0 ), Qt::FuzzyHit );

        moveCursor( cursor, true );
    } else {
        KLineEdit::mouseMoveEvent( ev );
    }
}

void JourneySearchLineEdit::moveCursor( int pos, bool mark )
{
    if ( mark ) {
        setSelection( m_cursor, pos - m_cursor );
    } else {
        setCursorPosition( pos );
        update();
    }
}

void JourneySearchLineEdit::paintEvent( QPaintEvent* )
{
    QPainter p( this );
    QRect cr = lineEditContents();

    // Draw background panel
    if ( hasFrame() ) {
        QStyleOptionFrameV2 opt;
        initStyleOption( &opt );
        style()->drawPrimitive( QStyle::PE_PanelLineEdit, &opt, &p, this );
    }

    // Draw text, cursor and selection
    int cursorPos = cursorPosition();
    QTextBlock block = m_doc.findBlockByNumber( 0 );
    if ( block.isValid() ) {
        int width = cr.width();
        if ( isClearButtonShown() ) { // Add space for the clear button
            width -= clearButtonUsedSize().width();
        }
        QTextLine line = block.layout()->lineForTextPosition(
                            block.layout()->isValidCursorPosition( cursorPos ) ? cursorPos : cursorPos - 1 );
        if ( line.isValid() ) {
            // Horizontal scrolling
            int cix = line.cursorToX( cursorPos );
            int minLB = qMax( 0, -fontMetrics().minLeftBearing() );
            int minRB = qMax( 0, -fontMetrics().minRightBearing() );
            int widthUsed = line.width() + 1 + minRB;
            if (( minLB + widthUsed ) <= width ) {
                switch ( alignment() ) {
                case Qt::AlignRight:
                    m_hScroll = widthUsed - width;
                    break;
                case Qt::AlignHCenter:
                    m_hScroll = ( widthUsed - width ) / 2;
                    break;
                default:
                    m_hScroll = 0;
                    break;
                }
                m_hScroll -= minLB;
            } else if ( cix - m_hScroll >= width ) {
                m_hScroll = cix - width + 1; // Scroll to the right
            } else if ( cix - m_hScroll < 0 ) {
                m_hScroll = cix; // Scroll to the left
            } else if ( widthUsed - m_hScroll < width ) {
                // Scroll to the left, because there's space on the right
                m_hScroll = widthUsed - width + 1;
            }
        }
        QPoint topLeft = cr.topLeft() - QPoint( m_hScroll, 0 );

        // Set formats for a selection
        QVector<QTextLayout::FormatRange> formats;
        if ( hasSelectedText() ) {
            QTextLayout::FormatRange selection;
            selection.format.setBackground( palette().highlight() );
            selection.format.setForeground( palette().highlightedText() );
            selection.start = selectionStart();
            selection.length = selectedText().length();
            formats << selection;
        }

        // Draw text and cursor
        block.layout()->drawCursor( &p, topLeft, cursorPosition() );

        // The clipping of QTextLayout::draw doesn't work with no selection
        p.setClipRect( cr );
        // Get text width
        int textWidth = 0;
        QTextBlock block = m_doc.findBlockByNumber( 0 );
        if ( block.isValid() ) {
            textWidth = block.layout()->boundingRect().width();
        }
        int availableWidth = cr.width() + m_hScroll;
        if ( isClearButtonShown() ) {
            availableWidth -= clearButtonUsedSize().width();
        }
        if ( m_hScroll > 0 || textWidth > availableWidth ) {
            int fadeArea = 20;

            QPixmap pix( cr.size() );
            pix.fill( Qt::transparent );
            QPainter pixPainter( &pix );

            // Draw text
            block.layout()->draw( &pixPainter, QPoint( -m_hScroll, 0 ), formats,
                                QRect( 0, 0, cr.width(), cr.height() ) );

            // Draw fade out rects
            pixPainter.setCompositionMode( QPainter::CompositionMode_DestinationIn );
            if ( m_hScroll > 0 ) {
                QLinearGradient alphaGradient( 0, 0, 1, 0 );
                alphaGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
                if ( isLeftToRight() ) {
                    alphaGradient.setColorAt( 0, Qt::transparent );
                    alphaGradient.setColorAt( 1, Qt::black );
                } else {
                    alphaGradient.setColorAt( 0, Qt::black );
                    alphaGradient.setColorAt( 1, Qt::transparent );
                }

                pixPainter.fillRect( QRect( 0, 0, fadeArea, cr.height() ), alphaGradient );
            }

            if ( textWidth > width + m_hScroll ) {
                QLinearGradient alphaGradient( 0, 0, 1, 0 );
                alphaGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
                if ( isLeftToRight() ) {
                    alphaGradient.setColorAt( 0, Qt::black );
                    alphaGradient.setColorAt( 1, Qt::transparent );
                } else {
                    alphaGradient.setColorAt( 0, Qt::transparent );
                    alphaGradient.setColorAt( 1, Qt::black );
                }

                if ( isClearButtonShown() ) {
                    fadeArea += clearButtonUsedSize().width();
                }
                pixPainter.fillRect( QRect( cr.width() - fadeArea, 0, fadeArea, cr.height() ), alphaGradient );
            }

            pixPainter.end();
            p.drawPixmap( cr, pix );
        } else {
            block.layout()->draw( &p, topLeft, formats, cr );
        }
    }
}

QRect JourneySearchLineEdit::lineEditContents() const
{
    QStyleOptionFrameV2 opt;
    initStyleOption( &opt );
    QRect cr = style()->subElementRect( QStyle::SE_LineEditContents, &opt, this );
    cr.setLeft( cr.left() + 2 );
    cr.setRight( cr.right() - 2 );
    cr.setTop(( height() - cr.height() ) / 2 + 1 );
    return cr;
}
