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

#include "stopwidget.h"
#include "stopsettingsdialog.h"

#include <KPushButton>
#include <KDebug>

#include <QHBoxLayout>
#include <QSpacerItem>
#include <QFormLayout>
#include <QStandardItemModel>
#include "settings.h"

StopWidget::StopWidget( const StopSettings& stopSettings,
			const QStringList& filterConfigurations,
			QStandardItemModel* modelLocations,
			QStandardItemModel* modelServiceProviders,
			Plasma::DataEngine* publicTransportEngine,
			Plasma::DataEngine* osmEngine,
			Plasma::DataEngine* geolocationEngine, QWidget* parent )
			: QWidget( parent ), m_newlyAdded(stopSettings.stops.isEmpty()),
			m_stopSettings( stopSettings ),
			m_filterConfigurations(filterConfigurations),
			m_stop(0), m_provider(0), //m_filterConfiguration(0),
			m_modelLocations( modelLocations ),
			m_modelServiceProviders( modelServiceProviders ),
			m_publicTransportEngine( publicTransportEngine ),
			m_osmEngine( osmEngine ),
			m_geolocationEngine( geolocationEngine ) {
    QFormLayout *infoLayout = new QFormLayout;
    m_stop = new QLabel( this );
    m_provider = new QLabel( this );
//     m_filterConfiguration = new QLabel( this );

    m_stop->setWordWrap( true );
    m_provider->setWordWrap( true );
//     m_filterConfiguration->setWordWrap( true );
    m_stop->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );

    infoLayout->addRow( stopSettings.stops.count() > 1 ? i18n("Stops:")
                        : i18n("Stop:"), m_stop );
    infoLayout->addRow( i18n("Service Provider:"), m_provider );
//     infoLayout->addRow( i18n("Filter Configuration:"), m_filterConfiguration );

    KPushButton *change = new KPushButton( KIcon("configure"),
                                           i18n("&Change..."), this );
    connect( change, SIGNAL(clicked()), this, SLOT(changeClicked()) );

    QHBoxLayout *mainLayout = new QHBoxLayout( this );
    mainLayout->addLayout( infoLayout );
    mainLayout->addWidget( change );
}

void StopWidget::setStopSettings( const StopSettings& stopSettings ) {
    m_stop->setText( stopSettings.city.isEmpty()
                     ? stopSettings.stops.join(",\n")
                     : i18nc("Shown in the basic config page for the current stop "
                             "(%1: stop name(s), %2: city)", "%1 in %2",
                             stopSettings.stops.join(",\n"), stopSettings.city) );

    QModelIndexList indices = m_modelServiceProviders->match(
                                  m_modelServiceProviders->index(0, 0),
                                  ServiceProviderIdRole, stopSettings.serviceProviderID, 1,
                                  Qt::MatchFixedString );
    if ( indices.isEmpty() ) {
        kDebug() << "Didn't find service provider" << stopSettings.serviceProviderID;
        m_provider->setText( "-" );
    } else
        m_provider->setText( indices.first().data().toString() );

//     m_filterConfiguration->setText( SettingsUiManager::translateKey(
// 				stopSettings.filterConfiguration) );

    m_stopSettings = stopSettings;
    m_newlyAdded = false;
}

void StopWidget::addButton( QToolButton* button ) {
    QHBoxLayout *mainLayout = dynamic_cast< QHBoxLayout* >( layout() );
    mainLayout->addWidget( button );
}

void StopWidget::removeButton( QToolButton* button ) {
    QHBoxLayout *mainLayout = dynamic_cast< QHBoxLayout* >( layout() );
    mainLayout->removeWidget( button );
}

bool StopWidget::isHighlighted() const {
    return m_stop->font().bold();
}

void StopWidget::setHighlighted( bool highlighted ) {
    QFont font = m_stop->font();
    font.setBold( highlighted );
    m_stop->setFont( font );
}

void StopWidget::changeClicked() {
    StopSettingsDialog *dlg = new StopSettingsDialog( m_stopSettings,
            m_filterConfigurations, m_modelLocations, m_modelServiceProviders,
            m_publicTransportEngine, m_osmEngine, m_geolocationEngine, this );
    int result = dlg->exec();
    if ( result == KDialog::Accepted ) {
        setStopSettings( dlg->stopSettings() );
        delete dlg;
        m_newlyAdded = false;
        emit changed( m_stopSettings );
    } else {
        delete dlg;
        if ( m_newlyAdded )
            emit remove(); // Remove again if the dialog is canceled directly 
			   // after the StopWidget was added
    }
}

StopListWidget::StopListWidget( const StopSettingsList& stopSettingsList,
	    const QStringList& filterConfigurations,
	    QStandardItemModel* modelLocations,
	    QStandardItemModel* modelServiceProviders,
	    Plasma::DataEngine* publicTransportEngine, Plasma::DataEngine* osmEngine,
	    Plasma::DataEngine* geolocationEngine, QWidget* parent )
	    : AbstractDynamicWidgetContainer( RemoveButtonsBesideWidgets,
		    AddButtonAfterLastWidget, ShowSeparators, parent ),
	      m_filterConfigurations( filterConfigurations ),
	      m_modelLocations( modelLocations ),
	      m_modelServiceProviders( modelServiceProviders ),
	      m_publicTransportEngine( publicTransportEngine ),
	      m_osmEngine( osmEngine ), m_geolocationEngine( geolocationEngine ) {
    addButton()->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
    addButton()->setText( i18n("&Add Stop") );

    m_currentStopIndex = -1;
    setStopSettingsList( stopSettingsList );
}

void StopListWidget::setCurrentStopSettingIndex( int currentStopIndex ) {
    if ( m_currentStopIndex < dynamicWidgets().count() && m_currentStopIndex >= 0 ) {
	StopWidget *oldStopWidget =
		dynamicWidgets()[ m_currentStopIndex ]->contentWidget< StopWidget* >();
	oldStopWidget->setHighlighted( false );
    }
    
    if ( currentStopIndex >= dynamicWidgets().count() )
	m_currentStopIndex = dynamicWidgets().count() - 1;
    else
	m_currentStopIndex = currentStopIndex;

    if ( m_currentStopIndex != -1 ) {
	StopWidget *stopWidget =
		dynamicWidgets()[ m_currentStopIndex ]->contentWidget< StopWidget* >();
	stopWidget->setHighlighted( true );
    }
}

void StopListWidget::setFilterConfigurations( const QStringList& filterConfigurations ) {
    m_filterConfigurations = filterConfigurations;
    foreach ( StopWidget *stopWidget, widgets<StopWidget*>() ) {
	stopWidget->setFilterConfigurations( filterConfigurations );
    }
}

void StopListWidget::setStopSettingsList( const StopSettingsList& stopSettingsList ) {
    setWidgetCountRange();
    removeAllWidgets();
    
    for ( int i = 0; i < qMin(stopSettingsList.count(), 5); ++i ) {
	QWidget *widget = createNewWidget();
	StopWidget *stopWidget = qobject_cast< StopWidget* >( widget );
	stopWidget->setStopSettings( stopSettingsList[i] );
	addWidget( widget );
    }
    
    setWidgetCountRange( 1, 5 );
}

StopSettingsList StopListWidget::stopSettingsList() const {
    StopSettingsList list;
    foreach ( StopWidget *stopWidget, widgets<StopWidget*>() )
	list << stopWidget->stopSettings();
    return list;
}

void StopListWidget::changed( const StopSettings& stopSettings ) {
    StopWidget *stopWidget = qobject_cast< StopWidget* >( sender() );
    Q_ASSERT_X( stopWidget, "StopListWidget::changed", "Sender isn't a StopWidget" );
    int index = indexOf( stopWidget );
    emit changed( index, stopSettings );
}

QWidget* StopListWidget::createNewWidget() {
    StopWidget *stopWidget = new StopWidget( StopSettings(),
            m_filterConfigurations, m_modelLocations, m_modelServiceProviders,
	    m_publicTransportEngine, m_osmEngine, m_geolocationEngine, this );
    connect( stopWidget, SIGNAL(remove()), this, SLOT(removeLastWidget()) );
    connect( stopWidget, SIGNAL(changed(StopSettings)),
	     this, SLOT(changed(StopSettings)) );
    return stopWidget;
}

DynamicWidget* StopListWidget::createDynamicWidget( QWidget* contentWidget ) {
    DynamicWidget *dynamicWidget =
        AbstractDynamicWidgetContainer::createDynamicWidget( contentWidget );
    // Move the remove button into the StopWidget
    StopWidget *stopWidget = qobject_cast< StopWidget* >( contentWidget );
    stopWidget->addButton( dynamicWidget->takeRemoveButton() );
    return dynamicWidget;
}

DynamicWidget* StopListWidget::addWidget( QWidget* widget ) {
    DynamicWidget *dynamicWidget = AbstractDynamicWidgetContainer::addWidget( widget );
    StopWidget *stopWidget = qobject_cast< StopWidget* >( widget );
    if ( m_currentStopIndex == dynamicWidgets().count() - 1 )
	stopWidget->setHighlighted( true );
	
    // Open the configuration dialog when a StopWidget without settings gets added
    if ( stopWidget->stopSettings().stops.isEmpty() )
        stopWidget->changeClicked();
    return dynamicWidget;
}

int StopListWidget::removeWidget( QWidget* widget ) {
    int index = AbstractDynamicWidgetContainer::removeWidget( widget );
    if ( index == m_currentStopIndex )
	setCurrentStopSettingIndex( index );
    return index;
}
