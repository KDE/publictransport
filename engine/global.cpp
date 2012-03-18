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

VehicleType Global::vehicleTypeFromString( QString sVehicleType )
{
    QString sLower = sVehicleType.toLower();
    if ( sLower == "unknown" ) {
        return Unknown;
    } else if ( sLower == "tram" ) {
        return Tram;
    } else if ( sLower == "bus" ) {
        return Bus;
    } else if ( sLower == "subway" ) {
        return Subway;
    } else if ( sLower == "traininterurban" || sLower == "interurbantrain" ) {
        return InterurbanTrain;
    } else if ( sLower == "metro" ) {
        return Metro;
    } else if ( sLower == "trolleybus" ) {
        return TrolleyBus;
    } else if ( sLower == "trainregional" || sLower == "regionaltrain" ) {
        return RegionalTrain;
    } else if ( sLower == "trainregionalexpress" || sLower == "regionalexpresstrain" ) {
        return RegionalExpressTrain;
    } else if ( sLower == "traininterregio" || sLower == "interregionaltrain" ) {
        return InterregionalTrain;
    } else if ( sLower == "trainintercityeurocity" || sLower == "intercitytrain" ) {
        return IntercityTrain;
    } else if ( sLower == "trainintercityexpress" || sLower == "highspeedtrain" ) {
        return HighSpeedTrain;
    } else if ( sLower == "feet" ) {
        return Feet;
    } else if ( sLower == "ferry" ) {
        return Ferry;
    } else if ( sLower == "ship" ) {
        return Ship;
    } else if ( sLower == "plane" ) {
        return Plane;
    } else {
        return Unknown;
    }
}

QString Global::vehicleTypeToString( const VehicleType& vehicleType, bool plural )
{
    switch ( vehicleType ) {
    case Tram:
        return plural ? i18nc( "@info/plain", "trams" )
               : i18nc( "@info/plain", "tram" );
    case Bus:
        return plural ? i18nc( "@info/plain", "buses" )
               : i18nc( "@info/plain", "bus" );
    case Subway:
        return plural ? i18nc( "@info/plain", "subways" )
               : i18nc( "@info/plain", "subway" );
    case InterurbanTrain:
        return plural ? i18nc( "@info/plain", "interurban trains" )
               : i18nc( "@info/plain", "interurban train" );
    case Metro:
        return plural ? i18nc( "@info/plain", "metros" )
               : i18nc( "@info/plain", "metro" );
    case TrolleyBus:
        return plural ? i18nc( "@info/plain", "trolley buses" )
               : i18nc( "@info/plain", "trolley bus" );

    case RegionalTrain:
        return plural ? i18nc( "@info/plain", "regional trains" )
               : i18nc( "@info/plain", "regional train" );
    case RegionalExpressTrain:
        return plural ? i18nc( "@info/plain", "regional express trains" )
               : i18nc( "@info/plain", "regional express train" );
    case InterregionalTrain:
        return plural ? i18nc( "@info/plain", "interregional trains" )
               : i18nc( "@info/plain", "interregional train" );
    case IntercityTrain:
        return plural ? i18nc( "@info/plain", "intercity / eurocity trains" )
               : i18nc( "@info/plain", "intercity / eurocity train" );
    case HighSpeedTrain:
        return plural ? i18nc( "@info/plain", "intercity express trains" )
               : i18nc( "@info/plain", "intercity express train" );

    case Feet:
        return i18nc( "@info/plain", "Footway" );

    case Ferry:
        return plural ? i18nc( "@info/plain", "ferries" )
               : i18nc( "@info/plain", "ferry" );
    case Ship:
        return plural ? i18nc( "@info/plain", "ships" )
               : i18nc( "@info/plain", "ship" );
    case Plane:
        return plural ? i18nc( "@info/plain airplanes", "planes" )
               : i18nc( "@info/plain an airplane", "plane" );

    case Unknown:
    default:
        return i18nc( "@info/plain Unknown type of vehicle", "Unknown" );
    }
}

QString Global::vehicleTypeToIcon( const VehicleType& vehicleType )
{
    switch ( vehicleType ) {
    case Tram:
        return "vehicle_type_tram";
    case Bus:
        return "vehicle_type_bus";
    case Subway:
        return "vehicle_type_subway";
    case Metro:
        return "vehicle_type_metro";
    case TrolleyBus:
        return "vehicle_type_trolleybus";
    case Feet:
        return "vehicle_type_feet";

//     case TrainInterurban: // DEPRECATED
    case InterurbanTrain:
        return "vehicle_type_train_interurban";
//     case TrainRegional: // DEPRECATED Icon not done yet, using this for now
//     case TrainRegionalExpress: // DEPRECATED
    case RegionalTrain:
    case RegionalExpressTrain:
        return "vehicle_type_train_regional";
//     case TrainInterregio: // DEPRECATED
    case InterregionalTrain:
        return "vehicle_type_train_interregional";
//     case TrainIntercityEurocity: // DEPRECATED
    case IntercityTrain:
        return "vehicle_type_train_intercity";
//     case TrainIntercityExpress: // DEPRECATED
    case HighSpeedTrain:
        return "vehicle_type_train_highspeed";

    case Ferry:
    case Ship:
        return "vehicle_type_ferry";
    case Plane:
        return "vehicle_type_plane";

    case Unknown:
    default:
        return "status_unknown";
    }
}

TimetableInformation Global::timetableInformationFromString(
    const QString& sTimetableInformation )
{
    QString sInfo = sTimetableInformation.toLower();
    if ( sInfo == "nothing" ) {
        return Nothing;
    } else if ( sInfo == "departuredatetime" ) {
        return DepartureDateTime;
    } else if ( sInfo == "departuredate" ) {
        return DepartureDate;
    } else if ( sInfo == "departuretime" ) {
        return DepartureTime;
    } else if ( sInfo == "typeofvehicle" ) {
        return TypeOfVehicle;
    } else if ( sInfo == "transportline" ) {
        return TransportLine;
    } else if ( sInfo == "flightnumber" ) {
        return FlightNumber;
    } else if ( sInfo == "target" ) {
        return Target;
    } else if ( sInfo == "platform" ) {
        return Platform;
    } else if ( sInfo == "delay" ) {
        return Delay;
    } else if ( sInfo == "delayreason" ) {
        return DelayReason;
    } else if ( sInfo == "journeynews" ) {
        return JourneyNews;
    } else if ( sInfo == "journeynewsother" ) {
        return JourneyNewsOther;
    } else if ( sInfo == "journeynewslink" ) {
        return JourneyNewsLink;
    } else if ( sInfo == "status" ) {
        return Status;
    } else if ( sInfo == "routestops" ) {
        return RouteStops;
    } else if ( sInfo == "routetimes" ) {
        return RouteTimes;
    } else if ( sInfo == "routetimesdeparture" ) {
        return RouteTimesDeparture;
    } else if ( sInfo == "routetimesarrival" ) {
        return RouteTimesArrival;
    } else if ( sInfo == "routeexactstops" ) {
        return RouteExactStops;
    } else if ( sInfo == "routetypesofvehicles" ) {
        return RouteTypesOfVehicles;
    } else if ( sInfo == "routetransportlines" ) {
        return RouteTransportLines;
    } else if ( sInfo == "routeplatformsdeparture" ) {
        return RoutePlatformsDeparture;
    } else if ( sInfo == "routeplatformsarrival" ) {
        return RoutePlatformsArrival;
    } else if ( sInfo == "routetimesdeparturedelay" ) {
        return RouteTimesDepartureDelay;
    } else if ( sInfo == "routetimesarrivaldelay" ) {
        return RouteTimesArrivalDelay;
    } else if ( sInfo == "operator" ) {
        return Operator;
    } else if ( sInfo == "duration" ) {
        return Duration;
    } else if ( sInfo == "startstopname" ) {
        return StartStopName;
    } else if ( sInfo == "startstopid" ) {
        return StartStopID;
    } else if ( sInfo == "targetstopname" ) {
        return TargetStopName;
    } else if ( sInfo == "targetstopid" ) {
        return TargetStopID;
    } else if ( sInfo == "arrivaldatetime" ) {
        return ArrivalDateTime;
    } else if ( sInfo == "arrivaldate" ) {
        return ArrivalDate;
    } else if ( sInfo == "arrivaltime" ) {
        return ArrivalTime;
    } else if ( sInfo == "changes" ) {
        return Changes;
    } else if ( sInfo == "typesofvehicleinjourney" ) {
        return TypesOfVehicleInJourney;
    } else if ( sInfo == "pricing" ) {
        return Pricing;
    } else if ( sInfo == "isnightline" ) {
        return IsNightLine;
    } else if ( sInfo == "stopname" ) {
        return StopName;
    } else if ( sInfo == "stopid" ) {
        return StopID;
    } else if ( sInfo == "stopweight" ) {
        return StopWeight;
    } else if ( sInfo == "stopcity" ) {
        return StopCity;
    } else if ( sInfo == "stopcountrycode" ) {
        return StopCountryCode;
    } else {
        kDebug() << sTimetableInformation
        << "is an unknown timetable information value! Assuming value Nothing.";
        return Nothing;
    }
}

QString Global::timetableInformationToString( TimetableInformation timetableInformation )
{
    switch ( timetableInformation ) {
    case Nothing:
        return "Nothing";
    case DepartureDateTime:
        return "DepartureDateTime";
    case DepartureDate:
        return "DepartureDate";
    case DepartureTime:
        return "DepartureTime";
    case TypeOfVehicle:
        return "TypeOfVehicle";
    case TransportLine:
        return "TransportLine";
    case Target:
        return "Target";
    case TargetShortened:
        return "TargetShortened";
    case Platform:
        return "Platform";
    case Delay:
        return "Delay";
    case DelayReason:
        return "DelayReason";
    case JourneyNews:
        return "JourneyNews";
    case JourneyNewsOther:
        return "JourneyNewsOther";
    case JourneyNewsLink:
        return "JourneyNewsLink";
    case Operator:
        return "Operator";
    case Status:
        return "Status";
    case Duration:
        return "Duration";
    case StartStopName:
        return "StartStopName";
    case StartStopID:
        return "StartStopID";
    case StopWeight:
        return "StopWeight";
    case StopCity:
        return "StopCity";
    case StopCountryCode:
        return "StopCountryCode";
    case TargetStopName:
        return "TargetStopName";
    case TargetStopID:
        return "TargetStopID";
    case ArrivalDateTime:
        return "ArrivalDateTime";
    case ArrivalDate:
        return "ArrivalDate";
    case ArrivalTime:
        return "ArrivalTime";
    case Changes:
        return "Changes";
    case TypesOfVehicleInJourney:
        return "TypesOfVehicleInJourney";
    case Pricing:
        return "Pricing";
    case IsNightLine:
        return "IsNightline";
    case StopName:
        return "StopName";
    case StopID:
        return "StopID";
    case RouteStops:
        return "RouteStops";
    case RouteStopsShortened:
        return "RouteStopsShortened";
    case RouteTimes:
        return "RouteTimes";
    case RouteTimesDeparture:
        return "RouteTimesDeparture";
    case RouteTimesArrival:
        return "RouteTimesArrival";
    case RouteExactStops:
        return "RouteExactStops";
    case RouteTypesOfVehicles:
        return "RouteTypesOfVehicles";
    case RouteTransportLines:
        return "RouteTransportLines";
    case RoutePlatformsDeparture:
        return "RoutePlatformsDeparture";
    case RoutePlatformsArrival:
        return "RoutePlatformsArrival";
    case RouteTimesDepartureDelay:
        return "RouteTimesDepartureDelay";
    case RouteTimesArrivalDelay:
        return "RouteTimesArrivalDelay";

    default:
        return QString::number( static_cast<int>(timetableInformation) );
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

QString Global::decodeHtml( const QByteArray& document,
                                             const QByteArray& fallbackCharset )
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
            kDebug() << "No fallback charset, searching codec manually in the HTML";
            QString sDocument = QString( document );
            QRegExp rxCharset( "(?:<head>.*<meta http-equiv=\"Content-Type\" "
                    "content=\"text/html; charset=)([^\"]*)(?:\"[^>]*>)", Qt::CaseInsensitive );
            rxCharset.setMinimal( true );
            if ( rxCharset.indexIn(sDocument) != -1 ) {
                textCodec = QTextCodec::codecForName( rxCharset.cap(1).trimmed().toUtf8() );
            } else {
                kDebug() << "Manual codec search failed, using utf8";
                textCodec = QTextCodec::codecForName( "UTF-8" );
            }
        }
        return textCodec ? textCodec->toUnicode(document) : QString::fromUtf8(document);
    }
}
