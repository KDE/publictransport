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

#include "serviceprovidermodel.h"
#include "enums.h"
#include <Plasma/DataEngine>
#include <KCategorizedSortFilterProxyModel>

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

class ServiceProviderItemPrivate
{
public:
    ServiceProviderItemPrivate() {};

    QString name;
    QString formattedText;
    KIcon icon;
    QVariantHash data;
    QString category;
    QString sortString;
};

ServiceProviderItem::ServiceProviderItem( const QString &name, const QVariantHash &data )
        : d_ptr(new ServiceProviderItemPrivate())
{
    d_ptr->name = name;
    d_ptr->data = data;
    d_ptr->formattedText = QString( "<b>%1</b><br-wrap><small><b>Features:</b> %2</small>" )
            .arg( name )
            .arg( data["featuresLocalized"].toStringList().join( ", " ) );

    QString location = countryCode();
    if ( location == QLatin1String("international") ) {
        d_ptr->category = i18nc("@item:inlistbox Name of the category for international "
                                "service providers", "International");
        d_ptr->sortString = "XXXXX" + name;
    } else if ( location == QLatin1String("unknown") ) {
        d_ptr->category = i18nc("@item:inlistbox Name of the category for service providers "
                                "with unknown contries", "Unknown");
        d_ptr->sortString = "YYYYY" + name;
    } else {
        d_ptr->category = KGlobal::locale()->countryCodeToName( location );

        // TODO Add a flag to the accessor XML files, maybe <countryWide />
        bool isCountryWide = name.contains( location, Qt::CaseInsensitive );
        d_ptr->sortString = isCountryWide
                ? "WWWWW" + d_ptr->category + "11111" + name
                : "WWWWW" + d_ptr->category + name;
    }

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
    return d->data["country"].toString();
}

QString ServiceProviderItem::formattedText() const {
    Q_D( const ServiceProviderItem );
    return d->formattedText;
}

QVariantHash ServiceProviderItem::data() const {
    Q_D( const ServiceProviderItem );
    return d->data;
}

KIcon ServiceProviderItem::icon() const {
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

void ServiceProviderItem::setIcon(const KIcon& icon) {
    Q_D( ServiceProviderItem );
    d->icon = icon;
}

class ServiceProviderModelPrivate
{
public:
    ServiceProviderModelPrivate() : favIconEngine(0) {};
    ~ServiceProviderModelPrivate() {
        qDeleteAll( items );
    };

    QList<ServiceProviderItem*> items;
    Plasma::DataEngine* favIconEngine;
};

ServiceProviderModel::ServiceProviderModel( QObject* parent )
        : QAbstractListModel( parent ), d_ptr(new ServiceProviderModelPrivate())
{
}

ServiceProviderModel::~ServiceProviderModel()
{
    delete d_ptr;
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

bool serviceProviderGreaterThan( ServiceProviderItem* item1, ServiceProviderItem* item2 )
{
    return item1->sortValue() < item2->sortValue();
}

void ServiceProviderModel::syncWithDataEngine( Plasma::DataEngine* publicTransportEngine,
        Plasma::DataEngine* favIconEngine )
{
    Q_D( ServiceProviderModel );

    // Store pointer to favicons data engine to be able to disconnect sources from it later
    d->favIconEngine = favIconEngine;

    Plasma::DataEngine::Data serviceProviderData = publicTransportEngine->query( "ServiceProviders" );
    for ( Plasma::DataEngine::Data::const_iterator it = serviceProviderData.constBegin();
                it != serviceProviderData.constEnd(); ++it ) {
        // it.key() contains the service provider name
        QVariantHash serviceProviderData = it.value().toHash();
        d->items << new ServiceProviderItem( it.key(), serviceProviderData );

        // Request favicons
        if ( favIconEngine ) {
            QString favIconSource = serviceProviderData["url"].toString();
            favIconEngine->connectSource( favIconSource, this );
        }
    }

    qSort( d->items.begin(), d->items.end(), serviceProviderGreaterThan );
}

void ServiceProviderModel::dataUpdated( const QString& sourceName, const Plasma::DataEngine::Data& data )
{
    Q_D( const ServiceProviderModel );

    if ( sourceName.contains(QRegExp("^http")) ) {
        // Favicon of a service provider arrived
        QPixmap favicon( QPixmap::fromImage( data["Icon"].value<QImage>() ) );
        if ( favicon.isNull() ) {
//             kDebug() << "No favicon found for" << sourceName;
            favicon = QPixmap( 16, 16 );
            favicon.fill( Qt::transparent );
        }

        for ( int i = 0; i < rowCount(); ++i ) {
            ServiceProviderItem *item = d->items[i];
            QString favIconSource = item->data()["url"].toString();
            if ( favIconSource.compare( sourceName ) == 0 ) {
                item->setIcon( KIcon(favicon) );
            }
        }

        d->favIconEngine->disconnectSource( sourceName, this );
    }
}

} // namespace Timetable
