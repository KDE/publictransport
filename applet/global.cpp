/*
*   Copyright 2009 Friedrich Pülz <fpuelz@gmx.de>
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

#include "global.h"
#include <QDebug>
#include <qmath.h>


KIcon Global::makeOverlayIcon( const KIcon &icon, const KIcon &overlayIcon, const QSize &overlaySize, int iconExtend ) {
    QPixmap pixmap = icon.pixmap(iconExtend), pixmapOverlay = overlayIcon.pixmap(overlaySize);
    QPainter p(&pixmap);
    p.drawPixmap(QPoint(iconExtend - overlaySize.width(), iconExtend - overlaySize.height()), pixmapOverlay);
    p.end();
    KIcon resultIcon = KIcon();
    resultIcon.addPixmap(pixmap, QIcon::Normal);

    KIconEffect iconEffect;
    pixmap = iconEffect.apply( pixmap, KIconLoader::Small, KIconLoader::ActiveState );
    resultIcon.addPixmap(pixmap, QIcon::Selected);
    resultIcon.addPixmap(pixmap, QIcon::Active);

    return resultIcon;
}

KIcon Global::makeOverlayIcon( const KIcon &icon, const QString &overlayIconName, const QSize &overlaySize, int iconExtend ) {
    return makeOverlayIcon( icon, KIcon(overlayIconName), overlaySize, iconExtend );
}

KIcon Global::makeOverlayIcon ( const KIcon& icon, const QList<KIcon> &overlayIconsBottom, const QSize& overlaySize, int iconExtend ) {
    Q_ASSERT( !icon.isNull() );

    QPixmap pixmap = icon.pixmap(iconExtend);
    if ( pixmap.isNull() ) {
	qDebug() << "Global::makeOverlayIcon" << "pixmap is Null";
	return icon;
    }
//     QPixmap pixmapOverlayBottomLeft = overlayIconBottomLeft.pixmap(overlaySize);
//     QPixmap pixmapOverlayBottomRight = overlayIconBottomRight.pixmap(overlaySize);
    QPainter p(&pixmap);
    int x = 0, xStep = iconExtend / overlayIconsBottom.count();
    foreach( KIcon overlayIcon, overlayIconsBottom ) {
	p.drawPixmap(QPoint(x, iconExtend - overlaySize.height()), overlayIcon.pixmap(overlaySize));
	x += xStep;
    }
//     p.drawPixmap(QPoint(0, iconExtend - overlaySize.height()), pixmapOverlayBottomLeft);
//     p.drawPixmap(QPoint(iconExtend - overlaySize.width(), iconExtend - overlaySize.height()), pixmapOverlayBottomRight);
    p.end();
    KIcon resultIcon = KIcon();
    resultIcon.addPixmap(pixmap, QIcon::Normal);

    KIconEffect iconEffect;
    pixmap = iconEffect.apply( pixmap, KIconLoader::Small, KIconLoader::ActiveState );
    resultIcon.addPixmap(pixmap, QIcon::Selected);
    resultIcon.addPixmap(pixmap, QIcon::Active);

    return resultIcon;
}

KIcon Global::iconFromVehicleType( const VehicleType &vehicleType, const QString &overlayIcon ) {
    KIcon icon;

    switch ( vehicleType )
    {
	case Tram:
	    icon = KIcon( "vehicle_type_tram" );
	    break;
	case Bus:
	    icon = KIcon( "vehicle_type_bus" );
	    break;
	case Subway:
	    icon = KIcon( "vehicle_type_subway" );
	    break;
	case TrainInterurban:
	    icon = KIcon( "vehicle_type_train_interurban" );
	    break;

	case TrainRegionalExpress:
	    icon = KIcon( "vehicle_type_train_regionalexpress" );
	    break;
	case TrainInterregio:
	    icon = KIcon( "vehicle_type_train_interregio" );
	    break;
	case TrainIntercityEurocity:
	    icon = KIcon( "vehicle_type_train_intercityeurocity" );
	    break;
	case TrainIntercityExpress:
	    icon = KIcon( "vehicle_type_train_intercityexpress" );
	    break;

	case TrainRegional: // Icon not done yet
	case Unknown:
	default:
	    icon = KIcon( "status_unknown" );
    }

    if ( !overlayIcon.isEmpty() )
	icon = makeOverlayIcon( icon, overlayIcon );

    return icon;
}

KIcon Global::iconFromVehicleTypeList ( const QList< VehicleType >& vehicleTypes ) {
    QPixmap pixmap = QPixmap( 32, 32 );
    pixmap.fill( Qt::transparent );
    QPainter p(&pixmap);

    int rows = qCeil((float)vehicleTypes.count() / 2.0f); // Two vehicle types per row
    int yOffset = rows <= 1 ? 0 : 16 / (rows - 1);
    int x, y, i = 0;
    if ( rows == 1 )
	y = (32 - 16) / 2;
    else
	y = 0;
    foreach( VehicleType vehicleType, vehicleTypes ) {
	if ( i % 2 == 0 ) { // icon on the left
	    if ( i == vehicleTypes.count() - 1 ) // align last vehicle type to the center
		x = (32 - 16) / 2;
	    else
		x = 0;
	}
	else // icon on the right
	    x = 16;

	QPixmap pixmapVehicleType = iconFromVehicleType( vehicleType ).pixmap( 16 );
	p.drawPixmap( x, y, pixmapVehicleType );

	if ( i % 2 != 0 )
	    y += yOffset;
	++i;
    }
    p.end();

    KIcon resultIcon = KIcon();
    resultIcon.addPixmap(pixmap, QIcon::Normal);
    return resultIcon;
}

// Gets the name of the given type of vehicle
QString Global::vehicleTypeToString( const VehicleType &vehicleType, bool plural ) {
    switch ( vehicleType )
    {
	case Tram:
	    return plural ? i18n("trams") :i18n ("tram");
	case Bus:
	    return plural ? i18n("buses") : i18n("bus");
	case Subway:
	    return plural ? i18n("subways") : i18n("subway");
	case TrainInterurban:
	    return plural ? i18n("interurban trains") : i18n("interurban train");

	case TrainRegional:
	    return plural ? i18n("regional trains") : i18n("regional train");
	case TrainRegionalExpress:
	    return plural ? i18n("regional express trains") : i18n("regional express");
	case TrainInterregio:
	    return plural ? i18n("interregional trains") : i18n("interregional train" );
	case TrainIntercityEurocity:
	    return plural ? i18n("intercity / eurocity trains") : i18n("intercity / eurocity");
	case TrainIntercityExpress:
	    return plural ? i18n("intercity express trains") : i18n("intercity express");

	case Unknown:
	default:
	    return i18nc("Unknown type of vehicle", "Unknown" );
    }
}

QString Global::durationString ( int seconds ) {
    int minutes = (seconds / 60) % 60;
    int hours = seconds / 3600;

    if (hours > 0) {
	if (minutes > 0)
	    return i18nc("h:mm", "%1:%2 hours", hours, QString("%1").arg(minutes, 2, 10, QLatin1Char('0')));
	else
	    return i18np("%1 hour", "%1 hours", hours);
    }
    else if (minutes > 0)
	return i18np("%1 minute", "%1 minutes", minutes);
    else
	return i18n("now");
}
