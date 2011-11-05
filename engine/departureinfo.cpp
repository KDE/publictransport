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

#include "departureinfo.h"
#include <KDebug>

PublicTransportInfo::PublicTransportInfo( const QHash< TimetableInformation, QVariant >& data )
    : QHash<TimetableInformation, QVariant>(data)
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
        if ( !contains(Operator)
                || (value(Operator).canConvert(QVariant::String)
                     && value(Operator).toString().isEmpty()) ) {
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
    if ( contains(DepartureDate) && (!value(DepartureDate).canConvert(QVariant::Date)
                || !value(DepartureDate).toDate().isValid()) ) {
        if ( value(DepartureDate).canConvert(QVariant::String) ) {
            QString sDate = value(DepartureDate).toString();
            insert( DepartureDate, QDate::fromString(sDate, "dd.MM.yyyy") );

            if ( !value(DepartureDate).toDate().isValid() ) {
                int currentYear = QDate::currentDate().year();
                QDate date = QDate::fromString( sDate, "dd.MM.yy" );
                if ( date.year() <= currentYear - 99 ) {
                    date.setDate( currentYear, date.month(), date.day() );
                }

                insert( DepartureDate, date );
            }
        } else {
            kDebug() << "Departure date is in wrong format:" << value( DepartureDate );
            remove( DepartureDate );
        }
    }

    // Guess date value if none is given,
    // this could produce wrong dates (better give DepartureDate in scripts)
    if ( contains(DepartureHour) && contains(DepartureMinute) && !contains(DepartureDate) ) {
        QTime departureTime = QTime( value(DepartureHour).toInt(),
                                     value(DepartureMinute).toInt() );
        if ( departureTime < QTime::currentTime().addSecs(-5 * 60) ) {
            kDebug() << "Guessed DepartureDate as tomorrow";
            insert( DepartureDate, QDate::currentDate().addDays(1) );
        } else {
            kDebug() << "Guessed DepartureDate as today";
            insert( DepartureDate, QDate::currentDate() );
        }
        insert( DepartureYear, value(DepartureDate).toDate().year() );
    }

    if ( contains(RouteStops) ) {
        // Find words at the beginning/end of target and route stop names that have many
        // occurrences. These words are most likely the city names where the stops are in.
        // But the timetable becomes easier to read and looks nicer, if not each stop name
        // includes the same city name.
        QStringList stops = value( RouteStops ).toStringList();
        QString target = value( Target ).toString();
        QHash< QString, int > firstWordCounts; // Counts occurrences of words at the beginning
        QHash< QString, int > lastWordCounts; // Counts occurrences of words at the end

        // This regular expression gets used to search for word at the end, possibly including
        // a colon before the last word
        QRegExp rxLastWord( ",?\\s*\\S+$" );

        // First count the first/last word of the target stop name
        int pos = target.indexOf(' ');
        if ( pos > 0 ) {
            firstWordCounts.insert( target.left(pos), 1 );
        }
        int lastPos = rxLastWord.indexIn( target );
        if ( lastPos != -1 ) {
            lastWordCounts.insert( rxLastWord.cap(), 1 );
        }

        // Then count the first/last words of all route stop stop names
        QString removeFirstWord;
        QString removeLastWord;

        // Break if 70% or at least three of the stop names begin/end with the same word
        int minCount = qMax( 3, int(stops.count() * 0.7) );
        foreach ( const QString &stop, stops ) {
            // Get first word
            pos = stop.indexOf(' ');
            if ( pos > 0 ) {
                // Count first word of the current stop name
                const QString word = stop.left( pos );
                if ( firstWordCounts.contains(word) ) {
                    ++firstWordCounts[word];
                    if ( firstWordCounts[word] > minCount ) {
                        // Enough occurrences found
                        removeFirstWord = word;
                        break;
                    }
                } else {
                    // First occurrences of word at the beginning found
                    firstWordCounts.insert( word, 1 );
                }
            }

            // Get last word
            lastPos = rxLastWord.indexIn( stop );
            if ( lastPos != -1 ) {
                // Count last word of the current stop name
                const QString word = rxLastWord.cap();
                if ( lastWordCounts.contains(word) ) {
                    ++lastWordCounts[word];
                    if ( lastWordCounts[word] > minCount ) {
                        removeLastWord = word;
                        // Enough occurrences found
                        break;
                    }
                } else {
                    // First occurrences of word at the end found
                    lastWordCounts.insert( word, 1 );
                }
            }
        }

        // If no first/last word with at least minCount occurrences was found,
        // find the word with the most occurrences
        if ( removeFirstWord.isEmpty() && removeLastWord.isEmpty() ) {
            int max = 0;

            // Find word at the beginning with most occurrences
            for ( QHash< QString, int >::ConstIterator it = firstWordCounts.constBegin();
                  it != firstWordCounts.constEnd(); ++it )
            {
                if ( it.value() > max ) {
                    max = it.value();
                    removeFirstWord = it.key();
                }
            }

            // Find word at the end with more occurrences
            for ( QHash< QString, int >::ConstIterator it = lastWordCounts.constBegin();
                  it != lastWordCounts.constEnd(); ++it )
            {
                if ( it.value() > max ) {
                    max = it.value();
                    removeLastWord = it.key();
                }
            }

            if ( max < 3 ) {
                // The first/last word with the most occurrences has less than three occurrences
                // Do not remove any word
                removeFirstWord.clear();
                removeLastWord.clear();
            } else if ( !removeLastWord.isEmpty() ) {
                // removeLastWord has more occurrences than removeFirstWord
                removeFirstWord.clear();
            }
        }

        if ( !removeFirstWord.isEmpty() ) {
            // Remove removeFirstWord from all stop names
            if ( target.startsWith(removeFirstWord) ) {
                target = target.mid( removeFirstWord.length() + 1 );
                insert( Target, target );
            }

            for ( int i = 0; i < stops.count(); ++i ) {
                if ( stops[i].startsWith(removeFirstWord) ) {
                    stops[i] = stops[i].mid( removeFirstWord.length() + 1 );
                }
            }
            insert( RouteStops, stops );
        } else if ( !removeLastWord.isEmpty() ) {
            // Remove removeLastWord from all stop names
            if ( target.endsWith(removeLastWord) ) {
                target = target.left( target.length() - removeLastWord.length() );
                insert( Target, target );
            }

            for ( int i = 0; i < stops.count(); ++i ) {
                if ( stops[i].endsWith(removeLastWord) ) {
                    stops[i] = stops[i].left( stops[i].length() - removeLastWord.length() );
                }
            }
            insert( RouteStops, stops );
        }
    }
}

JourneyInfo::JourneyInfo( const QHash< TimetableInformation, QVariant >& data )
        : PublicTransportInfo( data )
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

    if ( contains(ArrivalHour) && contains(ArrivalMinute) && !contains(ArrivalDate) ) {
        QTime arrivalTime = QTime( value(ArrivalHour).toInt(),
                                   value(ArrivalMinute).toInt() );
        if ( arrivalTime < QTime::currentTime().addSecs(-5 * 60) ) {
            insert( ArrivalDate, QDate::currentDate().addDays(1) );
        } else {
            insert( ArrivalDate, QDate::currentDate() );
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
    if ( value(Duration).toInt() <= 0
            && contains(DepartureDate)
            && contains(DepartureHour) && contains(DepartureMinute)
            && contains(ArrivalDate)
            && contains(ArrivalHour) && contains(ArrivalMinute) )
    {
        QDateTime departure( value(DepartureDate).toDate(),
                             QTime( value(DepartureHour).toInt(),
                                    value(DepartureMinute).toInt() ) );
        QDateTime arrival( value(ArrivalDate).toDate(),
                           QTime( value(ArrivalHour).toInt(),
                                  value(ArrivalMinute).toInt() ) );
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

    m_isValid = contains( DepartureHour ) && contains( DepartureMinute )
            && contains( StartStopName ) && contains( TargetStopName )
            && contains( ArrivalHour ) && contains( ArrivalMinute );
}

StopInfo::StopInfo()
{
    m_isValid = false;
}

StopInfo::StopInfo(const QHash< TimetableInformation, QVariant >& data)
    : QHash<TimetableInformation, QVariant>(data)
{
    m_isValid = contains( StopName );
}

StopInfo::StopInfo(const QString& name, const QString& id, int weight, const QString& city,
                   const QString& countryCode)
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

DepartureInfo::DepartureInfo() : PublicTransportInfo()
{
    m_isValid = false;
}

DepartureInfo::DepartureInfo( const QHash<TimetableInformation, QVariant> &data )
        : PublicTransportInfo( data )
{
    m_isValid = contains( TransportLine ) && contains( Target )
            && contains( DepartureHour ) && contains( DepartureMinute );
}

VehicleType PublicTransportInfo::getVehicleTypeFromString( const QString& sLineType )
{
    QString sLineTypeLower = sLineType.trimmed().toLower();

    // See http://en.wikipedia.org/wiki/Train_categories_in_Europe
    if ( sLineTypeLower == "u-bahn" ||
            sLineTypeLower == "ubahn" ||
            sLineTypeLower == "u" ||
            sLineTypeLower == "subway" ||
            sLineTypeLower == "rt" || // regio tram TODO Which service provider uses this?
            sLineTypeLower.toInt() == static_cast<int>(Subway) ) {
        return Subway;
    } else if ( sLineTypeLower == "s-bahn" ||
            sLineTypeLower == "sbahn" ||
            sLineTypeLower == "s_bahn" ||
            sLineTypeLower == "s" ||
            sLineTypeLower == "interurban" ||
            sLineTypeLower == "rsb" || // "regio-s-bahn", TODO move to au_oebb
            sLineTypeLower.toInt() == static_cast<int>(InterurbanTrain) ) {
        return InterurbanTrain;
    } else if ( sLineTypeLower == "tram" ||
            sLineTypeLower == "straßenbahn" ||
            sLineTypeLower == "str" ||
            sLineTypeLower == "stb" || // "stadtbahn", germany
            sLineTypeLower == "dm_train" ||
            sLineTypeLower == "streetcar (tram)" || // for sk_imhd TODO move to the script
            sLineTypeLower.toInt() == static_cast<int>(Tram) ) { //
        return Tram;
    } else if ( sLineTypeLower == "bus" ||
            sLineTypeLower == "dm_bus" ||
            sLineTypeLower == "express bus" || // for sk_imhd
            sLineTypeLower == "night line - bus" || // for sk_imhd TODO move to the script
            sLineTypeLower.toInt() == static_cast<int>(Bus) ) {
        return Bus;
    } else if ( sLineTypeLower == "metro" ||
            sLineTypeLower == "m" ||
            sLineTypeLower.toInt() == static_cast<int>(Metro) ) {
        return Metro;
    } else if ( sLineTypeLower == "tro" ||
            sLineTypeLower == "trolleybus" ||
            sLineTypeLower == "trolley bus" ||
            sLineTypeLower.startsWith(QLatin1String("trolleybus")) || // for sk_imhd
            sLineTypeLower.toInt() == static_cast<int>(TrolleyBus) ) {
        return TrolleyBus;
    } else if ( sLineTypeLower == "rb" || // Regional, "RegionalBahn", germany
            sLineTypeLower == "me" || // "Metronom", germany
            sLineTypeLower == "mer" || // "Metronom", germany
            sLineTypeLower == "mr" || // "Märkische Regiobahn", germany
            sLineTypeLower == "erb" || // "EuroBahn", germany
            sLineTypeLower == "wfb" || // "WestfalenBahn", germany
            sLineTypeLower == "nwb" || // "NordWestBahn", germany
            sLineTypeLower == "osb" || // "Ortenau-S-Bahn GmbH" (no interurban train), germany
            sLineTypeLower == "regional" ||
            sLineTypeLower == "r" || // austria, switzerland
            sLineTypeLower == "os" || // czech, "Osobní vlak", basic local (stopping) trains
            sLineTypeLower.toInt() == static_cast<int>(RegionalTrain) ) {
        return RegionalTrain;
    } else if ( sLineTypeLower == "re" || // RegionalExpress
            sLineTypeLower == "rer" || // france, Reseau Express Regional
            sLineTypeLower == "sp" || // czech, "Spěšný vlak", semi-fast trains (Eilzug)
            sLineTypeLower == "zr" || // slovakia, "Zrýchlený vlak", train serving almost all stations en route fast
            sLineTypeLower == "regional express" ||
            sLineTypeLower == "regional express trains" || // used by gares-en-mouvement.com (france)
            sLineTypeLower.toInt() == static_cast<int>(RegionalExpressTrain) ) {
        return RegionalExpressTrain;
    } else if ( sLineTypeLower == "ir" || // InterRegio
            sLineTypeLower == "d" || // schnellzug, swiss
            sLineTypeLower == "ire" ||  // almost a local train (Nahverkehrszug) stopping at few stations; semi-fast
            sLineTypeLower == "er" ||  // border-crossing local train stopping at few stations; semi-fast
            sLineTypeLower == "ex" || // czech, express trains with no supplementary fare, similar to the German Interregio or also Regional-Express
            sLineTypeLower == "express" ||
            sLineTypeLower == "interregional" ||
            sLineTypeLower.toInt() == static_cast<int>(InterregionalTrain) ) {
        return InterregionalTrain;
    } else if ( sLineTypeLower == "ec_ic" || // Eurocity / Intercity
            sLineTypeLower == "ic" || // Intercity
            sLineTypeLower == "ec" || // Eurocity
            sLineTypeLower == "intercity" ||
            sLineTypeLower == "eurocity" ||
            sLineTypeLower == "cnl" || // CityNightLine
            sLineTypeLower == "en" || // EuroNight
            sLineTypeLower == "nz" || // "Nachtzug"
            sLineTypeLower == "icn" || // national long-distance train with tilting technology
            sLineTypeLower.toInt() == static_cast<int>(IntercityTrain) ) {
        return IntercityTrain;
    } else if ( sLineTypeLower == "ice" || // germany
            sLineTypeLower == "tgv" ||  // france
            sLineTypeLower == "tha" ||  // thalys
            sLineTypeLower == "hst" || // great britain
            sLineTypeLower == "est" || // eurostar
            sLineTypeLower == "es" || // eurostar, High-speed, tilting trains for long-distance services
            sLineTypeLower == "highspeed train" ||
            sLineTypeLower.toInt() == static_cast<int>(HighSpeedTrain) ) {
        return HighSpeedTrain;
    } else if ( sLineTypeLower == "feet" ||
            sLineTypeLower == "by feet" ||
            sLineTypeLower == "fu&#223;weg" ||
            sLineTypeLower == "fu&szlig;weg" ||
            sLineTypeLower == "fussweg" ||
            sLineTypeLower == "zu fu&#223;" ||
            sLineTypeLower == "zu fu&szlig;" ||
            sLineTypeLower == "zu fuss" ||
            sLineTypeLower == "&#220;bergang" ||
            sLineTypeLower == "uebergang" ||
            sLineTypeLower == "&uuml;bergang" ||
            sLineTypeLower.toInt() == static_cast<int>(Feet) ) {
        return Feet;
    } else if ( sLineTypeLower == "ferry" ||
            sLineTypeLower == "faehre" ||
            sLineTypeLower.toInt() == static_cast<int>(Ferry) ) {
        return Ferry;
    } else if ( sLineTypeLower == "ship" ||
            sLineTypeLower == "boat" ||
            sLineTypeLower == "schiff" ||
            sLineTypeLower.toInt() == static_cast<int>(Ship) ) {
        return Ship;
    } else if ( sLineTypeLower == "plane" ||
            sLineTypeLower == "air" ||
            sLineTypeLower == "aeroplane" ||
            sLineTypeLower.toInt() == static_cast<int>(Plane) ) {
        return Plane;
    } else {
        return Unknown;
    }
}

QString PublicTransportInfo::operatorFromVehicleTypeString( const QString& sLineType )
{
    QString sLineTypeLower = sLineType.trimmed().toLower();

    if ( sLineTypeLower == "me" ) {
        return "metronom Eisenbahngesellschaft mbH";
    } else if ( sLineTypeLower == "mer" ) {
        return "metronom regional";
    } else if ( sLineTypeLower == "arr" ) {
        return "Arriva";
    } else if ( sLineTypeLower == "abg" ) {
        return "Anhaltische Bahn Gesellschaft mbH";
    } else if ( sLineTypeLower == "abr" ) {
        return "ABELLIO Rail NRW GmbH";
    } else if ( sLineTypeLower == "akn" ) {
        return "AKN Eisenbahn AG";
    } else if ( sLineTypeLower == "alx" ) {
        return "alex (Vogtlandbahn GmbH)";
    } else if ( sLineTypeLower == "bsb" ) {
        return "Breisgau-S-Bahn GmbH";
    } else if ( sLineTypeLower == "byb" ) {
        return "BayernBahn GmbH";
    } else if ( sLineTypeLower == "cb" ) {
        return "City Bahn Chemnitz GmbH";
    } else if ( sLineTypeLower == "cx" ) {
        return "Connex";
    } else if ( sLineTypeLower == "dab" ) {
        return "Daadetalbahn, Züge der Westerwaldbahn GmbH";
    } else if ( sLineTypeLower == "eb" ) {
        return "Erfurter Bahn GmbH";
    } else if ( sLineTypeLower == "erb" ) {
        return "eurobahn Rhenus Keolis GmbH & Co. KG";
    } else if ( sLineTypeLower == "evb" ) {
        return "Eisenbahnen und Verkehrsbetriebe Elbe-Weser GmbH";
    } else if ( sLineTypeLower == "feg" ) {
        return "Freiberger Eisenbahngesellschaft mbH";
    } else if ( sLineTypeLower == "hex" ) {
        return "HarzElbeExpress";
    } else if ( sLineTypeLower == "hlb" ) {
        return "Hessische Landesbahn GmbH, HLB Basis AG, HLB Hessenbahn GmbH";
    } else if ( sLineTypeLower == "hsb" ) {
        return "Harzer Schmalspurbahnen GmbH";
    } else if ( sLineTypeLower == "htb" ) {
        return "HellertalBahn GmbH";
    } else if ( sLineTypeLower == "hzl" ) {
        return "Hohenzollerische Landesbahn AG";
    } else if ( sLineTypeLower == "lb" ) {
        return "Lausitzbahn";
    } else if ( sLineTypeLower == "lx" ) {
        return "Lausitz-Express";
    } else if ( sLineTypeLower == "mbb" ) {
        return "Mecklenburgische Bäderbahn „Molli“ GmbH";
    } else if ( sLineTypeLower == "mel" ) {
        return "Museums-Eisenbahn-Club Losheim";
    } else if ( sLineTypeLower == "mr" ) {
        return "Märkische Regiobahn";
    } else if ( sLineTypeLower == "mrb" ) {
        return "Mitteldeutsche Regiobahn";
    } else if ( sLineTypeLower == "msb" ) {
        return "Mainschleifenbahn";
    } else if ( sLineTypeLower == "nbe" ) {
        return "nordbahn Eisenbahngesellschaft mbH & Co KG";
    } else if ( sLineTypeLower == "neb" ) {
        return "NEB Betriebsgesellschaft mbH";
    } else if ( sLineTypeLower == "neg" ) {
        return "Norddeutsche Eisenbahn Gesellschaft Niebüll GmbH";
    } else if ( sLineTypeLower == "nob" ) {
        return "Nord-Ostsee-Bahn GmbH";
    } else if ( sLineTypeLower == "nwb" ) {
        return "NordWestBahn";
    } else if ( sLineTypeLower == "oe" ) {
        return "Ostdeutsche Eisenbahn GmbH";
    } else if ( sLineTypeLower == "ola" ) {
        return "Ostseeland Verkehr GmbH";
    } else if ( sLineTypeLower == "osb" ) {
        return "Ortenau-S-Bahn GmbH";
    } else if ( sLineTypeLower == "pre" ) {
        return "Eisenbahn-Bau- und Betriebsgesellschaft Pressnitztalbahn mbH";
    } else if ( sLineTypeLower == "peg" ) {
        return "Prignitzer Eisenbahn GmbH";
    } else if ( sLineTypeLower == "rnv" ) {
        return "Rhein-Neckar-Verkehr GmbH";
    } else if ( sLineTypeLower == "rt" ) {
        return "RegioTram KVG Kasseler Verkehrsgesellschaft mbH";
    } else if ( sLineTypeLower == "rtb" ) {
        return "Rurtalbahn GmbH";
    } else if ( sLineTypeLower == "sbb" ) {
        return "SBB GmbH";
    } else if ( sLineTypeLower == "sbe" ) {
        return "Sächsisch-Böhmische Eisenbahngesellschaft mbH";
    } else if ( sLineTypeLower == "sdg" ) {
        return "Sächsische Dampfeisenbahngesellschaft mbH";
    } else if ( sLineTypeLower == "shb" ) {
        return "Schleswig-Holstein-Bahn GmbH";
    } else if ( sLineTypeLower == "soe" ) {
        return "Sächsisch-Oberlausitzer Eisenbahngesellschaft mbH";
    } else if ( sLineTypeLower == "ssb" ) {
        return "Elektrische Bahnen der Stadt Bonn und des Rhein-Sieg-Kreises";
    } else if ( sLineTypeLower == "swb" ) {
        return "Stadtwerke Bonn Verkehrs-GmbH";
    } else if ( sLineTypeLower == "swe" ) {
        return "Südwestdeutsche Verkehrs-AG";
    } else if ( sLineTypeLower == "ubb" ) {
        return "Usedomer Bäderbahn GmbH";
    } else if ( sLineTypeLower == "vbg" ) {
        return "Vogtlandbahn GmbH";
    } else if ( sLineTypeLower == "vec" ) {
        return "vectus Verkehrsgesellschaft mbH";
    } else if ( sLineTypeLower == "via" ) {
        return "VIAS GmbH, Frankfurt/Main";
    } else if ( sLineTypeLower == "vx" ) {
        return "Vogtland-Express, Express-Zug der Vogtlandbahn-GmbH";
    } else if ( sLineTypeLower == "weg" ) {
        return "Württembergische Eisenbahn-Gesellschaft mbH";
    } else if ( sLineTypeLower == "wfb" ) {
        return "WestfalenBahn";
    } else if ( sLineTypeLower == "x" ) {
        return "InterConnex";
    } else if ( sLineTypeLower == "can" ) {
        return "cantus Verkehrsgesellschaft mbH";
    } else if ( sLineTypeLower == "erb" ) {
        return "EuroBahn";
    } else {
        return QString();
    }
}

QDateTime PublicTransportInfo::departure() const
{
    if ( contains( DepartureDate ) ) {
        return QDateTime( value( DepartureDate ).toDate(),
                          QTime( value( DepartureHour ).toInt(),
                                 value( DepartureMinute ).toInt() ) );
    } else {
        return QDateTime( QDate::currentDate(),
                          QTime( value( DepartureHour ).toInt(),
                                 value( DepartureMinute ).toInt() ) );
    }
}

QString PublicTransportInfo::operatorName() const
{
    return contains( Operator ) ? value( Operator ).toString() : QString();
}

QStringList PublicTransportInfo::routeStops() const
{
    return contains( RouteStops ) ? value( RouteStops ).toStringList() : QStringList();
}

int PublicTransportInfo::routeExactStops() const
{
    return contains( RouteExactStops ) ? value( RouteExactStops ).toInt() : 0;
}

QDateTime JourneyInfo::arrival() const
{
    if ( contains( ArrivalDate ) ) {
        return QDateTime( value( ArrivalDate ).toDate(),
                          QTime( value( ArrivalHour ).toInt(),
                                 value( ArrivalMinute ).toInt() ) );
    } else if ( contains( ArrivalHour ) && contains( ArrivalMinute ) ) {
        return QDateTime( QDate::currentDate(),
                          QTime( value( ArrivalHour ).toInt(),
                                 value( ArrivalMinute ).toInt() ) );
    } else {
        return QDateTime();
    }
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
