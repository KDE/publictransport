/*
*   Copyright 2009 Friedrich Pülz <fpuelz@gmx.de>
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

#include <QListWidget>
#include <QTreeView>
#include <QStandardItemModel>

#include <KConfigDialog>

#include "settings.h"
#include "htmldelegate.h"

PublicTransportSettings::PublicTransportSettings( AppletWithState *applet )
    : m_modelServiceProvider(0)
{
    m_applet = applet;

//     m_stopNameValid = false;

    m_dataSourceTester = new DataSourceTester( "", m_applet );
    connect( m_dataSourceTester, SIGNAL(testResult(DataSourceTester::TestResult,const QVariant&)), this, SLOT(testResult(DataSourceTester::TestResult,const QVariant&)) );
}

void PublicTransportSettings::dataUpdated ( const QString& sourceName, const Plasma::DataEngine::Data& data ) {
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
		    QMap< QString, QVariant > serviceProviderData = m_modelServiceProvider->item(i)->data( ServiceProviderDataRole ).toMap();
		    QString favIconSource = serviceProviderData["url"].toString();
		    if ( favIconSource.compare( sourceName ) == 0 )
			m_modelServiceProvider->item(i)->setIcon( KIcon(favicon) );
		}
	    }
	}
	else
	    qDebug() << "PublicTransport::dataUpdated" << "favicon is NULL";
	m_applet->dataEngine("favicons")->disconnectSource( sourceName, this );
    }
}

void PublicTransportSettings::readSettings() {
    KConfigGroup cg = m_applet->config();
    m_updateTimeout = cg.readEntry("updateTimeout", 60);
    m_showRemainingMinutes = cg.readEntry("showRemainingMinutes", true);
    m_showDepartureTime = cg.readEntry("showDepartureTime", true);
    m_displayTimeBold = cg.readEntry("displayTimeBold", true);
    m_serviceProvider = cg.readEntry("serviceProvider", 1); // 1 is germany (db.de)
    m_city = cg.readEntry("city", "");
    m_stop = cg.readEntry("stop", "");
    m_stopID = cg.readEntry("stopID", "");
    m_timeOffsetOfFirstDeparture = cg.readEntry("timeOffsetOfFirstDeparture", 0);
    m_maximalNumberOfDepartures = cg.readEntry("maximalNumberOfDepartures", 20);
    m_alarmTime = cg.readEntry("alarmTime", 5);
    m_linesPerRow = cg.readEntry("linesPerRow", 2);
    m_departureArrivalListType = static_cast<DepartureArrivalListType>( cg.readEntry("departureArrivalListType", static_cast<int>(DepartureList)) );
    m_journeyListType = static_cast<JourneyListType>( cg.readEntry("journeyListType", static_cast<int>(JourneysFromHomeStopList)) );
    m_showHeader = cg.readEntry("showHeader", true);
    m_hideColumnTarget = cg.readEntry("hideColumnTarget", false);

    QString fontFamily = cg.readEntry("fontFamily", "");
    m_useDefaultFont = fontFamily.isEmpty();
    if ( !m_useDefaultFont )
	m_font = QFont( fontFamily );

    m_showTypeOfVehicle[Tram] = cg.readEntry(vehicleTypeToConfigName(Tram), true);
    m_showTypeOfVehicle[Bus] = cg.readEntry(vehicleTypeToConfigName(Bus), true);
    m_showTypeOfVehicle[Subway] = cg.readEntry(vehicleTypeToConfigName(Subway), true);
    m_showTypeOfVehicle[TrainInterurban] = cg.readEntry(vehicleTypeToConfigName(TrainInterurban), true);
    m_showTypeOfVehicle[TrainRegional] = cg.readEntry(vehicleTypeToConfigName(TrainRegional), true);
    m_showTypeOfVehicle[TrainRegionalExpress] = cg.readEntry(vehicleTypeToConfigName(TrainRegionalExpress), true);
    m_showTypeOfVehicle[TrainInterregio] = cg.readEntry(vehicleTypeToConfigName(TrainInterregio), true);
    m_showTypeOfVehicle[TrainIntercityEurocity] = cg.readEntry(vehicleTypeToConfigName(TrainIntercityEurocity), true);
    m_showTypeOfVehicle[TrainIntercityExpress] = cg.readEntry(vehicleTypeToConfigName(TrainIntercityExpress), true);
    m_showNightlines = cg.readEntry("showNightlines", true);
    m_filterMinLine = cg.readEntry("filterMinLine", 1);
    m_filterMaxLine = cg.readEntry("filterMaxLine", 999);
    m_filterTypeTarget = static_cast<FilterType>(cg.readEntry("filterTypeTarget", static_cast<int>(ShowAll)));
    m_filterTargetList = cg.readEntry("filterTargetList", QStringList());

    getServiceProviderInfo();
}

void PublicTransportSettings::getServiceProviderInfo() {
    Plasma::DataEngine::Data data = m_applet->dataEngine("publictransport")->query("ServiceProviders");
    foreach ( QString serviceProviderName, data.keys() )
    {
	QMap< QString, QVariant > serviceProviderData = data.value(serviceProviderName).toMap();
	if ( serviceProvider() == serviceProviderData["id"].toInt() )
	{
	    m_useSeperateCityValue = serviceProviderData["useSeperateCityValue"].toBool();
	    m_onlyUseCitiesInList = serviceProviderData["onlyUseCitiesInList"].toBool();
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

void PublicTransportSettings::filterLineTypeAvaibleSelctionChanged ( int ) {
    m_uiFilter.filterLineType->setButtonsEnabled();
}

void PublicTransportSettings::filterLineTypeSelectedSelctionChanged ( int ) {
    m_uiFilter.filterLineType->setButtonsEnabled();
}

void PublicTransportSettings::addedFilterLineType ( QListWidgetItem* ) {
    m_uiFilter.filterLineType->setButtonsEnabled();
}

void PublicTransportSettings::removedFilterLineType( QListWidgetItem* ) {
    m_uiFilter.filterLineType->setButtonsEnabled();
}

QList< VehicleType > PublicTransportSettings::filteredOutVehicleTypes() const {
    return m_showTypeOfVehicle.keys( false );
}

void PublicTransportSettings::removeAllFiltersByVehicleType() {
    // TODO: go through all vehicle types
    m_showTypeOfVehicle[Tram] = true;
    m_showTypeOfVehicle[Bus] = true;
    m_showTypeOfVehicle[Subway] = true;
    m_showTypeOfVehicle[TrainInterurban] = true;
    m_showTypeOfVehicle[TrainRegional] = true;
    m_showTypeOfVehicle[TrainRegionalExpress] = true;
    m_showTypeOfVehicle[TrainInterregio] = true;
    m_showTypeOfVehicle[TrainIntercityEurocity] = true;
    m_showTypeOfVehicle[TrainIntercityExpress] = true;

    KConfigGroup cg = m_applet->config();
    cg.writeEntry(vehicleTypeToConfigName(Tram), true);
    cg.writeEntry(vehicleTypeToConfigName(Bus), true);
    cg.writeEntry(vehicleTypeToConfigName(Subway), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainInterurban), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainRegional), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainRegionalExpress), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainInterregio), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainIntercityEurocity), true);
    cg.writeEntry(vehicleTypeToConfigName(TrainIntercityExpress), true);
    //     emit settingsChanged();
    emit configNeedsSaving();
    emit modelNeedsUpdate();

    // TODO: Synchronize new settings with config dialog widgets
}

void PublicTransportSettings::testResult( DataSourceTester::TestResult result, const QVariant &data ) {
    //     qDebug() << "PublicTransport::testResult";
    if ( !m_applet->testState(ConfigDialogShown) )
	return;

    QMap< QString, QVariant > stopToStopID;
    switch ( result ) {
	case DataSourceTester::Error:
	    setStopNameValid( false, data.toString() );
	    break;

	case DataSourceTester::JourneyListReceived:
	    setStopNameValid( true );
	    break;

	case DataSourceTester::PossibleStopsReceived:
	    setStopNameValid( false, i18n("The stop name is ambigous.") );
	    stopToStopID = data.toMap();
	    m_ui.stop->setCompletedItems( stopToStopID.keys() );
	    m_stopIDinConfig = stopToStopID.value( m_ui.stop->text(), QString() ).toString();
	    break;
    }
}

void PublicTransportSettings::setFilterTypeTarget ( FilterType filterType ) {
    m_filterTypeTarget = filterType;

    m_applet->config().writeEntry("filterTypeTarget", static_cast<int>(m_filterTypeTarget));
    emit configNeedsSaving();

    // Synchronize new settings with config dialog widgets
    if ( m_applet->testState(ConfigDialogShown) )
	m_uiFilter.filterTypeTarget->setCurrentIndex( static_cast<int>(m_filterTypeTarget) );
}

void PublicTransportSettings::configDialogFinished() {
    m_dataSourceTester->setTestSource("");
    m_applet->removeState( ConfigDialogShown );
}

void PublicTransportSettings::accessorInfoDialogFinished() {
    m_applet->removeState( AccessorInfoDialogShown );
}

bool PublicTransportSettings::checkConfig() {
    if ( m_useSeperateCityValue && (m_city.isEmpty() || m_stop.isEmpty()) )
	emit configurationRequired(true, i18n("Please set a city and a stop."));
    else if ( m_stop.isEmpty() )
	emit configurationRequired(true, i18n("Please set a stop."));
    else if ( m_serviceProvider == -1 )
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
	m_ui.stop->setCompletedItems( QStringList() );
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

void PublicTransportSettings::serviceProviderChanged ( int index ) {
    QMap< QString, QVariant > serviceProviderData = m_modelServiceProvider->item( index )->data( ServiceProviderDataRole ).toMap();
    kDebug() << "New service provider" << serviceProviderData["name"].toString();

    m_stopIDinConfig = "";
    m_dataSourceTester->clearStopToStopIdMap();

    m_ui.kledStopValidated->setState( KLed::Off );
    m_ui.kledStopValidated->setToolTip( i18n("Checking validity of the stop name.") );

    // Only show "Departures"/"Arrivals"-radio buttons if arrivals are supported by the service provider
    bool supportsArrivals = serviceProviderData["features"].toStringList().contains("Arrivals");
    m_ui.grpDefaultView->setVisible( supportsArrivals );
    if ( !supportsArrivals )
	m_ui.showDepartures->setChecked( true );

    bool useSeperateCityValue = serviceProviderData["useSeperateCityValue"].toBool();
    m_ui.lblCity->setVisible( useSeperateCityValue );
    m_ui.city->setVisible( useSeperateCityValue );

    if ( useSeperateCityValue )
    {
	m_ui.city->clear();
	QStringList cities = serviceProviderData["cities"].toStringList();
	if ( !cities.isEmpty() )
	{
	    m_ui.city->setEditText( cities.first() );
	    // 	    m_ui.city->setCompletedItems( cities, false );
	    m_ui.city->addItems( cities );
	}
	m_ui.city->setEditable( !serviceProviderData["onlyUseCitiesInList"].toBool() );
    }
    else
	m_ui.city->setEditText( "" );

    stopNameChanged( m_ui.stop->text() );
}

void PublicTransportSettings::stopNameChanged ( QString stopName ) {
    m_stopIDinConfig = m_dataSourceTester->stopToStopID( stopName );

    QMap< QString, QVariant > serviceProviderData = m_modelServiceProvider->item( m_ui.serviceProvider->currentIndex() )->data( ServiceProviderDataRole ).toMap();
    bool useSeperateCityValue = serviceProviderData["useSeperateCityValue"].toBool();
    int serviceProviderIndex = serviceProviderData["id"].toInt();
    if ( useSeperateCityValue ) {
	// 	QStringList values = stopName.split(',');
	// 	if ( values.count() > 1 )
	// 	{
	    // 	    if ( m_serviceProvider == 10 ) { // switzerland
	    // 		m_ui.city->setText( values[1].trimmed() );
	    // 		m_ui.stop->setText( values[0].trimmed() );
	    // 	    } else {
		// 		m_ui.city->setText( values[0].trimmed() );
		// 		m_ui.stop->setText( values[1].trimmed() );
		// 	    }
		// 	}
	m_dataSourceTester->setTestSource( QString("%6 %1:city%2:stop%3:maxDeps%4:timeOffset%5").arg( serviceProviderIndex ).arg( configCityValue() ).arg( m_stopIDinConfig.isEmpty() ? stopName : m_stopIDinConfig ).arg( m_ui.maximalNumberOfDepartures->value() ).arg( m_ui.timeOfFirstDeparture->value() ).arg( m_ui.showArrivals->isChecked() ? "Arrivals" : "Departures" ) );
    } else
	m_dataSourceTester->setTestSource( QString("%5 %1:stop%2:maxDeps%3:timeOffset%4").arg( serviceProviderIndex ).arg(  m_stopIDinConfig.isEmpty() ? stopName : m_stopIDinConfig ).arg( m_ui.maximalNumberOfDepartures->value() ).arg( m_ui.timeOfFirstDeparture->value() ).arg( m_ui.showArrivals->isChecked() ? "Arrivals" : "Departures" ) );
}

void PublicTransportSettings::clickedServiceProviderInfo ( bool ) {
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

    QMap< QString, QVariant > serviceProviderData = m_modelServiceProvider->item( m_ui.serviceProvider->currentIndex() )->data( ServiceProviderDataRole ).toMap();
    QIcon favIcon = m_ui.serviceProvider->itemIcon( m_ui.serviceProvider->currentIndex() );
    m_uiAccessorInfo.icon->setPixmap( favIcon.pixmap(32) );
    m_uiAccessorInfo.serviceProviderName->setText( m_ui.serviceProvider->currentText() );
    m_uiAccessorInfo.version->setText( i18n("Version %1", serviceProviderData["version"].toString()) );
    m_uiAccessorInfo.url->setUrl( serviceProviderData["url"].toString() );
    m_uiAccessorInfo.url->setText( QString("<a href='%1'>%1</a>").arg( serviceProviderData["url"].toString() ) );

    if ( serviceProviderData["email"].toString().isEmpty() )
	m_uiAccessorInfo.author->setText( QString("%1").arg( serviceProviderData["author"].toString() ) );
    else {
	m_uiAccessorInfo.author->setText( QString("<a href='mailto:%2'>%1</a>").arg( serviceProviderData["author"].toString() ).arg( serviceProviderData["email"].toString() ) );
	m_uiAccessorInfo.author->setToolTip( i18n("Write an email to %1 <%2>").arg( serviceProviderData["author"].toString() ).arg( serviceProviderData["email"].toString() ) );
    }
    m_uiAccessorInfo.description->setText( serviceProviderData["description"].toString() );
    m_uiAccessorInfo.features->setText( serviceProviderData["features"].toStringList().join(", ") );

    infoDialog->show();
}

void PublicTransportSettings::createConfigurationInterface ( KConfigDialog* parent, bool stopNameValid ) {
    m_configDialog = parent;
    m_applet->addState( ConfigDialogShown );
    parent->setButtons( KDialog::Ok | KDialog::Apply | KDialog::Cancel );

    QWidget *widget = new QWidget;
    QWidget *widgetAppearance = new QWidget;
    QWidget *widgetFilter = new QWidget;

    m_ui.setupUi(widget);
    m_uiAppearance.setupUi(widgetAppearance);
    m_uiFilter.setupUi(widgetFilter);

    connect( parent, SIGNAL(finished()), this, SLOT(configDialogFinished()));
    connect( parent, SIGNAL(applyClicked()), this, SLOT(configAccepted()) );
    connect( parent, SIGNAL(okClicked()), this, SLOT(configAccepted()) );

    parent->addPage(widget, i18n("General"), "public-transport-stop");
    parent->addPage(widgetAppearance, i18n("Appearance"), "package_settings_looknfeel");
    parent->addPage(widgetFilter, i18n("Filter"), "view-filter");

    if (m_updateTimeout == 0) {
	m_ui.updateAutomatically->setChecked(false);
	m_ui.updateTimeout->setValue(60); // Set to default
    }
    else {
	m_ui.updateAutomatically->setChecked(true);
	m_ui.updateTimeout->setValue(m_updateTimeout);
    }
    m_ui.stop->setText(m_stop);
    m_ui.timeOfFirstDeparture->setValue(m_timeOffsetOfFirstDeparture);
    m_ui.maximalNumberOfDepartures->setValue(m_maximalNumberOfDepartures);
    m_ui.alarmTime->setValue(m_alarmTime);

    m_uiAppearance.linesPerRow->setValue(m_linesPerRow);
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

    m_ui.showDepartures->setChecked( m_departureArrivalListType == DepartureList );
    m_ui.showArrivals->setChecked( m_departureArrivalListType == ArrivalList );
    m_ui.showJourneysFromHomeStop->setChecked( m_journeyListType == JourneysFromHomeStopList );
    m_ui.showJourneysToHomeStop->setChecked( m_journeyListType == JourneysToHomeStopList );
    m_ui.btnServiceProviderInfo->setIcon( KIcon("help-about") );
    m_ui.btnServiceProviderInfo->setText( "" );

    if ( m_modelServiceProvider != NULL )
	delete m_modelServiceProvider;
    m_modelServiceProvider = new QStandardItemModel( 0, 1 );
    m_ui.serviceProvider->setModel( m_modelServiceProvider );

    HtmlDelegate *htmlDelegate = new HtmlDelegate;
    htmlDelegate->setAlignText( true );
    m_ui.serviceProvider->setItemDelegate( htmlDelegate );

    Plasma::DataEngine::Data data = m_applet->dataEngine("publictransport")->query("ServiceProviders");
    foreach ( QString serviceProviderName, data.keys() )
    {
	QMap< QString, QVariant > serviceProviderData = data.value(serviceProviderName).toMap();
	bool isCountryWide = serviceProviderName.contains( serviceProviderData["country"].toString(), Qt::CaseInsensitive );
	QString formattedText;
	QStandardItem *item = new QStandardItem( serviceProviderName );

	// TODO: Add titles for city specific accessors
	if ( isCountryWide ) {
	    item->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole ); formattedText = QString( "<b>%1</b><br-wrap><small><b>Features:</b> %2</small>" ).arg( serviceProviderName ).arg( serviceProviderData["features"].toStringList().join(", ")/*.replace(QRegExp("([^,]*,[^,]*,[^,]*,)"), "\\1<br>")*/ );
	    item->setData( 4, HtmlDelegate::LinesPerRowRole );
	}
	else {
	    item->setData( QStringList() << "sunken" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole ); formattedText = QString( "<b>%1</b><br-wrap><small><b>Features:</b> %2</small>" ).arg( serviceProviderName ).arg( serviceProviderData["features"].toStringList().join(", ")/*.replace(QRegExp("([^,]*,[^,]*,[^,]*,)"), "\\1<br>")*/ );
	    item->setData( 3, HtmlDelegate::LinesPerRowRole );
	}
	item->setData( formattedText, HtmlDelegate::FormattedTextRole );
	item->setData( serviceProviderData, ServiceProviderDataRole );
	// Sort service providers containing the contry in it's name to the top of the list for that count
	QString sortString = isCountryWide
	? serviceProviderData["country"].toString() + "AAAAA" + serviceProviderName
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
	QString title = m_modelServiceProvider->item( row )->data( ServiceProviderDataRole ).toMap()["country"].toString();
	if ( lastTitles.contains(title) )
	    continue;

	QStandardItem *itemTitle = new QStandardItem(title);
	itemTitle->setData( "<span style='font-weight:bold;font-size:large;text-decoration:underline;'>" + title + ":</span>", HtmlDelegate::FormattedTextRole );
	itemTitle->setData( 0, HtmlDelegate::GroupTitleRole );
	itemTitle->setData( 2, HtmlDelegate::LinesPerRowRole );
	itemTitle->setSelectable( false );
	m_modelServiceProvider->insertRow( row, itemTitle );

	lastTitles << title;
	++row;
    }

    // Get (combobox-) index of the currently selected service provider
    int curServiceProviderIndex = -1;
    for ( int i = 0; i < m_ui.serviceProvider->count(); ++i )
    {
	if ( m_ui.serviceProvider->itemData( i, ServiceProviderDataRole ).toMap()["id"].toInt() == m_serviceProvider ) {
	    curServiceProviderIndex = i;
	    break;
	}
    }
    connect( m_ui.serviceProvider, SIGNAL(currentIndexChanged(int)), this, SLOT(serviceProviderChanged(int)) );
    if ( curServiceProviderIndex != -1 )
	m_ui.serviceProvider->setCurrentIndex( curServiceProviderIndex );

    QMap<QString, QVariant> serviceProviderData = m_ui.serviceProvider->itemData( curServiceProviderIndex, ServiceProviderDataRole ).toMap();
    if ( serviceProviderData["onlyUseCitiesInList"].toBool() )
	m_ui.city->setEditText(m_city);
    else
	m_ui.city->setCurrentItem(m_city);

    QListWidget *available, *selected;
    available = m_uiFilter.filterLineType->availableListWidget();
    selected = m_uiFilter.filterLineType->selectedListWidget();

    available->addItems( QStringList() << i18n("Trams") << i18n("Buses") << i18n("Subways") << i18n("Interurban trains") << i18n("Regional trains") << i18n("Regional express trains") << i18n("Interregio trains") << i18n("Intercity / Eurocity trains") << i18n("Intercity express trains") );

    available->item( 0 )->setIcon( Global::iconFromVehicleType(Tram) );
    available->item( 1 )->setIcon( Global::iconFromVehicleType(Bus) );
    available->item( 2 )->setIcon( Global::iconFromVehicleType(Subway) );
    available->item( 3 )->setIcon( Global::iconFromVehicleType(TrainInterurban) );
    available->item( 4 )->setIcon( Global::iconFromVehicleType(TrainRegional) );
    available->item( 5 )->setIcon( Global::iconFromVehicleType(TrainRegionalExpress) );
    available->item( 6 )->setIcon( Global::iconFromVehicleType(TrainInterregio) );
    available->item( 7 )->setIcon( Global::iconFromVehicleType(TrainIntercityEurocity) );
    available->item( 8 )->setIcon( Global::iconFromVehicleType(TrainIntercityExpress) );

    if ( m_showTypeOfVehicle[TrainIntercityExpress] )
	selected->addItem( available->takeItem(8) );
    if ( m_showTypeOfVehicle[TrainIntercityEurocity] )
	selected->addItem( available->takeItem(7) );
    if ( m_showTypeOfVehicle[TrainInterregio] )
	selected->addItem( available->takeItem(6) );
    if ( m_showTypeOfVehicle[TrainRegionalExpress] )
	selected->addItem( available->takeItem(5) );
    if ( m_showTypeOfVehicle[TrainRegional] )
	selected->addItem( available->takeItem(4) );
    if ( m_showTypeOfVehicle[TrainInterurban] )
	selected->addItem( available->takeItem(3) );
    if ( m_showTypeOfVehicle[Subway] )
	selected->addItem( available->takeItem(2) );
    if ( m_showTypeOfVehicle[Bus] )
	selected->addItem( available->takeItem(1) );
    if ( m_showTypeOfVehicle[Tram] )
	selected->addItem( available->takeItem(0) );

    m_uiFilter.showNightLines->setChecked(m_showNightlines);
    m_uiFilter.filterMinLine->setValue(m_filterMinLine);
    m_uiFilter.filterMaxLine->setValue(m_filterMaxLine);
    m_uiFilter.filterTypeTarget->setCurrentIndex( static_cast<int>(m_filterTypeTarget) );
    m_uiFilter.filterTargetList->setItems(m_filterTargetList);

    setStopNameValid( stopNameValid, "" );

    connect( m_ui.stop, SIGNAL(userTextChanged(QString)), this, SLOT(stopNameChanged(QString)) );
    connect( m_ui.btnServiceProviderInfo, SIGNAL(clicked(bool)), this, SLOT(clickedServiceProviderInfo(bool)));

    connect( selected, SIGNAL(currentRowChanged(int)), this, SLOT(filterLineTypeSelectedSelctionChanged(int)) );
    connect( available, SIGNAL(currentRowChanged(int)), this, SLOT(filterLineTypeAvaibleSelctionChanged(int)) );
    connect( m_uiFilter.filterLineType, SIGNAL(added(QListWidgetItem*)), this, SLOT(addedFilterLineType(QListWidgetItem*)) );
    connect( m_uiFilter.filterLineType, SIGNAL(removed(QListWidgetItem*)), this, SLOT(removedFilterLineType(QListWidgetItem*)) );
}

void PublicTransportSettings::configAccepted() {
    bool changed = false, changedServiceProviderSettings = false;

    QMap< QString, QVariant > serviceProviderData = m_modelServiceProvider->item( m_ui.serviceProvider->currentIndex() )->data( ServiceProviderDataRole ).toMap();
    const int serviceProviderIndex = serviceProviderData["id"].toInt();
    if (m_serviceProvider != serviceProviderIndex) {
	m_serviceProvider = serviceProviderIndex;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("serviceProvider", m_serviceProvider);
	changed = true;

	changedServiceProviderSettings = true;
	// 	m_applet->addState( WaitingForDepartureData );
	emit departureListNeedsClearing(); // Clear departures from the old service provider

	m_onlyUseCitiesInList = serviceProviderData["onlyUseCitiesInList"].toBool();
	m_useSeperateCityValue = serviceProviderData["useSeperateCityValue"].toBool();
    }

    if (!m_ui.updateAutomatically->isChecked() && m_updateTimeout != 0) {
	m_updateTimeout = 0;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("updateTimeout", m_updateTimeout);
	changed = true;
    } else if (m_updateTimeout  != m_ui.updateTimeout->value()) {
	m_updateTimeout = m_ui.updateTimeout->value();
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

    if (m_city  != configCityValue()) {
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

    if (m_stop  != m_ui.stop->text()) {
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

    if (m_timeOffsetOfFirstDeparture  != m_ui.timeOfFirstDeparture->value()) {
	m_timeOffsetOfFirstDeparture = m_ui.timeOfFirstDeparture->value();
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("timeOffsetOfFirstDeparture", m_timeOffsetOfFirstDeparture);
	changed = true;

	changedServiceProviderSettings = true;
// 	m_applet->addState( WaitingForDepartureData );
    }

    if (m_maximalNumberOfDepartures  != m_ui.maximalNumberOfDepartures->value()) {
	m_maximalNumberOfDepartures = m_ui.maximalNumberOfDepartures->value();
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("maximalNumberOfDepartures", m_maximalNumberOfDepartures);
	changed = true;

	changedServiceProviderSettings = true;
// 	m_applet->addState( WaitingForDepartureData );
    }

    if (m_alarmTime  != m_ui.alarmTime->value()) {
	m_alarmTime = m_ui.alarmTime->value();
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("alarmTime", m_alarmTime);
	changed = true;
    }

    if (m_linesPerRow  != m_uiAppearance.linesPerRow->value()) {
	m_linesPerRow = m_uiAppearance.linesPerRow->value();
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("linesPerRow", m_linesPerRow);
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

    if ( (m_departureArrivalListType == DepartureList && !m_ui.showDepartures->isChecked()) ||
	 (m_departureArrivalListType == ArrivalList && !m_ui.showArrivals->isChecked()) )
    {
	if ( m_ui.showArrivals->isChecked() )
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

    if ((m_journeyListType == JourneysFromHomeStopList && !m_ui.showJourneysFromHomeStop->isChecked()) ||
	(m_journeyListType == JourneysToHomeStopList && !m_ui.showJourneysToHomeStop->isChecked()))
    {
	if ( m_ui.showJourneysFromHomeStop->isChecked() )
	    m_journeyListType = JourneysFromHomeStopList;
	else
	    m_journeyListType = JourneysToHomeStopList;

	KConfigGroup cg = m_applet->config();
	cg.writeEntry("journeyListType", static_cast<int>(m_journeyListType));
	changed = true;

	changedServiceProviderSettings = true;
// 	m_applet->addState( WaitingForJourneyData );
	emit journeyListTypeChanged( m_journeyListType );
    }

    QListWidget *selTypes = m_uiFilter.filterLineType->selectedListWidget();
    bool showTrams = !selTypes->findItems( i18n("Trams"), Qt::MatchExactly ).isEmpty();
    bool showBuses = !selTypes->findItems( i18n("Buses"), Qt::MatchExactly ).isEmpty();
    bool showSubways = !selTypes->findItems( i18n("Subways"), Qt::MatchExactly ).isEmpty();
    bool showInterurbanTrains = !selTypes->findItems( i18n("Interurban trains"), Qt::MatchExactly ).isEmpty();
    bool showRegionalTrains = !selTypes->findItems( i18n("Regional trains"), Qt::MatchExactly ).isEmpty();
    bool showRegionalExpressTrains = !selTypes->findItems( i18n("Regional express trains"), Qt::MatchExactly ).isEmpty();
    bool showInterregioTrains = !selTypes->findItems( i18n("Interregio trains"), Qt::MatchExactly ).isEmpty();
    bool showIntercityEurocityTrains = !selTypes->findItems( i18n("Intercity / Eurocity trains"), Qt::MatchExactly ).isEmpty();
    bool showIntercityExpressTrains = !selTypes->findItems( i18n("Intercity express trains"), Qt::MatchExactly ).isEmpty();

    if (m_showTypeOfVehicle[Tram] != showTrams) {
	m_showTypeOfVehicle[Tram] = showTrams;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry(vehicleTypeToConfigName(Tram), showTrams);
	changed = true;
    }

    if (m_showTypeOfVehicle[Bus] != showBuses) {
	m_showTypeOfVehicle[Bus] = showBuses;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry(vehicleTypeToConfigName(Bus), showBuses);
	changed = true;
    }

    if (m_showTypeOfVehicle[Subway] != showSubways) {
	m_showTypeOfVehicle[Subway] = showSubways;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry(vehicleTypeToConfigName(Subway), showSubways);
	changed = true;
    }

    if (m_showTypeOfVehicle[TrainInterurban] != showInterurbanTrains) {
	m_showTypeOfVehicle[TrainInterurban] = showInterurbanTrains;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry(vehicleTypeToConfigName(TrainInterurban), showInterurbanTrains);
	changed = true;
    }

    if (m_showTypeOfVehicle[TrainRegional] != showRegionalTrains) {
	m_showTypeOfVehicle[TrainRegional] = showRegionalTrains;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry(vehicleTypeToConfigName(TrainRegional), showRegionalTrains);
	changed = true;
    }

    if (m_showTypeOfVehicle[TrainRegionalExpress] != showRegionalExpressTrains) {
	m_showTypeOfVehicle[TrainRegionalExpress] = showRegionalExpressTrains;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry(vehicleTypeToConfigName(TrainRegionalExpress), showRegionalExpressTrains);
	changed = true;
    }

    if (m_showTypeOfVehicle[TrainInterregio] != showInterregioTrains) {
	m_showTypeOfVehicle[TrainInterregio] = showInterregioTrains;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry(vehicleTypeToConfigName(TrainInterregio), showInterregioTrains);
	changed = true;
    }

    if (m_showTypeOfVehicle[TrainIntercityEurocity] != showIntercityEurocityTrains) {
	m_showTypeOfVehicle[TrainIntercityEurocity] = showIntercityEurocityTrains;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry(vehicleTypeToConfigName(TrainIntercityEurocity), showIntercityEurocityTrains);
	changed = true;
    }

    if (m_showTypeOfVehicle[TrainIntercityExpress] != showIntercityExpressTrains) {
	m_showTypeOfVehicle[TrainIntercityExpress] = showIntercityExpressTrains;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry(vehicleTypeToConfigName(TrainIntercityExpress), showIntercityExpressTrains);
	changed = true;
    }

    if (m_showNightlines != (m_uiFilter.showNightLines->checkState() == Qt::Checked)) {
	m_showNightlines = !m_showNightlines;
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("showNightlines", m_showNightlines);
	changed = true;
    }

    if (m_filterMinLine  != m_uiFilter.filterMinLine->value()) {
	m_filterMinLine = m_uiFilter.filterMinLine->value();
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("filterMinLine", m_filterMinLine);
	changed = true;
    }

    if (m_filterMaxLine  != m_uiFilter.filterMaxLine->value()) {
	m_filterMaxLine = m_uiFilter.filterMaxLine->value();
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("filterMaxLine", m_filterMaxLine);
	changed = true;
    }

    if (m_filterTypeTarget  != (m_uiFilter.filterTypeTarget->currentIndex())) {
	m_filterTypeTarget = static_cast<FilterType>(m_uiFilter.filterTypeTarget->currentIndex());
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("filterTypeTarget", static_cast<int>(m_filterTypeTarget));
	changed = true;
    }

    if (m_filterTargetList  != m_uiFilter.filterTargetList->items()) {
	m_filterTargetList = m_uiFilter.filterTargetList->items();
	m_filterTargetList.removeDuplicates();
	KConfigGroup cg = m_applet->config();
	cg.writeEntry("filterTargetList", m_filterTargetList);
	changed = true;
    }

    if (changed) {
	emit settingsChanged();
	emit configNeedsSaving();
    }
    if (changedServiceProviderSettings)
	emit serviceProviderSettingsChanged();
}

QString PublicTransportSettings::vehicleTypeToConfigName ( const VehicleType& vehicleType ) {
    switch ( vehicleType )
    {
	case Tram:
	    return "showTrams";
	case Bus:
	    return "showBuses";
	case Subway:
	    return "showSubways";
	case TrainInterurban:
	    return "showInterurbanTrains";

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

	case Unknown:
	default:
	    return "showUnknownTypeOfVehicle";
    }
}

