/*
*   Copyright 2010 Friedrich Pülz <fpuelz@gmx.de>
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
#include "departureinfo.h"
#include "settings.h" // TODO Only used for AlarmSettings. Removed here?

/** The position of the decoration. */
enum DecorationPosition {
	DecorationLeft, /**< Show the decoration on the left side. */
	DecorationRight /**< Show the decoration on the right side. */
};

/** Holds information about settings from the applet. */
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
};

/** Child item types. */
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

/** Columns for the departure/journey model. */
enum Columns {
	ColumnLineString = 0, /**< The column showing the line string. */
	ColumnTarget = 1, /**< The column showing the target/origin. */
	ColumnDeparture = 2, /**< The column showing the departure/arrival time. */

	ColumnJourneyInfo = ColumnTarget, /**< The column showing information about a journey. */
	ColumnArrival = 3 /**< The column showing the arrival time of a journey. */
};

class ChildItem;
class PublicTransportModel;
/** Base class for items of PublicTransportModel. */
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

	/** @returns the first child item with the given @p itemType. */
	ChildItem *childByType( ItemType itemType ) const;
	/** @returns the child item in the given @p row.. */
	ChildItem *child( int row ) const { return m_children[row]; };
	/** @returns a list of all child items. */
	QList< ChildItem* > children() const { return m_children; };
	/** @returns the number of child items. */
	int childCount() const { return m_children.count(); };

	/** Removes the given @p child item. */
	void removeChild( ChildItem *child );
	/** Appends the given @p child item. */
	void appendChild( ChildItem *child );

	/** @returns the QModelIndex of this item. */
	QModelIndex index();
	/** @returns the row of this item. */
	virtual int row() const = 0;
	/** @returns the data of this item in the given @p role at the given @p column. */
	virtual QVariant data( int role = Qt::DisplayRole, int column = 0 ) const = 0;

	/** @returns a pointer to the model of this item if any. */
	PublicTransportModel *model() const { return m_model; };
	/** Sets the model of this item to @p model. */
	void setModel( PublicTransportModel *model );

	/** Whether or not this item has a pending alarm.
	* The default implementation always returns false. */
	virtual bool hasPendingAlarm() const { return false; };
	/** Updates remaining time values in the departure/arrival column.
	* The default implementation does nothing. */
	virtual void updateTimeValues() {};

protected:
	/** Removes @p count child items beginning with the child item at row @p first. */
	void removeChildren( int first, int count );

	ItemBase *m_parent;
	PublicTransportModel *m_model;
	QList< ChildItem* > m_children;
	const Info *m_info;
};

/** A child item in PublicTransportModel. */
class ChildItem : public ItemBase {
public:
	ChildItem( ItemType itemType, const QString &formattedText, const QIcon &icon, const Info *info );
	ChildItem( ItemType itemType, const QString &formattedText, const Info *info );
	ChildItem( ItemType itemType, const Info *info );

	/** The row of this child item in it's parent. */
	virtual int row() const {
		return m_parent ? m_parent->children().indexOf( const_cast< ChildItem* >(this) ) : -1; };

	/** The type of this child item. */
	ItemType type() const { return m_type; };

	virtual QVariant data( int role = Qt::DisplayRole, int = 0 ) const;
	void setData( const QVariant &data, int role = Qt::UserRole );

	/** Gets the text of this child item. */
	inline QString text() const {
		return m_data.value( Qt::DisplayRole ).toString(); };
	/** Sets the text of this child item to @p text.
	* The @ref HtmlDelegate ignores this unformatted text if a formatted
	* text is set.
	* @see setFormattedText */
	inline void setText( const QString &text ) {
		setData( text, Qt::DisplayRole ); };

	/** Gets the formatted text of this child item. */
	inline QString formattedText() const {
		return m_data.value( FormattedTextRole ).toString(); };
	/** Sets the formatted text of this child item to @p text.
	* @p text can contain HTML tags. The @ref HtmlDelegate ignores the
	* unformatted text (stored in Qt::DisplayRole) if a formatted text is set.
	* @see setText */
	inline void setFormattedText( const QString &text ) {
		setData( text, FormattedTextRole ); };

	/** Gets the icon of this child item. */
	virtual inline QIcon icon() const {
		return m_data.value( Qt::DecorationRole ).value< QIcon >(); };
	/** Sets the icon of this child item to @p icon. */
	inline void setIcon( const QIcon &icon ) {
		setData( icon, Qt::DecorationRole ); };

protected:
	QHash< int, QVariant > m_data;
	ItemType m_type;
};

/** Base class for top level items in PublicTransportModel. */
class TopLevelItem : public ItemBase {
public:
	TopLevelItem( const Info *info );

	virtual void setData( Columns column, const QVariant &data, int role = Qt::UserRole );

	/** Gets the text of the given @p column. */
	inline QString text( Columns column = ColumnLineString ) const {
		return m_columnData.value( column )[ Qt::DisplayRole ].toString(); };
	/** Sets the text of the given @p column to @p text.
	* The @ref HtmlDelegate ignores this unformatted text if a formatted
	* text is set.
	* @see setFormattedText */
	inline void setText( Columns column, const QString &text ) {
		setData( column, text, Qt::DisplayRole ); };

	/** Gets the formatted text of the given @p column. */
	inline QString formattedText( Columns column = ColumnLineString ) const {
		return m_columnData.value( column )[ FormattedTextRole ].toString(); };
	/** Sets the formatted text of the given @p column to @p text.
	* @p text can contain HTML tags. The @ref HtmlDelegate ignores the
	* unformatted text (stored in Qt::DisplayRole) if a formatted text is set.
	* @see setText */
	inline void setFormattedText( Columns column, const QString &text ) {
	    setData( column, text, FormattedTextRole ); };

	/** Gets the icon of the given @p column. */
	inline QIcon icon( Columns column = ColumnLineString ) const {
		return m_columnData.value( column )[ Qt::DecorationRole ].value< QIcon >(); };
	/** Sets the icon of the given @p column to @p icon. */
	inline void setIcon( Columns column, const QIcon &icon ) {
	    setData( column, icon, Qt::DecorationRole ); };

protected:
	QHash< int, QHash<int, QVariant> > m_columnData;
};

/** An item which automatically creates/updates child items according to the
* information in @ref journeyInfo. To update this item and it's child items
* call @ref setJourneyInfo. To only update remaining time values, call
* @ref updateTimeValues. */
class JourneyItem : public TopLevelItem {
public:
	JourneyItem( const JourneyInfo &journeyInfo, const Info *info );

	/** The row of this journey item in the model. */
	virtual int row() const;
	/** @returns the information that this item currently shows. */
	const JourneyInfo *journeyInfo() const { return &m_journeyInfo; };
	/** @returns a hash with child items by their type. */
	QHash< ItemType, ChildItem* > typedChildren() const;

	virtual QVariant data( int role = Qt::DisplayRole, int column = 0 ) const;

	/** Updates remaining time values in the departure/arrival column. */
	virtual void updateTimeValues();
	/** Updates this item and all it's child items according to the
	* inforamtion in @p journeyInfo. */
	void setJourneyInfo( const JourneyInfo &journeyInfo );

protected:
	/** Updates this item. */
	void updateValues();
	/** Creates and adds all children for the current data. */
	void createChildren();
	/** Updates all child items, add/removes children if necessary. */
	void updateChildren();
	/** Adds a new child item of the given @p itemType. */
	ChildItem *appendNewChild( ItemType itemType );
	/** Updates the given @p child item which is of type @p itemType. */
	void updateChild( ItemType itemType, ChildItem *child );
	/** @returns true, if there's data to be shown for the given @p itemType. */
	bool hasDataForChildType( ItemType itemType );
	/** @param itemType The child item type to get the display text for.
	* @param linesPerRow The number of lines for the new item is put here,
	* if it's not NULL.
	* @returns the text to be displayed for the given @p itemType. */
	QString childItemText( ItemType itemType, int *linesPerRow = NULL );
	/** Creates a route item with one child item for each route stop. */
	ChildItem *createRouteItem();
	/** A rating value between 0 (best) and 1 (worst). Journeys are rated
	* by their duration and the needed changes relative to the global
	* maximal/minimal duration/changes of the model. */
	qreal rating() const;

	JourneyInfo m_journeyInfo;
};

/** An item which automatically creates/updates child items according to the
* information in @ref departureInfo. To update this item and it's child items
* call @ref setDepartureInfo. To only update remaining time values, call
* @ref updateTimeValues. */
class DepartureItem : public QObject, public TopLevelItem {
	Q_OBJECT
	Q_PROPERTY( qreal alarmColorIntensity READ alarmColorIntensity WRITE setAlarmColorIntensity )

public:
	DepartureItem( const DepartureInfo &departureInfo, const Info *info );

	/** The row of this departure item in the model. */
	virtual int row() const;
	/** @returns the information that this item currently shows. */
	const DepartureInfo *departureInfo() const { return &m_departureInfo; };
	/** @returns a hash with child items by their type. */
	QHash< ItemType, ChildItem* > typedChildren() const;

	virtual QVariant data( int role = Qt::DisplayRole, int column = 0 ) const;

	/** @returns the current alarm states.
	* @see AlarmStates */
	AlarmStates alarmStates() const { return m_alarm; };
	/** Sets the alarm states. */
	void setAlarmStates( AlarmStates alarmStates );
	/** Sets an alarm for this departure. */
	void setAlarm();
	/** Removes a possibly set alarm. */
	void removeAlarm();
	/** Whether or not this departure has an alarm (pending or fired). */
	bool hasAlarm() const { return m_alarm.testFlag(AlarmPending) || m_alarm.testFlag(AlarmFired); };
	/** Whether or not this departure has a pending alarm. */
	virtual bool hasPendingAlarm() const { return m_alarm.testFlag(AlarmPending); };

	/** Gets the date and time at which an alarm for this departure should be fired. */
	QDateTime alarmTime() const {
		return m_departureInfo.predictedDeparture().addSecs(
				-m_info->alarmMinsBeforeDeparture * 60 );
	};
	qreal alarmColorIntensity() const { return m_alarmColorIntensity; };
	void setAlarmColorIntensity( qreal alarmColorIntensity );

	/** Updates remaining time values in the departure column. */
	virtual void updateTimeValues();
	/** Updates this item and all it's child items according to the
	* inforamtion in @p departureInfo. */
	void setDepartureInfo( const DepartureInfo &departureInfo );

protected:
	/** Updates this item. */
	void updateValues();
	/** Creates and adds all children for the current data. */
	void createChildren();
	/** Updates all child items, add/removes children if necessary. */
	void updateChildren();
	/** Adds a new child item of the given @p itemType. */
	ChildItem *appendNewChild( ItemType itemType );
	/** Updates the given @p child item which is of type @p itemType. */
	void updateChild( ItemType itemType, ChildItem *child );
	/** @returns true, if there's data to be shown for the given @p itemType. */
	bool hasDataForChildType( ItemType itemType );
	/** @param itemType The child item type to get the display text for.
	* @param linesPerRow The number of lines for the new item is put here,
	* if it's not NULL.
	* @returns the text to be displayed for the given @p itemType. */
	QString childItemText( ItemType itemType, int *linesPerRow = NULL );
	/** Creates a route item with one child item for each route stop. */
	ChildItem *createRouteItem();

	DepartureInfo m_departureInfo;
	AlarmStates m_alarm;
	qreal m_alarmColorIntensity;
};

/** Base class for DepartureModel and JourneyModel. */
class PublicTransportModel : public QAbstractItemModel {
	Q_OBJECT
	friend class ItemBase;

public:
	PublicTransportModel( QObject *parent = 0 );
	virtual ~PublicTransportModel() { qDeleteAll( m_items ); };

	virtual QModelIndex index( int row, int column,
				   const QModelIndex& parent = QModelIndex() ) const;
	virtual QModelIndex index( ItemBase *item ) const {
	    return createIndex( item->row(), 0, item ); };
	virtual int rowCount( const QModelIndex& parent = QModelIndex() ) const;
	virtual QModelIndex parent( const QModelIndex& child ) const;
	virtual QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const;

	/** @returns a pointer to the toplevel item at the given @p row. */
	ItemBase *item( int row ) const { return m_items.at( row ); };
	ItemBase *itemFromIndex( const QModelIndex &index ) const;
	QModelIndex indexFromItem( ItemBase *item, int column = 0 ) const;
	int rowFromItem( ItemBase *item );

	QList< ItemBase* > items() const { return m_items; };
	virtual bool removeItem( ItemBase *item ) {
	    return removeRows( item->row(), 1 ); };
	/** Removes all items from the model, but doesn't clear header data. */
	virtual void clear();
	/** @returns true, if this model has no items. */
	inline bool isEmpty() const { return rowCount() == 0; };

	Info info() const { return m_info; };
	void setAlarmMinsBeforeDeparture( int alarmMinsBeforeDeparture = 5 ) {
	    m_info.alarmMinsBeforeDeparture = alarmMinsBeforeDeparture; };
	void setLinesPerRow( int linesPerRow );
	void setSizeFactor( float sizeFactor );
	void setDepartureColumnSettings( bool displayTimeBold = true,
		bool showRemainingMinutes = true, bool showDepartureTime = true );

	void itemChanged( ItemBase *item, int columnLeft = 0, int columnRight = 0 );
	void childrenChanged( ItemBase *parentItem );

protected slots:
	void startUpdateTimer();
	/** Called each full minute. */
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
/** A model for departure items. */
class DepartureModel : public PublicTransportModel {
	Q_OBJECT
	friend class ItemBase;

public:
	DepartureModel( QObject *parent = 0 );

	virtual int columnCount( const QModelIndex& parent = QModelIndex() ) const;
	virtual bool removeRows( int row, int count, const QModelIndex& parent = QModelIndex() );
	virtual QVariant headerData( int section, Qt::Orientation orientation,
								 int role = Qt::DisplayRole ) const;
	virtual void sort( int column, Qt::SortOrder order = Qt::AscendingOrder );

	ItemBase *itemFromInfo( const DepartureInfo &info ) const {
		return m_infoToItem.contains(info.hash()) ? m_infoToItem[info.hash()] : NULL; };
	QModelIndex indexFromInfo( const DepartureInfo &info ) const {
		return m_infoToItem.contains(info.hash()) ? m_infoToItem[info.hash()]->index() : QModelIndex(); };

	/** A list of hashes of all departure items. Hashes can be retrieved
	* using qHash or @ref DepartureInfo::hash. */
	QList< uint > itemHashes() const;
	virtual DepartureItem *addItem( const DepartureInfo &departureInfo,
				Columns sortColumn = ColumnDeparture,
				Qt::SortOrder sortOrder = Qt::AscendingOrder );
	/** Updates the given @p departureItem with the given @p newDepartureInfo.
	* This simply calls setDepartureInfo() on @p deoartureItem. */
	virtual void updateItem( DepartureItem *departureItem,
				 const DepartureInfo &newDepartureInfo ) {
		departureItem->setDepartureInfo( newDepartureInfo ); };
	/** Removes all departures from the model, but doesn't clear header data. */
	virtual void clear();

	Info info() const { return m_info; };
	void setDepartureArrivalListType( DepartureArrivalListType departureArrivalListType );
	void setAlarmSettings( const AlarmSettingsList &alarmSettings );

	/** Whether or not there are pending alarms. */
	bool hasAlarms() const { return !m_alarms.isEmpty(); };
	/** The number of pending alarms. */
	int alarmCount() const { return m_alarms.count(); };
	/** The date and time of the next alarm or a null QDateTime if
	* there's no pending alarm. */
	QDateTime nextAlarmTime() const {
		return m_alarms.isEmpty() ? QDateTime() : m_alarms.keys().first(); };
	/** The departure with the next alarm or NULL if there's no pending alarm. */
	DepartureItem *nextAlarmDeparture() const {
		return m_alarms.isEmpty() ? NULL : m_alarms.values().first(); };
	/** A map with all pending alarms. There can be multiple departures for
	* each alarm time. */
	const QMultiMap< QDateTime, DepartureItem* > *alarms() const {
		return &m_alarms; };
	/** Adds an alarm for the given @p item. */
	void addAlarm( DepartureItem *item );
	/** Removes an alarm from the given @p item. */
	void removeAlarm( DepartureItem *item );

signals:
	/** The alarm for @p item has been fired. */
	void alarmFired( DepartureItem *item );
	void updateAlarms( const AlarmSettingsList &newAlarmSettings,
			   const QList< int > &removedAlarms );

protected slots:
	/** Called each full minute. Updates time values, checks for alarms
	* and sorts out old departures */
	virtual void update();
	void alarmItemDestroyed( QObject *item );

private:
	virtual DepartureItem *findNextItem( bool sortedByDepartureAscending = false ) const;
	void fireAlarm( const QDateTime& dateTime, DepartureItem* item );

	QMultiMap< QDateTime, DepartureItem* > m_alarms;
};

/** A model for journey items. */
class JourneyModel : public PublicTransportModel {
	Q_OBJECT
	friend class ItemBase;

public:
	JourneyModel( QObject *parent = 0 );

	virtual int columnCount( const QModelIndex& parent = QModelIndex() ) const;
	virtual bool removeRows( int row, int count, const QModelIndex& parent = QModelIndex() );
	/** Removes all journeys from the model, but doesn't clear header data. */
	virtual void clear();
	virtual QVariant headerData( int section, Qt::Orientation orientation,
				     int role = Qt::DisplayRole ) const;
	virtual void sort( int column, Qt::SortOrder order = Qt::AscendingOrder );

	ItemBase *itemFromInfo( const JourneyInfo &info ) const {
		return m_infoToItem.contains(info.hash()) ? m_infoToItem[info.hash()] : NULL; };
	QModelIndex indexFromInfo( const JourneyInfo &info ) const {
		return m_infoToItem.contains(info.hash()) ? m_infoToItem[info.hash()]->index() : QModelIndex(); };

	/** A list of hashes of all departure items. Hashes can be retrieved
	* using qHash or @ref JourneyInfo::hash. */
	QList< uint > itemHashes() const;
	virtual JourneyItem *addItem( const JourneyInfo &journeyInfo,
			Columns sortColumn = ColumnDeparture, Qt::SortOrder sortOrder = Qt::AscendingOrder );
	virtual void updateItem( JourneyItem *journeyItem, const JourneyInfo &newJourneyInfo ) {
		journeyItem->setJourneyInfo( newJourneyInfo ); };

	Info info() const { return m_info; };
	void setDepartureArrivalListType( DepartureArrivalListType departureArrivalListType );

	int smallestDuration() const { return m_smallestDuration; };
	int biggestDuration() const { return m_biggestDuration; };
	int smallestChanges() const { return m_smallestChanges; };
	int biggestChanges() const { return m_biggestChanges; };

protected slots:
	virtual void update();

private:
	virtual JourneyItem *findNextItem( bool sortedByDepartureAscending = false ) const;

	int m_smallestDuration, m_biggestDuration;
	int m_smallestChanges, m_biggestChanges;
};

#endif // Multiple inclusion guard
