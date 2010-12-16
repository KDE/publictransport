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

#include "locationmodel.h"
#include "global.h"
#include <Plasma/DataEngine>

LocationItem::LocationItem(const QString& countryCode, int accessorCount,
			   const QString &description)
{
    setFromCountryCode( countryCode, accessorCount, description );
}

void LocationItem::setFromCountryCode(const QString& countryCode, 
			    int accessorCount, const QString &description)
{
    m_countryCode = countryCode;
    if ( countryCode.compare("showAll", Qt::CaseInsensitive) == 0 ) {
	m_text = i18nc("@item:inlistbox", "Show all available service providers");
	m_icon = KIcon("package_network");
	m_formattedText = QString( "<span><b>%1</b></span> <br-wrap><small><b>%2</b></small>" )
	    .arg( m_text )
	    .arg( i18nc("@info:plain Label for the total number of accessors", "Total: ")
		+ i18ncp("@info:plain", "%1 accessor", "%1 accessors", accessorCount) );
	m_itemType = Total;
	return;
    } else if ( countryCode.compare("errornous", Qt::CaseInsensitive) == 0 ) {
	m_icon = KIcon("edit-delete");
	m_formattedText = QString( "<span><b>%1</b></span><br-wrap><small>%2</small>" )
	    .arg( i18ncp("@info:plain", "%1 accessor is errornous:",
			 "%1 accessors are errornous:", accessorCount) )
	    .arg( description );
	m_itemType = Errornous;
	return;
    } else if ( countryCode.compare("international", Qt::CaseInsensitive) == 0 ) {
	m_text = i18nc("@item:inlistbox Name of the category for international "
			"service providers", "International");
	m_icon = Global::internationalIcon();
	m_itemType = International;
    } else if ( countryCode.compare("unknown", Qt::CaseInsensitive) == 0 ) {
	m_text = i18nc("@item:inlistbox Name of the category for service providers "
			"with unknown contries", "Unknown");
	m_icon = KIcon("dialog-warning");
	m_itemType = Unknown;
    } else {
	if ( KGlobal::locale()->allCountriesList().contains( countryCode ) )
	    m_text = KGlobal::locale()->countryCodeToName( countryCode );
	else
	    m_text = countryCode;
	m_icon = Global::putIconIntoBiggerSizeIcon(KIcon(countryCode), QSize(32, 23));
	m_itemType = Country;
    }

    m_formattedText = QString( "<span><b>%1</b></span> <small>(<b>%2</b>)<br-wrap>%3</small>" )
	    .arg( m_text )
	    .arg( i18np("%1 accessor", "%1 accessors", accessorCount) )
	    .arg( description );
}

LocationModel::LocationModel(QObject* parent): QAbstractListModel(parent)
{

}

bool locationGreaterThan(LocationItem* item1, LocationItem* item2)
{
    if ( item1->itemType() == item2->itemType() ) {
	return item1->text() < item2->text();
    } else {
	return item1->itemType() < item2->itemType();
    }
}

void LocationModel::syncWithDataEngine(Plasma::DataEngine* publicTransportEngine)
{
    // Get locations
    Plasma::DataEngine::Data locationData = publicTransportEngine->query("Locations");
    QStringList uniqueCountries = locationData.keys();
    QStringList countries;

//     TODO: Give ServiceProviderModel or ..Data as parameter?
    // Get a list with the location of each service provider
    // (locations can be contained multiple times)
    Plasma::DataEngine::Data serviceProviderData = publicTransportEngine->query("ServiceProviders");
    for ( Plasma::DataEngine::Data::const_iterator it = serviceProviderData.constBegin();
	  it != serviceProviderData.constEnd(); ++it )
    {
	countries << serviceProviderData.value( it.key() )
			.toHash()[ "country" ].toString();
    }

    // Create location items
    foreach( const QString &country, uniqueCountries ) {
	m_items << new LocationItem( country, countries.count(country),
		locationData[country].toHash()["description"].toString() );
    }

    // Append item to show all service providers
    m_items << new LocationItem( "showAll", countries.count() );
    
    // Get errornous service providers (TODO: Get error messages)
    QStringList errornousAccessorNames = publicTransportEngine
	    ->query("ErrornousServiceProviders")["names"].toStringList();
    if ( !errornousAccessorNames.isEmpty() ) {
	QStringList errorLines;
	for( int i = 0; i < errornousAccessorNames.count(); ++i ) {
	    errorLines << QString("<b>%1</b>").arg( errornousAccessorNames[i] );//.arg( errorMessages[i] );
	}

	m_items << new LocationItem( "errornous", errornousAccessorNames.count(),
				     errorLines.join(",<br-wrap>") );
    }

    qSort( m_items.begin(), m_items.end(), locationGreaterThan );
}

QVariant LocationModel::data(const QModelIndex& index, int role) const
{
    LocationItem *item = static_cast<LocationItem*>( index.internalPointer() );
    if ( !item ) {
	kDebug() << "No item found for index" << index;
	return QVariant();
    }
    
    switch ( role ) {
	case Qt::DisplayRole:
	    return item->text();
	case Qt::DecorationRole:
	    return item->icon();
	case LocationCodeRole:
	    return item->countryCode();
	case LinesPerRowRole:
	    switch ( item->itemType() ) {
		case LocationItem::Country:
		case LocationItem::International:
		case LocationItem::Unknown:
		    return 4;
		case LocationItem::Total:
		case LocationItem::Errornous:
		default:
		    return 3;
	    }
	case FormattedTextRole:
	    return item->formattedText();
    }

    return QVariant();
}

Qt::ItemFlags LocationModel::flags(const QModelIndex& index) const
{
    LocationItem *item = static_cast<LocationItem*>( index.internalPointer() );
    if ( !item ) {
	kDebug() << "No item found for index" << index;
	return Qt::NoItemFlags;
    }

    if ( item->itemType() == LocationItem::Errornous ) {
	// The item showing information about errornous service providers isn't selectable
	return Qt::ItemIsEnabled;
    } else {
	return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    }
}

int LocationModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_items.count();
}

QModelIndex LocationModel::index(int row, int column, const QModelIndex& parent) const
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

QModelIndex LocationModel::indexOfLocation(const QString& countryCode)
{
    for ( int row = 0; row < m_items.count(); ++row ) {
	LocationItem *item = m_items[row];
	if ( item->countryCode() == countryCode ) {
	    return createIndex( row, 0, item );
	}
    }

    // Location for given country code not found
    return QModelIndex();
}
