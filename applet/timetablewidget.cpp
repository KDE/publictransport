/*
*   Copyright 2013 Friedrich Pülz <fpuelz@gmx.de>
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
#include <Plasma/BusyWidget>
#include <Plasma/Label>

// KDE includes
#include <KColorScheme>
#include <KColorUtils>
#include <KMenu>
#include <KPixmapCache>
#include <KAction>
#include <KToolInvocation>

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
#include <QParallelAnimationGroup>
#include <QStyleOption>
#include <QAbstractTextDocumentLayout>
#include <QLabel>
#include <qmath.h>
#include <QTimer>

PublicTransportGraphicsItem::PublicTransportGraphicsItem(
        PublicTransportWidget *publicTransportWidget, QGraphicsItem *parent,
        StopAction *copyStopToClipboardAction, StopAction *showInMapAction/*, QAction *toggleAlarmAction*/ )
        : QGraphicsWidget(parent), m_item(0), m_parent(publicTransportWidget),
        m_resizeAnimation(0), m_pixmap(0), m_copyStopToClipboardAction(copyStopToClipboardAction),
        m_showInMapAction(showInMapAction), m_ensureVisibleTimer(0)/*,
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

QList< QAction* > JourneyTimetableWidget::contextMenuActions()
{
    QList< QAction* > actions;
    if ( m_earlierAction ) {
        actions << m_earlierAction;
    }
    if ( m_laterAction ) {
        actions << m_laterAction;
    }
    return actions;
}

void DepartureGraphicsItem::updateSettings()
{
    if ( m_routeItem ) {
        m_routeItem->setZoomFactor( m_parent->zoomFactor() );
        updateGeometry();
        update();
    }
}

void JourneyGraphicsItem::updateSettings()
{
    if ( m_routeItem ) {
        m_routeItem->setZoomFactor( m_parent->zoomFactor() );
        updateGeometry();
        update();
    }
}

void PublicTransportGraphicsItem::setExpandedNotAffectingOtherItems( bool expand )
{
    if ( m_expanded == expand ) {
        return; // Unchanged
    }

    m_expanded = expand;
    if ( expand ) {
        QGraphicsWidget *route = routeItem();
        if ( route ) {
            route->setVisible( true );
        }
    }

    if ( m_resizeAnimation ) {
        m_resizeAnimation->stop();
    } else {
        m_resizeAnimation = new QPropertyAnimation( this, "expandStep" );
        connect( m_resizeAnimation, SIGNAL(finished()), this, SLOT(resizeAnimationFinished()) );

        // The easing curve should be chosen so that another running resize animation
        // to the other direction will (almost) not move the top edge of following items
        m_resizeAnimation->setEasingCurve( QEasingCurve(QEasingCurve::InOutBack) );

        // Make the duration longer, because many visible items may get moved when resizing an item
        m_resizeAnimation->setDuration( 550 );

        if ( expand ) {
            // Call ensureVisible() multiple times while expanding
            if ( !m_ensureVisibleTimer ) {
                m_ensureVisibleTimer = new QTimer( this );
                connect( m_ensureVisibleTimer, SIGNAL(timeout()), this, SLOT(ensureVisibleSnapped()) );
            }
            m_ensureVisibleTimer->start( 120 );
        }
    }

    m_resizeAnimation->setStartValue( m_expandStep );
    m_resizeAnimation->setEndValue( expand ? 1.0 : 0.0 );
    m_resizeAnimation->start( QAbstractAnimation::KeepWhenStopped );
    updateGeometry();

    emit expandedStateChanged( this, expand );
}

void PublicTransportGraphicsItem::setExpanded( bool expand )
{
    publicTransportWidget()->setItemExpanded( this, expand );
}

void PublicTransportGraphicsItem::ensureVisibleSnapped()
{
    // Ensure that the item is visible,
    // if it does not fit ensure that the top area of the item is visible
    const qreal height = m_parent->size().height();
    QRectF rect = geometry();
    if ( rect.height() > height ) {
        rect.setHeight( height );
    }

    // Align the rect vertically to scroll snap points,
    // otherwise the item might get moved back afterwards by the snap scroll feature
    const qreal heightUnit = m_parent->snapSize().height();
    rect.moveTop( qCeil(rect.top() / heightUnit) * heightUnit - 0.5 );
    m_parent->ensureRectVisible( rect );
}

void PublicTransportGraphicsItem::resizeAnimationFinished()
{
    QGraphicsWidget *route = routeItem();
    if ( route ) {
        route->setVisible( m_expanded );
    }

    delete m_ensureVisibleTimer;
    m_ensureVisibleTimer = 0;

    delete m_resizeAnimation;
    m_resizeAnimation = 0;
}

void PublicTransportGraphicsItem::updateGeometry()
{
    QGraphicsWidget::updateGeometry();
}

void TimetableListItem::paint( QPainter *painter, const QStyleOptionGraphicsItem *option,
                               QWidget *widget )
{
    Q_UNUSED( widget );
    painter->setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );

    // Paint background on whole item
    const QFontMetrics fontMetrics( font() );
    const QString text = KGlobal::locale()->removeAcceleratorMarker( m_action->text() );
    const int iconSize = 16;
    const int padding = 4;
    const int textWidth = fontMetrics.width( text );
    const int usedWidth = iconSize + padding + textWidth;
    const QRect targetRect( (option->rect.width() - usedWidth) / 2,
                            (option->rect.height() - iconSize) / 2,
                            usedWidth, iconSize );
    const QRect iconRect( 0, 0, iconSize, iconSize );
    const QRect textRect( iconRect.right() + padding, (iconSize - fontMetrics.height()) / 2,
                          textWidth, fontMetrics.height() );
    QPixmap iconPixmap = m_action->icon().pixmap( iconSize );

    // Draw halo/shadow
    // TODO Requires KDE > 4.6, remove other 4.6 #ifdefs
    const QColor textColor = Plasma::Theme::defaultTheme()->color(
            option->state.testFlag(QStyle::State_HasFocus) ? Plasma::Theme::ViewFocusColor :
            (option->state.testFlag(QStyle::State_MouseOver)
             ? Plasma::Theme::ViewHoverColor : Plasma::Theme::ViewTextColor) );
    const bool drawShadowsOrHalos =
            m_parent->isOptionEnabled( PublicTransportWidget::DrawShadowsOrHalos );
    const bool drawHalos = drawShadowsOrHalos && qGray(textColor.rgb()) < 192;

    // Draw everything to a pixmap
    QPixmap pixmap( usedWidth, iconSize );
    pixmap.fill( Qt::transparent );
    QPainter p( &pixmap );
    p.setPen( textColor );
    p.drawPixmap( iconRect, iconPixmap );
    p.drawText( textRect, text );
    p.end();

    // Draw halos/shadows under the pixmap
    if ( drawHalos ) {
        Plasma::PaintUtils::drawHalo( painter,
                textRect.adjusted(targetRect.left(), targetRect.top(),
                                  targetRect.left(), targetRect.top()) );
    } else if ( drawShadowsOrHalos ) {
        QImage shadow = pixmap.toImage();
        Plasma::PaintUtils::shadowBlur( shadow, 3, Qt::black );
        painter->drawImage( targetRect.topLeft() + QPoint(1, 2), shadow );
    }

    // Draw the pixmap above halos/shadows
    painter->drawPixmap( targetRect.topLeft(), pixmap );
}

void PublicTransportGraphicsItem::paint( QPainter* painter,
        const QStyleOptionGraphicsItem* option, QWidget* widget )
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
    const QRectF _rect = rect();
    paintBackground( painter, option, _rect );

    // Paint item (excluding expand area)
    paintItem( painter, option, _rect );

    // Draw expand area if this item isn't currently completely unexpanded
    if ( m_expanded || !qFuzzyIsNull(m_expandStep) ) {
        QRectF rectItem = _rect;
        rectItem.setHeight( unexpandedHeight() );
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
    return padding( m_parent->zoomFactor() );
}

qreal PublicTransportGraphicsItem::padding( qreal zoomFactor )
{
    return 4.0 * zoomFactor;
}

qreal PublicTransportGraphicsItem::unexpandedHeight() const
{
    return unexpandedHeight( m_parent->iconSize(), padding(),
                             QFontMetrics(font()).lineSpacing(), m_parent->maxLineCount() );
}

qreal PublicTransportGraphicsItem::unexpandedHeight( qreal iconSize, qreal padding,
                                                     int lineSpacing, int maxLineCount )
{
    return qMax( iconSize * 1.1, lineSpacing * maxLineCount + padding );
}

qreal PublicTransportGraphicsItem::routeItemHeight() const
{
    return 3 * unexpandedHeight();
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

QTextDocument* TextDocumentHelper::createTextDocument( const QString& html, const QSizeF& size,
    const QTextOption &textOption, const QFont &font )
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
        const QRectF &textRect, Option options )
{
    if ( textRect.size().toSize().isEmpty() ) {
        kDebug() << "No text visible, do not draw anything!" << textRect;
        return;
    }

    QList< QRectF > haloRects, fadeRects;
    const qreal fadeWidth = 30;

    QPixmap pixmap( textRect.size().toSize() );
    pixmap.fill( Qt::transparent );
    QPainter p( &pixmap );
    p.setPen( painter->pen() );
    p.setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );

    const QPointF position( 0, (textRect.height() - document->size().height()) / 2.0f );
    for ( int b = 0; b < document->blockCount(); ++b ) {
        QTextLayout *textLayout = document->findBlockByNumber( b ).layout();
        const int lines = textLayout->lineCount();
        for ( int l = 0; l < lines; ++l ) {
            // Draw a text line
            QTextLine textLine = textLayout->lineAt( l );
            textLine.draw( &p, position );

            if ( options == DrawHalos ) {
                // Calculate halo rect
                QSizeF textSize = textLine.naturalTextRect().size();
                if ( textSize.width() > textRect.width() ) {
                    textSize.setWidth( textRect.width() );
                }
                QRectF haloRect = textLine.naturalTextRect();
                haloRect.moveTopLeft( haloRect.topLeft() + position + textRect.topLeft() );
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
                const qreal x = qMin(textLine.naturalTextWidth(), textRect.width() - textLine.x())
                        - fadeWidth + textLine.x() + position.x();
                const qreal y = textLine.position().y() + position.y();
                fadeRects << QRectF( x, y, fadeWidth, textLine.height() );
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
        foreach( const QRectF &rect, fadeRects ) {
            p.fillRect( rect, alphaGradient );
        }
    }
    p.end();

    // Draw halo/shadow
    if ( options == DrawHalos ) {
        foreach( const QRectF &haloRect, haloRects ) {
            Plasma::PaintUtils::drawHalo( painter, haloRect );
        }
    } else if ( options == DrawShadows ) {
        QImage shadow = pixmap.toImage();
        Plasma::PaintUtils::shadowBlur( shadow, 3, Qt::black );
        painter->drawImage( textRect.topLeft() + QPointF(1, 2), shadow );
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

    if ( m_routeItem || m_routeInfoWidget ) {
        const qreal indentation = expandAreaIndentation();
        const QRectF _rect = rect();
        const QRectF routeRect( _rect.left() + indentation,
                                _rect.top() + unexpandedHeight() + padding(),
                                _rect.width() - padding() - indentation,
                                routeItemHeight() );
        if ( m_routeItem ) {
            m_routeItem->setGeometry( routeRect );
        } else if ( m_routeInfoWidget ) {
            m_routeInfoWidget->setGeometry( routeRect );
        }
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

    QList< QAction* > actionList;
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
            if ( item->journeyInfo()->matchedAlarms().count() == 1 ) {
                infoAction = new QAction( KIcon("task-recurring"),
                        i18nc("@info/plain", "(has a recurring alarm)"), this );
            } else {
                infoAction = new QAction(
                        i18nc("@action:inmenu", "(has recurring/multiple alarms)"), this );
            }
        } else {
            // The 'Remove this Alarm' menu entry can only be
            // used with autogenerated alarms
            if ( item->journeyInfo()->matchedAlarms().count() == 1 ) {
                infoAction = new QAction( KIcon("task-recurring"),
                        i18nc("@info/plain", "(has a custom alarm)"), this );
            } else {
                infoAction = new QAction(
                        i18nc("@action:inmenu", "(has custom/multiple alarms)"), this );
            }
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

    // Add context menu actions of the parent PublicTransportWidget
    QList< QAction* > parentActions = m_parent->contextMenuActions();
    if ( !parentActions.isEmpty() ) {
        QAction *separator = new QAction( &contextMenu );
        separator->setSeparator( true );
        actionList << separator;
        actionList << parentActions;
    }

    contextMenu.addActions( actionList );

    QAction *executedAction = contextMenu.exec( event->screenPos() );

    if ( executedAction ) {
        const JourneyInfo *info = journeyItem()->journeyInfo();
        QString lineString = info->routeTransportLines().isEmpty()
                ? QString() : info->routeTransportLines().first();
        VehicleType vehicleType = info->routeVehicleTypes().isEmpty()
                ? UnknownVehicleType : info->routeVehicleTypes().first();
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
    if ( event->button() == Qt::LeftButton &&
        (event->lastPos() - event->pos()).manhattanLength() < 5 )
    {
        toggleExpanded();
        event->accept();
    } else {
        QGraphicsItem::mouseReleaseEvent( event );
    }
}

QPointF DepartureGraphicsItem::othersTextPos() const
{
    const qreal indentation = expandAreaIndentation();
    qreal y = unexpandedHeight() + 2 * padding();
    qreal height = expandAreaHeight() - 2 * padding();
    if ( m_routeItem ) {
        y += m_routeItem->size().height() + padding();
        height -= m_routeItem->size().height() + padding();
    } else if ( m_routeInfoWidget ) {
        y += m_routeInfoWidget->size().height() + padding();
        height -= m_routeInfoWidget->size().height() + padding();
    }
    if ( m_othersTextDocument && m_othersTextDocument->blockCount() > 0 ) {
        // Shift y because of vertical alignment
        y += (height - m_othersTextDocument->size().height()) / 2.0f;
    }
    return QPointF( indentation, y );
}

void DepartureGraphicsItem::hoverMoveEvent( QGraphicsSceneHoverEvent *event )
{
    QGraphicsWidget::hoverMoveEvent(event);
    if ( !m_othersTextDocument ) {
        return;
    }
    const QString anchor = m_othersTextDocument->documentLayout()->anchorAt(
            event->pos() - othersTextPos() );
    if ( !anchor.isEmpty() ) {
        setCursor( QCursor(Qt::PointingHandCursor) );
        event->accept();
    } else {
        unsetCursor();
    }
}

void DepartureGraphicsItem::mouseReleaseEvent( QGraphicsSceneMouseEvent *event )
{
    if ( !m_othersTextDocument || event->button() != Qt::LeftButton ) {
        PublicTransportGraphicsItem::mouseReleaseEvent(event);
        return;
    }
    const QString anchor = m_othersTextDocument->documentLayout()->anchorAt(
            event->pos() - othersTextPos() );
    if ( !anchor.isEmpty() ) {
        KToolInvocation::self()->invokeBrowser( anchor );
        event->accept();
    } else {
        PublicTransportGraphicsItem::mouseReleaseEvent(event);
    }
}

DepartureGraphicsItem::DepartureGraphicsItem( PublicTransportWidget* publicTransportWidget,
        QGraphicsItem* parent,
        StopAction *copyStopToClipboardAction, StopAction *showInMapAction,
        StopAction *showDeparturesAction, StopAction *highlightStopAction,
        StopAction *newFilterViaStopAction, KPixmapCache *pixmapCache )
        : PublicTransportGraphicsItem( publicTransportWidget, parent, copyStopToClipboardAction,
                                       showInMapAction ),
        m_infoTextDocument(0), m_timeTextDocument(0), m_othersTextDocument(0), m_routeItem(0),
        m_routeInfoWidget(0), m_highlighted(false), m_leavingAnimation(0),
        m_showDeparturesAction(showDeparturesAction), m_highlightStopAction(highlightStopAction),
        m_newFilterViaStopAction(newFilterViaStopAction), m_updateAdditionalDataAction(0),
        m_pixmapCache(pixmapCache)
{
    m_leavingStep = 0.0;
    m_updateAdditionalDataAction = new KAction( KIcon("view-refresh"),
                                                i18nc("@info", "Refresh"), this );
    connect( m_updateAdditionalDataAction, SIGNAL(triggered(bool)),
             this, SIGNAL(updateAdditionalDataRequest()) );
    setAcceptHoverEvents( true ); // Hover anchors in the document
}

DepartureGraphicsItem::~DepartureGraphicsItem()
{
    if ( m_leavingAnimation ) {
        m_leavingAnimation->stop();
    }

    delete m_infoTextDocument;
    delete m_timeTextDocument;
    delete m_othersTextDocument;
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
                         info->lineString(), info->targetShortened());
        } else { // if ( departureItem()->model()->info().departureArrivalListType == DepartureList ) {
            html = i18nc("@info", "<emphasis strong='1'>%1</emphasis> to %2",
                         info->lineString(), info->targetShortened());
        }
        m_infoTextDocument = TextDocumentHelper::createTextDocument( html, _infoRect.size(),
                                                                     textOption, font() );
    }

    QSizeF othersSize( rect.width() - expandAreaIndentation() - padding(), 999 );
    const QString html = othersText();
    if ( !m_othersTextDocument || m_othersTextDocument->textWidth() != othersSize.width() ||
         html != m_othersTextDocument->toHtml() )
    {
        delete m_othersTextDocument;
        textOption.setAlignment( Qt::AlignBottom | Qt::AlignLeft );

        m_othersTextDocument = TextDocumentHelper::createTextDocument( html, othersSize,
                                                                       textOption, font() );
    }
}

QString DepartureGraphicsItem::othersText() const
{
    DepartureItem *item = qobject_cast< DepartureItem* >( m_item.data() );
    ChildItem *delayItem = m_item->childByType( DelayItem );
    ChildItem *platformItem = m_item->childByType( PlatformItem );
    ChildItem *operatorItem = m_item->childByType( OperatorItem );
    ChildItem *journeyNewsItem = m_item->childByType( JourneyNewsItem );

    QString html;
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
        if ( journeyNewsItem ) {
            html.append( journeyNewsItem->formattedText() );
        }
    }
    return html;
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

    if ( !item->journeyInfo()->routeStopsShortened().isEmpty() ) {
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

void DepartureGraphicsItem::hideRouteInfoWidget()
{
    if ( m_routeInfoWidget ) {
        // Fade out the route busy widget, delete the route busy widget and the animation when done
        Plasma::Animation *animation =
                Plasma::Animator::create( Plasma::Animator::FadeAnimation, m_routeInfoWidget );
        animation->setTargetWidget( m_routeInfoWidget );
        animation->setProperty( "startOpacity", m_routeInfoWidget->opacity() );
        animation->setProperty( "targetOpacity", 0.0 );
        connect( animation, SIGNAL(finished()), m_routeInfoWidget, SLOT(deleteLater()) );
        animation->start( QAbstractAnimation::DeleteWhenStopped );
        m_routeInfoWidget = 0;
    }
}

void DepartureGraphicsItem::showRouteInfoWidget( QGraphicsWidget *routeInfoWidget )
{
    // Show the route info widget (busy widget or an error label) over the route item for
    // the fade out animation of the route info widget, while the route item gets faded in
    routeInfoWidget->setZValue( 100 );

    // Position the route info widget TODO Do the SAME for the route item
    const qreal indentation = expandAreaIndentation();
    const QRectF _rect = rect();
    routeInfoWidget->setPos( _rect.left() + indentation,
                             _rect.top() + unexpandedHeight() + padding() );
    routeInfoWidget->resize( _rect.width() - padding() - indentation, routeItemHeight() );
    m_routeInfoWidget = routeInfoWidget;
}

bool DepartureGraphicsItem::isRouteDataAvailable() const
{
    DepartureItem *item = qobject_cast<DepartureItem*>( m_item );
    if ( !item ) {
        return false;
    }
    return !item->departureInfo()->routeStopsShortened().isEmpty();
}

bool DepartureGraphicsItem::isRouteDataRequestable() const
{
    DepartureItem *item = qobject_cast<DepartureItem*>( m_item );
    if ( !item ) {
        return false;
    }
    return !item->departureInfo()->includesAdditionalData() &&
           publicTransportWidget()->model()->info().providerFeatures.contains(
                QLatin1String("ProvidesAdditionalData")) &&
           publicTransportWidget()->model()->info().providerFeatures.contains(
                QLatin1String("ProvidesRouteInformation"));
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

    // Test if route data is already available or if it should be available as additional data
    if ( isRouteDataAvailable() ) {
        hideRouteInfoWidget();
        if ( m_routeItem ) {
            m_routeItem->updateData( item );
        } else {
            m_routeItem = new RouteGraphicsItem( this, item, m_copyStopToClipboardAction,
                    m_showInMapAction, m_showDeparturesAction, m_highlightStopAction,
                    m_newFilterViaStopAction );
            QRectF _infoRect = infoRect( rect(), 0 );
            m_routeItem->setZoomFactor( m_parent->zoomFactor() );
            m_routeItem->setPos( _infoRect.left(), rect().top() + unexpandedHeight() + padding() );
            m_routeItem->resize( rect().width() - padding() - _infoRect.left(),
                                 routeItemHeight() );
            if ( isVisible() ) {
                // Fade-in animation for the route item
                m_routeItem->setOpacity( 0.0 );
                Plasma::Animation *animation =
                        Plasma::Animator::create( Plasma::Animator::FadeAnimation, m_routeItem );
                animation->setTargetWidget( m_routeItem );
                animation->setProperty( "startOpacity", 0.0 );
                animation->setProperty( "targetOpacity", 1.0 );
                animation->start( QAbstractAnimation::DeleteWhenStopped );
            } else {
                m_routeItem->hide();
            }
        }
    } else if ( isRouteDataRequestable() ) {
        if ( item->departureInfo()->isWaitingForAdditionalData() ) {
            if ( !qgraphicsitem_cast<Plasma::BusyWidget*>(m_routeInfoWidget) ) {
                // No busy widget shown
                if ( m_routeInfoWidget ) {
                    // Hide other info widget
                    hideRouteInfoWidget();
                }
                showRouteInfoWidget( new Plasma::BusyWidget(this) );
            }
        } else {
            Plasma::Label *label = qgraphicsitem_cast<Plasma::Label*>( m_routeInfoWidget );
            if ( !label ) {
                // No label shown, create a new one
                if ( m_routeInfoWidget ) {
                    // Hide other info widget
                    hideRouteInfoWidget();
                }

                if ( !item->departureInfo()->additionalDataError().isEmpty() ) {
                    label = new Plasma::Label( this );
                    label->addAction( m_updateAdditionalDataAction );
                    label->nativeWidget()->setContextMenuPolicy( Qt::ActionsContextMenu );
                    showRouteInfoWidget( label );
                }
            }

            // Update error text shown in the label
            if ( item->departureInfo()->additionalDataError().isEmpty() ) {
                // Hide error text if it changed to an empty string
                hideRouteInfoWidget();
            } else {
                label->setText( item->departureInfo()->additionalDataError() );
            }
        }
    } else if ( m_routeItem ) {
        delete m_routeItem;
        m_routeItem = 0;
    } else if ( m_routeInfoWidget ) {
        hideRouteInfoWidget();
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
    QColor borderColor = textColor();
    borderColor.setAlphaF( 0.5 );

    QRectF pixmapRect( 0, 0, rect.width(), rect.height() );
    QPixmap pixmap( pixmapRect.size().toSize() );
    QColor backgroundColor = Qt::transparent;
    pixmap.fill( backgroundColor );

    // Use journey rating background:
    //   green for relatively short duration, less changes;
    //   red for relatively long duration, more changes (controlled by the model).
    const QColor neutralBackground = KColorScheme( QPalette::Active )
            .background( KColorScheme::NeutralBackground ).color();
    QVariant vr = index().data( JourneyRatingRole );
    if ( vr.isValid() ) {
        qreal rating = vr.toReal();
        const QColor positiveBackground = KColorScheme( QPalette::Active )
                .background( KColorScheme::PositiveBackground ).color();
        const QColor negativeBackground = KColorScheme( QPalette::Active )
                .background( KColorScheme::NegativeBackground ).color();
        backgroundColor = rating > 0.5
                ? KColorUtils::mix( neutralBackground, negativeBackground, qMin(1.0, (rating - 0.5) * 2.5) )
                : KColorUtils::mix( positiveBackground, neutralBackground, rating * 2.0 );
    } else if ( index().row() % 2 == 1 ) {
        // Use alternate background (if journey ratings aren't available)
        backgroundColor = KColorScheme( QPalette::Active, KColorScheme::View )
                .background( KColorScheme::AlternateBackground ).color();
        backgroundColor.setAlphaF( 0.3 );
    } else {
        backgroundColor = neutralBackground;
    }

    const int textGray = qGray( borderColor.rgb() );
    int backgroundGray = qGray( backgroundColor.rgb() );
    const int minDifference = 128;
    int difference = textGray - backgroundGray;
    if ( textGray >= 128 && difference < minDifference ) {
        if ( difference <= 15 ) {
            difference = 15; // Prevent division by zero
        }
        // Text is light, but background not dark enough
        const float darkenFactor = qBound( 100,
                18 * qAbs(minDifference / difference) * backgroundGray / 164, 300 ) / 100.0f;
        int h, s, v;
        backgroundColor.getHsv( &h, &s, &v );
        backgroundColor.setHsv( h, s * darkenFactor, v / darkenFactor );
    } else if ( textGray < 128 && difference > -minDifference ) {
        if ( difference >= -15 ) {
            difference = -15; // Prevent division by zero
        }
        if ( backgroundGray == 0 ) {
            backgroundGray = 1;
        }
        // Text is dark, but background not light enough
        const float lightenFactor = qMax( 100,
                18 * qAbs(minDifference / -difference) * 164 / backgroundGray ) / 100.0f;
        int h, s, v;
        backgroundColor.getHsv( &h, &s, &v );
        backgroundColor.setHsv( h, s, v * lightenFactor );
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
    p.drawLine( 0, pixmap.size().height() - 0.5,
                pixmap.size().width(), pixmap.size().height() - 0.5 );

    // Fade out to the left and right
    drawFadeOutLeftAndRight( &p, pixmapRect );
    p.end();

    // Draw pixmap
    painter->drawPixmap( rect.topLeft(), pixmap );
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
    vehicleTypeSet.remove( UnknownVehicleType );
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
    const bool drawShadowsOrHalos = m_parent->isOptionEnabled( PublicTransportWidget::DrawShadowsOrHalos );
    const bool drawHalos = drawShadowsOrHalos && qGray(_textColor.rgb()) < 192;
    painter->setPen( _textColor );
    TextDocumentHelper::drawTextDocument( painter, option, m_infoTextDocument,
            _infoRect, drawShadowsOrHalos
            ? (drawHalos ? TextDocumentHelper::DrawHalos : TextDocumentHelper::DrawShadows)
            : TextDocumentHelper::DoNotDrawShadowOrHalos );

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
    bool manuallyHighlighted = false;
    DepartureModel *model = !m_item ? 0 : qobject_cast<DepartureModel*>( m_item->model() );
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

        // Make text color a little transparent, if not highlighted
        color.setAlpha( 230 );

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

    QRectF pixmapRect( 0, 0, rect.width(), rect.height() );
    QPixmap pixmap( pixmapRect.size().toSize() );

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

    // Draw a line at the bottom of this TimetableItem,
    // -0.5 to draw the line completely inside the pixmap
    p.setPen( borderColor );
    p.drawLine( 0, pixmap.size().height() - 0.5,
                pixmap.size().width(), pixmap.size().height() - 0.5 );

    // Fade out to the left and right
    drawFadeOutLeftAndRight( &p, pixmapRect );
    p.end();

    // Draw pixmap
    painter->drawPixmap( rect.topLeft(), pixmap );
}

void PublicTransportGraphicsItem::drawAlarmBackground( QPainter *painter, const QRectF &rect )
{
    // alarmColor is oxygen color "brick red5", with an alpha value added
    const QColor alarmColor( 191, 3, 3, 180 );
    // Draw the alarm gradients over the first and last third vertically
    const int alarmHeight = unexpandedHeight() / 3;

    // Draw the gradient at the top
    QLinearGradient alarmGradientTop( 0, 0, 0, alarmHeight );
    alarmGradientTop.setColorAt( 0, alarmColor );
    alarmGradientTop.setColorAt( 1, Qt::transparent );
    painter->fillRect( QRectF(0, 0, rect.width(), alarmHeight), alarmGradientTop );

    // Draw the gradient at the bottom
    QLinearGradient alarmGradientBottom( 0, rect.height() - alarmHeight, 0, rect.height() );
    alarmGradientBottom.setColorAt( 0, Qt::transparent );
    alarmGradientBottom.setColorAt( 1, alarmColor );
    painter->fillRect( QRectF(0, rect.height() - alarmHeight, rect.width(), alarmHeight),
                       alarmGradientBottom );
}

void PublicTransportGraphicsItem::drawFadeOutLeftAndRight( QPainter* painter, const QRectF& rect,
                                                           int fadeWidth )
{
    painter->setCompositionMode( QPainter::CompositionMode_DestinationIn );
    QLinearGradient alphaGradient( 0, 0, 1, 0 );
    alphaGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
    alphaGradient.setColorAt( 0, Qt::transparent );
    alphaGradient.setColorAt( 1, Qt::black );
    // Fade out on the left
    painter->fillRect( QRectF(rect.left(), rect.top(), fadeWidth, rect.height()), alphaGradient );

    alphaGradient.setColorAt( 0, Qt::black );
    alphaGradient.setColorAt( 1, Qt::transparent );
    // Fade out on the right (the +1 is to be sure, to not have a 1 pixel line on the right, which
    // isn't made transparent at all)
    painter->fillRect( QRectF(rect.right() - fadeWidth, rect.top(), fadeWidth + 1, rect.height()),
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
            if ( !vehicleKey.isEmpty() ) {
                kDebug() << "SVG element" << vehicleKey << "not found";
            }
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
    const bool drawShadowsOrHalos = m_parent->isOptionEnabled( PublicTransportWidget::DrawShadowsOrHalos );
    const bool drawHalos = drawShadowsOrHalos && qGray(_textColor.rgb()) < 192;
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
        QFont _font = font();
        _font.setItalic( manuallyHighlighted );
        m_infoTextDocument->setDefaultFont( _font );
        m_timeTextDocument->setDefaultFont( _font );
        m_othersTextDocument->setDefaultFont( _font );
        setFont( _font );
        m_highlighted = manuallyHighlighted;
    }

    TextDocumentHelper::Option options = drawShadowsOrHalos
            ? (drawHalos ? TextDocumentHelper::DrawHalos : TextDocumentHelper::DrawShadows)
            : TextDocumentHelper::DoNotDrawShadowOrHalos;
    TextDocumentHelper::drawTextDocument( painter, option, m_infoTextDocument,
            _infoRect, options );
    TextDocumentHelper::drawTextDocument( painter, option, m_timeTextDocument,
            _timeRect, options );

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
    const bool drawShadowsOrHalos = m_parent->isOptionEnabled( PublicTransportWidget::DrawShadowsOrHalos );
    const bool drawHalos = drawShadowsOrHalos && qGray(_textColor.rgb()) < 192;

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
                htmlRect, drawShadowsOrHalos
                ? (drawHalos ? TextDocumentHelper::DrawHalos : TextDocumentHelper::DrawShadows)
                : TextDocumentHelper::DoNotDrawShadowOrHalos );
    }
}

void DepartureGraphicsItem::paintExpanded( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                           const QRectF& rect )
{
    painter->setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );
    QColor _textColor = textColor();
    const bool drawShadowsOrHalos = m_parent->isOptionEnabled( PublicTransportWidget::DrawShadowsOrHalos );
    const bool drawHalos = drawShadowsOrHalos && qGray(_textColor.rgb()) < 192;

    qreal y = rect.top();
    if ( m_routeItem ) {
        y += m_routeItem->size().height() + padding();
    } else if ( m_routeInfoWidget ) {
        y += m_routeInfoWidget->size().height() + padding();
    }

    if ( y >= size().height() ) {
        return; // Currently not expanded enough to show more information (is animating)
    }

    if ( m_othersTextDocument ) {
        QFontMetrics fm( font() );
        QRectF htmlRect( rect.left(), y, rect.width(), rect.bottom() - y );
        painter->setPen( _textColor );
        TextDocumentHelper::drawTextDocument( painter, option, m_othersTextDocument,
                htmlRect, drawShadowsOrHalos
                ? (drawHalos ? TextDocumentHelper::DrawHalos : TextDocumentHelper::DrawShadows)
                : TextDocumentHelper::DoNotDrawShadowOrHalos);
    }
}

qreal PublicTransportGraphicsItem::expandAreaHeightTarget() const
{
    // Round expand area height to multiples of unexpandedHeight(),
    // because this is used for the snap size of the ScrollWidget to stop scrolling
    // at the top of an item (cannot set individual scroll stop positions)
    const qreal height = expandAreaHeightMinimum();
    const qreal heightUnit = unexpandedHeight();
    return qCeil(height / heightUnit) * heightUnit;
}

qreal PublicTransportGraphicsItem::expandAreaHeight() const
{
    return expandAreaHeightTarget() * m_expandStep;
}

qreal JourneyGraphicsItem::expandAreaHeightMinimum() const
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

    return height;
}

qreal DepartureGraphicsItem::expandAreaHeightMinimum() const
{
    if ( !m_item || qFuzzyIsNull(m_expandStep) ) {
        return 0.0;
    }

    qreal height = padding();
    if ( isRouteDataAvailable() || isRouteDataRequestable() ) {
        height += routeItemHeight() + padding();
    }
    if ( m_othersTextDocument ) {
        height += m_othersTextDocument->size().height() + padding();
    }
    return height;
}

qreal DepartureGraphicsItem::expandAreaHeightMaximum() const
{
    if ( !m_item ) {
        return 0.0;
    }

    qreal height = padding();
    height += routeItemHeight() + padding();
    if ( m_othersTextDocument ) {
        height += m_othersTextDocument->documentLayout()->documentSize().height() + padding();
    }
    const qreal heightUnit = unexpandedHeight();
    return qCeil(height / heightUnit) * heightUnit;
}

qreal DepartureGraphicsItem::expandAreaIndentation() const
{
    return m_parent->iconSize() * 0.65 + padding();
}

qreal JourneyGraphicsItem::expandAreaIndentation() const
{
    return m_parent->iconSize() * 0.65 + padding();
}

QSizeF TimetableListItem::sizeHint( Qt::SizeHint which, const QSizeF &constraint ) const
{
    if ( which == Qt::MinimumSize || which == Qt::MaximumSize ) {
        return QSizeF( which == Qt::MinimumSize ? 100 : 100000,
                       qMax(m_parent->iconSize() * 1.1,
                            QFontMetrics(font()).lineSpacing() + 4.0 * m_parent->zoomFactor()) );
    } else {
        return QGraphicsWidget::sizeHint( which, constraint );
    }
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

PublicTransportWidget::PublicTransportWidget( Options options, ExpandingOption expandingOption,
                                              QGraphicsItem* parent )
    : Plasma::ScrollWidget( parent ), m_options(options), m_expandingOption(expandingOption),
      m_model(0), m_prefixItem(0), m_postfixItem(0), m_svg(0),
      m_copyStopToClipboardAction(0), m_showInMapAction(0)
{
    setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    setupActions();

    QGraphicsWidget *container = new QGraphicsWidget( this );
    QGraphicsLinearLayout *l = new QGraphicsLinearLayout( Qt::Vertical, container );
    l->setSpacing( 0.0 );
    l->setContentsMargins( 0, 0, 0, 0 );
    container->setLayout( l );
    setWidget( container );

    m_maxLineCount = 2;
    m_iconSize = 32;
    m_zoomFactor = 1.0;
    updateSnapSize();
}

QPainterPath PublicTransportGraphicsItem::shape() const
{
    // Get the viewport geometry of the ScrollWidget in local coordinates
    // and adjusted to clip some pixels of children at the top/bottom
    // for the ScrollWidget overflow border
    QPainterPath parentPath;
    QRectF viewport = publicTransportWidget()->viewportGeometry();
    if ( publicTransportWidget()->overflowBordersVisible() ) {
        viewport.adjust( 0, 2, 0, -1 );
    }
    parentPath.addRect( viewport );
    parentPath = mapFromItem( publicTransportWidget(), parentPath );

    // Intersect the parent bounding rectangle with the bounding rectangle of this item
    // and return it as visible shape
    QPainterPath path;
    QRectF rect = parentPath.boundingRect().intersected( boundingRect() );
    path.addRect( rect );
    return path;
}

QPainterPath TimetableListItem::shape() const
{
    // Get the viewport geometry of the ScrollWidget in local coordinates
    // and adjusted to clip some pixels of children at the top/bottom
    // for the ScrollWidget overflow border
    QPainterPath parentPath;
    QRectF viewport = m_parent->viewportGeometry();
    if ( m_parent->overflowBordersVisible() ) {
        viewport.adjust( 0, 2, 0, -1 );
    }
    parentPath.addRect( viewport );
    parentPath = mapFromItem( m_parent, parentPath );

    // Intersect the parent bounding rectangle with the bounding rectangle of this item
    // and return it as visible shape
    QPainterPath path;
    QRectF rect = parentPath.boundingRect().intersected( boundingRect() );
    path.addRect( rect );
    return path;
}

void PublicTransportWidget::setupActions()
{
    m_copyStopToClipboardAction = new StopAction( StopAction::CopyStopNameToClipboard, this );
    connect( m_copyStopToClipboardAction, SIGNAL(stopActionTriggered(StopAction::Type,QString,QString)),
             this, SIGNAL(requestStopAction(StopAction::Type,QString,QString)) );

    m_showInMapAction = new StopAction( StopAction::ShowStopInMap, this );
    connect( m_showInMapAction, SIGNAL(stopActionTriggered(StopAction::Type,QString,QString)),
             this, SIGNAL(requestStopAction(StopAction::Type,QString,QString)) );
}

void PublicTransportWidget::setOption( PublicTransportWidget::Option option, bool enable )
{
    m_options = enable ? m_options | option : m_options & ~option;
    update();
}

void PublicTransportWidget::setOptions( PublicTransportWidget::Options options )
{
    m_options = options;
    update();
}

void PublicTransportWidget::updateSnapSize()
{
    setSnapSize( QSizeF(0, PublicTransportGraphicsItem::unexpandedHeight(m_iconSize,
            PublicTransportGraphicsItem::padding(m_zoomFactor), QFontMetrics(font()).lineSpacing(),
            m_maxLineCount)) );
}

void PublicTransportWidget::setPrefixItem( TimetableListItem *prefixItem )
{
    QGraphicsLinearLayout *l = static_cast<QGraphicsLinearLayout*>( widget()->layout() );
    if ( m_prefixItem == prefixItem ) {
        if ( l->count() == 0 || l->itemAt(0) != m_prefixItem ) {
            l->insertItem( 0, prefixItem );
        }
        return;
    }

    if ( m_prefixItem ) {
        l->removeItem( m_prefixItem );
        delete m_prefixItem;
        m_prefixItem = 0;
    }
    if ( prefixItem ) {
        kDebug() << prefixItem;
        l->insertItem( 0, prefixItem );
        m_prefixItem = prefixItem;
    }
}

void PublicTransportWidget::setPostfixItem( TimetableListItem *postfixItem )
{
    QGraphicsLinearLayout *l = static_cast<QGraphicsLinearLayout*>( widget()->layout() );
    if ( m_postfixItem == postfixItem ) {
        if ( l->count() == 0 || l->itemAt(l->count() - 1) != m_postfixItem ) {
            l->addItem( postfixItem );
        }
        return;
    }

    if ( m_postfixItem ) {
        l->removeItem( m_postfixItem );
        delete m_postfixItem;
        m_postfixItem = 0;
    }
    if ( postfixItem ) {
        l->addItem( postfixItem );
        m_postfixItem = postfixItem;
    }
}

JourneyTimetableWidget::JourneyTimetableWidget( Options options, Flags flags,
                                                ExpandingOption expandingOption,
                                                QGraphicsItem* parent )
    : PublicTransportWidget(options, expandingOption, parent), m_flags(flags),
      m_requestJourneyToStopAction(0), m_requestJourneyFromStopAction(0), m_earlierAction(0),
      m_laterAction(0)
{
    setupActions();

    // Add items to request earlier/later journeys
    if ( m_flags.testFlag(ShowEarlierJourneysItem) && m_earlierAction ) {
        setPrefixItem( new TimetableListItem(m_earlierAction, this, this) );
    }
    if ( m_flags.testFlag(ShowLaterJourneysItem) && m_laterAction ) {
        setPostfixItem( new TimetableListItem(m_laterAction, this, this) );
    }
}

TimetableWidget::TimetableWidget( Options options, ExpandingOption expandingOption,
                                  QGraphicsItem* parent )
    : PublicTransportWidget(options, expandingOption, parent), m_showDeparturesAction(0),
      m_highlightStopAction(0), m_newFilterViaStopAction(0),
      m_pixmapCache(new KPixmapCache("PublicTransportVehicleIcons"))
{
    m_targetHidden = false;
    setupActions();
}

TimetableWidget::~TimetableWidget()
{
    delete m_pixmapCache;
}

void TimetableWidget::setupActions()
{
    PublicTransportWidget::setupActions();

    m_showDeparturesAction = new StopAction( StopAction::ShowDeparturesForStop, this );
    m_highlightStopAction= new StopAction( StopAction::HighlightStop, this );
    m_newFilterViaStopAction = new StopAction( StopAction::CreateFilterForStop, this );
    connect( m_showDeparturesAction, SIGNAL(stopActionTriggered(StopAction::Type,QString,QString)),
             this, SIGNAL(requestStopAction(StopAction::Type,QString,QString)) );
    connect( m_highlightStopAction, SIGNAL(stopActionTriggered(StopAction::Type,QString,QString)),
             this, SIGNAL(requestStopAction(StopAction::Type,QString,QString)) );
    connect( m_newFilterViaStopAction, SIGNAL(stopActionTriggered(StopAction::Type,QString,QString)),
             this, SIGNAL(requestStopAction(StopAction::Type,QString,QString)) );
}

void JourneyTimetableWidget::setupActions()
{
    PublicTransportWidget::setupActions();

    m_requestJourneyToStopAction = new StopAction( StopAction::RequestJourneysToStop, this );
    m_requestJourneyFromStopAction = new StopAction( StopAction::RequestJourneysFromStop, this );
    connect( m_requestJourneyToStopAction, SIGNAL(stopActionTriggered(StopAction::Type,QString,QString)),
             this, SIGNAL(requestStopAction(StopAction::Type,QString,QString)) );
    connect( m_requestJourneyFromStopAction, SIGNAL(stopActionTriggered(StopAction::Type,QString,QString)),
             this, SIGNAL(requestStopAction(StopAction::Type,QString,QString)) );

    if ( m_flags.testFlag(ShowEarlierJourneysItem) ) {
        m_earlierAction = new QAction( KIcon("arrow-up-double"),
                i18nc("@action:inmenu", "Get &Earlier Journeys"), this );
        connect( m_earlierAction, SIGNAL(triggered(bool)), this, SIGNAL(requestEarlierItems()) );
    }
    if ( m_flags.testFlag(ShowLaterJourneysItem) ) {
        m_laterAction = new QAction( KIcon("arrow-down-double"),
                i18nc("@action:inmenu", "Get &Later Journeys"), this );
        connect( m_laterAction, SIGNAL(triggered(bool)), this, SIGNAL(requestLaterItems()) );
    }
}

void PublicTransportWidget::setModel( PublicTransportModel* model )
{
    if ( m_model ) {
        kWarning() << "Model already set";
        return;
    }
    m_model = model;

    if ( m_model ) {
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

int PublicTransportWidget::rowFromItem( PublicTransportGraphicsItem *item ) const
{
    for ( int row = 0; row < m_items.count(); ++row ) {
        if ( m_items[row] == item ) {
            // Found the item
            return row;
        }
    }

    // No such item found
    return -1;
}

void PublicTransportWidget::setExpandOption( ExpandingOption expandingOption )
{
    if ( expandingOption == NoExpanding ) {
        foreach ( PublicTransportGraphicsItem *item, m_items ) {
            item->setExpanded( false );
        }
    } else if ( expandingOption == ExpandSingle ) {
        PublicTransportGraphicsItem *visibleExpandedItem = 0;
        QList< PublicTransportGraphicsItem* > expandedItems;
        foreach ( PublicTransportGraphicsItem *item, m_items ) {
            if ( item->isExpanded() ) {
                if ( !visibleExpandedItem && item->geometry().intersects(boundingRect()) ) {
                    kDebug() << item->geometry();
                    visibleExpandedItem = item;
                } else {
                    expandedItems << item;
                }
            }
        }

        if ( !expandedItems.isEmpty() || visibleExpandedItem ) {
            if ( !visibleExpandedItem ) {
                visibleExpandedItem = expandedItems.takeFirst();
            }
            foreach ( PublicTransportGraphicsItem *item, expandedItems ) {
                item->setExpandedNotAffectingOtherItems( false );
            }
        }
    }

    m_expandingOption = expandingOption;
}

bool PublicTransportWidget::isItemExpanded( int row ) const
{
    return m_items[ row ]->isExpanded();
}

void PublicTransportWidget::setItemExpanded( int row, bool expanded )
{
    if ( m_expandingOption == NoExpanding ) {
        return;
    }

    if ( m_expandingOption == ExpandSingle && expanded ) {
        // Toggle expanded items TODO fix docu...
        foreach ( PublicTransportGraphicsItem *item, m_items ) {
            item->setExpandedNotAffectingOtherItems( false );
        }
    }
    PublicTransportGraphicsItem *expandItem = m_items[ row ];
    expandItem->setExpandedNotAffectingOtherItems( expanded );
}

void PublicTransportWidget::setZoomFactor( qreal zoomFactor )
{
    m_zoomFactor = zoomFactor;

    for ( int i = 0; i < m_items.count(); ++i ) {
        // Notify children about changed settings
        m_items[i]->updateSettings();
    }
    updateGeometry();
    updateItemGeometries();
    updateSnapSize();
    update();
}

void PublicTransportWidget::contextMenuEvent( QGraphicsSceneContextMenuEvent* event )
{
    KMenu contextMenu;
    QList< QAction* > actions = contextMenuActions();
    if ( actions.isEmpty() ) {
        QGraphicsItem::contextMenuEvent( event );
        return;
    }
    event->accept();

    contextMenu.addActions( actions );
    contextMenu.exec( event->screenPos() );
}

void TimetableWidget::contextMenuEvent( QGraphicsSceneContextMenuEvent *event )
{
    PublicTransportGraphicsItem *item = dynamic_cast<PublicTransportGraphicsItem*>(
            scene()->itemAt(event->scenePos()) );
    if ( item ) {
        event->accept();
        emit contextMenuRequested( item, event->pos() );
    } else {
        PublicTransportWidget::contextMenuEvent( event );
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
    const int prefixOffset = m_prefixItem ? 1 : 0;

    if ( m_items.isEmpty() ) {
        setPrefixItem( m_prefixItem );
        setPostfixItem( m_postfixItem );
    }

    for ( int row = first; row <= last; ++row ) {
        JourneyGraphicsItem *item = new JourneyGraphicsItem( this, widget(),
                m_copyStopToClipboardAction, m_showInMapAction,
                m_requestJourneyToStopAction, m_requestJourneyFromStopAction );
        item->updateData( static_cast<JourneyItem*>(m_model->item(row)) );
        connect( item, SIGNAL(requestAlarmCreation(QDateTime,QString,VehicleType,QString,QGraphicsWidget*)),
                 this, SIGNAL(requestAlarmCreation(QDateTime,QString,VehicleType,QString,QGraphicsWidget*)) );
        connect( item, SIGNAL(requestAlarmDeletion(QDateTime,QString,VehicleType,QString,QGraphicsWidget*)),
                 this, SIGNAL(requestAlarmDeletion(QDateTime,QString,VehicleType,QString,QGraphicsWidget*)) );
        connect( item, SIGNAL(expandedStateChanged(PublicTransportGraphicsItem*,bool)),
                 this, SIGNAL(expandedStateChanged(PublicTransportGraphicsItem*,bool)) );
        m_items.insert( row, item );

        // Fade new items in
        Plasma::Animation *fadeAnimation = Plasma::Animator::create(
                Plasma::Animator::FadeAnimation, item );
        fadeAnimation->setTargetWidget( item );
        fadeAnimation->setProperty( "startOpacity", 0.0 );
        fadeAnimation->setProperty( "targetOpacity", 1.0 );
        fadeAnimation->start( QAbstractAnimation::DeleteWhenStopped );

        l->insertItem( row + prefixOffset, item );
    }
}

void TimetableWidget::rowsInserted( const QModelIndex& parent, int first, int last )
{
    if ( parent.isValid() ) {
        QModelIndex topLevelParent = parent;
        while ( parent.parent().isValid() ) {
            topLevelParent = topLevelParent.parent();
        }
        DepartureGraphicsItem *departure = departureItem( topLevelParent );
        if ( departure ) {
            departure->updateData( departure->departureItem(), true );
        }
        return;
    }

    QGraphicsLinearLayout *l = static_cast<QGraphicsLinearLayout*>( widget()->layout() );
    const int prefixOffset = m_prefixItem ? 1 : 0;
    for ( int row = first; row <= last; ++row ) {
        DepartureGraphicsItem *item = new DepartureGraphicsItem( this, widget(),
                m_copyStopToClipboardAction, m_showInMapAction, m_showDeparturesAction,
                m_highlightStopAction, m_newFilterViaStopAction, m_pixmapCache );
        item->updateData( static_cast<DepartureItem*>(m_model->item(row)) );
        connect( item, SIGNAL(expandedStateChanged(PublicTransportGraphicsItem*,bool)),
                 this, SIGNAL(expandedStateChanged(PublicTransportGraphicsItem*,bool)) );
        m_items.insert( row, item );

        // Fade new items in
        Plasma::Animation *fadeAnimation = Plasma::Animator::create(
                Plasma::Animator::FadeAnimation, item );
        fadeAnimation->setTargetWidget( item );
        fadeAnimation->setProperty( "startOpacity", 0.0 );
        fadeAnimation->setProperty( "targetOpacity", 1.0 );
        fadeAnimation->start( QAbstractAnimation::DeleteWhenStopped );

        l->insertItem( row + prefixOffset, item );
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
