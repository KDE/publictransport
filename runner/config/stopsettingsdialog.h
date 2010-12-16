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

#ifndef STOPSETTINGSDIALOG_HEADER
#define STOPSETTINGSDIALOG_HEADER

#include <KDialog>
#include <Plasma/DataEngine>
#include <QSortFilterProxyModel>

#include "ui_publicTransportStopConfig.h"
#include "ui_accessorInfo.h"

#include "global.h"

class LocationModel;
class ServiceProviderModel;
class DynamicLabeledLineEditList;
class HtmlDelegate;
class KLineEdit;
class NearStopsDialog;

struct StopSettings {
    QString serviceProviderID;
    QString location;
    QString city;
};

/**
 * @brief This dialog is used to select a location, service provider and stop
 * TODO: This is a copy from the publicTransport applet, with most parts removed.
 * Maybe it's better to use it from a shared library? 
 * Other applets/runners could use it too.
 **/
class StopSettingsDialog : public KDialog {
    Q_OBJECT
    
    public:
	StopSettingsDialog( const StopSettings &stopSettings,
			    LocationModel *modelLocations,
			    ServiceProviderModel *modelServiceProviders,
			    Plasma::DataEngine *publicTransportEngine,
			    QWidget *parent = 0 );
	~StopSettingsDialog();

	/** @returns the current stop settings of the dialog. */
	StopSettings stopSettings() const;
	/** Sets the values of the widgets according to @p stopSettings. */
	void setStopSettings( const StopSettings &stopSettings );

    protected slots:
	/** Another service provider has been selected. */
	void serviceProviderChanged( int index );
	/** The city name has been changed. */
	void cityNameChanged( const QString &cityName );
	/** Another location has been selected. */
	void locationChanged( int index );
	/** The info button has been clicked. This shows information about the
	* currently selected service provider in a dialog. */
	void clickedServiceProviderInfo();
	/** The button to open the service provider in TimetableMate was clicked
	* in the service provider info dialog. */
	void openInTimetableMate();
// 	void downloadServiceProvidersClicked();
// 	void installServiceProviderClicked();
	
    private:
	/** Updates the service provider model by inserting service provider for the
	* current location. */
	void updateServiceProviderModel( int index );
	QString currentCityValue() const;
	
	Ui::publicTransportStopConfig m_uiStop;
// 	Ui::stopConfigDetails m_uiStopDetails;
	Ui::accessorInfo m_uiAccessorInfo;
	
	LocationModel *m_modelLocations; // Model of locations
	ServiceProviderModel *m_modelServiceProviders; // Model of service providers
	QSortFilterProxyModel *m_modelLocationServiceProviders; // Model of service providers for the current location
	HtmlDelegate *m_htmlDelegate;
	
	Plasma::DataEngine *m_publicTransportEngine;
	
	KDialog *m_infoDialog; // Stores a pointer to the service provider info dialog

	Q_DISABLE_COPY( StopSettingsDialog )
};

#endif // Multiple inclusion guard
