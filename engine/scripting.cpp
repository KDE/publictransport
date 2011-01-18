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

#include "scripting.h"
#include <KStandardDirs>
#include <QFile>

void Helper::error( const QString& message, const QString &failedParseText )
{
	// Output debug message and a maximum count of 200 characters of the text where the parsing failed
	QString shortParseText = failedParseText.trimmed().left(200);
	int diff = failedParseText.length() - shortParseText.length();
	if ( diff > 0 ) {
		shortParseText.append(QString("... <%1 more chars>").arg(diff));
	}
	shortParseText = shortParseText.replace('\n', "\n    "); // Indent
	
	qDebug() << QString("Error in %1 (maybe the website layout changed): \"%2\"\n"
						"  The text of the document where parsing failed is: \"%3\"")
				.arg(m_serviceProviderId).arg(message).arg(shortParseText);
	
	// Log the complete message to the log file.
	QString logFileName = KGlobal::dirs()->saveLocation( "data", "plasma_engine_publictransport" );
	logFileName.append( "accessors.log" );
	
	if ( !logFileName.isEmpty() ) {
		QFile logFile(logFileName);
		if ( logFile.size() > 1024 * 1024 ) { // == 1 MB
			if ( !logFile.open(QIODevice::Truncate) ) {
				kDebug() << "Couldn't open the log file in truncate mode" << logFileName << logFile.errorString();
				return;
			} else {
				kDebug() << "Removed old log file contents, to prevent too much disk usage";
			}
		} else if ( !logFile.open(QIODevice::Append | QIODevice::Text) ) {
			kDebug() << "Couldn't open the log file in append mode" << logFileName << logFile.errorString();
			return;
		}
		
		logFile.write( QString("%1 (%2): \"%3\"\n   Failed while reading this text: \"%4\"\n")
				.arg(m_serviceProviderId).arg(QDateTime::currentDateTime().toString())
				.arg(message).arg(failedParseText.trimmed()).toUtf8() );
		logFile.close();
	}
}

void TimetableData::set( TimetableInformation info, const QVariant& value )
{
	if ( info == Nothing ) {
		kDebug() << "Unknown timetable information" << info
				 << "with value" << (value.isValid() ? (value.isNull() ? "null" : value.toString())
													 : "invalid");
	} else if ( !value.isValid() || value.isNull() ) {
		kDebug() << "Value is invalid or null for" << info;
	} else {
		if ( value.canConvert(QVariant::String)
				&& (info == StopName || info == Target
					|| info == StartStopName || info == TargetStopName
					|| info == Operator || info == TransportLine
					|| info == Platform || info == DelayReason
					|| info == Status || info == Pricing) ) {
			m_values[ info ] = TimetableAccessorHtml::decodeHtmlEntities( value.toString() );
		} else if ( value.canConvert(QVariant::List) && info == DepartureDate ) {
			QVariantList date = value.toList();
			m_values[ info ] = date.length() == 3 ? QDate(date[2].toInt(), date[1].toInt(), date[0].toInt()) : value;
		} else {
			m_values[ info ] = value;
		}
	}
}
