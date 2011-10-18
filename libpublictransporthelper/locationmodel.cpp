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

#include "locationmodel.h"
#include "global.h"
#include "enums.h"
#include <Plasma/DataEngine>
#include <KStandardDirs>

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

class LocationItemPrivate
{
public:
    LocationItemPrivate() { itemType = LocationItem::Invalid; };

    void setFromCountryCode( const QString &countryCode, int accessorCount = -1,
                             const QString &description = QString() ) {
        this->countryCode = countryCode;
        if ( countryCode.compare("showAll", Qt::CaseInsensitive) == 0 ) {
            text = i18nc( "@item:inlistbox", "Show all available service providers" );
            icon = KIcon( "package_network" );
            formattedText = QString( "<span><b>%1</b></span> <br-wrap><small><b>%2</b></small>" )
                    .arg( text )
                    .arg( i18nc("@info/plain Label for the total number of accessors", "Total: ")
                        + i18ncp("@info/plain", "%1 accessor", "%1 accessors", accessorCount) );
            itemType = LocationItem::Total;
            return;
        } else if ( countryCode.compare("erroneous", Qt::CaseInsensitive) == 0 ) {
            icon = KIcon( "edit-delete" );
            formattedText = QString( "<span><b>%1</b></span><br-wrap><small>%2</small>" )
                    .arg( i18ncp("@info/plain", "%1 accessor is erroneous:",
                                "%1 accessors are erroneous:", accessorCount) )
                    .arg( description );
            itemType = LocationItem::Erroneous;
            return;
        } else if ( countryCode.compare("international", Qt::CaseInsensitive) == 0 ) {
            text = i18nc("@item:inlistbox Name of the category for international "
                        "service providers", "International");
            icon = Global::internationalIcon();
            itemType = LocationItem::International;
        } else if ( countryCode.compare("unknown", Qt::CaseInsensitive) == 0 ) {
            text = i18nc("@item:inlistbox Name of the category for service providers "
                        "with unknown contries", "Unknown");
            icon = KIcon( "dialog-warning" );
            itemType = LocationItem::Unknown;
        } else {
            if ( KGlobal::locale()->allCountriesList().contains( countryCode ) ) {
                text = KGlobal::locale()->countryCodeToName( countryCode );
            } else {
                text = countryCode;
            }

            // Get a flag icon for the country
            QString flag( KStandardDirs::locate("locale", QString::fromLatin1("l10n/%1/flag.png")
                    .arg(countryCode)) );
            icon.addFile( flag );
//             icon = Global::putIconIntoBiggerSizeIcon( icon, QSize(32, 23) );
            itemType = LocationItem::Country;
        }

        formattedText = QString( "<span><b>%1</b></span> <small>(<b>%2</b>)<br-wrap>%3</small>" )
                .arg( text )
                .arg( i18ncp("@info/plain", "%1 accessor", "%1 accessors", accessorCount) )
                .arg( description );
    };

    QString countryCode;
    QString text;
    QString formattedText;
    KIcon icon;
    LocationItem::ItemType itemType;
};

LocationItem::LocationItem( const QString& countryCode, int accessorCount,
                            const QString &description )
        : d_ptr(new LocationItemPrivate())
{
    d_ptr->setFromCountryCode( countryCode, accessorCount, description );
}

LocationItem::~LocationItem()
{
    delete d_ptr;
}

QString LocationItem::countryCode() const
{
    Q_D( const LocationItem );
    return d->countryCode;
}

QString LocationItem::text() const
{
    Q_D( const LocationItem );
    return d->text;
}

QString LocationItem::formattedText() const
{
    Q_D( const LocationItem );
    return d->formattedText;
}

KIcon LocationItem::icon() const
{
    Q_D( const LocationItem );
    return d->icon;
}

LocationItem::ItemType LocationItem::itemType() const
{
    Q_D( const LocationItem );
    return d->itemType;
}

bool locationGreaterThan( LocationItem* item1, LocationItem* item2 )
{
    if ( item1->itemType() == item2->itemType() ) {
        return item1->text() < item2->text();
    } else {
        return item1->itemType() < item2->itemType();
    }
}


class LocationModelPrivate
{
public:
    LocationModelPrivate() {};
    ~LocationModelPrivate() {
        qDeleteAll( items );
    };

    QList<LocationItem*> items;
};

LocationModel::LocationModel( QObject* parent )
        : QAbstractListModel( parent ), d_ptr(new LocationModelPrivate())
{
}

LocationModel::~LocationModel()
{
    delete d_ptr;
}

void LocationModel::syncWithDataEngine( Plasma::DataEngine* publicTransportEngine )
{
    Q_D( LocationModel );

    // Get locations
    Plasma::DataEngine::Data locationData = publicTransportEngine->query( "Locations" );
    QStringList uniqueCountries = locationData.keys();
    QStringList countries;

//     TODO: Give ServiceProviderModel or ..Data as parameter?
    // Get a list with the location of each service provider
    // (locations can be contained multiple times)
    Plasma::DataEngine::Data serviceProviderData = publicTransportEngine->query( "ServiceProviders" );
    for ( Plasma::DataEngine::Data::const_iterator it = serviceProviderData.constBegin();
                it != serviceProviderData.constEnd(); ++it ) {
        countries << serviceProviderData.value( it.key() ).toHash()[ "country" ].toString();
    }

    // Create location items
    foreach( const QString &country, uniqueCountries ) {
        d->items << new LocationItem( country, countries.count( country ),
                                     locationData[country].toHash()["description"].toString() );
    }

    // Append item to show all service providers
    d->items << new LocationItem( "showAll", countries.count() );

    // Get erroneous service providers (TODO: Get error messages)
    QStringList erroneousAccessorNames = publicTransportEngine
                                         ->query( "ErroneousServiceProviders" )["names"].toStringList();
    if ( !erroneousAccessorNames.isEmpty() ) {
        QStringList errorLines;
        for ( int i = 0; i < erroneousAccessorNames.count(); ++i ) {
            errorLines << QString( "<b>%1</b>" ).arg( erroneousAccessorNames[i] );//.arg( errorMessages[i] );
        }

        d->items << new LocationItem( "erroneous", erroneousAccessorNames.count(),
                                     errorLines.join( ",<br-wrap>" ) );
    }

    qSort( d->items.begin(), d->items.end(), locationGreaterThan );
}

QVariant LocationModel::data( const QModelIndex& index, int role ) const
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
        case LocationItem::Erroneous:
        default:
            return 3;
        }
    case FormattedTextRole:
        return item->formattedText();
    }

    return QVariant();
}

Qt::ItemFlags LocationModel::flags( const QModelIndex& index ) const
{
    LocationItem *item = static_cast<LocationItem*>( index.internalPointer() );
    if ( !item ) {
        kDebug() << "No item found for index" << index;
        return Qt::NoItemFlags;
    }

    if ( item->itemType() == LocationItem::Erroneous ) {
        // The item showing information about erroneous service providers isn't selectable
        return Qt::ItemIsEnabled;
    } else {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    }
}

int LocationModel::rowCount( const QModelIndex& parent ) const
{
    Q_D( const LocationModel );
    return parent.isValid() ? 0 : d->items.count();
}

QModelIndex LocationModel::index( int row, int column, const QModelIndex& parent ) const
{
    Q_D( const LocationModel );
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

QModelIndex LocationModel::indexOfLocation( const QString& countryCode )
{
    Q_D( const LocationModel );
    for ( int row = 0; row < d->items.count(); ++row ) {
        LocationItem *item = d->items[row];
        if ( item->countryCode() == countryCode ) {
            return createIndex( row, 0, item );
        }
    }

    // Location for given country code not found
    return QModelIndex();
}

}; // namespace Timetable
