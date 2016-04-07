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

#include "serviceprovidermodel.h"
#include "enums.h"
#include <Plasma/DataEngine>
#include <Plasma/DataEngineManager>
#include <KDebug>
#include <KGlobal>
#include <KLocalizedString>
#include <KCategorizedSortFilterProxyModel>

/** @brief Namespace for the publictransport helper library. */
namespace PublicTransport {

class ServiceProviderItemPrivate
{
public:
    ServiceProviderItemPrivate() {};

    void setData( const QVariantHash &_data ) {
        data = _data;
        name = data["name"].toString();
        if ( name.isEmpty() ) {
            name = i18nc("@info/plain", "<warning>Provider %1 not found!</warning>",
                         data["id"].toString() );
        } else if ( data["error"].toBool() ) {
            name = i18nc("@info/plain", "<warning>Invalid provider %1!</warning>", name);
            formattedText = QString( "%1<br /><b>Error:</b> %2" )
                    .arg( name ).arg( data["errorMessage"].toString() );
        } else {
            formattedText = QString( "%1<br /><b>Type:</b> %2" )
                    .arg( name ).arg( data["type"].toString() );
        }

        const QString location = countryCode();
        if ( location == QLatin1String("international") ) {
            category = i18nc("@item:inlistbox Name of the category for international "
                                    "service providers", "International");
            sortString = "XXXXX" + name;
        } else if ( location == QLatin1String("unknown") || location.isEmpty() ) {
            category = i18nc("@item:inlistbox Name of the category for service providers "
                                    "with unknown contries", "Unknown");
            sortString = "YYYYY" + name;
        } else {
            category = KGlobal::locale()->countryCodeToName( location );

            // TODO Add a flag to the accessor XML files, maybe <countryWide />
            bool isCountryWide = data["type"] == "GTFS" ? false
                    : name.contains( location, Qt::CaseInsensitive );
            sortString = isCountryWide ? "WWWWW" + category + "11111" + name
                                       : "WWWWW" + category + name;
        }
    };

    QString countryCode() const { return data["country"].toString(); };

    QString name;
    QString formattedText;
    QIcon icon;
    QVariantHash data;
    QString category;
    QString sortString;
};

ServiceProviderItem::ServiceProviderItem( const QVariantHash &data )
        : d_ptr(new ServiceProviderItemPrivate())
{
    d_ptr->setData( data );
}

ServiceProviderItem::~ServiceProviderItem()
{
    delete d_ptr;
}

QString ServiceProviderItem::id() const {
    Q_D( const ServiceProviderItem );
    return d->data["id"].toString();
}

QString ServiceProviderItem::name() const {
    Q_D( const ServiceProviderItem );
    return d->name;
}

QString ServiceProviderItem::countryCode() const {
    Q_D( const ServiceProviderItem );
    return d->countryCode();
}

QString ServiceProviderItem::formattedText() const {
    Q_D( const ServiceProviderItem );
    return d->formattedText;
}

QVariantHash ServiceProviderItem::data() const {
    Q_D( const ServiceProviderItem );
    return d->data;
}

QIcon ServiceProviderItem::icon() const {
    Q_D( const ServiceProviderItem );
    return d->icon;
}

QString ServiceProviderItem::category() const {
    Q_D( const ServiceProviderItem );
    return d->category;
}

QString ServiceProviderItem::sortValue() const {
    Q_D( const ServiceProviderItem );
    return d->sortString;
}

void ServiceProviderItem::setIcon( const QIcon &icon ) {
    Q_D( ServiceProviderItem );
    d->icon = icon;
}

void ServiceProviderItem::setData( const QVariantHash &data ) {
    Q_D( ServiceProviderItem );
    d->setData( data );
}

class ServiceProviderModelPrivate
{
public:
    ServiceProviderModelPrivate() {};
    ~ServiceProviderModelPrivate() {
        qDeleteAll( items );
    };

    QList<ServiceProviderItem*> items;
};

ServiceProviderModel::ServiceProviderModel( QObject* parent )
        : QAbstractListModel( parent ), d_ptr(new ServiceProviderModelPrivate())
{
    Plasma::DataEngineManager::self()->loadEngine( "publictransport" );
    Plasma::DataEngineManager::self()->loadEngine( "favicons" );
    Plasma::DataEngine *engine = Plasma::DataEngineManager::self()->engine( "publictransport" );
    engine->connectSource( "ServiceProviders", this );
}

ServiceProviderModel::~ServiceProviderModel()
{
    delete d_ptr;

    // Disconnect sources to prevent warnings (No such slot QObject::dataUpdated...)
    Plasma::DataEngine *engine = Plasma::DataEngineManager::self()->engine("publictransport");
    engine->disconnectSource( "ServiceProviders", this );

    Plasma::DataEngineManager::self()->unloadEngine( "publictransport" );
    Plasma::DataEngineManager::self()->unloadEngine( "favicons" );
}

QModelIndex ServiceProviderModel::index( int row, int column, const QModelIndex& parent ) const
{
    Q_D( const ServiceProviderModel );

    if ( parent.isValid() || !hasIndex( row, column, QModelIndex() ) ) {
        return QModelIndex();
    } else {
        if ( row >= 0 && row < d->items.count() && column == 0 ) {
            return createIndex( row, column, d->items[row] );
        } else {
            return QModelIndex();
        }
    }
}

QVariant ServiceProviderModel::data( const QModelIndex& index, int role ) const
{
    ServiceProviderItem *item = static_cast<ServiceProviderItem*>( index.internalPointer() );
    if ( !item ) {
        kDebug() << "No item found for index" << index;
        return QVariant();
    }

    switch ( role ) {
    case Qt::DisplayRole:
        return item->name();
    case Qt::DecorationRole:
        return item->icon();
    case LocationCodeRole:
        return item->countryCode();
    case ServiceProviderIdRole:
        return item->id();
    case LinesPerRowRole:
        return 4;
    case Qt::ToolTipRole:
    case FormattedTextRole:
        return item->formattedText();
    case ServiceProviderDataRole:
        return item->data();
    case KCategorizedSortFilterProxyModel::CategoryDisplayRole:
        return item->category();
    case KCategorizedSortFilterProxyModel::CategorySortRole:
        return item->sortValue();
    }

    return QVariant();
}

int ServiceProviderModel::rowCount( const QModelIndex& parent ) const
{
    Q_D( const ServiceProviderModel );
    return parent.isValid() ? 0 : d->items.count();
}

ServiceProviderItem *ServiceProviderModel::itemFromServiceProvider(
        const QString &serviceProviderId )
{
    Q_D( const ServiceProviderModel );
    foreach ( ServiceProviderItem *item, d->items ) {
        if ( item->id() == serviceProviderId ) {
            return item;
        }
    }

    // Location for given country code not found
    return 0;
}

QModelIndex ServiceProviderModel::indexOfServiceProvider( const QString& serviceProviderId )
{
    Q_D( const ServiceProviderModel );
    for ( int row = 0; row < d->items.count(); ++row ) {
        ServiceProviderItem *item = d->items[row];
        if ( item->id() == serviceProviderId ) {
            return createIndex( row, 0, item );
        }
    }

    // Location for given country code not found
    return QModelIndex();
}

QModelIndex ServiceProviderModel::indexFromItem( ServiceProviderItem *item )
{
    Q_D( const ServiceProviderModel );
    if ( !d->items.contains(item) ) {
        // Provider item is not contained in this model
        return QModelIndex();
    }

    return createIndex( d->items.indexOf(item), 0, item );
}

bool serviceProviderGreaterThan( ServiceProviderItem* item1, ServiceProviderItem* item2 )
{
    return item1->sortValue() < item2->sortValue();
}

void ServiceProviderModel::dataUpdated( const QString &sourceName,
                                        const Plasma::DataEngine::Data &data )
{
    Q_D( ServiceProviderModel );
    if ( sourceName == QLatin1String("ServiceProviders") ) {
        QList< ServiceProviderItem* > newProviders;
        for ( Plasma::DataEngine::Data::ConstIterator it = data.constBegin();
              it != data.constEnd(); ++it )
        {
            QVariantHash serviceProviderData = it.value().toHash();
            const QString id = serviceProviderData["id"].toString();
            ServiceProviderItem *item = itemFromServiceProvider( id );
            if ( item ) {
                // Update a service provider, that was already added to the model
                item->setData( serviceProviderData );
                const QModelIndex index = indexFromItem( item );
                dataChanged( index, index );
            } else {
                // Add new service provider
                newProviders << new ServiceProviderItem( serviceProviderData );
            }
        }

        // Append new providers sorted to the end of the provider list
        qSort( newProviders.begin(), newProviders.end(), serviceProviderGreaterThan );
        beginInsertRows( QModelIndex(), d->items.count(),
                         d->items.count() + newProviders.count() - 1 );
        d->items << newProviders;
        endInsertRows();

        // Request favicons for newly inserted providers
        // after inserting them (otherwise there will be no item to set the received icon for)
        foreach ( ServiceProviderItem *item, newProviders ) {
            Plasma::DataEngine *faviconEngine =
                    Plasma::DataEngineManager::self()->engine( "favicons" );
            if ( faviconEngine ) {
                QString favIconSource = item->data()["url"].toString();
                faviconEngine->connectSource( favIconSource, this );
            }
        }

        // TODO persistent indexes?
//         qSort( d->items.begin(), d->items.end(), serviceProviderGreaterThan );
    } else if ( sourceName.contains(QRegExp("^http")) ) {
        // Favicon of a service provider arrived
        QPixmap favicon( QPixmap::fromImage( data["Icon"].value<QImage>() ) );
        if ( favicon.isNull() ) {
            // No favicon found for sourceName
            favicon = QPixmap( 16, 16 );
            favicon.fill( Qt::transparent );
        }

        for ( int row = 0; row < rowCount(); ++row ) {
            ServiceProviderItem *item = d->items[ row ];
            const QString favIconSource = item->data()["url"].toString();
            if ( favIconSource.compare(sourceName) == 0 ) {
                item->setIcon( QIcon(favicon) );
                const QModelIndex index = createIndex( row, 0, item );
                dataChanged( index, index );

                Plasma::DataEngine *faviconEngine =
                        Plasma::DataEngineManager::self()->engine( "favicons" );
                faviconEngine->disconnectSource( sourceName, this );
                break;
            }
        }

//         d->favIconEngine->disconnectSource( sourceName, this );
    }
}

} // namespace Timetable
