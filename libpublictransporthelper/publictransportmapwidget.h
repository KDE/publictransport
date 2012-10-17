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

#ifndef PUBLICTRANSPORTMAPWIDGET_H
#define PUBLICTRANSPORTMAPWIDGET_H

#include "publictransporthelper_export.h"
#include <marble/MarbleWidget.h>

class QToolButton;
using namespace Marble;

/** @brief Namespace for the publictransport helper library. */
namespace PublicTransport {

struct Stop;
typedef QList<Stop> StopList;
class PublicTransportLayer;
class PublicTransportMapWidgetPrivate;

/**
 * @brief A MarbleWidget which uses a PublicTransportLayer and adds some methods/signals.
 *
 * Stops can be active (showing the stop name in an annotation). They can also be hovered or
 * selected. Hovering/selecting stops is done automatically and the stopHovered()/stopSelected()
 * signals get emitted. To select a stop it's icon can be clicked. To programatically change
 * the selected stop use publicTransportLayer() and call PublicTransportLayer::setSelectedStop()
 * on it.
 **/
class PUBLICTRANSPORTHELPER_EXPORT PublicTransportMapWidget : public MarbleWidget
{
    Q_OBJECT
    friend class PublicTransportLayer;

    // For some reason this is needed to use the correct D-Bus interface,
    // eg. org.kde.plasma-desktop.PublicTransport.PublicTransportMapWidget would be wrong
    // and leads to crashes
    Q_CLASSINFO("D-Bus Interface", "org.kde.MarbleWidget")

public:
    /** @brief Flags for PublicTransportMapWidget widgets. */
    enum Flag {
        NoFlags                     = 0x00, /**< No flags used. */
        AutoLoadStopsForMapRegion   = 0x01, /**< Automatically load stops when the visible
                * map region changes. This feature requires a service provider ID as argument in
                * the PublicTransportMapWidget constructor. The provider needs to support the
                * features ProvidesStopPosition and ProvidesStopsByGeoPosition. This flag
                * enables the corresponding flag PublicTransportLayer::AutoLoadStopsForMapRegion
                * flag in the created layer. */
        DefaultFlags = AutoLoadStopsForMapRegion
    };
    Q_DECLARE_FLAGS( Flags, Flag );

    /** @brief Options to filter stops. */
    enum StopOption {
        VisibleStopsOnly = 0,
        IncludeInvisibleStops
    };

    /**
     * @brief Create a new public transport map widget.
     *
     * @param serviceProvider The ID of the service provider to use for stop suggestion requests
     *   for the AutoLoadStopsForMapRegion feature.
     * @param flags See Flag for available flags.
     * @param parent The parent object.
     * @param layer If this is 0, a PublicTransportLayer object gets created automatically.
     *   Otherwise the given layer object gets used.
     **/
    PublicTransportMapWidget( const QString &serviceProvider, Flags flags = DefaultFlags,
                              QWidget *parent = 0, PublicTransportLayer *layer = 0 );

    /** @brief Destructor. */
    virtual ~PublicTransportMapWidget();

    /**
     * @brief Get the used PublicTransport layer.
     * Can be used to eg. change the currently selected stop.
     **/
    PublicTransportLayer *publicTransportLayer() const;

    /**
     * @brief Get the stop that is nearest to the coordinates @p x, @p y.
     * @param x The horizontal coordinate.
     * @param y The vertical coordinate.
     * @param maxDistance The maximal distance the found stop can have.
     * @param option Used to filter out invisible stop by default, use IncludeInvisibleStops
     *   to also get invisible stops (eg. internal stops for the current map region when zoomed
     *   out too far).
     **/
    Stop stopFromPosition( int x, int y, int maxDistance = 10,
                           StopOption option = VisibleStopsOnly ) const;

    /**
     * @brief Set a list of stops to be shown in the map.
     *
     * @param stops The list of stops to show in the map.
     * @param selectStopName If @p stops contains a stop with this name, it will be selected.
     * @param activeStopNames All stops in @p stops that have a name of this list will be made active.
     **/
    void setStops( const StopList &stops, const QString &selectStopName = QString(),
                   const QStringList &activeStopNames = QStringList() );

    /**
     * @brief Set the provider to be used for stop suggestion requests.
     * This is currently only used with the AutoLoadStopsForMapRegion feature.
     * @param serviceProvider The ID of the service provider to use.
     **/
    void setServiceProvider( const QString &serviceProvider );

Q_SIGNALS:
    /**
     * @brief A public transport @p stop was selected, eg. because it was clicked.
     * @param stop The stop that was selected or an invalid Stop object if there is no selection.
     **/
    void stopClicked( const Stop &stop );

    /**
     * @brief A public transport @p stop was selected, eg. because it was clicked.
     * @param stop The stop that was selected or an invalid Stop object if there is no selection.
     **/
    void stopSelected( const Stop &stop );

    /**
     * @brief A public transport @p stop was hovered.
     * @param stop The stop that was hovered.
     **/
    void stopHovered( const Stop &stop );

protected Q_SLOTS:
    /** @brief The mouse was clicked at @p lon, @p lat. */
    void slotMouseClickGeoPosition( qreal lon, qreal lat, GeoDataCoordinates::Unit unit );

protected:
    /** @brief Overwritten to draw a frame around the map. */
    virtual void paintEvent( QPaintEvent *event );

    /** @brief Overwritten to implement stop hovering. */
    virtual void mouseMoveEvent( QMouseEvent *event );

private:
    PublicTransportMapWidgetPrivate* const d_ptr;
    Q_DECLARE_PRIVATE( PublicTransportMapWidget )
    Q_DISABLE_COPY( PublicTransportMapWidget )
};
Q_DECLARE_OPERATORS_FOR_FLAGS( PublicTransportMapWidget::Flags )

}; // namespace PublicTransport

#endif // PUBLICTRANSPORTMAPWIDGET_H
