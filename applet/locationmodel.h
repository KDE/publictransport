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

/** @file
 * @brief This file contains a model for locations, to be filled by the public transport data engine.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef LOCATIONMODEL_H
#define LOCATIONMODEL_H

#include <QModelIndex>
#include <KIcon>

namespace Plasma {
	class DataEngine;
}

class LocationItem {
public:
	/**
	* The available location item types. The values are also used for sorting.
	**/
	enum ItemType {
		Total = 0,
		Country = 1,
		International = 2,
		Unknown = 3,
		Errornous = 4
	};

	LocationItem( const QString &countryCode, int accessorCount = -1,
				  const QString &description = QString() );

	QString countryCode() const { return m_countryCode; };
	QString text() const { return m_text; };
	QString formattedText() const { return m_formattedText; };
	KIcon icon() const { return m_icon; };
	ItemType itemType() const { return m_itemType; };

private:
	void setFromCountryCode( const QString &countryCode, int accessorCount = -1,
							 const QString &description = QString() );

	QString m_countryCode;
	QString m_text;
	QString m_formattedText;
	KIcon m_icon;
	ItemType m_itemType;
};

class LocationModel : public QAbstractListModel
{
public:
	explicit LocationModel(QObject* parent = 0);

	virtual QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const;
	virtual int rowCount( const QModelIndex& parent = QModelIndex() ) const;
	virtual QModelIndex index( int row, int column, const QModelIndex& parent = QModelIndex() ) const;
	virtual Qt::ItemFlags flags( const QModelIndex& index ) const;

	void syncWithDataEngine( Plasma::DataEngine *publicTransportEngine );
	QModelIndex indexOfLocation( const QString &countryCode );

private:
	QList<LocationItem*> m_items;
};

#endif // LOCATIONMODEL_H
