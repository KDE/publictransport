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

#include "departurepainter.h"
#include "departuremodel.h"
#include <publictransporthelper/departureinfo.h>
#include <publictransporthelper/global.h>

#include <Plasma/Theme>
#include <Plasma/Svg>
#include <Plasma/PaintUtils>
#include <qmath.h>

void DeparturePainter::paintVehicle( QPainter* painter, VehicleType vehicle,
        const QRectF& rect, const QString& transportLine )
{

    // Draw transport line string onto the vehicle type svg
    // only if activated in the settings and a supported vehicle type
    // (currently only local public transport)
    const bool m_drawTransportLine = true; // TODO Make configurable?
    bool drawTransportLine = m_drawTransportLine && !transportLine.isEmpty()
            && Timetable::Global::generalVehicleType(vehicle) == LocalPublicTransport;

    QString vehicleKey;
    switch ( vehicle ) {
        case Tram: vehicleKey = "tram"; break;
        case Bus: vehicleKey = "bus"; break;
        case TrolleyBus: vehicleKey = "trolleybus"; break;
        case Subway: vehicleKey = "subway"; break;
        case Metro: vehicleKey = "metro"; break;
        case InterurbanTrain: vehicleKey = "interurbantrain"; break;
        case RegionalTrain: vehicleKey = "regionaltrain"; break;
        case RegionalExpressTrain: vehicleKey = "regionalexpresstrain"; break;
        case InterregionalTrain: vehicleKey = "interregionaltrain"; break;
        case IntercityTrain: vehicleKey = "intercitytrain"; break;
        case HighSpeedTrain: vehicleKey = "highspeedtrain"; break;
        case Feet: vehicleKey = "feet"; break;
        case Ship: vehicleKey = "ship"; break;
        case Plane: vehicleKey = "plane"; break;
        default:
            kDebug() << "Unknown vehicle type" << vehicle;
            return; // TODO: draw a simple circle or something.. or an unknown vehicle type icon
    }
    if ( drawTransportLine ) {
        vehicleKey.append( "_empty" );
    }
    if ( !m_svg->hasElement(vehicleKey) ) {
        kDebug() << "SVG element" << vehicleKey << "not found";
        return;
    }

    int shadowWidth = 4;
    m_svg->resize( rect.width() - 2 * shadowWidth, rect.height() - 2 * shadowWidth );

    QPixmap pixmap( (int)rect.width(), (int)rect.height() );
    pixmap.fill( Qt::transparent );
    QPainter p( &pixmap );
    m_svg->paint( &p, shadowWidth, shadowWidth, vehicleKey );

    // Draw transport line string (only for local public transport)
    if ( drawTransportLine ) {
        QString text = transportLine;
        text.replace(' ', QString());

        QFont f = Plasma::Theme::defaultTheme()->font( Plasma::Theme::DefaultFont ); // TODO //font();
        f.setBold( true );
        if ( text.length() > 2 ) {
            f.setPixelSize( qMax(8, qCeil(1.2 * rect.width() / text.length())) );
        } else {
            f.setPixelSize( rect.width() * 0.55 );
        }
        p.setFont( f );
        p.setPen( Qt::white );

        QRect textRect( shadowWidth, shadowWidth, rect.width() - 2 * shadowWidth,
                        rect.height() - 2 * shadowWidth );
        p.drawText( textRect, text, QTextOption(Qt::AlignCenter) );
    }

    QImage shadow = pixmap.toImage();
    Plasma::PaintUtils::shadowBlur( shadow, shadowWidth - 1, Qt::black );
    painter->drawImage( rect.topLeft() + QPoint(1, 2), shadow );
    painter->drawPixmap( rect.topLeft(), pixmap );
}

QPixmap DeparturePainter::createDeparturesPixmap(const QList< DepartureItem* >& departures)
{

    QPixmap pixmap( 128, 128 );
    pixmap.fill( Qt::transparent );
    QPainter p( &pixmap );
    QRectF vehicleRect( 0, 0, pixmap.width(), pixmap.height() );

    // Calculate values for arranging vehicle type icons
    const int vehiclesPerRow = qCeil( qSqrt(departures.count()) );
    const int rows = qCeil( (qreal)departures.count() / (qreal)vehiclesPerRow );
    const qreal vehicleSize = vehiclesPerRow == 1 ? vehicleRect.width()
            : vehicleRect.width() / ( 0.8 * vehiclesPerRow );
    const qreal vehicleOffsetX = vehiclesPerRow == 1 ? 0.0
            : (vehicleRect.width() - vehicleSize) / (vehiclesPerRow - 1);
    qreal vehicleOffsetY = rows == 1 ? 0.0
            : (vehicleRect.height() - vehicleSize) / (rows - 1);
    int vehiclesInCurrentRow = 0;
    QDateTime time;
    int i = 0;
    qreal x = 0.0;
    qreal y = 0.0;
    if ( rows < vehiclesPerRow && rows > 1 ) {
//      vehicleOffsetY -= y / departures.count();
        y = (vehicleRect.height() - vehicleSize + (rows - 1) * vehicleOffsetY) / 2.0;
    }
    foreach ( const DepartureItem *item, departures ) {
        if ( !item ) {
            continue;
        }
        if ( vehiclesInCurrentRow == vehiclesPerRow ) {
            vehiclesInCurrentRow = 0;
            if ( departures.count() - i < vehiclesPerRow ) {
                x = vehicleOffsetX / 2.0;
            } else {
                x = 0;
            }
            y += vehicleOffsetY;
        }

        const DepartureInfo *data = item->departureInfo();
        paintVehicle( &p, data->vehicleType(),
                      QRectF(x, y, vehicleSize, vehicleSize), data->lineString() );

        // Move to next vehicle type svg position
//      vehicleRect.translate( translation, translation );

        if ( !time.isValid() ) {
            time = item->departureInfo()->predictedDeparture();
        }

        ++vehiclesInCurrentRow;
        x += vehicleOffsetX;
        ++i;
    }

    int minsToDeparture = qCeil( QDateTime::currentDateTime().secsTo(time) / 60.0 );
    QString text;
    if ( minsToDeparture < -1 ) {
        text.append( i18nc("Indicating the departure time of an already left vehicle", "left") );
    } else if ( minsToDeparture < 0 ) {
        text.append( i18nc("Indicating the departure time of a currently leaving vehicle", "leaving") );
    } else if ( minsToDeparture == 0 ) {
        text.append( i18nc("Indicating the departure time of a vehicle, that will leave now", "now") );
    } else if ( minsToDeparture >= 60 * 24 ) {
        text.append( i18np("1 day", "%1 days", qRound(minsToDeparture / (6 * 24)) / 10.0) );
    } else if ( minsToDeparture >= 60 ) {
        text.append( i18np("1 hour", "%1 hours", qRound(minsToDeparture / 6) / 10.0) );
    } else {
        text.append( i18np("1 min.", "%1 min.", minsToDeparture) );
    }

    QFont font = Plasma::Theme::defaultTheme()->font( Plasma::Theme::DefaultFont );
    font.setPixelSize( pixmap.width() / 4 );
    font.setBold( true );
    p.setFont( font );
    QFontMetrics fm( font );
    int textWidth = fm.width( text );
    QRectF textRect( 0, 0, pixmap.width(), pixmap.height() );
    QRectF haloRect( textRect.left() + (textRect.width() - textWidth) / 2,
                    textRect.bottom() - fm.height(), textWidth, fm.height() );
    haloRect = haloRect.intersected( textRect ).adjusted( 3, 3, -3, -3 );
    QTextOption option(Qt::AlignHCenter | Qt::AlignBottom);
    option.setWrapMode( QTextOption::NoWrap );

    Plasma::PaintUtils::drawHalo( &p, haloRect );
    p.drawText( textRect, fm.elidedText(text, Qt::ElideRight, pixmap.width()), option );

    p.end();
    return pixmap;
}

QPixmap DeparturePainter::createAlarmPixmap( DepartureItem* departure )
{
    QPixmap pixmap = createDeparturesPixmap( QList<DepartureItem*>() << departure );
    int iconSize = pixmap.width() / 2;
    QPixmap pixmapAlarmIcon = KIcon( "task-reminder" ).pixmap( iconSize );
    QPainter p( &pixmap );
    // Draw alarm icon in the top-right corner.
    Plasma::PaintUtils::drawHalo( &p, QRectF(pixmap.width() - iconSize * 0.85 - 1, 1, iconSize * 0.7, iconSize) );
    p.drawPixmap( pixmap.width() - iconSize - 1, 1, pixmapAlarmIcon );
    p.end();

    return pixmap;
}

QPixmap DeparturePainter::createPopupIcon( int startDepartureIndex,
        int endDepartureIndex, qreal departureIndex, DepartureModel* model,
        const QMap< QDateTime, QList< DepartureItem* > >& departureGroups )
{
    QPixmap pixmap;
    if ( qFuzzyCompare((qreal)qFloor(departureIndex), departureIndex) ) {
        // If departureIndex is an integer draw without transition
        departureIndex = qBound( model->hasAlarms() ? -1 : 0,
                qFloor(departureIndex), departureGroups.count() - 1 );

        if ( departureIndex < 0 ) {
            pixmap = createAlarmPixmap( model->nextAlarmDeparture() );
        } else {
            QDateTime time = departureGroups.keys()[ departureIndex ];
            pixmap = createDeparturesPixmap( departureGroups[time] );
        }
    } else {
        // Draw transition
        startDepartureIndex = qBound( model->hasAlarms() ? -1 : 0,
                startDepartureIndex, departureGroups.count() - 1 );
        endDepartureIndex = qBound( model->hasAlarms() ? -1 : 0,
                endDepartureIndex, departureGroups.count() - 1 );
        QPixmap startPixmap = startDepartureIndex < 0
                ? createAlarmPixmap(model->nextAlarmDeparture())
                : createDeparturesPixmap(
                departureGroups[departureGroups.keys()[startDepartureIndex]] );
        QPixmap endPixmap = endDepartureIndex < 0
                ? createAlarmPixmap(model->nextAlarmDeparture())
                : createDeparturesPixmap(
                departureGroups[departureGroups.keys()[endDepartureIndex]] );

        pixmap = QPixmap( 128, 128 );
        pixmap.fill( Qt::transparent );
        QPainter p( &pixmap );
        p.setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );

        qreal transition, startSize, endSize;
        if ( endDepartureIndex > startDepartureIndex  ) {
            // Move forward to next departure
            transition = qBound( 0.0, (departureIndex - startDepartureIndex)
                    / (endDepartureIndex - startDepartureIndex), 1.0 );
        } else {
            // Mave backward to previous departure
            transition = 1.0 - qBound( 0.0, (startDepartureIndex - departureIndex)
                    / (startDepartureIndex - endDepartureIndex), 1.0 );
            qSwap( startPixmap, endPixmap );
        }
        startSize = (1.0 + 0.25 * transition) * pixmap.width();
        endSize = transition * pixmap.width();

        p.drawPixmap( (pixmap.width() - endSize) / 2 + pixmap.width() * (1.0 - transition) / 2.0,
                    (pixmap.height() - endSize) / 2,
                    endSize, endSize, endPixmap );

        QPixmap startTransitionPixmap( pixmap.size() );
        startTransitionPixmap.fill( Qt::transparent );
        QPainter p2( &startTransitionPixmap );
        p2.drawPixmap( 0, 0, pixmap.width(), pixmap.height(), startPixmap );

        // Make startTransitionPixmap more transparent (for fading)
        p2.setCompositionMode( QPainter::CompositionMode_DestinationIn );
        p2.fillRect( startTransitionPixmap.rect(), QColor(0, 0, 0, 255 * (1.0 - transition * transition)) );
        p2.end();

        p.setTransform( QTransform().rotate(transition * 90, Qt::YAxis) );
        p.drawPixmap( (pixmap.width() - startSize) / 2 - pixmap.width() * transition / 5.0,
                    (pixmap.height() - startSize) / 2,
                    startSize, startSize, startTransitionPixmap );
        p.end();
    }

    return pixmap;
}
