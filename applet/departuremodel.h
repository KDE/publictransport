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

/** @file
* @brief This file contains a models which hold information about public transport journeys/departures/arrivals.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef DEPARTUREMODEL_HEADER
#define DEPARTUREMODEL_HEADER

#include <QAbstractItemModel>
#include "settings.h" // TODO Only used for AlarmSettings. Removed here?
#include <publictransporthelper/departureinfo.h>
#include <QSortFilterProxyModel>

/** @brief Holds information about settings from the applet. */
struct Info {
    Info() {
        departureArrivalListType = DepartureList;
        linesPerRow = 2;
        displayTimeBold = showRemainingMinutes = true;
        showDepartureTime = false;
        sizeFactor = 1.0f;
        alarmMinsBeforeDeparture = 5;
    };

    AlarmSettingsList alarmSettings;
    DepartureArrivalListType departureArrivalListType;
    int linesPerRow, alarmMinsBeforeDeparture;
    bool displayTimeBold, showRemainingMinutes, showDepartureTime;
    float sizeFactor;
    QString homeStop;
};

/**
 * @brief Child item types.
 *
 * @ingroup models */
enum ItemType {
    OtherItem, /**< For children of child items. */
    PlatformItem, /**< The item contains the platform */
    JourneyNewsItem, /**< The item contains the journey news */
    DelayItem, /**< The item contains the delay */
    OperatorItem, /**< The item contains the operator name */
    RouteItem, /**< The item contains a list of stops in the route
        * (to the destination for departures / arrivals) */

    DurationItem, /**< The items contains the duration in minutes of a journey */
    ChangesItem, /**< The item contains the number of changes of a journey */
    PricingItem /**< The item contains the pricing of a journey */
};

/**
 * @brief Columns for the departure/journey model.
 *
 * @ingroup models */
enum Columns {
    ColumnLineString = 0, /**< The column showing the line string. */
    ColumnTarget = 1, /**< The column showing the target/origin. */
    ColumnDeparture = 2, /**< The column showing the departure/arrival time. */

    ColumnJourneyInfo = ColumnTarget, /**< The column showing information about a journey. */
    ColumnArrival = 3 /**< The column showing the arrival time of a journey. */
};

class ChildItem;
class PublicTransportModel;

/**
 * @brief Base class for items of PublicTransportModel.
 *
 * @ingroup models
 **/
class ItemBase {
    friend class PublicTransportModel;
    friend class DepartureModel;
    friend class JourneyModel;

public:
    ItemBase( const Info *info );
    virtual ~ItemBase();

    /** @returns the parent item of this item, or NULL if this item is a
    * toplevel item. */
    ItemBase *parent() const { return m_parent; };

    /** @returns the toplevel parent item of this item, or a pointer to this
    * item if it is a toplevel item itself. Never returns NULL. */
    ItemBase *topLevelParent() const;

    const Info *info() const { return m_info; };

    /** @returns the first child item with the given @p itemType. */
    ChildItem *childByType( ItemType itemType ) const;

    /** @returns the child item in the given @p row.. */
    ChildItem *child( int row ) const { return m_children[row]; };

    /** @returns a list of all child items. */
    QList< ChildItem* > children() const { return m_children; };

    /** @returns the number of child items. */
    int childCount() const { return m_children.count(); };

    /** @brief Removes the given @p child item. */
    void removeChild( ChildItem *child );

    /** @brief Appends the given @p child item. */
    void appendChild( ChildItem *child );

    /** @returns the QModelIndex of this item. */
    QModelIndex index();

    /** @returns the row of this item. */
    virtual int row() const = 0;

    /** @returns the data of this item in the given @p role at the given @p column. */
    virtual QVariant data( int role = Qt::DisplayRole, int column = 0 ) const = 0;

    /** @returns a pointer to the model of this item if any. */
    PublicTransportModel *model() const { return m_model; };

    /** @brief Sets the model of this item to @p model. */
    void setModel( PublicTransportModel *model );

    /**
     * @brief Whether or not this item has a pending alarm.
     *
     * The default implementation always returns false.
     **/
    virtual bool hasPendingAlarm() const { return false; };

    /**
     * @brief Updates remaining time values in the departure/arrival column.
     *
     * The default implementation does nothing.
     **/
    virtual void updateTimeValues() {};

protected:
    /** @brief Removes @p count child items beginning with the child item at row @p first. */
    void removeChildren( int first, int count );

    ItemBase *m_parent;
    PublicTransportModel *m_model;
    QList< ChildItem* > m_children;
    const Info *m_info;
};

/**
 * @brief A child item in PublicTransportModel.
 *
 * @ingroup models
 **/
class ChildItem : public ItemBase {
public:
    ChildItem( ItemType itemType, const QString &formattedText, const QIcon &icon, const Info *info );
    ChildItem( ItemType itemType, const QString &formattedText, const Info *info );
    ChildItem( ItemType itemType, const Info *info );

    /** The row of this child item in it's parent. */
    virtual int row() const {
        return m_parent ? m_parent->children().indexOf( const_cast< ChildItem* >(this) ) : -1; };

    /** @brief The type of this child item. */
    ItemType type() const { return m_type; };

    virtual QVariant data( int role = Qt::DisplayRole, int = 0 ) const;
    void setData( const QVariant &data, int role = Qt::UserRole );

    /** @brief Gets the text of this child item. */
    inline QString text() const {
        return m_data.value( Qt::DisplayRole ).toString(); };

    /**
     * @brief Sets the text of this child item to @p text.
     *
     * Views can ignore this unformatted text if a formatted text is set.
     * @see setFormattedText
     **/
    inline void setText( const QString &text ) {
        setData( text, Qt::DisplayRole ); };

    /** @brief Gets the formatted text of this child item. */
    inline QString formattedText() const {
        return m_data.value( FormattedTextRole ).toString(); };

    /**
     * @brief Sets the formatted text of this child item to @p text.
     *
     * @param text can contain HTML tags. Views can ignores the unformatted text (stored in
     * Qt::DisplayRole) if a formatted text is set.
     *
     * @see setText
     **/
    inline void setFormattedText( const QString &text ) {
        setData( text, FormattedTextRole ); };

    /** @brief Gets the icon of this child item. */
    virtual inline QIcon icon() const {
        return m_data.value( Qt::DecorationRole ).value< QIcon >(); };

    /** @brief Sets the icon of this child item to @p icon. */
    inline void setIcon( const QIcon &icon ) {
        setData( icon, Qt::DecorationRole ); };

protected:
    QHash< int, QVariant > m_data;
    ItemType m_type;
};

/**
 * @brief Base class for top level items in PublicTransportModel.
 *
 * @ingroup models
 **/
class TopLevelItem : public QObject, public ItemBase {
    Q_OBJECT

public:
    TopLevelItem( const Info *info );

    virtual void setData( Columns column, const QVariant &data, int role = Qt::UserRole );

    /** @brief Gets the text of the given @p column. */
    inline QString text( Columns column = ColumnLineString ) const {
        return m_columnData.value( column )[ Qt::DisplayRole ].toString(); };

    /**
     * @brief Sets the text of the given @p column to @p text.
     *
     * Views can ignore this unformatted text if a formatted text is set.
     *
     * @see setFormattedText */
    inline void setText( Columns column, const QString &text ) {
        setData( column, text, Qt::DisplayRole ); };

    /** @brief Gets the formatted text of the given @p column. */
    inline QString formattedText( Columns column = ColumnLineString ) const {
        return m_columnData.value( column )[ FormattedTextRole ].toString(); };

    /**
     * @brief Sets the formatted text of the given @p column to @p text.
     *
     * @param text can contain HTML tags. The @ref HtmlDelegate ignores the
     * unformatted text (stored in Qt::DisplayRole) if a formatted text is set.
     *
     * @see setText */
    inline void setFormattedText( Columns column, const QString &text ) {
        setData( column, text, FormattedTextRole ); };

    /** @brief Gets the icon of the given @p column. */
    inline QIcon icon( Columns column = ColumnLineString ) const {
        return m_columnData.value( column )[ Qt::DecorationRole ].value< QIcon >(); };

    /** @brief Sets the icon of the given @p column to @p icon. */
    inline void setIcon( Columns column, const QIcon &icon ) {
        setData( column, icon, Qt::DecorationRole ); };

protected:
    QHash< int, QHash<int, QVariant> > m_columnData;
};

/**
 * @brief An item which automatically creates/updates child items according to the information in @ref journeyInfo.
 * 
 * To update this item and it's child items call @ref setJourneyInfo.
 * To only update remaining time values, call @ref updateTimeValues.
 *
 * @ingroup models
 **/
class JourneyItem : public TopLevelItem {
    Q_OBJECT

public:
    JourneyItem( const JourneyInfo &journeyInfo, const Info *info );

    /** @brief The row of this journey item in the model. */
    virtual int row() const;

    /** @returns the information that this item currently shows. */
    const JourneyInfo *journeyInfo() const { return &m_journeyInfo; };

    /** @returns a hash with child items by their type. */
    QHash< ItemType, ChildItem* > typedChildren() const;

    virtual QVariant data( int role = Qt::DisplayRole, int column = 0 ) const;

    /** @brief Updates remaining time values in the departure/arrival column. */
    virtual void updateTimeValues();

    /** @brief Updates this item and all it's child items according to the inforamtion
    * in @p journeyInfo. */
    void setJourneyInfo( const JourneyInfo &journeyInfo );

protected:
    /** @brief Updates this item. */
    void updateValues();

    /** @brief Creates and adds all children for the current data. */
    void createChildren();

    /** @brief Updates all child items, add/removes children if necessary. */
    void updateChildren();

    /** @brief Adds a new child item of the given @p itemType. */
    ChildItem *appendNewChild( ItemType itemType );

    /** @brief Updates the given @p child item which is of type @p itemType. */
    void updateChild( ItemType itemType, ChildItem *child );

    /** @returns true, if there's data to be shown for the given @p itemType. */
    bool hasDataForChildType( ItemType itemType );

    /**
     * @param itemType The child item type to get the display text for.
     *
     * @param linesPerRow The number of lines for the new item is put here, if it's not NULL.
     *
     * @returns the text to be displayed for the given @p itemType. */
    QString childItemText( ItemType itemType, int *linesPerRow = NULL );

    /** @brief Creates a route item with one child item for each route stop. */
    ChildItem *createRouteItem();

    /**
     * @brief A rating value between 0 (best) and 1 (worst).
     *
     * Journeys are rated by their duration and the needed changes relative
     * to the global maximal/minimal duration/changes of the model.
     **/
    qreal rating() const;

    JourneyInfo m_journeyInfo;
};

/**
 * @brief An item which automatically creates/updates child items according
 *   to the information in @ref departureInfo.
 *
 * To update this item and it's child items call @ref setDepartureInfo. To only update remaining
 * time values, call @ref updateTimeValues.
 *
 * @ingroup models */
class DepartureItem : public TopLevelItem {
    Q_OBJECT
    Q_PROPERTY( qreal alarmColorIntensity READ alarmColorIntensity WRITE setAlarmColorIntensity )

public:
    DepartureItem( const DepartureInfo &departureInfo, const Info *info );

    /** @brief The row of this departure item in the model. */
    virtual int row() const;

    /** @returns the information that this item currently shows. */
    const DepartureInfo *departureInfo() const { return &m_departureInfo; };

    /** @returns a hash with child items by their type. */
    QHash< ItemType, ChildItem* > typedChildren() const;

    virtual QVariant data( int role = Qt::DisplayRole, int column = 0 ) const;

    bool isLeavingSoon() const { return m_leavingSoon; };
    void setLeavingSoon( bool leavingSoon = true );

    /**
     * @returns the current alarm states.
     * @see AlarmStates */
    AlarmStates alarmStates() const { return m_alarm; };

    /** @brief Sets the alarm states. */
    void setAlarmStates( AlarmStates alarmStates );

    /** @brief Sets an alarm for this departure. */
    void setAlarm();

    /** @brief Removes a possibly set alarm. */
    void removeAlarm();

    /** @brief Whether or not this departure has an alarm (pending or fired). */
    bool hasAlarm() const { return m_alarm.testFlag(AlarmPending) || m_alarm.testFlag(AlarmFired); };

    /** @brief Whether or not this departure has a pending alarm. */
    virtual bool hasPendingAlarm() const { return m_alarm.testFlag(AlarmPending); };

    /** @brief Gets the date and time at which an alarm for this departure should be fired. */
    QDateTime alarmTime() const {
        return m_departureInfo.predictedDeparture().addSecs(
                -m_info->alarmMinsBeforeDeparture * 60 );
    };

    qreal alarmColorIntensity() const { return m_alarmColorIntensity; };
    void setAlarmColorIntensity( qreal alarmColorIntensity );

    /** @brief Updates remaining time values in the departure column. */
    virtual void updateTimeValues();

    /** @brief Updates this item and all it's child items according to the
     * inforamtion in @p departureInfo. */
    void setDepartureInfo( const DepartureInfo &departureInfo );

protected:
    /** @brief Updates this item. */
    void updateValues();

    /** @brief Creates and adds all children for the current data. */
    void createChildren();

    /** @brief Updates all child items, add/removes children if necessary. */
    void updateChildren();

    /** @brief Adds a new child item of the given @p itemType. */
    ChildItem *appendNewChild( ItemType itemType );

    /** @brief Updates the given @p child item which is of type @p itemType. */
    void updateChild( ItemType itemType, ChildItem *child );

    /** @returns true, if there's data to be shown for the given @p itemType. */
    bool hasDataForChildType( ItemType itemType );

    /**
     * @param itemType The child item type to get the display text for.
     *
     * @param linesPerRow The number of lines for the new item is put here, if it's not NULL.
     *
     * @returns the text to be displayed for the given @p itemType.
     **/
    QString childItemText( ItemType itemType, int *linesPerRow = NULL );

    /** @brief Creates a route item with one child item for each route stop. */
    ChildItem *createRouteItem();

    DepartureInfo m_departureInfo;
    AlarmStates m_alarm;
    qreal m_alarmColorIntensity;
    bool m_leavingSoon;
};

/**
 * @brief Base class for DepartureModel and JourneyModel.
 *
 * @ingroup models */
class PublicTransportModel : public QAbstractItemModel {
    Q_OBJECT
    friend class ItemBase;

public:
    PublicTransportModel( QObject *parent = 0 );
    virtual ~PublicTransportModel() { qDeleteAll( m_items ); };

    /**
     * @brief Gets the QModelIndex of the item at @p row and @p column.
     *
     * @param row The row of the index to get.
     *
     * @param column The column of the index to get.
     *
     * @param parent The parent index of the index to be returned or an invalid QModelIndex, to get
     *   the index of a toplevel item. Defaults to QModelIndex().
     *
     * @return The QModelIndex of the item at @p row and @p column.
     **/
    virtual QModelIndex index( int row, int column, const QModelIndex& parent = QModelIndex() ) const;

    /**
     * @brief Gets the QModelIndex of the given @p item.
     *
     * @param item The item to get the index for.
     * 
     * @return The QModelIndex for the given @p item.
     **/
    virtual QModelIndex index( ItemBase *item ) const {
        return createIndex( item->row(), 0, item ); };

    /**
     * @brief Gets the number of rows for the given @p parent in this model.
     *
     * @param parent The parent, which row count should be returned.
     *   Defaults to QModelIndex(), ie. the toplevel row count.
     *
     * @return The number of rows for the given @p parent in this model.
     **/
    virtual int rowCount( const QModelIndex& parent = QModelIndex() ) const;

    /**
     * @brief Gets the index of the parent item for the given @p child index.
     *
     * @param child The index of a child item of the parent to be returned
     *
     * @return The QModelIndex of the parent of the given @p child.
     **/
    virtual QModelIndex parent( const QModelIndex& child ) const;

    virtual QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const;

    /**
     * @brief Gets the toplevel item at @p row.
     *
     * @returns a pointer to the toplevel item at the given @p row.
     **/
    ItemBase *item( int row ) const { return m_items.at(row); };

    ItemBase *itemFromIndex( const QModelIndex &index ) const;

    QModelIndex indexFromItem( ItemBase *item, int column = 0 ) const;

    /**
     * @brief Gets the row of the given toplevel item.
     *
     * @param item The item which row should be returned.
     *
     * @return The row of the given @p item or -1, if it isn't a toplevel item of this model.
     **/
    int rowFromItem( ItemBase *item );

    /**
     * @brief Gets a list of all toplevel items of this model.
     *
     * @return The list of toplevel items.
     **/
    QList< ItemBase* > items() const { return m_items; };

    /**
     * @brief Removes the given @p item from the model.
     *
     * @param item The item to be removed.
     * 
     * @return True, if the item was successfully removed. False, otherwise.
     *   Removing may fail, eg. if it isn't inside the model.
     **/
    virtual bool removeItem( ItemBase *item ) {
        return removeRows( item->row(), 1 ); };

    /** @brief Removes all items from the model, but doesn't clear header data. */
    virtual void clear();

    /**
     * @brief Checks whether or not this model is empty.
     * @returns True, if this model has no items, ie. is empty. False, otherwise.
     **/
    inline bool isEmpty() const { return rowCount() == 0; };

    Info info() const { return m_info; };

    void setAlarmMinsBeforeDeparture( int alarmMinsBeforeDeparture = 5 ) {
        m_info.alarmMinsBeforeDeparture = alarmMinsBeforeDeparture; };

    void setLinesPerRow( int linesPerRow );

    void setSizeFactor( float sizeFactor );

    void setDepartureColumnSettings( bool displayTimeBold = true,
        bool showRemainingMinutes = true, bool showDepartureTime = true );

    void setHomeStop( const QString &homeStop ) {
        m_info.homeStop = homeStop;
    };

    /**
     * @brief Notifies the model about changes in the given @p item.
     *
     * @param item The item which data has changed.
     *
     * @param columnLeft The first changed column. Defaults to 0.
     * 
     * @param columnRight The last changed column Defaults to 0.
     **/
    void itemChanged( ItemBase *item, int columnLeft = 0, int columnRight = 0 );

    /**
     * @brief Notifies the model about changes in children of the given @p parentItem.
     *
     * @param parentItem The item, which children were changed.
     **/
    void childrenChanged( ItemBase *parentItem );

signals:
    /**
     * @brief The @p items will get removed after this signal was emitted.
     *
     * @param items Can be a list of DepartureItems or JourneyItems, that will
     *   get removed after this signal was emitted.
     **/
    void journeysAboutToBeRemoved( const QList<ItemBase*> &items );

protected slots:
    void startUpdateTimer();

    /** @brief Called each full minute. */
    virtual void update() = 0;

protected:
    void callAtNextFullMinute( const char *member );
    virtual ItemBase *findNextItem( bool sortedByTimeAscending = false ) const = 0;

    QList< ItemBase* > m_items;
    QHash< uint, ItemBase* > m_infoToItem;
    ItemBase *m_nextItem;

    Info m_info;
    QTimer *m_updateTimer;
};

class QTimer;

/**
 * @brief A model for departure items.
 *
 * @ingroup models
 **/
class DepartureModel : public PublicTransportModel {
    Q_OBJECT
    friend class ItemBase;

public:
    DepartureModel( QObject *parent = 0 );

    /**
     * @brief Gets the number of columns for the given @p parent in this model.
     *
     * @param parent The parent, which column count should be returned. Defaults to QModelIndex(),
     *   ie. the toplevel column count.
     * 
     * @return The number of columns for the given @p parent in this model.
     **/
    virtual int columnCount( const QModelIndex& parent = QModelIndex() ) const;

    /**
     * @brief Removes @p count items beginning at @p row from @p parent.
     *
     * @param row The row of the first item to be removed.
     * 
     * @param count The number of items to be removed, beginning at @p row.
     * 
     * @param parent The QModelIndex parent which child items should be removed or an invalid
     *   model index to remove toplevel items. Defaults to QModelIndex().
     * 
     * @return True, if the items were successfully removed from the model. False, otherwise.
     **/
    virtual bool removeRows( int row, int count, const QModelIndex& parent = QModelIndex() );
    virtual QVariant headerData( int section, Qt::Orientation orientation,
                                int role = Qt::DisplayRole ) const;
    virtual void sort( int column, Qt::SortOrder order = Qt::AscendingOrder );

    ItemBase *itemFromInfo( const DepartureInfo &info ) const {
        return m_infoToItem.contains(info.hash()) ? m_infoToItem[info.hash()] : NULL; };
    QModelIndex indexFromInfo( const DepartureInfo &info ) const {
        return m_infoToItem.contains(info.hash()) ? m_infoToItem[info.hash()]->index() : QModelIndex(); };

    /**
     * @brief A list of hashes of all departure items.
     *
     * Hashes can be retrieved using qHash or @ref DepartureInfo::hash.
     **/
    QList< uint > itemHashes() const;

    QList< DepartureInfo > departureInfos() const;
    
    virtual DepartureItem *addItem( const DepartureInfo &departureInfo,
                Columns sortColumn = ColumnDeparture,
                Qt::SortOrder sortOrder = Qt::AscendingOrder );
    /**
     * @brief Updates the given @p departureItem with the given @p newDepartureInfo.
     *
     * This simply calls setDepartureInfo() on @p deoartureItem.
     **/
    virtual void updateItem( DepartureItem *departureItem, const DepartureInfo &newDepartureInfo ) {
        departureItem->setDepartureInfo( newDepartureInfo ); };

    /** @brief Removes all departures from the model, but doesn't clear header data. */
    virtual void clear();

    Info info() const { return m_info; };
    void setDepartureArrivalListType( DepartureArrivalListType departureArrivalListType );
    void setAlarmSettings( const AlarmSettingsList &alarmSettings );

    /** @brief Whether or not there are pending alarms. */
    bool hasAlarms() const { return !m_alarms.isEmpty(); };

    /** @brief The number of pending alarms. */
    int alarmCount() const { return m_alarms.count(); };

    /** @brief The date and time of the next alarm or a null QDateTime if there's no pending alarm. */
    QDateTime nextAlarmTime() const {
        return m_alarms.isEmpty() ? QDateTime() : m_alarms.keys().first(); };

    /** @brief The departure with the next alarm or NULL if there's no pending alarm. */
    DepartureItem *nextAlarmDeparture() const {
        return m_alarms.isEmpty() ? NULL : m_alarms.values().first(); };

    /** @brief A map with all pending alarms. There can be multiple departures for each alarm time. */
    const QMultiMap< QDateTime, DepartureItem* > *alarms() const {
        return &m_alarms; };

    /** @brief Adds an alarm for the given @p item. */
    void addAlarm( DepartureItem *item );

    /** @brief Removes an alarm from the given @p item. */
    void removeAlarm( DepartureItem *item );
    
    /**
     * @brief Gets the flags for route stop items for the given @p stopName.
     *
     * @param stopName The stop name which is associated with the route stop
     *   items to get flags for.
     * @returns The flags for route stop items, which are associated with the
     *   given @p stopName.
     **/
    RouteItemFlags routeItemFlags( const QString &stopName ) const;

    /**
     * @brief Sets the route stop item to be highlighted.
     *
     * @param stopName The stop name associated with the route stop item
     *   to highlight. If this is empty (default), the currently highlighted
     *   stop gets unhighlighted.
     **/
    void setHighlightedStop( const QString &stopName = QString() );

    /** @brief Returns the currently highlighted stop or an empty string, 
      * if no stop is currently highlighted. */
    QString highlightedStop() const { return m_highlightedStopName; };

    void setColorGroups( const ColorGroupSettingsList &colorGroups );
    ColorGroupSettingsList colorGroups() const { return m_colorGroups; };

signals:
    /** @brief The alarm for @p item has been fired. */
    void alarmFired( DepartureItem *item );

    void updateAlarms( const AlarmSettingsList &newAlarmSettings, const QList<int> &removedAlarms );

    /**
    * @brief The @p departures have just left.
    *
    * The DepartureItems for the given @p departures were deleted before emitting this signal.
    **/
    void departuresLeft( const QList<DepartureInfo> &departures );

protected slots:
    /**
     * @brief Updates time values, checks for alarms and sorts out old departures.
     * 
     * Called each full minute. */
    virtual void update();

    void removeLeavingDepartures();

    void alarmItemDestroyed( QObject *item );

private:
    virtual DepartureItem *findNextItem( bool sortedByDepartureAscending = false ) const;
    void fireAlarm( const QDateTime& dateTime, DepartureItem* item );

    QMultiMap< QDateTime, DepartureItem* > m_alarms;
    QString m_highlightedStopName;
    ColorGroupSettingsList m_colorGroups; // A list of color groups for the current stop
};

/**
 * @brief A model for journey items.
 * @ingroup models */
class JourneyModel : public PublicTransportModel {
    Q_OBJECT
    friend class ItemBase;

public:
    JourneyModel( QObject *parent = 0 );

    virtual int columnCount( const QModelIndex& parent = QModelIndex() ) const;
    virtual bool removeRows( int row, int count, const QModelIndex& parent = QModelIndex() );

    /** @brief Removes all journeys from the model, but doesn't clear header data. */
    virtual void clear();

    virtual QVariant headerData( int section, Qt::Orientation orientation,
                    int role = Qt::DisplayRole ) const;
    virtual void sort( int column, Qt::SortOrder order = Qt::AscendingOrder );

    ItemBase *itemFromInfo( const JourneyInfo &info ) const {
        return m_infoToItem.contains(info.hash()) ? m_infoToItem[info.hash()] : NULL; };
    QModelIndex indexFromInfo( const JourneyInfo &info ) const {
        return m_infoToItem.contains(info.hash()) ? m_infoToItem[info.hash()]->index() : QModelIndex(); };

    /**
     * @brief A list of hashes of all departure items.
     *
     * Hashes can be retrieved using qHash or @ref JourneyInfo::hash. */
    QList< uint > itemHashes() const;

    virtual JourneyItem *addItem( const JourneyInfo &journeyInfo,
            Columns sortColumn = ColumnDeparture, Qt::SortOrder sortOrder = Qt::AscendingOrder );
    virtual void updateItem( JourneyItem *journeyItem, const JourneyInfo &newJourneyInfo ) {
        journeyItem->setJourneyInfo( newJourneyInfo ); };

    Info info() const { return m_info; };
    void setDepartureArrivalListType( DepartureArrivalListType departureArrivalListType );

    /**
     * @brief Gets the smallest duration in minutes of a journey in this model.
     *
     * @return The smallest duration in minutes of a journey in this model.
     **/
    int smallestDuration() const { return m_smallestDuration; };

    /** @brief Gets the biggest duration in minutes of a journey in this model.
     * @return The biggest duration in minutes of a journey in this model.
     **/
    int biggestDuration() const { return m_biggestDuration; };

    /** @brief Gets the smallest number of changes of a journey in this model.
     * @return The smallest number of changes of a journey in this model.
     **/
    int smallestChanges() const { return m_smallestChanges; };

    /** @brief Gets the biggest number of changes of a journey in this model.
     * @return The biggest number of changes of a journey in this model.
     **/
    int biggestChanges() const { return m_biggestChanges; };

protected slots:
    virtual void update();

private:
    virtual JourneyItem *findNextItem( bool sortedByDepartureAscending = false ) const;

    int m_smallestDuration, m_biggestDuration;
    int m_smallestChanges, m_biggestChanges;
};

#endif // Multiple inclusion guard
