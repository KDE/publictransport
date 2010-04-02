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
#include "ui_stopConfigDetails.h"
#include "ui_accessorInfo.h"

#include "global.h"
#include "stopfinder.h"

#if KDE_VERSION >= KDE_MAKE_VERSION(4,4,0)
#define USE_KCATEGORYVIEW // Comment out to not use KCategoryView, which causes problems with KDE 4.3
#endif

class DynamicLabeledLineEditList;
class HtmlDelegate;
class QStandardItemModel;
class KLineEdit;
class NearStopsDialog;
class StopSettingsDialog : public KDialog {
    Q_OBJECT
    
    public:
	StopSettingsDialog( const StopSettings &stopSettings,
			    const QStringList &filterConfigurations,
			    QStandardItemModel *modelLocations,
			    QStandardItemModel *modelServiceProviders,
			    Plasma::DataEngine *publicTransportEngine,
			    Plasma::DataEngine *osmEngine,
			    Plasma::DataEngine *geolocationEngine,
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
	void locationChanged( const QString &newLocation );
	/** The info button has been clicked. This shows information about the
	* currently selected service provider in a dialog. */
	void clickedServiceProviderInfo();
	void geolocateClicked();
	/** The stop name has been edited. */
	void stopNameEdited( const QString &text, int widgetIndex );
	/** Another combined stop has been added. */
	void stopAdded( QWidget *lineEdit );
	void downloadServiceProvidersClicked();
	void installServiceProviderClicked();
	void nearStopsDialogFinished( int result );
	void adjustStopListLayout();
	
	void stopFinderGeolocationData( const QString &countryCode, const QString &city,
			      qreal latitude, qreal longitude, int accuracy );
	void stopFinderError( StopFinder::Error error, const QString &errorMessage );
	void stopFinderFinished();
	void stopFinderFoundStops( const QStringList &stops, const QStringList &stopIDs,
				   const QString &serviceProviderID );
	
	/** The data from the data engine was updated. */
	void dataUpdated( const QString& sourceName,
			  const Plasma::DataEngine::Data &data );

    protected:
	virtual void resizeEvent( QResizeEvent* );
	virtual void accept();
	
    private:
	/** Updates the service provider model by inserting service provider for the
	* current location. */
	void updateServiceProviderModel( const QString &locationText = QString() );
	QString currentCityValue() const;
	void requestStopSuggestions( int stopIndex );
	void processStopSuggestions( const Plasma::DataEngine::Data& data );
	
	Ui::publicTransportStopConfig m_uiStop;
	Ui::stopConfigDetails m_uiStopDetails;
	Ui::accessorInfo m_uiAccessorInfo;

	StopFinder *m_stopFinder;
	NearStopsDialog *m_nearStopsDialog;
	QString m_stopFinderServiceProviderID;
	
	QStandardItemModel *m_modelLocations; // Model of locations
	QStandardItemModel *m_modelServiceProviders; // Model of service providers
	QSortFilterProxyModel *m_modelLocationServiceProviders; // Model of service providers for the current location
	HtmlDelegate *m_htmlDelegate;
	DynamicLabeledLineEditList *m_stopList;
	
	Plasma::DataEngine *m_publicTransportEngine, *m_osmEngine, *m_geolocationEngine;
	
	QHash< QString, QVariant > m_stopToStopID; /**< A hash with stop names as 
				* keys and the corresponding stop IDs as values. */

#ifdef USE_KCATEGORYVIEW
#if KDE_VERSION < KDE_MAKE_VERSION(4,4,0)
	class KCategoryDrawer;
	KCategoryDrawer *categoryDrawer; // not derived from QObject before KDE 4.4
#endif
#endif
    private:
	Q_DISABLE_COPY( StopSettingsDialog )
};

#endif // Multiple inclusion guard
