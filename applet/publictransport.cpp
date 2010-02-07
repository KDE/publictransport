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

// KDE includes
#include <KDebug>
#include <KLocale>
#include <KIconEffect>
#include <KNotification>
#include <KConfigDialog>
#include <KToolInvocation>
#include <KColorScheme>
#include <KSelectAction>
#include <KCompletion>
#include <KSqueezedTextLabel>

// Plasma includes
#include <Plasma/IconWidget>
#include <Plasma/Label>
#include <Plasma/ToolButton>
#include <Plasma/LineEdit>
#include <Plasma/TreeView>
#include <Plasma/PushButton>
#include <Plasma/PaintUtils>
#include <Plasma/Theme>
#include <Plasma/ToolTipManager>
#include <Plasma/ToolTipContent>

// Qt includes
#include <QPainter>
#include <QFontMetrics>
#include <QSizeF>
#include <QGraphicsLinearLayout>
#include <QGraphicsGridLayout>
#include <QGraphicsScene>
#include <QListWidget>
#include <QMenu>
#include <QTreeView>
#include <QStandardItemModel>
#include <QStringListModel>
#include <qmath.h>

// Own includes
#include "publictransport.h"
#include "htmldelegate.h"
#include "alarmtimer.h"
#include "settings.h"

#if QT_VERSION >= 0x040600
#include <QGraphicsEffect>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QParallelAnimationGroup>
#endif

#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
#include <Plasma/Animator>
#include <Plasma/Animation>
#include <QGraphicsSceneMouseEvent>

Plasma::Animation *fadeAnimation( QGraphicsWidget *w, qreal targetOpacity ) {
    if ( w->geometry().width() * w->geometry().height() > 250000 ) {
	// Don't fade big widgets for performance reasons
	w->setOpacity( targetOpacity );
	return NULL;
    }
    
    Plasma::Animation *anim = Plasma::Animator::create( Plasma::Animator::FadeAnimation );
    anim->setTargetWidget( w );
    anim->setProperty( "startOpacity", w->opacity() );
    anim->setProperty( "targetOpacity", targetOpacity );
    return anim;
}

void startFadeAnimation( QGraphicsWidget *w, qreal targetOpacity ) {
    Plasma::Animation *anim = fadeAnimation( w, targetOpacity );
    if ( anim )
	anim->start( QAbstractAnimation::DeleteWhenStopped );
}
#endif

OverlayWidget::OverlayWidget( QGraphicsWidget* parent, QGraphicsWidget* under )
	    : QGraphicsWidget( parent ), opacity( 0.4 )
#if QT_VERSION >= 0x040600
	    , m_under( 0 ), m_blur( 0 )
#endif
	    {
    resize( parent->size() );
    setZValue( 10000 );
    m_under = under;
    under->setEnabled( false );
    
#if QT_VERSION >= 0x040600
    if ( under && KGlobalSettings::graphicEffectsLevel() != KGlobalSettings::NoEffects ) {
	m_blur = new QGraphicsBlurEffect( this );
	m_blur->setBlurHints( QGraphicsBlurEffect::AnimationHint );
	QPropertyAnimation *blurAnim = new QPropertyAnimation( m_blur, "blurRadius" );
	blurAnim->setStartValue( 0 );
	blurAnim->setEndValue( 5 );
	blurAnim->setDuration( 1000 );
	under->setGraphicsEffect( m_blur );
	blurAnim->start( QAbstractAnimation::DeleteWhenStopped );
    }
#endif
}

void OverlayWidget::destroy() {
#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
    Plasma::Animation *fadeAnim = fadeAnimation( this, 0 );
    
    QParallelAnimationGroup *parGroup = new QParallelAnimationGroup;
    connect( parGroup, SIGNAL(finished()), this, SLOT(overlayAnimationComplete()) );
    if ( fadeAnim )
	parGroup->addAnimation( fadeAnim );
    if ( m_blur ) {
	QPropertyAnimation *blurAnim = new QPropertyAnimation( m_blur, "blurRadius" );
	blurAnim->setStartValue( m_blur->blurRadius() );
	blurAnim->setEndValue( 0 );
	parGroup->addAnimation( blurAnim );
    }
    parGroup->start( QAbstractAnimation::DeleteWhenStopped );
    
    m_under->setEnabled( true );
#else
    overlayAnimationComplete();
#endif
}

void OverlayWidget::paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
			   QWidget* widget ) {
    Q_UNUSED( option )
    Q_UNUSED( widget )

    if ( qFuzzyCompare(1, 1 + opacity) )
	return;

    QColor wash = Plasma::Theme::defaultTheme()->color( Plasma::Theme::BackgroundColor );
    wash.setAlphaF( opacity );

    Plasma::Applet *applet = qobject_cast<Plasma::Applet *>(parentWidget());
    QPainterPath backgroundShape;
    if ( !applet || applet->backgroundHints() & Plasma::Applet::StandardBackground ) {
	// FIXME: a resize here is nasty, but perhaps still better than an eventfilter just for that..
	if ( parentWidget()->contentsRect().size() != size() )
	    resize( parentWidget()->contentsRect().size() );

	backgroundShape = Plasma::PaintUtils::roundedRectangle(contentsRect(), 5);
    } else {
	backgroundShape = shape();
    }

    painter->setRenderHints( QPainter::Antialiasing );
    painter->fillPath( backgroundShape, wash );
}

void OverlayWidget::overlayAnimationComplete() {
    if ( scene() )
	scene()->removeItem( this );
    deleteLater();

    m_under->setEnabled( true );
#if QT_VERSION >= 0x040600
    m_under->setGraphicsEffect( NULL );
#endif
}



PublicTransport::PublicTransport( QObject *parent, const QVariantList &args )
	    : Plasma::PopupApplet(parent, args),
	    m_graphicsWidget(0), m_mainGraphicsWidget(0),
	    m_icon(0), m_label(0), m_labelInfo(0), m_treeView(0),
	    m_journeySearch(0), m_listStopsSuggestions(0), m_btnLastJourneySearches(0),
	    m_overlay(0), m_model(0), m_modelJourneys(0),
	    m_settings(new PublicTransportSettings(this)),
	    m_departureListUpdater(0), m_journeyListUpdater(0) {
    m_departureViewColumns << LineStringColumn << TargetColumn << DepartureColumn;
    m_journeyViewColumns << VehicleTypeListColumn << JourneyInfoColumn
			 << DepartureColumn << ArrivalColumn;

    m_journeySearchLastTextLength = 0;
    setBackgroundHints( DefaultBackground );
    setAspectRatioMode( Plasma::IgnoreAspectRatio );
    setHasConfigurationInterface( true );
    resize( 400, 300 );

    connect( m_settings, SIGNAL(configNeedsSaving()),
	     this, SLOT(emitConfigNeedsSaving()) );
    connect( m_settings, SIGNAL(configurationRequired(bool,QString)),
	     this, SLOT(configurationIsRequired(bool,QString)) );
    connect( m_settings, SIGNAL(departureListNeedsClearing()),
	     this, SLOT(departureListNeedsClearing()) );
    connect( m_settings, SIGNAL(modelNeedsUpdate()),
	     this, SLOT(modelNeedsUpdate()) );
    connect( m_settings, SIGNAL(settingsChanged()),
	     this, SLOT(emitSettingsChanged()) );
    connect( m_settings, SIGNAL(serviceProviderSettingsChanged()),
	     this, SLOT(serviceProviderSettingsChanged()) );
    connect( m_settings, SIGNAL(departureArrivalListTypeChanged(DepartureArrivalListType)),
	     this, SLOT(departureArrivalListTypeChanged(DepartureArrivalListType)) );
}

void PublicTransport::configurationIsRequired( bool needsConfiguring,
					       const QString &reason ) {
    setConfigurationRequired( needsConfiguring, reason );
}

PublicTransport::~PublicTransport() {
    for ( int row = 0; row < m_model->rowCount(); ++row )
	removeAlarmForDeparture( row );
    
    delete m_model;
    delete m_modelJourneys;
    delete m_departureListUpdater;
    delete m_journeyListUpdater;
    if ( m_treeView ) {
	HtmlDelegate *htmlDelegate = static_cast< HtmlDelegate* >(
		m_treeView->nativeWidget()->itemDelegate() );
	delete htmlDelegate;
    }
    qDeleteAll( m_abandonedAlarmTimer );
    
    if ( hasFailedToLaunch() ) {
        // Do some cleanup here
    } else {
	m_journeySearch->removeEventFilter( this );
	config().writeEntry( "recentJourneySearches", m_recentJourneySearches );
	emit configNeedsSaving();

	delete m_label;
	delete m_labelInfo;
	delete m_icon;
	delete m_iconClose;
	delete m_journeySearch;
	delete m_labelJourneysNotSupported;
	delete m_btnLastJourneySearches;
	delete m_graphicsWidget;
    }
}

void PublicTransport::init() {
    m_settings->readSettings();
    m_recentJourneySearches = config().readEntry( "recentJourneySearches", QStringList() );
    
    createModels();
    graphicsWidget();
    createTooltip();
    createPopupIcon();

    setDepartureArrivalListType( m_settings->departureArrivalListType() );
    initJourneyList();
    addState( ShowingDepartureArrivalList );
    addState( WaitingForDepartureData );

    connect( this, SIGNAL(geometryChanged()), this, SLOT(geometryChanged()) );
    connect( this, SIGNAL(settingsChanged()), this, SLOT(configChanged()) );
    connect( Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()),
	     this, SLOT(themeChanged()) );
    emit settingsChanged();

    setupActions();
    reconnectSource();
}

void PublicTransport::setupActions() {
    QAction *actionUpdate = new QAction( KIcon("view-refresh"),
					 i18n("&Update timetable"), this );
    connect( actionUpdate, SIGNAL(triggered(bool)), this, SLOT(updateDataSource(bool)) );
    addAction( "updateTimetable", actionUpdate );

    QAction *actionSetAlarmForDeparture = new QAction(
	    Global::makeOverlayIcon(KIcon("kalarm"), "list-add"),
	    m_settings->departureArrivalListType() == DepartureList
	    ? i18n("Set &Alarm for This Departure")
	    : i18n("Set &Alarm for This Arrival"), this );
    connect( actionSetAlarmForDeparture, SIGNAL(triggered(bool)),
	     this, SLOT(setAlarmForDeparture(bool)) );
    addAction( "setAlarmForDeparture", actionSetAlarmForDeparture );

    QAction *actionRemoveAlarmForDeparture = new QAction(
	    Global::makeOverlayIcon(KIcon("kalarm"), "list-remove"),
	    m_settings->departureArrivalListType() == DepartureList
	    ? i18n("Remove &Alarm for This Departure")
	    : i18n("Remove &Alarm for This Arrival"), this );
    connect( actionRemoveAlarmForDeparture, SIGNAL(triggered(bool)),
	     this, SLOT(removeAlarmForDeparture(bool)) );
    addAction( "removeAlarmForDeparture", actionRemoveAlarmForDeparture );

    QAction *actionSearchJourneys = new QAction( KIcon("edit-find"),
			    i18n("Search for &Journeys..."), this );
    connect( actionSearchJourneys, SIGNAL(triggered(bool)),
	     this, SLOT(showJourneySearch(bool)) );
    addAction( "searchJourneys", actionSearchJourneys );

    int iconExtend = 32;
    QAction *actionShowDepartures = new QAction(
		    Global::makeOverlayIcon(KIcon("public-transport-stop"),
		    QList<KIcon>() << KIcon("go-home") << KIcon("go-next"),
		    QSize(iconExtend / 2, iconExtend / 2), iconExtend),
		    i18n("Show &Departures"), this );
    connect( actionShowDepartures, SIGNAL(triggered(bool)),
	     m_settings, SLOT(setShowDepartures()) );
    addAction( "showDepartures", actionShowDepartures );

    QAction *actionShowArrivals = new QAction(
		    Global::makeOverlayIcon(KIcon("public-transport-stop"),
		    QList<KIcon>() << KIcon("go-next") << KIcon("go-home"),
		    QSize(iconExtend / 2, iconExtend / 2), iconExtend),
		    i18n("Show &Arrivals"), this );
    connect( actionShowArrivals, SIGNAL(triggered(bool)),
	     m_settings, SLOT(setShowArrivals()) );
    addAction( "showArrivals", actionShowArrivals );
    
    QAction *actionBackToDepartures = new QAction( KIcon("go-previous"),
		    i18n("Back to &Departure List"), this );
    connect( actionBackToDepartures, SIGNAL(triggered(bool)),
	     this, SLOT(goBackToDepartures()) );
    addAction( "backToDepartures", actionBackToDepartures );
	
    KSelectAction *actionSwitchFilterConfiguration = new KSelectAction(
	    Global::makeOverlayIcon(KIcon("folder"), "view-filter"),
	    i18n("Switch filter Configuration"), this );
    connect( actionSwitchFilterConfiguration, SIGNAL(triggered(QString)),
	     m_settings, SLOT(loadFilterConfiguration(QString)) ); //SLOT(switchFilterConfiguration(QString)) );
    addAction( "switchFilterConfiguration", actionSwitchFilterConfiguration );

    QAction *actionAddTargetToFilterList = new QAction(
	    Global::makeOverlayIcon(KIcon("view-filter"), "list-add"),
	    m_settings->departureArrivalListType() == DepartureList
	    ? i18n("&Hide target") : i18n("&Hide origin"), this );
    connect( actionAddTargetToFilterList, SIGNAL(triggered(bool)),
	     this, SLOT(addTargetToFilterList(bool)) );
    addAction( "addTargetToFilterList", actionAddTargetToFilterList );

    QAction *actionRemoveTargetFromFilterList = new QAction(
	    Global::makeOverlayIcon(KIcon("view-filter"), "list-remove"),
	    m_settings->departureArrivalListType() == DepartureList
	    ? i18n("Remove target from the &filter list")
	    : i18n("Remove origin from the &filter list"), this );
    connect( actionRemoveTargetFromFilterList, SIGNAL(triggered(bool)),
	     this, SLOT(removeTargetFromFilterList(bool)) );
    addAction( "removeTargetFromFilterList", actionRemoveTargetFromFilterList );

    QAction *actionAddTargetToFilterListAndHide = new QAction(
	    Global::makeOverlayIcon(KIcon("view-filter"), "list-add"),
	    m_settings->departureArrivalListType() == DepartureList
	    ? i18n("&Hide target") : i18n("&Hide origin"), this );
    connect( actionAddTargetToFilterListAndHide, SIGNAL(triggered(bool)),
	     this, SLOT(addTargetToFilterListAndHide(bool)) );
    addAction( "addTargetToFilterListAndHide", actionAddTargetToFilterListAndHide );

    QAction *actionSetFilterListToHideMatching = new QAction( KIcon("view-filter"),
	    m_settings->departureArrivalListType() == DepartureList
	    ? i18n("&Hide target") : i18n("&Hide origin"), this );
    connect( actionSetFilterListToHideMatching, SIGNAL(triggered(bool)),
	     this, SLOT(setTargetFilterToHideMatching(bool)) );
    addAction( "setFilterListToHideMatching", actionSetFilterListToHideMatching );

    QAction *actionSetFilterListToShowAll = new QAction(
	    Global::makeOverlayIcon(KIcon("view-filter"), "edit-delete"),
	    m_settings->departureArrivalListType() == DepartureList
	    ? i18n("Show all &targets") : i18n("&Show all origins"), this );
    connect( actionSetFilterListToShowAll, SIGNAL(triggered(bool)),
	     this, SLOT(setTargetFilterToShowAll(bool)) );
    addAction( "setFilterListToShowAll", actionSetFilterListToShowAll );

    QAction *actionShowEverything = new QAction(
	Global::makeOverlayIcon(KIcon("view-filter"), "edit-delete"),
	i18n("&Show everything"), this );
    connect( actionShowEverything, SIGNAL(triggered(bool)),
	     this, SLOT(showEverything(bool)) );
    addAction( "showEverything", actionShowEverything );

    QAction *actionAddLineNumberToFilterList = new QAction(
	Global::makeOverlayIcon(KIcon("view-filter"), "list-add"),
	i18n("&Hide by line number"), this );
    connect( actionAddLineNumberToFilterList, SIGNAL(triggered(bool)),
	     this, SLOT(addLineNumberToFilterList(bool)) );
    addAction( "addLineNumberToFilterList", actionAddLineNumberToFilterList );

    QAction *actionRemoveLineNumberFromFilterList = new QAction(
	Global::makeOverlayIcon(KIcon("view-filter"), "list-remove"),
	i18n("Remove line number from the &filter list"), this );
    connect( actionRemoveLineNumberFromFilterList, SIGNAL(triggered(bool)),
	     this, SLOT(removeLineNumberFromFilterList(bool)) );
    addAction( "removeLineNumberFromFilterList", actionRemoveLineNumberFromFilterList );

    QAction *actionAddLineNumberToFilterListAndHide = new QAction(
	Global::makeOverlayIcon(KIcon("view-filter"), "list-add"),
	i18n("&Hide by line number"), this );
    connect( actionAddLineNumberToFilterListAndHide, SIGNAL(triggered(bool)),
	     this, SLOT(addLineNumberToFilterListAndHide(bool)) );
    addAction( "addLineNumberToFilterListAndHide", actionAddLineNumberToFilterListAndHide );

    QAction *actionSetLineNumberFilterToHideMatching = new QAction( KIcon("view-filter"),
							      i18n("&Hide by line number"), this );
    connect( actionSetLineNumberFilterToHideMatching, SIGNAL(triggered(bool)),
	     this, SLOT(setLineNumberFilterToHideMatching(bool)) );
    addAction( "setLineNumberFilterToHideMatching", actionSetLineNumberFilterToHideMatching );

    QAction *actionSetLineNumberFilterToShowAll = new QAction(
	Global::makeOverlayIcon(KIcon("view-filter"), "edit-delete"),
	i18n("Show all &lines"), this );
    connect( actionSetLineNumberFilterToShowAll, SIGNAL(triggered(bool)),
	     this, SLOT(setLineNumberFilterToShowAll(bool)) );
    addAction( "setLineNumberFilterToShowAll", actionSetLineNumberFilterToShowAll );

    
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
    // TODO: create new method in PublicTransportSettings::stopOrStopID()?
    if ( m_settings->stops().isEmpty() )
	return QString();
    else if ( !m_settings->stopIDs().isEmpty()
		&& !m_settings->stopIDs().first().isEmpty() )
	return m_settings->stopIDs().first();
    else
	return m_settings->stops().first();
}

QStringList PublicTransport::stopValues() const {
    QStringList ret;

    kDebug() << "Stops:" << m_settings->stops() << "Stop IDs:" << m_settings->stopIDs();
    if ( m_settings->stopIDs().count() == m_settings->stops().count() ) {
	int i = 0;
	foreach ( QString stopID, m_settings->stopIDs() ) {
	    if ( !stopID.isEmpty() ) {
		ret << stopID;
	    } else if ( m_settings->stops()[i].isEmpty() ) {
		kDebug() << "Empty stop and stop ID";
	    } else {
		kDebug() << "Empty stop ID, using stop name";
		ret << m_settings->stops()[ i ];
	    }
	    ++i;
	}
    } else {
	foreach ( QString stop, m_settings->stops() ) {
	    if ( !stop.isEmpty() )
		ret << stop;
	    else
		kDebug() << "Empty stop";
	}
    }

    return ret;
}

void PublicTransport::disconnectJourneySource() {
    if ( !m_currentJourneySource.isEmpty() ) {
	kDebug() << "Disconnect journey data source" << m_currentJourneySource;
	dataEngine("publictransport")->disconnectSource( m_currentJourneySource, this );

	if ( m_journeyListUpdater ) {
	    m_journeyListUpdater->stop();
	    delete m_journeyListUpdater;
	    m_journeyListUpdater = NULL;
	}
    }
}

void PublicTransport::reconnectJourneySource( const QString& targetStopName,
					      const QDateTime& dateTime,
					      bool stopIsTarget, bool timeIsDeparture,
					      bool requestStopSuggestions ) {
    disconnectJourneySource();

    QString _targetStopName = targetStopName;
    QDateTime _dateTime = dateTime;
    if ( _targetStopName.isEmpty() ) {
	if ( m_lastSecondStopName.isEmpty() )
	    return;
	_targetStopName = m_lastSecondStopName;
	_dateTime = m_lastJourneyDateTime;
    }

    if ( requestStopSuggestions ) {
	m_currentJourneySource = QString( "Stops %1|stop=%2" )
		.arg( m_settings->serviceProvider() )
		.arg( _targetStopName );
    } else {
	m_currentJourneySource = QString( /*m_settings->journeyListType() == JourneysFromHomeStopList*/
		stopIsTarget
		? "%6 %1|originStop=%2|targetStop=%3|maxDeps=%4|datetime=%5"
		: "%6 %1|originStop=%3|targetStop=%2|maxDeps=%4|datetime=%5" )
		.arg( m_settings->serviceProvider() )
		.arg( stop() ).arg( _targetStopName )
		.arg( m_settings->maximalNumberOfDepartures() )
		.arg( _dateTime.toString() )
		.arg( timeIsDeparture ? "Journeys" : "JourneysArr" );
    }
    
    if ( m_settings->useSeperateCityValue() )
	m_currentJourneySource += QString("|city=%1").arg( m_settings->city() );

    kDebug() << "Connect journey data source" << m_currentJourneySource
	     << "Autoupdate" << m_settings->isAutoUpdateEnabled();
    m_lastSecondStopName = _targetStopName;
    addState( WaitingForJourneyData );

//     if ( m_settings->updateTimeout() == 0 )
	dataEngine("publictransport")->connectSource( m_currentJourneySource, this );
	
	m_journeyListUpdater = new QTimer( this );
	connect( m_journeyListUpdater, SIGNAL(timeout()),
		 this, SLOT(updateModelJourneys()) );
	m_journeyListUpdater->start( 60000 );
//     else
// 	dataEngine("publictransport")->connectSource( m_currentJourneySource, this,
// 			m_settings->updateTimeout() * 1000, Plasma::AlignToMinute );
}

void PublicTransport::disconnectSources() {
    if ( !m_currentSources.isEmpty() ) {
	foreach ( QString currentSource, m_currentSources ) {
	    kDebug() << "Disconnect data source" << currentSource;
	    dataEngine( "publictransport" )->disconnectSource( currentSource, this );
	}
	m_currentSources.clear();

	if ( m_departureListUpdater ) {
	    m_departureListUpdater->stop();
	    delete m_departureListUpdater;
	    m_departureListUpdater = NULL;
	}
    }
}

void PublicTransport::reconnectSource() {
    disconnectSources();
    
    // Get a list of stops (or stop IDs if available) which results are currently shown
    QStringList stops = stopValues();
    if ( m_settings->currentStopIndex() != -1 ) { // Show only results of one stop
	if ( m_settings->currentStopIndex() >= stops.count() ) {
	    kDebug() << "Stop with index" << m_settings->currentStopIndex()
		     << "not found, using -1. Stop values:" << stops;
	    m_settings->setCurrentStopIndex( -1 );
	} else {
	    kDebug() << "Show only stop" << m_settings->currentStopIndex() << "- Stop values:" << stops;
	    stops = QStringList() << stops[ m_settings->currentStopIndex() ];
	}
    }
    
    kDebug() << "Connect" << m_settings->currentStopIndex() << stops;
    QStringList sources;
    m_stopIndexToSourceName.clear();
    int i = 0;
    foreach ( QString stopValue, stops ) {
	QString currentSource = QString("%4 %1|stop=%2|maxDeps=%3")
		.arg( m_settings->serviceProvider() )
		.arg( stopValue ).arg( m_settings->maximalNumberOfDepartures() )
		.arg( m_settings->departureArrivalListType() == ArrivalList
		    ? "Arrivals" : "Departures" );
	if ( m_settings->firstDepartureConfigMode() == RelativeToCurrentTime ) {
	    currentSource += QString("|timeOffset=%1").arg(
		    m_settings->timeOffsetOfFirstDeparture() );
	} else {
	    currentSource += QString("|time=%1").arg(
		    m_settings->timeOfFirstDepartureCustom().toString("hh:mm") );
	}
	if ( m_settings->useSeperateCityValue() )
	    currentSource += QString("|city=%1").arg( m_settings->city() );
	
	m_stopIndexToSourceName[ i++ ] = currentSource;
	sources << currentSource;
    }

    foreach ( QString currentSource, sources ) {
	kDebug() << "Connect data source" << currentSource
		 << "Autoupdate" << m_settings->isAutoUpdateEnabled();
	m_currentSources << currentSource;
	if ( m_settings->isAutoUpdateEnabled() ) {
	    // Update once a minute to show updated duration times
	    dataEngine("publictransport")->connectSource( currentSource, this,
							  60000, Plasma::AlignToMinute );
	} else { // TODO: CHECK: Update duration times without source updates
	    dataEngine("publictransport")->connectSource( currentSource, this );

	    // TODO: Create and start timer when the data source got some results
	    m_departureListUpdater = new QTimer( this );
	    connect( m_departureListUpdater, SIGNAL(timeout()),
		     this, SLOT(updateModel()) );
	    m_departureListUpdater->start( 60000 );
	}
    }

    addState( WaitingForDepartureData );
}

void PublicTransport::processJourneyList( const QString &sourceName,
					  const Plasma::DataEngine::Data& data ) {
    Q_UNUSED( sourceName );
    
    // Remove old journey list
    m_journeyInfos.clear();
    
    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
    QUrl url = data["requestUrl"].toUrl();
    kDebug() << "APP-URL" << url;
    setAssociatedApplicationUrls( KUrl::List() << url );
    #endif
    
    int count = data["count"].toInt();
    for( int i = 0; i < count; ++i ) {
	QVariant journeyData = data.value( QString("%1").arg(i) );
	if ( !journeyData.isValid()
		|| m_journeyInfos.count() >= m_settings->maximalNumberOfDepartures() ) {
	    if ( !journeyData.isValid() )
		kDebug() << i << "Journey data is invalid";
	    break;
	}

	QHash<QString, QVariant> dataMap = journeyData.toHash();
	QList<QTime> routeTimesDeparture, routeTimesArrival;
	if ( dataMap.contains("routeTimesDeparture") ) {
	    QVariantList times = dataMap[ "routeTimesDeparture" ].toList();
	    foreach ( QVariant time, times )
		routeTimesDeparture << time.toTime();
	}
	if ( dataMap.contains("routeTimesArrival") ) {
	    QVariantList times = dataMap[ "routeTimesArrival" ].toList();
	    foreach ( QVariant time, times )
		routeTimesArrival << time.toTime();
	}
	QStringList routeStops, routeTransportLines,
		    routePlatformsDeparture, routePlatformsArrival;
	if ( dataMap.contains("routeStops") )
	    routeStops = dataMap[ "routeStops" ].toStringList();
	if ( dataMap.contains("routeTransportLines") )
	    routeTransportLines = dataMap[ "routeTransportLines" ].toStringList();
	if ( dataMap.contains("routePlatformsDeparture") )
	    routePlatformsDeparture = dataMap[ "routePlatformsDeparture" ].toStringList();
	if ( dataMap.contains("routePlatformsArrival") )
	    routePlatformsArrival = dataMap[ "routePlatformsArrival" ].toStringList();
	
	QList<int> routeTimesDepartureDelay, routeTimesArrivalDelay;
	if ( dataMap.contains("routeTimesDepartureDelay") ) {
	    QVariantList list = dataMap[ "routeTimesDepartureDelay" ].toList();
	    foreach ( QVariant var, list )
		routeTimesDepartureDelay << var.toInt();
	}
	if ( dataMap.contains("routeTimesArrivalDelay") ) {
	    QVariantList list = dataMap[ "routeTimesArrivalDelay" ].toList();
	    foreach ( QVariant var, list )
		routeTimesArrivalDelay << var.toInt();
	}
	
	JourneyInfo journeyInfo( dataMap["operator"].toString(),
				 dataMap["vehicleTypes"].toList(),
				 dataMap["departure"].toDateTime(),
				 dataMap["arrival"].toDateTime(),
				 dataMap["pricing"].toString(),
				 dataMap["startStopName"].toString(), dataMap["targetStopName"].toString(),
				 dataMap["duration"].toInt(),
				 dataMap["changes"].toInt(),
				 dataMap["journeyNews"].toString(),
				 routeStops, routeTransportLines,
				 routePlatformsDeparture, routePlatformsArrival,
				 dataMap["routeVehicleTypes"].toList(),
				 routeTimesDeparture, routeTimesArrival,
				 routeTimesDepartureDelay, routeTimesArrivalDelay );

	// ¿Only add journeys that are in the future?
// 	int secsToDepartureTime = QDateTime::currentDateTime().secsTo( journeyInfo.departure );
// 	if ( m_settings->firstDepartureConfigMode() == RelativeToCurrentTime )
// 	    secsToDepartureTime -= m_settings->timeOffsetOfFirstDeparture() * 60;
// 	if ( -secsToDepartureTime / 3600 >= 23 )
// 	    secsToDepartureTime += 24 * 3600;
// 	if ( secsToDepartureTime > -60 )
	    m_journeyInfos.append( journeyInfo );
    }

    kDebug() << m_journeyInfos.count() << "journeys received";
//     m_lastJourneySourceUpdate = data["updated"].toDateTime();
    updateModelJourneys();
}

void PublicTransport::processDepartureList( const QString &sourceName,
					    const Plasma::DataEngine::Data& data ) {
    // Remove old departure / arrival list
    QString strippedSourceName = stripDateAndTimeValues( sourceName );
    m_departureInfos[ strippedSourceName ].clear();
    
    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
    QUrl url = data["requestUrl"].toUrl();
    kDebug() << "APP-URL" << url;
    m_urlDeparturesArrivals = url;
    if ( testState(ShowingDepartureArrivalList) || testState(ShowingJourneySearch)
	    || testState(ShowingJourneysNotSupported) )
	setAssociatedApplicationUrls( KUrl::List() << url );
    #endif

    int count = data["count"].toInt();
    for ( int i = 0; i < count; ++i ) {
	QVariant departureData = data.value( QString("%1").arg(i) );
	// Don't process invalid data and stop processing once the maximal
	// departure number is reached (except multiple stops are set)
	if ( !departureData.isValid() ) {
// 	    || (!m_settings->hasMultipleStops()
// 		    && m_departureInfos.count() >= m_settings->maximalNumberOfDepartures()) ) {
// 	    if ( !(m_departureInfos.count() >= m_settings->maximalNumberOfDepartures()) )
		kDebug() << "Departure data for departure" << i << "is invalid" << data;
	    break;
	}

	QHash<QString, QVariant> dataMap = departureData.toHash();
	QList<QTime> routeTimes;
	if ( dataMap.contains("routeTimes") ) {
	    QVariantList times = dataMap[ "routeTimes" ].toList();
	    foreach ( QVariant time, times )
		routeTimes << time.toTime();
	}
	DepartureInfo departureInfo( dataMap["operator"].toString(),
				     dataMap["line"].toString(),
				     dataMap["target"].toString(),
				     dataMap["departure"].toDateTime(),
				     static_cast<VehicleType>(dataMap["vehicleType"].toInt()),
				     dataMap["nightline"].toBool(),
				     dataMap["expressline"].toBool(),
				     dataMap["platform"].toString(),
				     dataMap["delay"].toInt(),
				     dataMap["delayReason"].toString(),
				     dataMap["journeyNews"].toString(),
				     dataMap["routeStops"].toStringList(),
				     routeTimes,
				     dataMap["routeExactStops"].toInt() );

	// Only add departures / arrivals that are in the future
	if ( isTimeShown(departureInfo.predictedDeparture()) )
	    m_departureInfos[ strippedSourceName ].append( departureInfo );
	else
	    kDebug() << "Departure is in the past" << departureInfo.predictedDeparture();
    }

    for ( int i = m_departureInfos[strippedSourceName].count() - 1; i >= 0; --i ) {
	if ( !isTimeShown(m_departureInfos[strippedSourceName][i].predictedDeparture()) )
	    m_departureInfos[ strippedSourceName ].removeAt( i );
    }

    kDebug() << m_departureInfos[ strippedSourceName ].count() << "departures / arrivals received";
    setConfigurationRequired( false );
    m_stopNameValid = true;

    QDateTime lastUpdate = data["updated"].toDateTime();
    if ( lastUpdate > m_lastSourceUpdate )
	m_lastSourceUpdate = lastUpdate;
    updateModel();
}

bool PublicTransport::isTimeShown( const QDateTime& dateTime ) const {
    int secsToDepartureTime = QDateTime::currentDateTime().secsTo( dateTime );
    if ( m_settings->firstDepartureConfigMode() == RelativeToCurrentTime )
	secsToDepartureTime -= m_settings->timeOffsetOfFirstDeparture() * 60;
    if ( -secsToDepartureTime / 3600 >= 23 )
	secsToDepartureTime += 24 * 3600;
    return secsToDepartureTime > -60;
}

QString PublicTransport::stripDateAndTimeValues( const QString& sourceName ) const {
    QString ret = sourceName;
    QRegExp rx( "(time=[^\\|]*|datetime=[^\\|]*)", Qt::CaseInsensitive );
    rx.setMinimal( true );
    ret.replace( rx, "" );
    return ret;
}

QList< DepartureInfo > PublicTransport::departureInfos() const {
    QList<DepartureInfo> ret;
    if ( m_settings->currentStopIndex() == -1 ) {
	foreach ( QList<DepartureInfo> departures, m_departureInfos )
	    ret << departures;

	qSort( ret.begin(), ret.end() );
    } else {
	QString sourceName = stripDateAndTimeValues(
		m_stopIndexToSourceName[0] );
	if ( m_departureInfos.contains(sourceName) )
	    ret << m_departureInfos[ sourceName ];
    }
    
    return ret.mid( 0, m_settings->maximalNumberOfDepartures() );
}

void PublicTransport::clearDepartures() {
    m_departureInfos.clear(); // Clear data from data engine
    if ( m_model ) {
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

void PublicTransport::processData( const QString &sourceName,
				   const Plasma::DataEngine::Data& data ) {
    bool departureData = data.value("parseMode").toString() == "departures";
    bool journeyData = data.value("parseMode").toString() == "journeys";

    // Check for errors from the data engine
    if ( data.value("error").toBool() ) {
	if ( journeyData ) {
	    addState( ReceivedErroneousJourneyData );
	    
	    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
	    QUrl url = data["requestUrl"].toUrl();
	    kDebug() << "Errorneous journey url" << url;
	    m_urlJourneys = url;
	    if ( testState(ShowingJourneyList) )
		setAssociatedApplicationUrls( KUrl::List() << url );
	    #endif
	} else if ( departureData ) {
	    m_stopNameValid = false;
	    addState( ReceivedErroneousDepartureData );
	    
	    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
	    QUrl url = data["requestUrl"].toUrl();
	    kDebug() << "Errorneous departure/arrival url" << url;
	    m_urlDeparturesArrivals = url;
	    if ( testState(ShowingDepartureArrivalList) || testState(ShowingJourneySearch)
		    || testState(ShowingJourneysNotSupported) )
		setAssociatedApplicationUrls( KUrl::List() << url );
	    #endif
	    
	    if ( testState(ServiceProviderSettingsJustChanged) ) {
		if ( m_settings->departureArrivalListType() == DepartureList )
		    this->setConfigurationRequired( true, i18n("Error parsing "
			    "departure information or currently no departures") );
		else
		    this->setConfigurationRequired( true, i18n("Error parsing "
			    "arrival information or currently no arrivals") );
	    }

	    // Update remaining times
	    updateModel();
	}
    } else if ( data.value("receivedPossibleStopList").toBool() ) { // Possible stop list received
	if ( journeyData || data.value("parseMode").toString() == "stopSuggestions" ) {
	    if ( journeyData )
		addState( ReceivedErroneousJourneyData );
	    QHash< QString, QVariant > stopToStopID;
	    QHash< QString, int > stopToStopWeight;
	    QStringList possibleStops, weightedStops;

	    int count = data["count"].toInt();
	    bool hasAtLeastOneWeight = false;
	    for( int i = 0; i < count; ++i ) {
		if ( !data.contains( QString("stopName %1").arg(i) ) ) {
		    kDebug() << "doesn't contain 'stopName" << i << "'! count ="
			     << count << "data =" << data;
		    break;
		}

		QHash<QString, QVariant> dataMap = data.value(
			QString("stopName %1").arg(i) ).toHash();
		QString sStopName = dataMap["stopName"].toString();
		QString sStopID = dataMap["stopID"].toString();
		int stopWeight = dataMap["stopWeight"].toInt();
		possibleStops << sStopName;
		if ( stopWeight <= 0 )
		    stopWeight = 0;
		else
		    hasAtLeastOneWeight = true;
		weightedStops << QString( "%1:%2" ).arg( sStopName ).arg( stopWeight );
		
		stopToStopID.insert( sStopName, sStopID );
		stopToStopWeight.insert( sStopName, stopWeight );
	    }

	    if ( m_lettersAddedToJourneySearchLine
			&& m_journeySearch->nativeWidget()->completionMode() !=
			KGlobalSettings::CompletionNone ) {
		int posStart, len;
		QString stopName;
		stopNamePosition( &posStart, &len, &stopName );
		int selStart = m_journeySearch->nativeWidget()->selectionStart();
		if ( selStart == -1 )
		    selStart = m_journeySearch->nativeWidget()->cursorPosition();
		bool stopNameChanged = selStart > posStart
			&& selStart + m_journeySearch->nativeWidget()->selectedText().length()
			<= posStart + len;
		if ( stopNameChanged ) {
// 		    QString stopName = m_journeySearch->text().mid( posStart, len );
		    KCompletion *comp = m_journeySearch->nativeWidget()->completionObject( false );
		    comp->setIgnoreCase( true );
		    if ( hasAtLeastOneWeight ) {
			comp->setOrder( KCompletion::Weighted );
			comp->setItems( weightedStops );
		    } else
			comp->setItems( possibleStops );
		    QString completion = comp->makeCompletion( stopName );

		    if ( completion != stopName )
			setJourneySearchStopNameCompletion( completion );
		}
	    }
	    
// 	    QStandardItemModel *model = (QStandardItemModel*)m_journeySearch->nativeWidget()->completer()->model();
	    QStandardItemModel *model;
// 	    if ( !model ) {
		model = new QStandardItemModel();
// 		m_journeySearch->nativeWidget()->completer()->setModel( model );
		m_listStopsSuggestions->setModel( model );
// 	    }

// 	    model->clear();
	    foreach( QString s, possibleStops ) {
		QStandardItem *item = new QStandardItem( s );
		item->setIcon( KIcon("public-transport-stop") );
		model->appendRow( item );
	    }
	} else if ( departureData ) {
	    m_stopNameValid = false;
	    addState( ReceivedErroneousDepartureData );
	    clearDepartures();
	    this->setConfigurationRequired( true, i18n("The stop name is ambiguous.") );
	}
    } else {// List of departures / arrivals / journeys received
	if ( journeyData ) {
	    addState( ReceivedValidJourneyData );
	    if ( testState(ShowingJourneyList) )
		processJourneyList( sourceName, data );
	} else if ( departureData ) {
	    m_stopNameValid = true;
	    addState( ReceivedValidDepartureData );
	    processDepartureList( sourceName, data );
	}
    }

    removeState( SettingsJustChanged );
    removeState( ServiceProviderSettingsJustChanged );
}

void PublicTransport::dataUpdated( const QString& sourceName,
				   const Plasma::DataEngine::Data& data ) {
    Q_UNUSED( sourceName );

    if ( data.isEmpty() || (!m_currentSources.contains(sourceName)
		&& sourceName != m_currentJourneySource) )
	return;

    processData( sourceName, data );
    createTooltip();
    createPopupIcon();
}

void PublicTransport::geometryChanged() {
    // Update column sizes
    QTreeView *treeView = m_treeView->nativeWidget();
    QHeaderView *header = treeView->header();
    disconnect( header, SIGNAL(sectionResized(int,int,int)),
		this, SLOT(treeViewSectionResized(int,int,int)) );

    if ( testState(ShowingDepartureArrivalList) ) {
	int lineSectionSize = treeView->columnWidth(
		m_departureViewColumns.indexOf(LineStringColumn) );
	int departureSectionSize = treeView->columnWidth(
		m_departureViewColumns.indexOf(DepartureColumn) );

	if ( treeView->isColumnHidden(m_departureViewColumns.indexOf(TargetColumn)) ) { // target column hidden
// 	    kDebug() << "Target column is hidden";

    // 	float lineSectionSizeFactor = (float)lineSectionSize / (float)(lineSectionSize + departureSectionSize);
    // 	lineSectionSize = qFloor((float)header->width() * lineSectionSizeFactor);
    // 	departureSectionSize = header->width() - lineSectionSize;

    // 	treeView->header()->setResizeMode( 1, QHeaderView::Fixed );
    // 	treeView->header()->resizeSection( 1, 0 );
    // 	treeView->header()->resizeSection( 0, lineSectionSize );
    // 	treeView->header()->resizeSection( 2, departureSectionSize );

    // 	kDebug() << "new column width" << lineSectionSize << departureSectionSize << "total =" << (lineSectionSize + departureSectionSize) << "of" << treeView->header()->width();
	} else {
	    treeView->header()->setResizeMode( 1, QHeaderView::Interactive );
	    if ( lineSectionSize + departureSectionSize > header->width() - 10 )
	    {
		float lineSectionSizeFactor = (float)lineSectionSize
			/ (float)(lineSectionSize + departureSectionSize);
		lineSectionSize = header->width() * lineSectionSizeFactor;
		treeView->setColumnWidth( m_departureViewColumns.indexOf(LineStringColumn),
					  lineSectionSize );
		departureSectionSize = header->width() * (1 - lineSectionSizeFactor);
		treeView->setColumnWidth( m_departureViewColumns.indexOf(DepartureColumn),
					  departureSectionSize );
	    }

	    int targetSectionSize = header->width() - lineSectionSize - departureSectionSize;
	    if ( targetSectionSize < 10 )
		targetSectionSize = 10;
	    treeView->setColumnWidth( m_departureViewColumns.indexOf(TargetColumn),
				      targetSectionSize );
	}
    } else if ( testState(ShowingJourneyList) ) {
	int vehicleTypeListSectionSize = treeView->columnWidth(
		m_journeyViewColumns.indexOf(VehicleTypeListColumn) );
	int departureSectionSize = treeView->columnWidth(
		m_journeyViewColumns.indexOf(DepartureColumn) );
	int arrivalSectionSize = treeView->columnWidth(
		m_journeyViewColumns.indexOf(ArrivalColumn) );

	treeView->header()->setResizeMode( 1, QHeaderView::Interactive );
	if ( vehicleTypeListSectionSize + departureSectionSize + arrivalSectionSize
		    > header->width() - 10 ) {
	    float vehicleTypeListSizeFactor = (float)vehicleTypeListSectionSize
		    / (float)(vehicleTypeListSectionSize + departureSectionSize + arrivalSectionSize);
	    float departureSizeFactor = (float)departureSectionSize
		    / (float)(vehicleTypeListSectionSize + departureSectionSize + arrivalSectionSize);
	    float arrivalSizeFactor = 1.0f - vehicleTypeListSizeFactor - departureSizeFactor;

	    vehicleTypeListSectionSize = header->width() * vehicleTypeListSizeFactor;
	    treeView->setColumnWidth( m_journeyViewColumns.indexOf(VehicleTypeListColumn),
				      vehicleTypeListSectionSize );
	    departureSectionSize = header->width() * departureSizeFactor;
	    treeView->setColumnWidth( m_journeyViewColumns.indexOf(DepartureColumn),
				      departureSectionSize );
	    arrivalSectionSize = header->width() * arrivalSizeFactor;
	    treeView->setColumnWidth( m_journeyViewColumns.indexOf(ArrivalColumn),
				      arrivalSectionSize );
	}

	int journeyInfoSectionSize = header->width() - vehicleTypeListSectionSize - departureSectionSize - arrivalSectionSize;
	if ( journeyInfoSectionSize < 10 )
	    journeyInfoSectionSize = 10;
	treeView->setColumnWidth( m_journeyViewColumns.indexOf(JourneyInfoColumn), journeyInfoSectionSize );
    }

    connect ( treeView->header(), SIGNAL(sectionResized(int,int,int)),
	      this, SLOT(treeViewSectionResized(int,int,int)) );
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

void PublicTransport::popupEvent( bool show ) {
    destroyOverlay();
    addState( ShowingDepartureArrivalList ); // Hide opened journey views, ie. back to departure view

    Plasma::PopupApplet::popupEvent( show );
}

void PublicTransport::createPopupIcon() {
    AlarmTimer *alarmTimer = getNextAlarm();
    if ( alarmTimer && alarmTimer->timer()->isActive() ) {
	int minutesToAlarm = alarmTimer->timer()->interval() / 60000 -
				qCeil( (float)alarmTimer->startedAt().secsTo(
				       QDateTime::currentDateTime() ) / 60.0f );
	int hoursToAlarm = minutesToAlarm / 60;
	minutesToAlarm %= 60;
	QString text = i18nc( "This is displayed on the popup icon to indicate "
			      "the remaining time to the next alarm, %1=hours, "
			      "%2=minutes with padded 0", "%1:%2", hoursToAlarm,
			      QString("%1").arg(minutesToAlarm, 2, 10, QLatin1Char('0')) );

	QFont font = Plasma::Theme::defaultTheme()->font(Plasma::Theme::DefaultFont);

	QPixmap pixmapIcon128 = KIcon( "public-transport-stop" ).pixmap(128);
	QPixmap pixmapAlarmIcon32 = KIcon( "kalarm" ).pixmap(32);
	QPainter p128( &pixmapIcon128 );
	font.setPixelSize( 40 );
	p128.setFont( font );
	QPixmap shadowedText = Plasma::PaintUtils::shadowText( text, font );
	QSize sizeText( p128.fontMetrics().width(text), p128.fontMetrics().lineSpacing() );
	QRect rectText( QPoint(128 - 4 - sizeText.width(),128 - sizeText.height()), sizeText );
	QRect rectIcon( rectText.left() - 32 - 4,
			rectText.top() + (rectText.height() - 32) / 2, 32, 32 );
	p128.drawPixmap( rectIcon, pixmapAlarmIcon32 );
	p128.drawPixmap( rectText, shadowedText );
	p128.end();

	QPixmap pixmapIcon48 = KIcon( "public-transport-stop" ).pixmap(48);
	QPixmap pixmapAlarmIcon13 = KIcon("kalarm").pixmap(13);
	QPainter p48( &pixmapIcon48 );
	font.setPixelSize( 18 );
	font.setBold( true );
	p48.setFont( font );
	shadowedText = Plasma::PaintUtils::shadowText( text, font );
	sizeText = QSize( p48.fontMetrics().width(text), p48.fontMetrics().lineSpacing() );
	rectText = QRect( QPoint(48 - sizeText.width(),48 - sizeText.height()), sizeText );
	rectIcon = QRect( rectText.left() - 11 - 1,
			  rectText.top() + (rectText.height() - 11) / 2, 13, 13 );
	rectText.adjust( 0, 2, 0, 2 );
	p48.drawPixmap( rectIcon, pixmapAlarmIcon13 );
	p48.drawPixmap( rectText, shadowedText );
	p48.end();

	KIcon icon = KIcon();
	icon.addPixmap( pixmapIcon128, QIcon::Normal );
	icon.addPixmap( pixmapIcon48, QIcon::Normal );

	setPopupIcon( icon );
    } else
	setPopupIcon( "public-transport-stop" );
}

void PublicTransport::createTooltip() {
    DepartureInfo nextDeparture;
    Plasma::ToolTipContent data;
    data.setMainText( i18n("Public transport") );
    if ( departureInfos().isEmpty() )
	data.setSubText( i18n("View departure times for public transport") );
    else if ( (nextDeparture = getFirstNotFilteredDeparture()).isValid() ) {
	if ( m_settings->departureArrivalListType() ==  DepartureList ) {
	    if ( m_settings->stops().count() == 1 || m_settings->currentStopIndex() == -1 ) {
		data.setSubText( i18nc("%4 is the translated duration text, e.g. in 3 minutes",
				       "Next departure from '%1': line %2 (%3) %4",
				       m_settings->stops().first(),
				       nextDeparture.lineString(), nextDeparture.target(),
				       nextDeparture.durationString() ) );
	    } else {
		data.setSubText( i18nc("%3 is the translated duration text, e.g. in 3 minutes",
				       "Next departure from your home stop: line %1 (%2) %3",
				       nextDeparture.lineString(), nextDeparture.target(),
				       nextDeparture.durationString() ) );
	    }
	} else {
	    if ( m_settings->stops().count() == 1 || m_settings->currentStopIndex() == -1 ) {
		data.setSubText( i18nc("%4 is the translated duration text, e.g. in 3 minutes",
				       "Next arrival at '%1': line %2 (%3) %4",
				       m_settings->stops().first(),
				       nextDeparture.lineString(), nextDeparture.target(),
				       nextDeparture.durationString() ) );
	    } else {
		data.setSubText( i18nc("%3 is the translated duration text, e.g. in 3 minutes",
				       "Next arrival at your home stop: line %1 (%2) %3",
				       nextDeparture.lineString(), nextDeparture.target(),
				       nextDeparture.durationString() ) );
	    }
	}
    }
    
    data.setImage( KIcon("public-transport-stop").pixmap(IconSize(KIconLoader::Desktop)) );
    
    Plasma::ToolTipManager::self()->setContent( this, data );
}

DepartureInfo PublicTransport::getFirstNotFilteredDeparture() {
    QList<DepartureInfo> depInfos = departureInfos();
//     qSort( depInfos.begin(), depInfos.end() );
    DepartureInfo nextDeparture;
    for ( int i = 0; i < depInfos.count(); ++i ) {
	DepartureInfo departure = depInfos[i];
	if ( !filterOut(departure) ) {
	    nextDeparture = departure;
	    break;
	}
    }

    return nextDeparture;
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
    disconnect( this, SIGNAL(settingsChanged()), this, SLOT(configChanged()) );
    addState( ShowingDepartureArrivalList );
    addState( SettingsJustChanged );

    setDepartureArrivalListType( m_settings->departureArrivalListType() );
    m_treeView->nativeWidget()->header()->setVisible( m_settings->isHeaderVisible() );
    m_treeView->nativeWidget()->setColumnHidden( 1, m_settings->isColumnTargetHidden() );

    QFont font = m_settings->font();
    QFont smallFont = font, boldFont = font;
    float sizeFactor = m_settings->sizeFactor();
    if ( font.pointSize() == -1 ) {
	int pixelSize = font.pixelSize() * sizeFactor;
	font.setPixelSize( pixelSize > 0 ? pixelSize : 1 );
    } else {
	int pointSize = font.pointSize() * sizeFactor;
	font.setPointSize( pointSize > 0 ? pointSize : 1 );
    }
    int smallPointSize = KGlobalSettings::smallestReadableFont().pointSize() * sizeFactor;
    smallFont.setPointSize( smallPointSize > 0 ? smallPointSize : 1 );
    boldFont.setBold( true );
    
    m_treeView->setFont( font );
    m_label->setFont( boldFont );
    m_labelInfo->setFont( smallFont );
    m_listStopsSuggestions->setFont( font );
    m_journeySearch->setFont( font );

    int iconExtend = (testState(ShowingDepartureArrivalList) ? 16 : 32) * m_settings->sizeFactor();
    m_treeView->nativeWidget()->setIconSize( QSize(iconExtend, iconExtend) );
    
    int mainIconExtend = 32 * m_settings->sizeFactor();
    m_icon->setMinimumSize( mainIconExtend, mainIconExtend );
    m_icon->setMaximumSize( mainIconExtend, mainIconExtend );
    m_iconClose->setMinimumSize( mainIconExtend, mainIconExtend );
    m_iconClose->setMaximumSize( mainIconExtend, mainIconExtend );
    
    updateModel();
    updateModelJourneys();

    if ( m_settings->isColumnTargetHidden() )
	hideColumnTarget(true);
    else
	showColumnTarget(true);

    connect( this, SIGNAL(settingsChanged()), this, SLOT(configChanged()) );
}

void PublicTransport::serviceProviderSettingsChanged() {
    addState( ServiceProviderSettingsJustChanged );
    if ( m_settings->checkConfig() ) {
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

    int iconExtend = m_icon->size().width();
    
    // Make disabled icon
    switch ( mainIconDisplay ) {
	case DepartureListErrorIcon:
	    if ( m_settings->departureArrivalListType() == DepartureList ) {
		icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			    QList<KIcon>() << KIcon("go-home") << KIcon("go-next"),
			    QSize(iconExtend / 2, iconExtend / 2), iconExtend );
	    } else {
		icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			    QList<KIcon>() << KIcon("go-next") << KIcon("go-home"),
			    QSize(iconExtend / 2, iconExtend / 2), iconExtend );
	    }
	    pixmap = icon.pixmap( iconExtend );
	    pixmap = iconEffect.apply( pixmap, KIconLoader::Small, KIconLoader::DisabledState );
	    icon = KIcon();
	    icon.addPixmap( pixmap, QIcon::Normal );
	    break;

	case DepartureListOkIcon:
	    if ( m_settings->departureArrivalListType() == DepartureList ) {
		icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			    QList<KIcon>() << KIcon("go-home") << KIcon("go-next"),
			    QSize(iconExtend / 2, iconExtend / 2), iconExtend );
	    } else {
		icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			    QList<KIcon>() << KIcon("go-next") << KIcon("go-home"),
			    QSize(iconExtend / 2, iconExtend / 2), iconExtend );
	    }
	    break;

	case JourneyListOkIcon:
	    icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			QList<KIcon>() << KIcon("go-home")
			<< KIcon("go-next-view") << KIcon("public-transport-stop"),
			QSize(iconExtend / 3, iconExtend / 3), iconExtend );
	    break;

	case JourneyListErrorIcon:
	    icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
			QList<KIcon>() << KIcon("go-home")
			<< KIcon("go-next-view") << KIcon("public-transport-stop"),
			QSize(iconExtend / 3, iconExtend / 3), iconExtend );
	    pixmap = icon.pixmap( iconExtend );
	    pixmap = iconEffect.apply( pixmap, KIconLoader::Small, KIconLoader::DisabledState );
	    icon = KIcon();
	    icon.addPixmap( pixmap, QIcon::Normal );
	    break;

	case AbortJourneySearchIcon:
	    icon = KIcon( "edit-delete" );
	    break;

	case GoBackIcon:
	    icon = KIcon( "arrow-left" );
	    break;
    }

    m_icon->setIcon( icon );
}

void PublicTransport::iconClicked() {
    if ( !m_graphicsWidget )
	return;

    switch( m_titleType ) {
	case ShowSearchJourneyLineEdit:
	case ShowSearchJourneyLineEditDisabled:
	    addState( ShowingDepartureArrivalList );
	    break;
	    
	case ShowJourneyListTitle:
	case ShowDepartureArrivalListTitle:
	    showActionButtons();
// 	    showJourneySearch( false );
	    break;
    }
}

void PublicTransport::destroyOverlay() {
    if ( m_overlay ) {
	m_overlay->destroy();
	m_overlay = NULL;
    }
}

void PublicTransport::showActionButtons() {
    m_overlay = new OverlayWidget( m_graphicsWidget, m_mainGraphicsWidget );
    m_overlay->setGeometry( m_graphicsWidget->contentsRect() );

    Plasma::PushButton *btnJourney = new Plasma::PushButton( m_overlay );
    btnJourney->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
    btnJourney->setAction( action("searchJourneys") );
    connect( btnJourney, SIGNAL(clicked()), this, SLOT(destroyOverlay()) );

    Plasma::PushButton *btnShowDepArr = new Plasma::PushButton( m_overlay );
    btnShowDepArr->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
    if ( testState(ShowingJourneyList) ) {
	btnShowDepArr->setAction( updatedAction("backToDepartures") );
    } else {
	if ( m_settings->departureArrivalListType() == DepartureList )
	    btnShowDepArr->setAction( action("showArrivals") );
	else
	    btnShowDepArr->setAction( action("showDepartures") );
    }
    connect( btnShowDepArr, SIGNAL(clicked()), this, SLOT(destroyOverlay()) );

    // Add stop selector if multiple stops are defined
    Plasma::PushButton *btnMultipleStops = NULL;
    if ( m_settings->hasMultipleStops() ) {
	btnMultipleStops = new Plasma::PushButton( m_overlay );
	btnMultipleStops->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
	btnMultipleStops->setIcon( KIcon("public-transport-stop") );
	btnMultipleStops->setZValue( 1000 );
	QMenu *menu = new QMenu( btnMultipleStops->nativeWidget() );
	if ( m_settings->currentStopIndex() >= m_settings->stops().count() ) {
	    kDebug() << "Invalid stop index, using -1, was" << m_settings->currentStopIndex();
	    m_settings->setCurrentStopIndex( -1 );
	}
	
	if ( m_settings->currentStopIndex() == -1 ) {
	    btnMultipleStops->setText( i18n("Show Results For All Stops") );
	} else {
	    btnMultipleStops->setText( i18n("Show Results Only For '%1'",
					    m_settings->stops()[m_settings->currentStopIndex()]) );
	    QAction *action = menu->addAction( KIcon("public-transport-stop"),
			     i18n("Show Results For All Stops") );
	    action->setData( -1 );
	    connect( action, SIGNAL(triggered(bool)), this, SLOT(destroyOverlay()) );
	}

	for ( int i = 0; i < m_settings->stops().count(); ++i ) {
	    if ( i != m_settings->currentStopIndex() ) {
		QAction *action = menu->addAction( KIcon("public-transport-stop"),
			i18n("Show Results Only For '%1'", m_settings->stops()[i]) );
		action->setData( i );
		connect( action, SIGNAL(triggered(bool)), this, SLOT(destroyOverlay()) );
	    }
	}

	connect( menu, SIGNAL(triggered(QAction*)),
		 this, SLOT(setCurrentStopIndex(QAction*)) );
	btnMultipleStops->nativeWidget()->setMenu( menu );
    }
    
    Plasma::PushButton *btnCancel = new Plasma::PushButton( m_overlay );
    btnCancel->setText( i18n("Cancel") );
    btnCancel->setIcon( KIcon("dialog-cancel") );
    btnCancel->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
    connect( btnCancel, SIGNAL(clicked()), this, SLOT(destroyOverlay()) );

    QGraphicsWidget *spacer = new QGraphicsWidget( m_overlay );
    spacer->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    QGraphicsWidget *spacer2 = new QGraphicsWidget( m_overlay );
    spacer2->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

    // Create a seperate layout for the cancel button to have some more space 
    // between the cancel button and the others
    QGraphicsLinearLayout *layoutCancel = new QGraphicsLinearLayout( Qt::Vertical );
    layoutCancel->setContentsMargins( 0, 10, 0, 0 );
    layoutCancel->addItem( btnCancel );
    
    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout( Qt::Vertical );
    layout->setContentsMargins( 15, 10, 15, 10 );
    layout->setContentsMargins( 15, 10, 15, 10 );
    layout->addItem( spacer );
    layout->addItem( btnJourney );
    layout->setAlignment( btnJourney, Qt::AlignCenter );
    layout->addItem( btnShowDepArr );
    layout->setAlignment( btnShowDepArr, Qt::AlignCenter );
    if ( btnMultipleStops ) {
	layout->addItem( btnMultipleStops );
	layout->setAlignment( btnMultipleStops, Qt::AlignCenter );
    }
    layout->addItem( layoutCancel );
    layout->setAlignment( layoutCancel, Qt::AlignCenter );
    layout->addItem( spacer2 );
    m_overlay->setLayout( layout );

    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
	m_overlay->setOpacity( 0 );
	Plasma::Animation *fadeAnimOverlay = fadeAnimation( m_overlay, 1 );

	btnJourney->setOpacity( 0 );
	btnShowDepArr->setOpacity( 0 );
	btnCancel->setOpacity( 0 );
	Plasma::Animation *fadeAnim1 = fadeAnimation( btnJourney, 1 );
	Plasma::Animation *fadeAnim2 = fadeAnimation( btnShowDepArr, 1 );
	Plasma::Animation *fadeAnim3 = fadeAnimation( btnCancel, 1 );
	Plasma::Animation *fadeAnim4 = NULL;
	if ( btnMultipleStops ) {
	    btnMultipleStops->setOpacity( 0 );
	    fadeAnim4 = fadeAnimation( btnMultipleStops, 1 );
	} 

	QSequentialAnimationGroup *seqGroup = new QSequentialAnimationGroup;
	if ( fadeAnimOverlay )
	    seqGroup->addAnimation( fadeAnimOverlay );
	if ( fadeAnim1 ) {
	    fadeAnim1->setProperty( "duration", 150 );
	    seqGroup->addAnimation( fadeAnim1 );
	}
	if ( fadeAnim2 ) {
	    fadeAnim2->setProperty( "duration", 150 );
	    seqGroup->addAnimation( fadeAnim2 );
	}
	if ( fadeAnim4 ) {
	    fadeAnim4->setProperty( "duration", 150 );
	    seqGroup->addAnimation( fadeAnim4 );
	}
	if ( fadeAnim3 ) {
	    fadeAnim3->setProperty( "duration", 150 );
	    seqGroup->addAnimation( fadeAnim3 );
	}
	seqGroup->start( QAbstractAnimation::DeleteWhenStopped );
    #endif
}

void PublicTransport::setCurrentStopIndex( QAction* action ) {
    bool ok;
    int stopIndex = action->data().toInt( &ok );
    if ( !ok ) {
	kDebug() << "Couldn't find stop index";
	return;
    }

    kDebug() << stopIndex;
    disconnectSources();
    m_settings->setCurrentStopIndex( stopIndex );
    clearDepartures();
    reconnectSource();
    configChanged();
}

void PublicTransport::iconCloseClicked() {
    addState( ShowingDepartureArrivalList );
}

bool PublicTransport::parseDate( const QString& sDate, QDate* date ) const {
    if ( sDate == i18n("today") ) {
	*date = QDate::currentDate();
	return true;
    } else if ( sDate == i18n("tomorrow") ) {
	*date = QDate::currentDate().addDays( 1 );
	return true;
    }
	
    bool ok;
    *date = KGlobal::locale()->readDate( sDate, &ok );
    if ( !ok ) {
	// Allow date input without year
	if ( sDate.count('-') == 1 ) { // like 12-31
	    *date = KGlobal::locale()->readDate(
		    QDate::currentDate().toString("yy") + "-" + sDate, &ok );
	} else if ( sDate.count('.') == 1 ) { // like 31.12
	    *date = KGlobal::locale()->readDate(
		    sDate + "." + QDate::currentDate().toString("yy"), &ok );
	} else if ( sDate.count('.') == 2 && sDate.endsWith('.') ) { // like 31.12.
	    *date = KGlobal::locale()->readDate(
		    sDate + QDate::currentDate().toString("yy"), &ok );
	}

	if ( !ok )
	    *date = QDate::currentDate();
    }
    return ok;
}

bool PublicTransport::parseTime( const QString& sTime, QTime *time ) const {
    if ( sTime == i18n("now") ) {
	*time = QTime::currentTime();
	return true;
    }
    
    bool ok;
    *time = KGlobal::locale()->readTime( sTime, &ok );
    if ( !ok )
	*time = QTime::currentTime();
    return ok;
}

void PublicTransport::stopNamePosition( int *posStart, int *len, QString *stop ) {
    QString stopName;
    bool stopIsTarget, timeIsDeparture;
    QDateTime departure;
    parseJourneySearch( m_journeySearch->text(), &stopName, &departure,
			&stopIsTarget, &timeIsDeparture, posStart, len, false );
    if ( stop )
	*stop = stopName;
}

bool PublicTransport::searchForJourneySearchKeywords( const QString& journeySearch,
			    const QStringList &timeKeywordsTomorrow,
			    const QStringList &departureKeywords,
			    const QStringList &arrivalKeywords,
			    QDate *date, QString *stop, bool *timeIsDeparture,
			    int *len ) const {
    if ( stop->startsWith('\"') && stop->endsWith('\"') ) {
	if ( len )
	    *len = stop->length();
	*stop = stop->mid( 1, stop->length() - 2 );
	return false;
    } else if ( stop->trimmed().isEmpty() ) {
	if ( len )
	    *len = 0;
	*stop = "";
	return false;
    }
    
    bool found = false, continueSearch;
    do {
	continueSearch = false;
	
	// If the tomorrow keyword is found, set date to tomorrow
	QStringList wordsStop = journeySearch.split( ' ', QString::SkipEmptyParts );
	QString lastWordInStop = wordsStop.last();
	if ( !lastWordInStop.isEmpty() && timeKeywordsTomorrow.contains(lastWordInStop,
						Qt::CaseInsensitive) ) {
	    *stop = stop->left( stop->length() - lastWordInStop.length() ).trimmed();
	    *date = QDate::currentDate().addDays( 1 );

	    found = continueSearch = true;
	    lastWordInStop = wordsStop.count() >= 2 ? wordsStop[ wordsStop.count() - 2 ] : "";
	}

	// Search for departure / arrival keywords
	if ( !lastWordInStop.isEmpty() ) {
	    if ( departureKeywords.contains(lastWordInStop, Qt::CaseInsensitive) ) {
		// If a departure keyword is found, use given time as departure time
		*stop = stop->left( stop->length() - lastWordInStop.length() ).trimmed();
		*timeIsDeparture = true;
		found = continueSearch = true;
	    } else if ( arrivalKeywords.contains(lastWordInStop, Qt::CaseInsensitive) ) {
		// If an arrival keyword is found, use given time as arrival time
		*stop = stop->left( stop->length() - lastWordInStop.length() ).trimmed();
		*timeIsDeparture = false;
		found = continueSearch = true;
	    }
	}
    } while ( continueSearch );
    
    if ( len )
	*len = stop->length();
    if ( stop->startsWith('\"') && stop->endsWith('\"') )
	*stop = stop->mid( 1, stop->length() - 2 );
    return found;
}

void PublicTransport::parseDateAndTime( const QString& sDateTime, QDateTime* dateTime,
					QDate *alreadyParsedDate ) const {
    QDate date;
    QTime time;
    bool callParseDate = alreadyParsedDate->isNull();

    // Parse date and/or time from sDateTime
    QStringList timeValues = sDateTime.split( QRegExp("\\s|,"), QString::SkipEmptyParts );
    if ( timeValues.count() >= 2 ) {
	if ( callParseDate && !parseDate(timeValues[0], &date)
		    && !parseDate(timeValues[1], &date) )
	    date = QDate::currentDate();
	else
	    date = *alreadyParsedDate;

	if ( !parseTime(timeValues[1], &time) && !parseTime(timeValues[0], &time) )
	    time = QTime::currentTime();
    } else {
	if ( !parseTime(sDateTime, &time) ) {
	    time = QTime::currentTime();
	    if ( callParseDate && !parseDate(sDateTime, &date) )
		date = QDate::currentDate();
	    else
		date = *alreadyParsedDate;
	} else if ( callParseDate )
	    date = QDate::currentDate();
	else
	    date = *alreadyParsedDate;
    }

    *dateTime = QDateTime( date, time );
}

void PublicTransport::splitWordList( const QStringList& wordList, int splitWordPos,
				     QString *leftOfSplitWord, QString *rightOfSplitWord,
				     int excludeWordsFromleft ) {
    *leftOfSplitWord = ( (QStringList)wordList.mid(excludeWordsFromleft,
			splitWordPos - excludeWordsFromleft) ).join( " " );
    *rightOfSplitWord = ( (QStringList)wordList.mid(splitWordPos + 1,
			wordList.count() - splitWordPos) ).join( " " );
}

void PublicTransport::combineDoubleQuotedWords( QStringList* words ) const {
    int quotedStart = -1, quotedEnd = -1;
    for ( int i = 0; i < words->count(); ++i ) {
	if ( words->at(i).startsWith('\"') )
	    quotedStart = i;
	if ( words->at(i).endsWith('\"') ) {
	    quotedEnd = i;
	    break;
	}
    }
    if ( quotedStart != -1 ) {
	if ( quotedEnd == -1 )
	    quotedEnd = words->count() - 1;
	
	// Combine words
	QString combinedWord;
	for ( int i = quotedEnd; i >= quotedStart; --i )
	    combinedWord = words->takeAt( i ) + " " + combinedWord;
	words->insert( quotedStart, combinedWord.trimmed() );
    }
}

bool PublicTransport::parseJourneySearch( const QString& search, QString *stop,
					  QDateTime *departure,
					  bool *stopIsTarget, bool *timeIsDeparture,
					  int *posStart, int *len,
					  bool correctString ) {
    kDebug() << search;
    
    QString searchLine = search;
    *stopIsTarget = true;
    *timeIsDeparture = true;
    int removedWordsFromLeft = 0;

    // Remove double spaces without changing the cursor position or selection
    int selStart = m_journeySearch->nativeWidget()->selectionStart();
    int selLength = m_journeySearch->nativeWidget()->selectedText().length();
    int cursorPos = m_journeySearch->nativeWidget()->cursorPosition();
    cursorPos -= searchLine.left( cursorPos ).count( "  " );
    if ( selStart != -1 ) {
	selStart -= searchLine.left( selStart ).count( "  " );
	selLength -= searchLine.mid( selStart, selLength ).count( "  " );
    }
    
    searchLine = searchLine.replace( "  ", " " );
    m_journeySearch->setText( searchLine );
    m_journeySearch->nativeWidget()->setCursorPosition( cursorPos );
    if ( selStart != -1 )
	m_journeySearch->nativeWidget()->setSelection( selStart, selLength );

    // Get word list
    QStringList words = searchLine.split( ' ', QString::SkipEmptyParts );
    if ( words.isEmpty() )
	return false;

    // Combine words between double quotes to one word 
    // to allow stop names containing keywords.
    combineDoubleQuotedWords( &words );

    // Check if the cursor is inside the (first) two double quotes,
    // to disable autocompletion
    int posQuotes1 = searchLine.indexOf( '\"' );
    int posQuotes2 = searchLine.indexOf( '\"', posQuotes1 + 1 );
    if ( posQuotes2 == -1 )
	posQuotes2 = searchLine.length();
    bool isInsideQuotedString = posQuotes1 == -1
	    ? false : cursorPos > posQuotes1 && cursorPos <= posQuotes2;

    // Localized keywords
    QStringList toKeywords = i18nc("A comma seperated list of keywords for the "
	    "journey search, indicating that a journey TO the given stop should "
	    "be searched. This keyword needs to be placed at the beginning of "
	    "the field.", "to").split(',', QString::SkipEmptyParts);
    QStringList fromKeywords = i18nc("A comma seperated list of keywords for the "
	    "journey search, indicating that a journey FROM the given stop should "
	    "be searched. This keyword needs to be placed at the beginning of the "
	    "field.", "from").split(',', QString::SkipEmptyParts);
    QStringList departureKeywords = i18nc("A comma seperated list of keywords for "
	    "the journey search to indicate that given times are meant as "
	    "departures (default). The order is used for autocompletion.\n"
	    "Note: Keywords should be unique for each meaning.",
	    "departing,depart,departure,dep").split(',', QString::SkipEmptyParts);
    QStringList arrivalKeywords = i18nc("A comma seperated list of keywords for the "
	    "journey search to indicate that given times are meant as arrivals. "
	    "The order is used for autocompletion.\n"
	    "Note: Keywords should be unique for each meaning.",
	    "arriving,arrive,arrival,arr").split(',', QString::SkipEmptyParts);
    QStringList timeKeywordsAt = i18nc("A comma seperated list of keywords for "
	    "the journey search field, indicating that a date/time string follows.\n"
	    "Note: Keywords should be unique for each meaning.",
	    "at").split(',', QString::SkipEmptyParts);
    QStringList timeKeywordsIn = i18nc("A comma seperated list of keywords for "
	    "the journey search field, indicating that a relative time string "
	    "follows.\nNote: Keywords should be unique for each meaning.",
	    "in").split(',', QString::SkipEmptyParts);
    QStringList timeKeywordsTomorrow = i18nc("A comma seperated list of keywords "
	    "for the journey search field, as replacement for tomorrows date.\n"
	    "Note: Keywords should be unique for each meaning.",
	    "tomorrow").split(',', QString::SkipEmptyParts);
    
    // First search for keywords at the beginning of the string ('to' or 'from')
    QString firstWord = words.first();
    if ( toKeywords.contains(firstWord, Qt::CaseInsensitive) ) {
	searchLine = searchLine.mid( firstWord.length() + 1 );
	cursorPos -= firstWord.length() + 1;
	++removedWordsFromLeft;
    } else if ( fromKeywords.contains(firstWord, Qt::CaseInsensitive) ) {
	searchLine = searchLine.mid( firstWord.length() + 1 );
	*stopIsTarget = false; // the given stop is the origin
	cursorPos -= firstWord.length() + 1;
	++removedWordsFromLeft;
    }
    if ( posStart ) {
	QStringList removedWords = (QStringList)words.mid( 0, removedWordsFromLeft );
	if ( removedWords.isEmpty() )
	    *posStart = 0;
	else
	    *posStart = removedWords.join( " " ).length() + 1;
    }
    
    // Do corrections
    if ( correctString && !isInsideQuotedString
		&& m_journeySearch->nativeWidget()->completionMode() !=
		KGlobalSettings::CompletionNone ) {
	selStart = -1;
	selLength = 0;

	int pos = searchLine.lastIndexOf( ' ', cursorPos - 1 );
	int posEnd = searchLine.indexOf( ' ', cursorPos );
	if ( posEnd == -1 )
	    posEnd = searchLine.length();
	QString lastWordBeforeCursor;
	if ( posEnd == cursorPos && pos != -1
		    && !(lastWordBeforeCursor = searchLine.mid(
			pos, posEnd - pos).trimmed()).isEmpty() ) {
	    if ( timeKeywordsAt.contains(lastWordBeforeCursor, Qt::CaseInsensitive) ) {
		// Automatically add the current time after 'at'
		QString formattedTime = KGlobal::locale()->formatTime( QTime::currentTime() );
		searchLine.insert( posEnd, " " + formattedTime );
		selStart = posEnd + 1; // +1 for the added space
		selLength = formattedTime.length();
	    } else if ( timeKeywordsIn.contains(lastWordBeforeCursor, Qt::CaseInsensitive) ) {
		// Automatically add '5 minutes' after 'in'
		QString defaultRelTime = i18nc("The automatically added relative time "
			"string, when the journey search line ends with the keyword 'in'."
			"This should be match by the regular expression for a relative "
			"time, like '(in) 5 minutes'. "
			"That regexp and the keyword ('in') are also localizable. "
			"Don't include the 'in' here.",
			"%1 minutes", 5);
		searchLine.insert( posEnd, " " + defaultRelTime );
		selStart = posEnd + 1; // +1 for the added space
		selLength = 1; // only select the number (5)
	    } else {
		QStringList completionItems;
		completionItems << timeKeywordsAt;
		completionItems << timeKeywordsIn;
		completionItems << timeKeywordsTomorrow;
		completionItems << departureKeywords;
		completionItems << arrivalKeywords;

		KCompletion *comp = m_journeySearch->nativeWidget()->completionObject( false );
		comp->setItems( completionItems );
		comp->setIgnoreCase( true );
		QString completion = comp->makeCompletion( lastWordBeforeCursor );
		setJourneySearchWordCompletion( completion );
	    }
	}
	
	// Select an appropriate substring after inserting something
	if ( selStart != -1 ) {
	    QStringList removedWords = (QStringList)words.mid( 0, removedWordsFromLeft );
	    QString removedPart = removedWords.join( " " ).trimmed();
	    QString correctedSearch;
	    if ( removedPart.isEmpty() ) {
		correctedSearch = searchLine;
	    } else {
		correctedSearch = removedPart + " " + searchLine;
		selStart += removedPart.length() + 1;
	    }
	    m_journeySearch->setText( correctedSearch );
	    m_journeySearch->nativeWidget()->setSelection( selStart, selLength );
	}
    } // Do corrections

    // Now search for keywords inside the string from back
    QStringList parts;
    for ( int i = words.count() - 1; i >= removedWordsFromLeft; --i ) {
	QString word = words[ i ];
	if ( timeKeywordsAt.contains(word, Qt::CaseInsensitive) ) {
	    // An 'at' keyword was found at position i
	    QString sDeparture;
	    splitWordList( words, i, stop, &sDeparture, removedWordsFromLeft );

	    // Search for keywords before 'at'
	    QDate date;
	    searchForJourneySearchKeywords( *stop, timeKeywordsTomorrow,
					    departureKeywords, arrivalKeywords,
					    &date, stop, timeIsDeparture, len );
	    
	    // Parse date and/or time from the string after 'at'
	    parseDateAndTime( sDeparture, departure, &date );
	    return true;
	} else if ( timeKeywordsIn.contains(word, Qt::CaseInsensitive) ) {
	    // An 'in' keyword was found at position i
	    QString sDeparture;
	    splitWordList( words, i, stop, &sDeparture, removedWordsFromLeft );

	    QRegExp rx( i18nc("This is a regular expression used to match a string "
			    "after the 'in' keyword in the journey search line. "
			    "The english version matches strings like '5 mins.', "
			    "'1 minute', ... '\\d+' stands for at least one digit, "
			    "'\\.' is just a point, a '?' after a character means "
			    "that it's optional (eg. the 's' in 'mins?' is optional "
			    "to match singular and plural forms). Normally you will "
			    "only have to translate 'mins?' and 'minutes?'."
			    "The regexp must include one pair of matching parantheses, "
			    "that match an int (the number of minutes from now). "
			    "Note: '(?:...)' are non-matching parantheses.",
			    "(\\d+)\\s+(?:mins?\\.?|minutes?)"), Qt::CaseInsensitive );

	    // Match the regexp and extract the relative datetime value
	    int pos = rx.indexIn( sDeparture );
	    if ( pos != -1 ) {
		int minutes = rx.cap( 1 ).toInt();
		
		// Search for keywords before 'in'
		QDate date = QDate::currentDate();
		searchForJourneySearchKeywords( *stop, timeKeywordsTomorrow,
						departureKeywords, arrivalKeywords,
						&date, stop, timeIsDeparture, len );
		*departure = QDateTime( date, QTime::currentTime().addSecs(minutes * 60) );
		return true;
	    }
	}
    }
    
    *stop = searchLine;
    
    // Search for keywords at the end of the string
    QDate date = QDate::currentDate();
    searchForJourneySearchKeywords( *stop, timeKeywordsTomorrow,
				    departureKeywords, arrivalKeywords,
				    &date, stop, timeIsDeparture, len );
    *departure = QDateTime( date, QTime::currentTime() );
    return false;
}

void PublicTransport::setJourneySearchWordCompletion( const QString& match ) {
    kDebug() << "MATCH" << match;
    if ( match.isEmpty() )
	return;

    int posStart = m_journeySearch->text().lastIndexOf( ' ',
	    m_journeySearch->nativeWidget()->cursorPosition() - 1 ) + 1;
    if ( posStart == -1 )
	posStart = 0;

    int posEnd = m_journeySearch->text().indexOf( ' ',
	    m_journeySearch->nativeWidget()->cursorPosition() );
    if ( posEnd == -1 )
	posEnd = m_journeySearch->text().length();

    int len = posEnd - posStart;
    if ( len == m_journeySearch->text().length() ) {
	kDebug() << "I'm not going to replace the whole word.";
	return; // Don't replace the whole word
    }
    kDebug() << "Current Word" << m_journeySearch->text().mid( posStart, len ) << posStart << len << m_journeySearch->nativeWidget()->cursorPosition();
    
    m_journeySearch->nativeWidget()->setText( m_journeySearch->text().replace(posStart, len, match) );
    m_journeySearch->nativeWidget()->setSelection( posStart + len, match.length() - len );
}

void PublicTransport::setJourneySearchStopNameCompletion( const QString &match ) {
    kDebug() << "MATCH" << match;
    if ( match.isEmpty() )
	return;
    
    int stopNamePosStart, stopNameLen;
    stopNamePosition( &stopNamePosStart, &stopNameLen );
    kDebug() << "STOPNAME =" << m_journeySearch->text().mid( stopNamePosStart, stopNameLen );
    
    int selStart = m_journeySearch->nativeWidget()->selectionStart();
    if ( selStart == -1 )
	selStart = m_journeySearch->nativeWidget()->cursorPosition();
    bool stopNameChanged = selStart > stopNamePosStart
	    && selStart + m_journeySearch->nativeWidget()->selectedText().length()
	    <= stopNamePosStart + stopNameLen;
    int posStart, len;
    if ( stopNameChanged ) {
	posStart = stopNamePosStart;
	len = stopNameLen;
	
	m_journeySearch->setText( m_journeySearch->text().replace(posStart, len, match) );
	m_journeySearch->nativeWidget()->setSelection( posStart + len, match.length() - len );
    }
}

void PublicTransport::journeySearchInputFinished() {
    clearJourneys();
    addState( ShowingJourneyList );

    if ( !m_recentJourneySearches.contains(m_journeySearch->text(), Qt::CaseInsensitive) )
	m_recentJourneySearches.prepend( m_journeySearch->text() );
    while ( m_recentJourneySearches.count() > MAX_RECENT_JOURNEY_SEARCHES )
	m_recentJourneySearches.takeLast();
    
    QString stop;
    QDateTime departure;
    bool stopIsTarget;
    bool timeIsDeparture;
    parseJourneySearch( m_journeySearch->text(), &stop, &departure,
			&stopIsTarget, &timeIsDeparture );
    reconnectJourneySource( stop, departure, stopIsTarget, timeIsDeparture );
}

void PublicTransport::journeySearchInputEdited( const QString &newText ) {
    QString stop;
    QDateTime departure;
    bool stopIsTarget;
    bool timeIsDeparture;
    
    // Only correct the input string if letter were added
    // (eg. not after pressing backspace).
    m_lettersAddedToJourneySearchLine = newText.length() > m_journeySearchLastTextLength;
    
    parseJourneySearch( newText, &stop, &departure,
			&stopIsTarget, &timeIsDeparture, 0, 0, m_lettersAddedToJourneySearchLine );
    m_journeySearchLastTextLength = m_journeySearch->text().length()
	    - m_journeySearch->nativeWidget()->selectedText().length();
	    
    reconnectJourneySource( stop, departure, stopIsTarget, timeIsDeparture, true );
}

QGraphicsLayout *PublicTransport::createLayoutTitle( TitleType titleType ) {
    QGraphicsGridLayout *layoutTop = new QGraphicsGridLayout;
    switch( titleType ) {
	case ShowDepartureArrivalListTitle:
	    m_icon->setVisible( true );
	    m_label->setVisible( true );
	    m_labelInfo->setVisible( true );
	    layoutTop->addItem( m_icon, 0, 0 );
	    layoutTop->addItem( m_label, 0, 1 );

	    // To make the link clickable (don't let plasma eat click events for dragging)
	    m_labelInfo->installSceneEventFilter( this );
	    break;
	    
	case ShowSearchJourneyLineEdit:
	case ShowSearchJourneyLineEditDisabled:
	    m_icon->setVisible( true );
	    m_journeySearch->setVisible( true );
	    layoutTop->addItem( m_icon, 0, 0 );
	    layoutTop->addItem( m_journeySearch, 0, 1 );
	    layoutTop->addItem( m_btnLastJourneySearches, 0, 2 );
	    break;

	case ShowJourneyListTitle:
	    m_icon->setVisible( true );
	    m_label->setVisible( true );
	    m_iconClose->setVisible( true );
	    layoutTop->addItem( m_icon, 0, 0 );
	    layoutTop->addItem( m_label, 0, 1 );
	    layoutTop->addItem( m_iconClose, 0, 2 );
	    break;
    }
    return layoutTop;
}

void PublicTransport::possibleStopClicked( const QModelIndex &modelIndex ) {
    // Insert the clicked stop into the journey search line,
    // don't override keywords and other infos
    int posStart, len;
    stopNamePosition( &posStart, &len );

    QString quotedStop = "\"" + modelIndex.data().toString() + "\"";
    if ( posStart == -1 ) {
	m_journeySearch->nativeWidget()->setText( quotedStop );
    } else {
	m_journeySearch->nativeWidget()->setText( m_journeySearch->text().replace(
		posStart, len, quotedStop) );
    }
    m_journeySearch->setFocus();
}

void PublicTransport::possibleStopDoubleClicked( const QModelIndex& modelIndex ) {
    Q_UNUSED( modelIndex );
    journeySearchInputFinished();
}

void PublicTransport::useCurrentPlasmaTheme() {
    QFont font = Plasma::Theme::defaultTheme()->font( Plasma::Theme::DefaultFont );
    int newPixelSize = qCeil((float)font.pixelSize() * 1.4f);
    if ( newPixelSize > 1 )
	font.setPixelSize( newPixelSize );
    font.setBold( true );
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
    setDepartureArrivalListType( m_settings->departureArrivalListType() );
}

QGraphicsWidget* PublicTransport::graphicsWidget() {
    if ( !m_graphicsWidget ) {
	m_graphicsWidget = new QGraphicsWidget( this );
        m_graphicsWidget->setMinimumSize( 250, 150 );
        m_graphicsWidget->setPreferredSize( 400, 300 );
	
	// Create a child graphics widget, eg. to apply a blur effect to it
	// but not to an overlay widget (which then gets a child of m_graphicsWidget).
	m_mainGraphicsWidget = new QGraphicsWidget( m_graphicsWidget );
	m_mainGraphicsWidget->setSizePolicy( QSizePolicy::Expanding,
					     QSizePolicy::Expanding );
	QGraphicsLinearLayout *mainLayout = new QGraphicsLinearLayout( Qt::Vertical );
	mainLayout->setContentsMargins( 0, 0, 0, 0 );
// 	mainLayout->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
	mainLayout->addItem( m_mainGraphicsWidget );
	m_graphicsWidget->setLayout( mainLayout );

	int iconExtend = 32 * m_settings->sizeFactor();
	
	m_iconClose = new Plasma::IconWidget;
	m_iconClose->setIcon("window-close");
	m_iconClose->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
	m_iconClose->setMinimumSize( iconExtend, iconExtend );
	m_iconClose->setMaximumSize( iconExtend, iconExtend );
	connect( m_iconClose, SIGNAL(clicked()), this, SLOT(iconCloseClicked()) );

	m_icon = new Plasma::IconWidget;
	m_icon->setIcon( "public-transport-stop" );
	m_icon->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
	m_icon->setMinimumSize( iconExtend, iconExtend );
	m_icon->setMaximumSize( iconExtend, iconExtend );
	connect( m_icon, SIGNAL(clicked()), this, SLOT(iconClicked()) );

	m_label = new Plasma::Label;
	m_label->setAlignment( Qt::AlignVCenter | Qt::AlignLeft );
// 	m_label->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Maximum,
// 				QSizePolicy::Label );
	QLabel *label = m_label->nativeWidget();
	label->setTextInteractionFlags( Qt::LinksAccessibleByMouse );
// 	label->setWordWrap( true );

	m_labelInfo = new Plasma::Label;
	m_labelInfo->setAlignment( Qt::AlignVCenter | Qt::AlignRight );
// 	m_labelInfo->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred,
// 				    QSizePolicy::Label );
	connect( m_labelInfo, SIGNAL(linkActivated(QString)),
		 this, SLOT(infoLabelLinkActivated(QString)) );
	QLabel *labelInfo = m_labelInfo->nativeWidget();
	labelInfo->setOpenExternalLinks( true );
	labelInfo->setWordWrap( true );

	m_btnLastJourneySearches = new Plasma::ToolButton;
	m_btnLastJourneySearches->setIcon( KIcon("document-open-recent") );
	m_btnLastJourneySearches->setToolTip( i18n("Use a recent journey search") );
	m_btnLastJourneySearches->nativeWidget()->setPopupMode( QToolButton::InstantPopup );

	m_journeySearch = new Plasma::LineEdit;
	m_journeySearch->setToolTip( i18nc("This should match the localized keywords.",
		"Type a <b>target stop</b> or <b>journey request</b>."
		"<br><br> <b>Samples:</b><br>"
		"&nbsp;&nbsp;&bull; <i>To target in 15 mins</i><br>"
		"&nbsp;&nbsp;&bull; <i>From origin arriving tomorrow at 18:00</i><br>"
		"&nbsp;&nbsp;&bull; <i>Target at 6:00 2010-03-07</i>"/*, sColor*/) );
	m_journeySearch->installEventFilter( this ); // Handle up/down keys (selecting stop suggestions)
	KLineEdit *journeySearch = m_journeySearch->nativeWidget();
	journeySearch->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
	journeySearch->setClickMessage( i18n("Target stop name or journey request") );

	m_labelJourneysNotSupported = new Plasma::Label;
	m_labelJourneysNotSupported->setAlignment( Qt::AlignCenter );
	m_labelJourneysNotSupported->setSizePolicy( QSizePolicy::Expanding,
						    QSizePolicy::Expanding, QSizePolicy::Label );
	m_labelJourneysNotSupported->setText( i18n("Journey searches aren't supported "
						   "by the currently used service provider "
						   "or it's accessor.") );
	QLabel *labelJourneysNotSupported = m_labelJourneysNotSupported->nativeWidget();
	labelJourneysNotSupported->setWordWrap( true );

	m_listStopsSuggestions = new Plasma::TreeView( m_mainGraphicsWidget );
	m_listStopsSuggestions->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
	m_listStopsSuggestions->nativeWidget()->setRootIsDecorated( false );
	m_listStopsSuggestions->nativeWidget()->setHeaderHidden( true );
	m_listStopsSuggestions->nativeWidget()->setAlternatingRowColors( true );
	m_listStopsSuggestions->nativeWidget()->setEditTriggers( QAbstractItemView::NoEditTriggers );
	#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
	    m_listStopsSuggestions->setOpacity( 0 );
	#else
	    m_listStopsSuggestions->hide();
	#endif

	m_journeySearch->nativeWidget()->setCompletionMode( KGlobalSettings::CompletionAuto );
	m_journeySearch->nativeWidget()->setCompletionModeDisabled(
		KGlobalSettings::CompletionMan );
	m_journeySearch->nativeWidget()->setCompletionModeDisabled(
		KGlobalSettings::CompletionPopup );
	m_journeySearch->nativeWidget()->setCompletionModeDisabled(
		KGlobalSettings::CompletionPopupAuto );
	m_journeySearch->nativeWidget()->setCompletionModeDisabled(
		KGlobalSettings::CompletionShell );
	KCompletion *completion = m_journeySearch->nativeWidget()->completionObject( false );
	completion->setIgnoreCase( true );
	
	connect( m_journeySearch, SIGNAL(returnPressed()),
		 this, SLOT(journeySearchInputFinished()) );
	connect( m_journeySearch, SIGNAL(textEdited(QString)),
		 this, SLOT(journeySearchInputEdited(QString)) );
	connect( m_listStopsSuggestions->nativeWidget(), SIGNAL(clicked(QModelIndex)),
		 this, SLOT(possibleStopClicked(QModelIndex)) );
	connect( m_listStopsSuggestions->nativeWidget(), SIGNAL(doubleClicked(QModelIndex)),
		 this, SLOT(possibleStopDoubleClicked(QModelIndex)) );

	// Create treeview
	m_treeView = new Plasma::TreeView( m_mainGraphicsWidget );
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
	treeView->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
	treeView->header()->setCascadingSectionResizes( true );
	treeView->header()->setResizeMode( QHeaderView::Interactive );
	treeView->header()->setSortIndicator( 2, Qt::AscendingOrder );
	treeView->header()->setContextMenuPolicy( Qt::CustomContextMenu );
	treeView->setContextMenuPolicy( Qt::CustomContextMenu );
	HtmlDelegate *htmlDelegate = new HtmlDelegate;
	treeView->setItemDelegate( htmlDelegate );
	connect( treeView, SIGNAL(customContextMenuRequested(const QPoint &)),
		 this, SLOT(showDepartureContextMenu(const QPoint &)) );
	connect( treeView->header(), SIGNAL(customContextMenuRequested(const QPoint &)),
		this, SLOT(showHeaderContextMenu(const QPoint &)) );

	if ( KGlobalSettings::singleClick() ) {
	    connect( treeView, SIGNAL(clicked(const QModelIndex &)),
		     this, SLOT(doubleClickedDepartureItem(const QModelIndex &)) );
	} else {
	    connect( treeView, SIGNAL(doubleClicked(const QModelIndex &)),
		     this, SLOT(doubleClickedDepartureItem(const QModelIndex &)) );
	} 

	if ( !m_model )
	    createModels();
	m_treeView->setModel( m_model );
	treeView->header()->setStretchLastSection( false );
	treeView->header()->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	treeView->header()->resizeSection( 0, 60 );
	connect ( treeView->header(), SIGNAL(sectionResized(int,int,int)),
		  this, SLOT(treeViewSectionResized(int,int,int)) );

	QGraphicsLinearLayout *layout = new QGraphicsLinearLayout( Qt::Vertical );
	layout->setContentsMargins( 0, 0, 0, 0 );
	layout->setSpacing( 0 );

	QGraphicsGridLayout *layoutTop = new QGraphicsGridLayout; // = createLayoutTitle();
	layout->addItem( layoutTop );
	layout->addItem( m_treeView );
	layout->addItem( m_labelInfo );
	layout->setAlignment( m_labelInfo, Qt::AlignRight | Qt::AlignVCenter );
	m_mainGraphicsWidget->setLayout( layout );

	registerAsDragHandle( m_mainGraphicsWidget );
	registerAsDragHandle( m_label );
// 	registerAsDragHandle( m_labelInfo );
// 	registerAsDragHandle( m_treeView );

	useCurrentPlasmaTheme();
    }

    return m_graphicsWidget;
}

void PublicTransport::infoLabelLinkActivated( const QString& link ) {
    KToolInvocation::invokeBrowser( link );
}

bool PublicTransport::sceneEventFilter( QGraphicsItem* watched, QEvent* event ) {
    if ( watched && watched == m_labelInfo ) {
	if ( event->type() == QEvent::GraphicsSceneMousePress ) {
	    return true; // To make links clickable, otherwise Plasma takes all
			 // clicks to move the applet
	}
    }
    
    return Plasma::Applet::sceneEventFilter( watched, event );
}

bool PublicTransport::eventFilter( QObject *watched, QEvent *event ) {
    if ( watched && watched == m_journeySearch && m_listStopsSuggestions
		&& m_listStopsSuggestions->model()
		&& m_listStopsSuggestions->model()->rowCount() > 0 ) {
	QKeyEvent *keyEvent;
	QModelIndex curIndex;
	int row;
	switch ( event->type() ) {
	    case QEvent::KeyPress:
		keyEvent = dynamic_cast<QKeyEvent*>( event );
		curIndex = m_listStopsSuggestions->nativeWidget()->currentIndex();
		
		if ( keyEvent->key() == Qt::Key_Up ) {
		    if ( !curIndex.isValid() ) {
			curIndex = m_listStopsSuggestions->nativeWidget()->model()->index( 0, 0 );
			m_listStopsSuggestions->nativeWidget()->setCurrentIndex( curIndex );
			possibleStopClicked( curIndex );
			return true;
		    } else {
			row = curIndex.row();
			if ( row >= 1 ) {
			    m_listStopsSuggestions->nativeWidget()->setCurrentIndex(
				    m_listStopsSuggestions->model()->index(row - 1,
				    curIndex.column(), curIndex.parent()) );
			    possibleStopClicked( m_listStopsSuggestions->nativeWidget()->currentIndex() );
			    return true;
			} else
			    return false;
		    }
		} else if ( keyEvent->key() == Qt::Key_Down ) {
		    if ( !curIndex.isValid() ) {
			curIndex = m_listStopsSuggestions->nativeWidget()->model()->index( 0, 0 );
			m_listStopsSuggestions->nativeWidget()->setCurrentIndex( curIndex );
			possibleStopClicked( curIndex );
			return true;
		    } else {
			row = curIndex.row();
			if ( row < m_listStopsSuggestions->model()->rowCount() - 1 ) {
			    m_listStopsSuggestions->nativeWidget()->setCurrentIndex(
				    m_listStopsSuggestions->model()->index(row + 1,
				    curIndex.column(), curIndex.parent()) );
			    possibleStopClicked( m_listStopsSuggestions->nativeWidget()->currentIndex() );
			    return true;
			} else
			    return false;
		    }
		} else
		    return Plasma::PopupApplet::eventFilter( watched, event );
		break;
	    
	    default:
		return Plasma::PopupApplet::eventFilter( watched, event );
	}
    } else
	return Plasma::PopupApplet::eventFilter( watched, event );
}

void PublicTransport::constraintsEvent( Plasma::Constraints /*constraints*/ ) {
    if ( !m_graphicsWidget ) {
        graphicsWidget();
    }

//     if ( (constraints|Plasma::FormFactorConstraint || constraints|Plasma::SizeConstraint) &&
//         layout()->itemAt(0) != m_graphicsWidget ) {
//     }
}

void PublicTransport::createConfigurationInterface( KConfigDialog* parent ) {
    m_settings->createConfigurationInterface( parent, m_stopNameValid );
}

QString PublicTransport::nameForTimetableColumn( TimetableColumn timetableColumn,
						 DepartureArrivalListType departureArrivalListType ) {
    if ( departureArrivalListType == _UseCurrentDepartureArrivalListType )
	departureArrivalListType = m_settings->departureArrivalListType();

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

void PublicTransport::departureArrivalListTypeChanged(
		DepartureArrivalListType departureArrivalListType ) {
    setDepartureArrivalListType( departureArrivalListType );
}

void PublicTransport::setDepartureArrivalListType( DepartureArrivalListType departureArrivalListType ) {
    QBrush textBrush = QBrush( Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor) );
    QStringList titles;

    foreach( TimetableColumn column, m_departureViewColumns )
	titles << nameForTimetableColumn(column, departureArrivalListType);
    m_model->setHorizontalHeaderLabels( titles );

//     int i = 0;
//     foreach( TimetableColumn column, m_departureViewColumns ) {
    for ( int i = 0; i < m_departureViewColumns.count(); ++i )
	m_model->horizontalHeaderItem(i)->setForeground( textBrush );
// 	if ( column == LineStringColumn )
// 	    m_model->horizontalHeaderItem(i)->setTextAlignment( Qt::AlignRight );
// 	++i;
//     }
}

void PublicTransport::initJourneyList() {
    QBrush textBrush = QBrush( Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor) );
    QStringList titles;

    foreach( TimetableColumn column, m_journeyViewColumns )
	titles << nameForTimetableColumn(column);
    m_modelJourneys->setHorizontalHeaderLabels( titles );

    for( int i = 0; i < m_journeyViewColumns.count(); ++i )
	m_modelJourneys->horizontalHeaderItem(i)->setForeground( textBrush );
}

void PublicTransport::setTitleType( TitleType titleType ) {
    if ( !m_graphicsWidget )//|| m_titleType == titleType )
	return;

    QGraphicsLinearLayout *layoutMain =
	    dynamic_cast<QGraphicsLinearLayout*>( m_mainGraphicsWidget->layout() );
    QGraphicsGridLayout *layoutTop =
	    dynamic_cast<QGraphicsGridLayout*>( layoutMain->itemAt(0) );
    Q_ASSERT( layoutTop );
    Q_ASSERT( layoutMain );
    
    QGraphicsWidget *w = static_cast< QGraphicsWidget* >( layoutMain->itemAt(1) );
    if ( w ) {
	#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
	    startFadeAnimation( w, 0 );
	#else
	    w->hide();
	#endif
    }
    
    // Hide widgets from the old title layout
    for ( int col = 0; col < layoutTop->columnCount(); ++col ) {
	for ( int row = 0; row < layoutTop->rowCount(); ++row ) {
	    QGraphicsWidget *w = static_cast< QGraphicsWidget* >( layoutTop->itemAt(row, col) );
	    if ( w ) {
		#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
		    startFadeAnimation( w, 0 );
		#else
		    w->hide();
		#endif
	    }
	}
    }
    
    // Delete old title layout
    layoutMain->removeItem( layoutTop );
    delete layoutTop;

    // Setup new main layout
    QGraphicsLinearLayout *layoutMainNew = new QGraphicsLinearLayout( Qt::Vertical );
    layoutMainNew->setContentsMargins( 0, 0, 0, 0 );
    layoutMainNew->setSpacing( 0 );
//     layoutMainNew->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    m_mainGraphicsWidget->setLayout( layoutMainNew );
    layoutMain = NULL;
    
    // Add new title layout
    QGraphicsLayout *layoutTopNew = createLayoutTitle( titleType );
    if ( layoutTopNew ) {
	layoutMainNew->addItem( layoutTopNew );
	layoutMainNew->setAlignment( layoutTopNew, Qt::AlignTop );
    }

    // Setup the new main layout
    switch( titleType ) {
	case ShowDepartureArrivalListTitle:
	    setMainIconDisplay( testState(ReceivedValidDepartureData)
		    ? DepartureListOkIcon : DepartureListErrorIcon );
	    m_icon->setToolTip( i18n("Search journeys to or from the home stop") );
	    m_label->setText( titleText() );
	    m_labelInfo->setToolTip( courtesyToolTip() );
	    m_labelInfo->setText( infoText() );
		    
	    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
		startFadeAnimation( m_icon, 1 );
		startFadeAnimation( m_label, 1 );
		startFadeAnimation( m_labelInfo, 1 );
		startFadeAnimation( m_treeView, 1 );
	    #else
		m_treeView->show();
	    #endif
	    layoutMainNew->addItem( m_treeView );
	    break;

	case ShowSearchJourneyLineEdit:
	    setMainIconDisplay( AbortJourneySearchIcon );
	    m_icon->setToolTip( i18n("Abort search for journeys to or from the home stop") );

	    m_journeySearch->setEnabled( true );
	    
	    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
		startFadeAnimation( m_icon, 1 );
		startFadeAnimation( m_journeySearch, 1 );
		startFadeAnimation( m_listStopsSuggestions, 1 );
		startFadeAnimation( m_btnLastJourneySearches, 1 );
	    #else
		m_listStopsSuggestions->show();
		m_btnLastJourneySearches->show();
	    #endif
	    layoutMainNew->addItem( m_listStopsSuggestions );
	    break;
	    
	case ShowSearchJourneyLineEditDisabled:
	    setMainIconDisplay( AbortJourneySearchIcon );
	    m_icon->setToolTip( i18n("Abort search for journeys to or from the home stop") );

	    m_journeySearch->setEnabled( false );
	    m_btnLastJourneySearches->setEnabled( false );
	    
	    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
		startFadeAnimation( m_icon, 1 );
		startFadeAnimation( m_btnLastJourneySearches, 1 );
		startFadeAnimation( m_labelJourneysNotSupported, 1 );
	    #else
		m_btnLastJourneySearches->show();
		m_labelJourneysNotSupported->show();
	    #endif
	    layoutMainNew->addItem( m_labelJourneysNotSupported );
	    break;

	case ShowJourneyListTitle:
	    setMainIconDisplay( testState(ReceivedValidJourneyData)
		    ? JourneyListOkIcon : JourneyListErrorIcon );
	    m_icon->setToolTip( i18n("Search journeys to or from the home stop") );
	    m_iconClose->setToolTip( i18n("Show departures / arrivals") );
	    m_label->setText( i18n("<b>Journeys</b>") );
	    
	    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
		startFadeAnimation( m_label, 1 );
		startFadeAnimation( m_icon, 1 );
		startFadeAnimation( m_iconClose, 1 );
		startFadeAnimation( m_treeView, 1 );
	    #else
		m_treeView->show();
	    #endif
	    layoutMainNew->addItem( m_treeView );
	    break;
    }

    layoutMainNew->addItem( m_labelInfo );
    layoutMainNew->setAlignment( m_labelInfo, Qt::AlignRight | Qt::AlignVCenter );

    if ( titleType == ShowSearchJourneyLineEdit )
	m_journeySearch->setFocus();
    if ( titleType != ShowDepartureArrivalListTitle )
	m_journeySearch->nativeWidget()->selectAll();

    m_titleType = titleType;
}

void PublicTransport::recentJourneyActionTriggered( QAction* action ) {
    if ( action->data().isValid() && action->data().toBool() ) {
	// Clear recent journey list
	m_recentJourneySearches.clear();
	m_btnLastJourneySearches->setEnabled( false );
    } else
	m_journeySearch->setText( action->text() );

    m_journeySearch->setFocus();
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
    QMenu *menu;

    switch ( state ) {
	case ShowingDepartureArrivalList:
	    setTitleType( ShowDepartureArrivalListTitle );
	    m_icon->setToolTip( i18n("Search journeys to or from the home stop") );
	    m_treeView->setModel( m_model );
	    m_treeView->nativeWidget()->setIconSize(
		    QSize(16 * m_settings->sizeFactor(), 16 * m_settings->sizeFactor()) );
	    geometryChanged();
	    setBusy( testState(WaitingForDepartureData) );
	    disconnectJourneySource();
	    
	    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
	    setAssociatedApplicationUrls( KUrl::List() << m_urlDeparturesArrivals );
	    #endif
	    unsetStates( QList<AppletState>() << ShowingJourneyList
		    << ShowingJourneySearch << ShowingJourneysNotSupported );
	    break;

	case ShowingJourneyList:
	    setTitleType( ShowJourneyListTitle );
	    m_icon->setToolTip( i18n("Quick configuration and journey search") );
	    m_treeView->setModel( m_modelJourneys );
	    m_treeView->nativeWidget()->setIconSize(
		    QSize(32 * m_settings->sizeFactor(), 32 * m_settings->sizeFactor()) );
	    setBusy( testState(WaitingForJourneyData) );
	    
	    #if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
	    setAssociatedApplicationUrls( KUrl::List() << m_urlJourneys );
	    #endif
	    unsetStates( QList<AppletState>() << ShowingDepartureArrivalList
		    << ShowingJourneySearch << ShowingJourneysNotSupported );
	    break;

	case ShowingJourneySearch:
	    setTitleType( ShowSearchJourneyLineEdit );
	    
	    if ( m_recentJourneySearches.isEmpty() ) {
		m_btnLastJourneySearches->setEnabled( false );
	    } else {
		menu = new QMenu( m_btnLastJourneySearches->nativeWidget() );
		foreach ( QString recent, m_recentJourneySearches )
		    menu->addAction( recent );
		menu->addSeparator();
		menu->addAction( KIcon("edit-clear-list"),
				 i18n("&Clear list") )->setData( true );
		connect( menu, SIGNAL(triggered(QAction*)),
			this, SLOT(recentJourneyActionTriggered(QAction*)) );
		m_btnLastJourneySearches->nativeWidget()->setMenu( menu );
		m_btnLastJourneySearches->setEnabled( true );
	    }

	    m_icon->setToolTip( i18n("Go back to the departure / arrival list") );
	    setBusy( false);

	    unsetStates( QList<AppletState>() << ShowingDepartureArrivalList
		    << ShowingJourneyList << ShowingJourneysNotSupported );
	    break;

	case ShowingJourneysNotSupported:
	    setTitleType( ShowSearchJourneyLineEditDisabled );
	    m_icon->setToolTip( i18n("Go back to the departure / arrival list") );
	    setBusy( false);
	    
	    unsetStates( QList<AppletState>() << ShowingDepartureArrivalList
		    << ShowingJourneyList << ShowingJourneySearch );
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
	    setDepartureArrivalListType( m_settings->departureArrivalListType() );
	    break;

	case ConfigDialogShown:
// 	    m_dataSourceTester->setTestSource("");
	    break;

	case SettingsJustChanged:
	case ServiceProviderSettingsJustChanged:
	case ShowingDepartureArrivalList:
	case ShowingJourneySearch:
	case ShowingJourneysNotSupported:
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
    m_settings->setShowHeader( false );
}

void PublicTransport::showHeader ( bool ) {
    QTreeView *treeView = m_treeView->nativeWidget();
    treeView->header()->setVisible( true );
    m_settings->setShowHeader( true );
}

void PublicTransport::hideColumnTarget( bool ) {
    QTreeView *treeView = m_treeView->nativeWidget();
    treeView->hideColumn( 1 );
    treeView->header()->setStretchLastSection( true );
    m_settings->setHideColumnTarget( true );
}

void PublicTransport::showColumnTarget( bool ) {
    QTreeView *treeView = m_treeView->nativeWidget();
    treeView->showColumn( 1 );

    m_settings->setHideColumnTarget( false );
    geometryChanged();
}

void PublicTransport::toggleExpanded( bool ) {
    doubleClickedDepartureItem( m_clickedItemIndex );
}

void PublicTransport::doubleClickedDepartureItem( const QModelIndex &modelIndex ) {
//     if( modelIndex.parent().isValid() )
// 	return; // Only expand top level items

    QModelIndex firstIndex;
    if ( testState(ShowingDepartureArrivalList) )
	firstIndex = m_model->index( modelIndex.row(), 0, modelIndex.parent() );
    else
	firstIndex = m_modelJourneys->index( modelIndex.row(), 0, modelIndex.parent() );

    QTreeView* treeView = m_treeView->nativeWidget();
    if ( treeView->isExpanded(firstIndex) )
	treeView->collapse( firstIndex );
    else
	treeView->expand( firstIndex );
}

QAction* PublicTransport::updatedAction ( const QString& actionName ) {
    QAction *a = action(actionName);
    if ( a == NULL ) {
	if ( actionName == "seperator" ) {
	    a = new QAction(this);
	    a->setSeparator(true);
	    return a;
	} else {
	    kDebug() << "Action not found:" << actionName;
	    return NULL;
	}
    }

    QAbstractItemModel *model;
    if ( testState(ShowingDepartureArrivalList) )
	model = m_model;
    else
	model = m_modelJourneys;

    QString line, direction;
    if ( m_clickedItemIndex.isValid() ) {
	QStandardItem *itemLine = m_model->item( m_clickedItemIndex.row(), 0 );
	line = itemLine->text();
	QStandardItem *itemDirection = m_model->item( m_clickedItemIndex.row(), 1 );
	direction = itemDirection->text();
    }

    if ( actionName == "backToDepartures" ) {
	a->setText( m_settings->departureArrivalListType() == DepartureList
	    ? i18n("Back to &Departure List")
	    : i18n("Back to &Arrival List") );
    } else if ( actionName == "toggleExpanded" ) {
	if ( m_treeView->nativeWidget()->isExpanded( model->index(m_clickedItemIndex.row(), 0) ) ) {
	    a->setText( i18n("Hide Additional &Information") );
	    a->setIcon( KIcon("arrow-up") );
	} else {
	    a->setText( i18n("Show Additional &Information") );
	    a->setIcon( KIcon("arrow-down") );
	}
    } else if ( actionName == "removeAlarmForDeparture" ) {
	a->setText( m_settings->departureArrivalListType() == DepartureList
		? i18n("Remove &Alarm for This Departure")
		: i18n("Remove &Alarm for This Arrival") );
    } else if ( actionName == "setAlarmForDeparture" ) {
	a->setText( m_settings->departureArrivalListType() == DepartureList
		? i18n("Set &Alarm for This Departure")
		: i18n("Set &Alarm for This Arrival") );
    } else if ( actionName == "filterOutByVehicleType" ) {
	QStandardItem *itemDeparture = m_model->item( m_clickedItemIndex.row(), 2 );
	VehicleType vehicleType = static_cast<VehicleType>(
		itemDeparture->data( VehicleTypeRole ).toInt() );
	if ( vehicleType == Unknown ) {
	    a->setIcon( KIcon("view-filter") );
	    a->setText( i18n("By &Vehicle Type (unknown)") );
	} else {
	    a->setIcon( Global::makeOverlayIcon(KIcon("view-filter"),
			Global::iconFromVehicleType(vehicleType)) );
	    a->setText( i18n("By &Vehicle Type (%1)",
			     Global::vehicleTypeToString(vehicleType, true)) );
	}
	
    } else if ( actionName == "removeTargetFromFilterList" ) {
	if ( m_settings->filterTypeTarget() == ShowMatching )
	    a->setText( m_settings->departureArrivalListType() == DepartureList
		    ? i18n("By &Target (%1)", direction)
		    : i18n("By &Origin (%1)", direction) );
	else if ( m_settings->filterTypeTarget() == ShowAll )
	    a->setText( m_settings->departureArrivalListType() == DepartureList
		    ? i18n("&Remove Target From Filter List (%1)", direction)
		    : i18n("&Remove Origin From Filter List (%1)", direction) );
    } else if ( actionName == "removeTargetFromFilterList") {
	a->setText( m_settings->departureArrivalListType() == DepartureList
		? i18n("&Remove Target From Filter List (%1)", direction)
		: i18n("&Remove Origin From Filter List (%1)", direction) );
    } else if ( actionName == "setFilterListToHideMatching") {
	a->setText( m_settings->departureArrivalListType() == DepartureList
		? i18n("By &Target (%1)", direction)
		: i18n("By &Origin (%1)", direction) );
    } else if ( actionName == "addTargetToFilterList") {
	a->setText( m_settings->departureArrivalListType() == DepartureList
		? i18n("By &Target (%1)", direction)
		: i18n("By &Origin (%1)", direction) );
    } else if ( actionName == "addTargetToFilterListAndHide") {
	a->setText( m_settings->departureArrivalListType() == DepartureList
		? i18n("By &Target (%1)", direction)
		: i18n("By &Origin (%1)", direction) );
		
    } else if ( actionName == "removeLineNumberFromFilterList" ) {
	if ( m_settings->filterTypeLineNumber() == ShowMatching )
	    a->setText( i18n("By &Line Number (%1)", line) );
	else if ( m_settings->filterTypeLineNumber() == ShowAll )
	    a->setText( i18n("&Remove Line Number From Filter List (%1)", line) );
    } else if ( actionName == "removeLineNumberFromFilterList") {
	a->setText( i18n("&Remove Line Number From Filter List (%1)", line) );
    } else if ( actionName == "setLineNumberFilterToHideMatching") {
	a->setText( i18n("By &Line Number (%1)", line) );
    } else if ( actionName == "addLineNumberToFilterList") {
	a->setText( i18n("By &Line Number (%1)", line) );
    } else if ( actionName == "addLineNumberToFilterListAndHide") {
	a->setText( i18n("By &Line Number (%1)", line) );
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
    QAction *subMenuAction = 0;
    QMenu *subMenu = 0;

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
	
	actions.append( updatedAction("searchJourneys") );
	actions.append( updatedAction("seperator") );

	if ( testState(ShowingDepartureArrivalList) ) {
	    QList< QAction* > restoreFilterList, addFilterList;
	  
	    addFilterList << updatedAction("filterOutByVehicleType");
	    if ( !m_settings->filteredOutVehicleTypes().isEmpty() )
		restoreFilterList << action("removeAllFiltersByVehicleType");

	    QString sLineNumber = m_model->item( m_clickedItemIndex.row(), 0 )->text();
	    if ( m_settings->filterLineNumberList().contains( sLineNumber ) ) {
		if ( m_settings->filterTypeLineNumber() == ShowMatching ) {
		    restoreFilterList << updatedAction("removeLineNumberFromFilterList");
		    restoreFilterList << action("setLineNumberFilterToShowAll");
		} else if ( m_settings->filterTypeLineNumber() == ShowAll ) {
		    addFilterList << updatedAction("setLineNumberFilterToHideMatching");
		    restoreFilterList << updatedAction("removeLineNumberFromFilterList");
		} // never m_filterTypeLineNumber == HideMatching => journeys with lines in filter list won't be shown
	    } else { // Line number isn't contained in the filter list
		if ( m_settings->filterTypeLineNumber() == HideMatching ) {
		    addFilterList << updatedAction("addLineNumberToFilterList");
		    restoreFilterList << action("setLineNumberFilterToShowAll");
		} else if ( m_settings->filterTypeLineNumber() == ShowAll ) {
		    addFilterList << updatedAction("addLineNumberToFilterListAndHide");
		} // never m_filterTypeLineNumber == ShowMatching => journeys with lines not in filter list won't be shown
	    }

	    QString sTarget = m_model->item( m_clickedItemIndex.row(), 1 )->text();
	    if ( m_settings->filterTargetList().contains( sTarget ) ) {
		if ( m_settings->filterTypeTarget() == ShowMatching ) {
		    restoreFilterList << updatedAction("removeTargetFromFilterList");
		    restoreFilterList << action("setFilterListToShowAll");
		} else if ( m_settings->filterTypeTarget() == ShowAll ) {
		    addFilterList << updatedAction("setFilterListToHideMatching");
		    restoreFilterList << updatedAction("removeTargetFromFilterList");
		} // never m_filterTypeTarget == HideMatching => journeys with target/origin in filter list won't be shown
	    } else { // Target isn't contained in the filter list
		if ( m_settings->filterTypeTarget() == HideMatching ) {
		    addFilterList << updatedAction("addTargetToFilterList");
		    restoreFilterList << action("setFilterListToShowAll");
		} else if ( m_settings->filterTypeTarget() == ShowAll ) {
		    addFilterList << updatedAction("addTargetToFilterListAndHide");
		} // never m_filterTypeTarget == ShowMatching => journeys with target/origin not in filter list won't be shown
	    }

	    bool addSeperator = true;
	    if ( !addFilterList.isEmpty() ) {
		actions.append( updatedAction("seperator") );
		addSeperator = false; // Only one seperator for filter sub menus
		
		if ( addFilterList.count() == 1 ) {
		    actions.append( addFilterList );
		} else {
		    subMenuAction = new QAction(
			Global::makeOverlayIcon(KIcon("view-filter"), "list-add"),
			m_settings->departureArrivalListType() == DepartureList
			? i18n("&Filter This Departure") : i18n("&Filter This Arrival"),
			this );
		    subMenu = new QMenu;
		    subMenu->addActions( addFilterList );
		    subMenuAction->setMenu( subMenu );
		    actions.append( subMenuAction );
		}
	    }
	    
	    if ( !restoreFilterList.isEmpty() ) {
		if ( addSeperator )
		    actions.append( updatedAction("seperator") );
		  
		if ( restoreFilterList.count() == 1 ) {
		  actions.append( action("showEverything") );
		} else {
		  restoreFilterList.insert( 0, action("showEverything") );
		  restoreFilterList.insert( 1, updatedAction("seperator") );

		  subMenuAction = new QAction(
		      Global::makeOverlayIcon(KIcon("view-filter"), "list-remove"),
		      i18n("&Remove filters"), this );
		  subMenu = new QMenu;
		  subMenu->addActions( restoreFilterList );
		  subMenuAction->setMenu( subMenu );
		  actions.append( subMenuAction );
		}
	    }

	    QStringList filterConfigurationList = m_settings->filterConfigurationListLocalized();
	    if ( !filterConfigurationList.isEmpty() ) {
		KSelectAction *actionSwitch = dynamic_cast< KSelectAction* >(
			action("switchFilterConfiguration") );

		actionSwitch->clear();
		foreach ( QString filterConfig, filterConfigurationList ) {
		    actionSwitch->addAction( filterConfig );
		    if ( filterConfig == m_settings->filterConfigurationLocalized() ) {
			QAction *actionFilterConfig =
				actionSwitch->selectableActionGroup()->actions().last();
			actionFilterConfig->setChecked( true );
			
			if ( m_settings->isCurrentFilterConfigChanged() )
			    actionFilterConfig->setText( actionFilterConfig->text() + "*" );
		    }
		}
		actions.append( actionSwitch );
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
    } else { // No context item
	actions.append( updatedAction("searchJourneys") );
	actions.append( updatedAction("seperator") );
	
	if ( testState(ShowingDepartureArrivalList) ) {
	    QList< QAction* > restoreFilterList;
	    if ( !m_settings->filteredOutVehicleTypes().isEmpty() )
		restoreFilterList << action("removeAllFiltersByVehicleType");
	    if ( m_settings->filterTypeTarget() != ShowAll )
		restoreFilterList << action("setFilterListToShowAll");
	    if ( m_settings->filterTypeLineNumber() != ShowAll )
		restoreFilterList << action("setLineNumberFilterToShowAll");
	    
	    if ( restoreFilterList.count() == 1 ) {
		actions.append( action("showEverything") );
	    } else {
		restoreFilterList.insert( 0, action("showEverything") );
		restoreFilterList.insert( 1, updatedAction("seperator") );
		
		subMenuAction = new QAction(
			Global::makeOverlayIcon(KIcon("view-filter"), "list-remove"),
			i18n("&Remove filters"), this );
		subMenu = new QMenu;
		subMenu->addActions( restoreFilterList );
		subMenuAction->setMenu( subMenu );
		actions.append( subMenuAction );
	    }
	    
	    QStringList filterConfigurationList = m_settings->filterConfigurationListLocalized();
	    if ( !filterConfigurationList.isEmpty() ) {
		KSelectAction *actionSwitch = dynamic_cast< KSelectAction* >(
		action("switchFilterConfiguration") );
		
		actionSwitch->clear();
		foreach ( QString filterConfig, filterConfigurationList ) {
		    actionSwitch->addAction( filterConfig );
		    if ( filterConfig == m_settings->filterConfigurationLocalized() ) {
			QAction *actionFilterConfig =
				actionSwitch->selectableActionGroup()->actions().last();
			actionFilterConfig->setChecked( true );
			
			if ( m_settings->isCurrentFilterConfigChanged() )
			    actionFilterConfig->setText( actionFilterConfig->text() + "*" );
		    }
		}
		actions.append( actionSwitch );
	    }
	}
	if ( !treeView->header()->isVisible() )
	    actions.append( action("showHeader") );
    }

    if ( actions.count() > 0 && view() ) {
	QMenu::exec( actions, QCursor::pos() );

	delete subMenuAction;
	delete subMenu;
    }
}

void PublicTransport::showEverything( bool b ) {
  removeAllFiltersByVehicleType( b );
  setLineNumberFilterToShowAll( b );
  setTargetFilterToShowAll( b );
}

void PublicTransport::filterOutByVehicleType( bool ) {
    QStandardItem *itemDeparture = m_model->item( m_clickedItemIndex.row(), 2 );
    VehicleType vehicleType = static_cast<VehicleType>(
	itemDeparture->data( VehicleTypeRole ).toInt() );
    m_settings->hideTypeOfVehicle( vehicleType );

    // TODO: To PublicTransportSettings
    KConfigGroup cg = config();
    cg.writeEntry( PublicTransportSettings::vehicleTypeToConfigName(vehicleType), false );
    //     emit settingsChanged();
    emit configNeedsSaving();
    updateModel(); // apply new filter settings
}

void PublicTransport::removeAllFiltersByVehicleType ( bool ) {
    m_settings->removeAllFiltersByVehicleType();
}

void PublicTransport::addTargetToFilterList( bool ) {
    QString target = m_model->item( m_clickedItemIndex.row(), 1 )->text();
    m_settings->setFilterTargetList( m_settings->filterTargetList() << target );
//     if ( m_isConfigDialogShown )
// 	m_uiFilter.filterTargetList->items() << target;

    KConfigGroup cg = config();
    cg.writeEntry("filterTargetList", m_settings->filterTargetList());
//     emit settingsChanged();
    emit configNeedsSaving();
    updateModel(); // apply new filter settings
}

void PublicTransport::removeTargetFromFilterList( bool ) {
    QString target = m_model->item( m_clickedItemIndex.row(), 1 )->text();
    QStringList filters = m_settings->filterTargetList();
    filters.removeOne( target );
    m_settings->setFilterTargetList( filters );
//     if ( m_isConfigDialogShown )
// 	m_uiFilter.filterTargetList->items().removeOne( target );

    // TODO: to PublicTransportSettings
    KConfigGroup cg = config();
    cg.writeEntry("filterTargetList", m_settings->filterTargetList());
    cg.writeEntry("filterTypeTarget", static_cast<int>(m_settings->filterTypeTarget()));
    //     emit settingsChanged();
    emit configNeedsSaving();
    updateModel(); // apply new filter settings
}

void PublicTransport::setTargetFilterToShowAll( bool ) {
    m_settings->setFilterTypeTarget( ShowAll );
    updateModel(); // apply new filter settings

//     if ( testState(ConfigDialogShown) )
// 	m_uiFilter.filterTypeTarget->setCurrentIndex( static_cast<int>(m_filterTypeTarget) );
}

void PublicTransport::setTargetFilterToHideMatching ( bool ) {
    m_settings->setFilterTypeTarget( HideMatching );
    updateModel(); // apply new filter settings

//     if ( testState(ConfigDialogShown) )
// 	m_uiFilter.filterTypeTarget->setCurrentIndex( static_cast<int>(m_filterTypeTarget) );
}

void PublicTransport::addTargetToFilterListAndHide( bool b ) {
    m_settings->setFilterTypeTarget( HideMatching );
    updateModel(); // apply new filter settings

//     if ( testState(ConfigDialogShown) )
// 	m_uiFilter.filterTypeTarget->setCurrentIndex( static_cast<int>(m_filterTypeTarget) );

    addTargetToFilterList( b );
}

void PublicTransport::addLineNumberToFilterList( bool ) {
    QString lineNumber = m_model->item( m_clickedItemIndex.row(), 0 )->text();
    m_settings->setFilterLineNumberList( m_settings->filterLineNumberList() << lineNumber );
//     if ( m_isConfigDialogShown )
// 	m_uiFilter.filterLineNumberList->items() << target;

    KConfigGroup cg = config();
    cg.writeEntry("filterLineNumberList", m_settings->filterLineNumberList());
//     emit settingsChanged();
    emit configNeedsSaving();
    updateModel(); // apply new filter settings
}

void PublicTransport::removeLineNumberFromFilterList( bool ) {
    QString lineNumber = m_model->item( m_clickedItemIndex.row(), 0 )->text();
    QStringList filters = m_settings->filterLineNumberList();
    filters.removeOne( lineNumber );
    m_settings->setFilterLineNumberList( filters );
//     if ( m_isConfigDialogShown )
// 	m_uiFilter.filterLineNumberList->items().removeOne( target );

    // TODO: to PublicTransportSettings
    KConfigGroup cg = config();
    cg.writeEntry("filterLineNumberList", m_settings->filterLineNumberList());
    cg.writeEntry("filterTypeLineNumber", static_cast<int>(m_settings->filterTypeLineNumber()));
    //     emit settingsChanged();
    emit configNeedsSaving();
    updateModel(); // apply new filter settings
}

void PublicTransport::setLineNumberFilterToShowAll( bool ) {
    m_settings->setFilterTypeLineNumber( ShowAll );
    updateModel(); // apply new filter settings

//     if ( testState(ConfigDialogShown) )
// 	m_uiFilter.filterTypeTarget->setCurrentIndex( static_cast<int>(m_filterTypeTarget) );
}

void PublicTransport::setLineNumberFilterToHideMatching ( bool ) {
    m_settings->setFilterTypeLineNumber( HideMatching );
    updateModel(); // apply new filter settings

//     if ( testState(ConfigDialogShown) )
// 	m_uiFilter.filterTypeTarget->setCurrentIndex( static_cast<int>(m_filterTypeTarget) );
}

void PublicTransport::addLineNumberToFilterListAndHide( bool b ) {
    m_settings->setFilterTypeLineNumber( HideMatching );
    updateModel(); // apply new filter settings

//     if ( testState(ConfigDialogShown) )
// 	m_uiFilter.filterTypeTarget->setCurrentIndex( static_cast<int>(m_filterTypeTarget) );

    addLineNumberToFilterList( b );
}

void PublicTransport::showJourneySearch( bool ) {
    addState( m_settings->serviceProviderSupportsJourneySearch()
	      ? ShowingJourneySearch : ShowingJourneysNotSupported );
}

void PublicTransport::goBackToDepartures() {
    addState( ShowingDepartureArrivalList );
}

void PublicTransport::markAlarmRow ( const QPersistentModelIndex& modelIndex, AlarmState alarmState ) {
    if( !modelIndex.isValid() ) {
	kDebug() << "!index.isValid(), row =" << modelIndex.row();
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
	QPixmap pixmap = iconEffect.apply( KIcon("kalarm").pixmap(16 * m_settings->sizeFactor()),
					   KIconLoader::Small, KIconLoader::DisabledState );
	KIcon disabledAlarmIcon;
	disabledAlarmIcon.addPixmap( pixmap, QIcon::Normal );
	itemDeparture->setIcon( disabledAlarmIcon );
	itemDeparture->setData( static_cast<int>(HtmlDelegate::Right),
				HtmlDelegate::DecorationPositionRole );
	m_model->item( modelIndex.row(), 0 )->setBackground( brush );
	m_model->item( modelIndex.row(), 1 )->setBackground( brush );
    }
}

void PublicTransport::removeAlarmForDeparture( int row ) {
    QStandardItem *itemDeparture = m_model->item( row, 2 );
    AlarmTimer *alarmTimer = (AlarmTimer*)itemDeparture->data( AlarmTimerRole ).value<void*>();
    if ( alarmTimer ) {
	itemDeparture->setData( 0, AlarmTimerRole );
	alarmTimer->timer()->stop();
	delete alarmTimer;
	markAlarmRow( m_clickedItemIndex, NoAlarm );
    }
    
    createPopupIcon();
}

void PublicTransport::removeAlarmForDeparture( bool ) {
    removeAlarmForDeparture( m_clickedItemIndex.row() );
}

void PublicTransport::setAlarmForDeparture( const QPersistentModelIndex &modelIndex,
					    AlarmTimer *alarmTimer ) {
    if( !modelIndex.isValid() ) {
	kDebug() << "!modelIndex.isValid(), row =" << modelIndex.row();
	return;
    }

    QStandardItem *itemDeparture = m_model->item( modelIndex.row(), 2 );
    markAlarmRow( modelIndex, AlarmPending );

    if ( alarmTimer == NULL ) {
	QDateTime predictedDeparture = itemDeparture->data( SortRole ).toDateTime();
	int secsTo = QDateTime::currentDateTime().secsTo(
		predictedDeparture.addSecs(-m_settings->alarmTime() * 60) );
	if ( secsTo < 0 )
	    secsTo = 0;
	alarmTimer = new AlarmTimer( secsTo * 1000, modelIndex );
    }
    itemDeparture->setData( qVariantFromValue((void*)alarmTimer), AlarmTimerRole );
    connect( alarmTimer, SIGNAL(timeout(const QPersistentModelIndex &)),
	     this, SLOT(showAlarmMessage(const QPersistentModelIndex &)) );

    createPopupIcon();
}

void PublicTransport::setAlarmForDeparture ( bool ) {
//     QString sLine = m_model->item( m_clickedItemIndex.row(), 0 )->text();
//     QString sTarget = m_model->item( m_clickedItemIndex.row(), 1 )->text();

//     if ( m_model->item( m_clickedItemIndex.row(), 2 ) != NULL &&
// 	 !m_model->item( m_clickedItemIndex.row(), 2 )->icon().isNull() ) {
// 	kDebug() << "icon = null";
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
// 	kDebug() << "Couldn't get dbus interface for kalarm";
// 	return;
//     }

//     QString message = i18n("PublicTransport: Line %1 to %2 departs", sLine, sTarget);
//     dbus signature:
// 		scheduleMessage(QString message, QString startDateTime, int lateCancel, uint flags, QString bgColor, QString fgColor, QString font, QString audioFile, int reminderMins, QString recurrence, int subRepeatInterval, int subRepeatCount)
//     QDBusReply<bool> reply = remoteApp.callWithArgumentList( QDBus::Block, "scheduleMessage", QList<QVariant>() << message << predictedDeparture.toString(Qt::ISODate) << 0 << (uint)0 << "null" << "null" << "null" << "null" /*audiofile*/ << 5 /*reminderMins*/ << "" << 0 << 0 );

//     kDebug() << "qdbus org.kde.kalarm /kalarm org.kde.kalarm.kalarm.scheduleMessage" << message << predictedDeparture.toString(Qt::ISODate) << 0 << (uint)0 << "\"null\"" << "\"null\"" << "\"null\"" << "\"null\"" /*audiofile*/ << 5 /*reminderMins*/ << "\"\"" << 0 << 0 ;
//     kDebug() << "Set alarm for" << sLine << sTarget << "reply =" << reply.value() << "time arg=" << predictedDeparture.toString("hh:mm") << "reply errormsg" << reply.error().message() << "name" << reply.error().name();
}

void PublicTransport::showAlarmMessage( const QPersistentModelIndex &modelIndex ) {
    if ( !modelIndex.isValid() ) {
	kDebug() << "!modelIndex.isValid(), row =" << modelIndex.row();
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
    int minsToDeparture = qCeil( (float)QDateTime::currentDateTime().secsTo(predictedDeparture) / 60.0f );
    VehicleType vehicleType = static_cast<VehicleType>(
	    m_model->item(row, 2)->data(VehicleTypeRole).toInt() );
//     kDebug() << vehicleType << m_model->item( row, 2 )->data( VehicleTypeRole ).toInt();
    QString message;
    if ( minsToDeparture > 0 ) {
	if ( vehicleType == Unknown )
	    message = i18np("Line %2 to '%3' departs in %1 minute at %4",
			    "Line %2 to '%3' departs in %1 minutes at %4",
			    minsToDeparture, sLine, sTarget, predictedDeparture.toString("hh:mm") );
	else
	    message = i18ncp("%2: Line string (e.g. 'U3'), %4: Vehicle type name (e.g. tram, subway)",
			     "The %4 %2 to '%3' departs in %1 minute at %5",
			     "The %4 %2 to '%3' departs in %1 minutes at %5",
			     minsToDeparture, sLine, sTarget,
			     Global::vehicleTypeToString(vehicleType),
			     predictedDeparture.toString("hh:mm"));
    }
    else if ( minsToDeparture < 0 ) {
	if ( vehicleType == Unknown )
	    message = i18np("Line %2 to '%3' has departed %1 minute ago at %4",
			    "Line %2 to '%3' has departed %1 minutes ago at %4",
			    -minsToDeparture, sLine, sTarget,
			    predictedDeparture.toString("hh:mm"));
	else
	    message = i18ncp("%2: Line string (e.g. 'U3'), %4: Vehicle type name (e.g. tram, subway)",
			     "The %4 %2 to '%3' has departed %1 minute ago at %5",
			     "The %4 %2 to %3 has departed %1 minutes ago at %5",
			     -minsToDeparture, sLine, sTarget,
			     Global::vehicleTypeToString(vehicleType),
			     predictedDeparture.toString("hh:mm"));
    }
    else {
	if ( vehicleType == Unknown )
	    message = i18n("Line %1 to '%2' departs now at %3", sLine, sTarget,
			   predictedDeparture.toString("hh:mm"));
	else
	    message = i18nc("%1: Line string (e.g. 'U3'), %3: Vehicle type name (e.g. tram, subway)",
			    "The %3 %1 to '%2' departs now at %4", sLine, sTarget,
			    Global::vehicleTypeToString(vehicleType),
			    predictedDeparture.toString("hh:mm"));
    }

    KNotification::event( KNotification::Warning, message,
			  KIcon("public-transport-stop").pixmap(16), 0L,
			  KNotification::Persistent );

//     QMessageBox box( QMessageBox::Information, "Public transport: Alarm", message, QMessageBox::Ok );
//     box.setWindowIcon( KIcon("public-transport-stop") );
//     box.exec();
}

bool PublicTransport::filterOut( const DepartureInfo &departureInfo ) const {
    return
	// Filter vehicle types
	!m_settings->isTypeOfVehicleShown( departureInfo.vehicleType() ) ||
    
	// Filter night lines
	(departureInfo.isNightLine() && !m_settings->showNightlines()) ||

	// Filter min/max line numbers
	(departureInfo.isLineNumberValid() && !departureInfo.isLineNumberInRange(
	    m_settings->filterMinLine(), m_settings->filterMaxLine() )) ||

	// Filter target (direction)
	(m_settings->filterTypeTarget() == ShowMatching
	    && !m_settings->filterTargetList().contains(departureInfo.target())) ||
	(m_settings->filterTypeTarget() == HideMatching
	    && m_settings->filterTargetList().contains(departureInfo.target())) ||

	// Filter line numbers
	(m_settings->filterTypeLineNumber() == ShowMatching
	    && !m_settings->filterLineNumberList().contains(departureInfo.lineString())) ||
	(m_settings->filterTypeLineNumber() == HideMatching
	    && m_settings->filterLineNumberList().contains(departureInfo.lineString())) ||

	// Filter past departures
	!isTimeShown( departureInfo.predictedDeparture() );
// 	QDateTime::currentDateTime().secsTo( departureInfo.predictedDeparture() ) < -60;
}

QHash<QString, QVariant> PublicTransport::serviceProviderData() const {
    QString sServiceProvider;
    Plasma::DataEngine::Data data = dataEngine("publictransport")->query("ServiceProviders");
    foreach ( QString serviceProviderName, data.keys() )
    {
	QHash< QString, QVariant > serviceProviderData = data.value(serviceProviderName).toHash();
	if ( serviceProviderData["id"].toString() == m_settings->serviceProvider() )
	    return serviceProviderData;
    }

    kDebug() << "Name not found for" << m_settings->serviceProvider();
    return QHash<QString,QVariant>();
}

QString PublicTransport::titleText() const {
    QString sServiceProvider = serviceProviderData()["shortUrl"].toString();
    QString sStops = m_settings->currentStopIndex() == -1
	    ? m_settings->stops().join(", ") : m_settings->stops()[m_settings->currentStopIndex()];
    if ( m_settings->useSeperateCityValue() )
	return QString("%1, %2").arg( sStops )
				       .arg( m_settings->city() );
    else
	return QString("%1").arg( sStops );
}

QString PublicTransport::infoText() const {
    QHash< QString, QVariant > data = serviceProviderData();
    QString shortUrl = data[ "shortUrl" ].toString();
    QString url = data[ "url" ].toString();
    QString sLastUpdate = m_lastSourceUpdate.toString( "hh:mm" );
    if ( sLastUpdate.isEmpty() )
	sLastUpdate = i18nc("This is used as 'last data update' text when there "
			    "hasn't been any updates yet.", "none");
    return QString("<nobr>%4: %1, %5: <a href='%2'>%3</a></nobr>")
	.arg( sLastUpdate, url, shortUrl, i18n("last update"), i18n("data by") );
}

QString PublicTransport::courtesyToolTip() const {
    QHash< QString, QVariant > data = serviceProviderData();
    QString credit = data["credit"].toString();
    if ( credit.isEmpty() )
	return QString();
    else
	return i18n( "By courtesy of %1 (%2)", credit, data["url"].toString() );
}

QString PublicTransport::formatDateFancyFuture( const QDate& date ) const {
    int dayDiff = QDate::currentDate().daysTo( date );
    if ( dayDiff == 1 )
	return i18n( "tomorrow" );
    else if ( dayDiff <= 6 )
	return date.toString( "ddd" );
    else
	return KGlobal::locale()->formatDate( date, KLocale::ShortDate );
}

QString PublicTransport::departureText( const JourneyInfo& journeyInfo ) const {
    QString sTime, sDeparture = journeyInfo.departure().toString("hh:mm");
    if ( m_settings->displayTimeBold() )
	sDeparture = sDeparture.prepend("<span style='font-weight:bold;'>").append("</span>");
    
    if ( journeyInfo.departure().date() != QDate::currentDate() )
	sDeparture += ", " + formatDateFancyFuture( journeyInfo.departure().date() );
    
    if ( m_settings->isDepartureTimeShown() && m_settings->isRemainingMinutesShown() ) {
	QString sText = journeyInfo.durationToDepartureString();
	sText = sText.replace(QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");

	if ( m_settings->linesPerRow() > 1 )
	    sTime = QString("%1<br>(%2)").arg( sDeparture ).arg( sText );
	else
	    sTime = QString("%1 (%2)").arg( sDeparture ).arg( sText );
    } else if (m_settings->isDepartureTimeShown()) {
	sTime = sDeparture;
    } else if (m_settings->isRemainingMinutesShown()) {
	sTime = journeyInfo.durationToDepartureString();
	sTime = sTime.replace(QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");
    } else
	sTime = "";

    return sTime;
}

QString PublicTransport::arrivalText( const JourneyInfo& journeyInfo ) const {
    QString sTime, sArrival = journeyInfo.arrival().toString("hh:mm");
    if ( m_settings->displayTimeBold() )
	sArrival = sArrival.prepend("<span style='font-weight:bold;'>").append("</span>");
    
    if ( journeyInfo.arrival().date() != QDate::currentDate() )
	sArrival += ", " + formatDateFancyFuture( journeyInfo.arrival().date() );
    
    if ( m_settings->isDepartureTimeShown() && m_settings->isRemainingMinutesShown() ) {
	QString sText = journeyInfo.durationToDepartureString(true);
	sText = sText.replace(QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");

	if ( m_settings->linesPerRow() > 1 )
	    sTime = QString("%1<br>(%2)").arg( sArrival ).arg( sText );
	else
	    sTime = QString("%1 (%2)").arg( sArrival ).arg( sText );
    } else if (m_settings->isDepartureTimeShown()) {
	sTime = sArrival;
    } else if (m_settings->isRemainingMinutesShown()) {
	sTime = journeyInfo.durationToDepartureString(true);
	sTime = sTime.replace(QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");
    } else
	sTime = "";

    return sTime;
}

QString PublicTransport::departureText( const DepartureInfo &departureInfo ) const {
    QDateTime predictedDeparture = departureInfo.predictedDeparture();
    QString sTime, sDeparture = predictedDeparture.toString("hh:mm");
    QString sColor;
    if ( departureInfo.delayType() == OnSchedule )
	sColor = "color:darkgreen;"; // TODO: works good with Air-Theme, but is too dark for dark themes
    else if ( departureInfo.delayType() == Delayed )
	sColor = "color:darkred;"; // TODO: works good with Air-Theme, but is too dark for dark themes

    if ( m_settings->displayTimeBold() )
	sDeparture = sDeparture.prepend(QString("<span style='font-weight:bold;%1'>").arg(sColor)).append("</span>");
    if ( predictedDeparture.date() != QDate::currentDate() )
	sDeparture += ", " + formatDateFancyFuture( predictedDeparture.date() );

    if (m_settings->isDepartureTimeShown() && m_settings->isRemainingMinutesShown()) {
	QString sText = departureInfo.durationString();
	sText = sText.replace( QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"),
			       "<span style='color:red;'>+&nbsp;\\1</span>" );

	if ( m_settings->linesPerRow() > 1 )
	    sTime = QString("%1<br>(%2)").arg( sDeparture ).arg( sText );
	else
	    sTime = QString("%1 (%2)").arg( sDeparture ).arg( sText );
    } else if (m_settings->isDepartureTimeShown()) {
	sTime = sDeparture;
	if ( departureInfo.delayType() == Delayed ) {
	    QString sText = i18np("+ %1 minute", "+ %1 minutes", departureInfo.delay() );
	    sText = sText.prepend(" (").append(")");
	    sText = sText.replace(QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");
	    sTime += sText;
	}
    } else if (m_settings->isRemainingMinutesShown()) {
	sTime = departureInfo.durationString();
	sTime = sTime.replace(QRegExp("\\+(?:\\s*|&nbsp;)(\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");
    } else
	sTime = "";

    return sTime;
}

void PublicTransport::setTextColorOfHtmlItem ( QStandardItem *item, const QColor &textColor ) {
    item->setText( item->text().prepend("<span style='color:rgba(%1,%2,%3,%4);'>")
	    .arg( textColor.red() ).arg( textColor.green() ).arg( textColor.blue() )
	    .arg( textColor.alpha() ).append("</span>") );
}

int PublicTransport::findDeparture( const DepartureInfo& departureInfo ) const {
    // TODO: Could start at an index based on the departure time.
    // Compute some index that is somewhere at the position of that time.
    QModelIndexList indices = m_model->match( m_model->index(0, 0),
					      TimetableItemHashRole,
					      departureInfo.hash(), 1,
					      Qt::MatchFixedString );
					      
    return indices.isEmpty() ? -1 : indices.first().row();
}

int PublicTransport::findJourney( const JourneyInfo& journeyInfo ) const {
    //     kDebug() << m_model->rowCount() << "rows" << departureInfo.lineString << departureInfo.target << departureInfo.departure.toString("hh:mm");
    for ( int row = 0; row < m_modelJourneys->rowCount(); ++row )
    {
	// TODO, item( row, JOURNEYS_COLUMN_DEPARTURE )
	QDateTime departure = m_modelJourneys->item( row, 1 )->data( SortRole ).toDateTime();
	if ( departure != journeyInfo.departure() ) {
// 	    kDebug() << "Departure Test failed" << departure << journeyInfo.departure;
	    continue;
	}

	// TODO, item( row, JOURNEYS_COLUMN_ARRIVAL )
	QDateTime arrival = m_modelJourneys->item( row, 2 )->data( SortRole ).toDateTime();
	if ( arrival != journeyInfo.arrival() ) {
	    continue;
	}

	// TODO, item( row, JOURNEYS_COLUMN_CHANGES )
	int changes = m_modelJourneys->item( row, 3 )->data( SortRole ).toInt();
	if ( changes != journeyInfo.changes() ) {
	    continue;
	}

	QString operatorName = m_modelJourneys->item( row, 0 )->data( OperatorRole ).toString();
	if ( operatorName != journeyInfo.operatorName() ) {
	    continue;
	}

	// 	kDebug() << "Found" << row << departureInfo.lineString << departureInfo.target << departureInfo.departure.toString("hh:mm");;
	return row;
    }

    //     if ( m_model->rowCount() > 0 )
    // 	kDebug() << "Not found" << departureInfo.lineString << departureInfo.target << departureInfo.departure.toString("hh:mm");;
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
	    sText = i18np("+%1 minute", "+%1 minutes", departureInfo.delay());
	    sText = sText.replace(QRegExp("(+?\\s*\\d+)"), "<span style='color:red;'>+&nbsp;\\1</span>");

	    if ( !departureInfo.delayReason().isEmpty() ) {
		sText += ", " + departureInfo.delayReason();
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
void PublicTransport::setValuesOfJourneyItem( QStandardItem* journeyItem,
					      const JourneyInfo &journeyInfo,
					      ItemInformation journeyInformation,
					      bool update ) {
    QStringList sList;
    QString s, s2, s3;
    QStandardItem *item;
    int row;
    switch ( journeyInformation ) {
	case VehicleTypeListItem:
// 	    foreach( VehicleType vehicleType, journeyInfo.vehicleTypes )
// 		slist << Global::vehicleTypeToString(vehicleType);
// 	    journeyItem->setText( slist.join(", ") );
	    journeyItem->setIcon( Global::iconFromVehicleTypeList(
		    journeyInfo.vehicleTypes(), 32 * m_settings->sizeFactor()) );
	    journeyItem->setData( journeyItem->text(), SortRole );
	    journeyItem->setData( journeyInfo.hash(), TimetableItemHashRole );
	    journeyItem->setData( journeyInfo.operatorName(), OperatorRole );
	    journeyItem->setData( m_settings->linesPerRow(), HtmlDelegate::LinesPerRowRole );
	    if ( !update )
		journeyItem->setData( QStringList() << "raised"
			<< "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    break;

	case JourneyInfoItem:
	    s = QString( i18n("<b>From:</b> %1<br><b>To:</b> %2") )
		    .arg( journeyInfo.startStopName() )
		    .arg( journeyInfo.targetStopName() );
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    journeyItem->setData( journeyItem->text(), SortRole );
	    journeyItem->setData( m_settings->linesPerRow(), HtmlDelegate::LinesPerRowRole );
	    
	    if ( !journeyInfo.journeyNews().isEmpty() ) {
		journeyItem->setIcon( Global::makeOverlayIcon(
			KIcon("view-pim-news"), "arrow-down", QSize(12,12)) );
		journeyItem->setData( static_cast<int>(HtmlDelegate::Right),
				      HtmlDelegate::DecorationPositionRole );
	    }
	    if ( !update )
		journeyItem->setData( QStringList() << "raised"
			<< "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    break;
	    
	case JourneyNewsItem:
	    s2 = journeyInfo.journeyNews();
	    if ( s2.startsWith("http://") ) // TODO: Make the link clickable...
		s2 = QString("<a href='%1'>%2</a>").arg(s2).arg(i18n("Link to journey news"));
	    s3 = s = QString("<b>%1</b> %2").arg( i18nc("News for a journey with public "
			"transport, like 'platform changed'", "News:") ).arg( s2 );
	    s3.replace(QRegExp("<[^>]*>"), "");
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s3 );
	    if ( !update ) {
		journeyItem->setData( 4, Qt::UserRole );
		journeyItem->setData( qMin(3, s3.length() / 20), HtmlDelegate::LinesPerRowRole );
		setTextColorOfHtmlItem( journeyItem, m_colorSubItemLabels );
	    }
	    break;
	    
	case DepartureItem:
	    journeyItem->setData( s = departureText(journeyInfo),
				  HtmlDelegate::FormattedTextRole );
	    if ( m_settings->linesPerRow() > 1 ) {
		// Get longest line for auto column sizing
		sList = s.split("<br>", QString::SkipEmptyParts, Qt::CaseInsensitive);
		s = "";
		foreach( QString sCurrent, sList ) {
		    sCurrent.replace(QRegExp("<[^>]*>"), "");
		    if ( sCurrent.replace(QRegExp("(&\\w{2,5};|&#\\d{3,4};)"), " ").length()
				> s.length() )
			s = sCurrent;
		}
		journeyItem->setText( s ); // This is just used for auto column sizing
	    } else
		journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") ); // This is just used for auto column sizing
	    journeyItem->setData( journeyInfo.departure(), SortRole );
	    journeyItem->setData( m_settings->linesPerRow(), HtmlDelegate::LinesPerRowRole );
	    journeyItem->setData( qCeil((float)QDateTime::currentDateTime().secsTo(
		    journeyInfo.departure() ) / 60.0f), RemainingMinutesRole );
	    journeyItem->setData( journeyInfo.vehicleTypesVariant(), VehicleTypeListRole );
	    if ( !update ) {
		journeyItem->setData( QStringList() << "raised"
			<< "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case ArrivalItem:
	    journeyItem->setData( s = arrivalText(journeyInfo),
				  HtmlDelegate::FormattedTextRole );
	    if ( m_settings->linesPerRow() > 1 ) {
		// Get longest line for auto column sizing
		sList = s.split("<br>", QString::SkipEmptyParts, Qt::CaseInsensitive);
		s = "";
		foreach( QString sCurrent, sList ) {
		    sCurrent.replace(QRegExp("<[^>]*>"), "");
		    if ( sCurrent.replace(QRegExp("(&\\w{2,5};|&#\\d{3,4};)"), " ").length()
				> s.length() )
			s = sCurrent;
		}
		journeyItem->setText( s ); // This is just used for auto column sizing
	    } else
		journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") ); // This is just used for auto column sizing
	    journeyItem->setData( journeyInfo.arrival(), SortRole );
	    journeyItem->setData( m_settings->linesPerRow(), HtmlDelegate::LinesPerRowRole );
	    journeyItem->setData( qCeil((float)QDateTime::currentDateTime().secsTo(
		    journeyInfo.arrival() ) / 60.0f), RemainingMinutesRole );
	    journeyItem->setData( journeyInfo.vehicleTypesVariant(), VehicleTypeListRole );
	    if ( !update ) {
		journeyItem->setData( QStringList() << "raised"
			<< "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case StartStopNameItem:
	    journeyItem->setText( journeyInfo.startStopName() );
	    journeyItem->setData( journeyInfo.startStopName(), SortRole );
// 	    journeyItem->setData( m_settings->linesPerRow(), HtmlDelegate::LinesPerRowRole );
// 	    if ( !update ) {
// 		journeyItem->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
// 	    }
	    break;

	case TargetStopNameItem:
	    journeyItem->setText( journeyInfo.targetStopName() );
	    journeyItem->setData( journeyInfo.targetStopName(), SortRole );
// 	    journeyItem->setData( m_settings->linesPerRow(), HtmlDelegate::LinesPerRowRole );
	    break;

	case DurationItem:
	    if ( journeyInfo.duration() <= 0 ) {
		s = QString( "<b>%1</b> %2" )
			.arg( i18nc("The duration of a journey", "Duration:") )
			.arg( 0 );
	    } else {
		s = QString( "<b>%1</b> %2" )
			.arg( i18nc("The duration of a journey", "Duration:") )
			.arg( Global::durationString(journeyInfo.duration() * 60) );
	    }
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    journeyItem->setData( journeyInfo.duration(), SortRole );
// 	    if ( !update ) {
// 		journeyItem->setData( QStringList() << "raised" << "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
// 	    }
	    break;

	case ChangesItem:
	    s = QString( "<b>%1</b> %2" )
		    .arg( i18nc("The changes of a journey", "Changes:") )
		    .arg( journeyInfo.changes() );
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    journeyItem->setData( journeyInfo.changes(), SortRole );
	    break;

	case PricingItem:
	    s = QString( "<b>%1</b> %2" )
		    .arg( i18nc("The pricing of a journey", "Pricing:") )
		    .arg( journeyInfo.pricing() );
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s );
	    journeyItem->setData( journeyInfo.pricing(), SortRole );
	    break;

	case OperatorItem:
	    s = QString( "<b>%1</b> %2" )
		    .arg( i18nc("The company that is responsible for this "
				"departure/arrival/journey", "Operator:") )
		    .arg( journeyInfo.operatorName() );
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    if ( !update ) {
		journeyItem->setData( 5, Qt::UserRole );
		setTextColorOfHtmlItem( journeyItem, m_colorSubItemLabels );
	    }
	    break;
	    
	case RouteItem:
	    if ( journeyInfo.routeExactStops() > 0
		    && journeyInfo.routeExactStops() < journeyInfo.routeStops().count() ) {
		s = QString("<b>%1</b> %2")
		    .arg( i18nc("The route of this departure/arrival/journey", "Route:") )
		    .arg( i18n("> %1 stops", journeyInfo.routeStops().count()) );
	    } else {
		s = QString("<b>%1</b> %2")
		    .arg( i18nc("The route of this departure/arrival/journey", "Route:") )
		    .arg( i18n("%1 stops", journeyInfo.routeStops().count()) );
	    }
	    journeyItem->setData( s, HtmlDelegate::FormattedTextRole );
	    journeyItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    
	    // Remove all child rows
	    journeyItem->removeRows( 0, journeyItem->rowCount() );
	    
	    // Add route stops as child rows
	    for ( row = 0; row < journeyInfo.routeStops().count() - 1; ++row ) {
		// Add a seperator item, when the exact route ends
		if ( row == journeyInfo.routeExactStops() && row > 0 ) {
		    QStandardItem *itemSeparator = new QStandardItem(
			    i18n("  - End of exact route -  ") );
		    setTextColorOfHtmlItem( itemSeparator, m_colorSubItemLabels );
		    journeyItem->appendRow( itemSeparator );
		}

		KIcon icon;
		QString sTransportLine = "";
		if ( row < journeyInfo.routeVehicleTypes().count()
			    && journeyInfo.routeVehicleTypes()[row] != Unknown)
		    icon = Global::iconFromVehicleType( journeyInfo.routeVehicleTypes()[row] );
		    if ( journeyInfo.routeVehicleTypes()[row] == Feet )
			sTransportLine = i18n("Footway");
		    else if ( journeyInfo.routeTransportLines().count() > row )
			sTransportLine = journeyInfo.routeTransportLines()[row];
		else {
		    icon = KIcon("public-transport-stop");
		    if ( journeyInfo.routeTransportLines().count() > row )
			sTransportLine = journeyInfo.routeTransportLines()[row];
		}

		QString stopDep = journeyInfo.routeStops()[row];
		QString stopArr = journeyInfo.routeStops()[row + 1];
		if ( journeyInfo.routePlatformsDeparture().count() > row
			&& !journeyInfo.routePlatformsDeparture()[row].isEmpty() ) {
		    stopDep = i18n("Platform %1", journeyInfo.routePlatformsDeparture()[row])
			    + " - " + stopDep;
		}
		if ( journeyInfo.routePlatformsArrival().count() > row 
			&& !journeyInfo.routePlatformsArrival()[row].isEmpty() ) {
		    stopArr = i18n("Platform %1", journeyInfo.routePlatformsArrival()[row])
			    + " - " + stopArr;
		}
		
		QString sTimeDep = journeyInfo.routeTimesDeparture()[row].toString("hh:mm");
		if ( journeyInfo.routeTimesDepartureDelay().count() > row ) {
		    int delay = journeyInfo.routeTimesDepartureDelay()[ row ];
		    if ( delay > 0 ) {
			sTimeDep += QString( " <span style='color:red;'>+%1</span>" )
				.arg( delay );
		    } else if ( delay == 0 ) {
			sTimeDep = sTimeDep.prepend( "<span style='color:green;'>" )
					   .append( "</span>" );
		    }
		}
		
		QString sTimeArr = journeyInfo.routeTimesArrival()[row].toString("hh:mm");
		if ( journeyInfo.routeTimesArrivalDelay().count() > row ) {
		    int delay = journeyInfo.routeTimesArrivalDelay()[ row ];
		    if ( delay > 0 ) {
			sTimeArr += QString( " <span style='color:red;'>+%1</span>" )
				.arg( delay );
		    } else if ( delay == 0 ) {
			sTimeArr = sTimeArr.prepend( "<span style='color:green;'>" )
					   .append( "</span>" );
		    }
		    
		}
			    
		if ( sTransportLine.isEmpty() ) {
		    item = new QStandardItem( icon,
			    i18nc("%1 is the departure time, %2 the origin stop name, "
				  "%3 the arrival time, %4 the target stop name.",
				  "dep: %1 - %2<br>arr: %3 - %4",
				  sTimeDep, stopDep, sTimeArr, stopArr) );
		    item->setData( 2, HtmlDelegate::LinesPerRowRole );
		} else {
		    item = new QStandardItem( icon,
			    i18nc("%1 is the departure time, %2 the origin stop name, "
				  "%3 the arrival time, %4 the target stop name, "
				  "%5 the transport line.",
				  "<b>%5</b><br>dep: %1 - %2<br>arr: %3 - %4",
				  sTimeDep, stopDep, sTimeArr, stopArr,
				  sTransportLine) );
		    item->setData( 3, HtmlDelegate::LinesPerRowRole );
		}

		item->setData( true, HtmlDelegate::DrawBabkgroundGradientRole );
		item->setData( row, SortRole );
		int iconExtend = 16 * m_settings->sizeFactor();
		item->setData( QSize(iconExtend, iconExtend),
			       HtmlDelegate::IconSizeRole );
		setTextColorOfHtmlItem( item, m_colorSubItemLabels );

		journeyItem->appendRow( item );
	    }
	    
	    if ( !update ) {
		journeyItem->setData( 6, Qt::UserRole );
		setTextColorOfHtmlItem( journeyItem, m_colorSubItemLabels );
	    }
	    break;

	default:
	    break;
    }
}

void PublicTransport::setValuesOfDepartureItem( QStandardItem* departureItem,
						const DepartureInfo &departureInfo,
						ItemInformation departureInformation,
						bool update ) {
    QString s, s2, s3;
    QStringList sList;
    QStandardItem *item;
    int row;
    switch ( departureInformation ) {
	case LineNameItem:
	    departureItem->setText( departureInfo.lineString() );
	    departureItem->setData( QString("<span style='font-weight:bold;'>%1</span>")
		    .arg(departureInfo.lineString()), HtmlDelegate::FormattedTextRole );
	    departureItem->setData( departureInfo.lineString(), SortRole );
	    departureItem->setData( departureInfo.hash(), TimetableItemHashRole );
	    departureItem->setData( departureInfo.operatorName(), OperatorRole );
	    departureItem->setData( m_settings->linesPerRow(), HtmlDelegate::LinesPerRowRole );
// 	    departureItem->setData( QVariant::fromValue<DepartureInfo>(departureInfo), DepartureInfoRole );
	    if ( departureInfo.vehicleType() != Unknown )
		departureItem->setIcon( Global::iconFromVehicleType(departureInfo.vehicleType()) );
	    if ( !update ) {
		departureItem->setTextAlignment( Qt::AlignRight );
		departureItem->setData( QStringList() << "raised"
			<< "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case TargetItem:
	    departureItem->setText( departureInfo.target() );
	    departureItem->setData( departureInfo.target(), SortRole );
	    departureItem->setData( m_settings->linesPerRow(), HtmlDelegate::LinesPerRowRole );
	    if ( !departureInfo.journeyNews().isEmpty() ) {
		departureItem->setIcon( Global::makeOverlayIcon(KIcon("view-pim-news"),
								"arrow-down", QSize(12,12)) );
		departureItem->setData( static_cast<int>(HtmlDelegate::Right),
					HtmlDelegate::DecorationPositionRole );
	    }
	    if ( !update ) {
		departureItem->setData( QStringList() << "raised"
			<< "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case DepartureItem:
	    departureItem->setData( s = departureText(departureInfo),
				    HtmlDelegate::FormattedTextRole );
	    if ( m_settings->linesPerRow() > 1 ) {
		// Get longest line for auto column sizing
		sList = s.split("<br>", QString::SkipEmptyParts, Qt::CaseInsensitive);
		s = "";
		foreach( QString sCurrent, sList ) {
		    sCurrent.replace(QRegExp("<[^>]*>"), "");
		    if ( sCurrent.replace(QRegExp("(&\\w{2,5};|&#\\d{3,4};)"), " ").length()
				> s.length() )
			s = sCurrent;
		}
		departureItem->setText( s ); // This is just used for auto column sizing
	    } else
		departureItem->setText( s.replace(QRegExp("<[^>]*>"), "") ); // This is just used for auto column sizing
	    departureItem->setData( departureInfo.predictedDeparture(), SortRole ); // TODO: Could make findDeparture not working, when the delay has changed. Maybe change to departure with seperate TimeRole..
	    departureItem->setData( m_settings->linesPerRow(), HtmlDelegate::LinesPerRowRole );
	    departureItem->setData( qCeil((float)QDateTime::currentDateTime().secsTo(
		    departureInfo.predictedDeparture() ) / 60.0f), RemainingMinutesRole );
	    departureItem->setData( static_cast<int>(departureInfo.vehicleType()), VehicleTypeRole );
	    if ( !update ) {
		departureItem->setData( QStringList() << "raised"
			<< "drawFrameForWholeRow", HtmlDelegate::TextBackgroundRole );
	    }
	    break;

	case PlatformItem:
	    s = QString("<b>%1</b> %2")
		    .arg( i18nc("The platform from which a tram/bus/train departs",
				"Platform:") )
		    .arg( departureInfo.platform() );
	    departureItem->setData( s, HtmlDelegate::FormattedTextRole );
	    departureItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    if ( !update ) {
		departureItem->setData( 1, Qt::UserRole ); // TODO 1 => enum
		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;

	case OperatorItem:
	    s = QString("<b>%1</b> %2").arg( i18nc("The company that is responsible for this departure/arrival/journey", "Operator:") ).arg( departureInfo.operatorName() );
	    departureItem->setData( s, HtmlDelegate::FormattedTextRole );
	    departureItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    if ( !update ) {
		departureItem->setData( 4, Qt::UserRole );
		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;
	    
	case RouteItem:
	    if ( departureInfo.routeExactStops() < departureInfo.routeStops().count() ) {
		s = QString("<b>%1</b> %2")
		    .arg( i18nc("The route of this departure/arrival/journey", "Route:") )
		    .arg( i18n("> %1 stops", departureInfo.routeStops().count()) );
	    } else {
		s = QString("<b>%1</b> %2")
		    .arg( i18nc("The route of this departure/arrival/journey", "Route:") )
		    .arg( i18n("%1 stops", departureInfo.routeStops().count()) );
	    }
	    departureItem->setData( s, HtmlDelegate::FormattedTextRole );
	    departureItem->setText( s.replace(QRegExp("<[^>]*>"), "") );

	    // Remove all child rows
	    departureItem->removeRows( 0, departureItem->rowCount() );
	    
	    // Add route stops as child rows
	    for ( row = 0; row < departureInfo.routeStops().count(); ++row ) {
		// Add a seperator item, when the exact route ends
		if ( row == departureInfo.routeExactStops() && row > 0 ) {
		    QStandardItem *itemSeparator = new QStandardItem(
			    i18n("  - End of exact route -  "));
		    setTextColorOfHtmlItem( itemSeparator, m_colorSubItemLabels );
		    departureItem->appendRow( itemSeparator );
		}
		
		item = new QStandardItem( KIcon("public-transport-stop"),
			QString("%1 - %2")
			.arg(departureInfo.routeTimes()[row].toString("hh:mm"))
			.arg(departureInfo.routeStops()[row]) );
		item->setData( row, SortRole );
		setTextColorOfHtmlItem( item, m_colorSubItemLabels );
		
		departureItem->appendRow( item );
	    }
	    
	    if ( !update ) {
		departureItem->setData( 5, Qt::UserRole );
		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;

	case JourneyNewsItem:
	    s2 = departureInfo.journeyNews();
	    if ( s2.startsWith("http://") ) // TODO: Make the link clickable...
		s2 = QString("<a href='%1'>%2</a>").arg(s2).arg(i18n("Link to journey news"));
	    s3 = s = QString("<b>%1</b> %2").arg( i18nc("News for a journey with public "
		    "transport, like 'platform changed'", "News:") ).arg( s2 );
	    s3.replace( QRegExp("<[^>]*>"), "" );
	    departureItem->setData( s, HtmlDelegate::FormattedTextRole );
	    departureItem->setText( s3 );
	    if ( !update ) {
		departureItem->setData( 2, Qt::UserRole );
		departureItem->setData( qMin(3, s3.length() / 20), HtmlDelegate::LinesPerRowRole );
		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;

	case DelayItem:
	    s = QString("<b>%1</b> %2")
		    .arg( i18nc("Information about delays of a journey with "
				    "public transport", "Delay:") )
		    .arg( delayText(departureInfo) );
	    if ( departureInfo.delayType() == Delayed ) {
		s += "<br><b>" + (m_settings->departureArrivalListType() == ArrivalList
			? i18n("Original arrival time:")
			: i18n("Original departure time:")) + "</b> " +
			  departureInfo.departure().toString("hh:mm");
		departureItem->setData( 2, HtmlDelegate::LinesPerRowRole );
	    }

	    departureItem->setData( s, HtmlDelegate::FormattedTextRole );
	    departureItem->setText( s.replace(QRegExp("<[^>]*>"), "") );
	    if ( !update ) {
		departureItem->setData( 0, Qt::UserRole );
		setTextColorOfHtmlItem( departureItem, m_colorSubItemLabels );
	    }
	    break;

	default:
	    break;
    }
}

void PublicTransport::appendJourney ( const JourneyInfo& journeyInfo ) {
//     kDebug() << "Append journey";

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
		kDebug() << "not included" << column;
		break;
	}

	items << item;
    }
    m_modelJourneys->appendRow( items );

    int iRow = 0;
    if ( journeyInfo.changes() >= 0 ) {
	QStandardItem *itemChanges = new QStandardItem();
	setValuesOfJourneyItem( itemChanges, journeyInfo, ChangesItem );
	items[0]->insertRow( iRow++, itemChanges );
    }

    if ( !journeyInfo.pricing().isEmpty() ) {
	QStandardItem *itemPricing = new QStandardItem();
	setValuesOfJourneyItem( itemPricing, journeyInfo, PricingItem );
	items[0]->insertRow( iRow++, itemPricing );
    }

    if ( journeyInfo.duration() > 0 ) {
	QStandardItem *itemDuration = new QStandardItem();
	setValuesOfJourneyItem( itemDuration, journeyInfo, DurationItem );
	items[0]->insertRow( iRow++, itemDuration );
    }
    
    if ( !journeyInfo.journeyNews().isEmpty() ) {
	QStandardItem *itemJourneyNews = new QStandardItem();
	setValuesOfJourneyItem( itemJourneyNews, journeyInfo, JourneyNewsItem );
	items[0]->insertRow( iRow++, itemJourneyNews );
    }
    
    if ( !journeyInfo.operatorName().isEmpty() ) {
	QStandardItem *itemOperatorName = new QStandardItem();
	setValuesOfJourneyItem( itemOperatorName, journeyInfo, OperatorItem );
	items[0]->insertRow( iRow++, itemOperatorName );
    }
    
    if ( !journeyInfo.routeStops().isEmpty() ) {
	QStandardItem *itemRoute = new QStandardItem();
	setValuesOfJourneyItem( itemRoute, journeyInfo, RouteItem );
	items[0]->insertRow( iRow++, itemRoute );

	m_treeView->nativeWidget()->expand( itemRoute->index() );
	for ( int row = 0; row < itemRoute->rowCount(); ++row ) {
	    m_treeView->nativeWidget()->setFirstColumnSpanned(
		    row, itemRoute->index(), true );
	}
    }

    for ( int row = 0; row < items[0]->rowCount(); ++row )
	m_treeView->nativeWidget()->setFirstColumnSpanned(row, items[0]->index(), true);
}

void PublicTransport::appendDeparture( const DepartureInfo& departureInfo ) {
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
    if ( !departureInfo.platform().isEmpty() ) {
	QStandardItem *itemPlatform = new QStandardItem();
	setValuesOfDepartureItem( itemPlatform, departureInfo, PlatformItem );
	items[0]->insertRow( iRow++, itemPlatform );
    }

    if ( !departureInfo.journeyNews().isEmpty() ) {
	QStandardItem *itemJourneyNews = new QStandardItem();
	setValuesOfDepartureItem( itemJourneyNews, departureInfo, JourneyNewsItem );
	items[0]->insertRow( iRow++, itemJourneyNews );
    }

    if ( !departureInfo.operatorName().isEmpty() ) {
	QStandardItem *itemOperatorName = new QStandardItem();
	setValuesOfDepartureItem( itemOperatorName, departureInfo, OperatorItem );
	items[0]->insertRow( iRow++, itemOperatorName );
    }
    
    if ( !departureInfo.routeStops().isEmpty() ) {
	QStandardItem *itemRoute = new QStandardItem();
	setValuesOfDepartureItem( itemRoute, departureInfo, RouteItem );
	items[0]->insertRow( iRow++, itemRoute );

	for ( int row = 0; row < itemRoute->rowCount(); ++row ) {
	    m_treeView->nativeWidget()->setFirstColumnSpanned(
		    row, itemRoute->index(), true );
	}
    }

    QStandardItem *itemDelay = new QStandardItem();;
    setValuesOfDepartureItem( itemDelay, departureInfo, DelayItem ); // TODO: set sort data = 0
    items[0]->insertRow( iRow++, itemDelay );

    for ( int row = 0; row < items[0]->rowCount(); ++row )
	m_treeView->nativeWidget()->setFirstColumnSpanned( row, items[0]->index(), true );
}

void PublicTransport::updateJourney ( int row, const JourneyInfo& journeyInfo ) {
    QStandardItem *itemFirst = m_modelJourneys->item( row, 0 );
    QStandardItem *itemVehicleTypes = m_modelJourneys->item(
	    row, m_journeyViewColumns.indexOf(VehicleTypeListColumn) );
    QStandardItem *itemJourneyInfo = m_modelJourneys->item(
	    row, m_journeyViewColumns.indexOf(JourneyInfoColumn) );
    QStandardItem *itemDeparture = m_modelJourneys->item(
	    row, m_journeyViewColumns.indexOf(DepartureColumn) );
    QStandardItem *itemArrival = m_modelJourneys->item(
	    row, m_journeyViewColumns.indexOf(ArrivalColumn) );
    setValuesOfJourneyItem( itemVehicleTypes, journeyInfo, VehicleTypeListItem, true );
    setValuesOfJourneyItem( itemJourneyInfo, journeyInfo, JourneyInfoItem, true );
    setValuesOfJourneyItem( itemDeparture, journeyInfo, DepartureItem, true );
    setValuesOfJourneyItem( itemArrival, journeyInfo, ArrivalItem, true );
    
    // Get items
    QStandardItem *itemChanges = NULL;
    QStandardItem *itemPricing = NULL;
    QStandardItem *itemDuration = NULL;
    QStandardItem *itemJourneyNews = NULL;
    QStandardItem *itemOperatorName = NULL;
    QStandardItem *itemRoute = NULL;
    for ( int i = 0; i < itemFirst->rowCount(); ++i ) {
	int type = itemFirst->child( i )->data( Qt::UserRole ).toInt();
	switch ( type ) { // TODO Put type in an enum
	    case 1:
		itemChanges = itemFirst->child( i );
		break;
	    case 2:
		itemPricing = itemFirst->child( i );
		break;
	    case 3:
		itemDuration = itemFirst->child( i );
		break;
	    case 4:
		itemJourneyNews = itemFirst->child( i );
		break;
	    case 5:
		itemOperatorName = itemFirst->child( i );
		break;
	    case 6:
		itemRoute = itemFirst->child( i );
		break;
	}
    }
    
    // Update changes
    int iRow = itemFirst->rowCount();
    if ( journeyInfo.changes() > 0 ) {
	if ( !itemChanges ) { // Create new changes item
	    itemChanges = new QStandardItem();
	    setValuesOfJourneyItem( itemChanges, journeyInfo, ChangesItem );
	    itemFirst->insertRow( iRow++, itemChanges );
	} else // Update old changes item
	    setValuesOfJourneyItem( itemChanges, journeyInfo, ChangesItem, true );
    } else if ( itemChanges ) {
	itemFirst->removeRow( itemChanges->row() );
	--iRow;
    }

    // Update pricing
    if ( !journeyInfo.pricing().isEmpty() ) {
	if ( !itemPricing ) { // Create new pricing item
	    itemPricing = new QStandardItem();
	    setValuesOfJourneyItem( itemPricing, journeyInfo, PricingItem );
	    itemFirst->insertRow( iRow++, itemPricing );
	} else // Update old pricing item
	    setValuesOfJourneyItem( itemPricing, journeyInfo, PricingItem, true );
    } else if ( itemPricing ) {
	itemFirst->removeRow( itemPricing->row() );
	--iRow;
    }

    // Update duration
    if ( !journeyInfo.pricing().isEmpty() ) {
	if ( !itemDuration ) { // Create new duration item
	    itemDuration = new QStandardItem();
	    setValuesOfJourneyItem( itemDuration, journeyInfo, DurationItem );
	    itemFirst->insertRow( iRow++, itemDuration );
	} else // Update old duration item
	    setValuesOfJourneyItem( itemDuration, journeyInfo, DurationItem, true );
    } else if ( itemDuration ) {
	itemFirst->removeRow( itemDuration->row() );
	--iRow;
    }
    
    // Update journey news
    if ( !journeyInfo.journeyNews().isEmpty() ) {
	if ( !itemJourneyNews ) { // Create new journey news item
	    itemJourneyNews = new QStandardItem();
	    setValuesOfJourneyItem( itemJourneyNews, journeyInfo, JourneyNewsItem );
	    itemFirst->insertRow( iRow++, itemJourneyNews );
	} else // Update old journey news item
	    setValuesOfJourneyItem( itemJourneyNews, journeyInfo, JourneyNewsItem, true );
    } else if ( itemJourneyNews ) {
	itemFirst->removeRow( itemJourneyNews->row() );
	--iRow;
    }
    
    // Update operator name
    if ( !journeyInfo.operatorName().isEmpty() ) {
	if ( !itemOperatorName ) { // Create new operator item
	    itemOperatorName = new QStandardItem();
	    setValuesOfJourneyItem( itemOperatorName, journeyInfo, OperatorItem );
	    itemFirst->insertRow( iRow++, itemOperatorName );
	} else // Update old journey news item
	    setValuesOfJourneyItem( itemOperatorName, journeyInfo, OperatorItem, true );
    } else if ( itemOperatorName ) {
	itemFirst->removeRow( itemOperatorName->row() );
	--iRow;
    }
    
    // Update route
    if ( !journeyInfo.routeStops().isEmpty() ) {
	if ( !itemRoute ) { // Create new route item
	    itemRoute = new QStandardItem();
	    setValuesOfJourneyItem( itemRoute, journeyInfo, RouteItem );
	    itemFirst->insertRow( iRow++, itemRoute );
	} else // Update old route item
	    setValuesOfJourneyItem( itemRoute, journeyInfo, RouteItem, true );
	
	m_treeView->nativeWidget()->expand( itemRoute->index() );
	
	for ( int row = 0; row < itemRoute->rowCount(); ++row ) {
	    m_treeView->nativeWidget()->setFirstColumnSpanned(
	    row, itemRoute->index(), true );
	}
    } else if ( itemRoute ) {
	itemFirst->removeRow( itemRoute->row() );
	--iRow;
    }
    
    for ( int row = 0; row < itemFirst->rowCount(); ++row )
	m_treeView->nativeWidget()->setFirstColumnSpanned(row, itemFirst->index(), true);
}

void PublicTransport::updateDeparture( int row, const DepartureInfo& departureInfo ) {
    QStandardItem *itemLineString = m_model->item(
	    row, m_departureViewColumns.indexOf(LineStringColumn) );
    QStandardItem *itemTarget = m_model->item(
	    row, m_departureViewColumns.indexOf(TargetColumn) );
    QStandardItem *itemDeparture = m_model->item(
	    row, m_departureViewColumns.indexOf(DepartureColumn) );
    setValuesOfDepartureItem( itemLineString, departureInfo, LineNameItem, true );
    setValuesOfDepartureItem( itemTarget, departureInfo, TargetItem, true );
    setValuesOfDepartureItem( itemDeparture, departureInfo, DepartureItem, true );

    // Get items
    QStandardItem *itemPlatform = NULL;
    QStandardItem *itemJourneyNews = NULL;
    QStandardItem *itemOperatorName = NULL;
    QStandardItem *itemRoute = NULL;
    QStandardItem *itemDelay = NULL;
    for ( int i = 0; i < itemLineString->rowCount(); ++i ) {
	int type = itemLineString->child( i )->data( Qt::UserRole ).toInt();
	switch ( type ) { // TODO Put type in an enum
	    case 0:
		itemDelay = itemLineString->child( i );
		break;
	    case 1:
		itemPlatform = itemLineString->child( i );
		break;
	    case 2:
		itemJourneyNews = itemLineString->child( i );
		break;
	    case 4:
		itemOperatorName = itemLineString->child( i );
		break;
	    case 5:
		itemRoute = itemLineString->child( i );
		break;
	}
    }
    
    // Update platform
    int iRow = itemLineString->rowCount();
    if ( !departureInfo.platform().isEmpty() ) {
	if ( !itemPlatform ) { // Create new platform item
	    itemPlatform = new QStandardItem();
	    setValuesOfDepartureItem( itemPlatform, departureInfo, PlatformItem );
	    itemLineString->insertRow( iRow++, itemPlatform );
	} else // Update old platform item
	    setValuesOfDepartureItem( itemPlatform, departureInfo, PlatformItem, true );
    } else if ( itemPlatform ) {
	itemLineString->removeRow( itemPlatform->row() );
	--iRow;
    }

    // Update journey news
    if ( !departureInfo.journeyNews().isEmpty() ) {
	if ( !itemJourneyNews ) { // Create new journey news item
	    itemJourneyNews = new QStandardItem();
	    setValuesOfDepartureItem( itemJourneyNews, departureInfo, JourneyNewsItem );
	    itemLineString->insertRow( iRow++, itemJourneyNews );
	} else // Update old journey news item
	    setValuesOfDepartureItem( itemJourneyNews, departureInfo, JourneyNewsItem, true );
    } else if ( itemJourneyNews ) {
	itemLineString->removeRow( itemJourneyNews->row() );
	--iRow;
    }
    
    // Update operator name
    if ( !departureInfo.operatorName().isEmpty() ) {
	if ( !itemOperatorName ) { // Create new operator item
	    itemOperatorName = new QStandardItem();
	    setValuesOfDepartureItem( itemOperatorName, departureInfo, OperatorItem );
	    itemLineString->insertRow( iRow++, itemOperatorName );
	} else // Update old operator item
	    setValuesOfDepartureItem( itemOperatorName, departureInfo, OperatorItem, true );
    } else if ( itemOperatorName ) {
	itemLineString->removeRow( itemOperatorName->row() );
	--iRow;
    }
    
    // Update route
    if ( !departureInfo.routeStops().isEmpty() ) {
	if ( !itemRoute ) { // Create new route item
	    itemRoute = new QStandardItem();
	    setValuesOfDepartureItem( itemRoute, departureInfo, RouteItem );
	    itemLineString->insertRow( iRow++, itemRoute );
	} else // Update old route item
	    setValuesOfDepartureItem( itemRoute, departureInfo, RouteItem, true );
	
	for ( int row = 0; row < itemRoute->rowCount(); ++row ) {
	    m_treeView->nativeWidget()->setFirstColumnSpanned(
		    row, itemRoute->index(), true );
	}
    } else if ( itemRoute ) {
	itemLineString->removeRow( itemRoute->row() );
	--iRow;
    }

    // Update delay
    if ( !itemDelay ) { // Create new delay item
	itemDelay = new QStandardItem();
	setValuesOfDepartureItem( itemDelay, departureInfo, DelayItem );
	itemLineString->insertRow( iRow++, itemDelay );
    } else { // Update old delay item
	setValuesOfDepartureItem( itemDelay, departureInfo, DelayItem );
    }

    for ( int row = 0; row < itemLineString->rowCount(); ++row )
	m_treeView->nativeWidget()->setFirstColumnSpanned(row, itemLineString->index(), true);
}

void PublicTransport::removeOldDepartures() {
    // Go through departure lists of all stops.
    // Add all departures that can be found in the model to foundRows.
    int newDepartureCount = 0;
    QList< int > foundRows;
    for ( int n = m_stopIndexToSourceName.count() - 1; n >= 0; --n ) {
	QString strippedSourceName = stripDateAndTimeValues( m_stopIndexToSourceName[n] );
	if ( !m_departureInfos.contains(strippedSourceName) ) {
	    kDebug() << "Source has no stored data in applet" << strippedSourceName;
	    continue;
	}
	
	// Go through departure infos of the current stop in the loop
	QList<DepartureInfo> &depInfos = m_departureInfos[ strippedSourceName ];
	newDepartureCount += depInfos.count(); // For debug only
	for ( int i = depInfos.count() - 1; i >= 0; --i ) {
	    DepartureInfo &departureInfo = depInfos[ i ];
	    int row = findDeparture( departureInfo );
	    if ( row != -1 ) {
		if ( filterOut(departureInfo) ) {
		    // Filtered out because departure/arrival is in the past
		    // or because of a change to the filter settings
		    kDebug() << "Item gets filtered out at row" << row;
		    depInfos.removeAt( i );
		} else
		    foundRows << row;
	    } /*else {
		kDebug() << "New departure" << departureInfo.departure().time()
			 << departureInfo.target() << departureInfo.lineString();
	    }*/
	}
    }

    kDebug() << "Found" << foundRows.count() << "of" << newDepartureCount
	     << "departures," << (m_model->rowCount() - foundRows.count())
	     << "departures get removed";

    // Remove all departures that no longer have a corresponding departure 
    // from the data engine (old departures).
    for ( int row = m_model->rowCount() - 1; row >= 0; --row ) {
	if ( foundRows.contains(row) )
	    continue;

	QStandardItem *itemDeparture = m_model->item( row, 2 );
	AlarmTimer *alarmTimer = static_cast< AlarmTimer* >(
		itemDeparture->data(AlarmTimerRole).value<void*>() );
	if ( alarmTimer && alarmTimer->timer()->isActive() ) {
	    m_abandonedAlarmTimer.append( alarmTimer );
	    kDebug() << "Append abandoned alarm timer of not found row" << row
		     << "Abandoned alarm timer count:" << m_abandonedAlarmTimer.count();
	}

	m_model->removeRow( row );
    }
}

void PublicTransport::removeOldJourneys() {
    kDebug() << m_journeyInfos.count() << "journeys";

    QList< int > foundRows;
    foreach( JourneyInfo journeyInfo, m_journeyInfos ) {
	int row = findJourney( journeyInfo );
	if ( row != -1 ) {
	    // TODO: Filtering for journeys?
	    foundRows << row;
	}
    }
    
    for ( int row = m_modelJourneys->rowCount() - 1; row >= 0; --row ) {
	if ( foundRows.contains(row) )
	    continue;
	
	m_modelJourneys->removeRow( row );
    }
}

void PublicTransport::updateModelJourneys() {
    if ( !m_graphicsWidget )
	graphicsWidget();

    int sortSection = m_treeView->nativeWidget()->header()->sortIndicatorSection();
    Qt::SortOrder sortOrder = m_treeView->nativeWidget()->header()->sortIndicatorOrder();

    removeOldJourneys(); //  also remove filtered departures  (after changing filter settings)?
    foreach( JourneyInfo journeyInfo, m_journeyInfos )
    {
	int row = findJourney(journeyInfo);

	// Apply filters TODO
// 	if ( filterOut(journeyInfo) )
// 	    continue;

	if ( row != -1 ) { // just update departure data
	    updateJourney( row, journeyInfo );
// 	    kDebug() << "Update journey row" << row;
	} else { // append new departure
	    appendJourney( journeyInfo );
// 	    kDebug() << "Append journey row";
	}
    }

    // Restore sort indicator
    m_treeView->nativeWidget()->header()->setSortIndicator( sortSection, sortOrder );

    // Sort list of journeys
    qSort( m_journeyInfos.begin(), m_journeyInfos.end() );

    geometryChanged();
}

void PublicTransport::updateModel() {
    if ( !m_graphicsWidget )
	graphicsWidget();

    int sortSection = m_treeView->nativeWidget()->header()->sortIndicatorSection();
    Qt::SortOrder sortOrder = m_treeView->nativeWidget()->header()->sortIndicatorOrder();

    if ( m_titleType == ShowDepartureArrivalListTitle ) {
	m_label->setText( titleText() );
	m_labelInfo->setToolTip( courtesyToolTip() );
	m_labelInfo->setText( infoText() );
    }
    removeOldDepartures(); // also remove filtered departures  (after changing filter settings)?
    
    QList< DepartureInfo > depInfos = departureInfos();
    kDebug() << "Update / add" << depInfos.count() << "departures";
    int updated = 0, appended = 0;
    foreach( DepartureInfo departureInfo, depInfos ) {
	// Apply filters
	if ( filterOut(departureInfo) ) {
	    kDebug() << "Departure filtered out" << departureInfo.predictedDeparture().time()
		     << departureInfo.target() << departureInfo.lineString();
	    continue;
	}

	int row = findDeparture( departureInfo );
	if ( row != -1 ) { // just update departure data
	    updateDeparture( row, departureInfo );
	    ++updated;
// 	    kDebug() << "Update row" << row;
	} else { // append new departure
	    appendDeparture( departureInfo );
	    ++appended;
// 	    kDebug() << "Append row";
	}
    }
    kDebug() << "   - " << appended << "new departures";
    kDebug() << "   - " << updated << "departures updated";

    // Restore sort indicator
    m_treeView->nativeWidget()->header()->setSortIndicator( sortSection, sortOrder );

    geometryChanged();
}


#include "publictransport.moc"