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

PublicTransportInfo::PublicTransportInfo( const QHash< TimetableInformation, QVariant >& data,
                                          QObject *parent )
    : QObject(parent), m_data(data)
{
    m_isValid = false;

    // Insert -1 as Delay if none is given (-1 means "no delay information available")
    if ( !contains(Delay) ) {
        insert( Delay, -1 );
    }

    // Convert route times list to a list of QTime objects (from string and time objects)
    if ( contains(RouteTimes) ) {
        QVariantList times;
        if ( value(RouteTimes).canConvert(QVariant::List) ) {
            QVariantList vars = value( RouteTimes ).toList();
            foreach( const QVariant &var, vars ) {
                if ( var.canConvert(QVariant::Time) && var.toTime().isValid() ) {
                    times << var.toTime();
                } else if ( var.canConvert(QVariant::String) ) {
                    times << QTime::fromString( var.toString().trimmed(), "hh:mm" );
                }
            }
        }

        insert( RouteTimes, times );
    }

    // Convert vehicle types given as string to the associated enumerable value
    if ( contains(TypeOfVehicle) && value(TypeOfVehicle).canConvert(QVariant::String) ) {
        QString vehicleType = value(TypeOfVehicle).toString();
        insert( TypeOfVehicle, getVehicleTypeFromString(vehicleType) );

        // Try to get operator from vehicle type string if no operator is given
        if ( !contains(Operator) || (value(Operator).canConvert(QVariant::String)
                                     && value(Operator).toString().isEmpty()) )
        {
            QString sOperator = operatorFromVehicleTypeString( vehicleType );
            if ( !sOperator.isNull() ) {
                insert( Operator, sOperator );
            }
        }
    }

    // Fix transport line string and try to get vehicle type and/or operator from it
    if ( contains(TransportLine) ) {
        QString sTransportLine = value(TransportLine).toString();

        // Try to get operator from transport line string if no operator is given
        if ( !contains(Operator) || (value(Operator).canConvert(QVariant::String)
             && value(Operator).toString().isEmpty()) )
        {
            QRegExp rx( "^[abcdefghijklmnopqrstuvwxyz]+", Qt::CaseInsensitive );
            if ( rx.indexIn(sTransportLine) != -1 ) {
                QString sOperator = operatorFromVehicleTypeString( rx.cap() );
                if ( !sOperator.isNull() ) {
                    insert( Operator, sOperator );
                }
            }
        }

        // Try to get vehicle type from transport line string if no vehicle type is given
        if ( !contains(TypeOfVehicle) || value(TypeOfVehicle) == Unknown ) {
            if ( sTransportLine.contains("F&#228;hre") ) {
                insert( TypeOfVehicle, Ferry );
            } else {
                QRegExp rx = QRegExp( "^(bus|tro|tram|str|s|u|m)\\s*", Qt::CaseInsensitive );
                rx.indexIn( sTransportLine );
                QString vehicleType = rx.cap();
                insert( TypeOfVehicle, vehicleType );
            }
        }

        sTransportLine = sTransportLine.trimmed()
                .remove( QRegExp("^(bus|tro|tram|str)", Qt::CaseInsensitive) )
                .replace( QRegExp("\\s{2,}"), " " );
        insert( TransportLine, sTransportLine.trimmed() );
    }

    // Convert date value from string if not given as date
    if ( !contains(DepartureDateTime) ) {
        if ( contains(DepartureTime) ) {
            const QTime time = value( DepartureTime ).toTime();
            QDate date;
            if ( contains(DepartureDate) ) {
                date = value( DepartureDate ).toDate();
            } else if ( time < QTime::currentTime().addSecs(-5 * 60) ) {
                kDebug() << "Guessed DepartureDate as tomorrow";
                date = QDate::currentDate().addDays( 1 );
            } else {
                kDebug() << "Guessed DepartureDate as today";
                date = QDate::currentDate();
            }
            insert( DepartureDateTime, QDateTime(date, time) );
            remove( DepartureDate );
            remove( DepartureTime );
        } else {
            kDebug() << "No DepartureDateTime or DepartureTime information given";
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
}

JourneyInfo::JourneyInfo( const QHash< TimetableInformation, QVariant >& data, QObject *parent )
        : PublicTransportInfo( data, parent )
{
    if ( contains(TypesOfVehicleInJourney) ) {
        QVariantList vehicleTypes;

        if ( value(TypesOfVehicleInJourney).canConvert(QVariant::StringList) ) {
            QStringList strings = value( TypesOfVehicleInJourney ).toStringList();
            foreach( const QString &str, strings ) {
                int vehicleType = static_cast<int>( getVehicleTypeFromString(str) );
                if ( !vehicleTypes.contains( vehicleType ) ) {
                    vehicleTypes << vehicleType;
                }
            }
        } else if ( value(TypesOfVehicleInJourney).canConvert(QVariant::List) ) {
            QVariantList vars = value( TypesOfVehicleInJourney ).toList();
            foreach( const QVariant &var, vars ) {
                if ( var.canConvert(QVariant::Int) ) {
                    int vehicleType = var.toInt();
                    if ( !vehicleTypes.contains( vehicleType ) ) {
                        vehicleTypes << vehicleType;
                    }
                }
            }
        }

        insert( TypesOfVehicleInJourney, vehicleTypes );
    }

    if ( contains(RouteTypesOfVehicles) ) {
        QVariantList vehicleTypes;

        if ( value(RouteTypesOfVehicles).canConvert(QVariant::StringList) ) {
            QStringList strings = value( RouteTypesOfVehicles ).toStringList();
            foreach( const QString &str, strings ) {
                vehicleTypes << static_cast<int>( getVehicleTypeFromString(str) );
            }
        } else if ( value(RouteTypesOfVehicles).canConvert(QVariant::List) ) {
            QVariantList vars = value( RouteTypesOfVehicles ).toList();
            foreach( const QVariant &var, vars ) {
                if ( var.canConvert(QVariant::Int) ) {
                    vehicleTypes << var.toInt();
                }
            }
        }

        insert( RouteTypesOfVehicles, vehicleTypes );
    }

    if ( contains(RouteTransportLines) ) {
        QStringList transportLines = value( RouteTransportLines ).toStringList();
        for ( int i = 0; i < transportLines.count(); ++i ) {
            transportLines[i] = transportLines[i].trimmed()
                    .remove( QRegExp("^(bus|tro|tram|str)", Qt::CaseInsensitive) )
                    .replace( QRegExp("\\s{2,}"), " " ).trimmed();
        }
        insert( RouteTransportLines, transportLines );
    }

    if ( contains(ArrivalDate) && (!value(ArrivalDate).canConvert(QVariant::Date)
                || !value(ArrivalDate).toDate().isValid()) ) {
        if ( value(ArrivalDate).canConvert(QVariant::String) ) {
            QString sDate = value(ArrivalDate).toString();

            insert( ArrivalDate, QDate::fromString(sDate, "dd.MM.yyyy") );

            if ( !value(ArrivalDate).toDate().isValid() ) {
                int currentYear = QDate::currentDate().year();
                QDate date = QDate::fromString( sDate, "dd.MM.yy" );
                if ( date.year() <= currentYear - 99 ) {
                    date.setDate( currentYear, date.month(), date.day() );
                }

                insert( ArrivalDate, date );
            }
        } else {
            kDebug() << "Arrival date is in wrong format:" << value(ArrivalDate);
            remove( ArrivalDate );
        }
    }

    if ( contains(Duration) && value(Duration).toInt() <= 0
         && value(Duration).canConvert(QVariant::String) )
    {
        QString duration = value( Duration ).toString();
        QTime timeDuration = QTime::fromString( duration, "hh:mm" );
        if ( timeDuration.isValid() ) {
            int minsDuration = QTime( 0, 0 ).secsTo( timeDuration ) / 60;
            insert( Duration, minsDuration );
        }
    }
    if ( !contains(ArrivalDateTime) ) {
        if ( contains(ArrivalTime) ) {
            const QTime time = value( ArrivalTime ).toTime();
            QDate date;
            if ( contains(ArrivalDate) ) {
                date = value( ArrivalDate ).toDate();
            } else if ( time < QTime::currentTime().addSecs(-5 * 60) ) {
                kDebug() << "Guessed ArrivalDate as tomorrow";
                date = QDate::currentDate().addDays( 1 );
            } else {
                kDebug() << "Guessed ArrivalDate as today";
                date = QDate::currentDate();
            }
            insert( ArrivalDateTime, QDateTime(date, time) );
            remove( ArrivalDate );
            remove( ArrivalTime );
        } else {
            kDebug() << "No ArrivalDateTime or ArrivalTime information given";
        }
    }
    if ( value(Duration).toInt() <= 0 && contains(DepartureDateTime) && contains(ArrivalDateTime) ) {
        QDateTime departure = value( DepartureDateTime ).toDateTime();
        QDateTime arrival = value( ArrivalDateTime ).toDateTime();
        int minsDuration = departure.secsTo( arrival ) / 60;
        if ( minsDuration < 0 ) {
            kDebug() << "Calculated duration is negative" << minsDuration
                     << "departure" << departure << "arrival" << arrival;
            insert( Duration, -1 );
        } else {
            insert( Duration, minsDuration );
        }
    }

    if ( contains(RouteTimesDeparture) ) {
        QVariantList times;
        if ( value(RouteTimesDeparture).canConvert(QVariant::StringList) ) {
            QStringList strings = value( RouteTimesDeparture ).toStringList();
            foreach( const QString &str, strings ) {
                times << QTime::fromString( str.trimmed(), "hh:mm" );
            }
        } else if ( value(RouteTimesDeparture).canConvert(QVariant::List) ) {
            QVariantList vars = value( RouteTimesDeparture ).toList();
            foreach( const QVariant &var, vars ) {
                times << var.toTime();
            }
        }

        insert( RouteTimesDeparture, times );
    }

    if ( contains(RouteTimesArrival) ) {
        QVariantList times;
        if ( value(RouteTimesArrival).canConvert(QVariant::StringList) ) {
            QStringList strings = value( RouteTimesArrival ).toStringList();
            foreach( const QString &str, strings ) {
                times << QTime::fromString( str.trimmed(), "hh:mm" );
            }
        } else if ( value(RouteTimesArrival).canConvert(QVariant::List) ) {
            QVariantList vars = value( RouteTimesArrival ).toList();
            foreach( const QVariant &var, vars ) {
                times << var.toTime();
            }
        }

        insert( RouteTimesArrival, times );
    }

    m_isValid = contains( DepartureDateTime ) && contains( ArrivalDateTime )
            && contains( StartStopName ) && contains( TargetStopName );
}

StopInfo::StopInfo( QObject *parent ) : PublicTransportInfo(parent)
{
    m_isValid = false;
}

StopInfo::StopInfo( const QHash< TimetableInformation, QVariant >& data, QObject *parent )
    : PublicTransportInfo(parent)
{
    m_data.unite( data );
    m_isValid = contains( StopName );
}

StopInfo::StopInfo( const QString& name, const QString& id, int weight, const QString& city,
                    const QString& countryCode,  QObject *parent  ) : PublicTransportInfo(parent)
{
    insert( StopName, name );
    if ( !id.isNull() ) {
        insert( StopID, id );
    }
    if ( !city.isNull() ) {
        insert( StopCity, city );
    }
    if ( !countryCode.isNull() ) {
        insert( StopCountryCode, countryCode );
    }
    if ( weight != -1 ) {
        insert( StopWeight, weight );
    }

    m_isValid = !name.isEmpty();
}

DepartureInfo::DepartureInfo( QObject *parent ) : PublicTransportInfo(parent)
{
    m_isValid = false;
}

DepartureInfo::DepartureInfo( const TimetableData &data,  QObject *parent )
        : PublicTransportInfo( data, parent )
{
    if ( (contains(RouteStops) || contains(RouteTimes)) &&
         value(RouteTimes).toList().count() != value(RouteStops).toStringList().count() )
    {
        int difference = value(RouteStops).toList().count() -
                         value(RouteTimes).toStringList().count();
        if ( difference > 0 ) {
            // More route stops than times, add invalid times
            kDebug() << "The script stored" << difference << "more route stops than route times "
                        "for a departure, invalid route times will be added";
            QVariantList routeTimes = value( RouteTimes ).toList();
            while ( difference > 0 ) {
                routeTimes << QTime();
                --difference;
            }
        } else {
            // More route times than stops, add empty stops
            kDebug() << "The script stored" << -difference << "more route times than route "
                        "stops for a departure, empty route stops will be added";
            QStringList routeStops = value( RouteStops ).toStringList();
            while ( difference < 0 ) {
                routeStops << QString();
                ++difference;
            }
        }
    }

    m_isValid = contains( TransportLine ) && contains( Target ) && contains( DepartureDateTime );
}

VehicleType PublicTransportInfo::getVehicleTypeFromString( const QString& sLineType )
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
            sLineTypeLower.toInt() == static_cast<int>(Subway) ) {
        return Subway;
    } else if ( sLineTypeLower == QLatin1String("interurban") ||
            sLineTypeLower == QLatin1String("interurbantrain") ||
            sLineTypeLower == QLatin1String("s-bahn") ||
            sLineTypeLower == QLatin1String("sbahn") ||
            sLineTypeLower == QLatin1String("s") ||
            sLineTypeLower == QLatin1String("rsb") || // "regio-s-bahn", TODO move to au_oebb
            sLineTypeLower.toInt() == static_cast<int>(InterurbanTrain) ) {
        return InterurbanTrain;
    } else if ( sLineTypeLower == QLatin1String("tram") ||
            sLineTypeLower == QLatin1String("straßenbahn") ||
            sLineTypeLower == QLatin1String("str") ||
            sLineTypeLower == QLatin1String("stb") || // "stadtbahn", germany
            sLineTypeLower == QLatin1String("dm_train") ||
            sLineTypeLower == QLatin1String("streetcar (tram)") || // for sk_imhd TODO move to the script
            sLineTypeLower.toInt() == static_cast<int>(Tram) ) { //
        return Tram;
    } else if ( sLineTypeLower == QLatin1String("bus") ||
            sLineTypeLower == QLatin1String("dm_bus") ||
            sLineTypeLower == QLatin1String("express bus") || // for sk_imhd
            sLineTypeLower == QLatin1String("night line - bus") || // for sk_imhd TODO move to the script
            sLineTypeLower.toInt() == static_cast<int>(Bus) ) {
        return Bus;
    } else if ( sLineTypeLower == QLatin1String("metro") ||
            sLineTypeLower == QLatin1String("m") ||
            sLineTypeLower.toInt() == static_cast<int>(Metro) ) {
        return Metro;
    } else if ( sLineTypeLower == QLatin1String("trolleybus") ||
            sLineTypeLower == QLatin1String("tro") ||
            sLineTypeLower == QLatin1String("trolley bus") ||
            sLineTypeLower.startsWith(QLatin1String("trolleybus")) || // for sk_imhd
            sLineTypeLower.toInt() == static_cast<int>(TrolleyBus) ) {
        return TrolleyBus;
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
            sLineTypeLower.toInt() == static_cast<int>(RegionalTrain) ) {
        return RegionalTrain;
    } else if ( sLineTypeLower == QLatin1String("regionalexpresstrain") ||
            sLineTypeLower == QLatin1String("regionalexpress") ||
            sLineTypeLower == QLatin1String("re") || // RegionalExpress
            sLineTypeLower == QLatin1String("rer") || // france, Reseau Express Regional
            sLineTypeLower == QLatin1String("sp") || // czech, "Spěšný vlak", semi-fast trains (Eilzug)
            sLineTypeLower == QLatin1String("zr") || // slovakia, "Zrýchlený vlak", train serving almost all stations en route fast
            sLineTypeLower == QLatin1String("regional express trains") || // used by gares-en-mouvement.com (france)
            sLineTypeLower.toInt() == static_cast<int>(RegionalExpressTrain) ) {
        return RegionalExpressTrain;
    } else if ( sLineTypeLower == QLatin1String("interregionaltrain") ||
            sLineTypeLower == QLatin1String("interregional") ||
            sLineTypeLower == QLatin1String("ir") || // InterRegio
            sLineTypeLower == QLatin1String("d") || // schnellzug, swiss
            sLineTypeLower == QLatin1String("ire") ||  // almost a local train (Nahverkehrszug) stopping at few stations; semi-fast
            sLineTypeLower == QLatin1String("er") || // border-crossing local train stopping at few stations; semi-fast
            sLineTypeLower == QLatin1String("ex") || // czech, express trains with no supplementary fare, similar to the German Interregio or also Regional-Express
            sLineTypeLower == QLatin1String("express") ||
            sLineTypeLower.toInt() == static_cast<int>(InterregionalTrain) ) {
        return InterregionalTrain;
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
            sLineTypeLower.toInt() == static_cast<int>(IntercityTrain) ) {
        return IntercityTrain;
    } else if ( sLineTypeLower == QLatin1String("highspeedtrain") ||
            sLineTypeLower == QLatin1String("highspeed") ||
            sLineTypeLower == QLatin1String("ice") || // germany
            sLineTypeLower == QLatin1String("tgv") ||  // france
            sLineTypeLower == QLatin1String("tha") ||  // thalys
            sLineTypeLower == QLatin1String("hst") || // great britain
            sLineTypeLower == QLatin1String("est") || // eurostar
            sLineTypeLower == QLatin1String("es") || // eurostar, High-speed, tilting trains for long-distance services
            sLineTypeLower.toInt() == static_cast<int>(HighSpeedTrain) ) {
        return HighSpeedTrain;
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
            sLineTypeLower.toInt() == static_cast<int>(Feet) ) {
        return Feet;
    } else if ( sLineTypeLower == QLatin1String("ferry") ||
            sLineTypeLower == QLatin1String("faehre") ||
            sLineTypeLower.toInt() == static_cast<int>(Ferry) ) {
        return Ferry;
    } else if ( sLineTypeLower == QLatin1String("ship") ||
            sLineTypeLower == QLatin1String("boat") ||
            sLineTypeLower == QLatin1String("schiff") ||
            sLineTypeLower.toInt() == static_cast<int>(Ship) ) {
        return Ship;
    } else if ( sLineTypeLower == QLatin1String("plane") ||
            sLineTypeLower == QLatin1String("air") ||
            sLineTypeLower == QLatin1String("aeroplane") ||
            sLineTypeLower.toInt() == static_cast<int>(Plane) ) {
        return Plane;
    } else {
        return Unknown;
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
    return value( DepartureDateTime ).toDateTime();
}

QString PublicTransportInfo::operatorName() const
{
    return contains( Operator ) ? value( Operator ).toString() : QString();
}

QStringList PublicTransportInfo::routeStops( PublicTransportInfo::StopNameOptions stopNameOptions ) const
{
    switch ( stopNameOptions ) {
    case UseShortenedStopNames:
        return contains(RouteStopsShortened) ? value(RouteStopsShortened).toStringList()
                                             : routeStops(UseFullStopNames);
    case UseFullStopNames:
    default:
        return contains( RouteStops ) ? value( RouteStops ).toStringList() : QStringList();
    }
}

int PublicTransportInfo::routeExactStops() const
{
    return contains( RouteExactStops ) ? value( RouteExactStops ).toInt() : 0;
}

QString DepartureInfo::target( PublicTransportInfo::StopNameOptions stopNameOptions ) const
{
    switch ( stopNameOptions ) {
    case UseShortenedStopNames:
        return contains(TargetShortened) ? value(TargetShortened).toString()
                                         : target(UseFullStopNames);
    case UseFullStopNames:
    default:
        return contains( Target ) ? value( Target ).toString() : QString();
    }
}

QDateTime JourneyInfo::arrival() const
{
    return value( ArrivalDateTime ).toDateTime();
}

QList< VehicleType > JourneyInfo::vehicleTypes() const
{
    if ( contains( TypesOfVehicleInJourney ) ) {
        QVariantList listVariant = value( TypesOfVehicleInJourney ).toList();
        QList<VehicleType> listRet;
        foreach( QVariant vehicleType, listVariant ) {
            listRet.append( static_cast<VehicleType>( vehicleType.toInt() ) );
        }
        return listRet;
    } else {
        return QList<VehicleType>();
    }
}

QStringList JourneyInfo::vehicleIconNames() const
{
    if ( !contains( TypesOfVehicleInJourney ) ) {
        return QStringList();
    }
    QVariantList vehicles = value( TypesOfVehicleInJourney ).toList();
    QStringList iconNames;
    foreach( QVariant vehicle, vehicles ) {
        iconNames << Global::vehicleTypeToIcon( static_cast<VehicleType>( vehicle.toInt() ) );
    }
    return iconNames;
}

QStringList JourneyInfo::vehicleNames( bool plural ) const
{
    if ( !contains( TypesOfVehicleInJourney ) ) {
        return QStringList();
    }
    QVariantList vehicles = value( TypesOfVehicleInJourney ).toList();
    QStringList names;
    foreach( QVariant vehicle, vehicles ) {
        names << Global::vehicleTypeToString( static_cast<VehicleType>( vehicle.toInt() ), plural );
    }
    return names;
}

QVariantList JourneyInfo::vehicleTypesVariant() const
{
    return contains( TypesOfVehicleInJourney )
           ? value( TypesOfVehicleInJourney ).toList() : QVariantList();
}

QVariantList JourneyInfo::routeVehicleTypesVariant() const
{
    return contains( RouteTypesOfVehicles )
           ? value( RouteTypesOfVehicles ).toList() : QVariantList();
}

QStringList JourneyInfo::routeTransportLines() const
{
    return contains( RouteTransportLines )
           ? value( RouteTransportLines ).toStringList() : QStringList();
}

QStringList JourneyInfo::routePlatformsDeparture() const
{
    return contains( RoutePlatformsDeparture )
           ? value( RoutePlatformsDeparture ).toStringList() : QStringList();
}

QStringList JourneyInfo::routePlatformsArrival() const
{
    return contains( RoutePlatformsArrival )
           ? value( RoutePlatformsArrival ).toStringList() : QStringList();
}

int JourneyInfo::changes() const
{
    return contains( Changes ) ? value( Changes ).toInt() : -1;
}

QVariantList JourneyInfo::routeTimesDepartureVariant() const
{
    return contains( RouteTimesDeparture )
           ? value( RouteTimesDeparture ).toList() : QVariantList();
}

QList< QTime > JourneyInfo::routeTimesDeparture() const
{
    if ( contains( RouteTimesDeparture ) ) {
        QList<QTime> ret;
        QVariantList times = value( RouteTimesDeparture ).toList();
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
    return contains( RouteTimesArrival )
           ? value( RouteTimesArrival ).toList() : QVariantList();
}

QList< QTime > JourneyInfo::routeTimesArrival() const
{
    if ( contains( RouteTimesArrival ) ) {
        QList<QTime> ret;
        QVariantList times = value( RouteTimesArrival ).toList();
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
    if ( contains( RouteTimesDepartureDelay ) ) {
        return value( RouteTimesDepartureDelay ).toList();
    } else {
        return QVariantList();
    }
}

QVariantList JourneyInfo::routeTimesArrivalDelay() const
{
    if ( contains( RouteTimesArrivalDelay ) ) {
        return value( RouteTimesArrivalDelay ).toList();
    } else {
        return QVariantList();
    }
}

QList< QTime > DepartureInfo::routeTimes() const
{
    if ( contains( RouteTimes ) ) {
        QList<QTime> ret;
        QVariantList times = value( RouteTimes ).toList();
        foreach( QVariant time, times ) {
            ret << time.toTime();
        }
        return ret;
    } else {
        return QList<QTime>();
    }
}
