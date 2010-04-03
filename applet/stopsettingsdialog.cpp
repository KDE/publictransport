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
#include "dynamicwidget.h"
#include "settings.h"

#include <Plasma/Theme>
#include <KColorScheme>
#include <KMessageBox>
#include <KFileDialog>
#include <KStandardDirs>
#include <KLineEdit>
#include <kdeversion.h>
#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
    #include <knewstuff3/downloaddialog.h>
#else
    #include <knewstuff2/engine.h>
#endif

#include <QMenu>
#include <QListView>
#include <QStringListModel>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QProcess>
#if QT_VERSION >= 0x040600
    #include <QParallelAnimationGroup>
    #include <QPropertyAnimation>
    #include <QGraphicsEffect>
#endif

#ifdef USE_KCATEGORYVIEW
    #include <KCategorizedSortFilterProxyModel>
    #include <KCategorizedView>
    #include <KCategoryDrawer>
#endif


class NearStopsDialog : public KDialog {
    public:
	NearStopsDialog( const QString &text, QWidget* parent = 0 ) : KDialog( parent ) {
	    setButtons( Ok | Cancel );

	    QWidget *w = new QWidget;
	    QVBoxLayout *layout = new QVBoxLayout;
	    m_label = new QLabel( text, this );
	    m_label->setWordWrap( true );
	    m_listView = new QListView( this );
	    m_listView->setSelectionMode( QAbstractItemView::SingleSelection );
	    m_listView->setEditTriggers( QAbstractItemView::NoEditTriggers );
	    m_listModel = new QStringListModel( QStringList()
			    << i18n("Please Wait..."), this );
	    m_listView->setModel( m_listModel );
	    layout->addWidget( m_label );
	    layout->addWidget( m_listView );
	    w->setLayout( layout );
	    setMainWidget( w );

	    m_noItem = true;
	};

	QListView *listView() const { return m_listView; };
	QString selectedStop() const {
	    QModelIndex index = m_listView->currentIndex();
	    if ( index.isValid() )
		return m_listModel->data( index, Qt::DisplayRole ).toString();
	    else
		return QString(); };
	QStringListModel *stopsModel() const { return m_listModel; };

	void addStops( const QStringList &stops ) {
	    if ( m_noItem ) {
		// Remove the "waiting for data..." item
		m_listModel->setStringList( QStringList() );
	    }

	    QStringList oldStops = m_listModel->stringList();
	    QStringList newStops;
	    newStops << oldStops;
	    foreach ( const QString &stop, stops ) {
		if ( !newStops.contains(stop) && !stop.isEmpty() )
		    newStops << stop;
	    }
	    newStops.removeDuplicates();

	    if ( !newStops.isEmpty() ) {
		if ( m_noItem ) {
		    m_noItem = false;
		    m_listView->setEnabled( true );
		}
		m_listModel->setStringList( newStops );
		m_listModel->sort( 0 );
	    } else if ( m_noItem ) {
		m_listModel->setStringList( oldStops );
	    }
	};

	bool hasItems() const { return !m_noItem; };

    private:
	QLabel *m_label;
	QListView *m_listView;
	QStringListModel *m_listModel;
	bool m_noItem;
};

StopSettingsDialog::StopSettingsDialog( const StopSettings &stopSettings,
	    const QStringList &filterConfigurations, QStandardItemModel *modelLocations,
	    QStandardItemModel *modelServiceProviders,
	    Plasma::DataEngine *publicTransportEngine, Plasma::DataEngine *osmEngine,
	    Plasma::DataEngine *geolocationEngine, QWidget *parent )
	    : KDialog( parent ), m_stopFinder(0), m_nearStopsDialog( 0 ),
	      m_modelLocations( modelLocations ),
	      m_modelServiceProviders( modelServiceProviders ),
	      m_modelLocationServiceProviders(0),
	      m_publicTransportEngine( publicTransportEngine ),
	      m_osmEngine( osmEngine ), m_geolocationEngine( geolocationEngine ) {
    setWindowTitle( i18n("Change Stop(s)") );
    m_uiStop.setupUi( mainWidget() );

    setButtons( Ok | Cancel | Details | User1 );
    setButtonIcon( User1, KIcon("tools-wizard") );
    setButtonText( User1, i18n("Nearby Stops...") );
    
    QWidget *detailsWidget = new QWidget( this );
    m_uiStopDetails.setupUi( detailsWidget );
    setDetailsWidget( detailsWidget );
    
    // Create model that filters service providers for the current location
    m_modelLocationServiceProviders = new QSortFilterProxyModel( this );
    m_modelLocationServiceProviders->setSourceModel( modelServiceProviders );
    m_modelLocationServiceProviders->setFilterRole( LocationCodeRole );
    
    #ifdef USE_KCATEGORYVIEW
    KCategorizedSortFilterProxyModel *modelCategorized =
	    new KCategorizedSortFilterProxyModel( this );
    modelCategorized->setCategorizedModel( true );
    modelCategorized->setSourceModel( m_modelLocationServiceProviders );
    #endif
    
    // Create stop list widget
    m_stopList = new DynamicLabeledLineEditList( 
	    DynamicLabeledLineEditList::RemoveButtonsBesideWidgets,
	    DynamicLabeledLineEditList::AddButtonBesideFirstWidget,
	    DynamicLabeledLineEditList::NoSeparator, QString(), this );
    m_stopList->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
    connect( m_stopList, SIGNAL(added(QWidget*)), this, SLOT(stopAdded(QWidget*)) );
    connect( m_stopList, SIGNAL(added(QWidget*)), this, SLOT(adjustStopListLayout()) );
    connect( m_stopList, SIGNAL(removed(QWidget*)), this, SLOT(adjustStopListLayout()) );
    m_stopList->setLabelTexts( i18n("Combined Stop") + " %1:", QStringList() << "Stop:" );
    m_stopList->setWidgetCountRange( 1, 3 );
    if ( m_stopList->addButton() ) {
	m_stopList->addButton()->setToolTip( i18n("Add another stop.\n"
		"The departures/arrivals of all stops get combined.") );
    }
    m_stopList->setWhatsThis( i18n("All departures/arrivals for these stops get "
	    "<b>displayed combined</b> in the applet.<br>"
	    "To add a stop that doesn't get combined with others use the 'Add Stop' "
	    "button of the main settings dialog.") );
    
    QVBoxLayout *l = new QVBoxLayout( m_uiStop.stops );
    l->setContentsMargins( 0, 0, 0, 0 );
    l->addWidget( m_stopList );

    m_uiStop.btnServiceProviderInfo->setIcon( KIcon("help-about") );
    m_uiStop.btnServiceProviderInfo->setText( QString() );

    m_uiStopDetails.filterConfiguration->addItems( filterConfigurations );

    QMenu *menu = new QMenu( this );
    menu->addAction( KIcon("get-hot-new-stuff"), i18n("Get new service providers..."),
		     this, SLOT(downloadServiceProvidersClicked()) );
    menu->addAction( KIcon("text-xml"),
		     i18n("Install new service provider from local file..."),
		     this, SLOT(installServiceProviderClicked()) );
    m_uiStopDetails.downloadServiceProviders->setMenu( menu );
    m_uiStopDetails.downloadServiceProviders->setIcon( KIcon("list-add") );
    
#ifdef USE_KCATEGORYVIEW
    KCategorizedView *serviceProviderView = new KCategorizedView( this );
    #if KDE_VERSION >= KDE_MAKE_VERSION(4,4,0)
    KCategoryDrawerV2 *categoryDrawer = new KCategoryDrawerV2( this );
    serviceProviderView->setCategorySpacing( 10 );
    #else
    categoryDrawer = new KCategoryDrawer();
    #endif
    serviceProviderView->setWordWrap( true );
    serviceProviderView->setCategoryDrawer( categoryDrawer );
    serviceProviderView->setSelectionMode( QAbstractItemView::SingleSelection );
    serviceProviderView->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel ); // If ScrollPerItem is used the view can't be scrolled in QListView::ListMode.

    m_uiStop.serviceProvider->setView( serviceProviderView );
    m_uiStop.serviceProvider->setModel( modelCategorized );
#else
    m_uiStop.serviceProvider->setModel( m_modelLocationServiceProviders );
#endif
    m_uiStop.location->setModel( m_modelLocations );
    
    // Set html delegate
    m_htmlDelegate = new HtmlDelegate( HtmlDelegate::NoOption, this );
    m_htmlDelegate->setAlignText( true );
    m_uiStop.serviceProvider->setItemDelegate( m_htmlDelegate );
    m_uiStop.location->setItemDelegate( m_htmlDelegate );

    connect( this, SIGNAL(user1Clicked()), this, SLOT(geolocateClicked()) );
    connect( m_uiStop.location, SIGNAL(currentIndexChanged(const QString&)),
	     this, SLOT(locationChanged(const QString&)) );
    connect( m_uiStop.serviceProvider, SIGNAL(currentIndexChanged(int)),
	     this, SLOT(serviceProviderChanged(int)) );
    connect( m_uiStop.city, SIGNAL(currentIndexChanged(QString)),
	     this, SLOT(cityNameChanged(QString)) );
    connect( m_stopList, SIGNAL(textEdited(QString,int)),
	     this, SLOT(stopNameEdited(QString,int)) );
    connect( m_uiStop.btnServiceProviderInfo, SIGNAL(clicked()),
	     this, SLOT(clickedServiceProviderInfo()) );
    
    setStopSettings( stopSettings );
    m_stopList->lineEditWidgets().first()->setFocus(); // Minimum widget count is 1
}

StopSettingsDialog::~StopSettingsDialog() {
#ifdef USE_KCATEGORYVIEW
#if KDE_VERSION < KDE_MAKE_VERSION(4,4,0)
    delete categoryDrawer;
#endif
#endif
}

void StopSettingsDialog::setStopSettings( const StopSettings& stopSettings ) {
    // Set stops from stopSettings
    m_stopList->setLineEditTexts( stopSettings.stops );

    // Select filter configuration from stopSettings
    QString trFilterConfiguration = SettingsUiManager::translateKey(
	    stopSettings.filterConfiguration );
    if ( m_uiStopDetails.filterConfiguration->contains(trFilterConfiguration) ) {
	m_uiStopDetails.filterConfiguration->setCurrentItem( trFilterConfiguration );
    } /*else if ( trFilterConfiguration.isEmpty() ) {
	int noneIndex = m_uiStopDetails.filterConfiguration->findData( "none" );
	kDebug() << "NONE INDEX" << noneIndex;
	m_uiStopDetails.filterConfiguration->setCurrentIndex( noneIndex );
    }*/
    
    // Select location from stopSettings
    QModelIndexList indicesLocation = m_modelLocations->match(
	    m_modelLocations->index(0, 0), LocationCodeRole,
	    stopSettings.location, 1, Qt::MatchFixedString );
    if ( !indicesLocation.isEmpty() )
	m_uiStop.location->setCurrentIndex( indicesLocation.first().row() );
    else
	m_uiStop.location->setCurrentIndex( 1 );
    
    // Select service provider from stopSettings
    if ( !stopSettings.serviceProviderID.isEmpty() ) {
	QModelIndexList indices = m_modelLocationServiceProviders->match(
		m_modelLocationServiceProviders->index(0, 0), ServiceProviderIdRole,
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

    m_uiStopDetails.timeOfFirstDeparture->setValue(
	    stopSettings.timeOffsetOfFirstDeparture );
    m_uiStopDetails.timeOfFirstDepartureCustom->setTime(
	    stopSettings.timeOfFirstDepartureCustom );
    m_uiStopDetails.firstDepartureUseCurrentTime->setChecked(
	    stopSettings.firstDepartureConfigMode == RelativeToCurrentTime );
    m_uiStopDetails.firstDepartureUseCustomTime->setChecked(
	    stopSettings.firstDepartureConfigMode == AtCustomTime );
    m_uiStopDetails.alarmTime->setValue( stopSettings.alarmTime );
}

StopSettings StopSettingsDialog::stopSettings() const {
    StopSettings stopSettings;
    QVariantHash serviceProviderData = m_modelLocationServiceProviders->index(
	    m_uiStop.serviceProvider->currentIndex(), 0 )
	    .data( ServiceProviderDataRole ).toHash();
    stopSettings.serviceProviderID = serviceProviderData["id"].toString();
    stopSettings.location = m_uiStop.location->itemData(
	m_uiStop.location->currentIndex(), LocationCodeRole ).toString();

    QVariantHash curServiceProviderData = m_uiStop.serviceProvider->itemData(
	m_uiStop.serviceProvider->currentIndex(), ServiceProviderDataRole ).toHash();
    if ( curServiceProviderData["useSeparateCityValue"].toBool() ) {
	stopSettings.city = m_uiStop.city->isEditable()
		? m_uiStop.city->lineEdit()->text() : m_uiStop.city->currentText();
    }
//     if ( m_uiStopDetails.filterConfiguration->itemData(
// 	    m_uiStopDetails.filterConfiguration->currentIndex()).toString() == "none" ) {
// 	stopSettings.filterConfiguration = QString();
//     } else {
	stopSettings.filterConfiguration = SettingsUiManager::untranslateKey(
		m_uiStopDetails.filterConfiguration->currentText() );
//     }
    stopSettings.stops = m_stopList->lineEditTexts();
    foreach ( const QString &stop, stopSettings.stops ) {
	if ( m_stopToStopID.contains(stop) )
	    stopSettings.stopIDs << m_stopToStopID[ stop ].toString();
	else
	    stopSettings.stopIDs << stop;
    }
    
    stopSettings.timeOffsetOfFirstDeparture = m_uiStopDetails.timeOfFirstDeparture->value();
    stopSettings.timeOfFirstDepartureCustom = m_uiStopDetails.timeOfFirstDepartureCustom->time();
    stopSettings.firstDepartureConfigMode =
	    m_uiStopDetails.firstDepartureUseCurrentTime->isChecked()
	    ? RelativeToCurrentTime : AtCustomTime;
    stopSettings.alarmTime = m_uiStopDetails.alarmTime->value();
    
    return stopSettings;
}

void StopSettingsDialog::geolocateClicked() {
    m_stopFinder = new StopFinder( StopFinder::ValidatedStopNamesFromOSM,
		    m_publicTransportEngine, m_osmEngine, m_geolocationEngine,
		    25, StopFinder::DeleteWhenFinished, this );
    connect( m_stopFinder, SIGNAL(geolocationData(QString,QString,qreal,qreal,int)),
	     this, SLOT(stopFinderGeolocationData(QString,QString,qreal,qreal,int)) );
    connect( m_stopFinder, SIGNAL(error(StopFinder::Error,QString)),
	     this, SLOT(stopFinderError(StopFinder::Error,QString)) );
    connect( m_stopFinder, SIGNAL(finished()), this, SLOT(stopFinderFinished()) );
    connect( m_stopFinder, SIGNAL(stopsFound(QStringList,QStringList,QString)),
	     this, SLOT(stopFinderFoundStops(QStringList,QStringList,QString)) );

    m_stopFinder->start();
}

void StopSettingsDialog::stopFinderError( StopFinder::Error /*error*/,
					  const QString& errorMessage ) {
    if ( m_nearStopsDialog ) {
	m_nearStopsDialog->close();
	m_nearStopsDialog = NULL;
	
	KMessageBox::information( this, errorMessage );
    }
}

void StopSettingsDialog::stopFinderFinished() {
    m_stopFinder = NULL; // Deletes itself when finished
    
    // Close dialog and show info if no stops could be found
    if ( m_nearStopsDialog && !m_nearStopsDialog->hasItems() ) {
	m_nearStopsDialog->close();
	m_nearStopsDialog = NULL;

	// Get data from the geolocation data engine
	Plasma::DataEngine::Data dataGeo = m_geolocationEngine->query("location");
	QString country = dataGeo["country code"].toString().toLower();
	QString city = dataGeo["city"].toString();

	KMessageBox::information( this,
		i18n("No stop could be found for your current position (%2 in %1).\n"
		     "This doesn't mean that there is no public transport "
		     "stop near you. Try setting the stop name manually.",
		KGlobal::locale()->countryCodeToName(country), city) );
    }
}

void StopSettingsDialog::stopFinderFoundStops( const QStringList& stops,
		    const QStringList& stopIDs, const QString &serviceProviderID ) {
    for ( int i = 0; i < qMin(stops.count(), stopIDs.count()); ++i )
	m_stopToStopID.insert( stops[i], stopIDs[i] );
    m_stopFinderServiceProviderID = serviceProviderID;

    if ( m_nearStopsDialog )
	m_nearStopsDialog->addStops( stops );
}

void StopSettingsDialog::stopFinderGeolocationData( const QString& countryCode,
	const QString& city, qreal /*latitude*/, qreal /*longitude*/, int accuracy ) {
    m_nearStopsDialog = new NearStopsDialog( accuracy > 10000
	    ? i18n("These stops <b>may</b> be near you, but your position "
		"couldn't be determined exactly (city: %1, country: %2). "
		"Choose one of them or cancel.",
		city, KGlobal::locale()->countryCodeToName(countryCode))
	    : i18n("These stops have been found to be near you (city: %1, "
		"country: %2). Choose one of them or cancel.",
		city, KGlobal::locale()->countryCodeToName(countryCode)),
	    this );
    m_nearStopsDialog->setModal( true );
    m_nearStopsDialog->listView()->setDisabled( true );
    connect( m_nearStopsDialog, SIGNAL(finished(int)),
	     this, SLOT(nearStopsDialogFinished(int)) );
    m_nearStopsDialog->show();
}

void StopSettingsDialog::nearStopsDialogFinished( int result ) {
    if ( result == KDialog::Accepted ) {
	QString stop = m_nearStopsDialog->selectedStop();
	m_stopFinder->deleteLater();
	m_stopFinder = NULL;
	
	if ( stop.isNull() ) {
	    kDebug() << "No stop selected";
	} else {
	    StopSettings settings = stopSettings();
	    Plasma::DataEngine::Data geoData = m_geolocationEngine->query("location");
	    settings.city = geoData["city"].toString();
	    settings.location = geoData["country code"].toString();
	    settings.serviceProviderID = m_stopFinderServiceProviderID;
	    settings.stops.clear();
	    settings.stops << stop;
	    settings.stopIDs.clear();
	    if ( m_stopToStopID.contains(stop) )
		settings.stopIDs << m_stopToStopID[ stop ].toString();
	    setStopSettings( settings );
	}
    }
    
//     delete m_nearStopsDialog; // causes a crash (already deleted..?)
    m_nearStopsDialog = NULL;
}

// void StopSettingsDialog::selectLocaleLocation() {
//     QString location = KGlobal::locale()->country();
//     QModelIndexList indices = m_modelLocations->match( m_modelLocations->index(0, 0),
// 			LocationCodeRole, location, 1, Qt::MatchFixedString );
//     if ( !indices.isEmpty() ) {
// 	m_uiStop.location->setCurrentIndex( indices.first().row() );
//     } else {
// 	kDebug() << "Location with country code" << location << "not found";
// 	m_uiStop.location->setCurrentIndex( 0 );
//     }
// }

void StopSettingsDialog::accept() {
    m_stopList->removeEmptyLineEdits();

    QStringList stops = m_stopList->lineEditTexts();
    int indexOfFirstEmpty = stops.indexOf( QString() );
    if ( indexOfFirstEmpty != -1 ) {
	KMessageBox::information( this, i18n("Empty stop names are not allowed.") );
	m_stopList->lineEditWidgets()[ indexOfFirstEmpty ]->setFocus();
    } else {
	KDialog::accept();
    }
}

void StopSettingsDialog::resizeEvent( QResizeEvent *event ) {
    KDialog::resizeEvent( event );
    adjustStopListLayout();
}

void StopSettingsDialog::adjustStopListLayout() {
    // Align elements in m_stopList with the main dialog's layout (at least a bit ;))
    QWidgetList labelList = QWidgetList() << m_uiStop.lblLocation
	    << m_uiStop.lblServiceProvider << m_uiStop.lblCity;
    int maxLabelWidth = 0;
    foreach ( QWidget *w, labelList ) {
	if ( w->width() > maxLabelWidth )
	    maxLabelWidth = w->width();
    }
    QLabel *label = m_stopList->labelFor( m_stopList->lineEditWidgets().first() );
    label->setMinimumWidth( maxLabelWidth );
    label->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
}

void StopSettingsDialog::stopAdded( QWidget *lineEdit ) {
    // Enable completer for new line edits
    KLineEdit *edit = qobject_cast< KLineEdit* >( lineEdit );
    edit->setCompletionMode( KGlobalSettings::CompletionPopup );
}

void StopSettingsDialog::stopNameEdited( const QString&, int widgetIndex ) {
    requestStopSuggestions( widgetIndex );
}

void StopSettingsDialog::requestStopSuggestions( int stopIndex ) {
    StopSettings settings = stopSettings();
    if ( !settings.city.isEmpty() ) { // m_useSeparateCityValue ) {
	m_publicTransportEngine->connectSource( QString("Stops %1|stop=%2|city=%3")
		.arg(settings.serviceProviderID, settings.stops[stopIndex],
		settings.city), this );
    } else {
	m_publicTransportEngine->connectSource( QString("Stops %1|stop=%2")
		.arg(settings.serviceProviderID, settings.stops[stopIndex]), this );
    }
}

void StopSettingsDialog::dataUpdated( const QString& sourceName,
				      const Plasma::DataEngine::Data& data ) {
    if ( sourceName.startsWith(QLatin1String("Stops")) ) {
	// Stop suggestions data
	if ( data.value("error").toBool() ) {
	    kDebug() << "Stop suggestions error";
	    // TODO: Handle error somehow?
	} else if ( data.value("receivedPossibleStopList").toBool() ) {
	    processStopSuggestions( data );
	}
    }
}

void StopSettingsDialog::processStopSuggestions( const Plasma::DataEngine::Data& data ) {
    QStringList stops, weightedStops;
    QHash<QString, QVariant> stopToStopWeight;
    bool hasAtLeastOneWeight = false;
    int count = data["count"].toInt();
    for (int i = 0; i < count; ++i) {
	QVariant stopData = data.value( QString("stopName %1").arg(i) );
	if ( !stopData.isValid() )
	    continue;
	
	QHash<QString, QVariant> dataMap = stopData.toHash();
	QString sStopName = dataMap["stopName"].toString();
	QString sStopID = dataMap["stopID"].toString();
	int stopWeight = dataMap["stopWeight"].toInt();
	stops.append( sStopName );
	stopToStopWeight.insert( sStopName, stopWeight );
	
	m_stopToStopID.insert( sStopName, sStopID );
    }

    // Construct weighted stop list for KCompletion
    foreach ( const QString &stop, stops ) {
	int stopWeight = stopToStopWeight[ stop ].toInt();
	if ( stopWeight <= 0 )
	    stopWeight = 0;
	else
	    hasAtLeastOneWeight = true;
	
	weightedStops << QString( "%1:%2" ).arg( stop ).arg( stopWeight );
    }
    
    KLineEdit *stop = m_stopList->focusedLineEdit();
    if ( stop ) { // One stop edit line has focus
	kDebug() << "Prepare completion object";
	KCompletion *comp = stop->completionObject();
	comp->setIgnoreCase( true );
	if ( hasAtLeastOneWeight ) {
	    comp->setOrder( KCompletion::Weighted );
	    comp->insertItems( weightedStops );
	} else {
	    comp->setOrder( KCompletion::Insertion);
	    comp->insertItems( stops );
	}
	
	// Complete manually, because the completions are requested asynchronously
	stop->doCompletion( stop->text() );
    } else
	kDebug() << "No stop line edit has focus, discard received stops.";
}

void StopSettingsDialog::updateServiceProviderModel( const QString& locationText ) {
    // Filter service providers for the given locationText
    int index = locationText.isEmpty() ? m_uiStop.location->currentIndex()
	    : m_uiStop.location->findText( locationText );
    QString locationCode = m_uiStop.location->itemData(index, LocationCodeRole).toString();
    if ( locationCode == "showAll" ) {
	m_modelLocationServiceProviders->setFilterRegExp( QString() );
    } else {
	m_modelLocationServiceProviders->setFilterRegExp(
		QString("%1|international|unknown").arg(locationCode) );
    }
}

void StopSettingsDialog::locationChanged( const QString& newLocation ) {
    updateServiceProviderModel( newLocation );

    // Select default accessor of the selected location
    QString locationCode = m_uiStop.location->itemData(
	    m_uiStop.location->findText(newLocation), LocationCodeRole ).toString();
    Plasma::DataEngine::Data locationData = m_publicTransportEngine->query( "Locations" );
    QString defaultServiceProviderId =
	    locationData[locationCode].toHash()["defaultAccessor"].toString();
    if ( !defaultServiceProviderId.isEmpty() ) {
	QModelIndexList indices = m_modelLocationServiceProviders->match(
		m_modelLocationServiceProviders->index(0, 0), ServiceProviderIdRole,
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
	    m_modelLocationServiceProviders->index( index, 0 )
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
//     QVariantHash serviceProviderData = m_modelLocationServiceProviders->index(
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
    infoDialog->setWindowTitle( i18n("Service provider info") );
    infoDialog->setWindowIcon( KIcon("help-about") );

    QVariantHash serviceProviderData = m_modelLocationServiceProviders->index(
	    m_uiStop.serviceProvider->currentIndex(), 0 )
	    .data( ServiceProviderDataRole ).toHash();
    QIcon favIcon = m_uiStop.serviceProvider->itemIcon(
	    m_uiStop.serviceProvider->currentIndex() );
    m_uiAccessorInfo.icon->setPixmap( favIcon.pixmap(32) );
    m_uiAccessorInfo.serviceProviderName->setText(
	    m_uiStop.serviceProvider->currentText() );
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
	m_uiAccessorInfo.author->setText( serviceProviderData["author"].toString() );
    else {
	m_uiAccessorInfo.author->setText( QString("<a href='mailto:%2'>%1</a>")
		.arg( serviceProviderData["author"].toString() )
		.arg( serviceProviderData["email"].toString() ) );
	m_uiAccessorInfo.author->setToolTip( i18n("Write an email to %1 <%2>",
		serviceProviderData["author"].toString(),
		serviceProviderData["email"].toString()) );
    }
    m_uiAccessorInfo.description->setText( serviceProviderData["description"].toString() );
    m_uiAccessorInfo.features->setText( serviceProviderData["featuresLocalized"].toStringList().join(", ") );

    infoDialog->show();
}

void StopSettingsDialog::downloadServiceProvidersClicked(  ) {
    if ( KMessageBox::warningContinueCancel(this,
	    i18n("The downloading may currently not work as expected, sorry."))
	    == KMessageBox::Cancel )
	return;

#if KDE_VERSION >= KDE_MAKE_VERSION(4,3,80)
    KNS3::DownloadDialog *dialog = new KNS3::DownloadDialog( "publictransport.knsrc", this );
    dialog->exec();
    kDebug() << "KNS3 Results: " << dialog->changedEntries().count();

    KNS3::Entry::List installed = dialog->installedEntries();
    foreach ( const KNS3::Entry &entry, installed )
      kDebug() << entry.name() << entry.installedFiles();

//     if ( !dialog->changedEntries().isEmpty() )
// 	currentServiceProviderIndex();

    delete dialog;
#else
    KNS::Engine engine( this );
    if (engine.init("publictransport2.knsrc")) {
	KNS::Entry::List entries = engine.downloadDialogModal( this );

	kDebug() << entries.count();
	if (entries.size() > 0) {
	    foreach ( KNS::Entry *entry, entries ) {
		// Downloaded file has the name "hotstuff-access" which is wrong (maybe it works
		// better with archives). So rename the file to the right name from the payload:
		QString filename = entry->payload().representation()
		    .remove( QRegExp("^.*\\?file=") ).remove( QRegExp("&site=.*$") );
		QStringList installedFiles = entry->installedFiles();

		kDebug() << "installedFiles =" << installedFiles;
		if ( !installedFiles.isEmpty() ) {
		    QString installedFile = installedFiles[0];

		    QString path = KUrl( installedFile ).path().remove( QRegExp("/[^/]*$") ) + '/';
		    QFile( installedFile ).rename( path + filename );

		    kDebug() << "Rename" << installedFile << "to" << path + filename;
		}
	    }

	    // Get a list of with the location of each service provider (locations can be contained multiple times)
// 	    Plasma::DataEngine::Data serviceProviderData =
// 		    m_publicTransportEngine->query("ServiceProviders");
	    // TODO: Update "ServiceProviders"-data source in the data engine.
	    // TODO: Update country list (group titles in the combo box)
// 	    foreach ( QString serviceProviderName, serviceProviderData.keys() )  {
// 		QHash< QString, QVariant > curServiceProviderData = m_serviceProviderData.value(serviceProviderName).toHash();
// 		countries << curServiceProviderData["country"].toString();
// 	    }
	    updateServiceProviderModel();
	}
    }
#endif
}

void StopSettingsDialog::installServiceProviderClicked() {
    QString fileName = KFileDialog::getOpenFileName( KUrl(), "*.xml", this );
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
