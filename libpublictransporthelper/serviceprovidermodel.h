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
 * @brief Contains a model for service providers, to be filled by the public transport data engine.
 *
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef SERVICEPROVIDERMODEL_H
#define SERVICEPROVIDERMODEL_H

#include "publictransporthelper_export.h"

#include <QModelIndex>
#include <QIcon>
#include <Plasma/DataEngine> // For dataUpdated slot (Plasma::DataEngine::Data)

class QStringList;

/** @brief Namespace for the publictransport helper library. */
namespace PublicTransport {

class ServiceProviderItemPrivate;
class ServiceProviderModelPrivate;

/**
 * @brief An item of a ServiceProviderModel.
 *
 * @since 0.9
 **/
class ServiceProviderItem
{
public:
    /**
     * @brief Creates a new service provider item.
     *
     * @param serviceProviderData A QVariantHash with data from the "publictransport" data engine.
     *
     * @note There's no need to call this yourself, just use
     *   @ref ServiceProviderModel::syncWithDataEngine to fill the model with items.
     **/
    ServiceProviderItem( const QVariantHash &serviceProviderData );

    virtual ~ServiceProviderItem();

    /** @brief Gets the ID of the service provider. */
    QString id() const;
    /** @brief Gets the name of the service provider. */
    QString name() const;
    /** @brief Gets the country code of the country the service provider supports
     *   or "international", "unknown". */
    QString countryCode() const;
    /** @brief Gets formatted text to be displayed. This is used by the @ref HtmlDelegate. */
    QString formattedText() const;
    /** @brief Gets the data from the "publictransport" data engine for the service provider. */
    QVariantHash data() const;
    /** @brief Gets the icon for this item, ie. a favicon for the service provider. */
    QIcon icon() const;
    /** @brief Gets the category of this service provider, can be used for KCategoryView. */
    QString category() const;
    /** @brief Gets a string used to sort the items. */
    QString sortValue() const;

    /**
     * @brief Sets the icon of this item.
     *
     * It gets automatically set asynchronously to the favicon of the service providers website,
     * if you use @ref ServiceProviderModel::syncWithDataEngine.
     *
     * @param icon The new icon for this item.
     **/
    void setIcon( const QIcon &icon );

    void setData( const QVariantHash &data );

protected:
    ServiceProviderItemPrivate* const d_ptr;

private:
    Q_DECLARE_PRIVATE( ServiceProviderItem )
    Q_DISABLE_COPY( ServiceProviderItem )
};

/**
 * @brief A model containing service providers.
 *
 * It automatically connects to the "ServiceProviders" data source of the "publictransport" engine
 * and will stay up to date. Plasma::DataEngineManager gets used to get a pointer to the
 * data engine.
 *
 * @note removeRow(s) doesn't work, this model should be handled read-only.
 *
 * @since 0.9
 **/
class PUBLICTRANSPORTHELPER_EXPORT ServiceProviderModel : public QAbstractListModel
{
    Q_OBJECT

public:
    /**
     * @brief Creates a new service provider model.
     *
     * @param parent The parent of this model. Defaults to 0.
     **/
    explicit ServiceProviderModel(QObject* parent = 0);

    /**
     * @brief Destructor
     **/
    virtual ~ServiceProviderModel();

    /**
     * @brief Gets an index for the given @p row and @p column. @p parent isn't used.
     **/
    virtual QModelIndex index(int row, int column = 0, const QModelIndex& parent = QModelIndex()) const;
    /**
     * @brief Gets the data for the given @p index and @p role.
     **/
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    /**
    * @brief Gets the number of rows in this model.
    *
    * @param parent Isn't used, because this model has no subitems.
    *   If a valid parent index is given, 0 is returned. Defaults to QModelIndex().
    * @return The number of rows in this model.
    **/
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const;

    /** @brief Gets QModelIndex of the item with the given @p serviceProviderId. */
    QModelIndex indexOfServiceProvider( const QString &serviceProviderId );

    /** @brief Gets QModelIndex of the given @p item. */
    QModelIndex indexFromItem( ServiceProviderItem *item );

    /** @brief Get the ServiceProviderItem from the given @p serviceProviderId. */
    ServiceProviderItem *itemFromServiceProvider( const QString &serviceProviderId );

protected Q_SLOTS:
    /** @brief The data from the favicons data engine was updated. */
    void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );

protected:
    ServiceProviderModelPrivate* const d_ptr;

private:
    Q_DECLARE_PRIVATE( ServiceProviderModel )
    Q_DISABLE_COPY( ServiceProviderModel )
};

} // namespace Timetable

#endif // SERVICEPROVIDERMODEL_H
