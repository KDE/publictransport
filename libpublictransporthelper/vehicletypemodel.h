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

/** @file
 * @brief Contains a model for vehicle types.
 *
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef VEHICLETYPEMODEL_H
#define VEHICLETYPEMODEL_H

#include "publictransporthelper_export.h"

#include <QModelIndex>
#include <KIcon>
#include <Plasma/DataEngine>
#include "enums.h"

class VehicleTypeModelPrivate;
class QStringList;

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

/**
 * @brief A model containing vehicle types.
 *
 * Example use case: Can be used with CheckComboBox to allow the selection of vehicle types to be shown/filtered.
 *
 * @note removeRow(s) doesn't work, this model should be handled read-only.
 *
 * @since 0.10
 **/
class PUBLICTRANSPORTHELPER_EXPORT VehicleTypeModel : public QAbstractListModel
{
    Q_OBJECT

public:
    /**
     * @brief Creates a new vehicle type model.
     *
     * @param parent The parent of this model. Defaults to 0.
     **/
    explicit VehicleTypeModel(QObject* parent = 0);

    /**
     * @brief Destructor
     **/
    virtual ~VehicleTypeModel();

    /**
     * @brief Gets an index for the given @p row and @p column. @p parent isn't used.
     **/
    virtual QModelIndex index(int row, int column = 0, const QModelIndex& parent = QModelIndex()) const;

    /**
     * @brief Gets the data for the given @p index and @p role.
     **/
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;

    /**
     * @brief Sets the data for the given @p index and @p role to @p value.
     **/
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);

    /**
     * @brief Gets the number of rows in this model.
     *
     * @param parent Isn't used, because this model has no subitems.
     *   If a valid parent index is given, 0 is returned. Defaults to QModelIndex().
     *
     * @return The number of rows in this model.
     **/
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const;

    /** @brief Gets QModelIndex of the item with the given @p serviceProviderId. */
    QModelIndex indexOfVehicleType( VehicleType vehicleType );

    /**
     * @brief Checks/unchecks all vehicle types.
     *
     * @param check Whether the vehicle types should be checked or unchecked. Default is true.
     **/
    void checkAll( bool check = true );

    /**
     * @brief Checks/unchecks all vehicle types in the given class @p generalVehicleType.
     *
     * @param generalVehicleType The class of vehicle types to be checked/unchecked.
     *
     * @param check Whether the vehicle types should be checked or unchecked. Default is true.
     **/
    void checkVehicleTypes( GeneralVehicleType generalVehicleType, bool check = true );

    /**
     * @brief Checks/unchecks all vehicle types in the given list @p vehicleTypes.
     *
     * @param vehicleTypes The vehicle types to be checked/unchecked.
     *
     * @param check Whether the vehicle types should be checked or unchecked. Default is true.
     **/
    void checkVehicleTypes( const QList<VehicleType> &vehicleTypes, bool check = true );

    /**
     * @brief Gets a list of all checked vehicle types.
     **/
    QList<VehicleType> checkedVehicleTypes() const;

protected:
    VehicleTypeModelPrivate* const d_ptr;

private:
    Q_DECLARE_PRIVATE( VehicleTypeModel )
    Q_DISABLE_COPY( VehicleTypeModel )
};

}; // namespace Timetable

#endif // VEHICLETYPEMODEL_H
