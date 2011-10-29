/*
    Copyright (C) 2011  Friedrich Pülz <fieti1983@gmx.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

// Header include
#include "journeysearchmodel.h"

// KDE includes
#include <KIconLoader>
#include <KIcon>
#include <KDebug>

// Qt includes
#include <QDataStream>

JourneySearchModelItem::JourneySearchModelItem( JourneySearchModel *model, const QString &journeySearch,
                                      const QString &name, bool favorite )
    : JourneySearchItem(journeySearch, name, favorite), m_model(model)
{
    Q_ASSERT( model );
}

QModelIndex JourneySearchModelItem::index() const
{
    return m_model->indexFromItem( this );
}

void JourneySearchModelItem::dataChanged() const
{
    QModelIndex _index = index();
    m_model->emitDataChanged( _index, _index );
}

JourneySearchModel::JourneySearchModel( QObject *parent )
        : QAbstractListModel(parent)
{
}

QPixmap JourneySearchModel::favoriteIconPixmap( const QIcon &icon, bool favorite )
{
    return icon.pixmap( KIconLoader::SizeSmall, favorite ? QIcon::Normal : QIcon::Disabled );
}

QPixmap JourneySearchModel::favoriteIconPixmap( bool favorite )
{
    return favoriteIconPixmap( favoriteIcon(), favorite );
}

QIcon JourneySearchModel::favoriteIcon( bool favorite )
{
    KIcon icon( "favorites" );
    if ( favorite ) {
        return icon;
    }

    QIcon nonFavoriteIcon;
    nonFavoriteIcon.addPixmap( favoriteIconPixmap(icon, false) );
    return nonFavoriteIcon;
}

void JourneySearchModel::emitDataChanged( const QModelIndex &topLeft,
                                          const QModelIndex &bottomRight )
{
    emit dataChanged( topLeft, bottomRight );
}

JourneySearchModelItem *JourneySearchModel::item( const QModelIndex &index )
{
    return static_cast<JourneySearchModelItem*>( index.internalPointer() );
}

JourneySearchModelItem *JourneySearchModel::addJourneySearch( const QString &journeySearch,
                                                         const QString &name, bool favorite )
{
    JourneySearchModelItem *item = new JourneySearchModelItem( this, journeySearch, name, favorite );

    beginInsertRows( QModelIndex(), 0, 0 );
    m_items.prepend( item );
    endInsertRows();

    return item;
}

bool JourneySearchModel::removeJourneySearch( const QModelIndex &index )
{
    if ( !index.isValid() ) {
        return false;
    }

    beginRemoveRows( QModelIndex(), index.row(), index.row() );
    m_items.removeAt( index.row() );
    endRemoveRows();

    return true;
}

void JourneySearchModel::clear()
{
    beginRemoveRows( QModelIndex(), 0, m_items.count() );
    m_items.clear();
    endRemoveRows();
}

QModelIndex JourneySearchModel::index( int row, int column, const QModelIndex &parent ) const
{
    if ( parent.isValid() || !hasIndex(row, column, QModelIndex()) ) {
        return QModelIndex();
    } else {
        if ( row >= 0 && row < m_items.count() && column == 0 ) {
            return createIndex( row, column, m_items[row] );
        } else {
            return QModelIndex();
        }
    }
}

QModelIndex JourneySearchModel::indexFromJourneySearch( const QString &journeySearch )
{
    for ( int row = 0; row < m_items.count(); ++row ) {
        JourneySearchModelItem *item = m_items[row];
        if ( item->journeySearch() == journeySearch ) {
            return createIndex( row, 0, item );
        }
    }

    // JourneySearchModelItem for given journey search string not found
    return QModelIndex();
}

bool JourneySearchModel::insertRows( int row, int count, const QModelIndex &parent )
{
    if ( parent.isValid() ) {
        // This model has no children.. and does not want any ;)
        return false;
    }

    Q_ASSERT( row >= 0 && row <= m_items.count() );
    beginInsertRows( parent, row, row + count );
    for ( int i = count; i > 0; --i ) {
        m_items.insert( row, new JourneySearchModelItem(this, QString()) );
    }
    endInsertRows();

    return true;
}

bool JourneySearchModel::removeRows( int row, int count, const QModelIndex &parent )
{
    if ( parent.isValid() ) {
        // This model has no children
        return false;
    }

    Q_ASSERT( row >= 0 && row <= m_items.count() );
    beginRemoveRows( parent, row, row + count );
    for ( int i = count; i > 0; --i ) {
        m_items.removeAt( row );
    }
    endRemoveRows();

    return true;
}

QVariant JourneySearchModel::data( const QModelIndex &index, int role ) const
{
    JourneySearchModelItem *item = static_cast<JourneySearchModelItem*>( index.internalPointer() );
    if ( !item ) {
        kDebug() << "No item found for index" << index;
        return QVariant();
    }

    switch ( role ) {
    case Qt::DisplayRole:
        if ( item->name().isEmpty() ) {
            return item->journeySearch();
        } else {
            return QString("%1 <span style='color=gray;'>%2</span>").arg( item->name(), item->journeySearch() );
        }
    case JourneySearchRole:
        return item->journeySearch();
    case NameRole:
        return item->name();
    case Qt::DecorationRole:
        return item->icon();
    case FavoriteRole:
        return item->isFavorite();
    }

    return QVariant();
}

bool JourneySearchModel::setDataWithoutNotify( JourneySearchModelItem *item, const QVariant &value,
                                               int role )
{
    switch ( role ) {
    case JourneySearchRole:
        item->setJourneySearch( value.toString() );
        break;
    case NameRole:
        item->setName( value.toString() );
        break;
    case FavoriteRole:
        item->setFavorite( value.toBool() );
        break;
    default:
        return false;
    }
    return true;
}

bool JourneySearchModel::setData( const QModelIndex &index, const QVariant &value, int role )
{
    if ( !index.isValid() ) {
        return false;
    }

    bool ret = setDataWithoutNotify( item(index), value, role );
    if ( ret ) {
        emit dataChanged( index, index );
    }
    return ret;
}

bool JourneySearchModel::setItemData( const QModelIndex &index, const QMap< int, QVariant > &roles )
{
    if ( !index.isValid() ) {
        return false;
    }

    bool ret = false;
    for ( QMap<int, QVariant>::ConstIterator it = roles.constBegin();
          it != roles.constEnd(); ++it )
    {
        if ( setDataWithoutNotify(item(index), it.value(), it.key()) ) {
            ret = true;
        }
    }
    if ( ret ) {
        emit dataChanged( index, index );
    }
    return ret;
}

QList< JourneySearchItem > JourneySearchModel::journeySearchItems()
{
    QList< JourneySearchItem > items;
    foreach ( const JourneySearchModelItem *modelItem, m_items ) {
        items << *modelItem;
    }
    return items;
}

// Used to sort journey search items in the model
class JourneySearchModelLessThan
{
public:
    inline bool operator()( const JourneySearchModelItem* l, const JourneySearchModelItem* r ) const {
        if ( l->isFavorite() != r->isFavorite() ) {
            // Sort favorites in front of non-favorites
            return l->isFavorite() && !r->isFavorite();
        } else if ( l->name().isEmpty() != r->name().isEmpty() ) {
            // Sort items with a name in front of items without a name
            return !l->name().isEmpty() && r->name().isEmpty();
        } else if ( !l->name().isEmpty() ) {
            // Favorite/name state is the same, names are available, sort by name alhabetical
            return l->name().localeAwareCompare( r->name() ) < 0;
        } else {
            // Favorite/name state is the same, names are not available,
            // sort by journey search string alhabetical
            return l->journeySearch().localeAwareCompare( r->journeySearch() ) < 0;
        }
    };
};

void JourneySearchModel::sort( int column, Qt::SortOrder order )
{
    if ( column != 0 ) {
        return;
    }

    emit layoutAboutToBeChanged();
    if ( order == Qt::AscendingOrder ) {
        qStableSort( m_items.begin(), m_items.end(), JourneySearchModelLessThan() );
    } else {
        kDebug() << "Not implemented";
    }
    emit layoutChanged();
}
