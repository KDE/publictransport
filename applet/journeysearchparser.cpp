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

#include "journeysearchparser.h"

#include <QStringList>
#include <QTime>

#include <KGlobal>
#include <KLocale>
#include <KLocalizedString>
#include <KLineEdit>
#include <KDebug>

const QStringList JourneySearchParser::arrivalKeywords()
{
	return i18nc( "A comma separated list of keywords for the journey search to indicate "
				  "that given times are meant as arrivals. The order is used for "
				  "autocompletion.\nNote: Keywords should be unique for each meaning.",
				  "arriving,arrive,arrival,arr" ).split( ',', QString::SkipEmptyParts );
}

const QStringList JourneySearchParser::departureKeywords()
{
	return i18nc( "A comma separated list of keywords for the journey search to indicate "
				  "that given times are meant as departures (default). The order is used "
				  "for autocompletion.\nNote: Keywords should be unique for each meaning.",
				  "departing,depart,departure,dep" ).split( ',', QString::SkipEmptyParts );
}

const QStringList JourneySearchParser::fromKeywords()
{
	return i18nc( "A comma separated list of keywords for the journey search, indicating "
				  "that a journey FROM the given stop should be searched. This keyword "
				  "needs to be placed at the beginning of the field.", "from" )
			.split( ',', QString::SkipEmptyParts );
}

const QStringList JourneySearchParser::toKeywords()
{
	return i18nc( "A comma separated list of keywords for the journey search, indicating "
				  "that a journey TO the given stop should be searched. This keyword needs "
				  "to be placed at the beginning of the field.", "to" )
			.split( ',', QString::SkipEmptyParts );
}

const QStringList JourneySearchParser::timeKeywordsAt()
{
	return i18nc( "A comma separated list of keywords for the journey search field, "
				  "indicating that a date/time string follows.\nNote: Keywords should be "
				  "unique for each meaning.", "at" ).split( ',', QString::SkipEmptyParts );
}

const QStringList JourneySearchParser::timeKeywordsIn()
{
	return i18nc( "A comma separated list of keywords for the journey search field, "
				  "indicating that a relative time string follows.\nNote: Keywords should "
				  "be unique for each meaning.", "in" ).split( ',', QString::SkipEmptyParts );
}

const QStringList JourneySearchParser::timeKeywordsTomorrow()
{
	return i18nc( "A comma separated list of keywords for the journey search field, as "
				  "replacement for tomorrows date.\nNote: Keywords should be unique for "
				  "each meaning.", "tomorrow" ).split( ',', QString::SkipEmptyParts );
}

const QString JourneySearchParser::relativeTimeString( const QVariant &value )
{
	return i18nc( "The automatically added relative time string, when the journey "
				  "search line ends with the keyword 'in'. This should be match by the "
				  "regular expression for a relative time, like '(in) 5 minutes'. That "
				  "regexp and the keyword ('in') are also localizable. Don't include "
				  "the 'in' here.", "%1 minutes", value.toString() );
}

const QString JourneySearchParser::relativeTimeStringPattern()
{
	return i18nc( "This is a regular expression used to match a string after the "
				  "'in' keyword in the journey search line. The english version matches "
				  "'strings like '5 mins.', '1 minute', ... '\\d+' stands for at least "
				  "'one digit, '\\.' is just a point, a '?' after a character means "
				  "that it's optional (eg. the 's' in 'mins?' is optional to match "
				  "singular and plural forms). Normally you will only have to translate "
				  "'mins?' and 'minutes?'. The regexp must include one pair of matching "
				  "'parantheses, that match an int (the number of minutes from now). "
				  "Note: '(?:...)' are non-matching parantheses.",
				  "(\\d+)\\s+(?:mins?\\.?|minutes?)" );
}

void JourneySearchParser::doCorrections( KLineEdit *lineEdit, QString *searchLine, int cursorPos,
										 const QStringList &words, int removedWordsFromLeft )
{
	int selStart = -1;
	int selLength = 0;

	int pos = searchLine->lastIndexOf( ' ', cursorPos - 1 );
	int posEnd = searchLine->indexOf( ' ', cursorPos );
	if ( posEnd == -1 ) {
		posEnd = searchLine->length();
	}
	QString lastWordBeforeCursor;
	if ( posEnd == cursorPos && pos != -1 && !( lastWordBeforeCursor = searchLine->mid(
					pos, posEnd - pos ).trimmed() ).isEmpty() ) {
		if ( timeKeywordsAt().contains( lastWordBeforeCursor, Qt::CaseInsensitive ) ) {
			// Automatically add the current time after 'at'
			QString formattedTime = KGlobal::locale()->formatTime( QTime::currentTime() );
			searchLine->insert( posEnd, ' ' + formattedTime );
			selStart = posEnd + 1; // +1 for the added space
			selLength = formattedTime.length();
		} else if ( timeKeywordsIn().contains( lastWordBeforeCursor, Qt::CaseInsensitive ) ) {
			// Automatically add '5 minutes' after 'in'
			searchLine->insert( posEnd, ' ' + relativeTimeString() );
			selStart = posEnd + 1; // +1 for the added space
			selLength = 1; // only select the number (5)
		} else {
			QStringList completionItems;
			completionItems << timeKeywordsAt() << timeKeywordsIn() << timeKeywordsTomorrow()
					<< departureKeywords() << arrivalKeywords();

			KCompletion *comp = lineEdit->completionObject( false );
			comp->setItems( completionItems );
			comp->setIgnoreCase( true );
			QString completion = comp->makeCompletion( lastWordBeforeCursor );
			setJourneySearchWordCompletion( lineEdit, completion );
		}
	}

	// Select an appropriate substring after inserting something
	if ( selStart != -1 ) {
		QStringList removedWords = ( QStringList )words.mid( 0, removedWordsFromLeft );
		QString removedPart = removedWords.join( " " ).trimmed();
		QString correctedSearch;
		if ( removedPart.isEmpty() ) {
			correctedSearch = *searchLine;
		} else {
			correctedSearch = removedPart + ' ' + *searchLine;
			selStart += removedPart.length() + 1;
		}
		lineEdit->setText( correctedSearch );
		lineEdit->setSelection( selStart, selLength );
	}
}

bool JourneySearchParser::isInsideQuotedString( const QString& testString, int cursorPos )
{
	int posQuotes1 = testString.indexOf( '\"' );
	int posQuotes2 = testString.indexOf( '\"', posQuotes1 + 1 );
	if ( posQuotes2 == -1 ) {
		posQuotes2 = testString.length();
	}
	return posQuotes1 == -1 ? false : cursorPos > posQuotes1 && cursorPos <= posQuotes2;
}

bool JourneySearchParser::parseJourneySearch( KLineEdit* lineEdit,
		const QString& search, QString* stop, QDateTime* departure,
		bool* stopIsTarget, bool* timeIsDeparture, int* posStart, int* len, bool correctString )
{
	kDebug() << search;
	// Initialize output parameters
	stop->clear();
	*departure = QDateTime();
	*stopIsTarget = true;
	*timeIsDeparture = true;
	if ( posStart ) {
		*posStart = -1;
	}
	if ( len ) {
		*len = 0;
	}

	QString searchLine = search;
	int removedWordsFromLeft = 0;

	// Remove double spaces without changing the cursor position or selection
	int selStart = lineEdit->selectionStart();
	int selLength = lineEdit->selectedText().length();
	int cursorPos = lineEdit->cursorPosition();
	cursorPos -= searchLine.left( cursorPos ).count( "  " );
	if ( selStart != -1 ) {
		selStart -= searchLine.left( selStart ).count( "  " );
		selLength -= searchLine.mid( selStart, selLength ).count( "  " );
	}

	searchLine = searchLine.replace( "  ", " " );
	lineEdit->setText( searchLine );
	lineEdit->setCursorPosition( cursorPos );
	if ( selStart != -1 ) {
		lineEdit->setSelection( selStart, selLength );
	}

	// Get word list
	QStringList words = searchLine.split( ' ', QString::SkipEmptyParts );
	if ( words.isEmpty() ) {
		return false;
	}

	// Combine words between double quotes to one word
	// to allow stop names containing keywords.
	JourneySearchParser::combineDoubleQuotedWords( &words );

	// First search for keywords at the beginning of the string ('to' or 'from')
	QString firstWord = words.first();
	if ( toKeywords().contains( firstWord, Qt::CaseInsensitive ) ) {
		searchLine = searchLine.mid( firstWord.length() + 1 );
		cursorPos -= firstWord.length() + 1;
		++removedWordsFromLeft;
	} else if ( fromKeywords().contains( firstWord, Qt::CaseInsensitive ) ) {
		searchLine = searchLine.mid( firstWord.length() + 1 );
		*stopIsTarget = false; // the given stop is the origin
		cursorPos -= firstWord.length() + 1;
		++removedWordsFromLeft;
	}
	if ( posStart ) {
		QStringList removedWords = ( QStringList )words.mid( 0, removedWordsFromLeft );
		*posStart = removedWords.isEmpty() ? 0 : removedWords.join( " " ).length() + 1;
	}

	// Do corrections
	if ( correctString && !isInsideQuotedString( searchLine, cursorPos )
				&& lineEdit->completionMode() != KGlobalSettings::CompletionNone ) {
		doCorrections( lineEdit, &searchLine, cursorPos, words, removedWordsFromLeft );
	}

	// Now search for keywords inside the string from back
	for ( int i = words.count() - 1; i >= removedWordsFromLeft; --i ) {
		QString word = words[ i ];
		if ( timeKeywordsAt().contains( word, Qt::CaseInsensitive ) ) {
			// An 'at' keyword was found at position i
			QString sDeparture;
			JourneySearchParser::splitWordList( words, i, stop, &sDeparture, removedWordsFromLeft );

			// Search for keywords before 'at'
			QDate date;
			JourneySearchParser::searchForJourneySearchKeywords( *stop,
			        timeKeywordsTomorrow(), departureKeywords(), arrivalKeywords(),
			        &date, stop, timeIsDeparture, len );

			// Parse date and/or time from the string after 'at'
			JourneySearchParser::parseDateAndTime( sDeparture, departure, &date );
			return true;
		} else if ( timeKeywordsIn().contains( word, Qt::CaseInsensitive ) ) {
			// An 'in' keyword was found at position i
			QString sDeparture;
			JourneySearchParser::splitWordList( words, i, stop, &sDeparture, removedWordsFromLeft );

			// Match the regexp and extract the relative datetime value
			QRegExp rx( relativeTimeStringPattern(), Qt::CaseInsensitive );
			int pos = rx.indexIn( sDeparture );
			if ( pos != -1 ) {
				int minutes = rx.cap( 1 ).toInt();

				// Search for keywords before 'in'
				QDate date = QDate::currentDate();
				JourneySearchParser::searchForJourneySearchKeywords( *stop,
						timeKeywordsTomorrow(), departureKeywords(), arrivalKeywords(),
						&date, stop, timeIsDeparture, len );
				*departure = QDateTime( date, QTime::currentTime().addSecs( minutes * 60 ) );
				return true;
			}
		}
	}

	*stop = searchLine;

	// Search for keywords at the end of the string
	QDate date = QDate::currentDate();
	JourneySearchParser::searchForJourneySearchKeywords( *stop, timeKeywordsTomorrow(),
			departureKeywords(), arrivalKeywords(),
			&date, stop, timeIsDeparture, len );
	*departure = QDateTime( date, QTime::currentTime() );
	return false;
}

QHash< JourneySearchParser::Keyword, QVariant > JourneySearchParser::keywordValues(
		const QString& searchLine )
{
	QHash< Keyword, QVariant > ret;

	// Get word list
	QStringList words = searchLine.split( ' ', QString::SkipEmptyParts );
	if ( words.isEmpty() ) {
		return ret;
	}

	// Combine words between double quotes to one word
	// to allow stop names containing keywords.
	JourneySearchParser::combineDoubleQuotedWords( &words );

	// First search for keywords at the beginning of the string ('to' or 'from')
	int removedWordsFromLeft = 0;
	QString firstWord = words.first();
	if ( toKeywords().contains( firstWord, Qt::CaseInsensitive ) ) {
		ret.insert( KeywordTo, true );
		++removedWordsFromLeft;
	} else if ( fromKeywords().contains( firstWord, Qt::CaseInsensitive ) ) {
		ret.insert( KeywordFrom, true );
		++removedWordsFromLeft;
	}

	for ( int i = words.count() - 1; i >= removedWordsFromLeft; --i ) {
		QString word = words[ i ];
		if ( timeKeywordsAt().contains( word, Qt::CaseInsensitive ) ) {
			// An 'at' keyword was found at position i
			QString sDeparture, stop;
			QDate date;
			JourneySearchParser::splitWordList( words, i, &stop, &sDeparture, removedWordsFromLeft );

			// Parse date and/or time from the string after 'at'
			QDateTime departure;
			JourneySearchParser::parseDateAndTime( sDeparture, &departure, &date );
			ret.insert( KeywordTimeAt, departure );
			break;
		} else if ( timeKeywordsIn().contains( word, Qt::CaseInsensitive ) ) {
			// An 'in' keyword was found at position i
			QString sDeparture, stop;
			JourneySearchParser::splitWordList( words, i, &stop, &sDeparture, removedWordsFromLeft );

			// Match the regexp and extract the relative datetime value
			QRegExp rx( relativeTimeStringPattern(), Qt::CaseInsensitive );
			int pos = rx.indexIn( sDeparture );
			if ( pos != -1 ) {
				int minutes = rx.cap( 1 ).toInt();
				ret.insert( KeywordTimeIn, minutes );
				break;
			}
		}
	}

	return ret;
}

void JourneySearchParser::stopNamePosition( KLineEdit *lineEdit,
											int* posStart, int* len, QString* stop )
{
	QString stopName;
	bool stopIsTarget, timeIsDeparture;
	QDateTime departure;
	JourneySearchParser::parseJourneySearch( lineEdit, lineEdit->text(),
			&stopName, &departure, &stopIsTarget,
			&timeIsDeparture, posStart, len, false );
	if ( stop ) {
		*stop = stopName;
	}
}

void JourneySearchParser::setJourneySearchStopNameCompletion(
		KLineEdit *lineEdit, const QString &completion )
{
	kDebug() << "MATCH" << completion;
	if ( completion.isEmpty() ) {
		return;
	}

	int stopNamePosStart, stopNameLen;
	JourneySearchParser::stopNamePosition( lineEdit, &stopNamePosStart, &stopNameLen );
	kDebug() << "STOPNAME =" << lineEdit->text().mid( stopNamePosStart, stopNameLen );

	int selStart = lineEdit->selectionStart();
	if ( selStart == -1 ) {
		selStart = lineEdit->cursorPosition();
	}
	bool stopNameChanged = selStart > stopNamePosStart && selStart + lineEdit->selectedText().length()
	                       <= stopNamePosStart + stopNameLen;
	int posStart, len;
	if ( stopNameChanged ) {
		posStart = stopNamePosStart;
		len = stopNameLen;

		lineEdit->setText( lineEdit->text().replace( posStart, len, completion ) );
		lineEdit->setSelection( posStart + len, completion.length() - len );
	}
}

void JourneySearchParser::setJourneySearchWordCompletion( KLineEdit* lineEdit,
		const QString& match )
{
	kDebug() << "MATCH" << match;
	if ( match.isEmpty() ) {
		return;
	}

	int posStart = lineEdit->text().lastIndexOf( ' ', lineEdit->cursorPosition() - 1 ) + 1;
	if ( posStart == -1 ) {
		posStart = 0;
	}

	int posEnd = lineEdit->text().indexOf( ' ', lineEdit->cursorPosition() );
	if ( posEnd == -1 ) {
		posEnd = lineEdit->text().length();
	}

	int len = posEnd - posStart;
	if ( len == lineEdit->text().length() ) {
		kDebug() << "I'm not going to replace the whole word.";
		return; // Don't replace the whole word
	}
	kDebug() << "Current Word" << lineEdit->text().mid( posStart, len ) << posStart
			 << len << lineEdit->cursorPosition();

	lineEdit->setText( lineEdit->text().replace( posStart, len, match ) );
	lineEdit->setSelection( posStart + len, match.length() - len );
}

bool JourneySearchParser::searchForJourneySearchKeywords( const QString& journeySearch,
		const QStringList& timeKeywordsTomorrow, const QStringList& departureKeywords,
		const QStringList& arrivalKeywords, QDate* date, QString* stop,
		bool* timeIsDeparture, int* len )
{
	if ( stop->startsWith( '\"' ) && stop->endsWith( '\"' ) ) {
		if ( len ) {
			*len = stop->length();
		}
		*stop = stop->mid( 1, stop->length() - 2 );
		return false;
	} else if ( stop->trimmed().isEmpty() ) {
		if ( len ) {
			*len = 0;
		}
		stop->clear();
		return false;
	}

	bool found = false, continueSearch;
	do {
		continueSearch = false;

		// If the tomorrow keyword is found, set date to tomorrow
		QStringList wordsStop = journeySearch.split( ' ', QString::SkipEmptyParts );
		if ( wordsStop.isEmpty() ) {
			continue;
		}
		QString lastWordInStop = wordsStop.last();
		if ( !lastWordInStop.isEmpty() && timeKeywordsTomorrow.contains(
		            lastWordInStop, Qt::CaseInsensitive ) ) {
			*stop = stop->left( stop->length() - lastWordInStop.length() ).trimmed();
			*date = QDate::currentDate().addDays( 1 );

			found = continueSearch = true;
			lastWordInStop = wordsStop.count() >= 2 ? wordsStop[ wordsStop.count() - 2 ] : "";
		}

		// Search for departure / arrival keywords
		if ( !lastWordInStop.isEmpty() ) {
			if ( departureKeywords.contains( lastWordInStop, Qt::CaseInsensitive ) ) {
				// If a departure keyword is found, use given time as departure time
				*stop = stop->left( stop->length() - lastWordInStop.length() ).trimmed();
				*timeIsDeparture = true;
				found = continueSearch = true;
			} else if ( arrivalKeywords.contains( lastWordInStop, Qt::CaseInsensitive ) ) {
				// If an arrival keyword is found, use given time as arrival time
				*stop = stop->left( stop->length() - lastWordInStop.length() ).trimmed();
				*timeIsDeparture = false;
				found = continueSearch = true;
			}
		}
	} while ( continueSearch );

	if ( len ) {
		*len = stop->length();
	}
	if ( stop->startsWith('\"') && stop->endsWith('\"') ) {
		*stop = stop->mid( 1, stop->length() - 2 );
	}
	return found;
}

QStringList JourneySearchParser::notDoubleQuotedWords( const QString& searchLine )
{
	QStringList words = searchLine.split( ' ', QString::SkipEmptyParts );
	combineDoubleQuotedWords( &words, false );
	return words;
}

void JourneySearchParser::combineDoubleQuotedWords( QStringList* words, bool reinsertQuotedWords )
{
	int quotedStart = -1, quotedEnd = -1;
	for ( int i = 0; i < words->count(); ++i ) {
		if ( words->at( i ).startsWith( '\"' ) ) {
			quotedStart = i;
		}
		if ( words->at( i ).endsWith( '\"' ) ) {
			quotedEnd = i;
			break;
		}
	}
	if ( quotedStart != -1 ) {
		if ( quotedEnd == -1 ) {
			quotedEnd = words->count() - 1;
		}

		// Combine words
		QString combinedWord;
		for ( int i = quotedEnd; i >= quotedStart; --i ) {
			combinedWord = words->takeAt( i ) + ' ' + combinedWord;
		}

		if ( reinsertQuotedWords ) {
			words->insert( quotedStart, combinedWord.trimmed() );
		}
	}
}

void JourneySearchParser::splitWordList( const QStringList& wordList, int splitWordPos,
		QString* leftOfSplitWord, QString* rightOfSplitWord, int excludeWordsFromleft )
{
	*leftOfSplitWord = ((QStringList)wordList.mid(excludeWordsFromleft,
			splitWordPos - excludeWordsFromleft)).join(" ");
	*rightOfSplitWord = ((QStringList)wordList.mid(splitWordPos + 1,
				wordList.count() - splitWordPos)).join(" ");
}

void JourneySearchParser::parseDateAndTime( const QString& sDateTime,
											QDateTime* dateTime, QDate* alreadyParsedDate )
{
	QDate date;
	QTime time;
	bool callParseDate = alreadyParsedDate->isNull();

	// Parse date and/or time from sDateTime
	QStringList timeValues = sDateTime.split( QRegExp( "\\s|," ), QString::SkipEmptyParts );
	if ( timeValues.count() >= 2 ) {
		if ( callParseDate && !parseDate(timeValues[0], &date)
					&& !parseDate(timeValues[1], &date) ) {
			date = QDate::currentDate();
		} else {
			date = *alreadyParsedDate;
		}

		if ( !parseTime(timeValues[1], &time) && !parseTime(timeValues[0], &time) ) {
			time = QTime::currentTime();
		}
	} else {
		if ( !parseTime(sDateTime, &time) ) {
			time = QTime::currentTime();
			if ( callParseDate && !parseDate(sDateTime, &date) ) {
				date = QDate::currentDate();
			} else {
				date = *alreadyParsedDate;
			}
		} else if ( callParseDate ) {
			date = QDate::currentDate();
		} else {
			date = *alreadyParsedDate;
		}
	}

	*dateTime = QDateTime( date, time );
}

bool JourneySearchParser::parseDate( const QString& sDate, QDate* date )
{
	if ( sDate == i18nc( "@info/plain Used as date keyword in the journey search string", "today" ) ) {
		*date = QDate::currentDate();
		return true;
	} else if ( sDate == i18nc( "@info/plain Used as date keyword in the journey search string", "tomorrow" ) ) {
		*date = QDate::currentDate().addDays( 1 );
		return true;
	}

	bool ok;
	*date = KGlobal::locale()->readDate( sDate, &ok );
	if ( !ok ) {
		// Allow date input without year
		if ( sDate.count( '-' ) == 1 ) { // like 12-31
			*date = KGlobal::locale()->readDate(
					QDate::currentDate().toString( "yy" ) + '-' + sDate, &ok );
		} else if ( sDate.count( '.' ) == 1 ) { // like 31.12
			*date = KGlobal::locale()->readDate(
					sDate + '.' + QDate::currentDate().toString( "yy" ), &ok );
		} else if ( sDate.count( '.' ) == 2 && sDate.endsWith( '.' ) ) { // like 31.12.
			*date = KGlobal::locale()->readDate(
					sDate + QDate::currentDate().toString( "yy" ), &ok );
		}

		if ( !ok ) {
			*date = QDate::currentDate();
		}
	}
	return ok;
}

bool JourneySearchParser::parseTime( const QString& sTime, QTime* time )
{
	if ( sTime == i18nc( "@info/plain", "now" ) ) {
		*time = QTime::currentTime();
		return true;
	}

	bool ok;
	*time = KGlobal::locale()->readTime( sTime, &ok );
	if ( !ok ) {
		*time = QTime::currentTime();
	}
	return ok;
}
