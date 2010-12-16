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

#include "serviceprovidermodel.h"
#include <Plasma/DataEngine>
#include "global.h"
#include <KCategorizedSortFilterProxyModel>

ServiceProviderItem::ServiceProviderItem(const QString &name, const QVariantHash &data)
{
    m_name = name;
    m_data = data;
    m_formattedText = QString( "<b>%1</b><br-wrap><small><b>Features:</b> %2</small>" )
	.arg( name )
	.arg( data["featuresLocalized"].toStringList().join(", ") );

    QString location = countryCode();
    if ( location == "international" ) {
	m_category = i18nc("@info:inlistbox Name of the category for international "
			"service providers", "International");
	m_sortString = "XXXXX" + name;
    } else if ( location == "unknown" ) {
	m_category = i18nc("@info:inlistbox Name of the category for service providers "
			"with unknown contries", "Unknown");
	m_sortString = "YYYYY" + name;
    } else {
	m_category = KGlobal::locale()->countryCodeToName( location );

	// TODO Add a flag to the accessor XML files, maybe <countryWide />
	bool isCountryWide = name.contains( location, Qt::CaseInsensitive );
	m_sortString = isCountryWide
		? "WWWWW" + m_category + "11111" + name
		: "WWWWW" + m_category + name;
    }

}

ServiceProviderModel::ServiceProviderModel(QObject* parent) : QAbstractListModel(parent)
{

}

QModelIndex ServiceProviderModel::index(int row, int column, const QModelIndex& parent) const
{
    if ( parent.isValid() || !hasIndex(row, column, QModelIndex()) ) {
	return QModelIndex();
    } else {
	if ( row >= 0 && row < m_items.count() && column == 0 )
	    return createIndex( row, column, m_items[row] );
	else
	    return QModelIndex();
    }
}

QVariant ServiceProviderModel::data(const QModelIndex& index, int role) const
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

int ServiceProviderModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_items.count();
}

QModelIndex ServiceProviderModel::indexOfServiceProvider(const QString& serviceProviderId)
{
    for ( int row = 0; row < m_items.count(); ++row ) {
	ServiceProviderItem *item = m_items[row];
	if ( item->id() == serviceProviderId ) {
	    return createIndex( row, 0, item );
	}
    }

    // Location for given country code not found
    return QModelIndex();
}

bool serviceProviderGreaterThan(ServiceProviderItem* item1, ServiceProviderItem* item2)
{
    return item1->sortValue() < item2->sortValue();
}

void ServiceProviderModel::syncWithDataEngine(Plasma::DataEngine* publicTransportEngine,
					      Plasma::DataEngine* favIconEngine )
{
    // Store pointer to favicons data engine to be able to disconnect sources from it later
    m_favIconEngine = favIconEngine;
    
    Plasma::DataEngine::Data serviceProviderData = publicTransportEngine->query("ServiceProviders");
    for ( Plasma::DataEngine::Data::const_iterator it = serviceProviderData.constBegin();
	it != serviceProviderData.constEnd(); ++it )
    {
	// it.key() contains the service provider name
	QVariantHash serviceProviderData = it.value().toHash();
	m_items << new ServiceProviderItem( it.key(), serviceProviderData );

	// Request favicons
	QString favIconSource = serviceProviderData["url"].toString();
	favIconEngine->connectSource( favIconSource, this );
    }
    
    qSort( m_items.begin(), m_items.end(), serviceProviderGreaterThan );
}

void ServiceProviderModel::dataUpdated(const QString& sourceName, const Plasma::DataEngine::Data& data)
{
    if ( sourceName.contains(QRegExp("^http")) ) {
	// Favicon of a service provider arrived
	QPixmap favicon( QPixmap::fromImage(data["Icon"].value<QImage>()) );
	if ( favicon.isNull() ) {
	    kDebug() << "Favicon is NULL for" << sourceName << data;
	    favicon = QPixmap( 16, 16 );
	    favicon.fill( Qt::transparent );
	}

	for ( int i = 0; i < rowCount(); ++i ) {
	    ServiceProviderItem *item = m_items[i];
	    QString favIconSource = item->data()["url"].toString();
	    if ( favIconSource.compare(sourceName) == 0 ) {
		item->setIcon( KIcon(favicon) );
	    }
	}

	m_favIconEngine->disconnectSource( sourceName, this );
    }
}

