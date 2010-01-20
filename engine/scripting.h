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
#include "enums.h"
#include "timetableaccessor.h"
#include <KDebug>
#include "timetableaccessor_html.h"

class Helper : public QObject {
    Q_OBJECT 
    public:
	Helper( QObject* parent = 0 ) : QObject( parent ) {};

    public Q_SLOTS:
	QString stripTags( const QString &str ) {
	    QRegExp rx( "<\\/?[^>]+>" );
	    rx.setMinimal( true );
	    return QString( str ).replace( rx, "" );
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
