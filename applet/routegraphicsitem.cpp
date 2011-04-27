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
#include "departureinfo.h"
#include "departuremodel.h"
#include "timetablewidget.h"

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
#include <QClipboard>

RouteGraphicsItem::RouteGraphicsItem( QGraphicsItem* parent, DepartureItem *item )
    : QGraphicsWidget(parent), m_item(item)
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
        const qreal step = (routeRect.width() - 20 * m_zoomFactor) / count;
        const qreal routeLineWidth = 4.0 * m_zoomFactor;
        const QPointF startStopPos( 10 * m_zoomFactor, padding() + routeLineWidth / 2.0 );

        // Compute minimal text angle between 15 and 90 degrees,
        // so that the stop names don't overlap
        m_textAngle = qBound( 15.0, qAtan(fm.height() / step) * 180.0 / 3.14159, 90.0 );

        // Compute maximal text width for the computed angle,
        // so that the stop name won't go outside of routeRect
        m_maxTextWidth = (routeRect.height() - startStopPos.y() /*- 10*/ - 6.0 * m_zoomFactor
                - qCos(m_textAngle * 3.14159 / 180.0) * fm.height())
                / qSin(m_textAngle * 3.14159 / 180.0);

        for ( int i = 0; i < info->routeStops().count(); ++i ) {
            QPointF stopMarkerPos( startStopPos.x() + i * step, startStopPos.y() );
            QPointF stopTextPos( stopMarkerPos.x() - 4 * m_zoomFactor,
                                stopMarkerPos.y() + 6.0 * m_zoomFactor );
            QString stopName = info->routeStops()[i];
            QString stopText = stopName;
            QFontMetrics *fontMetrics;
            QFont *font;
            if ( i == 0 || i == info->routeStops().count() - 1 ) {
                font = &boldRouteFont;
                fontMetrics = &fmBold;
            } else {
                font = &routeFont;
                fontMetrics = &fm;
            }

            // Prepend departure time at the current stop, if a time is given
            if ( i < info->routeTimes().count() && info->routeTimes()[i].isValid() ) {
                stopText = stopText.prepend( QString("%1 - ")
                        .arg(KGlobal::locale()->formatTime(info->routeTimes()[i])) );
            }

            // Elide long stop names
// 			QString shortStop;
            qreal baseSize;
            if ( i >= info->routeStops().count() - 2 ) {
// 				// The last stop names may not fit horizontally (correct the last two here)
// 				shortStop = fontMetrics->elidedText( stop, Qt::ElideRight, qMin(m_maxTextWidth,
// 						(routeRect.right() - stopPos.x() - padding() /*- 2 * fontMetrics->height()*/)
// 						/ qCos(m_textAngle * 3.14159 / 180.0)) );
                baseSize = qMin( m_maxTextWidth,
                        (routeRect.width() - stopTextPos.x() /*- 2 * fontMetrics->height()*/)
                        / qCos(m_textAngle * 3.14159 / 180.0) );
            } else {
                baseSize = m_maxTextWidth;
// 				shortStop = fontMetrics->elidedText( stop, Qt::ElideRight, m_maxTextWidth );
            }

            RouteStopMarkerGraphicsItem *markerItem = m_markerItems[ i ];
            markerItem->setPos( stopMarkerPos );

            // Create sub item, that displays a single stop name
            // and automatically elides it and stretches it on hover to show hidden text
            RouteStopTextGraphicsItem *textItem = m_textItems[ i ];
            textItem->resetTransform();
            textItem->setStop( stopText, stopName );
            textItem->setFont( *font );
            textItem->setPos( stopTextPos );
            textItem->setBaseSize( baseSize );
            textItem->resize( baseSize + 10, fontMetrics->height() );
            textItem->rotate( m_textAngle );
// 			kDebug() << "Add stop at" << stopPos;
// 			m_textItems << routeStopItem;
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
        int count = info->routeStops().count();
        QFont routeFont = KGlobalSettings::smallestReadableFont();
        routeFont.setPointSizeF( routeFont.pointSizeF() * m_zoomFactor );
        QFont boldRouteFont = routeFont;
        boldRouteFont.setBold( true );
        QFontMetrics fm( routeFont );
        QFontMetrics fmBold( boldRouteFont );
        const QRectF routeRect = rect();
        const qreal step = (routeRect.width() - 20 * m_zoomFactor) / count;
        const qreal routeLineWidth = 4.0 * m_zoomFactor;
// 		const qreal stopMarkerSize = 5.0 * m_zoomFactor;
// 		const qreal smallStopMarkerSize = 3.0 * m_zoomFactor;
        const QPointF startStopPos( (10 /*- 4*/) * m_zoomFactor,  padding() + routeLineWidth / 2.0 );

        // Compute minimal text angle between 15 and 90 degrees,
        // so that the stop names don't overlap
        m_textAngle = qBound( 15.0, qAtan(fm.height() / step) * 180.0 / 3.14159, 90.0 );

        // Compute maximal text width for the computed angle,
        // so that the stop name won't go outside of routeRect
        m_maxTextWidth = (routeRect.height() - startStopPos.y() /*- 10*/ - 6.0 * m_zoomFactor
                - qCos(m_textAngle * 3.14159 / 180.0) * fm.height())
                / qSin(m_textAngle * 3.14159 / 180.0);

        for ( int i = 0; i < info->routeStops().count(); ++i ) {
            QPointF stopMarkerPos( startStopPos.x() + i * step, startStopPos.y() );
            QPointF stopTextPos( stopMarkerPos.x() - 4 * m_zoomFactor,
                                stopMarkerPos.y() + 6.0 * m_zoomFactor );
            QString stopName = info->routeStops()[i];
            QString stopText = stopName;
            QFontMetrics *fontMetrics;
            QFont *font;
            if ( i == 0 || i == info->routeStops().count() - 1 ) {
                font = &boldRouteFont;
                fontMetrics = &fmBold;
            } else {
                font = &routeFont;
                fontMetrics = &fm;
            }

            // Prepend departure time at the current stop, if a time is given
            if ( i < info->routeTimes().count() && info->routeTimes()[i].isValid() ) {
                stopText = stopText.prepend( QString("%1: ")
                        .arg(KGlobal::locale()->formatTime(info->routeTimes()[i])) );
            }

            // Elide long stop names
// 			QString shortStop;
            qreal baseSize;
            if ( i >= info->routeStops().count() - 2 ) {
// 				// The last stop names may not fit horizontally (correct the last two here)
// 				shortStop = fontMetrics->elidedText( stop, Qt::ElideRight, qMin(m_maxTextWidth,
// 						(routeRect.right() - stopPos.x() - padding() /*- 2 * fontMetrics->height()*/)
// 						/ qCos(m_textAngle * 3.14159 / 180.0)) );
                baseSize = qMin( m_maxTextWidth,
                        (routeRect.width() - stopTextPos.x() /*- 2 * fontMetrics->height()*/)
                        / qCos(m_textAngle * 3.14159 / 180.0) );
            } else {
                baseSize = m_maxTextWidth;
// 				shortStop = fontMetrics->elidedText( stop, Qt::ElideRight, m_maxTextWidth );
            }
            RouteStopMarkerGraphicsItem *markerItem = new RouteStopMarkerGraphicsItem( this );
            markerItem->setPos( stopMarkerPos );
            m_markerItems << markerItem;

            // Create sub item, that displays a single stop name
            // and automatically elides it and stretches it on hover to show hidden text
            RouteStopTextGraphicsItem *textItem = new RouteStopTextGraphicsItem(
                    this, *font, baseSize, stopText, stopName );
            textItem->setPos( stopTextPos );
// 			textItem->setBaseSize( baseSize );
            textItem->resize( baseSize + 10, fontMetrics->height() );
            textItem->rotate( m_textAngle );
            m_textItems << textItem;

            connect( markerItem, SIGNAL(hovered(RouteStopMarkerGraphicsItem*)),
                    textItem, SLOT(hover()) );
            connect( markerItem, SIGNAL(unhovered(RouteStopMarkerGraphicsItem*)),
                    textItem, SLOT(unhover()) );
            connect( textItem, SIGNAL(hovered(RouteStopTextGraphicsItem*)),
                    markerItem, SLOT(hover()) );
            connect( textItem, SIGNAL(unhovered(RouteStopTextGraphicsItem*)),
                    markerItem, SLOT(unhover()) );
            connect( textItem, SIGNAL(requestFilterCreation(QString,RouteStopTextGraphicsItem*)),
                    this, SIGNAL(requestFilterCreation(QString,RouteStopTextGraphicsItem*)) );
            connect( textItem, SIGNAL(showDepartures(QString,RouteStopTextGraphicsItem*)),
                    this, SIGNAL(showDepartures(QString,RouteStopTextGraphicsItem*)) );
        }
    }
}

void RouteGraphicsItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                            QWidget* widget )
{
    Q_UNUSED( option );
    Q_UNUSED( widget );

    if ( !m_item ) {
        kDebug() << "No Info Object!";
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
//
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
// 			stopIcon.paint( painter, stopPos.x() + step * 0.333333 - smallStopMarkerSize / 2.0,
// 							stopPos.y() - smallStopMarkerSize / 2.0,
// 							smallStopMarkerSize, smallStopMarkerSize );
// 			stopIcon.paint( painter, stopPos.x() + step * 0.666666 - smallStopMarkerSize / 2.0,
// 							stopPos.y() - smallStopMarkerSize / 2.0,
// 							smallStopMarkerSize, smallStopMarkerSize );
            painter->drawEllipse( stopPos + QPointF(step * 0.333333, 0.0),
                                smallStopMarkerSize, smallStopMarkerSize );
            painter->drawEllipse( stopPos + QPointF(step * 0.666666, 0.0),
                                smallStopMarkerSize, smallStopMarkerSize );
        }
    }
}

RouteStopMarkerGraphicsItem::RouteStopMarkerGraphicsItem( QGraphicsItem* parent )
    : QGraphicsWidget(parent)
{
    m_hoverStep = 0.0;
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
// 	update();
    updateGeometry();
}

void RouteStopMarkerGraphicsItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                        QWidget* widget )
{
    Q_UNUSED( widget );
    painter->setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );
    KIcon("public-transport-stop").paint( painter, option->rect );
}

RouteStopTextGraphicsItem::RouteStopTextGraphicsItem( QGraphicsItem* parent, const QFont &font,
                                                    qreal baseSize, const QString& stopText, const QString &stopName )
    : QGraphicsWidget(parent)
{
    m_expandStep = 0.0;
    m_baseSize = baseSize;
    setFont( font );
    setStop( stopText, stopName );
    setAcceptHoverEvents( true );
}

void RouteStopTextGraphicsItem::setStop( const QString& stopText, const QString &stopName )
{
    m_stopText = stopText;
    m_stopName = stopName;
    qreal maxSize = QFontMetrics( font() ).width( m_stopText ) + 5;
    if ( maxSize > m_baseSize ) {
        setToolTip( stopText );
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
    QMenu *menu = new QMenu( event->widget() );
    QAction *showDeparturesAction = new QAction( KIcon("public-transport-stop"),
            i18n("Show &Departures From '%1'", m_stopName), menu );
    QAction *newFilterViaStopAction = new QAction( KIcon("view-filter"),
            i18n("&Create Filter 'Via %1'", m_stopName), menu );
    QAction *copyStopToClipboardAction = new QAction( KIcon("edit-copy"),
            i18n("&Copy Stop Name"), menu );
    menu->addAction( showDeparturesAction );
    menu->addAction( newFilterViaStopAction );
    menu->addAction( copyStopToClipboardAction );

    QAction *executedAction = menu->exec( event->screenPos() );
    if ( executedAction == newFilterViaStopAction ) {
        emit requestFilterCreation( m_stopName, this );
    } else if ( executedAction == showDeparturesAction ) {
        emit showDepartures( m_stopName, this );
    } else if ( executedAction == copyStopToClipboardAction ) {
        QApplication::clipboard()->setText( m_stopName );
    }
}

void RouteStopTextGraphicsItem::setExpandStep( qreal expandStep )
{
    qreal maxSize = QFontMetrics( font() ).width( m_stopText ) + 5;
    if ( m_baseSize < maxSize ) {
        resize( m_baseSize + (maxSize - m_baseSize) * expandStep, size().height() );
    }

#if KDE_VERSION < KDE_MAKE_VERSION(4,6,0)
    QColor normalColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor );
    QColor hoverColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::HighlightColor );
#else
    QColor normalColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::ViewTextColor );
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

void RouteStopTextGraphicsItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                    QWidget* widget )
{
    Q_UNUSED( widget );
    QColor textColor = palette().color( QPalette::Active, QPalette::Text );
    bool drawHalos = /*m_options.testFlag(DrawShadows) &&*/ qGray(textColor.rgb()) < 156;

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
    const QPixmap &vehiclePixmap, const QString &text, bool isIntermediate, const QString &stopName )
    : QGraphicsWidget(parent), m_parent(parent), m_infoTextDocument(0)
{
    m_vehiclePixmap = vehiclePixmap;
    m_intermediate = isIntermediate;
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
    if ( m_intermediate ) {
        QMenu *menu = new QMenu( event->widget() );
        // TODO newOriginStopAction with "from" instead of "to" for journeys
        QAction *newTargetStopAction = new QAction( KIcon("edit-find"),
                i18n("&Search Journeys to %1", m_stopName), menu );
        menu->addAction( newTargetStopAction );

        QAction *executedAction = menu->exec( event->screenPos() );
        if ( executedAction == newTargetStopAction ) {
            emit requestJourneys( m_stopName, this );
        }
    } else {
        QGraphicsItem::contextMenuEvent(event);
    }
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
    bool drawHalos = /*m_options.testFlag(DrawShadows) &&*/ qGray(textColor.rgb()) < 156;
    QRectF textRect = infoTextRect();
    TextDocumentHelper::drawTextDocument( painter, option, m_infoTextDocument,
                                        textRect.toRect(), drawHalos );
}

JourneyRouteGraphicsItem::JourneyRouteGraphicsItem( QGraphicsItem* parent, JourneyItem* item,
    Plasma::Svg* svg ) : QGraphicsWidget(parent), m_item(item), m_svg(svg)
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
// 	qDeleteAll( m_routeItems );
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
// 		const qreal routeLineWidth = 4.0 * m_zoomFactor;

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

            QString text = QString( "<b>%1</b>" ).arg( info->routeStops()[i] );

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


            JourneyRouteStopGraphicsItem *routeItem = new JourneyRouteStopGraphicsItem(
                    this, QPixmap(32, 32), text, i >= 1 && i < info->routeStops().count() - 1,
                    info->routeStops()[i] );
            routeItem->setFont( *font );
            connect( routeItem, SIGNAL(requestJourneys(QString,JourneyRouteStopGraphicsItem*)),
                    this, SLOT(processJourneyRequest(QString,JourneyRouteStopGraphicsItem*)) );
            m_routeItems << routeItem;
            l->addItem( routeItem );
        }
    }
}

void JourneyRouteGraphicsItem::processJourneyRequest( const QString& newTargetStop,
                                                    JourneyRouteStopGraphicsItem* item )
{
    Q_UNUSED( item );

    if ( m_routeItems.isEmpty() ) {
        kDebug() << "Requested a new journey, but no route items present in list anymore";
        return;
    }

    QString startStop = m_routeItems.first()->stopName();
    if ( startStop == newTargetStop ) {
        kDebug() << "Requested a new journey with start stop = target stop";
        return;
    }

    // Request journeys from the first route stop to the given new target stop
    emit requestJourneys( startStop, newTargetStop );
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
// 		painter->drawEllipse( stopPos, 4.0, 4.0 );
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
// 	painter->drawEllipse( stopPos, 4.0, 4.0 );
    KIcon("public-transport-stop").paint( painter,
                                        stopPos.x() - stopRadius, stopPos.y() - stopRadius,
                                        2.0 * stopRadius, 2.0 * stopRadius );
}
