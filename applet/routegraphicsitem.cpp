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

#include "routegraphicsitem.h"
#include "departuremodel.h"
#include "timetablewidget.h"
#include <publictransporthelper/departureinfo.h>

#include <Plasma/PaintUtils>
#include <KColorUtils>
#include <KGlobalSettings>

#include <QPainter>
#include <QPropertyAnimation>
#include <qmath.h>
#include <QGraphicsLinearLayout>
#include <QTextDocument>
#include <QGraphicsEffect>
#include <QGraphicsSceneHoverEvent>
#include <QMenu>
#include <KMenu>

RouteGraphicsItem::RouteGraphicsItem( QGraphicsItem* parent, DepartureItem *item,
        StopAction *copyStopToClipboardAction, StopAction *showDeparturesAction,
        StopAction *highlightStopAction, StopAction *newFilterViaStopAction )
        : QGraphicsWidget(parent), m_item(item),
        m_copyStopToClipboardAction(copyStopToClipboardAction),
        m_showDeparturesAction(showDeparturesAction), m_highlightStopAction(highlightStopAction),
        m_newFilterViaStopAction(newFilterViaStopAction)
{
    m_zoomFactor = 1.0;
    m_textAngle = 15.0;
    m_maxTextWidth = 100.0;
    updateData( item );
}

void RouteGraphicsItem::resizeEvent(QGraphicsSceneResizeEvent* event)
{
    QGraphicsWidget::resizeEvent(event);
    arrangeStopItems();
}

void RouteGraphicsItem::arrangeStopItems()
{
    if ( !m_item ) {
        return;
    }

    const DepartureInfo *info = m_item->departureInfo();
    if ( info->routeStops().count() != m_textItems.count() ) {
        updateData( m_item );
    } else if ( !info->routeStops().isEmpty() ) {
        int count = info->routeStops().count();
        QFont routeFont = KGlobalSettings::smallestReadableFont();
        routeFont.setPointSizeF( routeFont.pointSizeF() * m_zoomFactor );
        QFont boldRouteFont = routeFont;
        boldRouteFont.setBold( true );
        QFontMetrics fm( routeFont );
        QFontMetrics fmBold( boldRouteFont );
        const QRectF routeRect = rect();
        const qreal routeStopAreaWidth = routeRect.width() - 20 * m_zoomFactor;

        // Width of the route line (on which the stop items are displayed)
        const qreal routeLineWidth = 4.0 * m_zoomFactor;

        // The position of the first stop item
        const QPointF startStopPos( 2 * padding() * m_zoomFactor,
                                    padding() * m_zoomFactor + routeLineWidth / 2.0 );

        // Distance between two stop items
        if ( (routeRect.width() - 4 * padding() * m_zoomFactor) / count < 2 * fm.height() ) {
            count = qFloor( routeRect.width() / (2 * fm.height()) );
        }
        const qreal step = routeStopAreaWidth / count;

        // Compute minimal text angle between 15 and 90 degrees,
        // so that the stop names don't overlap
        m_textAngle = qBound( 15.0, qAtan(fm.height() / step) * 180.0 / 3.14159, 90.0 );

        // Compute maximal text width for the computed angle,
        // so that the stop name won't go outside of routeRect
        m_maxTextWidth = (routeRect.height() - startStopPos.y() /*- 10*/ - 6.0 * m_zoomFactor
                - qCos(m_textAngle * 3.14159 / 180.0) * fm.height())
                / qSin(m_textAngle * 3.14159 / 180.0);

        for ( int i = 0; i < count; ++i ) {
            QPointF stopMarkerPos( startStopPos.x() + i * step, startStopPos.y() );
            QPointF stopTextPos( stopMarkerPos.x() - 4 * m_zoomFactor,
                                 stopMarkerPos.y() + 6.0 * m_zoomFactor );
            QString stopName = info->routeStops()[i];
            QString stopText = stopName;
            QFontMetrics *fontMetrics;
            QFont *font;
            if ( i == 0 || i == count - 1 ) {
                font = &boldRouteFont;
                fontMetrics = &fmBold;
            } else {
                font = &routeFont;
                fontMetrics = &fm;
            }

            // Prepend departure time at the current stop, if a time is given
            QTime time;
            int minsFromFirstRouteStop = -1;
            if ( i < info->routeTimes().count() && info->routeTimes()[i].isValid() ) {
                time = info->routeTimes()[i];
                minsFromFirstRouteStop = qCeil( info->departure().time().secsTo(time) / 60 );
            }

            // Elide long stop names
            qreal baseSize;
            if ( i >= count - 2 ) {
                // The last stop names may not fit horizontally (correct the last two here)
                baseSize = qMin( m_maxTextWidth,
                        (routeRect.width() - stopTextPos.x() /*- 2 * fontMetrics->height()*/)
                        / qCos(m_textAngle * 3.14159 / 180.0) );
            } else {
                baseSize = m_maxTextWidth;
            }

            RouteStopMarkerGraphicsItem *markerItem = m_markerItems[ i ];
            markerItem->setPos( stopMarkerPos );

            // Create sub item, that displays a single stop name
            // and automatically elides it and stretches it on hover to show hidden text
            RouteStopTextGraphicsItem *textItem = m_textItems[ i ];
            textItem->resetTransform();
            textItem->setStop( time, stopName, minsFromFirstRouteStop );
            textItem->setFont( *font );
            textItem->setPos( stopTextPos );
            textItem->setBaseSize( baseSize );
            textItem->resize( baseSize + 10, fontMetrics->height() );
            textItem->rotate( m_textAngle );
        }
    }
}

void RouteGraphicsItem::updateData( DepartureItem *item )
{
    if ( rect().isEmpty() ) {
        kDebug() << "Empty rect for the RouteGraphicsItem";
        return;
    }
    m_item = item;
    const DepartureInfo *info = m_item->departureInfo();

    // First remove all old RouteStopGraphicsItems
    qDeleteAll( m_textItems );
    qDeleteAll( m_markerItems );
    m_textItems.clear();
    m_markerItems.clear();

    // Add route stops if there are at least two stops given from the data engine
    if ( info->routeStops().count() >= 2 ) {
        QFont routeFont = KGlobalSettings::smallestReadableFont();
        routeFont.setPointSizeF( routeFont.pointSizeF() * m_zoomFactor );
        QFont boldRouteFont = routeFont;
        boldRouteFont.setBold( true );
        QFontMetrics fm( routeFont );
        QFontMetrics fmBold( boldRouteFont );
        const QRectF routeRect = rect();
        const qreal routeLineWidth = 4.0 * m_zoomFactor;
        const QPointF startStopPos( (10 /*- 4*/) * m_zoomFactor,  padding() + routeLineWidth / 2.0 );
        const qreal routeStopAreaWidth = routeRect.width() - 20 * m_zoomFactor;
        const qreal minStep = fm.height() * 3.0;

        // Compute number of route stop items 
        // without using more space than routeStopAreaWidth
        int count = info->routeStops().count();
        if ( minStep * count > routeStopAreaWidth ) {
            count = qFloor( routeStopAreaWidth / minStep );
        }

        // Compute distance between two route stop items
        const qreal step = routeStopAreaWidth / count;

        // Compute minimal text angle between 15 and 90 degrees,
        // so that the stop names don't overlap
        m_textAngle = qBound( 15.0, qAtan(fm.height() / step) * 180.0 / 3.14159, 90.0 );

        // Compute maximal text width for the computed angle,
        // so that the stop name won't go outside of routeRect
        m_maxTextWidth = (routeRect.height() - startStopPos.y() /*- 10*/ - 6.0 * m_zoomFactor
                - qCos(m_textAngle * 3.14159 / 180.0) * fm.height())
                / qSin(m_textAngle * 3.14159 / 180.0);

        // TODO: Ensure the highlighted stop name gets shown (not omitted)
        int omitCount = info->routeStops().count() - count;
        int omitIndex = omitCount == 0 ? -1 : qFloor(count / 2.0);

        DepartureModel *model = qobject_cast<DepartureModel*>( m_item->model() );
        QPalette highlightedPalette = palette();
        QPalette defaultPalette = palette();
        if ( !model->highlightedStop().isEmpty() ) {
            QColor highlightColor = KColorUtils::mix(
                    Plasma::Theme::defaultTheme()->color(Plasma::Theme::HighlightColor),
                    palette().color(QPalette::Active, QPalette::Text), 0.3 );
            highlightedPalette.setColor( QPalette::Active, QPalette::Text,
                                         highlightColor );
            highlightedPalette.setColor( QPalette::Active, QPalette::ButtonText,
                                         highlightColor );
        } else {
            #if KDE_VERSION < KDE_MAKE_VERSION(4,6,0)
                defaultPalette.setColor( QPalette::Active, QPalette::ButtonText,
                                         Plasma::Theme::TextColor );
            #else
                defaultPalette.setColor( QPalette::Active, QPalette::ButtonText,
                                         Plasma::Theme::ViewTextColor );
            #endif
        }

        for ( int positionIndex = 0; positionIndex < count; ++positionIndex ) {
            const QPointF stopMarkerPos( startStopPos.x() + positionIndex * step, startStopPos.y() );
            int index;
            if ( positionIndex == omitIndex ) { // Currently at first omitted stop
                // Create intermediate marker item
                RouteStopMarkerGraphicsItem *markerItem = new RouteStopMarkerGraphicsItem(
                        this, RouteStopMarkerGraphicsItem::IntermediateStopMarker );
                markerItem->setPos( stopMarkerPos );
                m_markerItems << markerItem;

                // Create a list with all omitted stops (and times if available)
                // to be used for the tooltip of the intermediate marker item
                QStringList omittedStopList;
                for ( int omittedIndex = omitIndex;
                      omittedIndex < omitIndex + omitCount; ++omittedIndex )
                {
                    QString stopText = info->routeStops()[omittedIndex];

                    // Prepend departure time at the current stop, if a time is given
                    if ( info->routeTimes()[omittedIndex].isValid() ) {
                        stopText = stopText.prepend( QString("%1: ")
                                .arg(KGlobal::locale()->formatTime(info->routeTimes()[omittedIndex])) );
                    }

                    bool manuallyHighlighted =
                            model->routeItemFlags(stopText).testFlag(RouteItemHighlighted);
                    omittedStopList << (manuallyHighlighted
                            ? QString("<emphasis strong='1'>%1</emphasis>").arg(stopText)
                            : stopText);
                }
                markerItem->setToolTip( i18nc("@info:tooltip This is the title for "
                        "tooltips of stop marker items for omitted route stops. "
                        "The names (and times if available) of the omitted stops "
                        "get placed at '%1'.",
                        "<emphasis strong='1'>Intermediate stops:</emphasis><nl/>%1",
                        omittedStopList.join(",<nl/>")) );
                continue;
            } else if ( positionIndex > omitIndex ) {
                // Currently after the omitted stops, compute index in stop list 
                // by adding omitted count to positional index
                index = positionIndex + omitCount;
            } else {
                // Currently before the omitted stops, index in stop list equals
                // positional index
                index = positionIndex;
            }

            const QPointF stopTextPos( stopMarkerPos.x() - 4 * m_zoomFactor,
                                       stopMarkerPos.y() + 6.0 * m_zoomFactor );
            QString stopName = info->routeStops()[index];
            QString stopText = stopName;
            QFontMetrics *fontMetrics;
            QFont *font;

            bool manuallyHighlighted = model->routeItemFlags(stopName).testFlag(RouteItemHighlighted);
            if ( index == 0 || index == info->routeStops().count() - 1 // first and last item
                 || manuallyHighlighted )
            {
                font = &boldRouteFont;
                fontMetrics = &fmBold;
            } else {
                font = &routeFont;
                fontMetrics = &fm;
            }

            // Prepend departure time at the current stop, if a time is given
            if ( index < info->routeTimes().count() && info->routeTimes()[index].isValid() ) {
                stopText = stopText.prepend( QString("%1: ")
                        .arg(KGlobal::locale()->formatTime(info->routeTimes()[index])) );
            }

            // Get max text width
            qreal baseSize;
            if ( index >= info->routeStops().count() - 2 ) {
                // The last stop names may not fit horizontally (correct the last two here)
                baseSize = qMin( m_maxTextWidth,
                        (routeRect.width() - stopTextPos.x() /*- 2 * fontMetrics->height()*/)
                        / qCos(m_textAngle * 3.14159 / 180.0) );
            } else {
                baseSize = m_maxTextWidth;
            }

            // Create marker item
            RouteStopFlags routeStopFlags;
            if ( positionIndex == 0 ) {
                routeStopFlags |= RouteStopIsOrigin;
            } else if ( positionIndex == info->routeStops().count() - 1 ) {
                routeStopFlags |= RouteStopIsTarget;
            } else {
                routeStopFlags |= RouteStopIsIntermediate;
            }
            DepartureModel *model = qobject_cast<DepartureModel*>( m_item->model() );
            if ( model->info().homeStop == stopName ) {
                routeStopFlags |= RouteStopIsHomeStop;
            }
            RouteStopMarkerGraphicsItem *markerItem = new RouteStopMarkerGraphicsItem( this,
                    RouteStopMarkerGraphicsItem::DefaultStopMarker, routeStopFlags );
            markerItem->setPos( stopMarkerPos );
            m_markerItems << markerItem;

            // Create text item, that displays a single stop name
            // and automatically elides and stretches it on hover to show hidden text
            QTime time;
            int minsFromFirstRouteStop = -1;
            if ( index < info->routeTimes().count() && info->routeTimes()[index].isValid() ) {
                time = info->routeTimes()[index];
                minsFromFirstRouteStop = qCeil( info->departure().time().secsTo(time) / 60 );
            }
            RouteStopTextGraphicsItem *textItem = new RouteStopTextGraphicsItem(
                    this, *font, baseSize, time, stopName, minsFromFirstRouteStop );
            textItem->setPos( stopTextPos );
            textItem->resize( baseSize + 10, fontMetrics->height() );
            textItem->rotate( m_textAngle );
            textItem->addActions( QList<QAction*>() << m_showDeparturesAction
                    << m_highlightStopAction << m_newFilterViaStopAction
                    << m_copyStopToClipboardAction );
            if ( manuallyHighlighted ) {
                textItem->setPalette( manuallyHighlighted
                        ? highlightedPalette : defaultPalette );
            }
            m_textItems << textItem;

            // Connect (un)hovered signals and (un)hover slots of text and marker items
            connect( markerItem, SIGNAL(hovered(RouteStopMarkerGraphicsItem*)),
                     textItem, SLOT(hover()) );
            connect( markerItem, SIGNAL(unhovered(RouteStopMarkerGraphicsItem*)),
                     textItem, SLOT(unhover()) );
            connect( textItem, SIGNAL(hovered(RouteStopTextGraphicsItem*)),
                     markerItem, SLOT(hover()) );
            connect( textItem, SIGNAL(unhovered(RouteStopTextGraphicsItem*)),
                     markerItem, SLOT(unhover()) );
        }
    }
}

void RouteGraphicsItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                               QWidget* widget )
{
    Q_UNUSED( option );
    Q_UNUSED( widget );

    if ( !m_item ) {
        // Item was already deleted
        // TODO: Paint to a pixmap before deletion and just paint that pixmap here?
        return;
    }

    const DepartureInfo *info = m_item->departureInfo();
    int count = info->routeStops().count();
    if ( count == 0 ) {
        kDebug() << "No route information";
        return;
    }

    painter->setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );

    const QRectF routeRect = rect();
    const qreal step = (routeRect.width() - 20 * m_zoomFactor) / count;
    const qreal routeLineWidth = 4.0 * m_zoomFactor;
    const qreal smallStopMarkerSize = 3 * m_zoomFactor;

    // Draw horizontal timeline
#if KDE_VERSION < KDE_MAKE_VERSION(4,6,0)
    QColor backgroundColor = Plasma::Theme::defaultTheme()->color(Plasma::Theme::BackgroundColor);
    painter->setPen( Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor) );
#else
    QColor backgroundColor = Plasma::Theme::defaultTheme()->color(Plasma::Theme::ViewBackgroundColor);
    painter->setPen( Plasma::Theme::defaultTheme()->color(Plasma::Theme::ViewTextColor) );
#endif
    QLinearGradient backgroundGradient( 0, 0, 1, 0 );
    backgroundGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
    backgroundGradient.setColorAt( 0, backgroundColor );
    backgroundGradient.setColorAt( 1, Qt::transparent );
    painter->setBrush( backgroundGradient );
    painter->drawRoundedRect( QRectF(routeRect.left(), routeRect.top() + padding(),
                                    routeRect.width() - step, routeLineWidth),
                              routeLineWidth / 2.0, routeLineWidth / 2.0 );

    painter->setBrush( backgroundColor );
    const QPointF startStopPos( routeRect.left() + 10 * m_zoomFactor,
                                routeRect.top() + padding() + routeLineWidth / 2.0 );
    for ( int i = 0; i < info->routeStops().count(); ++i ) {
        QPointF stopPos( startStopPos.x() + i * step, startStopPos.y() );
        if ( info->routeExactStops() > 1 && i >= info->routeExactStops() - 1
            && i < info->routeStops().count() - 1 )
        {
            // Visualize intermediate stops after the last exact stop
            KIcon stopIcon("public-transport-stop");
            painter->drawEllipse( stopPos + QPointF(step * 0.333333, 0.0),
                                  smallStopMarkerSize, smallStopMarkerSize );
            painter->drawEllipse( stopPos + QPointF(step * 0.666666, 0.0),
                                  smallStopMarkerSize, smallStopMarkerSize );
        }
    }
}

RouteStopMarkerGraphicsItem::RouteStopMarkerGraphicsItem( QGraphicsItem* parent,
        MarkerType markerType, RouteStopFlags stopFlags )
        : QGraphicsWidget(parent)
{
    m_hoverStep = 0.0;
    m_markerType = markerType;
    m_stopFlags = stopFlags;
    setAcceptHoverEvents( true );

    QPalette p = palette();
#if KDE_VERSION < KDE_MAKE_VERSION(4,6,0)
    p.setColor( QPalette::Active, QPalette::Background,
                Plasma::Theme::defaultTheme()->color(Plasma::Theme::BackgroundColor) );
#else
    p.setColor( QPalette::Active, QPalette::Background,
                Plasma::Theme::defaultTheme()->color(Plasma::Theme::ViewBackgroundColor) );
#endif
    setPalette( p );

    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect;
    shadowEffect->setBlurRadius( 8.0 );
    shadowEffect->setOffset( 1.0 );
    setGraphicsEffect( shadowEffect );
}

void RouteStopMarkerGraphicsItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    QGraphicsItem::hoverEnterEvent(event);
    hover();
    emit hovered( this );
}

void RouteStopMarkerGraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    QGraphicsWidget::hoverLeaveEvent(event);
    unhover();
    emit unhovered( this );
}

void RouteStopMarkerGraphicsItem::hover()
{
    setZValue( 1.0 );
    QPropertyAnimation *hoverAnimation = new QPropertyAnimation( this, "hoverStep" );
    hoverAnimation->setEasingCurve( QEasingCurve(QEasingCurve::OutCubic) );
    hoverAnimation->setStartValue( hoverStep() );
    hoverAnimation->setEndValue( 1.0 );
    hoverAnimation->start( QAbstractAnimation::DeleteWhenStopped );
}

void RouteStopMarkerGraphicsItem::unhover()
{
    setZValue( 0.0 );
    QPropertyAnimation *hoverAnimation = new QPropertyAnimation( this, "hoverStep" );
    hoverAnimation->setEasingCurve( QEasingCurve(QEasingCurve::InOutCubic) );
    hoverAnimation->setStartValue( hoverStep() );
    hoverAnimation->setEndValue( 0.0 );
    hoverAnimation->start( QAbstractAnimation::DeleteWhenStopped );
}

void RouteStopMarkerGraphicsItem::setHoverStep( qreal hoverStep )
{
#if KDE_VERSION < KDE_MAKE_VERSION(4,6,0)
    QColor normalColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::BackgroundColor );
    QColor hoverColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::HighlightColor );
#else
    QColor normalColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::ViewBackgroundColor );
    QColor hoverColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::ViewHoverColor );
#endif
    QColor currentColor = KColorUtils::mix( normalColor, hoverColor, hoverStep );
    QPalette p = palette();
    p.setColor( QPalette::Active, QPalette::Background, currentColor );
    setPalette( p );

    m_hoverStep = hoverStep;
    updateGeometry();
}

qreal RouteStopMarkerGraphicsItem::radius() const
{
    if ( m_markerType == IntermediateStopMarker ) {
        return 12.0 + 2.0 * m_hoverStep;
    } else {
        if ( m_stopFlags.testFlag(RouteStopIsHomeStop) ) {
            return 7.5 + 2.0 * m_hoverStep;
        } else {
            return 6.0 + 2.0 * m_hoverStep;
        }
    }
}

void RouteStopMarkerGraphicsItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                         QWidget* widget )
{
    Q_UNUSED( widget );
    painter->setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );

    KIcon stopIcon( m_markerType == DefaultStopMarker
            ? (m_stopFlags.testFlag(RouteStopIsHomeStop) ? "go-home" : "public-transport-stop")
            : "public-transport-intermediate-stops" );
    stopIcon.paint( painter, option->rect );
}

RouteStopTextGraphicsItem::RouteStopTextGraphicsItem( QGraphicsItem* parent, const QFont &font,
        qreal baseSize, const QTime &time, const QString &stopName, int minsFromFirstRouteStop )
        : QGraphicsWidget(parent)
{
    m_expandStep = 0.0;
    m_baseSize = baseSize;
    setFont( font );
    setStop( time, stopName, minsFromFirstRouteStop );
    setAcceptHoverEvents( true );
}

void RouteStopTextGraphicsItem::setStop( const QTime &time, const QString &stopName,
                                         int minsFromFirstRouteStop )
{
    m_stopName = stopName;
    m_stopText = minsFromFirstRouteStop < 0
            ? stopName : QString("%1: %2").arg(minsFromFirstRouteStop).arg(stopName);

    qreal maxSize = QFontMetrics( font() ).width( m_stopText ) + 5;
    if ( maxSize > m_baseSize ) {
        if ( time.isValid() ) {
            setToolTip( QString("%1: %2").arg(KGlobal::locale()->formatTime(time))
                                         .arg(stopName) );
        } else {
            setToolTip( stopName );
        }
    } else {
        setToolTip( QString() );
    }
}

void RouteStopTextGraphicsItem::hover()
{
    setZValue( 1.0 );
    QPropertyAnimation *expandAnimation = new QPropertyAnimation( this, "expandStep" );
    expandAnimation->setEasingCurve( QEasingCurve(QEasingCurve::OutCubic) );
    expandAnimation->setStartValue( expandStep() );
    expandAnimation->setEndValue( 1.0 );
    expandAnimation->start( QAbstractAnimation::DeleteWhenStopped );
}

void RouteStopTextGraphicsItem::unhover()
{
    setZValue( 0.0 );
    QPropertyAnimation *expandAnimation = new QPropertyAnimation( this, "expandStep" );
    expandAnimation->setEasingCurve( QEasingCurve(QEasingCurve::InOutCubic) );
    expandAnimation->setStartValue( expandStep() );
    expandAnimation->setEndValue( 0.0 );
    expandAnimation->start( QAbstractAnimation::DeleteWhenStopped );
}

void RouteStopTextGraphicsItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    QGraphicsItem::hoverEnterEvent(event);
    hover();
    emit hovered( this );
}

void RouteStopTextGraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    QGraphicsWidget::hoverLeaveEvent(event);
    unhover();
    emit unhovered( this );
}

void RouteStopTextGraphicsItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    QList<QAction*> actionList = actions();
    if ( actionList.isEmpty() ) {
        return; // Don't show an empty menu if there are no actions to show
    }

    for ( int i = 0; i < actionList.count(); ++i ) {
        StopAction *action = qobject_cast<StopAction*>( actionList[i] );
        action->setStopName( m_stopName );

        if ( action->type() == StopAction::HighlightStop ) {
            // Update text of the highlight stop action
            RouteGraphicsItem *routeItem = qgraphicsitem_cast<RouteGraphicsItem*>( parentItem() );
            DepartureModel *model = !routeItem ? NULL :
                    qobject_cast<DepartureModel*>( routeItem->item()->model() );
            QString highlightStopActionText =
                    (!model || !model->routeItemFlags(m_stopName).testFlag(RouteItemHighlighted))
		    ? i18nc("@action:inmenu", "&Highlight This Stop")
		    : i18nc("@action:inmenu", "&Unhighlight This Stop");
            action->setText( highlightStopActionText );
        }
    }

    KMenu contextMenu;
    contextMenu.addTitle( KIcon("public-transport-stop"), m_stopName );
    contextMenu.addActions( actionList );
    contextMenu.exec( event->screenPos() );
}

void RouteStopTextGraphicsItem::setExpandStep( qreal expandStep )
{
    qreal maxSize = QFontMetrics( font() ).width( m_stopText ) + 5;
    if ( m_baseSize < maxSize ) {
        resize( m_baseSize + (maxSize - m_baseSize) * expandStep, size().height() );
    }

    QColor normalColor = palette().color( QPalette::Active, QPalette::ButtonText );
#if KDE_VERSION < KDE_MAKE_VERSION(4,6,0)
    QColor hoverColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::HighlightColor );
#else
    QColor hoverColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::ViewHoverColor );
#endif
    QColor currentColor = KColorUtils::mix( normalColor, hoverColor, expandStep / 2.0 );
    QPalette p = palette();
    p.setColor( QPalette::Active, QPalette::Text, currentColor );
    setPalette( p );

    m_expandStep = expandStep;
}

void RouteStopTextGraphicsItem::setBaseSize( qreal baseSize )
{
    m_baseSize = baseSize + 10;
}

void RouteStopTextGraphicsItem::paint( QPainter* painter,
        const QStyleOptionGraphicsItem* option, QWidget* widget )
{
    Q_UNUSED( widget );
    QColor textColor = palette().color( QPalette::Active, QPalette::Text );
    bool drawHalos = /*m_options.testFlag(DrawShadows) &&*/ qGray(textColor.rgb()) < 192;

    QFontMetrics fm( font() );
    painter->setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );
    painter->setFont( font() );

    QRect rect = option->rect;
    rect.setTop( 0 );
    QString stopText = fm.elidedText( m_stopText, Qt::ElideRight, rect.width() );

    QPixmap pixmap( rect.size() );
    pixmap.fill( Qt::transparent );
    QPainter p( &pixmap );
    p.setRenderHints( QPainter::Antialiasing );
    p.setBrush( textColor ); // Set text color as brush, because it's filled using a QPainterPath
    p.setPen( Qt::NoPen ); // No text outline
    if ( drawHalos ) {
        Plasma::PaintUtils::drawHalo( &p, QRectF(rect.left(), rect.top(),//-fm.ascent(), TODO Move f.ascent to setPos()?
                                      fm.width(stopText), fm.height()) );
    }

    // Use a QPainterPath to draw the text, because it's better antialiased then
    QPainterPath path;
    path.addText( 0, fm.ascent(), font(), stopText );
    p.drawPath( path );
    p.end();

    if ( !drawHalos ) {
        // Create and draw a shadow
        QImage shadow = pixmap.toImage();
        Plasma::PaintUtils::shadowBlur( shadow, 3, Qt::black );
        painter->drawImage( rect.topLeft() + QPoint(1, 2), shadow );
    }

    // Draw the route pixmap
    painter->drawPixmap( rect.topLeft(), pixmap );
}

JourneyRouteStopGraphicsItem::JourneyRouteStopGraphicsItem( JourneyRouteGraphicsItem* parent,
    const QPixmap &vehiclePixmap, const QString &text, RouteStopFlags routeStopFlags, const QString &stopName )
    : QGraphicsWidget(parent), m_parent(parent), m_infoTextDocument(0)
{
    m_vehiclePixmap = vehiclePixmap;
    m_stopFlags = routeStopFlags;
    m_stopName = stopName;
    setText( text );
    setAcceptHoverEvents( true );
}

void JourneyRouteStopGraphicsItem::setText( const QString& text )
{
    delete m_infoTextDocument;
    m_infoTextDocument = NULL;

    QTextOption textOption( Qt::AlignVCenter | Qt::AlignLeft );
    m_infoTextDocument = TextDocumentHelper::createTextDocument( text, infoTextRect().size(),
                                                                 textOption, font() );

    updateGeometry();
    update();
}

void JourneyRouteStopGraphicsItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    QList<QAction*> actionList = actions();
    if ( actionList.isEmpty() ) {
        return; // Don't show an empty menu if there are no actions to show
    }
    for ( int i = 0; i < actionList.count(); ++i ) {
        StopAction *action = qobject_cast<StopAction*>( actionList[i] );
        action->setStopName( m_stopName );
    }

    KMenu contextMenu;
    contextMenu.addTitle( KIcon("public-transport-stop"), m_stopName );
    contextMenu.addActions( actionList );
    contextMenu.exec( event->screenPos() );
}

QSizeF JourneyRouteStopGraphicsItem::sizeHint( Qt::SizeHint which, const QSizeF& constraint ) const
{
    if ( which == Qt::MinimumSize || which == Qt::MaximumSize ) {
        const qreal marginLeft = 32.0; // TODO
        return QSizeF( marginLeft + TextDocumentHelper::textDocumentWidth(m_infoTextDocument),
                    m_infoTextDocument->size().height() + 5 );
    } else {
        return QGraphicsWidget::sizeHint(which, constraint);
    }
}

QRectF JourneyRouteStopGraphicsItem::infoTextRect() const
{
    const qreal marginLeft = 32.0; // TODO
    return contentsRect().adjusted( marginLeft, 0.0, 0.0, 0.0 );
}

void JourneyRouteStopGraphicsItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                        QWidget* widget )
{
    Q_UNUSED( widget );

    if ( option->state.testFlag(QStyle::State_MouseOver) ) {
        #if KDE_VERSION < KDE_MAKE_VERSION(4,6,0)
            QColor hoverColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::HighlightColor );
        #else
            QColor hoverColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::ViewHoverColor );
        #endif

        // Draw hover background
        QLinearGradient bgGradient( 0, 0, 1, 0 );
        bgGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
        bgGradient.setColorAt( 0, Qt::transparent );
        bgGradient.setColorAt( 0.4, hoverColor );
        bgGradient.setColorAt( 0.6, hoverColor );
        bgGradient.setColorAt( 1, Qt::transparent );

        painter->fillRect( option->rect, QBrush(bgGradient) );
    }

#if KDE_VERSION < KDE_MAKE_VERSION(4,6,0)
    QColor textColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor );
#else
    QColor textColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::ViewTextColor );
#endif
    bool drawHalos = /*m_options.testFlag(DrawShadows) &&*/ qGray(textColor.rgb()) < 192;
    QRectF textRect = infoTextRect();
    TextDocumentHelper::drawTextDocument( painter, option, m_infoTextDocument,
                                        textRect.toRect(), drawHalos );
}

JourneyRouteGraphicsItem::JourneyRouteGraphicsItem( QGraphicsItem* parent, JourneyItem* item,
        Plasma::Svg* svg, StopAction *copyStopToClipboardAction,
        StopAction *requestJourneyToStopAction, StopAction *requestJourneyFromStopAction )
        : QGraphicsWidget(parent), m_item(item), m_svg(svg),
          m_copyStopToClipboardAction(copyStopToClipboardAction),
          m_requestJourneyToStopAction(requestJourneyToStopAction),
          m_requestJourneyFromStopAction(requestJourneyFromStopAction)
{
    m_zoomFactor = 1.0;
    new QGraphicsLinearLayout( Qt::Vertical, this );
    updateData( item );
}

void JourneyRouteGraphicsItem::updateData( JourneyItem* item )
{
    if ( rect().isEmpty() ) {
        kDebug() << "Empty rect for the JourneyRouteGraphicsItem";
        return;
    }
    m_item = item;
    const JourneyInfo *info = m_item->journeyInfo();

    // First remove all old RouteStopGraphicsItems
    foreach ( JourneyRouteStopGraphicsItem *item, m_routeItems ) {
        item->deleteLater();
    }
    m_routeItems.clear();

    // Add route stops if there are at least two stops given from the data engine
    QGraphicsLinearLayout *l = static_cast<QGraphicsLinearLayout*>( layout() );
    if ( info->routeStops().count() >= 2 ) {
        QFont routeFont = Plasma::Theme::defaultTheme()->font( Plasma::Theme::DefaultFont );
        routeFont.setPointSizeF( routeFont.pointSizeF() * m_zoomFactor );
        QFont boldRouteFont = routeFont;
        boldRouteFont.setBold( true );
        QFontMetrics fm( routeFont );
        QFontMetrics fmBold( boldRouteFont );
        const QRectF routeRect = rect();

        // Add the route stop items (JourneyRouteStopGraphicsItem)
        for ( int i = 0; i < info->routeStops().count(); ++i ) {
            QFontMetrics *fontMetrics;
            QFont *font;
            if ( i == 0 || i == info->routeStops().count() - 1 ) {
                font = &boldRouteFont;
                fontMetrics = &fmBold;
            } else {
                font = &routeFont;
                fontMetrics = &fm;
            }

            const QString stopName = info->routeStops()[i];
            QString text = QString( "<b>%1</b>" ).arg( stopName );

            // Prepend departure time at the current stop, if a time is given
            if ( i - 1 >= 0 && i - 1 < info->routeTimesArrival().count()
                && info->routeTimesArrival()[i - 1].isValid() )
            {
                text = text.append( i18nc("@info Info string for a stop in a journey shown in "
                        "the route item", "<nl/>Arr.: <emphasis strong='1'>%1</emphasis>",
                        KGlobal::locale()->formatTime(info->routeTimesArrival()[i - 1])) );

                if ( i - 1 < info->routePlatformsArrival().count()
                    && !info->routePlatformsArrival()[i - 1].isEmpty() )
                {
                    text = text.append( i18nc("@info Info string for a stop in a journey shown in "
                            "the route item after the arrival time",
                            " at platform <emphasis strong='1'>%1</emphasis>",
                            info->routePlatformsArrival()[i - 1]) );
                    // TODO Delay
                }
            }
            if ( i < info->routeTimesDeparture().count() && info->routeTimesDeparture()[i].isValid() ) {
                // TODO Delay

                text = text.append( i18nc("@info Info string for a stop in a journey shown in "
                        "the route item", "<nl/>Dep.: <emphasis strong='1'>%1</emphasis>",
                        KGlobal::locale()->formatTime(info->routeTimesDeparture()[i])) );

                if ( i < info->routePlatformsDeparture().count()
                    && !info->routePlatformsDeparture()[i].isEmpty() )
                {
                    text = text.append( i18nc("@info Info string for a stop in a journey shown in "
                            "the route item after the departure time",
                            " from platform <emphasis strong='1'>%1</emphasis>",
                            info->routePlatformsDeparture()[i]) );
                    // TODO Delay
                }

                if ( i < info->routeTransportLines().count()
                    && !info->routeTransportLines()[i].isEmpty() )
                {
                    text = text.append( i18nc("@info Info string for a stop in a journey shown in "
                            "the route item after the departure time. %1 is one of the transport "
                            "lines used in the journey.",
                            " using <emphasis strong='1'>%1</emphasis>",
                            info->routeTransportLines()[i]) );
                    // TODO Delay
                }
            }

            RouteStopFlags routeStopFlags;
            if ( i == 0 ) {
                routeStopFlags |= RouteStopIsOrigin;
            } else if ( i == info->routeStops().count() - 1 ) {
                routeStopFlags |= RouteStopIsTarget;
            } else {
                routeStopFlags |= RouteStopIsIntermediate;
            }
            JourneyModel *model = qobject_cast<JourneyModel*>( m_item->model() );
            if ( model->info().homeStop == stopName ) {
                routeStopFlags |= RouteStopIsHomeStop;
            }
            JourneyRouteStopGraphicsItem *routeItem = new JourneyRouteStopGraphicsItem(
                    this, QPixmap(32, 32), text, routeStopFlags, info->routeStops()[i] );
            routeItem->setFont( *font );

            QList<QAction*> actionList;
            if ( !routeStopFlags.testFlag(RouteStopIsHomeStop) ) {
                if ( !routeStopFlags.testFlag(RouteStopIsTarget) ) {
                    routeItem->addAction( m_requestJourneyToStopAction );
                }
                if ( !routeStopFlags.testFlag(RouteStopIsOrigin) ) {
                    routeItem->addAction( m_requestJourneyFromStopAction );
                }
            }
            actionList << m_copyStopToClipboardAction;
            routeItem->addActions( actionList );

            m_routeItems << routeItem;
            l->addItem( routeItem );
        }
    }
}

void JourneyRouteGraphicsItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                      QWidget* widget )
{
    Q_UNUSED( option );
    Q_UNUSED( widget );

    if ( m_item.isNull() ) {
        // Item already deleted
        return;
    }

    painter->setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );

    const qreal marginLeft = 32.0; // TODO
    const qreal iconSize = 32.0;
    QRectF timelineRect = contentsRect();
    timelineRect.setWidth( marginLeft - 6 );
    timelineRect.setLeft( iconSize / 2.0 );
    const qreal routeLineWidth = 4.0 * m_zoomFactor;

    // Draw vertical timeline
#if KDE_VERSION < KDE_MAKE_VERSION(4,6,0)
    painter->setPen( Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor) );
    painter->setBrush( Plasma::Theme::defaultTheme()->color(Plasma::Theme::BackgroundColor) );
#else
    painter->setPen( Plasma::Theme::defaultTheme()->color(Plasma::Theme::ViewTextColor) );
    painter->setBrush( Plasma::Theme::defaultTheme()->color(Plasma::Theme::ViewBackgroundColor) );
#endif
    painter->drawRoundedRect( QRectF(timelineRect.left(), timelineRect.top() + padding(),
                                    routeLineWidth, timelineRect.height() - 2 * padding()),
                            routeLineWidth / 2.0, routeLineWidth / 2.0 );

    if ( m_routeItems.isEmpty() ) {
        return;
    }

    qreal stopRadius = 5.0;
    qreal lastY = -5.0;
    for ( int i = 0; i < m_routeItems.count() - 1; ++i ) {
        JourneyRouteStopGraphicsItem *routeItem = m_routeItems[i];
        qreal y = routeItem->pos().y() + routeItem->size().height();
        QRectF iconRect( timelineRect.left() + (routeLineWidth - iconSize) / 2.0,
                        y - iconSize / 2.0, iconSize, iconSize );
        QPointF stopPos( timelineRect.left() + routeLineWidth / 2.0,
                        lastY + (y - lastY) / 2.0 + 1.0 );

        // Draw lines to connect to the stop text
        qreal lineWidth = iconSize / 2.0;
        qreal lineHeight = routeItem->size().height() / 3.0;
        painter->drawLine( stopPos.x(), stopPos.y(), stopPos.x() + lineWidth, stopPos.y() );
        painter->drawLine( stopPos.x() + lineWidth, stopPos.y() - lineHeight,
                        stopPos.x() + lineWidth, stopPos.y() + lineHeight );

        // Draw the stop
        KIcon("public-transport-stop").paint( painter,
                                              stopPos.x() - stopRadius, stopPos.y() - stopRadius,
                                              2.0 * stopRadius, 2.0 * stopRadius );
        lastY = y;

        if ( i < m_item->journeyInfo()->routeVehicleTypes().count() ) {
            QString vehicleKey;
            switch ( m_item->journeyInfo()->routeVehicleTypes()[i] ) {
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
                    kDebug() << "Unknown vehicle type" << m_item->journeyInfo()->routeVehicleTypes()[i];
                    painter->drawEllipse( iconRect.adjusted(5, 5, -5, -5) );
                    painter->drawText( iconRect, "?", QTextOption(Qt::AlignCenter) );
                    continue;
            }
        // 	if ( drawTransportLine ) {
        // 		vehicleKey.append( "_empty" );
        // 	}

            if ( !m_svg->hasElement(vehicleKey) ) {
                kDebug() << "SVG element" << vehicleKey << "not found";
            } else {
                // Draw SVG vehicle element into pixmap
                int shadowWidth = 4;
                QPixmap pixmap( (int)iconRect.width(), (int)iconRect.height() );
                pixmap.fill( Qt::transparent );
                QPainter p( &pixmap );
                m_svg->resize( iconRect.width() - 2 * shadowWidth, iconRect.height() - 2 * shadowWidth );
                m_svg->paint( &p, shadowWidth, shadowWidth, vehicleKey );

                // Create shadow for the SVG element and draw the SVG and it's shadow.
                QImage shadow = pixmap.toImage();
                Plasma::PaintUtils::shadowBlur( shadow, shadowWidth - 1, Qt::black );
                painter->drawImage( iconRect.topLeft() + QPoint(1, 2), shadow );
                painter->drawPixmap( iconRect.topLeft(), pixmap );
            }
        }
    }

    // Draw last stop marker
    JourneyRouteStopGraphicsItem *routeItem = m_routeItems.last();
    QPointF stopPos( timelineRect.left() + routeLineWidth / 2.0,
                    lastY + (timelineRect.bottom() - lastY) / 2.0 + 1.0 );
    qreal lineWidth = iconSize / 2.0;
    qreal lineHeight = routeItem->size().height() / 3.0;
    painter->drawLine( stopPos.x(), stopPos.y(), stopPos.x() + lineWidth, stopPos.y() );
    painter->drawLine( stopPos.x() + lineWidth, stopPos.y() - lineHeight,
                        stopPos.x() + lineWidth, stopPos.y() + lineHeight );
    KIcon("public-transport-stop").paint( painter,
                                        stopPos.x() - stopRadius, stopPos.y() - stopRadius,
                                        2.0 * stopRadius, 2.0 * stopRadius );
}
