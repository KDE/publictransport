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

/** @file
 * @brief Contains a model for locations, to be filled by the public transport data engine.
 *
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef LOCATIONMODEL_H
#define LOCATIONMODEL_H

#include "publictransporthelper_export.h"

#include <QModelIndex>
#include <QIcon>
#include <Plasma/DataEngine>

/** @brief Namespace for the publictransport helper library. */
namespace PublicTransport {

class LocationModelPrivate;
class LocationItemPrivate;

/**
 * @brief An item of a LocationModel.
 *
 * @since 0.9
 **/
class LocationItem {
public:
    /**
     * @brief The available location item types. The values are also used for sorting.
     **/
    enum ItemType {
        Invalid = 0, /**< An invalid item, that is not completely initialized. */
        Total = 1, /**< Displays the number of total providers. */
        Country = 2, /**< Displays a country. */
        International = 3, /**< Special type for providers that support countries all over the world. */
        Unknown = 4, /**< For providers with unknown country. */
        Erroneous = 5 /**< Displays erroneous providers. */
    };

    /**
     * @brief Creates a new location item.
     *
     * @param countryCode The country code for the new item.
     * @param providerCount The number of provider plugins for the location. Defaults to -1.
     * @param description A description for the location, eg. information about the providers for
     *   the location. Defaults to QString().
     *
     * @note There's no need to call this yourself, just use
     *   @ref LocationModel::syncWithDataEngine to fill the model with items.
     **/
    explicit LocationItem( const QString &countryCode, int providerCount = -1,
                           const QString &description = QString() );

    /**
     * @brief Destructor.
     **/
    virtual ~LocationItem();

    /**
     * @brief Gets the country code of the location, if the type is @ref Country.
     *
     * Otherwise "unknown", "international", "showAll" or "erroneous" is returned. */
    QString countryCode() const;

    /** @brief Gets the (unformatted) text to be displayed for this item. */
    QString text() const;

    /** @brief Gets formatted text to be displayed. This is used by the @ref HtmlDelegate. */
    QString formattedText() const;

    /** @brief Gets the icon for this item, eg. a flag for items of type @ref Country. */
    QIcon icon() const;

    /** @brief Gets the type of this item. */
    ItemType itemType() const;

protected:
    LocationItemPrivate* const d_ptr;

private:
    Q_DECLARE_PRIVATE( LocationItem )
    Q_DISABLE_COPY( LocationItem )
};

/**
 * @brief A model containing locations with supported providers in the data engine.
 *
 * There are different location types, a list of them can be seen in @ref LocationItem::ItemType.
 * This model automatically connects to the "Locations" data source of the "publictransport" engine
 * and will stay up to date. Plasma::DataEngineManager gets used to get a pointer to the data
 * engine.
 *
 * @note removeRow(s) doesn't work, this model should be handled read-only.
 *
 * @since 0.9
 **/
class PUBLICTRANSPORTHELPER_EXPORT LocationModel : public QAbstractListModel
{
    Q_OBJECT

public:
    /**
     * @brief Creates a new location model.
     *
     * @param parent The parent of this model. Defaults to 0.
     **/
    explicit LocationModel( QObject* parent = 0 );

    /**
     * @brief Destructor.
     **/
    virtual ~LocationModel();

    /**
     * @brief Gets the data for the given @p index and @p role.
     **/
    virtual QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const;

    /**
     * @brief Gets the number of rows in this model.
     *
     * @param parent Isn't used, because this model has no subitems.
     *   If a valid parent index is given, 0 is returned. Defaults to QModelIndex().
     * @return The number of rows in this model.
     **/
    virtual int rowCount( const QModelIndex& parent = QModelIndex() ) const;

    /**
     * @brief Gets an index for the given @p row and @p column. @p parent isn't used.
     **/
    virtual QModelIndex index( int row, int column, const QModelIndex& parent = QModelIndex() ) const;

    /**
     * @brief Gets flags for the given @p index.
     *
     * It's used to mark items of type Erroneous as non-selectable.
     *
     * @param index The index of the item to get flags for.
     * @return The flags of the given @p index.
     **/
    virtual Qt::ItemFlags flags( const QModelIndex& index ) const;

    /** @brief Gets QModelIndex of the item with the given @p countryCode. */
    QModelIndex indexOfLocation( const QString &countryCode );

    LocationItem *itemFromLocation( const QString &countryCode );

protected Q_SLOTS:
    /** @brief The data from the data engine was updated. */
    void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );

protected:
    LocationModelPrivate* const d_ptr;

private:
    Q_DECLARE_PRIVATE( LocationModel )
    Q_DISABLE_COPY( LocationModel )
};

} // namespace Timetable

#endif // LOCATIONMODEL_H
