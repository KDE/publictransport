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

/** @file
 * @brief This file contains the widget that is used to display journey search suggestions.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef JOURNEYSEARCHSUGGESTIONWIDGET_H
#define JOURNEYSEARCHSUGGESTIONWIDGET_H

#include <Plasma/TreeView>
#include <QModelIndex>

namespace Plasma {
	class LineEdit;
}
class Settings;
class QStandardItemModel;

/**
 * @brief Represents the widget used to display journey search suggestions.
 **/
class JourneySearchSuggestionWidget : public Plasma::TreeView
{
	Q_OBJECT

public:
	/**
	 * @brief Creates a new journey search suggestion widget.
	 *
	 * @param settings A pointer to the settings object of the applet.
	 * @param palette The palette to use. Defaults to QPalette().
	 **/
	JourneySearchSuggestionWidget( Settings *settings, const QPalette &palette = QPalette() );

	/**
	 * @brief Attaches the given @p lineEdit with this widget. All changes made to the text in
	 *   @p lineEdit are handled to generate suggestions.
	 *
	 * @param lineEdit A pointer to the Plasma::LineEdit to attach with.
	 * @see detachLineEdit
	 **/
	void attachLineEdit( Plasma::LineEdit *lineEdit );
	/**
	 * @brief Detaches a previously attached line edit widget.
	 *
	 * @see attachLineEdit
	 **/
	void detachLineEdit();

	/** @brief Clears the suggestion model. */
	void clear();

signals:
	/** @brief A suggestion has been activated, eg. by a double click. */
	void suggestionActivated();

	/**
	 * @brief Emitted after the attached line edit has been edited and it's content has been parsed.
	 *
	 * @param stopName The parsed stop name.
	 * @param departure The parsed departure date and time.
	 * @param stopIsTarget Wether or not the parsed stop should be treated as target (true)
	 *   or as origin stop (false).
	 * @param timeIsDeparture Wether or not the parsed time should be treated as departure (true)
	 *   or as arrival time (false).
	 **/
	void journeySearchLineChanged( const QString &stopName, const QDateTime &departure,
								   bool stopIsTarget, bool timeIsDeparture );

public slots:
	/**
	 * @brief Uses the stop suggestion at the given @p index.
	 *
	 * It is handled as if the stop suggestion was clicked.
	 * Only if the item at the given @p index is a stop suggestion or a recent journey search.
	 * @param index The QModelIndex of the stop suggestion to use.
	 **/
	void useStopSuggestion( const QModelIndex &index );
	/** @brief Updates the stop suggestions with stop suggestions in @p stopSuggestionData.
	 *
	 * @param stopSuggestionData Data with stop suggestions from the PublicTransport data engine.
	 **/
	void updateStopSuggestionItems( const QVariantHash &stopSuggestionData );

protected slots:
	/**
	 * @brief A suggestion item was clicked.
	 *
	 * @param index The QModelIndex of the clicked item.
	 **/
	void suggestionClicked( const QModelIndex &index );
	/**
	 * @brief A suggestion item was doubleclicked.
	 *
	 * @param index The QModelIndex of the clicked item.
	 **/
	void suggestionDoubleClicked( const QModelIndex &index );
	/**
	 * @brief The journey search line edit has been edited.
	 *
	 * @param newText The new text.
	 **/
    void journeySearchLineEdited( const QString &newText );

protected:
	/** @brief Removes all general suggestion items, ie. no stop suggestion items. */
	void removeGeneralSuggestionItems();
	/** @brief Add general completions, eg. "in X minutes". */
	void addJourneySearchCompletions();
	/** @brief Add stop suggestions given in @p stopSuggestions. */
    void addStopSuggestionItems( const QStringList &stopSuggestions );
	void addAllKeywordAddRemoveItems();
	void maybeAddKeywordAddRemoveItems( const QStringList &words, const QStringList &keywords,
										const QString &type, const QStringList &descriptions,
										const QStringList &extraRegExps = QStringList() );

	void journeySearchItemCompleted( const QString &newJourneySearch,
									 const QModelIndex &index = QModelIndex(), int newCursorPos = -1 );

private:
	QStandardItemModel *m_model;
	Settings *m_settings;
	Plasma::LineEdit *m_lineEdit;

	int m_journeySearchLastTextLength; /**< The last number of unselected characters in the
			* journey search input field. */
	bool m_lettersAddedToJourneySearchLine; /**< Whether or not the last edit of the
			* journey search line added letters o(r not. Used for auto completion. */
};

#endif // JOURNEYSEARCHSUGGESTIONWIDGET_H
