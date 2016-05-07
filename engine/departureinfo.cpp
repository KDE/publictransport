/*
 *   Copyright 2012 Friedrich Pülz <fpuelz@gmx.de>
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

// Header
#include "departureinfo.h"

// Own includes
#include "global.h"

// Qt includes
#include <QDebug>

PublicTransportInfo::PublicTransportInfo(QObject* parent): QObject(parent)
{
}

PublicTransportInfo::~PublicTransportInfo()
{
}

PublicTransportInfo::PublicTransportInfo( const QHash< Enums::TimetableInformation, QVariant >& data,
                                          Corrections corrections, QObject *parent )
    : QObject(parent), m_data(data)
{
    m_isValid = false;

    // Insert -1 as Delay if none is given (-1 means "no delay information available")
    if ( !contains(Enums::Delay) ) {
        insert( Enums::Delay, -1 );
    }

    if ( corrections.testFlag(DeduceMissingValues) ) {
        // Guess date value if none is given,
        // this could produce wrong dates (better give DepartureDate in scripts)
        if ( !contains(Enums::DepartureDate) && contains(Enums::DepartureTime) ) {
            QTime departureTime = value( Enums::DepartureTime ).toTime();
            if ( departureTime < QTime::currentTime().addSecs(-5 * 60) ) {
                qDebug() << "Guessed DepartureDate as tomorrow";
                insert( Enums::DepartureDate, QDate::currentDate().addDays(1) );
            } else {
                qDebug() << "Guessed DepartureDate as today";
                insert( Enums::DepartureDate, QDate::currentDate() );
            }
        }
    }

    if ( corrections.testFlag(CombineToPreferredValueType) ) {
        // Convert date value from string if not given as date
        if ( !contains(Enums::DepartureDateTime) ) {
            if ( contains(Enums::DepartureTime) ) {
                const QTime time = value( Enums::DepartureTime ).toTime();
                QDate date;
                if ( contains(Enums::DepartureDate) ) {
                    date = value( Enums::DepartureDate ).toDate();
                } else if ( time < QTime::currentTime().addSecs(-5 * 60) ) {
                    qDebug() << "Guessed DepartureDate as tomorrow";
                    date = QDate::currentDate().addDays( 1 );
                } else {
                    qDebug() << "Guessed DepartureDate as today";
                    date = QDate::currentDate();
                }
                insert( Enums::DepartureDateTime, QDateTime(date, time) );
                remove( Enums::DepartureDate );
                remove( Enums::DepartureTime );
            } else {
                qDebug() << "No DepartureDateTime or DepartureTime information given";
            }
        }
    }

    if ( corrections.testFlag(TestValuesForCorrectFormat) ) {
        // Check RouteTimes values
        if ( contains(Enums::RouteTimes) ) {
            if ( value(Enums::RouteTimes).canConvert(QVariant::List) ) {
                QVariantList vars = value( Enums::RouteTimes ).toList();
                foreach ( const QVariant &var, vars ) {
                    // Convert from QVariant to QTime
                    if ( !var.canConvert(QVariant::DateTime) || !var.toDateTime().isValid() ) {
                        // Value for DepartureTime is not QDateTime
                        qWarning() << "Invalid time in RouteTimes:" << var;
                        remove( Enums::RouteTimes );
                        break;
                    }
                }
            } else {
                // No list of values given for RouteTimes, remove the invalid value
                qWarning() << "RouteTimes value is invalid (not a list of values): "
                           << value(Enums::RouteTimes);
                remove( Enums::RouteTimes );
            }
        }
    }
}

JourneyInfo::JourneyInfo( const TimetableData &data, Corrections corrections, QObject *parent )
        : PublicTransportInfo( data, corrections, parent )
{
    if ( corrections.testFlag(DeduceMissingValues) ) {
        // Guess arrival date value if none is given,
        // this could produce wrong dates (better give ArrivalDate eg. in scripts)
        if ( !contains(Enums::ArrivalDateTime) &&
             !contains(Enums::ArrivalDate) && contains(Enums::ArrivalTime) )
        {
            QTime arrivalTime = value(Enums::ArrivalTime).toTime();
            if ( arrivalTime < QTime::currentTime().addSecs(-5 * 60) ) {
                insert( Enums::ArrivalDate, QDate::currentDate().addDays(1) );
            } else {
                insert( Enums::ArrivalDate, QDate::currentDate() );
            }
        }

        // Deduce Duration from DepartureDateTime and ArrivalDateTime
        if ( value(Enums::Duration).toInt() <= 0 && contains(Enums::DepartureDateTime) &&
             contains(Enums::ArrivalDateTime) )
        {
            QDateTime departure = value( Enums::DepartureDateTime ).toDateTime();
            QDateTime arrival = value( Enums::ArrivalDateTime ).toDateTime();
            int minsDuration = departure.secsTo( arrival ) / 60;
            if ( minsDuration < 0 ) {
                qWarning() << "Calculated duration is negative" << minsDuration
                           << "departure" << departure << "arrival" << arrival;
                insert( Enums::Duration, -1 );
            } else {
                insert( Enums::Duration, minsDuration );
            }
        }
    }

//     TODO Move to PublicTransportInfo? ...or is it already in there?
    if ( corrections.testFlag(TestValuesForCorrectFormat) ) {
        // Test departure time format
        if ( contains(Enums::DepartureTime) &&
             !value(Enums::DepartureTime).canConvert(QVariant::Time) )
        {
            // Value for DepartureTime is not QTime
            qWarning() << "DepartureTime needs to be convertable to QTime:"
                       << value(Enums::DepartureTime);
            remove( Enums::DepartureTime );
        }

        // Test departure date format
        if ( contains(Enums::DepartureDate) &&
             (!value(Enums::DepartureDate).canConvert(QVariant::Date) ||
              !value(Enums::DepartureDate).toDate().isValid()) )
        {
            qWarning() << "DepartureDate needs to be convertable to QDate:"
                       << value( Enums::DepartureDate );
            remove( Enums::DepartureDate );
        }

        // Check ArrivalDate value
        if ( contains(Enums::ArrivalDate) &&
             (!value(Enums::ArrivalDate).canConvert(QVariant::Date) ||
              !value(Enums::ArrivalDate).toDate().isValid()) )
        {
            qWarning() << "ArrivalDate needs to be convertable to QDate:"
                       << value(Enums::ArrivalDate);
            remove( Enums::ArrivalDate );
        }

        // Check Duration value
        if ( contains(Enums::Duration) && value(Enums::Duration).toInt() <= 0 &&
             value(Enums::Duration).canConvert(QVariant::String) )
        {
            qWarning() << "Duration needs to be convertable to int:" << value(Enums::Duration);
            remove( Enums::Duration );
        }

        // Check RouteTimesDeparture values
        if ( contains(Enums::RouteTimesDeparture) ) {
            if ( !value(Enums::RouteTimesDeparture).canConvert(QVariant::List) ) {
                qWarning() << "Value for RouteTimesDeparture needs to be a list"
                           << value(Enums::RouteTimesDeparture);
                remove( Enums::RouteTimesDeparture );
            } else {
                QVariantList vars = value( Enums::RouteTimesDeparture ).toList();
                foreach( const QVariant &var, vars ) {
                    if ( !var.canConvert(QVariant::DateTime) ) {
                        qWarning() << "Invalid time in RouteTimesDeparture" << var;
                        remove( Enums::RouteTimesDeparture );
                        break;
                    }
                }
            }
        }

        // Check RouteTimesArrival values
        if ( contains(Enums::RouteTimesArrival) ) {
            if ( !value(Enums::RouteTimesArrival).canConvert(QVariant::List) ) {
                qWarning() << "Value for RouteTimesArrival needs to be a list"
                           << value(Enums::RouteTimesArrival);
                remove( Enums::RouteTimesArrival );
            } else {
                QVariantList vars = value( Enums::RouteTimesArrival ).toList();
                foreach( const QVariant &var, vars ) {
                    if ( !var.canConvert(QVariant::DateTime) ) {
                        qWarning() << "Invalid time in RouteTimesArrival" << var;
                        remove( Enums::RouteTimesArrival );
                        break;
                    }
                }
            }
        }
    }

    if ( corrections.testFlag(CombineToPreferredValueType) ) {
        // If the duration value is invalid, but there are departure and arrival date and time
        // values available, calculate the duration between departure and arrival
        if ( value(Enums::Duration).toInt() <= 0 && contains(Enums::DepartureDate) &&
             contains(Enums::DepartureTime) && contains(Enums::ArrivalDate) &&
             contains(Enums::ArrivalTime) )
        {
            QDateTime departure( value(Enums::DepartureDate).toDate(), value(Enums::ArrivalTime).toTime() );
            QDateTime arrival( value(Enums::ArrivalDate).toDate(), value(Enums::ArrivalTime).toTime() );
            int minsDuration = departure.secsTo( arrival ) / 60;
            if ( minsDuration < 0 ) {
                qDebug() << "Calculated duration is negative" << minsDuration
                        << "departure" << departure << "arrival" << arrival;
                insert( Enums::Duration, -1 );
            } else {
                insert( Enums::Duration, minsDuration );
            }
        }

        // Combine ArrivalTime and ArrivalDate to ArrivalDateTime
        if ( !contains(Enums::ArrivalDateTime) ) {
            if ( contains(Enums::ArrivalTime) ) {
                const QTime time = value( Enums::ArrivalTime ).toTime();
                QDate date;
                if ( contains(Enums::ArrivalDate) ) {
                    date = value( Enums::ArrivalDate ).toDate();
                } else if ( time < QTime::currentTime().addSecs(-5 * 60) ) {
                    qDebug() << "Guessed ArrivalDate as tomorrow";
                    date = QDate::currentDate().addDays( 1 );
                } else {
                    qDebug() << "Guessed ArrivalDate as today";
                    date = QDate::currentDate();
                }
                insert( Enums::ArrivalDateTime, QDateTime(date, time) );
                remove( Enums::ArrivalDate );
                remove( Enums::ArrivalTime );
            } else {
                qWarning() << "No ArrivalDateTime or ArrivalTime information given";
            }
        }
    }

    m_isValid = contains(Enums::DepartureDateTime) && contains(Enums::ArrivalDateTime) &&
                contains(Enums::StartStopName) && contains(Enums::TargetStopName);
}

StopInfo::StopInfo( QObject *parent ) : PublicTransportInfo(parent)
{
    m_isValid = false;
}

StopInfo::StopInfo( const QHash< Enums::TimetableInformation, QVariant >& data, QObject *parent )
    : PublicTransportInfo(parent)
{
    m_data.unite( data );
    m_isValid = contains( Enums::StopName );
}

StopInfo::StopInfo( const QString &name, const QString& id, int weight,
                    qreal longitude, qreal latitude, const QString &city,
                    const QString &countryCode, QObject *parent ) : PublicTransportInfo(parent)
{
    insert( Enums::StopName, name );
    if ( !id.isNull() ) {
        insert( Enums::StopID, id );
    }
    if ( longitude != 0.0 ) {
        insert( Enums::StopLongitude, longitude );
    }
    if ( latitude != 0.0 ) {
        insert( Enums::StopLatitude, latitude );
    }
    if ( !city.isNull() ) {
        insert( Enums::StopCity, city );
    }
    if ( !countryCode.isNull() ) {
        insert( Enums::StopCountryCode, countryCode );
    }
    if ( weight != -1 ) {
        insert( Enums::StopWeight, weight );
    }

    m_isValid = !name.isEmpty();
}

DepartureInfo::DepartureInfo( QObject *parent ) : PublicTransportInfo(parent)
{
    m_isValid = false;
}

DepartureInfo::DepartureInfo( const TimetableData &data, Corrections corrections, QObject *parent )
        : PublicTransportInfo( data, corrections, parent )
{
    if ( (contains(Enums::RouteStops) || contains(Enums::RouteTimes)) &&
         value(Enums::RouteTimes).toList().count() != value(Enums::RouteStops).toStringList().count() )
    {
        int difference = value(Enums::RouteStops).toList().count() -
                         value(Enums::RouteTimes).toList().count();
        if ( difference > 0 ) {
            // More route stops than times, add invalid times
            qDebug() << "The script stored" << difference << "more route stops than route times "
                        "for a departure, invalid route times will be added";
            QVariantList routeTimes = value( Enums::RouteTimes ).toList();
            while ( difference > 0 ) {
                routeTimes << QDateTime();
                --difference;
            }
        } else {
            // More route times than stops, add empty stops
            qDebug() << "The script stored" << -difference << "more route times than route "
                        "stops for a departure, empty route stops will be added";
            QStringList routeStops = value( Enums::RouteStops ).toStringList();
            while ( difference < 0 ) {
                routeStops << QString();
                ++difference;
            }
        }
    }

    m_isValid = contains( Enums::TransportLine ) && contains( Enums::Target ) &&
                contains( Enums::DepartureDateTime );
}

QStringList JourneyInfo::vehicleIconNames() const
{
    if ( !contains(Enums::TypesOfVehicleInJourney) ) {
        return QStringList();
    }
    QVariantList vehicles = value( Enums::TypesOfVehicleInJourney ).toList();
    QStringList iconNames;
    foreach( QVariant vehicle, vehicles ) {
        iconNames << Global::vehicleTypeToIcon( static_cast<Enums::VehicleType>( vehicle.toInt() ) );
    }
    return iconNames;
}

QStringList JourneyInfo::vehicleNames( bool plural ) const
{
    if ( !contains( Enums::TypesOfVehicleInJourney ) ) {
        return QStringList();
    }
    QVariantList vehicles = value( Enums::TypesOfVehicleInJourney ).toList();
    QStringList names;
    foreach( QVariant vehicle, vehicles ) {
        names << Global::vehicleTypeToString( static_cast<Enums::VehicleType>( vehicle.toInt() ), plural );
    }
    return names;
}
