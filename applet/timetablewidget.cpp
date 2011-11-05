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

// Own includes
#include "timetablewidget.h"
#include "routegraphicsitem.h"
#include "departuremodel.h"

// Plasma includes
#include <Plasma/PaintUtils>
#include <Plasma/Svg>
#include <Plasma/Animator>
#include <Plasma/Animation>
#include <Plasma/DataEngineManager>

// KDE includes
#include <KColorScheme>
#include <KColorUtils>
#include <KMenu>
#include <KPixmapCache>

// Qt includes
#include <QGraphicsLinearLayout>
#include <QModelIndex>
#include <QPainter>
#include <QTextLayout>
#include <QTextDocument>
#include <QTextBlock>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsScene>
#include <QPropertyAnimation>
#include <QStyleOption>
#include <qmath.h>

const qreal PublicTransportGraphicsItem::ROUTE_ITEM_HEIGHT = 60.0;

PublicTransportGraphicsItem::PublicTransportGraphicsItem(
        PublicTransportWidget* publicTransportWidget, QGraphicsItem* parent,
        StopAction *copyStopToClipboardAction, StopAction *showInMapAction/*, QAction *toggleAlarmAction*/ )
        : QGraphicsWidget(parent), m_item(0), m_parent(publicTransportWidget),
        m_resizeAnimation(0), m_pixmap(0), m_copyStopToClipboardAction(copyStopToClipboardAction),
        m_showInMapAction(showInMapAction)/*,
        m_toggleAlarmAction(toggleAlarmAction)*/
{
    setFlag( ItemClipsToShape );
    setFlag( ItemClipsChildrenToShape );
    m_expanded = false;
    m_expandStep = 0.0;
    m_fadeOut = 1.0;
}

PublicTransportGraphicsItem::~PublicTransportGraphicsItem()
{
    delete m_pixmap;
}

void DepartureGraphicsItem::updateSettings()
{
    if ( m_routeItem ) {
        m_routeItem->setZoomFactor( m_parent->zoomFactor() );
    }
    update();
}

void JourneyGraphicsItem::updateSettings()
{
    if ( m_routeItem ) {
        m_routeItem->setZoomFactor( m_parent->zoomFactor() );
    }
    update();
}

void PublicTransportGraphicsItem::setExpanded( bool expand )
{
    m_expanded = expand;
    if ( expand && routeItem() ) {
        routeItem()->setVisible( true );
    }

    if ( m_resizeAnimation ) {
        m_resizeAnimation->stop();
    } else {
        m_resizeAnimation = new QPropertyAnimation( this, "expandStep" );
        m_resizeAnimation->setEasingCurve( QEasingCurve(QEasingCurve::InOutBack) );
        connect( m_resizeAnimation, SIGNAL(finished()), this, SLOT(resizeAnimationFinished()) );
    }

    m_resizeAnimation->setStartValue( m_expandStep );
    m_resizeAnimation->setEndValue( expand ? 1.0 : 0.0 );
    m_resizeAnimation->start( QAbstractAnimation::KeepWhenStopped );
    updateGeometry();
}

void PublicTransportGraphicsItem::resizeAnimationFinished()
{
    if ( routeItem() ) {
        routeItem()->setVisible( m_expanded );
    }
    delete m_resizeAnimation;
    m_resizeAnimation = 0;
}

void PublicTransportGraphicsItem::updateGeometry()
{
    QGraphicsWidget::updateGeometry();
}

void PublicTransportGraphicsItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                         QWidget* widget )
{
    Q_UNUSED( widget );
    painter->setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );
    if ( !m_item || !isValid() ) {
        if ( m_pixmap ) {
            // Draw captured pixmap, the item in the model is already deleted
            // but still needs to be drawn here while animating
            QRectF sourceRect = boundingRect();
            sourceRect.moveTopLeft( QPointF(0, 0) );
            painter->drawPixmap( boundingRect(), *m_pixmap, sourceRect );
        }

        return;
    }

    // Paint background on whole item (including expand area)
    QRectF rect = boundingRect();
    paintBackground( painter, option, rect );

    // Paint item (excluding expand area)
    QRectF rectItem = rect;
    rectItem.setHeight( unexpandedHeight() );
    paintItem( painter, option, rectItem );

    // Draw expand area if this item isn't currently completely unexpanded
    if ( m_expanded || !qFuzzyIsNull(m_expandStep) ) {
        qreal pad = padding();
        qreal indentation = expandAreaIndentation();
        QRectF rectExpanded( rectItem.left() + indentation, rectItem.bottom() + 2 * pad,
                             rectItem.width() - indentation - pad, expandAreaHeight() - 2 * pad );
        paintExpanded( painter, option, rectExpanded );
    }
}

void PublicTransportGraphicsItem::capturePixmap()
{
    // Delete previous pixmap if any
    delete m_pixmap;
    m_pixmap = 0;

    // Create new pixmap
    m_pixmap = new QPixmap( size().toSize() );
    m_pixmap->fill( Qt::transparent );

    // Draw this item into the new pixmap
    QPainter p( m_pixmap );
    QStyleOptionGraphicsItem option;
    option.rect = rect().toRect();
    paint( &p, &option );
}

qreal PublicTransportGraphicsItem::padding() const
{
    return 4.0 * m_parent->zoomFactor();
}

qreal PublicTransportGraphicsItem::unexpandedHeight() const
{
    return qMax( m_parent->iconSize() * 1.1,
                 (qreal)QFontMetrics(font()).lineSpacing() * m_parent->maxLineCount() + padding() );
}

bool PublicTransportGraphicsItem::hasExtraIcon( Columns column ) const
{
    if ( !m_item ) {
        // Item was already deleted
        return false;
    }

    QModelIndex modelIndex = index().model()->index( index().row(), column );
    return modelIndex.data( Qt::DecorationRole ).isValid()
            && !modelIndex.data( Qt::DecorationRole ).value<QIcon>().isNull();
}

int PublicTransportGraphicsItem::extraIconSize() const
{
    return m_parent->iconSize() / 2;
}

QTextDocument* TextDocumentHelper::createTextDocument(const QString& html, const QSizeF& size,
    const QTextOption &textOption, const QFont &font  )
{
    QTextDocument *textDocument = new QTextDocument;
    textDocument->setDefaultFont( font );
    textDocument->setDocumentMargin( 0 );
    textDocument->setDefaultTextOption( textOption );
    textDocument->setPageSize( size );
    textDocument->setHtml( html );
    textDocument->documentLayout();
    return textDocument;
}

void TextDocumentHelper::drawTextDocument( QPainter *painter,
        const QStyleOptionGraphicsItem* option, QTextDocument *document,
        const QRect &textRect, bool drawHalos )
{
    if ( textRect.isEmpty() ) {
        kDebug() << "Empty text rect given!";
        return;
    }

    QList<QRect> haloRects, fadeRects;
    const int fadeWidth = 30;

    QPixmap pixmap( textRect.size() );
    pixmap.fill( Qt::transparent );
    QPainter p( &pixmap );
    p.setPen( painter->pen() );
    p.setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );

    QFontMetrics fm( document->defaultFont() );
    int maxLineCount = qMax( qFloor(textRect.height() / fm.lineSpacing()), 1 );
    int blockCount = document->blockCount();
    int lineCount = 0;
    for ( int b = 0; b < blockCount; ++b ) {
        lineCount += document->findBlockByNumber( b ).layout()->lineCount();
    }
    if ( lineCount > maxLineCount ) {
        lineCount = maxLineCount;
    }
    int textHeight = lineCount * ( fm.lineSpacing() + 1 );

    // Draw text and calculate halo/fade rects
    for ( int b = 0; b < blockCount; ++b ) {
        QTextLayout *textLayout = document->findBlockByNumber( b ).layout();
        const int lines = textLayout->lineCount();
        const QPointF position( 0, (textRect.height() - textHeight) / 2.0f );
        for ( int l = 0; l < lines; ++l ) {
            // Draw a text line
            QTextLine textLine = textLayout->lineAt( l );
            textLine.draw( &p, position );

            if ( drawHalos ) {
                // Calculate halo rect
                QSize textSize = textLine.naturalTextRect().size().toSize();
                if ( textSize.width() > textRect.width() ) {
                    textSize.setWidth( textRect.width() );
                }
                QRect haloRect = QStyle::visualRect( textLayout->textOption().textDirection(),
                        textRect, QRect((textLine.position() + position).toPoint() +
                        (document->defaultTextOption().alignment().testFlag(Qt::AlignRight)
                        ? (textRect.topRight() - QPoint(textSize.width(), 0)) : textRect.topLeft()),
                        textSize) );
                if ( haloRect.top() <= textRect.bottom() ) {
                    if ( haloRect.width() > textRect.width() ) {
                        haloRect.setWidth( textRect.width() );
                    }
                    // Add a halo rect for each drawn text line
                    haloRects << haloRect;
                }
            }

            // Add a fade out rect to the list if the line is too long
            if ( textLine.naturalTextWidth() > textRect.width() - textLine.x() ) {
                int x = int( qMin(textLine.naturalTextWidth(), (qreal)textRect.width()) )
                        - fadeWidth + textLine.x() + position.x();
                int y = int( textLine.position().y() + position.y() );
                QRect fadeRect = QStyle::visualRect( textLayout->textOption().textDirection(),
                        textRect, QRect(x, y, fadeWidth, int(textLine.height()) + 1) );
                fadeRects << fadeRect;
            }
        }
    }

    // Reduce the alpha in each fade out rect using the alpha gradient
    if ( !fadeRects.isEmpty() ) {
        // (From the tasks plasmoid) Create the alpha gradient for the fade out effect
        QLinearGradient alphaGradient( 0, 0, 1, 0 );
        alphaGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
        if ( option->direction == Qt::LeftToRight ) {
            alphaGradient.setColorAt( 0, Qt::black );
            alphaGradient.setColorAt( 1, Qt::transparent );
        } else {
            alphaGradient.setColorAt( 0, Qt::transparent );
            alphaGradient.setColorAt( 1, Qt::black );
        }

        p.setCompositionMode( QPainter::CompositionMode_DestinationIn );
        foreach( const QRect &rect, fadeRects ) {
            p.fillRect( rect, alphaGradient );
        }
    }
    p.end();

    // Draw halo/shadow
    if ( drawHalos ) {
        foreach( const QRect &haloRect, haloRects ) {
            Plasma::PaintUtils::drawHalo( painter, haloRect );
        }
    } else {
        QImage shadow = pixmap.toImage();
        Plasma::PaintUtils::shadowBlur( shadow, 3, Qt::black );
        painter->drawImage( textRect.topLeft() + QPoint(1, 2), shadow );
    }

    painter->drawPixmap( textRect.topLeft(), pixmap );
}

qreal TextDocumentHelper::textDocumentWidth( QTextDocument* document )
{
    if ( !document ) {
        return 0.0;
    }

    qreal maxWidth = 0.0;
    int blockCount = document->blockCount();
    for ( int b = 0; b < blockCount; ++b ) {
        QTextLayout *textLayout = document->findBlockByNumber( b ).layout();
        int lines = textLayout->lineCount();
        for ( int l = 0; l < lines; ++l ) {
            QTextLine textLine = textLayout->lineAt( l );
            if ( textLine.naturalTextWidth() > maxWidth ) {
                maxWidth = textLine.naturalTextWidth();
            }
        }
    }
    return maxWidth;
}

void PublicTransportGraphicsItem::resizeEvent( QGraphicsSceneResizeEvent* event )
{
    QGraphicsWidget::resizeEvent( event );
    updateTextLayouts();
}

void DepartureGraphicsItem::resizeEvent( QGraphicsSceneResizeEvent* event )
{
    PublicTransportGraphicsItem::resizeEvent(event);

    if ( m_routeItem ) {
        QRectF _infoRect = infoRect( rect(), 0 );
        m_routeItem->setGeometry( _infoRect.left(), rect().top() + unexpandedHeight() + padding(),
                                  rect().width() - padding() - _infoRect.left(),
                                  ROUTE_ITEM_HEIGHT * m_parent->zoomFactor() );
    }
}

void JourneyGraphicsItem::resizeEvent( QGraphicsSceneResizeEvent* event )
{
    PublicTransportGraphicsItem::resizeEvent(event);

    if ( m_routeItem ) {
        QRectF _infoRect = infoRect( rect() );
        m_routeItem->setGeometry( _infoRect.left(), rect().top() + unexpandedHeight() + padding(),
                                  rect().width() - padding() - _infoRect.left(),
                                  m_routeItem->size().height() );
    }
}

void JourneyGraphicsItem::contextMenuEvent( QGraphicsSceneContextMenuEvent* event )
{
    JourneyItem *item = qobject_cast<JourneyItem*>( m_item );
    KMenu contextMenu;

    QList<QAction*> actionList;
    QAction *infoAction = 0;
    QAction *addAlarmAction = 0;
    QAction *removeAlarmAction = 0;
    if ( item->hasAlarm() ) {
        if ( item->alarmStates().testFlag(AlarmIsAutoGenerated) ) {
            removeAlarmAction = new QAction( KIcon("task-reminder"),
                    i18nc("@action:inmenu", "Remove &Alarm For This Journey"), &contextMenu );
            actionList << removeAlarmAction;
        } else if ( item->alarmStates().testFlag(AlarmIsRecurring) ) {
            // The 'Remove this Alarm' menu entry can only be
            // used with autogenerated alarms
//             if ( item->departureInfo()->matchedAlarms().count() == 1 ) { TODO
//                 infoAction = new QAction( KIcon("task-recurring"),
//                         i18nc("@info/plain", "(has a recurring alarm)"), this );
//             } else {
                infoAction = new QAction( i18nc("@action:inmenu", "(has recurring/multiple alarms)"), this );
//             }
        } else {
            // The 'Remove this Alarm' menu entry can only be
            // used with autogenerated alarms
//             if ( item->departureInfo()->matchedAlarms().count() == 1 ) {
//                 infoAction = new QAction( KIcon("task-recurring"),
//                         i18nc("@info/plain", "(has a custom alarm)"), this );
//             } else {
                infoAction = new QAction( i18nc("@action:inmenu", "(has custom/multiple alarms)"), this );
//             }
        }
        if ( infoAction ) {
            infoAction->setDisabled( true );
            actionList << infoAction;
        }
    } else {
        addAlarmAction = new QAction( KIcon("task-reminder"),
                i18nc("@action:inmenu", "Add &Alarm For This Journey"), &contextMenu );
        actionList << addAlarmAction;
    }

    contextMenu.addActions( actionList );

    QAction *executedAction = contextMenu.exec( event->screenPos() );

    if ( executedAction ) {
        const JourneyInfo *info = journeyItem()->journeyInfo();
        QString lineString = info->routeTransportLines().isEmpty()
                ? QString() : info->routeTransportLines().first();
        VehicleType vehicleType = info->routeVehicleTypes().isEmpty()
                ? Unknown : info->routeVehicleTypes().first();
        if ( executedAction == addAlarmAction ) {
            emit requestAlarmCreation( info->departure(), lineString, vehicleType, QString(), this );
        } else if ( executedAction == removeAlarmAction ) {
            emit requestAlarmDeletion( info->departure(), lineString, vehicleType, QString(), this );
        }
    }
}

void PublicTransportGraphicsItem::mousePressEvent( QGraphicsSceneMouseEvent* event )
{
    QGraphicsItem::mousePressEvent( event );
    if ( event->button() == Qt::LeftButton ) {
        event->accept();
    }
}

void PublicTransportGraphicsItem::mouseReleaseEvent( QGraphicsSceneMouseEvent* event )
{
    if ( event->button() == Qt::LeftButton
        && (event->lastPos() - event->pos()).manhattanLength() < 5 )
    {
        toggleExpanded();
        event->accept();
    } else {
        QGraphicsItem::mousePressEvent( event );
    }
}

DepartureGraphicsItem::DepartureGraphicsItem( PublicTransportWidget* publicTransportWidget,
        QGraphicsItem* parent,
        StopAction *copyStopToClipboardAction, StopAction *showInMapAction,
        StopAction *showDeparturesAction, StopAction *highlightStopAction,
        StopAction *newFilterViaStopAction, KPixmapCache *pixmapCache )
        : PublicTransportGraphicsItem( publicTransportWidget, parent, copyStopToClipboardAction,
                                       showInMapAction ),
        m_infoTextDocument(0), m_timeTextDocument(0), m_routeItem(0), m_highlighted(false),
        m_leavingAnimation(0), m_showDeparturesAction(showDeparturesAction),
        m_highlightStopAction(highlightStopAction), m_newFilterViaStopAction(newFilterViaStopAction),
        m_pixmapCache(pixmapCache)
{
    m_leavingStep = 0.0;
}

DepartureGraphicsItem::~DepartureGraphicsItem()
{
    if ( m_leavingAnimation ) {
        m_leavingAnimation->stop();
    }

    delete m_infoTextDocument;
    delete m_timeTextDocument;
}

JourneyGraphicsItem::JourneyGraphicsItem( PublicTransportWidget* publicTransportWidget,
        QGraphicsItem* parent,
        StopAction *copyStopToClipboardAction, StopAction *showInMapAction,
        StopAction *requestJourneyToStopAction, StopAction *requestJourneyFromStopAction )
        : PublicTransportGraphicsItem( publicTransportWidget, parent, copyStopToClipboardAction,
                                       showInMapAction
        /*, toggleAlarmAction*/ ),
        m_infoTextDocument(0), m_routeItem(0),
        m_requestJourneyToStopAction(requestJourneyToStopAction),
        m_requestJourneyFromStopAction(requestJourneyFromStopAction)
{
}

JourneyGraphicsItem::~JourneyGraphicsItem()
{
    delete m_infoTextDocument;
}

void DepartureGraphicsItem::setLeavingStep( qreal leavingStep )
{
    m_leavingStep = leavingStep;
    setOpacity( 1.0 - leavingStep );
    update();
}

void DepartureGraphicsItem::updateTextLayouts()
{
    if ( !m_item ) {
        // Item already destroyed
        return;
    }

    QRectF rect = contentsRect();
    const QRectF _timeRect = timeRect( rect );
    QTextOption textOption( Qt::AlignVCenter | Qt::AlignLeft );
    textOption.setWrapMode( m_parent->maxLineCount() == 1
            ? QTextOption::NoWrap : QTextOption::WordWrap );

    // Update text layouts
    if ( !m_timeTextDocument || (m_timeTextDocument->pageSize() != _timeRect.size()) ) {
        delete m_timeTextDocument;
        textOption.setAlignment( Qt::AlignVCenter | Qt::AlignRight );
        m_timeTextDocument = TextDocumentHelper::createTextDocument(
                index().model()->index(index().row(), 2).data(FormattedTextRole).toString(),
                _timeRect.size(), textOption, font() );
    }

    const qreal timeWidth = timeColumnWidth();
    const QRectF _infoRect = infoRect( rect, timeWidth );

    // Create layout for the main column showing information about the departure
    if ( !m_infoTextDocument || (m_infoTextDocument->pageSize() != _infoRect.size()) ) {
        delete m_infoTextDocument;
        textOption.setAlignment( Qt::AlignVCenter | Qt::AlignLeft );
        QString html;
        const DepartureInfo *info = departureItem()->departureInfo();
        TimetableWidget *timetableWidget = qobject_cast<TimetableWidget*>( m_parent );
        if ( timetableWidget->isTargetHidden() ) {
            html = i18nc("@info", "<emphasis strong='1'>%1</emphasis>", info->lineString());
        } else if ( departureItem()->model()->info().departureArrivalListType == ArrivalList ) {
            html = i18nc("@info", "<emphasis strong='1'>%1</emphasis> from %2",
                         info->lineString(), info->target());
        } else { // if ( departureItem()->model()->info().departureArrivalListType == DepartureList ) {
            html = i18nc("@info", "<emphasis strong='1'>%1</emphasis> to %2",
                         info->lineString(), info->target());
        }
        m_infoTextDocument = TextDocumentHelper::createTextDocument( html, _infoRect.size(),
                                                                     textOption, font() );
    }
}

void JourneyGraphicsItem::updateTextLayouts()
{
    if ( !m_item ) {
        return;
    }

    QRectF rect = contentsRect();
    QTextOption textOption( Qt::AlignVCenter | Qt::AlignLeft );
    textOption.setWrapMode( m_parent->maxLineCount() == 1
            ? QTextOption::NoWrap : QTextOption::ManualWrap );

    // Create layout for the main column showing information about the departure
    const QRectF _infoRect = infoRect( rect/*, departureTimeWidth, arrivalTimeWidth*/ );
    if ( !m_infoTextDocument || (m_infoTextDocument->pageSize() != _infoRect.size()) ) {
        delete m_infoTextDocument;
        textOption.setAlignment( Qt::AlignVCenter | Qt::AlignLeft );
        QString html;
        const JourneyInfo *info = journeyItem()->journeyInfo();
        if ( m_parent->maxLineCount() == 1 ) {
            // Single line string
            html = i18nc("@info", "<emphasis strong='1'>Duration:</emphasis> %1, "
                         "<emphasis strong='1'>Changes:</emphasis> %2",
                         KGlobal::locale()->formatDuration(info->duration()*60*1000),
                         ((info->changes() == 0
                         ? i18nc("@info No vehicle changes in a journey", "none")
                         : QString::number(info->changes()))));
        } else {
            // Two (or more) line string
            html = i18nc("@info", "<emphasis strong='1'>Duration:</emphasis> %1, "
                         "<emphasis strong='1'>Changes:</emphasis> %2<nl />"
                         "<emphasis strong='1'>Departing:</emphasis> %3, "
                         "<emphasis strong='1'>Arriving:</emphasis> %4",
                          KGlobal::locale()->formatDuration(info->duration()*60*1000),
                         (info->changes() == 0
                         ? i18nc("@info No vehicle changes in a journey", "none")
                         : QString::number(info->changes())),
                         KGlobal::locale()->formatDateTime(info->departure(), KLocale::FancyShortDate),
                         KGlobal::locale()->formatDateTime(info->arrival(), KLocale::FancyShortDate));
        }
        m_infoTextDocument = TextDocumentHelper::createTextDocument( html, _infoRect.size(),
                                                                     textOption, font() );
    }
}

void JourneyGraphicsItem::updateData( JourneyItem* item, bool updateLayouts )
{
    m_item = item;
    setAcceptHoverEvents( true );
    updateGeometry();

    if ( updateLayouts ) {
        delete m_infoTextDocument;
        m_infoTextDocument = 0;
    }
    updateTextLayouts();

    if ( !item->journeyInfo()->routeStops().isEmpty() ) {
        if ( m_routeItem ) {
            m_routeItem->updateData( item );
        } else {
            m_routeItem = new JourneyRouteGraphicsItem( this, item, m_parent->svg(),
                    m_copyStopToClipboardAction, m_showInMapAction, m_requestJourneyToStopAction,
                    m_requestJourneyFromStopAction );
            QRectF _infoRect = infoRect( rect() );
            m_routeItem->setZoomFactor( m_parent->zoomFactor() );
            m_routeItem->setPos( _infoRect.left(), rect().top() + unexpandedHeight() + padding() );
            m_routeItem->resize( rect().width() - padding() - _infoRect.left(),
                                m_routeItem->size().height() );
            m_routeItem->updateData( item );
        }
    } else if ( m_routeItem ) {
        delete m_routeItem;
        m_routeItem = 0;
    }

    update();
}

void DepartureGraphicsItem::updateData( DepartureItem* item, bool updateLayouts )
{
    m_item = item;
    updateGeometry();

    if ( updateLayouts ) {
        delete m_infoTextDocument;
        delete m_timeTextDocument;
        m_infoTextDocument = 0;
        m_timeTextDocument = 0;
    }
    updateTextLayouts();

    if ( !item->departureInfo()->routeStops().isEmpty() ) {
        if ( m_routeItem ) {
            m_routeItem->updateData( item );
        } else {
            m_routeItem = new RouteGraphicsItem( this, item, m_copyStopToClipboardAction,
                    m_showInMapAction, m_showDeparturesAction, m_highlightStopAction,
                    m_newFilterViaStopAction );
            m_routeItem->setVisible( false );

            QRectF _infoRect = infoRect( rect(), 0 );
            m_routeItem->setZoomFactor( m_parent->zoomFactor() );
            m_routeItem->setPos( _infoRect.left(), rect().top() + unexpandedHeight() + padding() );
            m_routeItem->resize( rect().width() - padding() - _infoRect.left(),
                                 ROUTE_ITEM_HEIGHT * m_parent->zoomFactor() );
        }
    } else if ( m_routeItem ) {
        delete m_routeItem;
        m_routeItem = 0;
    }

    if ( item->isLeavingSoon() && !m_leavingAnimation ) {
        m_leavingAnimation = new QPropertyAnimation( this, "leavingStep", this );
        m_leavingAnimation->setStartValue( 0.0 );
        m_leavingAnimation->setKeyValueAt( 0.5, 0.5 );
        m_leavingAnimation->setEndValue( 0.0 );
        m_leavingAnimation->setDuration( 1000 );
        m_leavingAnimation->setEasingCurve( QEasingCurve(QEasingCurve::InOutCubic) );
        m_leavingAnimation->setLoopCount( -1 );
        m_leavingAnimation->start( QAbstractAnimation::DeleteWhenStopped );
    }

    update();
}

qreal DepartureGraphicsItem::timeColumnWidth() const
{
    qreal width = TextDocumentHelper::textDocumentWidth( m_timeTextDocument );

    QRectF rect = contentsRect();
    if ( qobject_cast<TimetableWidget*>(m_parent)->isTargetHidden() ) {
        if ( width > rect.width() * 3 / 4 - padding() ) {
            width = rect.width() * 3 / 4 - padding();
        }
    } else if ( width > rect.width() / 2 - padding() ) {
        width = rect.width() / 2 - padding();
    }

    return width;
}

void JourneyGraphicsItem::paintBackground( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                           const QRectF& rect )
{
    Q_UNUSED( option );
    QColor alternateBackgroundColor = KColorScheme( QPalette::Active, KColorScheme::View )
            .background( KColorScheme::AlternateBackground ).color();
    alternateBackgroundColor.setAlphaF( 0.3 );
    QColor borderColor = textColor();
    borderColor.setAlphaF( 0.5 );

    QRect pixmapRect( 0, 0, rect.width(), rect.height() );
    QPixmap pixmap( pixmapRect.size());
    QColor backgroundColor = Qt::transparent;
    pixmap.fill( backgroundColor );

    // Use journey rating background:
    //   green for relatively short duration, less changes;
    //   red for relatively long duration, more changes (controlled by the model).
    QVariant vr = index().data( JourneyRatingRole );
    if ( vr.isValid() ) {
        qreal rating = vr.toReal();
        QColor ratingColor = KColorUtils::mix(
                KColorScheme(QPalette::Active).background(KColorScheme::PositiveBackground).color(),
                KColorScheme(QPalette::Active).background(KColorScheme::NegativeBackground).color(),
                rating );
        bool drawRatingBackground = true;
        if ( rating >= 0 && rating <= 0.5 ) {
            ratingColor.setAlphaF(( 0.5 - rating ) * 2 );
        } else if ( rating >= 0.5 && rating <= 1.0 ) {
            ratingColor.setAlphaF(( rating - 0.5 ) * 2 );
        } else {
            drawRatingBackground = false;
        }

        if ( drawRatingBackground ) {
            backgroundColor = ratingColor;
        }
    } else if ( index().row() % 2 == 1 ) {
        // Use alternate background (if journey ratings aren't available)
        backgroundColor = alternateBackgroundColor;
    }

    // Fill the pixmap with the mixed background color
    pixmap.fill( backgroundColor );

    // Draw special background for departures with an alarm
    QPainter p( &pixmap );
    if ( index().data(DrawAlarmBackgroundRole).toBool() ) {
        drawAlarmBackground( &p, pixmapRect );
    }

    // Draw a line at the bottom of this TimetableItem
    p.setPen( borderColor );
    p.drawLine( pixmapRect.bottomLeft(), pixmapRect.bottomRight() );

    // Fade out to the left and right
    drawFadeOutLeftAndRight( &p, pixmapRect );
    p.end();

    painter->drawPixmap( rect.toRect(), pixmap );
}

void JourneyGraphicsItem::paintItem( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                     const QRectF &rect )
{
    QFontMetrics fm( font() );
    const QRectF _vehicleRect = vehicleRect( rect );
    const QRectF _infoRect = infoRect( rect );

    const int shadowWidth = 4;
    QPixmap pixmap( (int)_vehicleRect.width(), (int)_vehicleRect.height() );
    pixmap.fill( Qt::transparent );
    QPainter p( &pixmap );

    // Get a list of vehicles used in the journey, but without unknown vehicle type.
    QSet<VehicleType> vehicleTypeSet = journeyItem()->journeyInfo()->vehicleTypes();
    vehicleTypeSet.remove( Unknown );
    QList<VehicleType> vehicleTypes = vehicleTypeSet.toList();

    // Calculate values for arranging vehicle type icons
    const int vehiclesPerRow = qCeil( qSqrt(vehicleTypes.count()) );
    const int rows = qCeil( (qreal)vehicleTypes.count() / (qreal)vehiclesPerRow );
    const qreal vehicleSize = vehiclesPerRow == 1 ? _vehicleRect.width()
            : _vehicleRect.width() / ( 0.7 * vehiclesPerRow );
    const qreal vehicleOffsetX = vehiclesPerRow == 1 ? 0.0
            : (_vehicleRect.width() - vehicleSize) / (vehiclesPerRow - 1);
    const qreal vehicleOffsetY = rows == 1 ? 0.0
            : (_vehicleRect.height() - vehicleSize) / (rows - 1);
    int vehiclesInCurrentRow = 0;
    qreal x = shadowWidth;
    qreal y = shadowWidth;
    QSizeF iconSize( vehicleSize - 2 * shadowWidth, vehicleSize - 2 * shadowWidth );
    m_parent->svg()->resize( iconSize );

    // Draw the vehicle type icons
    for ( int i = 0; i < vehicleTypes.count(); ++i ) {
        VehicleType vehicleType = vehicleTypes[i];
        if ( vehiclesInCurrentRow == vehiclesPerRow ) {
            vehiclesInCurrentRow = 0;
            if ( vehicleTypes.count() - i < vehiclesPerRow ) {
                x = shadowWidth + vehicleOffsetX / 2.0;
            } else {
                x = shadowWidth;
            }
            y += vehicleOffsetY;
        }

        QString vehicleKey;
        switch ( vehicleType ) {
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
                kDebug() << "Unknown vehicle type" << vehicleType;
                painter->drawEllipse( QRectF(x, y, iconSize.width(), iconSize.height())
                                    .adjusted(5, 5, -5, -5) );
                painter->drawText( QRectF(x, y, iconSize.width(), iconSize.height()),
                                "?", QTextOption(Qt::AlignCenter) );
        }
        if ( !m_parent->svg()->hasElement(vehicleKey) ) {
            kDebug() << "SVG element" << vehicleKey << "not found";
        } else {
            // Draw SVG vehicle element into pixmap
            m_parent->svg()->paint( &p, x, y, vehicleKey );
        }

        ++vehiclesInCurrentRow;
        x += vehicleOffsetX;
    }
    p.end();

    QPixmap fadePixmap( pixmap.size() );
    fadePixmap.fill( Qt::transparent );
    QPainter p2( &fadePixmap );

    // Create shadow for the SVG element and draw the SVG and it's shadow.
    QImage shadow = pixmap.toImage();
    Plasma::PaintUtils::shadowBlur( shadow, shadowWidth - 1, Qt::black );
    p2.drawImage( QPoint(1, 2), shadow );
    p2.drawPixmap( QPoint(0, 0), pixmap );

    // Make startTransitionPixmap more transparent (for fading)
    p2.setCompositionMode( QPainter::CompositionMode_DestinationIn );
    QLinearGradient gradient( pixmap.width() / 4, 0, pixmap.width(), 0 );
    gradient.setColorAt( 0.0, Qt::black );
    gradient.setColorAt( 1.0, Qt::transparent );
    p2.fillRect( fadePixmap.rect(), QBrush(gradient) );
    p2.end();

    painter->drawPixmap( _vehicleRect.topLeft(), fadePixmap );

    // Draw text
    QColor _textColor = textColor();
    bool drawHalos = /*m_options.testFlag(DrawShadows) &&*/ qGray(_textColor.rgb()) < 128;
    painter->setPen( _textColor );
    TextDocumentHelper::drawTextDocument( painter, option, m_infoTextDocument,
                                          _infoRect.toRect(), drawHalos );

    // Draw extra icon(s)
    QRectF _extraIconRect;
    if ( hasExtraIcon() ) {
        QModelIndex modelIndex = index().model()->index( index().row(), ColumnTarget );
        QIcon icon = modelIndex.data( Qt::DecorationRole ).value<QIcon>();
        _extraIconRect = extraIconRect(rect/*, departureTimeWidth, arrivalTimeWidth*/);
        painter->drawPixmap( _extraIconRect.toRect(), icon.pixmap(extraIconSize()) );
    }
    if ( hasExtraIcon(ColumnDeparture) ) {
        if ( _extraIconRect.isValid() ) {
            // Move icon rect to the left
            _extraIconRect.moveRight( _extraIconRect.left() - 4 );
        } else {
            _extraIconRect = extraIconRect(rect/*, departureTimeWidth, arrivalTimeWidth*/);
        }

        QModelIndex modelIndex = index().model()->index( index().row(), ColumnDeparture );
        QIcon icon = modelIndex.data( Qt::DecorationRole ).value<QIcon>();
        painter->drawPixmap( _extraIconRect.toRect(), icon.pixmap(extraIconSize()) );
    }
}

QColor PublicTransportGraphicsItem::textColor() const
{
    DepartureModel *model = qobject_cast<DepartureModel*>( m_item->model() );
    bool manuallyHighlighted = false;
    if ( model ) {
        // Only proceed with highlighted stops, if the model is a DepartureModel (not a JourneyModel)
        DepartureItem *item = qobject_cast<DepartureItem*>( m_item.data() );
        manuallyHighlighted = item->departureInfo()->routeStops()
                .contains( model->highlightedStop(), Qt::CaseInsensitive );
    }

    if ( manuallyHighlighted ) {
        QColor highlightColor = KColorUtils::mix(
                Plasma::Theme::defaultTheme()->color(Plasma::Theme::HighlightColor),
                palette().color(QPalette::Active, QPalette::Text), 0.5 );
        return highlightColor;
    } else {
        #if KDE_VERSION < KDE_MAKE_VERSION(4,6,0)
            QColor color = Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor);
        #else
            QColor color = Plasma::Theme::defaultTheme()->color(Plasma::Theme::ViewTextColor);
        #endif

        // Mix with group color if not highlighted
        QColor groupColor = index().data(Qt::BackgroundColorRole).value<QColor>();
        if ( groupColor != Qt::transparent ) {
            color = KColorUtils::mix( color, groupColor, 0.2 );
        }

        return color;
    }
}

QColor PublicTransportGraphicsItem::backgroundColor() const
{
    #if KDE_VERSION < KDE_MAKE_VERSION(4,6,0)
        return Plasma::Theme::defaultTheme()->color(Plasma::Theme::BackgroundColor);
    #else
        return Plasma::Theme::defaultTheme()->color(Plasma::Theme::ViewBackgroundColor);
    #endif
}

void DepartureGraphicsItem::paintBackground( QPainter* painter,
        const QStyleOptionGraphicsItem* option, const QRectF& rect )
{
    Q_UNUSED( option );
    QColor borderColor = textColor();
    borderColor.setAlphaF( 0.5 );

    QRect pixmapRect( 0, 0, rect.width(), rect.height() );
    QPixmap pixmap( pixmapRect.size() );

    // Get the background color for departures in color groups / alternative background colors
    QColor backgroundColor = index().data( Qt::BackgroundColorRole ).value<QColor>();
    if ( backgroundColor == Qt::transparent && index().row() % 2 == 1 ) {
        QColor alternateBackgroundColor = KColorScheme( QPalette::Active, KColorScheme::View )
                .background( KColorScheme::AlternateBackground ).color();
        backgroundColor =  KColorUtils::mix( backgroundColor, alternateBackgroundColor, 0.4 );
    }

    // Fill the pixmap with the mixed background color
    pixmap.fill( backgroundColor );

    // Draw special background for departures with an alarm
    QPainter p( &pixmap );
    if ( index().data(DrawAlarmBackgroundRole).toBool() ) {
//      qreal bias = index().data( AlarmColorIntensityRole ).toReal();
        drawAlarmBackground( &p, pixmapRect );
    }

    // Draw a line at the bottom of this TimetableItem
    p.setPen( borderColor );
    p.drawLine( pixmapRect.bottomLeft(), pixmapRect.bottomRight() );

    // Fade out to the left and right
    drawFadeOutLeftAndRight( &p, pixmapRect );
    p.end();

    painter->drawPixmap( rect.toRect(), pixmap );
}

void PublicTransportGraphicsItem::drawAlarmBackground( QPainter *painter, const QRect &rect )
{
    // alarmColor is oxygen color "brick red5", with an alpha value added
    const QColor alarmColor( 191, 3, 3, 180 );
    // Draw the alarm gradients over the first and last third vertically
    const int alarmHeight = unexpandedHeight() / 3;

    // Draw the gradient at the top
    QLinearGradient alarmGradientTop( 0, 0, 0, alarmHeight );
    alarmGradientTop.setColorAt( 0, alarmColor );
    alarmGradientTop.setColorAt( 1, Qt::transparent );
    painter->fillRect( QRect(0, 0, rect.width(), alarmHeight), alarmGradientTop );

    // Draw the gradient at the bottom
    QLinearGradient alarmGradientBottom( 0, rect.height() - alarmHeight, 0, rect.height() );
    alarmGradientBottom.setColorAt( 0, Qt::transparent );
    alarmGradientBottom.setColorAt( 1, alarmColor );
    painter->fillRect( QRect(0, rect.height() - alarmHeight, rect.width(), alarmHeight),
                       alarmGradientBottom );
}

void PublicTransportGraphicsItem::drawFadeOutLeftAndRight( QPainter* painter, const QRect& rect,
                                                           int fadeWidth )
{
    painter->setCompositionMode( QPainter::CompositionMode_DestinationIn );
    QLinearGradient alphaGradient( 0, 0, 1, 0 );
    alphaGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
    alphaGradient.setColorAt( 0, Qt::transparent );
    alphaGradient.setColorAt( 1, Qt::black );
    // Fade out on the left
    painter->fillRect( QRect(rect.left(), rect.top(), fadeWidth, rect.height()), alphaGradient );

    alphaGradient.setColorAt( 0, Qt::black );
    alphaGradient.setColorAt( 1, Qt::transparent );
    // Fade out on the right (the +1 is to be sure, to not have a 1 pixel line on the right, which
    // isn't made transparent at all)
    painter->fillRect( QRect(rect.right() - fadeWidth, rect.top(), fadeWidth + 1, rect.height()),
                       alphaGradient );
}

void DepartureGraphicsItem::paintItem( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                       const QRectF &rect )
{
    QColor _backgroundColor = backgroundColor(); // TODO: store these colors in option->palette?
    QColor _textColor = textColor();
    QFontMetrics fm( font() );
    const QRectF _vehicleRect = vehicleRect( rect );
    const QRectF _timeRect = timeRect( rect );
    const qreal timeWidth = timeColumnWidth();
    const QRectF _infoRect = infoRect( rect, timeWidth );

    int shadowWidth = 4;
    QSizeF iconSize( _vehicleRect.width() - 2 * shadowWidth, _vehicleRect.height() - 2 * shadowWidth );
    QString vehicleKey;
    switch ( departureItem()->departureInfo()->vehicleType() ) {
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
            kDebug() << "Unknown vehicle type" << departureItem()->departureInfo()->vehicleType();
            painter->setPen( _textColor );
            painter->setBrush( _backgroundColor );
            painter->drawEllipse( QRectF(shadowWidth, shadowWidth, iconSize.width(), iconSize.height())
                                .adjusted(2, 2, -2, -2) );
            painter->drawText( QRectF(shadowWidth, shadowWidth, iconSize.width(), iconSize.height()),
                            "?", QTextOption(Qt::AlignCenter) );
    }

    const QString vehicleCacheKey
            = vehicleKey + QString("%1%2").arg( iconSize.width() ).arg( iconSize.height() );
    QPixmap vehiclePixmap;
    if ( !m_pixmapCache || !m_pixmapCache->find(vehicleCacheKey, vehiclePixmap) ) {
        if ( !m_parent->svg()->hasElement(vehicleKey) ) {
            kDebug() << "SVG element" << vehicleKey << "not found";
        } else {
            // Draw SVG vehicle element into pixmap
            QPixmap pixmap( (int)_vehicleRect.width(), (int)_vehicleRect.height() );
            pixmap.fill( Qt::transparent );
            QPainter p( &pixmap );
            m_parent->svg()->resize( iconSize );
            m_parent->svg()->paint( &p, shadowWidth, shadowWidth, vehicleKey );

            vehiclePixmap = QPixmap( pixmap.size() );
            vehiclePixmap.fill( Qt::transparent );
            QPainter p2( &vehiclePixmap );

            // Create shadow for the SVG element and draw the SVG and it's shadow.
            QImage shadow = pixmap.toImage();
            Plasma::PaintUtils::shadowBlur( shadow, shadowWidth - 1, Qt::black );
            p2.drawImage( QPoint(1, 2), shadow );
            p2.drawPixmap( QPoint(0, 0), pixmap );

            // Make startTransitionPixmap more transparent (for fading)
            p2.setCompositionMode( QPainter::CompositionMode_DestinationIn );
            QLinearGradient gradient( pixmap.width() / 4, 0, pixmap.width(), 0 );
            gradient.setColorAt( 0.0, Qt::black );
            gradient.setColorAt( 1.0, Qt::transparent );
            p2.fillRect( vehiclePixmap.rect(), QBrush(gradient) );
            p2.end();

            if ( m_pixmapCache ) {
                m_pixmapCache->insert( vehicleCacheKey, vehiclePixmap );
            }
        }
    }
    if ( !vehicleKey.isEmpty() ) {
        painter->drawPixmap( _vehicleRect.topLeft(), vehiclePixmap );
    }

    // Draw text
    bool drawHalos = /*m_options.testFlag(DrawShadows) &&*/ qGray(_textColor.rgb()) < 128;
    painter->setPen( _textColor );

    DepartureModel *model = qobject_cast<DepartureModel*>( m_item->model() );
    bool manuallyHighlighted = false;
    if ( model ) {
        // Only proceed with highlighted stops, if the model is a DepartureModel (not a JourneyModel)
        DepartureItem *item = qobject_cast<DepartureItem*>( m_item.data() );
        manuallyHighlighted = item->departureInfo()->routeStops()
                .contains( model->highlightedStop(), Qt::CaseInsensitive );
    }

    if ( m_highlighted != manuallyHighlighted ) {
        kDebug() << "Highlighting changed from" << m_highlighted << "to" << manuallyHighlighted;
        QFont _font = font();
        if ( manuallyHighlighted ) {
            _font.setItalic( true );
        }
        m_infoTextDocument->setDefaultFont( _font );
        m_timeTextDocument->setDefaultFont( _font );
        m_highlighted = manuallyHighlighted;
    }

    TextDocumentHelper::drawTextDocument( painter, option, m_infoTextDocument,
            _infoRect.toRect(), drawHalos );
    TextDocumentHelper::drawTextDocument( painter, option, m_timeTextDocument,
            _timeRect.toRect(), drawHalos );

    // Draw extra icon(s), eg. an alarm icon or an indicator for additional news for a journey
    QRectF _extraIconRect;
    if ( hasExtraIcon() ) {
        QModelIndex modelIndex = index().model()->index( index().row(), ColumnTarget );
        QIcon icon = modelIndex.data( Qt::DecorationRole ).value<QIcon>();
        _extraIconRect = extraIconRect( rect, timeWidth );
        painter->drawPixmap( _extraIconRect.toRect(), icon.pixmap(extraIconSize()) );
    }
    if ( hasExtraIcon(ColumnDeparture) ) {
        if ( _extraIconRect.isValid() ) {
            // Move icon rect to the left
            _extraIconRect.moveRight( _extraIconRect.left() - 4 );
        } else {
            _extraIconRect = extraIconRect(rect, timeWidth);
        }

        QModelIndex modelIndex = index().model()->index( index().row(), ColumnDeparture );
        QIcon icon = modelIndex.data( Qt::DecorationRole ).value<QIcon>();
        painter->drawPixmap( _extraIconRect.toRect(), icon.pixmap(extraIconSize()) );
    }
}

void JourneyGraphicsItem::paintExpanded( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                         const QRectF& rect )
{
    painter->setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );
    QColor _textColor = textColor();
    bool drawHalos = /*m_options.testFlag(DrawShadows) &&*/ qGray(_textColor.rgb()) < 128;

    qreal y = rect.top() - padding();
    if ( m_routeItem ) {
        y += m_routeItem->size().height() + padding();
    }

    if ( y > rect.bottom() ) {
        return; // Currently not expanded enough to show more information (is animating)
    }

    QString html;
    ChildItem *operatorItem = m_item->childByType( OperatorItem );
    ChildItem *journeyNewsItem = m_item->childByType( JourneyNewsItem );
    ChildItem *pricingItem = m_item->childByType( PricingItem );
    if ( operatorItem ) {
        if ( !html.isEmpty() ) {
            html.append("<br />");
        }
        html.append( operatorItem->formattedText() );
    }
    if ( journeyNewsItem ) {
        if ( !html.isEmpty() ) {
            html.append("<br />");
        }
        html.append( journeyNewsItem->formattedText() );
    }
    if ( pricingItem ) {
        if ( !html.isEmpty() ) {
            html.append("<br />");
        }
        html.append( pricingItem->formattedText() );
    }

    if ( !html.isEmpty() ) {
        QFontMetrics fm( font() );
        QRectF htmlRect( rect.left(), y, rect.width(), rect.bottom() - y );

        // Create layout for the departure time column
        QTextDocument additionalInformationTextDocument;
        additionalInformationTextDocument.setDefaultFont( font() );
        QTextOption textOption( Qt::AlignVCenter | Qt::AlignLeft );
        additionalInformationTextDocument.setDefaultTextOption( textOption );
        additionalInformationTextDocument.setDocumentMargin( 0 );
        additionalInformationTextDocument.setPageSize( htmlRect.size() );
        additionalInformationTextDocument.setHtml( html );
        additionalInformationTextDocument.documentLayout();

        painter->setPen( _textColor );
        TextDocumentHelper::drawTextDocument( painter, option, &additionalInformationTextDocument,
                                              htmlRect.toRect(), drawHalos );
    }
}

void DepartureGraphicsItem::paintExpanded( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                           const QRectF& rect )
{
    painter->setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );
    QColor _textColor = textColor();
    bool drawHalos = /*m_options.testFlag(DrawShadows) &&*/ qGray(_textColor.rgb()) < 128;

    qreal y = rect.top() - padding();
    if ( m_routeItem ) {
        y += m_routeItem->size().height() /*+ padding()*/;
    }

    if ( y > rect.bottom() ) {
        return; // Currently not expanded enough to show more information (is animating)
    }

    // Draw other inforamtion items
    QString html;
    ChildItem *delayItem = m_item->childByType( DelayItem );
    ChildItem *platformItem = m_item->childByType( PlatformItem );
    ChildItem *operatorItem = m_item->childByType( OperatorItem );
    ChildItem *journeyNewsItem = m_item->childByType( JourneyNewsItem );
    if ( delayItem ) {
        html.append( delayItem->formattedText() );
    }
    if ( platformItem ) {
        if ( !html.isEmpty() ) {
            html.append("<br />");
        }
        html.append( platformItem->formattedText() );
    }
    if ( operatorItem ) {
        if ( !html.isEmpty() ) {
            html.append("<br />");
        }
        html.append( operatorItem->formattedText() );
    }
    if ( journeyNewsItem ) {
        if ( !html.isEmpty() ) {
            html.append("<br />");
        }
        html.append( journeyNewsItem->formattedText() );
    }

    if ( !html.isEmpty() ) {
        QFontMetrics fm( font() );
        QRectF htmlRect( rect.left(), y, rect.width(), rect.bottom() - y );

        // Create layout for the departure time column
        QTextDocument additionalInformationTextDocument;
        additionalInformationTextDocument.setDefaultFont( font() );
        QTextOption textOption( Qt::AlignVCenter | Qt::AlignLeft );
        additionalInformationTextDocument.setDefaultTextOption( textOption );
        additionalInformationTextDocument.setDocumentMargin( 0 );
        additionalInformationTextDocument.setPageSize( htmlRect.size() );
        additionalInformationTextDocument.setHtml( html );
        additionalInformationTextDocument.documentLayout();

        painter->setPen( _textColor );
        TextDocumentHelper::drawTextDocument( painter, option, &additionalInformationTextDocument,
                                              htmlRect.toRect(), drawHalos );
    }
}

qreal JourneyGraphicsItem::expandAreaHeight() const
{
    if ( !m_item || qFuzzyIsNull(m_expandStep) ) {
        return 0.0;
    }

    qreal height = padding();
    if ( m_routeItem ) {
        height += m_routeItem->size().height() + padding();
    }

    qreal extraInformationHeight = 0.0;
    QFontMetrics fm( font() );
    ChildItem *delayItem = m_item->childByType( DelayItem );
    if ( delayItem ) {
        extraInformationHeight += 2 * fm.height();
    }

    ChildItem *operatorItem = m_item->childByType( OperatorItem );
    if ( operatorItem ) {
        extraInformationHeight += fm.height();
    }

    ChildItem *journeyNewsItem = m_item->childByType( JourneyNewsItem );
    if ( journeyNewsItem ) {
        extraInformationHeight += 3 * fm.height();
    }

    ChildItem *pricingItem = m_item->childByType( PricingItem );
    if ( pricingItem ) {
        extraInformationHeight += fm.height();
    }

    if ( extraInformationHeight != 0.0 ) {
        height += extraInformationHeight + padding();
    }

    return height * m_expandStep;
}

qreal DepartureGraphicsItem::expandAreaHeight() const
{
    if ( !m_item || qFuzzyIsNull(m_expandStep) ) {
        return 0.0;
    }

    qreal height = padding();
    const DepartureInfo *info = departureItem()->departureInfo();
    if ( info->routeStops().count() >= 2 ) {
        height += ROUTE_ITEM_HEIGHT * m_parent->zoomFactor() + padding();
    }

    qreal extraInformationHeight = 0.0;
    QFontMetrics fm( font() );
    ChildItem *delayItem = m_item->childByType( DelayItem );
    if ( delayItem ) {
        extraInformationHeight += 2 * fm.height();
    }

    ChildItem *operatorItem = m_item->childByType( OperatorItem );
    if ( operatorItem ) {
        extraInformationHeight += fm.height();
    }

    ChildItem *platformItem = m_item->childByType( PlatformItem );
    if ( platformItem ) {
        extraInformationHeight += fm.height();
    }

    ChildItem *journeyNewsItem = m_item->childByType( JourneyNewsItem );
    if ( journeyNewsItem ) {
        extraInformationHeight += 3 * fm.height();
    }
    if ( extraInformationHeight != 0.0 ) {
        height += extraInformationHeight + padding();
    }

    return height * m_expandStep;
}

qreal DepartureGraphicsItem::expandAreaIndentation() const
{
    return m_parent->iconSize() * 0.65 + padding();
}

qreal JourneyGraphicsItem::expandAreaIndentation() const
{
    return m_parent->iconSize() * 0.65 + padding();
}

QSizeF PublicTransportGraphicsItem::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
{
    if ( which == Qt::MinimumSize ) {
        return QSizeF( 100, m_fadeOut *
                (m_expanded || !qFuzzyIsNull(m_expandStep)
                ? qFloor(unexpandedHeight() + expandAreaHeight()) : qFloor(unexpandedHeight())) );
    } else if ( which == Qt::MaximumSize ) {
        return QSizeF( 100000, m_fadeOut *
                (m_expanded || !qFuzzyIsNull(m_expandStep)
                ? qFloor(unexpandedHeight() + expandAreaHeight()) : qFloor(unexpandedHeight())) );
    } else {
        return QGraphicsWidget::sizeHint( which, constraint );
    }
}

QSizeF PublicTransportWidget::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
{
    if ( which == Qt::MinimumSize ) {
        return QSizeF( 100, 50 );
    } else {
        return Plasma::ScrollWidget::sizeHint(which, constraint);
    }
}

PublicTransportWidget::PublicTransportWidget( QGraphicsItem* parent )
    : Plasma::ScrollWidget( parent ), m_model(0), m_svg(0), m_copyStopToClipboardAction(0),
      m_showInMapAction(0)/*,
      m_toggleAlarmAction(0)*/
{
    setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    setupActions();

    QGraphicsWidget *container = new QGraphicsWidget( this );
    QGraphicsLinearLayout *l = new QGraphicsLinearLayout( Qt::Vertical, container );
    l->setSpacing( 0.0 );
    container->setLayout( l );
    setWidget( container );

    m_maxLineCount = 2;
    m_iconSize = 32;
    m_zoomFactor = 1.0;
}

void PublicTransportWidget::setupActions()
{
    m_copyStopToClipboardAction = new StopAction( StopAction::CopyStopNameToClipboard, this );
    connect( m_copyStopToClipboardAction, SIGNAL(stopActionTriggered(StopAction::Type,QString)),
             this, SIGNAL(requestStopAction(StopAction::Type,QString)) );

    if ( Plasma::DataEngineManager::listAllEngines().contains("openstreetmap") ) {
        m_showInMapAction = new StopAction( StopAction::ShowStopInMap, this );
        connect( m_showInMapAction, SIGNAL(stopActionTriggered(StopAction::Type,QString)),
                 this, SIGNAL(requestStopAction(StopAction::Type,QString)) );
    } else {
        kDebug() << "Not using 'Show Stop in Map' action, because the 'openstreetmap' "
                    "data engine isn't installed!";
    }
}

JourneyTimetableWidget::JourneyTimetableWidget( QGraphicsItem* parent )
    : PublicTransportWidget(parent), m_requestJourneyToStopAction(0), m_requestJourneyFromStopAction(0)
{
    setupActions();
}

TimetableWidget::TimetableWidget( QGraphicsItem* parent )
    : PublicTransportWidget(parent), m_showDeparturesAction(0), m_highlightStopAction(0),
      m_newFilterViaStopAction(0), m_pixmapCache(new KPixmapCache("PublicTransportVehicleIcons"))
{
    m_targetHidden = false;
    setupActions();
}

void TimetableWidget::setupActions()
{
    PublicTransportWidget::setupActions();

    m_showDeparturesAction = new StopAction( StopAction::ShowDeparturesForStop, this );
    m_highlightStopAction= new StopAction( StopAction::HighlightStop, this );
    m_newFilterViaStopAction = new StopAction( StopAction::CreateFilterForStop, this );
//     m_toggleAlarmAction = new QAction( KIcon("task-reminder"),
//             i18n("Toggle &Alarm For This D"), this );
    connect( m_showDeparturesAction, SIGNAL(stopActionTriggered(StopAction::Type,QString)),
             this, SIGNAL(requestStopAction(StopAction::Type,QString)) );
    connect( m_highlightStopAction, SIGNAL(stopActionTriggered(StopAction::Type,QString)),
             this, SIGNAL(requestStopAction(StopAction::Type,QString)) );
    connect( m_newFilterViaStopAction, SIGNAL(stopActionTriggered(StopAction::Type,QString)),
             this, SIGNAL(requestStopAction(StopAction::Type,QString)) );
}

void JourneyTimetableWidget::setupActions()
{
    PublicTransportWidget::setupActions();

    m_requestJourneyToStopAction = new StopAction( StopAction::RequestJourneysToStop, this );
    m_requestJourneyFromStopAction = new StopAction( StopAction::RequestJourneysFromStop, this );
    connect( m_requestJourneyToStopAction, SIGNAL(stopActionTriggered(StopAction::Type,QString)),
             this, SIGNAL(requestStopAction(StopAction::Type,QString)) );
    connect( m_requestJourneyFromStopAction, SIGNAL(stopActionTriggered(StopAction::Type,QString)),
             this, SIGNAL(requestStopAction(StopAction::Type,QString)) );
}

void PublicTransportWidget::setModel( PublicTransportModel* model )
{
    m_model = model;

    connect( m_model, SIGNAL(rowsInserted(QModelIndex,int,int)),
             this, SLOT(rowsInserted(QModelIndex,int,int)) );
    connect( m_model, SIGNAL(itemsAboutToBeRemoved(QList<ItemBase*>)),
             this, SLOT(itemsAboutToBeRemoved(QList<ItemBase*>)) );
    connect( m_model, SIGNAL(rowsRemoved(QModelIndex,int,int)),
             this, SLOT(rowsRemoved(QModelIndex,int,int)) );
    connect( m_model, SIGNAL(modelReset()), this, SLOT(modelReset()) );
    connect( m_model, SIGNAL(layoutChanged()), this, SLOT(layoutChanged()) );
    connect( m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
             this, SLOT(dataChanged(QModelIndex,QModelIndex)) );
}

PublicTransportGraphicsItem* PublicTransportWidget::item( const QModelIndex& index )
{
    foreach ( PublicTransportGraphicsItem *item, m_items ) {
        if ( item->index() == index ) {
            return item;
        }
    }

    return 0;
}

void PublicTransportWidget::setZoomFactor( qreal zoomFactor )
{
    m_zoomFactor = zoomFactor;

    for ( int i = 0; i < m_items.count(); ++i ) {
        // Notify children about changed settings
        m_items[i]->updateSettings();
    }
    update();
}

void PublicTransportWidget::contextMenuEvent( QGraphicsSceneContextMenuEvent* event )
{
    PublicTransportGraphicsItem *item = dynamic_cast<PublicTransportGraphicsItem*>(
            scene()->itemAt(event->scenePos()) );
    if ( item ) {
        event->accept();
        emit contextMenuRequested( item, event->pos() );
    } else {
        QGraphicsItem::contextMenuEvent( event );
    }
}

void PublicTransportWidget::paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                   QWidget* widget )
{
    QGraphicsWidget::paint(painter, option, widget);

    if ( m_items.isEmpty() && !m_noItemsText.isEmpty() ) {
        painter->drawText( boundingRect(), m_noItemsText, QTextOption(Qt::AlignCenter) );
    }
}

void PublicTransportWidget::updateItemLayouts()
{
    foreach ( PublicTransportGraphicsItem *item, m_items ) {
        item->updateTextLayouts();
    }
}

void PublicTransportWidget::updateItemGeometries()
{
    foreach ( PublicTransportGraphicsItem *item, m_items ) {
        item->updateGeometry();
    }
}

void PublicTransportWidget::modelReset()
{
    qDeleteAll( m_items );
    m_items.clear();
}

void TimetableWidget::dataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight )
{
    if ( !topLeft.isValid() || !bottomRight.isValid() ) {
        return;
    }
    for ( int row = topLeft.row(); row <= bottomRight.row() && row < m_model->rowCount(); ++row ) {
        departureItem( row )->updateData( static_cast<DepartureItem*>(m_model->item(row)), true );
    }
}

void JourneyTimetableWidget::dataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight )
{
    if ( !topLeft.isValid() || !bottomRight.isValid() ) {
        return;
    }
    for ( int row = topLeft.row(); row <= bottomRight.row() && row < m_model->rowCount(); ++row ) {
        journeyItem( row )->updateData( static_cast<JourneyItem*>(m_model->item(row)), true );
    }
}

void PublicTransportWidget::layoutChanged()
{

}

void JourneyTimetableWidget::rowsInserted( const QModelIndex& parent, int first, int last )
{
    if ( parent.isValid() ) {
        kDebug() << "Item with parent" << parent << "Inserted" << first << last;
        return;
    }

    QGraphicsLinearLayout *l = static_cast<QGraphicsLinearLayout*>( widget()->layout() );
    for ( int row = first; row <= last; ++row ) {
        JourneyGraphicsItem *item = new JourneyGraphicsItem( this, widget(),
                m_copyStopToClipboardAction, m_showInMapAction,
                m_requestJourneyToStopAction, m_requestJourneyFromStopAction );
        item->updateData( static_cast<JourneyItem*>(m_model->item(row)) );
        connect( item, SIGNAL(requestAlarmCreation(QDateTime,QString,VehicleType,QString,QGraphicsWidget*)),
                 this, SIGNAL(requestAlarmCreation(QDateTime,QString,VehicleType,QString,QGraphicsWidget*)) );
        connect( item, SIGNAL(requestAlarmDeletion(QDateTime,QString,VehicleType,QString,QGraphicsWidget*)),
                 this, SIGNAL(requestAlarmDeletion(QDateTime,QString,VehicleType,QString,QGraphicsWidget*)) );
        m_items.insert( row, item );

        // Fade new items in
        Plasma::Animation *fadeAnimation = Plasma::Animator::create(
                Plasma::Animator::FadeAnimation, item );
        fadeAnimation->setTargetWidget( item );
        fadeAnimation->setProperty( "startOpacity", 0.0 );
        fadeAnimation->setProperty( "targetOpacity", 1.0 );
        fadeAnimation->start( QAbstractAnimation::DeleteWhenStopped );

        l->insertItem( row, item );
    }
}

void TimetableWidget::rowsInserted( const QModelIndex& parent, int first, int last )
{
    if ( parent.isValid() ) {
        kDebug() << "Item with parent" << parent << "Inserted" << first << last;
        return;
    }

    QGraphicsLinearLayout *l = static_cast<QGraphicsLinearLayout*>( widget()->layout() );
    for ( int row = first; row <= last; ++row ) {
        DepartureGraphicsItem *item = new DepartureGraphicsItem( this, widget(),
                m_copyStopToClipboardAction, m_showInMapAction, m_showDeparturesAction,
                m_highlightStopAction, m_newFilterViaStopAction, m_pixmapCache );
        item->updateData( static_cast<DepartureItem*>(m_model->item(row)) );
        m_items.insert( row, item );

        // Fade new items in
        Plasma::Animation *fadeAnimation = Plasma::Animator::create(
                Plasma::Animator::FadeAnimation, item );
        fadeAnimation->setTargetWidget( item );
        fadeAnimation->setProperty( "startOpacity", 0.0 );
        fadeAnimation->setProperty( "targetOpacity", 1.0 );
        fadeAnimation->start( QAbstractAnimation::DeleteWhenStopped );

        l->insertItem( row, item );
    }
}

void PublicTransportWidget::itemsAboutToBeRemoved( const QList< ItemBase* >& items )
{
    // Capture pixmaps for departures that will get removed
    // to be able to animate it's disappearance
    foreach ( const ItemBase *item, items ) {
        if ( item->row() >= m_items.count() ) {
            kDebug() << "Index out of bounds!";
            continue;
        }

        PublicTransportGraphicsItem *timetableItem = m_items[ item->row() ];
        timetableItem->capturePixmap();
    }
}

void PublicTransportWidget::rowsRemoved( const QModelIndex& parent, int first, int last )
{
    if ( parent.isValid() ) {
        kDebug() << "Item with parent" << parent << "Removed" << first << last;
        return;
    }

    if ( last >= m_items.count() ) {
        kDebug() << "Cannot remove item, out of bounds:" << first << last;
        last = m_items.count() - 1;
    }

    if ( first == 0 && last == m_items.count() - 1 ) {
        // All items get removed, the shrink animations wouldn't be smooth
        for ( int row = last; row >= first; --row ) {
            PublicTransportGraphicsItem *item = m_items.takeAt( row );

            // Fade old items out
            Plasma::Animation *fadeAnimation = Plasma::Animator::create(
                    Plasma::Animator::FadeAnimation, item );
            fadeAnimation->setTargetWidget( item );
            fadeAnimation->setProperty( "startOpacity", 1.0 );
            fadeAnimation->setProperty( "targetOpacity", 0.0 );
            connect( fadeAnimation, SIGNAL(finished()), item, SLOT(deleteLater()) );
            fadeAnimation->start( QAbstractAnimation::DeleteWhenStopped );
        }
    } else {
        // Only some items get removed, most probably they're currently departing
        for ( int row = last; row >= first; --row ) {
            PublicTransportGraphicsItem *item = m_items.takeAt( row );

            // Shrink departing items
            QPropertyAnimation *shrinkAnimation = new QPropertyAnimation( item, "fadeOut" );
            shrinkAnimation->setEasingCurve( QEasingCurve(QEasingCurve::InOutQuart) );
            shrinkAnimation->setStartValue( item->fadeOut() );
            shrinkAnimation->setEndValue( 0.0 );
            shrinkAnimation->setDuration( 1000 );
            connect( shrinkAnimation, SIGNAL(finished()), item, SLOT(deleteLater()) );
            shrinkAnimation->start( QAbstractAnimation::DeleteWhenStopped );
        }
    }
}

QRectF JourneyGraphicsItem::vehicleRect(const QRectF& rect) const
{
    return QRectF( rect.left(), rect.top() + (unexpandedHeight() - m_parent->iconSize()) / 2,
                   m_parent->iconSize(), m_parent->iconSize() );
}

QRectF JourneyGraphicsItem::infoRect( const QRectF& rect ) const
{
    qreal indentation = expandAreaIndentation();
    return QRectF( rect.left() + indentation, rect.top(),
                   rect.width() - indentation - padding()
                   - (hasExtraIcon() ? m_parent->iconSize() + padding() : 0),
                   unexpandedHeight() );
}

QRectF JourneyGraphicsItem::extraIconRect( const QRectF& rect ) const
{
    return QRectF( rect.right() - extraIconSize() - 2 * padding(),
                   rect.top() + (unexpandedHeight() - extraIconSize()) / 2,
                   extraIconSize(), extraIconSize() );
}

QRectF DepartureGraphicsItem::vehicleRect(const QRectF& rect) const {
    return QRectF( rect.left(), rect.top() + (unexpandedHeight() - m_parent->iconSize()) / 2,
                   m_parent->iconSize(), m_parent->iconSize() );
}

QRectF DepartureGraphicsItem::infoRect(const QRectF& rect, qreal timeColumnWidth) const {
    qreal indentation = expandAreaIndentation();
    return QRectF( rect.left() + indentation, rect.top(),
                   rect.width() - indentation - padding() - timeColumnWidth
                   - (hasExtraIcon() ? m_parent->iconSize() + padding() : 0),
                   unexpandedHeight() );
}

QRectF DepartureGraphicsItem::extraIconRect(const QRectF& rect, qreal timeColumnWidth) const {
    return QRectF( rect.right() - timeColumnWidth - extraIconSize() - 2 * padding(),
                   rect.top() + (unexpandedHeight() - extraIconSize()) / 2,
                   extraIconSize(), extraIconSize() );
}

QRectF DepartureGraphicsItem::timeRect(const QRectF& rect) const {
    if ( qobject_cast<TimetableWidget*>(m_parent)->isTargetHidden() ) {
        return QRectF( rect.width() / 4, rect.top(),
                       rect.width() * 3 / 4 - padding(), unexpandedHeight() );
    } else {
        return QRectF( rect.width() / 2, rect.top(),
                       rect.width() / 2 - padding(), unexpandedHeight() );
    }
}

QGraphicsWidget* DepartureGraphicsItem::routeItem() const
{
    return m_routeItem;
}

QGraphicsWidget* JourneyGraphicsItem::routeItem() const
{
    return m_routeItem;
}
