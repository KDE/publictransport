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

// Header
#include "departureinfo.h"

// Own includes
#include "global.h"

// KDE includes
#include <KDebug>

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
        // Try to deduct operator from vehicle type
        if ( contains(Enums::TypeOfVehicle) && value(Enums::TypeOfVehicle).canConvert(QVariant::String) ) {
            // Try to get operator from vehicle type string if no operator is given
            const QString vehicleType = value(Enums::TypeOfVehicle).toString();
            if ( !contains(Enums::Operator) || (value(Enums::Operator).canConvert(QVariant::String)
                                      && value(Enums::Operator).toString().isEmpty()) )
            {
                // Operator not available or empty, try to get it from the vehicle type string
                QString sOperator = operatorFromVehicleTypeString( vehicleType );
                if ( !sOperator.isNull() ) {
                    insert( Enums::Operator, sOperator );
                }
            }
        }

        // Fix transport line string and try to get vehicle type and/or operator from it
        if ( contains(Enums::TransportLine) ) {
            const QString sTransportLine = value(Enums::TransportLine).toString();

            // Try to get vehicle type from transport line string if no vehicle type is given
            if ( !contains(Enums::TypeOfVehicle) || value(Enums::TypeOfVehicle) == Enums::Unknown ) {
                if ( sTransportLine.contains("F&#228;hre") ) {
                    insert( Enums::TypeOfVehicle, Enums::Ferry );
                } else {
                    QRegExp rx = QRegExp( "^(bus|tro|tram|str|s|u|m)\\s*", Qt::CaseInsensitive );
                    rx.indexIn( sTransportLine );
                    insert( Enums::TypeOfVehicle, rx.cap() );
                }
            }
        }

        // Guess date value if none is given,
        // this could produce wrong dates (better give DepartureDate in scripts)
        if ( !contains(Enums::DepartureDate) && contains(Enums::DepartureTime) ) {
            QTime departureTime = value( Enums::DepartureTime ).toTime();
            if ( departureTime < QTime::currentTime().addSecs(-5 * 60) ) {
                kDebug() << "Guessed DepartureDate as tomorrow";
                insert( Enums::DepartureDate, QDate::currentDate().addDays(1) );
            } else {
                kDebug() << "Guessed DepartureDate as today";
                insert( Enums::DepartureDate, QDate::currentDate() );
            }
    //         insert( Enums::DepartureYear, value(DepartureDate).toDate().year() ); TEST not needed? maybe use DepartureYear as comine-correction to DepartureDate
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
                    kDebug() << "Guessed DepartureDate as tomorrow";
                    date = QDate::currentDate().addDays( 1 );
                } else {
                    kDebug() << "Guessed DepartureDate as today";
                    date = QDate::currentDate();
                }
                insert( Enums::DepartureDateTime, QDateTime(date, time) );
                remove( Enums::DepartureDate );
                remove( Enums::DepartureTime );
            } else {
                kDebug() << "No DepartureDateTime or DepartureTime information given";
            }
        }
    }

//     if ( contains(DepartureDate) && (!value(DepartureDate).canConvert(QVariant::Date)
//                 || !value(DepartureDate).toDate().isValid()) )
//     {
//         if ( value(DepartureDate).canConvert(QVariant::String) ) {
//             QString sDate = value(DepartureDate).toString();
//             insert( DepartureDate, QDate::fromString(sDate, "dd.MM.yyyy") );
//
//             if ( !value(DepartureDate).toDate().isValid() ) {
//                 int currentYear = QDate::currentDate().year();
//                 QDate date = QDate::fromString( sDate, "dd.MM.yy" );
//                 if ( date.year() <= currentYear - 99 ) {
//                     date.setDate( currentYear, date.month(), date.day() );
//                 }
//
//                 insert( DepartureDate, date );
//             }
//         } else {
//             kDebug() << "Departure date is in wrong format:" << value( DepartureDate );
//             remove( DepartureDate );
//         }
//     }

//     // Guess date value if none is given,
//     // this could produce wrong dates (better give DepartureDate in scripts)
//     if ( contains(DepartureTime) && !contains(DepartureDate) ) {
//         QTime departureTime = QTime( value(DepartureHour).toInt(),
//                                      value(DepartureMinute).toInt() );
//         if ( departureTime < QTime::currentTime().addSecs(-5 * 60) ) {
//             kDebug() << "Guessed DepartureDate as tomorrow";
//             insert( DepartureDate, QDate::currentDate().addDays(1) );
//         } else {
//             kDebug() << "Guessed DepartureDate as today";
//             insert( DepartureDate, QDate::currentDate() );
//         }
//         insert( DepartureYear, value(DepartureDate).toDate().year() );
//     }

//     if ( corrections.testFlag(CombineToPreferredValueType) ) {
//         // Combine DepartureHour and DepartureMinute to DepartureTime
//         if ( !contains(DepartureTime) ) {
//             insert( DepartureTime, QTime(value(DepartureHour).toInt(),
//                                          value(DepartureMinute).toInt()) );
//         }
//     }

    if ( corrections.testFlag(ConvertValuesToCorrectFormat) ) {
        // Convert route times list to a list of QTime objects (from string and time objects)
        if ( contains(Enums::RouteTimes) ) {
            if ( value(Enums::RouteTimes).canConvert(QVariant::List) ) {
                QVariantList times;
                QVariantList vars = value( Enums::RouteTimes ).toList();
                foreach ( const QVariant &var, vars ) {
                    // Convert from QVariant to QTime
                    if ( var.canConvert(QVariant::Time) && var.toTime().isValid() ) {
                        times << var.toTime();
                    } else if ( var.canConvert(QVariant::String) ) {
                        times << QTime::fromString( var.toString().trimmed(), "hh:mm" );
                    }
                }

                insert( Enums::RouteTimes, times );
            } else {
                // No list of values given for RouteTimes, remove the invalid value
                kDebug() << "RouteTimes value is invalid (not a list of values): "
                         << value(Enums::RouteTimes);
                remove( Enums::RouteTimes );
            }
        }

        // Convert vehicle types given as strings to the associated enumerable values
        if ( contains(Enums::TypeOfVehicle) && value(Enums::TypeOfVehicle).canConvert(QVariant::String) ) {
            insert( Enums::TypeOfVehicle, getVehicleTypeFromString(value(Enums::TypeOfVehicle).toString()) );
        }
    }
}

JourneyInfo::JourneyInfo( const TimetableData &data, Corrections corrections, QObject *parent )
        : PublicTransportInfo( data, corrections, parent )
{
    if ( corrections.testFlag(DeduceMissingValues) ) {
        // Guess arrival date value if none is given,
        // this could produce wrong dates (better give ArrivalDate eg. in scripts)
        if ( !contains(Enums::ArrivalDate) && contains(Enums::ArrivalTime) ) {
            QTime arrivalTime = value(Enums::ArrivalTime).toTime();
            if ( arrivalTime < QTime::currentTime().addSecs(-5 * 60) ) {
                insert( Enums::ArrivalDate, QDate::currentDate().addDays(1) );
            } else {
                insert( Enums::ArrivalDate, QDate::currentDate() );
            }
        }
    }

//     TODO Move to PublicTransportInfo? ...or is it already in there?
    if ( corrections.testFlag(ConvertValuesToCorrectFormat) ) {
        // Convert departure time from string value
        if ( contains(Enums::DepartureTime) ) {
            QVariant timeValue = value( Enums::DepartureTime );
            if ( !timeValue.canConvert(QVariant::Time) ) {
                // Value for DepartureTime is not QTime, read string
                QTime departureTime = QTime::fromString( timeValue.toString(), "hh:mm:ss" );
                if ( !departureTime.isValid() ) {
                    departureTime = QTime::fromString( timeValue.toString(), "hh:mm" );
                }
                insert( Enums::DepartureTime, departureTime );
            }
        }

        // Convert date value from string if not given as date
        if ( contains(Enums::DepartureDate) && (!value(Enums::DepartureDate).canConvert(QVariant::Date)
                                      || !value(Enums::DepartureDate).toDate().isValid()) )
        {
            if ( value(Enums::DepartureDate).canConvert(QVariant::String) ) {
                QString sDate = value(Enums::DepartureDate).toString();
                QDate date = QDate::fromString( sDate, "dd.MM.yyyy" );
                if ( !date.isValid() ) {
                    int currentYear = QDate::currentDate().year();
                    date = QDate::fromString( sDate, "dd.MM.yy" );
                    if ( date.year() <= currentYear - 99 ) {
                        date.setDate( currentYear, date.month(), date.day() );
                    }
                }
                insert( Enums::DepartureDate, date );
            } else {
                kDebug() << "Departure date is in wrong format:" << value( Enums::DepartureDate );
                remove( Enums::DepartureDate );
            }
        }

        if ( contains(Enums::TransportLine) ) {
            // Fix transport line string, eg. do not repeat vehicle type name
            const QString sTransportLine = value(Enums::TransportLine).toString().trimmed()
                    .remove( QRegExp("^(bus|tro|tram|str)", Qt::CaseInsensitive) )
                    .replace( QRegExp("\\s{2,}"), " " );
            insert( Enums::TransportLine, sTransportLine );
        }

        // Convert TypesOfVehicleInJourney value from a string list or integer list
        // into a list of valid, distinct VehicleType values
        if ( contains(Enums::TypesOfVehicleInJourney) ) {
            QVariantList vehicleTypes;
            if ( value(Enums::TypesOfVehicleInJourney).canConvert(QVariant::StringList) ) {
                QStringList strings = value( Enums::TypesOfVehicleInJourney ).toStringList();
                foreach( const QString &str, strings ) {
                    int vehicleType = static_cast<int>( getVehicleTypeFromString(str) );
                    if ( !vehicleTypes.contains(vehicleType) ) {
                        vehicleTypes << vehicleType;
                    }
                }
            } else if ( value(Enums::TypesOfVehicleInJourney).canConvert(QVariant::List) ) {
                QVariantList vars = value( Enums::TypesOfVehicleInJourney ).toList();
                foreach( const QVariant &var, vars ) {
                    if ( var.canConvert(QVariant::Int) ) {
                        int vehicleType = var.toInt();
                        if ( !vehicleTypes.contains(vehicleType) ) {
                            vehicleTypes << vehicleType;
                        }
                    }
                }
            }

            insert( Enums::TypesOfVehicleInJourney, vehicleTypes );
        }

        // Convert RouteTypesOfVehicles value from a string list or integer list
        // into a list of valid, distinct VehicleType values
        if ( contains(Enums::RouteTypesOfVehicles) ) {
            QVariantList vehicleTypes;
            if ( value(Enums::RouteTypesOfVehicles).canConvert(QVariant::StringList) ) {
                QStringList strings = value( Enums::RouteTypesOfVehicles ).toStringList();
                foreach( const QString &str, strings ) {
                    vehicleTypes << static_cast<int>( getVehicleTypeFromString(str) );
                }
            } else if ( value(Enums::RouteTypesOfVehicles).canConvert(QVariant::List) ) {
                QVariantList vars = value( Enums::RouteTypesOfVehicles ).toList();
                foreach( const QVariant &var, vars ) {
                    if ( var.canConvert(QVariant::Int) ) {
                        vehicleTypes << var.toInt();
                    }
                }
            }

            insert( Enums::RouteTypesOfVehicles, vehicleTypes );
        }

        // Fix transport line strings in RouteTransportLines, eg. do not repeat vehicle type name
        if ( contains(Enums::RouteTransportLines) ) {
            QStringList transportLines = value( Enums::RouteTransportLines ).toStringList();
            for ( int i = 0; i < transportLines.count(); ++i ) {
                transportLines[i] = transportLines[i].trimmed()
                        .remove( QRegExp("^(bus|tro|tram|str)", Qt::CaseInsensitive) )
                        .replace( QRegExp("\\s{2,}"), " " ).trimmed();
            }
            insert( Enums::RouteTransportLines, transportLines );
        }

        // Convert date value from string if not given as date
        if ( contains(Enums::ArrivalDate) && (!value(Enums::ArrivalDate).canConvert(QVariant::Date)
                                    || !value(Enums::ArrivalDate).toDate().isValid()) )
        {
            if ( value(Enums::ArrivalDate).canConvert(QVariant::String) ) {
                const QString sDate = value(Enums::ArrivalDate).toString();
                QDate date = QDate::fromString( sDate, "dd.MM.yyyy" );

                if ( !date.isValid() ) {
                    int currentYear = QDate::currentDate().year();
                    date = QDate::fromString( sDate, "dd.MM.yy" );
                    if ( date.year() <= currentYear - 99 ) {
                        date.setDate( currentYear, date.month(), date.day() );
                    }
                }
                insert( Enums::ArrivalDate, date );
            } else {
                kDebug() << "Arrival date is in wrong format:" << value(Enums::ArrivalDate);
                remove( Enums::ArrivalDate );
            }
        }

        if ( !contains(Enums::ArrivalDateTime) ) {
            if ( contains(Enums::ArrivalTime) ) {
                const QTime time = value( Enums::ArrivalTime ).toTime();
                QDate date;
                if ( contains(Enums::ArrivalDate) ) {
                    date = value( Enums::ArrivalDate ).toDate();
                } else if ( time < QTime::currentTime().addSecs(-5 * 60) ) {
                    kDebug() << "Guessed ArrivalDate as tomorrow";
                    date = QDate::currentDate().addDays( 1 );
                } else {
                    kDebug() << "Guessed ArrivalDate as today";
                    date = QDate::currentDate();
                }
                insert( Enums::ArrivalDateTime, QDateTime(date, time) );
                remove( Enums::ArrivalDate );
                remove( Enums::ArrivalTime );
            } else {
                kDebug() << "No ArrivalDateTime or ArrivalTime information given";
            }
        }
        if ( value(Enums::Duration).toInt() <= 0 && contains(Enums::DepartureDateTime) &&
             contains(Enums::ArrivalDateTime) )
        {
            QDateTime departure = value( Enums::DepartureDateTime ).toDateTime();
            QDateTime arrival = value( Enums::ArrivalDateTime ).toDateTime();
            int minsDuration = departure.secsTo( arrival ) / 60;
            if ( minsDuration < 0 ) {
                kDebug() << "Calculated duration is negative" << minsDuration
                        << "departure" << departure << "arrival" << arrival;
                insert( Enums::Duration, -1 );
            } else {
                insert( Enums::Duration, minsDuration );
            }
        }

        // Convert duration value given as string in "h:mm" format to minutes
        if ( contains(Enums::Duration) && value(Enums::Duration).toInt() <= 0 &&
             value(Enums::Duration).canConvert(QVariant::String) )
        {
            QString duration = value( Enums::Duration ).toString();
            QTime timeDuration = QTime::fromString( duration, "h:mm" );
            if ( timeDuration.isValid() ) {
                const int minsDuration = QTime( 0, 0 ).secsTo( timeDuration ) / 60;
                insert( Enums::Duration, minsDuration );
            }
        }

        // Convert route departure time values to QTime
        if ( contains(Enums::RouteTimesDeparture) ) {
            QVariantList times;
            if ( value(Enums::RouteTimesDeparture).canConvert(QVariant::StringList) ) {
                QStringList strings = value( Enums::RouteTimesDeparture ).toStringList();
                foreach( const QString &str, strings ) {
                    times << QTime::fromString( str.trimmed(), "hh:mm" );
                }
            } else if ( value(Enums::RouteTimesDeparture).canConvert(QVariant::List) ) {
                QVariantList vars = value( Enums::RouteTimesDeparture ).toList();
                foreach( const QVariant &var, vars ) {
                    times << var.toTime();
                }
            }

            insert( Enums::RouteTimesDeparture, times );
        }

        // Convert route arrival time values to QTime
        if ( contains(Enums::RouteTimesArrival) ) {
            QVariantList times;
            if ( value(Enums::RouteTimesArrival).canConvert(QVariant::StringList) ) {
                QStringList strings = value( Enums::RouteTimesArrival ).toStringList();
                foreach( const QString &str, strings ) {
                    times << QTime::fromString( str.trimmed(), "hh:mm" );
                }
            } else if ( value(Enums::RouteTimesArrival).canConvert(QVariant::List) ) {
                QVariantList vars = value( Enums::RouteTimesArrival ).toList();
                foreach( const QVariant &var, vars ) {
                    times << var.toTime();
                }
            }

            insert( Enums::RouteTimesArrival, times );
        }
    }

    if ( corrections.testFlag(CombineToPreferredValueType) ) {
        // If the duration value is invalid, but there are departure and arrival date and time
        // values available, calculate the duration between departure and arrival
        if ( value(Enums::Duration).toInt() <= 0 && contains(Enums::DepartureDate) && contains(Enums::DepartureTime) &&
             contains(Enums::ArrivalDate) && contains(Enums::ArrivalTime) )
        {
            QDateTime departure( value(Enums::DepartureDate).toDate(), value(Enums::ArrivalTime).toTime() );
            QDateTime arrival( value(Enums::ArrivalDate).toDate(), value(Enums::ArrivalTime).toTime() );
            int minsDuration = departure.secsTo( arrival ) / 60;
            if ( minsDuration < 0 ) {
                kDebug() << "Calculated duration is negative" << minsDuration
                        << "departure" << departure << "arrival" << arrival;
                insert( Enums::Duration, -1 );
            } else {
                insert( Enums::Duration, minsDuration );
            }
        }
    }

    m_isValid = contains( Enums::DepartureDateTime ) && contains( Enums::ArrivalDateTime )
            && contains( Enums::StartStopName ) && contains( Enums::TargetStopName );
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

StopInfo::StopInfo( const QString& name, const QString& id, int weight, const QString& city,
                    const QString& countryCode,  QObject *parent  ) : PublicTransportInfo(parent)
{
    insert( Enums::StopName, name );
    if ( !id.isNull() ) {
        insert( Enums::StopID, id );
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
                         value(Enums::RouteTimes).toStringList().count();
        if ( difference > 0 ) {
            // More route stops than times, add invalid times
            kDebug() << "The script stored" << difference << "more route stops than route times "
                        "for a departure, invalid route times will be added";
            QVariantList routeTimes = value( Enums::RouteTimes ).toList();
            while ( difference > 0 ) {
                routeTimes << QTime();
                --difference;
            }
        } else {
            // More route times than stops, add empty stops
            kDebug() << "The script stored" << -difference << "more route times than route "
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

Enums::VehicleType PublicTransportInfo::getVehicleTypeFromString( const QString& sLineType )
{
    QString sLineTypeLower = sLineType.trimmed().toLower();

    // TODO: Remove most special string, eg. german translations like "u-bahn" and
    //       have the scripts only use correct vehicle type strings

    // See http://en.wikipedia.org/wiki/Train_categories_in_Europe
    if ( sLineTypeLower == QLatin1String("subway") ||
            sLineTypeLower == QLatin1String("u-bahn") ||
            sLineTypeLower == QLatin1String("ubahn") ||
            sLineTypeLower == QLatin1String("u") ||
            sLineTypeLower == QLatin1String("rt") || // regio tram TODO Which service provider uses this?
            sLineTypeLower.toInt() == static_cast<int>(Enums::Subway) ) {
        return Enums::Subway;
    } else if ( sLineTypeLower == QLatin1String("interurban") ||
            sLineTypeLower == QLatin1String("interurbantrain") ||
            sLineTypeLower == QLatin1String("s-bahn") ||
            sLineTypeLower == QLatin1String("sbahn") ||
            sLineTypeLower == QLatin1String("s") ||
            sLineTypeLower == QLatin1String("rsb") || // "regio-s-bahn", TODO move to au_oebb
            sLineTypeLower.toInt() == static_cast<int>(Enums::InterurbanTrain) ) {
        return Enums::InterurbanTrain;
    } else if ( sLineTypeLower == QLatin1String("tram") ||
            sLineTypeLower == QLatin1String("straßenbahn") ||
            sLineTypeLower == QLatin1String("str") ||
            sLineTypeLower == QLatin1String("stb") || // "stadtbahn", germany
            sLineTypeLower == QLatin1String("dm_train") ||
            sLineTypeLower == QLatin1String("streetcar (tram)") || // for sk_imhd TODO move to the script
            sLineTypeLower.toInt() == static_cast<int>(Enums::Tram) ) { //
        return Enums::Tram;
    } else if ( sLineTypeLower == QLatin1String("bus") ||
            sLineTypeLower == QLatin1String("dm_bus") ||
            sLineTypeLower == QLatin1String("express bus") || // for sk_imhd
            sLineTypeLower == QLatin1String("night line - bus") || // for sk_imhd TODO move to the script
            sLineTypeLower.toInt() == static_cast<int>(Enums::Bus) ) {
        return Enums::Bus;
    } else if ( sLineTypeLower == QLatin1String("metro") ||
            sLineTypeLower == QLatin1String("m") ||
            sLineTypeLower.toInt() == static_cast<int>(Enums::Metro) ) {
        return Enums::Metro;
    } else if ( sLineTypeLower == QLatin1String("trolleybus") ||
            sLineTypeLower == QLatin1String("tro") ||
            sLineTypeLower == QLatin1String("trolley bus") ||
            sLineTypeLower.startsWith(QLatin1String("trolleybus")) || // for sk_imhd
            sLineTypeLower.toInt() == static_cast<int>(Enums::TrolleyBus) ) {
        return Enums::TrolleyBus;
    } else if ( sLineTypeLower == QLatin1String("regionaltrain") ||
            sLineTypeLower == QLatin1String("regional") ||
            sLineTypeLower == QLatin1String("rb") || // Regional, "RegionalBahn", germany
            sLineTypeLower == QLatin1String("me") || // "Metronom", germany
            sLineTypeLower == QLatin1String("mer") || // "Metronom", germany
            sLineTypeLower == QLatin1String("mr") || // "Märkische Regiobahn", germany
            sLineTypeLower == QLatin1String("erb") || // "EuroBahn", germany
            sLineTypeLower == QLatin1String("wfb") || // "WestfalenBahn", germany
            sLineTypeLower == QLatin1String("nwb") || // "NordWestBahn", germany
            sLineTypeLower == QLatin1String("osb") || // "Ortenau-S-Bahn GmbH" (no interurban train), germany
            sLineTypeLower == QLatin1String("r") || // austria, switzerland
            sLineTypeLower == QLatin1String("os") || // czech, "Osobní vlak", basic local (stopping) trains
            sLineTypeLower.toInt() == static_cast<int>(Enums::RegionalTrain) ) {
        return Enums::RegionalTrain;
    } else if ( sLineTypeLower == QLatin1String("regionalexpresstrain") ||
            sLineTypeLower == QLatin1String("regionalexpress") ||
            sLineTypeLower == QLatin1String("re") || // RegionalExpress
            sLineTypeLower == QLatin1String("rer") || // france, Reseau Express Regional
            sLineTypeLower == QLatin1String("sp") || // czech, "Spěšný vlak", semi-fast trains (Eilzug)
            sLineTypeLower == QLatin1String("zr") || // slovakia, "Zrýchlený vlak", train serving almost all stations en route fast
            sLineTypeLower == QLatin1String("regional express trains") || // used by gares-en-mouvement.com (france)
            sLineTypeLower.toInt() == static_cast<int>(Enums::RegionalExpressTrain) ) {
        return Enums::RegionalExpressTrain;
    } else if ( sLineTypeLower == QLatin1String("interregionaltrain") ||
            sLineTypeLower == QLatin1String("interregional") ||
            sLineTypeLower == QLatin1String("ir") || // InterRegio
            sLineTypeLower == QLatin1String("d") || // schnellzug, swiss
            sLineTypeLower == QLatin1String("ire") ||  // almost a local train (Nahverkehrszug) stopping at few stations; semi-fast
            sLineTypeLower == QLatin1String("er") || // border-crossing local train stopping at few stations; semi-fast
            sLineTypeLower == QLatin1String("ex") || // czech, express trains with no supplementary fare, similar to the German Interregio or also Regional-Express
            sLineTypeLower == QLatin1String("express") ||
            sLineTypeLower.toInt() == static_cast<int>(Enums::InterregionalTrain) ) {
        return Enums::InterregionalTrain;
    } else if ( sLineTypeLower == QLatin1String("intercitytrain") ||
            sLineTypeLower == QLatin1String("eurocitytrain") ||
            sLineTypeLower == QLatin1String("intercity") ||
            sLineTypeLower == QLatin1String("eurocity") ||
            sLineTypeLower == QLatin1String("ec_ic") || // Eurocity / Intercity
            sLineTypeLower == QLatin1String("ic") || // Intercity
            sLineTypeLower == QLatin1String("ec") || // Eurocity
            sLineTypeLower == QLatin1String("cnl") || // CityNightLine
            sLineTypeLower == QLatin1String("en") || // EuroNight
            sLineTypeLower == QLatin1String("nz") || // "Nachtzug"
            sLineTypeLower == QLatin1String("icn") || // national long-distance train with tilting technology
            sLineTypeLower.toInt() == static_cast<int>(Enums::IntercityTrain) ) {
        return Enums::IntercityTrain;
    } else if ( sLineTypeLower == QLatin1String("highspeedtrain") ||
            sLineTypeLower == QLatin1String("highspeed") ||
            sLineTypeLower == QLatin1String("ice") || // germany
            sLineTypeLower == QLatin1String("tgv") ||  // france
            sLineTypeLower == QLatin1String("tha") ||  // thalys
            sLineTypeLower == QLatin1String("hst") || // great britain
            sLineTypeLower == QLatin1String("est") || // eurostar
            sLineTypeLower == QLatin1String("es") || // eurostar, High-speed, tilting trains for long-distance services
            sLineTypeLower.toInt() == static_cast<int>(Enums::HighSpeedTrain) ) {
        return Enums::HighSpeedTrain;
    } else if ( sLineTypeLower == QLatin1String("feet") ||
            sLineTypeLower == QLatin1String("by feet") ||
            sLineTypeLower == QLatin1String("fu&#223;weg") ||
            sLineTypeLower == QLatin1String("fu&szlig;weg") ||
            sLineTypeLower == QLatin1String("fussweg") ||
            sLineTypeLower == QLatin1String("zu fu&#223;") ||
            sLineTypeLower == QLatin1String("zu fu&szlig;") ||
            sLineTypeLower == QLatin1String("zu fuss") ||
            sLineTypeLower == QLatin1String("&#220;bergang") ||
            sLineTypeLower == QLatin1String("uebergang") ||
            sLineTypeLower == QLatin1String("&uuml;bergang") ||
            sLineTypeLower.toInt() == static_cast<int>(Enums::Feet) ) {
        return Enums::Feet;
    } else if ( sLineTypeLower == QLatin1String("ferry") ||
            sLineTypeLower == QLatin1String("faehre") ||
            sLineTypeLower.toInt() == static_cast<int>(Enums::Ferry) ) {
        return Enums::Ferry;
    } else if ( sLineTypeLower == QLatin1String("ship") ||
            sLineTypeLower == QLatin1String("boat") ||
            sLineTypeLower == QLatin1String("schiff") ||
            sLineTypeLower.toInt() == static_cast<int>(Enums::Ship) ) {
        return Enums::Ship;
    } else if ( sLineTypeLower == QLatin1String("plane") ||
            sLineTypeLower == QLatin1String("air") ||
            sLineTypeLower == QLatin1String("aeroplane") ||
            sLineTypeLower.toInt() == static_cast<int>(Enums::Plane) ) {
        return Enums::Plane;
    } else {
        return Enums::Unknown;
    }
}

QString PublicTransportInfo::operatorFromVehicleTypeString( const QString& sLineType )
{
    QString sLineTypeLower = sLineType.trimmed().toLower();

    if ( sLineTypeLower == QLatin1String("me") ) {
        return "metronom Eisenbahngesellschaft mbH";
    } else if ( sLineTypeLower == QLatin1String("mer") ) {
        return "metronom regional";
    } else if ( sLineTypeLower == QLatin1String("arr") ) {
        return "Arriva";
    } else if ( sLineTypeLower == QLatin1String("abg") ) {
        return "Anhaltische Bahn Gesellschaft mbH";
    } else if ( sLineTypeLower == QLatin1String("abr") ) {
        return "ABELLIO Rail NRW GmbH";
    } else if ( sLineTypeLower == QLatin1String("akn") ) {
        return "AKN Eisenbahn AG";
    } else if ( sLineTypeLower == QLatin1String("alx") ) {
        return "alex (Vogtlandbahn GmbH)";
    } else if ( sLineTypeLower == QLatin1String("bsb") ) {
        return "Breisgau-S-Bahn GmbH";
    } else if ( sLineTypeLower == QLatin1String("byb") ) {
        return "BayernBahn GmbH";
    } else if ( sLineTypeLower == QLatin1String("cb") ) {
        return "City Bahn Chemnitz GmbH";
    } else if ( sLineTypeLower == QLatin1String("cx") ) {
        return "Connex";
    } else if ( sLineTypeLower == QLatin1String("dab") ) {
        return "Daadetalbahn, Züge der Westerwaldbahn GmbH";
    } else if ( sLineTypeLower == QLatin1String("eb") ) {
        return "Erfurter Bahn GmbH";
    } else if ( sLineTypeLower == QLatin1String("erb") ) {
        return "eurobahn Rhenus Keolis GmbH & Co. KG";
    } else if ( sLineTypeLower == QLatin1String("evb") ) {
        return "Eisenbahnen und Verkehrsbetriebe Elbe-Weser GmbH";
    } else if ( sLineTypeLower == QLatin1String("feg") ) {
        return "Freiberger Eisenbahngesellschaft mbH";
    } else if ( sLineTypeLower == QLatin1String("hex") ) {
        return "HarzElbeExpress";
    } else if ( sLineTypeLower == QLatin1String("hlb") ) {
        return "Hessische Landesbahn GmbH, HLB Basis AG, HLB Hessenbahn GmbH";
    } else if ( sLineTypeLower == QLatin1String("hsb") ) {
        return "Harzer Schmalspurbahnen GmbH";
    } else if ( sLineTypeLower == QLatin1String("htb") ) {
        return "HellertalBahn GmbH";
    } else if ( sLineTypeLower == QLatin1String("hzl") ) {
        return "Hohenzollerische Landesbahn AG";
    } else if ( sLineTypeLower == QLatin1String("lb") ) {
        return "Lausitzbahn";
    } else if ( sLineTypeLower == QLatin1String("lx") ) {
        return "Lausitz-Express";
    } else if ( sLineTypeLower == QLatin1String("mbb") ) {
        return "Mecklenburgische Bäderbahn „Molli“ GmbH";
    } else if ( sLineTypeLower == QLatin1String("mel") ) {
        return "Museums-Eisenbahn-Club Losheim";
    } else if ( sLineTypeLower == QLatin1String("mr") ) {
        return "Märkische Regiobahn";
    } else if ( sLineTypeLower == QLatin1String("mrb") ) {
        return "Mitteldeutsche Regiobahn";
    } else if ( sLineTypeLower == QLatin1String("msb") ) {
        return "Mainschleifenbahn";
    } else if ( sLineTypeLower == QLatin1String("nbe") ) {
        return "nordbahn Eisenbahngesellschaft mbH & Co KG";
    } else if ( sLineTypeLower == QLatin1String("neb") ) {
        return "NEB Betriebsgesellschaft mbH";
    } else if ( sLineTypeLower == QLatin1String("neg") ) {
        return "Norddeutsche Eisenbahn Gesellschaft Niebüll GmbH";
    } else if ( sLineTypeLower == QLatin1String("nob") ) {
        return "Nord-Ostsee-Bahn GmbH";
    } else if ( sLineTypeLower == QLatin1String("nwb") ) {
        return "NordWestBahn";
    } else if ( sLineTypeLower == QLatin1String("oe") ) {
        return "Ostdeutsche Eisenbahn GmbH";
    } else if ( sLineTypeLower == QLatin1String("ola") ) {
        return "Ostseeland Verkehr GmbH";
    } else if ( sLineTypeLower == QLatin1String("osb") ) {
        return "Ortenau-S-Bahn GmbH";
    } else if ( sLineTypeLower == QLatin1String("pre") ) {
        return "Eisenbahn-Bau- und Betriebsgesellschaft Pressnitztalbahn mbH";
    } else if ( sLineTypeLower == QLatin1String("peg") ) {
        return "Prignitzer Eisenbahn GmbH";
    } else if ( sLineTypeLower == QLatin1String("rnv") ) {
        return "Rhein-Neckar-Verkehr GmbH";
    } else if ( sLineTypeLower == QLatin1String("rt") ) {
        return "RegioTram KVG Kasseler Verkehrsgesellschaft mbH";
    } else if ( sLineTypeLower == QLatin1String("rtb") ) {
        return "Rurtalbahn GmbH";
    } else if ( sLineTypeLower == QLatin1String("sbb") ) {
        return "SBB GmbH";
    } else if ( sLineTypeLower == QLatin1String("sbe") ) {
        return "Sächsisch-Böhmische Eisenbahngesellschaft mbH";
    } else if ( sLineTypeLower == QLatin1String("sdg") ) {
        return "Sächsische Dampfeisenbahngesellschaft mbH";
    } else if ( sLineTypeLower == QLatin1String("shb") ) {
        return "Schleswig-Holstein-Bahn GmbH";
    } else if ( sLineTypeLower == QLatin1String("soe") ) {
        return "Sächsisch-Oberlausitzer Eisenbahngesellschaft mbH";
    } else if ( sLineTypeLower == QLatin1String("ssb") ) {
        return "Elektrische Bahnen der Stadt Bonn und des Rhein-Sieg-Kreises";
    } else if ( sLineTypeLower == QLatin1String("swb") ) {
        return "Stadtwerke Bonn Verkehrs-GmbH";
    } else if ( sLineTypeLower == QLatin1String("swe") ) {
        return "Südwestdeutsche Verkehrs-AG";
    } else if ( sLineTypeLower == QLatin1String("ubb") ) {
        return "Usedomer Bäderbahn GmbH";
    } else if ( sLineTypeLower == QLatin1String("vbg") ) {
        return "Vogtlandbahn GmbH";
    } else if ( sLineTypeLower == QLatin1String("vec") ) {
        return "vectus Verkehrsgesellschaft mbH";
    } else if ( sLineTypeLower == QLatin1String("via") ) {
        return "VIAS GmbH, Frankfurt/Main";
    } else if ( sLineTypeLower == QLatin1String("vx") ) {
        return "Vogtland-Express, Express-Zug der Vogtlandbahn-GmbH";
    } else if ( sLineTypeLower == QLatin1String("weg") ) {
        return "Württembergische Eisenbahn-Gesellschaft mbH";
    } else if ( sLineTypeLower == QLatin1String("wfb") ) {
        return "WestfalenBahn";
    } else if ( sLineTypeLower == QLatin1String("x") ) {
        return "InterConnex";
    } else if ( sLineTypeLower == QLatin1String("can") ) {
        return "cantus Verkehrsgesellschaft mbH";
    } else if ( sLineTypeLower == QLatin1String("erb") ) {
        return "EuroBahn";
    } else {
        return QString();
    }
}

QDateTime PublicTransportInfo::departure() const
{
    return value( Enums::DepartureDateTime ).toDateTime();
}

QString PublicTransportInfo::operatorName() const
{
    return contains( Enums::Operator ) ? value( Enums::Operator ).toString() : QString();
}

QStringList PublicTransportInfo::routeStops( PublicTransportInfo::StopNameOptions stopNameOptions ) const
{
    switch ( stopNameOptions ) {
    case UseShortenedStopNames:
        return contains(Enums::RouteStopsShortened) ? value(Enums::RouteStopsShortened).toStringList()
                                             : routeStops(UseFullStopNames);
    case UseFullStopNames:
    default:
        return contains( Enums::RouteStops )
                ? value( Enums::RouteStops ).toStringList() : QStringList();
    }
}

int PublicTransportInfo::routeExactStops() const
{
    return contains( Enums::RouteExactStops ) ? value( Enums::RouteExactStops ).toInt() : 0;
}

QString DepartureInfo::target( PublicTransportInfo::StopNameOptions stopNameOptions ) const
{
    switch ( stopNameOptions ) {
    case UseShortenedStopNames:
        return contains(Enums::TargetShortened) ? value(Enums::TargetShortened).toString()
                                         : target(UseFullStopNames);
    case UseFullStopNames:
    default:
        return contains( Enums::Target ) ? value( Enums::Target ).toString() : QString();
    }
}

QDateTime JourneyInfo::arrival() const
{
    return value( Enums::ArrivalDateTime ).toDateTime();
}

QList< Enums::VehicleType > JourneyInfo::vehicleTypes() const
{
    if ( contains( Enums::TypesOfVehicleInJourney ) ) {
        QVariantList listVariant = value( Enums::TypesOfVehicleInJourney ).toList();
        QList<Enums::VehicleType> listRet;
        foreach( QVariant vehicleType, listVariant ) {
            listRet.append( static_cast<Enums::VehicleType>( vehicleType.toInt() ) );
        }
        return listRet;
    } else {
        return QList<Enums::VehicleType>();
    }
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

QVariantList JourneyInfo::vehicleTypesVariant() const
{
    return contains( Enums::TypesOfVehicleInJourney )
           ? value( Enums::TypesOfVehicleInJourney ).toList() : QVariantList();
}

QVariantList JourneyInfo::routeVehicleTypesVariant() const
{
    return contains( Enums::RouteTypesOfVehicles )
           ? value( Enums::RouteTypesOfVehicles ).toList() : QVariantList();
}

QStringList JourneyInfo::routeTransportLines() const
{
    return contains( Enums::RouteTransportLines )
           ? value( Enums::RouteTransportLines ).toStringList() : QStringList();
}

QStringList JourneyInfo::routePlatformsDeparture() const
{
    return contains( Enums::RoutePlatformsDeparture )
           ? value( Enums::RoutePlatformsDeparture ).toStringList() : QStringList();
}

QStringList JourneyInfo::routePlatformsArrival() const
{
    return contains( Enums::RoutePlatformsArrival )
           ? value( Enums::RoutePlatformsArrival ).toStringList() : QStringList();
}

int JourneyInfo::changes() const
{
    return contains( Enums::Changes ) ? value( Enums::Changes ).toInt() : -1;
}

QVariantList JourneyInfo::routeTimesDepartureVariant() const
{
    return contains( Enums::RouteTimesDeparture )
           ? value( Enums::RouteTimesDeparture ).toList() : QVariantList();
}

QList< QTime > JourneyInfo::routeTimesDeparture() const
{
    if ( contains(Enums::RouteTimesDeparture) ) {
        QList<QTime> ret;
        QVariantList times = value( Enums::RouteTimesDeparture ).toList();
        foreach( QVariant time, times ) {
            ret << time.toTime();
        }
        return ret;
    } else {
        return QList<QTime>();
    }
}

QVariantList JourneyInfo::routeTimesArrivalVariant() const
{
    return contains( Enums::RouteTimesArrival )
           ? value( Enums::RouteTimesArrival ).toList() : QVariantList();
}

QList< QTime > JourneyInfo::routeTimesArrival() const
{
    if ( contains(Enums::RouteTimesArrival) ) {
        QList<QTime> ret;
        QVariantList times = value( Enums::RouteTimesArrival ).toList();
        foreach( QVariant time, times ) {
            ret << time.toTime();
        }
        return ret;
    } else {
        return QList<QTime>();
    }
}

QVariantList JourneyInfo::routeTimesDepartureDelay() const
{
    if ( contains( Enums::RouteTimesDepartureDelay ) ) {
        return value( Enums::RouteTimesDepartureDelay ).toList();
    } else {
        return QVariantList();
    }
}

QVariantList JourneyInfo::routeTimesArrivalDelay() const
{
    if ( contains(Enums::RouteTimesArrivalDelay) ) {
        return value( Enums::RouteTimesArrivalDelay ).toList();
    } else {
        return QVariantList();
    }
}

QList< QTime > DepartureInfo::routeTimes() const
{
    if ( contains(Enums::RouteTimes) ) {
        QList<QTime> ret;
        QVariantList times = value( Enums::RouteTimes ).toList();
        foreach( QVariant time, times ) {
            ret << time.toTime();
        }
        return ret;
    } else {
        return QList<QTime>();
    }
}
