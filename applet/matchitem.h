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

#ifndef MATCHITEM_HEADER
#define MATCHITEM_HEADER

#include <QString>
#include <QVariant>
#include <QExplicitlySharedDataPointer>

#include "journeysearchenums.h"
#include "lexem.h"

namespace Parser {

class Lexem;
class MatchItemPrivate;
class MatchItem;
typedef QList<MatchItem> MatchItems;

/**
 * @brief Represents a terminal match item or a tree of matches tree, eg. with sequences/options.
 *
 * MatchItem objects are elements of the output list of SyntacticalAnalyzer and ContextualAnalyzer
 * and elements of the input list to ContextualAnalyzer.
 *
 * There are different types of match items (@ref MatchItem::Type), some of which have an
 * associated value (@ref MatchItem::value). To get the position in the input string use
 * @ref MatchItem::position. The matched part of the input string can be retrieved using
 * @ref MatchItem::input. To get a corrected input string, use @ref MatchItem::text. TODO
 *
 * Match items can have children, but terminal items don't have any children (@ref isTerminal).
 * That means that match items form a syntax tree (derived from the structure of the match item
 * tree, with terminal items at the leafs.
 * The tree of match items generated for an input string using a tree of @ref SyntaxItem objects
 * has a similiar tree structure, but without non-matching optional items and with KleeneStar /
 * KleenePlus unrolled. For example one SyntaxItem with a KleenePlus operator produces a sequence of
 * syntax items, one sequence item for each repeated match of the SyntaxItem.
 * The syntax items are filled with information about each match they produced. But this information
 * can be ambigous, if Kleene operators are used (making it impossible to generate a new input
 * string again). The produced tree of match items can be used to unambigously generate the input
 * string again, if the input was correct (otherwise errors may have been corrected).
 *
 * @em Conclusion: SyntaxItems are used to define a syntax and produce MatchItems on matches, while
 *   MatchItems are used to analyze the matches for a specific input string.
 *
 * The data of MatchItem objects is explicitly shared, so copying is fast and changes to the data
 * affect all associated match items.
 *
 * @see SyntaxItem
 * @see SyntacticalAnalyzer
 * @see ContextualAnalyzer
 **/
class MatchItem {
    Q_GADGET // To get the names of enumerables
public:
    Q_ENUMS( Type Flag )

    // Friends for calling the protected constructor
    friend class SyntacticalAnalyzer;
    friend class ContextualAnalyzer;
    friend class JourneySearchAnalyzer;
    friend class Results;

    friend class MatchItemPrivate;

    /**
    * @brief Types of match items.
    **/
    enum Type {
        Invalid = -1, /**< An invalid match item. Isn't used for invalid read match items (Error
                * is used for such cases), but for eg. arguments to functions to have a value that
                * isn't associated with a real match item. */
        Error = 0, /**< An error item. Get the error message with @ref text, the error severity
                * with @ref value (cast to @ref ErrorSeverity). Error items appear inside the
                * output list of match items at the position where the error appeared in the
                * input string. */
        Sequence, /**< A sequence item representing a match of a @ref MatchItemSequence. */
        Option, /**< An option item representing a match of a @ref MatchItemOption. */
        Keyword,
        Number,
        Character,
        String,
        Words
    };

    /**
     * @brief Flags for match items.
     **/
    enum Flag { // TODO DEPRECATED?
        DefaultMatchItem   = 0x0000, /**< Default flag for nonterminal match items. */
        CorrectedMatchItem = 0x0001  /**< The match item didn't match, but was corrected by
                                        * changing the matched string. TODO */
    };
    Q_DECLARE_FLAGS( Flags, Flag );

    /**
     * @brief Constructs an invalid MatchItem.
     *
     * This is used for eg. QHash as default value.
     **/
    MatchItem();

    MatchItem( const MatchItem &other );
    virtual ~MatchItem();
    MatchItem &operator=( const MatchItem &other );

    /**
     * @brief The type of this match item.
     *
     * @see Type
     **/
    Type type() const;

    /**
     * @brief Flags of this match item.
     *
     * For example corrected match items have the flag @ref CorrectedSyntaxItem set.
     *
     * @see Flags
     **/
    Flags flags() const;

    /**
     * @brief The original text of this match item in the input string.
     *
     * For keywords, it contains only the used keyword translation. Keywords with a value (eg.
     * @ref KeywordTimeIn or @ref KeywordTimeAt) the value text isn't contained in the returned
     * string. To get the value use @ref value, eg. to construct an output string again.
     * If @ref type returns @ref StopName, this method returns the stop name, without quotation
     * marks, if there are any in the input string.
     * For error items (@ref Error), this contains the original errornous string.
     **/
    QString input() const;

    QString text( AnalyzerCorrections appliedCorrections = CorrectEverything ) const;

    /**
     * @brief The value of this match item, if there is any.
     *
     * This only returns a valid QVariant, if there is a value associated with this match item.
     * For example @ref KeywordTimeIn has the number of minutes from now stored as an integer and
     * @ref KeywordTimeAt has the time stored as a QTime.
     * For @ref Error this is the error message (QString).
     **/
    QVariant value() const;

    JourneySearchValueType valueType() const;

    bool isTerminal() const;

    /** @brief The position of this match item in the input string. */
    int position() const;

    /** @brief Wether or not there are any errors in the input string for this match item. */
    bool isErrornous() const;

    /**
     * @brief Wether or not this is a valid match item, that has been read from the input string.
     *
     * Invalid match items are returned by QHash if trying to get an item, which isn't in the hash.
     **/
    bool isValid() const;

    /**
     * @brief Gets the name for the given @p keywordType.
     *
     * This is used for error messages, eg. "Keyword 'name' isn't allowed here".
     **/
    static QString keywordId( KeywordType keywordType );

    const LexemList lexems() const;

    QString toString( int level = 0 ) const;

protected:
    /**
     * @brief Constructs a new match item object.
     *
     * @param type The type of the match item. Default is Invalid.
     * @param lexems A list of the matched lexems for this match item, ie. unchanged input. Don't
     *   include values of keywords here. Default is an empty LexemList.
     * @param valueType The type of the value of this match item, ie. what to do with the value.
     *   For example the value can contain a time value: @ref TimeValue, which can be retrieved
     *   from child items with types @ref TimeHourValue and @ref TimeMinuteValue. Default is
     *   NoValue.
     * @param value A value associated with this match item or an invalid QVariant, if there is
     *   no value associated, ie. @p valueType is NoValue. Default is QVariant().
     * @param flags Flags for the match item, eg. @ref CorrectedMatchItem.
     *   Default is DefaultMatchItem.
     **/
    MatchItem( Type type, const LexemList &lexems = LexemList(), // TODO docu
               JourneySearchValueType valueType = NoValue, const QVariant &value = QVariant(),
               Flags flags = DefaultMatchItem );

    MatchItem( const LexemList &lexems, KeywordType keyword, const QString &completedKeyword,
               Flags flags = DefaultMatchItem );

    void addChild( const MatchItem &matchItem );
    void addChildren( const MatchItems &matchItems );
    const MatchItems children() const;
    MatchItems &children();
    const MatchItems allChildren() const;

    void setValue( const QVariant &value ) const;
    const Lexem firstLexem() const;

private:
    QExplicitlySharedDataPointer<MatchItemPrivate> d;
};

inline bool operator <( const MatchItem &matchItem1, const MatchItem &matchItem2 ) {
    return matchItem1.position() < matchItem2.position() ||
           matchItem1.type() == MatchItem::Error;
};

QDebug &operator <<( QDebug debug, MatchItem::Type type );

bool operator ==( const MatchItem &l, const MatchItem &r );

} // namespace

#endif // MATCHITEM_HEADER
