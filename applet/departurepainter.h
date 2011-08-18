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
class QPixmap;

using namespace Timetable;

class DeparturePainter : public QObject {
    Q_OBJECT

public:
    DeparturePainter( QObject *parent = 0 ) : QObject(parent), m_svg(0) {};

    void setSvg( Plasma::Svg *svg ) { m_svg = svg; };
    Plasma::Svg *svg() const { return m_svg; };

    void paintVehicle( QPainter* painter, VehicleType vehicle,
                       const QRectF& rect, const QString &transportLine );

    QPixmap createMainIconPixmap( const QSize &size ) const;

    /**
     * @brief Creates a pixmap for the given @p departures.
     *
     * This pixmap is used for the popup icon for departures. Multiple
     * departures may be drawn side by side, eg. if they depart at the same
     * time.
     **/
    QPixmap createDeparturesPixmap( const QList<DepartureItem*> &departures, const QSize &size );

    /**
     * @brief Creates a pixmap for the given @p departure with an alarm.
     *
     * This pixmap is used for the popup icon for departures for which an alarm
     * is set.
     **/
    QPixmap createAlarmPixmap( DepartureItem *departure, const QSize &size );

    QPixmap createPopupIcon( int startDepartureIndex, int endDepartureIndex,
            qreal departureIndex, DepartureModel *model,
            const QMap< QDateTime, QList<DepartureItem*> > &departureGroups, const QSize &size );

private:
    Plasma::Svg *m_svg;
};

#endif // DEPARTUREPAINTER_HEADER
