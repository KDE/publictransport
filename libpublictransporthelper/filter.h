/*
 *   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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
#include <KDebug>

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

/**
 * @brief A single constraint.
 * 
 * @note You can create a widget to show/edit this constraint with
 * @ref ConstraintWidget::create.
 * 
 * @ingroup filterSystem
 **/
struct PUBLICTRANSPORTHELPER_EXPORT Constraint {
    FilterType type; /**< The type of this constraint, ie. what to filter. */
    FilterVariant variant; /**< The variant of this constraint, eg. equals/doesn't equal. */
    QVariant value; /**< The value of this constraint. */

    /** @brief Creates a new constraint with default values. */
    Constraint() {
        type = FilterByVehicleType;
        variant = FilterIsOneOf;
        value = QVariantList() << static_cast< int >( Unknown );
    };

    /**
    * @brief Creates a new constraint with the given values.
    *
    * @param type The type of the new constraint, ie. what to filter.
    * 
    * @param variant The variant of the new constraint, eg. equals/doesn't equal.
    * 
    * @param value The value of the new constraint. Defaults to QVariant().
    **/
    Constraint( FilterType type, FilterVariant variant, const QVariant &value = QVariant() ) {
        this->type = type;
        this->variant = variant;
        this->value = value;
    };
};
bool PUBLICTRANSPORTHELPER_EXPORT operator==( const Constraint &l, const Constraint &r );

class FilterWidget;
class DepartureInfo;

/**
 * @brief A filter, which is a list of constraints.
 *
 * The constraints are logically combined using AND.
 * 
 * @note You can create a widget to show/edit this filter with @ref FilterWidget::create.
 * 
 * @ingroup filterSystem
 **/
class PUBLICTRANSPORTHELPER_EXPORT Filter : public QList< Constraint > {
public:
    Filter() : QList<Constraint>() {};
    Filter( const QList<Constraint> other ) : QList<Constraint>(other) {};

    /** @brief Returns true, if all constraints of this filter match. */
    bool match( const DepartureInfo &departureInfo ) const;

    /**
     * @brief Serializes this filter to a @ref QByteArray
     * 
     * @returns the serialized filter as a @ref QByteArray
     **/
    QByteArray toData() const;

    /**
     * @brief Reads the data for this filter from the given @ref QByteArray
     * 
     * @param ba The @ref QByteArray to read the data for this filter from.
     **/
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

/**
 * @brief A list of filters, serializable to and from QByteArray by @ref toData / @ref fromData.
 *
 * The filters are logically combined using OR, while the filters are logical
 * combinations of constraints using AND.
 * 
 * @Note You can create a widget to show/edit this filter list with
 * @ref FilterListWidget::create.
 * 
 * @ingroup filterSystem
 **/
class PUBLICTRANSPORTHELPER_EXPORT FilterList : public QList< Filter > {
public:
    FilterList() : QList<Filter>() {};
    FilterList( const QList<Filter> other ) : QList<Filter>(other) {};

    /**
     * @brief Returns true, if one of the filters in this FilterList matches.
     * 
     * This uses @ref Filter::match. */
    bool match( const DepartureInfo &departureInfo ) const;

    /**
     * @brief Serializes this list of filters to a @ref QByteArray
     * 
     * @returns the serialized filter as a @ref QByteArray
     **/
    QByteArray toData() const;
    
    /**
     * @brief Reads the data for this list of filters from the given @ref QByteArray
     * 
     * @param ba The @ref QByteArray to read the data for this filter list from.
     **/
    void fromData( const QByteArray &ba );
};
QDataStream& operator<<( QDataStream &out, const FilterList &filterList );
QDataStream& operator>>( QDataStream &in, FilterList &filterList );

/**
 * @brief Contains information about a filter configuration, ie. the settings of a filter.
 * 
 * @ingroup filterSystem
 **/
struct PUBLICTRANSPORTHELPER_EXPORT FilterSettings {
    /** @brief The action to take on matching items. */
    FilterAction filterAction;

    /**
     * @brief A list of filters for this filter configuration.
     *
     * Filters are OR combined while constraints are AND combined.
     **/
    FilterList filters;

    /** @brief A list of stop settings indices for which this filter should be applied. */
    QSet< int > affectedStops;

    /** @brief The Name of this filter settings. */
    QString name;

    /** @brief Create a new FilterSettings object with the given @p name. */
    explicit FilterSettings( const QString &name = "<unnamed>" ) {
        this->filterAction = ShowMatching;
        this->name = name;
    };

    /** @brief Applies this filter configuration on the given @p departureInfo. */
    bool filterOut( const DepartureInfo& departureInfo ) const;
};
bool PUBLICTRANSPORTHELPER_EXPORT operator ==( const FilterSettings &l, const FilterSettings &r );
inline bool PUBLICTRANSPORTHELPER_EXPORT operator !=( const FilterSettings &l, const FilterSettings &r ) {
    return !(l == r);
}

/**
 * @brief A QList of FilterSettings with some convenience methods.
 *
 * @ingroup filterSystem
 **/
class PUBLICTRANSPORTHELPER_EXPORT FilterSettingsList : public QList< FilterSettings > {
public:
    /** @brief Applies all filter configurations in this list on the given @p departureInfo. */
    bool filterOut( const DepartureInfo& departureInfo ) const;

    /** @brief Get a list of the names of all filter settings in this list. */
    QStringList names() const;

    /** @brief Checks if there is a filter settings object with the given @p name in this list. */
    bool hasName( const QString &name ) const;

    /**
     * @brief Gets the filter settings object with the given @p name from this list.
     *
     * If there is no such filter settings object, a default constructed FilterSettings object
     * gets returned.
     **/
    FilterSettings byName( const QString &name ) const;

    /** @brief Removes the filter settings object with the given @p name from this list. */
    void removeByName( const QString &name );

    /**
     * @brief Adds the given @p newFilterSettings to this list or changes an existing one with
     *   the same name, if there is already one in this list. */
    void set( const FilterSettings &newFilterSettings );
};
bool PUBLICTRANSPORTHELPER_EXPORT operator ==( const FilterSettingsList &l, const FilterSettingsList &r );

inline QDebug& operator<<(QDebug debug, const Constraint& constraint)
{
    return debug << "Constraint, type " << constraint.type << ", variant " << constraint.variant;
//                  << ", value " << constraint.value;
}

inline QDebug& operator<<(QDebug debug, const Filter& filter)
{
    debug << "Filter, " << filter.count() << " constraints:";
    for ( int i = 0; i < filter.count() - 1; ++i ) {
        debug << ' ' << filter[i];
    }
    return debug << ' ' << filter.last();
}

inline QDebug& operator<<(QDebug debug, const FilterSettings& filterSettings)
{
    debug << "FilterSettings " << filterSettings.name
          << " affectedStops: " << filterSettings.affectedStops
          << " filterAction: " << filterSettings.filterAction
          << ' ' << filterSettings.filters.count() << " filters:\n";
    for ( int i = 0; i < filterSettings.filters.count() - 1; ++i ) {
        debug << ' ' << filterSettings.filters[i] << ", ";
    }
    return debug << ' ' << filterSettings.filters.count() << filterSettings.filters.last();
}

inline QDebug& operator<<(QDebug debug, const FilterSettingsList& filterSettingsList)
{
    debug << "FilterSettingsList, " << filterSettingsList.count() << "filter settings:\n";
    for ( int i = 0; i < filterSettingsList.count() - 1; ++i ) {
        debug << ' ' << filterSettingsList[i];
    }
    return debug << ' ' << filterSettingsList.last();
}

} // namespace Timetable

Q_DECLARE_METATYPE( Timetable::Constraint )
Q_DECLARE_METATYPE( Timetable::Filter )
Q_DECLARE_METATYPE( Timetable::FilterSettings )
Q_DECLARE_METATYPE( Timetable::FilterSettingsList )

#endif // Multiple inclusion guard
