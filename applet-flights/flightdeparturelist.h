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

#ifndef FLIGHTDEPARTURELIST_H
#define FLIGHTDEPARTURELIST_H

#include <QGraphicsWidget>
#include <Plasma/DataEngine>

namespace Plasma
{
    class Label;
    class IconWidget;
}

class FlightDeparture : public QGraphicsWidget {
public:
    FlightDeparture( QGraphicsItem* parent = 0 );

    virtual void paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                        QWidget* widget = 0 );

    QDateTime departure() const { return m_departure; };
    QString target() const { return m_target; };
    QString flightNumber() const { return m_flightNumber; };
    QString status() const { return m_status; };
    QString airline() const { return m_airline; };

    void setDeparture( const QDateTime& departure );
    void setTarget( const QString &target );
    void setFlightNumber( const QString &flightNumber );
    void setStatus( const QString &status );
    void setAirline( const QString &airline );

private:
    QString headerText() const;
    QString infoText() const;

    QDateTime m_departure;
    QString m_target;
    QString m_flightNumber;
    QString m_status;
    QString m_airline;

    Plasma::IconWidget *m_icon;
    Plasma::Label *m_header;
    Plasma::Label *m_info;
};

class FlightDepartureList : public QGraphicsWidget
{
public:
    explicit FlightDepartureList( QGraphicsItem* parent = 0, Qt::WindowFlags wFlags = 0 );

    void setTimetableData( const Plasma::DataEngine::Data &timetableData );
    void updateLayout();

    QList<FlightDeparture*> departures() const { return m_departures; };

private:
    QList<FlightDeparture*> m_departures;
    QGraphicsWidget *m_contentWidget;
};

#endif // FLIGHTDEPARTURELIST_H
