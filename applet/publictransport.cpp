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

// KDE includes
#undef QT_NO_DEBUG
#include <KDebug>
#include <KLocale>
#include <KIconEffect>
#include <KNotification>
#include <KConfigDialog>

// Plasma includes
#include <Plasma/PaintUtils>
#include <Plasma/Svg>
#include <Plasma/Theme>
#include <Plasma/ToolTipManager>
#include <Plasma/ToolTipContent>

// Qt includes
#include <QPainter>
#include <QFontMetrics>
#include <QSizeF>
#include <QTextDocument>
#include <QGraphicsLinearLayout>
#include <QListWidget>
#include <QMenu>
#include <QDBusInterface>
#include <QDBusReply>
#include <QTreeView>
#include <qmath.h>

// Own includes
#include "publictransport.h"
#include "htmldelegate.h"


PublicTransport::PublicTransport(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_svg(this),
    m_icon("public-transport-stop"),
    m_graphicsWidget(0),
    m_label(0),
    m_treeView(0),
    m_model(0),
    m_modelServiceProvider(0),
    m_currentSource(""),
    m_useSeperateCityValue(true)
{
    setBackgroundHints(DefaultBackground);
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
    m_svg.setImagePath("widgets/background");
    setHasConfigurationInterface(true);
    resize(300, 200);
    m_settingsJustChanged = false;

    m_dataSourceTester = new DataSourceTester( "", this );
    connect( m_dataSourceTester, SIGNAL(testResult(DataSourceTester::TestResult,const QVariant&)), this, SLOT(testResult(DataSourceTester::TestResult,const QVariant&)) );

    m_isAccessorInfoDialogShown = m_isConfigDialogShown = false;
}

PublicTransport::~PublicTransport() {
    if (hasFailedToLaunch()) {
        // Do some cleanup here
    } else {
	delete m_label; // I *think* this gets deleted when I delete m_graphicsWidget...
// 	delete m_treeWidget; // I *think* this gets deleted when I delete m_graphicsWidget...
	delete m_graphicsWidget; // ...which I *think* also gets deleted automatically by plasma?
    }
}

void PublicTransport::init() {
//     if (m_icon.isNull()) {
//         setFailedToLaunch(true, i18n("No world to say hello"));
//     }

    // Read config
    KConfigGroup cg = config();
    m_updateTimeout = cg.readEntry("updateTimeout", 60);
    m_showRemainingMinutes = cg.readEntry("showRemainingMinutes", true);
    m_showDepartureTime = cg.readEntry("showDepartureTime", true);
    m_serviceProvider = cg.readEntry("serviceProvider", 7); // 7 is germany (db.de)
    m_city = cg.readEntry("city", "");
    m_stop = cg.readEntry("stop", "");
    m_stopID = cg.readEntry("stopID", "");
    m_timeOffsetOfFirstDeparture = cg.readEntry("timeOffsetOfFirstDeparture", 0);
    m_maximalNumberOfDepartures = cg.readEntry("maximalNumberOfDepartures", 20);
    m_alarmTime = cg.readEntry("alarmTime", 5);
    m_linesPerRow = cg.readEntry("linesPerRow", 2);
    m_dataType = static_cast<DataType>( cg.readEntry("dataType", static_cast<int>(Departures)) );

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

    createModel();
    graphicsWidget();
    createTooltip();
    createPopupIcon();

    connect(this, SIGNAL(geometryChanged()), this, SLOT(geometryChanged()));
    connect(this, SIGNAL(settingsChanged()), this, SLOT(configChanged()));
    emit settingsChanged();

    setupActions();
}

void PublicTransport::setupActions() {
    QAction *actionUpdate = new QAction( KIcon("view-refresh"), i18n("&Update timetable"), this );
    connect( actionUpdate, SIGNAL(triggered(bool)), this, SLOT(updateDataSource(bool)) );
    addAction( "updateTimetable", actionUpdate );

    QAction *actionSetAlarmForDeparture = new QAction( makeOverlayIcon(KIcon("kalarm"), "list-add"), m_dataType == Departures ? i18n("Set &alarm for this departure") : i18n("Set &alarm for this arrival"), this );
    connect( actionSetAlarmForDeparture, SIGNAL(triggered(bool)), this, SLOT(setAlarmForDeparture(bool)) );
    addAction( "setAlarmForDeparture", actionSetAlarmForDeparture );

    QAction *actionRemoveAlarmForDeparture = new QAction( makeOverlayIcon(KIcon("kalarm"), "list-remove"), m_dataType == Departures ? i18n("Remove &alarm for this departure") : i18n("Remove &alarm for this arrival"), this );
    connect( actionRemoveAlarmForDeparture, SIGNAL(triggered(bool)), this, SLOT(removeAlarmForDeparture(bool)) );
    addAction( "removeAlarmForDeparture", actionRemoveAlarmForDeparture );

    QAction *actionAddTargetToFilterList = new QAction( makeOverlayIcon(KIcon("view-filter"), "list-add"), m_dataType == Departures ? i18n("&Hide target") : i18n("&Hide origin"), this );
    connect( actionAddTargetToFilterList, SIGNAL(triggered(bool)), this, SLOT(addTargetToFilterList(bool)) );
    addAction( "addTargetToFilterList", actionAddTargetToFilterList );

    QAction *actionRemoveTargetFromFilterList = new QAction( makeOverlayIcon(KIcon("view-filter"), "list-remove"), m_dataType == Departures ? i18n("Remove target from the &filter list") : i18n("Remove origin from the &filter list"), this );
    connect( actionRemoveTargetFromFilterList, SIGNAL(triggered(bool)), this, SLOT(removeTargetFromFilterList(bool)) );
    addAction( "removeTargetFromFilterList", actionRemoveTargetFromFilterList );

    QAction *actionAddTargetToFilterListAndHide = new QAction( makeOverlayIcon(KIcon("view-filter"), "list-add"), m_dataType == Departures ? i18n("&Hide target") : i18n("&Hide origin"), this );
    connect( actionAddTargetToFilterListAndHide, SIGNAL(triggered(bool)), this, SLOT(addTargetToFilterListAndHide(bool)) );
    addAction( "addTargetToFilterListAndHide", actionAddTargetToFilterListAndHide );

    QAction *actionSetFilterListToHideMatching = new QAction( KIcon("view-filter"), m_dataType == Departures ? i18n("&Hide target") : i18n("&Hide origin"), this );
    connect( actionSetFilterListToHideMatching, SIGNAL(triggered(bool)), this, SLOT(setFilterListToHideMatching(bool)) );
    addAction( "setFilterListToHideMatching", actionSetFilterListToHideMatching );

    QAction *actionSetFilterListToShowAll = new QAction( makeOverlayIcon(KIcon("view-filter"), "edit-delete"), m_dataType == Departures ? i18n("&Show all targets") : i18n("&Show all origins"), this );
    connect( actionSetFilterListToShowAll, SIGNAL(triggered(bool)), this, SLOT(setFilterListToShowAll(bool)) );
    addAction( "setFilterListToShowAll", actionSetFilterListToShowAll );

    QAction *actionFilterOutByVehicleType = new QAction( KIcon("view-filter"), i18n("Filter out by vehicle type"), this );
    connect( actionFilterOutByVehicleType, SIGNAL(triggered(bool)), this, SLOT(filterOutByVehicleType(bool)) );
    addAction( "filterOutByVehicleType", actionFilterOutByVehicleType );

    QAction *actionRemoveAllFiltersByVehicleType = new QAction( makeOverlayIcon(KIcon("view-filter"), "edit-delete"), i18n("Sho&w all vehicle types"), this );
    connect( actionRemoveAllFiltersByVehicleType, SIGNAL(triggered(bool)), this, SLOT(removeAllFiltersByVehicleType(bool)) );
    addAction( "removeAllFiltersByVehicleType", actionRemoveAllFiltersByVehicleType );

    QAction *actionToggleExpanded = new QAction( KIcon("arrow-down"), i18n("&Show additional information"), this );
    connect( actionToggleExpanded, SIGNAL(triggered(bool)), this, SLOT(toggleExpanded(bool)) );
    addAction( "toggleExpanded", actionToggleExpanded );
}

QList< QAction* > PublicTransport::contextualActions() {
    return QList< QAction* >() << action( "updateTimetable" );
}

void PublicTransport::updateDataSource ( bool ) {
    reconnectSource();
}

QString PublicTransport::stop() const {
    return m_stopID.isEmpty() ? m_stop : m_stopID;
}

void PublicTransport::reconnectSource() {
    if ( !m_currentSource.isEmpty() )
    {
	kDebug() << "Disconnect data source" << m_currentSource;
	dataEngine("publictransport")->disconnectSource(m_currentSource, this);
    }

    if ( m_useSeperateCityValue )
	m_currentSource = QString("%6 %1:city%2:stop%3:maxDeps%4:timeOffset%5").arg( static_cast<int>(m_serviceProvider) ).arg( m_city ).arg( stop() ).arg( m_maximalNumberOfDepartures ).arg( m_timeOffsetOfFirstDeparture ).arg( m_dataType == Arrivals ? "Arrivals" : "Departures" );
    else
	m_currentSource = QString("%5 %1:stop%2:maxDeps%3:timeOffset%4").arg( static_cast<int>(m_serviceProvider) ).arg( stop() ).arg( m_maximalNumberOfDepartures ).arg( m_timeOffsetOfFirstDeparture ).arg( m_dataType == Arrivals ? "Arrivals" : "Departures" );

    qDebug() << "Connect data source" << m_currentSource << "Timeout" << m_updateTimeout;
    setBusy( true );
    if ( m_updateTimeout == 0 )
	dataEngine("publictransport")->connectSource( m_currentSource, this );
    else
	dataEngine("publictransport")->connectSource( m_currentSource, this, m_updateTimeout * 1000, Plasma::AlignToMinute );
}

void PublicTransport::testResult ( DataSourceTester::TestResult result, const QVariant &data ) {
//     qDebug() << "PublicTransport::testResult";
    if ( !m_isConfigDialogShown )
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

void PublicTransport::setStopNameValid ( bool valid, const QString &toolTip ) {
    // For safety's sake:
    if ( !m_isConfigDialogShown )
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

void PublicTransport::processJourneyList ( const Plasma::DataEngine::Data& data ) {
    // Remove old journey list
    m_departureInfos.clear();

    for (int i = 0; i < data.keys().count(); ++i)
    {
	QVariant departureData = data.value( QString("%1").arg(i) );
	if ( !departureData.isValid() || m_departureInfos.count() >= m_maximalNumberOfDepartures )
	    break;

	QMap<QString, QVariant> dataMap = departureData.toMap();
	DepartureInfo departureInfo( dataMap["line"].toString(), dataMap["target"].toString(), dataMap["departure"].toDateTime(), static_cast<VehicleType>(dataMap["lineType"].toInt()), dataMap["nightline"].toBool(), dataMap["expressline"].toBool(), dataMap["platform"].toString(), dataMap["delay"].toInt(), dataMap["delayReason"].toString(), dataMap["journeyNews"].toString() );

	// Only add departures that are in the future
	QDateTime predictedDeparture = departureInfo.predictedDeparture();
	int secsToDepartureTime = QDateTime::currentDateTime().secsTo( predictedDeparture.addSecs(-m_timeOffsetOfFirstDeparture * 60) );
	if ( -secsToDepartureTime / 3600 >= 23 )
	    secsToDepartureTime += 24 * 3600;
	if ( secsToDepartureTime > -60 )
	    m_departureInfos.append( departureInfo );
    }

    kDebug() << m_departureInfos.count() << "departures received";
    setBusy( false );
    setConfigurationRequired( false );
    m_stopNameValid = true;
    m_lastSourceUpdate = data["updated"].toDateTime();
    updateModel();
}

void PublicTransport::clearDepartures() {
    m_departureInfos.clear(); // Clear data from data engine
    if (m_model != NULL)
	m_model->removeRows( 0, m_model->rowCount() ); // Clear data to be displayed
	updateModel(); // TODO: Check if this is needed
}

void PublicTransport::processData ( const Plasma::DataEngine::Data& data ) {
    // Check for errors from the data engine
    if ( data.value("error").toBool() ) {
	m_stopNameValid = false;
// 	clearDepartures();     Dont't clear on error, departures in the past will get filtered by filterOut()
	if ( m_settingsJustChanged ) {
	    if ( m_dataType == Departures )
		this->setConfigurationRequired(true, i18n("Error parsing departure information or currently no departures"));
	    else
		this->setConfigurationRequired(true, i18n("Error parsing arrival information or currently no arrivals"));
	}

	// Update remaining times
	updateModel();
    } else {
	// Check if we got a possible stop list or a journey list
	if ( data.value("receivedPossibleStopList").toBool() ) {
	    m_stopNameValid = false;
	    clearDepartures();
	    this->setConfigurationRequired(true, i18n("The stop name is ambigous."));
	} else {
	    // List of journeys received
	    m_stopNameValid = true;
	    processJourneyList( data );
	}
    }

    m_settingsJustChanged = false;
}

void PublicTransport::dataUpdated ( const QString& sourceName, const Plasma::DataEngine::Data& data ) {
//     qDebug() << "PublicTransport::dataUpdated" << sourceName; // << data;

    // TODO: connect favicon-data engine to a seperate class?
    if ( sourceName.contains(QRegExp("^http")) ) {
	if ( m_modelServiceProvider == NULL )
	    return;

	QPixmap favicon(QPixmap::fromImage(data["Icon"].value<QImage>()));
	if ( !favicon.isNull() ) {
	    if ( m_isAccessorInfoDialogShown )
		m_uiAccessorInfo.icon->setPixmap( favicon );
	    if ( m_isConfigDialogShown ) {
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
	dataEngine("favicons")->disconnectSource( sourceName, this );
    } else {
// 	this->setBusy(true);
	processData( data );
	createTooltip();
	createPopupIcon();
// 	this->setBusy(false);
    }
}

void PublicTransport::geometryChanged() {
    if ( !isPopupShowing() ) {
	m_label->setMaximumWidth( contentsRect().width() );
    }

    QTreeView *treeView = (QTreeView*)m_treeView->widget();
    disconnect ( treeView->header(), SIGNAL(sectionResized(int,int,int)), this, SLOT(treeViewSectionResized(int,int,int)) );

//     treeView->resizeColumnToContents( 0 );
//     int lineColumnWidth = treeView->columnWidth(0);
//     if ( m_linesPerRow > 1 && treeView->columnWidth(0) < 50 )
// 	treeView->setColumnWidth( 0, 50 );

//     treeView->resizeColumnToContents( 2 );

    QHeaderView *header = treeView->header();
    int lineSectionSize = treeView->columnWidth(0);
    int departureSectionSize = treeView->columnWidth(2);
    if ( lineSectionSize + departureSectionSize > header->width() - 10 )
    {
	float lineSectionSizeFactor = (float)lineSectionSize / (float)(lineSectionSize + departureSectionSize);
	lineSectionSize = header->width() * lineSectionSizeFactor;
	treeView->setColumnWidth( 0, lineSectionSize );
	departureSectionSize = header->width() * (1 - lineSectionSizeFactor);
	treeView->setColumnWidth( 2, departureSectionSize );
    }

    int targetSectionSize = header->width() - lineSectionSize - departureSectionSize;
    if ( targetSectionSize < 10 )
	targetSectionSize = 10;
    treeView->setColumnWidth( 1, targetSectionSize );

    connect ( treeView->header(), SIGNAL(sectionResized(int,int,int)), this, SLOT(treeViewSectionResized(int,int,int)) );
}

void PublicTransport::dialogSizeChanged() {
    geometryChanged();
}

void PublicTransport::treeViewSectionResized ( int logicalIndex, int oldSize, int newSize ) {
    Q_UNUSED( logicalIndex );
    Q_UNUSED( oldSize );
    Q_UNUSED( newSize );
    geometryChanged();
}

void PublicTransport::popupEvent ( bool show ) {
    m_label->setMaximumSize(9999, 9999);

    Plasma::PopupApplet::popupEvent ( show );
}

void PublicTransport::createPopupIcon() {
    AlarmTimer *alarmTimer = getNextAlarm();
    if ( alarmTimer && alarmTimer->timer()->isActive() ) {
// 	QStandardItem *itemDeparture = m_model->item( alarmTimer->data().row(), 2 );
// 	int minutesToDeparture = itemDeparture->data( RemainingMinutesRole ).toInt();
// itemDeparture->data().toInt()  				m_alarmTime
	int minutesToAlarm = alarmTimer->timer()->interval() / 60000 - qCeil( (float)alarmTimer->startedAt().secsTo( QDateTime::currentDateTime() ) / 60.0f );
	int hoursToAlarm = minutesToAlarm / 60;
	minutesToAlarm %= 60;
	QString text = i18nc( "This is displayed on the popup icon to indicate the remaining time to the next alarm, %1=hours, %2=minutes with padded 0", "%1:%2", hoursToAlarm, QString("%1").arg(minutesToAlarm, 2, 10, QLatin1Char('0')) );

	QFont font = Plasma::Theme::defaultTheme()->font(Plasma::Theme::DefaultFont);

	QPixmap pixmapIcon128 = KIcon("public-transport-stop").pixmap(128);
	QPixmap pixmapAlarmIcon32 = KIcon("kalarm").pixmap(32);
	QPainter p128(&pixmapIcon128);
	font.setPixelSize( 40 );
	p128.setFont( font );
	QPixmap shadowedText = Plasma::PaintUtils::shadowText( text, font );
	QSize sizeText(p128.fontMetrics().width(text), p128.fontMetrics().lineSpacing());
	QRect rectText(QPoint(128 - 4 - sizeText.width(),128 - sizeText.height()), sizeText);
	QRect rectIcon(rectText.left() - 32 - 4, rectText.top() + (rectText.height() - 32) / 2, 32, 32);
	p128.drawPixmap( rectIcon, pixmapAlarmIcon32 );
	p128.drawPixmap( rectText, shadowedText );
	p128.end();

	QPixmap pixmapIcon48 = KIcon("public-transport-stop").pixmap(48);
	QPixmap pixmapAlarmIcon13 = KIcon("kalarm").pixmap(13);
	QPainter p48(&pixmapIcon48);
	font.setPixelSize( 18 );
	font.setBold( true );
	p48.setFont( font );
	shadowedText = Plasma::PaintUtils::shadowText( text, font );
	sizeText = QSize(p48.fontMetrics().width(text), p48.fontMetrics().lineSpacing());
	rectText = QRect(QPoint(48 - sizeText.width(),48 - sizeText.height()), sizeText);
	rectIcon = QRect(rectText.left() - 11 - 1, rectText.top() + (rectText.height() - 11) / 2, 13, 13);
	rectText.adjust(0, 2, 0, 2);
	p48.drawPixmap( rectIcon, pixmapAlarmIcon13 );
	p48.drawPixmap( rectText, shadowedText );
	p48.end();

	KIcon icon = KIcon();
	icon.addPixmap(pixmapIcon128, QIcon::Normal);
	icon.addPixmap(pixmapIcon48, QIcon::Normal);


	setPopupIcon( icon );
    } else
	setPopupIcon("public-transport-stop");
}

void PublicTransport::createTooltip() {
    DepartureInfo nextDeparture;
    Plasma::ToolTipContent data;
    data.setMainText( i18n("Public transport") );
    if (m_departureInfos.isEmpty())
	data.setSubText( i18n("View departure times for public transport") );
    else if ( (nextDeparture = getFirstNotFilteredDeparture()).isValid() ) {
	data.setSubText( i18nc("%4 is the translated duration text, e.g. in 3 minutes", "Next departure from '%1': line %2 (%3) %4", m_stop, nextDeparture.lineString, nextDeparture.target , nextDeparture.durationString() ) );
    }
    data.setImage( KIcon("public-transport-stop").pixmap(IconSize(KIconLoader::Desktop)) );
    Plasma::ToolTipManager::self()->setContent(this, data);
}

DepartureInfo PublicTransport::getFirstNotFilteredDeparture()
{
    for ( int i = 0; i < m_departureInfos.count(); ++i ) {
	DepartureInfo nextDeparture = m_departureInfos[i];
	if ( !filterOut(nextDeparture) )
	    return nextDeparture;
    }

    return DepartureInfo();
}

AlarmTimer* PublicTransport::getNextAlarm()
{
    for ( int row = 0; row < m_model->rowCount(); ++row ) {
	QStandardItem *itemDeparture = m_model->item( row, 2 );
	AlarmTimer *alarmTimer = (AlarmTimer*)itemDeparture->data( AlarmTimerRole ).value<void*>();
	if ( alarmTimer != NULL && alarmTimer->timer()->isActive() )
	    return alarmTimer;
    }

    return NULL;
}

void PublicTransport::configChanged() {
    m_settingsJustChanged = true;

    Plasma::DataEngine::Data data = dataEngine("publictransport")->query("ServiceProviders");
    foreach ( QString serviceProviderName, data.keys() )
    {
	QMap< QString, QVariant > serviceProviderData = data.value(serviceProviderName).toMap();
	if ( m_serviceProvider == serviceProviderData["id"].toInt() )
	{
	    m_useSeperateCityValue = serviceProviderData["useSeperateCityValue"].toBool();
	    break;
	}
    }

    // TODO: move to configAccepted() ?
    if ( m_dataType == Departures )
	m_model->setHorizontalHeaderLabels( QStringList()
	<< i18nc("A tramline or busline", "Line")
	<< i18nc("Target of a tramline or busline", "Target")
	<< i18nc("Time of departure of a tram or bus", "Departure") );
    else if ( m_dataType == Arrivals )
	m_model->setHorizontalHeaderLabels( QStringList()
	<< i18nc("A tramline or busline", "Line")
	<< i18nc("Origin of a tramline or busline", "Origin")
	<< i18nc("Time of arrival of a tram or bus", "Arrival") );

    if ( checkConfig() )
	reconnectSource();
}

bool PublicTransport::checkConfig() {
    if ( m_useSeperateCityValue && (m_city.isEmpty() || m_stop.isEmpty()) )
	this->setConfigurationRequired(true, i18n("Please set a city and a stop."));
    else if ( m_stop.isEmpty() )
	this->setConfigurationRequired(true, i18n("Please set a stop."));
    else if ( m_serviceProvider == -1 )
	this->setConfigurationRequired(true, i18n("Please select a service provider."));
    else if ( m_filterMinLine > m_filterMaxLine )
	this->setConfigurationRequired(true, i18n("The minimal shown line can't be bigger than the maximal shown line."));
    else
    {
	this->setConfigurationRequired(false);
	return true;
    }

    return false;
}

void PublicTransport::createModel() {
    m_model = new QStandardItemModel( 0, 3 );
    if ( m_dataType == Departures )
	m_model->setHorizontalHeaderLabels( QStringList()
	<< i18nc("A tramline or busline", "Line")
	<< i18nc("Target of a tramline or busline", "Target")
	<< i18nc("Time of departure of a tram or bus", "Departure") );
    else if ( m_dataType == Arrivals )
	m_model->setHorizontalHeaderLabels( QStringList()
	<< i18nc("A tramline or busline", "Line")
	<< i18nc("Origin of a tramline or busline", "Origin")
	<< i18nc("Time of arrival of a tram or bus", "Arrival") );
    m_model->setSortRole( SortRole );
    m_model->sort( 2, Qt::AscendingOrder );
}

QGraphicsWidget* PublicTransport::graphicsWidget() {
    if ( !m_graphicsWidget )
    {
	m_graphicsWidget = new QGraphicsWidget(this);
        m_graphicsWidget->setMinimumSize(225, 150);
        m_graphicsWidget->setPreferredSize(350, 200);

	m_label = new Plasma::Label();
	m_label->setAlignment(Qt::AlignTop);
	QLabel *label = (QLabel*)m_label->widget();
	label->setTextInteractionFlags( Qt::LinksAccessibleByMouse );
	label->setOpenExternalLinks( true );

	// Create treeview
	m_treeView = new Plasma::TreeView();
	m_treeView->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
	QTreeView *treeView = (QTreeView*)m_treeView->widget();
// 	treeView->setAlternatingRowColors( true );
	treeView->setAllColumnsShowFocus( false );
	treeView->setRootIsDecorated( false );
	treeView->setAnimated( true );
	treeView->setSortingEnabled( true );
	treeView->setWordWrap( true );
	treeView->setUniformRowHeights( false );
	treeView->setExpandsOnDoubleClick( false );
	treeView->setAllColumnsShowFocus( true );
	treeView->setFrameShape( QFrame::StyledPanel );
	treeView->setEditTriggers( QAbstractItemView::NoEditTriggers );
	treeView->setSelectionMode( QAbstractItemView::NoSelection );
	treeView->setSelectionBehavior( QAbstractItemView::SelectRows );
	treeView->header()->setCascadingSectionResizes( true );
	treeView->header()->setResizeMode( QHeaderView::Interactive );
	treeView->header()->setSortIndicator( 2, Qt::AscendingOrder );
	treeView->setContextMenuPolicy( Qt::CustomContextMenu );
	HtmlDelegate *htmlDelegate = new HtmlDelegate;
	treeView->setItemDelegate( htmlDelegate );
	connect( treeView, SIGNAL(customContextMenuRequested(const QPoint &)),
		this, SLOT(showDepartureContextMenu(const QPoint &)) );
	connect( treeView, SIGNAL(doubleClicked(QModelIndex)),
		 this, SLOT(doubleClickedDepartureItem(QModelIndex)) );

	// Get colors
	QColor textColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor );
	QColor baseColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::BackgroundColor );
// 	QColor altBaseColor = baseColor.darker();
// 	int green = altBaseColor.green() * 1.8;
// 	altBaseColor.setGreen( green > 255 ? 255 : green ); // tint green
	QColor buttonColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::BackgroundColor );
	baseColor.setAlpha(50);
// 	altBaseColor.setAlpha(50);
	buttonColor.setAlpha(150);
	m_colorSubItemLabels = textColor;
	m_colorSubItemLabels.setAlpha( 170 );

	// Set colors
	QPalette p = palette();
	p.setColor( QPalette::Base, baseColor );
// 	p.setColor( QPalette::AlternateBase, altBaseColor );
	p.setColor( QPalette::Button, buttonColor );
	p.setColor( QPalette::Foreground, textColor );
	p.setColor( QPalette::Text, textColor );
	treeView->setPalette(p);

	if ( !m_model )
	    createModel();
	m_model->horizontalHeaderItem(0)->setTextAlignment( Qt::AlignRight );
	m_model->horizontalHeaderItem(0)->setForeground( QBrush(textColor) );
	m_model->horizontalHeaderItem(1)->setForeground( QBrush(textColor) );
	m_model->horizontalHeaderItem(2)->setForeground( QBrush(textColor) );
	m_treeView->setModel( m_model );
// 	treeView->header()->setFirstColumnSpanned()
	treeView->header()->setStretchLastSection( false );
	treeView->header()->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	treeView->header()->resizeSection( 0, 50 );
	treeView->header()->setPalette( p );
	connect ( treeView->header(), SIGNAL(sectionResized(int,int,int)),
		  this, SLOT(treeViewSectionResized(int,int,int)) );

	QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical);
	layout->setContentsMargins( 0, 4, 0, 0 );
	layout->setSpacing( 0 );
	layout->addItem(m_label);
	layout->setAlignment(m_label, Qt::AlignTop);
	layout->addItem(m_treeView);
	layout->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
	m_graphicsWidget->setLayout(layout);

	registerAsDragHandle(m_graphicsWidget);
	registerAsDragHandle(m_label);
// 	registerAsDragHandle(m_treeView);
    }

    return m_graphicsWidget;
}

void PublicTransport::constraintsEvent(Plasma::Constraints constraints) {
    if (!m_graphicsWidget) {
        graphicsWidget();
    }

    if ((constraints|Plasma::FormFactorConstraint || constraints|Plasma::SizeConstraint) &&
        layout()->itemAt(0) != m_graphicsWidget) {
    }
}

void PublicTransport::paintInterface(QPainter *p,
        const QStyleOptionGraphicsItem *option, const QRect &contentsRect) {
    Q_UNUSED(p)
    Q_UNUSED(option)
    Q_UNUSED(contentsRect)

//     p->setRenderHint(QPainter::SmoothPixmapTransform);
//     p->setRenderHint(QPainter::Antialiasing);

// p->drawImage( QPoint(0,0), KIcon("kalarm").pixmap(16).toImage() );
    // Now we draw the applet, starting with our svg
//     m_svg.resize((int)contentsRect.width(), (int)contentsRect.height());
//     m_svg.paint(p, (int)contentsRect.left(), (int)contentsRect.top());
}

void PublicTransport::createConfigurationInterface ( KConfigDialog* parent ) {
    m_configDialog = parent;
    parent->setButtons( KDialog::Ok | KDialog::Apply | KDialog::Cancel );

    QWidget *widget = new QWidget;
    QWidget *widgetFilter = new QWidget;

    m_ui.setupUi(widget);
    m_uiFilter.setupUi(widgetFilter);
    m_isConfigDialogShown = true;

    connect( parent, SIGNAL(finished()), this, SLOT(configDialogFinished()));
    connect( parent, SIGNAL(applyClicked()), this, SLOT(configAccepted()) );
    connect( parent, SIGNAL(okClicked()), this, SLOT(configAccepted()) );

    parent->addPage(widget, i18n("General"), "public-transport-stop");
    parent->addPage(widgetFilter, i18n("Filter"), "view-filter");

    if (m_updateTimeout == 0) {
	m_ui.updateAutomatically->setChecked(false);
	m_ui.updateTimeout->setValue(60); // Set to default
    }
    else {
	m_ui.updateAutomatically->setChecked(true);
	m_ui.updateTimeout->setValue(m_updateTimeout);
    }
    m_ui.showRemainingMinutes->setChecked(m_showRemainingMinutes);
    m_ui.showDepartureTime->setChecked(m_showDepartureTime);
    m_ui.city->setText(m_city);
    m_ui.stop->setText(m_stop);
    m_ui.timeOfFirstDeparture->setValue(m_timeOffsetOfFirstDeparture);
    m_ui.maximalNumberOfDepartures->setValue(m_maximalNumberOfDepartures);
    m_ui.alarmTime->setValue(m_alarmTime);
    m_ui.linesPerRow->setValue(m_linesPerRow);
    m_ui.showDepartures->setChecked( m_dataType == Departures );
    m_ui.showArrivals->setChecked( m_dataType == Arrivals );
    m_ui.btnServiceProviderInfo->setIcon( KIcon("help-about") );
    m_ui.btnServiceProviderInfo->setText( "" );

    if ( m_modelServiceProvider != NULL )
	delete m_modelServiceProvider;
    m_modelServiceProvider = new QStandardItemModel( 0, 1 );
    m_ui.serviceProvider->setModel( m_modelServiceProvider );

    HtmlDelegate *htmlDelegate = new HtmlDelegate;
    htmlDelegate->setAlignText( true );
    m_ui.serviceProvider->setItemDelegate( htmlDelegate );

    Plasma::DataEngine::Data data = dataEngine("publictransport")->query("ServiceProviders");
    foreach ( QString serviceProviderName, data.keys() )
    {
	QMap< QString, QVariant > serviceProviderData = data.value(serviceProviderName).toMap();
	QString formattedText = QString( "<b>%1</b><br-wrap><small><b>Features:</b> %2</small>" ).arg( serviceProviderName ).arg( serviceProviderData["features"].toStringList().join(", ")/*.replace(QRegExp("([^,]*,[^,]*,[^,]*,)"), "\\1<br>")*/ );
	QStandardItem *item = new QStandardItem( serviceProviderName );
	item->setData( formattedText, HtmlDelegate::FormattedTextRole );
	item->setData( 4, HtmlDelegate::LinesPerRowRole );
	item->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	item->setData( serviceProviderData, ServiceProviderDataRole );
	QString sortString = serviceProviderName.contains( serviceProviderData["country"].toString(), Qt::CaseInsensitive )
	    // sort service providers containing the contry in it's name to the top of the list of service provider for that country
	    ? serviceProviderData["country"].toString() + "AAAAA" + serviceProviderName
	    : serviceProviderData["country"].toString() + serviceProviderName;
	item->setData( sortString, SortRole );
	m_modelServiceProvider->appendRow( item );

	// TODO: add favicon to data engine
	QString favIconSource = serviceProviderData["url"].toString();
	dataEngine("favicons")->disconnectSource( favIconSource, this );
	dataEngine("favicons")->connectSource( favIconSource, this );
	dataEngine("favicons")->query( favIconSource );
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

    QListWidget *available, *selected;
    available = m_uiFilter.filterLineType->availableListWidget();
    selected = m_uiFilter.filterLineType->selectedListWidget();

    available->addItems( QStringList() << i18n("Trams") << i18n("Buses") << i18n("Subways") << i18n("Interurban trains") << i18n("Regional trains") << i18n("Regional express trains") << i18n("Interregio trains") << i18n("Intercity / Eurocity trains") << i18n("Intercity express trains") );

    available->item( 0 )->setIcon( iconFromVehicleType(Tram) );
    available->item( 1 )->setIcon( iconFromVehicleType(Bus) );
    available->item( 2 )->setIcon( iconFromVehicleType(Subway) );
    available->item( 3 )->setIcon( iconFromVehicleType(TrainInterurban) );
    available->item( 4 )->setIcon( iconFromVehicleType(TrainRegional) );
    available->item( 5 )->setIcon( iconFromVehicleType(TrainRegionalExpress) );
    available->item( 6 )->setIcon( iconFromVehicleType(TrainInterregio) );
    available->item( 7 )->setIcon( iconFromVehicleType(TrainIntercityEurocity) );
    available->item( 8 )->setIcon( iconFromVehicleType(TrainIntercityExpress) );

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

    setStopNameValid( m_stopNameValid, "" );

    connect( m_ui.stop, SIGNAL(userTextChanged(QString)), this, SLOT(stopNameChanged(QString)) );
    connect( m_ui.btnServiceProviderInfo, SIGNAL(clicked(bool)), this, SLOT(clickedServiceProviderInfo(bool)));

    connect( selected, SIGNAL(currentRowChanged(int)), this, SLOT(filterLineTypeSelectedSelctionChanged(int)) );
    connect( available, SIGNAL(currentRowChanged(int)), this, SLOT(filterLineTypeAvaibleSelctionChanged(int)) );
    connect( m_uiFilter.filterLineType, SIGNAL(added(QListWidgetItem*)), this, SLOT(addedFilterLineType(QListWidgetItem*)) );
    connect( m_uiFilter.filterLineType, SIGNAL(removed(QListWidgetItem*)), this, SLOT(removedFilterLineType(QListWidgetItem*)) );
}

void PublicTransport::filterLineTypeAvaibleSelctionChanged ( int )
{
    m_uiFilter.filterLineType->setButtonsEnabled();
}

void PublicTransport::filterLineTypeSelectedSelctionChanged ( int )
{
    m_uiFilter.filterLineType->setButtonsEnabled();
}

void PublicTransport::addedFilterLineType ( QListWidgetItem* )
{
    m_uiFilter.filterLineType->setButtonsEnabled();
}

void PublicTransport::removedFilterLineType ( QListWidgetItem* )
{
    m_uiFilter.filterLineType->setButtonsEnabled();
}

void PublicTransport::configDialogFinished() {
    m_isConfigDialogShown = false;
    m_dataSourceTester->setTestSource("");
}

void PublicTransport::accessorInfoDialogFinished() {
    m_isAccessorInfoDialogShown = false;
}

void PublicTransport::clickedServiceProviderInfo ( bool ) {
    QWidget *widget = new QWidget;
    m_uiAccessorInfo.setupUi( widget );
    m_isAccessorInfoDialogShown = true;

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

void PublicTransport::serviceProviderChanged ( int index ) {
    QMap< QString, QVariant > serviceProviderData = m_modelServiceProvider->item( index )->data( ServiceProviderDataRole ).toMap();
    kDebug() << "New service provider" << serviceProviderData["name"].toString();

    m_stopIDinConfig = "";
    m_dataSourceTester->clearStopToStopIdMap();

    m_ui.kledStopValidated->setState( KLed::Off );
    m_ui.kledStopValidated->setToolTip( i18n("Checking validity of the stop name.") );

    // Only show "Departures"/"Arrivals"-radio buttons if arrivals are supported by the service provider
    bool supportsArrivals = serviceProviderData["features"].toStringList().contains("Arrivals");
    m_ui.lblTypeOfData->setVisible( supportsArrivals );
    m_ui.showDepartures->setVisible( supportsArrivals );
    m_ui.showArrivals->setVisible( supportsArrivals );
    if ( !supportsArrivals )
	m_ui.showDepartures->setChecked( true );

    bool useSeperateCityValue = serviceProviderData["useSeperateCityValue"].toBool();
    m_ui.lblCity->setVisible( useSeperateCityValue );
    m_ui.city->setVisible( useSeperateCityValue );

    if ( useSeperateCityValue )
    {
	QStringList cities = serviceProviderData["cities"].toStringList();
	if ( !cities.isEmpty() )
	{
	    m_ui.city->setText( cities.first() );
	    m_ui.city->setCompletedItems( cities, false );
	}
    }
    else
	m_ui.city->setText( "" );

    stopNameChanged( m_ui.stop->text() );
}

void PublicTransport::stopNameChanged ( QString stopName ) {
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
	m_dataSourceTester->setTestSource( QString("Departures %1:city%2:stop%3:maxDeps%4:timeOffset%5").arg( serviceProviderIndex ).arg( m_ui.city->text() ).arg( m_stopIDinConfig.isEmpty() ? stopName : m_stopIDinConfig ).arg( m_ui.maximalNumberOfDepartures->value() ).arg( m_ui.timeOfFirstDeparture->value() ) );
    } else
	m_dataSourceTester->setTestSource( QString("Departures %1:stop%2:maxDeps%3:timeOffset%4").arg( serviceProviderIndex ).arg(  m_stopIDinConfig.isEmpty() ? stopName : m_stopIDinConfig ).arg( m_ui.maximalNumberOfDepartures->value() ).arg( m_ui.timeOfFirstDeparture->value() ) );
}

void PublicTransport::configAccepted() {
    bool changed = false;

    if (!m_ui.updateAutomatically->isChecked() && m_updateTimeout != 0) {
        m_updateTimeout = 0;
        KConfigGroup cg = config();
        cg.writeEntry("updateTimeout", m_updateTimeout);
        changed = true;
    }
    else if (m_updateTimeout  != m_ui.updateTimeout->value()) {
        m_updateTimeout = m_ui.updateTimeout->value();
        KConfigGroup cg = config();
        cg.writeEntry("updateTimeout", m_updateTimeout);
        changed = true;
    }

    if (m_showRemainingMinutes != (m_ui.showRemainingMinutes->checkState() == Qt::Checked)) {
        m_showRemainingMinutes = !m_showRemainingMinutes;;
        KConfigGroup cg = config();
        cg.writeEntry("showRemainingMinutes", m_showRemainingMinutes);
        changed = true;
    }

    if (m_showDepartureTime != (m_ui.showDepartureTime->checkState() == Qt::Checked)) {
        m_showDepartureTime = !m_showDepartureTime;;
        KConfigGroup cg = config();
        cg.writeEntry("showDepartureTime", m_showDepartureTime);
        changed = true;
    }

    if (m_city  != m_ui.city->text()) {
        m_city = m_ui.city->text();
        KConfigGroup cg = config();
        cg.writeEntry("city", m_city);
	changed = true;

	clearDepartures(); // Clear departures using the old city name
    }

    if (m_stop  != m_ui.stop->text()) {
        m_stop = m_ui.stop->text();
	m_stopID = m_stopIDinConfig;
        KConfigGroup cg = config();
        cg.writeEntry("stop", m_stop);
        cg.writeEntry("stopID", m_stopID);
	changed = true;

	clearDepartures(); // Clear departures using the old stop name
    }

    if (m_timeOffsetOfFirstDeparture  != m_ui.timeOfFirstDeparture->value()) {
        m_timeOffsetOfFirstDeparture = m_ui.timeOfFirstDeparture->value();
        KConfigGroup cg = config();
        cg.writeEntry("timeOffsetOfFirstDeparture", m_timeOffsetOfFirstDeparture);
        changed = true;
    }

    if (m_maximalNumberOfDepartures  != m_ui.maximalNumberOfDepartures->value()) {
	m_maximalNumberOfDepartures = m_ui.maximalNumberOfDepartures->value();
	KConfigGroup cg = config();
	cg.writeEntry("maximalNumberOfDepartures", m_maximalNumberOfDepartures);
	changed = true;
    }

    if (m_alarmTime  != m_ui.alarmTime->value()) {
	m_alarmTime = m_ui.alarmTime->value();
	KConfigGroup cg = config();
	cg.writeEntry("alarmTime", m_alarmTime);
	changed = true;
    }

    if (m_linesPerRow  != m_ui.linesPerRow->value()) {
	m_linesPerRow = m_ui.linesPerRow->value();
	KConfigGroup cg = config();
	cg.writeEntry("linesPerRow", m_linesPerRow);
	changed = true;
    }

    if ((m_dataType == Departures && !m_ui.showDepartures->isChecked()) ||
	(m_dataType == Arrivals && !m_ui.showArrivals->isChecked())) {
	m_dataType = m_ui.showArrivals->isChecked() ? Arrivals : Departures;
	KConfigGroup cg = config();
	cg.writeEntry("dataType", static_cast<int>(m_dataType));
	changed = true;

	clearDepartures(); // Clear departures using the old data source type
    }

    m_ui.showDepartures->setChecked( m_dataType == Departures );

    QMap< QString, QVariant > serviceProviderData = m_modelServiceProvider->item( m_ui.serviceProvider->currentIndex() )->data( ServiceProviderDataRole ).toMap();
    const int serviceProviderIndex = serviceProviderData["id"].toInt();
    if (m_serviceProvider != serviceProviderIndex) {
        m_serviceProvider = serviceProviderIndex;
        KConfigGroup cg = config();
        cg.writeEntry("serviceProvider", m_serviceProvider);
        changed = true;

	clearDepartures(); // Clear departures from the old service provider
	m_useSeperateCityValue = serviceProviderData["useSeperateCityValue"].toBool();
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
        KConfigGroup cg = config();
	cg.writeEntry(vehicleTypeToConfigName(Tram), showTrams);
        changed = true;
    }

    if (m_showTypeOfVehicle[Bus] != showBuses) {
        m_showTypeOfVehicle[Bus] = showBuses;
        KConfigGroup cg = config();
	cg.writeEntry(vehicleTypeToConfigName(Bus), showBuses);
        changed = true;
    }

    if (m_showTypeOfVehicle[Subway] != showSubways) {
        m_showTypeOfVehicle[Subway] = showSubways;
        KConfigGroup cg = config();
	cg.writeEntry(vehicleTypeToConfigName(Subway), showSubways);
        changed = true;
    }

    if (m_showTypeOfVehicle[TrainInterurban] != showInterurbanTrains) {
        m_showTypeOfVehicle[TrainInterurban] = showInterurbanTrains;
        KConfigGroup cg = config();
	cg.writeEntry(vehicleTypeToConfigName(TrainInterurban), showInterurbanTrains);
        changed = true;
    }

    if (m_showTypeOfVehicle[TrainRegional] != showRegionalTrains) {
        m_showTypeOfVehicle[TrainRegional] = showRegionalTrains;
        KConfigGroup cg = config();
	cg.writeEntry(vehicleTypeToConfigName(TrainRegional), showRegionalTrains);
        changed = true;
    }

    if (m_showTypeOfVehicle[TrainRegionalExpress] != showRegionalExpressTrains) {
        m_showTypeOfVehicle[TrainRegionalExpress] = showRegionalExpressTrains;
        KConfigGroup cg = config();
	cg.writeEntry(vehicleTypeToConfigName(TrainRegionalExpress), showRegionalExpressTrains);
        changed = true;
    }

    if (m_showTypeOfVehicle[TrainInterregio] != showInterregioTrains) {
        m_showTypeOfVehicle[TrainInterregio] = showInterregioTrains;
        KConfigGroup cg = config();
	cg.writeEntry(vehicleTypeToConfigName(TrainInterregio), showInterregioTrains);
        changed = true;
    }

    if (m_showTypeOfVehicle[TrainIntercityEurocity] != showIntercityEurocityTrains) {
        m_showTypeOfVehicle[TrainIntercityEurocity] = showIntercityEurocityTrains;
        KConfigGroup cg = config();
	cg.writeEntry(vehicleTypeToConfigName(TrainIntercityEurocity), showIntercityEurocityTrains);
        changed = true;
    }

    if (m_showTypeOfVehicle[TrainIntercityExpress] != showIntercityExpressTrains) {
        m_showTypeOfVehicle[TrainIntercityExpress] = showIntercityExpressTrains;
        KConfigGroup cg = config();
	cg.writeEntry(vehicleTypeToConfigName(TrainIntercityExpress), showIntercityExpressTrains);
        changed = true;
    }

    if (m_showNightlines != (m_uiFilter.showNightLines->checkState() == Qt::Checked)) {
        m_showNightlines = !m_showNightlines;
        KConfigGroup cg = config();
        cg.writeEntry("showNightlines", m_showNightlines);
        changed = true;
    }

    if (m_filterMinLine  != m_uiFilter.filterMinLine->value()) {
        m_filterMinLine = m_uiFilter.filterMinLine->value();
        KConfigGroup cg = config();
        cg.writeEntry("filterMinLine", m_filterMinLine);
        changed = true;
    }

    if (m_filterMaxLine  != m_uiFilter.filterMaxLine->value()) {
        m_filterMaxLine = m_uiFilter.filterMaxLine->value();
        KConfigGroup cg = config();
        cg.writeEntry("filterMaxLine", m_filterMaxLine);
        changed = true;
    }

    if (m_filterTypeTarget  != (m_uiFilter.filterTypeTarget->currentIndex())) {
        m_filterTypeTarget = static_cast<FilterType>(m_uiFilter.filterTypeTarget->currentIndex());
        KConfigGroup cg = config();
	cg.writeEntry("filterTypeTarget", static_cast<int>(m_filterTypeTarget));
        changed = true;
    }

    if (m_filterTargetList  != m_uiFilter.filterTargetList->items()) {
        m_filterTargetList = m_uiFilter.filterTargetList->items();
	m_filterTargetList.removeDuplicates();
        KConfigGroup cg = config();
	cg.writeEntry("filterTargetList", m_filterTargetList);
        changed = true;
    }

    if (changed) {
        emit settingsChanged();
        emit configNeedsSaving();
    }
}

void PublicTransport::toggleExpanded( bool ) {
    doubleClickedDepartureItem( m_clickedItemIndex );
}

void PublicTransport::doubleClickedDepartureItem( QModelIndex modelIndex ) {
    if( modelIndex.parent().isValid() )
	return; // Only expand top level items

    QTreeView* treeView = (QTreeView*)m_treeView->widget();
    modelIndex = m_model->index( modelIndex.row(), 0 );
    if ( treeView->isExpanded(modelIndex) )
	treeView->collapse( modelIndex );
    else {
	if ( treeView->columnWidth(0) < 70 )
	    treeView->setColumnWidth(0, 70);
	treeView->expand( modelIndex );
    }
}

QAction* PublicTransport::updatedAction ( const QString& actionName )
{
    QAction *a = action(actionName);
    if ( a == NULL ) {
	if ( actionName == "seperator" ) {
	    a = new QAction(this);
	    a->setSeparator(true);
	    return a;
	} else
	    return NULL;
    }

    if ( actionName == "toggleExpanded" ) {
	if ( ((QTreeView*)m_treeView->widget())->isExpanded( m_model->index(m_clickedItemIndex.row(), 0) ) ) {
	    a->setText( i18n("Hide additional &information") );
	    a->setIcon( KIcon("arrow-up") );
	} else {
	    a->setText( i18n("Show additional &information") );
	    a->setIcon( KIcon("arrow-down") );
	}
    } else if ( actionName == "removeAlarmForDeparture" ) {
	a->setText( m_dataType == Departures ? i18n("Remove &alarm for this departure") : i18n("Remove &alarm for this arrival") );
    } else if ( actionName == "setAlarmForDeparture" ) {
	a->setText( m_dataType == Departures ? i18n("Set &alarm for this departure") : i18n("Set &alarm for this arrival") );
    } else if ( actionName == "filterOutByVehicleType" ) {
	QStandardItem *itemDeparture = m_model->item( m_clickedItemIndex.row(), 2 );
	VehicleType vehicleType = static_cast<VehicleType>( itemDeparture->data( VehicleTypeRole ).toInt() );
	if ( vehicleType == Unknown ) {
	    a->setIcon( KIcon("view-filter") );
	    a->setText( i18n("&Filter out by vehicle type") );
	} else {
	    QString sVehicleType = vehicleTypeToString( vehicleType );
	    a->setIcon( makeOverlayIcon(KIcon("view-filter"), iconFromVehicleType(vehicleType)) );
	    a->setText( i18n("&Filter out %1", vehicleTypeToString(vehicleType, true)) );
	}
    } else if ( actionName == "removeTargetFromFilterList" ) {
	if ( m_filterTypeTarget == ShowMatching )
	    a->setText( m_dataType == Departures ? i18n("&Hide target") : i18n("&Hide origin") );
	else if ( m_filterTypeTarget == ShowAll )
	    a->setText( m_dataType == Departures ? i18n("&Remove target from filter list") : i18n("&Remove origin from filter list") );
    } else if ( actionName == "setFilterListToHideMatching") {
	if ( m_filterTargetList.isEmpty() )
	    a->setText( m_dataType == Departures ? i18n("&Hide target") : i18n("&Hide origin") );
	else
	    a->setText( m_dataType == Departures ? i18n("&Hide target and others in the filter list") : i18n("&Hide origin and others in the filter list" ) );
    } else if ( actionName == "removeTargetFromFilterList") {
	a->setText( m_dataType == Departures ? i18n("&Remove target from filter list") : i18n("&Remove origin from filter list") );
    } else if ( actionName == "addTargetToFilterList") {
	a->setText( m_dataType == Departures ? i18n("&Hide target") : i18n("&Hide origin") );
    } else if ( actionName == "addTargetToFilterListAndHide") {
	if ( m_filterTargetList.isEmpty() )
	    a->setText( m_dataType == Departures ? i18n("&Hide target") : i18n("&Hide origin") );
	else
	    a->setText( m_dataType == Departures ? i18n("&Hide target and others in the filter list") : i18n("&Hide origin and others in the filter list") );
    }

    return a;
}

void PublicTransport::showDepartureContextMenu ( const QPoint& position ) {
    QTreeView* treeView = (QTreeView*)m_treeView->widget();
    QList<QAction *> actions;
    if ( (m_clickedItemIndex = treeView->indexAt(position)).isValid() ) {
	while( m_clickedItemIndex.parent().isValid() )
	    m_clickedItemIndex = m_clickedItemIndex.parent();

	actions.append( updatedAction("toggleExpanded") );
	if ( m_model->item( m_clickedItemIndex.row(), 2 )->icon().isNull() )
	    actions.append( updatedAction("setAlarmForDeparture") );
	else
	    actions.append( updatedAction("removeAlarmForDeparture") );

	actions.append( updatedAction("seperator") );

	actions.append( updatedAction("filterOutByVehicleType") );
	if ( !filteredOutVehicleTypes().isEmpty() ) {
	    actions.append( action("removeAllFiltersByVehicleType") );
	    actions.append( updatedAction("seperator") );
	}

	QString sTarget = m_model->item( m_clickedItemIndex.row(), 1 )->text();
	if ( m_filterTargetList.contains( sTarget ) ) {
	    if ( m_filterTypeTarget == ShowMatching ) {
		actions.append( updatedAction("removeTargetFromFilterList") );
		actions.append( action("setFilterListToShowAll") );
	    } else if ( m_filterTypeTarget == ShowAll ) {
		// Adding an already existing item, which is then removed because it's a duplicate
		// Could add another QAction to only set to HideMatching
		actions.append( updatedAction("setFilterListToHideMatching") );
		actions.append( updatedAction("removeTargetFromFilterList") );
	    }// never m_filterTypeTarget == HideMatching => journeys with target/origin in filter list won't be shown
	} else { // Target isn't contained in the filter list
	    if ( m_filterTypeTarget == HideMatching ) {
		actions.append( updatedAction("addTargetToFilterList") );
		actions.append( action("setFilterListToShowAll") );
	    } else if ( m_filterTypeTarget == ShowAll ) {
		actions.append( updatedAction("addTargetToFilterListAndHide") );
	    } // never m_filterTypeTarget == ShowMatching => journeys with target/origin not in filter list won't be shown
	}
    } else { // No item
	if ( !filteredOutVehicleTypes().isEmpty() )
	    actions.append( action("removeAllFiltersByVehicleType") );
	if ( m_filterTypeTarget != ShowAll )
	    actions.append( action("setFilterListToShowAll") );
    }

    if ( actions.count() > 0 && view() )
	QMenu::exec( actions, QCursor::pos() );
}

void PublicTransport::filterOutByVehicleType( bool ) {
    QStandardItem *itemDeparture = m_model->item( m_clickedItemIndex.row(), 2 );
    VehicleType vehicleType = static_cast<VehicleType>( itemDeparture->data( VehicleTypeRole ).toInt() );
    m_showTypeOfVehicle[vehicleType] = false;

    KConfigGroup cg = config();
    cg.writeEntry(vehicleTypeToConfigName(vehicleType), false);
    //     emit settingsChanged();
    emit configNeedsSaving();
    updateModel(); // apply new filter settings
}

void PublicTransport::removeAllFiltersByVehicleType ( bool )
{
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

    KConfigGroup cg = config();
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
    updateModel(); // apply new filter settings
}

void PublicTransport::addTargetToFilterList( bool ) {
    QString target = m_model->item( m_clickedItemIndex.row(), 1 )->text();
    m_filterTargetList << target;
//     if ( m_isConfigDialogShown )
// 	m_uiFilter.filterTargetList->items() << target;

    KConfigGroup cg = config();
    cg.writeEntry("filterTargetList", m_filterTargetList
);
//     emit settingsChanged();
    emit configNeedsSaving();
    updateModel(); // apply new filter settings
}

void PublicTransport::removeTargetFromFilterList( bool ) {
    QString target = m_model->item( m_clickedItemIndex.row(), 1 )->text();
    m_filterTargetList.removeOne( target );
//     if ( m_isConfigDialogShown )
// 	m_uiFilter.filterTargetList->items().removeOne( target );

    KConfigGroup cg = config();
    cg.writeEntry("filterTargetList", m_filterTargetList);
    cg.writeEntry("filterTypeTarget", static_cast<int>(m_filterTypeTarget));
    //     emit settingsChanged();
    emit configNeedsSaving();
    updateModel(); // apply new filter settings
}

void PublicTransport::setFilterListToShowAll( bool ) {
    m_filterTypeTarget = ShowAll;
    KConfigGroup cg = config();
    cg.writeEntry("filterTypeTarget", static_cast<int>(m_filterTypeTarget));
    //     emit settingsChanged();
    emit configNeedsSaving();
    updateModel(); // apply new filter settings

    if ( m_isConfigDialogShown )
	m_uiFilter.filterTypeTarget->setCurrentIndex( static_cast<int>(m_filterTypeTarget
) );
}

void PublicTransport::setFilterListToHideMatching ( bool ) {
    m_filterTypeTarget = HideMatching;
    KConfigGroup cg = config();
    cg.writeEntry("filterTypeTarget", static_cast<int>(m_filterTypeTarget));
    //     emit settingsChanged();
    emit configNeedsSaving();
    updateModel(); // apply new filter settings

    if ( m_isConfigDialogShown )
	m_uiFilter.filterTypeTarget->setCurrentIndex( static_cast<int>(m_filterTypeTarget) );
}

void PublicTransport::addTargetToFilterListAndHide
 ( bool b ) {
    m_filterTypeTarget
 = HideMatching;
    if ( m_isConfigDialogShown )
	m_uiFilter.filterTypeTarget->setCurrentIndex( static_cast<int>(m_filterTypeTarget
) );

    addTargetToFilterList
( b );
}

void PublicTransport::markAlarmRow ( const QPersistentModelIndex& modelIndex, AlarmState alarmState ) {
    if( !modelIndex.isValid() ) {
	qDebug() << "PublicTransport::markAlarmRow" << "!index.isValid(), row =" << modelIndex.row();
	return;
    }

    QStandardItem *itemDeparture = m_model->item( modelIndex.row(), 2 );
    if ( !itemDeparture )
	return;
    if ( alarmState == AlarmPending ) {
	// Make background ground color light red and store original background color
	QColor color( 255, 200, 200, 180 );
	itemDeparture->setIcon( KIcon("kalarm") );
	itemDeparture->setData( static_cast<int>(HtmlDelegate::Right), HtmlDelegate::DecorationPositionRole );
	itemDeparture->setData( itemDeparture->background(), OriginalBackgroundColorRole );
	itemDeparture->setBackground( QBrush(color) );
	m_model->item( modelIndex.row(), 0 )->setBackground( QBrush(color) );
	m_model->item( modelIndex.row(), 1 )->setBackground( QBrush(color) );
    } else if (alarmState == NoAlarm) {
	// Set background color back to original
	QBrush brush =  itemDeparture->data(OriginalBackgroundColorRole).value<QBrush>();
	itemDeparture->setBackground( brush );
	itemDeparture->setIcon( KIcon() );
	m_model->item( modelIndex.row(), 0 )->setBackground( brush );
	m_model->item( modelIndex.row(), 1 )->setBackground( brush );
    } else if (alarmState == AlarmFired) {
	// Set background color back to original
	QBrush brush =  itemDeparture->data(OriginalBackgroundColorRole).value<QBrush>();
	itemDeparture->setBackground( brush );
	KIconEffect iconEffect;
	QPixmap pixmap =iconEffect.apply( KIcon("kalarm").pixmap(16), KIconLoader::Small, KIconLoader::DisabledState);
	KIcon disabledAlarmIcon;
	disabledAlarmIcon.addPixmap(pixmap, QIcon::Normal);
	itemDeparture->setIcon( disabledAlarmIcon );
	itemDeparture->setData( static_cast<int>(HtmlDelegate::Right), HtmlDelegate::DecorationPositionRole );
	m_model->item( modelIndex.row(), 0 )->setBackground( brush );
	m_model->item( modelIndex.row(), 1 )->setBackground( brush );
    }
}

void PublicTransport::removeAlarmForDeparture ( bool ) {
    QStandardItem *itemDeparture = m_model->item( m_clickedItemIndex.row(), 2 );
    AlarmTimer *alarmTimer = (AlarmTimer*)itemDeparture->data( AlarmTimerRole ).value<void*>();
    if ( alarmTimer ) {
	itemDeparture->setData( 0, AlarmTimerRole );
	alarmTimer->timer()->stop();
	delete alarmTimer;
	markAlarmRow( m_clickedItemIndex, NoAlarm );
    }

    createPopupIcon();
}

void PublicTransport::setAlarmForDeparture( const QPersistentModelIndex &modelIndex, AlarmTimer *alarmTimer ) {
    if( !modelIndex.isValid() ) {
	qDebug() << "PublicTransport::setAlarmForDeparture" << "!modelIndex.isValid(), row =" << modelIndex.row();
	return;
    }

    QStandardItem *itemDeparture = m_model->item( modelIndex.row(), 2 );
    markAlarmRow( modelIndex, AlarmPending );

    if ( alarmTimer == NULL ) {
	QDateTime predictedDeparture = itemDeparture->data( SortRole ).toDateTime();
	int secsTo = QDateTime::currentDateTime().secsTo( predictedDeparture.addSecs(-m_alarmTime * 60) );
	if ( secsTo < 0 )
	    secsTo = 0;
	alarmTimer = new AlarmTimer( secsTo * 1000, modelIndex );
    }
    itemDeparture->setData( qVariantFromValue((void*)alarmTimer), AlarmTimerRole );
    connect( alarmTimer, SIGNAL(timeout(const QPersistentModelIndex &)), this, SLOT(showAlarmMessage(const QPersistentModelIndex &)) );

    createPopupIcon();
}

void PublicTransport::setAlarmForDeparture ( bool ) {
//     QString sLine = m_model->item( m_clickedItemIndex.row(), 0 )->text();
//     QString sTarget = m_model->item( m_clickedItemIndex.row(), 1 )->text();

//     if ( m_model->item( m_clickedItemIndex.row(), 2 ) != NULL &&
// 	 !m_model->item( m_clickedItemIndex.row(), 2 )->icon().isNull() ) {
// 	qDebug() << "PublicTransport::setAlarmForDeparture" << "icon = null";
// 	setAlarmForDeparture( m_clickedItemIndex, false );
// // 	markAlarmRow( m_clickedItemIndex, false );
// 	return;
//     }

    setAlarmForDeparture( m_model->index(m_clickedItemIndex.row(), 2) );
/*
    QDateTime predictedDeparture = m_model->item( m_clickedItemIndex.row(), 2 )->data( SortRole ).toDateTime();
    int alarmTimeBeforeDeparture = 5; // in minutes
    int secsTo = QDateTime::currentDateTime().secsTo( predictedDeparture.addSecs(-alarmTimeBeforeDeparture * 60) );
    if ( secsTo < 0 )
	secsTo = 0;

    m_model->item( m_clickedItemIndex.row(), 2 )->setIcon( KIcon("kalarm") );

    AlarmTimer *alarmTimer = new AlarmTimer( secsTo * 1000, m_clickedItemIndex );
//     QTimer *alarmTimer = new QTimer(this);
    m_model->item( m_clickedItemIndex.row(), 2 )->setData( qVariantFromValue((void*)alarmTimer), AlarmTimerRole );
    //m_alarmTimerItemIndices.insert( alarmTimer, m_clickedItemIndex );
    connect( alarmTimer, SIGNAL(timeout(QTimer*)), this, SLOT(showAlarmMessage(QTimer*)) );*/
//     alarmTimer->setSingleShot( true );
//     alarmTimer->start( secsTo * 1000 );
//     QTimer::singleShot( secsTo * 1000, this, SLOT(showAlarmMessage()) );

//     QDBusInterface remoteApp( "org.kde.kalarm", "/kalarm", "org.kde.kalarm.kalarm" );
//     if ( !remoteApp.isValid() ) {
// 	qDebug() << "Couldn't get dbus interface for kalarm";
// 	return;
//     }

//     QString message = i18n("PublicTransport: Line %1 to %2 departs", sLine, sTarget);
//     dbus signature:
// 		scheduleMessage(QString message, QString startDateTime, int lateCancel, uint flags, QString bgColor, QString fgColor, QString font, QString audioFile, int reminderMins, QString recurrence, int subRepeatInterval, int subRepeatCount)
//     QDBusReply<bool> reply = remoteApp.callWithArgumentList( QDBus::Block, "scheduleMessage", QList<QVariant>() << message << predictedDeparture.toString(Qt::ISODate) << 0 << (uint)0 << "null" << "null" << "null" << "null" /*audiofile*/ << 5 /*reminderMins*/ << "" << 0 << 0 );

//     qDebug() << "qdbus org.kde.kalarm /kalarm org.kde.kalarm.kalarm.scheduleMessage" << message << predictedDeparture.toString(Qt::ISODate) << 0 << (uint)0 << "\"null\"" << "\"null\"" << "\"null\"" << "\"null\"" /*audiofile*/ << 5 /*reminderMins*/ << "\"\"" << 0 << 0 ;
//     qDebug() << "Set alarm for" << sLine << sTarget << "reply =" << reply.value() << "time arg=" << predictedDeparture.toString("hh:mm") << "reply errormsg" << reply.error().message() << "name" << reply.error().name();
}

void PublicTransport::showAlarmMessage( const QPersistentModelIndex &modelIndex ) {
    //     qDebug() << "PublicTransport::showAlarmMessage";
    if ( !modelIndex.isValid() ) {
	qDebug() << "PublicTransport::showAlarmMessage" << "!modelIndex.isValid(), row =" << modelIndex.row();
	return;
    }

    QModelIndex topLevelIndex = modelIndex;
    while( topLevelIndex.parent().isValid() )
	topLevelIndex = topLevelIndex.parent();
    markAlarmRow( topLevelIndex, AlarmFired );

    int row = topLevelIndex.row();
    QString sLine = m_model->item( row, 0 )->text();
    QString sTarget = m_model->item( row, 1 )->text();
    QDateTime predictedDeparture = m_model->item( row, 2 )->data( SortRole ).toDateTime();
    int minsToDeparture = qCeil((float)QDateTime::currentDateTime().secsTo( predictedDeparture ) / 60.0f);
    VehicleType vehicleType = static_cast<VehicleType>( m_model->item( row, 2 )->data( VehicleTypeRole ).toInt() );
//     qDebug() << vehicleType << m_model->item( row, 2 )->data( VehicleTypeRole ).toInt();
    QString message;
    if ( minsToDeparture > 0 ) {
	if ( vehicleType == Unknown )
	    message = i18np("Line %2 to '%3' departs in %1 minute at %4", "Line %2 to '%3' departs in %1 minutes at %4", minsToDeparture, sLine, sTarget, predictedDeparture.toString("hh:mm") );
	else
	    message = i18ncp("%2: Line string (e.g. 'U3'), %4: Vehicle type name (e.g. tram, subway)", "The %4 %2 to '%3' departs in %1 minute at %5", "The %4 %2 to '%3' departs in %1 minutes at %5", minsToDeparture, sLine, sTarget, vehicleTypeToString(vehicleType), predictedDeparture.toString("hh:mm"));
    }
    else if ( minsToDeparture < 0 ) {
	if ( vehicleType == Unknown )
	    message = i18np("Line %2 to '%3' has departed %1 minute ago at %4", "Line %2 to '%3' has departed %1 minutes ago at %4", -minsToDeparture, sLine, sTarget, predictedDeparture.toString("hh:mm"));
	else
	    message = i18ncp("%2: Line string (e.g. 'U3'), %4: Vehicle type name (e.g. tram, subway)", "The %4 %2 to '%3' has departed %1 minute ago at %5", "The %4 %2 to %3 has departed %1 minutes ago at %5", -minsToDeparture, sLine, sTarget, vehicleTypeToString(vehicleType), predictedDeparture.toString("hh:mm"));
    }
    else {
	if ( vehicleType == Unknown )
	    message = i18n("Line %1 to '%2' departs now at %3", sLine, sTarget, predictedDeparture.toString("hh:mm"));
	else
	    message = i18nc("%2: Line string (e.g. 'U3'), %4: Vehicle type name (e.g. tram, subway)", "The %3 %1 to '%2' departs now at %3", sLine, sTarget, vehicleTypeToString(vehicleType), predictedDeparture.toString("hh:mm"));
    }

    KNotification::event( KNotification::Warning, message, KIcon("public-transport-stop").pixmap(16), 0L, KNotification::Persistent );

//     QMessageBox box( QMessageBox::Information, "Public transport: Alarm", message, QMessageBox::Ok );
//     box.setWindowIcon( KIcon("public-transport-stop") );
//     box.exec();
}

QList< VehicleType > PublicTransport::filteredOutVehicleTypes() const {
    return m_showTypeOfVehicle.keys( false );
}

bool PublicTransport::filterOut ( const DepartureInfo &departureInfo ) const {
    return (departureInfo.vehicleType != Unknown && !m_showTypeOfVehicle[ departureInfo.vehicleType ]) ||
	(departureInfo.isNightLine() && !m_showNightlines) ||
	(departureInfo.isLineNumberValid() && !departureInfo.isLineNumberInRange( m_filterMinLine, m_filterMaxLine )) ||
	(m_filterTypeTarget == ShowMatching && !m_filterTargetList.contains(departureInfo.target)) ||
	(m_filterTypeTarget == HideMatching && m_filterTargetList.contains(departureInfo.target)) ||
	QDateTime::currentDateTime().secsTo( departureInfo.predictedDeparture() ) < -60;
}

QMap<QString, QVariant> PublicTransport::serviceProviderData() const {
    QString sServiceProvider;
    Plasma::DataEngine::Data data = dataEngine("publictransport")->query("ServiceProviders");
    foreach ( QString serviceProviderName, data.keys() )
    {
	QMap< QString, QVariant > serviceProviderData = data.value(serviceProviderName).toMap();
	if ( serviceProviderData["id"].toInt() == m_serviceProvider )
	    return serviceProviderData;
    }

    kDebug() << "Name not found for" << m_serviceProvider;
    return QMap<QString,QVariant>();
}

QString PublicTransport::titleText( const QString &sServiceProvider ) const {
    if ( m_useSeperateCityValue )
	return QString("<html><body><center><big><b>%1, %4</b></big></center> <center><small>(last update: %2, data by <a href='http://www.%3'>%3</a>)</small></center></body></html>").arg( m_stop ).arg( m_lastSourceUpdate.toString("hh:mm") ).arg( sServiceProvider ).arg( m_city );
    else
	return QString("<html><body><center><big><b>%1</b></big></center> <center><small>(last update: %2, data by <a href='http://www.%3'>%3</a>)</small></center></body></html>").arg( m_stop ).arg( m_lastSourceUpdate.toString("hh:mm") ).arg( sServiceProvider );
}

QString PublicTransport::departureText( const DepartureInfo &departureInfo ) const {
    QString sTime, sDeparture = departureInfo.departure.toString("hh:mm");
    QString sColor = departureInfo.delayType() == OnSchedule ? "color:darkgreen;" : "";
    sDeparture = sDeparture.prepend(QString("<span style='font-weight:bold;%1'>").arg(sColor)).append("</span>");
    if (m_showDepartureTime && m_showRemainingMinutes) {
	QString sText = departureInfo.durationString();
	sText = sText.replace(QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");

	if ( m_linesPerRow > 1 )
	    sTime = QString("%1<br>(%2)").arg( sDeparture ).arg( sText );
	else
	    sTime = QString("%1 (%2)").arg( sDeparture ).arg( sText );
    } else if (m_showDepartureTime) {
	sTime = sDeparture;
	if ( departureInfo.delayType() == Delayed ) {
	    QString sText = i18np("+ %1 minute", "+ %1 minutes", departureInfo.delay );
	    sText = sText.prepend(" (").append(")");
	    sText = sText.replace(QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");
	    sTime += sText;
	}
    } else if (m_showRemainingMinutes) {
	sTime = departureInfo.durationString();
	sTime = sTime.replace(QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");
    } else
	sTime = "";

    return sTime;
}

QStandardItem* PublicTransport::createStandardItem ( const QString& text, Qt::Alignment textAlignment, QVariant sortData ) {
    QStandardItem *item = new QStandardItem( text );
    if ( sortData.isValid() )
	item->setData( sortData, Qt::UserRole );
    item->setTextAlignment( textAlignment );
    return item;
}

QStandardItem* PublicTransport::createStandardItem ( const QString& text, QColor textColor, Qt::Alignment textAlignment, QVariant sortData ) {
    QStandardItem *item = createStandardItem( text, textAlignment, sortData );
//     item->setForeground( QBrush(textColor) );
    setTextColorOfHtmlItem( item, textColor );
    return item;
}

void PublicTransport::setTextColorOfHtmlItem ( QStandardItem *item, QColor textColor )
{
    item->setText( item->text().prepend("<span style='color:rgba(%1,%2,%3,%4);'>").arg( textColor.red() ).arg( textColor.green() ).arg( textColor.blue() ).arg( textColor.alpha() ).append("</span>") );
}

int PublicTransport::findDeparture ( const DepartureInfo& departureInfo ) const {
//     qDebug() << "PublicTransport::findDeparture" << m_model->rowCount() << "rows" << departureInfo.lineString << departureInfo.target << departureInfo.departure.toString("hh:mm");
    for ( int row = 0; row < m_model->rowCount(); ++row )
    {
	QString line = m_model->item( row, 0 )->data( SortRole ).toString();
	if ( line != departureInfo.lineString ) {
// 	    qDebug() << "Line Test failed" << line << departureInfo.lineString;
	    continue;
	}

	QString target = m_model->item( row, 1 )->data( SortRole ).toString();
	if ( target != departureInfo.target ) {
// 	    qDebug() << "Target Test failed" << target << departureInfo.target;
	    continue;
	}

	QDateTime time = m_model->item( row, 2 )->data( SortRole ).toDateTime();
	if ( time != departureInfo.predictedDeparture() ) {
// 	    qDebug() << "Time Test failed" << time.toString("hh:mm") << departureInfo.departure.toString("hh:mm");
	    continue;
	}

// 	qDebug() << "Found" << row << departureInfo.lineString << departureInfo.target << departureInfo.departure.toString("hh:mm");;
	return row;
    }

    if ( m_model->rowCount() > 0 )
	qDebug() << "Not found" << departureInfo.lineString << departureInfo.target << departureInfo.departure.toString("hh:mm");;
    return -1; // Departure not found
}

void PublicTransport::getDelayItems ( const DepartureInfo& departureInfo, QStandardItem **itemDelayInfo, QStandardItem **itemDelay ) {
    QString sText;
    switch ( departureInfo.delayType() )
    {
	case OnSchedule:
	    sText = i18nc("A public transport vehicle departs on schedule", "On schedule");
	    sText = sText.prepend("<span style='color:green;'>").append("</span>");
	    if ( *itemDelayInfo != NULL ) {
		*itemDelayInfo = new QStandardItem(sText);
		*itemDelay = new QStandardItem("");
	    } else {
		(*itemDelayInfo)->setText(sText);
		(*itemDelay)->setText("");
	    }
	    break;
	case Delayed:
	    sText = i18np("+%1 minute", "+%1 minutes", departureInfo.delay);
	    sText = sText.replace(QRegExp("(+?\\s*\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");
	    if ( *itemDelayInfo != NULL ) {
		*itemDelayInfo = new QStandardItem(departureInfo.delayReason);
		*itemDelay = new QStandardItem(sText);
	    } else {
		(*itemDelayInfo)->setText(departureInfo.delayReason);
		(*itemDelay)->setText(sText);
	    }
	    if ( !departureInfo.delayReason.isEmpty() ) {
		(*itemDelayInfo)->setData( departureInfo.delayReason.length() / 25 + (departureInfo.delayReason.length() % 25 > 0 ? 1 : 0), HtmlDelegate::LinesPerRowRole ); // 25 chars per line
	    }
	    break;

	case DelayUnknown:
	default:
	    if ( *itemDelayInfo != NULL ) {
		*itemDelayInfo = new QStandardItem(i18n("No information available"));
		*itemDelay = new QStandardItem("");
	    } else {
		(*itemDelayInfo)->setText(i18n("No information available"));
		(*itemDelay)->setText("");
	    }
	    break;
    }

    setTextColorOfHtmlItem( *itemDelayInfo, m_colorSubItemLabels );
    setTextColorOfHtmlItem( *itemDelay, m_colorSubItemLabels );
}

void PublicTransport::setValuesOfDepartureItem ( QStandardItem* departureItem, DepartureInfo departureInfo, ItemInformation
 departureInformation, bool update ) {
    switch ( departureInformation ) {
	case LineNameItem:
	    departureItem->setText( QString("<span style='font-weight:bold;'>%1</span>").arg(departureInfo.lineString) );
	    departureItem->setData( departureInfo.lineString, SortRole );
// 	    departureItem->setData( QVariant::fromValue<DepartureInfo>(departureInfo), DepartureInfoRole );
	    if ( departureInfo.vehicleType != Unknown )
		departureItem->setIcon( iconFromVehicleType(departureInfo.vehicleType) );
	    if ( !update ) {
		departureItem->setTextAlignment( Qt::AlignRight );
		departureItem->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case TargetItem:
	    departureItem->setText( departureInfo.target );
	    departureItem->setData( departureInfo.target, SortRole );
	    departureItem->setData( m_linesPerRow, HtmlDelegate::LinesPerRowRole );
	    if ( !departureInfo.journeyNews.isEmpty() ) {
		departureItem->setIcon( makeOverlayIcon(KIcon("view-pim-news"), "arrow-down", QSize(12,12)) );
		departureItem->setData( static_cast<int>(HtmlDelegate::Right), HtmlDelegate::DecorationPositionRole );
	    }
	    if ( !update ) {
		departureItem->setTextAlignment( Qt::AlignLeft );
		departureItem->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case DepartureItem:
	    departureItem->setText( departureText(departureInfo) );
	    departureItem->setData( departureInfo.predictedDeparture(), SortRole ); // TODO: Could make findDeparture not working, when the delay has changed. Maybe change to departure with seperate TimeRole..
	    departureItem->setData( qCeil((float)QDateTime::currentDateTime().secsTo( departureInfo.predictedDeparture() ) / 60.0f), RemainingMinutesRole );
	    departureItem->setData( static_cast<int>(departureInfo.vehicleType), VehicleTypeRole );
	    if ( !update ) {
		departureItem->setTextAlignment( Qt::AlignLeft );
		departureItem->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case PlatformLabelItem:
	    if ( !update ) {
		departureItem->setText( i18nc("The platform from which a tram/bus/train departs", "Platform:") );
		departureItem->setTextAlignment( Qt::AlignRight );
		departureItem->setData( 1, SortRole );
		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;

	case PlatformItem:
	    departureItem->setText( departureInfo.platform );
	    if ( !update ) {
		departureItem->setTextAlignment( Qt::AlignLeft );
		departureItem->setData( 1, SortRole );
		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;

	case JourneyNewsLabelItem:
	    if ( !update ) {
		departureItem->setText( i18nc("News for a journey with public transport, like 'platform changed'", "News:") );
		departureItem->setTextAlignment( Qt::AlignRight );
		departureItem->setData( 2, SortRole );
		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;

	case JourneyNewsItem:
	    departureItem->setText( departureInfo.journeyNews );
	    if ( !update ) {
		departureItem->setTextAlignment( Qt::AlignLeft );
		departureItem->setData( 2, SortRole );
		departureItem->setData( 3, HtmlDelegate::LinesPerRowRole ); // 3 lines for journey news
		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;

	case DelayLabelItem:
	    if ( !update ) {
		departureItem->setText(  i18nc("Information about delays of a journey with public transport", "Delay:") + "<br>" );
		departureItem->setTextAlignment( Qt::AlignRight );
		departureItem->setData( 0, SortRole );
		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;

	default:
	    break;
    }
}

void PublicTransport::appendDeparture ( const DepartureInfo& departureInfo ) {
    QStandardItem *itemLineString = new QStandardItem();
    QStandardItem *itemTarget = new QStandardItem();
    QStandardItem *itemDeparture = new QStandardItem();
    setValuesOfDepartureItem( itemLineString, departureInfo, LineNameItem );
    setValuesOfDepartureItem( itemTarget, departureInfo, TargetItem );
    setValuesOfDepartureItem( itemDeparture, departureInfo, DepartureItem );
    m_model->appendRow( QList< QStandardItem* >() << itemLineString << itemTarget << itemDeparture );

    // Search if an abandoned alarm timer matches
    for( int i = m_abandonedAlarmTimer.count() - 1; i >= 0; --i ) {
	AlarmTimer* alarmTimer = m_abandonedAlarmTimer[i];
	setAlarmForDeparture( itemDeparture->index(), alarmTimer );
	m_abandonedAlarmTimer.removeAt(i);
    }

    int iRow = 0;
    if ( !departureInfo.platform.isEmpty() )
    {
	QStandardItem *itemLabelPlatform = new QStandardItem();
	QStandardItem *itemPlatform = new QStandardItem();
	setValuesOfDepartureItem( itemLabelPlatform, departureInfo, PlatformLabelItem );
	setValuesOfDepartureItem( itemPlatform, departureInfo, PlatformItem );
	itemLineString->insertRow( iRow++, QList<QStandardItem*>() << itemLabelPlatform << itemPlatform << new QStandardItem() );
    }

    if ( !departureInfo.journeyNews.isEmpty() )
    {
	QStandardItem *itemLabelJourneyNews = new QStandardItem();
	QStandardItem *itemJourneyNews = new QStandardItem();
	setValuesOfDepartureItem( itemLabelJourneyNews, departureInfo, JourneyNewsLabelItem );
	setValuesOfDepartureItem( itemJourneyNews, departureInfo, JourneyNewsItem );
	itemLineString->insertRow( iRow++, QList<QStandardItem*>() << itemLabelJourneyNews << itemJourneyNews << new QStandardItem() );
    }

    QStandardItem *itemLabelDelay = new QStandardItem();
    setValuesOfDepartureItem( itemLabelDelay, departureInfo, DelayLabelItem );
    QStandardItem *itemDelayInfo, *itemDelay;
    getDelayItems( departureInfo, &itemDelayInfo, &itemDelay ); // TODO: set sort data = 0
    itemLineString->insertRow( iRow++, QList<QStandardItem*>() << itemLabelDelay << itemDelayInfo << itemDelay );
}

void PublicTransport::updateDeparture ( int row, const DepartureInfo& departureInfo ) {
    QStandardItem *itemLineString = m_model->item( row, 0 );
    QStandardItem *itemTarget = m_model->item( row, 1 );
    QStandardItem *itemDeparture = m_model->item( row, 2 );
    setValuesOfDepartureItem( itemLineString, departureInfo, LineNameItem, true );
    setValuesOfDepartureItem( itemTarget, departureInfo, TargetItem, true );
    setValuesOfDepartureItem( itemDeparture, departureInfo, DepartureItem, true );

    // Update platform
    int iRow = itemLineString->rowCount();
    QStandardItem *itemLabelPlatform = NULL, *itemPlatform = NULL;
    for ( int i = 0; i < itemLineString->rowCount(); ++i ) {
	if ( itemLineString->child( i )->data( SortRole ).toInt() == 1 ) { // TODO == 1   =>   "== enum"
	    itemLabelPlatform = itemLineString->child( i, 0 );
	    itemPlatform = itemLineString->child( i, 1 );
	    break;
	}
    }
    if ( !departureInfo.platform.isEmpty() ) {
	if ( itemLabelPlatform == NULL ) { // Create new platform item
	    itemLabelPlatform = new QStandardItem();
	    itemPlatform = new QStandardItem();
	    setValuesOfDepartureItem( itemLabelPlatform, departureInfo, PlatformLabelItem );
	    setValuesOfDepartureItem( itemPlatform, departureInfo, PlatformItem );
	    itemLineString->insertRow( iRow++, QList<QStandardItem*>() << itemLabelPlatform << itemPlatform << new QStandardItem() );
	} else // Update old platform item
	    setValuesOfDepartureItem( itemPlatform, departureInfo, PlatformItem, true );
    } else if ( itemLabelPlatform != NULL ) {
	itemLineString->removeRow( itemLabelPlatform->row() );
	--iRow;
    }

    // Update journey news
    QStandardItem *itemLabelJourneyNews = NULL, *itemJourneyNews = NULL;
    for ( int i = 0; i < itemLineString->rowCount(); ++i ) {
	if ( itemLineString->child( i )->data( Qt::UserRole ).toInt() == 2 ) { // TODO == 1   =>   "== enum"
	    itemLabelJourneyNews = itemLineString->child( i, 0 );
	    itemJourneyNews = itemLineString->child( i, 1 );
	    break;
	}
    }
    if ( !departureInfo.journeyNews.isEmpty() ) {
	if ( itemLabelJourneyNews == NULL ) { // Create new journey news item
	    QStandardItem *itemLabelJourneyNews = new QStandardItem();
	    QStandardItem *itemJourneyNews = new QStandardItem();
	    setValuesOfDepartureItem( itemLabelJourneyNews, departureInfo, JourneyNewsLabelItem );
	    setValuesOfDepartureItem( itemJourneyNews, departureInfo, JourneyNewsItem );
	    itemLineString->insertRow( iRow++, QList<QStandardItem*>() << itemLabelJourneyNews << itemJourneyNews << new QStandardItem() );
	} else // Update old journey news item
	    setValuesOfDepartureItem( itemJourneyNews, departureInfo, JourneyNewsItem, true );
    } else if ( itemLabelJourneyNews != NULL ) {
	itemLineString->removeRow( itemLabelJourneyNews->row() );
	--iRow;
    }

    // Update delay
    QStandardItem *itemLabelDelay = NULL, *itemDelayInfo = NULL, *itemDelay = NULL;
    for ( int i = 0; i < itemLineString->rowCount(); ++i ) {
	if ( itemLineString->child( i )->data( Qt::UserRole ).toInt() == 0 ) { // TODO == 0   =>   "== enum"
	    itemLabelDelay = itemLineString->child( i, 0 );
	    itemDelayInfo = itemLineString->child( i, 1 );
	    itemDelay = itemLineString->child( i, 2 );
	    break;
	}
    }
    if ( itemLabelDelay == NULL ) { // Create new delay item
	itemLabelDelay = new QStandardItem();
	setValuesOfDepartureItem( itemLabelDelay, departureInfo, DelayLabelItem );
	getDelayItems( departureInfo, &itemDelayInfo, &itemDelay ); // TODO: set sort data = 0
	itemLineString->insertRow( iRow++, QList<QStandardItem*>() << itemLabelDelay << itemDelayInfo << itemDelay );
    } else { // Update old delay item
// 	QStandardItem *newItemDelayInfo, *newItemDelay;
	getDelayItems( departureInfo, &itemDelayInfo, &itemDelay ); // TODO: set sort data = 0
// 	itemDelayInfo->setText( newItemDelayInfo->text() );
// 	itemDelay->setText( newItemDelay->text() );

// 	delete newItemDelayInfo;
// 	delete newItemDelay;
    }
}

void PublicTransport::removeOldDepartures() {
    qDebug() << "PublicTransport::removeOldDepartures" << m_departureInfos.count() << "departures";

    QList< QModelIndex > notFoundRows;
    for ( int row = m_model->rowCount() - 1; row >= 0; --row )
	notFoundRows.append( m_model->index(row, 0) );
    foreach( DepartureInfo departureInfo, m_departureInfos )
    {
	int row = findDeparture( departureInfo );
	if ( row != -1 ) {
	    if ( !filterOut( departureInfo ) && !notFoundRows.removeOne( m_model->index(row, 0) ) )
		qDebug() << "PublicTransport::removeOldDepartures" << "Couldn't remove index";
	}
    }

    foreach( QModelIndex notFoundRow, notFoundRows )
    {
	qDebug() << "PublicTransport::removeOldDepartures" << "remove row" << notFoundRow.row();

	QStandardItem *itemDeparture = m_model->item( notFoundRow.row(), 2 );
	AlarmTimer *alarmTimer = (AlarmTimer*)itemDeparture->data( AlarmTimerRole ).value<void*>();
	if ( alarmTimer &&  alarmTimer->timer()->isActive() ) {
	    m_abandonedAlarmTimer.append( alarmTimer );
	    qDebug() << "PublicTransport::removeOldDepartures" << "append abandoned alarm timer of not found row" << notFoundRow.row();
	}

	m_model->removeRow( notFoundRow.row() );
    }
    qDebug() << "PublicTransport::removeOldDepartures" << "END";
}

void PublicTransport::updateModel() {
    if (m_graphicsWidget == NULL)
	graphicsWidget();

    int sortSection = ((QTreeView*)m_treeView->widget())->header()->sortIndicatorSection();
    Qt::SortOrder sortOrder = ((QTreeView*)m_treeView->widget())->header()->sortIndicatorOrder();

    m_label->setText( titleText(serviceProviderData()["shortUrl"].toString()) );
    removeOldDepartures(); // also remove filtered departures  (after changing filter settings)?

    foreach( DepartureInfo departureInfo, m_departureInfos )
    {
	int row = findDeparture(departureInfo);

	// Apply filters
	if ( filterOut(departureInfo) )
	    continue;

	if ( row != -1 ) { // just update departure data
	    updateDeparture( row, departureInfo );
	    kDebug() << "update row" << row;
	} else { // append new departure
	    appendDeparture( departureInfo );
	    kDebug() << "append row";
	}
    }

    // Restore sort indicator
    ((QTreeView*)m_treeView->widget())->header()->setSortIndicator( sortSection, sortOrder );

    // Sort list of departures / arrivals
    // TODO: first in sorted departure list WITH delay
    qSort( m_departureInfos.begin(), m_departureInfos.end() );

    geometryChanged();
}

KIcon PublicTransport::iconFromVehicleType ( const VehicleType &vehicleType, const QString &overlayIcon ) {
    KIcon icon;

    switch ( vehicleType )
    {
    case Tram:
        icon = KIcon( "vehicle_type_tram" );
	break;
    case Bus:
	icon = KIcon( "vehicle_type_bus" );
	break;
    case Subway:
	icon = KIcon( "vehicle_type_subway" );
	break;
    case TrainInterurban:
	icon = KIcon( "vehicle_type_train_interurban" );
	break;

    case TrainRegionalExpress:
	icon = KIcon( "vehicle_type_train_regionalexpress" );
	break;
    case TrainInterregio:
	icon = KIcon( "vehicle_type_train_interregio" );
	break;
    case TrainIntercityEurocity:
	icon = KIcon( "vehicle_type_train_intercityeurocity" );
	break;
    case TrainIntercityExpress:
	icon = KIcon( "vehicle_type_train_intercityexpress" );
	break;

    case TrainRegional: // Icon not done yet
    case Unknown:
    default:
	icon = KIcon( "status_unknown" );
    }

    if ( !overlayIcon.isEmpty() )
	icon = makeOverlayIcon( icon, overlayIcon );

    return icon;
}

KIcon PublicTransport::makeOverlayIcon ( const KIcon &icon, const KIcon &overlayIcon, const QSize &overlaySize ) {
    QPixmap pixmap = icon.pixmap(16), pixmapOverlay = overlayIcon.pixmap(overlaySize);
    QPainter p(&pixmap);
    p.drawPixmap(QPoint(16 - overlaySize.width(), 16 - overlaySize.height()), pixmapOverlay);
    p.end();
    KIcon resultIcon = KIcon();
    resultIcon.addPixmap(pixmap, QIcon::Normal);

    KIconEffect iconEffect;
    pixmap = iconEffect.apply( pixmap, KIconLoader::Small, KIconLoader::ActiveState );
    resultIcon.addPixmap(pixmap, QIcon::Selected);
    resultIcon.addPixmap(pixmap, QIcon::Active);

    return resultIcon;
}

KIcon PublicTransport::makeOverlayIcon ( const KIcon &icon, const QString &overlayIconName, const QSize &overlaySize ) {
    return makeOverlayIcon( icon, KIcon(overlayIconName), overlaySize );
}

QString PublicTransport::vehicleTypeToString( const VehicleType &vehicleType, bool plural ) {
    switch ( vehicleType )
    {
    case Tram:
        return plural ? i18n("trams") :i18n ("tram");
    case Bus:
	return plural ? i18n("buses") : i18n("bus");
    case Subway:
	return plural ? i18n("subways") : i18n("subway");
    case TrainInterurban:
	return plural ? i18n("interurban trains") : i18n("interurban train");

    case TrainRegional:
	return plural ? i18n("regional trains") : i18n("regional train");
    case TrainRegionalExpress:
	return plural ? i18n("regional express trains") : i18n("regional express");
    case TrainInterregio:
	return plural ? i18n("interregional trains") : i18n("interregional train" );
    case TrainIntercityEurocity:
	return plural ? i18n("intercity / eurocity trains") : i18n("intercity / eurocity");
    case TrainIntercityExpress:
	return plural ? i18n("intercity express trains") : i18n("intercity express");

    case Unknown:
    default:
        return i18nc("Unknown type of vehicle", "Unknown" );
    }
}

QString PublicTransport::vehicleTypeToConfigName ( const VehicleType& vehicleType )
{
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


#include "publictransport.moc"