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

// Uncomment to print debug messages while parsing
#define DEBUG_PARSING

// Uncomment to have the MatchItem::matches() & co. functions work
// If defined, USE_MATCHES_IN_MATCHITEM should also be defined in matchitem.h
// #define FILL_MATCHES_IN_MATCHITEM

// Uncomment to allow keywords with spaces, ie. made out of multiple words
// #define ALLOW_MULTIWORD_KEYWORDS

class QString;
class QStringList;
class QDate;
class QTime;
class QDateTime;
class KLineEdit;

namespace Parser {

/**
 * @brief Base class for lexical/syntactical/contextual analyzers.
 *
 * An LR(1)-Parser, for an RL(1)-Parser, see @ref AnalyzerRL.
 * No parser generation, 
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
     * Error checking is done by using @ref isItemErrornous. If there is an error and the input
     * isn't already @ref Rejected, the result value is set to @ref AcceptedWithErrors.
     **/
    virtual inline void readItem() {
        moveToNextToken();
        if ( m_result != Rejected && !isAtImaginaryIteratorEnd() && isItemErrornous() ) {
            m_result = AcceptedWithErrors;
        }
    };
    virtual inline void updateLookahead() {
        if ( m_inputIteratorLookahead != m_input.constEnd() ) {
            ++m_inputIteratorLookahead;
        }
    };
    virtual inline void incrementLookahead() { ++m_inputIteratorLookahead; };
    virtual inline void decrementLookahead() { --m_inputIteratorLookahead; };

    /**
     * @brief Checks for errors in the current item.
     *
     * The default implementation always returns false. Can be overwritten to set an error when
     * reading an error item generated by a previous analyzer pass.
     **/
    virtual inline bool isItemErrornous() { return false; };

    /**
     * @brief Checks wether or not the input interator points to the last item of the input.
     **/
    virtual inline bool isAtEnd() const { return m_inputIteratorLookahead == m_input.constEnd(); };

    virtual void setError( ErrorSeverity severity, const QString& errornousText,
                           const QString& errorMessage, int position,
                           MatchItem *parent = 0, int index = -1 );

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
//         qDebug() << "| Current Lexem:" << (isAtImaginaryIteratorEnd() ? "END" : m_inputIterator->text())
//                  << ", Next Lexem:" <<  (isNextAtImaginaryIteratorEnd() ? "END" : m_inputIteratorLookahead->text());
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
 * @brief Base class for lexical/syntactical/contextual analyzers, that also parse from right to left.
 *
 * This class can be used for both parsing directions, left-to-right and right-to-left. The
 * direction of parsing can be changed by setting m_direction to the new direction. It can also
 * be changed while parsing, ie. start with parsing from left, then from right and see what items
 * remain in the  middle (eg. some free text).
 * The current direction impacts @ref readItem (going to the left/right) and @ref isAtEnd (if
 * @ref RightToLeft, the "end" is actually the beginning of the input, ie. the last readable item
 * in the current direction).
 **/
template <typename Container, typename Item>
class AnalyzerRL : public Analyzer<Container, Item> {
public:
    AnalyzerRL( AnalyzerCorrections corrections = CorrectEverything,
                int cursorPositionInInputString = -1, int cursorOffset = 0 );

    virtual void moveToBeginning();
    virtual void moveToEnd();

protected:
    /** @brief The current parsing direction. */
    AnalyzerReadDirection direction() const { return m_direction; };

    /**
     * @brief Increases/Decreases the input iterator and checks for errors.
     *
     * Increases if direction is @ref LeftToRight, decreses if direction is @ref RightToLeft. 
     * Error checking is done by using @ref isItemErrornous.
     * If there is an error and the input isn't already @ref Rejected, the result value is set to
     * @ref AcceptedWithErrors.
     **/
    virtual inline void readItem() {
        this->moveToNextToken();
        #ifdef DEBUG_PARSING
            qDebug() << "readItem()" << (this->isAtImaginaryIteratorEnd()
                    ? "END!" : this->currentInputToken().input());
            qDebug() << "   > Lookahead:" << (this->isNextAtImaginaryIteratorEnd()
                    ? "END!" : this->nextInputToken().input());
        #endif

        if ( !this->isAtImaginaryIteratorEnd() && this->isItemErrornous() &&
             this->m_result != Rejected )
        {
            this->m_result = AcceptedWithErrors;
        }
    };
    virtual inline void updateLookahead() {
        if ( m_direction == RightToLeft ) {
            if ( this->isNextAtImaginaryIteratorEnd() ) {// constEnd() same as constBegin()-1 ?    OLD CODE: this->m_inputIteratorLookahead != this->m_input.constBegin() - 1 ) {
                this->decrementLookahead();
            }
        } else {
            Analyzer<Container, Item>::updateLookahead();
        }
    };

    /**
     * @brief Checks wether or not the input interator points to the last item of the input.
     *
     * If direction is @ref RightToLeft the last item is actually the first.
     **/
    virtual inline bool isAtEnd() const {
        return m_direction == RightToLeft ? this->isAtImaginaryIteratorEnd()
                                          : this->isNextAtImaginaryIteratorEnd();
    };

    AnalyzerReadDirection m_direction;
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
//         Character, /**< A special character, eg. a quotation mark, colon or point. */
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

    SyntacticalAnalyzer( JourneySearchKeywords *keywords = 0,
                         AnalyzerCorrections corrections = CorrectEverything,
                         int cursorPositionInInputString = -1, int cursorOffset = 0 );
    ~SyntacticalAnalyzer();

    /**
     * @brief Analyzes the given @p input list of Lexem objects.
     *
     * @param input The input list to analyze.
     *
     * @return A list of SyntaxItem objects, that have been recognized in @p input.
     **/
    QLinkedList<MatchItem> analyze( const QLinkedList<Lexem> &input );

    /** @brief Returns the output of the last call to @ref analyze. */
    QLinkedList<MatchItem> output() const { return m_output; };

    /** @brief Returns a pointer to the used JourneySearchKeywords object. */
    JourneySearchKeywords *keywords() const { return m_keywords; };

    /**
     * @brief Matches @p input using @p item.
     *
     * @param input The input that should be matched.
     * @param item The item to be used for matching.
     *
     * @return True, if @p item matches @p input. False otherwise
     **/
    bool matchItem( const QLinkedList<Lexem> &input, SyntaxItemPointer item );

protected:
//     enum MatchResult {
//         MatchedNothing = 0,
//         MatchedCompletely = 1,
//         MatchedPartially = 2
//     };
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

    virtual inline bool isItemErrornous() { return currentInputToken().isErrornous(); };
    virtual void setError2( ErrorSeverity severity, const QString& errornousText,
                            const QString& errorMessage, int position,
                            const LexemList &lexems = LexemList(),
                            MatchItem *parent = 0, int index = -1 );
    virtual void setError2( ErrorSeverity severity, const QString& errornousText,
                            AnalyzerCorrection errorCorrection, int position,
                            const LexemList &lexems = LexemList(),
                            MatchItem *parent = 0, int index = -1 );

    inline void readItemAndSkipSpaces( MatchItem *parentItem = 0 ) {
        Analyzer::readItem();
        readSpaceItems( parentItem );
    };
    void readSpaceItems( MatchItem *parentItem = 0 );

    QVariant readValueFromChildren( JourneySearchValueType valueType, MatchItem *syntaxItem ) const;
    QTime timeValue( const QHash< JourneySearchValueType, QVariant > &values ) const;
    QDate dateValue( const QHash< JourneySearchValueType, QVariant > &values ) const;

    /**
     * @brief Inserts @p syntaxItem into the output, sorted by SyntaxItem::position().
     *
     * TODO parentItem documentation
     **/
    void addOutputItem( const MatchItem &syntaxItem, MatchItem *parentItem = 0 );

    /**
     * @brief Parses the given input for a journey search.
     *
     * Uses recursive descent, ie. one match function for each derivation rule.
     * These are the underlying derivation rules:
     * @verbatim
     *  [JourneySearch] := [Prefix][StopName][Suffix]
     *         [Prefix] := To|From|<nothing>
     *         [Suffix] := [ArriveOrDepart][Tomorrow][TimeAt]|<nothing>
     * [ArriveOrDepart] := arrival|departure|<nothing>
     *         [TimeAt] := at [Number:0-23]:[Number:0-59]|<nothing>
     *         [TimeIn] := in [Number:1-1339]+ minutes|<nothing>
     *       [Tomorrow] := tomorrow|<nothing>
     *       [StopName] := (letter|other|digit)*
     * @endverbatim
     *
     * ArriveOrDepart, Tomorrow, To and From are matched using @ref matchKeywordInList
     * (keywords can have multiple translations).
     *
     * Match functions check at the current position for a rule/string.
     * They only change the current position/input if they do match (and return true).
     *
     * @return bool True if a journey search was matched, ie. at least a stop name was found.
     *   False, otherwise.
     **/
    bool parseJourneySearch();

    bool matchPrefix(); // Parse left-to-right
    bool matchSuffix(); // Parse right-to-left
    bool matchStopName(); // Needs to be called AFTER parsePrefix AND parseSuffix
    bool matchTimeAt();
    bool matchTimeIn();
    bool matchKeywordInList( KeywordType type, MatchItem *match, MatchItem *parent = 0 );
    bool matchNumber( int *number, int min = -1, int max = 9999999, MatchItem *parent = 0,
                      bool *corrected = 0, int *removedDigits = 0 );

    bool matchCharacter( char character, QString *text = 0, MatchItem *parent = 0 );

    /// @p wordCount == -1 to match all found words (string/number/other)
    bool matchWords( int wordCount = 1, QStringList *matchedWords = 0, QString *matchedText = 0,
                     LexemList *matchedLexems = 0, const QList<Lexem::Type> &wordTypes =
                       QList<Lexem::Type>() << Lexem::String << Lexem::Number << Lexem::Space,
                     MatchItem *parent = 0 );

    bool matchString( const QStringList &strings, QString *matchedString = 0, MatchItem *parent = 0 );
    inline bool matchString( const QString &string ) {
        return matchString( QStringList() << string );
    };
    bool matchMinutesString();

    /**
     * @brief Matches input using @p items, applying the Kleene star/plus if used by an item.
     *
     * This function checks the flags of @p item for KleeneStar / KleenePlus and calls
     * @ref matchKleeneStar if one of these flags is set. Otherwise @ref matchItemKleeneUnaware
     * gets called.
     *
     * @param items A list of match items, representing a sequence of rules.
     * @param item The current match item in @p items. This is an iterator to be able to test if
     *   the next item matches, which is needed for non-greedy Kleene star/plus items.
     * @param matchedItem A SyntaxItem object with information about a match, if @p item matches.
     * @param parent The parent SyntaxItem for new output. If this is 0, output gets added to the
     *   global output list (@ref SyntacticalAnalyzer::output). Sequence and option items use
     *   themselves as parent in calls to @ref matchItem. Default is 0.
     *
     * @return True, if @p item has matched. False otherwise.
     **/
    bool matchItem( SyntaxItems *items, SyntaxItems::ConstIterator *item, MatchItem *matchedItem,
                    MatchItem *parent = 0 );

    /**
     * @brief Matches input using @p items, applying the Kleene star, ie. match them multiple times.
     *
     * Matches input using the rules associated with the given @p items, starting with @p item.
     * Matching is done greedy if @p item has the @ref MatchItem::MatchGreedy flag set. Otherwise
     * it's done non-greedy.
     *
     * This iterator @p item may be incremented, if it's MatchItem matches non-greedy (has the
     * @ref MatchItem::MatchGreedy flag not set) and the following item in @p items matches. The
     * following item gets tested before each recursion in a Kleene star. If it matches, the Kleene
     * star is finished. The other termination condition is the end of input items (lexems).
     *
     * @note @p item must have the @ref MatchItem::KleeneStar or @ref MathItem::KleenePlus flag set.
     *
     * @param items A list of match items, representing a sequence of rules.
     * @param item The current match item in @p items. This is an iterator to be able to test if
     *   the next item matches, which is needed for non-greedy Kleene star/plus items.
     * @param matchedItem A SyntaxItem object with information about the last matched item, if
     *   there were at least one match.
     * @param parent The parent SyntaxItem for new output. If this is 0, output gets added to the
     *   global output list (@ref SyntacticalAnalyzer::output). Sequence and option items use
     *   themselves as parent in calls to @ref matchItem. Default is 0.
     *
     * @returns The number of matches of @p item.
     **/
    int matchKleeneStar( SyntaxItems *items, SyntaxItems::ConstIterator *item,
                         MatchItem *matchedItem, MatchItem *parent = 0 );

    /**
     * @brief Matches input using a @p sequence item, ie. it's child items.
     *
     * The sequence matches, if all items in the sequence match one after the other.
     * Optional items always match, Kleene star objects are optional Kleene plus objects.
     *
     * @note This doesn't handle KleeneStar or KleenePlus flags of @p sequence.
     *   To do so use @ref SyntacticalAnalyzer::matchItem.
     *
     * @param sequence A pointer to the @ref MatchItemSequence object containing the items in the
     *   sequence to match.
     * @param matchedItem Gets set to the @ref SyntaxItem object generated for a match of the
     *   sequence, if it has matched. Default is 0.
     * @param parent The parent SyntaxItem for new output. If this is 0, output gets added to the
     *   global output list (@ref SyntacticalAnalyzer::output). Sequence and option items use
     *   themselves as parent in calls to @ref matchItem. Default is 0.
     *
     * @return True, if the sequence has matched. False otherwise.
     **/
    bool matchSequence( const SyntaxSequencePointer &sequence, MatchItem *matchedItem = 0,
                        MatchItem *parent = 0 );

    /**
     * @brief Matches input using an @p option item, ie. it's child items.
     *
     * The option matches, if one it's items matches.
     * Optional items always match, Kleene star objects are optional Kleene plus objects.
     *
     * @note This doesn't handle KleeneStar or KleenePlus flags of @p sequence.
     *   To do so use @ref SyntacticalAnalyzer::matchItem.
     *
     * @param option A pointer to the @ref MatchItemOption object containing the items in the
     *   option to match.
     * @param matchedItem Gets set to the @ref SyntaxItem object generated for a match of the
     *   option, if it has matched. Default is 0.
     * @param parent The parent SyntaxItem for new output. If this is 0, output gets added to the
     *   global output list (@ref SyntacticalAnalyzer::output). Sequence and option items use
     *   themselves as parent in calls to @ref matchItem. Default is 0.
     *
     * @return True, if the option has matched. False otherwise.
     **/ // TODO replace argument type with SyntaxOptionPointer?
    bool matchOption( SyntaxOptionItem *option, MatchItem *matchedItem = 0,
                      MatchItem *parent = 0 );

    bool matchKeyword( SyntaxKeywordItem* keyword, MatchItem *matchedItem = 0,
                       MatchItem *parent = 0 );

    /**
     * @brief TODO.
     *
     * Doesn't handle KleenePlusItem and KleeneStarItem, but OptionalMatchItem.
     * This function matches terminal symbols, but not only.
     *
     * @param items A list of match items, representing a sequence of rules.
     * @param item The current match item in @p items. This is an iterator to be able to test if
     *   the next item matches, which is needed for non-greedy Kleene star/plus items.
     * @param matchedItem A SyntaxItem object with information about the last matched item, if
     *   there were at least one match.
     * @param parent The parent SyntaxItem for new output. If this is 0, output gets added to the
     *   global output list (@ref SyntacticalAnalyzer::output). Sequence and option items use
     *   themselves as parent in calls to @ref matchItem. Default is 0.
     *
     * @return True, if @p item has matched. False otherwise.
     **/
    bool matchItemKleeneUnaware( SyntaxItems *items, SyntaxItems::ConstIterator *item,
                                 MatchItem *matchedItem, MatchItem *parent = 0,
                                 AnalyzerCorrections enabledCorrections = CorrectEverything );

//     bool stringsSimilar( const QString &string1, const QString &string2 );

private:
    QLinkedList<MatchItem> m_output;
    QLinkedList<Lexem>::ConstIterator m_stopNameBegin; // Pointing to the first stop name lexem
    QLinkedList<Lexem>::ConstIterator m_stopNameEnd; // Pointing the the lexem after the last stop name lexem
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
    virtual void setError( ErrorSeverity severity, const QString& errornousText,
                           const QString& errorMessage, int position,
                           MatchItem *parent = 0, int index = -1 );
    virtual inline bool isItemErrornous() { return currentInputToken().isErrornous(); };
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
     * @brief The output string, constructed from the syntax items in syntaxItems.
     *
     * @note This method is fast, because the output strings are already generated.
     **/
    QString outputString( OutputStringFlags flags = DefaultOutputString ) const;

    /**
     * @brief Same as @ref outputString, but with an updated stop name.
     *
     * @note This method needs to TODO
     **/
    QString updatedOutputString( const QHash< JourneySearchValueType, QVariant > &updateItemValues,
            const QList<MatchItem::Type> &removeItems = QList<MatchItem::Type>(),
            OutputStringFlags flags = DefaultOutputString,
            JourneySearchKeywords *keywords = 0 ) const;

    /**
     * @brief All syntax items ordered by position in the input string.
     *
     * This list also contains error items.
     **/
    QLinkedList<MatchItem> allItems() const { return m_allItems; };

    /**
     * @brief All syntax items that are no error items.
     *
     * All syntax items in this hash are pointers to items in @ref Results::allItems.
     **/
    QHash< MatchItem::Type, const MatchItem* > syntaxItems() const { return m_syntaxItems; };

    /**
     * @brief The syntax item with the given @p type.
     *
     * @note Use @ref SyntaxItem::isValid to check if the returned syntax item is valid, ie.
     *   if it was found in the input string. If @ref hasSyntaxItem returns false for @p type,
     *   this method will return an invalid syntax item.
     **/
    const MatchItem *syntaxItem( MatchItem::Type type ) const { return m_syntaxItems[type]; };

    /**
     * @brief Wether or not the given @p type is contained in the list of syntax items.
     *
     * @note If this method returns false, @ref syntaxItem will return an invalid syntax item
     *   for @p type.
     **/
    bool hasSyntaxItem( MatchItem::Type type ) const { return m_syntaxItems.contains(type); };

    /**
     * @brief All error syntax items.
     *
     * All syntax items in this list are pointers to items in @ref Results::allItems.
     **/
    QList< const MatchItem* > errorItems() const { return m_errorItems; };

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
    AnalyzerResult m_result;
    QDateTime m_time;
    QString m_stopName;
    QString m_outputStringWithErrors;
    QString m_outputString;
    QString m_inputString;
    bool m_hasErrors;
    bool m_timeIsDeparture;
    bool m_stopIsTarget;
    int m_cursorOffset;
    int m_selectionLength;
    QLinkedList<MatchItem> m_allItems;
    QHash< MatchItem::Type, const MatchItem* > m_syntaxItems;
    QList< const MatchItem* > m_errorItems;
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
     **/
    JourneySearchAnalyzer( JourneySearchKeywords *keywords = 0,
                           AnalyzerCorrections corrections = CorrectEverything,
                           int cursorPositionInInputString = -1  );

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

    static Results resultsFromSyntaxItemList( const QLinkedList<MatchItem> &itemList,
                                              JourneySearchKeywords *keywords );
    Results resultsFromSyntaxItemList( const QLinkedList<MatchItem> &itemList ) {
        return resultsFromSyntaxItemList( itemList, m_keywords );
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
