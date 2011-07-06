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

#ifndef ROUTEGRAPHICSITEM_HEADER
#define ROUTEGRAPHICSITEM_HEADER

#include <QGraphicsWidget>
#include <QPointer>
#include <Plasma/Svg>
#include "global.h"

class KMenu;
class JourneyItem;
class DepartureItem;

class RouteStopMarkerGraphicsItem : public QGraphicsWidget {
    Q_OBJECT
    Q_PROPERTY( qreal hoverStep READ hoverStep WRITE setHoverStep )

public:
    /**
     * @brief The type of a RouteStopMarkerGraphicsItem.
     **/
    enum MarkerType {
        DefaultStopMarker = 0, /**< A default route stop marker. */
        IntermediateStopMarker = 1 /**< A route stop marker for intermediate
                * stops that are omitted / not displayed. */
    };

    RouteStopMarkerGraphicsItem( QGraphicsItem* parent = 0,
                                 MarkerType markerType = DefaultStopMarker );

    qreal radius() const { return 6.0 + 2.0 * m_hoverStep; };
    MarkerType markerType() const { return m_markerType; };

    virtual void paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                        QWidget* widget = 0 );

    qreal hoverStep() const { return m_hoverStep; };
    void setHoverStep( qreal expandStep );

    virtual QRectF boundingRect() const {
        return QRectF( -radius(), -radius(), 2 * radius(), 2 * radius() );
    };

    virtual QSizeF sizeHint( Qt::SizeHint which, const QSizeF& constraint = QSizeF() ) const {
        if ( which == Qt::MinimumSize || which == Qt::MaximumSize ) {
            return QSizeF( 2 * radius(), 2 * radius() );
        } else {
            return QGraphicsWidget::sizeHint( which, constraint );
        }
    };

signals:
    void hovered( RouteStopMarkerGraphicsItem *item );
    void unhovered( RouteStopMarkerGraphicsItem *item );

public slots:
    void hover();
    void unhover();

protected:
    virtual void hoverEnterEvent( QGraphicsSceneHoverEvent* event );
    virtual void hoverLeaveEvent( QGraphicsSceneHoverEvent* event );

private:
    qreal m_hoverStep;
    MarkerType m_markerType;
};

/**
* @brief A QGraphicsWidget showing the a single route stop of a public transport vehicle.
*
* On hover it expands too show all of the given stop text (if it is too long).
*
* @note To make all context menu entries work, connect to the slots
* requestFilterCreation and showDepartures.
**/
class RouteStopTextGraphicsItem : public QGraphicsWidget {
    Q_OBJECT
    Q_PROPERTY( qreal expandStep READ expandStep WRITE setExpandStep )

public:
    RouteStopTextGraphicsItem( QGraphicsItem* parent, const QFont &font, qreal baseSize,
                               const QString &stopText, const QString &stopName );
    ~RouteStopTextGraphicsItem();

    QString stopText() const { return m_stopText; };
    QString stopName() const { return m_stopName; };
    void setStop( const QString &stopText, const QString &stopName );

    qreal expandStep() const { return m_expandStep; };
    void setExpandStep( qreal expandStep );
    qreal baseSize() const { return m_baseSize; };
    void setBaseSize( qreal baseSize );

    virtual void paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                        QWidget* widget = 0 );

signals:
    void hovered( RouteStopTextGraphicsItem *item );
    void unhovered( RouteStopTextGraphicsItem *item );
    void requestStopAction( StopAction stopAction, const QString &stopName, const QVariant &data,
                            QGraphicsWidget *item );

public slots:
    void hover();
    void unhover();

protected:
    virtual void hoverEnterEvent( QGraphicsSceneHoverEvent* event );
    virtual void hoverLeaveEvent( QGraphicsSceneHoverEvent* event );
    virtual void contextMenuEvent( QGraphicsSceneContextMenuEvent* event );

private:
    QString m_stopText;
    QString m_stopName;
    qreal m_expandStep;
    qreal m_baseSize;
    QPointer<KMenu> m_contextMenu;
};

/**
* @brief A QGraphicsWidget showing the route of a public transport vehicle.
*
* The route is shown as a thick white line with stop markers on it. For each
* stop marker the associated stop name is shown with it's departure time.
* This widget automatically shows/hides stops on size changes.
**/
class RouteGraphicsItem : public QGraphicsWidget {
    Q_OBJECT

public:
    RouteGraphicsItem( QGraphicsItem* parent, DepartureItem *item );

    void updateData( DepartureItem *item );

    void setZoomFactor( qreal zoomFactor = 1.0 ) { m_zoomFactor = zoomFactor; };
    qreal zoomFactor() const { return m_zoomFactor; };
    inline qreal padding() const { return 5.0; };

    virtual void paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                        QWidget* widget = 0 );

    QPointer<DepartureItem> item() const { return m_item; };

signals:
    void requestStopAction( StopAction stopAction, const QString &stopName, const QVariant &data,
                            QGraphicsWidget *item );

// public slots:
//     void dataChanged();

protected:
    virtual void resizeEvent( QGraphicsSceneResizeEvent* event );
    void arrangeStopItems();

private:
    QPointer<DepartureItem> m_item;
    QList<RouteStopMarkerGraphicsItem*> m_markerItems;
    QList<RouteStopTextGraphicsItem*> m_textItems;
    qreal m_zoomFactor;
    qreal m_textAngle;
    qreal m_maxTextWidth;
};

class JourneyRouteGraphicsItem;
class JourneyRouteStopGraphicsItem : public QGraphicsWidget {
    Q_OBJECT

public:
    JourneyRouteStopGraphicsItem( JourneyRouteGraphicsItem* parent, const QPixmap &vehiclePixmap,
                                  const QString &text, RouteStopFlags routeStopFlags,
                                  const QString &stopName );
    ~JourneyRouteStopGraphicsItem();

    void setText( const QString &text );

    QRectF infoTextRect() const;
    RouteStopFlags routeStopFlags() const { return m_stopFlags; };
    QString stopName() const { return m_stopName; };

    virtual void paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                        QWidget* widget = 0 );

signals:
    void requestStopAction( StopAction stopAction, const QString &stopName, const QVariant &data,
                            QGraphicsWidget *routeStopItem );

protected:
    virtual QSizeF sizeHint( Qt::SizeHint which, const QSizeF& constraint = QSizeF() ) const;
    virtual void contextMenuEvent( QGraphicsSceneContextMenuEvent* event );
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* /*event*/) { update(); };

private:
    JourneyRouteGraphicsItem *m_parent;
    QPixmap m_vehiclePixmap;
    QTextDocument *m_infoTextDocument;

    RouteStopFlags m_stopFlags;
    QString m_stopName;
    QPointer<KMenu> m_contextMenu;
};

class JourneyRouteGraphicsItem : public QGraphicsWidget {
    Q_OBJECT

public:
    JourneyRouteGraphicsItem( QGraphicsItem *parent, JourneyItem *item, Plasma::Svg *svg );

    void updateData( JourneyItem *item );

    void setZoomFactor( qreal zoomFactor = 1.0 ) { m_zoomFactor = zoomFactor; };
    qreal zoomFactor() const { return m_zoomFactor; };
    inline qreal padding() const { return 5.0; };
    Plasma::Svg *svg() const { return m_svg; };
    JourneyItem *journeyItem() const { return m_item; };

    virtual void paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                        QWidget* widget = 0 );

signals:
    void requestStopAction( StopAction stopAction, const QString &stopName, const QVariant &data,
                            QGraphicsWidget *routeStopItem );

protected slots:
    void processStopAction( StopAction stopAction, const QString &stopName, const QVariant &data,
                            QGraphicsWidget *routeStopItem );

private:
    QPointer<JourneyItem> m_item;
    Plasma::Svg *m_svg;
    qreal m_zoomFactor;
    QList<JourneyRouteStopGraphicsItem*> m_routeItems;
};

#endif // Multiple inclusion guard
