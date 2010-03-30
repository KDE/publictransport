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
		: QSyntaxHighlighter( parent ) {
    m_formatStopName.setFontWeight( QFont::Bold );
    m_formatStopName.setForeground( Qt::darkMagenta );
    m_formatKeyword.setFontWeight( QFont::Bold );
    m_formatKeyword.setForeground( Qt::darkRed );
    m_formatValue.setForeground( Qt::blue );
    m_formatError.setFontItalic( true );
    m_formatError.setForeground( Qt::red );
}

void JourneySearchHighlighter::highlightKeywords( const QString& text,
		    const QStringList& keywords, const QTextCharFormat& format,
		    int maxAllowedOccurances, int needsToStartAt ) {
    QTextCharFormat curFormat = format, curKeywordFormat = m_formatKeyword;
    QRegExp expression( QString("\\b(%1)\\b").arg( keywords.join("|")), Qt::CaseInsensitive );
    int index = text.indexOf( expression );
    int count = 0;
    while ( index >= 0 ) {
        if ( needsToStartAt != -1 && index != needsToStartAt ) {
	    // The keyword doesn't start at needsToStartAt, if given
	    curFormat = m_formatError;
	    curKeywordFormat = m_formatError;
	}
	
        if ( expression.captureCount() >= 3 ) {
	    // Matched a keyword with it's value
	    setFormat( expression.pos(2), expression.cap(2).length(), curKeywordFormat );
            setFormat( expression.pos(3), expression.cap(3).length(), curFormat );
        } else
	    setFormat( index, expression.matchedLength(), curFormat );

	// Get next keyword
	index = text.indexOf( expression, index + expression.matchedLength() );

        ++count;
        if ( count == maxAllowedOccurances ) {
	    // Max occurance count of keywords reached
            curFormat = m_formatError;
            curKeywordFormat = m_formatError;
	}
    }
}

void JourneySearchHighlighter::highlightCombinations( const QString& text,
		const QStringList& keywords, const QStringList& keywordValues,
		const QTextCharFormat& format, int maxAllowedOccurances, int needsToStartAt ) {
    foreach ( const QString &keyword, keywords ) {
        foreach ( const QString &value, keywordValues ) {
            QString str = QString("(%1) (%2)").arg(keyword).arg(value);
            highlightKeywords( text, QStringList() << str, format, maxAllowedOccurances, needsToStartAt );
        }
    }
}

void JourneySearchHighlighter::highlightBlock( const QString& text ) {
    // Highlight keywords
    highlightKeywords( text, QStringList() << JourneySearchParser::toKeywords()
					   << JourneySearchParser::fromKeywords(),
                       m_formatKeyword, 1, 0 );
    highlightKeywords( text, QStringList() << JourneySearchParser::arrivalKeywords()
					   << JourneySearchParser::departureKeywords(),
                       m_formatKeyword, 1 );
    highlightKeywords( text, JourneySearchParser::timeKeywordsTomorrow(),
                       m_formatKeyword, 1 );

    // Highlight date/time keys and values 
    // ("[time]" or "[date]" or "[time], [date]" or "[date], [time]")
    highlightCombinations( text, JourneySearchParser::timeKeywordsAt(),
                           QStringList()
                           << "\\d{2}:\\d{2}(, \\d{2}\\.\\d{2}\\.(\\d{2,4})?)?"
                           << "\\d{2}:\\d{2}(, \\d{2}-\\d{2}(-\\d{2,4})?)?"
                           << "\\d{2}:\\d{2}(, (\\d{2,4}-)?\\d{2}-\\d{2})?"
                           << "\\d{2}\\.\\d{2}\\.(\\d{2,4})?(, \\d{2}:\\d{2})?"
                           << "\\d{2}-\\d{2}(-\\d{2,4})?(, \\d{2}:\\d{2})?"
                           << "(\\d{2,4}-)?\\d{2}-\\d{2}(, \\d{2}:\\d{2})?",
                           m_formatValue, 1 );

    // Highlight relative time keys and values
    highlightCombinations( text, JourneySearchParser::timeKeywordsIn(),
                           QStringList() << JourneySearchParser::relativeTimeString( "\\d{1,}" ),
                           m_formatValue, 1 );

    // Highlight stop name if it is inside double quotes
    QRegExp expression = QRegExp( "\\s?\"[^\"]*\"\\s?" );
    int index = text.indexOf( expression );
    while ( index >= 0 ) {
        int length = expression.matchedLength();
        setFormat( index, length, m_formatStopName );
        index = text.indexOf( expression, index + length );
    }
}

JourneySearchLineEdit::JourneySearchLineEdit( QWidget* parent ) : KLineEdit( parent ) {
    init();
}

JourneySearchLineEdit::JourneySearchLineEdit( const QString& string, QWidget* parent )
		    : KLineEdit( string, parent ) {
    init();
}

void JourneySearchLineEdit::init() {
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
    
    connect( this, SIGNAL(textChanged(QString)), this, SLOT(slotTextChanged(QString)) );
}

void JourneySearchLineEdit::slotTextChanged( const QString& newText ) {
    m_doc.setHtml( text() );
    m_doc.documentLayout();
}

void JourneySearchLineEdit::mouseDoubleClickEvent( QMouseEvent* ev ) {
    if ( ev->button() == Qt::LeftButton ) {
	deselect();
        QRect cr = lineEditContents();
        m_cursor = m_doc.documentLayout()->hitTest(
                       ev->posF() - cr.topLeft() + QPoint(m_hScroll, 0), Qt::FuzzyHit );

	QTextBlock block = m_doc.findBlockByNumber( 0 );
	if ( block.isValid() ) {
	    m_cursor = block.layout()->previousCursorPosition( m_cursor, QTextLayout::SkipWords );
	    int end = block.layout()->nextCursorPosition( m_cursor, QTextLayout::SkipWords );
	    
	    QString t = text();
	    while ( end > m_cursor && t[end-1].isSpace() )
		--end;
	    moveCursor( end, true );
	}
    } else
	KLineEdit::mouseDoubleClickEvent( ev );
}

void JourneySearchLineEdit::mousePressEvent( QMouseEvent* ev ) {
    if ( ev->button() == Qt::LeftButton ) {
        bool mark = ev->modifiers() & Qt::ShiftModifier;
        QRect cr = lineEditContents();
        m_cursor = m_doc.documentLayout()->hitTest(
                       ev->posF() - cr.topLeft() + QPoint(m_hScroll, 0), Qt::FuzzyHit );
        moveCursor( m_cursor, mark );
    } else
        KLineEdit::mousePressEvent( ev );
}

void JourneySearchLineEdit::mouseMoveEvent( QMouseEvent* ev ) {
    if ( ev->buttons().testFlag(Qt::LeftButton) ) {
        int cursor = 0;
        QRect cr = lineEditContents();
        cursor = m_doc.documentLayout()->hitTest(
                     ev->posF() - cr.topLeft() + QPoint(m_hScroll, 0), Qt::FuzzyHit );

        moveCursor( cursor, true );
    } else
        KLineEdit::mouseMoveEvent( ev );
}

void JourneySearchLineEdit::moveCursor( int pos, bool mark ) {
    if ( mark ) {
        setSelection( m_cursor, pos - m_cursor );
    } else {
        setCursorPosition( pos );
        update();
    }
}

void JourneySearchLineEdit::paintEvent( QPaintEvent* ) {
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
        QTextLine line = block.layout()->lineForTextPosition(
		block.layout()->isValidCursorPosition(cursorPos) ? cursorPos : cursorPos - 1 );
        if ( line.isValid() ) {
            // Horizontal scrolling
            int cix = line.cursorToX( cursorPos );
            int minLB = qMax( 0, -fontMetrics().minLeftBearing() );
            int minRB = qMax( 0, -fontMetrics().minRightBearing() );
            int widthUsed = line.width() + 1 + minRB;
            if ( (minLB + widthUsed) <=  cr.width() ) {
                switch ( alignment() ) {
                case Qt::AlignRight:
                    m_hScroll = widthUsed - cr.width();
                    break;
                case Qt::AlignHCenter:
                    m_hScroll = ( widthUsed - cr.width() ) / 2;
                    break;
                default:
                    m_hScroll = 0;
                    break;
                }
                m_hScroll -= minLB;
            } else if ( cix - m_hScroll >= cr.width() ) {
                m_hScroll = cix - cr.width() + 1;
            } else if ( cix - m_hScroll < 0 ) {
                m_hScroll = cix;
            } else if ( widthUsed - m_hScroll < cr.width() ) {
                m_hScroll = widthUsed - cr.width() + 1;
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
	p.setClipRect( cr ); // the clipping of QTextLayout::draw doesn't work with no selection
        block.layout()->draw( &p, topLeft, formats, cr );
    }
}

QRect JourneySearchLineEdit::lineEditContents() const {
    QStyleOptionFrameV2 opt;
    initStyleOption( &opt );
    QRect cr = style()->subElementRect( QStyle::SE_LineEditContents, &opt, this );
    cr.setLeft( cr.left() + 2 );
    cr.setRight( cr.right() - 2 );
    cr.setTop( (height() - cr.height()) / 2 + 1 );
    return cr;
}
