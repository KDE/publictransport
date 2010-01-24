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

// Qt includes
#include <QListWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include <QMenu>
#include <QProcess>

// KDE includes
#include <KTabWidget>
#include <KConfigDialog>
#include <KColorScheme>
#include <KFileDialog>
#include <KStandardDirs>
#include <KMessageBox>

#include <kdeversion.h>
#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
  #include <knewstuff3/downloaddialog.h>
#else
  #include <knewstuff2/engine.h>
#endif

// Own includes
#include "settings.h"
#include "htmldelegate.h"

#define NO_EXPORT_PLASMA_APPLET
#include "publictransport.h"
#undef NO_EXPORT_PLASMA_APPLET


PublicTransportSettings::PublicTransportSettings( PublicTransport *applet )
	: m_configDialog(0), m_modelServiceProvider(0), m_modelLocations(0) {
    m_applet = applet;

    m_dataSourceTester = new DataSourceTester( "", m_applet );
    connect( m_dataSourceTester,
	     SIGNAL(testResult(DataSourceTester::TestResult,const QVariant&,const QVariant&)),
	     this, SLOT(testResult(DataSourceTester::TestResult,const QVariant&,const QVariant&)) );
}

void PublicTransportSettings::dataUpdated( const QString& sourceName,
					   const Plasma::DataEngine::Data& data ) {
    if ( data.isEmpty() )
	return;

    if ( sourceName.contains(QRegExp("^http")) ) {
	if ( m_modelServiceProvider == NULL )
	    return;

	QPixmap favicon(QPixmap::fromImage(data["Icon"].value<QImage>()));
	if ( !favicon.isNull() ) {
	    if ( m_applet->testState(AccessorInfoDialogShown) )
		m_uiAccessorInfo.icon->setPixmap( favicon );
	    if ( m_applet->testState(ConfigDialogShown) ) {
		for ( int i = 0; i < m_modelServiceProvider->rowCount(); ++i ) {
		    QHash< QString, QVariant > serviceProviderData = m_modelServiceProvider->item(i)->data( ServiceProviderDataRole ).toHash();
		    QString favIconSource = serviceProviderData["url"].toString();
		    if ( favIconSource.compare( sourceName ) == 0 )
			m_modelServiceProvider->item(i)->setIcon( KIcon(favicon) );
		}
	    }
	}
	else
	    kDebug() << "favicon is NULL";

	m_applet->dataEngine("favicons")->disconnectSource( sourceName, this );
    }
}

void PublicTransportSettings::readSettings() {
    KConfigGroup cg = m_applet->config();
    m_updateTimeout = cg.readEntry("updateTimeout", 60);
    m_showRemainingMinutes = cg.readEntry("showRemainingMinutes", true);
    m_showDepartureTime = cg.readEntry("showDepartureTime", true);
    m_displayTimeBold = cg.readEntry("displayTimeBold", true);
    m_serviceProvider = cg.readEntry("serviceProvider", "de_db"); // "de_db" is "Germany (db.de)" ("Deutsche Bahn")
    if ( cg.hasKey("location") )
	m_location = cg.readEntry("location", "showAll");
    else
	m_location = KGlobal::locale()->country();
    m_city = cg.readEntry("city", "");
    m_stop = cg.readEntry("stop", "");
    m_stopID = cg.readEntry("stopID", "");
    m_timeOffsetOfFirstDeparture = cg.readEntry("timeOffsetOfFirstDeparture", 0);
    m_timeOfFirstDepartureCustom = QTime::fromString(
	    cg.readEntry("timeOfFirstDepartureCustom", "12:00"), "hh:mm" );
    m_firstDepartureConfigMode = static_cast<FirstDepartureConfigMode>(
	    cg.readEntry("firstDepartureConfigMode",
			 static_cast<int>(RelativeToCurrentTime)) );
    m_maximalNumberOfDepartures = cg.readEntry("maximalNumberOfDepartures", 20);
    m_alarmTime = cg.readEntry("alarmTime", 5);
    m_linesPerRow = cg.readEntry("linesPerRow", 2);
    m_size = cg.readEntry("size", 2);
    m_departureArrivalListType = static_cast<DepartureArrivalListType>(
	    cg.readEntry("departureArrivalListType", static_cast<int>(DepartureList)) );
    m_showHeader = cg.readEntry("showHeader", true);
    m_hideColumnTarget = cg.readEntry("hideColumnTarget", false);

    QString fontFamily = cg.readEntry("fontFamily", "");
    m_useDefaultFont = fontFamily.isEmpty();
    if ( !m_useDefaultFont )
	m_font = QFont( fontFamily );

    // Read filter settings
    m_filterConfiguration = cg.readEntry("filterConfiguration", "Default");
    m_filterConfigurationList = cg.readEntry("filterConfigurationList", QStringList() << "Default");
    if ( !m_filterConfigurationList.contains("Default") )
	m_filterConfigurationList.prepend( "Default" );
    m_filterConfigChanged = isCurrentFilterConfigChanged();
    kDebug() << "Read filter config settings:" << m_filterConfiguration
	     << "changed?" << m_filterConfigChanged
	     << "list" << m_filterConfigurationList;
    
    m_showTypeOfVehicle[Unknown] = cg.readEntry(vehicleTypeToConfigName(Unknown), true);
    m_showTypeOfVehicle[Tram] = cg.readEntry(vehicleTypeToConfigName(Tram), true);
    m_showTypeOfVehicle[Bus] = cg.readEntry(vehicleTypeToConfigName(Bus), true);
    m_showTypeOfVehicle[Subway] = cg.readEntry(vehicleTypeToConfigName(Subway), true);
    m_showTypeOfVehicle[Metro] = cg.readEntry(vehicleTypeToConfigName(Metro), true);
    m_showTypeOfVehicle[TrolleyBus] = cg.readEntry(vehicleTypeToConfigName(TrolleyBus), true);
    m_showTypeOfVehicle[TrainInterurban] = cg.readEntry(vehicleTypeToConfigName(TrainInterurban), true);
    m_showTypeOfVehicle[TrainRegional] = cg.readEntry(vehicleTypeToConfigName(TrainRegional), true);
    m_showTypeOfVehicle[TrainRegionalExpress] = cg.readEntry(vehicleTypeToConfigName(TrainRegionalExpress), true);
    m_showTypeOfVehicle[TrainInterregio] = cg.readEntry(vehicleTypeToConfigName(TrainInterregio), true);
    m_showTypeOfVehicle[TrainIntercityEurocity] = cg.readEntry(vehicleTypeToConfigName(TrainIntercityEurocity), true);
    m_showTypeOfVehicle[TrainIntercityExpress] = cg.readEntry(vehicleTypeToConfigName(TrainIntercityExpress), true);
    m_showTypeOfVehicle[Ferry] = cg.readEntry(vehicleTypeToConfigName(Ferry), true);
    m_showTypeOfVehicle[Plane] = cg.readEntry(vehicleTypeToConfigName(Plane), true);
    m_showNightlines = cg.readEntry("showNightlines", true);
    m_filterMinLine = cg.readEntry("filterMinLine", 1);
    m_filterMaxLine = cg.readEntry("filterMaxLine", 999);
    m_filterTypeTarget = static_cast<FilterType>(cg.readEntry("filterTypeTarget", static_cast<int>(ShowAll)));
    m_filterTargetList = cg.readEntry("filterTargetList", QStringList());
    m_filterTypeLineNumber = static_cast<FilterType>(cg.readEntry("filterTypeLineNumber", static_cast<int>(ShowAll)));
    m_filterLineNumberList = cg.readEntry("filterLineNumberList", QStringList());
    
    getServiceProviderInfo();
//     selectLocaleLocation();
}

bool PublicTransportSettings::isCurrentFilterConfigChanged() {
    return isCurrentFilterConfigChangedFrom( m_applet->config(
	    "filterConfig_" + m_filterConfiguration) );
}

bool PublicTransportSettings::isCurrentFilterConfigChangedFrom( const KConfigGroup& cg ) {
    kDebug() << "Changed?" << cg.name();
    if ( m_showTypeOfVehicle[Unknown] != cg.readEntry(vehicleTypeToConfigName(Unknown), true) )
	return true;
    if ( m_showTypeOfVehicle[Tram] != cg.readEntry(vehicleTypeToConfigName(Tram), true) )
	return true;
    if ( m_showTypeOfVehicle[Bus] != cg.readEntry(vehicleTypeToConfigName(Bus), true) )
	return true;
    if ( m_showTypeOfVehicle[Subway] != cg.readEntry(vehicleTypeToConfigName(Subway), true) )
	return true;
    if ( m_showTypeOfVehicle[Metro] != cg.readEntry(vehicleTypeToConfigName(Metro), true) )
	return true;
    if ( m_showTypeOfVehicle[TrolleyBus] !=
		cg.readEntry(vehicleTypeToConfigName(TrolleyBus), true) )
	return true;
    if ( m_showTypeOfVehicle[TrainInterurban] !=
		cg.readEntry(vehicleTypeToConfigName(TrainInterurban), true) )
	return true;
    if ( m_showTypeOfVehicle[TrainRegional] !=
		cg.readEntry(vehicleTypeToConfigName(TrainRegional), true) )
	return true;
    if ( m_showTypeOfVehicle[TrainRegionalExpress] !=
		cg.readEntry(vehicleTypeToConfigName(TrainRegionalExpress), true) )
	return true;
    if ( m_showTypeOfVehicle[TrainInterregio] !=
		cg.readEntry(vehicleTypeToConfigName(TrainInterregio), true) )
	return true;
    if ( m_showTypeOfVehicle[TrainIntercityEurocity] !=
		cg.readEntry(vehicleTypeToConfigName(TrainIntercityEurocity), true) )
	return true;
    if ( m_showTypeOfVehicle[TrainIntercityExpress] !=
		cg.readEntry(vehicleTypeToConfigName(TrainIntercityExpress), true) )
	return true;
    if ( m_showTypeOfVehicle[Ferry] != cg.readEntry(vehicleTypeToConfigName(Ferry), true) )
	return true;
    if ( m_showTypeOfVehicle[Plane] != cg.readEntry(vehicleTypeToConfigName(Plane), true) )
	return true;
    
    if ( m_filterTypeTarget != static_cast<FilterType>(
		cg.readEntry("filterTypeTarget", static_cast<int>(ShowAll))) )
	return true;
    if ( m_filterTargetList != cg.readEntry("filterTargetList", QStringList()) )
	return true;
    if ( m_filterTypeLineNumber != static_cast<FilterType>(
		cg.readEntry("filterTypeLineNumber", static_cast<int>(ShowAll))) )
	return true;
    if ( m_filterLineNumberList != cg.readEntry("filterLineNumberList", QStringList()) )
	return true;

    kDebug() << "Not changed";
    return false;
}

void PublicTransportSettings::getServiceProviderInfo() {
    Plasma::DataEngine::Data data =
	    m_applet->dataEngine("publictransport")->query("ServiceProviders");
    foreach ( QString serviceProviderName, data.keys() ) {
	QHash< QString, QVariant > serviceProviderData =
		data.value(serviceProviderName).toHash();
	if ( serviceProvider() == serviceProviderData["id"].toString() ) {
	    m_useSeperateCityValue = serviceProviderData["useSeperateCityValue"].toBool();
	    m_onlyUseCitiesInList = serviceProviderData["onlyUseCitiesInList"].toBool();
	    m_serviceProviderFeatures = serviceProviderData["features"].toStringList();
	    break;
	}
    }
}

void PublicTransportSettings::setShowHeader( bool showHeader ) {
    m_showHeader = showHeader;

    KConfigGroup cg = m_applet->config();
    cg.writeEntry( "showHeader", m_showHeader );
//     emit settingsChanged();
    emit configNeedsSaving();
}

void PublicTransportSettings::setHideColumnTarget( bool hideColumnTarget ) {
    m_hideColumnTarget = hideColumnTarget;

    KConfigGroup cg = m_applet->config();
    cg.writeEntry( "hideColumnTarget", m_hideColumnTarget );
//     qDebug() << "PublicTransportSettings::setHideColumnTarget" << hideColumnTarget;
//     emit settingsChanged();
    emit configNeedsSaving();
}

void PublicTransportSettings::filterLineTypeAvailableSelectionChanged( int ) {
    m_uiFilter.filterLineType->setButtonsEnabled();
}

void PublicTransportSettings::filterLineTypeSelectedSelectionChanged( int ) {
    m_uiFilter.filterLineType->setButtonsEnabled();
}

void PublicTransportSettings::addedFilterLineType( QListWidgetItem* ) {
    m_uiFilter.filterLineType->setButtonsEnabled();
    setFilterConfigurationChanged();
}

void PublicTransportSettings::removedFilterLineType( QListWidgetItem* ) {
    m_uiFilter.filterLineType->setButtonsEnabled();
    setFilterConfigurationChanged();
}

QList< VehicleType > PublicTransportSettings::filteredOutVehicleTypes() const {
    return m_showTypeOfVehicle.keys( false );
}

void PublicTransportSettings::removeAllFiltersByVehicleType() {
    // TODO: go through all vehicle types
    m_showTypeOfVehicle[Unknown] = true;
    m_showTypeOfVehicle[Tram] = true;
    m_showTypeOfVehicle[Bus] = true;
    m_showTypeOfVehicle[Subway] = true;
    m_showTypeOfVehicle[Metro] = true;
    m_showTypeOfVehicle[TrolleyBus] = true;
    m_showTypeOfVehicle[TrainInterurban] = true;
    m_showTypeOfVehicle[TrainRegional] = true;
    m_showTypeOfVehicle[TrainRegionalExpress] = true;
    m_showTypeOfVehicle[TrainInterregio] = true;
    m_showTypeOfVehicle[TrainIntercityEurocity] = true;
    m_showTypeOfVehicle[TrainIntercityExpress] = true;
    m_showTypeOfVehicle[Ferry] = true;
    m_showTypeOfVehicle[Plane] = true;

    KConfigGroup cg = m_applet->config();
    cg.writeEntry(vehicleTypeToConfigName(Unknown), true);
    cg.writeEntry(vehicleTypeToConfigName(Tram), true);
    cg.writeEntry(vehicleTypeToConfigName(Bus), true);
    cg.writeEntry(vehicleTypeToConfigName(Subway), true);
    cg.writeEntry(vehicleTypeToConfigName(Metro), true);
    cg.writeEntry(vehicleTypeToConfigName(TrolleyBus), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainInterurban), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainRegional), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainRegionalExpress), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainInterregio), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainIntercityEurocity), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainIntercityExpress), true);
    cg.writeEntry(vehicleTypeToConfigName(Ferry), true);
    cg.writeEntry(vehicleTypeToConfigName(Plane), true);
    //     emit settingsChanged();
    emit configNeedsSaving();
    emit modelNeedsUpdate();

    setFilterConfigurationChanged();
    // TODO: Synchronize new settings with config dialog widgets
}

// TODO: Plasma::DataEngine::Data instead of QVariant?
void PublicTransportSettings::testResult( DataSourceTester::TestResult result,
					  const QVariant &data, const QVariant &data2 ) {
    //     qDebug() << "PublicTransport::testResult";
    if ( !m_applet->testState(ConfigDialogShown) )
	return;

    QStringList stops;
    QHash< QString, QVariant > stopToStopID;
    switch ( result ) {
	case DataSourceTester::Error:
	    setStopNameValid( false, data.toString() );
	    break;

	case DataSourceTester::JourneyListReceived:
	    setStopNameValid( true );
	    m_ui.stop->setCompletedItems( QStringList() );
	    break;

	case DataSourceTester::PossibleStopsReceived:
	    setStopNameValid( false, i18n("The stop name is ambiguous.") );
	    stops = data.toStringList();
	    stopToStopID = data2.toHash();
	    kDebug() << "Set" << stopToStopID.count() << "suggestions.";
	    m_ui.stop->setCompletedItems( stops );
	    m_stopIDinConfig = stopToStopID.value( m_ui.stop->text(), QString() ).toString();
	    break;
    }
}

void PublicTransportSettings::setFilterTypeTarget( FilterType filterType ) {
    m_filterTypeTarget = filterType;

    m_applet->config().writeEntry("filterTypeTarget", static_cast<int>(m_filterTypeTarget));
    emit configNeedsSaving();

    // Synchronize new settings with config dialog widgets
    if ( m_applet->testState(ConfigDialogShown) )
	m_uiFilter.filterTypeTarget->setCurrentIndex( static_cast<int>(m_filterTypeTarget) );
}

void PublicTransportSettings::setFilterTypeLineNumber( FilterType filterType ) {
    m_filterTypeLineNumber = filterType;

    m_applet->config().writeEntry("filterTypeLineNumber", static_cast<int>(m_filterTypeLineNumber));
    emit configNeedsSaving();

    // Synchronize new settings with config dialog widgets
    if ( m_applet->testState(ConfigDialogShown) )
	m_uiFilter.filterTypeLineNumber->setCurrentIndex( static_cast<int>(m_filterTypeLineNumber) );
}

void PublicTransportSettings::configDialogFinished() {
    m_dataSourceTester->setTestSource("");
    m_applet->removeState( ConfigDialogShown );
    m_configDialog = NULL;
}

void PublicTransportSettings::accessorInfoDialogFinished() {
    m_applet->removeState( AccessorInfoDialogShown );
}

bool PublicTransportSettings::checkConfig() {
    if ( m_useSeperateCityValue && (m_city.isEmpty() || m_stop.isEmpty()) )
	emit configurationRequired(true, i18n("Please set a city and a stop."));
    else if ( m_stop.isEmpty() )
	emit configurationRequired(true, i18n("Please set a stop."));
    else if ( m_serviceProvider == "" )
	emit configurationRequired(true, i18n("Please select a service provider."));
    else if ( m_filterMinLine > m_filterMaxLine )
	emit configurationRequired(true, i18n("The minimal shown line can't be bigger than the maximal shown line."));
    else {
	emit configurationRequired(false);
	return true;
    }

    return false;
}

void PublicTransportSettings::setStopNameValid( bool valid, const QString &toolTip ) {
    // For safety's sake:
    if ( !m_applet->testState(ConfigDialogShown) )
	return;

    if ( valid ) {
	m_ui.kledStopValidated->setState( KLed::On );
	m_ui.kledStopValidated->setToolTip( i18n("The stop name is valid.") );
    } else {
	m_ui.kledStopValidated->setState( KLed::Off );
	m_ui.kledStopValidated->setToolTip( toolTip );
    }
}

QString PublicTransportSettings::configCityValue() const {
    if ( m_ui.city->isEditable() )
	return m_ui.city->lineEdit()->text();
    else
	return m_ui.city->currentText();
}

void PublicTransportSettings::serviceProviderChanged( int index ) {
    QHash< QString, QVariant > serviceProviderData = m_modelServiceProvider->item( index )->data( ServiceProviderDataRole ).toHash();
    kDebug() << "New service provider" << serviceProviderData["name"].toString();

    m_stopIDinConfig = "";
    m_dataSourceTester->clearStopToStopIdMap();

    m_ui.kledStopValidated->setState( KLed::Off );
    m_ui.kledStopValidated->setToolTip( i18n("Checking validity of the stop name.") );

    // Only show "Departures"/"Arrivals"-radio buttons if arrivals are supported by the service provider
    bool supportsArrivals =
	serviceProviderData["features"].toStringList().contains("Arrivals");
    m_uiAdvanced.grpDefaultView->setVisible( supportsArrivals );
    if ( !supportsArrivals )
	m_uiAdvanced.showDepartures->setChecked( true );

    bool useSeperateCityValue = serviceProviderData["useSeperateCityValue"].toBool();
    m_ui.lblCity->setVisible( useSeperateCityValue );
    m_ui.city->setVisible( useSeperateCityValue );

    if ( useSeperateCityValue ) {
	m_ui.city->clear();
	QStringList cities = serviceProviderData["cities"].toStringList();
	if ( !cities.isEmpty() ) {
	    // 	    m_ui.city->setCompletedItems( cities, false );
	    cities.sort();
	    m_ui.city->addItems( cities );
	    m_ui.city->setEditText( cities.first() );
	}
	m_ui.city->setEditable( !serviceProviderData["onlyUseCitiesInList"].toBool() );
    } else
	m_ui.city->setEditText( "" );

    stopNameChanged( m_ui.stop->text() );
}

void PublicTransportSettings::cityNameChanged( const QString &cityName ) {
    QHash< QString, QVariant > serviceProviderData = m_modelServiceProvider->item( m_ui.serviceProvider->currentIndex() )->data( ServiceProviderDataRole ).toHash();
    bool useSeperateCityValue = serviceProviderData["useSeperateCityValue"].toBool();
    QString serviceProviderID = serviceProviderData["id"].toString();

    if ( !useSeperateCityValue )
	return; // City value not used by service provider

    QString testSource = QString("%5 %1|stop=%2|maxDeps=%3|timeOffset=%4|city=%6")
	.arg( serviceProviderID )
	.arg( m_stopIDinConfig.isEmpty() ? m_ui.stop->text() : m_stopIDinConfig )
	.arg( m_uiAdvanced.maximalNumberOfDepartures->value() )
	.arg( m_uiAdvanced.timeOfFirstDeparture->value() )
	.arg( m_uiAdvanced.showArrivals->isChecked() ? "Arrivals" : "Departures" )
	.arg( cityName );
    m_dataSourceTester->setTestSource( testSource );
}

void PublicTransportSettings::stopNameChanged( const QString &stopName ) {
    m_stopIDinConfig = m_dataSourceTester->stopToStopID( stopName );

    // TODO: prevent crash, when no service provider data is available
    QHash< QString, QVariant > serviceProviderData = m_modelServiceProvider->item( m_ui.serviceProvider->currentIndex() )->data( ServiceProviderDataRole ).toHash();
    bool useSeperateCityValue = serviceProviderData["useSeperateCityValue"].toBool();
    QString serviceProviderID = serviceProviderData["id"].toString();

    QString testSource = QString("Stops %1|stop=%2")
	.arg( serviceProviderID )
	.arg( stopName );
//     QString testSource = QString("%5 %1|stop=%2|maxDeps=%3|timeOffset=%4")
// 	.arg( serviceProviderID )
// 	.arg( m_stopIDinConfig.isEmpty() ? stopName : m_stopIDinConfig )
// 	.arg( m_uiAdvanced.maximalNumberOfDepartures->value() )
// 	.arg( m_uiAdvanced.timeOfFirstDeparture->value() )
// 	.arg( m_uiAdvanced.showArrivals->isChecked() ? "Arrivals" : "Departures" );
    if ( useSeperateCityValue )
	testSource += QString("|city=%1").arg( configCityValue() );
    m_dataSourceTester->setTestSource( testSource );
}

void PublicTransportSettings::clickedServiceProviderInfo( bool ) {
    QWidget *widget = new QWidget;
    m_uiAccessorInfo.setupUi( widget );
    m_applet->addState( AccessorInfoDialogShown );

    KDialog *infoDialog = new KDialog( m_configDialog );
    infoDialog->setModal( true );
    infoDialog->setButtons( KDialog::Ok );
    infoDialog->setMainWidget( widget );
    infoDialog->setWindowTitle( i18n("Service provider info") );
    infoDialog->setWindowIcon( KIcon("help-about") );
    connect( infoDialog, SIGNAL(finished()), this, SLOT(accessorInfoDialogFinished()) );

    QHash< QString, QVariant > serviceProviderData = m_modelServiceProvider->item(
	    m_ui.serviceProvider->currentIndex() )->data( ServiceProviderDataRole ).toHash();
    QIcon favIcon = m_ui.serviceProvider->itemIcon( m_ui.serviceProvider->currentIndex() );
    m_uiAccessorInfo.icon->setPixmap( favIcon.pixmap(32) );
    m_uiAccessorInfo.serviceProviderName->setText( m_ui.serviceProvider->currentText() );
    m_uiAccessorInfo.version->setText( i18n("Version %1", serviceProviderData["version"].toString()) );
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
	m_uiAccessorInfo.author->setText( QString("%1").arg( serviceProviderData["author"].toString() ) );
    else {
	m_uiAccessorInfo.author->setText( QString("<a href='mailto:%2'>%1</a>")
		.arg( serviceProviderData["author"].toString() )
		.arg( serviceProviderData["email"].toString() ) );
	m_uiAccessorInfo.author->setToolTip( i18n("Write an email to %1 <%2>")
		.arg( serviceProviderData["author"].toString() )
		.arg( serviceProviderData["email"].toString() ) );
    }
    m_uiAccessorInfo.description->setText( serviceProviderData["description"].toString() );
    m_uiAccessorInfo.features->setText( serviceProviderData["featuresLocalized"].toStringList().join(", ") );

    infoDialog->show();
}

void PublicTransportSettings::createConfigurationInterface( KConfigDialog* parent,
							    bool stopNameValid ) {
    m_configDialog = parent;
    m_applet->addState( ConfigDialogShown );
    parent->setButtons( KDialog::Ok | KDialog::Cancel | KDialog::Apply );

    QWidget *widget = new QWidget;
    QWidget *widgetAdvanced = new QWidget;
    QWidget *widgetAppearance = new QWidget;
    QWidget *widgetFilter = new QWidget;
    m_ui.setupUi(widget);
    m_uiAdvanced.setupUi(widgetAdvanced);
    m_uiAppearance.setupUi(widgetAppearance);
    m_uiFilter.setupUi(widgetFilter);

    KTabWidget *tabMain = new KTabWidget;
    tabMain->addTab(widget, i18n("&Stop selection"));
    tabMain->addTab(widgetAdvanced, i18n("&Advanced"));

    parent->addPage(tabMain, i18n("General"), "public-transport-stop");
    parent->addPage(widgetAppearance, i18n("Appearance"), "package_settings_looknfeel");
    parent->addPage(widgetFilter, i18n("Filter"), "view-filter");

    QColor textColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor );
    QPalette p = m_ui.location->palette();
    p.setColor( QPalette::Foreground, textColor );
    m_ui.location->setPalette(p);
    m_ui.serviceProvider->setPalette(p);

    setStopNameValid( stopNameValid, "" );
    setValuesOfStopSelectionConfig();
    setValuesOfAdvancedConfig();
    setValuesOfAppearanceConfig();
    setValuesOfFilterConfig();
    
    m_uiFilter.saveFilterConfiguration->setIcon( KIcon("document-save") );
    m_uiFilter.addFilterConfiguration->setIcon( KIcon("list-add") );
    m_uiFilter.removeFilterConfiguration->setIcon( KIcon("list-remove") );
    m_uiFilter.renameFilterConfiguration->setIcon( KIcon("edit-rename") );
    
    connect( parent, SIGNAL(finished()), this, SLOT(configDialogFinished()));
    connect( parent, SIGNAL(applyClicked()), this, SLOT(configAccepted()) );
    connect( parent, SIGNAL(okClicked()), this, SLOT(configAccepted()) );
    connect( m_ui.location, SIGNAL(currentIndexChanged(const QString&)),
	     this, SLOT(locationChanged(const QString&)) );
    connect( m_ui.serviceProvider, SIGNAL(currentIndexChanged(int)),
	     this, SLOT(serviceProviderChanged(int)) );
    connect( m_ui.city, SIGNAL(currentIndexChanged(QString)),
	     this, SLOT(cityNameChanged(QString)) );
    connect( m_ui.stop, SIGNAL(textChanged(QString)),
	     this, SLOT(stopNameChanged(QString)) );
    connect( m_ui.btnServiceProviderInfo, SIGNAL(clicked(bool)),
	     this, SLOT(clickedServiceProviderInfo(bool)));
    connect( m_uiFilter.filterLineType->selectedListWidget(), SIGNAL(currentRowChanged(int)),
	     this, SLOT(filterLineTypeSelectedSelectionChanged(int)) );
    connect( m_uiFilter.filterLineType->availableListWidget(), SIGNAL(currentRowChanged(int)),
	     this, SLOT(filterLineTypeAvailableSelectionChanged(int)) );
    connect( m_uiFilter.filterLineType, SIGNAL(added(QListWidgetItem*)),
	     this, SLOT(addedFilterLineType(QListWidgetItem*)) );
    connect( m_uiFilter.filterLineType, SIGNAL(removed(QListWidgetItem*)),
	     this, SLOT(removedFilterLineType(QListWidgetItem*)) );

    connect( m_uiFilter.filterConfigurations, SIGNAL(currentIndexChanged(QString)),
	     this, SLOT(loadFilterConfiguration(QString)) );
    connect( m_uiFilter.saveFilterConfiguration, SIGNAL(clicked()),
	     this, SLOT(saveFilterConfiguration()) );
    connect( m_uiFilter.addFilterConfiguration, SIGNAL(clicked()),
	     this, SLOT(addFilterConfiguration()) );
    connect( m_uiFilter.removeFilterConfiguration, SIGNAL(clicked()),
	     this, SLOT(removeFilterConfiguration()) );
    connect( m_uiFilter.renameFilterConfiguration, SIGNAL(clicked()),
	     this, SLOT(renameFilterConfiguration()) );

    connect( m_uiFilter.filterTypeTarget, SIGNAL(currentIndexChanged(int)),
	     this, SLOT(filterTypeTargetChanged(int)) );
    connect( m_uiFilter.filterTargetList, SIGNAL(changed()),
	     this, SLOT(filterTargetListChanged()) );

    connect( m_uiFilter.filterTypeLineNumber, SIGNAL(currentIndexChanged(int)),
	     this, SLOT(filterTypeLineNumberChanged(int)) );
    connect( m_uiFilter.filterLineNumberList, SIGNAL(changed()),
	     this, SLOT(filterLineNumberListChanged()) );

    // Check stop name validity
    stopNameChanged( m_ui.stop->text() );
}

void PublicTransportSettings::setValuesOfStopSelectionConfig() {
    m_ui.stop->setText(m_stop);
    m_ui.btnServiceProviderInfo->setIcon( KIcon("help-about") );
    m_ui.btnServiceProviderInfo->setText( "" );

    QMenu *menu = new QMenu(m_configDialog);
    menu->addAction( KIcon("get-hot-new-stuff"), i18n("Get new service providers..."),
		    this, SLOT(downloadServiceProvidersClicked(bool)) );
    menu->addAction( KIcon("text-xml"), i18n("Install new service provider from local file..."),
		     this, SLOT(installServiceProviderClicked(bool)) );
    m_ui.downloadServiceProviders->setMenu(menu);
    m_ui.downloadServiceProviders->setIcon( KIcon("list-add") );

    // Setup model and item delegate for the service provider combobox
    if ( m_modelServiceProvider != NULL )
	delete m_modelServiceProvider;
    m_modelServiceProvider = new QStandardItemModel( 0, 1 );
    m_ui.serviceProvider->setModel( m_modelServiceProvider );
    HtmlDelegate *htmlDelegate = new HtmlDelegate;
    htmlDelegate->setAlignText( true );
    m_ui.serviceProvider->setItemDelegate( htmlDelegate );

    // Setup model and item delegate for the location combobox
    if ( m_modelLocations != NULL )
	delete m_modelLocations;
    m_modelLocations = new QStandardItemModel( 0, 1 );
    m_ui.location->setModel( m_modelLocations );
    HtmlDelegate *htmlDelegateLocation = new HtmlDelegate;
    htmlDelegateLocation->setAlignText( true );
    m_ui.location->setItemDelegate( htmlDelegateLocation );
    //     m_ui.location->setIconSize(QSize(24, 16));

    // Get locations
    m_locationData = m_applet->dataEngine("publictransport")->query("Locations");
    QStringList uniqueCountries = m_locationData.keys();
    QStringList countries;

    // Get a list of with the location of each service provider (locations can be contained multiple times)
    m_serviceProviderData = m_applet->dataEngine("publictransport")->query("ServiceProviders");
    foreach ( QString serviceProviderName, m_serviceProviderData.keys() )  {
	QHash< QString, QVariant > serviceProviderData = m_serviceProviderData.value(serviceProviderName).toHash();
	countries << serviceProviderData["country"].toString();
    }

    QString highlightTextColor;
    QColor textColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor );
    if ( qGray(textColor.rgb()) > 192 )
	highlightTextColor = "red";
    else
	highlightTextColor = "darkred";

    // Create location items
    foreach( QString country, uniqueCountries ) {
	QStandardItem *item;
	QString text, sortText, cssTitle;
	item = new QStandardItem();

	if ( country.compare("international", Qt::CaseInsensitive)  == 0 ) {
	    text = i18n("International");
	    sortText = "00000" + text;
	    cssTitle = QString("color: %1;").arg(highlightTextColor);
	    item->setIcon( Global::internationalIcon() );
	} else if ( country.compare("unknown", Qt::CaseInsensitive)  == 0 ) {
	    text = i18n("Unknown");
	    sortText = "00000" + text;
	    cssTitle = "color: darkgray;";
	    item->setIcon( KIcon("dialog-warning") );
	} else {
	    if ( KGlobal::locale()->allCountriesList().contains( country ) )
		text = KGlobal::locale()->countryCodeToName( country );
	    else
		text = country;
	    sortText = "11111" + text;
	    item->setIcon( Global::putIconIntoBiggerSizeIcon(KIcon(country), QSize(32, 23)) );
	}

	QString formattedText = QString( "<span style='%4'><b>%1</b></span> <small>(<b>%2</b>)<br-wrap>%3</small>" )
	.arg( text )
	.arg( i18np("%1 accessor", "%1 accessors", countries.count(country)) )
	.arg( m_locationData[country].toHash()["description"].toString() )
	.arg( cssTitle );

	item->setText( text );
	item->setData( country, LocationCodeRole );
	item->setData( formattedText, HtmlDelegate::FormattedTextRole );
	item->setData( sortText, SortRole );
	item->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	item->setData( 4, HtmlDelegate::LinesPerRowRole );

	m_modelLocations->appendRow( item );
    }

    QString sShowAll = i18n("Show all available service providers");
    QStandardItem *itemShowAll = new QStandardItem();
    itemShowAll->setData( "000000", SortRole );
    QString formattedText = QString( "<span style='color:%3;'><b>%1</b></span><br-wrap><small><b>%2</b></small>" )
	.arg( sShowAll )
	.arg( i18n("Total: ") + i18np("%1 accessor", "%1 accessors", countries.count()) )
	.arg( highlightTextColor );
    itemShowAll->setData( "showAll", LocationCodeRole );
    itemShowAll->setData( formattedText, HtmlDelegate::FormattedTextRole );
    itemShowAll->setText( sShowAll );
    itemShowAll->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
    itemShowAll->setData( 3, HtmlDelegate::LinesPerRowRole );
    itemShowAll->setIcon( KIcon("package_network") ); // TODO: Other icon
    m_modelLocations->appendRow( itemShowAll );

    // TODO: Get error messages from the data engine
    QStringList errornousAccessorNames = m_applet->dataEngine("publictransport")->query("ErrornousServiceProviders")["names"].toStringList();
    if ( !errornousAccessorNames.isEmpty() ) {
	QStringList errorLines;
	for( int i = 0; i < errornousAccessorNames.count(); ++i ) {
	    errorLines << QString("<b>%1</b>").arg( errornousAccessorNames[i] );//.arg( errorMessages[i] );
	}
	QStandardItem *itemErrors = new QStandardItem();
	itemErrors->setData( "ZZZZZ", SortRole );
	formattedText = QString( "<span style='color:%3;'><b>%1</b></span><br-wrap><small>%2</small>" )
	    .arg( i18np("%1 accessor is errornous:", "%1 accessors are errornous:", errornousAccessorNames.count()) )
	    .arg( errorLines.join(",<br-wrap>") )
	    .arg( highlightTextColor );
	itemErrors->setData( formattedText, HtmlDelegate::FormattedTextRole );
	itemErrors->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	itemErrors->setData( 1 + errornousAccessorNames.count(), HtmlDelegate::LinesPerRowRole );
	itemErrors->setSelectable( false );
	itemErrors->setIcon( KIcon("edit-delete") );
	m_modelLocations->appendRow( itemErrors );
    }

    m_modelLocations->setSortRole( SortRole );
    m_modelLocations->sort( 0 );

    // Get (combobox-) index of the currently selected location
    int curLocationIndex = m_ui.location->findText(m_location);
    if ( curLocationIndex != -1 )
	m_ui.location->setCurrentIndex( curLocationIndex );

    int curServiceProviderIndex = updateServiceProviderModel(); // will also select the current service provider (if it's in the list)

    // Select current city
    QHash<QString, QVariant> serviceProviderData = m_ui.serviceProvider->itemData( curServiceProviderIndex, ServiceProviderDataRole ).toHash();
    if ( serviceProviderData["onlyUseCitiesInList"].toBool() )
	m_ui.city->setCurrentItem(m_city);
    else
	m_ui.city->setEditText(m_city);
}

void PublicTransportSettings::setValuesOfAdvancedConfig() {
    if (m_updateTimeout == 0) {
	m_uiAdvanced.updateAutomatically->setChecked(false);
	m_uiAdvanced.updateTimeout->setValue(60); // Set to default
    }
    else {
	m_uiAdvanced.updateAutomatically->setChecked(true);
	m_uiAdvanced.updateTimeout->setValue(m_updateTimeout);
    }
    m_uiAdvanced.timeOfFirstDeparture->setValue(m_timeOffsetOfFirstDeparture);
    m_uiAdvanced.timeOfFirstDepartureCustom->setTime(m_timeOfFirstDepartureCustom);
    m_uiAdvanced.firstDepartureUseCurrentTime->setChecked(m_firstDepartureConfigMode == RelativeToCurrentTime);
    m_uiAdvanced.firstDepartureUseCustomTime->setChecked(m_firstDepartureConfigMode == AtCustomTime);
    m_uiAdvanced.maximalNumberOfDepartures->setValue(m_maximalNumberOfDepartures);
    m_uiAdvanced.alarmTime->setValue(m_alarmTime);

    m_uiAdvanced.showDepartures->setChecked( m_departureArrivalListType == DepartureList );
    m_uiAdvanced.showArrivals->setChecked( m_departureArrivalListType == ArrivalList );
}

void PublicTransportSettings::exportFilterSettings() {
    QString fileName = KFileDialog::getSaveFileName(
	    KUrl("kfiledialog:///filterSettings"), QString(), m_configDialog,
	    i18n("Export Filter Settings") );
    if ( fileName.isEmpty() )
	return;
	    
    KConfig config( fileName, KConfig::SimpleConfig );
    writeFilterConfig( config.group(QString()), false );
}

void PublicTransportSettings::importFilterSettings() {
    QString fileName = KFileDialog::getOpenFileName(
	    KUrl("kfiledialog:///filterSettings"), QString(), m_configDialog,
	    i18n("Import Filter Settings") );
    if ( fileName.isEmpty() )
	return;
    
    KConfig config( fileName, KConfig::SimpleConfig );
    readFilterConfig( config.group(QString()) );
}

void PublicTransportSettings::addFilterConfiguration() {
    QString newFilterConfig = i18n( "New Configuration" );
    int i = 2;
    while ( m_filterConfigurationList.contains(newFilterConfig) ) {
	newFilterConfig = i18n( "New Configuration %1", i );
	++i;
    }

    kDebug() << "Add new filter config" << newFilterConfig;

    writeFilterConfig( m_applet->config("filterConfig_" + newFilterConfig) );
    
    m_filterConfigurationList.append( newFilterConfig );
    KConfigGroup cg = m_applet->config();
    cg.writeEntry("filterConfigurationList", m_filterConfigurationList);
    
    m_filterConfiguration = newFilterConfig;

    if ( m_configDialog )
	m_uiFilter.filterConfigurations->setCurrentItem( newFilterConfig, true );
    
    kDebug() << "END: Added new filter config" << newFilterConfig;
    
//     renameFilterConfiguration();

//     if ( m_uiFilter.filterConfigurations->currentText().isEmpty() ) {
	// Rename canceled
// 	removeFilterConfiguration();
// 	m_uiFilter.filterConfigurations->removeItem(
// 		m_uiFilter.filterConfigurations->currentIndex() );
// 	m_uiFilter.filterConfigurations->setCurrentIndex(
// 		filterConfigurationIndex(i18n("Default")) );
//     }
}

void PublicTransportSettings::removeFilterConfiguration() {
    if ( KMessageBox::warningContinueCancel(m_configDialog,
		i18n("This will permanently delete the selected filter configuration."))
		!= KMessageBox::Continue )
	return;

    kDebug() << "Delete filter config" << m_filterConfiguration;
//     m_applet->config().m_filterConfiguration )
    KConfigGroup cgOld = m_applet->config();
    cgOld.deleteGroup( "filterConfig_" + m_filterConfiguration );
    
    if ( m_filterConfigurationList.contains(m_filterConfiguration) ) {
	m_filterConfigurationList.removeOne(m_filterConfiguration);
	
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("filterConfigurationList", m_filterConfigurationList);
    }
    
    if ( m_configDialog ) {
	int index = filterConfigurationIndex( translateKey(m_filterConfiguration) );
	kDebug() << "Found current filter config in combo box?" << index;
	if ( index != -1  )
	    m_uiFilter.filterConfigurations->removeItem( index );

	index = filterConfigurationIndex( i18n("Default") );
	kDebug() << "Found default filter config in combo box?" << index;
	if ( index != -1 )
	    m_uiFilter.filterConfigurations->setCurrentIndex( index );
	else
	    kDebug() << "Default filter configuration not found!";
    }
}

QString PublicTransportSettings::showStringInputBox( const QString &labelString,
						     const QString &initialText,
						     const QString &clickMessage,
						     const QString &title,
						     QValidator *validator ) {
    KDialog *dialog = new KDialog( m_configDialog );
    dialog->setButtons( KDialog::Ok | KDialog::Cancel );
    dialog->setWindowTitle( title );
    
    QWidget *w = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout;
    QLabel *label = new QLabel( labelString );
    label->setAlignment( Qt::AlignRight );
    KLineEdit *input = new KLineEdit( initialText );
    input->setClearButtonShown( true );
    input->setClickMessage( clickMessage );
    validator->setParent( input );
    input->setValidator( validator );
    
    layout->addWidget( label );
    layout->addWidget( input );
    w->setLayout( layout );
    dialog->setMainWidget( w );
    
    input->setFocus();
    if ( dialog->exec() == KDialog::Rejected ) {
	delete dialog;
	return QString();
    }

    QString text = input->text();
    delete dialog;
    
    return text;
}

void PublicTransportSettings::renameFilterConfiguration() {
    QString newFilterConfig = showStringInputBox( i18n("New Name of the Filter Configuration:"),
						  translateKey(m_filterConfiguration),
						  i18nc("This is a clickMessage for the line edit "
							"in the rename filter config dialog",
							"Type the new name"),
						  i18n("Choose a Name"),
						  new QRegExpValidator(QRegExp("[^\\*]*"), this) );
    if ( newFilterConfig.isNull() )
	return; // Canceled

    kDebug() << "newFilterConfig:" << newFilterConfig
	     << "current filter config:" << m_filterConfiguration;
    if ( newFilterConfig == m_filterConfiguration )
	return; // Not changed, but accepted

    // Check if the new name is valid.
    // '*' isn't allowed in the name but already validated by a QRegExpValidator.
    if ( newFilterConfig.isEmpty() ) {
	KMessageBox::information( m_configDialog, i18n("Empty names can't be used.") );
	return;
    } else if ( newFilterConfig == i18n("Default") ) {
	KMessageBox::information( m_configDialog, i18n("The filter configuration "
		"'%1' can't be changed.", i18n("Default")) );
	return;
    }
    
    if ( m_filterConfigurationList.contains(newFilterConfig)
		&& KMessageBox::warningYesNo(m_configDialog,
		   i18n("There is already a filter configuration with the name '%1'.\n"
		   "Do you want to overwrite it?") ) != KMessageBox::Yes ) {
	return; // No pressed
    }
    
    if ( m_configDialog ) {
	disconnect( m_uiFilter.filterConfigurations, SIGNAL(currentIndexChanged(QString)),
		    this, SLOT(loadFilterConfiguration(QString)) );
    }
    QString newFilterConfigUntranslated = untranslateKey( newFilterConfig );
    KConfigGroup newConfigGroup = m_applet->config(
	    "filterConfig_" + newFilterConfigUntranslated );
    //     writeFilterConfig( newConfigGroup ); // OLDTODO: Warn the user that the settings get applied?
    m_applet->config("filterConfig_" + m_filterConfiguration).copyTo( &newConfigGroup );
    m_applet->config().deleteGroup( "filterConfig_" + m_filterConfiguration );
    
    if ( m_filterConfigurationList.contains(m_filterConfiguration) )
	m_filterConfigurationList.removeOne(m_filterConfiguration);
    if ( !m_filterConfigurationList.contains(newFilterConfigUntranslated) )
	m_filterConfigurationList << newFilterConfigUntranslated;
    kDebug() << "New Filter Configuration List" << m_filterConfigurationList;
    KConfigGroup cg = m_applet->config();
    cg.writeEntry("filterConfigurationList", m_filterConfigurationList);

    if ( m_configDialog ) {
	int index = m_uiFilter.filterConfigurations->findText( translateKey(m_filterConfiguration) );
	if ( index != -1  ) {
	    kDebug() << "Remove old filter config" << m_filterConfiguration;
	    m_uiFilter.filterConfigurations->removeItem( index );
	}
    }
    
    kDebug() << "Set current filter config" << newFilterConfigUntranslated;
    m_filterConfiguration = newFilterConfigUntranslated;
    m_applet->config().writeEntry("filterConfiguration", m_filterConfiguration);

    if ( m_configDialog ) {
	m_uiFilter.filterConfigurations->setCurrentItem( newFilterConfig, true );
	connect( m_uiFilter.filterConfigurations, SIGNAL(currentIndexChanged(QString)),
		this, SLOT(loadFilterConfiguration(QString)) );
    }
}

QString PublicTransportSettings::translateKey( const QString& key ) const {
    if ( key == "Default" )
	return i18n("Default");
    else if ( key == "Default*" )
	return i18n("Default") + "*";
    else
	return key;
}

QString PublicTransportSettings::untranslateKey( const QString& translatedKey ) const {
    if ( translatedKey == i18n("Default") || translatedKey == i18n("Default") + "*" )
	return "Default";
    else
	return translatedKey;
}

void PublicTransportSettings::saveFilterConfiguration() {
    if ( m_filterConfiguration == "Default" ) {
	// TODO: Put this text as label into the rename mini-dialog
// 	KMessageBox::information( m_configDialog, i18n("The filter configuration "
// 		"'%1' can't be changed. Choose a new name.", i18n("Default")) );
	renameFilterConfiguration();
	
	if ( m_filterConfiguration == "Default" )
	    return;
    }

    if ( !m_filterConfigurationList.contains(m_filterConfiguration) ) {
	m_filterConfigurationList << m_filterConfiguration;
	
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("filterConfigurationList", m_filterConfigurationList);
    }

    if ( m_configDialog ) {
	QString _filterConfiguration = translateKey( m_filterConfiguration );
	int index = filterConfigurationIndex( _filterConfiguration );
	if ( index != -1 )
	    m_uiFilter.filterConfigurations->setItemText( index, _filterConfiguration );
	else
	    kDebug() << "Item to remove changed indicator (*) from not found";
    }
	
    QString filterConfig = "filterConfig_" + m_filterConfiguration;
    writeFilterConfig( m_applet->config(filterConfig), false );
    setFilterConfigurationChanged( false );
}

int PublicTransportSettings::filterConfigurationIndex( const QString& filterConfig ) {
    int index = m_uiFilter.filterConfigurations->findText( filterConfig );
    if ( index == -1 )
	index = m_uiFilter.filterConfigurations->findText( filterConfig + "*" );
    else
	kDebug() << "Item" << filterConfig << "not found!";

    return index;
}

void PublicTransportSettings::loadFilterConfiguration( const QString& filterConfig ) {
    QString _filterConfig = untranslateKey( filterConfig );
    if ( _filterConfig == m_filterConfiguration )
	return;
    
    if ( m_filterConfigChanged && m_filterConfiguration != "Default" ) {
	QString _filterConfiguration = translateKey( m_filterConfiguration );

	int result = KMessageBox::questionYesNoCancel( m_configDialog,
		    i18n("The current filter configuration is unsaved.\n"
			 "Do you want to store it now to '%1'?", _filterConfiguration),
		    i18n("Save Filter Configuration?") );
	if ( result == KMessageBox::Cancel )
	    return;
	else if ( result == KMessageBox::Yes )
	    saveFilterConfiguration();

	if ( m_configDialog ) {
	    int index = filterConfigurationIndex( _filterConfiguration );
	    if ( index == -1 ) {
		kDebug() << "Didn't find" << _filterConfiguration;
	    } else {
		kDebug() << "Rename" << index << _filterConfiguration + "*"
			<< "to" << _filterConfiguration;
		m_uiFilter.filterConfigurations->setItemText( index, _filterConfiguration );
	    }
	}
    }

    kDebug() << "Loading" << "filterConfig_" + _filterConfig
	     << "Current filterconfig:" << m_filterConfiguration;
    m_filterConfiguration = _filterConfig;
    m_applet->config().writeEntry("filterConfiguration", m_filterConfiguration);
    
    readFilterConfig( m_applet->config("filterConfig_" + _filterConfig) );
    setFilterConfigurationChanged( false );

    if ( !m_configDialog ) {
	emit configNeedsSaving();
	emit modelNeedsUpdate(); // Apply new filter settings when no dialog is shown
				 // ie. the slot was called from the context menu
    }
}

void PublicTransportSettings::filterTypeTargetChanged( int ) {
    setFilterConfigurationChanged();
}

void PublicTransportSettings::filterTargetListChanged() {
    setFilterConfigurationChanged();
}

void PublicTransportSettings::filterTypeLineNumberChanged( int ) {
    setFilterConfigurationChanged();
}

void PublicTransportSettings::filterLineNumberListChanged() {
    setFilterConfigurationChanged();
}

void PublicTransportSettings::setFilterConfigurationChanged( bool changed ) {
    if ( m_filterConfigChanged == changed )
	return;

    if ( m_configDialog ) {
	bool deaultFilterConfig = m_filterConfiguration == "Default";
	m_uiFilter.saveFilterConfiguration->setEnabled( changed && !deaultFilterConfig );
	m_uiFilter.removeFilterConfiguration->setDisabled( deaultFilterConfig );
	m_uiFilter.renameFilterConfiguration->setDisabled( deaultFilterConfig );

	QString sTitle = m_uiFilter.filterConfigurations->currentText();
	if ( changed && !sTitle.endsWith('*') )
	    sTitle.append( '*' );
	else if ( !changed && sTitle.endsWith('*') )
	    sTitle.chop( 1 );
	disconnect( m_uiFilter.filterConfigurations, SIGNAL(currentIndexChanged(QString)),
		    this, SLOT(loadFilterConfiguration(QString)) );
	m_uiFilter.filterConfigurations->setItemText(
		m_uiFilter.filterConfigurations->currentIndex(), sTitle );
	connect( m_uiFilter.filterConfigurations, SIGNAL(currentIndexChanged(QString)),
		this, SLOT(loadFilterConfiguration(QString)) );
    }
	    
    m_filterConfigChanged = changed;
}

void PublicTransportSettings::setValuesOfFilterConfig() {
    if ( !m_applet->config().hasGroup("filterConfig_Default") ) {
	kDebug() << "Adding 'filterConfig_Default'";
	writeDefaultFilterConfig( m_applet->config("filterConfig_Default") );
    }
    if ( !m_filterConfigurationList.contains("Default") )
	m_filterConfigurationList.prepend( "Default" );
    
    QStringList filterConfigGroups;
    kDebug() << "Group list" << m_applet->config().groupList();
    kDebug() << "Filter Config List:" << m_filterConfigurationList;
    foreach ( QString filterConfig, m_filterConfigurationList )
	filterConfigGroups << translateKey( filterConfig );

    bool deaultFilterConfig = m_filterConfiguration == "Default";
    m_uiFilter.saveFilterConfiguration->setEnabled( m_filterConfigChanged && !deaultFilterConfig );
    m_uiFilter.removeFilterConfiguration->setDisabled( deaultFilterConfig );
    m_uiFilter.renameFilterConfiguration->setDisabled( deaultFilterConfig );
    
    disconnect( m_uiFilter.filterConfigurations, SIGNAL(currentIndexChanged(QString)),
	     this, SLOT(loadFilterConfiguration(QString)) );
    m_uiFilter.filterConfigurations->clear();
    m_uiFilter.filterConfigurations->addItems( filterConfigGroups );

    QString _filterConfig = translateKey( m_filterConfiguration );
    kDebug() << "Set current item to" << _filterConfig;
    m_uiFilter.filterConfigurations->setCurrentItem( _filterConfig );
    if ( m_filterConfigChanged ) {
	m_uiFilter.filterConfigurations->setItemText(
		m_uiFilter.filterConfigurations->currentIndex(), _filterConfig + "*" );
    }
    connect( m_uiFilter.filterConfigurations, SIGNAL(currentIndexChanged(QString)),
	     this, SLOT(loadFilterConfiguration(QString)) );
    
    QListWidget *available, *selected;
    available = m_uiFilter.filterLineType->availableListWidget();
    selected = m_uiFilter.filterLineType->selectedListWidget();

    available->clear();
    selected->clear();
    available->addItems( QStringList() << i18n("Unknown") << i18n("Trams")
	    << i18n("Buses") << i18n("Subways") << i18n("Metros")
	    << i18n("Trolley buses") << i18n("Interurban trains")
	    << i18n("Regional trains") << i18n("Regional express trains")
	    << i18n("Interregio trains") << i18n("Intercity / Eurocity trains")
	    << i18n("Intercity express trains") << i18n("Ferries")
	    << i18n("Planes") );

    available->item( 0 )->setIcon( Global::iconFromVehicleType(Unknown) );
    available->item( 1 )->setIcon( Global::iconFromVehicleType(Tram) );
    available->item( 2 )->setIcon( Global::iconFromVehicleType(Bus) );
    available->item( 3 )->setIcon( Global::iconFromVehicleType(Subway) );
    available->item( 4 )->setIcon( Global::iconFromVehicleType(Metro) );
    available->item( 5 )->setIcon( Global::iconFromVehicleType(TrolleyBus) );
    available->item( 6 )->setIcon( Global::iconFromVehicleType(TrainInterurban) );
    available->item( 7 )->setIcon( Global::iconFromVehicleType(TrainRegional) );
    available->item( 8 )->setIcon( Global::iconFromVehicleType(TrainRegionalExpress) );
    available->item( 9 )->setIcon( Global::iconFromVehicleType(TrainInterregio) );
    available->item( 10 )->setIcon( Global::iconFromVehicleType(TrainIntercityEurocity) );
    available->item( 11 )->setIcon( Global::iconFromVehicleType(TrainIntercityExpress) );
    available->item( 12 )->setIcon( Global::iconFromVehicleType(Ferry) );
    available->item( 13 )->setIcon( Global::iconFromVehicleType(Plane) );

    if ( m_showTypeOfVehicle[Plane] )
	selected->addItem( available->takeItem(13) );
    if ( m_showTypeOfVehicle[Ferry] )
	selected->addItem( available->takeItem(12) );
    if ( m_showTypeOfVehicle[TrainIntercityExpress] )
	selected->addItem( available->takeItem(11) );
    if ( m_showTypeOfVehicle[TrainIntercityEurocity] )
	selected->addItem( available->takeItem(10) );
    if ( m_showTypeOfVehicle[TrainInterregio] )
	selected->addItem( available->takeItem(9) );
    if ( m_showTypeOfVehicle[TrainRegionalExpress] )
	selected->addItem( available->takeItem(8) );
    if ( m_showTypeOfVehicle[TrainRegional] )
	selected->addItem( available->takeItem(7));
    if ( m_showTypeOfVehicle[TrainInterurban] )
	selected->addItem( available->takeItem(6) );
    if ( m_showTypeOfVehicle[TrolleyBus] )
	selected->addItem( available->takeItem(5) );
    if ( m_showTypeOfVehicle[Metro] )
	selected->addItem( available->takeItem(4) );
    if ( m_showTypeOfVehicle[Subway] )
	selected->addItem( available->takeItem(3) );
    if ( m_showTypeOfVehicle[Bus] )
	selected->addItem( available->takeItem(2) );
    if ( m_showTypeOfVehicle[Tram] )
	selected->addItem( available->takeItem(1) );
    if ( m_showTypeOfVehicle[Unknown] )
	selected->addItem( available->takeItem(0) );

    m_uiFilter.filterTypeTarget->setCurrentIndex( static_cast<int>(m_filterTypeTarget) );
    m_uiFilter.filterTargetList->setItems(m_filterTargetList);

    m_uiFilter.filterTypeLineNumber->setCurrentIndex(
	    static_cast<int>(m_filterTypeLineNumber) );
    m_uiFilter.filterLineNumberList->setItems(m_filterLineNumberList);
}

void PublicTransportSettings::setValuesOfAppearanceConfig() {
    m_uiAppearance.linesPerRow->setValue(m_linesPerRow);
    m_uiAppearance.size->setValue(m_size);
    if ( m_showRemainingMinutes && m_showDepartureTime )
	m_uiAppearance.cmbDepartureColumnInfos->setCurrentIndex(0);
    else if ( m_showRemainingMinutes )
	m_uiAppearance.cmbDepartureColumnInfos->setCurrentIndex(2);
    else
	m_uiAppearance.cmbDepartureColumnInfos->setCurrentIndex(1);
    m_uiAppearance.displayTimeBold->setChecked( m_displayTimeBold );

    m_uiAppearance.showHeader->setChecked( m_showHeader );
    m_uiAppearance.showColumnTarget->setChecked( !m_hideColumnTarget );
    m_uiAppearance.radioUseDefaultFont->setChecked( m_useDefaultFont );
    m_uiAppearance.radioUseOtherFont->setChecked( !m_useDefaultFont );
    m_uiAppearance.font->setCurrentFont( font() );
}

void PublicTransportSettings::downloadServiceProvidersClicked( bool ) {
    if ( !m_applet->testState(ConfigDialogShown) )
	return;

    if ( KMessageBox::warningContinueCancel(m_configDialog,
	    i18n("The downloading may currently not work as expected, sorry."))
	    == KMessageBox::Cancel )
	return;

#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
    KNS3::DownloadDialog *dialog = new KNS3::DownloadDialog(
			      "publictransport.knsrc", m_configDialog );
    dialog->exec();
    qDebug() << "KNS3 Results: " << dialog->changedEntries().count();
    
    KNS3::Entry::List installed = dialog->installedEntries();
    foreach ( KNS3::Entry entry, installed )
      kDebug() << entry.name() << entry.installedFiles();

    if ( !dialog->changedEntries().isEmpty() )
      updateServiceProviderModel(); // TODO

    delete dialog;
#else
    KNS::Engine engine(m_configDialog);
    if (engine.init("publictransport2.knsrc")) {
	KNS::Entry::List entries = engine.downloadDialogModal(m_configDialog);

	qDebug() << "PublicTransportSettings::downloadServiceProvidersClicked" << entries.count();
	if (entries.size() > 0) {
	    foreach ( KNS::Entry *entry, entries ) {
		// Downloaded file has the name "hotstuff-access" which is wrong (maybe it works
		// better with archives). So rename the file to the right name from the payload:
		QString filename = entry->payload().representation()
		    .remove( QRegExp("^.*\\?file=") ).remove( QRegExp("&site=.*$") );
		QStringList installedFiles = entry->installedFiles();

		qDebug() << "installedFiles =" << installedFiles;
		if ( !installedFiles.isEmpty() ) {
		    QString installedFile = installedFiles[0];

		    QString path = KUrl( installedFile ).path().remove( QRegExp("/[^/]*$") ) + "/";
		    QFile( installedFile ).rename( path + filename );

		    qDebug() << "PublicTransportSettings::downloadServiceProvidersClicked" <<
		    "Rename" << installedFile << "to" << path + filename;
		}
	    }

	    // Get a list of with the location of each service provider (locations can be contained multiple times)
	    m_serviceProviderData = m_applet->dataEngine("publictransport")->query("ServiceProviders");
	    // TODO: Update "ServiceProviders"-data source in the data engine.
	    // TODO: Update country list (group titles in the combo box)
// 	    foreach ( QString serviceProviderName, m_serviceProviderData.keys() )  {
// 		QHash< QString, QVariant > serviceProviderData = m_serviceProviderData.value(serviceProviderName).toHash();
// 		countries << serviceProviderData["country"].toString();
// 	    }
	    updateServiceProviderModel(); // TODO
	}
    }
#endif
}

void PublicTransportSettings::installServiceProviderClicked( bool ) {
    if ( !m_applet->testState(ConfigDialogShown) )
	return;

    QString fileName = KFileDialog::getOpenFileName( KUrl(), "*.xml", m_configDialog );
    if ( !fileName.isEmpty() ) {
	QStringList dirs = KGlobal::dirs()->findDirs( "data",
		"plasma_engine_publictransport/accessorInfos/" );
	if ( dirs.isEmpty() )
	    return;

	QString targetDir = dirs[0];
	kDebug() << "PublicTransportSettings::installServiceProviderClicked"
		 << "Install file" << fileName << "to" << targetDir;
	QProcess::execute( "kdesu", QStringList() << QString("cp %1 %2").arg( fileName ).arg( targetDir ) );
    }
}

void PublicTransportSettings::selectLocaleLocation() {
    updateServiceProviderModel();

    m_location = KGlobal::locale()->country();
    if ( m_locationData.keys().contains( m_location ) ) {
	int curLocationIndex = -1;
	for ( int i = 0; i < m_ui.location->count(); ++i ) {
	    if ( m_ui.location->itemData(i, LocationCodeRole) == m_location ) {
		curLocationIndex = i;
		break;
	    }
	}
	if ( curLocationIndex != -1 )
	    m_ui.location->setCurrentIndex( curLocationIndex );
	else
	    m_ui.location->setCurrentIndex( 0 );
    } else {
	m_ui.location->setCurrentIndex( 0 );
    }
}

void PublicTransportSettings::locationChanged( const QString &newLocation ) { // TODO: use the (int)-version
    updateServiceProviderModel( newLocation );

    // Select default accessor of the selected location
    m_location = m_ui.location->itemData( m_ui.location->findText(newLocation), LocationCodeRole ).toString();
    foreach( QString serviceProviderName, m_serviceProviderData.keys() ) {
	QVariantHash serviceProviderData = m_serviceProviderData[serviceProviderName].toHash();
	if ( serviceProviderData["country"].toString() == m_location ) {
	    if ( serviceProviderData["id"].toString().compare( m_locationData[m_location].toHash()["defaultAccessor"].toString(), Qt::CaseInsensitive ) == 0 ) {
		qDebug() << "found default accessor" << serviceProviderName;
		m_ui.serviceProvider->setCurrentItem( serviceProviderName );
		break;
	    }
	}
    }
}

int PublicTransportSettings::updateServiceProviderModel( const QString &itemText ) {
    m_modelServiceProvider->clear();
    int index = itemText.isEmpty() ? m_ui.location->currentIndex() : m_ui.location->findText( itemText );

    foreach( QString serviceProviderName, m_serviceProviderData.keys() ) {
	QVariantHash serviceProviderData = m_serviceProviderData[serviceProviderName].toHash();

	// Filter out service providers with the value of the currently selected location
	if ( index != 0 && serviceProviderData["country"].toString().toLower() != "international" &&
	    m_ui.location->itemData(index, LocationCodeRole).toString()
	    .compare( serviceProviderData["country"].toString(), Qt::CaseInsensitive ) != 0 )
	    continue;

	bool isCountryWide = serviceProviderName.contains(
	    serviceProviderData["country"].toString(), Qt::CaseInsensitive );
	QString formattedText;
	QStandardItem *item = new QStandardItem( serviceProviderName );

// 	if ( isCountryWide ) {
	    item->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole ); formattedText =
		QString( "<b>%1</b><br-wrap><small><b>Features:</b> %2</small>" )
		.arg( serviceProviderName )
		.arg( serviceProviderData["features"].toStringList().join(", ")/*.replace(QRegExp("([^,]*,[^,]*,[^,]*,)"), "\\1<br>")*/ );
	    item->setData( 4, HtmlDelegate::LinesPerRowRole );
// 	} else {
// 	    item->setData( QStringList() << "sunken" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole ); formattedText = QString( "<b>%1</b><br-wrap><small><b>Features:</b> %2</small>" )
// 		.arg( serviceProviderName )
// 		.arg( serviceProviderData["features"].toStringList().join(", ")/*.replace(QRegExp("([^,]*,[^,]*,[^,]*,)"), "\\1<br>")*/ );
// 	    item->setData( 4, HtmlDelegate::LinesPerRowRole );
// 	}
	item->setData( formattedText, HtmlDelegate::FormattedTextRole );
	item->setData( serviceProviderData, ServiceProviderDataRole );

	// Sort service providers containing the country in it's name to the top of the list for that country
	QString sortString = isCountryWide
	    ? serviceProviderData["country"].toString() + "11111" + serviceProviderName
	    : serviceProviderData["country"].toString() + serviceProviderName;
	item->setData( sortString, SortRole );
	m_modelServiceProvider->appendRow( item );

	// TODO: add favicon to data engine?
	QString favIconSource = serviceProviderData["url"].toString();
	m_applet->dataEngine("favicons")->disconnectSource( favIconSource, this );
	m_applet->dataEngine("favicons")->connectSource( favIconSource, this );
	m_applet->dataEngine("favicons")->query( favIconSource );
    }
    m_modelServiceProvider->setSortRole( SortRole );
    m_modelServiceProvider->sort( 0 );

    // Add title items
    QStringList lastTitles;
    for( int row = 0; row < m_modelServiceProvider->rowCount(); ++row ) {
	QString title = m_modelServiceProvider->item( row )->data( ServiceProviderDataRole ).toHash()["country"].toString();
	if ( KGlobal::locale()->allCountriesList().contains(title) )
	    title = KGlobal::locale()->countryCodeToName(title);
	else if ( title == "international" )
	    title = i18n("International");
	else if ( title == "unknown" )
	    title = i18n("Unknown");

	if ( lastTitles.contains(title) )
	    continue;

	QStandardItem *itemTitle = new QStandardItem(title);
	QColor textColor = KColorScheme( QPalette::Active ).foreground().color();
	itemTitle->setData( QString("<span style='font-weight:bold;font-size:large;text-decoration:underline;color:rgb(%1,%2,%3);'>")
		.arg(textColor.red()).arg(textColor.green()).arg(textColor.blue())
		+ title + ":</span>", HtmlDelegate::FormattedTextRole );
	itemTitle->setData( 0, HtmlDelegate::GroupTitleRole );
	itemTitle->setData( 2, HtmlDelegate::LinesPerRowRole );
// 	itemTitle->setForeground( KColorScheme(QPalette::Active).foreground() );
	itemTitle->setSelectable( false );
	m_modelServiceProvider->insertRow( row, itemTitle );

	lastTitles << title;
	++row;
    }

    // Get (combobox-) index of the currently selected service provider
    int curServiceProviderIndex = -1;
    for ( int i = 0; i < m_ui.serviceProvider->count(); ++i ) {
	if ( m_ui.serviceProvider->itemData( i, ServiceProviderDataRole ).toHash()["id"].toString() == m_serviceProvider ) {
	    curServiceProviderIndex = i;
	    break;
	}
    }
    if ( curServiceProviderIndex != -1 ) {
	m_ui.serviceProvider->setCurrentIndex( curServiceProviderIndex );
	serviceProviderChanged( curServiceProviderIndex );
    }

    return curServiceProviderIndex;
}

void PublicTransportSettings::configAccepted() {
    bool changed = false, changedServiceProviderSettings = false;

    if (m_location != m_ui.location->currentText()) {
	m_location = m_ui.location->currentText();

	KConfigGroup cg = m_applet->config();
	cg.writeEntry("location", m_location);
	changed = true;

	// 	m_applet->addState( WaitingForDepartureData );
	emit departureListNeedsClearing(); // Clear departures from the old service provider TODO: don't call multiple times in configAccepted()
    }

    QHash< QString, QVariant > serviceProviderData = m_modelServiceProvider->item( m_ui.serviceProvider->currentIndex() )->data( ServiceProviderDataRole ).toHash();
    const QString serviceProviderID = serviceProviderData["id"].toString();
    if (m_serviceProvider != serviceProviderID) {
	m_serviceProvider = serviceProviderID;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("serviceProvider", m_serviceProvider);
	changed = true;

	changedServiceProviderSettings = true;
	// 	m_applet->addState( WaitingForDepartureData );
	emit departureListNeedsClearing(); // Clear departures from the old service provider

	m_onlyUseCitiesInList = serviceProviderData["onlyUseCitiesInList"].toBool();
	m_useSeperateCityValue = serviceProviderData["useSeperateCityValue"].toBool();
	m_serviceProviderFeatures = serviceProviderData["features"].toStringList();
    }

    if (!m_uiAdvanced.updateAutomatically->isChecked() && m_updateTimeout != 0) {
	m_updateTimeout = 0;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("updateTimeout", m_updateTimeout);
	changed = true;
    } else if (m_updateTimeout != m_uiAdvanced.updateTimeout->value()) {
	m_updateTimeout = m_uiAdvanced.updateTimeout->value();
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("updateTimeout", m_updateTimeout);
	changed = true;
    }

    if (m_showRemainingMinutes != (m_uiAppearance.cmbDepartureColumnInfos->currentIndex() != 1)) {
	m_showRemainingMinutes = !m_showRemainingMinutes;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("showRemainingMinutes", m_showRemainingMinutes);
	changed = true;
    }

    if (m_showDepartureTime != (m_uiAppearance.cmbDepartureColumnInfos->currentIndex() <= 1)) {
	m_showDepartureTime = !m_showDepartureTime;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("showDepartureTime", m_showDepartureTime);
	changed = true;
    }

    if (m_displayTimeBold != (m_uiAppearance.displayTimeBold->checkState() == Qt::Checked)) {
	m_displayTimeBold = !m_displayTimeBold;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("displayTimeBold", m_displayTimeBold);
	changed = true;
    }

    if (m_showHeader != (m_uiAppearance.showHeader->checkState() == Qt::Checked)) {
	m_showHeader = !m_showHeader;
// 	m_applet->m_treeView->nativeWidget()->header()->setVisible( m_showHeader );
	KConfigGroup cg = m_applet->config();
	cg.writeEntry( "showHeader", m_showHeader );
	changed = true;
    }

    if (m_hideColumnTarget == (m_uiAppearance.showColumnTarget->checkState() == Qt::Checked)) {
	m_hideColumnTarget = !m_hideColumnTarget;
// 	if ( m_hideColumnTarget )
// 	    m_applet->showColumnTarget(true);
// 	else
// 	    m_applet->hideColumnTarget(true);
	// 	((QTreeView*)m_treeView->widget())->header()->setSectionHidden( 1, !m_ui.showColumnTarget->isChecked() );
	KConfigGroup cg = m_applet->config();
	cg.writeEntry( "hideColumnTarget", m_hideColumnTarget );
	changed = true;
    }

    if (m_city != configCityValue()) {
	m_city = configCityValue();
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("city", m_city);
	changed = true;

	// Service provider data must only be newly requested caused by a city value change, if the current service provider uses this city value
	if ( m_useSeperateCityValue ) {
	    changedServiceProviderSettings = true;
    // 	m_applet->addState( WaitingForDepartureData );
	    emit departureListNeedsClearing(); // Clear departures using the old city name
	}
    }

    if (m_stop != m_ui.stop->text()) {
	m_stop = m_ui.stop->text();
	m_stopID = m_stopIDinConfig;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("stop", m_stop);
	cg.writeEntry("stopID", m_stopID);
	changed = true;

	changedServiceProviderSettings = true;
// 	m_applet->addState( WaitingForDepartureData );
	emit departureListNeedsClearing(); // Clear departures using the old stop name
    }

    if (m_timeOffsetOfFirstDeparture != m_uiAdvanced.timeOfFirstDeparture->value()) {
	m_timeOffsetOfFirstDeparture = m_uiAdvanced.timeOfFirstDeparture->value();
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("timeOffsetOfFirstDeparture", m_timeOffsetOfFirstDeparture);
	changed = true;

	changedServiceProviderSettings = true;
    }

    if (m_timeOfFirstDepartureCustom != m_uiAdvanced.timeOfFirstDepartureCustom->time()) {
	m_timeOfFirstDepartureCustom = m_uiAdvanced.timeOfFirstDepartureCustom->time();
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("timeOfFirstDepartureCustom", m_timeOfFirstDepartureCustom.toString("hh:mm"));
	changed = true;

	changedServiceProviderSettings = true;
    }

    if ( (m_firstDepartureConfigMode == RelativeToCurrentTime &&
	!m_uiAdvanced.firstDepartureUseCurrentTime->isChecked()) ||
	(m_firstDepartureConfigMode == AtCustomTime &&
	!m_uiAdvanced.firstDepartureUseCustomTime->isChecked()) )
    {
	m_firstDepartureConfigMode = m_uiAdvanced.firstDepartureUseCurrentTime->isChecked()
	    ? RelativeToCurrentTime : AtCustomTime;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("firstDepartureConfigMode", static_cast<int>(m_firstDepartureConfigMode));
	changed = true;

	changedServiceProviderSettings = true;
    }

    if (m_maximalNumberOfDepartures != m_uiAdvanced.maximalNumberOfDepartures->value()) {
	m_maximalNumberOfDepartures = m_uiAdvanced.maximalNumberOfDepartures->value();
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("maximalNumberOfDepartures", m_maximalNumberOfDepartures);
	changed = true;

	changedServiceProviderSettings = true;
// 	m_applet->addState( WaitingForDepartureData );
    }

    if (m_alarmTime != m_uiAdvanced.alarmTime->value()) {
	m_alarmTime = m_uiAdvanced.alarmTime->value();
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("alarmTime", m_alarmTime);
	changed = true;
    }
    
    if (m_linesPerRow != m_uiAppearance.linesPerRow->value()) {
	m_linesPerRow = m_uiAppearance.linesPerRow->value();
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("linesPerRow", m_linesPerRow);
	changed = true;
    }
    
    if (m_size != m_uiAppearance.size->value()) {
	m_size = m_uiAppearance.size->value();
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("size", m_size);
	changed = true;
    }

    if (m_useDefaultFont != m_uiAppearance.radioUseDefaultFont->isChecked()) {
	m_useDefaultFont = !m_useDefaultFont;

	if ( m_useDefaultFont ) {
	    KConfigGroup cg = m_applet->config();
	    cg.writeEntry("fontFamily", "");
	    changed = true;
	} else {
	    m_font.setFamily( m_uiAppearance.font->currentFont().family() );
	    KConfigGroup cg = m_applet->config();
	    cg.writeEntry("fontFamily", m_font.family());
	    changed = true;
	}
    }

    if ( !m_useDefaultFont && m_font.family() != m_uiAppearance.font->currentFont().family()) {
	m_font.setFamily( m_uiAppearance.font->currentFont().family() );
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("fontFamily", m_font.family());
	changed = true;
    }

/*
    qDebug()  << "PublicTransportSettings::configAccepted" << m_departureArrivalListType
	<< "showDepartures =" << m_ui.showDepartures->isChecked()
	<< "showArrivals =" << m_ui.showArrivals->isChecked()
	<< (m_departureArrivalListType == DepartureList && !m_ui.showDepartures->isChecked())
	<< (m_departureArrivalListType == ArrivalList && !m_ui.showArrivals->isChecked());*/

    if ( (m_departureArrivalListType == DepartureList && !m_uiAdvanced.showDepartures->isChecked()) ||
	(m_departureArrivalListType == ArrivalList && !m_uiAdvanced.showArrivals->isChecked()) )
    {
	if ( m_uiAdvanced.showArrivals->isChecked() )
	    m_departureArrivalListType = ArrivalList;
	else
	    m_departureArrivalListType = DepartureList;

	KConfigGroup cg = m_applet->config();
	cg.writeEntry("departureArrivalListType", static_cast<int>(m_departureArrivalListType));
	changed = true;

// 	m_applet->addState( WaitingForDepartureData );
	changedServiceProviderSettings = true;
	emit departureArrivalListTypeChanged( m_departureArrivalListType );
	emit departureListNeedsClearing(); // Clear departures using the old data source type
    }
    
    KConfigGroup cg = m_applet->config();
    cg.writeEntry("filterConfiguration", m_filterConfiguration);
    cg.writeEntry("filterConfigurationList", m_filterConfigurationList);
    if ( writeFilterConfig(m_applet->config()) )
	changed = true;
    
    if ( changed ) {
	emit settingsChanged();
	emit configNeedsSaving();
    }
    if (changedServiceProviderSettings)
	emit serviceProviderSettingsChanged();
}

bool PublicTransportSettings::readFilterConfig( const KConfigGroup& cg ) {
    kDebug() << "Read filter config from" << cg.name();
    
    m_showTypeOfVehicle[Unknown] =
	    cg.readEntry(vehicleTypeToConfigName(Unknown), true);
    m_showTypeOfVehicle[Tram] =
	    cg.readEntry(vehicleTypeToConfigName(Tram), true);
    m_showTypeOfVehicle[Bus] =
	    cg.readEntry(vehicleTypeToConfigName(Bus), true);
    m_showTypeOfVehicle[Subway] =
	    cg.readEntry(vehicleTypeToConfigName(Subway), true);
    m_showTypeOfVehicle[Metro] =
	    cg.readEntry(vehicleTypeToConfigName(Metro), true);
    m_showTypeOfVehicle[TrolleyBus] =
	    cg.readEntry(vehicleTypeToConfigName(TrolleyBus), true);
    m_showTypeOfVehicle[TrainInterurban] =
	    cg.readEntry(vehicleTypeToConfigName(TrainInterurban), true);
    m_showTypeOfVehicle[TrainRegional] =
	    cg.readEntry(vehicleTypeToConfigName(TrainRegional), true);
    m_showTypeOfVehicle[TrainRegionalExpress] =
	     cg.readEntry(vehicleTypeToConfigName(TrainRegionalExpress), true);
    m_showTypeOfVehicle[TrainInterregio] =
	    cg.readEntry(vehicleTypeToConfigName(TrainInterregio), true);
    m_showTypeOfVehicle[TrainIntercityEurocity] =
	     cg.readEntry(vehicleTypeToConfigName(TrainIntercityEurocity), true);
    m_showTypeOfVehicle[TrainIntercityExpress] =
	     cg.readEntry(vehicleTypeToConfigName(TrainIntercityExpress), true);
    m_showTypeOfVehicle[Ferry] =
	    cg.readEntry(vehicleTypeToConfigName(Ferry), true);
    m_showTypeOfVehicle[Plane] =
	    cg.readEntry(vehicleTypeToConfigName(Plane), true);

    m_filterTypeTarget = static_cast<FilterType>(
	    cg.readEntry("filterTypeTarget", static_cast<int>(ShowAll)) );
    m_filterTargetList = cg.readEntry("filterTargetList", QStringList());
    m_filterTargetList.removeDuplicates();

    m_filterTypeLineNumber = static_cast<FilterType>(
	    cg.readEntry("filterTypeLineNumber", static_cast<int>(ShowAll)) );
    m_filterLineNumberList = cg.readEntry("filterLineNumberList", QStringList());
    m_filterLineNumberList.removeDuplicates();

    kDebug() << "read ok, set values";
    setFilterConfigurationChanged( false );
    if ( m_configDialog )
	setValuesOfFilterConfig();

    return true;
}

void PublicTransportSettings::writeDefaultFilterConfig( KConfigGroup cg ) {
    cg.writeEntry(vehicleTypeToConfigName(Unknown), true);
    cg.writeEntry(vehicleTypeToConfigName(Tram), true);
    cg.writeEntry(vehicleTypeToConfigName(Bus), true);
    cg.writeEntry(vehicleTypeToConfigName(Subway), true);
    cg.writeEntry(vehicleTypeToConfigName(Metro), true);
    cg.writeEntry(vehicleTypeToConfigName(TrolleyBus), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainInterurban), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainRegional), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainRegionalExpress), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainInterregio), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainIntercityEurocity), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainIntercityExpress), true);
    cg.writeEntry(vehicleTypeToConfigName(Ferry), true);
    cg.writeEntry(vehicleTypeToConfigName(Plane), true);
    
    cg.writeEntry("filterTypeTarget", 0);
    cg.writeEntry("filterTargetList", QStringList());
    
    cg.writeEntry("filterTypeLineNumber", 0);
    cg.writeEntry("filterLineNumberList", QStringList());
}

bool PublicTransportSettings::writeFilterConfig( KConfigGroup cg,
						 bool mainConfig ) {
    kDebug() << "Write filter config to" << cg.name();

    if ( m_configDialog ) {
	QListWidget *selTypes = m_uiFilter.filterLineType->selectedListWidget();
	bool showUnknown = !selTypes->findItems( i18n("Unknown"), Qt::MatchExactly ).isEmpty();
	bool showTrams = !selTypes->findItems( i18n("Trams"), Qt::MatchExactly ).isEmpty();
	bool showBuses = !selTypes->findItems( i18n("Buses"), Qt::MatchExactly ).isEmpty();
	bool showSubways = !selTypes->findItems( i18n("Subways"), Qt::MatchExactly ).isEmpty();
	bool showMetros = !selTypes->findItems( i18n("Metros"), Qt::MatchExactly ).isEmpty();
	bool showTrolleyBuses = !selTypes->findItems( i18n("Trolley buses"), Qt::MatchExactly ).isEmpty();
	bool showInterurbanTrains = !selTypes->findItems( i18n("Interurban trains"), Qt::MatchExactly ).isEmpty();
	bool showRegionalTrains = !selTypes->findItems( i18n("Regional trains"), Qt::MatchExactly ).isEmpty();
	bool showRegionalExpressTrains = !selTypes->findItems( i18n("Regional express trains"), Qt::MatchExactly ).isEmpty();
	bool showInterregioTrains = !selTypes->findItems( i18n("Interregio trains"), Qt::MatchExactly ).isEmpty();
	bool showIntercityEurocityTrains = !selTypes->findItems( i18n("Intercity / Eurocity trains"), Qt::MatchExactly ).isEmpty();
	bool showIntercityExpressTrains = !selTypes->findItems( i18n("Intercity express trains"), Qt::MatchExactly ).isEmpty();
	bool showFerries = !selTypes->findItems( i18n("Ferries"), Qt::MatchExactly ).isEmpty();
	bool showPlanes = !selTypes->findItems( i18n("Planes"), Qt::MatchExactly ).isEmpty();

	if ( mainConfig ) {
	    bool changed = false;

	    if (m_showTypeOfVehicle[Unknown] != showUnknown) {
		m_showTypeOfVehicle[Unknown] = showUnknown;
		cg.writeEntry(vehicleTypeToConfigName(Unknown), showUnknown);
		changed = true;
	    }

	    if (m_showTypeOfVehicle[Tram] != showTrams) {
		m_showTypeOfVehicle[Tram] = showTrams;
		cg.writeEntry(vehicleTypeToConfigName(Tram), showTrams);
		changed = true;
	    }

	    if (m_showTypeOfVehicle[Bus] != showBuses) {
		m_showTypeOfVehicle[Bus] = showBuses;
		cg.writeEntry(vehicleTypeToConfigName(Bus), showBuses);
		changed = true;
	    }

	    if (m_showTypeOfVehicle[Subway] != showSubways) {
		m_showTypeOfVehicle[Subway] = showSubways;
		cg.writeEntry(vehicleTypeToConfigName(Subway), showSubways);
		changed = true;
	    }

	    if (m_showTypeOfVehicle[Metro] != showMetros) {
		m_showTypeOfVehicle[Metro] = showMetros;
		cg.writeEntry(vehicleTypeToConfigName(Metro), showMetros);
		changed = true;
	    }

	    if (m_showTypeOfVehicle[TrolleyBus] != showTrolleyBuses) {
		m_showTypeOfVehicle[TrolleyBus] = showTrolleyBuses;
		cg.writeEntry(vehicleTypeToConfigName(TrolleyBus), showTrolleyBuses);
		changed = true;
	    }

	    if (m_showTypeOfVehicle[TrainInterurban] != showInterurbanTrains) {
		m_showTypeOfVehicle[TrainInterurban] = showInterurbanTrains;
		cg.writeEntry(vehicleTypeToConfigName(TrainInterurban), showInterurbanTrains);
		changed = true;
	    }

	    if (m_showTypeOfVehicle[TrainRegional] != showRegionalTrains) {
		m_showTypeOfVehicle[TrainRegional] = showRegionalTrains;
		cg.writeEntry(vehicleTypeToConfigName(TrainRegional), showRegionalTrains);
		changed = true;
	    }

	    if (m_showTypeOfVehicle[TrainRegionalExpress] != showRegionalExpressTrains) {
		m_showTypeOfVehicle[TrainRegionalExpress] = showRegionalExpressTrains;
		cg.writeEntry(vehicleTypeToConfigName(TrainRegionalExpress), showRegionalExpressTrains);
		changed = true;
	    }

	    if (m_showTypeOfVehicle[TrainInterregio] != showInterregioTrains) {
		m_showTypeOfVehicle[TrainInterregio] = showInterregioTrains;
		cg.writeEntry(vehicleTypeToConfigName(TrainInterregio), showInterregioTrains);
		changed = true;
	    }

	    if (m_showTypeOfVehicle[TrainIntercityEurocity] != showIntercityEurocityTrains) {
		m_showTypeOfVehicle[TrainIntercityEurocity] = showIntercityEurocityTrains;
		cg.writeEntry(vehicleTypeToConfigName(TrainIntercityEurocity), showIntercityEurocityTrains);
		changed = true;
	    }

	    if (m_showTypeOfVehicle[TrainIntercityExpress] != showIntercityExpressTrains) {
		m_showTypeOfVehicle[TrainIntercityExpress] = showIntercityExpressTrains;
		cg.writeEntry(vehicleTypeToConfigName(TrainIntercityExpress), showIntercityExpressTrains);
		changed = true;
	    }

	    if (m_showTypeOfVehicle[Ferry] != showFerries) {
		m_showTypeOfVehicle[Ferry] = showFerries;
		cg.writeEntry(vehicleTypeToConfigName(Ferry), showFerries);
		changed = true;
	    }

	    if (m_showTypeOfVehicle[Plane] != showPlanes) {
		m_showTypeOfVehicle[Plane] = showPlanes;
		cg.writeEntry(vehicleTypeToConfigName(Plane), showPlanes);
		changed = true;
	    }


	    if (m_filterTypeTarget  != (m_uiFilter.filterTypeTarget->currentIndex())) {
		m_filterTypeTarget = static_cast<FilterType>(m_uiFilter.filterTypeTarget->currentIndex());
		cg.writeEntry("filterTypeTarget", static_cast<int>(m_filterTypeTarget));
		changed = true;
	    }

	    if (m_filterTargetList  != m_uiFilter.filterTargetList->items()) {
		m_filterTargetList = m_uiFilter.filterTargetList->items();
		m_filterTargetList.removeDuplicates();
		cg.writeEntry("filterTargetList", m_filterTargetList);
		changed = true;
	    }

	    if (m_filterTypeLineNumber != (m_uiFilter.filterTypeLineNumber->currentIndex())) {
		m_filterTypeLineNumber = static_cast<FilterType>(m_uiFilter.filterTypeLineNumber->currentIndex());
		cg.writeEntry("filterTypeLineNumber", static_cast<int>(m_filterTypeLineNumber));
		changed = true;
	    }

	    if (m_filterLineNumberList != m_uiFilter.filterLineNumberList->items()) {
		m_filterLineNumberList = m_uiFilter.filterLineNumberList->items();
		m_filterLineNumberList.removeDuplicates();
		cg.writeEntry("filterLineNumberList", m_filterLineNumberList);
		changed = true;
	    }

	    return changed;
	} else {
	    cg.writeEntry(vehicleTypeToConfigName(Unknown), showUnknown);
	    cg.writeEntry(vehicleTypeToConfigName(Tram), showTrams);
	    cg.writeEntry(vehicleTypeToConfigName(Bus), showBuses);
	    cg.writeEntry(vehicleTypeToConfigName(Subway), showSubways);
	    cg.writeEntry(vehicleTypeToConfigName(Metro), showMetros);
	    cg.writeEntry(vehicleTypeToConfigName(TrolleyBus), showTrolleyBuses);
	    cg.writeEntry(vehicleTypeToConfigName(TrainInterurban), showInterurbanTrains);
	    cg.writeEntry(vehicleTypeToConfigName(TrainRegional), showRegionalTrains);
	    cg.writeEntry(vehicleTypeToConfigName(TrainRegionalExpress), showRegionalExpressTrains);
	    cg.writeEntry(vehicleTypeToConfigName(TrainInterregio), showInterregioTrains);
	    cg.writeEntry(vehicleTypeToConfigName(TrainIntercityEurocity), showIntercityEurocityTrains);
	    cg.writeEntry(vehicleTypeToConfigName(TrainIntercityExpress), showIntercityExpressTrains);
	    cg.writeEntry(vehicleTypeToConfigName(Ferry), showFerries);
	    cg.writeEntry(vehicleTypeToConfigName(Plane), showPlanes);

	    cg.writeEntry("filterTypeTarget", m_uiFilter.filterTypeTarget->currentIndex());
	    cg.writeEntry("filterTargetList", m_uiFilter.filterTargetList->items());

	    cg.writeEntry("filterTypeLineNumber", m_uiFilter.filterTypeLineNumber->currentIndex());
	    cg.writeEntry("filterLineNumberList", m_uiFilter.filterLineNumberList->items());

	    return true;
	}
    } else { // Config dialog not shown
	cg.writeEntry(vehicleTypeToConfigName(Unknown), m_showTypeOfVehicle[Unknown]);
	cg.writeEntry(vehicleTypeToConfigName(Tram), m_showTypeOfVehicle[Tram]);
	cg.writeEntry(vehicleTypeToConfigName(Bus), m_showTypeOfVehicle[Bus]);
	cg.writeEntry(vehicleTypeToConfigName(Subway), m_showTypeOfVehicle[Subway]);
	cg.writeEntry(vehicleTypeToConfigName(Metro), m_showTypeOfVehicle[Metro]);
	cg.writeEntry(vehicleTypeToConfigName(TrolleyBus),
		      m_showTypeOfVehicle[TrolleyBus]);
	cg.writeEntry(vehicleTypeToConfigName(TrainInterurban),
		      m_showTypeOfVehicle[TrainInterurban]);
	cg.writeEntry(vehicleTypeToConfigName(TrainRegional),
		      m_showTypeOfVehicle[TrainRegional]);
	cg.writeEntry(vehicleTypeToConfigName(TrainRegionalExpress),
		      m_showTypeOfVehicle[TrainRegionalExpress]);
	cg.writeEntry(vehicleTypeToConfigName(TrainInterregio),
		      m_showTypeOfVehicle[TrainInterregio]);
	cg.writeEntry(vehicleTypeToConfigName(TrainIntercityEurocity),
		      m_showTypeOfVehicle[TrainIntercityEurocity]);
	cg.writeEntry(vehicleTypeToConfigName(TrainIntercityExpress),
		      m_showTypeOfVehicle[TrainIntercityExpress]);
	cg.writeEntry(vehicleTypeToConfigName(Ferry), m_showTypeOfVehicle[Ferry]);
	cg.writeEntry(vehicleTypeToConfigName(Plane), m_showTypeOfVehicle[Plane]);
	
	cg.writeEntry("filterTypeTarget", static_cast<int>(m_filterTypeTarget));
	cg.writeEntry("filterTargetList", m_filterTargetList);
	
	cg.writeEntry("filterTypeLineNumber", static_cast<int>(m_filterTypeLineNumber));
	cg.writeEntry("filterLineNumberList", m_filterLineNumberList);

	emit settingsChanged();
	return true;
    }
}

QString PublicTransportSettings::vehicleTypeToConfigName ( const VehicleType& vehicleType ) {
    switch ( vehicleType )
    {
	case Unknown:
	    return "showUnknown";

	case Tram:
	    return "showTrams";
	case Bus:
	    return "showBuses";
	case Subway:
	    return "showSubways";
	case TrainInterurban:
	    return "showInterurbanTrains";
	case Metro:
	    return "showMetros";
	case TrolleyBus:
	    return "showTrolleyBuses";

	case TrainRegional:
	    return "showRegionalTrains";
	case TrainRegionalExpress:
	    return "showRegionalExpressTrains";
	case TrainInterregio:
	    return "showInterregioTrains";
	case TrainIntercityEurocity:
	    return "showIntercityEurocityTrains";
	case TrainIntercityExpress:
	    return "showIntercityExpressTrains";

	case Ferry:
	    return "showFerries";
	case Plane:
	    return "showPlanes";

	default:
	    return "showUnknownTypeOfVehicle";
    }
}

void PublicTransportSettings::hideTypeOfVehicle( VehicleType vehicleType ) {
    m_showTypeOfVehicle[vehicleType] = false;

    KConfigGroup cg = m_applet->config();
    cg.writeEntry ( vehicleTypeToConfigName(vehicleType), false );

    emit configNeedsSaving();
    emit modelNeedsUpdate();
}

