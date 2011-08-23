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

/** @file
 * @brief This file contains the widget that is used to display journey search suggestions.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef JOURNEYSEARCHSUGGESTIONWIDGET_H
#define JOURNEYSEARCHSUGGESTIONWIDGET_H

#include "journeysearchparser.h"

#include <Plasma/ScrollWidget>

#include <QGraphicsWidget>
#include <QModelIndex>

class Settings;
class JourneySearchSuggestionItem;
namespace Parser {
    class JourneySearchKeywords;
    class JourneySearchAnalyzer;
    class Results;
};
namespace Plasma {
    class LineEdit;
};
class QStandardItemModel;
class QGraphicsSceneMouseEvent;

/**
 * @brief Represents the widget used to display journey search suggestions.
 *
 * Derived from Plasma::ScrollWidget, shows suggestions. Suggestions are automatically added when
 * the attached line edit widget is edited. To attach a line edit widget use @ref attachLineEdit.
 * Completions are also automatically set on the attached line edit and it's text is updated when
 * a suggestion is applied.
 *
 * By default all available suggestions are shown. To disable suggestions by type use
 * @ref setEnabledSuggestions.
 *
 * @since 0.9.1
 **/
class JourneySearchSuggestionWidget : public Plasma::ScrollWidget
{
    Q_OBJECT
    friend class JourneySearchSuggestionItem;

public:
    /**
     * @brief Types of suggestions displayed by this widget.
     **/
    enum Suggestion {
        NoSuggestions = 0x0000, /**< No suggestions. */

        StopNameSuggestion = 0x0001, /**< A stop name suggestion. */
        RecentJourneySearchSuggestion = 0x0002, /**< A recent journey search suggestion. */
        KeywordSuggestion = 0x0004, /**< A keyword add/remove suggestion. */

        AllSuggestions = StopNameSuggestion | RecentJourneySearchSuggestion |
                KeywordSuggestion /**< All available suggestion types. */
    };
    Q_DECLARE_FLAGS(Suggestions, Suggestion);

    /**
     * @brief Creates a new journey search suggestion widget.
     *
     * @param parent The parent item.
     *
     * @param settings A pointer to the settings object of the applet.
     *
     * @param palette The palette to use. Defaults to QPalette().
     **/
    explicit JourneySearchSuggestionWidget( QGraphicsItem *parent, Settings *settings,
                                            const QPalette &palette = QPalette() );
    virtual ~JourneySearchSuggestionWidget();

    QModelIndex currentIndex() const;
    void setCurrentIndex( const QModelIndex &currentIndex );

    QStandardItemModel *model() const { return m_model; };
    void setModel( QStandardItemModel *model );

    /**
     * @brief Attaches the given @p lineEdit with this widget. All changes made to the text in
     *   @p lineEdit are handled to generate suggestions.
     *
     * @param lineEdit A pointer to the Plasma::LineEdit to attach with.
     *
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

    /** @brief Sets the types of suggestions to show to @p suggestions. */
    void setEnabledSuggestions( Suggestions suggestions = AllSuggestions ) {
        m_enabledSuggestions = suggestions; };

    /** @brief Gets the types of suggestions to show. */
    Suggestions enabledSuggestions() const { return m_enabledSuggestions; };

    const Parser::Results results() const;

signals:
    /** @brief A suggestion has been activated, eg. by a double click. */
    void suggestionActivated();

    /**
     * @brief Emitted after the attached line edit has been edited and it's content has been parsed.
     *
     * @param stopName The parsed stop name.
     *
     * @param departure The parsed departure date and time.
     *
     * @param stopIsTarget Whether or not the parsed stop should be treated as target (true)
     *   or as origin stop (false).
     *
     * @param timeIsDeparture Whether or not the parsed time should be treated as departure (true)
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
     *
     * @param index The QModelIndex of the stop suggestion to use.
     **/
    void useStopSuggestion( const QModelIndex &index );

    /** @brief Updates the stop suggestions with stop suggestions in @p stopSuggestionData.
     *
     * @param stopSuggestionData Data with stop suggestions from the PublicTransport data engine.
     **/
    void updateStopSuggestionItems( const QVariantHash &stopSuggestionData );

protected slots:
    virtual void rowsInserted( const QModelIndex &parent, int first, int last );
    virtual void rowsRemoved( const QModelIndex &parent, int first, int last );
    virtual void modelReset();
    virtual void layoutChanged();
    virtual void dataChanged( const QModelIndex &topLeft, const QModelIndex &bottomRight );

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

    void journeySearchLineSelectionChanged();
    void journeySearchLineCursorPositionChanged();

protected:
    QModelIndex indexFromItem( JourneySearchSuggestionItem *item );

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
    QList<JourneySearchSuggestionItem*> m_items;
    Settings *m_settings;
    Plasma::LineEdit *m_lineEdit;
    Suggestions m_enabledSuggestions;
    Parser::JourneySearchAnalyzer *m_journeySearchAnalyzer;
    Parser::JourneySearchKeywords *m_journeySearchKeywords;

    int m_journeySearchLastTextLength; /**< The last number of unselected characters in the
            * journey search input field. */
    bool m_lettersAddedToJourneySearchLine; /**< Whether or not the last edit of the
            * journey search line added letters o(r not. Used for auto completion. */
};
Q_DECLARE_OPERATORS_FOR_FLAGS(JourneySearchSuggestionWidget::Suggestions)

/**
 * @brief A QGraphicsWidget representing a single suggestion in a JourneySearchSuggestionWidget.
 *
 * It draws the icon stored in role Qt::DecorationRole and HTML code in Qt::DisplayRole gets
 * drawn using a QTextDocument.
 */
class JourneySearchSuggestionItem : public QGraphicsWidget
{
    Q_OBJECT
    friend class JourneySearchSuggestionWidget;

public:
    JourneySearchSuggestionItem( JourneySearchSuggestionWidget *parent,
                                const QModelIndex& index );

    void updateTextLayout();
    void updateData( const QModelIndex& index );
    void setHtml( const QString &html );
    QModelIndex index();

    void setInitialized() { m_initializing = false; };

    virtual void paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                        QWidget* widget = 0 );

signals:
    void suggestionClicked( const QModelIndex &index );
    void suggestionDoubleClicked( const QModelIndex &index );

protected:
    virtual void resizeEvent( QGraphicsSceneResizeEvent* event );
    virtual QSizeF sizeHint( Qt::SizeHint which, const QSizeF& constraint = QSizeF() ) const;
    virtual void mouseReleaseEvent( QGraphicsSceneMouseEvent* event );
    virtual void mouseDoubleClickEvent( QGraphicsSceneMouseEvent* event );

private:
    QTextDocument *m_textDocument;
    JourneySearchSuggestionWidget *m_parent;
// 	const QAbstractItemModel *m_model;
// 	int m_row;
    bool m_initializing;
};

#endif // JOURNEYSEARCHSUGGESTIONWIDGET_H
