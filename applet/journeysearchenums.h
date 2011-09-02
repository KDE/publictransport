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

#ifndef JOURNEYSEARCHENUMS_HEADER
#define JOURNEYSEARCHENUMS_HEADER

#include <QDebug>

/**
 * @brief This namespace contains classes/enums used for parsing input strings.
 **/
#ifndef Q_MOC_RUN
    namespace
#else
    class
#endif

Parser {

#if defined(Q_MOC_RUN)
    Q_GADGET
    Q_ENUMS(AnalyzerState AnalyzerReadDirection AnalyzerResult AnalyzerCorrections
            ErrorSeverity KeywordType JourneySearchValueType)
    Q_FLAGS(OutputStringFlags)
public:
#endif

/**
 * @brief The state of an analyzer.
 **/
enum AnalyzerState {
    NotStarted = 0, /**< The analyzer hasn't been started yet. */
    Running, /**< The analyzer is currently running. */
    Finished /**< The analyzer has finished. */
};

/**
 * @brief The read direction of an analyzer.
 **/
enum AnalyzerReadDirection {
    LeftToRight = 0, /**< Read the input from left to right. */
    RightToLeft /**< Read the input from right to left. */
};

/**
 * @brief The result of an analyzer pass.
 *
 * @note Bigger values mean "more acception", 100 means fully accepted, 0 means rejected.
 **/
enum AnalyzerResult {
    Rejected = 0, /**< The input was rejected. */
    AcceptedWithErrors = 50, /**< The input was accepted, but there were some errors. */
    Accepted = 100 /**< The input was accepted. */
};

/**
 * @brief The level of correction of an analyzer.
 *
 * @note Bigger values mean more correction.
 **/
enum AnalyzerCorrection {
    CorrectNothing              = 0x0000, /**< Do not correct anything. */
    RemoveInvalidCharacters     = 0x0001, // in lexical analyzer
    SkipUnexpectedTokens        = 0x0002,
    InsertMissingRequiredTokens = 0x0004, // Not implemented
    CorrectNumberRanges         = 0x0008,
    CompleteKeywords            = 0x0010,

    CorrectEverything = RemoveInvalidCharacters | SkipUnexpectedTokens
            | InsertMissingRequiredTokens | CorrectNumberRanges | CompleteKeywords
            /**< Correct whenever it is possible. */
};
Q_DECLARE_FLAGS( AnalyzerCorrections, AnalyzerCorrection );

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

enum KeywordType {
    KeywordTo = 0, /**< The used keyword translation is returned by @ref text. TODO UPDATE DOCU */
    KeywordFrom, /**< A "from" keyword, saying that the stop name is the origin stop.
            * The used keyword translation is returned by @ref text. */
    KeywordTimeIn, /**< An "in" keyword, saying when the searched journeys should depart/arrive.
            * The used keyword translation is returned by @ref text. The time in minutes from
            * now can be got using @ref value (it's an integer). */
    KeywordTimeInMinutes, /**< Can follow @ref KeywordTimeIn. */
    KeywordTimeAt, /**< An "at" keyword, saying when the searched journeys should depart/arrive.
            * The used keyword translation is returned by @ref text. The time can be got using
            * @ref value (it's a QTime). */
    KeywordTomorrow, /**< A "tomorrow" keyword, saying that the searched journey should
            * depart/arrive tomorrow. The used keyword translation is returned by @ref text. */
    KeywordDeparture, /**< A "departure" keyword, saying that the searched journeys should
            * depart at the given time. The used keyword translation is returned by @ref text. */
    KeywordArrival, /**< A "arrival" keyword, saying that the searched journeys should arrive
            * at the given time. The used keyword translation is returned by @ref text. */
};

enum JourneySearchValueType {
    NoValue,
    ErrorMessageValue, /**< TODO Value contains the error message. */
    ErrorCorrectionValue, /**< TODO Value contains the Correction used to correct the error. */

    StopNameValue, /**< The stop name as string list with the words of the stop name. */

    DateAndTimeValue, /**< TODO A date and time value, eg. "hh:mm, yyyy-MM-dd". */
    RelativeTimeValue, /**< TODO A relative time value in minutes (1-1339, ie. max. 1 day). */

    TimeValue, /**< TODO A time value, eg. "hh:mm". */
    TimeHourValue, /**< TODO An hour value (0 - 23). */
    TimeMinuteValue, /**< TODO A minute value (0 - 59). */

    DateValue, /**< TODO A date value, eg. "yyyy-MM-dd". */
    DateDayValue, /**< TODO A day value (1 - 31). */
    DateMonthValue, /**< TODO A month value (1 - 12). */
    DateYearValue /**< TODO A year value (1970 - 2999). */
};


inline QDebug &operator <<( QDebug debug, KeywordType keywordType ) {
    switch( keywordType ) {
    case KeywordTo:
        return debug << "KeywordTo";
    case KeywordFrom:
        return debug << "KeywordFrom";
    case KeywordTimeIn:
        return debug << "KeywordTimeIn";
    case KeywordTimeInMinutes:
        return debug << "KeywordTimeInMinutes";
    case KeywordTimeAt:
        return debug << "KeywordTimeAt";
    case KeywordTomorrow:
        return debug << "KeywordTomorrow";
    case KeywordDeparture:
        return debug << "KeywordDeparture";
    case KeywordArrival:
        return debug << "KeywordArrival";
    default:
        return debug << "Unknown keyword type";
    }
};

#ifndef Q_MOC_RUN
    extern const QMetaObject staticMetaObject;
#endif
} // namespace/class

#ifdef Q_MOC_RUN
;
#endif

#endif // JOURNEYSEARCHENUMS_HEADER
