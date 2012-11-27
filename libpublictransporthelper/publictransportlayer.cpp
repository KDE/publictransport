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
#include "publictransportlayer.h"

// KDE includes
#include <KIcon>
#include <KDebug>
#include <KColorScheme>
#include <KGlobalSettings>
#include <Plasma/DataEngineManager>

// Marble includes
#include <marble/MarbleWidget.h>
#include <marble/MarbleModel.h>
#include <marble/ViewportParams.h>
#include <marble/GeoPainter.h>
#include <marble/GeoDataLatLonAltBox.h>

// Qt includes
#include <qmath.h>
#include <QTimer>

/** @brief Namespace for the publictransport helper library. */
namespace PublicTransport {

/** @brief Flags for stops. */
enum StopFlag {
    NoStopFlags  = 0x00, /**< No flags, the stop is not active and will be drawn with
                            * less opacity and without annotation. */
    ActiveStop   = 0x01, /**< The stop is active and will be drawn with full opacity.
                            * Some active stops get drawn with an annotation. */
    HoveredStop  = 0x02, /**< The stop is hovered, only one stop can be hovered at a time. */
    SelectedStop = 0x04, /**< The stop is selected, only one stop can be selected at a time. */
    InternalStop = 0x08  /**< The stop was requested internally by PublicTransportLayer. */
};
Q_DECLARE_FLAGS( StopFlags, StopFlag );

/** @brief Stores additional data for a stop. */
struct StopData {
    /** @brief Create a stop data object. */
    StopData( const GeoDataCoordinates &coords = GeoDataCoordinates(),
              StopFlags flags = NoStopFlags )
            : coords(coords), flags(flags) {};

    /** @brief Create a stop data object, get coordinates for @p stop. */
    StopData( const Stop &stop, StopFlags flags = NoStopFlags )
            : coords(PublicTransportLayer::coordsFromStop(stop)), flags(flags) {};

    bool isActive() const { return flags.testFlag(ActiveStop); };
    bool isHovered() const { return flags.testFlag(HoveredStop); };
    bool isSelected() const { return flags.testFlag(SelectedStop); };
    bool isInternal() const { return flags.testFlag(InternalStop); };

    GeoDataCoordinates coords; /**< Coordinates of the stop. */
    StopFlags flags; /**< Flags of the stop. */
};

/** @brief Stores values for drawing an annotation. */
struct AnnotationData {
    /** @brief Create an invalid annotation data object. */
    AnnotationData() {};

    /** @brief Create an annotation data object. */
    AnnotationData( const Stop &stop, const StopData &stopData,
                    const QSize &mapSize, const QFontMetrics &metrics, qreal x, qreal y )
            : stop(stop), stopData(stopData)
    {
        // If the stop is too near the top edge of the map widget,
        // draw the annotation below the stop, otherwise above
        drawAbove = y > metrics.height() + 25;

        // Draw the stop name in an annotation, elide it to fit into maximally 150 pixels.
        // Always try to draw the full annotation inside the viewport.
        // The tip of the annotation bubble is horizontally located at 1/3 or 2/3 of the width.
        const int width = qMin( 150, metrics.width(stop.name) + 10 );
        text = metrics.elidedText( stop.name, Qt::ElideRight, width - 7 );
        const int third = qCeil( width / 3.0 );
        xOffset = qBound( -qFloor(x) + (x < third ? 2 * third + 3 : third), -5,
                          mapSize.width() - width - qFloor(x) + third - 5 );
        yOffset = drawAbove ? -15 : 15;

        x = qBound( 0, int(x) - qCeil(width / 3) - 3, mapSize.width() - width );
        size = QSizeF( width, metrics.height() + 5 );
        rect = QRectF( QPointF(x, y + (drawAbove ? -33 : (15 - 10))),
                       QSizeF(width, metrics.height() + 5 + 10) );
    };

    /** @brief Change the position of the annotation and recalculate affected values. */
    void setDrawAbove( bool _drawAbove )
    {
        // Update affected variables
        yOffset = _drawAbove ? -15 : 15;
        rect = QRectF( QPointF(rect.x(),
                               rect.y() - (drawAbove ? -33 : (15-10)) + (_drawAbove ? -33 : (15-10))),
                       rect.size() );
        drawAbove = _drawAbove;
    };

    /** @brief Compare this annotation with @p other, based on the equality of the stop. */
    bool operator ==( const AnnotationData &other )
    {
        return stop == other.stop;
    };

    /** @brief Whether or not this annotation is valid. */
    bool isValid() const { return stop.isValid(); };

    /** @brief Whether or not the offset is very big and would not look nice anymore. */
    bool hasBigOffset() const { return qAbs(xOffset - 15) > size.width() * 0.4; };

    Stop stop; /**< The stop associated with this annotation. */
    StopData stopData; /**< Additional data for the stop. */

    bool drawAbove; /**< Whether the annotation gets drawn above or below the stop. */
    QSizeF size; /**< The size of the annotation bubble. */
    QRectF rect; /**< A rectangle to test for intersections between annotations, bigger than size. */
    QString text; /**< The text shown in the annotation, ie. the elided stop name. */
    int xOffset, yOffset; /**< Offset of the annotation bubble. */
};

/** @brief Private class of PublicTransportLayer. */
class PublicTransportLayerPrivate {
    Q_DECLARE_PUBLIC( PublicTransportLayer )

public:
    /** @brief Create the private object. */
    PublicTransportLayerPrivate( PublicTransportLayer *q, MarbleWidget *mapWidget,
                                 const QString &serviceProvider, PublicTransportLayer::Flags flags )
            : q_ptr(q), mapWidget(mapWidget), flags(flags), serviceProvider(serviceProvider),
              loadTimer(0) {};

    /** @brief Add @p stops with the ActiveStop flag and @p inactiveStops without any flags. */
    void addStops( const QList< Stop > &stops, const QList< Stop > &inactiveStops )
    {
        foreach ( const Stop &stop, stops ) {
            addStop( stop, ActiveStop );
        }
        foreach ( const Stop &stop, inactiveStops ) {
            addStop( stop, NoStopFlags );
        }
        updateAnnotationData();
    };

    /** @brief Add @p stop with @p flags. */
    bool addStop( const Stop &stop, StopFlags flags )
    {
        // Test if the stop is valid and has valid coordinates before adding it
        if ( !stop.hasValidCoordinates || !stop.isValid() ) {
            kWarning() << "Invalid stop/stop coordinates";
            return false;
        }

        // If the stop was already added, combine the flags
        QMap< Stop, StopData >::Iterator it = stops.find( stop );
        if ( it == stops.end() ) {
            stops.insert( stop, StopData(stop, flags) );
        } else {
            stops.insert( stop, StopData(stop, it->flags | flags) );
        }
        return true;
    };

    /** @brief Find a prepared annotation that overlaps @p annotation. */
    int findOverlappingAnnotation( const AnnotationData &annotation )
    {
        for ( int i = 0; i < annotations.count(); ++i ) {
            const AnnotationData currentAnnotation = annotations[ i ];
            if ( annotation.rect.intersects(currentAnnotation.rect) ) {
                // Found an overlapping annotation at index i
                return i;
            }
        }

        // No overlapping annotation found
        return -1;
    };

    /** @brief Update prepared annotation data objects. */
    void updateAnnotationData()
    {
        // Clear old list
        annotations.clear();

        // Use small font for stop name annotations
        QFont font = KGlobalSettings::smallestReadableFont();
        const QFontMetrics metrics( font );

        // Go through all stops and filter active stops that are in the viewport of the map widget.
        // Also check for intersections with other annotations.
        QStringList annotatedStops;
        for ( QMap<Stop, StopData>::ConstIterator it = stops.constBegin();
              it != stops.constEnd(); ++it )
        {
            // Only prepare annotations for stops for which doPrepareAnnotationForStop()
            // returns true, eg. active stops
            if ( !doPrepareAnnotationForStop(*it) ) {
                continue;
            }

            // Test if the stop is visible in the map widget
            qreal x, y;
            GeoDataCoordinates::Unit unit = GeoDataCoordinates::Degree;
            if ( mapWidget->screenCoordinates(it->coords.longitude(unit),
                                              it->coords.latitude(unit), x, y) )
            {
                // Create annotation data for the current stop
                AnnotationData annotation( it.key(), it.value(), mapWidget->size(), metrics, x, y );
                if ( it->isHovered() || it->isSelected() ) {
                    // Always add prepared annotation data for hovered/selected stops
                    annotations << annotation;
                    annotatedStops << it.key().name;
                } else if ( !annotation.hasBigOffset() && !annotatedStops.contains(it.key().name) ) {
                    // Test if another annotation gets drawn too near
                    int foundIndex = findOverlappingAnnotation( annotation );
                    if ( foundIndex != -1 ) {
                        // Found an overlapping annotation, try to draw it on the other side
                        annotation.setDrawAbove( !annotation.drawAbove );
                        foundIndex = findOverlappingAnnotation( annotation );
                    }
                    if ( foundIndex == -1 ) {
                        // Found a free position for the annotation,
                        // add it to the list of annotations to be drawn
                        annotations << annotation;

                        // Store name to only draw one annotation for a stop with the same name
                        // but slightly different position
                        annotatedStops << it.key().name;
                    }
                }
            }
        }

        // Request an update in the connected map widget
        mapWidget->update();
    };

    /** @brief Draw @p pixmap at @p coords using @p painter. */
    inline void drawStop( GeoPainter *painter, const GeoDataCoordinates &coords,
                          const QPixmap &pixmap )
    {
        painter->drawPixmap( coords, pixmap );
    };

    /** @brief Draw @p annotation using @p painter. */
    void drawAnnotation( GeoPainter *painter, const AnnotationData &annotation )
    {
        // Use active text color for the selected stop if any
        painter->setPen( KColorScheme(QPalette::Active).foreground(
                annotation.stop == selectedStop ? KColorScheme::ActiveText
                                                : KColorScheme::NormalText).color() );
        painter->drawAnnotation( annotation.stopData.coords, annotation.text, annotation.size,
                                 annotation.xOffset, annotation.yOffset, 3, 3 );
    };

    /** @brief The visible map region changed. */
    void visibleLatLonAltBoxChanged( const GeoDataLatLonAltBox &latLonAltBox )
    {
        if ( !flags.testFlag(PublicTransportLayer::AutoLoadStopsForMapRegion) ||
             !showInternalStops() ||
             (!viewBox.isEmpty() &&
             (latLonAltBox.width() >= 0.7 * viewBox.width() &&
             latLonAltBox.width() <= 1.3 * viewBox.width()) &&
             (latLonAltBox.height() >= 0.7 * viewBox.height() &&
             latLonAltBox.height() <= 1.3 * viewBox.height()) &&
             qAbs(latLonAltBox.center().longitude() - viewBox.center().longitude()) <=
                    viewBox.width() &&
             qAbs(latLonAltBox.center().latitude() - viewBox.center().latitude()) <=
                    viewBox.height()) )
        {
            // Zoom/center did not change enough for another request or auto stop loading is disabled
            // or too big region in the view (too many stops)
            return;
        }

        // Store view box and start a timer to request the stops
        viewBox = latLonAltBox;
        startStopsByGeoPositionRequestLater();
    };

    /** @brief Request stops for the currently visible map region later. */
    void startStopsByGeoPositionRequestLater()
    {
        Q_Q( PublicTransportLayer );

        // Disconnect running requests,
        // most probably for a map region that was shortly visible while scrolling
        if ( !dataSource.isEmpty() ) {
            Plasma::DataEngine *engine =
                    Plasma::DataEngineManager::self()->engine( "publictransport" );
            engine->disconnectSource( dataSource, q );
            dataSource.clear();
        }

        if ( !loadTimer ) {
            // No running timer, create one
            loadTimer = new QTimer( q );
            loadTimer->setSingleShot( true );
            q->connect( loadTimer, SIGNAL(timeout()), q, SLOT(startStopsByGeoPositionRequest()) );

            // Start quickly with an interval of 50ms, if this function is called again within this
            // time span, the interval gets doubled until maximally 1 second
            loadTimer->start( 50 );
        } else {
            // A timer is already running, double it's interval until maximally 1 second
            // to not stress the engine too much with too many requests that get disconnected
            // before the data arrives, because the visible map region changed again quickly
            loadTimer->start( qMin(loadTimer->interval() * 2, 1000) );
        }
    };

    /** @brief Request stops for the currently visible map region now. */
    void startStopsByGeoPositionRequest()
    {
        Q_Q( PublicTransportLayer );

        delete loadTimer;
        loadTimer = 0;

        // Test if a service provider has been specified
        // either in the constructor or with setServiceProvider()
        if ( serviceProvider.isEmpty() ) {
            kWarning() << "No service provider specified to use for the AutoLoadStopsForMapRegion"
                          "feature in PublicTransportLayer.";
            return;
        }

        Plasma::DataEngine *engine = Plasma::DataEngineManager::self()->engine( "publictransport" );
        const Marble::GeoDataCoordinates::Unit unit = Marble::GeoDataCoordinates::Degree;

        // Arc-length in meters of the longest side of the view box is radius * angle.
        // The distance (ie. radius) in which to search for stops is doubled to not need to
        // reload stops on every map movement
        const qreal radius = mapWidget->model()->planetRadius(); // in meters
        const qreal angle = qMax( viewBox.width(), viewBox.height() ); // in rad
        const qreal distance = qBound( 500, qCeil(2 * radius * angle), 20000 );

        // Connect data source
        dataSource = QString("Stops %1|latitude=%2|longitude=%3|distance=%4|count=999")
                .arg(serviceProvider)
                .arg(viewBox.center().latitude(unit))
                .arg(viewBox.center().longitude(unit))
                .arg(distance);
        engine->connectSource( dataSource, q );
    };

    /**
     * @brief Clear non-internal stops.
     * @warning If the selected/hovered stops get erased, they are still stored, use the clear()
     *   method of the public class instead.
     **/
    void clear()
    {
        // Remove stops that are not internal
        QMap<Stop, StopData>::Iterator it = stops.begin();
        while ( it != stops.end() ) {
            if ( it->isInternal() ) {
                ++it;
            } else {
                it = stops.erase( it );
            }
        }
    };

    /** @brief Filter out stops that are active or inactive depending on the argument @p active. */
    QList< Stop > filteredStops( bool active ) const
    {
        QList< Stop > filteredStops;
        for ( QMap<Stop, StopData>::ConstIterator it = stops.constBegin();
              it != stops.constEnd(); ++it )
        {
            if ( active == it->flags.testFlag(ActiveStop) ) {
                filteredStops << it.key();
            }
        }
        return filteredStops;
    };

    /** @brief Whether or not internal stops should be shown. */
    inline bool showInternalStops() const { return mapWidget->zoom() > 2500; };

    /** @brief Whether or not an AnnotationData object should be prepared for @p stop. */
    inline bool doPrepareAnnotationForStop( const StopData &stopData ) const
    {
        return stopData.isActive() || stopData.isHovered() || stopData.isSelected() ||
               (stopData.isInternal() && showInternalStops());
    };

    /**
     * @brief Render the layer.
     * Implement rendering inline in the private class, the d-pointer would get accessed very
     * often otherwise. This function gets only called from PublicTransportLayer::render().
     **/
    inline bool render( GeoPainter *painter, ViewportParams *viewport )
    {
        painter->setRenderHints( QPainter::Antialiasing | QPainter::TextAntialiasing |
                                 QPainter::SmoothPixmapTransform );
        painter->setBrush( KColorScheme(QPalette::Active).background() );

        // Use small font for stop name annotations
        QFont font = KGlobalSettings::smallestReadableFont();
        painter->setFont( font );
        const QFontMetrics metrics( font );

        // Read the stop pixmap
        QPixmap stopPixmap = KIcon("public-transport-stop").pixmap(12);

        // Draw inactive and internal stop icons transparently (not active nor hovered)
        // Draw more transparently if zoomed farther away
        painter->setOpacity( qBound(0.0, 0.9 * (mapWidget->zoom() - 1500) / 1500 - 0.3, 0.7) );
        QList< GeoDataCoordinates > stopCoords;
        for ( QMap<Stop, StopData>::ConstIterator it = stops.constBegin();
              it != stops.constEnd(); ++it )
        {
            // Only draw inactive, not hovered/selected or internal stops with less transparency.
            // Do not draw internal stops when zooming far out.
            if ( it->isActive() || it->isHovered() || it->isSelected() ||
                 (it->isInternal() && !showInternalStops()) )
            {
                continue;
            }

            // Check if the stop is on the same pixel as an already drawn stop
            bool draw = true;
            foreach ( const GeoDataCoordinates &coords, stopCoords ) {
                if ( viewport->resolves(coords, it->coords) ) {
                    draw = false;
                    break;
                }
            }
            if ( !draw ) {
                continue;
            }

            // Draw the stop
            drawStop( painter, it->coords, stopPixmap );
            stopCoords << it->coords;
        }

        // Draw stop icons for hovered stops
        painter->setOpacity( 1 );
        for ( QMap<Stop, StopData>::ConstIterator it = stops.constBegin();
              it != stops.constEnd(); ++it )
        {
            // Draw a stop icon
            if ( it->isActive() ) {
                drawStop( painter, it->coords, stopPixmap );
            }
        }

        // Limit number of drawn annotations based on the size of the map widget.
        // If there are more annotations to draw than this limit, skip some annotations, ie. do not
        // draw every nth annotation (this should distribute the drawn annotations over the map,
        // because the annotation list is sorted by latitude, ie. filled in the sorted order from stops).
        const int maxAnnotations = qBound( 2, 7 * viewport->width() * viewport->height() / 80000, 10 );
        const int skip = qMax( 1, qFloor(annotations.count() / maxAnnotations) );
        int drawnAnnotations = 0;
        painter->setOpacity( 0.6 );
        AnnotationData selectedAnnotation, hoveredAnnotation;
        for ( int i = 0; i < annotations.count(); ++i ) {
            // Only one stop can be selected/hovered at a time,
            // store found annotations for them and draw other annotations
            const AnnotationData &annotation = annotations[ i ];
            if ( annotation.stopData.isSelected() || annotation.stopData.isHovered() ) {
                if ( annotation.stopData.isSelected() ) {
                    selectedAnnotation = annotation;
                }
                if ( annotation.stopData.isHovered() ) {
                    hoveredAnnotation = annotation;
                }
                continue;
            } else if ( i % skip != 0 ) {
                // Skip annotation
                continue;
            }

            // Draw the annotation
            drawAnnotation( painter, annotation );

            // Limit number of drawn annotations
            ++drawnAnnotations;
            if ( drawnAnnotations >= maxAnnotations ) {
                break;
            }
        }
        painter->setOpacity( 1 );

        // Draw selected stop
        if ( selectedStop.isValid() ) {
            drawStop( painter, stops[selectedStop].coords, stopPixmap );
            if ( selectedAnnotation.isValid() ) {
                drawAnnotation( painter, selectedAnnotation );
            }
        }

        // Draw annotations for hovered stop
        if ( hoveredStop.isValid() ) {
            drawStop( painter, stops[hoveredStop].coords, stopPixmap );
            if ( hoveredAnnotation.isValid() ) {
                drawAnnotation( painter, hoveredAnnotation );
            }
        }

        return true;
    };

    PublicTransportLayer *q_ptr; /**< q-pointer. */
    MarbleWidget *mapWidget; /**< The Associated MarbleWidget. */
    PublicTransportLayer::Flags flags; /**< Flags for this layer. */
    QString serviceProvider; /**< The ID of the service provider to be used for stop requests. */
    QMap< Stop, StopData > stops; /**< Maps all contained stops to associated StopData objects,
            * use QMap instead of QHash to automatically sort the stops for drawing. */
    Stop selectedStop; /**< The currently selected stop in stops. */
    Stop hoveredStop; /**< The currently hovered stop in stops. */
    QList< AnnotationData > annotations; /**< Prepared data for drawing annotations. */
    QTimer *loadTimer; /**< A timer to not start too many requests while scrolling. */
    GeoDataLatLonAltBox viewBox; /**< Stores the currently visible map region. */
    QString dataSource; /**< The name of the currently connected data source from the engine. */
};

PublicTransportLayer::PublicTransportLayer( MarbleWidget *mapWidget, const QString &serviceProvider,
                                            Flags flags, QObject *parent )
        : QObject(parent),
          d_ptr(new PublicTransportLayerPrivate(this, mapWidget, serviceProvider, flags))
{
    // Automatically update the map when new stops got loaded internally
    connect( this, SIGNAL(stopsForVisibleMapRegionLoaded()), mapWidget, SLOT(update()) );

    // Connect to the visibleLatLonAltBoxChanged() signal to know when to request new stops
    // for the current map region
    if ( flags.testFlag(AutoLoadStopsForMapRegion) ) {
        Plasma::DataEngineManager::self()->loadEngine( "publictransport" );
    }
    connect( mapWidget, SIGNAL(visibleLatLonAltBoxChanged(GeoDataLatLonAltBox)),
             this, SLOT(visibleLatLonAltBoxChanged(GeoDataLatLonAltBox)) );
}

PublicTransportLayer::~PublicTransportLayer()
{
    Q_D( PublicTransportLayer );
    if ( d->flags.testFlag(AutoLoadStopsForMapRegion) ) {
        Plasma::DataEngineManager::self()->unloadEngine( "publictransport" );
    }
    delete d_ptr;
}

GeoDataCoordinates PublicTransportLayer::coordsFromStop( const Stop &stop )
{
    if ( stop.hasValidCoordinates ) {
        return GeoDataCoordinates( stop.longitude, stop.latitude, 0,
                                   Marble::GeoDataCoordinates::Degree );
    } else {
        kWarning() << "No coordinates available for stop" << stop.name;
        return GeoDataCoordinates();
    }
}

GeoDataLatLonBox PublicTransportLayer::boundingBoxFromStops( const QList<Stop> &stops )
{
    if ( stops.isEmpty() ) {
        return GeoDataLatLonBox();
    }

    GeoDataLatLonBox boundingBox;
    foreach ( const Stop &stop, stops ) {
        if ( stop.name.isEmpty() || !stop.hasValidCoordinates ) {
            continue;
        }

        // A size of 0.001 (in degrees) means approximately 111m side length of the stop box,
        // the box covers an area of 12,391m²
        const qreal size = 0.001; // in degrees
        const GeoDataLatLonBox stopBox( stop.latitude + size, stop.latitude - size,
                                        stop.longitude + size, stop.longitude - size,
                                        Marble::GeoDataCoordinates::Degree );
        boundingBox = boundingBox.isEmpty() ? stopBox : boundingBox.united( stopBox );
    }
    return boundingBox;
}

void PublicTransportLayer::setServiceProvider( const QString &serviceProvider )
{
    Q_D( PublicTransportLayer );
    d->serviceProvider = serviceProvider;

    if ( d->flags.testFlag(AutoLoadStopsForMapRegion) ) {
        // Update stops for the current map region, when the auto load feature is enbaled
        startStopsByGeoPositionRequest();
    }
}

void PublicTransportLayer::addStops( const QList<Stop> &stops, bool activeStops )
{
    Q_D( PublicTransportLayer );
    d->addStops( activeStops ? stops : QList<Stop>(),
                 activeStops ? QList<Stop>() : stops );
}

bool PublicTransportLayer::addStop( const Stop &stop, bool activeStop )
{
    Q_D( PublicTransportLayer );
    const bool result = d->addStop( stop, activeStop ? ActiveStop : NoStopFlags );
    d->updateAnnotationData();
    return result;
}

void PublicTransportLayer::removeStop( const Stop &stop )
{
    Q_D( PublicTransportLayer );
    d->stops.remove( stop );
    if ( d->selectedStop == stop ) {
        setSelectedStop( Stop() );
    } else if ( d->hoveredStop == stop ) {
        setHoveredStop( Stop() );
    }
}

void PublicTransportLayer::clear()
{
    Q_D( PublicTransportLayer );
    d->clear();
    if ( !d->stops.contains(d->selectedStop) ) {
        setSelectedStop( Stop() );
    } else if ( !d->stops.contains(d->hoveredStop)) {
        setHoveredStop( Stop() );
    }
}

void PublicTransportLayer::setStops( const QList< Stop > &stops, const QList< Stop > &inactiveStops )
{
    Q_D( PublicTransportLayer );
    clear();
    d->addStops( stops, inactiveStops );
}

QList< Stop > PublicTransportLayer::activeStops() const
{
    Q_D( const PublicTransportLayer );
    return d->filteredStops( true );
}

QList< Stop > PublicTransportLayer::inactiveStops() const
{
    Q_D( const PublicTransportLayer );
    return d->filteredStops( false );
}

void PublicTransportLayer::setSelectedStop( const QString &stopName )
{
    Q_D( PublicTransportLayer );
    for ( QMap<Stop, StopData>::ConstIterator it = d->stops.constBegin();
          it != d->stops.constEnd(); ++it )
    {
        if ( it.key().name == stopName ) {
            setSelectedStop( it.key() );
            break;
        }
    }
}

void PublicTransportLayer::setSelectedStop( const Stop &stop )
{
    Q_D( PublicTransportLayer );

    if ( d->selectedStop == stop ) {
        return;
    } else if ( d->selectedStop.isValid() ) {
        // Remove SelectedStop flag from previously selected stop
        QMap<Stop, StopData>::Iterator it = d->stops.find( d->selectedStop );
        it->flags &= ~SelectedStop;
    }

    // Store newly selected stop
    d->selectedStop = stop;

    // An invalid stop is used for deselection, valid stops get added/overwritten
    if ( !stop.isValid() || d->addStop(stop, SelectedStop) ) {
        // Stop was selected/deselected
        d->updateAnnotationData();
        emit stopSelected( stop );
    }
}

void PublicTransportLayer::setHoveredStop( const Stop &stop )
{
    Q_D( PublicTransportLayer );

    if ( d->hoveredStop == stop ) {
        return;
    } else if ( d->hoveredStop.isValid() ) {
        // Remove HoveredStop flag from previously hovered stop
        QMap<Stop, StopData>::Iterator it = d->stops.find( d->hoveredStop );
        it->flags &= ~HoveredStop;
    }

    // Store newly hovered stop
    d->hoveredStop = stop;

    // An invalid stop is used for unhovering, valid stops get added/overwritten
    if ( !stop.isValid() || d->addStop(stop, HoveredStop) ) {
        // Stop was hovered/unhovered
        d->updateAnnotationData();
        emit stopHovered( stop );
    }
}

void PublicTransportLayer::visibleLatLonAltBoxChanged( const GeoDataLatLonAltBox &latLonAltBox )
{
    Q_D( PublicTransportLayer );

    // Load new stops and update annotations for the new view box
    d->visibleLatLonAltBoxChanged( latLonAltBox );
    d->updateAnnotationData();
}

void PublicTransportLayer::startStopsByGeoPositionRequest()
{
    Q_D( PublicTransportLayer );
    d->startStopsByGeoPositionRequest();
}

void PublicTransportLayer::dataUpdated( const QString &sourceName,
                                        const Plasma::DataEngine::Data &data )
{
    Q_D( PublicTransportLayer );
    if ( !sourceName.startsWith(QLatin1String("Stops")) ) {
        kDebug() << "Wrong source" << sourceName;
        return;
    }

    // Check for errors
    if ( data.value("error").toBool() || !data.contains("stops") ) {
        return;
    }

    // Delete old stops that are out of an extended map viewport rectangle
    ViewportParams *viewport = d->mapWidget->viewport();
    qreal centerX, centerY;
    viewport->screenCoordinates( viewport->centerLongitude(), viewport->centerLatitude(),
                                 centerX, centerY );
    QRect viewportRect( centerX - viewport->width() / 2, centerY - viewport->width() / 2,
                        viewport->width(), viewport->height() );
    viewportRect.adjust( -viewport->width(), -viewport->height(),
                         viewport->width(), viewport->height() );

    QMap<Stop, StopData>::Iterator it = d->stops.begin();
    while ( it != d->stops.end() ) {
        if ( !it->isInternal() ) {
            ++it;
        } else {
            qreal stopX, stopY;
            viewport->screenCoordinates( it->coords, stopX, stopY );
            if ( viewportRect.contains(stopX, stopY) ) {
                ++it;
            } else {
                // The stop is internal (loaded for the current map region)
                // and out of an extended map viewport rectangle
                it = d->stops.erase( it );
            }
        }
    }

    // Convert stop data from the engine to PublicTransport::Stop objects
    // and insert them as internal stops (where requested for the currently visible map region)
    QVariantList stops = data["stops"].toList();
    foreach ( const QVariant &stopData, stops ) {
        const QVariantHash stop = stopData.toHash();
        const QString stopName = stop["StopName"].toString();
        const QString stopID = stop["StopID"].toString();
        const bool coordsAvailable = stop.contains("StopLongitude") && stop.contains("StopLatitude");
        const qreal longitude = coordsAvailable ? stop["StopLongitude"].toReal() : 0;
        const qreal latitude = coordsAvailable? stop["StopLatitude"].toReal() : 0;

        const Stop stopItem( stopName, stopID, coordsAvailable, longitude, latitude );
        d->addStop( stopItem, InternalStop );
    }

    // Disconnect source again
    if ( d->dataSource == sourceName ) {
        Plasma::DataEngine *engine = Plasma::DataEngineManager::self()->engine( "publictransport" );
        engine->disconnectSource( sourceName, this );
        d->dataSource.clear();
    }

    // Notify about the new stops
    emit stopsForVisibleMapRegionLoaded();
}

bool PublicTransportLayer::render( GeoPainter *painter, ViewportParams *viewport,
                                   const QString &renderPos, GeoSceneLayer *layer )
{
    Q_UNUSED( renderPos )
    Q_UNUSED( layer )
    Q_D( PublicTransportLayer );
    return d->render( painter, viewport );
}

bool PublicTransportLayer::containsStop( const Stop &stop ) const
{
    Q_D( const PublicTransportLayer );
    return d->stops.contains( stop );
}

QList< Stop > PublicTransportLayer::stops() const
{
    Q_D( const PublicTransportLayer );
    return d->stops.keys();
}

Stop PublicTransportLayer::selectedStop() const
{
    Q_D( const PublicTransportLayer );
    return d->selectedStop;
}

Stop PublicTransportLayer::hoveredStop() const
{
    Q_D( const PublicTransportLayer );
    return d->hoveredStop;
}

bool PublicTransportLayer::isStopVisible( const Stop &stop ) const
{
    Q_D( const PublicTransportLayer );
    if ( !d->stops.contains(stop) ) {
        return false;
    } else {
        const StopData &data = d->stops[ stop ];
        return !data.isInternal() || d->showInternalStops();
    }
}

}; // namespace PublicTransport
