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

class QString;
class QStringList;
class QDate;
class QTime;
class QDateTime;
class KLineEdit;

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
 * @brief The state of an analyzer.
 **/
enum AnalyzerState {
    NotStarted, /**< The analyzer hasn't been started yet. */
    Running, /**< The analyzer is currently running. */
    Finished /**< The analyzer has finished. */
};

/**
 * @brief The read direction of an analyzer.
 **/
enum AnalyzerReadDirection {
    LeftToRight, /**< Read the input from left to right. */
    RightToLeft /**< Read the input from right to left. */
};

/**
 * @brief The result of an analyzer pass.
 **/
enum AnalyzerResult {
    Accepted, /**< The input was accepted. */
    AcceptedWithErrors, /**< The input was accepted, but there were some errors. */
    Rejected /**< The input was rejected. */
};

/**
 * @brief The level of correction of an analyzer.
 *
 * Bigger values mean more correction.
 **/
enum AnalyzerCorrectionLevel {
    CorrectNothing = 0, /**< Don't correct anything. */
    CorrectEverything = 100 /**< Correct whenever it is possible. */
};

/**
* @brief The severity of an error.
*
* @note Bigger values mean more severe errors.
**/
enum ErrorSeverity {
    NoError = 0, /**< No error has happened. */
    ErrorInformational = 1, /**< Simple information errors, nothing critical. */
    ErrorMinor = 2, /**< Recoverable errors, eg. wrong keyword order. */
    ErrorSevere = 3,
    ErrorFatal = 4, /**< Input is invalid, eg. essential information is missing. */
    InfiniteErrorSeverity = 100 /**< Not a severity class, but to be used as minimum error value
            * in @ref Analyzer::setErrorHandling. */
};

/** @brief Options for how to construct an output string from a list of syntax items. */
enum OutputStringFlag {
    DefaultOutputString = 0x0000, /**< Don't include errornous parts. This shouldn't be used if
            * the output string is currently typed in by the user, because correct strings
            * (almost) can't be typed with a single keystroke. */
    ErrornousOutputString = 0x0001, /** Include errornous parts. This flag should be used if
            * the output string gets typed in interactively. */
};
Q_DECLARE_FLAGS( OutputStringFlags, OutputStringFlag );

/**
 * @brief Base class for lexical/syntactical/contextual analyzers.
 *
 * An LR(1)-Parser, for an RL(1)-Parser, see @ref AnalyzerRL.
 **/
template <typename Container>
class Analyzer {
public:
    Analyzer( AnalyzerCorrectionLevel correctionLevel = CorrectEverything,
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

    /** @brief The current correction level. */
    AnalyzerCorrectionLevel correctionLevel() const { return m_correctionLevel; };

    /**
     * @brief Sets the correction level to @p correctionLevel.
     *
     * @note This should be called before calling @ref analyze.
     *
     * @param correctionLevel The new correction level.
     **/
    void setCorrectionLevel( AnalyzerCorrectionLevel correctionLevel ) {
            m_correctionLevel = correctionLevel; };

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

protected:
    /**
     * @brief Increases the input iterator and checks for errors.
     *
     * Error checking is done by using @ref isItemErrornous. If there is an error and the input
     * isn't already @ref Rejected, the result value is set to @ref AcceptedWithErrors.
     **/
    virtual inline void readItem() {
        m_inputIterator = m_inputIteratorLookahead;
        ++m_inputIteratorLookahead;
        if ( isItemErrornous() && m_result != Rejected ) {
            m_result = AcceptedWithErrors;
        }
    };

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
                           const QString& errorMessage, int position, int index = -1 );

    AnalyzerState m_state;
    AnalyzerResult m_result;
    AnalyzerCorrectionLevel m_correctionLevel;
    ErrorSeverity m_minRejectSeverity;
    ErrorSeverity m_minAcceptWithErrorsSeverity;
    Container m_input;
    typename Container::ConstIterator m_inputIterator;
    typename Container::ConstIterator m_inputIteratorLookahead; // Lookahead iterator, always the next item of m_inputIterator
    int m_cursorPositionInInputString; // In the input string of the first analyzer pass
    int m_cursorOffset;
    int m_selectionLength;
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
template <typename Container>
class AnalyzerRL : public Analyzer<Container> {
public:
    AnalyzerRL( AnalyzerCorrectionLevel correction = CorrectEverything,
                int cursorPositionInInputString = -1, int cursorOffset = 0 )
            : Analyzer<Container>(correction, cursorPositionInInputString, cursorOffset) {};

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
        this->m_inputIterator = this->m_inputIteratorLookahead;
        this->m_inputIteratorLookahead += m_direction == RightToLeft ? -1 : 1;
        if ( this->m_inputIterator != this->m_input.constEnd() && this->isItemErrornous() &&
             this->m_result != Rejected )
        {
            this->m_result = AcceptedWithErrors;
        }
    };

    /**
     * @brief Checks wether or not the input interator points to the last item of the input.
     *
     * If direction is @ref RightToLeft the last item is actually the first.
     **/
    virtual inline bool isAtEnd() const {
        return m_direction == RightToLeft ? this->m_inputIterator == this->m_input.constBegin()
                                          : this->m_inputIteratorLookahead == this->m_input.constEnd();
    };

    AnalyzerReadDirection m_direction;
};

class LexicalAnalyzer;
/**
 * @brief Represents a lexem, eg. a string or a number.
 *
 * There are different types of lexems (@ref Lexem::Type). To get the position in the input string
 * use @ref Lexem::position.
 *
 * Lexem objects are elements of the output list of LexicalAnalyzer and elements of the input list
 * to SyntacticalAnalyzer.
 **/
class Lexem {
public:
    // Friend classes are allowed to call the protected constructor
    friend class LexicalAnalyzer;

    /**
     * @brief Types of lexems.
     **/
    enum Type {
        Error = 0, /**< An illegal string/character in the input string. */
        Number, /**< A string of digits. */
        QuotationMark, /**< A single quotation mark ("). */
        Colon, /**< A colon (:). */
        Space, /**< A space character (" ") at the end of the input or at the specified
                * cursor position */ // TODO Add cursorPosition to Analyzer?
        String /**< A string, maybe a keyword. */
    };

    /**
     * @brief Constructs an invalid Lexem.
     *
     * This is used for eg. QHash as default value.
     **/
    Lexem();

    /**
     * @brief The type of this lexem.
     *
     * @see Type
     **/
    inline Type type() const { return m_type; };

    /**
     * @brief The original text of this lexem in the input string.
     *
     * For error items (@ref Error), this contains the illegal string read from the input string.
     **/
    inline QString text() const { return m_text; };

    /** @brief The position of this lexem in the input string. */
    inline int position() const { return m_pos; };

    /** @brief Wether or not there are any errors in the input string for this lexem. */
    inline bool isErrornous() const { return m_type == Error; };

    /** @brief Wether or not there is a space character after this lexem in the input string. */
    inline bool isFollowedBySpace() const { return m_followedBySpace; };

    /**
     * @brief Wether or not this is a valid lexem, that has been read from the input string.
     *
     * Invalid lexems are returned by QHash if trying to get an item, which isn't in the hash.
     * That means that
     * @code
     * QHash< Lexem::Type, Lexem > hash;
     * if ( hash.contains(Lexem::Number) ) {
     *   ...
     * }
     * @endcode
     * is equivalent to
     * @code
     * QHash< Lexem::Type, Lexem > hash;
     * if ( hash[Lexem::Number].isValid() ) {
     *   ...
     * }
     * @endcode
     **/
    inline bool isValid() const { return m_pos >= 0; };

protected:
    /**
     * @brief Constructs a new Lexem object.
     *
     * @param type The type of the Lexem.
     *
     * @param text The text in the input string for which this Lexem is created.
     *
     * @param pos The position in the input string of the Lexem.
     *
     * @param followedBySpace Whether or not there is a space character after this lexem in the
     *   input string.
     **/
    Lexem( Type type, const QString &text, int pos, bool followedBySpace = true );

private:
    Type m_type;
    QString m_text;
    int m_pos;
    bool m_followedBySpace;
};
inline bool operator <( const Lexem &lexem1, const Lexem &lexem2 ) {
    return lexem1.position() < lexem2.position();
};

/**
 * @brief Analyzes a given input string and constructs a list of @ref Lexem objects.
 **/
class LexicalAnalyzer : public Analyzer<QString> {
public:
    friend class Lexem; // To call the protected constructor

    /** @brief Symbols recognized in input strings. */
    enum Symbol {
        Invalid, /**< Invalid terminal symbol. */
        Digit, /**< A digit. Gets combined with adjacent digits to a @ref Lexem::Number. */
        Letter, /**< A letter. Gets combined with adjacent letters/other symbols to a
                * @ref Lexem::String. */
        QuotationMark, /**< A quotation mark. */
        Space, /**< A space character at the end of the input or at the specified cursor position. */
        Colon, /**< A colon. */
        OtherSymbol /**< Another valid symbol from @ref ALLOWED_OTHER_CHARACTERS. Gets combined
                * with adjacent letters/other symbols to a @ref Lexem::String. */
    };

    static const QString ALLOWED_OTHER_CHARACTERS;

    LexicalAnalyzer( AnalyzerCorrectionLevel correction = CorrectEverything,
                     int cursorPositionInInputString = -1, int cursorOffset = 0 );

    QLinkedList<Lexem> analyze( const QString &input );
    QLinkedList<Lexem> output() const { return m_output; };

protected:
    Symbol symbolOf( const QChar &c ) const;

    void endCurrentWord( bool followedBySpace = false );
    bool isSpaceFollowing();
    virtual inline void readItem() { ++m_inputIterator; ++m_pos; };
    virtual inline bool isAtEnd() const { return m_pos >= m_input.length(); };

private:
    QLinkedList<Lexem> m_output;

    QString m_currentWord;
    int m_wordStartPos;
    Symbol m_firstWordSymbol;
    int m_pos;
};

/**
 * @brief Represents a syntax item, eg. a keyword.
 *
 * There are different types of syntax items (@ref SyntaxItem::Type), some of which have an
 * associated value (@ref SyntaxItem::value). To get the position in the input string use
 * @ref SytaxItem::position.
 *
 * SyntaxItem objects are elements of the output list of SyntacticalAnalyzer and ContextualAnalyzer
 * and elements of the input list to ContextualAnalyzer.
 **/
class SyntaxItem {
public:
    // Friends for calling the protected constructor
    friend class SyntacticalAnalyzer;
    friend class ContextualAnalyzer;
    friend class JourneySearchAnalyzer;
    friend class Results;

    /**
     * @brief Types of syntax items.
     **/
    enum Type {
        Error = 0, /**< An error item. Get the error message with @ref text, the error severity
                * with @ref value (cast to @ref ErrorSeverity). Error items appear inside the
                * output list of syntax items at the position where the error appeared in the
                * input string. */
        StopName, /**< The stop name, get it using @ref text. If the stop name is surrounded by
                * quotation marks in the input string, they are stripped here. */
        KeywordTo, /**< A "to" keyword, saying that the stop name is the target stop.
                * The used keyword translation is returned by @ref text. */
        KeywordFrom, /**< A "from" keyword, saying that the stop name is the origin stop.
                * The used keyword translation is returned by @ref text. */
        KeywordTimeIn, /**< An "in" keyword, saying when the searched journeys should depart/arrive.
                * The used keyword translation is returned by @ref text. The time in minutes from
                * now can be got using @ref value (it's an integer). */
        KeywordTimeAt, /**< An "at" keyword, saying when the searched journeys should depart/arrive.
                * The used keyword translation is returned by @ref text. The time can be got using
                * @ref value (it's a QTime). */
        KeywordTomorrow, /**< A "tomorrow" keyword, saying that the searched journey should
                * depart/arrive tomorrow. The used keyword translation is returned by @ref text. */
        KeywordDeparture, /**< A "departure" keyword, saying that the searched journeys should
                * depart at the given time. The used keyword translation is returned by @ref text. */
        KeywordArrival /**< A "arrival" keyword, saying that the searched journeys should arrive
                * at the given time. The used keyword translation is returned by @ref text. */
    };

    /**
     * @brief Flags for syntax items.
     **/
    enum Flag {
        DefaultSyntaxItem   = 0x0000, /**< Default flag for syntax items. */
        CorrectedSyntaxItem = 0x0001 /**< This flag is set for syntax items that weren't read
                * correctly from the input string, but were corrected. */
    };
    Q_DECLARE_FLAGS( Flags, Flag );

    /**
     * @brief Constructs an invalid SyntaxItem.
     *
     * This is used for eg. QHash as default value.
     **/
    SyntaxItem();

    /**
     * @brief The type of this syntax item.
     *
     * @see Type
     **/
    inline Type type() const { return m_type; };

    /**
     * @brief Flags of this syntax item.
     *
     * For example corrected syntax items have the flag @ref CorrectedSyntaxItem set.
     *
     * @see Flags
     **/
    inline Flags flags() const { return m_flags; };

    /**
     * @brief The original text of this syntax item in the input string.
     *
     * For keywords, it contains only the used keyword translation. Keywords with a value (eg.
     * @ref KeywordTimeIn or @ref KeywordTimeAt) the value text isn't contained in the returned
     * string. To get the value use @ref value, eg. to construct an output string again.
     * If @ref type returns @ref StopName, this method returns the stop name, without quotation
     * marks, if there are any in the input string.
     * For error items (@ref Error), this contains the original errornous string.
     **/
    inline QString text() const { return m_text; };

    /**
     * @brief The value of this syntax item, if there is any.
     *
     * This only returns a valid QVariant, if there is a value associated with this syntax item.
     * For example @ref KeywordTimeIn has the number of minutes from now stored as an integer and
     * @ref KeywordTimeAt has the time stored as a QTime.
     * For @ref Error this is the error message (QString).
     **/
    inline QVariant value() const { return m_value; };

    /** @brief The position of this syntax item in the input string. */
    inline int position() const { return m_pos; };

    /** @brief Wether or not there are any errors in the input string for this syntax item. */
    inline bool isErrornous() const { return m_type == Error; };

    /**
     * @brief Wether or not this is a valid syntax item, that has been read from the input string.
     *
     * Invalid syntax items are returned by QHash if trying to get an item, which isn't in the hash.
     * That means that
     * @code
     * QHash< SyntaxItem::Type, SyntaxItem > hash;
     * if ( hash.contains(SyntaxItem::StopName) ) {
     *   ...
     * }
     * @endcode
     * is equivalent to
     * @code
     * QHash< SyntaxItem::Type, SyntaxItem > hash;
     * if ( hash[SyntaxItem::StopName].isValid() ) {
     *   ...
     * }
     * @endcode
     **/
    inline bool isValid() const { return m_pos >= 0; };

    /**
     * @brief Gets the name for the given @p type.
     *
     * This is used for error messages, eg. "Keyword 'name' isn't allowed here".
     **/
    static QString typeName( Type type );

protected:
    /**
     * @brief Constructs a new syntax item object.
     *
     * @param type The type of the syntax item. Default is Invalid.
     * @param text The read text for this syntax item, eg. the used keyword translation. Don't
     *   include values of keywords here. Default is QString().
     * @param pos The position of this syntax item in the input string. Default is -1.
     * @param value A value associated with this syntax item or an invalid QVariant, if there is
     *   no value associated. Default is QVariant().
     * @param flags Flags for the syntax item, eg. @ref CorrectedSyntaxItem.
     *   Default is DefaultSyntaxItem.
     **/
    SyntaxItem( Type type, const QString &text, int pos,
                const QVariant &value = QVariant(), Flags flags = DefaultSyntaxItem );

private:
    QString m_text;
    QVariant m_value; // For KeywordTimeAt (QTime), KeywordTimeIn (int)
    Type m_type;
    Flags m_flags;
    int m_pos; // Position in source string of LexicalAnalyzer (Lexem::pos())
};
inline bool operator <( const SyntaxItem &syntaxItem1, const SyntaxItem &syntaxItem2 ) {
    return syntaxItem1.position() < syntaxItem2.position() ||
           syntaxItem1.type() == SyntaxItem::Error;
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
class SyntacticalAnalyzer : public AnalyzerRL< QLinkedList<Lexem> > {
public:
    friend class SyntaxItem;

    SyntacticalAnalyzer( JourneySearchKeywords *keywords = 0,
                         AnalyzerCorrectionLevel correction = CorrectEverything,
                         int cursorPositionInInputString = -1, int cursorOffset = 0 );
    ~SyntacticalAnalyzer();

    QLinkedList<SyntaxItem> analyze( const QLinkedList<Lexem> &input );
    QLinkedList<SyntaxItem> output() const { return m_output; };

protected:
//     enum MatchResult {
//         MatchedNothing = 0,
//         MatchedCompletely = 1,
//         MatchedPartially = 2
//     };

    virtual inline bool isItemErrornous() { return m_inputIterator->isErrornous(); };
    virtual void setError( ErrorSeverity severity, const QString& errornousText,
                           const QString& errorMessage, int position, int index = -1 );

    inline void readItemAndSkipSpaces() {
        AnalyzerRL::readItem();
        readSpaceItems();
    };

    void readSpaceItems() {
        while ( m_inputIterator != m_input.constEnd() && m_inputIterator->type() == Lexem::Space ) {
            addOutputItem( SyntaxItem(SyntaxItem::Error, " ", m_inputIterator->position(),
                                      "Double space character") );
            readItem();
        }
    };

    /**
     * @brief Inserts @p syntaxItem into the output, sorted by SyntaxItem::position().
     *
     * @returns An iterator pointing at the inserted item.
     **/
    QLinkedList<SyntaxItem>::iterator addOutputItem( const SyntaxItem &syntaxItem );

    JourneySearchKeywords *keywords() const { return m_keywords; };

    // Recursive descent (keywords can have multiple translations)
    //  [JourneySearch] := [Prefix][StopName][Suffix]
    //         [Prefix] := To|From|<nothing>
    //         [Suffix] := [ArriveOrDepart][Tomorrow][TimeAt]|<nothing>
    // [ArriveOrDepart] := arrival|departure|<nothing>
    //         [TimeAt] := at (digit)(digit):(digit)(digit)|<nothing>
    //         [TimeIn] := in (digit)+ minutes|<nothing>
    //       [Tomorrow] := tomorrow|<nothing>
    //       [StopName] := (letter|other|digit)*
    bool parseJourneySearch();
    bool matchPrefix(); // Parse from left
    bool matchSuffix(); // Parse from right
    bool matchStopName(); // needs to be called AFTER parsePrefix AND parseSuffix

    // Match functions check at the current position for a rule/string
    // They only change the current position/input if they do match (and return true)
    bool matchTimeAt();
    bool matchTimeIn();
    bool matchMinutesString();
    bool matchKeywordInList( SyntaxItem::Type type, const QStringList &keywordList,
                             QLinkedList<SyntaxItem>::iterator *match = 0 );
    inline bool matchKeyword( SyntaxItem::Type type, const QString &keyword ) {
        return matchKeywordInList( type, QStringList() << keyword );
    };
    bool matchNumber( int *number, int min = -1, int max = 9999999, int *removedDigits = 0 );

private:
    QLinkedList<SyntaxItem> m_output;
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
 * Depending on the @ref correctionLevel, the input string may be corrected on errors, eg. syntax
 * items may be removed or reordered.
 **/
class ContextualAnalyzer : public Analyzer< QLinkedList<SyntaxItem> > {
public:
    friend class SyntaxItem;

    ContextualAnalyzer( AnalyzerCorrectionLevel correction = CorrectEverything,
                        int cursorPositionInInputString = -1, int cursorOffset = 0  );

    QLinkedList<SyntaxItem> analyze( const QLinkedList<SyntaxItem> &input );
    QLinkedList<SyntaxItem> output() const { return m_input; };

protected:
    virtual void setError( ErrorSeverity severity, const QString& errornousText,
                           const QString& errorMessage, int position, int index = -1 );
    virtual inline bool isItemErrornous() { return m_inputIterator->isErrornous(); };
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
    QString updatedOutputString( const QHash< SyntaxItem::Type, QVariant > &updateItemValues,
            const QList<SyntaxItem::Type> &removeItems = QList<SyntaxItem::Type>(),
            OutputStringFlags flags = DefaultOutputString,
            JourneySearchKeywords *keywords = 0 ) const;

    /**
     * @brief All syntax items ordered by position in the input string.
     *
     * This list also contains error items.
     **/
    QLinkedList<SyntaxItem> allItems() const { return m_allItems; };

    /**
     * @brief All syntax items that are no error items.
     *
     * All syntax items in this hash are pointers to items in @ref Results::allItems.
     **/
    QHash< SyntaxItem::Type, const SyntaxItem* > syntaxItems() const { return m_syntaxItems; };

    /**
     * @brief The syntax item with the given @p type.
     *
     * @note Use @ref SyntaxItem::isValid to check if the returned syntax item is valid, ie.
     *   if it was found in the input string. If @ref hasSyntaxItem returns false for @p type,
     *   this method will return an invalid syntax item.
     **/
    const SyntaxItem *syntaxItem( SyntaxItem::Type type ) const { return m_syntaxItems[type]; };

    /**
     * @brief Wether or not the given @p type is contained in the list of syntax items.
     *
     * @note If this method returns false, @ref syntaxItem will return an invalid syntax item
     *   for @p type.
     **/
    bool hasSyntaxItem( SyntaxItem::Type type ) const { return m_syntaxItems.contains(type); };

    /**
     * @brief All error syntax items.
     *
     * All syntax items in this list are pointers to items in @ref Results::allItems.
     **/
    QList< const SyntaxItem* > errorItems() const { return m_errorItems; };

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
    QLinkedList<SyntaxItem> m_allItems;
    QHash< SyntaxItem::Type, const SyntaxItem* > m_syntaxItems;
    QList< const SyntaxItem* > m_errorItems;
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
                           AnalyzerCorrectionLevel correctionLevel = CorrectEverything,
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
                           AnalyzerCorrectionLevel correctionLevel = CorrectEverything );

    /**
     * @brief Returns the results of the last analyzation.
     *
     * Before calling this method @ref analyze should be called. Otherwise an invalid Results
     * object is returned.
     **/
    const Results results() const { return m_results; };

    static Results resultsFromSyntaxItemList( const QLinkedList<SyntaxItem> &itemList,
                                              JourneySearchKeywords *keywords );
    Results resultsFromSyntaxItemList( const QLinkedList<SyntaxItem> &itemList ) {
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
QDebug &operator <<( QDebug debug, Lexem::Type type );
QDebug &operator <<( QDebug debug, SyntaxItem::Type type );

bool operator ==( const SyntaxItem &l, const SyntaxItem &r );

#endif // Multiple inclusion guard
