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
* @brief This file contains classes used to filter departures/arrivals/journeys.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef FILTER_HEADER
#define FILTER_HEADER

#include "global.h"

#include <QVariant>

/** @brief A single constraint.
* @note You can create a widget to show/edit this constraint with
* @ref ConstraintWidget::create.
* @ingroup filterSystem */
struct Constraint {
    FilterType type; /**< The type of this constraint, ie. what to filter. */
    FilterVariant variant; /**< The variant of this constraint, eg. equals/doesn't equal. */
    QVariant value; /**< The value of this constraint. */

    /** Creates a new constraint with default values. */
    Constraint() {
        type = FilterByVehicleType;
        variant = FilterIsOneOf;
        value = QVariantList() << static_cast< int >( Unknown );
    };

    /**
    * @brief Creates a new constraint with the given values.
    *
    * @param type The type of the new constraint, ie. what to filter.
    * @param variant The variant of the new constraint, eg. equals/doesn't equal.
    * @param value The value of the new constraint. Defaults to QVariant().
    **/
    Constraint( FilterType type, FilterVariant variant, const QVariant &value = QVariant() ) {
        this->type = type;
        this->variant = variant;
        this->value = value;
    };
};
bool operator==( const Constraint &l, const Constraint &r );

class FilterWidget;
class DepartureInfo;
/** @brief A filter, which is a list of constraints.
* @note You can create a widget to show/edit this filter with @ref FilterWidget::create.
* @ingroup filterSystem */
class Filter : public QList< Constraint > {
public:
    /** Returns true, if all constraints of this filter match. */
    bool match( const DepartureInfo &departureInfo ) const;

    /**
    * @brief Serializes this filter to a @ref QByteArray
    * @returns the serialized filter as a @ref QByteArray
    **/
    QByteArray toData() const;
    /**
    * @brief Reads the data for this filter from the given @ref QByteArray
    * @param ba The @ref QByteArray to read the data for this filter from.
    **/
    void fromData( const QByteArray &ba );

private:
    bool matchString( FilterVariant variant, const QString &filterString,
            const QString &testString ) const;
    bool matchInt( FilterVariant variant, int filterInt, int testInt ) const;
    bool matchList( FilterVariant variant, const QVariantList &filterValues,
                    const QVariant &testValue ) const;
    bool matchTime( FilterVariant variant, const QTime &filterTime, const QTime &testTime ) const;
};
QDataStream& operator<<( QDataStream &out, const Filter &filter );
QDataStream& operator>>( QDataStream &in, Filter &filter );

/** @brief A list of filters, serializable to and from QByteArray by @ref toData / @ref fromData.
* @Note You can create a widget to show/edit this filter list with
* @ref FilterListWidget::create.
* @ingroup filterSystem */
class FilterList : public QList< Filter > {
public:
    /** Returns true, if one of the filters in this FilterList matches.
    * This uses @ref Filter::match. */
    bool match( const DepartureInfo &departureInfo ) const;

    /**
    * @brief Serializes this list of filters to a @ref QByteArray
    * @returns the serialized filter as a @ref QByteArray
    **/
    QByteArray toData() const;
    /**
    * @brief Reads the data for this list of filters from the given @ref QByteArray
    * @param ba The @ref QByteArray to read the data for this filter list from.
    **/
    void fromData( const QByteArray &ba );
};
QDataStream& operator<<( QDataStream &out, const FilterList &filterList );
QDataStream& operator>>( QDataStream &in, FilterList &filterList );

/** @brief Contains information about a filter configuration, ie. the settings of a filter.
* @ingroup filterSystem */
struct FilterSettings {
    /** The action to take on matching items. */
    FilterAction filterAction;
    /** A list of filters for this filter configuration. Filters are OR combined
    * while there constraints are AND combined. */
    FilterList filters;

    FilterSettings() {
        filterAction = ShowMatching;
    };

    /** Applies this filter configuration on the given @p departureInfo. */
    bool filterOut( const DepartureInfo& departureInfo ) const;
};
typedef QList< FilterSettings > FilterSettingsList;
bool operator ==( const FilterSettings &l, const FilterSettings &r );

#endif // Multiple inclusion guard
