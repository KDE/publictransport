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

#ifndef SYNTAXITEM_HEADER
#define SYNTAXITEM_HEADER

#define QT_SHAREDPOINTER_TRACK_POINTERS
#include <QPointer>
#include <QStringList>
#include <QLinkedList>
#include <QVariant>
#include <KDebug>

#include "journeysearchenums.h"
#include "lexem.h"

// TODO? The comma operator doesn't work.. it changes the order of sequence items? TEST verify this
#ifndef MATCH_SEQUENCE_CONCATENATION_OPERATOR
    #define MATCH_SEQUENCE_CONCATENATION_OPERATOR +
#endif

// Uncomment to print out debug messages when sequence/option operators are executed
// #define DEBUG_SYNTAXITEM_OPERATORS

// Uncomment to print out debug messages when sequence/option operators are executed
// with the full information about the left/right items
// #define DEBUG_SYNTAXITEM_OPERATORS_FULL

// Uncomment to use SyntaxItem::matches() & co. functions
// Doesn't impair performance a lot, but uses some more space. Can be useful for debugging
// #define USE_MATCHES_IN_SYNTAXITEM

// Uncomment to include pointers to SyntaxItems in SyntaxItem::toString()
// and to print out debug messages when deleting SyntaxItems
// #define DEBUG_SYNTAXITEM_PARENTS

namespace Parser {

class SyntacticalAnalyzer;
class MatchItem;

template <typename SyntaxItemType>
class TSyntaxItemPointer;

class SyntaxItem;
class SyntaxSequenceItem;
class SyntaxOptionItem;
class SyntaxKeywordItem;
typedef TSyntaxItemPointer<SyntaxItem> SyntaxItemPointer;
typedef TSyntaxItemPointer<SyntaxSequenceItem> SyntaxSequencePointer;
typedef TSyntaxItemPointer<SyntaxOptionItem> SyntaxOptionPointer;
typedef TSyntaxItemPointer<SyntaxKeywordItem> SyntaxKeywordPointer;

/**
 * @brief Type for lists of MatchItemPointer objects.
 *
 * Using a QLinkedList instead of a QList saves ~30% time when constructing a new syntax definition
 * with sequences and options. This is because QLinkedList is a bit faster with appending new items,
 * which is what is done for each operator call that adds a new item to a sequence/option list.
 *
 * No other changes are made on the sequence/option lists, only appending using the << operator.
 * Reading the lists is done with a constant iterator, mostly incrementing, but sometimes also
 * decrementing (after a test for the match of the next item wasn't successful).
 *
 * TODO
 **/
typedef QLinkedList<SyntaxItemPointer> SyntaxItems;

/**
 * @brief A pointer class for SyntaxItem objects with special operators.
 *
 * This class only exists to have a pointer type to SyntaxItem objects with operators to combine
 * SyntaxItem objects to sequences/options. Most functions are copied from QPointer, but without
 * guarding the pointer.
 *
 * This class makes it easy to define sequences/options of SyntaxItems, eg.:
 * @code
 *   SyntaxSequencePointer sequence = SyntaxItemPointer(new SyntaxNumberItem(0, 23))
 *          + SyntaxItemPointer(new SyntaxLexemItem(':') + SyntaxItemPointer(new SyntaxNumberItem(0, 59));
 * @endcode
 * Note the use of typedefs, like @verbatim MatchItemPointer @endverbatim for
 * @verbatim TMatchItemPointer<MatchItem> @endverbatim.
 *
 * To make it even easier there is a class @ref Match, which returns MatchItemPointer objects
 * for all match types. The above code example can be written like this:
 * @code
 *   SyntaxSequencePointer sequence = Match::number(0, 23) + Match::lexem(':') + Match::number(0, 59);
 * @endcode
 *
 * As you might have guessed, this matches time strings like "15:45".
 **/
template <typename SyntaxItemType>
class TSyntaxItemPointer : public QPointer<SyntaxItemType> {
public:
//     inline TSyntaxItemPointer()
//             : QPointer<SyntaxItemType>()/*, m_item(0)*/ {};
//     inline TSyntaxItemPointer( SyntaxItemType *p ) : m_item(p) {
//     };
//     virtual ~TSyntaxItemPointer() { delete m_item; };
    inline TSyntaxItemPointer( SyntaxItemType *p )
            : QPointer<SyntaxItemType>(p) {};
//     inline TSyntaxItemPointer( SyntaxItemType *p )
//             : QPointer<SyntaxItemType>(p) {};
//     inline TSyntaxItemPointer( const TSyntaxItemPointer &p )
//             : QPointer<SyntaxItemType>(p) {};

//     inline TSyntaxItemPointer &operator=( const TSyntaxItemPointer &p )
//         { if (this != &p) m_item = p.m_item; return *this; };
//     inline TSyntaxItemPointer &operator=( SyntaxItemType *p )
//         { if (m_item != p) m_item = p; return *this; };
//
//     inline bool isNull() const { return !m_item; };
//
//     inline SyntaxItemType* operator->() const { return m_item; };
//     inline SyntaxItemType& operator*() const { return *m_item; };
//     inline SyntaxItemType* data() const { return m_item; };
//
//     inline operator SyntaxItemType*() const { return m_item; };
//     inline operator SyntaxItemPointer() const { return m_item; };

    // Conversion to pointers to other SyntaxItem types.
    // Needs to be able to qobject_cast between these types,
    // except for cast to SyntaxSequencePointer (can cast every SyntaxItem into a SyntaxSequenceItem)
    template <typename Other>
    operator TSyntaxItemPointer<Other>() const;

    SyntaxSequencePointer operator MATCH_SEQUENCE_CONCATENATION_OPERATOR( SyntaxItemPointer item );
    SyntaxOptionPointer operator |( SyntaxItemPointer item );

    /** @brief The decrement operator is a shortcut for @ref optional. */
//     inline SyntaxItemPointer operator --() { return --*m_item; };
    inline SyntaxItemPointer operator --() { return --*QPointer<SyntaxItemType>::data(); };

    /** @brief The increment operator is a shortcut for @ref required. */
//     inline SyntaxItemPointer operator ++() { return ++*m_item; };
    inline SyntaxItemPointer operator ++() { return ++*QPointer<SyntaxItemType>::data(); };

// private:
//     SyntaxItemType *m_item;
};

/**
 * @brief Base class for all syntax objects.
 *
 * Contains information about what and how to match and where to put the result.
 * SyntaxItems can be used to create a parser, which parses a list of @ref Lexem objects and outputs
 * a tree of SyntaxItem objects. Output is only generated for SyntaxItems which have an
 * output type defined (by default an output type other than @ref Invalid is used). Except for
 * @ref SyntaxItemSequence and @ref SyntaxItemOption, which always create an output @ref SyntaxItem
 * with type @ref SytaxItem::Sequence / @ref SyntaxItem::Option and add the inner matches as childs
 * to that output item.
 *
 * To construct a SyntaxItem you can use one of the constructors of the derived classes or
 * use one of the static functions of the class @ref Syntax (recommended).
 * The options of a SyntaxItem can be changed on the fly, using several functions which change flags
 * and return a SyntaxItemPointer. These functions can be used to make syntax definitions easier
 * to read: @ref optional / @ref required, @ref kleeneStar or @ref star, @ref kleenePlus or
 * @ref plus, @ref greedy / @ref nonGreedy, @ref error / @ref ok, @ref outputTo / @ref noOutput.
 * The function @ref setFlag / @ref unsetFlag can also be used
 *
 * The operators + and | can be used to combine two SyntaxItemPointers in a sequence / option.
 * These operators are defined for SyntaxItems and SyntaxItemPointers for easy usage. If you want
 * to make the code even shorter, you can use the increment (required) or decrement (optional)
 * operators to make an item required or optional. By default syntax items are required.
 *
 * A syntax definition can then look like this (this example matches time strings or only the hour):
 * @code
 * typedef Syntax S; // A bit shorter syntax definition
 * SyntaxItemPointer syntaxItem =
 *      S::number(0, 23) + S::colon()->optional() + S::number(0, 59)->optional()
 * @endcode
 *
 * @note If a SyntaxItem has no parent in a constructor or a sequence/option operator, it's parent
 *   gets set to the new parent SyntaxItem. For example each option item in a SyntaxOptionItem
 *   get's the SytnaxOptionItem as parent (if the item didn't already have a parent). That way
 *   all SyntaxItem trees constructed with the sequence/option operators can be easily deleted
 *   by deleting the root item.
 *   If a SyntaxItem gets stored in a variable to be used at multiple places in another SyntaxItem,
 *   the parent of these items is set the item where they were first added.
 **/
class SyntaxItem : public QObject {
    Q_OBJECT
    Q_ENUMS(Type Flag)

public:
    friend class SyntacticalAnalyzer;

    /**
     * @brief Available match types, each type matches a specific input.
     *
     * For each type (except for MatchNothing) there is a class derived from SyntaxItem.
     * The names of the classes associated with these types are almost equal to the type names,
     * but they contain "Item" after "Match" at the beginning, eg. "MatchNumber" => "MatchItemNumber".
     **/
    enum Type {
        MatchNothing = 0, /**< Don't match anything, matching always succeeds. */

        // Non-terminals
        MatchSequence, /**< Match a sequence of SyntaxItem objects (SyntaxSequenceItem). */
        MatchOption, /**< Match one of a list of SyntaxItem objects (SyntaxOptionItem). */
        MatchKeyword, /**< Match a keyword, can have a value (SyntaxKeywordItem). */

        // Terminals
        MatchCharacter, /**< Match a special character (SyntaxCharacterItem). */
        MatchNumber, /**< Match a number. Limits for the number can be defined.
                * Currently only positive numbers are supported (SyntaxNumberItem). */
        MatchString, /**< Match a specific string. (SyntaxStringItem). */
        MatchWords /**< Match one or more arbitrary words. The lexem types to be used can be
                * defined. By default strings, numbers and spaces are read as words. If a lexem is
                * read which type isn't in the list or if the given maximum word count is reached,
                * the word matching stops (SyntaxWordsItem). */
    };

    /**
     * @brief Flags for SyntaxItem objects.
     **/
    enum Flag {
        DefaultMatch     = 0x0000, /**< A match is required (non-optional) and non-greedy.
                                     * Non-greedy means, that matching stops,
                                     * if the next SyntaxItem matches. */
        MatchIsOptional  = 0x0001, /**< A match isn't required, ie. it is optional. */
        MatchGreedy      = 0x0002, /**< Match greedy, ie. until a lexem doesn't match. */
        KleenePlus       = 0x0004, /**< Match multiple times, but at least once. */
        KleeneStar       = KleenePlus | MatchIsOptional, /**< Match multiple times or don't match.
                                     * This is an optional KleenePlus. */
        MatchIsErrornous = 0x0008  /**< The match is errornous. Errornous match items can be used
                                     * to make the syntax more flexible, and know if the match went
                                     * over an errornous syntax item. */ // TODO
    };
    Q_DECLARE_FLAGS( Flags, Flag );

    #ifdef USE_MATCHES_IN_SYNTAXITEM
        /** @brief Contains information about one match of a SyntaxItem in an input string. */
        struct MatchData {
            int position;
            QString input;
            QVariant value;

            MatchData( int position, const QString &input, const QVariant &value ) {
                this->position = position;
                this->input = input;
                this->value = value;
            };

            const QString toString() const {
                return QString("Match at %1, input: \"%2\", value: %3")
                    .arg(position).arg(input)
                    .arg(value.canConvert(QVariant::StringList)
                        ? value.toStringList().join(", ") : value.toString());
            };
        };
    #endif

    virtual ~SyntaxItem();

    inline JourneySearchValueType valueType() const { return m_valueType; };
    inline bool hasValue() const { return m_valueType != NoValue; };

    inline Type type() const { return m_type; };
    inline bool isOfType( Type type ) const { return m_type == type; };

    /** @brief Wether or not this item is optional. */
    inline bool isOptional() const { return m_flags.testFlag(MatchIsOptional); };

    /** @brief Wether or not matching is done greedy for this item. */
    inline bool isGreedy() const { return m_flags.testFlag(MatchGreedy); };

    /** @brief Gets the flags of this item. */
    inline Flags flags() const { return m_flags; };

    /** @brief Sets @p flag and returns a pointer to this SyntaxItem. */
    SyntaxItemPointer setFlag( Flag flag ) {
        if ( !m_flags.testFlag(flag) ) {
            m_flags |= flag;
        }
        return SyntaxItemPointer( this );
    };

    /** @brief Unsets @p flag and returns a pointer to this SyntaxItem. */
    SyntaxItemPointer unsetFlag( Flag flag ) {
        m_flags &= ~flag;
        return SyntaxItemPointer( this );
    };

    /**
     * @brief Sets the @ref KleeneStar flag and returns a pointer to this SyntaxItem.
     * @see star
     **/
    inline SyntaxItemPointer kleeneStar() { return setFlag(KleeneStar); };

    /** @brief Shortcut for @ref kleeneStar. */
    inline SyntaxItemPointer star() { return kleeneStar(); };

    /**
     * @brief Sets the @ref KleenePlus flag and returns a pointer to this SyntaxItem.
     * @see plus
     **/
    inline SyntaxItemPointer kleenePlus() { return setFlag(KleenePlus); };

    /** @brief Shortcut for @ref kleenePlus. */
    inline SyntaxItemPointer plus() { return kleenePlus(); };

    /** @brief Sets the @ref MatchGreedy flag and returns a pointer to this SyntaxItem. */
    inline SyntaxItemPointer greedy() { return setFlag(MatchGreedy); };

    /** @brief Unsets the @ref MatchGreedy flag and returns a pointer to this SyntaxItem. */
    inline SyntaxItemPointer nonGreedy() { return unsetFlag(MatchGreedy); };

    /** @brief Sets the @ref MatchIsOptional flag and returns a pointer to this SyntaxItem. */
    inline SyntaxItemPointer optional() { return setFlag(MatchIsOptional); };

    /**
     * @brief Unsets the @ref MatchIsOptional flag and returns a pointer to this SyntaxItem.
     *
     * @note By default all match items are already required.
     * @see optional
     **/
    inline SyntaxItemPointer required() { return unsetFlag(MatchIsOptional); };

    /** @brief Sets the @ref MatchIsErrornous flag and returns a pointer to this SyntaxItem. */
    inline SyntaxItemPointer error() { return setFlag(MatchIsErrornous); };

    /** @brief Unsets the @ref MatchIsErrornous flag and returns a pointer to this SyntaxItem. */
    inline SyntaxItemPointer ok() { return unsetFlag(MatchIsErrornous); };


    /** @brief Associates the value of this SyntaxItem with @p valueType and returns a pointer
     *  to this MatchItem. */
    inline SyntaxItemPointer outputTo( JourneySearchValueType valueType ) {
            m_valueType = valueType; return this; };

    /** TODO @brief Makes this SyntaxItem not use any value and returns a pointer to this SyntaxItem. */
    inline SyntaxItemPointer noOutput() { return outputTo(NoValue); };

    #ifdef USE_MATCHES_IN_SYNTAXITEM
    /**
     * @brief The number of matches since the last call to clearResults.
     *
     * Match items can match multiple times, if a Kleene star or plus is used.
     *
     * @see match
     * @see matches
     **/
    inline int matchCount() const { return m_matches.count(); };

    /** @brief Returns true, if this item has a match in the input string. False, otherwise. */
    inline bool hasMatch() const { return !m_matches.isEmpty(); };

    /**
     * @brief Gets the match with the given @p index.
     *
     * @param index The index of the match to get data for. Must be less than @ref matchCount.
     * @see matchCount
     **/
    inline MatchData match( int index ) const { return m_matches[index]; };

    /** @brief Gets all matches since the last call to clearResults. */
    inline QList<MatchData> matches() const { return m_matches; };

    /** @brief Clears the input strings and values for this item from the last parsing run. */
    virtual void clearResults() { m_matches.clear(); };
    #endif

    virtual QString toString( int level = 0 ) const;

    /** @brief The decrement operator is a shortcut for @ref optional. */
    inline SyntaxItemPointer operator --() { return optional(); };

    /** @brief The increment operator is a shortcut for @ref required. */
    inline SyntaxItemPointer operator ++() { return required(); };

protected:
    /**
     * @brief Creates a new MatchItem.
     *
     * @param type The type of the new MatchItem.
     * @param flags Flags for the new MatchItem. Default is DefaultMatch.
     * @param valueType The type of the value of the new MatchItem. Default is Other TODO.
     * @param parent The parent object.
     **/
    SyntaxItem( Type type, Flags flags = DefaultMatch,
                JourneySearchValueType valueType = NoValue, QObject *parent = 0 );

    #ifdef USE_MATCHES_IN_SYNTAXITEM
    /**
     * @brief Adds information about a match with this MatchItem.
     *
     * There can be multiple matches for an item, eg. because of a Kleene star.
     * @see clearResults
     **/
    void addMatch( int position, const QString &input, const QVariant &value ) {
        m_matches << MatchData( position, input, value );
    };

    void addMatch( const SyntaxItem &matchedItem );
    #endif

private:
    JourneySearchValueType m_valueType;
    Type m_type;
    Flags m_flags;
    #ifdef USE_MATCHES_IN_SYNTAXITEM
        QList<MatchData> m_matches;
    #endif
};

/**
 * @brief Matches a sequence of items.
 *
 * @see MatchItem
 **/
class SyntaxSequenceItem : public SyntaxItem {
    Q_OBJECT
public:
    SyntaxSequenceItem( const SyntaxItems &items = SyntaxItems(), Flags flags = DefaultMatch,
                       QObject *parent = 0 )
            : SyntaxItem(MatchSequence, flags, NoValue, parent), m_items(items)
    {
        for ( SyntaxItems::Iterator it = m_items.begin(); it != m_items.end(); ++it ) {
            if ( !(*it)->parent() ) {
                (*it)->setParent( this );
            }
        }
    };
//     SyntaxSequenceItem( SyntaxItemPointer item )
//             : SyntaxItem(MatchSequence), m_items(SyntaxItems() << item)
//     {
//         if ( !item->parent() ) {
//             item->setParent( this );
//         }
//     };
    SyntaxSequenceItem( SyntaxSequencePointer pointer )
            : SyntaxItem(MatchSequence, pointer->flags(), pointer->valueType()),
              m_items(*pointer->items())
    {};
    virtual ~SyntaxSequenceItem() { /*qDeleteAll(m_items);*/ };

    SyntaxItems *items() { return &m_items; };

    inline SyntaxSequenceItem *operator MATCH_SEQUENCE_CONCATENATION_OPERATOR( const SyntaxItemPointer &item );
    inline operator SyntaxItem*() { return this; };

    #ifdef USE_MATCHES_IN_SYNTAXITEM
    /** @brief Clears the input strings and values for this item from the last parsing run. */
    virtual void clearResults() {
        MatchItem::clearResults();
        foreach ( const MatchItemPointer &item, m_items ) {
            item->clearResults();
        }
    };
    #endif

    virtual QString toString( int level = 0 ) const;

private:
    SyntaxItems m_items; // TODO rename to SyntaxItemPointers?
};

/**
 * @brief Matches one of a set of option items.
 *
 * @see MatchItem
 **/
class SyntaxOptionItem : public SyntaxItem {
    Q_OBJECT
public:
    SyntaxOptionItem( const SyntaxItems &options = SyntaxItems(), Flags flags = DefaultMatch,
                      QObject *parent = 0 )
            : SyntaxItem(MatchOption, flags, NoValue, parent), m_options(options)
    {
        for ( SyntaxItems::Iterator it = m_options.begin(); it != m_options.end(); ++it ) {
            if ( !(*it)->parent() ) {
                (*it)->setParent( this );
            }
        }
    };
    SyntaxOptionItem( SyntaxItemPointer item )
            : SyntaxItem(MatchOption), m_options(SyntaxItems() << item)
    {
        if ( !item->parent() ) {
            item->setParent( this );
        }
    };
    virtual ~SyntaxOptionItem() { /*qDeleteAll(m_options);*/ };

    SyntaxItems *options() { return &m_options; };

    inline SyntaxOptionItem *operator |( const SyntaxItemPointer &item );
    inline operator SyntaxItem*() { return this; };

    #ifdef USE_MATCHES_IN_SYNTAXITEM
    /** @brief Clears the input strings and values for this item from the last parsing run. */
    virtual void clearResults() {
        SyntaxItem::clearResults();
        foreach ( const SyntaxItemPointer &option, m_options ) {
            option->clearResults();
        }
    };
    #endif

    virtual QString toString( int level = 0 ) const;

private:
    SyntaxItems m_options;
};

/**
 * @brief Matches a special character.
 **/
class SyntaxCharacterItem : public SyntaxItem {
    Q_OBJECT
public:
    SyntaxCharacterItem( char character, Flags flags = DefaultMatch,
                         JourneySearchValueType valueType = NoValue, QObject *parent = 0 )
            : SyntaxItem(MatchCharacter, flags, valueType, parent), m_character(character)
    {};

    char character() const { return m_character; };

private:
    const char m_character;
};

/**
 * @brief Matches one or more specific words.
 **/
class SyntaxStringItem : public SyntaxItem {
    Q_OBJECT
public:
    SyntaxStringItem( const QString &string, Flags flags = DefaultMatch,
                      JourneySearchValueType valueType = NoValue, QObject *parent = 0 )
            : SyntaxItem(MatchString, flags, valueType, parent), m_strings(QStringList() << string)
    {};
    SyntaxStringItem( const QStringList &strings, Flags flags = DefaultMatch,
                      JourneySearchValueType valueType = NoValue, QObject *parent = 0 )
            : SyntaxItem(MatchString, flags, valueType, parent), m_strings(strings)
    {};

    const QStringList strings() const { return m_strings; };

private:
    const QStringList m_strings;
};

/**
 * @brief Matches words.
 **/
class SyntaxWordsItem : public SyntaxItem {
    Q_OBJECT
public:
    SyntaxWordsItem( Flags flags = DefaultMatch, JourneySearchValueType valueType = NoValue,
                     int wordCount = -1, const QList<Lexem::Type> &wordTypes =
                        QList<Lexem::Type>() << Lexem::String << Lexem::Number << Lexem::Space,
                     QObject *parent = 0 )
            : SyntaxItem(MatchWords, flags, valueType, parent),
              m_wordCount(wordCount), m_wordTypes(wordTypes)
    {};

    int wordCount() const { return m_wordCount; };
    const QList<Lexem::Type> wordTypes() const { return m_wordTypes; };

    virtual QString toString( int level = 0 ) const;

private:
    const int m_wordCount;
    const QList<Lexem::Type> m_wordTypes;
};

/**
 * @brief Matches a keyword, with or without a value sequence.
 **/
class SyntaxKeywordItem : public SyntaxItem {
    Q_OBJECT
public:
    SyntaxKeywordItem( KeywordType keyword, SyntaxSequencePointer valueSequence = 0,
                       Flags flags = DefaultMatch, QObject *parent = 0 )
            : SyntaxItem(MatchKeyword, flags, NoValue, parent),
              m_keyword(keyword), m_valueSequence(valueSequence)
    {
        if ( valueSequence && !valueSequence->parent() ) {
            valueSequence->setParent( this );
        }
    };
    virtual ~SyntaxKeywordItem() { /*delete m_valueSequence;*/ };

    KeywordType keyword() const { return m_keyword; };
    SyntaxSequencePointer valueSequence() const { return m_valueSequence; };

    virtual QString toString( int level = 0 ) const;

private:
    const KeywordType m_keyword;
    SyntaxSequencePointer m_valueSequence;
};

/**
 * @brief Matches a number.
 **/
class SyntaxNumberItem : public SyntaxItem {
    Q_OBJECT
public:
    SyntaxNumberItem( int min = 1, int max = 9999999, Flags flags = DefaultMatch,
                      JourneySearchValueType valueType = NoValue, QObject *parent = 0 )
            : SyntaxItem(MatchNumber, flags, valueType, parent), m_min(min), m_max(max)
    {};

    int min() const { return m_min; };
    int max() const { return m_max; };

    virtual QString toString( int level = 0 ) const;

private:
    const int m_min;
    const int m_max;
};

/**
 * @brief Creates SyntaxItem objects wrapped by SyntaxItemPointer.
 *
 * Using the functions of this class, a syntax can be specified quite easily (also easy to read).
 * It is comparable to a subset of the syntax of regular expressions, where a question mark at the
 * end of an item gets replaced by @verbatimoptional()@endverbatim, a Kleene star gets replaced
 * by @verbatimkleeneStar()@endverbatim and a Kleene plus gets replaced by
 * @verbatimkleenePlus()@endverbatim. But it doesn't operate on strings but on lists of @p Lexem
 * objects.
 *
 * There is a static method @ref journeySearchSyntaxItem which creates a SyntaxItem that matches
 * journey searches. To change the syntax of the journey search, simply update the definition
 * of this method.
 *
 * A regular expression @verbatim\d+:?\d*@endverbatim can be written like this (with
 * @verbatim\d@endverbatim being replaced by whole numbers, since it operates of lexem lists,
 * not strings):
 * TODO Update this code segment: TODO
 * @code
 *   MatchNumber().kleenePlus() & MatchColon()->optional() & MatchNumber()->kleeneStar()
 * @endcode
 *
 * The @verbatim+@endverbatim operator concatenates two SyntaxItems into a new SyntaxSequenceItem.
 * Options can be created similarly using the @verbatim|@endverbatim operator, which creates a
 * new SyntaxOptionItem.
 * Another option to create sequences/options is to use @ref Syntax::sequence and @ref Syntax::option.
 * Or you can use the constructors of @ref SyntaxSequenceItem and @ref SyntaxOptionItem directly.
 *
 * @note The Kleene star/plus gets done by default using non-greedy matching, ie. it stops if the
 *   next SyntaxItem matches. You can make a SyntaxItem greedy on the fly by using
 *   @p SyntaxItem::greedy.
 **/
class Syntax {
public:
    static SyntaxItemPointer journeySearchSyntaxItem();

    static SyntaxItemPointer sequence( const SyntaxItems &items = SyntaxItems() )
        { return new SyntaxSequenceItem(items); };
    static SyntaxItemPointer option( const SyntaxItems &items = SyntaxItems() )
        { return new SyntaxOptionItem(items); };

    static SyntaxItemPointer keywordTo()
        { return new SyntaxKeywordItem(KeywordTo); };
    static SyntaxItemPointer keywordFrom()
        { return new SyntaxKeywordItem(KeywordFrom); };
    static SyntaxItemPointer keywordDeparture()
        { return new SyntaxKeywordItem(KeywordDeparture); };
    static SyntaxItemPointer keywordArrival()
        { return new SyntaxKeywordItem(KeywordArrival); };
    static SyntaxItemPointer keywordTomorrow()
        { return new SyntaxKeywordItem(KeywordTomorrow); };
    static SyntaxItemPointer keyword( KeywordType keyword, SyntaxItemPointer value )
        { return new SyntaxKeywordItem(keyword, value); };
    static SyntaxItemPointer keyword( KeywordType keyword, SyntaxSequencePointer valueSequence = 0 )
        { return new SyntaxKeywordItem(keyword, valueSequence); };

    static SyntaxItemPointer character( char ch )
        { return new SyntaxCharacterItem(ch); };
//     static MatchItemPointer colon()
//         { return new MatchItemColon; };
//     static MatchItemPointer quotationMark()
//         { return new MatchItemQuotationMark; };
    static SyntaxItemPointer number( int min = 1, int max = 9999999, JourneySearchValueType valueType = NoValue )
        { return new SyntaxNumberItem(min, max, SyntaxItem::DefaultMatch, valueType); };
    static SyntaxItemPointer words( int wordCount = -1, JourneySearchValueType valueType = NoValue,
            const LexemTypes &wordTypes = LexemTypes() << Lexem::String << Lexem::Number << Lexem::Space )
        { return new SyntaxWordsItem( SyntaxItem::DefaultMatch, valueType, wordCount, wordTypes ); };
    static SyntaxItemPointer oneWord(  JourneySearchValueType valueType = NoValue,
            const LexemTypes &wordTypes = LexemTypes() << Lexem::String << Lexem::Number << Lexem::Space )
        { return new SyntaxWordsItem( SyntaxItem::DefaultMatch, valueType, 1, wordTypes ); };
};

template <typename SyntaxItemType>
SyntaxSequencePointer TSyntaxItemPointer<SyntaxItemType>::operator MATCH_SEQUENCE_CONCATENATION_OPERATOR(
        SyntaxItemPointer item )
{
    if ( /*QPointer<SyntaxItemType>::data()*/QPointer<SyntaxItemType>::data()->type() == SyntaxItem::MatchSequence ) {
        return qobject_cast<SyntaxSequenceItem*>( QPointer<SyntaxItemType>::data()/*QPointer<SyntaxItemType>::data()*/ )
                ->operator MATCH_SEQUENCE_CONCATENATION_OPERATOR( item );
//     } else if ( item->type() == MatchItem::MatchSequence &&
//                 item->flags() == QPointer<SyntaxItemType>::data()->flags() )
//     {
// //         TODO If the whole sequence on the right (to be concatenated with the left item)
// //              has some flags set, eg. MatchIsOptional, all childs should inherit that flag?
// //         MatchItems _items = qobject_cast<MatchItemSequence*>(item.data())->items();
// //         Q_FOREACH ( MatchItem _item, _items ) {
// //             _item.fl
// //         }
//         qDebug() << "TMIP: Concat with a sequence:" << "\nA:" << QPointer<SyntaxItemType>::data()->toString() << "\nB:" << item->toString();
//         return new MatchItemSequence( MatchItems() << QPointer<SyntaxItemType>::data()
//                 << *qobject_cast<MatchItemSequence*>(item.data())->items() );
    } else {
        #ifdef DEBUG_SYNTAXITEM_OPERATORS_FULL
            qDebug() << "TMatchItemPointer: Create new sequence with two items"
                     << "\nLeft:" << m_item->toString()
                     << "\nRight:" << item->toString();
        #elif defined(DEBUG_SYNTAXITEM_OPERATORS)
            qDebug() << "TMatchItemPointer: Create new sequence with two items";
        #endif
        return new SyntaxSequenceItem( SyntaxItems() << QPointer<SyntaxItemType>::data() << item );
    }
};

template <typename SyntaxItemType>
SyntaxOptionPointer TSyntaxItemPointer<SyntaxItemType>::operator |( SyntaxItemPointer item )
{
    if ( QPointer<SyntaxItemType>::data()->isOptional() || item->isOptional() ) {
        kDebug() << "Option item with optional child items!\n"
                "Because an optional item always matches, items after an optional child\n"
                "are never used. Make the option item optional instead.";
    }

    if ( QPointer<SyntaxItemType>::data()->type() == SyntaxItem::MatchOption ) {
        return qobject_cast<SyntaxOptionItem*>(QPointer<SyntaxItemType>::data())->operator |(item);
    } else if ( item->type() == SyntaxItem::MatchOption ) {
        return qobject_cast<SyntaxOptionItem*>(item.data())->operator |(QPointer<SyntaxItemType>::data());
    } else {
        return new SyntaxOptionItem( SyntaxItems() << QPointer<SyntaxItemType>::data() << item );
    }
};

inline SyntaxSequenceItem* SyntaxSequenceItem::operator MATCH_SEQUENCE_CONCATENATION_OPERATOR(
        const SyntaxItemPointer& item )
{
    // Add items in the sequence on the right to this sequence,
    // but only if they have the same flags and output type.
    // Otherwise the flags/output type of the second item would be discarded.
    if ( item->type() == SyntaxItem::MatchSequence && item->flags() == flags() &&
         item->valueType() == valueType() ) // TODO Add a function that checks if the item can be concatenated with another one
    {
        #ifdef DEBUG_SYNTAXITEM_OPERATORS_FULL
            qDebug() << "MatchItemSequence: Concat a sequence with another sequence:"
                     << "\nLeft:" << toString() << "\nRight:" << item->toString();
        #elif defined(DEBUG_SYNTAXITEM_OPERATORS)
            qDebug() << "MatchItemSequence: Concat a sequence with another sequence";
        #endif
        SyntaxSequenceItem *sequenceItem = qobject_cast<SyntaxSequenceItem*>( item.data() );
        for ( SyntaxItems::Iterator it = sequenceItem->items()->begin();
              it != sequenceItem->items()->end(); ++it )
        {
            if ( !(*it)->parent() ) {
                (*it)->setParent( this );
            }
        }
        m_items << *sequenceItem->items();
    } else {
        #ifdef DEBUG_SYNTAXITEM_OPERATORS_FULL
            qDebug() << "MatchItemSequence: Concat a sequence with an item:"
                     << "\nLeft:" << toString() << "\nRight:" << item->toString();
        #elif defined(DEBUG_SYNTAXITEM_OPERATORS)
            qDebug() << "MatchItemSequence: Concat a sequence with an item";
        #endif
        if ( !item->parent() ) {
            item->setParent( this );
        }
        m_items << item;
    }
    return this;
};

inline SyntaxOptionItem* SyntaxOptionItem::operator|( const Parser::SyntaxItemPointer& item )
{
    // Add items in the option on the right to this option,
    // but only if they have the same flags and output type.
    // Otherwise the flags/output type of the second item would be discarded.
    if ( item->type() == SyntaxItem::MatchOption && item->flags() == flags() &&
         item->valueType() == valueType() )
    {
        #ifdef DEBUG_SYNTAXITEM_OPERATORS_FULL
            qDebug() << "MatchItemOption: Concat an option with another option:"
                     << "\nLeft:" << toString() << "\nRight:" << item->toString();
        #elif defined(DEBUG_SYNTAXITEM_OPERATORS)
            qDebug() << "MatchItemOption: Concat an option with another option";
        #endif
        SyntaxOptionItem *optionItem = qobject_cast<SyntaxOptionItem*>( item.data() );
        for ( SyntaxItems::Iterator it = optionItem->options()->begin();
              it != optionItem->options()->end(); ++it )
        {
            if ( !(*it)->parent() ) {
                (*it)->setParent( this );
            }
        }
        m_options << *qobject_cast<SyntaxOptionItem*>(item.data())->options();
    } else {
        #ifdef DEBUG_SYNTAXITEM_OPERATORS_FULL
            qDebug() << "MatchItemOption: Concat an option with an item:"
                     << "\nLeft:" << toString() << "\nRight:" << item->toString();
        #elif defined(DEBUG_SYNTAXITEM_OPERATORS)
            qDebug() << "MatchItemOption: Concat an option with an item";
        #endif
        if ( !item->parent() ) {
            item->setParent( this );
        }
        m_options << item;
    }
    return this;
}

template< typename SyntaxItemType > template< typename Other >
TSyntaxItemPointer< SyntaxItemType >::operator TSyntaxItemPointer<Other>() const
{
    Other *pointer = qobject_cast<Other*>( QPointer<SyntaxItemType>::data() ); // QPointer<SyntaxItemType>::data() );
    if ( !pointer ) {
        // If casting to MatchItemSequence, put this item into a new sequence item
        if ( Other::staticMetaObject.className() == QLatin1String("Parser::SyntaxSequenceItem") ) {
            return new SyntaxSequenceItem( SyntaxItems() << TSyntaxItemPointer< SyntaxItemType >::data() );
        } else {
            kWarning() << "Invalid conversion from"
                       << TSyntaxItemPointer< SyntaxItemType >::data()->metaObject()->className()
                       << "to" << Other::staticMetaObject.className();
            #ifdef DEBUG_SYNTAXITEM_OPERATORS
                qDebug() << TMatchItemPointer< SyntaxItemType >::data()->toString();
            #endif
        }
    }
    return pointer;
};

inline QDebug& operator<<( QDebug debug, SyntaxItem::Type type )
{
    switch ( type ) {
    case SyntaxItem::MatchNothing:
        return debug << "MatchItem::MatchNothing";
    case SyntaxItem::MatchSequence:
        return debug << "MatchItem::MatchSequence";
    case SyntaxItem::MatchOption:
        return debug << "MatchItem::MatchOption";
    case SyntaxItem::MatchCharacter:
        return debug << "MatchItem::MatchCharacter";
    case SyntaxItem::MatchNumber:
        return debug << "MatchItem::MatchNumber";
    case SyntaxItem::MatchString:
        return debug << "MatchItem::MatchString";
    case SyntaxItem::MatchKeyword:
        return debug << "MatchItem::MatchKeyword";
    case SyntaxItem::MatchWords:
        return debug << "MatchItem::MatchWords";
    default:
        return debug << static_cast<int>(type);
    }
}

inline QDebug& operator<<( QDebug debug, SyntaxItem::Flag flag )
{
    switch ( flag ) {
    case SyntaxItem::DefaultMatch:
        return debug << "MatchItem::DefaultMatch";
    case SyntaxItem::MatchIsOptional:
        return debug << "MatchItem::MatchIsOptional";
    case SyntaxItem::MatchGreedy:
        return debug << "MatchItem::MatchGreedy";
    case SyntaxItem::KleenePlus:
        return debug << "MatchItem::KleenePlus";
    case SyntaxItem::KleeneStar:
        return debug << "MatchItem::KleeneStar";
    default:
        return debug << static_cast<int>(flag);
    }
}

inline QDebug& operator<<( QDebug debug, SyntaxItem item ) {
    return debug << item.toString();
};

}; // namespace Parser

#endif // SYNTAXITEM_HEADER
