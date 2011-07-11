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

#include "departuremodel.h"
#include "settings.h"

#include <KIconEffect>
#include <KIconLoader>
#include <KDebug>
#include <QTimer>
#include <QPropertyAnimation>
#include <Plasma/Animator>
#include <Plasma/Animation>

class DepartureModelLessThan
{
public:
    inline DepartureModelLessThan( Columns column = ColumnDeparture ) {
        this->column = column;
    };

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
            if ( l->lineNumber() < r->lineNumber() ) {
                return true;
            } else {
                return l->lineString().localeAwareCompare( r->lineString() ) < 0;
            }
        default:
            kDebug() << "Can't sort unknown column" << column;
            return false;
        }
    };
    Columns column;
};

class DepartureModelGreaterThan
{
public:
    inline DepartureModelGreaterThan( Columns column = ColumnDeparture ) {
        this->column = column;
    };

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
            if ( l->lineNumber() > r->lineNumber() ) {
                return true;
            } else {
                return l->lineString().localeAwareCompare( r->lineString() ) > 0;
            }
        default:
            kDebug() << "Can't sort unknown column" << column;
            return false;
        }
    };
    Columns column;
};

class JourneyModelLessThan
{
public:
    inline JourneyModelLessThan( Columns column = ColumnDeparture ) {
        this->column = column;
    };

    inline bool operator()( const QPair<JourneyItem*, int> &l,
                            const QPair<JourneyItem*, int> &r ) const {
        return operator()( l.first->journeyInfo(), r.first->journeyInfo() );
    };

    inline bool operator()( const JourneyInfo* l, const JourneyInfo* r ) const {
        switch ( column ) {
        case ColumnDeparture:
            return l->departure() < r->departure();
        case ColumnArrival:
            return l->arrival() < r->arrival();
        case ColumnTarget:
            return l->duration() < r->duration();
        case ColumnLineString:
            if ( l->vehicleTypes().count() < r->vehicleTypes().count() ) {
                return true;
            }
        default:
            kDebug() << "Can't sort unknown column" << column;
            return false;
        }
    };
    Columns column;
};

class JourneyModelGreaterThan
{
public:
    inline JourneyModelGreaterThan( Columns column = ColumnDeparture ) {
        this->column = column;
    };

    inline bool operator()( const QPair<JourneyItem*, int> &l,
                            const QPair<JourneyItem*, int> &r ) const {
        return operator()( l.first->journeyInfo(), r.first->journeyInfo() );
    };

    inline bool operator()( const JourneyInfo* l, const JourneyInfo* r ) const {
        switch ( column ) {
        case ColumnDeparture:
            return l->departure() > r->departure();
        case ColumnArrival:
            return l->arrival() > r->arrival();
        case ColumnTarget:
            return l->duration() > r->duration();
        case ColumnLineString:
            if ( l->vehicleTypes().count() > r->vehicleTypes().count() ) {
                return true;
            }
        default:
            kDebug() << "Can't sort unknown column" << column;
            return false;
        }
    };
    Columns column;
};

ItemBase::ItemBase( const Info *info ) : m_parent( 0 ), m_model( 0 ), m_info( info )
{
    Q_ASSERT_X( info, "DepartureModelItemBase::DepartureModelItemBase",
                "The pointer to the Info object must be given." );
}

ItemBase::~ItemBase()
{
    qDeleteAll( m_children );
}

void ItemBase::setModel( PublicTransportModel* model )
{
    m_model = model;
    foreach( ChildItem *child, m_children ) {
        child->setModel( model );
    }
}

ItemBase* ItemBase::topLevelParent() const
{
    ItemBase *p = const_cast< ItemBase* >( this );
    while ( p->parent() ) {
        p = p->parent();
    }
    return p;
}

QModelIndex ItemBase::index()
{
    if ( m_model ) {
        return m_model->index( this );
    } else {
        return QModelIndex();
    }
}

ChildItem* ItemBase::childByType( ItemType itemType ) const
{
    foreach( ChildItem *child, m_children ) {
        if ( child->type() == itemType ) {
            return child;
        }
    }
    return NULL;
}

void ItemBase::removeChildren( int first, int count )
{
    if ( first == -1 ) {
        kDebug() << "Not a child of this item";
        return;
    }

    for ( int i = 0; i < count; ++i ) {
        ChildItem *child = m_children.takeAt( first );
        delete child;
    }
}

void ItemBase::removeChild( ChildItem* child )
{
    m_model->removeRow( m_children.indexOf( child ), index() );
}

void ItemBase::appendChild( ChildItem* child )
{
    m_children.append( child );
    child->m_parent = this;
    child->m_model = m_model;
}

TopLevelItem::TopLevelItem( const Info* info ) : QObject(0), ItemBase( info )
{
}

void DepartureItem::setLeavingSoon( bool leavingSoon )
{
    m_leavingSoon = leavingSoon;
    if ( m_model ) {
        m_model->itemChanged( this, 0, 0 );
    }
}

void TopLevelItem::setData( Columns column, const QVariant& data, int role )
{
    m_columnData[ column ][ role ] = data;
    if ( m_model ) {
        m_model->itemChanged( this, column, column );
    }
}

ChildItem::ChildItem( ItemType itemType, const QString& formattedText,
                    const QIcon& icon, const Info *info )
        : ItemBase( info )
{
    m_type = itemType;
    setFormattedText( formattedText );
    setIcon( icon );
}

ChildItem::ChildItem( ItemType itemType, const QString& formattedText, const Info* info )
        : ItemBase( info )
{
    m_type = itemType;
    setFormattedText( formattedText );
}

ChildItem::ChildItem( ItemType itemType, const Info* info )
        : ItemBase( info )
{
    m_type = itemType;
}

QVariant ChildItem::data( int role, int ) const
{
    if ( m_data.contains( role ) ) {
        return m_data.value( role );
    } else if ( role == DrawAlarmBackgroundRole ) {
        ItemBase *p = topLevelParent();
        return p->data( role );
    } else if ( role == FormattedTextRole ) {
        return m_data.value( Qt::DisplayRole );
    } else if ( role == JourneyRatingRole && dynamic_cast<JourneyModel*>( m_model ) ) {
        JourneyItem *topLevelJourneyItem = static_cast<JourneyItem*>( topLevelParent() );
        return topLevelJourneyItem->data( JourneyRatingRole );
    }

    return QVariant();
}

void ChildItem::setData( const QVariant& data, int role )
{
    m_data[ role ] = data;
    if ( m_model ) {
        m_model->itemChanged( this, 0, 0 );
    }
}


JourneyItem::JourneyItem( const JourneyInfo& journeyInfo, const Info* info )
        : TopLevelItem( info )
{
    m_alarm = NoAlarm;
    setJourneyInfo( journeyInfo );
}

int JourneyItem::row() const
{
    if ( m_model ) {
        return m_model->rowFromItem( const_cast<JourneyItem*>( this ) );
    } else {
        return -1;
    }
}

QHash< ItemType, ChildItem* > JourneyItem::typedChildren() const
{
    QHash< ItemType, ChildItem* > children;
    foreach( ChildItem *child, m_children ) {
        if ( child->type() != OtherItem ) {
            children.insert( child->type(), child );
        }
    }

    return children;
}

QVariant JourneyItem::data( int role, int column ) const
{
    if ( column >= m_columnData.count() ) {
        return QVariant();
    }

    if ( column < m_columnData.count() && m_columnData[column].contains(role) ) {
        return m_columnData[column].value( role );
    } else if ( role == DrawAlarmBackgroundRole ) {
        return m_alarm.testFlag( AlarmPending );// || !qFuzzyIsNull( m_alarmColorIntensity );
    } else if ( role == AlarmColorIntensityRole ) {
        return m_alarm.testFlag( AlarmPending ) ? 1.0 : 0.0;//m_alarmColorIntensity;
    } else if ( !m_parent ) {
        switch ( role ) {
        case LinesPerRowRole:
            return m_info->linesPerRow;
        case Qt::TextAlignmentRole:
            return static_cast<int>(( column == 0 ? Qt::AlignRight : Qt::AlignLeft )
                                    | Qt::AlignVCenter );
        case DecorationPositionRole:
            return column == 0 ? DecorationLeft : DecorationRight;

        case FormattedTextRole: // No formatted text defined
            if ( column < m_columnData.count() ) {
                return m_columnData[column].value( Qt::DisplayRole );
            }

        case JourneyRatingRole:
            return rating();

        default:
            return QVariant();
        }
    }

    return QVariant();
}

qreal JourneyItem::rating() const
{
    if ( !m_model ) {
        return 0.5;
    }

    JourneyModel *model = static_cast<JourneyModel*>( m_model );
    int durationSpan = model->biggestDuration() - model->smallestDuration();
    int changesSpan = model->biggestChanges() - model->smallestChanges();

    if (( journeyInfo()->changes() == model->biggestChanges()
            && changesSpan > 4 && model->biggestChanges() >  3 * model->smallestChanges() )
            || ( journeyInfo()->duration() == model->biggestDuration() && durationSpan > 30 ) ) {
        return 1.0;
    }

    qreal durationRating = durationSpan == 0 ? -1.0
                        : qreal( journeyInfo()->duration() - model->smallestDuration() ) / qreal( durationSpan );
    qreal changesRating = changesSpan == 0 ? -1.0
                        : qreal( journeyInfo()->changes() - model->smallestChanges() ) / qreal( changesSpan );

    if ( durationRating == -1.0 ) {
        return changesRating;
    } else if ( changesRating == -1.0 ) {
        return durationRating;
    } else {
        if ( changesRating < 0.1 || changesRating > 0.9 )
            return durationRating * 0.75 + changesRating * 0.25;
        else
            return durationRating;
    }
}

void JourneyItem::setJourneyInfo( const JourneyInfo& journeyInfo )
{
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

void JourneyItem::updateValues()
{
    setIcon( ColumnLineString, GlobalApplet::iconFromVehicleTypeList(
            m_journeyInfo.vehicleTypes().toList(), 32 * m_info->sizeFactor ) );

    QString sDuration = KGlobal::locale()->prettyFormatDuration(
                m_journeyInfo.duration() * 60 * 1000 );
    QString text = i18ncp("@info Text of journey items in an 'info' column",
                        "<emphasis strong='1'>Duration:</emphasis> %2, "
                        "<nobr><emphasis strong='1'>%1</emphasis> change</nobr>",
                        "<emphasis strong='1'>Duration:</emphasis> %2, "
                        "<nobr><emphasis strong='1'>%1</emphasis> changes</nobr>",
                        m_journeyInfo.changes(), sDuration);
    setFormattedText( ColumnJourneyInfo, text );
//     setText( s.replace(QRegExp("<[^>]*>"), "") );
    if ( !m_journeyInfo.journeyNews().isEmpty() ) {
        setIcon( ColumnJourneyInfo, GlobalApplet::makeOverlayIcon(
                    KIcon( "view-pim-news" ), "arrow-down", QSize( 12, 12 ) ) );
    }

    updateTimeValues();

    if ( m_model ) {
        m_model->itemChanged( this, 0, 2 );
    }
}

void JourneyItem::updateChildren()
{
    QHash< ItemType, ChildItem* > children = typedChildren();

    QList< ItemType > types;
    types << DurationItem << ChangesItem << PricingItem << JourneyNewsItem << RouteItem;
    foreach( ItemType type, types ) {
        if ( hasDataForChildType( type ) ) {
            if ( children.contains( type ) ) {
                updateChild( type, children[type] );
            } else {
                appendNewChild( type );
            }
        } else if ( children.contains( type ) ) {
            removeChild( children[type] );
        }
    }
}

void JourneyItem::createChildren()
{
    QList< ItemType > types;
    types << DurationItem << ChangesItem << PricingItem << JourneyNewsItem << RouteItem;
    foreach( ItemType type, types ) {
        if ( hasDataForChildType( type ) ) {
            appendNewChild( type );
        }
    }
}

void JourneyItem::updateChild( ItemType itemType, ChildItem* child )
{
    if ( itemType == RouteItem ) {
        m_model->removeRows( child->row(), 1, child->parent()->index() );
        appendNewChild( RouteItem );
    } else {
        int linesPerRow;
        child->setFormattedText( childItemText( itemType, &linesPerRow ) );
        if ( itemType == JourneyNewsItem || itemType == DelayItem ) {
            child->setData( linesPerRow, LinesPerRowRole );
        }
    }
}

ChildItem* JourneyItem::appendNewChild( ItemType itemType )
{
    ChildItem *child;
    if ( itemType == RouteItem ) {
        child = createRouteItem();
    } else {
        int linesPerRow;
        child = new ChildItem(
                itemType, childItemText( itemType, &linesPerRow ), KIcon(), m_info );
        if ( itemType == JourneyNewsItem || itemType == DelayItem ) {
            child->setData( linesPerRow, LinesPerRowRole );
        }
    }

    appendChild( child );
    return child;
}

void JourneyItem::updateTimeValues()
{
    QString depTextFormatted = m_journeyInfo.departureText( true,
            m_info->displayTimeBold, true, true, m_info->linesPerRow );
    QString oldTextFormatted = formattedText( ColumnDeparture );
    if ( oldTextFormatted != depTextFormatted ) {
        setFormattedText( ColumnDeparture, depTextFormatted );

        QString longestLine;
        QString depText = m_journeyInfo.departureText( false,
                m_info->displayTimeBold, true, true, m_info->linesPerRow );
        if ( m_info->linesPerRow > 1 ) {
            // Get longest line for auto column sizing
            QStringList lines = depText.split( '\n', QString::SkipEmptyParts,
                                            Qt::CaseInsensitive );
            foreach( const QString &sCurrent, lines ) {
                if ( sCurrent.length() > longestLine.length() ) {
                    longestLine = sCurrent;
                }
            }
        } else {
            longestLine = depText;
        }
        setText( ColumnDeparture, longestLine ); // This is just used for auto column sizing
    }

    QString arrTextFormatted = m_journeyInfo.arrivalText( true,
                            m_info->displayTimeBold, true, true, m_info->linesPerRow );
    oldTextFormatted = formattedText( ColumnArrival );
    if ( oldTextFormatted != arrTextFormatted ) {
        setFormattedText( ColumnArrival, arrTextFormatted );

        QString longestLine;
        QString depText = m_journeyInfo.departureText( false,
                        m_info->displayTimeBold, true, true, m_info->linesPerRow );
        if ( m_info->linesPerRow > 1 ) {
            // Get longest line for auto column sizing
            QStringList lines = depText.split( '\n', QString::SkipEmptyParts,
                                            Qt::CaseInsensitive );
            foreach( const QString &sCurrent, lines ) {
                if ( sCurrent.length() > longestLine.length() ) {
                    longestLine = sCurrent;
                }
            }
        } else {
            longestLine = depText;
        }
        setText( ColumnDeparture, longestLine ); // This is just used for auto column sizing
    }

    if ( m_model ) {
        m_model->itemChanged( this, 2, 2 );
    }
}

bool JourneyItem::hasDataForChildType( ItemType itemType )
{
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

QString JourneyItem::childItemText( ItemType itemType, int* linesPerRow )
{
    QString text;
    if ( linesPerRow ) {
        *linesPerRow = 1;
    }
    switch ( itemType ) {
    case JourneyNewsItem:
        text = m_journeyInfo.journeyNews();
        if ( text.startsWith( QLatin1String( "http://" ) ) ) { // TODO: Make the link clickable...
            text = QString( "<a href='%1'>%2</a>" ).arg( text )
                .arg( i18nc( "@info/plain", "Link to journey news" ) );
        }
        text = QString( "<b>%1</b> %2" ).arg( i18nc( "@info/plain News for a journey with public "
                                            "transport, like 'platform changed'", "News:" ) ).arg( text );
        if ( linesPerRow ) {
            *linesPerRow = qMin( 3, text.length() / 25 );
        }
        break;
    case OperatorItem:
        text = QString( "<b>%1</b> %2" )
                .arg( i18nc( "@info/plain The company that is responsible for "
                            "this departure/arrival/journey", "Operator:" ) )
                .arg( m_journeyInfo.operatorName() );
        break;
    case DurationItem:
        if ( m_journeyInfo.duration() <= 0 ) {
            text = QString( "<b>%1</b> %2" )
                    .arg( i18nc( "@info/plain The duration of a journey", "Duration:" ) )
                    .arg( 0 );
        } else {
            text = QString( "<b>%1</b> %2" )
                    .arg( i18nc( "@info/plain The duration of a journey", "Duration:" ) )
                    .arg( GlobalApplet::durationString( m_journeyInfo.duration() * 60 ) );
        }
        break;
    case ChangesItem:
        text = QString( "<b>%1</b> %2" )
                .arg( i18nc( "@info/plain The changes of a journey", "Changes:" ) )
                .arg( m_journeyInfo.changes() );
        break;
    case PricingItem:
        text = QString( "<b>%1</b> %2" )
                .arg( i18nc( "@info/plain The pricing of a journey", "Pricing:" ) )
                .arg( m_journeyInfo.pricing() );
        break;
    case RouteItem:
        if ( m_journeyInfo.routeExactStops() > 0
                && m_journeyInfo.routeExactStops() < m_journeyInfo.routeStops().count() ) {
            text = QString( "<b>%1</b> %2" )
                    .arg( i18nc( "@info/plain The route of this departure/arrival/journey", "Route:" ) )
                    .arg( i18nc( "@info/plain For routes of journey items, if not "
                                "all intermediate stops are known", "> %1 stops",
                                m_journeyInfo.routeStops().count() ) );
        } else {
            text = QString( "<b>%1</b> %2" )
                    .arg( i18nc( "@info/plain The route of this departure/arrival/journey", "Route:" ) )
                    .arg( i18nc( "@info/plain For routes of journey items, if all "
                                "intermediate stops are known", "%1 stops",
                                m_journeyInfo.routeStops().count() ) );
        }
        break;

    case OtherItem:
    default:
        kDebug() << "Wrong item type" << itemType;
        break;
    }

    return text;
}

ChildItem* JourneyItem::createRouteItem()
{
    ChildItem *routeItem = new ChildItem( RouteItem, childItemText( RouteItem ), m_info );

    // Add route stops as child rows				TODO: -1 ?
    for ( int row = 0; row < m_journeyInfo.routeStops().count() - 1; ++row ) {
        // Add a separator item, when the exact route ends
        if ( row == m_journeyInfo.routeExactStops() && row > 0 ) {
            ChildItem *separatorItem = new ChildItem(
                OtherItem, i18nc( "@info/plain Marker for the first place in a list of "
                                  "intermediate stops, where at least one stop has been omitted",
                                  "  - End of exact route -  " ), m_info );
            routeItem->appendChild( separatorItem );
        }

        KIcon icon;
        QString sTransportLine;
        if ( row < m_journeyInfo.routeVehicleTypes().count()
                    && m_journeyInfo.routeVehicleTypes()[row] != Unknown ) {
            icon = GlobalApplet::vehicleTypeToIcon( m_journeyInfo.routeVehicleTypes()[row] );
        }
        if ( m_journeyInfo.routeVehicleTypes()[row] == Feet ) {
            sTransportLine = i18nc( "@info/plain", "Footway" );
        } else if ( m_journeyInfo.routeTransportLines().count() > row ) {
            sTransportLine = m_journeyInfo.routeTransportLines()[row];
        } else {
            icon = KIcon( "public-transport-stop" );
            if ( m_journeyInfo.routeTransportLines().count() > row )
                sTransportLine = m_journeyInfo.routeTransportLines()[row];
        }

        QString stopDep = m_journeyInfo.routeStops()[row];
        QString stopArr = m_journeyInfo.routeStops()[row + 1];
        if ( m_journeyInfo.routePlatformsDeparture().count() > row
                    && !m_journeyInfo.routePlatformsDeparture()[row].isEmpty() ) {
            stopDep = i18nc( "@info/plain", "Platform %1", m_journeyInfo.routePlatformsDeparture()[row] )
                    + " - " + stopDep;
        }
        if ( m_journeyInfo.routePlatformsArrival().count() > row
                    && !m_journeyInfo.routePlatformsArrival()[row].isEmpty() ) {
            stopArr = i18nc( "@info/plain", "Platform %1", m_journeyInfo.routePlatformsArrival()[row] )
                    + " - " + stopArr;
        }

        QString sTimeDep = m_journeyInfo.routeTimesDeparture()[row].toString( "hh:mm" );
        if ( m_journeyInfo.routeTimesDepartureDelay().count() > row ) {
            int delay = m_journeyInfo.routeTimesDepartureDelay()[ row ];
            if ( delay > 0 ) {
                sTimeDep += QString( " <span style='color:%2;'>+%1</span>" )
                            .arg( delay ).arg( Global::textColorDelayed().name() );
            } else if ( delay == 0 ) {
                sTimeDep = sTimeDep.prepend( QString( "<span style='color:%1;'>" )
                                             .arg( Global::textColorOnSchedule().name() ) )
                                .append( "</span>" );
            }
        }

        QString sTimeArr = m_journeyInfo.routeTimesArrival()[row].toString( "hh:mm" );
        if ( m_journeyInfo.routeTimesArrivalDelay().count() > row ) {
            int delay = m_journeyInfo.routeTimesArrivalDelay()[ row ];
            if ( delay > 0 ) {
                sTimeArr += QString( " <span style='color:%2;'>+%1</span>" )
                            .arg( delay ).arg( Global::textColorDelayed().name() );
            } else if ( delay == 0 ) {
                sTimeArr = sTimeArr.prepend( QString( "<span style='color:%1;'>" )
                                             .arg( Global::textColorOnSchedule().name() ) )
                                .append( "</span>" );
            }
        }

        ChildItem *routeStopItem;
        if ( sTransportLine.isEmpty() ) {
            routeStopItem = new ChildItem( OtherItem,
                    i18nc("@info/plain %1 is the departure time, %2 the origin stop name, "
                          "%3 the arrival time, %4 the target stop name.",
                          "dep: %1 - %2<nl/>arr: %3 - %4",
                          sTimeDep, stopDep, sTimeArr, stopArr),
                    icon, m_info );
            routeStopItem->setData( 2, LinesPerRowRole );
        } else {
            routeStopItem = new ChildItem( OtherItem,
                    i18nc("@info/plain %1 is the departure time, %2 the origin stop name, "
                          "%3 the arrival time, %4 the target stop name, %5 the transport line.",
                          "<emphasis strong='1'>%5</emphasis><nl/>dep: %1 - %2<nl/>arr: %3 - %4",
                          sTimeDep, stopDep, sTimeArr, stopArr, sTransportLine),
                    icon, m_info );
            routeStopItem->setData( 3, LinesPerRowRole );
        }

        int iconExtend = 16 * m_info->sizeFactor;
        routeStopItem->setData( QSize(iconExtend, iconExtend), IconSizeRole );

        routeItem->appendChild( routeStopItem );
    }

    return routeItem;
}

DepartureItem::DepartureItem( const DepartureInfo &departureInfo, const Info *info )
        : TopLevelItem( info )
{
    m_leavingSoon = false;
    m_alarm = NoAlarm;
    m_alarmColorIntensity = 0.0;
    setDepartureInfo( departureInfo );
}

int DepartureItem::row() const
{
    if ( m_model ) {
        return m_model->rowFromItem( const_cast<DepartureItem*>(this) );
    } else {
        return -1;
    }
}

QHash< ItemType, ChildItem* > DepartureItem::typedChildren() const
{
    QHash< ItemType, ChildItem* > children;
    foreach( ChildItem *child, m_children ) {
        if ( child->type() != OtherItem ) {
            children.insert( child->type(), child );
        }
    }

    return children;
}

void DepartureItem::setAlarmColorIntensity( qreal alarmColorIntensity )
{
    m_alarmColorIntensity = alarmColorIntensity;
    if ( m_model ) {
        m_model->itemChanged( this, 0, 2 );
    }
}

void DepartureItem::setDepartureInfo( const DepartureInfo &departureInfo )
{
    if ( m_departureInfo.isValid() ) {
        if ( m_departureInfo == departureInfo ) {
            // Unchanged, but matchedAlarms may have changed
            m_departureInfo = departureInfo;
            return;
        }

        m_departureInfo = departureInfo;
        updateValues();
        updateChildren();
    } else {
        m_departureInfo = departureInfo;
        updateValues();
        createChildren();
    }
}

void DepartureItem::updateValues()
{
    setText( ColumnLineString, m_departureInfo.lineString() );
    setFormattedText( ColumnLineString, QString("<span style='font-weight:bold;'>%1</span>")
                    .arg(m_departureInfo.lineString()) );
    // if ( departureInfo.vehicleType() != Unknown )
    setIcon( ColumnLineString, GlobalApplet::vehicleTypeToIcon(m_departureInfo.vehicleType()) );

    setText( ColumnTarget, m_departureInfo.target() );
    if ( !m_departureInfo.journeyNews().isEmpty() ) {
        setIcon( ColumnTarget, GlobalApplet::makeOverlayIcon(KIcon("view-pim-news"),
                "arrow-down", QSize(12, 12)) );
    }

    updateTimeValues();

    if ( m_model ) {
        m_model->itemChanged( this, 0, 2 );
    }
}

void DepartureItem::updateChildren()
{
    QHash< ItemType, ChildItem* > children = typedChildren();
    QList< ItemType > types;
    types << PlatformItem << JourneyNewsItem << DelayItem << OperatorItem << RouteItem;
    foreach( ItemType type, types ) {
        if ( hasDataForChildType(type) ) {
            if ( children.contains(type) ) {
                updateChild( type, children[type] );
            } else {
                appendNewChild( type );
            }
        } else if ( children.contains(type) ) {
            removeChild( children[type] );
        }
    }
}

void DepartureItem::createChildren()
{
    QList< ItemType > types;
    types << PlatformItem << JourneyNewsItem << DelayItem << OperatorItem << RouteItem;
    foreach( ItemType type, types ) {
        if ( hasDataForChildType(type) ) {
            appendNewChild( type );
        }
    }
}

void DepartureItem::updateChild( ItemType itemType, ChildItem* child )
{
    if ( itemType == RouteItem ) {
        m_model->removeRows( child->row(), 1, child->parent()->index() );
        appendNewChild( RouteItem );
    } else {
        int linesPerRow;
        child->setFormattedText( childItemText(itemType, &linesPerRow) );
        if ( itemType == JourneyNewsItem || itemType == DelayItem ) {
            child->setData( linesPerRow, LinesPerRowRole );
        }
    }
}

ChildItem* DepartureItem::appendNewChild( ItemType itemType )
{
    ChildItem *child;
    if ( itemType == RouteItem ) {
        child = createRouteItem();
    } else {
        int linesPerRow;
        child = new ChildItem( itemType, childItemText(itemType, &linesPerRow),
                               KIcon(), m_info );
        if ( itemType == JourneyNewsItem || itemType == DelayItem ) {
            child->setData( linesPerRow, LinesPerRowRole );
        }
    }

    appendChild( child );
    return child;
}

void DepartureItem::updateTimeValues()
{
    QString depTextFormatted = m_departureInfo.departureText( true,
                            m_info->displayTimeBold, m_info->showRemainingMinutes,
                            m_info->showDepartureTime, m_info->linesPerRow );
    QString oldTextFormatted = formattedText( ColumnDeparture );
    if ( oldTextFormatted != depTextFormatted ) {
        setFormattedText( ColumnDeparture, depTextFormatted );

        QString longestLine;
        QString depText = m_departureInfo.departureText( false,
                m_info->displayTimeBold, m_info->showRemainingMinutes,
                m_info->showDepartureTime, m_info->linesPerRow );
        if ( m_info->linesPerRow > 1 ) {
            // Get longest line for auto column sizing
            QStringList lines = depText.split( '\n', QString::SkipEmptyParts,
                                            Qt::CaseInsensitive );
            foreach( const QString &sCurrent, lines ) {
                if ( sCurrent.length() > longestLine.length() ) {
                    longestLine = sCurrent;
                }
            }
        } else
            longestLine = depText;
        setText( ColumnDeparture, longestLine ); // This is just used for auto column sizing
    }

    if ( m_model ) {
        m_model->itemChanged( this, 2, 2 );
    }
}

bool DepartureItem::hasDataForChildType( ItemType itemType )
{
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

QString DepartureItem::childItemText( ItemType itemType, int *linesPerRow )
{
    QString text;
    if ( linesPerRow ) {
        *linesPerRow = 1;
    }
    switch ( itemType ) {
    case PlatformItem:
        text = QString( "<b>%1</b> %2" )
            .arg( i18nc( "@info/plain The platform from which a tram/bus/train departs", "Platform:" ) )
            .arg( m_departureInfo.platform() );
        break;
    case JourneyNewsItem:
        text = m_departureInfo.journeyNews();
        if ( text.startsWith( QLatin1String( "http://" ) ) ) { // TODO: Make the link clickable...
            text = QString( "<a href='%1'>%2</a>" ).arg( text )
                .arg( i18nc("@info/plain Display text for a link to a website with "
                            "journey news for the current journey item",
                            "Link to journey news") );
        }
        text = QString( "<b>%1</b> %2" )
                .arg( i18nc("@info/plain News for a journey with public transport, "
                            "like 'platform changed'", "News:") )
                .arg( text );
        // Try to set enough lines to show all text
        if ( linesPerRow ) {
            *linesPerRow = qMin( 3, text.length() / 25 );
        }
        break;
    case DelayItem:
        text = QString( "<b>%1</b> %2" ).arg( i18nc( "@info/plain Information about delays "
                                            "of a journey with public transport", "Delay:" ) )
            .arg( m_departureInfo.delayText() );
        if ( m_departureInfo.delayType() == Delayed ) {
            text += "<br><b>" + ( m_info->departureArrivalListType == ArrivalList
                                ? i18nc( "@info/plain", "Original arrival time:" )
                                : i18nc( "@info/plain", "Original departure time:" ) ) + "</b> " +
                    m_departureInfo.departure().toString( "hh:mm" );
            // When there's a delay use two lines
            if ( linesPerRow ) {
                *linesPerRow = 2;
            }
        } else if ( linesPerRow ) {
            *linesPerRow = 1;
        }
        break;
    case OperatorItem:
        text = QString( "<b>%1</b> %2" ).arg( i18nc( "@info/plain The company that is "
                                            "responsible for this departure/arrival/journey", "Operator:" ) )
            .arg( m_departureInfo.operatorName() );
        break;
    case RouteItem:
        if ( m_departureInfo.routeExactStops() > 0
                && m_departureInfo.routeExactStops() < m_departureInfo.routeStops().count() ) {
            text = QString( "<b>%1</b> %2" )
                .arg( i18nc( "@info/plain The route of this departure/arrival/journey", "Route:" ) )
                .arg( i18nc( "@info/plain For routes of departure/arrival items, if "
                                "not all intermediate stops are known", "> %1 stops",
                                m_departureInfo.routeStops().count() ) );
        } else {
            text = QString( "<b>%1</b> %2" )
                .arg( i18nc( "@info/plain The route of this departure/arrival/journey", "Route:" ) )
                .arg( i18nc( "@info/plain For routes of departure/arrival items, if "
                                "all intermediate stops are known", "%1 stops",
                                m_departureInfo.routeStops().count() ) );
        }
        break;

    case OtherItem:
    default:
        kDebug() << "Wrong item type" << itemType;
        break;
    }

    return text;
}

ChildItem* DepartureItem::createRouteItem()
{
    ChildItem *routeItem = new ChildItem( RouteItem, childItemText(RouteItem), m_info );

    // Add route stops as child rows
    for ( int row = 0; row < m_departureInfo.routeStops().count(); ++row ) {
        // Add a separator item, when the exact route ends
        // TODO "End of exact route" is "Start of exact route" for arrivals
        if ( m_info->departureArrivalListType == ArrivalList ) {
            // The exact route stops number here means "number of inexact route stops"...
            if ( row == m_departureInfo.routeExactStops() && row > 0 ) {
                ChildItem *separatorItem = new ChildItem( OtherItem,
                        i18nc("@info/plain Marker for the first place in a list of "
                            "intermediate stops, where no stop has been omitted (for arrival lists)",
                            "  - Start of exact route -  "),
                        m_info );
                routeItem->appendChild( separatorItem );
            }
        } else { // if ( m_info->departureArrivalListType == DepartureList ) {
            if ( row == m_departureInfo.routeExactStops() && row > 0 ) {
                ChildItem *separatorItem = new ChildItem( OtherItem,
                        i18nc("@info/plain Marker for the first place in a list of intermediate "
                            "stops, where at least one stop has been omitted (for departure lists)",
                            "  - End of exact route -  "),
                        m_info );
                routeItem->appendChild( separatorItem );
            }
        }

        // Add the current route stop ("departure - stop name")
        QString text = QString( "%1 - %2" )
                        .arg( m_departureInfo.routeTimes()[row].toString("hh:mm") )
                        .arg( m_departureInfo.routeStops()[row] );
        ChildItem *routeStopItem = new ChildItem(
                OtherItem, text, KIcon( "public-transport-stop" ), m_info );
        routeItem->appendChild( routeStopItem );
    }

    return routeItem;
}

QVariant DepartureItem::data( int role, int column ) const
{
    if ( column >= m_columnData.count() ) {
        return QVariant();
    }

    if ( column < m_columnData.count() && m_columnData[column].contains(role) ) {
        return m_columnData[column].value( role );
    } else if ( role == IsLeavingSoonRole ) {
        return m_leavingSoon;
    } else if ( role == DrawAlarmBackgroundRole ) {
        return m_alarm.testFlag( AlarmPending ) || !qFuzzyIsNull( m_alarmColorIntensity );
    } else if ( role == AlarmColorIntensityRole ) {
        return m_alarm.testFlag( AlarmPending ) ? 1.0 : m_alarmColorIntensity;
    } else if ( !m_parent ) {
        // Top level item (m_parent should always be NULL for DepartureItems)
        switch ( role ) {
        case LinesPerRowRole:
            return m_info->linesPerRow;
        case Qt::TextAlignmentRole:
            return static_cast<int>(( column == 0 ? Qt::AlignRight : Qt::AlignLeft )
                                    | Qt::AlignVCenter );
        case DecorationPositionRole:
            return column == 0 ? DecorationLeft : DecorationRight;

        case FormattedTextRole: // No formatted text defined
            if ( column < m_columnData.count() ) {
                return m_columnData[column].value( Qt::DisplayRole );
            }
            break;

        case Qt::BackgroundColorRole: {
            ColorGroupSettingsList colorGroups = static_cast<DepartureModel*>( model() )->colorGroups();
            foreach ( const ColorGroupSettings &colorGroup, colorGroups ) {
                if ( colorGroup.matches(m_departureInfo) ) {
                    return colorGroup.color;
                }
            }
            return Qt::transparent;
        }

        default:
            return QVariant();
        }
    }

    return QVariant();
}

void DepartureItem::setAlarm()
{
    removeAlarm(); // Remove old alarm, if any
    static_cast<DepartureModel*>( m_model )->addAlarm( this );
}

void DepartureItem::removeAlarm()
{
    if ( hasAlarm() ) {
        static_cast<DepartureModel*>( m_model )->removeAlarm( this );
    }
}

void DepartureItem::setAlarmStates( AlarmStates alarmStates )
{ // TODO
    m_alarm = alarmStates;

    if ( alarmStates.testFlag(AlarmPending) ) {
        if ( alarmStates.testFlag(AlarmIsRecurring) ) {
            // Add alarm icon with a recurring-icon as overlay
            setIcon( ColumnDeparture, KIcon("task-reminder", NULL,
                                            QStringList() << "task-recurring") );
        } else {
            // Add alarm icon
            setIcon( ColumnDeparture, KIcon("task-reminder") );
        }
    } else if ( alarmStates == NoAlarm ) {
        // Remove alarm icon
        setIcon( ColumnDeparture, KIcon() );
    } else if ( alarmStates.testFlag(AlarmFired) ) {
        // Add disabled alarm icon
        KIconEffect iconEffect;
        KIcon icon = alarmStates.testFlag( AlarmIsRecurring )
                    ? KIcon( "task-reminder", NULL, QStringList() << "task-recurring" )
                    : KIcon( "task-reminder" );
        QPixmap pixmap = iconEffect.apply( icon.pixmap( 16 * m_info->sizeFactor ),
                                           KIconLoader::Small, KIconLoader::DisabledState );
        KIcon disabledAlarmIcon;
        disabledAlarmIcon.addPixmap( pixmap, QIcon::Normal );
        setIcon( ColumnDeparture, disabledAlarmIcon );
    }

    m_model->itemChanged( this, 0, 2 );
    m_model->childrenChanged( this ); // Children inherit the alarm background
}

void JourneyItem::setAlarmStates(AlarmStates alarmStates)
{ // TODO
    m_alarm = alarmStates;

    if ( alarmStates.testFlag(AlarmPending) ) {
        if ( alarmStates.testFlag(AlarmIsRecurring) ) {
            // Add alarm icon with a recurring-icon as overlay
            setIcon( ColumnDeparture, KIcon("task-reminder", NULL,
                                            QStringList() << "task-recurring") );
        } else {
            // Add alarm icon
            setIcon( ColumnDeparture, KIcon("task-reminder") );
        }
    } else if ( alarmStates == NoAlarm ) {
        // Remove alarm icon
        setIcon( ColumnDeparture, KIcon() );
    } else if ( alarmStates.testFlag(AlarmFired) ) {
        // Add disabled alarm icon
        KIconEffect iconEffect;
        KIcon icon = alarmStates.testFlag( AlarmIsRecurring )
                    ? KIcon( "task-reminder", NULL, QStringList() << "task-recurring" )
                    : KIcon( "task-reminder" );
        QPixmap pixmap = iconEffect.apply( icon.pixmap( 16 * m_info->sizeFactor ),
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
        m_updateTimer( new QTimer(this) )
{
    m_updateTimer->setInterval( 60000 );
    connect( m_updateTimer, SIGNAL(timeout()), this, SLOT(update()) );

    callAtNextFullMinute( SLOT(startUpdateTimer()) );
}

void PublicTransportModel::startUpdateTimer()
{
    update();

    kDebug() << "start update timer" << QTime::currentTime();
    m_updateTimer->start();
}

void PublicTransportModel::callAtNextFullMinute( const char* member )
{
    QTime time = QTime::currentTime();
    QTime nextMinute( time.hour(), time.minute() );
    nextMinute = nextMinute.addSecs( 60 );
    int msecs = time.secsTo( nextMinute ) * 1000;
    QTimer::singleShot( qMin(60000, msecs), this, member );
}

void PublicTransportModel::setLinesPerRow( int linesPerRow )
{
    if ( m_info.linesPerRow == linesPerRow ) {
        return;
    }
    m_info.linesPerRow = linesPerRow;
    emit dataChanged( index(0, 0), index(rowCount(), 0) );
}

void PublicTransportModel::setSizeFactor( float sizeFactor )
{
    m_info.sizeFactor = sizeFactor;
    //     TODO: Update items?
}

void PublicTransportModel::setDepartureColumnSettings( bool displayTimeBold,
        bool showRemainingMinutes, bool showDepartureTime )
{
    m_info.displayTimeBold = displayTimeBold;
    m_info.showRemainingMinutes = showRemainingMinutes;
    m_info.showDepartureTime = showDepartureTime;
    foreach( ItemBase *item, m_items ) {
        item->updateTimeValues();
    }
}

QModelIndex PublicTransportModel::index( int row, int column,
        const QModelIndex& parent ) const
{
    if ( parent.isValid() ) {
        if ( !hasIndex(row, column, parent) ) {
            return QModelIndex();
        }

        ItemBase *parentItem = static_cast<ItemBase*>( parent.internalPointer() );
        if ( row < parentItem->childCount() ) {
            return createIndex( row, column, parentItem->child(row) );
        } else {
            return QModelIndex();
        }
    } else {
        if ( !hasIndex(row, column, QModelIndex()) ) {
            return QModelIndex();
        }

        if ( row >= 0 && row < m_items.count() ) {
            return createIndex( row, column, m_items[row] );
        } else {
            return QModelIndex();
        }
    }
}

QModelIndex PublicTransportModel::parent( const QModelIndex& child ) const
{
    if ( !child.isValid() ) {
        return QModelIndex();
    }

    ItemBase *childItem = static_cast<ItemBase*>( child.internalPointer() );
    if ( !childItem ) {
        return QModelIndex();
    }

    ItemBase *parent = childItem->parent();
    if ( parent ) {
        return createIndex( parent->row(), 0, parent );
    } else {
        return QModelIndex();
    }
}

int PublicTransportModel::rowCount( const QModelIndex& parent ) const
{
    if ( parent.column() > 0 ) {
        return 0;
    }

    if ( parent.isValid() ) {
        ItemBase *parentItem = static_cast<ItemBase*>( parent.internalPointer() );
        return parentItem->childCount();
    } else {
        return m_items.count();
    }
}

ItemBase* PublicTransportModel::itemFromIndex( const QModelIndex& index ) const
{
    return static_cast<ItemBase*>( index.internalPointer() );
}

int PublicTransportModel::rowFromItem( ItemBase* item )
{
    return m_items.indexOf( item );
}

QModelIndex PublicTransportModel::indexFromItem( ItemBase* item, int column ) const
{
    return item ? createIndex( item->row(), column, item ) : QModelIndex();
}

void PublicTransportModel::itemChanged( ItemBase* item, int columnLeft, int columnRight )
{
    if ( columnLeft == columnRight ) {
        QModelIndex index = indexFromItem( item, columnLeft );
        if ( !index.isValid() ) {
            kDebug() << "The given item is not in the model";
        } else {
            emit dataChanged( index, index );
        }
    } else {
        QModelIndex indexLeft = indexFromItem( item, columnLeft );
        QModelIndex indexRight = indexFromItem( item, columnRight );
        if ( !indexLeft.isValid() ) {
            kDebug() << "The given item is not in the model";
        } else {
            emit dataChanged( indexLeft, indexRight );
        }
    }
}

void PublicTransportModel::childrenChanged( ItemBase* parentItem )
{
    if ( !parentItem->children().isEmpty() ) {
        QModelIndex indexFirst = indexFromItem( parentItem->children().first() );
        QModelIndex indexLast = indexFromItem( parentItem->children().last() );
        emit dataChanged( indexFirst, indexLast );

        foreach( ChildItem *child, parentItem->children() ) {
            childrenChanged( child );
        }
    }
}

QVariant PublicTransportModel::data( const QModelIndex& index, int role ) const
{
    if ( !index.isValid() ) {
        return QVariant();
    }

    ItemBase *item = static_cast<ItemBase*>( index.internalPointer() );
    return item->data( role, index.column() );
}

void PublicTransportModel::clear()
{
    emit journeysAboutToBeRemoved( m_items );

    beginRemoveRows( QModelIndex(), 0, m_items.count() );

    m_infoToItem.clear();
    qDeleteAll( m_items );
    m_items.clear();
    m_nextItem = NULL;

    endRemoveRows();
}

JourneyModel::JourneyModel( QObject* parent ) : PublicTransportModel( parent )
{
    m_smallestDuration = 999999;
    m_biggestDuration = 0;
    m_smallestChanges = 999999;
    m_biggestChanges = 0;
}

int JourneyModel::columnCount( const QModelIndex& parent ) const
{
    return parent.isValid() ? 1 : 4;
}

QVariant JourneyModel::headerData( int section, Qt::Orientation orientation,
                                int role ) const
{
    if ( orientation == Qt::Horizontal && role == Qt::DisplayRole ) {
        switch ( section ) {
        case 0: return i18nc( "@title:column A public transport line", "Line" );
        case 1: return i18nc( "@title:column Information about a journey with public transport", "Information" );
        case 2: return i18nc( "@title:column Time of departure of a tram or bus", "Departure" );
        case 3: return i18nc( "@title:column Time of arrival of a tram or bus", "Arrival" );
        }
    }

    return QVariant();
}

void JourneyModel::setDepartureArrivalListType( DepartureArrivalListType departureArrivalListType )
{
    m_info.departureArrivalListType = departureArrivalListType;
}

void JourneyModel::setCurrentStopIndex( int currentStopSettingsIndex )
{
    m_info.currentStopSettingsIndex = currentStopSettingsIndex;
}

void JourneyModel::setAlarmSettings( const AlarmSettingsList& alarmSettings )
{
    m_info.alarmSettings = alarmSettings;

    for ( int row = 0; row < m_items.count(); ++row ) {
        JourneyItem *journeyItem = static_cast<JourneyItem*>( m_items[row] );
        updateItemAlarm( journeyItem );
    }
}

void JourneyModel::updateItemAlarm( JourneyItem* journeyItem )
{
    // Store old alarm states
    AlarmStates oldAlarmStates = journeyItem->alarmStates();
    
    // Use a dummy DepartureInfo that "mimics" the first journey part of the current journey
    JourneyInfo journeyInfo = *journeyItem->journeyInfo();
    QString lineString = journeyInfo.routeTransportLines().isEmpty()
            ? QString() : journeyInfo.routeTransportLines().first();
    VehicleType vehicleType = journeyInfo.routeVehicleTypes().isEmpty()
            ? Unknown : journeyInfo.routeVehicleTypes().first();
    DepartureInfo departureInfo( QString(), lineString, QString(),
                                 journeyInfo.departure(), vehicleType );
    AlarmStates alarmStates = NoAlarm;
    for ( int a = 0; a < m_info.alarmSettings.count(); ++a ) {
        AlarmSettings alarmSettings = m_info.alarmSettings[ a ];

        // Remove target constraints from the alarm filter (because the target is unknown)
        Filter alarmFilter = alarmSettings.filter;
        for ( int i = 0; i < alarmFilter.count(); ++i ) {
            if ( alarmFilter[i].type == Timetable::FilterByTarget ) {
                alarmFilter.removeAt( i );
                break;
            }
        }

        if ( alarmSettings.affectedStops.contains(m_info.currentStopSettingsIndex)
                && alarmSettings.enabled && !alarmFilter.isEmpty()
                && alarmFilter.match(departureInfo) )
        {
            const QDateTime alarmTime = journeyItem->alarmTime();
            if ( QDateTime::currentDateTime() > alarmTime ) {
                alarmStates |= AlarmFired;
            } else {
                alarmStates |= AlarmPending;
            }
            if ( alarmSettings.autoGenerated ) {
                alarmStates |= AlarmIsAutoGenerated;
            }
            if ( alarmSettings.type == AlarmApplyToNewDepartures ) {
                alarmStates |= AlarmIsRecurring;
            }
            break;
        }
    }

    if ( oldAlarmStates != alarmStates ) {
        journeyItem->setAlarmStates( alarmStates );
    }
}

bool JourneyModel::removeRows( int row, int count, const QModelIndex& parent )
{
    beginRemoveRows( parent, row, row + count - 1 );
    if ( parent.isValid() ) {
        ItemBase *item = itemFromIndex( parent );
        item->removeChildren( row, count );
    } else {
        for ( int i = 0; i < count; ++i ) {
            JourneyItem *item = static_cast<JourneyItem*>( m_items[row] );
            emit journeysAboutToBeRemoved( QList<ItemBase*>() << item );

            m_items.removeAt( row );
            m_infoToItem.remove( item->journeyInfo()->hash() );
            if ( m_nextItem == item ) {
                m_nextItem = findNextItem();
            }
// 	    	if ( item->journeyInfo()->duration() == m_biggestDuration )
                // TODO Find new biggest duration item
// 	    	else if ( item->journeyInfo()->duration() == m_smallestDuration )
                // TODO Find new smallest duration item
            delete item;
        }
    }

    if ( isEmpty() ) {
        m_smallestDuration = 999999;
        m_biggestDuration = 0;
        m_smallestChanges = 999999;
        m_biggestChanges = 0;
    }
    endRemoveRows();

    return true;
}

void JourneyModel::clear()
{
    PublicTransportModel::clear();

    m_smallestDuration = 999999;
    m_biggestDuration = 0;
    m_smallestChanges = 999999;
    m_biggestChanges = 0;
}

void JourneyModel::update()
{
//     if ( m_info.showRemainingMinutes ) {
    foreach( ItemBase *item, m_items ) {
        item->updateTimeValues();
    }
//     }
}

void JourneyModel::sort( int column, Qt::SortOrder order )
{
    if ( column < 0 || rowCount() == 0 ) {
        return;
    }

    emit layoutAboutToBeChanged();

    QVector< QPair<JourneyItem*, int> > sortable;
    for ( int row = 0; row < m_items.count(); ++row )
        sortable.append( QPair<JourneyItem*, int>(
                            static_cast<JourneyItem*>( m_items[row] ), row ) );

    if ( order == Qt::AscendingOrder ) {
        JourneyModelLessThan lt( static_cast<Columns>( column ) );
        qStableSort( sortable.begin(), sortable.end(), lt );
    } else {
        JourneyModelGreaterThan gt( static_cast<Columns>( column ) );
        qStableSort( sortable.begin(), sortable.end(), gt );
    }

    QModelIndexList changedPersistentIndexesFrom, changedPersistentIndexesTo;
    QList< ItemBase* > sorted_children;
    for ( int i = 0; i < m_items.count(); ++i ) {
        int r = sortable.at( i ).second;
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
                                    Columns sortColumn, Qt::SortOrder sortOrder )
{
    ItemBase *existingItem = m_infoToItem.value( journeyInfo.hash(), NULL );
    if ( existingItem ) {
        kDebug() << "Journey already added to the model" << journeyInfo;
        return static_cast<JourneyItem*>( existingItem );
    }

    // Find the row where to insert the new journey
    int count = m_items.count();
    int insertBefore = count;
    if ( sortOrder == Qt::AscendingOrder ) {
        JourneyModelGreaterThan gt( static_cast<Columns>( sortColumn ) );
        for ( int i = 0; i < count; ++i ) {
            JourneyItem *item = static_cast<JourneyItem*>( m_items.at( i ) );
            if ( gt.operator()( item->journeyInfo(), &journeyInfo ) ) {
                insertBefore = i;
                break;
            }
        }
    } else {
        JourneyModelLessThan lt( static_cast<Columns>( sortColumn ) );
        for ( int i = 0; i < count; ++i ) {
            JourneyItem *item = static_cast<JourneyItem*>( m_items.at( i ) );
            if ( lt.operator()( item->journeyInfo(), &journeyInfo ) ) {
                insertBefore = i;
                break;
            }
        }
    }

    beginInsertRows( QModelIndex(), insertBefore, insertBefore );
    JourneyItem *item = new JourneyItem( journeyInfo, &m_info );
    m_infoToItem.insert( journeyInfo.hash(), item );
    m_items.insert( insertBefore, item );
    item->setModel( this );
    endInsertRows();

    // Update next departing journey
    if ( m_nextItem ) {
        if ( item->journeyInfo()->departure()
                < static_cast<JourneyItem*>( m_nextItem )->journeyInfo()->departure() ) {
            m_nextItem = item;
        }
    } else {
        m_nextItem = findNextItem( sortColumn == ColumnDeparture
                                && sortOrder == Qt::AscendingOrder );
    }

    // Update biggest/smallest duration and changes for rating of journeys
    if ( item->journeyInfo()->duration() > m_biggestDuration ) {
        m_biggestDuration = item->journeyInfo()->duration();
    } else if ( item->journeyInfo()->duration() < m_smallestDuration ) {
        m_smallestDuration = item->journeyInfo()->duration();
    }

    if ( item->journeyInfo()->changes() > m_biggestChanges ) {
        m_biggestChanges = item->journeyInfo()->changes();
    } else if ( item->journeyInfo()->changes() < m_smallestChanges ) {
        m_smallestChanges = item->journeyInfo()->changes();
    }

    // Set alarm flags
    updateItemAlarm( item );

    return item;
}

JourneyItem* JourneyModel::findNextItem( bool sortedByDepartureAscending ) const
{
    if ( m_items.isEmpty() ) {
        return NULL;
    }

    if ( sortedByDepartureAscending ) {
        return static_cast<JourneyItem*>( m_items.first() );
    } else {
        JourneyItem *earliest = static_cast<JourneyItem*>( m_items.first() );
        for ( int i = 1; i < m_items.count(); ++i ) {
            JourneyItem *item = static_cast<JourneyItem*>( m_items[i] );
            if ( item->journeyInfo()->departure() < earliest->journeyInfo()->departure() ) {
                earliest = item;
            }
        }
        return earliest;
    }
}

DepartureModel::DepartureModel( QObject* parent ) : PublicTransportModel( parent )
{
}

void DepartureModel::update()
{
    // Check for alarms that should now be fired
    if ( !m_alarms.isEmpty() ) {
        QDateTime nextAlarm = m_alarms.keys().first();
        int secs = QDateTime::currentDateTime().secsTo( nextAlarm );
        if ( secs < 10 ) {
            while ( m_alarms.contains( nextAlarm ) ) {
                DepartureItem *item = m_alarms.take( nextAlarm );
                fireAlarm( nextAlarm, item );
            }
        }
    }

    // Sort out departures in the past
    QList<DepartureInfo> leaving;
    int row = 0;
    m_nextItem = m_items.isEmpty() ? NULL : static_cast<DepartureItem*>( m_items[row] );
    QDateTime nextDeparture = m_nextItem
            ? static_cast<DepartureItem*>(m_nextItem)->departureInfo()->predictedDeparture()
            : QDateTime();
    nextDeparture.setTime( QTime(nextDeparture.time().hour(), nextDeparture.time().minute()) ); // Set second to 0
    while ( m_nextItem && nextDeparture < QDateTime::currentDateTime() ) {
        DepartureItem *leavingItem = static_cast<DepartureItem*>( m_nextItem );
        leaving << *leavingItem->departureInfo();
        leavingItem->setLeavingSoon( true );

// 		removeRows( m_nextItem->row(), 1 );
// 		m_nextItem = findNextItem();
        ++row;
        if ( row >= m_items.count() ) {
            break;
        }
        m_nextItem = static_cast<DepartureItem*>( m_items[row] );
        nextDeparture = static_cast<DepartureItem*>(m_nextItem)->departureInfo()->predictedDeparture();
        nextDeparture.setTime( QTime(nextDeparture.time().hour(), nextDeparture.time().minute()) ); // Set second to 0
    }
    emit departuresLeft( leaving );

    // Wait 10 seconds before removing the departure.
    // By having called setLeavingSoon(true) the items to be removed will animate to indicate that
    // they are leaving soon.
    QTimer::singleShot( 10000, this, SLOT(removeLeavingDepartures()) );

    // Update departure column if necessary (remaining minutes)
    if ( m_info.showRemainingMinutes ) {
        foreach( ItemBase *item, m_items ) {
            item->updateTimeValues();
        }
    }
}

void DepartureModel::removeLeavingDepartures()
{
    for ( int row = 0; row < m_items.count(); ++row ) {
        DepartureItem *item = static_cast<DepartureItem*>( m_items[row] );
        if ( item->isLeavingSoon() ) {
            removeRows( row, 1 );
            --row;
        } else {
            break;
        }
    }
}

int DepartureModel::columnCount( const QModelIndex& parent ) const
{
    // Top level items have three columns (line, target, departure),
    // child items have only one stretched column
    return parent.isValid() ? 1 : 3;
}

void DepartureModel::setAlarmSettings( const AlarmSettingsList& alarmSettings )
{
    m_info.alarmSettings = alarmSettings;

    // Remove old alarms
    QMultiMap< QDateTime, DepartureItem* >::iterator it;
    for ( it = m_alarms.begin(); it != m_alarms.end(); ++it ) {
        disconnect( *it, SIGNAL(destroyed(QObject*)), this, SLOT(alarmItemDestroyed(QObject*)) );
        (*it)->setAlarmStates( NoAlarm );

        it = m_alarms.erase( it );
    }

    // Set new alarms (go through all alarm settings for all departures)
    for ( int row = 0; row < m_items.count(); ++row ) {
        for ( int a = 0; a < m_info.alarmSettings.count(); ++a ) {
            AlarmSettings alarmSettings = m_info.alarmSettings.at( a );
            if ( alarmSettings.enabled
                    && alarmSettings.filter.match(
                        *static_cast<DepartureItem*>(m_items[row])->departureInfo() ) )
            {
                // Current alarm is enabled and matches the current departure
                DepartureItem *depItem = static_cast<DepartureItem*>( m_items[row] );
                if ( !depItem->hasAlarm() ) {
                    addAlarm( depItem );
                }

                if ( !depItem->departureInfo()->matchedAlarms().contains(a) ) {
                    depItem->departureInfo()->matchedAlarms() << a;
                }
                if ( alarmSettings.autoGenerated ) {
                    depItem->setAlarmStates( depItem->alarmStates() | AlarmIsAutoGenerated );
                }
                if ( alarmSettings.type != AlarmRemoveAfterFirstMatch ) {
                    depItem->setAlarmStates( depItem->alarmStates() | AlarmIsRecurring );
                }
            }
        }
    }
}

void DepartureModel::setDepartureArrivalListType( DepartureArrivalListType departureArrivalListType )
{
    if ( m_info.departureArrivalListType == departureArrivalListType ) {
        return;
    }
    m_info.departureArrivalListType = departureArrivalListType;
    emit headerDataChanged( Qt::Horizontal, 1, 2 );
}

void DepartureModel::setCurrentStopIndex(int currentStopSettingsIndex)
{
    m_info.currentStopSettingsIndex = currentStopSettingsIndex;
}

QVariant DepartureModel::headerData( int section, Qt::Orientation orientation,
                                    int role ) const
{
    if ( orientation == Qt::Horizontal && role == Qt::DisplayRole ) {
        switch ( section ) {
        case 0: return i18nc( "@title:column A public transport line", "Line" );
        case 1:
            if ( m_info.departureArrivalListType == DepartureList ) {
                return i18nc( "@title:column Target of a tramline or busline", "Target" );
            } else {
                return i18nc( "@title:column Origin of a tramline or busline", "Origin" );
            }
        case 2:
            if ( m_info.departureArrivalListType == DepartureList ) {
                return i18nc( "@title:column Time of departure of a tram or bus", "Departure" );
            } else {
                return i18nc( "@title:column Time of arrival of a tram or bus", "Arrival" );
            }
        }
    }

    return QVariant();
}

QList< uint > DepartureModel::itemHashes() const
{
    QList< uint > hashes;
    foreach( ItemBase *item, m_items ) {
        hashes << static_cast<DepartureItem*>( item )->departureInfo()->hash();
    }
    return hashes;
}

QList< DepartureInfo > DepartureModel::departureInfos() const
{
    QList< DepartureInfo > infoList;
    foreach( ItemBase *item, m_items ) {
        infoList << *static_cast<DepartureItem*>( item )->departureInfo();
    }
    return infoList;
}

QStringList DepartureModel::allStopNames( int maxDepartureCount ) const
{
    QStringList stopNames;
    for ( int i = 0; i < m_items.count() && (maxDepartureCount == -1 || i <= maxDepartureCount); ++i ) {
        const DepartureInfo *info = static_cast<DepartureItem*>( m_items[i] )->departureInfo();
        stopNames << info->target();
        stopNames << info->routeStops();
    }
    stopNames.removeDuplicates();
    return stopNames;
}

void DepartureModel::sort( int column, Qt::SortOrder order )
{
    if ( column < 0 || rowCount() == 0 ) {
        return;
    }

    emit layoutAboutToBeChanged();

    QVector< QPair<DepartureItem*, int> > sortable;
    for ( int row = 0; row < m_items.count(); ++row ) {
        sortable.append( QPair<DepartureItem*, int>(static_cast<DepartureItem*>(m_items[row]), row) );
    }

    if ( order == Qt::AscendingOrder ) {
        DepartureModelLessThan lt( static_cast<Columns>( column ) );
        qStableSort( sortable.begin(), sortable.end(), lt );
    } else {
        DepartureModelGreaterThan gt( static_cast<Columns>( column ) );
        qStableSort( sortable.begin(), sortable.end(), gt );
    }

    QModelIndexList changedPersistentIndexesFrom, changedPersistentIndexesTo;
    QList< ItemBase* > sorted_children;
    for ( int i = 0; i < m_items.count(); ++i ) {
        int r = sortable.at( i ).second;
        ItemBase *item = m_items[r];
        sorted_children << item;

        // Store changed persistent indices
        for ( int c = 0; c < columnCount(); ++c ) {
            changedPersistentIndexesFrom.append( createIndex( r, c ) );
            changedPersistentIndexesTo.append( createIndex( i, c ) );
        }
    }

    m_items = sorted_children;
    changePersistentIndexList( changedPersistentIndexesFrom, changedPersistentIndexesTo );

    emit layoutChanged();
}

DepartureItem* DepartureModel::findNextItem( bool sortedByDepartureAscending ) const
{
    if ( m_items.isEmpty() ) {
        return NULL;
    }

    if ( sortedByDepartureAscending ) {
        return static_cast<DepartureItem*>( m_items.first() );
    } else {
        // Find the next departing item
        DepartureItem *earliest = static_cast<DepartureItem*>( m_items.first() );
        for ( int i = 1; i < m_items.count(); ++i ) {
            DepartureItem *item = static_cast<DepartureItem*>( m_items[i] );
            if ( item->departureInfo()->predictedDeparture() <
                    earliest->departureInfo()->predictedDeparture() ) {
                earliest = item;
            }
        }
        return earliest;
    }
}

DepartureItem *DepartureModel::addItem( const DepartureInfo& departureInfo,
                                        Columns sortColumn,
                                        Qt::SortOrder sortOrder )
{
    ItemBase *existingItem = m_infoToItem.value( departureInfo.hash(), NULL );
    if ( existingItem ) {
        kDebug() << "Departure already added to the model at index" << departureInfo;
        return static_cast<DepartureItem*>( existingItem );
    }

    // Find the row where to insert the new departure
    int count = m_items.count();
    int insertBefore = count;
    if ( sortOrder == Qt::AscendingOrder ) {
        DepartureModelGreaterThan gt( static_cast<Columns>( sortColumn ) );
        for ( int i = 0; i < count; ++i ) {
            DepartureItem *item = static_cast<DepartureItem*>( m_items.at(i) );
            if ( gt.operator()(item->departureInfo(), &departureInfo) ) {
                insertBefore = i;
                break;
            }
        }
    } else {
        DepartureModelLessThan lt( static_cast<Columns>( sortColumn ) );
        for ( int i = 0; i < count; ++i ) {
            DepartureItem *item = static_cast<DepartureItem*>( m_items.at(i) );
            if ( lt.operator()(item->departureInfo(), &departureInfo) ) {
                insertBefore = i;
                break;
            }
        }
    }

    beginInsertRows( QModelIndex(), insertBefore, insertBefore );
    DepartureItem *newItem = new DepartureItem( departureInfo, &m_info );
    m_infoToItem.insert( departureInfo.hash(), newItem );
    m_items.insert( insertBefore, newItem );
    newItem->setModel( this );
    endInsertRows();

    if ( m_nextItem ) {
        if ( newItem->departureInfo()->predictedDeparture() <
             static_cast<DepartureItem*>(m_nextItem)->departureInfo()->predictedDeparture() )
        {
            m_nextItem = newItem;
        }
    } else {
        m_nextItem = findNextItem( sortColumn == ColumnDeparture
                                && sortOrder == Qt::AscendingOrder );
    }

    // Handle alarms
    if ( !departureInfo.matchedAlarms().isEmpty() ) {
        addAlarm( newItem );

        // Check if there's only one matching autogenerated alarm
        // and/or at least one recurring alarm
        if ( departureInfo.matchedAlarms().count() == 1 ) {
            int matchedAlarm = departureInfo.matchedAlarms().first();
            if ( matchedAlarm < 0 || matchedAlarm >= m_info.alarmSettings.count() ) {
                kDebug() << "Matched alarm is out of range of current alarm settings" << matchedAlarm;
            } else {
                AlarmSettings alarmSettings = m_info.alarmSettings.at( matchedAlarm );
                if ( alarmSettings.autoGenerated ) {
                    newItem->setAlarmStates( newItem->alarmStates() | AlarmIsAutoGenerated );
                }
                if ( alarmSettings.type != AlarmRemoveAfterFirstMatch ) {
                    newItem->setAlarmStates( newItem->alarmStates() | AlarmIsRecurring );
                }
            }
        } else {
            for ( int a = 0; a < departureInfo.matchedAlarms().count(); ++a ) {
                int matchedAlarm = departureInfo.matchedAlarms().at( a );
                if ( matchedAlarm < 0 || matchedAlarm >= m_info.alarmSettings.count() ) {
                    kDebug() << "Matched alarm is out of range of current alarm settings" << matchedAlarm;
                    continue;
                }
                if ( m_info.alarmSettings.at( matchedAlarm ).type != AlarmRemoveAfterFirstMatch ) {
                    newItem->setAlarmStates( newItem->alarmStates() | AlarmIsRecurring );
                    break;
                }
            }
        }
    }

    return newItem;
}

bool DepartureModel::removeRows( int row, int count, const QModelIndex& parent )
{
    beginRemoveRows( parent, row, row + count - 1 );
    if ( parent.isValid() ) {
        ItemBase *item = itemFromIndex( parent );
        item->removeChildren( row, count );
    } else {
        for ( int i = 0; i < count; ++i ) {
            DepartureItem *item = static_cast<DepartureItem*>( m_items[row] );
            emit journeysAboutToBeRemoved( QList<ItemBase*>() << item );

            m_items.removeAt( row );
            item->removeChildren( 0, item->childCount() ); // Needed?
            m_infoToItem.remove( item->departureInfo()->hash() );
            if ( item->hasAlarm() ) {
                removeAlarm( item );
            }
            if ( m_nextItem == item ) {
                m_nextItem = findNextItem();
            }
            delete item;
        }
    }
    endRemoveRows();

    return true;
}

void DepartureModel::clear()
{
    PublicTransportModel::clear();
    m_alarms.clear();
}

void DepartureModel::alarmItemDestroyed( QObject* item )
{
    DepartureItem *depItem = qobject_cast< DepartureItem* >( item );
    int index;
    while ( (index = m_alarms.values().indexOf(depItem)) != -1 ) {
        m_alarms.remove( m_alarms.keys().at(index), depItem );
    }
}

void DepartureModel::addAlarm( DepartureItem* item )
{
    const QDateTime alarmTime = item->alarmTime();
    if ( QDateTime::currentDateTime() > alarmTime ) {
        fireAlarm( alarmTime, item );
    } else {
        connect( item, SIGNAL(destroyed(QObject*)), this, SLOT(alarmItemDestroyed(QObject*)) );
        m_alarms.insert( alarmTime, item );
        item->setAlarmStates( (item->alarmStates() & ~AlarmFired) | AlarmPending );
    }
}

void DepartureModel::removeAlarm( DepartureItem* item )
{
    int index = m_alarms.values().indexOf( item );
    if ( index == -1 ) {
        kDebug() << "Alarm not found!";
        return;
    }
    int removed = m_alarms.remove( m_alarms.keys().at( index ), item );
    if ( removed > 0 ) {
        disconnect( item, SIGNAL(destroyed(QObject*)), this, SLOT(alarmItemDestroyed(QObject*)) );
        item->setAlarmStates( NoAlarm );
    }
}

void DepartureModel::fireAlarm( const QDateTime& dateTime, DepartureItem* item )
{
    if ( item->alarmStates().testFlag( AlarmFired ) ) {
        return; // Don't fire alarms more than once
    }

    kDebug() << "FIRE" << dateTime << item->departureInfo()->matchedAlarms();
    bool fireAlarm = true;
    for ( int i = item->departureInfo()->matchedAlarms().count() - 1; i >= 0; --i ) {
        int matchedAlarm = item->departureInfo()->matchedAlarms().at( i );
        if ( matchedAlarm < 0 || matchedAlarm >= m_info.alarmSettings.count() ) {
            kDebug() << "Matched alarm is out of range of current alarm settings";
            continue;
        }

        QDateTime lastFired = m_info.alarmSettings[ matchedAlarm ].lastFired;
        if ( lastFired.isValid() ) {
            int secs = lastFired.secsTo( dateTime );
            kDebug() << "Alarm already fired?" << secs << "seconds from last fired to alarm time.";
            if ( secs >= 0 ) {
                fireAlarm = false;
            }
        }
    }
    kDebug() << "Fire alarm?" << fireAlarm;
    if ( !fireAlarm ) {
        return;
    }

    item->setAlarmStates( (item->alarmStates() & ~(AlarmPending | AlarmIsAutoGenerated)) | AlarmFired );
    emit alarmFired( item );

    QList< int > alarmsToRemove;
    for ( int i = item->departureInfo()->matchedAlarms().count() - 1; i >= 0; --i ) {
        int matchedAlarm = item->departureInfo()->matchedAlarms().at( i );
        if ( matchedAlarm < 0 || matchedAlarm >= m_info.alarmSettings.count() ) {
            kDebug() << "Matched alarm is out of range of current alarm settings";
            continue;
        }
        if ( m_info.alarmSettings[matchedAlarm].type == AlarmRemoveAfterFirstMatch ) {
            alarmsToRemove << matchedAlarm;
        }
        m_info.alarmSettings[ matchedAlarm ].lastFired = QDateTime::currentDateTime();

#if QT_VERSION >= 0x040600
        QPropertyAnimation *anim = new QPropertyAnimation( item, "alarmColorIntensity", this );
        anim->setStartValue( 1.0 );
        anim->setEndValue( 0.0 );
        anim->setDuration( 1000 );
        anim->setLoopCount( 5 );
        anim->start( QAbstractAnimation::DeleteWhenStopped );
        kDebug() << "Start anim";
#endif
    }
    kDebug() << "ALARMS TO BE REMOVED" << alarmsToRemove << item->departureInfo()->lineString()
             << item->departureInfo()->target() << item->departureInfo()->departure();
    if ( !alarmsToRemove.isEmpty() ) {
        foreach( int i, alarmsToRemove )  // stored in descending order...
        m_info.alarmSettings.removeAt( i ); // ...so removing is ok
        emit updateAlarms( m_info.alarmSettings, alarmsToRemove );
    }
}

RouteItemFlags DepartureModel::routeItemFlags( const QString& stopName ) const
{
    return m_highlightedStopName.compare( stopName, Qt::CaseInsensitive ) == 0
            ? RouteItemHighlighted : RouteItemDefault;
}

void DepartureModel::setHighlightedStop( const QString& stopName )
{
    m_highlightedStopName = stopName;

    if ( !m_items.isEmpty() ) {
        emit dataChanged( m_items.first()->index(), m_items.last()->index() );
    }
}

void DepartureModel::setColorGroups( const ColorGroupSettingsList& colorGroups )
{
    if ( m_colorGroups == colorGroups ) {
        return; // Unchanged
    }
    m_colorGroups = colorGroups;

    if ( !m_items.isEmpty() ) {
        QModelIndex topLeft = m_items.first()->index();
        QModelIndex bottomRight = m_items.last()->index();
        if ( topLeft.isValid() && bottomRight.isValid() ) {
            emit dataChanged( topLeft, bottomRight );
        }
    }
}
