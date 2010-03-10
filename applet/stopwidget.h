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

#ifndef STOPWIDGET_HEADER
#define STOPWIDGET_HEADER

#include "global.h"
#include "dynamicwidget.h"

#include <QStringList>

namespace Plasma {
    class DataEngine;
}
class DynamicWidget;
class QStandardItemModel;
class StopWidget : public QWidget {
    Q_OBJECT

    public:
	StopWidget( const StopSettings &stopSettings,
		    const QStringList &filterConfigurations,
		    QStandardItemModel *modelLocations,
		    QStandardItemModel *modelServiceProviders,
		    Plasma::DataEngine *publicTransportEngine,
		    Plasma::DataEngine *osmEngine,
		    Plasma::DataEngine *geolocationEngine, QWidget* parent );

	StopSettings stopSettings() const { return m_stopSettings; };
	void setStopSettings( const StopSettings &stopSettings );
	
	void setFilterConfigurations( const QStringList &filterConfigurations ) {
	    m_filterConfigurations = filterConfigurations; };

	/** Adds the given @p button. */
	void addButton( QToolButton *button );
	/** Removes the given @p button. */
	void removeButton( QToolButton *button );

	bool isHighlighted() const;
	void setHighlighted( bool highlighted );

    signals:
	void changed( const StopSettings &stopSettings );
	void remove();

    public slots:
	void changeClicked();

    private:
	bool m_newlyAdded;
	StopSettings m_stopSettings;
	QStringList m_filterConfigurations;
	QLabel *m_stop, *m_provider, *m_filterConfiguration;
	QStandardItemModel *m_modelLocations; // Model of locations
	QStandardItemModel *m_modelServiceProviders; // Model of service providers
	Plasma::DataEngine *m_publicTransportEngine;
	Plasma::DataEngine *m_osmEngine;
	Plasma::DataEngine *m_geolocationEngine;
};

class StopListWidget : public AbstractDynamicWidgetContainer {
    Q_OBJECT

    public:
	StopListWidget( const StopSettingsList &stopSettingsList,
			const QStringList &filterConfigurations,
			QStandardItemModel *modelLocations,
			QStandardItemModel *modelServiceProviders,
			Plasma::DataEngine *publicTransportEngine,
			Plasma::DataEngine *osmEngine,
			Plasma::DataEngine *geolocationEngine, QWidget *parent = 0 );

	StopSettingsList stopSettingsList() const;
	void setStopSettingsList( const StopSettingsList &stopSettingsList );
	void setFilterConfigurations( const QStringList &filterConfigurations );

	int currentStopSettingIndex() const { return m_currentStopIndex; };
	void setCurrentStopSettingIndex( int currentStopIndex );
	
    signals:
	void changed( int index, const StopSettings &stopSettings );
    
    protected slots:
	void changed( const StopSettings &stopSettings );

    protected:
	virtual QWidget* createNewWidget();
	virtual DynamicWidget* createDynamicWidget( QWidget* contentWidget );
	virtual DynamicWidget* addWidget( QWidget* widget );
	virtual int removeWidget( QWidget* widget );

    private:
	QStringList m_filterConfigurations;
	QStandardItemModel *m_modelLocations; // Model of locations
	QStandardItemModel *m_modelServiceProviders; // Model of service providers
	Plasma::DataEngine *m_publicTransportEngine;
	Plasma::DataEngine *m_osmEngine;
	Plasma::DataEngine *m_geolocationEngine;
	int m_currentStopIndex;
};

#endif // Multiple inclusion guard
