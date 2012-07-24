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

#ifndef TIMETABLEWIDGET_H
#define TIMETABLEWIDGET_H

// Own includes
#include "stopaction.h" // for StopAction::Type
#include "departuremodel.h"

// Plasma includes
#include <Plasma/ScrollWidget> // Base class

// Qt includes
#include <QGraphicsWidget> // Base class
#include <QPointer> // Member variable

/** @file
 * @brief This file contains the TimetableWidget / JourneyTimetableWidget and it's item classes.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

class KPixmapCache;
namespace Plasma
{
    class Svg;
}

class QPropertyAnimation;
class QWidget;
class QStyleOptionGraphicsItem;
class QPainter;
class QTextOption;

class RouteGraphicsItem;
class JourneyRouteGraphicsItem;
class DepartureModel;
class DepartureItem;
class TimetableWidget;
class PublicTransportWidget;

/**
 * @brief Abstract base class for DepartureGraphicsItem and JourneyGraphicsItem.
 *
 * For this QGraphicsWidget an expand area can be shown/hidden by clicking it.
 *
 * The item itself (without the expand area) gets drawn in @ref paintItem, while the background
 * gets drawn in @ref paintBackground (stretched over the expand area). The area only visible when
 * expanded gets drawn in @ref paintExpanded.
 *
 * The height of the item without the expand area can be given by overwriting @ref unexpandedHeight,
 * overwrite @ref expandAreaHeight to give the height of the expand area. The expand area can be
 * indented by overwriting @ref expandAreaIndentation, by default no indentation is used.
 *
 * To programatically change the expanded state use @ref toggleExpanded or @ref setExpanded, get
 * the state using @ref isExpanded.
 **/
class PublicTransportGraphicsItem : public QGraphicsWidget {
    Q_OBJECT
    Q_PROPERTY( qreal expandStep READ expandStep WRITE setExpandStep )
    Q_PROPERTY( qreal fadeOut READ fadeOut WRITE setFadeOut )
    friend class PublicTransportWidget;

public:
    explicit PublicTransportGraphicsItem( PublicTransportWidget* publicTransportWidget,
                                          QGraphicsItem* parent = 0,
                                          StopAction *copyStopToClipboardAction = 0,
                                          StopAction *showInMapAction = 0 );
    virtual ~PublicTransportGraphicsItem();

    /** @brief The height of route items. */
    static const qreal ROUTE_ITEM_HEIGHT;

    /** @brief A pointer to the containing PublicTransportWidget. */
    PublicTransportWidget *publicTransportWidget() const { return m_parent; };

    /** @brief The QModelIndex to the data for this item. */
    QModelIndex index() const { return m_item ? m_item->index() : QModelIndex(); };

    /**
     * @brief Notifies this item about changed settings in the parent PublicTransportWidget.
     *
     * The default implementation does nothing.
     **/
    virtual void updateSettings() {};

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

    /**
     * @brief The amount between 0 and 1 the item is currently expanded.
     *
     * 0 means not expanded, 1 means fully expanded, everything in between means partly expanded.
     **/
    qreal expandStep() const { return m_expandStep; };

    /**
     * @brief Sets the amount between 0 and 1 the item should be expanded to @p expandStep.
     *
     * This gets used to animate showing/hiding of the expand area.
     *
     * @param expandStep The new expand amount between 0 and 1. 0 means not expanded, 1 means
     *   fully expanded, everything in between means partly expanded.
     **/
    void setExpandStep( qreal expandStep ) { m_expandStep = expandStep; updateGeometry(); };

    /**
     * @brief The amount between 0 and 1 the item is currently faded out.
     *
     * 0 means fully shown, 1 means fully faded out, everything in between means partly faded out.
     **/
    qreal fadeOut() const { return m_fadeOut; };

    /**
     * @brief Sets the amount between 0 and 1 the item should be faded out to @p fadeOut.
     *
     * This gets used to animate hiding/deletion of PublicTransportGraphicsItem's.
     *
     * @param fadeOut The new fade out amount between 0 and 1. 0 means not faded out, 1 means
     *   fully faded out, everything in between means partly faded out.
     **/
    void setFadeOut( qreal fadeOut ) { m_fadeOut = fadeOut; updateGeometry(); };

    /**
     * @brief Paints this item to an internal cache.
     *
     * The cached pixmap can later be used for drawing, if the model data is already deleted.
     * So it's a good idea to call this method directly before the model data gets deleted, eg.
     * by connecting to @ref PublicTransportModel::itemsAboutToBeRemoved.
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
     * @param rect The rectangle in which to paint. It's height changes when the expand area
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
    virtual QColor backgroundColor() const;

signals:
    void expandedStateChanged( PublicTransportGraphicsItem *item, bool expanded );

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
    virtual QGraphicsWidget *routeItem() const = 0;
    virtual void drawFadeOutLeftAndRight( QPainter *painter, const QRect &rect, int fadeWidth = 40 );
    virtual void drawAlarmBackground( QPainter *painter, const QRect &rect );

    QPointer<TopLevelItem> m_item;
    PublicTransportWidget *m_parent;
    bool m_expanded;
    qreal m_expandStep;
    qreal m_fadeOut;
    QPropertyAnimation *m_resizeAnimation;
    QPixmap *m_pixmap;
    StopAction *m_copyStopToClipboardAction;
    StopAction *m_showInMapAction;
};

class TextDocumentHelper {
public:
    enum Option {
        DoNotDrawShadowOrHalos, DrawShadows, DrawHalos, DefaultOption = DrawShadows
    };

    static QTextDocument *createTextDocument( const QString &html, const QSizeF &size,
            const QTextOption &textOption, const QFont &font );
    static void drawTextDocument( QPainter *painter, const QStyleOptionGraphicsItem* option,
            QTextDocument *document, const QRect &textRect, Option options = DefaultOption );
    static qreal textDocumentWidth( QTextDocument *document );
};

/**
 * @brief A QGraphicsWidget representing a departure/arrival with public transport.
 *
 * It gets normally added into @ref TimetableWidget and can include other items like
 * @ref RouteGraphicsItem.
 **/
class DepartureGraphicsItem : public PublicTransportGraphicsItem {
    Q_OBJECT
    Q_PROPERTY( qreal leavingStep READ leavingStep WRITE setLeavingStep )
    friend class TimetableWidget;

public:
    explicit DepartureGraphicsItem( PublicTransportWidget* publicTransportWidget,
                                    QGraphicsItem* parent = 0,
                                    StopAction *copyStopToClipboardAction = 0,
                                    StopAction *showInMapAction = 0,
                                    StopAction *showDeparturesAction = 0,
                                    StopAction *highlightStopAction = 0,
                                    StopAction *newFilterViaStopAction = 0,
                                    KPixmapCache *pixmapCache = 0 );
    virtual ~DepartureGraphicsItem();

    /**
     * @brief Updates this graphics item to visualize the given @p item.
     *
     * @param item The item with the new data.
     * @param update Whether or not to update the layouts of the QTextDocuments. Default is false.
     **/
    void updateData( DepartureItem* item, bool update = false );

    /** @brief Notifies this item about changed settings in the parent PublicTransportWidget. */
    virtual void updateSettings();

    /** @brief The DepartureItem representing this item in the model. */
    inline DepartureItem *departureItem() const { return qobject_cast<DepartureItem*>(m_item); };

    /** @brief Whether or not this item is valid, ie. it has data for painting. */
    virtual bool isValid() const { return m_infoTextDocument && m_timeTextDocument; };

    /** @brief The size of the expand area. */
    virtual qreal expandAreaHeight() const;

    /** @brief The indentation of the expand area. */
    virtual qreal expandAreaIndentation() const;

    /**
     * @brief Paint the background of this item, including the expand area (if expanded).
     *
     * This gets always called on @ref paint.
     *
     * @param painter The painter to use for painting.
     * @param option Style options.
     * @param rect The rectangle in which to paint. It's height changes when the expand area
     *   gets toggled.
     **/
    virtual void paintBackground( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                  const QRectF &rect );

    /**
     * @brief Paint this item, without the expand area.
     *
     * This gets always called on @ref paint.
     *
     * @param painter The painter to use for painting.
     * @param option Style options.
     * @param rect The rectangle in which to paint.
     **/
    virtual void paintItem( QPainter* painter, const QStyleOptionGraphicsItem* option,
                            const QRectF &rect );

    /**
     * @brief Paint the expand area.
     *
     * This gets only called if @ref isExpanded returns true or if @ref expandStep is bigger
     * than zero.
     *
     * @param painter The painter to use for painting.
     * @param option Style options.
     * @param rect The rectangle in which to paint.
     **/
    virtual void paintExpanded( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                const QRectF &rect );

    /**
     * @brief Calculates the bounding rect for the vehicle type icon.
     *
     * @param rect The rect of the item (without the expand area).
     **/
    QRectF vehicleRect( const QRectF &rect ) const;

    /**
     * @brief Calculates the bounding rect for the info text (line string and target).
     *
     * @param rect The rect of the item (without the expand area).
     * @param timeColumnWidth The width of the time column.
     **/
    QRectF infoRect( const QRectF &rect, qreal timeColumnWidth ) const;

    /**
     * @brief Calculates the bounding rect for the departure/arrival time text.
     *
     * @param rect The rect of the item (without the expand area).
     **/
    QRectF timeRect( const QRectF &rect ) const;

    /**
     * @brief Calculates the bounding rect for an additional icon.
     *
     * @param rect The rect of the item (without the expand area).
     * @param timeColumnWidth The width of the time column.
     **/
    QRectF extraIconRect( const QRectF &rect, qreal timeColumnWidth ) const;

    /**
     * @brief A value between 0 and 1 to control the leaving animation.
     *
     * The leaving animation (currently) makes the item fade in and out to show that the associated
     * departure is leaving soon.
     **/
    qreal leavingStep() const { return m_leavingStep; };

    /** @brief Set a value between 0 and 1 to control the leaving animation. */
    void setLeavingStep( qreal leavingStep );

protected:
    virtual void updateTextLayouts();
    qreal timeColumnWidth() const;
    virtual void resizeEvent( QGraphicsSceneResizeEvent* event );
    virtual QGraphicsWidget *routeItem() const;

private:
    QTextDocument *m_infoTextDocument;
    QTextDocument *m_timeTextDocument;
    RouteGraphicsItem *m_routeItem; // Pointer to the route item or 0 if no route data is available
    bool m_highlighted;

    QPropertyAnimation *m_leavingAnimation;
    qreal m_leavingStep;

    StopAction *m_showDeparturesAction;
    StopAction *m_highlightStopAction;
    StopAction *m_newFilterViaStopAction;

    KPixmapCache *m_pixmapCache;
};

/**
 * @brief A QGraphicsWidget representing a journey with public transport from A to B.
 *
 * It gets normally added into @ref JourneyTimetableWidget and can include other items like
 * @ref JourneyRouteGraphicsItem.
 **/
class JourneyGraphicsItem : public PublicTransportGraphicsItem {
    Q_OBJECT
    friend class TimetableWidget;

public:
    explicit JourneyGraphicsItem( PublicTransportWidget* publicTransportWidget,
                                  QGraphicsItem* parent = 0,
                                  StopAction *copyStopToClipboardAction = 0,
                                  StopAction *showInMapAction = 0,
                                  StopAction *requestJourneyToStopAction = 0,
                                  StopAction *requestJourneyFromStopAction = 0 );
    virtual ~JourneyGraphicsItem();

    /**
     * @brief Updates this graphics item to visualize the given @p item.
     *
     * @param item The item with the new data.
     * @param update Whether or not to update the layout of the QTextDocument. Default is false.
     **/
    void updateData( JourneyItem* item, bool update = false );

    /** @brief Notifies this item about changed settings in the parent PublicTransportWidget. */
    virtual void updateSettings();

    /** @brief The JourneyItem representing this item in the model. */
    inline JourneyItem *journeyItem() const { return qobject_cast<JourneyItem*>(m_item); };

    /** @brief Whether or not this item is valid, ie. it has data for painting. */
    virtual bool isValid() const { return m_infoTextDocument; };

    /** @brief The size of the expand area. */
    virtual qreal expandAreaHeight() const;

    /** @brief The indentation of the expand area. */
    virtual qreal expandAreaIndentation() const;

    /**
     * @brief Paint the background of this item, including the expand area (if expanded).
     *
     * This gets always called on @ref paint.
     *
     * @param painter The painter to use for painting.
     * @param option Style options.
     * @param rect The rectangle in which to paint. It's heihgt changes when the expand area
     *   gets toggled.
     **/
    virtual void paintBackground( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                  const QRectF &rect );

    /**
     * @brief Paint this item, without the expand area.
     *
     * This gets always called on @ref paint.
     *
     * @param painter The painter to use for painting.
     * @param option Style options.
     * @param rect The rectangle in which to paint.
     **/
    virtual void paintItem( QPainter* painter, const QStyleOptionGraphicsItem* option,
                            const QRectF &rect );

    /**
     * @brief Paint the expand area.
     *
     * This gets only called if @ref isExpanded returns true or if @ref expandStep is bigger
     * than zero.
     *
     * @param painter The painter to use for painting.
     * @param option Style options.
     * @param rect The rectangle in which to paint.
     **/
    virtual void paintExpanded( QPainter* painter, const QStyleOptionGraphicsItem* option,
                                const QRectF &rect );

    /**
     * @brief Calculates the bounding rect for the vehicle type icon.
     *
     * @param rect The rect of the item (without the expand area).
     **/
    QRectF vehicleRect( const QRectF &rect ) const;

    /**
     * @brief Calculates the bounding rect for the info text (departure, arrival, changes, duration).
     *
     * @param rect The rect of the item (without the expand area).
     * @param timeColumnWidth The width of the time column.
     **/
    QRectF infoRect( const QRectF &rect ) const;

    /**
     * @brief Calculates the bounding rect for an additional icon.
     *
     * @param rect The rect of the item (without the expand area).
     * @param timeColumnWidth The width of the time column.
     **/
    QRectF extraIconRect( const QRectF &rect ) const;

protected:
    virtual void resizeEvent( QGraphicsSceneResizeEvent* event );
    virtual void contextMenuEvent( QGraphicsSceneContextMenuEvent* event );
    virtual void updateTextLayouts();
    virtual QGraphicsWidget *routeItem() const;

private:
    QTextDocument *m_infoTextDocument;
    JourneyRouteGraphicsItem *m_routeItem; // Pointer to the route item or 0 if no route data is available
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
    enum Option {
        NoOption                = 0x0000, /**< No special option. */
        DrawShadowsOrHalos      = 0x0001, /**< Draw shadows/halos behind text, dependend on color. */

        DefaultOptions = DrawShadowsOrHalos /**< Options used by default */
    };
    Q_DECLARE_FLAGS( Options, Option )

    PublicTransportWidget( Options options = DefaultOptions, QGraphicsItem* parent = 0 );

    /** @brief Gets the model containing the data for this widget. */
    PublicTransportModel *model() const { return m_model; };

    /** @brief Sets the model containing the data for this widget to @p model. */
    void setModel( PublicTransportModel *model );

    /** @brief Gets the item at the given @p row. */
    PublicTransportGraphicsItem *item( int row ) { return m_items[row]; };

    /** @brief Gets the item with the given @p index. */
    PublicTransportGraphicsItem *item( const QModelIndex &index );

    void setSvg( Plasma::Svg *svg ) { m_svg = svg; };
    Plasma::Svg *svg() const { return m_svg; };

    void setIconSize( qreal iconSize ) { m_iconSize = iconSize; updateItemLayouts(); };
    qreal iconSize() const { return m_maxLineCount == 1 ? m_iconSize * m_zoomFactor * 0.75
            : m_iconSize * m_zoomFactor; };

    void setZoomFactor( qreal zoomFactor = 1.0 );
    qreal zoomFactor() const { return m_zoomFactor; };

    Options options() const { return m_options; };
    inline bool isOptionEnabled( Option option ) const { return m_options.testFlag(option); };
    void setOptions( Options options );
    void setOption( Option option, bool enable = true );

    /** @brief Sets the text shown when this item contains no item (ie. is empty) to @p noItemsText. */
    void setNoItemsText( const QString &noItemsText ) {
        m_noItemsText = noItemsText;
        update();
    };

    /** @brief Gets the text shown when this item contains no item, ie. is empty. */
    QString noItemsText() const { return m_noItemsText; };

    void setMaxLineCount( int maxLineCount ) { m_maxLineCount = maxLineCount; updateItemGeometries(); };
    int maxLineCount() const { return m_maxLineCount; };

    /** @brief Call this eg. when the DepartureArrivalListType changes in the model
     * (only header data gets changed...). */
    void updateItemLayouts();

signals:
    void contextMenuRequested( PublicTransportGraphicsItem *item, const QPointF &pos );
    void expandedStateChanged( PublicTransportGraphicsItem *item, bool expanded );

    /**
     * @brief Emitted, if a stop action was triggered from a route stop's context menu.
     *
     * @param stopAction The action to execute.
     * @param stopName The stop name for which the action should be executed.
     **/
    void requestStopAction( StopAction::Type stopAction, const QString &stopName,
                            const QString &stopNameShortened );

    void requestAlarmCreation( const QDateTime &departure, const QString &lineString,
                               VehicleType vehicleType, const QString &target,
                               QGraphicsWidget *item );
    void requestAlarmDeletion( const QDateTime &departure, const QString &lineString,
                               VehicleType vehicleType, const QString &target,
                               QGraphicsWidget *item );

protected slots:
    void itemsAboutToBeRemoved( const QList<ItemBase*> &journeys );
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

    Options m_options;
    PublicTransportModel *m_model;
    QList<PublicTransportGraphicsItem*> m_items;
    Plasma::Svg *m_svg;
    qreal m_iconSize;
    qreal m_zoomFactor;
    int m_maxLineCount;
    QString m_noItemsText;
    bool m_enableOpenStreetMap; // Enable actions using the openstreetmap data engine
    StopAction *m_copyStopToClipboardAction;
    StopAction *m_showInMapAction;
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
    TimetableWidget( Options options = DefaultOptions, QGraphicsItem* parent = 0 );

    void setTargetHidden( bool targetHidden ) { m_targetHidden = targetHidden; updateItemLayouts(); };
    bool isTargetHidden() const { return m_targetHidden; };

    /** @brief Gets the departure item at the given @p row. */
    inline DepartureGraphicsItem *departureItem( int row ) {
        return qobject_cast<DepartureGraphicsItem*>(item(row));
    };

    /** @brief Gets the departure item with the given @p index. */
    inline DepartureGraphicsItem *departureItem( const QModelIndex &index ) {
        return qobject_cast<DepartureGraphicsItem*>(item(index));
    };

    /** @brief Gets the departure model containing the data for this widget. */
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
    KPixmapCache *m_pixmapCache;
};

/**
 * @brief A Plasma::ScrollWidget containing JourneyGraphicsItems.
 **/
class JourneyTimetableWidget : public PublicTransportWidget
{
    Q_OBJECT
    friend class PublicTransportGraphicsItem;

public:
    JourneyTimetableWidget( Options options = DefaultOptions, QGraphicsItem* parent = 0 );

    /** @brief Gets the journey item at the given @p row. */
    inline JourneyGraphicsItem *journeyItem( int row ) {
        return qobject_cast<JourneyGraphicsItem*>(item(row));
    };

    /** @brief Gets the journey item with the given @p index. */
    inline JourneyGraphicsItem *journeyItem( const QModelIndex &index ) {
        return qobject_cast<JourneyGraphicsItem*>(item(index));
    };

    /** @brief Gets the journey model containing the data for this widget. */
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
