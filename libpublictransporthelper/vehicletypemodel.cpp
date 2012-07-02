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

#include "vehicletypemodel.h"
#include "global.h"

using namespace PublicTransport;

struct VehicleTypeItem {
    VehicleType vehicleType;
    bool checked;

    VehicleTypeItem( VehicleType vehicleType, bool checked = false ) {
        this->vehicleType = vehicleType;
        this->checked = checked;
    };
};

class VehicleTypeModelPrivate
{
public:
    VehicleTypeModelPrivate() {
    };

    ~VehicleTypeModelPrivate() {
        qDeleteAll( items );
    };

    void addItems() {
        items << new VehicleTypeItem( Unknown );
        items << new VehicleTypeItem( Tram );
        items << new VehicleTypeItem( Bus );
        items << new VehicleTypeItem( TrolleyBus );
        items << new VehicleTypeItem( InterurbanTrain );
        items << new VehicleTypeItem( Subway );
        items << new VehicleTypeItem( Metro );
        items << new VehicleTypeItem( RegionalTrain );
        items << new VehicleTypeItem( RegionalExpressTrain );
        items << new VehicleTypeItem( InterregionalTrain );
        items << new VehicleTypeItem( IntercityTrain );
        items << new VehicleTypeItem( HighSpeedTrain );
        items << new VehicleTypeItem( Ship );
        items << new VehicleTypeItem( Plane );
        items << new VehicleTypeItem( Feet );
    };

    void checkAll( bool check = true ) {
        foreach ( VehicleTypeItem *item, items ) {
            item->checked = check;
        }
    };

    void checkVehicleTypes( GeneralVehicleType generalVehicleType, bool check = true ) {
        foreach ( VehicleTypeItem *item, items ) {
            if ( Global::generalVehicleType(item->vehicleType) == generalVehicleType ) {
                item->checked = check;
            }
        }
    };

    void checkVehicleTypes( const QList< VehicleType > &vehicleTypes, bool check ) {
        foreach ( VehicleTypeItem *item, items ) {
            if ( vehicleTypes.contains(item->vehicleType) ) {
                item->checked = check;
            }
        }
    };

    QList< VehicleType > checkedVehicleTypes() const {
        QList< VehicleType > vehicleTypes;
        foreach ( VehicleTypeItem *item, items ) {
            if ( item->checked ) {
                vehicleTypes << item->vehicleType;
            }
        }
        return vehicleTypes;
    };

    QList<VehicleTypeItem*> items;
};

VehicleTypeModel::VehicleTypeModel( QObject* parent )
        : QAbstractListModel( parent ), d_ptr(new VehicleTypeModelPrivate())
{
    beginInsertRows( QModelIndex(), 0, 14 );
    d_ptr->addItems();
    endInsertRows();
}

VehicleTypeModel::~VehicleTypeModel()
{
    delete d_ptr;
}

QModelIndex VehicleTypeModel::index( int row, int column, const QModelIndex& parent ) const
{
    Q_D( const VehicleTypeModel );

    if ( parent.isValid() || !hasIndex(row, column, QModelIndex()) ) {
        return QModelIndex();
    } else {
        if ( row >= 0 && row < d->items.count() && column == 0 ) {
            return createIndex( row, column, d->items[row] );
        } else {
            return QModelIndex();
        }
    }
}

QVariant VehicleTypeModel::data( const QModelIndex& index, int role ) const
{
    VehicleTypeItem *item = static_cast<VehicleTypeItem*>( index.internalPointer() );

    switch ( role ) {
    case Qt::DisplayRole:
        return Global::vehicleTypeToString( item->vehicleType );
    case Qt::DecorationRole:
        return Global::vehicleTypeToIcon( item->vehicleType );
    case Qt::CheckStateRole:
        return item->checked ? Qt::Checked : Qt::Unchecked;
    }

    return QVariant();
}

bool VehicleTypeModel::setData( const QModelIndex& index, const QVariant& value, int role )
{
    if ( role == Qt::CheckStateRole ) {
        VehicleTypeItem *item = static_cast<VehicleTypeItem*>( index.internalPointer() );
        item->checked = value.toBool();
        return true;
    } else {
        return QAbstractItemModel::setData( index, value, role );
    }
}

int VehicleTypeModel::rowCount( const QModelIndex& parent ) const
{
    Q_D( const VehicleTypeModel );
    return parent.isValid() ? 0 : d->items.count();
}

QModelIndex VehicleTypeModel::indexOfVehicleType( VehicleType vehicleType )
{
    Q_D( const VehicleTypeModel );
    for ( int row = 0; row < d->items.count(); ++row ) {
        VehicleTypeItem *item = d->items[row];
        if ( item->vehicleType == vehicleType ) {
            return createIndex( row, 0, item );
        }
    }

    // Location for given country code not found
    return QModelIndex();
}

void VehicleTypeModel::checkAll(bool check)
{
    Q_D( VehicleTypeModel );
    d->checkAll( check );
    emit dataChanged( index(0), index(d->items.count() - 1) );
}

void VehicleTypeModel::checkVehicleTypes(GeneralVehicleType generalVehicleType, bool check)
{
    Q_D( VehicleTypeModel );
    d->checkVehicleTypes( generalVehicleType, check );
    emit dataChanged( index(0), index(d->items.count() - 1) );
}

void VehicleTypeModel::checkVehicleTypes(const QList< VehicleType >& vehicleTypes, bool check)
{
    Q_D( VehicleTypeModel );
    d->checkVehicleTypes( vehicleTypes, check );
    emit dataChanged( index(0), index(d->items.count() - 1) );
}

QList< VehicleType > VehicleTypeModel::checkedVehicleTypes() const
{
    Q_D( const VehicleTypeModel );
    return d->checkedVehicleTypes();
}
