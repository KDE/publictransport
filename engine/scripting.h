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
 * @brief This file contains helper classes to be used from inside accessor scripts.
 * 
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef SCRIPTING_HEADER
#define SCRIPTING_HEADER

#include <QObject>
#include <QHash>
#include <QTextCodec>

#include <KDebug>

#include "enums.h"
#include "timetableaccessor.h"
#include "timetableaccessor_html.h"

/**
 * @brief A helper class to be used from inside a script.
 * 
 * An instance of this class gets published to scripts as <em>"helper"</em>.
 * Scripts can use it's functions, like here:
 * @code
 * var stripped = helper.stripTags("<div>Test</div>");
 * // stripped == "Test"
 * 
 * var timeValues = helper.matchTime( "15:28" );
 * // timeValues == [15, 28]
 * 
 * var timeString = helper.formatTime( timeValues[0], timeValues[1] );
 * // timeString == "15:28"
 * 
 * var duration = helper.duration("15:20", "15:45");
 * // duration == 25
 * 
 * var time2 = helper.addMinsToTime("15:20", duration);
 * // time2 == "15:45"
 * 
 * helper.debug("Debug message, eg. something unexpected happened");
 * @endcode
 **/
class Helper : public QObject {
    Q_OBJECT 
    
public:
	/**
	 * @brief Creates a new helper object.
	 * 
	 * @param serviceProviderId The ID of the service provider this Helper object is created for.
	 * 
	 * @param parent The parent object.
	 **/
	Helper( const QString &serviceProviderId, QObject* parent = 0 ) : QObject( parent ) {
		m_serviceProviderId = serviceProviderId;
	};

public Q_SLOTS:
	/**
	 * @brief Prints @p message on stdout and logs it in a file.
	 *
	 * @param message The error message.
	 * @param failedParseText The text in the source document where parsing failed.
	 **/
	void error( const QString &message, const QString &failedParseText = QString() );
	
	/**
	 * @brief Trims spaces from the beginning and the end of the given string @p str.
	 *   The HTML entitiy <em>&nbsp;</em> is also trimmed.
	 *
	 * @param str The string to be trimmed.
	 * 
	 * @return @p str without spaces at the beginning or end.
	 **/
	QString trim( const QString &str ) {
		return str.trimmed().replace( QRegExp("^(&nbsp;)+|(&nbsp;)+$", Qt::CaseInsensitive), "" );
	};

	/**
	 * @brief Removes all HTML tags from str.
	 *
	 * @param str The string from which the HTML tags should be removed.
	 * 
	 * @return @p str without HTML tags.
	 **/
	QString stripTags( const QString &str ) {
		QRegExp rx( "<\\/?[^>]+>" );
		rx.setMinimal( true );
		return QString( str ).replace( rx, "" );
	};

	/**
	 * @brief Makes the first letter of each word upper case, all others lower case.
	 *
	 * @param str The input string.
	 * 
	 * @return @p str in camel case.
	 **/
	QString camelCase( const QString &str ) {
		QString ret = str.toLower();
		QRegExp rx( "(^\\w)|\\W(\\w)" );
		int pos = 0;
		while ( (pos = rx.indexIn(ret, pos)) != -1 ) {
			if ( rx.pos(2) < 0 || rx.pos(2) >= ret.length() ) { // TODO
				break;
			}
			ret[ rx.pos(2) ] = ret[ rx.pos(2) ].toUpper();
			pos += rx.matchedLength();
		}
		return ret;
	};

	/**
	 * @brief Extracts a block from @p str, which begins at the first occurance of @p beginString
	 *   in @p str and end at the first occurance of @p endString in @p str.
	 *
	 * @param str The input string.
	 * 
	 * @param beginString A string to search for in @p str and to use as start position.
	 * 
	 * @param endString A string to search for in @p str and to use as end position.
	 * 
	 * @return The text block in @p str between @p beginString and @p endString.
	 **/
	QString extractBlock( const QString &str,
						  const QString &beginString, const QString &endString ) {
		int pos = str.indexOf( beginString );
		if ( pos == -1 ) {
			return "";
		}

		int end = str.indexOf( endString, pos + 1 );
		return str.mid( pos, end - pos );
	};

	/**
	 * @brief Gets a list with the hour and minute values parsed from @p str, 
	 *   which is in the given @p format.
	 *
	 * @param str The string containing the time to be parsed, eg. "08:15".
	 * 
	 * @param format The format of the time string in @p str. Default is "hh:mm".
	 * 
	 * @return A list of two integers: The hour and minute values parsed from @p str.
	 * 
	 * @see formatTime
	 **/
	QVariantList matchTime( const QString &str, const QString &format = "hh:mm") {
		QString pattern = QRegExp::escape( format );
		pattern = pattern.replace( "hh", "\\d{2}" )
						 .replace( "h", "\\d{1,2}" )
						 .replace( "mm", "\\d{2}" )
						 .replace( "m", "\\d{1,2}" )
						 .replace( "AP", "(AM|PM)" )
						 .replace( "ap", "(am|pm)" );

		QVariantList ret;
		QRegExp rx( pattern );
		if ( rx.indexIn(str) != -1 ) {
			QTime time = QTime::fromString( rx.cap(), format );
			ret << time.hour() << time.minute();
		} else if ( format != "hh:mm" ) {
			// Try default format if the one specified doesn't work
			QRegExp rx2( "\\d{1,2}:\\d{2}" );
			if ( rx2.indexIn(str) != -1 ) {
				QTime time = QTime::fromString( rx2.cap(), "hh:mm" );
				ret << time.hour() << time.minute();
			}
		}
		return ret;
	};

	/**
	 * @brief Formats the time given by the values @p hour and @p minute 
	 *   as string in the given @p format.
	 *
	 * @param hour The hour value of the time.
	 * 
	 * @param minute The minute value of the time.
	 * 
	 * @param format The format of the time string to return. Default is "hh:mm".
	 * 
	 * @return The formatted time string.
	 * @see matchTime
	 **/
	QString formatTime( int hour, int minute, const QString &format = "hh:mm") {
		return QTime( hour, minute ).toString( format );
	};
	
	/**
	 * @brief Calculates the duration in minutes from the time in @p sTime1 until @p sTime2.
	 *
	 * @param sTime1 A string with the start time, in the given @p format.
	 * 
	 * @param sTime2 A string with the end time, in the given @p format.
	 * 
	 * @param format The format of @p sTime1 and @p sTime2. Default is "hh:mm".
	 * 
	 * @return The number of minutes from @p sTime1 until @p sTime2.
	 **/
	int duration( const QString &sTime1, const QString &sTime2,
				  const QString &format = "hh:mm" ) {
		QTime time1 = QTime::fromString( sTime1, format );
		QTime time2 = QTime::fromString( sTime2, format );
		if ( !time1.isValid() || !time2.isValid() ) {
			return -1;
		}
		return time1.secsTo( time2 ) / 60;
	};

	/**
	 * @brief Adds @p minsToAdd minutes to the time in @p sTime.
	 *
	 * @param sTime A string with the base time.
	 * 
	 * @param minsToAdd The number of minutes to add to @p sTime.
	 * 
	 * @param format The format of @p sTime. Default is "hh:mm".
	 * 
	 * @return A time string formatted in @p format with the calculated time.
	 **/
	QString addMinsToTime( const QString &sTime, int minsToAdd,
						   const QString &format = "hh:mm" ) {
		QTime time = QTime::fromString( sTime, format );
		if ( !time.isValid() ) {
			return "";
		}
		return time.addSecs( minsToAdd * 60 ).toString( format );
	};
	
	// TODO
	int* addDaysToDate( const int* dateArray, int daysToAdd ) {
		QDate date = QDate( dateArray[2], dateArray[1], dateArray[0] ).addDays( daysToAdd );
		int *ret = new int[3];
		ret[0] = date.day();
		ret[1] = date.month();
		ret[2] = date.year();
		return ret;
	};
	
	/**
	 * @brief Splits @p str at @p sep, but skips empty parts.
	 *
	 * @param str The string to split.
	 * 
	 * @param sep The separator.
	 * 
	 * @return A list of string parts.
	 **/
	QStringList splitSkipEmptyParts( const QString &str, const QString &sep ) {
		return str.split( sep, QString::SkipEmptyParts );
	};
	
private:
	QString m_serviceProviderId;
};

/**
 * @brief This class is used by scripts to store results in.
 * 
 * An instance of this class gets published to scripts as <em>"timetableData"</em>. 
 * Scripts can use it's functions, most important are @ref clear (to clear all values from the
 * TimetableData object) and @ref set (to set a timetable information to a value).
 * Once all values have been set, the script can add the current state to @p ResultObject,
 * available as <em>"result"</em>.
 * @ref value can be used to get a value of the TimtetableData object from a string (the name
 * of a timetable information like in @ref TimetableInformation).
 * @code
 * timetableData.clear(); // Clear all values
 * timetableData.set("TypeOfVehicle", "Bus"); // Set a value
 * var typeOfVehicle = timetableData.value("TypeOfVehicle"); // Get the value again
 * result.addData( timetableData ); // Add to result set
 * @endcode
 * 
 * @see ResultObject
 **/
class TimetableData : public QObject {
	Q_OBJECT

public:
	TimetableData( QObject* parent = 0 ) : QObject( parent ) {};

	TimetableData( const TimetableData &other ) : QObject( 0 ) {
		*this = other;
	};

	TimetableData &operator =( const TimetableData &other ) {
		m_values = other.values();
		return *this;
	};

public Q_SLOTS:
	/** @brief Removes all stored values. */
	void clear() { m_values.clear(); };

	/**
	 * @brief Stores @p value under the key @p timetableInformation.
	 * 
	 * This function uses @ref TimetableAccessor::timetableInformationFromString to get the
	 * @ref TimetableInformation from the given @p sTimetableInformation and use it as arguemnt
	 * for @ref set(TimetableInformation,QVariant).
	 *
	 * @param timetableInformation The type of the information in the given @p value.
	 * 
	 * @param value A value to be stored.
	 **/
	inline void set( const QString &sTimetableInformation, const QVariant &value ) {
		set( TimetableAccessor::timetableInformationFromString(sTimetableInformation), value ); };
	/**
	 * @brief Stores @p value under the key @p timetableInformation.
	 *
	 * @param timetableInformation The type of the information in the given @p value.
	 * 
	 * @param value A value to be stored.
	 **/
	void set( TimetableInformation timetableInformation, const QVariant &value );

	/**
	 * @brief Gets all values that are currently stored.
	 *
	 * @return A QHash with string keys (timetable information type, like "DepartureTime"), 
	 *   as given in @p set.
	 **/
	QHash<TimetableInformation, QVariant> values() const { return m_values; };
	/**
	 * @brief Gets the value for the given @p info.
	 *
	 * @param info The timetable information key, like in @ref TimetableInformation.
	 * 
	 * @return The value stored for @p info
	 **/
	QVariant value( TimetableInformation info ) const { return m_values[info]; };

private:
	QHash<TimetableInformation, QVariant> m_values;
};

/**
 * @brief This class is used by scripts to store results in.
 * 
 * An instance of this class gets published to scripts as <em>"result"</em>. 
 * Scripts can use it to add items to the result set, ie. departures/arrivals/journeys/
 * stop suggestions. An item can be added using @ref addData with the <em>"timetableData"</em> 
 * object as argument.
 * @code
 *   result.addData( timetableData ); // Add to result set
 * @endcode
 * 
 * @see TimetableData
 **/
class ResultObject : public QObject {
	Q_OBJECT

public:
	ResultObject( QObject* parent = 0 ) : QObject( parent ) {};

public Q_SLOTS:
	/** @brief Clears the list of stored TimetableData objects. */
	void clear() { m_timetableData.clear(); };
	/**
	 * @brief Checks whether or not the list of TimetableData objects is empty.
	 *
	 * @return True, if the list of TimetableData objects isn't empty. False, otherwise.
	 **/
	bool hasData() const { return !m_timetableData.isEmpty(); };

	/**
	 * @brief Adds the given @p departure to the list of TimetableData objects.
	 *
	 * @param departure The TimetableData object to add.
	 **/
	void addData( TimetableData *departure ) {
		TimetableData dep( *departure );
		m_timetableData.append( dep );
	};

	/**
	 * @brief Gets the list of stored TimetableData objects.
	 *
	 * @return The list of stored TimetableData objects.
	 **/
	QList< TimetableData > data() const { return m_timetableData; };

private:
	QList< TimetableData > m_timetableData;
};

#endif // SCRIPTING_HEADER
