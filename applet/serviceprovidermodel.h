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
 * @brief This file contains a model for service providers, to be filled by the public transport data engine.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef SERVICEPROVIDERMODEL_H
#define SERVICEPROVIDERMODEL_H

#include <QModelIndex>
#include <KIcon>
#include <Plasma/DataEngine>

class QStringList;
class ServiceProviderItem
{
public:
	ServiceProviderItem( const QString &name, const QVariantHash &serviceProviderData );

	QString id() const { return m_data["id"].toString(); };
	QString name() const { return m_name; };
	QString countryCode() const { return m_data["country"].toString(); };
	QString formattedText() const { return m_formattedText; };
	QVariantHash data() const { return m_data; };
	KIcon icon() const { return m_icon; };
	QString category() const { return m_category; };
	QString sortValue() const { return m_sortString; };

	void setIcon( const KIcon &icon ) { m_icon = icon; };

private:
	QString m_name;
	QString m_formattedText;
	KIcon m_icon;
	QVariantHash m_data;
	QString m_category;
	QString m_sortString;
};

class ServiceProviderModel : public QAbstractListModel
{
	Q_OBJECT

public:
	explicit ServiceProviderModel(QObject* parent = 0);

	virtual QModelIndex index(int row, int column = 0, const QModelIndex& parent = QModelIndex()) const;
	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const;

	void syncWithDataEngine( Plasma::DataEngine *publicTransportEngine,
							 Plasma::DataEngine* favIconEngine );
	QModelIndex indexOfServiceProvider( const QString &serviceProviderId );

protected slots:
	/** The data from the favicons data engine was updated. */
	void dataUpdated( const QString& sourceName, const Plasma::DataEngine::Data& data );

private:
	QList<ServiceProviderItem*> m_items;
	Plasma::DataEngine* m_favIconEngine;
};

#endif // SERVICEPROVIDERMODEL_H
