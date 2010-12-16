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

#include "stopsettingsdialog.h"

#include "htmldelegate.h"
#include "locationmodel.h"
#include "serviceprovidermodel.h"

#include <KMessageBox>
#include <KToolInvocation>
#include <KLineEdit>
#include <kdeversion.h>
// #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
//     #include <knewstuff3/downloaddialog.h>
// #else
//     #include <knewstuff2/engine.h>
// #endif

// #include <QMenu>
#include <QSortFilterProxyModel>

StopSettingsDialog::StopSettingsDialog( const StopSettings &stopSettings,
	    LocationModel *modelLocations,
	    ServiceProviderModel *modelServiceProviders,
	    Plasma::DataEngine *publicTransportEngine, QWidget *parent )
	    : KDialog( parent ),
	      m_modelLocations( modelLocations ),
	      m_modelServiceProviders( modelServiceProviders ),
	      m_modelLocationServiceProviders(0),
	      m_publicTransportEngine( publicTransportEngine ) {
    setWindowTitle( i18nc("@title:window", "Change Stop(s)") );
    m_uiStop.setupUi( mainWidget() );
    
    // Create model that filters service providers for the current location
    m_modelLocationServiceProviders = new QSortFilterProxyModel( this );
    m_modelLocationServiceProviders->setSourceModel( modelServiceProviders );
    m_modelLocationServiceProviders->setFilterRole( LocationCodeRole );

    m_uiStop.btnServiceProviderInfo->setIcon( KIcon("help-about") );
    m_uiStop.btnServiceProviderInfo->setText( QString() );
    
    m_uiStop.serviceProvider->setModel( m_modelLocationServiceProviders );
    m_uiStop.location->setModel( m_modelLocations );
    
    // Set html delegate
    m_htmlDelegate = new HtmlDelegate( HtmlDelegate::NoOption, this );
    m_htmlDelegate->setAlignText( true );
    m_uiStop.serviceProvider->setItemDelegate( m_htmlDelegate );
    m_uiStop.location->setItemDelegate( m_htmlDelegate );

    connect( this, SIGNAL(user1Clicked()), this, SLOT(geolocateClicked()) );
    connect( m_uiStop.location, SIGNAL(currentIndexChanged(int)),
	     this, SLOT(locationChanged(int)) );
    connect( m_uiStop.serviceProvider, SIGNAL(currentIndexChanged(int)),
	     this, SLOT(serviceProviderChanged(int)) );
    connect( m_uiStop.city, SIGNAL(currentIndexChanged(QString)),
	     this, SLOT(cityNameChanged(QString)) );
    connect( m_uiStop.btnServiceProviderInfo, SIGNAL(clicked()),
	     this, SLOT(clickedServiceProviderInfo()) );
    
    setStopSettings( stopSettings );
}

StopSettingsDialog::~StopSettingsDialog() {
}

void StopSettingsDialog::setStopSettings( const StopSettings& stopSettings ) {
    // Select location from stopSettings
    QModelIndex index = m_modelLocations->indexOfLocation(
	    stopSettings.location.isEmpty() ? KGlobal::locale()->country() : stopSettings.location );
    if ( index.isValid() )
	m_uiStop.location->setCurrentIndex( index.row() );
    else
	m_uiStop.location->setCurrentIndex( 0 );
    
    // Select service provider from stopSettings
    if ( !stopSettings.serviceProviderID.isEmpty() ) {
	QModelIndexList indices = m_uiStop.serviceProvider->model()->match(
		m_uiStop.serviceProvider->model()->index(0, 0), ServiceProviderIdRole,
		stopSettings.serviceProviderID, 1, Qt::MatchFixedString );
	
	if ( !indices.isEmpty() ) {
	    int curServiceProviderIndex = indices.first().row();
	    m_uiStop.serviceProvider->setCurrentIndex( curServiceProviderIndex );

	    QVariantHash curServiceProviderData = m_uiStop.serviceProvider->itemData(
		    curServiceProviderIndex, ServiceProviderDataRole ).toHash();
	    if ( curServiceProviderData["useSeparateCityValue"].toBool() ) {
		if ( curServiceProviderData["onlyUseCitiesInList"].toBool() )
		    m_uiStop.city->setCurrentItem( stopSettings.city );
		else
		    m_uiStop.city->setEditText( stopSettings.city );
	    } else
		m_uiStop.city->setCurrentItem( QString() );
	}
    }
}

StopSettings StopSettingsDialog::stopSettings() const {
    StopSettings stopSettings;

    QVariantHash serviceProviderData = m_uiStop.serviceProvider->itemData(
	m_uiStop.serviceProvider->currentIndex(), ServiceProviderDataRole ).toHash();
//     QVariantHash serviceProviderData = m_modelLocationServiceProviders->index(
// 	    m_uiStop.serviceProvider->currentIndex(), 0 )
// 	    .data( ServiceProviderDataRole ).toHash();
    stopSettings.serviceProviderID = serviceProviderData["id"].toString();
    stopSettings.location = m_uiStop.location->itemData(
	m_uiStop.location->currentIndex(), LocationCodeRole ).toString();

    if ( serviceProviderData["useSeparateCityValue"].toBool() ) {
	stopSettings.city = m_uiStop.city->isEditable()
		? m_uiStop.city->lineEdit()->text() : m_uiStop.city->currentText();
    }
    
    return stopSettings;
}

void StopSettingsDialog::updateServiceProviderModel( int index ) {
    // Filter service providers for the given locationText
    QString locationCode = m_uiStop.location->itemData(index, LocationCodeRole).toString();
    if ( locationCode == "showAll" ) {
	m_modelLocationServiceProviders->setFilterRegExp( QString() );
    } else {
	m_modelLocationServiceProviders->setFilterRegExp(
		QString("%1|international|unknown").arg(locationCode) );
    }
}

void StopSettingsDialog::locationChanged( int index ) {
    updateServiceProviderModel( index );

    // Select default accessor of the selected location
    QString locationCode = m_uiStop.location->itemData( index, LocationCodeRole ).toString();
    Plasma::DataEngine::Data locationData = m_publicTransportEngine->query( "Locations" );
    QString defaultServiceProviderId =
	    locationData[locationCode].toHash()["defaultAccessor"].toString();
    if ( !defaultServiceProviderId.isEmpty() ) {
	QModelIndexList indices = m_uiStop.serviceProvider->model()->match(
		m_uiStop.serviceProvider->model()->index(0, 0), ServiceProviderIdRole,
		defaultServiceProviderId, 1, Qt::MatchFixedString );
	if ( !indices.isEmpty() ) {
	    int curServiceProviderIndex = indices.first().row();
	    m_uiStop.serviceProvider->setCurrentIndex( curServiceProviderIndex );
	    serviceProviderChanged( curServiceProviderIndex );
	}
    }
}

void StopSettingsDialog::serviceProviderChanged( int index ) {
    QVariantHash serviceProviderData =
	    m_uiStop.serviceProvider->model()->index( index, 0 )
	    .data( ServiceProviderDataRole ).toHash();

// TODO: Show warning message in main config dialog, if not all selected stops support arrivals
//     bool supportsArrivals = serviceProviderData["features"].toStringList().contains("Arrivals");
//     m_ui.showArrivals->setEnabled( supportsArrivals );
//     if ( !supportsArrivals )
// 	m_ui.showDepartures->setChecked( true );
    
    bool useSeparateCityValue = serviceProviderData["useSeparateCityValue"].toBool();
    m_uiStop.lblCity->setVisible( useSeparateCityValue );
    m_uiStop.city->setVisible( useSeparateCityValue );
    
    if ( useSeparateCityValue ) {
	m_uiStop.city->clear();
	QStringList cities = serviceProviderData["cities"].toStringList();
	if ( !cities.isEmpty() ) {
	    cities.sort();
	    m_uiStop.city->addItems( cities );
	    m_uiStop.city->setEditText( cities.first() );
	}
	m_uiStop.city->setEditable( !serviceProviderData["onlyUseCitiesInList"].toBool() );
    } else
	m_uiStop.city->setEditText( QString() );
}

void StopSettingsDialog::cityNameChanged( const QString& /*cityName*/ ) {
//     QVariantHash serviceProviderData = m_uiStop.serviceProvider->model()->index(
// 	    m_uiStop.serviceProvider->currentIndex(), 0 )
// 	    .data( ServiceProviderDataRole ).toHash();
//     bool useSeparateCityValue = serviceProviderData["useSeparateCityValue"].toBool();
//     QString serviceProviderID = serviceProviderData["id"].toString();
//     if ( !useSeparateCityValue )
// 	return; // City value not used by service provider
	
//     QString testSource = QString("Stops %1|stop=%2|city=%3").arg( serviceProviderID )
// 	    .arg( m_uiStop.stop->text() ).arg( cityName );
//     m_dataSourceTester->setTestSource( testSource ); TODO
}

QString StopSettingsDialog::currentCityValue() const {
    if ( m_uiStop.city->isEditable() )
	return m_uiStop.city->lineEdit()->text();
    else
	return m_uiStop.city->currentText();
}

void StopSettingsDialog::clickedServiceProviderInfo() {
    QWidget *widget = new QWidget;
    m_uiAccessorInfo.setupUi( widget );

    KDialog *infoDialog = new KDialog( this );
    infoDialog->setModal( true );
    infoDialog->setButtons( KDialog::Ok );
    infoDialog->setMainWidget( widget );
    infoDialog->setWindowTitle( i18nc("@title:window", "Service Provider Information") );
    infoDialog->setWindowIcon( KIcon("help-about") );

    QVariantHash serviceProviderData = m_uiStop.serviceProvider->model()->index(
	    m_uiStop.serviceProvider->currentIndex(), 0 )
	    .data( ServiceProviderDataRole ).toHash();
    QIcon favIcon = m_uiStop.serviceProvider->itemIcon(
	    m_uiStop.serviceProvider->currentIndex() );
    m_uiAccessorInfo.icon->setPixmap( favIcon.pixmap(32) );
    m_uiAccessorInfo.serviceProviderName->setText(
	    m_uiStop.serviceProvider->currentText() );
    m_uiAccessorInfo.version->setText( i18nc("@info/plain", "Version %1",
					    serviceProviderData["version"].toString()) );
    m_uiAccessorInfo.url->setUrl( serviceProviderData["url"].toString() );
    m_uiAccessorInfo.url->setText( QString("<a href='%1'>%1</a>").arg(
	    serviceProviderData["url"].toString() ) );

    m_uiAccessorInfo.fileName->setUrl( serviceProviderData["fileName"].toString() );
    m_uiAccessorInfo.fileName->setText( QString("<a href='%1'>%1</a>").arg(
	    serviceProviderData["fileName"].toString() ) );

    QString scriptFileName = serviceProviderData["scriptFileName"].toString();
    if ( scriptFileName.isEmpty() ) {
	m_uiAccessorInfo.lblScriptFileName->setVisible( false );
	m_uiAccessorInfo.scriptFileName->setVisible( false );
    } else {
	m_uiAccessorInfo.lblScriptFileName->setVisible( true );
	m_uiAccessorInfo.scriptFileName->setVisible( true );
	m_uiAccessorInfo.scriptFileName->setUrl( scriptFileName );
	m_uiAccessorInfo.scriptFileName->setText( QString("<a href='%1'>%1</a>")
		.arg(scriptFileName) );
    }

    if ( serviceProviderData["email"].toString().isEmpty() )
	m_uiAccessorInfo.author->setText( serviceProviderData["author"].toString() );
    else {
	m_uiAccessorInfo.author->setText( QString("<a href='mailto:%2'>%1</a>")
		.arg( serviceProviderData["author"].toString() )
		.arg( serviceProviderData["email"].toString() ) );
	m_uiAccessorInfo.author->setToolTip( i18nc("@info",
		"Write an email to <email address='%2'>%1</email>",
		serviceProviderData["author"].toString(),
		serviceProviderData["email"].toString()) );
    }
    m_uiAccessorInfo.description->setText( serviceProviderData["description"].toString() );
    m_uiAccessorInfo.features->setText( serviceProviderData["featuresLocalized"].toStringList().join(", ") );

    connect( m_uiAccessorInfo.btnOpenInTimetableMate, SIGNAL(clicked()),
	     this, SLOT(openInTimetableMate()) );
    
    infoDialog->show();
}

void StopSettingsDialog::openInTimetableMate()
{
    QVariantHash serviceProviderData = m_uiStop.serviceProvider->model()->index(
	    m_uiStop.serviceProvider->currentIndex(), 0 )
	    .data( ServiceProviderDataRole ).toHash();
    QString error;
    int result = KToolInvocation::startServiceByDesktopName("timetablemate",
	    serviceProviderData["fileName"].toString(), &error);
    if ( result != 0 ) {
	KMessageBox::error(m_infoDialog, i18nc("@info",
		"TimetableMate couldn't be started, error message was: '%1'", error));
    }
}
