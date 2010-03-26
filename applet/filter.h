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

#ifndef FILTER_HEADER
#define FILTER_HEADER

#include "global.h"

#include <QVariant>

/** A single constraint.
* @Note You can create a widget to show/edit this constraint with
* @ref ConstraintWidget::create. */
struct Constraint {
    FilterType type;
    FilterVariant variant;
    QVariant value;
    
    Constraint() {
	type = FilterByVehicleType;
	variant = FilterIsOneOf;
	value = QVariantList() << static_cast< int >( Unknown );
    };
    
    Constraint( FilterType type, FilterVariant variant, const QVariant &value = QVariant() ) {
	this->type = type;
	this->variant = variant;
	this->value = value;
    };
};
bool operator==( const Constraint &l, const Constraint &r );

class FilterWidget;
class DepartureInfo;
/** A filter, which is a list of constraints.
* @Note You can create a widget to show/edit this filter with
* @ref FilterWidget::create. */
class Filter : public QList< Constraint > {
    public:
	/** Returns true, if all constraints of this filter match. */
	bool match( const DepartureInfo &departureInfo ) const;
	
	QByteArray toData() const;
	void fromData( const QByteArray &ba );
	
    private:
	bool matchString( FilterVariant variant, const QString &filterString,
			  const QString &testString ) const;
	bool matchInt( FilterVariant variant, int filterInt, int testInt ) const;
	bool matchList( FilterVariant variant, const QVariantList &filterValues,
			const QVariant &testValue ) const;
	bool matchTime( FilterVariant variant, const QTime &filterTime,
			const QTime &testTime ) const;
};
QDataStream& operator<<( QDataStream &out, const Filter &filter );
QDataStream& operator>>( QDataStream &in, Filter &filter );

/** A list of filters, serializable to and from QByteArray by @ref toData / @ref fromData.
* @Note You can create a widget to show/edit this filter list with
* @ref FilterListWidget::create. */
class FilterList : public QList< Filter > {
    public:
	/** Returns true, if one of the filters in this FilterList matches.
	* This uses @ref Filter::match. */
	bool match( const DepartureInfo &departureInfo ) const;
	
	QByteArray toData() const;
	void fromData( const QByteArray &ba );
};
QDataStream& operator<<( QDataStream &out, const FilterList &filterList );
QDataStream& operator>>( QDataStream &in, FilterList &filterList );

struct FilterSettings {
    FilterAction filterAction;
    FilterList filters;

    FilterSettings() {
	filterAction = ShowMatching;
    };
    
    bool filterOut( const DepartureInfo& departureInfo ) const;
};
typedef QList< FilterSettings > FilterSettingsList;
bool operator ==( const FilterSettings &l, const FilterSettings &r );

#endif // Multiple inclusion guard
