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

// Own includes
#include "scripting.h"
#include "timetableaccessor_script.h"

// KDE includes
#include <KStandardDirs>
#include <KDebug>

// Qt includes
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QTextCodec>
#include <QWaitCondition>

void Helper::error( const QString& message, const QString &failedParseText )
{
    // Output debug message and a maximum count of 200 characters of the text where the parsing failed
    QString shortParseText = failedParseText.trimmed().left(350);
    int diff = failedParseText.length() - shortParseText.length();
    if ( diff > 0 ) {
        shortParseText.append(QString("... <%1 more chars>").arg(diff));
    }
    shortParseText = shortParseText.replace('\n', "\n    "); // Indent

    kDebug() << QString("Error in %1 (maybe the website layout changed): \"%2\"")
                .arg(m_serviceProviderId).arg(message);
    if ( !shortParseText.isEmpty() ) {
        kDebug() << QString("The text of the document where parsing failed is: \"%1\"")
                    .arg(shortParseText);
    }

    // Log the complete message to the log file.
    QString logFileName = KGlobal::dirs()->saveLocation( "data", "plasma_engine_publictransport" );
    logFileName.append( "accessors.log" );

    if ( !logFileName.isEmpty() ) {
        QFile logFile(logFileName);
        if ( logFile.size() > 1024 * 512 ) { // == 0.5 MB
            if ( !logFile.remove() ) {
                kDebug() << "Error: Couldn't delete old log file.";
            } else {
                kDebug() << "Deleted old log file, because it was getting too big.";
            }
        }

        if ( !logFile.open(QIODevice::Append | QIODevice::Text) ) {
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
        return;
    }

    if ( !value.data() ) {
        // Can crash sometimes (reason unknown..)
        kDebug() << "The value given by the script isn't valid for" << info;
        return;
    }

    if ( !value.isValid() || value.isNull() ) {
        kDebug() << "Value is invalid or null for" << info;
    } else {
        if ( value.canConvert(QVariant::String)
                && (info == StopName || info == Target
                    || info == StartStopName || info == TargetStopName
                    || info == Operator || info == TransportLine
                    || info == Platform || info == DelayReason
                    || info == Status || info == Pricing) ) {
            m_values[ info ] = TimetableAccessorScript::decodeHtmlEntities( value.toString() );
        } else if ( value.canConvert(QVariant::List) && info == DepartureDate ) {
            QVariantList date = value.toList();
            m_values[ info ] = date.length() == 3 ? QDate(date[0].toInt(), date[1].toInt(), date[2].toInt()) : value;
        } else if ( value.canConvert(QVariant::StringList)
                && (info == RouteStops || info == RoutePlatformsDeparture
                    || info == RoutePlatformsArrival) )
        {
            QStringList stops = value.toStringList();
            QStringList::iterator it = stops.begin();
            while ( it != stops.end() ) {
                *it = TimetableAccessorScript::decodeHtmlEntities( *it )
                        .remove( QRegExp("^(&nbsp;)+|(&nbsp;)+$", Qt::CaseInsensitive) )
                        .trimmed();
                ++it;
            }
            m_values[ info ] = stops;
        } else {
            m_values[ info ] = value;
        }
    }
}

QString Helper::trim( const QString& str )
{
    QString s = str;
    return s.trimmed().replace( QRegExp( "^(&nbsp;)+|(&nbsp;)+$", Qt::CaseInsensitive ), "" ).trimmed();
}

QString Helper::stripTags( const QString& str )
{
    QRegExp rx( "<\\/?[^>]+>" );
    rx.setMinimal( true );
    return QString( str ).replace( rx, "" );
}

QString Helper::camelCase( const QString& str )
{
    QString ret = str.toLower();
    QRegExp rx( "(^\\w)|\\W(\\w)" );
    int pos = 0;
    while (( pos = rx.indexIn( ret, pos ) ) != -1 ) {
        if ( rx.pos( 2 ) < 0 || rx.pos( 2 ) >= ret.length() ) { // TODO
            break;
        }
        ret[ rx.pos( 2 )] = ret[ rx.pos( 2 )].toUpper();
        pos += rx.matchedLength();
    }
    return ret;
}

QString Helper::extractBlock( const QString& str, const QString& beginString, const QString& endString )
{
    int pos = str.indexOf( beginString );
    if ( pos == -1 ) {
        return "";
    }

    int end = str.indexOf( endString, pos + 1 );
    return str.mid( pos, end - pos );
}

QVariantList Helper::matchTime( const QString& str, const QString& format )
{
    QString pattern = QRegExp::escape( format );
    pattern = pattern.replace( "hh", "\\d{2}" )
              .replace( "h", "\\d{1,2}" )
              .replace( "mm", "\\d{2}" )
              .replace( "m", "\\d{1,2}" )
              .replace( "AP", "(AM|PM)" )
              .replace( "ap", "(am|pm)" );

    QVariantList ret;
    QRegExp rx( pattern );
    if ( rx.indexIn( str ) != -1 ) {
        QTime time = QTime::fromString( rx.cap(), format );
        ret << time.hour() << time.minute();
    } else if ( format != "hh:mm" ) {
        // Try default format if the one specified doesn't work
        QRegExp rx2( "\\d{1,2}:\\d{2}" );
        if ( rx2.indexIn( str ) != -1 ) {
            QTime time = QTime::fromString( rx2.cap(), "hh:mm" );
            ret << time.hour() << time.minute();
        } else {
            kDebug() << "Couldn't match time in" << str << pattern;
        }
    } else {
        kDebug() << "Couldn't match time in" << str << pattern;
    }
    return ret;
}

QVariantList Helper::matchDate( const QString& str, const QString& format )
{
    QString pattern = QRegExp::escape( format ).replace( "d", "D" );
    pattern = pattern.replace( "DD", "\\d{2}" )
              .replace( "D", "\\d{1,2}" )
              .replace( "MM", "\\d{2}" )
              .replace( "M", "\\d{1,2}" )
              .replace( "yyyy", "\\d{4}" )
              .replace( "yy", "\\d{2}" );

    QVariantList ret;
    QRegExp rx( pattern );
    QDate date;
    if ( rx.indexIn(str) != -1 ) {
        date = QDate::fromString( rx.cap(), format );
    } else if ( format != "yyyy-MM-dd" ) {
        // Try default format if the one specified doesn't work
        QRegExp rx2( "\\d{2,4}-\\d{2}-\\d{2}" );
        if ( rx2.indexIn(str) != -1 ) {
            date = QDate::fromString( rx2.cap(), "yyyy-MM-dd" );
        } else {
            kDebug() << "Couldn't match date in" << str << pattern;
        }
    } else {
        kDebug() << "Couldn't match date in" << str << pattern;
    }

    // Adjust date, needed for formats with only two "yy" for year matching
    // A year 12 means 2012, not 1912
    if ( date.year() < 1970 ) {
        date.setDate( date.year() + 100, date.month(), date.day() );
    }
    return ret;
}

QString Helper::formatTime( int hour, int minute, const QString& format )
{
    return QTime( hour, minute ).toString( format );
}

QString Helper::formatDate( int year, int month, int day, const QString& format )
{
    return QDate( year, month, day ).toString( format );
}

int Helper::duration( const QString& sTime1, const QString& sTime2, const QString& format )
{
    QTime time1 = QTime::fromString( sTime1, format );
    QTime time2 = QTime::fromString( sTime2, format );
    if ( !time1.isValid() || !time2.isValid() ) {
        return -1;
    }
    return time1.secsTo( time2 ) / 60;
}

QString Helper::addMinsToTime( const QString& sTime, int minsToAdd, const QString& format )
{
    QTime time = QTime::fromString( sTime, format );
    if ( !time.isValid() ) {
        kDebug() << "Couldn't parse the given time" << sTime << format;
        return "";
    }
    return time.addSecs( minsToAdd * 60 ).toString( format );
}

QString Helper::addDaysToDate( const QString& sDate, int daysToAdd, const QString& format )
{
    QDate date = QDate::fromString( sDate, format ).addDays( daysToAdd );
    if ( !date.isValid() ) {
        kDebug() << "Couldn't parse the given date" << sDate << format;
        return sDate;
    }
    return date.toString( format );
}

QVariantList Helper::addDaysToDateArray( const QVariantList& values, int daysToAdd )
{
    if ( values.count() != 3 ) {
        kDebug() << "The first argument needs to be a list with three values (year, month, day)";
        return values;
    }

    QDate date( values[0].toInt(), values[1].toInt(), values[2].toInt() );
    date = date.addDays( daysToAdd );
    return QVariantList() << date.year() << date.month() << date.day();
}

QStringList Helper::splitSkipEmptyParts( const QString& str, const QString& sep )
{
    return str.split( sep, QString::SkipEmptyParts );
}

void TimetableData::set( const QString& sTimetableInformation, const QVariant& value )
{
    set( TimetableAccessor::timetableInformationFromString( sTimetableInformation ), value );
}














