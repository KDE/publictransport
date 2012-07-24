/*
 *   Copyright 2012 Friedrich Pülz <fpuelz@gmx.de>
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

template<class Key, class T >
class QHash;
class QVariant;
class QString;
class QStringList;
class QDate;
class QTime;
class QDateTime;
class KLineEdit;

/**
 * @brief Provides static methods to parse journey search strings.
 *
 * @todo Could need some improvement, eg. parseJourneySearch() with a QString, not KLineEdit, new
 *   methods for getting a list of keywords inside a given QString, removing/adding keywords.
 **/
class JourneySearchParser {
public:
    /**
     * @brief Keywords to be used in a journey search line.
     *
     * @todo Add all keywords.
     **/
    enum Keyword {
        KeywordTo, /**< The "to" keyword indicates that journey to the given
                * stop should be searched. */
        KeywordFrom, /**< The "from" keyword indicates that journey from the
                * given stop should be searched. */
        KeywordTimeAt, /**< The "at" keyword is followed by a time and or date
                * string, indicating when the journeys should depart/arrive. */
        KeywordTimeIn /**< The "in" keyword is followed by a relative time
                * string (ie. "in 5 minutes"), indicating when the journeys
                * should depart/arrive relative to the current time. */
    };

    static bool parseJourneySearch( KLineEdit *lineEdit, const QString &search,
            QString *stop, QDateTime *departure, bool *stopIsTarget,
            bool *timeIsDeparture, int *posStart = 0, int *len = 0,
            bool correctString = true );
    static QHash<Keyword, QVariant> keywordValues( const QString &searchLine );

    /**
     * @brief Searches for the stop name in the given @p lineEdit.
     *
     * Eg. a double quoted string or the words between a to/from keyword (or from the beginning)
     * and the first other keyword (or the end of the text). The beginning is put into @p posStart,
     * the length of the stop name is put into @p len and the found stop name is put into @p stop.
     **/
    static void stopNamePosition( KLineEdit *lineEdit,
            int *posStart, int *len, QString *stop = 0 );

    static void setJourneySearchStopNameCompletion( KLineEdit *lineEdit,
                            const QString &completion );

    static bool isInsideQuotedString( const QString &testString, int cursorPos );
    static void doCorrections( KLineEdit *lineEdit, QString *searchLine,
            int cursorPos, const QStringList &words, int removedWordsFromLeft );

    /** @brief Returns a list of words in @p searchLine that aren't double quoted,
     * ie. may be keywords. */
    static QStringList notDoubleQuotedWords( const QString &searchLine );

    /** @brief A list of keywords which may be at the beginning of the journey
     * search string, indicating that a given stop name is the target stop. */
    static const QStringList toKeywords();

    /** @brief A list of keywords which may be at the beginning of the journey
     * search string, indicating that a given stop name is the origin stop. */
    static const QStringList fromKeywords();

    /** @brief A list of keywords, indicating that a given time is the departure
     * time of journeys to be found. */
    static const QStringList departureKeywords();

    /** @brief A list of keywords, indicating that a given time is the arrival
     * time of journeys to be found. */
    static const QStringList arrivalKeywords();

    /** @brief A list of keywords which should be followed by a time and/or date
     * string, eg. "at 12:00, 01.12.2010" (with "at" being the keyword). */
    static const QStringList timeKeywordsAt();

    /** @brief A list of keywords which should be followed by a relative time string,
     * eg. "in 5 minutes" (with "in" being the keyword). */
    static const QStringList timeKeywordsIn();

    /** @brief A list of keywords to be used instead of writing the date string for tomorrow. */
    static const QStringList timeKeywordsTomorrow();

    static const QString relativeTimeString( const QVariant &value );
    static const QString relativeTimeStringPattern();

private:
    static void setJourneySearchWordCompletion( KLineEdit *lineEdit,
                            const QString &match );

    static bool searchForJourneySearchKeywords( const QString &journeySearch,
            const QStringList &timeKeywordsTomorrow, const QStringList &departureKeywords,
            const QStringList &arrivalKeywords, QDate *date, QString *stop,
            bool *timeIsDeparture, int *len );

    static void combineDoubleQuotedWords( QStringList *words, bool reinsertQuotedWords = true );

    /**
     * @brief Get the strings on the left and right of the word at @p splitWordPos in @p wordList.
     *
     * The extracted strings are stored to @p leftOfSplitWord and @p rightOfSplitWord.
     **/
    static void splitWordList( const QStringList &wordList, int splitWordPos,
            QString *leftOfSplitWord, QString *rightOfSplitWord,
            int excludeWordsFromleft = 0 );
    static void parseDateAndTime( const QString &sDateTime, QDateTime *dateTime,
                    QDate *alreadyParsedDate );
    static bool parseTime( const QString &sTime, QTime *time );
    static bool parseDate( const QString &sDate, QDate *date );
};

#endif // Multiple inclusion guard
