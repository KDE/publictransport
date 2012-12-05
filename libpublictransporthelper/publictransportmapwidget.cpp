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
#include "publictransportmapwidget.h"

// Own includes
#include "publictransportlayer.h"
#include "stopsettings.h"

// KDE includes
#include <KColorScheme>
#include <KLineEdit>
#include <KCompletionBox>
#include <KDebug>

// Marble includes
#include <marble/ViewportParams.h>

// Qt includes
#include <QPaintEvent>
#include <QPainter>
#include <qmath.h>

namespace PublicTransport {

/** @brief Private class of PublicTransportMapWidget. */
class PublicTransportMapWidgetPrivate
{
    Q_DECLARE_PUBLIC( PublicTransportMapWidget )

public:
    PublicTransportMapWidgetPrivate( PublicTransportMapWidget *q,
                                     PublicTransportMapWidget::Flags flags,
                                     const QString &serviceProvider, PublicTransportLayer *layer )
            : serviceProvider(serviceProvider), publicTransportLayer(layer), flags(flags), q_ptr(q)
    {
        if ( !layer ) {
            publicTransportLayer = new PublicTransportLayer( q, serviceProvider,
                    flags.testFlag(PublicTransportMapWidget::AutoLoadStopsForMapRegion)
                    ? PublicTransportLayer::AutoLoadStopsForMapRegion
                    : PublicTransportLayer::NoFlags, q );
        }
        q->connect( publicTransportLayer, SIGNAL(stopSelected(Stop)), q, SIGNAL(stopSelected(Stop)) );
        q->connect( publicTransportLayer, SIGNAL(stopSelected(Stop)), q, SLOT(update()) );
        q->connect( publicTransportLayer, SIGNAL(stopHovered(Stop)), q, SIGNAL(stopHovered(Stop)) );
    };

    void setStops( const StopList &stops,
                   const QString &selectStopName, const QStringList &activeStopNames )
    {
        Q_Q( PublicTransportMapWidget );
        GeoDataLatLonBox centerBox;
        StopList activeStops, otherStops;
        int count = 0;
        Stop mainStop, selectedStop;

        foreach ( const Stop &stop, stops ) {
            if ( !stop.hasValidCoordinates ) {
                continue;
            }
            const bool selectStop = stop.name == selectStopName;
            if ( selectStop || activeStopNames.contains(stop.name) ) {
                ++count;
                activeStops << stop;

                if ( mainStop.name.isEmpty() &&
                     (selectStop || stop.name == activeStopNames.first()) )
                {
                    mainStop = stop;
                }
                if ( selectStop ) {
                    selectedStop = stop;
                }
            } else {
                otherStops << stop;
            }
        }
        if ( activeStops.isEmpty() && otherStops.isEmpty() ) {
            kDebug() << "No stops found";
            return;
        }

        // When stops were already added to the list, the map was already centered on those stops.
        // The new centering can then be animated.
        const bool animate = !publicTransportLayer->stops().isEmpty() &&
                KGlobalSettings::graphicEffectsLevel() == KGlobalSettings::ComplexAnimationEffects;

        publicTransportLayer->setStops( activeStops, otherStops );
        publicTransportLayer->setSelectedStop( selectedStop );

        // Center on found stops
        centerBox = PublicTransportLayer::boundingBoxFromStops(
                activeStops.isEmpty() ? otherStops : activeStops );
        q->centerOn( centerBox, animate );
        q->update();
    };

    QString serviceProvider;
    QString city;
    QString sourceName;
    QHash< Stop, bool > stops;
    PublicTransportLayer *publicTransportLayer;
    PublicTransportMapWidget::Flags flags;

protected:
    PublicTransportMapWidget* const q_ptr;
};

PublicTransportMapWidget::PublicTransportMapWidget( const QString &serviceProvider,
                                                    Flags flags, QWidget *parent,
                                                    PublicTransportLayer *layer )
        : MarbleWidget(parent),
          d_ptr(new PublicTransportMapWidgetPrivate(this, flags, serviceProvider, layer))
{
    Q_D( PublicTransportMapWidget );

    setMapThemeId("earth/openstreetmap/openstreetmap.dgml");

    // Disable overlays
    setShowOverviewMap( false );
    setShowScaleBar( false );
    setShowCompass( false );
    setShowCrosshairs( false );
    setShowGrid( false );

    // Add public transport layer
    addLayer( d->publicTransportLayer );

    setMinimumSize( 175, 125 );
    setAnimationsEnabled(
            KGlobalSettings::graphicEffectsLevel() == KGlobalSettings::ComplexAnimationEffects );
    setMapQualityForViewContext(
            KGlobalSettings::graphicEffectsLevel() == KGlobalSettings::NoEffects
            ? NormalQuality : HighQuality, Still );
    setMapQualityForViewContext(
            KGlobalSettings::graphicEffectsLevel() == KGlobalSettings::ComplexAnimationEffects
            ? HighQuality : NormalQuality, Animation );

    connect( this, SIGNAL(mouseClickGeoPosition(qreal,qreal,GeoDataCoordinates::Unit)),
             this, SLOT(slotMouseClickGeoPosition(qreal,qreal,GeoDataCoordinates::Unit)) );
}

PublicTransportMapWidget::~PublicTransportMapWidget()
{
    delete d_ptr;
}

void PublicTransportMapWidget::setServiceProvider( const QString &serviceProvider )
{
    Q_D( PublicTransportMapWidget );
    d->serviceProvider = serviceProvider;
    d->publicTransportLayer->setServiceProvider( serviceProvider );
}

void PublicTransportMapWidget::setStops( const StopList &stops, const QString &selectStopName,
                                         const QStringList &activeStopNames )
{
    Q_D( PublicTransportMapWidget );
    d->setStops( stops, selectStopName, activeStopNames );
}

PublicTransportLayer *PublicTransportMapWidget::publicTransportLayer() const
{
    Q_D( const PublicTransportMapWidget );
    return d->publicTransportLayer;
}

Stop PublicTransportMapWidget::stopFromPosition( int x, int y, int maxDistance,
                                                 StopOption option ) const
{
    Q_D( const PublicTransportMapWidget );
    qreal minDistance = maxDistance; // in pixels
    Stop foundStop;
    foreach ( const Stop &stop, d->publicTransportLayer->stops() ) {
        // Test if the current stop is currently shown in the map
        GeoDataCoordinates coords = PublicTransportLayer::coordsFromStop( stop );
        qreal stopX, stopY;
        if ( screenCoordinates(stop.longitude, stop.latitude, stopX, stopY) ) {
            // Calculate distance from the click position to the stop position
            const qreal distance = qSqrt( qPow(x - stopX, 2) + qPow(y - stopY, 2) );
            if ( distance < minDistance && (option == IncludeInvisibleStops ||
                                            d->publicTransportLayer->isStopVisible(stop)) )
            {
                minDistance = distance;
                foundStop = stop;
            }
        }
    }

    return foundStop;
}

void PublicTransportMapWidget::slotMouseClickGeoPosition( qreal lon, qreal lat,
                                                          GeoDataCoordinates::Unit unit )
{
    Q_D( PublicTransportMapWidget );
    qreal mouseX, mouseY;
    if ( !viewport()->screenCoordinates(GeoDataCoordinates(lon, lat, 0, unit), mouseX, mouseY) ) {
        return;
    }

    const Stop clickedStop = stopFromPosition( mouseX, mouseY );
    if ( clickedStop.isValid() ) {
        d->publicTransportLayer->setSelectedStop( clickedStop );
        update();
        emit stopClicked( clickedStop );
    }
}

void PublicTransportMapWidget::mouseMoveEvent( QMouseEvent *event )
{
    Q_D( PublicTransportMapWidget );
    QWidget::mouseMoveEvent( event );

    const Stop hoveredStop = stopFromPosition( event->x(), event->y() );
    if ( d->publicTransportLayer->hoveredStop() != hoveredStop ) {
        d->publicTransportLayer->setHoveredStop( hoveredStop );
        update();
    }
}

void PublicTransportMapWidget::paintEvent( QPaintEvent *event )
{
    Marble::MarbleWidget::paintEvent( event );

    // Draw a border around the widget
    QPainter *painter = new QPainter( this );
    painter->setPen( KColorScheme(QPalette::Active, KColorScheme::Window).foreground().color() );
    painter->drawRect( rect().adjusted(0, 0, -1, -1) );
    delete painter;
}

} // namespace PublicTransport
