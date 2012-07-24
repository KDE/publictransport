/*
 *   Copyright 2012 Friedrich Pülz <fpuelz@gmx.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *    published by the Free Software Foundation; either version 2 or
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

#include "global.h"

#include <KDebug>
#include <KIconEffect>
#include <KIconLoader>
#include <KGlobal>
#include <KLocale>
#include <Plasma/Theme>

#include <qmath.h>
#include <QPainter>
#include <KColorUtils>

// KCatalogLoader got fixed in KDE 4.6.2, before there're linker errors
#if KDE_VERSION >= KDE_MAKE_VERSION(4,6,2)
static const KCatalogLoader loader("libpublictransporthelper");
#endif

namespace PublicTransport {

GeneralVehicleType Global::generalVehicleType( VehicleType vehicleType )
{
    switch ( vehicleType ) {
    case Tram:
    case Bus:
    case TrolleyBus:
    case InterurbanTrain:
    case Subway:
    case Metro:
        return LocalPublicTransport;

    case RegionalTrain:
    case RegionalExpressTrain:
    case InterregionalTrain:
    case IntercityTrain:
    case HighSpeedTrain:
        return Train;

    case Ferry:
    case Ship:
        return WaterVehicle;

    case Plane:
        return AirVehicle;

    case UnknownVehicleType:
    default:
        return UnknownVehicle;
    }
}

KIcon Global::internationalIcon()
{
    // Size of the flag icons is 22x16 => 16x11.64
    QPixmap pixmap = QPixmap( 32, 32 );
    pixmap.fill( Qt::transparent );
    QPainter p( &pixmap );

    QStringList icons = QStringList() << "gb" << "de" << "es" << "jp";
    int yOffset = 12;
    int i = 0, x, y = 4;
    foreach( const QString &sIcon, icons ) {
        if ( i % 2 == 0 ) { // icon on the left
            x = 0;
        } else { // icon on the right
            x = 16;
        }

        QPixmap pixmapFlag = KIcon( sIcon ).pixmap( 16 );
        p.drawPixmap( x, y, 16, 12, pixmapFlag );

        if ( i % 2 != 0 ) {
            y += yOffset;
        }
        ++i;
    }
    p.end();

    KIcon resultIcon = KIcon();
    resultIcon.addPixmap( pixmap, QIcon::Normal );
    return resultIcon;
}

KIcon Global::vehicleTypeToIcon( const VehicleType &vehicleType/*, const QString &overlayIcon*/ )
{
    KIcon icon;
    switch ( vehicleType ) {
    case Tram:
        icon = KIcon( "vehicle_type_tram" );
        break;
    case Bus:
        icon = KIcon( "vehicle_type_bus" );
        break;
    case Subway:
        icon = KIcon( "vehicle_type_subway" );
        break;
    case Metro:
        icon = KIcon( "vehicle_type_metro" );
        break;
    case TrolleyBus:
        icon = KIcon( "vehicle_type_trolleybus" );
        break;
    case Feet:
        icon = KIcon( "vehicle_type_feet" );
        break;

    case TrainInterurban:
        icon = KIcon( "vehicle_type_train_interurban" );
        break;
    case TrainRegional: // Icon not done yet, using this for now
    case TrainRegionalExpress:
        icon = KIcon( "vehicle_type_train_regional" );
        break;
    case TrainInterregio:
        icon = KIcon( "vehicle_type_train_interregional" );
        break;
    case TrainIntercityEurocity:
        icon = KIcon( "vehicle_type_train_intercity" );
        break;
    case TrainIntercityExpress:
        icon = KIcon( "vehicle_type_train_highspeed" );
        break;

    case Ferry:
    case Ship:
        icon = KIcon( "vehicle_type_ferry" );
        break;
    case Plane:
        icon = KIcon( "vehicle_type_plane" );
        break;

    case UnknownVehicleType:
    default:
        icon = KIcon( "status_unknown" );
    }

//     if ( !overlayIcon.isEmpty() ) {
//         icon = makeOverlayIcon( icon, overlayIcon );
//     }

    return icon;
}

KIcon Global::iconFromVehicleTypeList( const QList< VehicleType >& vehicleTypes, int extend )
{
    QPixmap pixmap = QPixmap( extend, extend );
    int halfExtend = extend / 2;
    pixmap.fill( Qt::transparent );
    QPainter p( &pixmap );

    int rows = qCeil(( float )vehicleTypes.count() / 2.0f ); // Two vehicle types per row
    int yOffset = rows <= 1 ? 0 : halfExtend / ( rows - 1 );
    int x, y, i = 0;
    if ( rows == 1 ) {
        y = halfExtend / 2;
    } else {
        y = 0;
    }
    foreach( VehicleType vehicleType, vehicleTypes ) {
        if ( i % 2 == 0 ) { // icon on the left
            if ( i == vehicleTypes.count() - 1 ) { // align last vehicle type to the center
                x = halfExtend / 2;
            } else {
                x = 0;
            }
        } else { // icon on the right
            x = halfExtend;
        }

        QPixmap pixmapVehicleType = vehicleTypeToIcon( vehicleType ).pixmap( halfExtend );
        p.drawPixmap( x, y, pixmapVehicleType );

        if ( i % 2 != 0 ) {
            y += yOffset;
        }
        ++i;
    }
    p.end();

    KIcon resultIcon = KIcon();
    resultIcon.addPixmap( pixmap, QIcon::Normal );
    return resultIcon;
}

// Gets the name of the given type of vehicle
QString Global::vehicleTypeToString( const VehicleType &vehicleType, bool plural )
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
    case TrainInterurban:
        return plural ? i18nc( "@info/plain", "interurban trains" )
               : i18nc( "@info/plain", "interurban train" );
    case Metro:
        return plural ? i18nc( "@info/plain", "metros" )
               : i18nc( "@info/plain", "metro" );
    case TrolleyBus:
        return plural ? i18nc( "@info/plain A trolleybus (also known as trolley bus, trolley "
                               "coach, trackless trolley, trackless tram or trolley) is an "
                               "electric bus that draws its electricity from overhead wires "
                               "(generally suspended from roadside posts) using spring-loaded "
                               "trolley poles.", "trolley buses" )
               : i18nc( "@info/plain A trolleybus (also known as trolley bus, trolley coach, "
                        "trackless trolley, trackless tram or trolley) is an electric bus that "
                        "draws its electricity from overhead wires (generally suspended from "
                        "roadside posts) using spring-loaded trolley poles.", "trolley bus" );

    case TrainRegional:
        return plural ? i18nc( "@info/plain", "regional trains" )
               : i18nc( "@info/plain", "regional train" );
    case TrainRegionalExpress:
        return plural ? i18nc( "@info/plain", "regional express trains" )
               : i18nc( "@info/plain", "regional express train" );
    case TrainInterregio:
        return plural ? i18nc( "@info/plain", "interregional trains" )
               : i18nc( "@info/plain", "interregional train" );
    case TrainIntercityEurocity:
        return plural ? i18nc( "@info/plain", "intercity / eurocity trains" )
               : i18nc( "@info/plain", "intercity / eurocity train" );
    case TrainIntercityExpress:
        return plural ? i18nc( "@info/plain", "intercity express trains" )
               : i18nc( "@info/plain", "intercity express train" );

    case Feet:
        return i18nc( "@info/plain", "footway" );

    case Ferry:
        return plural ? i18nc( "@info/plain", "ferries" )
               : i18nc( "@info/plain", "ferry" );
    case Ship:
        return plural ? i18nc( "@info/plain", "ships" )
               : i18nc( "@info/plain", "ship" );
    case Plane:
        return plural ? i18nc( "@info/plain airplanes", "planes" )
               : i18nc( "@info/plain an airplane", "plane" );

    case UnknownVehicleType:
    default:
        return i18nc( "@info/plain Unknown type of vehicle", "Unknown" );
    }
}

QString Global::durationString( int seconds )
{
    int minutes = ( seconds / 60 ) % 60;
    int hours = seconds / 3600;

    if ( hours > 0 ) {
        if ( minutes > 0 ) {
            return i18nc( "@info/plain Duration string, %1 is hours, %2 minutes with leading zero",
                          "%1:%2 hours", hours,
                          QString("%1").arg(minutes, 2, 10, QLatin1Char('0')) );
        } else {
            return i18ncp( "@info/plain Duration string with zero minutes, %1 is hours",
                           "%1 hour", "%1 hours", hours );
        }
    } else if ( minutes > 0 ) {
        return i18ncp( "@info/plain Duration string with zero hours, %1 is minutes",
                      "%1 minute", "%1 minutes", minutes );
    } else {
        return i18nc( "@info/plain Used as duration string if the duration is less than a minute",
                      "now" );
    }
}

QColor Global::textColorOnSchedule()
{
    QColor color = Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor );
    return KColorUtils::tint( color, Qt::green, 0.5 );
}

QColor Global::textColorDelayed()
{
    QColor color = Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor );
    return KColorUtils::tint( color, Qt::red, 0.5 );
}

} // namespace Timetable
