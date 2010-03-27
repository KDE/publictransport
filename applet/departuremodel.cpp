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

#include "departuremodel.h"

#include <KIconEffect>
#include <KIconLoader>
#include <KDebug>
#include <QTimer>
#include "settings.h"

class DepartureModelLessThan {
public:
    inline DepartureModelLessThan( Columns column = ColumnDeparture ) {
	this->column = column; };
        
    inline bool operator()( const QPair<DepartureItem*, int> &l,
			    const QPair<DepartureItem*, int> &r ) const {
	return operator()( l.first->departureInfo(), r.first->departureInfo() );
    };
    
    inline bool operator()( const DepartureInfo* l, const DepartureInfo* r ) const {
	switch ( column ) {
	    case ColumnDeparture:
		return l->predictedDeparture() < r->predictedDeparture();
	    case ColumnTarget:
		return l->target() < r->target();
	    case ColumnLineString:
		if ( l->lineNumber() < r->lineNumber() )
		    return true;
		else
		    return l->lineString().localeAwareCompare( r->lineString() ) < 0;
	    default:
		kDebug() << "Can't sort unknown column" << column;
		return false;
	}
    };
    Columns column;
};

class DepartureModelGreaterThan {
public:
    inline DepartureModelGreaterThan( Columns column = ColumnDeparture ) {
	this->column = column; };

    inline bool operator()( const QPair<DepartureItem*, int> &l,
			    const QPair<DepartureItem*, int> &r ) const {
	return operator()( l.first->departureInfo(), r.first->departureInfo() );
    };
    
    inline bool operator()( const DepartureInfo* l, const DepartureInfo* r ) const {
	switch ( column ) {
	    case ColumnDeparture:
		return l->predictedDeparture() > r->predictedDeparture();
	    case ColumnTarget:
		return l->target() > r->target();
	    case ColumnLineString:
		if ( l->lineNumber() > r->lineNumber() )
		    return true;
		else
		    return l->lineString().localeAwareCompare(r->lineString() ) > 0;
	    default:
		kDebug() << "Can't sort unknown column" << column;
		return false;
	}
    };
    Columns column;
};

class JourneyModelLessThan {
public:
    inline JourneyModelLessThan( Columns column = ColumnDeparture ) {
	this->column = column; };

    inline bool operator()( const QPair<JourneyItem*, int> &l,
			    const QPair<JourneyItem*, int> &r ) const {
	return operator()( l.first->journeyInfo(), r.first->journeyInfo() );
    };

    inline bool operator()( const JourneyInfo* l, const JourneyInfo* r ) const {
	switch ( column ) {
	    case ColumnDeparture:
		return l->departure() < r->departure();
	    case ColumnTarget:
		return l->targetStopName() < r->targetStopName();
	    case ColumnLineString:
		if ( l->vehicleTypes().count() < r->vehicleTypes().count() )
		    return true;
	    default:
		kDebug() << "Can't sort unknown column" << column;
		return false;
	}
    };
    Columns column;
};

class JourneyModelGreaterThan {
public:
    inline JourneyModelGreaterThan( Columns column = ColumnDeparture ) {
	this->column = column; };

    inline bool operator()( const QPair<JourneyItem*, int> &l,
			    const QPair<JourneyItem*, int> &r ) const {
	return operator()( l.first->journeyInfo(), r.first->journeyInfo() );
    };

    inline bool operator()( const JourneyInfo* l, const JourneyInfo* r ) const {
	switch ( column ) {
	    case ColumnDeparture:
		return l->departure() > r->departure();
	    case ColumnTarget:
		return l->targetStopName() > r->targetStopName();
	    case ColumnLineString:
		if ( l->vehicleTypes().count() > r->vehicleTypes().count() )
		    return true;
	    default:
		kDebug() << "Can't sort unknown column" << column;
		return false;
	}
    };
    Columns column;
};

ItemBase::ItemBase( const Infos *infos ) : m_parent(0), m_model(0), m_infos(infos) {
    Q_ASSERT_X( infos, "DepartureModelItemBase::DepartureModelItemBase",
		"The pointer to the Info object must be given." );
}

ItemBase::~ItemBase() {
    qDeleteAll( m_children );
}

void ItemBase::setModel( PublicTransportModel* model ) {
    m_model = model;
    foreach ( ChildItem *child, m_children )
	child->setModel( model );
}

ItemBase* ItemBase::topLevelParent() const {
    ItemBase *p = const_cast< ItemBase* >( this );
    while ( p->parent() )
	p = parent();
    return p;
}

QModelIndex ItemBase::index() {
    if ( m_model )
	return m_model->index( this );
    else
	return QModelIndex();
}

ChildItem* ItemBase::childByType( ItemType itemType ) const {
    foreach ( ChildItem *child, m_children ) {
	if ( child->type() == itemType )
	    return child;
    }
    return NULL;
}

void ItemBase::removeChildren( int first, int count ) {
    if ( first == -1 ) {
	kDebug() << "Not a child of this item";
	return;
    }
    
    for ( int i = 0; i < count; ++i ) {
	ChildItem *child = m_children.takeAt( first );
	delete child;
    }
}

void ItemBase::appendChild( ChildItem* child ) {
    m_children.append( child );
    child->m_parent = this;
    child->m_model = m_model;
}

ChildItem::ChildItem( ItemType itemType,
	    const QString& formattedText, const QIcon& icon, const Infos *infos )
	    : ItemBase(infos) {
    m_type = itemType;
    setFormattedText( formattedText );
    setIcon( icon );
}

ChildItem::ChildItem( ItemType itemType,
			    const QString& formattedText, const Infos* infos )
			    : ItemBase(infos) {
    m_type = itemType;
    setFormattedText( formattedText );
}

ChildItem::ChildItem( ItemType itemType, const Infos* infos)
				: ItemBase(infos) {
    m_type = itemType;
}

QVariant ChildItem::data( int role, int ) const {
    if ( m_data.contains(role) ) {
	return m_data.value( role );
    } else if ( role == DrawAlarmBackground ) {
	ItemBase *p = topLevelParent();
	return p->hasPendingAlarm();
    } else if ( role == FormattedTextRole ) {
	return m_data.value( Qt::DisplayRole );
    } else if ( role == JourneyRatingRole && dynamic_cast<JourneyModel*>(m_model) ) {
	JourneyItem *topLevelJourneyItem = static_cast<JourneyItem*>( topLevelParent() );
	return topLevelJourneyItem->data( JourneyRatingRole );
    }
    
    return QVariant();
}

void ChildItem::setData( const QVariant& data, int role ) {
    m_data[ role ] = data;
    if ( m_model )
	m_model->itemChanged( this, 0, 0 );
}


JourneyItem::JourneyItem( const JourneyInfo& journeyInfo, const Infos* infos )
				: ItemBase( infos ) {
    setJourneyInfo( journeyInfo );
}

int JourneyItem::row() const {
    if ( m_model )
	return m_model->rowFromItem( const_cast<JourneyItem*>(this) );
    else
	return -1;
}

QHash< ItemType, ChildItem* > JourneyItem::typedChildren() const {
    QHash< ItemType, ChildItem* > children;
    foreach ( ChildItem *child, m_children ) {
	if ( child->type() != OtherItem )
	    children.insert( child->type(), child );
    }
    
    return children;
}

QVariant JourneyItem::data( int role, int column ) const {
    if ( column >= m_columnData.count() )
	return QVariant();

    if ( column < m_columnData.count() && m_columnData[column].contains(role) ) {
	return m_columnData[column].value( role );
//     } else if ( role == DrawAlarmBackground ) {
// 	JourneyItem *p = topLevelParent();
// 	return p->m_alarm.testFlag( AlarmPending );
    } else if ( !m_parent ) {
	switch ( role ) {
	    case LinesPerRowRole:
		return m_infos->linesPerRow;
	    case Qt::TextAlignmentRole:
		return static_cast<int>( (column == 0 ? Qt::AlignRight : Qt::AlignLeft)
				    | Qt::AlignVCenter );
	    case DecorationPositionRole:
		return column == 0 ? DecorationLeft : DecorationRight;

	    case FormattedTextRole: // No formatted text defined
		if ( column < m_columnData.count() )
		    return m_columnData[column].value( Qt::DisplayRole );
		
	    case JourneyRatingRole:
		return rating();

	    default:
		return QVariant();
	}
    }

    return QVariant();
}

qreal JourneyItem::rating() const {
    if ( !m_model )
	return 0.5;
    
    JourneyModel *model = static_cast<JourneyModel*>( m_model );
    int div = model->biggestDuration() - model->smallestDuration();
    if ( div == 0 ) // all journeys have equal durations
	return 0.5;
    else
	return qreal(journeyInfo()->duration() - model->smallestDuration()) / qreal(div);
}

void JourneyItem::setData( Columns column, const QVariant& data, int role ) {
    m_columnData[ column ][ role ] = data;
    if ( m_model )
	m_model->itemChanged( this, column, column );
}

void JourneyItem::setJourneyInfo( const JourneyInfo& journeyInfo ) {
    if ( m_journeyInfo.isValid() ) {
// 	if ( m_journeyInfo == journeyInfo ) // TODO
// 	    return; // Unchanged
	    
	m_journeyInfo = journeyInfo;
	updateValues();
	updateChildren();
    } else {
	m_journeyInfo = journeyInfo;
	updateValues();
	createChildren();
    }
}

void JourneyItem::updateValues() {
    setIcon( ColumnLineString, Global::iconFromVehicleTypeList(
	    m_journeyInfo.vehicleTypes(), 32 * m_infos->sizeFactor) );

    QString sDuration = KGlobal::locale()->prettyFormatDuration(
	    m_journeyInfo.duration() * 60 * 1000 );
    QString text = i18np("<b>Duration:</b> %2, <nobr><b>%1</b> change</nobr>",
			 "<b>Duration:</b> %2, <nobr><b>%1</b> changes</nobr>",
			 m_journeyInfo.changes(), sDuration);
    setFormattedText( ColumnJourneyInfo, text );
//     setText( s.replace(QRegExp("<[^>]*>"), "") );
    if ( !m_journeyInfo.journeyNews().isEmpty() ) {
	setIcon( ColumnJourneyInfo, Global::makeOverlayIcon(
		KIcon("view-pim-news"), "arrow-down", QSize(12,12)) );
    }
    
    updateTimeValues();
    
    if ( m_model )
	m_model->itemChanged( this, 0, 2 );
}

void JourneyItem::updateChildren() {
    QHash< ItemType, ChildItem* > children = typedChildren();
    
    QList< ItemType > types;
    types << DurationItem << ChangesItem << PricingItem << JourneyNewsItem << RouteItem;
    foreach ( ItemType type, types ) {
	if ( hasDataForChildType(type) ) {
	    if ( children.contains(type) )
		updateChild( type, children[type] );
	    else
		appendNewChild( type );
	} else if ( children.contains(type) )
	    removeChild( children[type] );
    }
}

void JourneyItem::createChildren() {
    QList< ItemType > types;
    types << DurationItem << ChangesItem << PricingItem << JourneyNewsItem << RouteItem;
    foreach ( ItemType type, types ) {
	if ( hasDataForChildType(type) )
	    appendNewChild( type );
    }
}

void JourneyItem::updateChild( ItemType itemType, ChildItem* child ) {
    if ( itemType == RouteItem ) {
	m_model->removeRows( child->row(), 1, child->parent()->index() );
	appendNewChild( RouteItem );
    } else {
	int linesPerRow;
	child->setFormattedText( childItemText(itemType, &linesPerRow) );
	if ( itemType == JourneyNewsItem || itemType == DelayItem )
	    child->setData( linesPerRow, LinesPerRowRole );
    }
}

ChildItem* JourneyItem::appendNewChild( ItemType itemType ) {
    ChildItem *child;
    if ( itemType == RouteItem ) {
	child = createRouteItem();
    } else {
	int linesPerRow;
	child = new ChildItem(
		itemType, childItemText(itemType, &linesPerRow), KIcon(), m_infos );
	if ( itemType == JourneyNewsItem || itemType == DelayItem )
	    child->setData( linesPerRow, LinesPerRowRole );
    }
    
    appendChild( child );
    return child;
}

void JourneyItem::updateTimeValues() {
    QString depTextFormatted = m_journeyInfo.departureText( true,
	    m_infos->displayTimeBold, true, true,
// 	    m_infos->showRemainingMinutes, m_infos->showDepartureTime,
	    m_infos->linesPerRow );
    QString oldTextFormatted = formattedText( ColumnDeparture );
    if ( oldTextFormatted != depTextFormatted ) {
	setFormattedText( ColumnDeparture, depTextFormatted );

	QString longestLine;
	QString depText = m_journeyInfo.departureText( false,
		m_infos->displayTimeBold, true, true,
// 	    m_infos->showRemainingMinutes, m_infos->showDepartureTime,
		m_infos->linesPerRow );
	if ( m_infos->linesPerRow > 1 ) {
	    // Get longest line for auto column sizing
	    QStringList lines = depText.split( "\n", QString::SkipEmptyParts,
					       Qt::CaseInsensitive );
	    foreach( QString sCurrent, lines ) {
		if ( sCurrent.length() > longestLine.length() )
		    longestLine = sCurrent;
	    }
	} else
	    longestLine = depText;
	setText( ColumnDeparture, longestLine ); // This is just used for auto column sizing
    }
    
    QString arrTextFormatted = m_journeyInfo.arrivalText( true,
	    m_infos->displayTimeBold,true, true,
// 	    m_infos->showRemainingMinutes, m_infos->showDepartureTime,
	    m_infos->linesPerRow );
    oldTextFormatted = formattedText( ColumnArrival );
    if ( oldTextFormatted != arrTextFormatted ) {
	setFormattedText( ColumnArrival, arrTextFormatted );
	
	QString longestLine;
	QString depText = m_journeyInfo.departureText( false,
		m_infos->displayTimeBold, true, true,
// 	    m_infos->showRemainingMinutes, m_infos->showDepartureTime,
		m_infos->linesPerRow );
	if ( m_infos->linesPerRow > 1 ) {
	    // Get longest line for auto column sizing
	    QStringList lines = depText.split( "\n", QString::SkipEmptyParts,
					       Qt::CaseInsensitive );
	    foreach( QString sCurrent, lines ) {
		if ( sCurrent.length() > longestLine.length() )
		    longestLine = sCurrent;
	    }
	} else
	    longestLine = depText;
	setText( ColumnDeparture, longestLine ); // This is just used for auto column sizing
    }

    if ( m_model )
	m_model->itemChanged( this, 2, 2 );
}

bool JourneyItem::hasDataForChildType( ItemType itemType ) {
    switch ( itemType ) {
	case JourneyNewsItem:
	    return !m_journeyInfo.journeyNews().isEmpty();
	case OperatorItem:
	    return !m_journeyInfo.operatorName().isEmpty();
	case RouteItem:
	    return !m_journeyInfo.routeStops().isEmpty();
	case DurationItem:
	    return m_journeyInfo.duration() > 0;
	case ChangesItem:
	    return m_journeyInfo.changes() > 0;
	case PricingItem:
	    return !m_journeyInfo.pricing().isEmpty();

	case OtherItem:
	default:
	    kDebug() << "Wrong item type" << itemType;
	    break;
    }

    return false;
}

QString JourneyItem::childItemText( ItemType itemType, int* linesPerRow ) {
    QString text;
    if ( linesPerRow )
	*linesPerRow = 1;
    switch ( itemType ) {
	case JourneyNewsItem:
	    text = m_journeyInfo.journeyNews();
	    if ( text.startsWith("http://") ) // TODO: Make the link clickable...
		text = QString("<a href='%1'>%2</a>").arg(text).arg(i18n("Link to journey news"));
	    text = QString("<b>%1</b> %2").arg( i18nc("News for a journey with public "
			"transport, like 'platform changed'", "News:") ).arg( text );
	    if ( linesPerRow )
		*linesPerRow = qMin( 3, text.length() / 25 );
	    break;
	case OperatorItem:
	    text = QString("<b>%1</b> %2").arg( i18nc("The company that is "
		"responsible for this departure/arrival/journey", "Operator:") )
		.arg( m_journeyInfo.operatorName() );
	    break;
	case DurationItem:
	    if ( m_journeyInfo.duration() <= 0 ) {
		text = QString( "<b>%1</b> %2" )
		    .arg( i18nc("The duration of a journey", "Duration:") )
		    .arg( 0 );
	    } else {
		text = QString( "<b>%1</b> %2" )
		    .arg( i18nc("The duration of a journey", "Duration:") )
		    .arg( Global::durationString(m_journeyInfo.duration() * 60) );
	    }
	    break;
	case ChangesItem:
	    text = QString( "<b>%1</b> %2" )
		    .arg( i18nc("The changes of a journey", "Changes:") )
		    .arg( m_journeyInfo.changes() );
	    break;
	case PricingItem:
	    text = QString( "<b>%1</b> %2" )
		    .arg( i18nc("The pricing of a journey", "Pricing:") )
		    .arg( m_journeyInfo.pricing() );
	    break;
	case RouteItem:
	    if ( m_journeyInfo.routeExactStops() > 0
	      && m_journeyInfo.routeExactStops() < m_journeyInfo.routeStops().count() )
	    {
		text = QString("<b>%1</b> %2")
		    .arg( i18nc("The route of this departure/arrival/journey", "Route:") )
		    .arg( i18n("> %1 stops", m_journeyInfo.routeStops().count()) );
	    } else {
		text = QString("<b>%1</b> %2")
		    .arg( i18nc("The route of this departure/arrival/journey", "Route:") )
		    .arg( i18n("%1 stops", m_journeyInfo.routeStops().count()) );
	    }
	    break;

	case OtherItem:
	default:
	    kDebug() << "Wrong item type" << itemType;
	    break;
    }

    return text;
}

ChildItem* JourneyItem::createRouteItem() {
    ChildItem *routeItem = new ChildItem( RouteItem, childItemText(RouteItem), m_infos );

    // Add route stops as child rows				TODO: -1 ?
    for ( int row = 0; row < m_journeyInfo.routeStops().count() - 1; ++row ) {
	// Add a separator item, when the exact route ends
	if ( row == m_journeyInfo.routeExactStops() && row > 0 ) {
	    ChildItem *separatorItem = new ChildItem(
		    OtherItem, i18n("  - End of exact route -  "), m_infos );
	    routeItem->appendChild( separatorItem );
	}

	KIcon icon;
	QString sTransportLine;
	if ( row < m_journeyInfo.routeVehicleTypes().count()
		    && m_journeyInfo.routeVehicleTypes()[row] != Unknown)
	    icon = Global::vehicleTypeToIcon( m_journeyInfo.routeVehicleTypes()[row] );
	    if ( m_journeyInfo.routeVehicleTypes()[row] == Feet )
		sTransportLine = i18n("Footway");
	    else if ( m_journeyInfo.routeTransportLines().count() > row )
		sTransportLine = m_journeyInfo.routeTransportLines()[row];
	else {
	    icon = KIcon("public-transport-stop");
	    if ( m_journeyInfo.routeTransportLines().count() > row )
		sTransportLine = m_journeyInfo.routeTransportLines()[row];
	}

	QString stopDep = m_journeyInfo.routeStops()[row];
	QString stopArr = m_journeyInfo.routeStops()[row + 1];
	if ( m_journeyInfo.routePlatformsDeparture().count() > row
		&& !m_journeyInfo.routePlatformsDeparture()[row].isEmpty() ) {
	    stopDep = i18n("Platform %1", m_journeyInfo.routePlatformsDeparture()[row])
		    + " - " + stopDep;
	}
	if ( m_journeyInfo.routePlatformsArrival().count() > row
		&& !m_journeyInfo.routePlatformsArrival()[row].isEmpty() ) {
	    stopArr = i18n("Platform %1", m_journeyInfo.routePlatformsArrival()[row])
		    + " - " + stopArr;
	}

	QString sTimeDep = m_journeyInfo.routeTimesDeparture()[row].toString("hh:mm");
	if ( m_journeyInfo.routeTimesDepartureDelay().count() > row ) {
	    int delay = m_journeyInfo.routeTimesDepartureDelay()[ row ];
	    if ( delay > 0 ) {
		sTimeDep += QString( " <span style='color:%2;'>+%1</span>" )
			.arg( delay ).arg( Global::textColorDelayed().name() );
	    } else if ( delay == 0 ) {
		sTimeDep = sTimeDep.prepend( QString("<span style='color:%1;'>")
			.arg(Global::textColorOnSchedule().name()) )
			.append( "</span>" );
	    }
	}

	QString sTimeArr = m_journeyInfo.routeTimesArrival()[row].toString("hh:mm");
	if ( m_journeyInfo.routeTimesArrivalDelay().count() > row ) {
	    int delay = m_journeyInfo.routeTimesArrivalDelay()[ row ];
	    if ( delay > 0 ) {
		sTimeArr += QString( " <span style='color:%2;'>+%1</span>" )
			.arg( delay ).arg( Global::textColorDelayed().name() );
	    } else if ( delay == 0 ) {
		sTimeArr = sTimeArr.prepend( QString("<span style='color:%1;'>")
			.arg(Global::textColorOnSchedule().name()) )
			.append( "</span>" );
	    }
	}
	
	ChildItem *routeStopItem;
	if ( sTransportLine.isEmpty() ) {
	    routeStopItem = new ChildItem( OtherItem,
		    i18nc("%1 is the departure time, %2 the origin stop name, "
			    "%3 the arrival time, %4 the target stop name.",
			    "dep: %1 - %2<br>arr: %3 - %4",
			    sTimeDep, stopDep, sTimeArr, stopArr),
			    icon, m_infos );
	    routeStopItem->setData( 2, LinesPerRowRole );
	} else {
	    routeStopItem = new ChildItem( OtherItem,
		    i18nc("%1 is the departure time, %2 the origin stop name, "
			    "%3 the arrival time, %4 the target stop name, "
			    "%5 the transport line.",
			    "<b>%5</b><br>dep: %1 - %2<br>arr: %3 - %4",
			    sTimeDep, stopDep, sTimeArr, stopArr,
			    sTransportLine), icon, m_infos );
	    routeStopItem->setData( 3, LinesPerRowRole );
	}

	int iconExtend = 16 * m_infos->sizeFactor;
	routeStopItem->setData( QSize(iconExtend, iconExtend), IconSizeRole );

	routeItem->appendChild( routeStopItem );
    }

    return routeItem;
}

DepartureItem::DepartureItem( const DepartureInfo &departureInfo, const Infos *infos )
				: ItemBase( infos ) {
    m_alarm = NoAlarm;
    setDepartureInfo( departureInfo );
}

int DepartureItem::row() const {
    if ( m_model )
	return m_model->rowFromItem( const_cast<DepartureItem*>(this) );
    else
	return -1;
}

QHash< ItemType, ChildItem* > DepartureItem::typedChildren() const {
    QHash< ItemType, ChildItem* > children;
    foreach ( ChildItem *child, m_children ) {
	if ( child->type() != OtherItem )
	    children.insert( child->type(), child );
    }

    return children;
}

void DepartureItem::setDepartureInfo( const DepartureInfo &departureInfo ) {
    if ( m_departureInfo.isValid() ) {
	if ( m_departureInfo == departureInfo )
	    return; // Unchanged
	
	m_departureInfo = departureInfo;
	updateValues();
	updateChildren();
    } else {
	m_departureInfo = departureInfo;
	updateValues();
	createChildren();
    }
}

void DepartureItem::updateValues() {
    setText( ColumnLineString, m_departureInfo.lineString() );
    setFormattedText( ColumnLineString, QString("<span style='font-weight:bold;'>%1</span>")
					.arg(m_departureInfo.lineString()) );
    // if ( departureInfo.vehicleType() != Unknown )
    setIcon( ColumnLineString, Global::vehicleTypeToIcon(m_departureInfo.vehicleType()) );

    setText( ColumnTarget, m_departureInfo.target() );
    if ( !m_departureInfo.journeyNews().isEmpty() ) {
	setIcon( ColumnTarget, Global::makeOverlayIcon(KIcon("view-pim-news"),
					 "arrow-down", QSize(12,12)) );
    }

    updateTimeValues();

    if ( m_model )
	m_model->itemChanged( this, 0, 2 );
}

void DepartureItem::updateChildren() {
    QHash< ItemType, ChildItem* > children = typedChildren();

    QList< ItemType > types;
    types << PlatformItem << JourneyNewsItem << DelayItem << OperatorItem << RouteItem;
    foreach ( ItemType type, types ) {
	if ( hasDataForChildType(type) ) {
	    if ( children.contains(type) )
		updateChild( type, children[type] );
	    else
		appendNewChild( type );
	} else if ( children.contains(type) )
	    removeChild( children[type] );
    }
}

void DepartureItem::createChildren() {
    QList< ItemType > types;
    types << PlatformItem << JourneyNewsItem << DelayItem << OperatorItem << RouteItem;
    foreach ( ItemType type, types ) {
	if ( hasDataForChildType(type) )
	    appendNewChild( type );
    }
}

void DepartureItem::updateChild( ItemType itemType, ChildItem* child ) {
    if ( itemType == RouteItem ) {
	m_model->removeRows( child->row(), 1, child->parent()->index() );
	appendNewChild( RouteItem );
    } else {
	int linesPerRow;
	child->setFormattedText( childItemText(itemType, &linesPerRow) );
	if ( itemType == JourneyNewsItem || itemType == DelayItem )
	    child->setData( linesPerRow, LinesPerRowRole );
    }
}

ChildItem* DepartureItem::appendNewChild( ItemType itemType ) {
    ChildItem *child;
    if ( itemType == RouteItem ) {
	child = createRouteItem();
    } else {
	int linesPerRow;
	child = new ChildItem( itemType, childItemText(itemType, &linesPerRow), KIcon(), m_infos );
	if ( itemType == JourneyNewsItem || itemType == DelayItem )
	    child->setData( linesPerRow, LinesPerRowRole );
    }
    
    appendChild( child );
    return child;
}

void DepartureItem::updateTimeValues() {
    QString depTextFormatted = m_departureInfo.departureText( true,
	    m_infos->displayTimeBold, m_infos->showRemainingMinutes,
	    m_infos->showDepartureTime, m_infos->linesPerRow );
    QString oldTextFormatted = formattedText( ColumnDeparture );
    if ( oldTextFormatted != depTextFormatted ) {
	setFormattedText( ColumnDeparture, depTextFormatted );

	QString longestLine;
	QString depText = m_departureInfo.departureText( false,
		m_infos->displayTimeBold, m_infos->showRemainingMinutes,
		m_infos->showDepartureTime, m_infos->linesPerRow );
	if ( m_infos->linesPerRow > 1 ) {
	    // Get longest line for auto column sizing
	    QStringList lines = depText.split( "\n", QString::SkipEmptyParts,
					       Qt::CaseInsensitive );
	    foreach( QString sCurrent, lines ) {
		if ( sCurrent.length() > longestLine.length() )
		    longestLine = sCurrent;
	    }
	} else
	    longestLine = depText;
	setText( ColumnDeparture, longestLine ); // This is just used for auto column sizing
    }
    
    if ( m_model )
	m_model->itemChanged( this, 2, 2 );
}

bool DepartureItem::hasDataForChildType( ItemType itemType ) {
    switch ( itemType ) {
	case PlatformItem:
	    return !m_departureInfo.platform().isEmpty();
	case JourneyNewsItem:
	    return !m_departureInfo.journeyNews().isEmpty();
	case DelayItem:
	    return true; // Also shows "no delay info available"
	case OperatorItem:
	    return !m_departureInfo.operatorName().isEmpty();
	case RouteItem:
	    return !m_departureInfo.routeStops().isEmpty();

	case OtherItem:
	default:
	    kDebug() << "Wrong item type" << itemType;
	    break;
    }

    return false;
}

QString DepartureItem::childItemText( ItemType itemType, int *linesPerRow ) {
    QString text;
    if ( linesPerRow )
	*linesPerRow = 1;
    switch ( itemType ) {
	case PlatformItem:
	    text = QString("<b>%1</b> %2")
		.arg( i18nc("The platform from which a tram/bus/train departs", "Platform:") )
		.arg( m_departureInfo.platform() );
	    break;
	case JourneyNewsItem:
	    text = m_departureInfo.journeyNews();
	    if ( text.startsWith("http://") ) { // TODO: Make the link clickable...
		text = QString("<a href='%1'>%2</a>").arg(text)
			    .arg(i18n("Link to journey news"));
	    }
	    text = QString("<b>%1</b> %2").arg( i18nc("News for a journey with public "
		    "transport, like 'platform changed'", "News:") ).arg( text );
	    if ( linesPerRow )
		*linesPerRow = qMin( 3, text.length() / 25 );
	    break;
	case DelayItem:
	    text = QString("<b>%1</b> %2").arg( i18nc("Information about delays "
		"of a journey with public transport", "Delay:") )
		.arg( m_departureInfo.delayText() );
	    if ( m_departureInfo.delayType() == Delayed ) {
		text += "<br><b>" + (m_infos->departureArrivalListType == ArrivalList
		    ? i18n("Original arrival time:")
		    : i18n("Original departure time:")) + "</b> " +
			m_departureInfo.departure().toString("hh:mm");
		if ( linesPerRow )
		    *linesPerRow = 2;
	    } else if ( linesPerRow )
		*linesPerRow = 1;
	    break;
	case OperatorItem:
	    text = QString("<b>%1</b> %2").arg( i18nc("The company that is "
		"responsible for this departure/arrival/journey", "Operator:") )
		.arg( m_departureInfo.operatorName() );
	    break;
	case RouteItem:
	    if ( m_departureInfo.routeExactStops() > 0
	      && m_departureInfo.routeExactStops() < m_departureInfo.routeStops().count() )
	    {
		text = QString("<b>%1</b> %2")
		    .arg( i18nc("The route of this departure/arrival/journey", "Route:") )
		    .arg( i18n("> %1 stops", m_departureInfo.routeStops().count()) );
	    } else {
		text = QString("<b>%1</b> %2")
		    .arg( i18nc("The route of this departure/arrival/journey", "Route:") )
		    .arg( i18n("%1 stops", m_departureInfo.routeStops().count()) );
	    }
	    break;

	case OtherItem:
	default:
	    kDebug() << "Wrong item type" << itemType;
	    break;
    }

    return text;
}

ChildItem* DepartureItem::createRouteItem() {
    ChildItem *routeItem = new ChildItem( RouteItem, childItemText(RouteItem), m_infos );

    // Add route stops as child rows
    for ( int row = 0; row < m_departureInfo.routeStops().count(); ++row ) {
	// Add a separator item, when the exact route ends
	if ( row == m_departureInfo.routeExactStops() && row > 0 ) {
	    ChildItem *separatorItem = new ChildItem(
		    OtherItem, i18n("  - End of exact route -  "), m_infos );
	    routeItem->appendChild( separatorItem );
	}

	QString text = QString("%1 - %2")
		.arg(m_departureInfo.routeTimes()[row].toString("hh:mm"))
		.arg(m_departureInfo.routeStops()[row]);
	ChildItem *routeStopItem = new ChildItem(
		OtherItem, text, KIcon("public-transport-stop"), m_infos );
	routeItem->appendChild( routeStopItem );
    }

    return routeItem;
}

QVariant DepartureItem::data( int role, int column ) const {
    if ( column >= m_columnData.count() )
	return QVariant();

    if ( column < m_columnData.count() && m_columnData[column].contains(role) ) {
	return m_columnData[column].value( role );
    } else if ( role == DrawAlarmBackground ) {
	return m_alarm.testFlag( AlarmPending );
    } else if ( !m_parent ) {
	switch ( role ) {
	    case LinesPerRowRole:
		return m_infos->linesPerRow;
	    case Qt::TextAlignmentRole:
		return static_cast<int>( (column == 0 ? Qt::AlignRight : Qt::AlignLeft)
			| Qt::AlignVCenter );
	    case DecorationPositionRole:
		return column == 0 ? DecorationLeft : DecorationRight;

	    case FormattedTextRole: // No formatted text defined
		if ( column < m_columnData.count() )
		    return m_columnData[column].value( Qt::DisplayRole );

	    default:
		return QVariant();
	}
    }

    return QVariant();
}

void DepartureItem::setData( Columns column, const QVariant& data, int role ) {
    m_columnData[ column ][ role ] = data;
    if ( m_model )
	m_model->itemChanged( this, column, column );
}

void DepartureItem::setAlarm() {
    removeAlarm(); // Remove old alarm, if any

    static_cast<DepartureModel*>(m_model)->addAlarm( this );
}

void DepartureItem::removeAlarm() {
    if ( hasAlarm() )
	static_cast<DepartureModel*>(m_model)->removeAlarm( this );
}

void DepartureItem::setAlarmStates( AlarmStates alarmStates ) {
    m_alarm = alarmStates;

    if ( alarmStates.testFlag(AlarmPending) ) {
	if ( alarmStates.testFlag(AlarmIsRecurring) )
	    setIcon( ColumnDeparture, KIcon("task-reminder", NULL, QStringList() << "task-recurring") );
	else
	    setIcon( ColumnDeparture, KIcon("task-reminder") );
    } else if ( alarmStates == NoAlarm ) {
	setIcon( ColumnDeparture, KIcon() );
    } else if ( alarmStates.testFlag(AlarmFired) ) {
	KIconEffect iconEffect;
	KIcon icon = alarmStates.testFlag(AlarmIsRecurring)
		? KIcon("task-reminder", NULL, QStringList() << "task-recurring")
		: KIcon("task-reminder");
	QPixmap pixmap = iconEffect.apply( icon.pixmap(16 * m_infos->sizeFactor),
		KIconLoader::Small, KIconLoader::DisabledState );
	KIcon disabledAlarmIcon;
	disabledAlarmIcon.addPixmap( pixmap, QIcon::Normal );
	setIcon( ColumnDeparture, disabledAlarmIcon );
    }
    
    m_model->itemChanged( this, 0, 2 );
    m_model->childrenChanged( this ); // Children inherit the alarm background
}

PublicTransportModel::PublicTransportModel( QObject* parent )
			: QAbstractItemModel( parent ), m_nextItem( 0 ),
			m_updateTimer( new QTimer(this) ) {
    m_updateTimer->setInterval( 60000 );
    connect( m_updateTimer, SIGNAL(timeout()), this, SLOT(update()) );

    callAtNextFullMinute( SLOT(startUpdateTimer()) );
}

void PublicTransportModel::startUpdateTimer() {
    update();
    m_updateTimer->start();
}

void PublicTransportModel::callAtNextFullMinute( const char* member ) {
    QTime time = QTime::currentTime();
    QTime nextMinute( time.hour(), time.minute() );
    nextMinute = nextMinute.addSecs( 60 );
    int secs = time.secsTo(nextMinute) * 1000;
    QTimer::singleShot( qMin(60000, secs), this, member );
}

void PublicTransportModel::setLinesPerRow( int linesPerRow ) {
    if ( m_infos.linesPerRow == linesPerRow )
	return;
    m_infos.linesPerRow = linesPerRow;
    emit dataChanged( index(0, 0), index(rowCount(), 0) );
}

void PublicTransportModel::setSizeFactor( float sizeFactor ) {
    m_infos.sizeFactor = sizeFactor;
    //     TODO: Update items?
}

void PublicTransportModel::setDepartureColumnSettings( bool displayTimeBold,
			    bool showRemainingMinutes, bool showDepartureTime) {
    m_infos.displayTimeBold = displayTimeBold;
    m_infos.showRemainingMinutes = showRemainingMinutes;
    m_infos.showDepartureTime = showDepartureTime;
    foreach ( ItemBase *item, m_items )
	item->updateTimeValues();
}

QModelIndex PublicTransportModel::index( int row, int column,
					 const QModelIndex& parent ) const {
    if ( parent.isValid() ) {
	ItemBase *parentItem = static_cast<ItemBase*>( parent.internalPointer() );
	if ( !hasIndex(row, column, parent) )
	    return QModelIndex();

	if ( row < parentItem->childCount() )
	    return createIndex( row, column, parentItem->child(row) );
	else
	    return QModelIndex();
    } else {
	if ( !hasIndex(row, column, QModelIndex()) )
	    return QModelIndex();

	if ( row >= 0 && row < m_items.count() )
	    return createIndex( row, column, m_items[row] );
	else
	    return QModelIndex();
    }
}

QModelIndex PublicTransportModel::parent( const QModelIndex& child ) const {
    if ( !child.isValid() )
	return QModelIndex();
    
    ItemBase *parent = static_cast<ItemBase*>( child.internalPointer() )->parent();
    if ( parent )
	return createIndex( parent->row(), 0, parent );
    else
	return QModelIndex();
}

int PublicTransportModel::rowCount( const QModelIndex& parent ) const {
    if ( parent.column() > 0 )
	return 0;

    if ( parent.isValid() ) {
	ItemBase *parentItem = static_cast<ItemBase*>( parent.internalPointer() );
	return parentItem->childCount();
    } else {
	return m_items.count();
    }
}

ItemBase* PublicTransportModel::itemFromIndex( const QModelIndex& index ) const {
    return static_cast<ItemBase*>( index.internalPointer() );
}

int PublicTransportModel::rowFromItem( ItemBase* item ) {
    return m_items.indexOf( item );
}

QModelIndex PublicTransportModel::indexFromItem( ItemBase* item, int column ) const {
    return item ? createIndex(item->row(), column, item) : QModelIndex();
}

void PublicTransportModel::itemChanged( ItemBase* item, int columnLeft, int columnRight ) {
    if ( columnLeft == columnRight ) {
	QModelIndex index = indexFromItem( item, columnLeft );
	emit dataChanged( index, index );
    } else {
	QModelIndex indexLeft = indexFromItem( item, columnLeft );
	QModelIndex indexRight = indexFromItem( item, columnRight );
	emit dataChanged( indexLeft, indexRight );
    }
}

void PublicTransportModel::childrenChanged( ItemBase* parentItem ) {
    if ( !parentItem->children().isEmpty() ) {
	QModelIndex indexFirst = indexFromItem( parentItem->children().first() );
	QModelIndex indexLast = indexFromItem( parentItem->children().last() );
	emit dataChanged( indexFirst, indexLast );

	foreach ( ChildItem *child, parentItem->children() )
	    childrenChanged( child );
    }
}

QVariant PublicTransportModel::data( const QModelIndex& index, int role ) const {
    if ( !index.isValid() )
	return QVariant();
    
    ItemBase *item = static_cast<ItemBase*>( index.internalPointer() );
    return item->data( role, index.column() );
}

void PublicTransportModel::clear() {
    beginRemoveRows( QModelIndex(), 0, m_items.count() );

    m_infoToItem.clear();
    qDeleteAll( m_items );
    m_items.clear();
    m_nextItem = NULL;

    endRemoveRows();
}

JourneyModel::JourneyModel( QObject* parent ) : PublicTransportModel(parent) {
    m_smallestDuration = 999999;
    m_biggestDuration = 0;
}

int JourneyModel::columnCount( const QModelIndex& parent ) const {
    return parent.isValid() ? 1 : 4;
}

QVariant JourneyModel::headerData( int section, Qt::Orientation orientation,
				   int role ) const {
    if ( orientation == Qt::Horizontal && role == Qt::DisplayRole ) {
	switch ( section ) {
	    case 0: return i18nc( "A public transport line", "Line" );
	    case 1: return i18nc("Information about a journey with public transport", "Information");
	    case 2: return i18nc("Time of departure of a tram or bus", "Departure");
	    case 3: return i18nc("Time of arrival of a tram or bus", "Arrival");
	}
    }

    return QVariant();
}

bool JourneyModel::removeRows( int row, int count, const QModelIndex& parent ) {
    beginRemoveRows( parent, row, row + count - 1 );
    if ( parent.isValid() ) {
	ItemBase *item = itemFromIndex( parent );
	item->removeChildren( row, count );
    } else {
	for ( int i = 0; i < count; ++i ) {
	    JourneyItem *item = static_cast<JourneyItem*>( m_items.takeAt(row) );
	    m_infoToItem.remove( item->journeyInfo()->hash() );
	    if ( m_nextItem == item )
		m_nextItem = findNextItem();
// 	    if ( item->journeyInfo()->duration() == m_biggestDuration )
		// TODO Find new biggest duration item
// 	    else if ( item->journeyInfo()->duration() == m_smallestDuration )
		// TODO Find new smallest duration item
	    delete item;
	}
    }

    if ( isEmpty() ) {
	m_smallestDuration = 999999;
	m_biggestDuration = 0;
    }
    endRemoveRows();

    return true;
}

void JourneyModel::clear() {
    PublicTransportModel::clear();
    
    m_smallestDuration = 999999;
    m_biggestDuration = 0;
}

void JourneyModel::update() {
//     if ( m_infos.showRemainingMinutes ) {
	foreach ( ItemBase *item, m_items )
	    item->updateTimeValues();
//     }
}

void JourneyModel::sort( int column, Qt::SortOrder order ) {
    if ( column < 0 || rowCount() == 0 )
	return;
    
    emit layoutAboutToBeChanged();
    
    QVector< QPair<JourneyItem*, int> > sortable;
    for ( int row = 0; row < m_items.count(); ++row )
	sortable.append( QPair<JourneyItem*,int>(
		static_cast<JourneyItem*>(m_items[row]), row) );
    
    if (order == Qt::AscendingOrder) {
	JourneyModelLessThan lt( static_cast<Columns>(column) );
	qStableSort( sortable.begin(), sortable.end(), lt );
    } else {
	JourneyModelGreaterThan gt( static_cast<Columns>(column) );
	qStableSort( sortable.begin(), sortable.end(), gt );
    }
    
    QModelIndexList changedPersistentIndexesFrom, changedPersistentIndexesTo;
    QList< ItemBase* > sorted_children;
    for ( int i = 0; i < m_items.count(); ++i ) {
	int r = sortable.at(i).second;
	ItemBase *item = m_items[r];
	sorted_children << item;
	
	for ( int c = 0; c < columnCount(); ++c ) {
	    changedPersistentIndexesFrom.append( createIndex(r, c) );
	    changedPersistentIndexesTo.append( createIndex(i, c) );
	}
    }
    
    m_items = sorted_children;
    changePersistentIndexList( changedPersistentIndexesFrom, changedPersistentIndexesTo );
    
    emit layoutChanged();
}

JourneyItem* JourneyModel::addItem( const JourneyInfo& journeyInfo,
				    Columns sortColumn, Qt::SortOrder sortOrder) {
    if ( m_infoToItem.contains(journeyInfo.hash()) ) {
	kDebug() << "Journey already added to the model";
	return NULL;
    }

    // Find the row where to insert the new journey
    int count = m_items.count();
    int insertBefore = count;
    if ( sortOrder == Qt::AscendingOrder ) {
	JourneyModelGreaterThan gt( static_cast<Columns>(sortColumn) );
	for ( int i = 0; i < count; ++i ) {
	    JourneyItem *item = static_cast<JourneyItem*>( m_items.at(i) );
	    if ( gt.operator()(item->journeyInfo(), &journeyInfo) ) {
		insertBefore = i;
		break;
	    }
	}
    } else {
	JourneyModelLessThan lt( static_cast<Columns>(sortColumn) );
	for ( int i = 0; i < count; ++i ) {
	    JourneyItem *item = static_cast<JourneyItem*>( m_items.at(i) );
	    if ( lt.operator()(item->journeyInfo(), &journeyInfo) ) {
		insertBefore = i;
		break;
	    }
	}
    }

    beginInsertRows( QModelIndex(), insertBefore, insertBefore );
    JourneyItem *item = new JourneyItem( journeyInfo, &m_infos );
    m_infoToItem.insert( journeyInfo.hash(), item );
    m_items.insert( insertBefore, item );
    item->setModel( this );
    endInsertRows();

    if ( m_nextItem ) {
	if ( item->journeyInfo()->departure()
	   < static_cast<JourneyItem*>(m_nextItem)->journeyInfo()->departure() )
	{
	    m_nextItem = item;
	}
    } else {
	m_nextItem = findNextItem( sortColumn == ColumnDeparture
				   && sortOrder == Qt::AscendingOrder );
    }

    if ( item->journeyInfo()->duration() > m_biggestDuration )
	m_biggestDuration = item->journeyInfo()->duration();
    else if ( item->journeyInfo()->duration() < m_smallestDuration )
	m_smallestDuration = item->journeyInfo()->duration();
    

    return item;
}

JourneyItem* JourneyModel::findNextItem( bool sortedByDepartureAscending ) const {
    if ( m_items.isEmpty() )
	return NULL;

    if ( sortedByDepartureAscending )
	return static_cast<JourneyItem*>( m_items.first() );
    else {
	JourneyItem *earliest = static_cast<JourneyItem*>( m_items.first() );
	for ( int i = 1; i < m_items.count(); ++i ) {
	    JourneyItem *item = static_cast<JourneyItem*>( m_items[i] );
	    if ( item->journeyInfo()->departure() < earliest->journeyInfo()->departure() )
		earliest = item;
	}
	return earliest;
    }
}

DepartureModel::DepartureModel( QObject* parent ) : PublicTransportModel( parent ) {
}

void DepartureModel::update() {
    if ( !m_alarms.isEmpty() ) {
	QDateTime nextAlarm = m_alarms.keys().first();
	int secs = QDateTime::currentDateTime().secsTo( nextAlarm );
	if ( secs < 10 ) {
	    while ( m_alarms.contains(nextAlarm) ) {
		DepartureItem *item = m_alarms.take( nextAlarm );
		fireAlarm( nextAlarm, item );
	    }
	}
    }

    // Sort out departures in the past
    while ( m_nextItem
	&& static_cast<DepartureItem*>(m_nextItem)->departureInfo()->predictedDeparture() <
		QDateTime::currentDateTime() )
    {
	kDebug() << endl << "REMOVE OLD DEPARTURE AT" << m_nextItem->row() << endl
		 << static_cast<DepartureItem*>(m_nextItem)->departureInfo()->predictedDeparture() 
		 << "<" << QDateTime::currentDateTime() << endl;
	removeRows( m_nextItem->row(), 1 );
	m_nextItem = findNextItem();
    }

    if ( m_infos.showRemainingMinutes ) {
	foreach ( ItemBase *item, m_items )
	    item->updateTimeValues();
    }
}

int DepartureModel::columnCount( const QModelIndex& parent ) const {
    return parent.isValid() ? 1 : 3;
}

void DepartureModel::setAlarmSettings( const AlarmSettingsList& alarmSettings ) {
    m_infos.alarmSettings = alarmSettings;

    // Remove old alarms
    QMultiMap< QDateTime, DepartureItem* >::iterator it;
    for ( it = m_alarms.begin(); it != m_alarms.end(); ++it )
	removeAlarm( *it );

    // Set new alarms
    for ( int row = 0; row < m_items.count(); ++row ) {
	for ( int a = 0; a < m_infos.alarmSettings.count(); ++a ) {
	    AlarmSettings alarmSettings = m_infos.alarmSettings.at( a );
	    if ( alarmSettings.enabled
		&& alarmSettings.filter.match(*static_cast<DepartureItem*>(m_items[row])->departureInfo()) )
	    {
		DepartureItem *depItem = static_cast<DepartureItem*>( m_items[row] );
		if ( !depItem->hasAlarm() )
		    addAlarm( depItem );
		
		if ( !depItem->departureInfo()->matchedAlarms().contains(a) )
		    depItem->departureInfo()->matchedAlarms() << a;
		if ( alarmSettings.autoGenerated )
		    depItem->setAlarmStates( depItem->alarmStates() | AlarmIsAutoGenerated );
		if ( alarmSettings.type != AlarmRemoveAfterFirstMatch )
		    depItem->setAlarmStates( depItem->alarmStates() | AlarmIsRecurring );
	    }
	}
    }
}

void DepartureModel::setDepartureArrivalListType(
		DepartureArrivalListType departureArrivalListType ) {
    if ( m_infos.departureArrivalListType == departureArrivalListType )
	return;
    m_infos.departureArrivalListType = departureArrivalListType;
    emit headerDataChanged( Qt::Horizontal, 1, 2 );
}

QVariant DepartureModel::headerData( int section, Qt::Orientation orientation,
				     int role ) const {
    if ( orientation == Qt::Horizontal && role == Qt::DisplayRole ) {
	switch ( section ) {
	    case 0: return i18nc( "A public transport line", "Line" );
	    case 1:
		if ( m_infos.departureArrivalListType == DepartureList )
		    return i18nc("Target of a tramline or busline", "Target");
		else
		    return i18nc("Origin of a tramline or busline", "Origin");
	    case 2:
		if ( m_infos.departureArrivalListType == DepartureList )
		    return i18nc("Time of departure of a tram or bus", "Departure");
		else
		    return i18nc("Time of arrival of a tram or bus", "Arrival");
	}
    }

    return QVariant();
}

QList< uint > DepartureModel::itemHashes() const {
    QList< uint > hashes;
    foreach ( ItemBase *item, m_items )
	hashes << static_cast<DepartureItem*>(item)->departureInfo()->hash();
    return hashes;
}

void DepartureModel::sort( int column, Qt::SortOrder order ) {
    if ( column < 0 || rowCount() == 0 )
	return;

    emit layoutAboutToBeChanged();

    QVector< QPair<DepartureItem*, int> > sortable;
    for ( int row = 0; row < m_items.count(); ++row )
	sortable.append( QPair<DepartureItem*,int>(
		static_cast<DepartureItem*>(m_items[row]), row) );

    if (order == Qt::AscendingOrder) {
	DepartureModelLessThan lt( static_cast<Columns>(column) );
	qStableSort( sortable.begin(), sortable.end(), lt );
    } else {
	DepartureModelGreaterThan gt( static_cast<Columns>(column) );
	qStableSort( sortable.begin(), sortable.end(), gt );
    }
    
    QModelIndexList changedPersistentIndexesFrom, changedPersistentIndexesTo;
    QList< ItemBase* > sorted_children;
    for ( int i = 0; i < m_items.count(); ++i ) {
	int r = sortable.at(i).second;
	ItemBase *item = m_items[r];
	sorted_children << item;

	for ( int c = 0; c < columnCount(); ++c ) {
	    changedPersistentIndexesFrom.append( createIndex(r, c) );
	    changedPersistentIndexesTo.append( createIndex(i, c) );
	}
    }
    
    m_items = sorted_children;
    changePersistentIndexList( changedPersistentIndexesFrom, changedPersistentIndexesTo );
    
    emit layoutChanged();
}

DepartureItem* DepartureModel::findNextItem( bool sortedByDepartureAscending ) const {
    if ( m_items.isEmpty() )
	return NULL;

    if ( sortedByDepartureAscending )
	return static_cast<DepartureItem*>( m_items.first() );
    else {
	DepartureItem *earliest = static_cast<DepartureItem*>( m_items.first() );
	for ( int i = 1; i < m_items.count(); ++i ) {
	    DepartureItem *item = static_cast<DepartureItem*>( m_items[i] );
	    if ( item->departureInfo()->predictedDeparture() <
		    earliest->departureInfo()->predictedDeparture() )
		earliest = item;
	}
	return earliest;
    }
}

DepartureItem *DepartureModel::addItem( const DepartureInfo& departureInfo,
				   Columns sortColumn,
				   Qt::SortOrder sortOrder ) {
    if ( m_infoToItem.contains(departureInfo.hash()) ) {
	kDebug() << "Departure already added to the model";
	return NULL;
    }

    // Find the row where to insert the new departure
    int count = m_items.count();
    int insertBefore = count;
    if ( sortOrder == Qt::AscendingOrder ) {
	DepartureModelGreaterThan gt( static_cast<Columns>(sortColumn) );
	for ( int i = 0; i < count; ++i ) {
	    DepartureItem *item = static_cast<DepartureItem*>( m_items.at(i) );
	    if ( gt.operator()(item->departureInfo(), &departureInfo) ) {
		insertBefore = i;
		break;
	    }
	}
    } else {
	DepartureModelLessThan lt( static_cast<Columns>(sortColumn) );
	for ( int i = 0; i < count; ++i ) {
	    DepartureItem *item = static_cast<DepartureItem*>( m_items.at(i) );
	    if ( lt.operator()(item->departureInfo(), &departureInfo) ) {
		insertBefore = i;
		break;
	    }
	}
    }

    beginInsertRows( QModelIndex(), insertBefore, insertBefore );
    DepartureItem *item = new DepartureItem( departureInfo, &m_infos );
    m_infoToItem.insert( departureInfo.hash(), item );
    m_items.insert( insertBefore, item );
    item->setModel( this );
    endInsertRows();

    if ( m_nextItem ) {
	if ( item->departureInfo()->predictedDeparture() <
		static_cast<DepartureItem*>(m_nextItem)->departureInfo()->predictedDeparture() )
	    m_nextItem = item;
    } else
	m_nextItem = findNextItem( sortColumn == ColumnDeparture
				   && sortOrder == Qt::AscendingOrder );

    
    // Handle alarms
    if ( !departureInfo.matchedAlarms().isEmpty() ) {
	addAlarm( item );

	// Check if there's only one matching autogenerated alarm 
	// and/or at least one recurring alarm
	if ( departureInfo.matchedAlarms().count() == 1 ) {
	    int matchedAlarm = departureInfo.matchedAlarms().first();
	    if ( matchedAlarm < 0 || matchedAlarm >= m_infos.alarmSettings.count() ) {
		kDebug() << "Matched alarm is out of range of current alarm settings";
	    } else {
		AlarmSettings alarmSettings = m_infos.alarmSettings.at( matchedAlarm );
		if ( alarmSettings.autoGenerated )
		    item->setAlarmStates( item->alarmStates() | AlarmIsAutoGenerated );
		if ( alarmSettings.type != AlarmRemoveAfterFirstMatch )
		    item->setAlarmStates( item->alarmStates() | AlarmIsRecurring );
	    }
	} else {
	    for ( int a = 0; a < departureInfo.matchedAlarms().count(); ++a ) {
		int matchedAlarm = departureInfo.matchedAlarms().at( a );
		if ( matchedAlarm < 0 || matchedAlarm >= m_infos.alarmSettings.count() ) {
		    kDebug() << "Matched alarm is out of range of current alarm settings";
		    continue;
		}
		if ( m_infos.alarmSettings.at(matchedAlarm).type != AlarmRemoveAfterFirstMatch ) {
		    item->setAlarmStates( item->alarmStates() | AlarmIsRecurring );
		    break;
		}
	    }
	}
    }

    return item;
}

bool DepartureModel::removeRows( int row, int count, const QModelIndex& parent ) {
    beginRemoveRows( parent, row, row + count - 1 );
    if ( parent.isValid() ) {
	ItemBase *item = itemFromIndex( parent );
	item->removeChildren( row, count );
    } else {
	for ( int i = 0; i < count; ++i ) {
	    DepartureItem *item = static_cast<DepartureItem*>( m_items.takeAt(row) );
	    m_infoToItem.remove( item->departureInfo()->hash() );
	    if ( item->hasAlarm() )
		removeAlarm( item );
	    if ( m_nextItem == item )
		m_nextItem = findNextItem();
	    delete item;
	}
    }
    endRemoveRows();
    
    return true;
}

void DepartureModel::clear() {
    PublicTransportModel::clear();
//     beginRemoveRows( QModelIndex(), 0, m_items.count() );
//     
//     m_infoToItem.clear();
//     qDeleteAll( m_items );
//     m_items.clear();
    m_alarms.clear();
//     m_nextDeparture = NULL;
    
//     endRemoveRows();
}

void DepartureModel::addAlarm( DepartureItem* item ) {
    const QDateTime alarmTime = item->alarmTime();
    if ( QDateTime::currentDateTime() > alarmTime ) {
	fireAlarm( alarmTime, item );
    } else {
	m_alarms.insert( alarmTime, item );
	item->setAlarmStates( (item->alarmStates() & ~AlarmFired) | AlarmPending );
    }
}

void DepartureModel::removeAlarm( DepartureItem* item ) {
    int index = m_alarms.values().indexOf( item );
    if ( index == -1 ) {
	kDebug() << "ALARM NOT FOUND!" << item;
	return;
    }
    int removed = m_alarms.remove( m_alarms.keys().at(index), item );
    if ( removed > 0 )
	item->setAlarmStates( NoAlarm );
}

void DepartureModel::fireAlarm( const QDateTime& dateTime, DepartureItem* item ) {
    if ( item->alarmStates().testFlag(AlarmFired) )
	return; // Don't fire alarms more than once

    kDebug() << "FIRE" << dateTime << item->departureInfo()->matchedAlarms();
    bool fireAlarm = true;
    for ( int i = item->departureInfo()->matchedAlarms().count() - 1; i >= 0; --i ) {
	int matchedAlarm = item->departureInfo()->matchedAlarms().at( i );
	if ( matchedAlarm < 0 || matchedAlarm >= m_infos.alarmSettings.count() ) {
	    kDebug() << "Matched alarm is out of range of current alarm settings";
	    continue;
	}
	
	QDateTime lastFired = m_infos.alarmSettings[ matchedAlarm ].lastFired;
	if ( lastFired.isValid() ) {
	    int secs = lastFired.secsTo( dateTime );
	    kDebug() << "Alarm already fired?" << secs << "seconds from last fired to alarm time.";
	    if ( secs >= 0 )
		fireAlarm = false;
	}
    }
    kDebug() << "Fire alarm?" << fireAlarm;
    if ( !fireAlarm )
	return;

    item->setAlarmStates( (item->alarmStates() & ~(AlarmPending | AlarmIsAutoGenerated)) | AlarmFired );
    emit alarmFired( item );
    
    QList< int > alarmsToRemove;
    for ( int i = item->departureInfo()->matchedAlarms().count() - 1; i >= 0; --i ) {
	int matchedAlarm = item->departureInfo()->matchedAlarms().at( i );
	if ( matchedAlarm < 0 || matchedAlarm >= m_infos.alarmSettings.count() ) {
	    kDebug() << "Matched alarm is out of range of current alarm settings";
	    continue;
	}
	if ( m_infos.alarmSettings[matchedAlarm].type == AlarmRemoveAfterFirstMatch )
	    alarmsToRemove << matchedAlarm;
	m_infos.alarmSettings[ matchedAlarm ].lastFired = QDateTime::currentDateTime();
    }
    kDebug() << "ALARMS TO BE REMOVED" << alarmsToRemove << item->departureInfo()->lineString()
	     << item->departureInfo()->target() << item->departureInfo()->departure();
    if ( !alarmsToRemove.isEmpty() ) {
	foreach ( int i, alarmsToRemove ) // stored in descending order...
	    m_infos.alarmSettings.removeAt( i ); // ...so removing is ok
	emit updateAlarms( m_infos.alarmSettings, alarmsToRemove );
    }
}

