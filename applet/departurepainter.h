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

#ifndef DEPARTUREPAINTER_HEADER
#define DEPARTUREPAINTER_HEADER

#include <publictransporthelper/enums.h>
#include <QPainter>

template<class QDateTime, class QList>
class QMap;

template<class DepartureItem>
class QList;

namespace Plasma {
    class Svg;
}

class DepartureModel;
class DepartureItem;
class PopupIcon;
class QPixmap;

using namespace Timetable;

class DeparturePainter : public QObject {
    Q_OBJECT

public:
    DeparturePainter( QObject *parent = 0 ) : QObject(parent), m_svg(0) {};

    /**
     * @brief Flags for vehicle icons.
     *
     * For each combination of these flags and for each vehicle type there is an icon in the SVG.
     **/
    enum VehicleIconFlag {
        ColoredIcon      = 0x0, /**< The default colored vehicle icon. */
        EmptyIcon        = 0x1, /**< Vehicle icon without content like "tram", "bus", "tro", etc.
                * Useful to draw custom constent like the transport line string, eg. "N1". */
        MonochromeIcon   = 0x2 /**< Use monochrome version of the icon. */
    };
    Q_DECLARE_FLAGS( VehicleIconFlags, VehicleIconFlag );

    /**
     * @brief Flags for drawing vehicle icons.
     **/
    enum VehicleIconDrawFlag {
        DrawColoredIcon     = 0x00, /**< Draw a colored vehicle type icon. */
        DrawMonochromeIcon  = 0x01, /**< Draw a monochrome version of the vehicle type icon. */
        DrawTransportLine   = 0x02, /**< Draw the transport line string into the vehicle type icon. */
        DrawTimeText        = 0x04, /**< Draw the time until departure/arrival as text. */
        DrawTimeGraphics    = 0x08, /**< Draw the time until departure/arrival indicated
                * using a graphical representation. */
    };
    Q_DECLARE_FLAGS( VehicleIconDrawFlags, VehicleIconDrawFlag );

    /**
     * @brief The maximum number of minutes until departure that has a distinct visualization.
     *
     * Departures with a higher number of minutes until their departure time get the same graphical
     * representation.
     * If this is 60, it can be read intuitively like a clock.
     **/
    static const int MAX_MINUTES_UNTIL_DEPARTURE = 60;

    void setSvg( Plasma::Svg *svg ) { m_svg = svg; };
    Plasma::Svg *svg() const { return m_svg; };

    static QString iconKey( VehicleType vehicle, VehicleIconFlags flags = ColoredIcon );
    static VehicleIconFlags iconFlagsFromIconDrawFlags( VehicleIconDrawFlags flags = DrawColoredIcon );

    /// @param iconDrawFlags DrawTimeText does nothing here, but DrawTimeGraphics works.
    void paintVehicle( QPainter* painter, VehicleType vehicle,
                       const QRectF& rect, const QString &transportLine = QString(),
                       int minsToDeparture = 0,
                       VehicleIconDrawFlags iconDrawFlags =
                       VehicleIconDrawFlag(DrawMonochromeIcon | DrawTransportLine | DrawTimeGraphics) );

    QPixmap createMainIconPixmap( const QSize &size ) const;

    /**
     * @brief Creates a pixmap for the given @p departures.
     *
     * This pixmap is used for the popup icon for departures. Multiple
     * departures may be drawn side by side, eg. if they depart at the same
     * time.
     **/
    QPixmap createDeparturesPixmap( DepartureItem *departure, const QSize &size,
            VehicleIconDrawFlags iconDrawFlags =
            VehicleIconDrawFlags(DrawMonochromeIcon | DrawTransportLine | DrawTimeGraphics) );

    /**
     * @brief Creates a pixmap for the given @p departure with an alarm.
     *
     * This pixmap is used for the popup icon for departures for which an alarm
     * is set.
     **/
    QPixmap createAlarmPixmap( DepartureItem *departure, const QSize &size );

    QPixmap createPopupIcon( PopupIcon *popupIconData, DepartureModel *model, const QSize &size );

private:
    Plasma::Svg *m_svg;
};
Q_DECLARE_OPERATORS_FOR_FLAGS( DeparturePainter::VehicleIconFlags );
Q_DECLARE_OPERATORS_FOR_FLAGS( DeparturePainter::VehicleIconDrawFlags );

#endif // DEPARTUREPAINTER_HEADER
