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

#include "filter.h"
#include "departureinfo.h"

#include <KDebug>

bool FilterSettings::filterOut( const DepartureInfo& departureInfo ) const {
    switch ( filterAction ) {
	case ShowMatching:
	    return !filters.match( departureInfo );
	case HideMatching:
	    return filters.match( departureInfo );
    }

    return false;
}

bool FilterList::match( const DepartureInfo& departureInfo ) const {
    foreach ( const Filter &filter, *this ) {
	if ( filter.match(departureInfo) )
	    return true;
    }

    return false;
}

bool Filter::match( const DepartureInfo& departureInfo ) const {
    bool viaMatched;
    foreach ( const Constraint &constraint, *this ) {
	switch ( constraint.type ) {
	    case FilterByTarget:
		if ( !matchString(constraint.variant, constraint.value.toString(),
				  departureInfo.target()) )
		    return false;
		break;
	    case FilterByVia:
		viaMatched = false;
		foreach ( const QString &via, departureInfo.routeStops() ) {
		    if ( matchString(constraint.variant, constraint.value.toString(), via) ) {
			viaMatched = true;
			break;
		    }
		}
		if ( !viaMatched )
		    return false;
		break;
	    case FilterByTransportLine:
		if ( !matchString(constraint.variant, constraint.value.toString(),
				  departureInfo.lineString()) )
		    return false;
		break;

	    case FilterByTransportLineNumber:
		if ( departureInfo.lineNumber() <= 0 ) {
		    // Invalid line numbers only match with variant DoesntEqual
		    return constraint.variant == FilterDoesntEqual;
		} else if ( !matchInt(constraint.variant, constraint.value.toInt(),
				      departureInfo.lineNumber()) ) {
		    return false;
		}
		break;
	    case FilterByDelay:
		if ( departureInfo.delay() < 0 ) {
		    // Invalid delays only match with variant DoesntEqual
		    return constraint.variant == FilterDoesntEqual;
		} else if ( !matchInt(constraint.variant, constraint.value.toInt(),
				      departureInfo.delay()) )
		    return false;
		break;

	    case FilterByVehicleType:
		if ( !matchList(constraint.variant, constraint.value.toList(),
				static_cast<int>(departureInfo.vehicleType())) )
		    return false;
		break;

	    case FilterByDeparture:
		if ( !matchTime(constraint.variant, constraint.value.toTime(),
			        departureInfo.departure().time()) )
		    return false;
		break;
	    case FilterByDayOfWeek:
		if ( !matchList(constraint.variant, constraint.value.toList(),
				static_cast<int>(departureInfo.departure().date().dayOfWeek())) )
		    return false;
		break;

	    default:
		kDebug() << "Filter unknown or invalid" << constraint.type;
	}
    }

    return true;
}

bool Filter::matchList( FilterVariant variant, const QVariantList& filterValues,
			const QVariant& testValue ) const {
    switch ( variant ) {
// 	case FilterEquals:
// 	case FilterDoesntEqual:
	case FilterIsOneOf:
	    return filterValues.contains( testValue );
	case FilterIsntOneOf:
	    return !filterValues.contains( testValue );

	default:
	    kDebug() << "Invalid filter variant for list matching:" << variant;
	    return false;
    }
}

bool Filter::matchInt( FilterVariant variant, int filterInt, int testInt ) const {
    switch ( variant ) {
	case FilterEquals:
	    return filterInt == testInt;
	case FilterDoesntEqual:
	    return filterInt != testInt;
	case FilterGreaterThan:
	    return testInt > filterInt;
	case FilterLessThan:
	    return testInt < filterInt;

	default:
	    kDebug() << "Invalid filter variant for integer matching:" << variant;
	    return false;
    }
}

bool Filter::matchString( FilterVariant variant, const QString& filterString,
			  const QString& testString ) const {
    switch ( variant ) {
	case FilterContains:
	    return testString.contains( filterString, Qt::CaseInsensitive );
	case FilterDoesntContain:
	    return !testString.contains( filterString, Qt::CaseInsensitive );

	case FilterEquals:
	    return testString.compare( filterString, Qt::CaseInsensitive ) == 0;
	case FilterDoesntEqual:
	    return testString.compare( filterString, Qt::CaseInsensitive ) != 0;

	case FilterMatchesRegExp:
	    return QRegExp( filterString ).indexIn( testString ) != -1;
	case FilterDoesntMatchRegExp:
	    return QRegExp( filterString ).indexIn( testString ) == 0;

	default:
	    kDebug() << "Invalid filter variant for string matching:" << variant;
	    return false;
    }
}

bool Filter::matchTime( FilterVariant variant, const QTime& filterTime,
			const QTime& testTime ) const {
    switch ( variant ) {
	case FilterEquals:
	    return testTime == filterTime;
	case FilterDoesntEqual:
	    return testTime != filterTime;

	case FilterGreaterThan:
	    return testTime > filterTime;
	case FilterLessThan:
	    return testTime < filterTime;

	default:
	    kDebug() << "Invalid filter variant for time matching:" << variant;
	    return false;
    }
}

QByteArray Filter::toData() const {
    QByteArray ba;
    QDataStream stream( &ba, QIODevice::WriteOnly );
    stream << *this;
    return ba;
}

void Filter::fromData( const QByteArray& ba ) {
    QDataStream stream( ba );
    stream >> *this;
}

QByteArray FilterList::toData() const {
    QByteArray ba;
    QDataStream stream( &ba, QIODevice::WriteOnly );
    stream << *this;
    return ba;
}

void FilterList::fromData( const QByteArray& ba ) {
    QDataStream stream( ba );
    stream >> *this;
}

QDataStream& operator<<( QDataStream& out, const FilterList &filterList ) {
    out << filterList.count();
    foreach ( const Filter &filter, filterList )
	out << filter;
    return out;
}

QDataStream& operator>>( QDataStream& in, FilterList &filterList ) {
    filterList.clear();
    
    int count;
    in >> count;
    for ( int i = 0; i < count; ++i ) {
	Filter filter;
	in >> filter;
	filterList << filter;
    }
    
    return in;
}

QDataStream& operator<<( QDataStream& out, const Filter &filter ) {
    out << filter.count();
    foreach ( const Constraint &constraint, filter ) {
	out << constraint.type;
	out << constraint.variant;
	
	QVariantList list;
	switch ( constraint.type ) {
	    case FilterByVehicleType:
	    case FilterByDayOfWeek:
		list = constraint.value.toList();
		out << list.count();
		foreach ( const QVariant &value, list )
		    out << value.toInt();
		break;
		
	    case FilterByTarget:
	    case FilterByVia:
	    case FilterByTransportLine:
		out << constraint.value.toString();
		break;
		
	    case FilterByTransportLineNumber:
	    case FilterByDelay:
		out << constraint.value.toInt();
		break;

	    case FilterByDeparture:
		out << constraint.value.toTime();
		break;
		
	    default:
		kDebug() << "Unknown filter type" << constraint.type;
	}
    }
    return out;
}

QDataStream& operator>>( QDataStream& in, Filter &filter ) {
    filter.clear();
    
    int count;
    in >> count;
    for ( int n = 0; n < count; ++n ) {
	int type, variant;
	in >> type;
	in >> variant;
	Constraint constraint;
	constraint.type = static_cast< FilterType >( type );
	constraint.variant = static_cast< FilterVariant >( variant );
	
	QVariantList list;
	QVariant v;
	QString s;
	int i, count;
	QTime time;
	switch ( constraint.type ) {
	    case FilterByVehicleType:
	    case FilterByDayOfWeek:
		in >> count;
		for ( int m = 0; m < count; ++m ) {
		    in >> i;
		    list << i;
		}
		constraint.value = list;
		break;
		
	    case FilterByTarget:
	    case FilterByVia:
	    case FilterByTransportLine:
		in >> s;
		constraint.value = s;
		break;
		
	    case FilterByTransportLineNumber:
	    case FilterByDelay:
		in >> i;
		constraint.value = i;
		break;
		
	    case FilterByDeparture:
		in >> time;
		constraint.value = time;
		break;
		
	    default:
		kDebug() << "Unknown filter type" << constraint.type << type;
		constraint.type = FilterByVehicleType;
		constraint.variant = FilterIsOneOf;
		constraint.value = QVariant();
	}
	
	filter << constraint;
    }
    return in;
}

bool operator==( const Constraint& l, const Constraint& r ) {
    return l.type == r.type && l.variant == r.variant && l.value == r.value;
}

bool operator==( const FilterSettings& l, const FilterSettings& r ) {
    return l.filterAction == r.filterAction && l.filters == r.filters;
}
