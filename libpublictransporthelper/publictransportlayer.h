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

/** @file
* @brief This file contains the PublicTransportLayer class to be used with a MarbleWidget.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef PUBLICTRANSPORTLAYER_H
#define PUBLICTRANSPORTLAYER_H

// Own includes
#include "publictransporthelper_export.h"
#include "stopsettings.h"

// Marble includes
#include <marble/LayerInterface.h>

// Plasma includes
#include <Plasma/DataEngine>

namespace Marble {
    class MarbleWidget;
    class GeoDataCoordinates;
    class GeoDataLatLonBox;
    class GeoDataLatLonAltBox;
}
using namespace Marble;

class QFontMetrics;

/** @brief Namespace for the publictransport helper library. */
namespace PublicTransport {

class PublicTransportLayerPrivate;

/**
 * @brief A Marble map layer showing public transport stops/stations.
 *
 * Stops can be active, showing the stop name in an annotation (if it does not averlap with another
 * annotation). They can also be hovered or selected, see setHoveredStop(), setSelectedStop().
 * An annotation is always drawn for the currently hovered/selected stops, if any.
 *
 * If the AutoLoadStopsForMapRegion flag is set in the constructor, the ID of the provider to be
 * used for stop suggestion requests needs to be given. The provider needs to support the features
 * ProvidesStopPosition and ProvidesStopSuggestionsByPosition.
 **/
class PUBLICTRANSPORTHELPER_EXPORT PublicTransportLayer : public QObject, public LayerInterface
{
    Q_OBJECT
    friend class PublicTransportMapWidget;

public:
    /** @brief Flags for this layer. */
    enum Flag {
        NoFlags                     = 0x00, /**< No special features. */
        AutoLoadStopsForMapRegion   = 0x01, /**< Automatically load stops when the visible
                * map region changes. This feature requires a service provider ID as argument in
                * the PublicTransportLayer constructor. The provider needs to support the features
                * ProvidesStopPosition and ProvidesStopSuggestionsByPosition. When these stops are
                * loaded the stopsForVisibleMapRegionLoaded() signal gets emitted. */
        DefaultFlags                = AutoLoadStopsForMapRegion /**< Default flags. */
    };
    Q_DECLARE_FLAGS( Flags, Flag )

    /** @brief Get coordinates from @p stop. */
    static GeoDataCoordinates coordsFromStop( const Stop &stop );

    /** @brief Construct a bounding box containing all given @p stops. */
    static GeoDataLatLonBox boundingBoxFromStops( const QList<Stop> &stops );

    /**
     * @brief Constructor.
     *
     * @param mapWidget The MarbleWidget that gets used to display this layer. The currently
     *   visible map region gets used for the AutoLoadStopsForMapRegion feature and to decide
     *   which annotations to draw.
     * @param serviceProvider The ID of the service provider to use for stop suggestion requests
     *   for the AutoLoadStopsForMapRegion feature.
     * @param flags See Flag for available flags.
     * @param parent Parent object.
     **/
    PublicTransportLayer( MarbleWidget *mapWidget, const QString &serviceProvider = QString(),
                          Flags flags = DefaultFlags, QObject *parent = 0 );

    /** @brief Destructor. */
    virtual ~PublicTransportLayer();

    /** @brief Set the provider to be used to request stops for the AutoLoadStopsForMapRegion. */
    void setServiceProvider( const QString &serviceProvider );

    /** @brief Clears the stop list and adds @p stops as active and @p otherStops as normal stops. */
    void setStops( const QList<Stop> &stops, const QList<Stop> &otherStops = QList<Stop>() );

    /** @brief Add @p stop to this layer, make it active if @p activeStop is @c true. */
    bool addStop( const Stop &stop, bool activeStop = true );

    /** @brief Add @p stops to this layer, make them active if @p activeStops is @c true. */
    void addStops( const QList<Stop> &stops, bool activeStops = true );

    /** @brief Remove @p stop from this layer. */
    void removeStop( const Stop &stop );

    /** @brief Remove all stops from this layer. */
    void clear();

    /** @brief Whether or not @p stop is contained in this layer. */
    bool containsStop( const Stop &stop ) const;

    /** @brief Get a list of all stops in this layer, including automatically loaded ones. */
    QList< Stop > stops() const;

    /** @brief Get a list of all stops in this layer, which are marked as active. */
    QList< Stop > activeStops() const;

    /** @brief Get a list of all stops in this layer, which are marked as inactive. */
    QList< Stop > inactiveStops() const;

    /** @brief Get the currently selected stop or an invalid Stop object if no stop is selected. */
    Stop selectedStop() const;

    /** @brief Get the currently hovered stop or an invalid Stop object if no stop is hovered. */
    Stop hoveredStop() const;

    /** @brief Whether or not @p stop is visible in the map widget. */
    bool isStopVisible( const Stop &stop ) const;

    /** @brief Implemented from LayerInterface. */
    virtual QStringList renderPosition() const { return QStringList() << "HOVERS_ABOVE_SURFACE"; };

    /** @brief Implemented from LayerInterface. */
    virtual bool render( GeoPainter *painter, ViewportParams *viewport,
            const QString& renderPos = "HOVERS_ABOVE_SURFACE", GeoSceneLayer * layer = 0 );

Q_SIGNALS:
    /**
     * @brief Emitted when stops were loaded automatically.
     * @note The MarbleWidget gets updated automatically using the QWidget::update() slot.
     **/
    void stopsForVisibleMapRegionLoaded();

    /**
     * @brief A public transport @p stop was selected.
     * @param stop The stop that was selected or an invalid Stop object if the selection was cleared.
     **/
    void stopSelected( const Stop &stop );

    /**
     * @brief A public transport @p stop was hovered.
     * @param stop The stop that was hovered or an invalid Stop object if a stop is no longer
     *   hovered.
     **/
    void stopHovered( const Stop &stop );

public Q_SLOTS:
    /**
     * @brief Make @p stop the currently selected stop.
     * To select nothing use an invalid Stop object.
     **/
    void setSelectedStop( const Stop &stop );

    /**
     * @brief Make the stop with @p stopName the currently selected stop, if any.
     * To select nothing use an empty string.
     **/
    void setSelectedStop( const QString &stopName );

    /**
     * @brief Make @p stop the currently hovered stop.
     * To hover nothing use an invalid Stop object.
     **/
    void setHoveredStop( const Stop &stop );

protected Q_SLOTS:
    /** @brief Updated data received from the engine. */
    void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );

    /** @brief The current map viewport changed. */
    void visibleLatLonAltBoxChanged( const GeoDataLatLonAltBox &latLonAltBox );

    /** @brief Start requesting stops from the maps current geo position. */
    void startStopsFromGeoPositionRequest();

private:
    PublicTransportLayerPrivate* const d_ptr;
    Q_DECLARE_PRIVATE( PublicTransportLayer )
    Q_DISABLE_COPY( PublicTransportLayer )
};
Q_DECLARE_OPERATORS_FOR_FLAGS( PublicTransportLayer::Flags )

}; // namespace PublicTransport

#endif // Multiple inclusion guard
