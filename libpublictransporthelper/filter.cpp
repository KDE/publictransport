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

#include "filter.h"
#include "departureinfo.h"

#include <KDebug>

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

bool FilterSettings::filterOut( const DepartureInfo& departureInfo ) const
{
    switch ( filterAction ) {
    case ShowMatching:
        return !filters.match( departureInfo );
    case HideMatching:
        return filters.match( departureInfo );
    }

    return false;
}

bool FilterList::match( const DepartureInfo& departureInfo ) const
{
    foreach( const Filter &filter, *this ) {
        if ( filter.match( departureInfo ) ) {
            return true;
        }
    }

    return false;
}

bool Filter::match( const DepartureInfo& departureInfo ) const
{
    foreach( const Constraint &constraint, *this ) {
        switch ( constraint.type ) {
        case FilterByTarget:
            if ( !matchString(constraint.variant, constraint.value.toString(),
                              departureInfo.target()) ) {
                return false;
            }
            break;
        case FilterByVia: {
            bool viaMatched = false;
            foreach( const QString &via, departureInfo.routeStops() ) {
                if ( matchString(constraint.variant, constraint.value.toString(), via) ) {
                    viaMatched = true;
                    break;
                }
            }

            // If no route stop matches or no route items are available, try to match the target
            if ( !viaMatched && !matchString(constraint.variant, constraint.value.toString(),
                                             departureInfo.target()) )
            {
                return false;
            }
            break;
        }
        case FilterByNextStop: {
            if ( departureInfo.routeStops().count() < 2 || departureInfo.routeExactStops() == 1 ) {
                // If too less or no route stops are available use the target as next stop
                return matchString( constraint.variant, constraint.value.toString(),
                                    departureInfo.target() );
            }

            QString nextStop = !departureInfo.isArrival() ? departureInfo.routeStops()[ 1 ]
                    : departureInfo.routeStops()[ departureInfo.routeStops().count() - 2 ];
            if ( !matchString(constraint.variant, constraint.value.toString(), nextStop) ) {
                return false;
            }
            break;
        } case FilterByTransportLine:
            if ( !matchString(constraint.variant, constraint.value.toString(),
                              departureInfo.lineString()) ) {
                return false;
            }
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
                                  departureInfo.delay()) ) {
                return false;
            }
            break;

        case FilterByVehicleType:
            if ( !matchList(constraint.variant, constraint.value.toList(),
                            static_cast<int>(departureInfo.vehicleType())) ) {
                return false;
            }
            break;

        case FilterByDepartureTime:
            if ( !matchTime(constraint.variant, constraint.value.toTime(),
                            departureInfo.departure().time()) ) {
                return false;
            }
            break;
        case FilterByDepartureDate:
            if ( !matchDate(constraint.variant, constraint.value.toDate(),
                            departureInfo.departure().date()) ) {
                return false;
            }
            break;
        case FilterByDayOfWeek:
            if ( !matchList(constraint.variant, constraint.value.toList(),
                            static_cast<int>(departureInfo.departure().date().dayOfWeek())) ) {
                return false;
            }
            break;

        default:
            kDebug() << "Filter unknown or invalid" << constraint.type;
            break;
        }
    }

    return true;
}

bool Filter::isOneTimeFilter() const
{
    bool hasDateEqualsConstraint = false, hasTimeEqualsConstraint = false;
    foreach( const Constraint &constraint, *this ) {
        switch ( constraint.type ) {
        case FilterByTarget:
        case FilterByVia:
        case FilterByNextStop:
        case FilterByTransportLine:
        case FilterByTransportLineNumber:
        case FilterByDelay:
        case FilterByVehicleType:
            break;

        case FilterByDepartureTime:
            hasTimeEqualsConstraint = constraint.variant == FilterEquals;
            break;
        case FilterByDepartureDate:
            hasDateEqualsConstraint = constraint.variant == FilterEquals;
            break;

        default:
            kDebug() << "Filter unknown or invalid" << constraint.type;
            break;
        }
    }

    return hasTimeEqualsConstraint && hasDateEqualsConstraint;
}

bool Filter::isExpired() const
{
    QDate date;
    QTime time;
    foreach( const Constraint &constraint, *this ) {
        switch ( constraint.type ) {
        case FilterByTarget:
        case FilterByVia:
        case FilterByNextStop:
        case FilterByTransportLine:
        case FilterByTransportLineNumber:
        case FilterByDelay:
        case FilterByVehicleType:
            break;

        case FilterByDepartureTime:
            if ( constraint.variant == FilterEquals ) {
                time = constraint.value.toTime();
            }
            break;
        case FilterByDepartureDate:
            if ( constraint.variant == FilterEquals ) {
                date = constraint.value.toDate();
            }
            break;

        default:
            kDebug() << "Filter unknown or invalid" << constraint.type;
            break;
        }
    }

    if ( !time.isValid() || !date.isValid() ) {
        kDebug() << "No one-time filter settings found, use Filter::isOneTimeFilter() to check";
        return false;
    }

    return QDateTime::currentDateTime() > QDateTime( date, time );
}

bool Filter::matchList( FilterVariant variant, const QVariantList& filterValues,
                        const QVariant& testValue ) const
{
    switch ( variant ) {
//     case FilterEquals:
//     case FilterDoesntEqual:
    case FilterIsOneOf:
        return filterValues.contains( testValue );
    case FilterIsntOneOf:
        return !filterValues.contains( testValue );

    default:
        kDebug() << "Invalid filter variant for list matching:" << variant;
        return false;
    }
}

bool Filter::matchInt( FilterVariant variant, int filterInt, int testInt ) const
{
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
                          const QString& testString ) const
{
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
                        const QTime& testTime ) const
{
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

bool Filter::matchDate( FilterVariant variant, const QDate& filterDate, const QDate& testDate ) const
{
    switch ( variant ) {
    case FilterEquals:
        return testDate == filterDate;
    case FilterDoesntEqual:
        return testDate != filterDate;

    case FilterGreaterThan:
        return testDate > filterDate;
    case FilterLessThan:
        return testDate < filterDate;

    default:
        kDebug() << "Invalid filter variant for date matching:" << variant;
        return false;
    }
}

QByteArray Filter::toData() const
{
    QByteArray ba;
    QDataStream stream( &ba, QIODevice::WriteOnly );
    stream << *this;
    return ba;
}

void Filter::fromData( const QByteArray& ba )
{
    QDataStream stream( ba );
    stream >> *this;
}

QByteArray FilterList::toData() const
{
    QByteArray ba;
    QDataStream stream( &ba, QIODevice::WriteOnly );
    stream << *this;
    return ba;
}

void FilterList::fromData( const QByteArray& ba )
{
    QDataStream stream( ba );
    stream >> *this;
}

QDataStream& operator<<( QDataStream& out, const FilterList &filterList )
{
    out << filterList.count();
    foreach( const Filter &filter, filterList ) {
        out << filter;
    }
    return out;
}

QDataStream& operator>>( QDataStream& in, FilterList &filterList )
{
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

QDataStream& operator<<( QDataStream& out, const Filter &filter )
{
    out << filter.count();
    foreach( const Constraint &constraint, filter ) {
        out << constraint.type;
        out << constraint.variant;

        QVariantList list;
        switch ( constraint.type ) {
        case FilterByVehicleType:
        case FilterByDayOfWeek:
            list = constraint.value.toList();
            out << list.count();
            foreach( const QVariant &value, list ) {
                out << value.toInt();
            }
            break;

        case FilterByTarget:
        case FilterByVia:
        case FilterByNextStop:
        case FilterByTransportLine:
            out << constraint.value.toString();
            break;

        case FilterByTransportLineNumber:
        case FilterByDelay:
            out << constraint.value.toInt();
            break;

        case FilterByDepartureTime:
            out << constraint.value.toTime();
            break;
        case FilterByDepartureDate:
            out << constraint.value.toDate();
            break;

        default:
            kDebug() << "Unknown filter type" << constraint.type;
        }
    }
    return out;
}

QDataStream& operator>>( QDataStream& in, Filter &filter )
{
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

        switch ( constraint.type ) {
        case FilterByVehicleType:
        case FilterByDayOfWeek: {
            int i, count;
            QVariantList list;
            in >> count;
            for ( int m = 0; m < count; ++m ) {
                in >> i;
                list << i;
            }
            constraint.value = list;
            break;
        }

        case FilterByTarget:
        case FilterByVia:
        case FilterByNextStop:
        case FilterByTransportLine: {
            QString s;
            in >> s;
            constraint.value = s;
            break;
        }

        case FilterByTransportLineNumber:
        case FilterByDelay: {
            int i;
            in >> i;
            constraint.value = i;
            break;
        }

        case FilterByDepartureTime: {
            QTime time;
            in >> time;
            constraint.value = time;
            break;
        } case FilterByDepartureDate: {
            QDate date;
            in >> date;
            constraint.value = date;
            break;
        }

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

bool operator==( const Constraint& l, const Constraint& r )
{
    return l.type == r.type && l.variant == r.variant && l.value == r.value;
}

bool operator==( const FilterSettings& l, const FilterSettings& r )
{
    return l.filterAction == r.filterAction && l.filters == r.filters
            && l.name == r.name && l.affectedStops == r.affectedStops;
}

bool operator==(const FilterSettingsList& l, const FilterSettingsList& r)
{
    if ( l.count() != r.count() ) {
        return false;
    }

    for ( int i = 0; i < l.count(); ++i ) {
        if ( !(l[i] == r[i]) ) {
            return false;
        }
    }

    return true;
}

bool FilterSettingsList::filterOut(const DepartureInfo& departureInfo) const {
    foreach ( const FilterSettings &filter, *this ) {
        if ( filter.filterOut(departureInfo) ) {
            return true;
        }
    }
    return false; // No filter settings filtered the departureInfo out
}

QStringList FilterSettingsList::names() const {
    QStringList ret;
    foreach ( const FilterSettings &filter, *this ) {
        ret << filter.name;
    }
    return ret;
}

bool FilterSettingsList::hasName(const QString& name) const {
    foreach ( const FilterSettings &filter, *this ) {
        if ( filter.name == name ) {
            return true;
        }
    }
    return false; // No filter with the given name found
}

FilterSettings FilterSettingsList::byName(const QString& name) const {
    foreach ( const FilterSettings &filter, *this ) {
        if ( filter.name == name ) {
            return filter;
        }
    }
    return FilterSettings(); // No filter with the given name found
}

void FilterSettingsList::removeByName(const QString& name) {
    for ( int i = 0; i < count(); ++i ) {
        if ( operator[](i).name == name ) {
            removeAt( i );
            return;
        }
    }

    kDebug() << "No filter configuration with the given name found:" << name;
    kDebug() << "Available names are:" << names();
}

void FilterSettingsList::set(const FilterSettings& newFilterSettings) {
    for ( int i = 0; i < count(); ++i ) {
        if ( operator[](i).name == newFilterSettings.name ) {
            operator[]( i ) = newFilterSettings;
            return;
        }
    }

    // No filter with the given name found, add newFilterSettings to this list
    *this << newFilterSettings;
}

} // namespace Timetable
