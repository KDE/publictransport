/*
*   Copyright 2010 Friedrich Pülz <fpuelz@gmx.de>
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

#include <KDebug>
#include <qmath.h>
#include <QPainter>
#include <KIconEffect>
#include <KIconLoader>
#include <KGlobal>
#include <KLocale>
#include <Plasma/Theme>
#include <KColorUtils>
#include <KStandardDirs>

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

		QString flag( KStandardDirs::locate("locale", QString::fromLatin1("l10n/%1/flag.png")
				.arg(sIcon)) ); 
// 		icon.addFile( flag );
		
// 		QPixmap pixmapFlag = KIcon( sIcon ).pixmap( 16 );
		QPixmap pixmapFlag( flag );
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

KIcon Global::makeOverlayIcon( const KIcon &icon, const KIcon &overlayIcon,
							   const QSize &overlaySize, int iconExtend )
{
	QPixmap pixmap = icon.pixmap( iconExtend ), pixmapOverlay = overlayIcon.pixmap( overlaySize );
	QPainter p( &pixmap );
	p.drawPixmap( QPoint( iconExtend - overlaySize.width(), iconExtend - overlaySize.height() ), pixmapOverlay );
	p.end();
	KIcon resultIcon = KIcon();
	resultIcon.addPixmap( pixmap, QIcon::Normal );

	KIconEffect iconEffect;
	pixmap = iconEffect.apply( pixmap, KIconLoader::Small, KIconLoader::ActiveState );
	resultIcon.addPixmap( pixmap, QIcon::Selected );
	resultIcon.addPixmap( pixmap, QIcon::Active );

	return resultIcon;
}

KIcon Global::makeOverlayIcon( const KIcon &icon, const QString &overlayIconName,
							   const QSize &overlaySize, int iconExtend )
{
	return makeOverlayIcon( icon, KIcon( overlayIconName ), overlaySize, iconExtend );
}

KIcon Global::makeOverlayIcon( const KIcon& icon, const QList<KIcon> &overlayIconsBottom,
							   const QSize& overlaySize, int iconExtend )
{
	Q_ASSERT( !icon.isNull() );

	QPixmap pixmap = icon.pixmap( iconExtend );
	if ( pixmap.isNull() ) {
		kDebug() << "pixmap is Null";
		return icon;
	}

	QPainter p( &pixmap );
	int x = 0, xStep = iconExtend / overlayIconsBottom.count();
	foreach( const KIcon &overlayIcon, overlayIconsBottom ) {
		p.drawPixmap( QPoint( x, iconExtend - overlaySize.height() ), overlayIcon.pixmap( overlaySize ) );
		x += xStep;
	}
	p.end();
	KIcon resultIcon = KIcon();
	resultIcon.addPixmap( pixmap, QIcon::Normal );

	KIconEffect iconEffect;
	pixmap = iconEffect.apply( pixmap, KIconLoader::Small, KIconLoader::ActiveState );
	resultIcon.addPixmap( pixmap, QIcon::Selected );
	resultIcon.addPixmap( pixmap, QIcon::Active );

	return resultIcon;
}

KIcon Global::vehicleTypeToIcon( const VehicleType &vehicleType, const QString &overlayIcon )
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

	case Unknown:
	default:
		icon = KIcon( "status_unknown" );
	}

	if ( !overlayIcon.isEmpty() ) {
		icon = makeOverlayIcon( icon, overlayIcon );
	}

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

QString Global::durationString( int seconds )
{
	int minutes = ( seconds / 60 ) % 60;
	int hours = seconds / 3600;

	if ( hours > 0 ) {
		if ( minutes > 0 ) {
			return i18nc( "h:mm", "%1:%2 hours", hours, QString( "%1" ).arg( minutes, 2, 10, QLatin1Char( '0' ) ) );
		} else {
			return i18np( "%1 hour", "%1 hours", hours );
		}
	} else if ( minutes > 0 ) {
		return i18np( "%1 minute", "%1 minutes", minutes );
	} else {
		return i18nc( "@info/plain Used as duration string if the duration is less than a minute", "now" );
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
