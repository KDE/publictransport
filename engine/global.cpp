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
#include "global.h"

// KDE includes
#include <KDebug>
#include <KLocalizedString>

// Qt includes
#include <QRegExp>
#include <QTextCodec>

Enums::VehicleType Global::vehicleTypeFromString( QString sVehicleType )
{
    return Enums::stringToVehicleType( sVehicleType.toAscii().data() );
    QString sLower = sVehicleType.toLower();
    if ( sLower == QLatin1String("unknown") ) {
        return Enums::UnknownVehicleType;
    } else if ( sLower == QLatin1String("tram") ) {
        return Enums::Tram;
    } else if ( sLower == QLatin1String("bus") ) {
        return Enums::Bus;
    } else if ( sLower == QLatin1String("subway") ) {
        return Enums::Subway;
    } else if ( sLower == QLatin1String("interurbantrain") ) {
        return Enums::InterurbanTrain;
    } else if ( sLower == QLatin1String("metro") ) {
        return Enums::Metro;
    } else if ( sLower == QLatin1String("trolleybus") ) {
        return Enums::TrolleyBus;
    } else if ( sLower == QLatin1String("regionaltrain") ) {
        return Enums::RegionalTrain;
    } else if ( sLower == QLatin1String("regionalexpresstrain") ) {
        return Enums::RegionalExpressTrain;
    } else if ( sLower == QLatin1String("interregionaltrain") ) {
        return Enums::InterregionalTrain;
    } else if ( sLower == QLatin1String("intercitytrain") ) {
        return Enums::IntercityTrain;
    } else if ( sLower == QLatin1String("highspeedtrain") ) {
        return Enums::HighSpeedTrain;
    } else if ( sLower == QLatin1String("feet") ) {
        return Enums::Feet;
    } else if ( sLower == QLatin1String("ferry") ) {
        return Enums::Ferry;
    } else if ( sLower == QLatin1String("ship") ) {
        return Enums::Ship;
    } else if ( sLower == QLatin1String("plane") ) {
        return Enums::Plane;
    } else {
        return Enums::InvalidVehicleType;
    }
}

QString Global::vehicleTypeToString( const Enums::VehicleType& vehicleType, bool plural )
{
    switch ( vehicleType ) {
    case Enums::Tram:
        return plural ? i18nc( "@info/plain", "trams" )
               : i18nc( "@info/plain", "tram" );
    case Enums::Bus:
        return plural ? i18nc( "@info/plain", "buses" )
               : i18nc( "@info/plain", "bus" );
    case Enums::Subway:
        return plural ? i18nc( "@info/plain", "subways" )
               : i18nc( "@info/plain", "subway" );
    case Enums::InterurbanTrain:
        return plural ? i18nc( "@info/plain", "interurban trains" )
               : i18nc( "@info/plain", "interurban train" );
    case Enums::Metro:
        return plural ? i18nc( "@info/plain", "metros" )
               : i18nc( "@info/plain", "metro" );
    case Enums::TrolleyBus:
        return plural ? i18nc( "@info/plain", "trolley buses" )
               : i18nc( "@info/plain", "trolley bus" );

    case Enums::RegionalTrain:
        return plural ? i18nc( "@info/plain", "regional trains" )
               : i18nc( "@info/plain", "regional train" );
    case Enums::RegionalExpressTrain:
        return plural ? i18nc( "@info/plain", "regional express trains" )
               : i18nc( "@info/plain", "regional express train" );
    case Enums::InterregionalTrain:
        return plural ? i18nc( "@info/plain", "interregional trains" )
               : i18nc( "@info/plain", "interregional train" );
    case Enums::IntercityTrain:
        return plural ? i18nc( "@info/plain", "intercity / eurocity trains" )
               : i18nc( "@info/plain", "intercity / eurocity train" );
    case Enums::HighSpeedTrain:
        return plural ? i18nc( "@info/plain", "intercity express trains" )
               : i18nc( "@info/plain", "intercity express train" );

    case Enums::Feet:
        return i18nc( "@info/plain", "Footway" );

    case Enums::Ferry:
        return plural ? i18nc( "@info/plain", "ferries" )
               : i18nc( "@info/plain", "ferry" );
    case Enums::Ship:
        return plural ? i18nc( "@info/plain", "ships" )
               : i18nc( "@info/plain", "ship" );
    case Enums::Plane:
        return plural ? i18nc( "@info/plain airplanes", "planes" )
               : i18nc( "@info/plain an airplane", "plane" );

    case Enums::UnknownVehicleType:
    default:
        return i18nc( "@info/plain Unknown type of vehicle", "Unknown" );
    }
}

QString Global::vehicleTypeToIcon( const Enums::VehicleType& vehicleType )
{
    switch ( vehicleType ) {
    case Enums::Tram:
        return "vehicle_type_tram";
    case Enums::Bus:
        return "vehicle_type_bus";
    case Enums::Subway:
        return "vehicle_type_subway";
    case Enums::Metro:
        return "vehicle_type_metro";
    case Enums::TrolleyBus:
        return "vehicle_type_trolleybus";
    case Enums::Feet:
        return "vehicle_type_feet";
    case Enums::InterurbanTrain:
        return "vehicle_type_train_interurban";
    case Enums::RegionalTrain: // aIcon not done yet, using this for now
    case Enums::RegionalExpressTrain:
        return "vehicle_type_train_regional";
    case Enums::InterregionalTrain:
        return "vehicle_type_train_interregional";
    case Enums::IntercityTrain:
        return "vehicle_type_train_intercity";
    case Enums::HighSpeedTrain:
        return "vehicle_type_train_highspeed";

    case Enums::Ferry:
    case Enums::Ship:
        return "vehicle_type_ferry";
    case Enums::Plane:
        return "vehicle_type_plane";

    case Enums::UnknownVehicleType:
    default:
        return "status_unknown";
    }
}

Enums::TimetableInformation Global::timetableInformationFromString(
    const QString& sTimetableInformation )
{
    QString sInfo = sTimetableInformation.toLower();
    if ( sInfo == QLatin1String("nothing") ) {
        return Enums::Nothing;
    } else if ( sInfo == QLatin1String("departuredatetime") ) {
        return Enums::DepartureDateTime;
    } else if ( sInfo == QLatin1String("departuredate") ) {
        return Enums::DepartureDate;
    } else if ( sInfo == QLatin1String("departuretime") ) {
        return Enums::DepartureTime;
    } else if ( sInfo == QLatin1String("typeofvehicle") ) {
        return Enums::TypeOfVehicle;
    } else if ( sInfo == QLatin1String("transportline") ) {
        return Enums::TransportLine;
    } else if ( sInfo == QLatin1String("flightnumber") ) {
        return Enums::FlightNumber;
    } else if ( sInfo == QLatin1String("target") ) {
        return Enums::Target;
    } else if ( sInfo == QLatin1String("platform") ) {
        return Enums::Platform;
    } else if ( sInfo == QLatin1String("delay") ) {
        return Enums::Delay;
    } else if ( sInfo == QLatin1String("delayreason") ) {
        return Enums::DelayReason;
    } else if ( sInfo == QLatin1String("journeynews") ) {
        return Enums::JourneyNews;
    } else if ( sInfo == QLatin1String("journeynewsother") ) {
        return Enums::JourneyNewsOther;
    } else if ( sInfo == QLatin1String("journeynewslink") ) {
        return Enums::JourneyNewsLink;
    } else if ( sInfo == QLatin1String("status") ) {
        return Enums::Status;
    } else if ( sInfo == QLatin1String("routestops") ) {
        return Enums::RouteStops;
    } else if ( sInfo == QLatin1String("routetimes") ) {
        return Enums::RouteTimes;
    } else if ( sInfo == QLatin1String("routetimesdeparture") ) {
        return Enums::RouteTimesDeparture;
    } else if ( sInfo == QLatin1String("routetimesarrival") ) {
        return Enums::RouteTimesArrival;
    } else if ( sInfo == QLatin1String("routeexactstops") ) {
        return Enums::RouteExactStops;
    } else if ( sInfo == QLatin1String("routetypesofvehicles") ) {
        return Enums::RouteTypesOfVehicles;
    } else if ( sInfo == QLatin1String("routetransportlines") ) {
        return Enums::RouteTransportLines;
    } else if ( sInfo == QLatin1String("routeplatformsdeparture") ) {
        return Enums::RoutePlatformsDeparture;
    } else if ( sInfo == QLatin1String("routeplatformsarrival") ) {
        return Enums::RoutePlatformsArrival;
    } else if ( sInfo == QLatin1String("routetimesdeparturedelay") ) {
        return Enums::RouteTimesDepartureDelay;
    } else if ( sInfo == QLatin1String("routetimesarrivaldelay") ) {
        return Enums::RouteTimesArrivalDelay;
    } else if ( sInfo == QLatin1String("operator") ) {
        return Enums::Operator;
    } else if ( sInfo == QLatin1String("duration") ) {
        return Enums::Duration;
    } else if ( sInfo == QLatin1String("startstopname") ) {
        return Enums::StartStopName;
    } else if ( sInfo == QLatin1String("startstopid") ) {
        return Enums::StartStopID;
    } else if ( sInfo == QLatin1String("targetstopname") ) {
        return Enums::TargetStopName;
    } else if ( sInfo == QLatin1String("targetstopid") ) {
        return Enums::TargetStopID;
    } else if ( sInfo == QLatin1String("arrivaldatetime") ) {
        return Enums::ArrivalDateTime;
    } else if ( sInfo == QLatin1String("arrivaldate") ) {
        return Enums::ArrivalDate;
    } else if ( sInfo == QLatin1String("arrivaltime") ) {
        return Enums::ArrivalTime;
    } else if ( sInfo == QLatin1String("changes") ) {
        return Enums::Changes;
    } else if ( sInfo == QLatin1String("typesofvehicleinjourney") ) {
        return Enums::TypesOfVehicleInJourney;
    } else if ( sInfo == QLatin1String("pricing") ) {
        return Enums::Pricing;
    } else if ( sInfo == QLatin1String("isnightline") ) {
        return Enums::IsNightLine;
    } else if ( sInfo == QLatin1String("stopname") ) {
        return Enums::StopName;
    } else if ( sInfo == QLatin1String("stopid") ) {
        return Enums::StopID;
    } else if ( sInfo == QLatin1String("stopweight") ) {
        return Enums::StopWeight;
    } else if ( sInfo == QLatin1String("stopcity") ) {
        return Enums::StopCity;
    } else if ( sInfo == QLatin1String("stopcountrycode") ) {
        return Enums::StopCountryCode;
    } else if ( sInfo == QLatin1String("stoplongitude") ) {
        return Enums::StopLongitude;
    } else if ( sInfo == QLatin1String("stoplatitude") ) {
        return Enums::StopLatitude;
    } else {
        kDebug() << sTimetableInformation
                 << "is an unknown timetable information value! Assuming value Nothing.";
        return Enums::Nothing;
    }
}

QString Global::timetableInformationToString( Enums::TimetableInformation timetableInformation )
{
    return Enums::toString( timetableInformation );
}

bool Global::checkTimetableInformation( Enums::TimetableInformation info, const QVariant &value )
{
    if ( !value.isValid() ) {
        return false;
    }

    switch ( info ) {
    case Enums::DepartureDateTime:
    case Enums::ArrivalDateTime:
        return value.toDateTime().isValid();
    case Enums::DepartureDate:
    case Enums::ArrivalDate:
        return value.toDate().isValid();
    case Enums::DepartureTime:
    case Enums::ArrivalTime:
        return value.toTime().isValid();
    case Enums::TypeOfVehicle:
        return vehicleTypeFromString( value.toString() ) != Enums::UnknownVehicleType;
    case Enums::TransportLine:
    case Enums::Target:
    case Enums::TargetShortened:
    case Enums::Platform:
    case Enums::DelayReason:
    case Enums::JourneyNews:
    case Enums::JourneyNewsOther:
    case Enums::JourneyNewsLink:
    case Enums::Operator:
    case Enums::Status:
    case Enums::StartStopName:
    case Enums::StartStopID:
    case Enums::StopCity:
    case Enums::StopCountryCode:
    case Enums::TargetStopName:
    case Enums::TargetStopID:
    case Enums::Pricing:
    case Enums::StopName:
    case Enums::StopID:
        return !value.toString().trimmed().isEmpty();
    case Enums::StopLongitude:
    case Enums::StopLatitude:
        if ( !value.canConvert(QVariant::Double) ) {
            return false;
        } else {
            bool ok;
            qreal real = value.toReal( &ok );
            return ok;
        }
    case Enums::Delay:
        return value.canConvert( QVariant::Int ) && value.toInt() >= -1;
    case Enums::Duration:
    case Enums::StopWeight:
    case Enums::Changes:
    case Enums::RouteExactStops:
        return value.canConvert( QVariant::Int ) && value.toInt() >= 0;
    case Enums::TypesOfVehicleInJourney:
    case Enums::RouteTimes:
    case Enums::RouteTimesDeparture:
    case Enums::RouteTimesArrival:
    case Enums::RouteTypesOfVehicles:
    case Enums::RouteTimesDepartureDelay:
    case Enums::RouteTimesArrivalDelay:
        return !value.toList().isEmpty();
    case Enums::IsNightLine:
        return value.canConvert( QVariant::Bool );
    case Enums::RouteStops:
    case Enums::RouteStopsShortened:
    case Enums::RouteTransportLines:
    case Enums::RoutePlatformsDeparture:
    case Enums::RoutePlatformsArrival:
        return !value.toStringList().isEmpty();

    default:
        return true;
    }
}

QString Global::decodeHtmlEntities( const QString& html )
{
    if ( html.isEmpty() ) {
        return html;
    }

    QString ret = html;
    QRegExp rx( "(?:&#)([0-9]+)(?:;)" );
    rx.setMinimal( true );
    int pos = 0;
    while ( (pos = rx.indexIn(ret, pos)) != -1 ) {
        const int charCode = rx.cap( 1 ).toInt();
        ret.replace( QString("&#%1;").arg(charCode), QChar(charCode) );
    }

    return ret.replace( QLatin1String("&nbsp;"), QLatin1String(" ") )
              .replace( QLatin1String("&amp;"), QLatin1String("&") )
              .replace( QLatin1String("&lt;"), QLatin1String("<") )
              .replace( QLatin1String("&gt;"), QLatin1String(">") )
              .replace( QLatin1String("&szlig;"), QLatin1String("ß") )
              .replace( QLatin1String("&auml;"), QLatin1String("ä") )
              .replace( QLatin1String("&Auml;"), QLatin1String("Ä") )
              .replace( QLatin1String("&ouml;"), QLatin1String("ö") )
              .replace( QLatin1String("&Ouml;"), QLatin1String("Ö") )
              .replace( QLatin1String("&uuml;"), QLatin1String("ü") )
              .replace( QLatin1String("&Uuml;"), QLatin1String("Ü") );
}

QString Global::encodeHtmlEntities( const QString &html )
{
    if ( html.isEmpty() ) {
        return html;
    }

    QString ret = html;
    return ret.replace( QLatin1String(" "), QLatin1String("&nbsp;") )
              .replace( QLatin1String("&"), QLatin1String("&amp;") )
              .replace( QLatin1String("<"), QLatin1String("&lt;") )
              .replace( QLatin1String(">"), QLatin1String("&gt;") )
              .replace( QLatin1String("ß"), QLatin1String("&szlig;") )
              .replace( QLatin1String("ä"), QLatin1String("&auml;") )
              .replace( QLatin1String("Ä"), QLatin1String("&Auml;") )
              .replace( QLatin1String("ö"), QLatin1String("&ouml;") )
              .replace( QLatin1String("Ö"), QLatin1String("&Ouml;") )
              .replace( QLatin1String("ü"), QLatin1String("&uuml;") )
              .replace( QLatin1String("Ü"), QLatin1String("&Uuml;") );
}

QString Global::decodeHtml( const QByteArray& document, const QByteArray& fallbackCharset )
{
    // Get charset of the received document and convert it to a unicode QString
    // First parse the charset with a regexp to get a fallback charset
    // if QTextCodec::codecForHtml doesn't find the charset
    QTextCodec *textCodec = QTextCodec::codecForHtml( document, 0 );
    if ( textCodec ) {
        return textCodec->toUnicode( document );
    } else {
        if ( !fallbackCharset.isEmpty() ) {
            textCodec = QTextCodec::codecForName( fallbackCharset );
            if ( !textCodec ) {
                kDebug() << "Fallback charset" << fallbackCharset << "not found! Using utf8 now.";
                textCodec = QTextCodec::codecForName( "UTF-8" );
            }
        } else {
            QString sDocument = QString( document );
            QRegExp rxCharset( "(?:<head>.*<meta http-equiv=\"Content-Type\" "
                    "content=\"text/html; charset=)([^\"]*)(?:\"[^>]*>)", Qt::CaseInsensitive );
            rxCharset.setMinimal( true );
            if ( rxCharset.indexIn(sDocument) != -1 ) {
                textCodec = QTextCodec::codecForName( rxCharset.cap(1).trimmed().toUtf8() );
            } else {
                kDebug() << "No fallback charset specified and manual codec search failed, using utf8";
                textCodec = QTextCodec::codecForName( "UTF-8" );
            }
        }
        return textCodec ? textCodec->toUnicode(document) : QString::fromUtf8(document);
    }
}
