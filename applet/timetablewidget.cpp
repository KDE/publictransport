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

#include "timetablewidget.h"
#include "routegraphicsitem.h"
#include "departuremodel.h"

#include <KColorScheme>
#include <KColorUtils>
#include <Plasma/PaintUtils>
#include <Plasma/Svg>
#include <Plasma/Animator>
#include <Plasma/Animation>

#include <QGraphicsLinearLayout>
#include <QModelIndex>
#include <QPainter>
#include <QTextLayout>
#include <QTextDocument>
#include <QTextBlock>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsScene>
#include <QPropertyAnimation>
#include <qmath.h>

PublicTransportGraphicsItem::PublicTransportGraphicsItem( QGraphicsItem* parent ) : QGraphicsWidget(parent),
    m_item(0), m_parent(0), m_resizeAnimation(0), m_pixmap(0)
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

void PublicTransportGraphicsItem::toggleExpanded()
{
    setExpanded( !m_expanded );
}

void PublicTransportGraphicsItem::setExpanded( bool expand )
{
    m_expanded = expand;

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
    delete m_resizeAnimation;
    m_resizeAnimation = NULL;
}

void PublicTransportGraphicsItem::updateGeometry()
{
    QGraphicsWidget::updateGeometry();
}

void PublicTransportGraphicsItem::capturePixmap()
{
    delete m_pixmap;
    m_pixmap = NULL;

    QPixmap *pixmap = new QPixmap( size().toSize() );
    pixmap->fill( Qt::transparent );
    QPainter p( pixmap );
    QStyleOptionGraphicsItem option;
    option.rect = rect().toRect();
    paint( &p, &option );
    m_pixmap = pixmap;
}

qreal PublicTransportGraphicsItem::padding() const
{
    return 4.0 * m_parent->zoomFactor();
}

qreal PublicTransportGraphicsItem::unexpandedHeight() const
{
    return qMax( m_parent->iconSize() * 1.1,
                (qreal)QFontMetrics(font()).ascent() * m_parent->maxLineCount() + padding() );
}

bool PublicTransportGraphicsItem::hasExtraIcon() const
{
    if ( !m_item ) {
        // Item was already deleted
        return false;
    }

    QModelIndex modelIndex = index().model()->index( index().row(), ColumnTarget );
    return modelIndex.data( Qt::DecorationRole ).isValid()
            && !modelIndex.data( Qt::DecorationRole ).value<QIcon>().isNull();
}

int PublicTransportGraphicsItem::extraIconSize() const
{
    return m_parent->iconSize() / 2;
}

QTextDocument* TextDocumentHelper::createTextDocument(const QString& html, const QSizeF& size,
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

    for ( int b = 0; b < blockCount; ++b ) {
        QTextLayout *textLayout = document->findBlockByNumber( b ).layout();
        int lines = textLayout->lineCount();
        QPointF position( 0, (textRect.height() - textHeight) / 2.0f );
        for ( int l = 0; l < lines; ++l ) {
            QTextLine textLine = textLayout->lineAt( l );
            textLine.draw( &p, position );

            if ( drawHalos ) {
                QSize textSize = textLine.naturalTextRect().size().toSize();
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


void PublicTransportGraphicsItem::resizeEvent( QGraphicsSceneResizeEvent* event )
{
    QGraphicsWidget::resizeEvent( event );
    updateTextLayouts();
}

void DepartureGraphicsItem::resizeEvent(QGraphicsSceneResizeEvent* event)
{
    PublicTransportGraphicsItem::resizeEvent(event);

    if ( m_routeItem ) {
        QRect _infoRect = infoRect( rect().toRect(), 0 );
        m_routeItem->setPos( _infoRect.left(), rect().top() + unexpandedHeight() + padding() );
        m_routeItem->resize( rect().width() - padding() - _infoRect.left(), ROUTE_ITEM_HEIGHT );
    }
}

void JourneyGraphicsItem::resizeEvent(QGraphicsSceneResizeEvent* event)
{
    PublicTransportGraphicsItem::resizeEvent(event);

    if ( m_routeItem ) {
        QRect _infoRect = infoRect( rect().toRect() );
        m_routeItem->setPos( _infoRect.left(), rect().top() + unexpandedHeight() + padding() );
        m_routeItem->resize( rect().width() - padding() - _infoRect.left(),
                            m_routeItem->size().height() );
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

DepartureGraphicsItem::DepartureGraphicsItem( QGraphicsItem* parent )
    : PublicTransportGraphicsItem( parent ),
    m_infoTextDocument(0), m_timeTextDocument(0), m_routeItem(0), m_leavingAnimation(0)
{
    m_leavingStep = 0.0;
}

DepartureGraphicsItem::~DepartureGraphicsItem()
{
    if ( m_leavingAnimation ) {
        m_leavingAnimation->stop();
    }
}

JourneyGraphicsItem::JourneyGraphicsItem( QGraphicsItem* parent )
    : PublicTransportGraphicsItem( parent ), m_infoTextDocument(0), m_routeItem(0)
{
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
    const QRect _timeRect = timeRect( rect.toRect() );
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
    const QRect _infoRect = infoRect( rect.toRect(), timeWidth );

    // Create layout for the main column showing information about the departure
    if ( !m_infoTextDocument || (m_infoTextDocument->pageSize() != _infoRect.size()) ) {
        delete m_infoTextDocument;
        textOption.setAlignment( Qt::AlignVCenter | Qt::AlignLeft );
        QString html;
        const DepartureInfo *info = departureItem()->departureInfo();
        TimetableWidget *timetableWidget = qobject_cast<TimetableWidget*>( m_parent );
        if ( timetableWidget->isTargetHidden() ) {
            html = i18nc("@info", "<emphasis strong='1'>%1</emphasis>",
                        info->lineString());
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
    const QRect _infoRect = infoRect( rect.toRect()/*, departureTimeWidth, arrivalTimeWidth*/ );
    if ( !m_infoTextDocument || (m_infoTextDocument->pageSize() != _infoRect.size()) ) {
// 		kDebug() << "UPDATE INFO LAYOUT" << font().pointSize() << font().pointSizeF();
        delete m_infoTextDocument;
        textOption.setAlignment( Qt::AlignVCenter | Qt::AlignLeft );
        QString html;
        const JourneyInfo *info = journeyItem()->journeyInfo();
        if ( m_parent->maxLineCount() == 1 ) {
            // Single line string
            html = i18nc("@info", "<emphasis strong='1'>Duration:</emphasis> %1, "
                        "<emphasis strong='1'>Changes:</emphasis> %2",
                        KGlobal::locale()->formatDuration(info->duration()*60*1000),
                        info->changes() == 0
                        ? i18nc("No vehicle changes in a journey", "none")
                        : QString::number(info->changes()));
        } else {
            // Two (or more) line string
            html = i18nc("@info", "<emphasis strong='1'>Duration:</emphasis> %1, "
                        "<emphasis strong='1'>Changes:</emphasis> %2<nl />"
                        "<emphasis strong='1'>Departing:</emphasis> %3, "
                        "<emphasis strong='1'>Arriving:</emphasis> %4",
                        KGlobal::locale()->formatDuration(info->duration()*60*1000),
                        info->changes() == 0
                        ? i18nc("No vehicle changes in a journey", "none")
                        : QString::number(info->changes()),
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
// 	setFlags( QGraphicsItem::ItemIsFocusable/* | QGraphicsItem::ItemIsSelectable*/ );
    setAcceptHoverEvents( true );
    updateGeometry();

    if ( updateLayouts ) {
        delete m_infoTextDocument;
        m_infoTextDocument = NULL;
    }
    updateTextLayouts();

    if ( !item->journeyInfo()->routeStops().isEmpty() ) {
        if ( m_routeItem ) {
            m_routeItem->updateData( item );
        } else {
            m_routeItem = new JourneyRouteGraphicsItem( this, item, m_parent->svg() );
            connect( m_routeItem, SIGNAL(requestJourneys(QString,QString)),
                    this, SIGNAL(requestJourneys(QString,QString)) );
            QRect _infoRect = infoRect( rect().toRect() );
            m_routeItem->setPos( _infoRect.left(), rect().top() + unexpandedHeight() + padding() );
            m_routeItem->resize( rect().width() - padding() - _infoRect.left(),
                                m_routeItem->size().height() );
            m_routeItem->updateData( item );
        }
    } else if ( m_routeItem ) {
        delete m_routeItem;
        m_routeItem = NULL;
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
        m_infoTextDocument = NULL;
        m_timeTextDocument = NULL;
    }
    updateTextLayouts();

    if ( !item->departureInfo()->routeStops().isEmpty() ) {
        if ( m_routeItem ) {
            m_routeItem->updateData( item );
        } else {
            m_routeItem = new RouteGraphicsItem( this, item );
            QRect _infoRect = infoRect( rect().toRect(), 0 );
            m_routeItem->setPos( _infoRect.left(), rect().top() + unexpandedHeight() + padding() );
            m_routeItem->resize( rect().width() - padding() - _infoRect.left(), ROUTE_ITEM_HEIGHT );
            connect( m_routeItem, SIGNAL(requestStopAction(StopAction,QString,RouteStopTextGraphicsItem*)),
                     this, SIGNAL(requestStopAction(StopAction,QString,RouteStopTextGraphicsItem*)) );
        }
    } else if ( m_routeItem ) {
        delete m_routeItem;
        m_routeItem = NULL;
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

qreal DepartureGraphicsItem::timeColumnWidth() const
{
    return TextDocumentHelper::textDocumentWidth( m_timeTextDocument );
}

void JourneyGraphicsItem::paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                 QWidget* widget )
{
    Q_UNUSED( widget );
    painter->setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );
    if ( !m_item || !m_infoTextDocument ) {
        if ( m_pixmap ) {
            painter->drawPixmap( option->rect, *m_pixmap );
        }

        return;
    }

    QColor alternateBackgroundColor = KColorScheme( QPalette::Active, KColorScheme::View )
            .background( KColorScheme::AlternateBackground ).color();
    alternateBackgroundColor.setAlphaF( 0.3 );
    QColor borderColor = textColor();
    borderColor.setAlphaF( 0.5 );
    if ( index().row() % 2 == 1 ) {
        // Draw alternate background
        QLinearGradient bgGradient( 0, 0, 1, 0 );
        bgGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
        bgGradient.setColorAt( 0, Qt::transparent );
        bgGradient.setColorAt( 0.4, alternateBackgroundColor );
        bgGradient.setColorAt( 0.6, alternateBackgroundColor );
        bgGradient.setColorAt( 1, Qt::transparent );

        painter->fillRect( option->rect, QBrush(bgGradient) );
    }

    // Draw a line at the bottom of this TimetableItem
    QLinearGradient borderGradient( 0, 0, 1, 0 );
    borderGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
    borderGradient.setColorAt( 0, Qt::transparent );
    borderGradient.setColorAt( 0.4, borderColor );
    borderGradient.setColorAt( 0.6, borderColor );
    borderGradient.setColorAt( 1, Qt::transparent );
    painter->fillRect( QRect(option->rect.bottomLeft() + QPoint(0, 1),
                            option->rect.bottomRight() - QPoint(0, 1)),
                    QBrush(borderGradient) );

    // Draw journey rating background:
    //   green for relatively short duration, less changes;
    //   red for relatively long duration, more changes (controlled by the model).
    QVariant vr = index().data( JourneyRatingRole );
    if ( vr.isValid() ) {
        qreal rating = vr.toReal();
        QColor ratingColor;
        ratingColor = KColorUtils::mix(
                KColorScheme(QPalette::Active).background( KColorScheme::PositiveBackground ).color(),
                KColorScheme(QPalette::Active).background( KColorScheme::NegativeBackground ).color(),
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
            QLinearGradient bgGradient( 0, 0, 1, 0 );
            bgGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
            bgGradient.setColorAt( 0, Qt::transparent );
            bgGradient.setColorAt( 0.1, ratingColor );
            bgGradient.setColorAt( 0.9, ratingColor );
            bgGradient.setColorAt( 1, Qt::transparent );
            painter->fillRect( option->rect, QBrush( bgGradient ) );
        }
    }

    // Draw special background for departures with an alarm
    if ( index().data(DrawAlarmBackgroundRole).toBool() ) {
// 		qreal bias = index().data( AlarmColorIntensityRole ).toReal();
        QColor alarmColor( 180, 0, 0, 128 );

        QLinearGradient bgGradient( 0, 0, 1, 0 );
        bgGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
        bgGradient.setColorAt( 0, Qt::transparent );
        bgGradient.setColorAt( 0.4, alarmColor );
        bgGradient.setColorAt( 0.6, alarmColor );
        bgGradient.setColorAt( 1, Qt::transparent );

        painter->fillRect( option->rect, QBrush(bgGradient) );
    }

    QFontMetrics fm( font() );
    const QRect _vehicleRect = vehicleRect( option->rect );
    const QRect _infoRect = infoRect( option->rect );

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
    bool drawHalos = /*m_options.testFlag(DrawShadows) &&*/ qGray(_textColor.rgb()) < 156;
    painter->setPen( _textColor );
    TextDocumentHelper::drawTextDocument( painter, option, m_infoTextDocument, _infoRect, drawHalos );
// 	TextDocumentHelper::drawTextDocument( painter, option, m_departureTimeTextDocument, _departureTimeRect, drawHalos );
// 	TextDocumentHelper::drawTextDocument( painter, option, m_arrivalTimeTextDocument, _arrivalTimeRect, drawHalos );

    // Draw an extra icon if there is one (in the target column)
    if ( hasExtraIcon() ) {
        QModelIndex modelIndex = index().model()->index( index().row(), ColumnTarget );
        QIcon icon = modelIndex.data( Qt::DecorationRole ).value<QIcon>();
        painter->drawPixmap( extraIconRect(option->rect/*, departureTimeWidth, arrivalTimeWidth*/),
                            icon.pixmap(extraIconSize()) );
    }

    // Draw expanded items if this TimetableItem isn't currently completely unexpanded
    if ( m_expanded || !qFuzzyIsNull(m_expandStep) ) {
        paintExpanded( painter, option,
                    QRectF(_infoRect.left(), option->rect.top() + unexpandedHeight() + padding(),
                            option->rect.right() - _infoRect.left() - padding(),
                            option->rect.height() - unexpandedHeight() - 2 * padding()) );
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
            return Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor);
        #else
            return Plasma::Theme::defaultTheme()->color(Plasma::Theme::ViewTextColor);
        #endif
    }
}

void DepartureGraphicsItem::paint( QPainter* painter,
        const QStyleOptionGraphicsItem* option, QWidget* widget )
{
    Q_UNUSED( widget );
    painter->setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );
    if ( !m_item || !m_timeTextDocument || !m_infoTextDocument ) {
        if ( m_pixmap ) {
            // Draw captured pixmap, the item in the model is already deleted
            // but still needs to be drawn here while animating
            painter->drawPixmap( option->rect, *m_pixmap );
        }

        return;
    }

    QColor _textColor = textColor();
    QColor _backgroundColor = backgroundColor();
    QColor alternateBackgroundColor = KColorScheme( QPalette::Active, KColorScheme::View )
            .background( KColorScheme::AlternateBackground ).color();
    alternateBackgroundColor.setAlphaF( 0.4 );
    QColor borderColor = _textColor;
    borderColor.setAlphaF( 0.5 );

    if ( index().row() % 2 == 1 ) {
        // Draw alternate background
        QLinearGradient bgGradient( 0, 0, 1, 0 );
        bgGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
        bgGradient.setColorAt( 0, Qt::transparent );
        bgGradient.setColorAt( 0.4, alternateBackgroundColor );
        bgGradient.setColorAt( 0.6, alternateBackgroundColor );
        bgGradient.setColorAt( 1, Qt::transparent );

        painter->fillRect( option->rect, QBrush(bgGradient) );
    }

    // Draw a line at the bottom of this TimetableItem
    QLinearGradient borderGradient( 0, 0, 1, 0 );
    borderGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
    borderGradient.setColorAt( 0, Qt::transparent );
    borderGradient.setColorAt( 0.4, borderColor );
    borderGradient.setColorAt( 0.6, borderColor );
    borderGradient.setColorAt( 1, Qt::transparent );
    painter->fillRect( QRect(option->rect.bottomLeft() + QPoint(0, 1),
                            option->rect.bottomRight() - QPoint(0, 1)),
                    QBrush(borderGradient) );

    // Draw special background for departures in color groups
    QColor bgColor = index().data(Qt::BackgroundColorRole).value<QColor>();
    if ( bgColor != Qt::transparent ) {
        QLinearGradient bgGradient( 0, 0, 1, 0 );
        bgGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
        bgGradient.setColorAt( 0, Qt::transparent );
        bgGradient.setColorAt( 0.4, bgColor );
        bgGradient.setColorAt( 0.6, bgColor );
        bgGradient.setColorAt( 1, Qt::transparent );

        painter->fillRect( option->rect, QBrush(bgGradient) );
    }
    
    // Draw special background for departures with an alarm
    if ( index().data(DrawAlarmBackgroundRole).toBool() ) {
// 		qreal bias = index().data( AlarmColorIntensityRole ).toReal();
        QColor alarmColor( 180, 0, 0, 128 );

        QLinearGradient bgGradient( 0, 0, 1, 0 );
        bgGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
        bgGradient.setColorAt( 0, Qt::transparent );
        bgGradient.setColorAt( 0.4, alarmColor );
        bgGradient.setColorAt( 0.6, alarmColor );
        bgGradient.setColorAt( 1, Qt::transparent );

        painter->fillRect( option->rect, QBrush(bgGradient) );
    }

    QFontMetrics fm( font() );
    const qreal timeWidth = timeColumnWidth();
    const QRect _vehicleRect = vehicleRect( option->rect );
    const QRect _infoRect = infoRect( option->rect, timeWidth );
    const QRect _timeRect = timeRect( option->rect );

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
// 	if ( drawTransportLine ) {
// 		vehicleKey.append( "_empty" );
// 	}

    if ( !m_parent->svg()->hasElement(vehicleKey) ) {
        kDebug() << "SVG element" << vehicleKey << "not found";
    } else {
        // Draw SVG vehicle element into pixmap
        QPixmap pixmap( (int)_vehicleRect.width(), (int)_vehicleRect.height() );
        pixmap.fill( Qt::transparent );
        QPainter p( &pixmap );
        m_parent->svg()->resize( iconSize );
        m_parent->svg()->paint( &p, shadowWidth, shadowWidth, vehicleKey );

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
    }

    // Draw text
    bool drawHalos = /*m_options.testFlag(DrawShadows) &&*/ qGray(_textColor.rgb()) < 156;
    painter->setPen( _textColor );
    TextDocumentHelper::drawTextDocument( painter, option, m_infoTextDocument, _infoRect, drawHalos );
    TextDocumentHelper::drawTextDocument( painter, option, m_timeTextDocument, _timeRect, drawHalos );

    // Draw an extra icon if there is one (in the target column)
    if ( hasExtraIcon() ) {
        QModelIndex modelIndex = index().model()->index( index().row(), ColumnTarget );
        QIcon icon = modelIndex.data( Qt::DecorationRole ).value<QIcon>();
        painter->drawPixmap( extraIconRect(option->rect, timeWidth), icon.pixmap(extraIconSize()) );
    }

    // Draw expanded items if this TimetableItem isn't currently completely unexpanded
    if ( m_expanded || !qFuzzyIsNull(m_expandStep) ) {
        paintExpanded( painter, option,
                    QRectF(_infoRect.left(), option->rect.top() + unexpandedHeight() + padding(),
                            option->rect.right() - _infoRect.left() - padding(),
                            option->rect.height() - unexpandedHeight() - 2 * padding()) );
    }
}

void JourneyGraphicsItem::paintExpanded( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                        const QRectF& rect )
{
    painter->setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );
    QColor _textColor = textColor();
    bool drawHalos = /*m_options.testFlag(DrawShadows) &&*/ qGray(_textColor.rgb()) < 156;

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

// 		y += htmlRect.height() + padding;
    }
}

void DepartureGraphicsItem::paintExpanded( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                        const QRectF& rect )
{
    painter->setRenderHints( QPainter::Antialiasing | QPainter::SmoothPixmapTransform );
    QColor _textColor = textColor();
    bool drawHalos = /*m_options.testFlag(DrawShadows) &&*/ qGray(_textColor.rgb()) < 156;

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
    ChildItem *operatorItem = m_item->childByType( OperatorItem );
    ChildItem *journeyNewsItem = m_item->childByType( JourneyNewsItem );
    if ( delayItem ) {
        html.append( delayItem->formattedText() );
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

// 		y += htmlRect.height() + padding;
    }
}

qreal JourneyGraphicsItem::expandSize() const
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

qreal DepartureGraphicsItem::expandSize() const
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

    ChildItem *journeyNewsItem = m_item->childByType( JourneyNewsItem );
    if ( journeyNewsItem ) {
        extraInformationHeight += 3 * fm.height();
    }
    if ( extraInformationHeight != 0.0 ) {
        height += extraInformationHeight + padding();
    }

    return height * m_expandStep;
}

// TODO move to publictransportitem
QSizeF JourneyGraphicsItem::sizeHint( Qt::SizeHint which, const QSizeF& constraint ) const
{
    if ( which == Qt::MinimumSize ) {
        return QSizeF( 100, (m_expanded || !qFuzzyIsNull(m_expandStep)
                ? (unexpandedHeight() + expandSize()) : unexpandedHeight()) * m_fadeOut );
    } else if ( which == Qt::MaximumSize ) {
        return QSizeF( 100000, (m_expanded || !qFuzzyIsNull(m_expandStep)
                ? (unexpandedHeight() + expandSize()) : unexpandedHeight()) * m_fadeOut );
    } else {
        return QGraphicsWidget::sizeHint( which, constraint );
    }
}

QSizeF DepartureGraphicsItem::sizeHint( Qt::SizeHint which, const QSizeF& constraint ) const
{
    if ( which == Qt::MinimumSize ) {
        return QSizeF( 100, (m_expanded || !qFuzzyIsNull(m_expandStep)
                ? (unexpandedHeight() + expandSize()) : unexpandedHeight()) * m_fadeOut );
    } else if ( which == Qt::MaximumSize ) {
        return QSizeF( 100000, (m_expanded || !qFuzzyIsNull(m_expandStep)
                ? (unexpandedHeight() + expandSize()) : unexpandedHeight()) * m_fadeOut );
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
    : Plasma::ScrollWidget( parent ), m_model(0), m_svg(0)
{
    setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );

    QGraphicsWidget *container = new QGraphicsWidget( this );
    QGraphicsLinearLayout *l = new QGraphicsLinearLayout( Qt::Vertical, container );
    l->setSpacing( 1.0 );
    container->setLayout( l );
    setWidget( container );

    m_maxLineCount = 2;
    m_iconSize = 32;
    m_zoomFactor = 1.0;
}

JourneyTimetableWidget::JourneyTimetableWidget( QGraphicsItem* parent )
    : PublicTransportWidget(parent)
{
}

TimetableWidget::TimetableWidget( QGraphicsItem* parent )
    : PublicTransportWidget(parent)
{
    m_targetHidden = false;
}

void PublicTransportWidget::setModel( PublicTransportModel* model )
{
    m_model = model;

    connect( m_model, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SLOT(rowsInserted(QModelIndex,int,int)) );
    connect( m_model, SIGNAL(journeysAboutToBeRemoved(QList<ItemBase*>)),
            this, SLOT(journeysAboutToBeRemoved(QList<ItemBase*>)) );
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

    return NULL;
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
        painter->drawText( option->rect, m_noItemsText, QTextOption(Qt::AlignCenter) );
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
    for ( int row = topLeft.row(); row <= bottomRight.row(); ++row ) {
        if ( row < m_model->rowCount() ) {
            departureItem( row )->updateData( static_cast<DepartureItem*>(m_model->item(row)), true );
        }
    }
}

void JourneyTimetableWidget::dataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight )
{
    for ( int row = topLeft.row(); row <= bottomRight.row(); ++row ) {
        if ( row < m_model->rowCount() ) {
            journeyItem( row )->updateData( static_cast<JourneyItem*>(m_model->item(row)), true );
        }
    }
}

void PublicTransportWidget::layoutChanged()
{

}

void JourneyTimetableWidget::rowsInserted(const QModelIndex& parent, int first, int last)
{
    if ( parent.isValid() ) {
        kDebug() << "Item with parent" << parent << "Inserted" << first << last;
        return;
    }

    QGraphicsLinearLayout *l = static_cast<QGraphicsLinearLayout*>( widget()->layout() );
    for ( int row = first; row <= last; ++row ) {
        JourneyGraphicsItem *item = new JourneyGraphicsItem( widget() );
        item->setPublicTransportWidget( this );
        item->updateData( static_cast<JourneyItem*>(m_model->item(row)) );
        connect( item, SIGNAL(requestJourneys(QString,QString)),
                 this, SIGNAL(requestJourneys(QString,QString)) );
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
        DepartureGraphicsItem *item = new DepartureGraphicsItem( widget() );
        item->setPublicTransportWidget( this );
        item->updateData( static_cast<DepartureItem*>(m_model->item(row)) );
        connect( item, SIGNAL(requestStopAction(StopAction,QString,RouteStopTextGraphicsItem*)),
                 this, SIGNAL(requestStopAction(StopAction,QString,RouteStopTextGraphicsItem*)) );
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

void PublicTransportWidget::journeysAboutToBeRemoved( const QList< ItemBase* >& journeys )
{
    // Capture pixmaps for departures that will get removed
    // to be able to animate it's disappearance
    foreach ( const ItemBase *item, journeys ) {
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

QRect JourneyGraphicsItem::vehicleRect(const QRect& rect) const
{
    return QRect( rect.left(), rect.top() + (unexpandedHeight() - m_parent->iconSize()) / 2,
                m_parent->iconSize(), m_parent->iconSize() );
}

QRect JourneyGraphicsItem::infoRect( const QRect& rect ) const
{
    return QRect( rect.left() + m_parent->iconSize() * 0.65 + padding(), rect.top(),
                rect.width() - m_parent->iconSize() * 0.65 - 2 * padding()
                - (hasExtraIcon() ? m_parent->iconSize() + padding() : 0),
                unexpandedHeight() );
}

QRect JourneyGraphicsItem::extraIconRect( const QRect& rect ) const
{
    return QRect( rect.right() - extraIconSize() - 2 * padding(),
                rect.top() + (unexpandedHeight() - extraIconSize()) / 2,
                extraIconSize(), extraIconSize() );
}

QRect DepartureGraphicsItem::vehicleRect(const QRect& rect) const {
    return QRect( rect.left(), rect.top() + (unexpandedHeight() - m_parent->iconSize()) / 2,
                m_parent->iconSize(), m_parent->iconSize() );
}

QRect DepartureGraphicsItem::infoRect(const QRect& rect, qreal timeColumnWidth) const {
    return QRect( rect.left() + m_parent->iconSize() * 0.65 + padding(), rect.top(),
                rect.width() - m_parent->iconSize() * 0.65 - 2 * padding() - timeColumnWidth
                - (hasExtraIcon() ? m_parent->iconSize() + padding() : 0),
                unexpandedHeight() );
}

QRect DepartureGraphicsItem::extraIconRect(const QRect& rect, qreal timeColumnWidth) const {
    return QRect( rect.right() - timeColumnWidth - extraIconSize() - 2 * padding(),
                rect.top() + (unexpandedHeight() - extraIconSize()) / 2,
                extraIconSize(), extraIconSize() );
}

QRect DepartureGraphicsItem::timeRect(const QRect& rect) const {
    if ( qobject_cast<TimetableWidget*>(m_parent)->isTargetHidden() ) {
        return QRect( rect.width() / 4, rect.top(), rect.width() * 3 / 4 - padding(), unexpandedHeight() );
    } else {
        return QRect( rect.width() / 2, rect.top(), rect.width() / 2 - padding(), unexpandedHeight() );
    }
}
