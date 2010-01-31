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

#ifndef SCRIPTING_HEADER
#define SCRIPTING_HEADER

#include <QObject>
#include <QHash>
#include <QTextCodec>

#include <KDebug>

#include "enums.h"
#include "timetableaccessor.h"
#include "timetableaccessor_html.h"

class Helper : public QObject {
    Q_OBJECT 
    public:
	Helper( QObject* parent = 0 ) : QObject( parent ) {};

    public Q_SLOTS:
	QString trim( const QString &str ) {
	    return str.trimmed().replace( QRegExp("^(&nbsp;)+|(&nbsp;)+$",
						  Qt::CaseInsensitive), "" );
	};
	
	QString stripTags( const QString &str ) {
	    QRegExp rx( "<\\/?[^>]+>" );
	    rx.setMinimal( true );
	    return QString( str ).replace( rx, "" );
	};

	QString camelCase( const QString &str ) {
	    QString ret = str.toLower();
	    QRegExp rx( "(^\\w)|\\W(\\w)" );
	    int pos = 0;
	    while ( (pos = rx.indexIn(ret, pos)) != -1 ) {
		ret[ rx.pos(2) ] = ret[ rx.pos(2) ].toUpper();
		pos += rx.matchedLength();
	    }
	    return ret;
	};

	QString extractBlock( const QString &str,
			      const QString &beginString, const QString &endString ) {
	    int pos = str.indexOf( beginString );
	    if ( pos == -1 )
		return "";
	    
	    int end = str.indexOf( endString, pos + 1 );
	    return str.mid( pos, end - pos );
	};

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
	    }
	    return ret;
	};

	QString formatTime( int hour, int minute, const QString &format = "hh:mm") {
	    return QTime( hour, minute ).toString( format );
	};
	
	int duration( const QString &sTime1, const QString &sTime2,
		      const QString &format = "hh:mm" ) {
	    QTime time1 = QTime::fromString( sTime1, format );
	    QTime time2 = QTime::fromString( sTime2, format );
	    if ( !time1.isValid() || !time2.isValid() )
		return -1;
	    return time1.secsTo( time2 ) / 60;
	};

	QString addMinsToTime( const QString &sTime, int minsToAdd,
			       const QString &format = "hh:mm" ) {
	    QTime time = QTime::fromString( sTime, format );
	    if ( !time.isValid() )
		return "";
	    return time.addSecs( minsToAdd * 60 ).toString( format );
	};

	QStringList splitSkipEmptyParts( const QString &str, const QString &sep ) {
	    return str.split( sep, QString::SkipEmptyParts );
	};
};

class TimetableData : public QObject {
    Q_OBJECT
    
    public:
	TimetableData( QObject* parent = 0 ) : QObject( parent ) {};

	TimetableData( const TimetableData &other ) : QObject( 0 ) {
		*this = other; };

	TimetableData &operator =( const TimetableData &other ) {
		m_values = other.values();
		return *this; };
	
    public Q_SLOTS:
	void clear() { m_values.clear(); };

	void set( const QString &sTimetableInformation, const QVariant &value );
	
	QHash<TimetableInformation, QVariant> values() const { return m_values; };
	QVariant value( TimetableInformation info ) const { return m_values[info]; };

    private:
	QHash<TimetableInformation, QVariant> m_values;
};

// This is our QObject our scripting code will access
class ResultObject : public QObject {
    Q_OBJECT
    
    public:
	ResultObject( QObject* parent = 0 ) : QObject( parent ) {};
	
    public Q_SLOTS:
	void clear() { m_timetableData.clear(); };
	bool hasData() const { return !m_timetableData.isEmpty(); };

	void addData( TimetableData *departure ) {
		TimetableData dep( *departure );
		m_timetableData.append( dep ); };
	
	QList< TimetableData > data() const { return m_timetableData; };

    private:
	QList< TimetableData > m_timetableData;
};

#endif // SCRIPTING_HEADER
