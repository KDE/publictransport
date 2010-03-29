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

#ifndef DEPARTUREMODEL_HEADER
#define DEPARTUREMODEL_HEADER

#include <QAbstractItemModel>
#include "departureinfo.h"
#include "settings.h" // For AlarmSettings. Should be removed here.

enum AdditionalRoles {
    FormattedTextRole = Qt::UserRole + 500, /**< Used to store formatted text. 
	* The text of an item should not contain html tags, if used in a combo box. */
    DecorationPositionRole = Qt::UserRole + 501,
    DrawAlarmBackground = Qt::UserRole + 502,
    JourneyRatingRole = Qt::UserRole + 503, /**< Stores a value between 0 and 1. 
	* 0 for the journey with the biggest duration, 1 for the smallest duration. */
    LinesPerRowRole = Qt::UserRole + 504, /**< Used to change the number of lines for a row. */
    IconSizeRole = Qt::UserRole + 505 /**< Used to set a specific icon size for an element. */
};

enum DecorationPosition {
    DecorationLeft,
    DecorationRight
};

struct Infos {
    Infos() {
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

enum ItemType {
    OtherItem, /**< For childs of child items. */
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

enum Columns {
    ColumnLineString = 0,
    ColumnTarget = 1,
    ColumnDeparture = 2,

    ColumnJourneyInfo = ColumnTarget,
    ColumnArrival = 3
};

class ChildItem;
class PublicTransportModel;
class ItemBase {
    public:
	ItemBase( const Infos *infos );
	virtual ~ItemBase();
	
	ItemBase *parent() const { return m_parent; };
	
	ItemBase *topLevelParent() const;
	ChildItem *childByType( ItemType itemType ) const;
	ChildItem *child( int row ) const { return m_children[row]; };
	QList< ChildItem* > children() const { return m_children; };
	
	int childCount() const { return m_children.count(); };
	QModelIndex index();
	virtual int row() const = 0;
	virtual QVariant data( int role = Qt::DisplayRole, int column = 0 ) const = 0;
	PublicTransportModel *model() const { return m_model; };
	void setModel( PublicTransportModel *model );

	void removeChildren( int first, int count );
	void removeChild( ChildItem *child ) {
	    removeChildren( m_children.indexOf(child), 1 ); };
	void appendChild( ChildItem *child );

	virtual bool hasPendingAlarm() const { return false; };
	virtual void updateTimeValues() {};

    protected:
	ItemBase *m_parent;
	PublicTransportModel *m_model;
	QList< ChildItem* > m_children;
	const Infos *m_infos;
};

class ChildItem : public ItemBase {
    public:
	ChildItem( ItemType itemType, const QString &formattedText,
		   const QIcon &icon, const Infos *infos );
	ChildItem( ItemType itemType, const QString &formattedText,
		   const Infos *infos );
	ChildItem( ItemType itemType, const Infos *infos );

	virtual int row() const {
	    return m_parent ? m_parent->children().indexOf( const_cast< ChildItem* >(this) ) : -1; };

	ItemType type() const { return m_type; };

	virtual QVariant data( int role = Qt::DisplayRole, int = 0 ) const;
	void setData( const QVariant &data, int role = Qt::UserRole );

	inline QString text() const {
	    return m_data.value( Qt::DisplayRole ).toString(); };
	inline void setText( const QString &text ) {
	    setData( text, Qt::DisplayRole ); };

	inline QString formattedText() const {
	    return m_data.value( FormattedTextRole ).toString(); };
	inline void setFormattedText( const QString &text ) {
	    setData( text, FormattedTextRole ); };

	inline QIcon icon() const {
	    return m_data.value( Qt::DecorationRole ).value< QIcon >(); };
	inline void setIcon( const QIcon &icon ) {
	    setData( icon, Qt::DecorationRole ); };

    protected:
	QHash< int, QVariant > m_data;
	ItemType m_type;
};

class JourneyItem : public ItemBase {
    public:
	JourneyItem( const JourneyInfo &journeyInfo, const Infos *infos );
	
	virtual int row() const;
	const JourneyInfo *journeyInfo() const { return &m_journeyInfo; };
	QHash< ItemType, ChildItem* > typedChildren() const;
	
	virtual QVariant data( int role = Qt::DisplayRole, int column = 0 ) const;
	void setData( Columns column, const QVariant &data, int role = Qt::UserRole );

	inline QString text( Columns column = ColumnLineString ) const {
	    return m_columnData.value( column )[ Qt::DisplayRole ].toString(); };
	inline void setText( Columns column, const QString &text ) {
	    setData( column, text, Qt::DisplayRole ); };
	
	inline QString formattedText( Columns column = ColumnLineString ) const {
	    return m_columnData.value( column )[ FormattedTextRole ].toString(); };
	inline void setFormattedText( Columns column, const QString &text ) {
	    setData( column, text, FormattedTextRole ); };

	inline QIcon icon( Columns column = ColumnLineString ) const {
	    return m_columnData.value( column )[ Qt::DecorationRole ].value< QIcon >(); };
	inline void setIcon( Columns column, const QIcon &icon ) {
	    setData( column, icon, Qt::DecorationRole ); };
	    
	virtual void updateTimeValues();
	void setJourneyInfo( const JourneyInfo &journeyInfo );
	
    protected:
	void updateValues();
	void createChildren();
	void updateChildren();
	ChildItem *appendNewChild( ItemType itemType );
	void updateChild( ItemType itemType, ChildItem *child );
	bool hasDataForChildType( ItemType itemType );
	QString childItemText( ItemType itemType, int *linesPerRow = NULL );
	ChildItem *createRouteItem();
	qreal rating() const;
	
	JourneyInfo m_journeyInfo;
	QHash< int, QHash<int, QVariant> > m_columnData;
	
};

class DepartureItem : public ItemBase {
    public:
	DepartureItem( const DepartureInfo &departureInfo, const Infos *infos );
	
	virtual int row() const;
	const DepartureInfo *departureInfo() const { return &m_departureInfo; };
	QHash< ItemType, ChildItem* > typedChildren() const;
	
	virtual QVariant data( int role = Qt::DisplayRole, int column = 0 ) const;
	void setData( Columns column, const QVariant &data, int role = Qt::UserRole );

	inline QString text( Columns column = ColumnLineString ) const {
	    return m_columnData.value( column )[ Qt::DisplayRole ].toString(); };
	inline void setText( Columns column, const QString &text ) {
	    setData( column, text, Qt::DisplayRole ); };
	
	inline QString formattedText( Columns column = ColumnLineString ) const {
	    return m_columnData.value( column )[ FormattedTextRole ].toString(); };
	inline void setFormattedText( Columns column, const QString &text ) {
	    setData( column, text, FormattedTextRole ); };
	
	inline QIcon icon( Columns column = ColumnLineString ) const {
	    return m_columnData.value( column )[ Qt::DecorationRole ].value< QIcon >(); };
	inline void setIcon( Columns column, const QIcon &icon ) {
	    setData( column, icon, Qt::DecorationRole ); };

	AlarmStates alarmStates() const { return m_alarm; };
	bool hasAlarm() const { return m_alarm.testFlag(AlarmPending) || m_alarm.testFlag(AlarmFired); };
	void setAlarm();
	void removeAlarm();
	virtual bool hasPendingAlarm() const { return m_alarm.testFlag(AlarmPending); };
	
	QDateTime alarmTime() const {
	    return m_departureInfo.predictedDeparture().addSecs(
		    -m_infos->alarmMinsBeforeDeparture * 60 );
	};

	virtual void updateTimeValues();
	void setAlarmStates( AlarmStates alarmStates );
	void setDepartureInfo( const DepartureInfo &departureInfo );
	
    protected:
	void updateValues();
	void createChildren();
	void updateChildren();
	ChildItem *appendNewChild( ItemType itemType );
	void updateChild( ItemType itemType, ChildItem *child );
	bool hasDataForChildType( ItemType itemType );
	QString childItemText( ItemType itemType, int *linesPerRow = NULL );
	ChildItem *createRouteItem();

	DepartureInfo m_departureInfo;
	QHash< int, QHash<int, QVariant> > m_columnData;

	AlarmStates m_alarm;
};

class PublicTransportModel : public QAbstractItemModel {
    Q_OBJECT
    public:
	PublicTransportModel( QObject *parent = 0 );
	virtual ~PublicTransportModel() {
	    qDeleteAll( m_items ); };

	virtual QModelIndex index( int row, int column,
				   const QModelIndex& parent = QModelIndex() ) const;
	virtual QModelIndex index( ItemBase *item ) const {
	    return createIndex( item->row(), 0, item ); };
	virtual int rowCount( const QModelIndex& parent = QModelIndex() ) const;
	virtual QModelIndex parent( const QModelIndex& child ) const;
	virtual QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const;

	ItemBase *item( int row ) const { return m_items.at( row ); };
	ItemBase *itemFromIndex( const QModelIndex &index ) const;
	QModelIndex indexFromItem( ItemBase *item, int column = 0 ) const;
	int rowFromItem( ItemBase *item );

	QList< ItemBase* > items() const { return m_items; };
	virtual bool removeItem( ItemBase *item ) {
	    return removeRows( item->row(), 1 ); };
	/** Removes all items from the model, but doesn't clear header data. */
	virtual void clear();
	inline bool isEmpty() const { return rowCount() == 0; };

	Infos infos() const { return m_infos; };
	void setAlarmMinsBeforeDeparture( int alarmMinsBeforeDeparture = 5 ) {
	    m_infos.alarmMinsBeforeDeparture = alarmMinsBeforeDeparture; };
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

	Infos m_infos;
	QTimer *m_updateTimer;
};

class QTimer;
class DepartureModel : public PublicTransportModel {
    Q_OBJECT
    
    public:
	DepartureModel( QObject *parent = 0 );

	virtual int columnCount( const QModelIndex& parent = QModelIndex() ) const;
	virtual bool removeRows( int row, int count,
				 const QModelIndex& parent = QModelIndex() );
	virtual QVariant headerData( int section, Qt::Orientation orientation,
				     int role = Qt::DisplayRole ) const;
	virtual void sort( int column, Qt::SortOrder order = Qt::AscendingOrder );
	
	ItemBase *itemFromInfo( const DepartureInfo &info ) const {
	    return m_infoToItem.contains(info.hash()) ? m_infoToItem[info.hash()] : NULL; };
	QModelIndex indexFromInfo( const DepartureInfo &info ) const {
	    return m_infoToItem.contains(info.hash()) ? m_infoToItem[info.hash()]->index() : QModelIndex(); };

	QList< uint > itemHashes() const;
	virtual DepartureItem *addItem( const DepartureInfo &departureInfo,
			   Columns sortColumn = ColumnDeparture,
			   Qt::SortOrder sortOrder = Qt::AscendingOrder );
	virtual void updateItem( DepartureItem *departureItem,
				 const DepartureInfo &newDepartureInfo ) {
	    departureItem->setDepartureInfo( newDepartureInfo ); };
	/** Removes all departures from the model, but doesn't clear header data. */
	virtual void clear();

	Infos infos() const { return m_infos; };
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
	void addAlarm( DepartureItem *item );
	void removeAlarm( DepartureItem *item );

    signals:
	/** The alarm for @p item has been fired. */
	void alarmFired( DepartureItem *item );
	void updateAlarms( const AlarmSettingsList &newAlarmSettings,
			   const QList< int > &removedAlarms );

    protected slots:
	virtual void update();

    private:
	virtual DepartureItem *findNextItem( bool sortedByDepartureAscending = false ) const;
	void fireAlarm( const QDateTime& dateTime, DepartureItem* item );
	
	QMultiMap< QDateTime, DepartureItem* > m_alarms;
};

class JourneyModel : public PublicTransportModel {
    Q_OBJECT

    public:
	JourneyModel( QObject *parent = 0 );

	virtual int columnCount( const QModelIndex& parent = QModelIndex() ) const;
	virtual bool removeRows( int row, int count,
				 const QModelIndex& parent = QModelIndex() );
	virtual void clear();
	virtual QVariant headerData( int section, Qt::Orientation orientation,
				     int role = Qt::DisplayRole ) const;
	virtual void sort( int column, Qt::SortOrder order = Qt::AscendingOrder );

	ItemBase *itemFromInfo( const JourneyInfo &info ) const {
	    return m_infoToItem.contains(info.hash()) ? m_infoToItem[info.hash()] : NULL; };
	QModelIndex indexFromInfo( const JourneyInfo &info ) const {
	    return m_infoToItem.contains(info.hash()) ? m_infoToItem[info.hash()]->index() : QModelIndex(); };

	QList< uint > itemHashes() const;
	virtual JourneyItem *addItem( const JourneyInfo &journeyInfo,
			   Columns sortColumn = ColumnDeparture,
			   Qt::SortOrder sortOrder = Qt::AscendingOrder );
	virtual void updateItem( JourneyItem *journeyItem, const JourneyInfo &newJourneyInfo ) {
	    journeyItem->setJourneyInfo( newJourneyInfo ); };

	Infos infos() const { return m_infos; };
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
