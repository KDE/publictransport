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

#ifndef TIMETABLEWIDGET_H
#define TIMETABLEWIDGET_H

#include "departuremodel.h"

#include <Plasma/ScrollWidget>
#include <KDebug>

#include <QGraphicsWidget>
#include <QModelIndex>
#include <QPainter>
#include <QPointer>

namespace Plasma
{
    class Svg;
}
class KMenu;

class QPropertyAnimation;
class QWidget;
class QTextLayout;
class QStyleOptionGraphicsItem;
class QPainter;

class RouteGraphicsItem;
class RouteStopTextGraphicsItem;
class JourneyRouteGraphicsItem;
class DepartureModel;
class DepartureItem;
class TimetableWidget;
class PublicTransportWidget;

/**
 * @brief Abstract base class for DepartureGraphicsItem and JourneyGraphicsItem.
 *
 * For this QGraphicsWidget an expanded state can be toggled by clicking it. The area only visible
 * when expanded gets painted in @ref paintExpanded, the size of that area can be given by
 * overwriting @ref expandSize. To programatically change the expanded state use @ref toggleExpanded
 * or @ref setExpanded, get the state using @ref isExpanded.
 **/
class PublicTransportGraphicsItem : public QGraphicsWidget {
    Q_OBJECT
    Q_PROPERTY( qreal expandStep READ expandStep WRITE setExpandStep )
    Q_PROPERTY( qreal fadeOut READ fadeOut WRITE setFadeOut )
    friend class PublicTransportWidget;

public:
    explicit PublicTransportGraphicsItem( PublicTransportWidget* publicTransportWidget,
                                          QGraphicsItem* parent = 0,
                                          StopAction *copyStopToClipboardAction = 0/*,
                                          QAction *toggleAlarmAction = 0*/ );
    virtual ~PublicTransportGraphicsItem();

    /** @brief The height of route items. */
    static const qreal ROUTE_ITEM_HEIGHT = 60.0;

    PublicTransportWidget *publicTransportWidget() const { return m_parent; };
    QModelIndex index() const { return m_item->index(); };

    virtual QSizeF sizeHint( Qt::SizeHint which, const QSizeF& constraint = QSizeF() ) const;

    /**
     * @brief Gets the size of the always shown part (without the expand area).
     *
     * The default implementation uses the PublicTransportWidget given in the constructor to
     * calculate a size, using @ref PublicTransportWidget::iconSize and
     * @ref PublicTransportWidget::maxLineCount.
     *
     * If you overwrite this method you should remember to add @ref padding.
     **/
    virtual qreal unexpandedHeight() const;

    /** @brief Overwrite to calculate the height of the expand area. */
    virtual qreal expandAreaHeight() const = 0;

    /**
     * @brief Overwrite to calculate the indentation of the expand area.
     *
     * The default implementation returns 0.0, ie. no indentation.
     **/
    virtual qreal expandAreaIndentation() const { return 0.0; };

    /**
     * @brief The extend of additional icons, eg. an alarm icons or journey news hint icons.
     *
     * The default implementation uses half of @ref PublicTransportWidget::iconSize, making
     * additional icons half as big as the vehicle type icons.
     **/
    int extraIconSize() const;

    /**
     * @brief The padding to the borders of this widget
     *
     * The default implementation scales with @ref PublicTransportWidget::zoomFactor.
     **/
    inline qreal padding() const;

    /**
     * @brief Checks whether or not there is a extra icon in the model in the given @p column.
     *
     * @param column The column to check for an icon. Default is ColumnTarget.
     * 
     * @return True, if there is an icon in the given @p column, ie. there is a QIcon stored in
     *   the model for the @ref Qt::DecorationRole.
     **/
    bool hasExtraIcon( Columns column = ColumnTarget ) const;

    qreal fadeOut() const { return m_fadeOut; };
    void setFadeOut( qreal fadeOut ) { m_fadeOut = fadeOut; updateGeometry(); };

    /** @brief Whether or not this item is valid, ie. it has data for painting. */
    virtual bool isValid() const { return true; };

    /** @brief Whether or not the expand area is currently shown or not. */
    bool isExpanded() const { return m_expanded; };

    /** @brief Toggles the expanded state, ie. showing/hiding the expand area. */
    inline void toggleExpanded() { setExpanded(!m_expanded); };

    /**
     * @brief Sets the expanded state to @p expand.
     *
     * @param expand Whether or not the expand area should be shown. Default is true.
     **/
    void setExpanded( bool expand = true );

    qreal expandStep() const { return m_expandStep; };
    void setExpandStep( qreal expandStep ) { m_expandStep = expandStep; updateGeometry(); };

    /**
     * @brief Paints this item to an internal cache.
     *
     * The cached pixmap can later be used for drawing, if the model data is already deleted.
     * So it's a good idea to call this method directly before the model data gets deleted, eg.
     * by connecting to @ref PublicTransportModel::departuresAboutToBeRemoved or
     * @ref PublicTransportModel::journeysAboutToBeRemoved.
     **/
    void capturePixmap();

    /**
     * @brief Calls @ref paintItem and @ref paintExpanded, if expanded.
     *
     * To draw the item (without the expand area) @ref paintItem gets called always from here.
     * If this item is currently expanded (ie. @ref isExpanded returns true) or partly expanded
     * (@ref expandStep > 0) @ref paintExpanded gets called to draw to the expand area.
     *
     * @note You should not need to overwrite this method, but more likely @ref paintItem
     *   and @ref paintExpanded.
     *
     * @param painter The painter to use for painting.
     * @param option Style options.
     * @param widget
     **/
    virtual void paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                        QWidget* widget = 0 );

    /**
     * @brief Called to paint the background of this item, including the expand area (if expanded).
     *
     * This gets always called on @ref paint.
     *
     * @param painter The painter to use for painting.
     * @param option Style options.
     * @param rect The rectangle in which to paint. It's heihgt changes when the expand area
     *   gets toggled.
     **/
    virtual void paintBackground( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                  const QRectF &rect ) = 0;

    /**
     * @brief Called to paint this item, without the expand area.
     *
     * This gets always called on @ref paint.
     *
     * @param painter The painter to use for painting.
     * @param option Style options.
     * @param rect The rectangle in which to paint.
     **/
    virtual void paintItem( QPainter* painter, const QStyleOptionGraphicsItem* option,
                            const QRectF &rect ) = 0;

    /**
     * @brief Called to paint the expand area.
     * 
     * This gets only called if @ref isExpanded returns true or if @ref expandStep is bigger
     * than zero.
     *
     * @param painter The painter to use for painting.
     * @param option Style options.
     * @param rect The rectangle in which to paint.
     **/
    virtual void paintExpanded( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                const QRectF &rect ) = 0;

    /** @brief The color to be used for text. */
    virtual QColor textColor() const;

    /** @brief The color to be used as background. */
    virtual inline QColor backgroundColor() const {
        #if KDE_VERSION < KDE_MAKE_VERSION(4,6,0)
            return Plasma::Theme::defaultTheme()->color(Plasma::Theme::BackgroundColor);
        #else
            return Plasma::Theme::defaultTheme()->color(Plasma::Theme::ViewBackgroundColor);
        #endif
    };

signals:
    /**
     * @brief Emitted, if a stop action was triggered from a route stop's context menu.
     *
     * @param stopAction The action to execute.
     * @param stopName The stop name for which the action should be executed.
     **/
    void requestStopAction( StopAction::Type stopAction, const QString &stopName );
    
    void requestAlarmCreation( const QDateTime &departure, const QString &lineString,
                               VehicleType vehicleType, const QString &target,
                               QGraphicsWidget *item );
    void requestAlarmDeletion( const QDateTime &departure, const QString &lineString,
                               VehicleType vehicleType, const QString &target,
                               QGraphicsWidget *item );

protected slots:
    void resizeAnimationFinished();

protected:
    virtual void resizeEvent( QGraphicsSceneResizeEvent* event );
    virtual void mousePressEvent( QGraphicsSceneMouseEvent* event );
    virtual void mouseReleaseEvent( QGraphicsSceneMouseEvent* event );
    virtual void updateGeometry();
    virtual void updateTextLayouts() = 0;

    QPointer<TopLevelItem> m_item;
    PublicTransportWidget *m_parent;
    bool m_expanded;
    qreal m_expandStep;
    qreal m_fadeOut;
    QPropertyAnimation *m_resizeAnimation;
    QPixmap *m_pixmap;
    StopAction *m_copyStopToClipboardAction;
//     QAction *m_toggleAlarmAction;
};

class TextDocumentHelper {
public:
    static QTextDocument *createTextDocument( const QString &html, const QSizeF &size,
            const QTextOption &textOption, const QFont &font );
    static void drawTextDocument( QPainter *painter, const QStyleOptionGraphicsItem* option,
                                QTextDocument *document, const QRect &textRect, bool drawHalos );
    static qreal textDocumentWidth( QTextDocument *document );
};

/**
* @brief A QGraphicsWidget representing a departure/arrival with public transport.
**/
class DepartureGraphicsItem : public PublicTransportGraphicsItem {
    Q_OBJECT
    Q_PROPERTY( qreal leavingStep READ leavingStep WRITE setLeavingStep )
    friend class TimetableWidget;

public:
    explicit DepartureGraphicsItem( PublicTransportWidget* publicTransportWidget,
                                    QGraphicsItem* parent = 0,
                                    StopAction *copyStopToClipboardAction = 0,
                                    StopAction *showDeparturesAction = 0,
                                    StopAction *highlightStopAction = 0,
                                    StopAction *newFilterViaStopAction = 0/*,
                                    QAction *toggleAlarmAction = 0*/ );
    virtual ~DepartureGraphicsItem();

    void updateData( DepartureItem* item, bool update = false );
    inline DepartureItem *departureItem() const { return qobject_cast<DepartureItem*>(m_item); };

    /** @brief Whether or not this item is valid, ie. it has data for painting. */
    virtual bool isValid() const { return m_infoTextDocument && m_timeTextDocument; };
    
    /** @brief The size of the expand area. */
    virtual qreal expandAreaHeight() const;

    /** @brief The indentation of the expand area. */
    virtual qreal expandAreaIndentation() const;

    virtual void paintBackground( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                  const QRectF &rect );
    virtual void paintItem( QPainter* painter, const QStyleOptionGraphicsItem* option,
                            const QRectF &rect );
    virtual void paintExpanded( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                const QRectF &rect );

    QRectF vehicleRect( const QRectF &rect ) const;
    QRectF infoRect( const QRectF &rect, qreal timeColumnWidth ) const;
    QRectF extraIconRect( const QRectF &rect, qreal timeColumnWidth ) const;
    QRectF timeRect( const QRectF &rect ) const;

    qreal leavingStep() const { return m_leavingStep; };
    void setLeavingStep( qreal leavingStep );

protected:
    virtual void updateTextLayouts();
    qreal timeColumnWidth() const;
    virtual void resizeEvent( QGraphicsSceneResizeEvent* event );

private:
    QTextDocument *m_infoTextDocument;
    QTextDocument *m_timeTextDocument;
    RouteGraphicsItem *m_routeItem;

    QPropertyAnimation *m_leavingAnimation;
    qreal m_leavingStep;

    StopAction *m_showDeparturesAction;
    StopAction *m_highlightStopAction;
    StopAction *m_newFilterViaStopAction;
};

/**
 * @brief A QGraphicsWidget representing a journey with public transport from A to B.
 **/
class JourneyGraphicsItem : public PublicTransportGraphicsItem {
    Q_OBJECT
    friend class TimetableWidget;

public:
    explicit JourneyGraphicsItem( PublicTransportWidget* publicTransportWidget,
                                  QGraphicsItem* parent = 0,
                                  StopAction *copyStopToClipboardAction = 0,
                                  StopAction *requestJourneyToStopAction = 0,
                                  StopAction *requestJourneyFromStopAction = 0/*,
                                  QAction *toggleAlarmAction = 0*/ );

    void updateData( JourneyItem* item, bool update = false );
    inline JourneyItem *journeyItem() const { return qobject_cast<JourneyItem*>(m_item); };

    /** @brief Whether or not this item is valid, ie. it has data for painting. */
    virtual bool isValid() const { return m_infoTextDocument; };
    
    /** @brief The size of the expand area. */
    virtual qreal expandAreaHeight() const;
    
    /** @brief The indentation of the expand area. */
    virtual qreal expandAreaIndentation() const;

    virtual void paintBackground( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                  const QRectF &rect );
    virtual void paintItem( QPainter* painter, const QStyleOptionGraphicsItem* option,
                            const QRectF &rect );
    virtual void paintExpanded( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                const QRectF &rect );

    QRectF vehicleRect( const QRectF &rect ) const;
    QRectF infoRect( const QRectF &rect ) const;
    QRectF extraIconRect( const QRectF &rect ) const;

protected:
    virtual void resizeEvent( QGraphicsSceneResizeEvent* event );
    virtual void contextMenuEvent( QGraphicsSceneContextMenuEvent* event );
    virtual void updateTextLayouts();

private:
    QTextDocument *m_infoTextDocument;
    JourneyRouteGraphicsItem *m_routeItem;
    StopAction *m_requestJourneyToStopAction;
    StopAction *m_requestJourneyFromStopAction;
};

/**
* @brief Base class for TimetableWidget and JourneyTimetableWidget.
**/
class PublicTransportWidget : public Plasma::ScrollWidget
{
    Q_OBJECT
    Q_PROPERTY( QString noItemsText READ noItemsText WRITE setNoItemsText )
    friend class PublicTransportGraphicsItem;
    friend class DepartureItem;

public:
    PublicTransportWidget( QGraphicsItem* parent = 0 );

    PublicTransportModel *model() const { return m_model; };
    void setModel( PublicTransportModel *model );
    PublicTransportGraphicsItem *item( int row ) { return m_items[row]; };
    PublicTransportGraphicsItem *item( const QModelIndex &index );

    void setSvg( Plasma::Svg *svg ) { m_svg = svg; };
    Plasma::Svg *svg() const { return m_svg; };

    void setIconSize( qreal iconSize ) { m_iconSize = iconSize; updateItemLayouts(); };
    qreal iconSize() const { return m_maxLineCount == 1 ? m_iconSize * m_zoomFactor * 0.75
            : m_iconSize * m_zoomFactor; };

    void setZoomFactor( qreal zoomFactor = 1.0 ) { m_zoomFactor = zoomFactor; };
    qreal zoomFactor() const { return m_zoomFactor; };

    void setNoItemsText( const QString &noItemsText ) {
        m_noItemsText = noItemsText;
        update();
    };
    QString noItemsText() const { return m_noItemsText; };

    void setMaxLineCount( int maxLineCount ) { m_maxLineCount = maxLineCount; updateItemGeometries(); };
    int maxLineCount() const { return m_maxLineCount; };

    /** Call this eg. when the DepartureArrivalListType changes in the model
     * (only header data gets changed...). */
    void updateItemLayouts();

signals:
    void contextMenuRequested( PublicTransportGraphicsItem *item, const QPointF &pos );

    /**
     * @brief Emitted, if a stop action was triggered from a route stop's context menu.
     *
     * @param stopAction The action to execute.
     * @param stopName The stop name for which the action should be executed.
     **/
    void requestStopAction( StopAction::Type stopAction, const QString &stopName );
    
    void requestAlarmCreation( const QDateTime &departure, const QString &lineString,
                               VehicleType vehicleType, const QString &target,
                               QGraphicsWidget *item );
    void requestAlarmDeletion( const QDateTime &departure, const QString &lineString,
                               VehicleType vehicleType, const QString &target,
                               QGraphicsWidget *item );

protected slots:
    void journeysAboutToBeRemoved( const QList<ItemBase*> &journeys );
    virtual void rowsRemoved( const QModelIndex &parent, int first, int last );
    virtual void modelReset();
    virtual void layoutChanged();
    virtual void dataChanged( const QModelIndex &topLeft, const QModelIndex &bottomRight ) = 0;

protected:
    virtual QSizeF sizeHint( Qt::SizeHint which, const QSizeF& constraint ) const;
    virtual void contextMenuEvent( QGraphicsSceneContextMenuEvent* event );
    virtual void paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                        QWidget* widget = 0 );
    void updateItemGeometries();
    virtual void setupActions();

    PublicTransportModel *m_model;
    QList<PublicTransportGraphicsItem*> m_items;
    Plasma::Svg *m_svg;
    qreal m_iconSize;
    qreal m_zoomFactor;
    int m_maxLineCount;
    QString m_noItemsText;
    StopAction *m_copyStopToClipboardAction;
//     QAction *m_toggleAlarmAction;
};

/**
* @brief A Plasma::ScrollWidget containing DepartureGraphicsItems.
**/
class TimetableWidget : public PublicTransportWidget
{
    Q_OBJECT
    friend class PublicTransportGraphicsItem;
    friend class DepartureGraphicsItem;

public:
    TimetableWidget( QGraphicsItem* parent = 0 );

    void setTargetHidden( bool targetHidden ) { m_targetHidden = targetHidden; updateItemLayouts(); };
    bool isTargetHidden() const { return m_targetHidden; };

    inline DepartureGraphicsItem *departureItem( int row ) {
        return qobject_cast<DepartureGraphicsItem*>(item(row));
    };
    inline DepartureGraphicsItem *departureItem( const QModelIndex &index ) {
        return qobject_cast<DepartureGraphicsItem*>(item(index));
    };
    inline DepartureModel *departureModel() const {
        return qobject_cast<DepartureModel*>(m_model);
    };

protected slots:
    virtual void rowsInserted( const QModelIndex &parent, int first, int last );
    virtual void dataChanged( const QModelIndex &topLeft, const QModelIndex &bottomRight );

protected:
    virtual void setupActions();

private:
    bool m_targetHidden;
    StopAction *m_showDeparturesAction;
    StopAction *m_highlightStopAction;
    StopAction *m_newFilterViaStopAction;
};

/**
* @brief A Plasma::ScrollWidget containing JourneyGraphicsItems.
**/
class JourneyTimetableWidget : public PublicTransportWidget
{
    Q_OBJECT
    friend class PublicTransportGraphicsItem;

public:
    JourneyTimetableWidget( QGraphicsItem* parent = 0 );

    inline JourneyGraphicsItem *journeyItem( int row ) {
        return qobject_cast<JourneyGraphicsItem*>(item(row));
    };
    inline JourneyGraphicsItem *journeyItem( const QModelIndex &index ) {
        return qobject_cast<JourneyGraphicsItem*>(item(index));
    };
    inline JourneyModel *journeyModel() const {
        return qobject_cast<JourneyModel*>(m_model);
    };

protected slots:
    virtual void rowsInserted( const QModelIndex &parent, int first, int last );
    virtual void dataChanged( const QModelIndex &topLeft, const QModelIndex &bottomRight );

protected:
    virtual void setupActions();

private:
    StopAction *m_requestJourneyToStopAction;
    StopAction *m_requestJourneyFromStopAction;
};

#endif // TIMETABLEWIDGET_H
