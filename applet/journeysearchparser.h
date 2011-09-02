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

#ifndef JOURNEYSEARCHPARSER_HEADER
#define JOURNEYSEARCHPARSER_HEADER

#include <QVariant>
#include <QStringList>
#include <QDebug>
#include <QLinkedList>
#include <QDate>

#include "journeysearchenums.h"
#include "syntaxitem.h"
#include "lexem.h"
#include "matchitem.h"

// Uncomment to have the SyntaxItem::matches() & co. functions work.
// If defined, USE_MATCHES_IN_SYNTAXITEM should also be defined in syntaxitem.h:
// #define FILL_MATCHES_IN_SYNTAXITEM

// Uncomment to allow keywords with spaces, ie. made out of multiple words:
// #define ALLOW_MULTIWORD_KEYWORDS

// Uncomment to print all debug messages while parsing:
#define DEBUG_PARSING_ALL

// Uncomment to print debug messages for calls to functions for parsing
// (also enabled, if DEBUG_PARSING_ALL is defined):
// #define DEBUG_PARSING_FUNCTION_CALLS

// Uncomment to print informational debug messages while parsing
// (also enabled, if DEBUG_PARSING_ALL is defined):
// #define DEBUG_PARSING_INFO

// Uncomment to print debug messages when new input lexems get read
// (also enabled, if DEBUG_PARSING_ALL is defined):
// #define DEBUG_PARSING_INPUT

// Uncomment to print debug messages when new output is generated while parsing
// (also enabled, if DEBUG_PARSING_ALL is defined).
// This is similiar to DEBUG_PARSING_MATCH, but also prints messages for erroneous output
// (eg. an unexpected and skipped lexem):
// #define DEBUG_PARSING_OUTPUT

// Uncomment to print debug messages for each match while parsing
// (also enabled, if DEBUG_PARSING_ALL is defined).
// This is similiar to DEBUG_PARSING_OUTPUT, but it only prints messages for correctly matched input:
// #define DEBUG_PARSING_MATCH

// Uncomment to print debug messages for each error found in the input while parsing
// (also enabled, if DEBUG_PARSING_ALL is defined):
#define DEBUG_PARSING_ERROR

// Indentation of debug output for different syntax item levels
#define DEBUG_PARSING_INDENTATION "  "

class QString;
class QStringList;
class QDate;
class QTime;
class QDateTime;
class KLineEdit;

namespace Parser {

#ifdef DEBUG_PARSING_ALL
    // Enable all debug output
    #define DEBUG_PARSING_FUNCTION_CALLS
    #define DEBUG_PARSING_INFO
    #define DEBUG_PARSING_INPUT
    #define DEBUG_PARSING_OUTPUT
    #define DEBUG_PARSING_MATCH
    #define DEBUG_PARSING_ERROR
#endif

#if (defined(DEBUG_PARSING_FUNCTION_CALLS) || defined(DEBUG_PARSING_INPUT) \
     || defined(DEBUG_PARSING_OUTPUT) || defined(DEBUG_PARSING_INFO) \
     || defined(DEBUG_PARSING_MATCH) || defined(DEBUG_PARSING_ERROR))
    // Define DEBUG_PARSING if at least one debug output class is enabled
    #define DEBUG_PARSING
#endif

#ifdef DEBUG_PARSING
    // This function makes the level in the syntax items better identifiable, to which a debug
    // message belongs (for example items in a sequence are one level deeper than the sequence
    // item itself, see DEBUG_PARSING_INDENTATION).
    // "void *matchData" is actually of type "SyntacticalAnalyzer::MatchData*", but nested classes
    // can not be forward declared.
    QByteArray __DEBUG_PARSING_LEVEL( void *matchData );
#endif

// The following macros do some magic for the debug messages to work easily and produce
// pretty output using __DEBUG_PARSING_LEVEL for indentation (the ".data()" is to avoid having
// quotation marks around strings to qDebug()).
#ifdef DEBUG_PARSING_FUNCTION_CALLS
    #define DO_DEBUG_PARSING_FUNCTION_CALL(function, output) \
        qDebug() << __DEBUG_PARSING_LEVEL(matchData).data() \
                 << QString("%1()").arg(#function).toLatin1().data() << output
    #define DO_DEBUG_PARSING_FUNCTION_CALL2(function) \
        qDebug() << __DEBUG_PARSING_LEVEL(matchData).data() \
                 << QString("%1()").arg(#function).toLatin1().data()
#else
    #define DO_DEBUG_PARSING_FUNCTION_CALL(function, output)
    #define DO_DEBUG_PARSING_FUNCTION_CALL2(function) 
#endif

#ifdef DEBUG_PARSING_INFO
    #define DO_DEBUG_PARSING_INFO(output) \
        qDebug() << __DEBUG_PARSING_LEVEL(matchData).data() << output
#else
    #define DO_DEBUG_PARSING_INFO(output)
#endif

#ifdef DEBUG_PARSING_INPUT
    #define DO_DEBUG_PARSING_INPUT() \
        if ( m_inputIterator != m_input.constEnd() ) { \
            qDebug() << __DEBUG_PARSING_LEVEL(matchData).data() \
                     << "< Input:" <<  m_inputIterator->input(); \
        } else { \
            qDebug() << __DEBUG_PARSING_LEVEL(matchData).data() << "< Input: <END>"; \
        }
#else
    #define DO_DEBUG_PARSING_INPUT()
#endif

#ifdef DEBUG_PARSING_OUTPUT
    #define DO_DEBUG_PARSING_OUTPUT(output) \
        qDebug() << __DEBUG_PARSING_LEVEL(matchData).data() << "> Output:" << output
#else
    #define DO_DEBUG_PARSING_OUTPUT(output)
#endif

#ifdef DEBUG_PARSING_MATCH
    #define DO_DEBUG_PARSING_MATCH(what, output) \
        qDebug() << __DEBUG_PARSING_LEVEL(matchData).data() << "MATCH ("#what"):" << output
#else
    #define DO_DEBUG_PARSING_MATCH(what, output)
#endif

#ifdef DEBUG_PARSING_ERROR
    #define DO_DEBUG_PARSING_ERROR(level, position, errorMessage, erroneousText) \
        qDebug() << QString("Error (%2) at %1 (\"%4\"): \"%3\"") \
                .arg(position).arg(#level).arg(errorMessage).arg(erroneousText).toLatin1().data()
#else
    #define DO_DEBUG_PARSING_ERROR(level, position, errorMessage, erroneousText)
#endif

/**
 * @brief Base class for lexical/syntactical/contextual analyzers.
 *
 * An LR(1)-Parser.
 * No parser generation, TODO
 **/
template <typename Container, typename Item>
class Analyzer {
public:
    Analyzer( AnalyzerCorrections corrections = CorrectEverything,
              int cursorPositionInInputString = -1, int cursorOffset = 0 );
    virtual ~Analyzer() {};

    /** @brief The current state of the analyzer. */
    AnalyzerState state() const { return m_state; };

    /**
     * @brief The result of analyzing.
     *
     * @note This may not return the end result, if @ref state isn't @ref Finished.
     **/
    AnalyzerResult result() const { return m_result; };

    /** @brief The current correction flags. */
    AnalyzerCorrections corrections() const { return m_corrections; };

    /**
     * @brief Sets the correction flags to @p corrections.
     *
     * @note This should be called before calling @ref analyze.
     *
     * @param corrections The new correction flags.
     **/
    void setCorrections( AnalyzerCorrections corrections ) { m_corrections = corrections; };

    /** @brief Wether or not the input was accepted (and analyzing has finished). */
    bool isAccepted() const { return m_state == Finished && m_result != Rejected; };

    /** @brief Wether or not the input was rejected (and analyzing has finished). */
    bool isRejected() const { return m_state == Finished && m_result == Rejected; };

    /** @brief Wether or not the input has errors. */
    bool hasErrors() const { return m_result == Rejected || m_result == AcceptedWithErrors; };

    /**
     * @brief Sets parameters to control error handling.
     *
     * @param minRejectSeverity The minimum ErrorSeverity at which the input gets rejected.
     *   Use @ref InfiniteErrorSeverity to never reject any input. Default is ErrorFatal.
     * @param minAcceptWithErrorsSeverity The minimum ErrorSeverity at which the input gets
     *   accepted, but marked with @ref AcceptedWithErrors. Use @ref InfiniteErrorSeverity to never
     *   mark any input with @ref AcceptedWithErrors. Default is ErrorSevere.
     **/
    void setErrorHandling( ErrorSeverity minRejectSeverity = ErrorFatal,
                           ErrorSeverity minAcceptWithErrorsSeverity = ErrorSevere );

    int cursorOffset() const { return m_cursorOffset; };
    int selectionLength() const { return m_selectionLength; };
    void setCursorValues( int cursorOffset = 0, int selectionLength = 0 ) {
        m_cursorOffset = cursorOffset;
        m_selectionLength = selectionLength;
    };
    void setCursorPositionInInputString( int cursorPositionInInputString = -1 ) {
        m_cursorPositionInInputString = cursorPositionInInputString;
        m_cursorOffset = 0;
        m_selectionLength = 0;
    };

    virtual void moveToBeginning();
    virtual void moveToEnd();

protected:
    /**
     * @brief Increases the input iterator and checks for errors.
     *
     * Error checking is done by using @ref isItemErroneous. If there is an error and the input
     * isn't already @ref Rejected, the result value is set to @ref AcceptedWithErrors.
     **/
    virtual inline void readItem() {
        moveToNextToken();
        if ( m_result != Rejected && !isAtImaginaryIteratorEnd() && isItemErroneous() ) {
            m_result = AcceptedWithErrors;
        }
    };
    virtual inline void updateLookahead() {
        if ( m_inputIteratorLookahead != m_input.constEnd() ) {
            ++m_inputIteratorLookahead;
        }
    };

    /**
     * @brief Checks for errors in the current item.
     *
     * The default implementation always returns false. Can be overwritten to set an error when
     * reading an error item generated by a previous analyzer pass.
     **/
    virtual inline bool isItemErroneous() { return false; };

    /**
     * @brief Checks wether or not the input interator points to the last item of the input.
     **/
    virtual inline bool isAtEnd() const { return m_inputIteratorLookahead == m_input.constEnd(); };

    virtual void setError( ErrorSeverity severity, const QString &erroneousText,
                           const QString &errorMessage, int position, MatchItem *parent = 0 );

    inline Item currentInputToken() const { return *m_inputIterator; };
    inline Item nextInputToken() const { return *m_inputIteratorLookahead; };

    /**
     * @brief Checks if the input iterator is at the imaginary end after the last input token.
     *
     * This method should be used instead of comparing @codem_inputIterator == m_input.constEnd()@endcode,
     * to prevent wrong usage, ie. @codem_input.end()@endcode instead of @code m_input.constEnd()@endcode.
     **/
    inline bool isAtImaginaryIteratorEnd() const { return m_inputIterator == m_input.constEnd(); };

    /**
     * @brief Checks if the lookahead input iterator is at the imaginary end after the last input token.
     *
     * This method should be used instead of comparing @codem_inputIteratorLookahead == m_input.constEnd()@endcode,
     * to prevent wrong usage, ie. @codem_inputIteratorLookahead.end()@endcode instead of
     * @code m_inputIteratorLookahead.constEnd()@endcode.
     **/
    inline bool isNextAtImaginaryIteratorEnd() const { return m_inputIteratorLookahead == m_input.constEnd(); };

    inline void moveToNextToken() {
        m_inputIterator = m_inputIteratorLookahead;
        updateLookahead();
    };

    AnalyzerState m_state;
    AnalyzerResult m_result;
    AnalyzerCorrections m_corrections;
    ErrorSeverity m_minRejectSeverity;
    ErrorSeverity m_minAcceptWithErrorsSeverity;
    Container m_input;
    int m_cursorPositionInInputString; // In the input string of the first analyzer pass
    int m_cursorOffset;
    int m_selectionLength;

    typename Container::ConstIterator m_inputIterator;
    typename Container::ConstIterator m_inputIteratorLookahead; // Lookahead iterator, always the next item of m_inputIterator
};

/**
 * @brief Analyzes a given input string and constructs a list of @ref Lexem objects.
 **/
class LexicalAnalyzer : public Analyzer<QString, QChar> {
public:
    friend class Lexem; // To call the protected constructor

    /** @brief Symbols recognized in input strings. */
    enum Symbol {
        Invalid, /**< Invalid terminal symbol. */
        Digit, /**< A digit. Gets combined with adjacent digits to a @ref Lexem::Number. */
        Letter, /**< A letter. Gets combined with adjacent letters/other symbols to a
                * @ref Lexem::String. */
        Space, /**< A space character at the end of the input or at the specified cursor position. */
        OtherSymbol /**< Another valid symbol from @ref ALLOWED_OTHER_CHARACTERS. Gets combined
                * with adjacent letters/other symbols to a @ref Lexem::String. */
    };

    /** @brief Characters allowed for class @ref OtherSymbol. */
    static const QString ALLOWED_OTHER_CHARACTERS;

    LexicalAnalyzer( AnalyzerCorrections corrections = CorrectEverything,
                     int cursorPositionInInputString = -1, int cursorOffset = 0 );

    /**
     * @brief Analyzes the given @p input string.
     *
     * @param input The input string to analyze.
     * 
     * @return A list of Lexem objects, that have been recognized in @p input.
     **/
    QLinkedList<Lexem> analyze( const QString &input );

    /** @brief Returns the output of the last call to @ref analyze. */
    QLinkedList<Lexem> output() const { return m_output; };

    virtual void moveToBeginning();

protected:
    Symbol symbolOf( const QChar &c ) const;

    void endCurrentWord( bool followedBySpace = false );
    bool isSpaceFollowing();
    virtual inline void readItem() { moveToNextToken(); ++m_pos; };
    virtual inline bool isAtEnd() const { return m_pos >= m_input.length(); };

private:
    QLinkedList<Lexem> m_output;

    QString m_currentWord;
    int m_wordStartPos;
    Symbol m_firstWordSymbol;
    int m_pos;
};

/**
 * @brief Caches keyword string lists.
 *
 * Keywords are translated as strings containing multiple comma seperated keyword translations.
 * That means that getting the list of translations for a keyword requires getting the string
 * from i18n and then splitting it (QString::split). As that takes too long for
 * JourneySearchAnalyzer, which uses these keywords for parsing, they are cached in this class.
 **/
class JourneySearchKeywords {
public:
    JourneySearchKeywords();

    bool hasValue( KeywordType type ) const;

    const QStringList keywords( KeywordType type ) const;

    /** @brief A list of keywords which may be at the beginning of the journey
     * search string, indicating that a given stop name is the target stop. */
    inline const QStringList toKeywords() const { return m_toKeywords; };

    /** @brief A list of keywords which may be at the beginning of the journey
     * search string, indicating that a given stop name is the origin stop. */
    inline const QStringList fromKeywords() const { return m_fromKeywords; };

    /** @brief A list of keywords, indicating that a given time is the departure
     * time of journeys to be found. */
    inline const QStringList departureKeywords() const { return m_departureKeywords; };

    /** @brief A list of keywords, indicating that a given time is the arrival
     * time of journeys to be found. */
    inline const QStringList arrivalKeywords() const { return m_arrivalKeywords; };

    /** @brief A list of keywords which should be followed by a time and/or date
     * string, eg. "at 12:00, 01.12.2010" (with "at" being the keyword). */
    inline const QStringList timeKeywordsAt() const { return m_timeKeywordsAt; };

    /** @brief A list of keywords which should be followed by a relative time string,
     * eg. "in 5 minutes" (with "in" being the keyword). */
    inline const QStringList timeKeywordsIn() const { return m_timeKeywordsIn; };

    /** @brief A list of keywords which can follow @ref KeywordTimeIn. */
    inline const QStringList timeKeywordsInMinutes() const { return m_timeKeywordsInMinutes; };

    /** @brief A list of keywords to be used instead of writing the date string for tomorrow. */
    inline const QStringList timeKeywordsTomorrow() const { return m_timeKeywordsTomorrow; };

    const QString relativeTimeString( const QVariant &value = 5 ) const;
    inline const QString relativeTimeStringPattern() const { return m_relativeTimeStringPattern; };

private:
    const QStringList m_toKeywords;
    const QStringList m_fromKeywords;
    const QStringList m_departureKeywords;
    const QStringList m_arrivalKeywords;
    const QStringList m_timeKeywordsAt;
    const QStringList m_timeKeywordsIn;
    const QStringList m_timeKeywordsInMinutes;
    const QStringList m_timeKeywordsTomorrow;
    const QString m_relativeTimeStringPattern;
};

/**
 * @brief Analyzes a given list of @ref Lexem objects and constructs a list of @ref SyntaxItem objects.
 *
 * Depending on the @ref correctionLevel, the input string may be corrected, resulting in syntax
 * items in the output that aren't completely/correctly read from the input. Inserted correction
 * items have the flag @ref SyntaxItem::CorrectedSyntaxItem set.
 *
 * This class has multiple matchX() functions, which return a boolean: true, if they have matched;
 * false, otherwise. If they have matched, the input iterator is set to the next item.
 **/
class SyntacticalAnalyzer : public Analyzer< QLinkedList<Lexem>, Lexem > {
public:
    friend class MatchItem;
    friend class SyntaxItem;

    SyntacticalAnalyzer( SyntaxItemPointer syntaxItem = 0, JourneySearchKeywords *keywords = 0,
                         AnalyzerCorrections corrections = CorrectEverything,
                         int cursorPositionInInputString = -1, int cursorOffset = 0 );
    ~SyntacticalAnalyzer();

    /**
     * @brief Analyzes the given @p input list of Lexem objects.
     *
     * @param input The input list to analyze.
     *
     * @return The root of the MatchItem objects, that have been recognized in @p input.
     **/
    MatchItem analyze( const QLinkedList<Lexem> &input );

    /** @brief Returns the root output item of the last call to @ref analyze. */
    MatchItem output() const { return m_outputRoot; };

    /** @brief Returns a pointer to the used JourneySearchKeywords object. */
    JourneySearchKeywords *keywords() const { return m_keywords; };

#ifndef DEBUG_PARSING
protected:
#endif
    struct MatchData {
        MatchData( SyntaxItems _items, MatchItem *parentMatchItem = 0, MatchData *parent = 0,
                   bool onlyTesting = false )
                : items(_items), item(items.constBegin()), parentMatchItem(parentMatchItem),
                  parent(parent), onlyTesting(onlyTesting)
        {
        };

        /** A list of match items, representing a sequence of rules. */
        SyntaxItems items;

        /** The current match item in @p items. This is an iterator to be able to test if the next
         * item matches, which is needed for non-greedy Kleene star/plus items. */
        SyntaxItems::ConstIterator item;

        /** The parent MatchItem object, used to add output to. */
        MatchItem *parentMatchItem;

        /** The parent MatchData object, used to find following items after the last SyntaxItem
         * in MatchData::items. */
        MatchData *parent;

        bool onlyTesting;
    };

protected:
    struct ErrorInformation {
        AnalyzerCorrection correction;
        int position;
        Lexem lexem;

        ErrorInformation( AnalyzerCorrection correction, int position, const Lexem &lexem ) {
            this->correction = correction;
            this->position = position;
            this->lexem = lexem;
        };
    };


    virtual inline bool isItemErroneous() { return currentInputToken().isErroneous(); };
    virtual void setError2( MatchData *matchData, ErrorSeverity severity,
                            const QString &erroneousText, const QString& errorMessage,
                            int position, const LexemList &lexems = LexemList() );
    virtual void setError2( MatchData *matchData, ErrorSeverity severity,
                            const QString &erroneousText, AnalyzerCorrection errorCorrection,
                            int position, const LexemList &lexems = LexemList() );

    inline void readItemAndSkipSpaces( MatchData *matchData ) {
        DO_DEBUG_PARSING_INPUT();
        Analyzer::readItem();
        readSpaceItems( matchData );
    };
    void readSpaceItems( MatchData *matchData );

    QVariant readValueFromChildren( JourneySearchValueType valueType, MatchItem *syntaxItem ) const;
    QTime timeValue( const QHash< JourneySearchValueType, QVariant > &values ) const;
    QDate dateValue( const QHash< JourneySearchValueType, QVariant > &values ) const;

    /**
     * @brief Inserts @p syntaxItem into the output, sorted by SyntaxItem::position().
     *
     * @param matchData Contains the parent MatchItem (MatchData::parentMatchItem), where output
     *   gets added to (can also be NULL, in this case output gets set to m_outputRoot). If
     *   MatchData::onlyTesting is true, nothing gets added to the output.
     * @param matchItem The MatchItem object to add to the output.
     **/
    void addOutputItem( MatchData *matchData, const MatchItem &matchItem );

    /**
     * @brief Matches input using the current item in @p matchData, applying Kleene star/plus.
     *
     * This function checks the flags of MatchData::item for KleeneStar / KleenePlus and uses
     * @ref matchKleeneStar if one of these flags is set. Otherwise @ref matchItemKleeneUnaware
     * is used.
     *
     * @param matchData Contains information about the items to be matched (MatchData::items) and
     *   the next item to be matched (MatchData::item). Also contains a parent MatchData object
     *   (MatchData::parent), which is needed to test following items, and a parent MatchItem
     *   (MatchData::parentMatchItem), where output gets added to. Sequence and option items use
     *   themselves as parent.
     * @param matchedItem A MatchItem object, which gets filled with information about a match,
     *   if the current item in @p matchData matches.
     *
     * @return True, if the current item in @p matchData matches. False otherwise.
     **/
    bool matchItem( MatchData *matchData, MatchItems *matchedItems );

    /**
     * @brief Matches input using the current item in @p matchData, applying the Kleene star.
     *
     * Matches input using the rules associated with the given items in @p matchData, starting
     * with MatchData::item. Matching is done greedy if MatchData::item has the
     * @ref MatchItem::MatchGreedy flag set. Otherwise it's done non-greedy.
     *
     * The iterator MatchData::item may be incremented, if it matches non-greedy (has the
     * @ref MatchItem::MatchGreedy flag not set) and the following item in MatchData::items matches.
     * The following item gets tested before each recursion in a Kleene star. If it matches, the
     * Kleene star is finished. The other termination condition is the end of input items (lexems).
     *
     * @note Greedy matching is more performant, because non-greedy matching requires testing
     *   following items for a match to stop the Kleene star.
     * @note MatchData::item must have the @ref MatchItem::KleeneStar or @ref MathItem::KleenePlus
     *   flag set. This function does not handle Kleene plus, to do so use @ref matchItem.
     *
     * @param matchData Contains information about the items to be matched (MatchData::items) and
     *   the next item to be matched (MatchData::item). Also contains a parent MatchData object
     *   (MatchData::parent), which is needed to test following items, and a parent MatchItem
     *   (MatchData::parentMatchItem), where output gets added to. Sequence and option items use
     *   themselves as parent.
     * @param matchedItem A SyntaxItem object with information about the last matched item, if
     *   there were at least one match.
     *
     * @returns The number of matches of MatchData::item.
     **/
    int matchKleeneStar( MatchData *matchData, MatchItems *matchedItems );

    /**
     * @brief Matches input using the current item in @p matchData, unaware of Kleene star/plus.
     *
     * Does not handle KleenePlus and KleeneStar. To do so use @ref matchItem.
     * This function calls a suitable method which performs the actual matching of the current
     * item, based on it's type. It matches terminal symbols, but not only.
     * If matching fails, error handling is performed according to @p enabledCorrections, eg.
     * skipping unexpected lexems.
     *
     * @param matchData Contains information about the items to be matched (MatchData::items) and
     *   the next item to be matched (MatchData::item). Also contains a parent MatchData object
     *   (MatchData::parent), which is needed to test following items, and a parent MatchItem
     *   (MatchData::parentMatchItem), where output gets added to. Sequence and option items use
     *   themselves as parent.
     * @param matchedItem A SyntaxItem object with information about the last matched item, if
     *   there were at least one match.
     * @param enabledCorrections Corrections to perform if needed.
     *
     * @return True, if @p item matches. False otherwise.
     **/
    bool matchItemKleeneUnaware( MatchData *matchData, MatchItems *matchedItems,
                                 AnalyzerCorrections enabledCorrections = CorrectEverything );

    /**
     * @brief Matches the current sequence item in @p matchData, ie. the sequence of it's child items.
     *
     * The sequence matches, if all items in the sequence match one after the other. Optional items
     * may be skipped, but the sequence does not match if all items are optional and do not match.
     *
     * @note This does not handle KleeneStar or KleenePlus flags. To do so use @ref matchItem.
     * @note This expects MatchData::item to be of type SyntaxItemSequence.
     *
     * @param matchData Contains information about the items to be matched (MatchData::items) and
     *   the next item to be matched (MatchData::item). Also contains a parent MatchData object
     *   (MatchData::parent), which is needed to test following items, and a parent MatchItem
     *   (MatchData::parentMatchItem), where output gets added to. Sequence and option items use
     *   themselves as parent.
     * @param matchedItem Gets set to the @ref MatchItem object generated for a match of the
     *   sequence, if it matches. Default is 0.
     *
     * @return True, if the sequence matches, ie. at least one of it's items match. False otherwise.
     **/
    bool matchSequence( MatchData *matchData, MatchItems *matchedItems );

    /**
     * @brief Matches the current option item in @p matchData, ie. one of it's child items.
     *
     * The option matches, if one it's items match.
     *
     * @note This doesn't handle KleeneStar or KleenePlus flags. To do so use @ref matchItem.
     * @note This expects MatchData::item to be of type SyntaxItemOption.
     *
     * @param matchData Contains information about the items to be matched (MatchData::items) and
     *   the next item to be matched (MatchData::item). Also contains a parent MatchData object
     *   (MatchData::parent), which is needed to test following items, and a parent MatchItem
     *   (MatchData::parentMatchItem), where output gets added to. Sequence and option items use
     *   themselves as parent.
     * @param matchedItem Gets set to the @ref MatchItem object generated for a match of the
     *   option, if it matches. Default is 0.
     *
     * @return True, if the option matches. False otherwise.
     **/
    bool matchOption( MatchData *matchData, MatchItems *matchedItems );

    /**
     * @brief Matches the current keyword item in @p matchData.
     *
     * The keyword matches, if the keyword itself and it's value sequence match. A keyword item
     * can also have no value sequence.
     *
     * @note This doesn't handle KleeneStar or KleenePlus flags. To do so use @ref matchItem.
     * @note This expects MatchData::item to be of type SyntaxItemKeyword.
     *
     * @param matchData Contains information about the items to be matched (MatchData::items) and
     *   the next item to be matched (MatchData::item). Also contains a parent MatchData object
     *   (MatchData::parent), which is needed to test following items, and a parent MatchItem
     *   (MatchData::parentMatchItem), where output gets added to. Sequence and option items use
     *   themselves as parent.
     * @param matchedItem Gets set to the @ref MatchItem object generated for a match of the
     *   keyword, if it matches. Default is 0.
     *
     * @return True, if the keyword matches. False otherwise.
     **/
    bool matchKeyword( MatchData *matchData, MatchItems *matchedItems );

    bool matchKeywordInList( MatchData *matchData, KeywordType type, MatchItem *matchedItem );
    bool matchNumber( MatchData *matchData, MatchItems *matchedItems );
    bool matchCharacter( MatchData *matchData, MatchItems *matchedItems );
    bool matchWord( MatchData *matchData, MatchItems *matchedItems );
    bool matchString( MatchData *matchData, MatchItems *matchedItems );

//     bool stringsSimilar( const QString &string1, const QString &string2 );

private:
    MatchItem m_outputRoot;
    SyntaxItemPointer m_syntaxItem;
    JourneySearchKeywords *m_keywords;
    bool m_ownKeywordsObject;
};

/**
 * @brief Analyzes a given list of @ref SyntaxItem objects and corrects contextual errors.
 *
 * Found errors are added as syntax items of type @ref SyntaxItem::Error.
 * The performed checks include checking for correct keyword order, double keywords.
 * Depending on the @ref correctionFlags, the input string may be corrected on errors, eg. syntax
 * items may be removed or reordered.
 **/
class ContextualAnalyzer : public Analyzer< QLinkedList<MatchItem>, MatchItem > {
public:
    friend class MatchItem;

    ContextualAnalyzer( AnalyzerCorrections corrections = CorrectEverything,
                        int cursorPositionInInputString = -1, int cursorOffset = 0  );

    /**
     * @brief Analyzes the given @p input list of SyntaxItem objects.
     *
     * @param input The input list to analyze.
     *
     * @return A new list of SyntaxItem objects, with corrections compared to @p input.
     **/
    QLinkedList<MatchItem> analyze( const QLinkedList<MatchItem> &input );

    /** @brief Returns the output of the last call to @ref analyze. */
    QLinkedList<MatchItem> output() const { return m_input; };

protected:
    virtual void setError( ErrorSeverity severity, const QString &erroneousText,
                           const QString &errorMessage, int position, MatchItem *parent = 0 );
    virtual inline bool isItemErroneous() { return currentInputToken().isErroneous(); };
};

// TODO namespace JourneySearchParser

/**
 * @brief Contains results of analyzation.
 **/
class Results {
public:
    friend class JourneySearchAnalyzer;

    /**
     * @brief The name of the (origin/target) stop of the searched journey.
     *
     * If @ref Results::stopIsTarget is true, this is the name of the target stop of the
     * searched journey. If it's false, this is the name of the origin stop.
     *
     * @see Results::timeIsDeparture
     **/
    QString stopName() const { return m_stopName; };

    MatchItemPointers stopNameItems() const { return m_stopNameItems; };

//         void setStopName( const QString &stopName );

    /**
     * @brief The (departure/arrival) time of the searched journey.
     *
     * If @ref Results::timeIsDeparture is true, this is the departure time of the searched
     * journey. If it's false, this is the arrival time.
     *
     * @see Results::timeIsDeparture
     **/
    QDateTime time() const { return m_time; };

    /**
     * @brief Wether the stopName is the name of the target stop or the origin stop.
     *
     * True, if @ref Results::stopName is the name of the target stop of the searched journey.
     * False, if it's the name of the origin stop.
     **/
    bool stopIsTarget() const { return m_stopIsTarget; };

    /**
     * @brief Wether the time is the departure or the arrival.
     *
     * True, if @ref Results::time is the departure of the searched journey.
     * False, if it's the arrival.
     **/
    bool timeIsDeparture() const { return m_timeIsDeparture; };

    /** @brief Wether or not there are errors in the input string. */
    bool hasErrors() const { return m_hasErrors; };

    /** @brief The input string, from which the list of syntax items got constructed. */
    QString inputString() const { return m_inputString; };

    /**
     * @brief The output string, constructed from the match items.
     *
     * This is equal to calling @coderootItem().text(appliedCorrections)@endcode.
     * TODO => (docu) @note This method is fast, because the output strings are already generated.
     **/
    QString outputString( AnalyzerCorrections appliedCorrections = CorrectEverything ) const {
        return m_rootItem.text( appliedCorrections ); };

    /**
     * @brief Same as @ref outputString, but with an updated stop name.
     *
     * @note This method needs to TODO
     **/
    QString updatedOutputString( const QString &updatedStopName,
            const QHash<KeywordType, QString> &updateKeywordValues,
            const MatchItemTypes &removeItems = MatchItemTypes(),
            AnalyzerCorrections appliedCorrections = CorrectEverything,
            JourneySearchKeywords *keywords = 0 ) const;

    QString updateOutputString( SyntaxItemPointer syntaxItem,
                                AnalyzerCorrections appliedCorrections = CorrectEverything,
                                JourneySearchKeywords *keywords = 0 );

//     /**
//      * @brief All match items ordered by position in the input string.
//      *
//      * This list also contains error items.
//      **/
    inline MatchItem rootItem() const { return m_rootItem; };

    /**
     * @brief All match items that are no error items.
     *
     * All match items in this hash are pointers to (child) items in @ref Results::rootItem.
     **/
    inline QHash< MatchItem::Type, MatchItemPointers > matchItems() const {
        return m_matchItems; };

    /**
     * @brief The match item(s) with the given @p type.
     **/
// TODO
//     * @note Use @ref MatchItem::isValid to check if the returned match item is valid, ie.
//     *   if it was found in the input string. If @ref hasMatchItem returns false for @p type,
//     *   this method will return an invalid match item.
    inline MatchItemPointers matchItem( MatchItem::Type type ) const {
        return m_matchItems[ type ]; };

    /**
     * @brief Wether or not the given @p type is contained in the list of match items.
     *
     * @note If this method returns false, @ref matchItem will return an empty list for @p type.
     **/
    inline bool hasMatchItem( MatchItem::Type type ) const { return m_matchItems.contains(type); };

    inline bool hasKeyword( KeywordType keyword ) const { return m_keywords.contains(keyword); };

    /** TODO To get the value of the keyword item use MatchItem::keywordValue(). */
    inline const MatchItem *keyword( KeywordType keyword ) const { return m_keywords[keyword]; };

    /**
     * @brief All error syntax items.
     *
     * All syntax items in this list are pointers to items in @ref Results::allItems.
     **/
    MatchItemPointers errorItems() const { return m_errorItems; };

    int cursorOffset() const { return m_cursorOffset; };
    int selectionLength() const { return m_selectionLength; };

    /**
     * @brief Wether or not this Results object is valid.
     *
     * Invalid results can be returned by @ref JourneySearchAnalyzer::results, if no
     * analyzation has happened before using @ref JourneySearchAnalyzer::analyze.
     **/
    bool isValid() const { return m_time.isValid(); };

    /** @brief Returns the result value of the analyzers, ie. Accepted or Rejected. */
    AnalyzerResult analyzerResult() const { return m_result; };

protected:
    /**
     * @brief Constructs a new invalid Results object.
     **/
    Results() { init(); };

    /**
     * @brief (Re-)Initializes this Results object.
     *
     * Afterwards @ref Results::isValid will return false until new data is set.
     **/
    void init();

private:
    QString updateOutputString( SyntaxItemPointer syntaxItem, const MatchItem *matchItem,
            AnalyzerCorrections appliedCorrections, JourneySearchKeywords *keywords );
    QString updateOutputStringChildren( SyntaxItemPointer syntaxItem, const MatchItem *matchItem,
            AnalyzerCorrections appliedCorrections, JourneySearchKeywords *keywords );
    QString updatedOutputStringSkipErrorItems( const MatchItems &matchItems,
            MatchItems::ConstIterator *iterator, AnalyzerCorrections appliedCorrections );

    AnalyzerResult m_result;
    QDateTime m_time;
    QString m_stopName;
    QString m_inputString;
    bool m_hasErrors;
    bool m_timeIsDeparture;
    bool m_stopIsTarget;
    int m_cursorOffset;
    int m_selectionLength;
    MatchItem m_rootItem;

    // TODO No need for MatchItemPointers, because of explicit sharing?
    MatchItemPointers m_stopNameItems;
    QHash< MatchItem::Type, MatchItemPointers > m_matchItems;
    QHash< KeywordType, const MatchItem* > m_keywords;
    MatchItemPointers m_errorItems;
};

/**
 * @brief Analyzes a journey search string and constructs a @ref JourneySearchAnalyzer::Results object.
 *
 * This class uses @ref LexicalAnalyzer, @ref SyntacticalAnalyzer and @ref ContextualAnalyzer to
 * analyze the input string. These analyzers can be retrieved using
 * @ref JourneySearchAnalyzer::lexical, @ref JourneySearchAnalyzer::syntactical and
 * @ref JourneySearchAnalyzer::contextual to eg. get their intermediate analyzation results.
 * After the analyzation a @ref JourneySearchAnalyzer::Results objects is constructed from the
 * @ref SyntaxItem list.
 *
 * @note In most cases this class should be used instead of the individual analyzers. But if for
 *   example you only need the results of the lexical analyzation, use @ref LexicalAnalyzer::analyze
 *   instead.
 **/
class JourneySearchAnalyzer {
public:
    /**
     * @brief Creates a new JourneySearchAnalyzer object.
     *
     * @param keywords The JourneySearchKeywords object to use. If this is 0 a new object gets
     *   created and deleted in the destructor. If an object is given, it won't get deleted.
     *   Default is 0.
     * TODO
     **/
    JourneySearchAnalyzer( SyntaxItemPointer syntaxItem = 0, JourneySearchKeywords *keywords = 0,
                           AnalyzerCorrections corrections = CorrectEverything,
                           int cursorPositionInInputString = -1 );

    /** @brief Destructor. */
    ~JourneySearchAnalyzer();

    /**
     * @brief Analyzes the input string and returns the results.
     *
     * The (last) results object is also returned by @ref JourneySearchAnalyzer::results.
     *
     * @note Analyzation is done using @ref LexicalAnalyzer::analyze,
     *   @ref SyntacticalAnalyzer::analyze and @ref ContextualAnalyzer::analyze.
     **/
    const Results analyze( const QString &input,
                           AnalyzerCorrections corrections = CorrectEverything,
                           ErrorSeverity minRejectSeverity = ErrorFatal,
                           ErrorSeverity minAcceptWithErrorsSeverity = ErrorSevere );

    /**
     * @brief Returns the results of the last analyzation.
     *
     * Before calling this method @ref analyze should be called. Otherwise an invalid Results
     * object is returned.
     **/
    const Results results() const { return m_results; };

    static Results resultsFromMatchItem( const MatchItem &matchItem,
                                         JourneySearchKeywords *keywords );
    Results resultsFromMatchItem( const MatchItem &matchItem ) {
        return resultsFromMatchItem( matchItem, m_keywords );
    };

    void completeStopName( KLineEdit *lineEdit, const QString &completion );

    static bool isInsideQuotedString( const QString &testString, int cursorPos );

    /** @brief Returns a list of words in @p input that aren't double quoted, ie. may be keywords. */
    static QStringList notDoubleQuotedWords( const QString &input );

    /**
     * @brief The used JourneySearchKeywords object.
     *
     * Can be used for other methods/classes using JourneySearchKeywords, to not unnecessarily
     * recreating a JourneySearchKeywords object.
     *
     * @warning If no JourneySearchKeywords object was given in the constructor, this object
     *   gets deleted in the destructor.
     **/
    JourneySearchKeywords *keywords() const { return m_keywords; };

    /**
     * @brief Returns the used LexicalAnalyzer.
     *
     * Can be used to get the output of the lexical analyzation, a list of Lexem objects.
     **/
    LexicalAnalyzer *lexical() { return m_lexical; };

    /**
     * @brief Returns the used SyntacticalAnalyzer.
     *
     * Can be used to get the output of the syntactical analyzation, a list of SyntaxItem objects.
     **/
    SyntacticalAnalyzer *syntactical() { return m_syntactical; };
    
    /**
     * @brief Returns the used ContextualAnalyzerSyntacticalAnalyzer.
     *
     * Can be used to get the output of the contextual analyzation, a list of SyntaxItem objects.
     **/
    ContextualAnalyzer *contextual() { return m_contextual; };

    void setCursorPositionInInputString( int cursorPositionInInputString = -1 ) {
        m_lexical->setCursorPositionInInputString( cursorPositionInInputString );
        m_syntactical->setCursorPositionInInputString( cursorPositionInInputString );
        m_contextual->setCursorPositionInInputString( cursorPositionInInputString );
    };

private:
    // Used by notDoubleQuotedWords
    static void combineDoubleQuotedWords( QStringList *words, bool reinsertQuotedWords = true );

    Results m_results;
    JourneySearchKeywords *m_keywords;
    bool m_ownKeywordsObject;
    LexicalAnalyzer *m_lexical;
    SyntacticalAnalyzer *m_syntactical;
    ContextualAnalyzer *m_contextual;
};

QDebug &operator <<( QDebug debug, ErrorSeverity error );
QDebug &operator <<( QDebug debug, AnalyzerState state );
QDebug &operator <<( QDebug debug, AnalyzerResult result );

} // namespace

#endif // Multiple inclusion guard
