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

#ifndef JOURNEYSEARCHLINEEDIT_HEADER
#define JOURNEYSEARCHLINEEDIT_HEADER

#include <KLineEdit>

#include <QSyntaxHighlighter>
#include <QTextDocument>

/** Highlights journey search keywords, values and the stop name if it is double quoted. */
class JourneySearchHighlighter : public QSyntaxHighlighter {
public:
	JourneySearchHighlighter( QTextDocument* parent );

	/** Format for the double quoted stop name (actually for every double quoted string). */
	QTextCharFormat &formatStopName() { return m_formatStopName; };
	/** Format for keywords. */
	QTextCharFormat &formatKeyword() { return m_formatKeyword; };
	/** Format for values of keywords. */
	QTextCharFormat &formatValue() { return m_formatValue; };
	/** Format for syntax errors, eg. a keyword which is already in the string
	* but only allowed once.
	* @note Not all syntax error are noticed currently. */
	QTextCharFormat &formatError() { return m_formatError; };

protected:
	/** @return The number of matched keywords. */
	int highlightKeywords( const QString &text, const QStringList &keywords,
			const QTextCharFormat &format, int maxAllowedOccurances = -1, int needsToStartAt = -1 );
	/** @return The number of matched keyword value combinations. */
	int highlightCombinations( const QString &text, const QStringList &keywords,
			const QStringList &keywordValues, const QTextCharFormat &format,
			int maxAllowedOccurances = -1, int needsToStartAt = -1 );

	virtual void highlightBlock( const QString& text );

private:
	QTextCharFormat m_formatStopName, m_formatKeyword, m_formatValue, m_formatError;
};

/** A KLineEdit with syntax highlighting. It uses @ref JourneySearchHighlighter
* but it could be replaced by any other QSyntaxHighlighter.
* Mouse events are reimplemented, to get correct positions in the highlighted
* QTextDocument. Some things are missing, like triple click or double click
* and mouse move to select more words (but one double click on a word works). */
class JourneySearchLineEdit : public KLineEdit {
	Q_OBJECT

public:
	JourneySearchLineEdit( QWidget* parent = 0 );
	explicit JourneySearchLineEdit( const QString &string, QWidget* parent = 0 );

	~JourneySearchLineEdit() { delete m_highlighter; };

protected slots:
	/** Sets the new text to the QTextDocument and highlights it. */
	void slotTextChanged( const QString &newText );

protected:
	/** Reimplemented to select the correct word in the QTextDocument. */
	virtual void mouseDoubleClickEvent( QMouseEvent *ev );
	/** Reimplemented to set the cursor to the correct position in the QTextDocument. */
	virtual void mousePressEvent( QMouseEvent *ev );
	/** Reimplemented to select the correct characters in the QTextDocument. */
	virtual void mouseMoveEvent( QMouseEvent *ev );
	/** Reimplemented to paint the highlighted QTextDocument. */
	virtual void paintEvent( QPaintEvent *ev );

	/** Does effectively almost the same as QLineEditPrivate::moveCursor(). */
	void moveCursor( int pos, bool mark );
	/** Gets the QRect in which the QTextDocument is drawn. */
	QRect lineEditContents() const;

private:
	void init();

	int m_hScroll, m_cursor; // contains values that are normally stored in QLineEditPrivate
	QTextDocument m_doc; // Used to draw the highlighted text
	JourneySearchHighlighter *m_highlighter; // The used QSyntaxHighlighter
};

#endif // Multiple inclusion guard
