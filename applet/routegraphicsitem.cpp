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

// Own includes
#include "routegraphicsitem.h"
#include "departuremodel.h"
#include "timetablewidget.h"

// libpublictransport helper includes
#include <departureinfo.h>

// Plasma/KDE includes
#include <Plasma/PaintUtils>
#include <Plasma/Svg>
#include <KColorUtils>
#include <KGlobalSettings>
#include <KMenu>
#include <KAction>

// Qt includes
#include <QPainter>
#include <QPropertyAnimation>
#include <QGraphicsLinearLayout>
#include <QTextDocument>
#include <QGraphicsEffect>
#include <QGraphicsSceneHoverEvent>
#include <QStyleOption>
#include <qmath.h>

RouteGraphicsItem::RouteGraphicsItem( QGraphicsItem* parent, DepartureItem *item,
        StopAction *copyStopToClipboardAction, StopAction *showInMapAction,
        StopAction *showDeparturesAction, StopAction *highlightStopAction,
        StopAction *newFilterViaStopAction )
        : QGraphicsWidget(parent), m_item(item),
        m_copyStopToClipboardAction(copyStopToClipboardAction), m_showInMapAction(showInMapAction),
        m_showDeparturesAction(showDeparturesAction), m_highlightStopAction(highlightStopAction),
        m_newFilterViaStopAction(newFilterViaStopAction)
{
    setFlag( ItemClipsToShape );
    m_zoomFactor = 1.0;
    m_textAngle = 15.0;
    m_maxTextWidth = 100.0;
    updateData( item );
}

void RouteGraphicsItem::resizeEvent(QGraphicsSceneResizeEvent* event)
{
    QGraphicsWidget::resizeEvent(event);
    if ( !isVisible() ) {
        // Don't rearrange if the route item isn't visible (TODO rearrange if shown?)
        return;
    }
    arrangeStopItems();
}

void RouteGraphicsItem::setZoomFactor( qreal zoomFactor )
{
    m_zoomFactor = zoomFactor;
    arrangeStopItems();
    updateGeometry();
    update();
}

void JourneyRouteStopGraphicsItem::setZoomFactor( qreal zoomFactor )
{
    m_zoomFactor = zoomFactor;
    update();
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
        const qreal smallestReadableFontSize = routeFont.pointSizeF();
        const qreal targetFontSize = smallestReadableFontSize * m_zoomFactor;
        if ( targetFontSize >= smallestReadableFontSize ) {
            routeFont = parentWidget()->font();
        }
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
        const qreal height = routeRect.height() - startStopPos.y();
        const qreal angle = m_textAngle * 3.14159 / 180.0;
        m_maxTextWidth = height / qSin(angle) - fm.height() / qTan(angle);

        for ( int i = 0; i < count; ++i ) {
            const QPointF stopMarkerPos( startStopPos.x() + i * step, startStopPos.y() );
            const QPointF stopTextPos( stopMarkerPos.x() - 4 * m_zoomFactor,
                                 stopMarkerPos.y() + 6.0 * m_zoomFactor );
            const QString stopName = info->routeStops()[i];
            const QString stopNameShortened = info->routeStopsShortened()[i];
            QFontMetrics *fontMetrics;
            QFont *font;
            if ( i == 0 || i == count - 1 ) {
                font = &boldRouteFont;
                fontMetrics = &fmBold;
            } else {
                font = &routeFont;
                fontMetrics = &fm;
            }

            // Get time information
            QTime time;
            int minsFromFirstRouteStop = -1;
            if ( i < info->routeTimes().count() && info->routeTimes()[i].isValid() ) {
                time = info->routeTimes()[i];
                minsFromFirstRouteStop = qCeil( info->departure().time().secsTo(time) / 60.0 );

                // Fix number of minutes if the date changes between route stops
                // NOTE This only works if the route extends over less than three days
                if ( info->isArrival() ) {
                    // Number of minutes should always be negative for arrivals
                    // (time from home stop back in time to stop X)
                    while ( minsFromFirstRouteStop > 0 ) {
                        minsFromFirstRouteStop -= 24 * 60;
                    }
                } else {
                    // Number of minutes should always be positive for departures
                    // (time from home stop to stop X)
                    while ( minsFromFirstRouteStop < 0 ) {
                        minsFromFirstRouteStop += 24 * 60;
                    }
                }
            }

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
            textItem->setStop( time, stopName, stopNameShortened, minsFromFirstRouteStop );
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
//         kDebug() << "Empty rect for the RouteGraphicsItem";
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
        const qreal smallestReadableFontSize = routeFont.pointSizeF();
        const qreal targetFontSize = smallestReadableFontSize * m_zoomFactor;
        if ( targetFontSize >= smallestReadableFontSize ) {
            routeFont = parentWidget()->font();
        }
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
            int index = -1;
            if ( positionIndex == omitIndex ) { // Currently at first omitted stop
                // Create intermediate marker item
                RouteStopMarkerGraphicsItem *markerItem = new RouteStopMarkerGraphicsItem(
                        this, 0, RouteStopMarkerGraphicsItem::IntermediateStopMarker );
                markerItem->setPos( stopMarkerPos );
                m_markerItems << markerItem;

                // Create a list with all omitted stops (and times if available)
                // to be used for the tooltip of the intermediate marker item
                QStringList omittedStopList;
                for ( int omittedIndex = omitIndex;
                      omittedIndex <= omitIndex + omitCount; ++omittedIndex )
                {
                    QString stopText = info->routeStopsShortened()[omittedIndex];

                    // Prepend departure time at the current stop, if a time is given
                    const QTime time = omittedIndex < info->routeTimes().count()
                            ? info->routeTimes()[omittedIndex] : QTime();
                    if ( time.isValid() ) {
                        stopText.prepend( KGlobal::locale()->formatTime(time) + ": " );
                    } else {
                        kDebug() << "Invalid QTime in RouteTimes at index" << index;
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
            QString stopNameShortened = info->routeStopsShortened()[index];
            QString stopText = stopNameShortened;
            QFontMetrics *fontMetrics;
            QFont *font;

            const bool manuallyHighlighted =
                    model->routeItemFlags(stopName).testFlag(RouteItemHighlighted);
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
            const QTime time = index < info->routeTimes().count()
                    ? info->routeTimes()[index] : QTime();
            if ( time.isValid() ) {
                stopText.prepend( KGlobal::locale()->formatTime(time) + ": " );
            } else {
                kDebug() << "Invalid QTime in RouteTimes at index" << index;
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

            // Get route flags
            int minsFromFirstRouteStop = -1;
            RouteStopFlags routeStopFlags = item->routeStopFlags( index, &minsFromFirstRouteStop );

            // Create text item, that displays a single stop name
            // and automatically elides and stretches it on hover to show hidden text
            RouteStopTextGraphicsItem *textItem = new RouteStopTextGraphicsItem(
                    this, model, *font, baseSize, time, stopName, stopNameShortened,
                    minsFromFirstRouteStop, routeStopFlags );
            textItem->setPos( stopTextPos );
            textItem->resize( baseSize + 10, fontMetrics->height() );
            textItem->rotate( m_textAngle );
            QList<QAction*> actions;
            if ( routeStopFlags.testFlag(RouteStopIsHomeStop) ) {
                if ( m_showInMapAction ) {
                    actions << m_showInMapAction;
                }
                actions << m_copyStopToClipboardAction;
            } else {
                actions << m_showDeparturesAction;
                if ( m_showInMapAction ) {
                    actions << m_showInMapAction;
                }
                actions << m_highlightStopAction << m_newFilterViaStopAction
                        << m_copyStopToClipboardAction;
            }
            textItem->addActions( actions );
            if ( manuallyHighlighted ) {
                textItem->setPalette( manuallyHighlighted
                        ? highlightedPalette : defaultPalette );
            }
            m_textItems << textItem;

            // Create marker item
            RouteStopMarkerGraphicsItem *markerItem = new RouteStopMarkerGraphicsItem( this,
                    textItem, RouteStopMarkerGraphicsItem::DefaultStopMarker, routeStopFlags );
            markerItem->setPos( stopMarkerPos );
            m_markerItems << markerItem;

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
    const qreal routeLineWidth = 4.0 * m_zoomFactor;

    // Draw horizontal timeline
#if KDE_VERSION < KDE_MAKE_VERSION(4,6,0)
    QColor backgroundColor = Plasma::Theme::defaultTheme()->color(Plasma::Theme::BackgroundColor);
    painter->setPen( Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor) );
#else
    QColor backgroundColor = Plasma::Theme::defaultTheme()->color(Plasma::Theme::ViewBackgroundColor);
    painter->setPen( Plasma::Theme::defaultTheme()->color(Plasma::Theme::ViewTextColor) );
#endif
    QColor backgroundFadeColor = backgroundColor;
    backgroundFadeColor.setAlphaF( 0.5 );
    QLinearGradient backgroundGradient( 0, 0, 1, 0 );
    backgroundGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
    backgroundGradient.setColorAt( 0, backgroundColor );
    backgroundGradient.setColorAt( 1,  backgroundFadeColor );
    painter->setBrush( backgroundGradient );
    const qreal arrowWidth = routeLineWidth * 2.5;
    const qreal arrowHeight = routeLineWidth * 1.0;
    const qreal timelineTop = routeRect.top() + padding();
    const qreal timelineBottom = timelineTop + routeLineWidth;
    const qreal timelineLeft = routeRect.left() + routeLineWidth * 3.0;
    const qreal timelineRight = (m_markerItems.isEmpty() ? routeRect.right()
            : m_markerItems.last()->pos().x() - m_markerItems.last()->size().width() / 2.0) - arrowWidth;
    const QPointF points[7] = {
            QPointF(timelineLeft, timelineBottom),
            QPointF(timelineLeft, timelineTop),
            QPointF(timelineRight, timelineTop),
            QPointF(timelineRight, timelineTop - arrowHeight),
            QPointF(timelineRight + arrowWidth, timelineTop + routeLineWidth / 2.0),
            QPointF(timelineRight, timelineBottom + arrowHeight),
            QPointF(timelineRight, timelineBottom) };
    painter->drawConvexPolygon( points, 7 );
}

RouteStopMarkerGraphicsItem::RouteStopMarkerGraphicsItem( QGraphicsItem* parent,
        RouteStopTextGraphicsItem *textItem, MarkerType markerType, RouteStopFlags stopFlags )
        : QGraphicsWidget(parent), m_textItem(textItem)
{
    setFlag( ItemClipsToShape );
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
    RouteGraphicsItem *routeItem = qgraphicsitem_cast<RouteGraphicsItem*>( parentItem() );
    qreal zoomFactor = routeItem->zoomFactor();
    if ( m_markerType == IntermediateStopMarker ) {
        return (12.0 + 2.0 * m_hoverStep) * zoomFactor;
    } else {
        RouteStopFlags stopFlags = routeStopFlags();
        if ( stopFlags.testFlag(RouteStopIsHighlighted) ) {
            return (7.5 + 2.0 * m_hoverStep) * zoomFactor;
        } else if ( stopFlags.testFlag(RouteStopIsHomeStop) ) {
            return (7.5 + 2.0 * m_hoverStep) * zoomFactor;
        } else if ( stopFlags.testFlag(RouteStopIsOrigin) ) {
            return (7.5 + 2.0 * m_hoverStep) * zoomFactor;
        } else if ( stopFlags.testFlag(RouteStopIsTarget) ) {
            return (7.5 + 2.0 * m_hoverStep) * zoomFactor;
        } else {
            return (6.0 + 2.0 * m_hoverStep) * zoomFactor;
        }
    }
}

void RouteStopMarkerGraphicsItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                         QWidget* widget )
{
    Q_UNUSED( option );
    Q_UNUSED( widget );
    painter->setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );

    KIcon stopIcon;
    if ( m_markerType == IntermediateStopMarker ) {
        stopIcon = KIcon( "public-transport-intermediate-stops" );
    } else {
        stopIcon = GlobalApplet::stopIcon( routeStopFlags() );
    }
    stopIcon.paint( painter, QRect(-radius(), -radius(), 2 * radius(), 2 * radius()) );
}

RouteStopFlags RouteStopMarkerGraphicsItem::routeStopFlags() const
{
    return m_textItem->routeStopFlags();
}

RouteStopFlags RouteStopTextGraphicsItem::routeStopFlags() const
{
    RouteStopFlags stopFlags = m_stopFlags;
    if ( !m_model ) {
        return stopFlags;
    }

    const RouteItemFlags itemFlags = m_model->routeItemFlags( m_stopName );
    if ( itemFlags.testFlag(RouteItemHighlighted) ) {
        stopFlags |= RouteStopIsHighlighted;
    }
    if ( itemFlags.testFlag(RouteItemHomeStop) ) {
        stopFlags |= RouteStopIsHomeStop;
    }
    return stopFlags;
}

RouteStopFlags JourneyRouteStopGraphicsItem::routeStopFlags() const
{
    RouteStopFlags stopFlags = m_stopFlags;
    RouteGraphicsItem *routeItem = qgraphicsitem_cast<RouteGraphicsItem*>( parentItem() );
    JourneyModel *model = !routeItem || !routeItem->item() ? 0 :
            qobject_cast<JourneyModel*>( routeItem->item()->model() );
    RouteItemFlags itemFlags = model ? model->routeItemFlags(m_stopName) : RouteItemDefault;
    if ( itemFlags.testFlag(RouteItemHighlighted) ) {
        stopFlags |= RouteStopIsHighlighted;
    }
    if ( itemFlags.testFlag(RouteItemHomeStop) ) {
        stopFlags |= RouteStopIsHomeStop;
    }
    return stopFlags;
}

RouteStopTextGraphicsItem::RouteStopTextGraphicsItem( QGraphicsItem* parent, DepartureModel *model,
        const QFont &font, qreal baseSize, const QTime &time,
        const QString &stopName, const QString &stopNameShortened,
        int minsFromFirstRouteStop, RouteStopFlags routeStopFlags )
        : QGraphicsWidget(parent), m_model(model)
{
    m_expandStep = 0.0;
    m_baseSize = baseSize;
    m_stopFlags = routeStopFlags;
    setFont( font );
    setStop( time, stopName, stopNameShortened, minsFromFirstRouteStop );
    setAcceptHoverEvents( true );
}

void RouteStopTextGraphicsItem::setStop( const QTime &time, const QString &stopName,
                                         const QString &stopNameShortened,
                                         int minsFromFirstRouteStop )
{
    m_stopName = stopName;
    m_stopNameShortened = stopNameShortened;
    m_stopText = minsFromFirstRouteStop == 999999 || !time.isValid()
            ? stopName : QString("%1: %2").arg(minsFromFirstRouteStop).arg(stopNameShortened);

    qreal maxSize = QFontMetrics( font() ).width( m_stopText ) + 5;
    if ( maxSize > m_baseSize ) {
        if ( time.isValid() ) {
            setToolTip( QString("%1: %2").arg(KGlobal::locale()->formatTime(time))
                                         .arg(stopNameShortened) );
        } else {
            setToolTip( stopNameShortened );
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
        action->setStopName( m_stopName, m_stopNameShortened );

        if ( action->type() == StopAction::HighlightStop ) {
            // Update text of the highlight stop action
            RouteGraphicsItem *routeItem = qgraphicsitem_cast<RouteGraphicsItem*>( parentItem() );
            DepartureModel *model = !routeItem || !routeItem->item() ? 0 :
                    qobject_cast<DepartureModel*>( routeItem->item()->model() );
            QString highlightStopActionText =
                    (!model || !model->routeItemFlags(m_stopName).testFlag(RouteItemHighlighted))
                    ? i18nc("@action:inmenu", "&Highlight This Stop")
                    : i18nc("@action:inmenu", "&Unhighlight This Stop");
            action->setText( highlightStopActionText );
        }
    }

    KMenu contextMenu;
    contextMenu.addTitle( GlobalApplet::stopIcon(routeStopFlags()), m_stopNameShortened );
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

    // Get the departure graphics item (parent of this is RouteGraphicsItem,
    // parent of RouteGraphicsItem is DepartureGraphicsItem)
    // to get access to the textColor() function
    DepartureGraphicsItem *departureItem =
            qgraphicsitem_cast<DepartureGraphicsItem*>( parentWidget()->parentWidget() );
    QColor textColor = departureItem->textColor();
    const bool drawShadowsOrHalos = departureItem->publicTransportWidget()->isOptionEnabled(
            PublicTransportWidget::DrawShadowsOrHalos );
    const bool drawHalos = drawShadowsOrHalos && qGray(textColor.rgb()) < 192;

    QFontMetrics fm( font() );
    painter->setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );
    painter->setFont( font() );

    QRect rect = option->rect;
    rect.setTop( 0 );
    QString stopText = fm.elidedText( m_stopText, Qt::ElideRight, rect.width() );

    // Prepare a pixmap and a painter drawing to that pixmap
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

    if ( !drawHalos && drawShadowsOrHalos ) {
        // Create and draw a shadow
        QImage shadow = pixmap.toImage();
        Plasma::PaintUtils::shadowBlur( shadow, 3, Qt::black );
        painter->drawImage( rect.topLeft() + QPoint(1, 2), shadow );
    }

    // Draw the route pixmap
    painter->drawPixmap( rect.topLeft(), pixmap );
}

JourneyRouteStopGraphicsItem::JourneyRouteStopGraphicsItem( JourneyRouteGraphicsItem* parent,
    const QPixmap &vehiclePixmap, const QString &text, RouteStopFlags routeStopFlags,
    const QString &stopName, const QString &stopNameShortened )
    : QGraphicsWidget(parent), m_parent(parent), m_infoTextDocument(0)
{
    m_zoomFactor = 1.0;
    m_vehiclePixmap = vehiclePixmap;
    m_stopFlags = routeStopFlags;
    m_stopName = stopName;
    m_stopNameShortened = stopNameShortened;
    setText( text );
    setAcceptHoverEvents( true );
}

JourneyRouteStopGraphicsItem::~JourneyRouteStopGraphicsItem()
{
    delete m_infoTextDocument;
}

void JourneyRouteStopGraphicsItem::setText( const QString& text )
{
    delete m_infoTextDocument;
    m_infoTextDocument = 0;

    QTextOption textOption( Qt::AlignVCenter | Qt::AlignLeft );
    m_infoTextDocument = TextDocumentHelper::createTextDocument( text, infoTextRect().size(),
                                                                 textOption, font() );

    updateGeometry();
    update();
}

void JourneyRouteStopGraphicsItem::contextMenuEvent( QGraphicsSceneContextMenuEvent *event )
{
    QList<QAction*> actionList = actions();
    if ( actionList.isEmpty() ) {
        return; // Don't show an empty menu if there are no actions to show
    }
    for ( int i = 0; i < actionList.count(); ++i ) {
        StopAction *action = qobject_cast<StopAction*>( actionList[i] );
        action->setStopName( m_stopName, m_stopNameShortened );
    }

    KAction *toggleIntermediateStopsAction = new KAction( m_parent->showIntermediateStops()
            ? i18nc("@info/plain", "&Hide intermediate stops")
            : i18nc("@info/plain", "&Show intermediate stops"),
            this );
    actionList << toggleIntermediateStopsAction;

    KMenu contextMenu;
    contextMenu.addTitle( GlobalApplet::stopIcon(routeStopFlags()), m_stopNameShortened );
    contextMenu.addActions( actionList );
    QAction *executeAction = contextMenu.exec( event->screenPos() );
    if ( executeAction == toggleIntermediateStopsAction ) {
        m_parent->setShowIntermediateStops( !m_parent->showIntermediateStops() );
    }
    delete toggleIntermediateStopsAction;
}

QSizeF JourneyRouteStopGraphicsItem::sizeHint( Qt::SizeHint which, const QSizeF& constraint ) const
{
    if ( which == Qt::MinimumSize || which == Qt::MaximumSize ) {
        const qreal marginLeft = 32.0 * m_zoomFactor;
        return QSizeF( marginLeft + TextDocumentHelper::textDocumentWidth(m_infoTextDocument),
                       m_infoTextDocument->size().height() + 5 * m_zoomFactor );
    } else {
        return QGraphicsWidget::sizeHint(which, constraint);
    }
}

QRectF JourneyRouteStopGraphicsItem::infoTextRect() const
{
    const qreal marginLeft = 32.0 * m_zoomFactor;
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

    // Draw text
    const JourneyGraphicsItem *journeyItem = qgraphicsitem_cast<JourneyGraphicsItem*>(
            qgraphicsitem_cast<JourneyRouteGraphicsItem*>(parentWidget())->parentWidget() );

#if KDE_VERSION < KDE_MAKE_VERSION(4,6,0)
    QColor textColor = journeyItem ? journeyItem->textColor
            : Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor );
#else
    QColor textColor = journeyItem ? journeyItem->textColor()
            : Plasma::Theme::defaultTheme()->color( Plasma::Theme::ViewTextColor );
#endif
    TextDocumentHelper::Option textOption;
    if ( !journeyItem->publicTransportWidget()->isOptionEnabled(
            PublicTransportWidget::DrawShadowsOrHalos) )
    {
        textOption = TextDocumentHelper::DoNotDrawShadowOrHalos;
    } else if ( qGray(textColor.rgb()) < 192 ) {
        textOption = TextDocumentHelper::DrawHalos;
    } else {
        textOption = TextDocumentHelper::DrawShadows;
    }
    QRectF textRect = infoTextRect();
    painter->setPen( textColor );
    TextDocumentHelper::drawTextDocument( painter, option, m_infoTextDocument,
                                          textRect.toRect(), textOption );
}

JourneyRouteGraphicsItem::JourneyRouteGraphicsItem( QGraphicsItem* parent, JourneyItem* item,
        Plasma::Svg* svg, StopAction *copyStopToClipboardAction, StopAction *showInMapAction,
        StopAction *requestJourneyToStopAction, StopAction *requestJourneyFromStopAction )
        : QGraphicsWidget(parent), m_item(item), m_svg(svg),
          m_copyStopToClipboardAction(copyStopToClipboardAction),
          m_showInMapAction(showInMapAction),
          m_requestJourneyToStopAction(requestJourneyToStopAction),
          m_requestJourneyFromStopAction(requestJourneyFromStopAction),
          m_showIntermediateStops(false)
{
    setFlag( ItemClipsChildrenToShape );
    m_zoomFactor = 1.0;
    new QGraphicsLinearLayout( Qt::Vertical, this );
    updateData( item );
}

void JourneyRouteGraphicsItem::setZoomFactor( qreal zoomFactor )
{
    m_zoomFactor = zoomFactor;
    updateData( m_item );
    updateGeometry();
}

void JourneyRouteGraphicsItem::updateData( JourneyItem* item )
{
    if ( rect().isEmpty() ) {
//         kDebug() << "Empty rect for the JourneyRouteGraphicsItem";
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
        QFont routeFont = font();
        routeFont.setPointSizeF( routeFont.pointSizeF() * m_zoomFactor );
        QFont boldRouteFont = routeFont;
        boldRouteFont.setBold( true );
        QFontMetrics fm( routeFont );
        QFontMetrics fmBold( boldRouteFont );

        // Add the route stop items (JourneyRouteStopGraphicsItem)
        for ( int i = 0; i < info->routeStops().count(); ++i ) {
            QFont *font = i == 0 || i == info->routeStops().count() - 1
                    ? &boldRouteFont : &routeFont;

            const RouteSubJourney subJourney =
                    i < info->routeSubJourneys().count() && m_showIntermediateStops
                    ? info->routeSubJourneys()[i] : RouteSubJourney();

            QStringList routePartStops = QStringList() << info->routeStops()[i];
            QStringList routePartStopsShortened = QStringList() << info->routeStopsShortened()[i];
            if ( i != info->routeStops().count() - 1 ) {
                routePartStops << subJourney.routeStops;
                routePartStopsShortened <<
                        (subJourney.routeStopsShortened.count() != subJourney.routeStops.count()
                        ? subJourney.routeStops : subJourney.routeStopsShortened);
            }

            QStringList routePlatformsArrival, routePlatformsDeparture, routeNews;
            QList<QTime> routeTimesArrival, routeTimesDeparture;
            QList<int> routeTimesArrivalDelay, routeTimesDepartureDelay;

            if ( i < info->routeTimesDeparture().count() &&
                 info->routeTimesDeparture()[i].isValid() )
            {
                // First add the subjourney departure time, then the intermediate departure times
                routeTimesDeparture << info->routeTimesDeparture()[i]
                                    << subJourney.routeTimesDeparture;
            }

            if ( i < info->routeTimesDepartureDelay().count() ) {
                routeTimesDepartureDelay << info->routeTimesDepartureDelay()[i]
                                         << subJourney.routeTimesDepartureDelay;
            }

            if ( i < info->routePlatformsDeparture().count() ) {
                routePlatformsDeparture << info->routePlatformsDeparture()[i]
                                        << subJourney.routePlatformsDeparture;
            }

            if ( i < info->routeNews().count() ) {
                routeNews << info->routeNews()[i] << subJourney.routeNews;
            }

            if ( i == 0 ) {
                routeTimesArrival << QTime();
                routeTimesArrivalDelay << -1;
                routePlatformsArrival << QString();
            } else {
                if( i - 1 < info->routeTimesArrival().count() &&
                    info->routeTimesArrival()[i - 1].isValid() )
                {
                    // First add the intermediate arrival times, then the subjourney arrival time
                    routeTimesArrival << info->routeTimesArrival()[i - 1];
                }

                if ( i - 1 < info->routeTimesArrivalDelay().count() ) {
                    routeTimesArrivalDelay << info->routeTimesArrivalDelay()[i - 1];
                }

                if ( i - 1 < info->routePlatformsArrival().count() ) {
                    routePlatformsArrival << info->routePlatformsArrival()[i - 1];
                }
            }
            routeTimesArrival << subJourney.routeTimesArrival;
            routeTimesArrivalDelay << subJourney.routeTimesArrivalDelay;
            routePlatformsArrival << subJourney.routePlatformsArrival;

            for ( int n = 0; n < routePartStops.count(); ++n ) {
                const QString stopName = routePartStops[n];
                const QString stopNameShortened = routePartStopsShortened[n];
                QString text = QString( "<b>%1</b>" ).arg( stopNameShortened );
                RouteStopFlags routeStopFlags = item->departureRouteStopFlags( i, n );

                // Prepend departure time at the current stop, if a time is given
                QTime departureTime = (n < routeTimesDeparture.count() && routeTimesDeparture[n].isValid())
                        ? routeTimesDeparture[n] : QTime();
                QTime arrivalTime = (n < routeTimesArrival.count() && routeTimesArrival[n].isValid())
                        ? routeTimesArrival[n] : QTime();
                if ( arrivalTime.isValid() && arrivalTime != departureTime ) {
                    QString timeString = KGlobal::locale()->formatTime(arrivalTime);
                    QString timeText( "<span style='font-weight:bold;'>" + timeString + "</span>" );
                    if ( n < routeTimesArrivalDelay.count() ) {
                        const int delay = routeTimesArrivalDelay[n];
                        if ( delay == 0 ) {
                            timeText.prepend( QString("<span style='color:%1;'>")
                                        .arg(Global::textColorOnSchedule().name()) )
                                    .append( QLatin1String("</span>") );
                        } else if ( delay > 0 ) {
                            timeText += ' ' + i18ncp( "@info/plain", "+%1 minute", "+%1 minutes", delay );
                            timeText.replace( QRegExp( "(+?\\s*\\d+)" ),
                                    QString( "%1 <span style='color:%2;'>+&nbsp;\\1</span>" )
                                    .arg(timeString).arg(Global::textColorDelayed().name()) );
                        }
                    }
                    text.append( "<br/>" + i18nc("@info", "Arrival:") + ' ' + timeText );

                        if ( n < routePlatformsArrival.count() &&
                             !routePlatformsArrival[n].isEmpty() )
                        {
                            text = text.append( i18nc("@info Info string for a stop in a journey shown in "
                                    "the route item after the arrival time",
                                    " at platform <emphasis strong='1'>%1</emphasis>",
                                    routePlatformsArrival[n]) );
                        }
                }
                if ( departureTime.isValid() ) {
                    QString timeString = KGlobal::locale()->formatTime( departureTime );
                    QString timeText( "<span style='font-weight:bold;'>" + timeString + "</span>" );
                    if ( n < routeTimesDepartureDelay.count() ) {
                        const int delay = routeTimesDepartureDelay[n];
                        if ( delay == 0 ) {
                            timeText.prepend( QString("<span style='color:%1;'>")
                                        .arg(Global::textColorOnSchedule().name()) )
                                    .append( QLatin1String("</span>") );
                        } else if ( delay > 0 ) {
                            timeText += ' ' + i18ncp( "@info/plain", "+%1 minute", "+%1 minutes", delay );
                            timeText.replace( QRegExp( "(+?\\s*\\d+)" ),
                                    QString( " <span style='color:%1;'>+&nbsp;\\1</span>" )
                                    .arg(Global::textColorDelayed().name()) );
                        }
                    }
                    text.append( "<br/>" + i18nc("@info", "Departure:") + ' ' + timeText );

                        if ( n < routePlatformsDeparture.count() &&
                             !routePlatformsDeparture[n].isEmpty() )
                        {
                            text = text.append( i18nc("@info Info string for a stop in a journey shown in "
                                    "the route item after the departure time",
                                    " from platform <emphasis strong='1'>%1</emphasis>",
                                    routePlatformsDeparture[n]) );
                        }

                    if ( routeStopFlags.testFlag(RouteStopIsConnectingStop) ||
                         !routeStopFlags.testFlag(RouteStopIsIntermediate) )
                    {
                        if ( i < info->routeTransportLines().count() &&
                            !info->routeTransportLines()[i].isEmpty() )
                        {
    //     TODO Show vehicle type and transport line only at the first stop of the journey part
                            text = text.append( "<br/>" + i18nc("@info Info string for a stop in a journey shown in "
                                    "the route item after the departure time. %1 is one of the transport "
                                    "lines used in the journey, %2 is the name of the used vehicle if "
                                    "available.", " using <emphasis strong='1'>%1%2</emphasis>",
                                    info->routeTransportLines()[i], i < info->routeVehicleTypes().count()
                                    ? QString(" (%1)").arg(Global::vehicleTypeToString(info->routeVehicleTypes()[i]))
                                    : QString()) );
                        }
                    }
                }
                if ( n < routeNews.count() && !routeNews[n].isEmpty() ) {
                    text.append( "<br/>" + i18nc("@info", "News:") + ' ' + routeNews[n] );
                }

                JourneyRouteStopGraphicsItem *routeItem = new JourneyRouteStopGraphicsItem(
                        this, QPixmap(32, 32), text, routeStopFlags, stopName, stopNameShortened );
                routeItem->setZoomFactor( m_zoomFactor );
                routeItem->setFont( *font );
                if ( n < routeNews.count() && !routeNews[n].isEmpty() ) {
                    routeItem->setToolTip( routeNews[n] );
                }

                QList<QAction*> actionList;
                if ( !routeStopFlags.testFlag(RouteStopIsHomeStop) ) {
                    if ( !routeStopFlags.testFlag(RouteStopIsTarget) ) {
                        routeItem->addAction( m_requestJourneyToStopAction );
                    }
                    if ( !routeStopFlags.testFlag(RouteStopIsOrigin) ) {
                        routeItem->addAction( m_requestJourneyFromStopAction );
                    }
                }

                if ( m_showInMapAction ) {
                    actionList << m_showInMapAction;
                }
                actionList << m_copyStopToClipboardAction;;
                routeItem->addActions( actionList );

                m_routeItems << routeItem;
                l->addItem( routeItem );
            } // for n, routePartStops
        } // for i, routeStops
    } // routeStops.count() >= 2
}

QString JourneyRouteGraphicsItem::svgVehicleKey( VehicleType vehicleType ) const
{
    switch ( vehicleType ) {
    case Tram:
        return "tram";
    case Bus:
        return "bus";
    case TrolleyBus:
        return "trolleybus";
    case Subway:
        return "subway";
    case Metro:
        return "metro";
    case InterurbanTrain:
        return "interurbantrain";
    case RegionalTrain:
        return "regionaltrain";
    case RegionalExpressTrain:
        return "regionalexpresstrain";
    case InterregionalTrain:
        return "interregionaltrain";
    case IntercityTrain:
        return "intercitytrain";
    case HighSpeedTrain:
        return "highspeedtrain";
    case Feet:
        return "feet";
    case Ship:
        return "ship";
    case Plane:
        return "plane";
    default:
        return QString();
    }
}

void JourneyRouteGraphicsItem::setShowIntermediateStops( bool showIntermediateStops )
{
    m_showIntermediateStops = showIntermediateStops;
    updateData( m_item );
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

    const qreal marginLeft = 32.0 * m_zoomFactor; // TODO
    const qreal iconSizeConnecting = 32.0 * m_zoomFactor;
    const qreal iconSizeIntermediate = 32.0 * m_zoomFactor/* * 0.7*/;
    QRectF timelineRect = contentsRect();
    timelineRect.setWidth( marginLeft - 6 * m_zoomFactor );
    timelineRect.setLeft( iconSizeConnecting / 2.0 );
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

    qreal stopRadius = 8.0 * m_zoomFactor;
    qreal lastY = -stopRadius;
    bool hasSubJourneys = false;
    for ( int i = 0, index = 0; i < m_routeItems.count() - 1; ++i ) {
        JourneyRouteStopGraphicsItem *routeItem = m_routeItems[i];
        JourneyRouteStopGraphicsItem *nextRouteItem = m_routeItems[i + 1];
        const RouteStopFlags flags = routeItem->routeStopFlags();
        const RouteStopFlags nextFlags = nextRouteItem->routeStopFlags();
        const bool isConnectingStop = flags.testFlag( RouteStopIsConnectingStop );
        const bool isOriginStop = i == 0 && flags.testFlag(RouteStopIsOrigin);
        if ( m_showIntermediateStops && (isConnectingStop || isOriginStop) ) {
            hasSubJourneys = !nextFlags.testFlag(RouteStopIsConnectingStop);
        }
        if ( (!hasSubJourneys && i > 0) || isConnectingStop ) {
            ++index;
        }
        const qreal iconSize = isConnectingStop ? iconSizeConnecting : iconSizeIntermediate;
        qreal y = routeItem->pos().y() + routeItem->size().height();
        QPointF stopPos( timelineRect.left() + routeLineWidth / 2.0,
                         lastY + (y - lastY) / 2.0 + 1.0 );
        lastY = y;

        // Draw lines to connect to the stop text
        qreal lineWidth = iconSize / 2.0;
        qreal lineHeight = routeItem->size().height() / 3.0;
        painter->drawLine( stopPos.x(), stopPos.y(), stopPos.x() + lineWidth, stopPos.y() );
        painter->drawLine( stopPos.x() + lineWidth, stopPos.y() - lineHeight,
                           stopPos.x() + lineWidth, stopPos.y() + lineHeight );

        const QRect stopRect( stopPos.x() - stopRadius, stopPos.y() - stopRadius,
                              2.0 * stopRadius, 2.0 * stopRadius );
        const QRectF iconRect = hasSubJourneys ? stopRect.adjusted(-5, -5, 5, 5)
                : QRectF( timelineRect.left() + (routeLineWidth - iconSize) / 2.0,
                          y - iconSize / 2.0, iconSize, iconSize );
        const int shadowWidth = 4;
        if ( !hasSubJourneys || (!isConnectingStop && !isOriginStop) ) {
            if ( index < m_item->journeyInfo()->routeVehicleTypes().count() ) {
                const VehicleType vehicleType = m_item->journeyInfo()->routeVehicleTypes()[index];
                if ( vehicleType == UnknownVehicleType ) {
                    painter->drawEllipse( iconRect.adjusted(5, 5, -5, -5) );
                    painter->drawText( iconRect, "?", QTextOption(Qt::AlignCenter) );
                    continue;
                }

                const QString vehicleKey = svgVehicleKey( vehicleType );
                if ( !m_svg->hasElement(vehicleKey) ) {
                    kDebug() << "SVG element" << vehicleKey << "not found";
                } else {
                    // Draw SVG vehicle element into pixmap
                    QPixmap vehicleTypePixmap( (int)iconRect.width(), (int)iconRect.height() );
                    vehicleTypePixmap.fill( Qt::transparent );
                    QPainter p( &vehicleTypePixmap );
                    m_svg->resize( iconRect.width() - 2 * shadowWidth, iconRect.height() - 2 * shadowWidth );
                    m_svg->paint( &p, shadowWidth, shadowWidth, vehicleKey );

                    // Draw the vehicle type with a shadow   TODO: Use shadow setting
                    QImage shadow = vehicleTypePixmap.toImage();
                    Plasma::PaintUtils::shadowBlur( shadow, shadowWidth - 1, Qt::black );
                    const QPoint pos = iconRect.topLeft().toPoint();
                    painter->drawImage( pos + QPoint(1, 2), shadow );
                    painter->drawPixmap( pos, vehicleTypePixmap );
                }
            }
        }

        if ( !hasSubJourneys || isConnectingStop || isOriginStop ) {
            // Draw the stop
            KIcon stopIcon = GlobalApplet::stopIcon( routeItem->routeStopFlags() );
            stopIcon.paint( painter, stopRect );
        }
    }

    // Draw last stop marker
    JourneyRouteStopGraphicsItem *routeItem = m_routeItems.last();
    QPointF stopPos( timelineRect.left() + routeLineWidth / 2.0,
                     lastY + (timelineRect.bottom() - lastY) / 2.0 + 1.0 );
    qreal lineWidth = iconSizeConnecting / 2.0;
    qreal lineHeight = routeItem->size().height() / 3.0;
    painter->drawLine( stopPos.x(), stopPos.y(), stopPos.x() + lineWidth, stopPos.y() );
    painter->drawLine( stopPos.x() + lineWidth, stopPos.y() - lineHeight,
                       stopPos.x() + lineWidth, stopPos.y() + lineHeight );
    KIcon stopIcon = GlobalApplet::stopIcon( routeItem->routeStopFlags() );
    stopIcon.paint( painter, stopPos.x() - stopRadius, stopPos.y() - stopRadius,
                    2.0 * stopRadius, 2.0 * stopRadius );
}
