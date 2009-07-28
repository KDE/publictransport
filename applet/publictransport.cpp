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
#include <QCompleter>
#include <QStringListModel>
#include <qmath.h>

// Own includes
#include "publictransport.h"
#include "htmldelegate.h"


PublicTransport::PublicTransport(QObject *parent, const QVariantList &args)
    : AppletWithState(parent, args), //Plasma::PopupApplet(parent, args),
    m_graphicsWidget(0),
    m_icon(0),
    m_label(0),
    m_labelInfo(0),
    m_treeView(0),
    m_model(0),
    m_currentSource(""),
    m_settings( this )
{
    m_departureViewColumns << LineStringColumn << TargetColumn << DepartureColumn;
    m_journeyViewColumns << VehicleTypeListColumn << JourneyInfoColumn << DepartureColumn << ArrivalColumn;

    setBackgroundHints(DefaultBackground);
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
//     m_svg.setImagePath("widgets/background");
    setHasConfigurationInterface(true);
    resize(300, 200);

    kDebug() << "                TRYING KDEBUG() OUTPUT?";
    kError() << "                TRYING KERROR() OUTPUT?";

    connect( &m_settings, SIGNAL(configNeedsSaving()), this, SLOT(emitConfigNeedsSaving()) );
    connect( &m_settings, SIGNAL(configurationRequired(bool,QString)), this, SLOT(configurationIsRequired(bool,QString)) );
    connect( &m_settings, SIGNAL(departureListNeedsClearing()), this, SLOT(departureListNeedsClearing()) );
    connect( &m_settings, SIGNAL(modelNeedsUpdate()), this, SLOT(modelNeedsUpdate()) );
    connect( &m_settings, SIGNAL(settingsChanged()), this, SLOT(emitSettingsChanged()) );
    connect( &m_settings, SIGNAL(serviceProviderSettingsChanged()), this, SLOT(serviceProviderSettingsChanged()) );
    connect( &m_settings, SIGNAL(departureArrivalListTypeChanged(DepartureArrivalListType)), this, SLOT(departureArrivalListTypeChanged(DepartureArrivalListType)) );
    connect( &m_settings, SIGNAL(journeyListTypeChanged(JourneyListType)), this, SLOT(journeyListTypeChanged(JourneyListType)) );
}

void PublicTransport::emitConfigNeedsSaving() {
//     qDebug() << "PublicTransport::emitConfigNeedsSaving";
    emit configNeedsSaving();
}

void PublicTransport::emitSettingsChanged() {
//     qDebug() << "PublicTransport::emitSettingsChanged";
    emit settingsChanged();
}

void PublicTransport::configurationIsRequired( bool needsConfiguring, const QString &reason ) {
//     qDebug() << "PublicTransport::configurationIsRequired";
    setConfigurationRequired( needsConfiguring, reason );
}

void PublicTransport::departureListNeedsClearing() {
//     qDebug() << "PublicTransport::configurationIsRequired";
    m_departureInfos.clear();
}

void PublicTransport::modelNeedsUpdate() {
//     qDebug() << "PublicTransport::modelNeedsUpdate";
    updateModel();
}

PublicTransport::~PublicTransport() {
    if (hasFailedToLaunch()) {
        // Do some cleanup here
    } else {
	delete m_label; // I *think* this gets deleted when I delete m_graphicsWidget...
	delete m_labelInfo;
// 	delete m_treeWidget; // I *think* this gets deleted when I delete m_graphicsWidget...
	delete m_graphicsWidget; // ...which I *think* also gets deleted automatically by plasma?
    }
}

void PublicTransport::init() {
    m_settings.readSettings();

    createModels();
    graphicsWidget();
    createTooltip();
    createPopupIcon();

    setDepartureArrivalListType( m_settings.departureArrivalListType() );
    setJourneyListType( m_settings.journeyListType() );
    addState( ShowingDepartureArrivalList );
    addState( WaitingForDepartureData );

    // Setup header options
//   TODO  m_treeView->nativeWidget()->header()->setVisible( cg.readEntry("showHeader", true) );
//     if ( cg.readEntry("hideColumnTarget", false) == true )
// 	hideColumnTarget( true );

    connect( this, SIGNAL(geometryChanged()), this, SLOT(geometryChanged()) );
    connect( this, SIGNAL(settingsChanged()), this, SLOT(configChanged()) );
    connect( Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), this, SLOT(themeChanged()) );
    emit settingsChanged();

    setupActions();
    reconnectSource();
}

void PublicTransport::themeChanged() {
    useCurrentPlasmaTheme();
}

void PublicTransport::setupActions() {
    QAction *actionUpdate = new QAction( KIcon("view-refresh"), i18n("&Update timetable"), this );
    connect( actionUpdate, SIGNAL(triggered(bool)), this, SLOT(updateDataSource(bool)) );
    addAction( "updateTimetable", actionUpdate );

    QAction *actionSetAlarmForDeparture = new QAction( Global::makeOverlayIcon(KIcon("kalarm"), "list-add"), m_settings.departureArrivalListType() == DepartureList ? i18n("Set &alarm for this departure") : i18n("Set &alarm for this arrival"), this );
    connect( actionSetAlarmForDeparture, SIGNAL(triggered(bool)), this, SLOT(setAlarmForDeparture(bool)) );
    addAction( "setAlarmForDeparture", actionSetAlarmForDeparture );

    QAction *actionRemoveAlarmForDeparture = new QAction( Global::makeOverlayIcon(KIcon("kalarm"), "list-remove"), m_settings.departureArrivalListType() == DepartureList ? i18n("Remove &alarm for this departure") : i18n("Remove &alarm for this arrival"), this );
    connect( actionRemoveAlarmForDeparture, SIGNAL(triggered(bool)), this, SLOT(removeAlarmForDeparture(bool)) );
    addAction( "removeAlarmForDeparture", actionRemoveAlarmForDeparture );

    QAction *actionAddTargetToFilterList = new QAction( Global::makeOverlayIcon(KIcon("view-filter"), "list-add"), m_settings.departureArrivalListType() == DepartureList ? i18n("&Hide target") : i18n("&Hide origin"), this );
    connect( actionAddTargetToFilterList, SIGNAL(triggered(bool)), this, SLOT(addTargetToFilterList(bool)) );
    addAction( "addTargetToFilterList", actionAddTargetToFilterList );

    QAction *actionRemoveTargetFromFilterList = new QAction( Global::makeOverlayIcon(KIcon("view-filter"), "list-remove"), m_settings.departureArrivalListType() == DepartureList ? i18n("Remove target from the &filter list") : i18n("Remove origin from the &filter list"), this );
    connect( actionRemoveTargetFromFilterList, SIGNAL(triggered(bool)), this, SLOT(removeTargetFromFilterList(bool)) );
    addAction( "removeTargetFromFilterList", actionRemoveTargetFromFilterList );

    QAction *actionAddTargetToFilterListAndHide = new QAction( Global::makeOverlayIcon(KIcon("view-filter"), "list-add"), m_settings.departureArrivalListType() == DepartureList ? i18n("&Hide target") : i18n("&Hide origin"), this );
    connect( actionAddTargetToFilterListAndHide, SIGNAL(triggered(bool)), this, SLOT(addTargetToFilterListAndHide(bool)) );
    addAction( "addTargetToFilterListAndHide", actionAddTargetToFilterListAndHide );

    QAction *actionSetFilterListToHideMatching = new QAction( KIcon("view-filter"), m_settings.departureArrivalListType() == DepartureList ? i18n("&Hide target") : i18n("&Hide origin"), this );
    connect( actionSetFilterListToHideMatching, SIGNAL(triggered(bool)), this, SLOT(setFilterListToHideMatching(bool)) );
    addAction( "setFilterListToHideMatching", actionSetFilterListToHideMatching );

    QAction *actionSetFilterListToShowAll = new QAction( Global::makeOverlayIcon(KIcon("view-filter"), "edit-delete"), m_settings.departureArrivalListType() == DepartureList ? i18n("&Show all targets") : i18n("&Show all origins"), this );
    connect( actionSetFilterListToShowAll, SIGNAL(triggered(bool)), this, SLOT(setFilterListToShowAll(bool)) );
    addAction( "setFilterListToShowAll", actionSetFilterListToShowAll );

    QAction *actionFilterOutByVehicleType = new QAction( KIcon("view-filter"), i18n("Filter out by vehicle type"), this );
    connect( actionFilterOutByVehicleType, SIGNAL(triggered(bool)), this, SLOT(filterOutByVehicleType(bool)) );
    addAction( "filterOutByVehicleType", actionFilterOutByVehicleType );

    QAction *actionRemoveAllFiltersByVehicleType = new QAction( Global::makeOverlayIcon(KIcon("view-filter"), "edit-delete"), i18n("Sho&w all vehicle types"), this );
    connect( actionRemoveAllFiltersByVehicleType, SIGNAL(triggered(bool)), this, SLOT(removeAllFiltersByVehicleType(bool)) );
    addAction( "removeAllFiltersByVehicleType", actionRemoveAllFiltersByVehicleType );

    QAction *actionToggleExpanded = new QAction( KIcon("arrow-down"), i18n("&Show additional information"), this );
    connect( actionToggleExpanded, SIGNAL(triggered(bool)), this, SLOT(toggleExpanded(bool)) );
    addAction( "toggleExpanded", actionToggleExpanded );

    QAction *actionHideHeader = new QAction( KIcon("edit-delete"), i18n("&Hide header"), this );
    connect( actionHideHeader, SIGNAL(triggered(bool)), this, SLOT(hideHeader(bool)) );
    addAction( "hideHeader", actionHideHeader );

    QAction *actionShowHeader = new QAction( KIcon("list-add"), i18n("Show &header"), this );
    connect( actionShowHeader, SIGNAL(triggered(bool)), this, SLOT(showHeader(bool)) );
    addAction( "showHeader", actionShowHeader );

    QAction *actionHideColumnTarget = new QAction( KIcon("view-right-close"), i18n("Hide &target column"), this );
    connect( actionHideColumnTarget, SIGNAL(triggered(bool)), this, SLOT(hideColumnTarget(bool)) );
    addAction( "hideColumnTarget", actionHideColumnTarget );

    QAction *actionShowColumnTarget = new QAction( KIcon("view-right-new"), i18n("Show &target column"), this );
    connect( actionShowColumnTarget, SIGNAL(triggered(bool)), this, SLOT(showColumnTarget(bool)) );
    addAction( "showColumnTarget", actionShowColumnTarget );
}

QList< QAction* > PublicTransport::contextualActions() {
    return QList< QAction* >() << action( "updateTimetable" );
}

void PublicTransport::updateDataSource ( bool ) {
    reconnectSource();
}

QString PublicTransport::stop() const {
    // TODO: create new method in PublicTransportSettings::stopOrStopID()
    return m_settings.stopID().isEmpty() ? m_settings.stop() : m_settings.stopID();
}

void PublicTransport::disconnectJourneySource() {
    if ( !m_currentJourneySource.isEmpty() ) {
	kDebug() << "Disconnect journey data source" << m_currentJourneySource;
	dataEngine("publictransport")->disconnectSource(m_currentJourneySource, this);
    }
}

void PublicTransport::reconnectJourneySource ( const QString& targetStopName ) {
    disconnectJourneySource();

    QString _targetStopName = targetStopName;
    if ( _targetStopName.isEmpty() ) {
	if ( m_lastSecondStopName.isEmpty() )
	    return;
	_targetStopName = m_lastSecondStopName;
    }

    m_currentJourneySource = QString("%5 %1|originStop=%2|targetStop=%3|maxDeps=%4").arg( m_settings.serviceProvider() ).arg( stop() ).arg( _targetStopName ).arg( m_settings.maximalNumberOfDepartures() ).arg( m_settings.journeyListType() == JourneysFromHomeStopList ? "Journeys" : "Journeys" );
    if ( m_settings.firstDepartureConfigMode() == RelativeToCurrentTime )
	m_currentJourneySource += QString("|timeOffset=%1").arg( m_settings.timeOffsetOfFirstDeparture() );
    else
	m_currentJourneySource += QString("|time=%1").arg( m_settings.timeOfFirstDepartureCustom().toString("hh:mm") );
    if ( m_settings.useSeperateCityValue() )
	m_currentJourneySource += QString("|city=%1").arg( m_settings.city() );
    /*
    if ( m_settings.useSeperateCityValue() )
	m_currentJourneySource = QString("%7 %1|city=%2|originStop=%3|targetStop=%4|maxDeps=%5|timeOffset=%6").arg( m_settings.serviceProvider() ).arg( m_settings.city() ).arg( stop() ).arg( _targetStopName ).arg( m_settings.maximalNumberOfDepartures() ).arg( m_settings.timeOffsetOfFirstDeparture() ).arg( m_settings.journeyListType() == JourneysFromHomeStopList ? "JourneysFrom" : "JourneysTo" );
    else
	m_currentJourneySource = QString("%6 %1|originStop=%2|targetStop=%3|maxDeps=%4|timeOffset=%5").arg( m_settings.serviceProvider() ).arg( stop() ).arg( _targetStopName ).arg( m_settings.maximalNumberOfDepartures() ).arg( m_settings.timeOffsetOfFirstDeparture() ).arg( m_settings.journeyListType() == JourneysFromHomeStopList ? "JourneysFrom" : "JourneysTo" );*/

    qDebug() << "Connect journey data source" << m_currentJourneySource << "Timeout" << m_settings.updateTimeout();
    m_lastSecondStopName = _targetStopName;
    addState( WaitingForJourneyData );

    if ( m_settings.updateTimeout() == 0 )
	dataEngine("publictransport")->connectSource( m_currentJourneySource, this );
    else
	dataEngine("publictransport")->connectSource( m_currentJourneySource, this, m_settings.updateTimeout() * 1000, Plasma::AlignToMinute );
}

void PublicTransport::reconnectSource() {
    if ( !m_currentSource.isEmpty() ) {
	kDebug() << "Disconnect data source" << m_currentSource;
	dataEngine("publictransport")->disconnectSource(m_currentSource, this);
    }

    m_currentSource = QString("%4 %1|stop=%2|maxDeps=%3").arg( m_settings.serviceProvider() ).arg( stop() ).arg( m_settings.maximalNumberOfDepartures() ).arg( m_settings.departureArrivalListType() == ArrivalList ? "Arrivals" : "Departures" );
    if ( m_settings.firstDepartureConfigMode() == RelativeToCurrentTime )
	m_currentSource += QString("|timeOffset=%1").arg( m_settings.timeOffsetOfFirstDeparture() );
    else
	m_currentSource += QString("|time=%1").arg( m_settings.timeOfFirstDepartureCustom().toString("hh:mm") );
    if ( m_settings.useSeperateCityValue() )
	m_currentSource += QString("|city=%1").arg( m_settings.city() );

    addState( WaitingForDepartureData );

    qDebug() << "Connect data source" << m_currentSource << "Timeout" << m_settings.updateTimeout();
    if ( m_settings.updateTimeout() == 0 )
	dataEngine("publictransport")->connectSource( m_currentSource, this );
    else
	dataEngine("publictransport")->connectSource( m_currentSource, this, m_settings.updateTimeout() * 1000, Plasma::AlignToMinute );
}

void PublicTransport::processJourneyList ( const Plasma::DataEngine::Data& data ) {
    // Remove old journey list
    m_journeyInfos.clear();

    qDebug() << "PublicTransport::processJourneyList" << data;
    int count = data["count"].toInt();
    for( int i = 0; i < count; ++i )
    {
	QVariant journeyData = data.value( QString("%1").arg(i) );
	if ( !journeyData.isValid() || m_journeyInfos.count() >= m_settings.maximalNumberOfDepartures() ) {
	    if ( !(m_journeyInfos.count() >= m_settings.maximalNumberOfDepartures()) )
		qDebug() << "PublicTransport::processJourneyList" << i << "Journey data is invalid";// << data;
	    break;
	}

	QHash<QString, QVariant> dataMap = journeyData.toHash();
	JourneyInfo journeyInfo( dataMap["operator"].toString(), dataMap["vehicleTypes"].toList(), dataMap["departure"].toDateTime(), dataMap["arrival"].toDateTime(), dataMap["pricing"].toString(), dataMap["startStopName"].toString(), dataMap["targetStopName"].toString(), dataMap["duration"].toInt(), dataMap["changes"].toInt(), dataMap["journeyNews"].toString() );

	// Only add journeys that are in the future
	// TODO: adjust for journeys TO the home stop (this works for journeys FROM the home stop)
	int secsToDepartureTime = QDateTime::currentDateTime().secsTo( journeyInfo.departure );
	if ( m_settings.firstDepartureConfigMode() == RelativeToCurrentTime )
	    secsToDepartureTime -= m_settings.timeOffsetOfFirstDeparture() * 60;
	if ( -secsToDepartureTime / 3600 >= 23 )
	    secsToDepartureTime += 24 * 3600;
	if ( secsToDepartureTime > -60 )
	    m_journeyInfos.append( journeyInfo );
    }

    qDebug() << "PublicTransport::processJourneyList" << m_journeyInfos.count() << "journeys received";
//     setBusy( false ); // call SetBusy() in add/removeState()
//     setConfigurationRequired( false );
//     m_stopNameValid = true;
//     m_lastSourceUpdate = data["updated"].toDateTime();
    updateModelJourneys();
}

void PublicTransport::processDepartureList ( const Plasma::DataEngine::Data& data ) {
    // Remove old departure / arrival list
    m_departureInfos.clear();

    int count = data["count"].toInt();
    for (int i = 0; i < count; ++i)
    {
	QVariant departureData = data.value( QString("%1").arg(i) );
	if ( !departureData.isValid() || m_departureInfos.count() >= m_settings.maximalNumberOfDepartures() ) {
	    if ( !(m_departureInfos.count() >= m_settings.maximalNumberOfDepartures()) )
		qDebug() << "PublicTransport::processDepartureList" << "Departure data for departure" << i << "is invalid" << data;
	    break;
	}

	QHash<QString, QVariant> dataMap = departureData.toHash();
	DepartureInfo departureInfo( dataMap["operator"].toString(), dataMap["line"].toString(), dataMap["target"].toString(), dataMap["departure"].toDateTime(), static_cast<VehicleType>(dataMap["vehicleType"].toInt()), dataMap["nightline"].toBool(), dataMap["expressline"].toBool(), dataMap["platform"].toString(), dataMap["delay"].toInt(), dataMap["delayReason"].toString(), dataMap["journeyNews"].toString() );

	// Only add departures / arrivals that are in the future
	QDateTime predictedDeparture = departureInfo.predictedDeparture();
	int secsToDepartureTime = QDateTime::currentDateTime().secsTo( predictedDeparture );
	if ( m_settings.firstDepartureConfigMode() == RelativeToCurrentTime )
	    secsToDepartureTime -= m_settings.timeOffsetOfFirstDeparture() * 60;
	if ( -secsToDepartureTime / 3600 >= 23 )
	    secsToDepartureTime += 24 * 3600;
	if ( secsToDepartureTime > -60 )
	    m_departureInfos.append( departureInfo );
    }

    kDebug() << m_departureInfos.count() << "departures / arrivals received";
//     setBusy( false );
    setConfigurationRequired( false );
    m_stopNameValid = true;
    m_lastSourceUpdate = data["updated"].toDateTime();
    updateModel();
}

void PublicTransport::clearDepartures() {
    m_departureInfos.clear(); // Clear data from data engine
    if (m_model != NULL) {
	m_model->removeRows( 0, m_model->rowCount() ); // Clear data to be displayed
	updateModel(); // TODO: Check if this is needed
    }
}

void PublicTransport::clearJourneys() {
    m_journeyInfos.clear(); // Clear data from data engine
    if ( m_modelJourneys != NULL ) {
	m_modelJourneys->removeRows( 0, m_modelJourneys->rowCount() ); // Clear data to be displayed
	updateModelJourneys(); // TODO: Check if this is needed
    }
}

void PublicTransport::processData ( const Plasma::DataEngine::Data& data ) {
//     qDebug() << "PublicTransport::processData" << data;
    bool journeyData = data.value("parseMode").toString() == "journeys";
    // TODO: handle journey data errors

    // Check for errors from the data engine
    if ( data.value("error").toBool() ) {
	if ( journeyData ) {
	    addState( ReceivedErroneousJourneyData );
	} else {
	    m_stopNameValid = false;
	    addState( ReceivedErroneousDepartureData );

	    if ( testState(ServiceProviderSettingsJustChanged) ) {
		if ( m_settings.departureArrivalListType() == DepartureList )
		    this->setConfigurationRequired(true, i18n("Error parsing departure information or currently no departures"));
		else
		    this->setConfigurationRequired(true, i18n("Error parsing arrival information or currently no arrivals"));
	    }

	    // Update remaining times
	    updateModel();
	}
    } else if ( data.value("receivedPossibleStopList").toBool() ) { // Check if we got a possible stop list or a journey list
	if ( journeyData ) {
	    addState( ReceivedErroneousJourneyData );
	    QHash< QString, QVariant > stopToStopID;
	    QStringList possibleStops;

	    int count = data["count"].toInt();
	    for( int i = 0; i < count; ++i ) {
		if ( !data.contains( QString("stopName %1").arg(i) ) ) {
		    qDebug() << "PublicTransport::processData" << "doesn't contain 'stopName" << i << "'! count =" << count << "data =" << data;
		    break;
		}

		QHash<QString, QVariant> dataMap = data.value( QString("stopName %1").arg(i) ).toHash();
		QString sStopName = dataMap["stopName"].toString();
		QString sStopID = dataMap["stopID"].toString();
		possibleStops << sStopName;
		stopToStopID.insert( sStopName, sStopID );
	    }

	    // TODO: use stop id
	    QStandardItemModel *model = (QStandardItemModel*)m_journeySearch->nativeWidget()->completer()->model();
	    if ( !model ) {
		model = new QStandardItemModel();
		m_journeySearch->nativeWidget()->completer()->setModel( model );
		m_listPossibleStops->setModel( model );
	    }

	    model->clear();
	    foreach( QString s, stopToStopID.keys() ) {
		QStandardItem *item = new QStandardItem(s);
		item->setIcon( KIcon("public-transport-stop") );
		model->appendRow( item );
	    }
	} else { // if ( journeyData )
	    m_stopNameValid = false;
	    addState( ReceivedErroneousDepartureData );
	    clearDepartures();
	    this->setConfigurationRequired(true, i18n("The stop name is ambiguous."));
	}
    } else {
	// List of journeys received
	if ( journeyData ) {
	    addState( ReceivedValidJourneyData );
	    if ( testState(ShowingJourneyList) )
		processJourneyList( data );
	} else {
	    m_stopNameValid = true;
	    addState( ReceivedValidDepartureData );
	    processDepartureList( data );
	}
    }

    removeState( SettingsJustChanged );
    removeState( ServiceProviderSettingsJustChanged );
}

void PublicTransport::dataUpdated ( const QString& sourceName, const Plasma::DataEngine::Data& data ) {
    Q_UNUSED( sourceName );

//     qDebug() << "PublicTransport::dataUpdated" << sourceName; // << data;
    if ( data.isEmpty() || (sourceName != m_currentSource && sourceName != m_currentJourneySource) )
	return;

//     this->setBusy(true);
    processData( data );
    createTooltip();
    createPopupIcon();
//     this->setBusy(false);
}

void PublicTransport::geometryChanged() {
//     if ( !isPopupShowing() ) {
// 	m_label->setMaximumWidth( contentsRect().width() );
//     }
//     qDebug() << "PublicTransport::geometryChanged";

    QTreeView *treeView = m_treeView->nativeWidget();
    QHeaderView *header = treeView->header();
    disconnect ( header, SIGNAL(sectionResized(int,int,int)), this, SLOT(treeViewSectionResized(int,int,int)) );

    if ( testState(ShowingDepartureArrivalList) ) {
	int lineSectionSize = treeView->columnWidth(m_departureViewColumns.indexOf(LineStringColumn));
	int departureSectionSize = treeView->columnWidth(m_departureViewColumns.indexOf(DepartureColumn));

	if ( treeView->isColumnHidden(m_departureViewColumns.indexOf(TargetColumn)) ) { // target column hidden
// 	    qDebug() << "PublicTransport::geometryChanged" << "Target column is hidden";

    // 	float lineSectionSizeFactor = (float)lineSectionSize / (float)(lineSectionSize + departureSectionSize);
    // 	lineSectionSize = qFloor((float)header->width() * lineSectionSizeFactor);
    // 	departureSectionSize = header->width() - lineSectionSize;

    // 	treeView->header()->setResizeMode( 1, QHeaderView::Fixed );
    // 	treeView->header()->resizeSection( 1, 0 );
    // 	treeView->header()->resizeSection( 0, lineSectionSize );
    // 	treeView->header()->resizeSection( 2, departureSectionSize );

    // 	qDebug() << "PublicTransport::geometryChanged" << "new column width" << lineSectionSize << departureSectionSize << "total =" << (lineSectionSize + departureSectionSize) << "of" << treeView->header()->width();
	} else {
	    treeView->header()->setResizeMode( 1, QHeaderView::Interactive );
	    if ( lineSectionSize + departureSectionSize > header->width() - 10 )
	    {
		float lineSectionSizeFactor = (float)lineSectionSize / (float)(lineSectionSize + departureSectionSize);
		lineSectionSize = header->width() * lineSectionSizeFactor;
		treeView->setColumnWidth( m_departureViewColumns.indexOf(LineStringColumn), lineSectionSize );
		departureSectionSize = header->width() * (1 - lineSectionSizeFactor);
		treeView->setColumnWidth( m_departureViewColumns.indexOf(DepartureColumn), departureSectionSize );
	    }

	    int targetSectionSize = header->width() - lineSectionSize - departureSectionSize;
	    if ( targetSectionSize < 10 )
		targetSectionSize = 10;
	    treeView->setColumnWidth( m_departureViewColumns.indexOf(TargetColumn), targetSectionSize );
	}
    } else if ( testState(ShowingJourneyList) ) {
	int vehicleTypeListSectionSize = treeView->columnWidth( m_journeyViewColumns.indexOf(VehicleTypeListColumn) );
	int departureSectionSize = treeView->columnWidth( m_journeyViewColumns.indexOf(DepartureColumn) );
	int arrivalSectionSize = treeView->columnWidth( m_journeyViewColumns.indexOf(ArrivalColumn) );

	treeView->header()->setResizeMode( 1, QHeaderView::Interactive );
	if ( vehicleTypeListSectionSize + departureSectionSize + arrivalSectionSize > header->width() - 10 )
	{
	    float vehicleTypeListSizeFactor = (float)vehicleTypeListSectionSize / (float)(vehicleTypeListSectionSize + departureSectionSize + arrivalSectionSize);
	    float departureSizeFactor = (float)departureSectionSize / (float)(vehicleTypeListSectionSize + departureSectionSize + arrivalSectionSize);
	    float arrivalSizeFactor = 1.0f - vehicleTypeListSizeFactor - departureSizeFactor;

	    vehicleTypeListSectionSize = header->width() * vehicleTypeListSizeFactor;
	    treeView->setColumnWidth( m_journeyViewColumns.indexOf(VehicleTypeListColumn), vehicleTypeListSectionSize );
	    departureSectionSize = header->width() * departureSizeFactor;
	    treeView->setColumnWidth( m_journeyViewColumns.indexOf(DepartureColumn), departureSectionSize );
	    arrivalSectionSize = header->width() * arrivalSizeFactor;
	    treeView->setColumnWidth( m_journeyViewColumns.indexOf(ArrivalColumn), arrivalSectionSize );
	}

	int journeyInfoSectionSize = header->width() - vehicleTypeListSectionSize - departureSectionSize - arrivalSectionSize;
	if ( journeyInfoSectionSize < 10 )
	    journeyInfoSectionSize = 10;
	treeView->setColumnWidth( m_journeyViewColumns.indexOf(JourneyInfoColumn), journeyInfoSectionSize );
    }

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
//     m_label->setMaximumSize(9999, 9999);

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
	if ( m_settings.departureArrivalListType() ==  DepartureList ) {
	    data.setSubText( i18nc("%4 is the translated duration text, e.g. in 3 minutes",
		    "Next departure from '%1': line %2 (%3) %4", m_settings.stop(),
		    nextDeparture.lineString, nextDeparture.target , nextDeparture.durationString() ) );
	} else {
	    data.setSubText( i18nc("%4 is the translated duration text, e.g. in 3 minutes",
		    "Next arrival at '%1': line %2 (%3) %4", m_settings.stop(),
		    nextDeparture.lineString, nextDeparture.target , nextDeparture.durationString() ) );
	}
    }
    data.setImage( KIcon("public-transport-stop").pixmap(IconSize(KIconLoader::Desktop)) );
    Plasma::ToolTipManager::self()->setContent(this, data);
}

DepartureInfo PublicTransport::getFirstNotFilteredDeparture() {
    for ( int i = 0; i < m_departureInfos.count(); ++i ) {
	DepartureInfo nextDeparture = m_departureInfos[i];
	if ( !filterOut(nextDeparture) )
	    return nextDeparture;
    }

    return DepartureInfo();
}

AlarmTimer* PublicTransport::getNextAlarm() {
    for ( int row = 0; row < m_model->rowCount(); ++row ) {
	QStandardItem *itemDeparture = m_model->item( row, 2 );
	AlarmTimer *alarmTimer = (AlarmTimer*)itemDeparture->data( AlarmTimerRole ).value<void*>();
	if ( alarmTimer != NULL && alarmTimer->timer()->isActive() )
	    return alarmTimer;
    }

    return NULL;
}

void PublicTransport::configChanged() {
    disconnect(this, SIGNAL(settingsChanged()), this, SLOT(configChanged()));

    addState( SettingsJustChanged );

    setDepartureArrivalListType( m_settings.departureArrivalListType() );
    setJourneyListType( m_settings.journeyListType() );
    m_treeView->nativeWidget()->header()->setVisible( m_settings.isHeaderVisible() );
    m_treeView->nativeWidget()->setColumnHidden( 1, m_settings.isColumnTargetHidden() );

    m_treeView->nativeWidget()->setFont( m_settings.font() );
    m_label->nativeWidget()->setFont( m_settings.font() );
    m_labelInfo->nativeWidget()-> setFont( m_settings.font() );
    m_listPossibleStops->nativeWidget()->setFont( m_settings.font() );
    m_journeySearch->nativeWidget()->setFont( m_settings.font() );
//     m_treeView->nativeWidget()->updateGeometry(); // TODO: check if this helps resizing items when the font changed
    updateModel();
    updateModelJourneys();

    if ( m_settings.isColumnTargetHidden() )
	hideColumnTarget(true);
    else
	showColumnTarget(true);

    connect(this, SIGNAL(settingsChanged()), this, SLOT(configChanged()));
}

void PublicTransport::serviceProviderSettingsChanged() {
    addState( ServiceProviderSettingsJustChanged );
    if ( m_settings.checkConfig() ) {
	reconnectSource();

	if ( !m_currentJourneySource.isEmpty() )
	    reconnectJourneySource();
    }
}

void PublicTransport::createModels() {
    m_model = new QStandardItemModel( 0, 3 );
    m_model->setSortRole( SortRole );
// m_model->sort( 2, Qt::AscendingOrder );

    m_modelJourneys = new QStandardItemModel( 0, 4 );
    m_modelJourneys->setSortRole( SortRole );
}

void PublicTransport::setMainIconDisplay( MainIconDisplay mainIconDisplay ) {
    KIcon icon;
    KIconEffect iconEffect;
    QPixmap pixmap;

    // Make disabled icon
    switch ( mainIconDisplay ) {
	case DepartureListErrorIcon:
	    if ( m_settings.departureArrivalListType() == DepartureList ) {
		icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			    QList<KIcon>() << KIcon("go-home") << KIcon("go-next"), QSize(16, 16), 32 );
	    } else{
		icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			    QList<KIcon>() << KIcon("go-next") << KIcon("go-home"), QSize(16, 16), 32 );
	    }
	    pixmap = icon.pixmap(32);
	    pixmap = iconEffect.apply( pixmap, KIconLoader::Small, KIconLoader::DisabledState );
	    icon = KIcon();
	    icon.addPixmap(pixmap, QIcon::Normal);
	    break;

	case DepartureListOkIcon:
	    if ( m_settings.departureArrivalListType() == DepartureList ) {
		icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			    QList<KIcon>() << KIcon("go-home") << KIcon("go-next"), QSize(16, 16), 32 );
	    } else {
		icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			    QList<KIcon>() << KIcon("go-next") << KIcon("go-home"), QSize(16, 16), 32 );
	    }
	    break;

	case JourneyListOkIcon:
	    if ( m_settings.journeyListType() == JourneysFromHomeStopList ) {
		icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			    QList<KIcon>() << KIcon("go-home") << KIcon("go-next-view") << KIcon("public-transport-stop"), QSize(11, 11), 32 );
	    } else {
		icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			    QList<KIcon>() << KIcon("public-transport-stop") << KIcon("go-next-view") << KIcon("go-home"), QSize(11, 11), 32 );
	    }
	    break;

	case JourneyListErrorIcon:
	    if ( m_settings.journeyListType() == JourneysFromHomeStopList ) {
		icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			    QList<KIcon>() << KIcon("go-home") << KIcon("go-next-view") << KIcon("public-transport-stop"), QSize(11, 11), 32 );
	    } else {
		icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			    QList<KIcon>() << KIcon("public-transport-stop") << KIcon("go-next-view") << KIcon("go-home"), QSize(11, 11), 32 );
	    }
	    pixmap = icon.pixmap(32);
	    pixmap = iconEffect.apply( pixmap, KIconLoader::Small, KIconLoader::DisabledState );
	    icon = KIcon();
	    icon.addPixmap(pixmap, QIcon::Normal);
	    break;

	case AbortJourneySearchIcon:
	    icon = KIcon("edit-delete");
	    break;

	case GoBackIcon:
	    icon = KIcon("arrow-left");
	    break;
    }

    m_icon->setIcon( icon );
}

void PublicTransport::iconClicked() {
    if ( m_graphicsWidget == NULL )
	return;

    switch( m_titleType ) {
	case ShowSearchJourneyLineEdit:
	    addState( ShowingDepartureArrivalList );
	    break;
	case ShowJourneyListTitle:
	case ShowDepartureArrivalListTitle:
	    addState( ShowingJourneySearch );
	    break;
    }
// 	setTitleType( ShowSearchJourneyLineEdit );
//     } else if ( m_titleType == ShowDepartureArrivalListTitle )
// 	setTitleType( ShowSearchJourneyLineEdit );
// 	addState( ShowingJourneySearch );
}

void PublicTransport::iconCloseClicked() {
//     setTitleType( ShowDepartureArrivalListTitle );
    addState( ShowingDepartureArrivalList );
}

void PublicTransport::journeySearchInputFinished() {
    addState( ShowingJourneyList );
    reconnectJourneySource( m_journeySearch->text() );
}

void PublicTransport::journeySearchInputEdited( const QString &newText ) {
    reconnectJourneySource( newText );
}

QGraphicsLayout *PublicTransport::createLayoutTitle( TitleType titleType ) {
    QGraphicsLinearLayout *layoutTop = new QGraphicsLinearLayout(Qt::Horizontal);
    switch( titleType ) {
	case ShowDepartureArrivalListTitle:
	    m_icon->setVisible(true);
	    m_label->setVisible(true);
	    m_labelInfo->setVisible(true);
	    layoutTop->addItem(m_icon);
	    layoutTop->addItem(m_label);
	    layoutTop->addItem(m_labelInfo);
	    break;

	case ShowSearchJourneyLineEdit:
	    m_icon->setVisible(true);
	    m_journeySearch->setVisible(true);
	    m_journeySearch->setText( i18n("Type a target stop name") );
	    layoutTop->addItem(m_icon);
	    layoutTop->addItem(m_journeySearch);
	    break;

	case ShowJourneyListTitle:
	    m_icon->setVisible(true);
	    m_label->setVisible(true);
	    m_iconClose->setVisible(true);
	    layoutTop->addItem(m_icon);
	    layoutTop->addItem(m_label);
	    layoutTop->addItem(m_iconClose);
	    break;
    }
    return layoutTop;
}

void PublicTransport::possibleStopClicked( const QModelIndex &modelIndex ) {
    m_journeySearch->setText( modelIndex.data().toString() );
    m_journeySearch->setFocus();
}

void PublicTransport::possibleStopDoubleClicked ( const QModelIndex& modelIndex ) {
    Q_UNUSED( modelIndex );
    journeySearchInputFinished();
}

void PublicTransport::useCurrentPlasmaTheme() {
    QFont font = Plasma::Theme::defaultTheme()->font( Plasma::Theme::DefaultFont );
    int newPixelSize = qCeil((float)font.pixelSize() * 1.4f);
    if ( newPixelSize > 1 )
	font.setPixelSize( newPixelSize );
    m_label->setFont( font );

    // Get theme colors
    QColor textColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor );
    QColor baseColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::BackgroundColor );
    // 	QColor altBaseColor = baseColor.darker();
    // 	int green = altBaseColor.green() * 1.8;
    // 	altBaseColor.setGreen( green > 255 ? 255 : green ); // tint green
    QColor buttonColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::BackgroundColor );
    baseColor.setAlpha(50);
    // 	altBaseColor.setAlpha(50);
    buttonColor.setAlpha(130);
    m_colorSubItemLabels = textColor;
    m_colorSubItemLabels.setAlpha( 170 );

    // Create palette with the used theme colors
    QPalette p = palette();
    p.setColor( QPalette::Background, baseColor );
    p.setColor( QPalette::Base, baseColor );
    // 	p.setColor( QPalette::AlternateBase, altBaseColor );
    p.setColor( QPalette::Button, buttonColor );
    p.setColor( QPalette::Foreground, textColor );
    p.setColor( QPalette::Text, textColor );

    QTreeView *treeView = m_treeView->nativeWidget();
    treeView->setPalette(p);
    treeView->header()->setPalette( p );

    // To set new text color of the header items
    setDepartureArrivalListType( m_settings.departureArrivalListType() );
}

QGraphicsWidget* PublicTransport::graphicsWidget() {
    if ( !m_graphicsWidget ) {
	m_graphicsWidget = new QGraphicsWidget(this);
        m_graphicsWidget->setMinimumSize(225, 150);
        m_graphicsWidget->setPreferredSize(350, 200);

	m_iconClose = new Plasma::IconWidget();
	m_iconClose->setIcon("window-close");
	m_iconClose->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	m_iconClose->setPreferredSize(32, 32);
	connect( m_iconClose, SIGNAL(clicked()), this, SLOT(iconCloseClicked()) );

	m_icon = new Plasma::IconWidget();
	m_icon->setIcon( "public-transport-stop" );
	m_icon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	m_icon->setPreferredSize(32, 32);
// 	m_icon->setAcceptedMouseButtons(0);
// 	setMainIconDisplay( DepartureListError );
	connect( m_icon, SIGNAL(clicked()), this, SLOT(iconClicked()) );

	m_label = new Plasma::Label();
	m_label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
	m_label->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Fixed, QSizePolicy::Label );
	QLabel *label = m_label->nativeWidget();
	label->setTextInteractionFlags( Qt::LinksAccessibleByMouse );
	label->setWordWrap( true );

	m_labelInfo = new Plasma::Label();
	m_labelInfo->setAlignment(Qt::AlignTop | Qt::AlignRight);
	m_labelInfo->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred, QSizePolicy::Label );
	QLabel *labelInfo = m_labelInfo->nativeWidget();
	labelInfo->setOpenExternalLinks( true );
	labelInfo->setWordWrap( false );

	m_journeySearch = new Plasma::LineEdit();
	QLineEdit *journeySearch = m_journeySearch->nativeWidget();
	journeySearch->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );

	m_listPossibleStops = new Plasma::TreeView(m_graphicsWidget);
	m_listPossibleStops->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
	m_listPossibleStops->nativeWidget()->setRootIsDecorated( false );
	m_listPossibleStops->nativeWidget()->setHeaderHidden( true );
	m_listPossibleStops->nativeWidget()->setAlternatingRowColors( true );
	m_listPossibleStops->nativeWidget()->setEditTriggers( QAbstractItemView::NoEditTriggers );

	QCompleter *completer = new QCompleter();
// 	completer->setPopup( m_listPossibleStops->nativeWidget() );
	completer->setCaseSensitivity( Qt::CaseInsensitive );
	completer->setCompletionMode( QCompleter::InlineCompletion );
	m_journeySearch->nativeWidget()->setCompleter( completer );

	connect( m_journeySearch, SIGNAL(returnPressed()), this, SLOT(journeySearchInputFinished()) );
	connect( m_journeySearch, SIGNAL(textEdited(QString)), this, SLOT(journeySearchInputEdited(QString)) );
	connect( m_listPossibleStops->nativeWidget(), SIGNAL(clicked(QModelIndex)),
		 this, SLOT(possibleStopClicked(QModelIndex)) );
	connect( m_listPossibleStops->nativeWidget(), SIGNAL(doubleClicked(QModelIndex)),
		 this, SLOT(possibleStopDoubleClicked(QModelIndex)) );

	// Create treeview
	m_treeView = new Plasma::TreeView(m_graphicsWidget);
	m_treeView->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
	QTreeView *treeView =m_treeView->nativeWidget();
// 	treeView->setAlternatingRowColors( true );
	treeView->setAllColumnsShowFocus( true );
	treeView->setRootIsDecorated( false );
	treeView->setAnimated( true );
	treeView->setSortingEnabled( true );
	treeView->setWordWrap( true );
	treeView->setUniformRowHeights( false );
	treeView->setExpandsOnDoubleClick( false );
	treeView->setFrameShape( QFrame::StyledPanel );
	treeView->setEditTriggers( QAbstractItemView::NoEditTriggers );
	treeView->setSelectionMode( QAbstractItemView::NoSelection );
	treeView->setSelectionBehavior( QAbstractItemView::SelectRows );
	treeView->header()->setCascadingSectionResizes( true );
	treeView->header()->setResizeMode( QHeaderView::Interactive );
	treeView->header()->setSortIndicator( 2, Qt::AscendingOrder );
	treeView->header()->setContextMenuPolicy( Qt::CustomContextMenu );
// 	treeView->header()->setVisible( false ); // TODO: member variable bool m_headersVisible?
	treeView->setContextMenuPolicy( Qt::CustomContextMenu );
	HtmlDelegate *htmlDelegate = new HtmlDelegate;
	treeView->setItemDelegate( htmlDelegate );
	connect( treeView, SIGNAL(customContextMenuRequested(const QPoint &)),
		 this, SLOT(showDepartureContextMenu(const QPoint &)) );
	connect( treeView->header(), SIGNAL(customContextMenuRequested(const QPoint &)),
		this, SLOT(showHeaderContextMenu(const QPoint &)) );
	connect( treeView, SIGNAL(doubleClicked(QModelIndex)),
		 this, SLOT(doubleClickedDepartureItem(QModelIndex)) );

	if ( !m_model )
	    createModels();
	m_treeView->setModel( m_model );
// 	treeView->header()->setFirstColumnSpanned()
	treeView->header()->setStretchLastSection( false );
	treeView->header()->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	treeView->header()->resizeSection( 0, 60 );
	connect ( treeView->header(), SIGNAL(sectionResized(int,int,int)),
		  this, SLOT(treeViewSectionResized(int,int,int)) );

	QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical);
	layout->setContentsMargins( 0, 0, 0, 0 );
	layout->setSpacing( 0 );
	layout->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

	QGraphicsLayout *layoutTop = new QGraphicsLinearLayout(Qt::Horizontal); // = createLayoutTitle();
	layout->addItem( layoutTop );

	layout->addItem( m_treeView );
	m_graphicsWidget->setLayout(layout);

	registerAsDragHandle(m_graphicsWidget);
	registerAsDragHandle(m_label);
	registerAsDragHandle(m_labelInfo);
// 	registerAsDragHandle(m_treeView);

	useCurrentPlasmaTheme();
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

void PublicTransport::createConfigurationInterface( KConfigDialog* parent ) {
    m_settings.createConfigurationInterface( parent, m_stopNameValid );
}

QString PublicTransport::nameForTimetableColumn( TimetableColumn timetableColumn, DepartureArrivalListType departureArrivalListType ) {
    if ( departureArrivalListType == _UseCurrentDepartureArrivalListType )
	departureArrivalListType = m_settings.departureArrivalListType();

    switch( timetableColumn ) {
	case LineStringColumn:
	    return i18nc("A tramline or busline", "Line");
	case TargetColumn:
	    if ( !testState(ShowingDepartureArrivalList) || departureArrivalListType == DepartureList )
		return i18nc("Target of a tramline or busline", "Target");
	    else // if ( departureArrivalListType == ArrivalList )
		return i18nc("Origin of a tramline or busline", "Origin");
	case DepartureColumn:
	    if ( !testState(ShowingDepartureArrivalList) || departureArrivalListType == DepartureList )
		return i18nc("Time of departure of a tram or bus", "Departure");
	    else
		return i18nc("Time of arrival of a tram or bus", "Arrival");
	case ArrivalColumn:
	    return i18nc("Time of arrival of a tram or bus", "Arrival");
	case JourneyInfoColumn:
	    return i18nc("Information about a journey with public transport", "Information");
	case VehicleTypeListColumn:
	    return i18nc("Vehicle types used in a journey with public transport", "Vehicle types");
    }

    return QString();
}

void PublicTransport::departureArrivalListTypeChanged( DepartureArrivalListType departureArrivalListType ) {
    setDepartureArrivalListType( departureArrivalListType );
}

void PublicTransport::journeyListTypeChanged ( JourneyListType journeyListType ) {
    setJourneyListType( journeyListType );
}

void PublicTransport::setDepartureArrivalListType( DepartureArrivalListType departureArrivalListType ) {
    QBrush textBrush = QBrush( Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor) );
    QStringList titles;

    foreach( TimetableColumn column, m_departureViewColumns )
	titles << nameForTimetableColumn(column, departureArrivalListType);
    m_model->setHorizontalHeaderLabels( titles );

    int i = 0;
    foreach( TimetableColumn column, m_departureViewColumns ) {
	if ( column == LineStringColumn )
	    m_model->horizontalHeaderItem(i)->setTextAlignment( Qt::AlignRight );
	m_model->horizontalHeaderItem(i)->setForeground( textBrush );
	++i;
    }
}

void PublicTransport::setJourneyListType( JourneyListType journeyListType ) {
    Q_UNUSED( journeyListType );

    QBrush textBrush = QBrush( Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor) );
    QStringList titles;

    foreach( TimetableColumn column, m_journeyViewColumns )
	titles << nameForTimetableColumn(column);
    m_modelJourneys->setHorizontalHeaderLabels( titles );

//     int i = 0;
//     foreach( TimetableColumn column, m_journeyViewColumns ) {
    for( int i = 0; i < m_journeyViewColumns.count(); ++i )
	m_modelJourneys->horizontalHeaderItem(i)->setForeground( textBrush );
// 	++i;
//     }
}

void PublicTransport::setTitleType( TitleType titleType ) {
    if ( m_graphicsWidget == NULL )//|| m_titleType == titleType )
	return;

    QGraphicsLinearLayout *layoutMain = (QGraphicsLinearLayout*)m_graphicsWidget->layout();
    QGraphicsLinearLayout *layoutTop = ((QGraphicsLinearLayout*)layoutMain->itemAt(0));
    QGraphicsLayout *layoutTopNew = NULL;

    // Hide widgets from the old layout
    for ( int i = 0; i < layoutTop->count(); ++i )
	((QGraphicsWidget*)layoutTop->itemAt(i))->setVisible( false );

    switch( titleType ) {
	case ShowDepartureArrivalListTitle:
	    setMainIconDisplay( testState(ReceivedValidDepartureData) ? DepartureListOkIcon : DepartureListErrorIcon );
	    m_icon->setToolTip( i18n("Search journeys to or from the home stop") );
	    m_label->setText( titleText() );
	    m_labelInfo->setText( infoText() );

	    m_listPossibleStops->setVisible(false);
	    m_treeView->setVisible(true);
	    m_iconClose->setVisible(false);
	    layoutMain->removeAt(1);
	    layoutMain->insertItem(1, m_treeView);
	    break;

	case ShowSearchJourneyLineEdit:
	    setMainIconDisplay( AbortJourneySearchIcon );
	    m_icon->setToolTip( i18n("Abort search for journeys to or from the home stop") );

	    m_treeView->setVisible(false);
	    m_listPossibleStops->setVisible(true);
	    m_iconClose->setVisible(false);
	    layoutMain->removeAt(1);
	    layoutMain->insertItem(1, m_listPossibleStops);
	    break;

	case ShowJourneyListTitle:
	    // TODO: testState(ReceivedValidJourneyData)  =>  new enum value "ReceivedValidJourneyData"
	    setMainIconDisplay( testState(ReceivedValidJourneyData) ? JourneyListOkIcon : JourneyListErrorIcon );
	    m_icon->setToolTip( i18n("Search journeys to or from the home stop") );
	    m_iconClose->setToolTip( i18n("Show departures / arrivals") );
	    m_label->setText( i18n("<b>Journeys</b>") );

	    m_listPossibleStops->setVisible(false);
	    m_treeView->setVisible(true);
	    layoutMain->removeAt(1);
	    layoutMain->insertItem(1, m_treeView);
	    break;
    }

    layoutTopNew = createLayoutTitle( titleType );
    if ( layoutTopNew != NULL ) {
	layoutMain->removeAt( 0 );
	layoutMain->insertItem(0, layoutTopNew );
	layoutMain->setAlignment( layoutTopNew, Qt::AlignTop );
    }

    if ( titleType == ShowSearchJourneyLineEdit )
	m_journeySearch->setFocus();
    if ( titleType != ShowDepartureArrivalListTitle )
	m_journeySearch->nativeWidget()->selectAll();

    m_titleType = titleType;
}

void PublicTransport::unsetStates( QList< AppletState > states ) {
    foreach( AppletState appletState, states ) {
	if ( m_appletStates.testFlag(appletState) )
	    m_appletStates ^= appletState;
    }
}

bool PublicTransport::testState( AppletState state ) const {
    return m_appletStates.testFlag( state );
}

void PublicTransport::addState( AppletState state ) {
    QColor textColor;

    switch ( state ) {
	case ShowingDepartureArrivalList:
	    setTitleType( ShowDepartureArrivalListTitle );
	    m_icon->setToolTip( i18n("Search journey to or from the home stop") );
	    m_treeView->setModel( m_model );
	    m_treeView->nativeWidget()->setIconSize( QSize(16, 16) );
	    geometryChanged();
	    setBusy( testState(WaitingForDepartureData) );
	    disconnectJourneySource();

	    unsetStates( QList<AppletState>() << ShowingJourneyList << ShowingJourneySearch );
	    break;

	case ShowingJourneyList:
	    setTitleType( ShowJourneyListTitle );
	    m_icon->setToolTip( i18n("Search for new journey to or from the home stop") );
	    m_treeView->setModel( m_modelJourneys );
	    m_treeView->nativeWidget()->setIconSize( QSize(32, 32) );
	    setBusy( testState(WaitingForJourneyData) );

	    unsetStates( QList<AppletState>() << ShowingDepartureArrivalList << ShowingJourneySearch );
	    break;

	case ShowingJourneySearch:
	    setTitleType( ShowSearchJourneyLineEdit );
	    m_icon->setToolTip( i18n("Go back to the departure / arrival list") );
	    setBusy( false);

	    unsetStates( QList<AppletState>() << ShowingDepartureArrivalList << ShowingJourneyList );
	    break;

	case ReceivedValidDepartureData:
	    if ( m_titleType == ShowDepartureArrivalListTitle ) {
		setMainIconDisplay( DepartureListOkIcon );
		setBusy( false );
	    }
	    unsetStates( QList<AppletState>() << WaitingForDepartureData << ReceivedErroneousDepartureData );
	    break;

	case ReceivedValidJourneyData:
	    if ( m_titleType == ShowJourneyListTitle ) {
		setMainIconDisplay( JourneyListOkIcon );
		setBusy( false );
	    }
	    unsetStates( QList<AppletState>() << WaitingForJourneyData << ReceivedErroneousJourneyData );
	    break;

	case ReceivedErroneousDepartureData:
	    if ( m_titleType == ShowDepartureArrivalListTitle ) {
		setMainIconDisplay( DepartureListErrorIcon );
		setBusy( false );
	    }
	    unsetStates( QList<AppletState>() << WaitingForDepartureData << ReceivedValidDepartureData );
	    break;

	case ReceivedErroneousJourneyData:
	    if ( m_titleType == ShowJourneyListTitle ) {
		setMainIconDisplay( JourneyListErrorIcon );
		setBusy( false );
	    }
	    unsetStates( QList<AppletState>() << WaitingForJourneyData << ReceivedValidJourneyData );
	    break;

	case WaitingForDepartureData:
	    if ( m_titleType == ShowDepartureArrivalListTitle ) {
		setMainIconDisplay( DepartureListErrorIcon ); // TODO: Add a special icon for "waiting for data"? (waits for first data of a new data source)
		setBusy( true );
	    }
	    unsetStates( QList<AppletState>() << ReceivedValidDepartureData << ReceivedErroneousDepartureData );
	    break;

	case WaitingForJourneyData:
	    if ( m_titleType == ShowJourneyListTitle ) {
		setMainIconDisplay( JourneyListErrorIcon );
		setBusy( true );
	    }
	    unsetStates( QList<AppletState>() << ReceivedValidJourneyData << ReceivedErroneousJourneyData );
	    break;

	case ConfigDialogShown:
	case AccessorInfoDialogShown:
	case SettingsJustChanged:
	case ServiceProviderSettingsJustChanged:
	case NoState:
	    break;
    }

    m_appletStates |= state;
}

void PublicTransport::removeState( AppletState state ) {
    if ( !m_appletStates.testFlag(state) )
	return;

    switch ( state ) {
	case ShowingJourneyList:
	    setMainIconDisplay( m_appletStates.testFlag(ReceivedValidDepartureData) ? DepartureListOkIcon : DepartureListErrorIcon );
	    setDepartureArrivalListType( m_settings.departureArrivalListType() );
	    break;

	case ConfigDialogShown:
// 	    m_dataSourceTester->setTestSource("");
	    break;

	case SettingsJustChanged:
	case ServiceProviderSettingsJustChanged:
	case ShowingDepartureArrivalList:
	case ShowingJourneySearch:
	case WaitingForDepartureData:
	case ReceivedValidDepartureData:
	case ReceivedErroneousDepartureData:
	case WaitingForJourneyData:
	case ReceivedValidJourneyData:
	case ReceivedErroneousJourneyData:
	case AccessorInfoDialogShown:
	case NoState:
	    break;
    }

    m_appletStates ^= state;
}

void PublicTransport::hideHeader ( bool ) {
    QTreeView *treeView = m_treeView->nativeWidget();
    treeView->header()->setVisible( false );
    m_settings.setShowHeader( false );
}

void PublicTransport::showHeader ( bool ) {
    QTreeView *treeView = m_treeView->nativeWidget();
    treeView->header()->setVisible( true );
    m_settings.setShowHeader( true );
}

void PublicTransport::hideColumnTarget( bool ) {
    QTreeView *treeView = m_treeView->nativeWidget();
    treeView->hideColumn( 1 );
    treeView->header()->setStretchLastSection( true );
    m_settings.setHideColumnTarget( true );
}

void PublicTransport::showColumnTarget( bool ) {
    QTreeView *treeView = m_treeView->nativeWidget();
    treeView->showColumn( 1 );

    m_settings.setHideColumnTarget( false );
    geometryChanged();
}

void PublicTransport::toggleExpanded( bool ) {
    doubleClickedDepartureItem( m_clickedItemIndex );
}

void PublicTransport::doubleClickedDepartureItem( QModelIndex modelIndex ) {
    if( modelIndex.parent().isValid() )
	return; // Only expand top level items
//     qDebug() << "PublicTransport::doubleClickedDepartureItem";

    if ( testState(ShowingDepartureArrivalList) )
	modelIndex = m_model->index( modelIndex.row(), 0 );
    else
	modelIndex = m_modelJourneys->index( modelIndex.row(), 0 );

    QTreeView* treeView = m_treeView->nativeWidget();
    if ( treeView->isExpanded(modelIndex) )
	treeView->collapse( modelIndex );
    else
	treeView->expand( modelIndex );
}

QAction* PublicTransport::updatedAction ( const QString& actionName ) {
    QAction *a = action(actionName);
    if ( a == NULL ) {
	if ( actionName == "seperator" ) {
	    a = new QAction(this);
	    a->setSeparator(true);
	    return a;
	} else {
	    qDebug() << "PublicTransport::updatedAction" << "Action not found:" << actionName;
	    return NULL;
	}
    }

    QAbstractItemModel *model;
    if ( testState(ShowingDepartureArrivalList) )
	model = m_model;
    else
	model = m_modelJourneys;

    if ( actionName == "toggleExpanded" ) {
	if ( m_treeView->nativeWidget()->isExpanded( model->index(m_clickedItemIndex.row(), 0) ) ) {
	    a->setText( i18n("Hide additional &information") );
	    a->setIcon( KIcon("arrow-up") );
	} else {
	    a->setText( i18n("Show additional &information") );
	    a->setIcon( KIcon("arrow-down") );
	}
    } else if ( actionName == "removeAlarmForDeparture" ) {
	a->setText( m_settings.departureArrivalListType() == DepartureList ? i18n("Remove &alarm for this departure") : i18n("Remove &alarm for this arrival") );
    } else if ( actionName == "setAlarmForDeparture" ) {
	a->setText( m_settings.departureArrivalListType() == DepartureList ? i18n("Set &alarm for this departure") : i18n("Set &alarm for this arrival") );
    } else if ( actionName == "filterOutByVehicleType" ) {
	QStandardItem *itemDeparture = m_model->item( m_clickedItemIndex.row(), 2 );
	VehicleType vehicleType = static_cast<VehicleType>( itemDeparture->data( VehicleTypeRole ).toInt() );
	if ( vehicleType == Unknown ) {
	    a->setIcon( KIcon("view-filter") );
	    a->setText( i18n("H&ide unknown vehicle types") );
	} else {
	    a->setIcon( Global::makeOverlayIcon(KIcon("view-filter"), Global::iconFromVehicleType(vehicleType)) );
	    a->setText( i18n("H&ide %1", Global::vehicleTypeToString(vehicleType, true)) );
	}
    } else if ( actionName == "removeTargetFromFilterList" ) {
	if ( m_settings.filterTypeTarget() == ShowMatching )
	    a->setText( m_settings.departureArrivalListType() == DepartureList ? i18n("&Hide target") : i18n("&Hide origin") );
	else if ( m_settings.filterTypeTarget() == ShowAll )
	    a->setText( m_settings.departureArrivalListType() == DepartureList ? i18n("&Remove target from filter list") : i18n("&Remove origin from filter list") );
    } else if ( actionName == "setFilterListToHideMatching") {
	if ( m_settings.filterTargetList().isEmpty() )
	    a->setText( m_settings.departureArrivalListType() == DepartureList ? i18n("&Hide target") : i18n("&Hide origin") );
	else
	    a->setText( m_settings.departureArrivalListType() == DepartureList ? i18n("&Hide target and others in the filter list") : i18n("&Hide origin and others in the filter list" ) );
    } else if ( actionName == "removeTargetFromFilterList") {
	a->setText( m_settings.departureArrivalListType() == DepartureList ? i18n("&Remove target from filter list") : i18n("&Remove origin from filter list") );
    } else if ( actionName == "addTargetToFilterList") {
	a->setText( m_settings.departureArrivalListType() == DepartureList ? i18n("&Hide target") : i18n("&Hide origin") );
    } else if ( actionName == "addTargetToFilterListAndHide") {
	if ( m_settings.filterTargetList().isEmpty() )
	    a->setText( m_settings.departureArrivalListType() == DepartureList ? i18n("&Hide target") : i18n("&Hide origin") );
	else
	    a->setText( m_settings.departureArrivalListType() == DepartureList ? i18n("&Hide target and others in the filter list") : i18n("&Hide origin and others in the filter list") );
    }

    return a;
}

void PublicTransport::showHeaderContextMenu ( const QPoint& position ) {
    Q_UNUSED( position );
    QHeaderView *header = m_treeView->nativeWidget()->header();
    QList<QAction *> actions;

    if ( testState(ShowingDepartureArrivalList) ) {
	if ( header->logicalIndexAt(position) == 1 )
	    actions.append( action("hideColumnTarget") );
	else if ( header->isSectionHidden(1) )
	    actions.append( action("showColumnTarget") );
    }
    actions.append( action("hideHeader") );

    if ( actions.count() > 0 && view() )
	QMenu::exec( actions, QCursor::pos() );
}

void PublicTransport::showDepartureContextMenu ( const QPoint& position ) {
    QTreeView* treeView = m_treeView->nativeWidget();
    QList<QAction *> actions;

    if ( (m_clickedItemIndex = treeView->indexAt(position)).isValid() ) {
	while( m_clickedItemIndex.parent().isValid() )
	    m_clickedItemIndex = m_clickedItemIndex.parent();

	actions.append( updatedAction("toggleExpanded") );

	if ( testState(ShowingDepartureArrivalList) ) {
	    if ( m_model->item( m_clickedItemIndex.row(), 2 )->icon().isNull() )
		actions.append( updatedAction("setAlarmForDeparture") );
	    else
		actions.append( updatedAction("removeAlarmForDeparture") );
	}

	actions.append( updatedAction("seperator") );

	if ( testState(ShowingDepartureArrivalList) ) {
	    actions.append( updatedAction("filterOutByVehicleType") );
	    if ( !m_settings.filteredOutVehicleTypes().isEmpty() ) {
		actions.append( action("removeAllFiltersByVehicleType") );
		actions.append( updatedAction("seperator") );
	    }

	    QString sTarget = m_model->item( m_clickedItemIndex.row(), 1 )->text();
	    if ( m_settings.filterTargetList().contains( sTarget ) ) {
		if ( m_settings.filterTypeTarget() == ShowMatching ) {
		    actions.append( updatedAction("removeTargetFromFilterList") );
		    actions.append( action("setFilterListToShowAll") );
		} else if ( m_settings.filterTypeTarget() == ShowAll ) {
		    // Adding an already existing item, which is then removed because it's a duplicate
		    // Could add another QAction to only set to HideMatching
		    actions.append( updatedAction("setFilterListToHideMatching") );
		    actions.append( updatedAction("removeTargetFromFilterList") );
		}// never m_filterTypeTarget == HideMatching => journeys with target/origin in filter list won't be shown
	    } else { // Target isn't contained in the filter list
		if ( m_settings.filterTypeTarget() == HideMatching ) {
		    actions.append( updatedAction("addTargetToFilterList") );
		    actions.append( action("setFilterListToShowAll") );
		} else if ( m_settings.filterTypeTarget() == ShowAll ) {
		    actions.append( updatedAction("addTargetToFilterListAndHide") );
		} // never m_filterTypeTarget == ShowMatching => journeys with target/origin not in filter list won't be shown
	    }
	}

	if ( !treeView->header()->isVisible() ) {
	    actions.append( updatedAction("seperator") );
	    actions.append( action("showHeader") );
	} else if ( treeView->header()->isSectionHidden(1) ) {
	    if ( testState(ShowingDepartureArrivalList) ) {
		actions.append( updatedAction("seperator") );
		actions.append( action("showColumnTarget") );
	    }
	}
    } else { // No item
	if ( testState(ShowingDepartureArrivalList) ) {
	    if ( !m_settings.filteredOutVehicleTypes().isEmpty() )
		actions.append( action("removeAllFiltersByVehicleType") );
	    if ( m_settings.filterTypeTarget() != ShowAll )
		actions.append( action("setFilterListToShowAll") );
	}
	if ( !treeView->header()->isVisible() )
	    actions.append( action("showHeader") );
    }

    if ( actions.count() > 0 && view() )
	QMenu::exec( actions, QCursor::pos() );
}

void PublicTransport::filterOutByVehicleType( bool ) {
    QStandardItem *itemDeparture = m_model->item( m_clickedItemIndex.row(), 2 );
    VehicleType vehicleType = static_cast<VehicleType>( itemDeparture->data( VehicleTypeRole ).toInt() );
    m_settings.hideTypeOfVehicle( vehicleType );

    // TODO: To PublicTransportSettings
    KConfigGroup cg = config();
    cg.writeEntry( PublicTransportSettings::vehicleTypeToConfigName(vehicleType), false );
    //     emit settingsChanged();
    emit configNeedsSaving();
    updateModel(); // apply new filter settings
}

void PublicTransport::removeAllFiltersByVehicleType ( bool ) {
    m_settings.removeAllFiltersByVehicleType();
}

void PublicTransport::addTargetToFilterList( bool ) {
    QString target = m_model->item( m_clickedItemIndex.row(), 1 )->text();
    m_settings.filterTargetList() << target;
//     if ( m_isConfigDialogShown )
// 	m_uiFilter.filterTargetList->items() << target;

    KConfigGroup cg = config();
    cg.writeEntry("filterTargetList", m_settings.filterTargetList());
//     emit settingsChanged();
    emit configNeedsSaving();
    updateModel(); // apply new filter settings
}

void PublicTransport::removeTargetFromFilterList( bool ) {
    QString target = m_model->item( m_clickedItemIndex.row(), 1 )->text();
    m_settings.filterTargetList().removeOne( target );
//     if ( m_isConfigDialogShown )
// 	m_uiFilter.filterTargetList->items().removeOne( target );

    // TODO: to PublicTransportSettings
    KConfigGroup cg = config();
    cg.writeEntry("filterTargetList", m_settings.filterTargetList());
    cg.writeEntry("filterTypeTarget", static_cast<int>(m_settings.filterTypeTarget()));
    //     emit settingsChanged();
    emit configNeedsSaving();
    updateModel(); // apply new filter settings
}

void PublicTransport::setFilterListToShowAll( bool ) {
    m_settings.setFilterTypeTarget( ShowAll );
    updateModel(); // apply new filter settings

//     if ( testState(ConfigDialogShown) )
// 	m_uiFilter.filterTypeTarget->setCurrentIndex( static_cast<int>(m_filterTypeTarget) );
}

void PublicTransport::setFilterListToHideMatching ( bool ) {
    m_settings.setFilterTypeTarget( HideMatching );
    updateModel(); // apply new filter settings

//     if ( testState(ConfigDialogShown) )
// 	m_uiFilter.filterTypeTarget->setCurrentIndex( static_cast<int>(m_filterTypeTarget) );
}

void PublicTransport::addTargetToFilterListAndHide( bool b ) {
    m_settings.setFilterTypeTarget( HideMatching );
    updateModel(); // apply new filter settings

//     if ( testState(ConfigDialogShown) )
// 	m_uiFilter.filterTypeTarget->setCurrentIndex( static_cast<int>(m_filterTypeTarget) );

    addTargetToFilterList( b );
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
	int secsTo = QDateTime::currentDateTime().secsTo( predictedDeparture.addSecs(-m_settings.alarmTime() * 60) );
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
	    message = i18ncp("%2: Line string (e.g. 'U3'), %4: Vehicle type name (e.g. tram, subway)", "The %4 %2 to '%3' departs in %1 minute at %5", "The %4 %2 to '%3' departs in %1 minutes at %5", minsToDeparture, sLine, sTarget, Global::vehicleTypeToString(vehicleType), predictedDeparture.toString("hh:mm"));
    }
    else if ( minsToDeparture < 0 ) {
	if ( vehicleType == Unknown )
	    message = i18np("Line %2 to '%3' has departed %1 minute ago at %4", "Line %2 to '%3' has departed %1 minutes ago at %4", -minsToDeparture, sLine, sTarget, predictedDeparture.toString("hh:mm"));
	else
	    message = i18ncp("%2: Line string (e.g. 'U3'), %4: Vehicle type name (e.g. tram, subway)", "The %4 %2 to '%3' has departed %1 minute ago at %5", "The %4 %2 to %3 has departed %1 minutes ago at %5", -minsToDeparture, sLine, sTarget, Global::vehicleTypeToString(vehicleType), predictedDeparture.toString("hh:mm"));
    }
    else {
	if ( vehicleType == Unknown )
	    message = i18n("Line %1 to '%2' departs now at %3", sLine, sTarget, predictedDeparture.toString("hh:mm"));
	else
	    message = i18nc("%2: Line string (e.g. 'U3'), %4: Vehicle type name (e.g. tram, subway)", "The %3 %1 to '%2' departs now at %3", sLine, sTarget, Global::vehicleTypeToString(vehicleType), predictedDeparture.toString("hh:mm"));
    }

    KNotification::event( KNotification::Warning, message, KIcon("public-transport-stop").pixmap(16), 0L, KNotification::Persistent );

//     QMessageBox box( QMessageBox::Information, "Public transport: Alarm", message, QMessageBox::Ok );
//     box.setWindowIcon( KIcon("public-transport-stop") );
//     box.exec();
}

bool PublicTransport::filterOut ( const DepartureInfo &departureInfo ) const {
//     qDebug() << "filterOut?" << (( !m_settings.isTypeOfVehicleShown(departureInfo.vehicleType)) ||
//     (departureInfo.isNightLine() && !m_settings.showNightlines()) ||
//     (departureInfo.isLineNumberValid() && !departureInfo.isLineNumberInRange( m_settings.filterMinLine(), m_settings.filterMaxLine() )) ||
//     (m_settings.filterTypeTarget() == ShowMatching && !m_settings.filterTargetList().contains(departureInfo.target)) ||
//     (m_settings.filterTypeTarget() == HideMatching && m_settings.filterTargetList().contains(departureInfo.target)) ||
//     QDateTime::currentDateTime().secsTo( departureInfo.predictedDeparture() ) < -60) << departureInfo.lineString << departureInfo.departure.toString("hh:mm");

    return ( !m_settings.isTypeOfVehicleShown(departureInfo.vehicleType)) ||
	(departureInfo.isNightLine() && !m_settings.showNightlines()) ||
	(departureInfo.isLineNumberValid() && !departureInfo.isLineNumberInRange( m_settings.filterMinLine(), m_settings.filterMaxLine() )) ||
	(m_settings.filterTypeTarget() == ShowMatching && !m_settings.filterTargetList().contains(departureInfo.target)) ||
	(m_settings.filterTypeTarget() == HideMatching && m_settings.filterTargetList().contains(departureInfo.target)) ||
	QDateTime::currentDateTime().secsTo( departureInfo.predictedDeparture() ) < -60;
}

QHash<QString, QVariant> PublicTransport::serviceProviderData() const {
    QString sServiceProvider;
    Plasma::DataEngine::Data data = dataEngine("publictransport")->query("ServiceProviders");
    foreach ( QString serviceProviderName, data.keys() )
    {
	QHash< QString, QVariant > serviceProviderData = data.value(serviceProviderName).toHash();
	if ( serviceProviderData["id"].toString() == m_settings.serviceProvider() )
	    return serviceProviderData;
    }

    kDebug() << "Name not found for" << m_settings.serviceProvider();
    return QHash<QString,QVariant>();
}

QString PublicTransport::titleText() const {
    QString sServiceProvider = serviceProviderData()["shortUrl"].toString();
    if ( m_settings.useSeperateCityValue() )
	return QString("<b>%1, %2</b>").arg( m_settings.stop() ).arg( m_settings.city() );
    else
	return QString("<b>%1</b>").arg( m_settings.stop() );
}

QString PublicTransport::infoText() const {
    QString sServiceProvider = serviceProviderData()["shortUrl"].toString();
    return QString("<small>last update: %1<br>data by: <a href='http://www.%2'>%2</a></small>").arg( m_lastSourceUpdate.toString("hh:mm") ).arg( sServiceProvider );
}

QString PublicTransport::departureText( const JourneyInfo& journeyInfo ) const {
    QString sTime, sDeparture = journeyInfo.departure.toString("hh:mm");
    if ( m_settings.displayTimeBold() )
	sDeparture = sDeparture.prepend("<span style='font-weight:bold;'>").append("</span>");

    if ( m_settings.isDepartureTimeShown() && m_settings.isRemainingMinutesShown() ) {
	QString sText = journeyInfo.durationToDepartureString();
	sText = sText.replace(QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");

	if ( m_settings.linesPerRow() > 1 )
	    sTime = QString("%1<br>(%2)").arg( sDeparture ).arg( sText );
	else
	    sTime = QString("%1 (%2)").arg( sDeparture ).arg( sText );
    } else if (m_settings.isDepartureTimeShown()) {
	sTime = sDeparture;
    } else if (m_settings.isRemainingMinutesShown()) {
	sTime = journeyInfo.durationToDepartureString();
	sTime = sTime.replace(QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");
    } else
	sTime = "";

    return sTime;
}

QString PublicTransport::arrivalText( const JourneyInfo& journeyInfo ) const {
    QString sTime, sDeparture = journeyInfo.arrival.toString("hh:mm");
    if ( m_settings.displayTimeBold() )
	sDeparture = sDeparture.prepend("<span style='font-weight:bold;'>").append("</span>");

    if ( m_settings.isDepartureTimeShown() && m_settings.isRemainingMinutesShown() ) {
	QString sText = journeyInfo.durationToDepartureString(true);
	sText = sText.replace(QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");

	if ( m_settings.linesPerRow() > 1 )
	    sTime = QString("%1<br>(%2)").arg( sDeparture ).arg( sText );
	else
	    sTime = QString("%1 (%2)").arg( sDeparture ).arg( sText );
    } else if (m_settings.isDepartureTimeShown()) {
	sTime = sDeparture;
    } else if (m_settings.isRemainingMinutesShown()) {
	sTime = journeyInfo.durationToDepartureString(true);
	sTime = sTime.replace(QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");
    } else
	sTime = "";

    return sTime;
}

QString PublicTransport::departureText( const DepartureInfo &departureInfo ) const {
    QString sTime, sDeparture = departureInfo.predictedDeparture().toString("hh:mm");
    QString sColor;
    if ( departureInfo.delayType() == OnSchedule )
	sColor = "color:darkgreen;"; // TODO: works good with Air-Theme, but too dark for dark themes
    else if ( departureInfo.delayType() == Delayed )
	sColor = "color:darkred;"; // TODO: works good with Air-Theme, but is too dark for dark themes

    if ( m_settings.displayTimeBold() )
	sDeparture = sDeparture.prepend(QString("<span style='font-weight:bold;%1'>").arg(sColor)).append("</span>");

    if (m_settings.isDepartureTimeShown() && m_settings.isRemainingMinutesShown()) {
	QString sText = departureInfo.durationString();
	sText = sText.replace(QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");

	if ( m_settings.linesPerRow() > 1 )
	    sTime = QString("%1<br>(%2)").arg( sDeparture ).arg( sText );
	else
	    sTime = QString("%1 (%2)").arg( sDeparture ).arg( sText );
    } else if (m_settings.isDepartureTimeShown()) {
	sTime = sDeparture;
	if ( departureInfo.delayType() == Delayed ) {
	    QString sText = i18np("+ %1 minute", "+ %1 minutes", departureInfo.delay );
	    sText = sText.prepend(" (").append(")");
	    sText = sText.replace(QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");
	    sTime += sText;
	}
    } else if (m_settings.isRemainingMinutesShown()) {
	sTime = departureInfo.durationString();
	sTime = sTime.replace(QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");
    } else
	sTime = "";

    return sTime;
}

void PublicTransport::setTextColorOfHtmlItem ( QStandardItem *item, QColor textColor ) {
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

	QString operatorName = m_model->item( row, 0 )->data( OperatorRole ).toString();
	if ( operatorName != departureInfo.operatorName ) {
// 	    qDebug() << "Operator Test failed" << operatorName << departureInfo.operatorName;
	    continue;
	}

// 	qDebug() << "Found" << row << departureInfo.lineString << departureInfo.target << departureInfo.departure.toString("hh:mm");;
	return row;
    }

//     if ( m_model->rowCount() > 0 )
// 	qDebug() << "Not found" << departureInfo.lineString << departureInfo.target << departureInfo.departure.toString("hh:mm");;
    return -1; // Departure not found
}

int PublicTransport::findJourney( const JourneyInfo& journeyInfo ) const {
    //     qDebug() << "PublicTransport::findDeparture" << m_model->rowCount() << "rows" << departureInfo.lineString << departureInfo.target << departureInfo.departure.toString("hh:mm");
    for ( int row = 0; row < m_modelJourneys->rowCount(); ++row )
    {
	// TODO, item( row, JOURNEYS_COLUMN_DEPARTURE )
	QDateTime departure = m_modelJourneys->item( row, 1 )->data( SortRole ).toDateTime();
	if ( departure != journeyInfo.departure ) {
// 	    qDebug() << "Departure Test failed" << departure << journeyInfo.departure;
	    continue;
	}

	// TODO, item( row, JOURNEYS_COLUMN_ARRIVAL )
	QDateTime arrival = m_modelJourneys->item( row, 2 )->data( SortRole ).toDateTime();
	if ( arrival != journeyInfo.arrival ) {
	    continue;
	}

	// TODO, item( row, JOURNEYS_COLUMN_CHANGES )
	int changes = m_modelJourneys->item( row, 3 )->data( SortRole ).toInt();
	if ( changes != journeyInfo.changes ) {
	    continue;
	}

	QString operatorName = m_modelJourneys->item( row, 0 )->data( OperatorRole ).toString();
	if ( operatorName != journeyInfo.operatorName ) {
	    continue;
	}

	// 	qDebug() << "Found" << row << departureInfo.lineString << departureInfo.target << departureInfo.departure.toString("hh:mm");;
	return row;
    }

    //     if ( m_model->rowCount() > 0 )
    // 	qDebug() << "Not found" << departureInfo.lineString << departureInfo.target << departureInfo.departure.toString("hh:mm");;
    return -1; // Departure not found
}

// TODO: move to DepartureInfo
QString PublicTransport::delayText( const DepartureInfo& departureInfo ) const {
    QString sText;
    switch ( departureInfo.delayType() )
    {
	case OnSchedule:
	    sText = i18nc("A public transport vehicle departs on schedule", "On schedule");
	    sText = sText.prepend("<span style='color:green;'>").append("</span>");
// 	    if ( *itemDelayInfo != NULL ) {
// 		*itemDelayInfo = new QStandardItem(sText);
// 		*itemDelay = new QStandardItem("");
// 	    } else {
// 		(*itemDelayInfo)->setText(sText);
// 		(*itemDelay)->setText("");
// 	    }
	    break;
	case Delayed:
	    sText = i18np("+%1 minute", "+%1 minutes", departureInfo.delay);
	    sText = sText.replace(QRegExp("(+?\\s*\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");

	    if ( !departureInfo.delayReason.isEmpty() ) {
		sText += ", " + departureInfo.delayReason;
	    }
// 	    if ( *itemDelayInfo != NULL ) {
// 		*itemDelayInfo = new QStandardItem(departureInfo.delayReason);
// 		*itemDelay = new QStandardItem(sText);
// 	    } else {
// 		(*itemDelayInfo)->setText(departureInfo.delayReason);
// 		(*itemDelay)->setText(sText);
// 	    }
// 	    if ( !departureInfo.delayReason.isEmpty() ) {
// 		(*itemDelayInfo)->setData( departureInfo.delayReason.length() / 25 + (departureInfo.delayReason.length() % 25 > 0 ? 1 : 0), HtmlDelegate::LinesPerRowRole ); // 25 chars per line
// 	    }
	    break;

	case DelayUnknown:
	default:
	    sText = i18n("No information available");
// 	    if ( *itemDelayInfo != NULL ) {
// 		*itemDelayInfo = new QStandardItem(i18n("No information available"));
// 		*itemDelay = new QStandardItem("");
// 	    } else {
// 		(*itemDelayInfo)->setText(i18n("No information available"));
// 		(*itemDelay)->setText("");
// 	    }
	    break;
    }

    return sText;
//     setTextColorOfHtmlItem( *itemDelayInfo, m_colorSubItemLabels );
//     setTextColorOfHtmlItem( *itemDelay, m_colorSubItemLabels );
}

// TODO
void PublicTransport::setValuesOfJourneyItem ( QStandardItem* journeyItem, JourneyInfo journeyInfo, ItemInformation journeyInformation, bool update ) {
    QStringList sList;
    QString s;
    switch ( journeyInformation ) {
	case VehicleTypeListItem:
// 	    foreach( VehicleType vehicleType, journeyInfo.vehicleTypes )
// 		slist << Global::vehicleTypeToString(vehicleType);
// 	    journeyItem->setText( slist.join(", ") );
	    journeyItem->setIcon( Global::iconFromVehicleTypeList(journeyInfo.vehicleTypes) );
	    journeyItem->setData( journeyItem->text(), SortRole );
	    journeyItem->setData( journeyInfo.operatorName, OperatorRole );
	    journeyItem->setData( m_settings.linesPerRow(), HtmlDelegate::LinesPerRowRole );
	    if ( !update )
		journeyItem->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    break;

	case JourneyInfoItem:
	    s = QString( i18n("<b>From:</b> %1<br><b>To:</b> %2") ).arg( journeyInfo.startStopName ).arg( journeyInfo.targetStopName );
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    journeyItem->setData( journeyItem->text(), SortRole );
	    journeyItem->setData( m_settings.linesPerRow(), HtmlDelegate::LinesPerRowRole );
	    if ( !update )
		journeyItem->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    break;

	case DepartureItem:
	    journeyItem->setData( s = departureText(journeyInfo), HtmlDelegate::FormattedTextRole );
	    if ( m_settings.linesPerRow() > 1 ) {
		// Get longest line
		sList = s.split("<br>", QString::SkipEmptyParts, Qt::CaseInsensitive);
		s = "";
		foreach( QString sCurrent, sList ) {
		    sCurrent.replace(QRegExp("(<[^>]*>|&\\w{2,5};|&#\\d{3,4};)"), "");
		    if ( sCurrent.length() > s.length() )
			s = sCurrent;
		}
		journeyItem->setText( s );
	    } else
		journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    journeyItem->setData( journeyInfo.departure, SortRole );
	    journeyItem->setData( m_settings.linesPerRow(), HtmlDelegate::LinesPerRowRole );
	    journeyItem->setData( qCeil((float)QDateTime::currentDateTime().secsTo( journeyInfo.departure ) / 60.0f), RemainingMinutesRole );
	    journeyItem->setData( journeyInfo.vehicleTypesVariant(), VehicleTypeListRole );
	    if ( !update ) {
		journeyItem->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case ArrivalItem:
	    journeyItem->setData( s = arrivalText(journeyInfo), HtmlDelegate::FormattedTextRole );
	    if ( m_settings.linesPerRow() > 1 ) {
		// Get longest line
		sList = s.split("<br>", QString::SkipEmptyParts, Qt::CaseInsensitive);
		s = "";
		foreach( QString sCurrent, sList ) {
		    sCurrent.replace(QRegExp("(<[^>]*>|&\\w{2,5};|&#\\d{3,4};)"), "");
		    if ( sCurrent.length() > s.length() )
			s = sCurrent;
		}
		journeyItem->setText( s );
	    } else
		journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    journeyItem->setData( journeyInfo.arrival, SortRole );
	    journeyItem->setData( m_settings.linesPerRow(), HtmlDelegate::LinesPerRowRole );
	    journeyItem->setData( qCeil((float)QDateTime::currentDateTime().secsTo( journeyInfo.arrival ) / 60.0f), RemainingMinutesRole );
	    journeyItem->setData( journeyInfo.vehicleTypesVariant(), VehicleTypeListRole );
	    if ( !update ) {
		journeyItem->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case StartStopNameItem:
	    journeyItem->setText( journeyInfo.startStopName );
	    journeyItem->setData( journeyInfo.startStopName, SortRole );
// 	    journeyItem->setData( m_settings.linesPerRow(), HtmlDelegate::LinesPerRowRole );
// 	    if ( !update ) {
// 		journeyItem->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
// 	    }
	    break;

	case TargetStopNameItem:
	    journeyItem->setText( journeyInfo.targetStopName );
	    journeyItem->setData( journeyInfo.targetStopName, SortRole );
// 	    journeyItem->setData( m_settings.linesPerRow(), HtmlDelegate::LinesPerRowRole );
	    break;

	case DurationItem:
	    if ( journeyInfo.duration <= 0 )
		s = QString( "<b>%1</b> %2" ).arg( i18nc("The duration of a journey", "Duration:") ).arg( 0 );
	    else
		s = QString( "<b>%1</b> %2" ).arg( i18nc("The duration of a journey", "Duration:") ).arg( Global::durationString(journeyInfo.duration * 60) );
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    journeyItem->setData( journeyInfo.duration, SortRole );
// 	    if ( !update ) {
// 		journeyItem->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
// 	    }
	    break;

	case ChangesItem:
	    s = QString("<b>%1</b> %2").arg( i18nc("The changes of a journey", "Changes:") ).arg(journeyInfo.changes);
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    journeyItem->setData( journeyInfo.changes, SortRole );
	    break;

	case PricingItem:
	    s = QString("<b>%1</b> %2").arg( i18nc("The pricing of a journey", "Pricing:") ).arg( journeyInfo.pricing );
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s );
	    journeyItem->setData( journeyInfo.pricing, SortRole );
	    break;

	case OperatorItem:
	    s = QString("<b>%1</b> %2").arg( i18nc("The company that is responsible for this departure/arrival/journey", "Operator:") ).arg( journeyInfo.operatorName );
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    if ( !update ) {
		journeyItem->setData( 4, SortRole );
		setTextColorOfHtmlItem( journeyItem, m_colorSubItemLabels );
	    }
	    break;

	default:
	    break;
    }
}

void PublicTransport::setValuesOfDepartureItem ( QStandardItem* departureItem, DepartureInfo departureInfo, ItemInformation departureInformation, bool update ) {
    QString s, s2;
    QStringList sList;
    switch ( departureInformation ) {
	case LineNameItem:
	    departureItem->setText( departureInfo.lineString );
	    departureItem->setData( QString("<span style='font-weight:bold;'>%1</span>").arg(departureInfo.lineString), HtmlDelegate::FormattedTextRole );
	    departureItem->setData( departureInfo.lineString, SortRole );
	    departureItem->setData( departureInfo.operatorName, OperatorRole );
	    departureItem->setData( m_settings.linesPerRow(), HtmlDelegate::LinesPerRowRole );
// 	    departureItem->setData( QVariant::fromValue<DepartureInfo>(departureInfo), DepartureInfoRole );
	    if ( departureInfo.vehicleType != Unknown )
		departureItem->setIcon( Global::iconFromVehicleType(departureInfo.vehicleType) );
	    if ( !update ) {
		departureItem->setTextAlignment( Qt::AlignRight );
		departureItem->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case TargetItem:
	    departureItem->setText( departureInfo.target );
	    departureItem->setData( departureInfo.target, SortRole );
	    departureItem->setData( m_settings.linesPerRow(), HtmlDelegate::LinesPerRowRole );
	    if ( !departureInfo.journeyNews.isEmpty() ) {
		departureItem->setIcon( Global::makeOverlayIcon(KIcon("view-pim-news"), "arrow-down", QSize(12,12)) );
		departureItem->setData( static_cast<int>(HtmlDelegate::Right), HtmlDelegate::DecorationPositionRole );
	    }
	    if ( !update ) {
		departureItem->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case DepartureItem:
	    departureItem->setData( s = departureText(departureInfo), HtmlDelegate::FormattedTextRole );
	    if ( m_settings.linesPerRow() > 1 ) {
		// Get longest line
		sList = s.split("<br>", QString::SkipEmptyParts, Qt::CaseInsensitive);

		s = "";
		foreach( QString sCurrent, sList ) {
		    sCurrent.replace(QRegExp("(<[^>]*>|&\\w{2,5};|&#\\d{3,4};)"), "");
		    if ( sCurrent.length() > s.length() )
			s = sCurrent;
		}
		departureItem->setText( s );
	    } else
		departureItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    departureItem->setData( departureInfo.predictedDeparture(), SortRole ); // TODO: Could make findDeparture not working, when the delay has changed. Maybe change to departure with seperate TimeRole..
	    departureItem->setData( m_settings.linesPerRow(), HtmlDelegate::LinesPerRowRole );
	    departureItem->setData( qCeil((float)QDateTime::currentDateTime().secsTo( departureInfo.predictedDeparture() ) / 60.0f), RemainingMinutesRole );
	    departureItem->setData( static_cast<int>(departureInfo.vehicleType), VehicleTypeRole );
	    if ( !update ) {
		departureItem->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case PlatformItem:
	    s = QString("<b>%1</b> %2").arg( i18nc("The platform from which a tram/bus/train departs", "Platform:") ).arg( departureInfo.platform );
	    departureItem->setData( s, HtmlDelegate::FormattedTextRole );
	    departureItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    if ( !update ) {
		departureItem->setData( 1, SortRole ); // TODO 1 => enum
		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;

	case OperatorItem:
	    s = QString("<b>%1</b> %2").arg( i18nc("The company that is responsible for this departure/arrival/journey", "Operator:") ).arg( departureInfo.operatorName );
	    departureItem->setData( s, HtmlDelegate::FormattedTextRole );
	    departureItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    if ( !update ) {
		departureItem->setData( 4, SortRole );
		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;

	case JourneyNewsItem:
	    s2 = departureInfo.journeyNews;
	    if ( s2.startsWith("http://") ) // TODO: Make the link clickable...
		s2 = QString("<a href='%1'>%2</a>").arg(s2).arg(i18n("Link to journey news"));
	    s = QString("<b>%1</b> %2").arg( i18nc("News for a journey with public transport, like 'platform changed'", "News:") ).arg( s2 );
	    departureItem->setData( s, HtmlDelegate::FormattedTextRole );
	    departureItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    if ( !update ) {
		departureItem->setData( 2, SortRole );
		departureItem->setData( 3, HtmlDelegate::LinesPerRowRole ); // 3 lines for journey news
		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;

	case DelayItem:
	    s = QString("<b>%1</b> %2").arg( i18nc("Information about delays of a journey with public transport", "Delay:") ).arg( delayText(departureInfo) );
	    if ( departureInfo.delayType() == Delayed ) {
		s += "<br><b>" + (m_settings.departureArrivalListType() == ArrivalList ? i18n("Original arrival time:") : i18n("Original departure time:")) + "</b> " + departureInfo.departure.toString("hh:mm");
		departureItem->setData( 2, HtmlDelegate::LinesPerRowRole );
	    }

	    departureItem->setData( s, HtmlDelegate::FormattedTextRole );
	    departureItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    if ( !update ) {
		departureItem->setData( 0, SortRole );
		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;

	default:
	    break;
    }
}

void PublicTransport::appendJourney ( const JourneyInfo& journeyInfo ) {
    qDebug() << "PublicTransport::appendJourney";

    QList<QStandardItem*> items = QList<QStandardItem*>();
    foreach( TimetableColumn column, m_journeyViewColumns ) {
	QStandardItem *item = new QStandardItem();
	switch( column ) {
	    case VehicleTypeListColumn:
		setValuesOfJourneyItem( item, journeyInfo, VehicleTypeListItem );
		break;
	    case JourneyInfoColumn:
		setValuesOfJourneyItem( item, journeyInfo, JourneyInfoItem );
		break;
	    case DepartureColumn:
		setValuesOfJourneyItem( item, journeyInfo, DepartureItem );
		break;
	    case ArrivalColumn:
		setValuesOfJourneyItem( item, journeyInfo, ArrivalItem );
		break;

	    default:
		qDebug() << "not included" << column;
		break;
	}

	items << item;
    }
    m_modelJourneys->appendRow( items );

    // Search if an abandoned alarm timer matches
//     for( int i = m_abandonedAlarmTimer.count() - 1; i >= 0; --i ) {
// 	AlarmTimer* alarmTimer = m_abandonedAlarmTimer[i];
// 	setAlarmForDeparture( itemDeparture->index(), alarmTimer );
// 	m_abandonedAlarmTimer.removeAt(i);
//     }

    int iRow = 0;
    if ( journeyInfo.changes >= 0 ) {
	QStandardItem *itemChanges = new QStandardItem();
	setValuesOfJourneyItem( itemChanges, journeyInfo, ChangesItem );
	items[0]->insertRow( iRow++, itemChanges );
    }

    if ( !journeyInfo.pricing.isEmpty() ) {
	QStandardItem *itemPricing = new QStandardItem();
	setValuesOfJourneyItem( itemPricing, journeyInfo, PricingItem );
	items[0]->insertRow( iRow++, itemPricing );
    }

    if ( journeyInfo.duration > 0 ) {
	QStandardItem *itemDuration = new QStandardItem();
	setValuesOfJourneyItem( itemDuration, journeyInfo, DurationItem );
	items[0]->insertRow( iRow++, itemDuration );
    }

    if ( !journeyInfo.operatorName.isEmpty() ) {
	QStandardItem *itemOperatorName = new QStandardItem();
	setValuesOfJourneyItem( itemOperatorName, journeyInfo, OperatorItem );
	items[0]->insertRow( iRow++, itemOperatorName );
    }

    for ( int row = 0; row < items[0]->rowCount(); ++row )
	m_treeView->nativeWidget()->setFirstColumnSpanned(row, items[0]->index(), true);
}

void PublicTransport::appendDeparture ( const DepartureInfo& departureInfo ) {
    QList<QStandardItem*> items = QList<QStandardItem*>();
    foreach( TimetableColumn column, m_departureViewColumns ) {
	QStandardItem *item = new QStandardItem();
	switch( column ) {
	    case LineStringColumn:
		setValuesOfDepartureItem( item, departureInfo, LineNameItem );
		break;
	    case TargetColumn:
		setValuesOfDepartureItem( item, departureInfo, TargetItem );
		break;
	    case DepartureColumn:
		setValuesOfDepartureItem( item, departureInfo, DepartureItem );

		// TODO: check this!
		// Search if an abandoned alarm timer matches
		for( int i = m_abandonedAlarmTimer.count() - 1; i >= 0; --i ) {
		    AlarmTimer* alarmTimer = m_abandonedAlarmTimer[i];
		    setAlarmForDeparture( item->index(), alarmTimer );
		    m_abandonedAlarmTimer.removeAt(i);
		}
		break;

	    default:
		break;
	}

	items << item;
    }
    m_model->appendRow( items );

    int iRow = 0;
    if ( !departureInfo.platform.isEmpty() ) {
	QStandardItem *itemPlatform = new QStandardItem();
	setValuesOfDepartureItem( itemPlatform, departureInfo, PlatformItem );
	items[0]->insertRow( iRow++, itemPlatform );
    }

    if ( !departureInfo.journeyNews.isEmpty() ) {
	QStandardItem *itemJourneyNews = new QStandardItem();
	setValuesOfDepartureItem( itemJourneyNews, departureInfo, JourneyNewsItem );
	items[0]->insertRow( iRow++, itemJourneyNews );
    }

    if ( !departureInfo.operatorName.isEmpty() ) {
	QStandardItem *itemOperatorName = new QStandardItem();
	setValuesOfDepartureItem( itemOperatorName, departureInfo, OperatorItem );
	items[0]->insertRow( iRow++, itemOperatorName );
    }

    QStandardItem *itemDelay = new QStandardItem();;
    setValuesOfDepartureItem( itemDelay, departureInfo, DelayItem ); // TODO: set sort data = 0
    items[0]->insertRow( iRow++, itemDelay );

    for ( int row = 0; row < items[0]->rowCount(); ++row )
	m_treeView->nativeWidget()->setFirstColumnSpanned(row, items[0]->index(), true);
}

void PublicTransport::updateJourney ( int row, const JourneyInfo& journeyInfo ) {
    QStandardItem *itemFirst = m_modelJourneys->item( row, 0 );
    QStandardItem *itemVehicleTypes = m_modelJourneys->item( row, m_journeyViewColumns.indexOf(VehicleTypeListColumn) );
    QStandardItem *itemJourneyInfo = m_modelJourneys->item( row, m_journeyViewColumns.indexOf(JourneyInfoColumn) );
    QStandardItem *itemDeparture = m_modelJourneys->item( row, m_journeyViewColumns.indexOf(DepartureColumn) );
    QStandardItem *itemArrival = m_modelJourneys->item( row, m_journeyViewColumns.indexOf(ArrivalColumn) );
    setValuesOfJourneyItem( itemVehicleTypes, journeyInfo, VehicleTypeListItem, true );
    setValuesOfJourneyItem( itemJourneyInfo, journeyInfo, JourneyInfoItem, true );
    setValuesOfJourneyItem( itemDeparture, journeyInfo, DepartureItem, true );
    setValuesOfJourneyItem( itemArrival, journeyInfo, ArrivalItem, true );

    // Update changes
    int iRow = itemFirst->rowCount();
    QStandardItem *itemChanges = NULL;
    for ( int i = 0; i < itemFirst->rowCount(); ++i ) {
	if ( itemFirst->child( i )->data( SortRole ).toInt() == 1 ) { // TODO == 1   =>   "== enum"
	    itemChanges = itemFirst->child( i, 1 );
	    break;
	}
    }
    if ( journeyInfo.changes > 0 ) {
	if ( itemChanges == NULL ) { // Create new platform item
	    itemChanges = new QStandardItem();
	    setValuesOfJourneyItem( itemChanges, journeyInfo, ChangesItem );
	    itemFirst->insertRow( iRow++, itemChanges );
	} else // Update old platform item
	    setValuesOfJourneyItem( itemChanges, journeyInfo, ChangesItem, true );
    } else if ( itemChanges != NULL ) {
	itemFirst->removeRow( itemChanges->row() );
	--iRow;
    }

    // Update pricing 2
    QStandardItem *itemPricing = NULL;
    for ( int i = 0; i < itemFirst->rowCount(); ++i ) {
	if ( itemFirst->child( i )->data( SortRole ).toInt() == 2 ) { // TODO == 2   =>   "== enum"
	    itemPricing = itemFirst->child( i, 1 );
	    break;
	}
    }
    if ( !journeyInfo.pricing.isEmpty() ) {
	if ( itemPricing == NULL ) { // Create new platform item
	    itemPricing = new QStandardItem();
	    setValuesOfJourneyItem( itemPricing, journeyInfo, PricingItem );
	    itemFirst->insertRow( iRow++, itemPricing );
	} else // Update old platform item
	    setValuesOfJourneyItem( itemPricing, journeyInfo, PricingItem, true );
    } else if ( itemPricing != NULL ) {
	itemFirst->removeRow( itemPricing->row() );
	--iRow;
    }

    // Update duration 3
    QStandardItem *itemDuration = NULL;
    for ( int i = 0; i < itemFirst->rowCount(); ++i ) {
	if ( itemFirst->child( i )->data( SortRole ).toInt() == 3 ) { // TODO == 3   =>   "== enum"
	    itemDuration = itemFirst->child( i, 1 );
	    break;
	}
    }
    if ( !journeyInfo.pricing.isEmpty() ) {
	if ( itemDuration == NULL ) { // Create new platform item
	    itemDuration = new QStandardItem();
	    setValuesOfJourneyItem( itemDuration, journeyInfo, DurationItem );
	    itemFirst->insertRow( iRow++, itemDuration );
	} else // Update old platform item
	    setValuesOfJourneyItem( itemDuration, journeyInfo, DurationItem, true );
    } else if ( itemDuration != NULL ) {
	itemFirst->removeRow( itemDuration->row() );
	--iRow;
    }

    // Update operator name
    QStandardItem *itemOperatorName = NULL;
    for ( int i = 0; i < itemFirst->rowCount(); ++i ) {
	if ( itemFirst->child( i )->data( Qt::UserRole ).toInt() == 4 ) { // TODO == 4   =>   "== enum"
	    itemOperatorName = itemFirst->child( i );
	    break;
	}
    }
    if ( !journeyInfo.operatorName.isEmpty() ) {
	if ( itemOperatorName == NULL ) { // Create new operator item
	    itemOperatorName = new QStandardItem();
	    setValuesOfJourneyItem( itemOperatorName, journeyInfo, OperatorItem );
	    itemFirst->insertRow( iRow++, itemOperatorName );
	} else // Update old journey news item
	    setValuesOfJourneyItem( itemOperatorName, journeyInfo, OperatorItem, true );
    } else if ( itemOperatorName != NULL ) {
	itemFirst->removeRow( itemOperatorName->row() );
	--iRow;
    }

    for ( int row = 0; row < itemFirst->rowCount(); ++row )
	m_treeView->nativeWidget()->setFirstColumnSpanned(row, itemFirst->index(), true);
}

void PublicTransport::updateDeparture ( int row, const DepartureInfo& departureInfo ) {
    QStandardItem *itemLineString = m_model->item( row, m_departureViewColumns.indexOf(LineStringColumn) );
    QStandardItem *itemTarget = m_model->item( row, m_departureViewColumns.indexOf(TargetColumn) );
    QStandardItem *itemDeparture = m_model->item( row, m_departureViewColumns.indexOf(DepartureColumn) );
    setValuesOfDepartureItem( itemLineString, departureInfo, LineNameItem, true );
    setValuesOfDepartureItem( itemTarget, departureInfo, TargetItem, true );
    setValuesOfDepartureItem( itemDeparture, departureInfo, DepartureItem, true );

    // Update platform
    int iRow = itemLineString->rowCount();
    QStandardItem *itemPlatform = NULL;
    for ( int i = 0; i < itemLineString->rowCount(); ++i ) {
	if ( itemLineString->child( i )->data( SortRole ).toInt() == 1 ) { // TODO == 1   =>   "== enum"
	    itemPlatform = itemLineString->child( i );
	    break;
	}
    }
    if ( !departureInfo.platform.isEmpty() ) {
	if ( itemPlatform == NULL ) { // Create new platform item
	    itemPlatform = new QStandardItem();
	    setValuesOfDepartureItem( itemPlatform, departureInfo, PlatformItem );
	    itemLineString->insertRow( iRow++, itemPlatform );
	} else // Update old platform item
	    setValuesOfDepartureItem( itemPlatform, departureInfo, PlatformItem, true );
    } else if ( itemPlatform != NULL ) {
	itemLineString->removeRow( itemPlatform->row() );
	--iRow;
    }

    // Update journey news
    QStandardItem *itemJourneyNews = NULL;
    for ( int i = 0; i < itemLineString->rowCount(); ++i ) {
	if ( itemLineString->child( i )->data( Qt::UserRole ).toInt() == 2 ) { // TODO == 2   =>   "== enum"
	    itemJourneyNews = itemLineString->child( i );
	    break;
	}
    }
    if ( !departureInfo.journeyNews.isEmpty() ) {
	if ( itemJourneyNews == NULL ) { // Create new journey news item
	    itemJourneyNews = new QStandardItem();
	    setValuesOfDepartureItem( itemJourneyNews, departureInfo, JourneyNewsItem );
	    itemLineString->insertRow( iRow++, itemJourneyNews );
	} else // Update old journey news item
	    setValuesOfDepartureItem( itemJourneyNews, departureInfo, JourneyNewsItem, true );
    } else if ( itemJourneyNews != NULL ) {
	itemLineString->removeRow( itemJourneyNews->row() );
	--iRow;
    }

    // Update operator name
    QStandardItem *itemOperatorName = NULL;
    for ( int i = 0; i < itemLineString->rowCount(); ++i ) {
	if ( itemLineString->child( i )->data( Qt::UserRole ).toInt() == 4 ) { // TODO == 4   =>   "== enum"
	    itemOperatorName = itemLineString->child( i );
	    break;
	}
    }
    if ( !departureInfo.operatorName.isEmpty() ) {
	if ( itemOperatorName == NULL ) { // Create new operator item
	    itemOperatorName = new QStandardItem();
	    setValuesOfDepartureItem( itemOperatorName, departureInfo, OperatorItem );
	    itemLineString->insertRow( iRow++, itemOperatorName );
	} else // Update old journey news item
	    setValuesOfDepartureItem( itemOperatorName, departureInfo, OperatorItem, true );
    } else if ( itemOperatorName != NULL ) {
	itemLineString->removeRow( itemOperatorName->row() );
	--iRow;
    }

    // Update delay
    QStandardItem *itemDelay = NULL;
    for ( int i = 0; i < itemLineString->rowCount(); ++i ) {
	if ( itemLineString->child( i )->data( Qt::UserRole ).toInt() == 0 ) { // TODO == 0   =>   "== enum"
	    itemDelay = itemLineString->child( i );
	    break;
	}
    }
    if ( itemDelay == NULL ) { // Create new delay item
	itemDelay = new QStandardItem();
	setValuesOfDepartureItem( itemDelay, departureInfo, DelayItem );
// 	getDelayItems( departureInfo, &itemDelayInfo, &itemDelay ); // TODO: set sort data = 0
	itemLineString->insertRow( iRow++, itemDelay );
    } else { // Update old delay item
	setValuesOfDepartureItem( itemDelay, departureInfo, DelayItem );
// 	QStandardItem *newItemDelayInfo, *newItemDelay;
// 	getDelayItems( departureInfo, &itemDelayInfo, &itemDelay ); // TODO: set sort data = 0
// 	itemDelayInfo->setText( newItemDelayInfo->text() );
// 	itemDelay->setText( newItemDelay->text() );

// 	delete newItemDelayInfo;
// 	delete newItemDelay;
    }

    for ( int row = 0; row < itemLineString->rowCount(); ++row )
	m_treeView->nativeWidget()->setFirstColumnSpanned(row, itemLineString->index(), true);
}

void PublicTransport::removeOldDepartures() {
//     qDebug() << "PublicTransport::removeOldDepartures" << m_departureInfos.count() << "departures";

    QList< QModelIndex > notFoundRows;
    for ( int row = m_model->rowCount() - 1; row >= 0; --row )
	notFoundRows.append( m_model->index(row, 0) );
    foreach( DepartureInfo departureInfo, m_departureInfos )
    {
	int row = findDeparture( departureInfo );
	if ( row != -1 ) {
	    if ( filterOut(departureInfo) )
		qDebug() << "PublicTransport::removeOldDepartures" << "Item will be removed at row" << row << "because filterOut returns true for that item";
	    else if ( !notFoundRows.removeOne(m_model->index(row, 0)) )
		qDebug() << "PublicTransport::removeOldDepartures" << "Couldn't find item not to be removed at row" << row;
	}
    }

    foreach( QModelIndex notFoundRow, notFoundRows )
    {
// 	qDebug() << "PublicTransport::removeOldDepartures" << "remove row" << notFoundRow.row();

	QStandardItem *itemDeparture = m_model->item( notFoundRow.row(), 2 );
	AlarmTimer *alarmTimer = (AlarmTimer*)itemDeparture->data( AlarmTimerRole ).value<void*>();
	if ( alarmTimer &&  alarmTimer->timer()->isActive() ) {
	    m_abandonedAlarmTimer.append( alarmTimer );
	    qDebug() << "PublicTransport::removeOldDepartures" << "append abandoned alarm timer of not found row" << notFoundRow.row();
	}

	m_model->removeRow( notFoundRow.row() );
    }
//     qDebug() << "PublicTransport::removeOldDepartures" << "END";
}

void PublicTransport::removeOldJourneys() {
    qDebug() << "PublicTransport::removeOldJourneys" << m_journeyInfos.count() << "journeys";

    QList< QModelIndex > notFoundRows;
    for ( int row = m_modelJourneys->rowCount() - 1; row >= 0; --row )
	notFoundRows.append( m_modelJourneys->index(row, 0) );
    foreach( JourneyInfo journeyInfo, m_journeyInfos )
    {
	int row = findJourney( journeyInfo );
	if ( row != -1 ) {
	    // TODO: filtering for journeys
	    if ( /* !filterOut( journeyInfo ) && */ !notFoundRows.removeOne( m_modelJourneys->index(row, 0) ) )
		qDebug() << "PublicTransport::removeOldJourneys" << "Couldn't remove index";
	}
    }

    foreach( QModelIndex notFoundRow, notFoundRows )
    {
	qDebug() << "PublicTransport::removeOldJourneys" << "remove row" << notFoundRow.row();

	// TODO:
// 	QStandardItem *itemDeparture = m_modelJourneys->item( notFoundRow.row(), 2 );
// 	AlarmTimer *alarmTimer = (AlarmTimer*)itemDeparture->data( AlarmTimerRole ).value<void*>();
// 	if ( alarmTimer &&  alarmTimer->timer()->isActive() ) {
// 	    m_abandonedAlarmTimer.append( alarmTimer );
// 	    qDebug() << "PublicTransport::removeOldJourneys" << "append abandoned alarm timer of not found row" << notFoundRow.row();
// 	}

	m_modelJourneys->removeRow( notFoundRow.row() );
    }
//     qDebug() << "PublicTransport::removeOldJourneys" << "END";
}

void PublicTransport::updateModelJourneys() {
    if (m_graphicsWidget == NULL)
	graphicsWidget();

    int sortSection = m_treeView->nativeWidget()->header()->sortIndicatorSection();
    Qt::SortOrder sortOrder = m_treeView->nativeWidget()->header()->sortIndicatorOrder();

//     m_label->setText( titleText() );
//     m_labelInfo->setText( infoText() );

    removeOldJourneys(); //  also remove filtered departures  (after changing filter settings)?
    foreach( JourneyInfo journeyInfo, m_journeyInfos )
    {
	int row = findJourney(journeyInfo);

	// Apply filters TODO
// 	if ( filterOut(journeyInfo) )
// 	    continue;

	if ( row != -1 ) { // just update departure data
	    updateJourney( row, journeyInfo );
	    qDebug() << "update journey row" << row;
	} else { // append new departure
	    appendJourney( journeyInfo );
	    qDebug() << "append journey row";
	}
    }

    // Restore sort indicator
    m_treeView->nativeWidget()->header()->setSortIndicator( sortSection, sortOrder );

    // Sort list of journeys
    qSort( m_journeyInfos.begin(), m_journeyInfos.end() );

    geometryChanged();
}

void PublicTransport::updateModel() {
    if (m_graphicsWidget == NULL)
	graphicsWidget();

    int sortSection = m_treeView->nativeWidget()->header()->sortIndicatorSection();
    Qt::SortOrder sortOrder = m_treeView->nativeWidget()->header()->sortIndicatorOrder();

    if ( m_titleType == ShowDepartureArrivalListTitle ) {
	m_label->setText( titleText() );
	m_labelInfo->setText( infoText() );
    }

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
    m_treeView->nativeWidget()->header()->setSortIndicator( sortSection, sortOrder );

    // Sort list of departures / arrivals
    // TODO: first in sorted departure list WITH delay
    qSort( m_departureInfos.begin(), m_departureInfos.end() );

    geometryChanged();
}


#include "publictransport.moc"