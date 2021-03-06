/*
 *   Copyright 2012 Friedrich Pülz <fpuelz@gmx.de>
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

// Own includes
#include "publictransport_p.h"
#include "colorgroups.h"

// Plasma includes
#include <Plasma/ToolButton>
#include <Plasma/Animation>
#include <Plasma/Theme>
#include <Plasma/ToolTipManager>
#include <Plasma/ServiceJob>

// KDE includes
#include <KColorScheme>
#include <KMenu>
#include <KActionMenu>
#include <KSelectAction>
#include <KIconLoader>
#include <KGlobalSettings>

// Qt includes
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QToolButton>
#include <QPainter>
#include <QGraphicsScene>
#include <QApplication>
#include <QList>
#include <qmath.h>

ToPropertyTransition::ToPropertyTransition( QObject *sender, const char *signal, QState *source,
                                            QObject *propertyObject, const char *targetStateProperty )
        : QSignalTransition( sender, signal, source ),
        m_propertyObject( propertyObject ),
        m_property( targetStateProperty )
{
    qRegisterMetaType<QState *>( "QState*" );
}

void ToPropertyTransition::setTargetStateProperty( const QObject *propertyObject, const char *property )
{
    m_propertyObject = propertyObject;
    m_property = property;
}

PublicTransportAppletPrivate::PublicTransportAppletPrivate( PublicTransportApplet *q )
        : graphicsWidget(0), mainGraphicsWidget(0),
        oldItem(0), titleWidget(0), labelInfo(0), timetable(0), journeyTimetable(0),
        labelJourneysNotSupported(0), listStopSuggestions(0), overlay(0), model(0),
        popupIcon(0), titleToggleAnimation(0), runningUpdateRequests(0), updateTimer(0),
        modelJourneys(0), originalStopIndex(-1), filtersGroup(0), colorFiltersGroup(0),
        departureProcessor(0), departurePainter(0), stateMachine(0), journeySearchTransition1(0),
        journeySearchTransition2(0), journeySearchTransition3(0), marble(0), q_ptr( q )
{
}

bool ToPropertyTransition::eventTest( QEvent* event )
{
    if ( !QSignalTransition::eventTest(event) ) {
        return false;
    }
    setTargetState( currentTargetState() );
    return true;
}

void PublicTransportAppletPrivate::onSettingsChanged( const Settings &_settings,
                                                SettingsIO::ChangedFlags changed )
{
    Q_Q( PublicTransportApplet );

    if ( !changed.testFlag(SettingsIO::IsChanged) ) {
        kDebug() << "No changes made in the settings";
        return;
    }

    // Copy new settings
    settings = _settings;

    emit q->configNeedsSaving();
    emit q->settingsChanged();

    // First update the departure processor
    if ( changed.testFlag(SettingsIO::ChangedDepartureArrivalListType) ) {
        departureProcessor->setDepartureArrivalListType( settings.departureArrivalListType() );
    }
    if ( changed.testFlag(SettingsIO::ChangedFilterSettings) ) {
        departureProcessor->setFilters( settings.currentFilters() );
    }
    if ( changed.testFlag(SettingsIO::ChangedColorGroupSettings) ) {
        departureProcessor->setColorGroups( settings.currentColorGroups() );
    }
    if ( changed.testFlag(SettingsIO::ChangedLinesPerRow) ) {
        timetable->setMaxLineCount( settings.linesPerRow() );
//         journeyTimetable->setMaxLineCount( settings.linesPerRow ); // TEST
        model->setLinesPerRow( settings.linesPerRow() );
    }
    // Apply show departures/arrivals setting
    if ( changed.testFlag(SettingsIO::ChangedDepartureArrivalListType) ) {
        model->setDepartureArrivalListType( settings.departureArrivalListType() );

        // Update text in the departure/arrival view that gets shown when the model is empty
        onDepartureDataStateChanged();
    }

    // If stop settings have changed the whole model gets cleared and refilled.
    // Therefore the other change flags can be in 'else' parts
    const bool reloadTimetableData =
            changed.testFlag(SettingsIO::ChangedServiceProvider) ||
            changed.testFlag(SettingsIO::ChangedCurrentStopSettings) ||
            changed.testFlag(SettingsIO::ChangedCurrentStop);
    if ( reloadTimetableData ) {
        clearDepartures();

        if ( changed.testFlag(SettingsIO::ChangedCurrentStopSettings) ) {
            // Apply first departure settings to the worker thread
            const StopSettings stop = settings.currentStop();
            departureProcessor->setFirstDepartureSettings(
                    static_cast<FirstDepartureConfigMode>(stop.get<int>(
                                FirstDepartureConfigModeSetting)),
                    stop.get<QTime>(TimeOfFirstDepartureSetting),
                    stop.get<int>(TimeOffsetOfFirstDepartureSetting) );

            int alarmMinsBeforeDeparture = stop.get<int>( AlarmTimeSetting );
            model->setAlarmMinsBeforeDeparture( alarmMinsBeforeDeparture );
            modelJourneys->setAlarmMinsBeforeDeparture( alarmMinsBeforeDeparture );
        }

        updateInfoText();

        settings.adjustColorGroupSettingsCount();
        onServiceProviderSettingsChanged();
    } else if ( changed.testFlag(SettingsIO::ChangedFilterSettings) ||
                changed.testFlag(SettingsIO::ChangedColorGroupSettings) )
    {
        for( int n = 0; n < stopIndexToSourceName.count(); ++n ) {
            QString sourceName = stripDateAndTimeValues( stopIndexToSourceName[n] );
            departureProcessor->filterDepartures( sourceName,
                                                  departureInfos[sourceName], model->itemHashes() );
        }
    } else if ( changed.testFlag(SettingsIO::ChangedLinesPerRow) ) {
        // Refill model to recompute item sizehints
        model->clear();
        fillModel( mergedDepartureList() );
    }

    if ( !reloadTimetableData &&
         changed.testFlag(SettingsIO::ChangedAdditionalDataRequestSettings) )
    {
        // Request additional data for all timetable items
        if ( settings.additionalDataRequestType() == Settings::RequestAdditionalDataDirectly ) {
            foreach ( const QString &currentSource, currentSources ) {
                Plasma::Service *service =
                        q->dataEngine("publictransport")->serviceForSource( currentSource );
                if ( !service ) {
                    kWarning() << "No Timetable Service!";
                    return;
                }

                int itemBegin = 999999999;
                int itemEnd = 0;
                foreach ( const DepartureInfo departure, model->departureInfos() ) {
                    if ( !departure.includesAdditionalData() &&
                         !departure.isWaitingForAdditionalData() &&
                          departure.additionalDataError().isEmpty() )
                    {
                        const int index = departure.index();
                        itemBegin = qMin( itemBegin, index );
                        itemEnd = qMax( itemEnd, index );
                    }
                }

                if ( itemBegin < 999999999 ) {
                    KConfigGroup op = service->operationDescription("requestAdditionalDataRange");
                    op.writeEntry( "itemnumberbegin", itemBegin );
                    op.writeEntry( "itemnumberend", itemEnd );
                    Plasma::ServiceJob *additionDataJob = service->startOperationCall( op );
                    q->connect( additionDataJob, SIGNAL(finished(KJob*)), service, SLOT(deleteLater()) );
                }
            }
        }
    }

    if ( changed.testFlag(SettingsIO::ChangedCurrentJourneySearchLists) ||
         changed.testFlag(SettingsIO::ChangedCurrentStop) )
    {
        // Update the journeys menu
        updateJourneyMenu();
    }
    if ( changed.testFlag(SettingsIO::ChangedDepartureArrivalListType) ) {
        onDepartureArrivalListTypeChanged();
    }

    // Update current stop settings / current home stop in the models
    if ( changed.testFlag(SettingsIO::ChangedCurrentStop) ||
         changed.testFlag(SettingsIO::ChangedCurrentStopSettings) )
    {
        onCurrentStopSettingsChanged();
    }

    // Update the filter widget
    if ( changed.testFlag(SettingsIO::ChangedCurrentFilterSettings) ||
         changed.testFlag(SettingsIO::ChangedColorGroupSettings) )
    {
        // Update the filter menu, if filter or color group settings have changed.
        // If the current stop or it's settings have changed, the active filters
        // and color groups may also have changed, requiring an update of the filter menu
        updateFilterMenu();
        titleWidget->updateFilterWidget();
    }

    // Update alarm settings
    if ( changed.testFlag(SettingsIO::ChangedAlarmSettings) ) {
        model->setAlarmSettings( settings.alarms() );
        if ( modelJourneys ) {
            modelJourneys->setAlarmSettings( settings.alarms() );
        }
        departureProcessor->setAlarms( settings.alarms() );
    }

    // Apply font / size factor
    if ( changed.testFlag(SettingsIO::ChangedFont) ||
         changed.testFlag(SettingsIO::ChangedSizeFactor) )
    {
        // Get fonts
        QFont font = settings.sizedFont();
        int smallPointSize = KGlobalSettings::smallestReadableFont().pointSize() * settings.sizeFactor();
        QFont smallFont = font;
        smallFont.setPointSize( smallPointSize > 0 ? smallPointSize : 1 );

        // Apply fonts
        labelInfo->setFont( smallFont );
        timetable->setFont( font );
        if ( journeyTimetable && isStateActive("journeyView") ) {
            journeyTimetable->setFont( font );
        }
    }

    // Apply size factor settings
    if ( changed.testFlag(SettingsIO::ChangedSizeFactor) ) {
        model->setSizeFactor( settings.sizeFactor() ); // Used for sizes of icons returned by the model
        timetable->setZoomFactor( settings.sizeFactor() );
        if ( journeyTimetable && isStateActive("journeyView") ) {
            journeyTimetable->setZoomFactor( settings.sizeFactor() );
        }
    }

    // Apply shadow settings
    if ( changed.testFlag(SettingsIO::ChangedShadows) ) {
        timetable->setOption( PublicTransportWidget::DrawShadowsOrHalos, settings.drawShadows() );
        if ( journeyTimetable && isStateActive("journeyView") ) {
            journeyTimetable->setOption( PublicTransportWidget::DrawShadowsOrHalos,
                                         settings.drawShadows() );
        }
    }

    // Update title widget to settings
    if ( changed.testFlag(SettingsIO::ChangedCurrentStopSettings) ||
         changed.testFlag(SettingsIO::ChangedFont) ||
         changed.testFlag(SettingsIO::ChangedSizeFactor) )
    {
        titleWidget->settingsChanged();
    }

    // Apply target column settings
    if ( changed.testFlag(SettingsIO::ChangedTargetColumn) &&
         stateMachine && isStateActive("departureView") )
    {
        timetable->setTargetHidden( settings.hideTargetColumn() );
        timetable->updateItemLayouts();
    }

    // Limit model item count to the maximal number of departures setting
    if ( model->rowCount() > settings.maximalNumberOfDepartures() ) {
        model->removeRows( settings.maximalNumberOfDepartures(),
                           model->rowCount() - settings.maximalNumberOfDepartures() );
    }

    if ( changed.testFlag(SettingsIO::ChangedDepartureTimeSettings) ) {
        model->setDepartureColumnSettings( settings.departureTimeFlags() );
    }
}

void PublicTransportAppletPrivate::onDepartureArrivalListTypeChanged()
{
    Q_Q( PublicTransportApplet );

    model->setDepartureArrivalListType( settings.departureArrivalListType() );
    timetable->updateItemLayouts();

    // Adjust action texts to departure / arrival list
    q->action( "removeAlarmForDeparture" )->setText(
        settings.departureArrivalListType() == DepartureList
        ? i18nc( "@action", "Remove &Alarm for This Departure" )
        : i18nc( "@action", "Remove &Alarm for This Arrival" ) );
    q->action( "createAlarmForDeparture" )->setText(
        settings.departureArrivalListType() == DepartureList
        ? i18nc( "@action", "Set &Alarm for This Departure" )
        : i18nc( "@action", "Set &Alarm for This Arrival" ) );
    q->action( "backToDepartures" )->setText(
        settings.departureArrivalListType() == DepartureList
        ? i18nc( "@action", "Back to &Departure List" )
        : i18nc( "@action", "Back to &Arrival List" ) );
}

void PublicTransportAppletPrivate::onCurrentStopSettingsChanged()
{
    model->setHomeStop( settings.currentStop().stop(0).name );
    model->setCurrentStopIndex( settings.currentStopIndex() );

    if ( modelJourneys ) {
        modelJourneys->setHomeStop( settings.currentStop().stop(0).name );
        modelJourneys->setCurrentStopIndex( settings.currentStopIndex() );
    }
}

void PublicTransportAppletPrivate::onServiceProviderSettingsChanged()
{
    Q_Q( PublicTransportApplet );

    if ( settings.checkConfig() ) {
        // Configuration is valid
        q->setConfigurationRequired( false );

        // Connect to the "ServiceProvider [providerId]" source if not done already
        // to get provider data and to get notified on changes
        const QString previousProviderId = currentProviderData["id"].toString();
        const QString providerId = settings.currentStop().get< QString >( ServiceProviderSetting );
        if ( providerId != previousProviderId ) {
            // First disable everything that depends on provider features,
            // enable it when the provider data arrives and the required features are enabled
            q->action( "journeys" )->setEnabled( false );
            titleWidget->setJourneysSupported( false );

            // Clear old provider data
            currentProviderData.clear();
            currentServiceProviderFeatures.clear();

            // Disconnect previous provider data source if any and connect new one
            Plasma::DataEngine *engine = q->dataEngine( "publictransport" );
            if ( !previousProviderId.isEmpty() ) {
                engine->disconnectSource( "ServiceProvider " + previousProviderId, q );
            }
            engine->connectSource( "ServiceProvider " + providerId, q );
        } else {
            // Call providerDataUpdated() manually to enable provider feature dependend actions,
            // the data source is already connected
            providerDataUpdated( currentProviderData );
        }

        // Reconnect with new settings
        reconnectSource();
        if ( !currentJourneySource.isEmpty() ) {
            reconnectJourneySource();
        }
    } else {
        clearDepartures();

        // Missing configuration, eg. no home stop
        q->setConfigurationRequired( true, i18nc( "@info/plain", "Please check your configuration." ) );

        q->action( "journeys" )->setEnabled( false );
        titleWidget->setJourneysSupported( false );
    }
}

void PublicTransportAppletPrivate::providerDataUpdated( const QVariantHash &data )
{
    Q_Q( PublicTransportApplet );
    currentProviderData = data;
    currentServiceProviderFeatures = data["features"].toStringList();
    model->setProviderFeatures( currentServiceProviderFeatures );
    if ( modelJourneys ) {
        modelJourneys->setProviderFeatures( currentServiceProviderFeatures );
    }

    // Only use the default target state (journey search) if journeys
    // are supported by the used service provider. Otherwise go to the
    // alternative target state (journeys not supported).
    const bool journeysSupported = currentServiceProviderFeatures.contains( "ProvidesJourneys" );
    QAbstractState *target = journeysSupported
                                ? states["journeySearch"] : states["journeysUnsupportedView"];
    journeySearchTransition1->setTargetState( target );
    journeySearchTransition2->setTargetState( target );
    journeySearchTransition3->setTargetState( target );

    q->action( "journeys" )->setEnabled( journeysSupported );
    titleWidget->setJourneysSupported( journeysSupported );

    // Check provider state
    const QString state = data[ "state" ].toString();
    if ( state == QLatin1String("ready") ) {
        // Provider is ready to use
        const bool hadProviderError = isStateActive("providerError");
        q->emit providerReady();

        if ( hadProviderError ) {
            reconnectSource();
        }
    } else {
        // provider is not ready, eg. needs to import a GTFS feed first
        q->emit providerNotReady();
        clearDepartures();
        clearJourneys();
    }

    // Check if arrivals are currently shown but not supported by the new provider
    if ( !currentServiceProviderFeatures.contains("ProvidesArrivals", Qt::CaseInsensitive) &&
         settings.departureArrivalListType() == ArrivalList )
    {
        Settings newSettings = settings;
        newSettings.setDepartureArrivalListType( DepartureList );
        q->setSettings( newSettings );
    }

    // Show error message in the departure/arrival view, if any
    onDepartureDataStateChanged();
}

void PublicTransportAppletPrivate::onResized()
{
    Q_Q( PublicTransportApplet );

    // Get the size of the applet/popup (not the size of the popup icon if iconified)
    QSizeF size = graphicsWidget->size();

    if ( titleWidget ) {
        q->updatePopupIcon();

        // Show/hide title widget
        const qreal minHeightWithTitle = 200.0;
        const qreal maxHeightWithoutTitle = 225.0;
        if ( size.height() <= minHeightWithTitle  // too small?
                && ( ( !titleToggleAnimation // title not already hidden?
                       && titleWidget->maximumHeight() > 0.1 )
                     || ( titleToggleAnimation // title not currently animated to be hidden?
                          && titleToggleAnimation->direction() != QAbstractAnimation::Forward ) ) ) {
            // Hide title: The applets vertical size is too small to show it
            //             and the title is not already hidden or currently being faded out
            if ( titleToggleAnimation ) {
                delete titleToggleAnimation;
            }

            // Create toggle animation with direction forward
            // to indicate that the title gets hidden
            titleToggleAnimation = new QParallelAnimationGroup( q );
            titleToggleAnimation->setDirection( QAbstractAnimation::Forward );

            Plasma::Animation *fadeAnimation = Plasma::Animator::create(
                                                   Plasma::Animator::FadeAnimation, titleToggleAnimation );
            fadeAnimation->setTargetWidget( titleWidget );
            fadeAnimation->setProperty( "startOpacity", titleWidget->opacity() );
            fadeAnimation->setProperty( "targetOpacity", 0.0 );

            QPropertyAnimation *shrinkAnimation = new QPropertyAnimation(
                titleWidget, "maximumSize", titleToggleAnimation );
            shrinkAnimation->setStartValue( QSizeF( titleWidget->maximumWidth(),
                                                    titleWidget->layout()->preferredHeight() ) );
            shrinkAnimation->setEndValue( QSizeF( titleWidget->maximumWidth(), 0 ) );

            q->connect( titleToggleAnimation, SIGNAL(finished()),
                        q, SLOT(titleToggleAnimationFinished()) );
            titleToggleAnimation->addAnimation( fadeAnimation );
            titleToggleAnimation->addAnimation( shrinkAnimation );
            titleToggleAnimation->start();
        } else if ( size.height() >= maxHeightWithoutTitle  // big enough?
                   && ( ( !titleToggleAnimation // title not already shown?
                          && titleWidget->maximumHeight() < titleWidget->layout()->preferredHeight() )
                        || ( titleToggleAnimation // title not currently animated to be shown?
                             && titleToggleAnimation->direction() != QAbstractAnimation::Backward ) ) ) {
            // Show title: The applets vertical size is big enough to show it
            //             and the title is not already shown or currently beging faded in
            if ( titleToggleAnimation ) {
                delete titleToggleAnimation;
            }

            // Create toggle animation with direction backward
            // to indicate that the title gets shown again.
            // The child animations use reversed start/end values.
            titleToggleAnimation = new QParallelAnimationGroup( q );
            titleToggleAnimation->setDirection( QAbstractAnimation::Backward );

            Plasma::Animation *fadeAnimation = Plasma::Animator::create(
                                                   Plasma::Animator::FadeAnimation, titleToggleAnimation );
            fadeAnimation->setTargetWidget( titleWidget );
            fadeAnimation->setProperty( "targetOpacity", titleWidget->opacity() );
            fadeAnimation->setProperty( "startOpacity", 1.0 );

            QPropertyAnimation *growAnimation = new QPropertyAnimation(
                titleWidget, "maximumSize", titleToggleAnimation );
            growAnimation->setEndValue( QSizeF( titleWidget->maximumWidth(),
                                                titleWidget->maximumHeight() ) );
            growAnimation->setStartValue( QSizeF( titleWidget->maximumWidth(),
                                                  titleWidget->layout()->preferredHeight() ) );

            q->connect( titleToggleAnimation, SIGNAL(finished()),
                        q, SLOT(titleToggleAnimationFinished()) );
            titleToggleAnimation->addAnimation( fadeAnimation );
            titleToggleAnimation->addAnimation( growAnimation );
            titleToggleAnimation->start();
        }

        // Show/hide vertical scrollbar
        const qreal minWidthWithScrollBar = 250.0;
        const qreal maxWidthWithoutScrollBar = 275.0;
        if ( size.width() <= minWidthWithScrollBar ) {
            timetable->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
        } else if ( size.width() >= maxWidthWithoutScrollBar ) {
            timetable->setVerticalScrollBarPolicy( Qt::ScrollBarAsNeeded );
        }

        // Update quick journey search widget (show icon or icon with text)
        Plasma::ToolButton *quickJourneySearchWidget =
            titleWidget->castedWidget<Plasma::ToolButton>( TitleWidget::WidgetQuickJourneySearch );
        Plasma::ToolButton *filterWidget =
            titleWidget->castedWidget<Plasma::ToolButton>( TitleWidget::WidgetFilter );
        if ( quickJourneySearchWidget ) {
            if ( titleWidget->layout()->preferredWidth() > size.width() ) {
                // Show only an icon on the quick journey search toolbutton,
                // if there is not enough horizontal space
                quickJourneySearchWidget->nativeWidget()->setToolButtonStyle( Qt::ToolButtonIconOnly );
                quickJourneySearchWidget->setMaximumWidth( quickJourneySearchWidget->size().height() );
            } else if ( quickJourneySearchWidget->nativeWidget()->toolButtonStyle() == Qt::ToolButtonIconOnly
                       && size.width() > titleWidget->layout()->minimumWidth() +
                       QFontMetrics( quickJourneySearchWidget->font() ).width( quickJourneySearchWidget->text() ) +
                       ( filterWidget->nativeWidget()->toolButtonStyle() == Qt::ToolButtonIconOnly
                         ? QFontMetrics( filterWidget->font() ).width( filterWidget->text() ) : 0 ) + 60 ) {
                // Show the icon with text beside if there is enough horizontal space again
                quickJourneySearchWidget->nativeWidget()->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
                quickJourneySearchWidget->setMaximumWidth( -1 );
            }
        }

        // Update filter widget (show icon or icon with text)
        if ( filterWidget ) {
            if ( titleWidget->layout()->preferredWidth() > size.width() ) {
                // Show only an icon on the filter toolbutton,
                // if there is not enough horizontal space
                filterWidget->nativeWidget()->setToolButtonStyle( Qt::ToolButtonIconOnly );
                filterWidget->setMaximumWidth( filterWidget->size().height() );
            } else if ( filterWidget->nativeWidget()->toolButtonStyle() == Qt::ToolButtonIconOnly
                       && size.width() > titleWidget->layout()->minimumWidth() +
                       QFontMetrics( filterWidget->font() ).width( filterWidget->text() ) + 60 ) {
                // Show the icon with text beside if there is enough horizontal space again
                filterWidget->nativeWidget()->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
                filterWidget->setMaximumWidth( -1 );
            }
        }
    }

    // Update line breaking of the courtesy label
    updateInfoText();
}

void PublicTransportAppletPrivate::onOldItemAnimationFinished()
{
    if ( oldItem && oldItem->scene() ) {
        oldItem->scene()->removeItem( oldItem );
    }
    delete oldItem;
    oldItem = 0;
}

void PublicTransportAppletPrivate::updateInfoText()
{
    labelInfo->setText( infoText() );
    labelInfo->setToolTip( infoTooltip() );
}

void PublicTransportAppletPrivate::applyTheme()
{
    Q_Q( PublicTransportApplet );
    // Get theme colors
    QColor textColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor );

    // Create palette with the used theme colors
    QPalette p = q->palette();
    p.setColor( QPalette::Background, Qt::transparent );
    p.setColor( QPalette::Base, Qt::transparent );
    p.setColor( QPalette::Button, Qt::transparent );
    p.setColor( QPalette::Foreground, textColor );
    p.setColor( QPalette::Text, textColor );
    p.setColor( QPalette::ButtonText, textColor );

    QColor bgColor = KColorScheme( QPalette::Active )
                     .background( KColorScheme::AlternateBackground ).color();
    bgColor.setAlpha( bgColor.alpha() / 3 );
    QLinearGradient bgGradient( 0, 0, 1, 0 );
    bgGradient.setCoordinateMode( QGradient::ObjectBoundingMode );
    bgGradient.setColorAt( 0, Qt::transparent );
    bgGradient.setColorAt( 0.3, bgColor );
    bgGradient.setColorAt( 0.7, bgColor );
    bgGradient.setColorAt( 1, Qt::transparent );
    QBrush brush( bgGradient );
    p.setBrush( QPalette::AlternateBase, brush );

    timetable->setPalette( p );

    // To set new text color of the header items
    model->setDepartureArrivalListType( settings.departureArrivalListType() );
    timetable->updateItemLayouts();
}

void PublicTransportAppletPrivate::createTooltip()
{
    Q_Q( PublicTransportApplet );

    if ( q->formFactor() != Plasma::Horizontal && q->formFactor() != Plasma::Vertical ) {
        // Create the tooltip only when in a panel
        Plasma::ToolTipManager::self()->clearContent( q );
        return;
    }

    Plasma::ToolTipContent data;
    data.setMainText( i18nc( "@info", "Public Transport" ) );
    if ( popupIcon->departureGroups()->isEmpty() ) {
        data.setSubText( i18nc( "@info", "View departure times for public transport" ) );
    } else if ( !popupIcon->departureGroups()->isEmpty() ) {
        const DepartureGroup currentGroup = popupIcon->currentDepartureGroup();
        if ( currentGroup.isEmpty() ) {
            kDebug() << "Empty group for popup icon!";
            return;
        }
        const bool isAlarmGroup = popupIcon->currentGroupIsAlarmGroup();
        const QString groupDurationString = currentGroup.first()->departureInfo()->durationString();
        QStringList infoStrings;

        if ( settings.departureArrivalListType() == DepartureList ) {
            // Showing a departure list
            foreach( const DepartureItem * item, currentGroup ) {
                infoStrings << i18nc( "@info Text for one departure for the tooltip (%1: line string, "
                                      "%2: target)",
                                      "Line <emphasis strong='1'>%1</emphasis> "
                                      "to <emphasis strong='1'>%2</emphasis>",
                                      item->departureInfo()->lineString(),
                                      item->departureInfo()->target() );
            }
            if ( isAlarmGroup ) {
                data.setSubText( i18ncp( "@info %2 is the translated duration text (e.g. in 3 minutes), "
                                         "%4 contains texts for a list of departures",
                                         "Alarm (%2) for a departure from '%3':<nl/>%4",
                                         "%1 Alarms (%2) for departures from '%3':<nl/>%4",
                                         currentGroup.count(),
                                         groupDurationString, settings.currentStop().stops().join( ", " ),
                                         infoStrings.join( ",<nl/>" ) ) );
            } else {
                data.setSubText( i18ncp( "@info %2 is the translated duration text (e.g. in 3 minutes), "
                                         "%4 contains texts for a list of departures",
                                         "Departure (%2) from '%3':<nl/>%4",
                                         "%1 Departures (%2) from '%3':<nl/>%4",
                                         currentGroup.count(),
                                         groupDurationString, settings.currentStop().stops().join( ", " ),
                                         infoStrings.join( ",<nl/>" ) ) );
            }
        } else {
            // Showing an arrival list
            foreach( const DepartureItem * item, currentGroup ) {
                infoStrings << i18nc( "@info Text for one arrival for the tooltip (%1: line string, "
                                      "%2: origin)",
                                      "Line <emphasis strong='1'>%1<emphasis> "
                                      "from <emphasis strong='1'>%2<emphasis>",
                                      item->departureInfo()->lineString(),
                                      item->departureInfo()->target() );
            }
            if ( isAlarmGroup ) {
                data.setSubText( i18ncp( "@info %2 is the translated duration text (e.g. in 3 minutes), "
                                         "%4 contains texts for a list of arrivals",
                                         "Alarm (%2) for an arrival at '%3':<nl/>%4",
                                         "%1 Alarms (%2) for arrivals at '%3':<nl/>%4",
                                         currentGroup.count(),
                                         groupDurationString, settings.currentStop().stops().join( ", " ),
                                         infoStrings.join( ",<nl/>" ) ) );
            } else {
                data.setSubText( i18ncp( "@info %2 is the translated duration text (e.g. in 3 minutes), "
                                         "%4 contains texts for a list of arrivals",
                                         "Arrival (%2) at '%3':<nl/>%4",
                                         "%1 Arrivals (%2) at '%3':<nl/>%4",
                                         currentGroup.count(),
                                         groupDurationString, settings.currentStop().stops().join( ", " ),
                                         infoStrings.join( ",<nl/>" ) ) );
            }
        }
    }

    data.setImage( KIcon( "public-transport-stop" ).pixmap( IconSize( KIconLoader::Desktop ) ) );
    Plasma::ToolTipManager::self()->setContent( q, data );
}

QString PublicTransportAppletPrivate::stripDateAndTimeValues( const QString &sourceName ) const
{
    QString ret = sourceName;
    QRegExp rx( "(time=[^\\|]*|datetime=[^\\|]*)", Qt::CaseInsensitive );
    rx.setMinimal( true );
    ret.replace( rx, QChar() );
    return ret;
}

void PublicTransportAppletPrivate::fillModel( const QList<DepartureInfo> &departures )
{
    bool modelFilled = model->rowCount() >= settings.maximalNumberOfDepartures();
    foreach( const DepartureInfo &departureInfo, departures ) {
        QModelIndex index = model->indexFromInfo( departureInfo );
        if ( !index.isValid() ) {
            // Departure wasn't in the model
            if ( !modelFilled && !departureInfo.isFilteredOut() ) {
                // Departure doesn't get filtered out and the model isn't full => Add departure
                model->addItem( departureInfo );
                modelFilled = model->rowCount() >= settings.maximalNumberOfDepartures();
            }
        } else if ( departureInfo.isFilteredOut() ) {
            // Departure has been marked as "filtered out" in the DepartureProcessor => Remove departure
            model->removeItem( model->itemFromInfo( departureInfo ) );
        } else {
            // Departure isn't filtered out => Update associated item in the model
            DepartureItem *item = dynamic_cast< DepartureItem* >( model->itemFromIndex(index) );
            model->updateItem( item, departureInfo );
        }
    }

    // Sort departures in the model.
    // They are most probably already sorted, but sometimes they are not
    model->sort( ColumnDeparture );
}

void PublicTransportAppletPrivate::fillModelJourney( const QList<JourneyInfo> &journeys )
{
    foreach( const JourneyInfo & journeyInfo, journeys ) {
        int row = modelJourneys->indexFromInfo( journeyInfo ).row();
        if ( row == -1 ) {
            // Journey wasn't in the model
            modelJourneys->addItem( journeyInfo );
        } else {
            // Update associated item in the model
            JourneyItem *item = static_cast<JourneyItem *>( modelJourneys->itemFromInfo( journeyInfo ) );
            modelJourneys->updateItem( item, journeyInfo );
        }
    }

    // Sort departures in the model.
    // They are most probably already sorted, but sometimes they are not
    modelJourneys->sort( ColumnDeparture );
}

void PublicTransportAppletPrivate::updateFilterMenu()
{
    Q_Q( PublicTransportApplet );

    KActionMenu *actionFilter = qobject_cast< KActionMenu * >( q->action( "filterConfiguration" ) );
    KMenu *menu = actionFilter->menu();
    menu->clear();

    QList< QAction * > oldActions = filtersGroup->actions();
    foreach( QAction * oldAction, oldActions ) {
        filtersGroup->removeAction( oldAction );
        delete oldAction;
    }

    bool showColorGrous = settings.colorize() && !settings.colorGroups().isEmpty();
    if ( settings.filters().isEmpty() && !showColorGrous ) {
        return; // Nothing to show in the filter menu
    }

    if ( !settings.filters().isEmpty() ) {
        menu->addTitle( KIcon( "view-filter" ), i18nc( "@title This is a menu title",
                        "Filters (reducing)" ) );
        foreach( const FilterSettings &filters, settings.filters() ) {
            QAction *action = new QAction( filters.name, filtersGroup );
            action->setCheckable( true );
            if ( filters.affectedStops.contains( settings.currentStopIndex() ) ) {
                action->setChecked( true );
            }

            menu->addAction( action );
        }
    }

    if ( showColorGrous ) {
        // Add checkbox entries to toggle color groups
        if ( settings.departureArrivalListType() == ArrivalList ) {
            menu->addTitle( KIcon( "object-group" ), i18nc( "@title This is a menu title",
                            "Arrival Groups (extending)" ) );
        } else {
            menu->addTitle( KIcon( "object-group" ), i18nc( "@title This is a menu title",
                            "Departure Groups (extending)" ) );
        }
        foreach( const ColorGroupSettings & colorGroup,
                 settings.currentColorGroups() ) {
            // Create action for current color group
            QAction *action = new QAction( colorGroup.displayText, colorFiltersGroup );
            action->setCheckable( true );
            if ( !colorGroup.filterOut ) {
                action->setChecked( true );
            }
            action->setData( QVariant::fromValue( colorGroup.color ) );

            // Draw a color patch with the color of the color group
            QPixmap pixmap( QSize( 16, 16 ) );
            pixmap.fill( Qt::transparent );
            QPainter p( &pixmap );
            p.setRenderHints( QPainter::Antialiasing );
            p.setBrush( colorGroup.color );
            QColor borderColor = KColorScheme( QPalette::Active ).foreground().color();
            borderColor.setAlphaF( 0.75 );
            p.setPen( borderColor );
            p.drawRoundedRect( QRect( QPoint( 1, 1 ), pixmap.size() - QSize( 2, 2 ) ), 4, 4 );
            p.end();

            // Put the pixmap into a KIcon
            KIcon colorIcon;
            colorIcon.addPixmap( pixmap );
            action->setIcon( colorIcon );

            menu->addAction( action );
        }
    }
}

void PublicTransportAppletPrivate::updateJourneyMenu()
{
    Q_Q( PublicTransportApplet );

    KActionMenu *journeysAction = qobject_cast<KActionMenu *>( q->action( "journeys" ) );
    KMenu *menu = journeysAction->menu();
    menu->clear();

    // Add action to go to journey search view
    // Do not add a separator after it, because a menu title item follows
    menu->addAction( q->action( "searchJourneys" ) );

    // Extract lists of journey search strings / names
    QStringList favoriteJourneySearchNames;
    QStringList favoriteJourneySearches;
    QStringList recentJourneySearchNames;
    QStringList recentJourneySearches;
    foreach( const JourneySearchItem & item, settings.currentJourneySearches() ) {
        if ( item.isFavorite() ) {
            favoriteJourneySearches << item.journeySearch();
            favoriteJourneySearchNames << item.nameOrJourneySearch();
        } else {
            recentJourneySearches << item.journeySearch();
            recentJourneySearchNames << item.nameOrJourneySearch();
        }
    }

    // Add favorite journey searches
    if ( !favoriteJourneySearches.isEmpty() ) {
        menu->addTitle( KIcon( "favorites" ),
                        i18nc( "@title Title item in quick journey search menu",
                               "Favorite Journey Searches" ) );
        QList< QAction * > actions;
        KIcon icon( "edit-find", 0, QStringList() << "favorites" );
        for( int i = 0; i < favoriteJourneySearches.count(); ++i ) {
            KAction *action = new KAction( icon, favoriteJourneySearchNames[i], menu );
            action->setData( favoriteJourneySearches[i] );
            actions << action;
        }
        menu->addActions( actions );
    }

    // Add recent journey searches
    if ( !recentJourneySearches.isEmpty() ) {
        menu->addTitle( KIcon( "document-open-recent" ),
                        i18nc( "@title Title item in quick journey search menu",
                               "Recent Journey Searches" ) );
        QList< QAction * > actions;
        KIcon icon( "edit-find" );
        for( int i = 0; i < recentJourneySearches.count(); ++i ) {
            KAction *action = new KAction( icon, recentJourneySearchNames[i], menu );
            action->setData( recentJourneySearches[i] );
            actions << action;
        }
        menu->addActions( actions );
    }

    // Add a separator before the configure action
    menu->addSeparator();

    // Add the configure action, which is distinguishable from others by having no data
    menu->addAction( q->action( "configureJourneys" ) );
}

QList<DepartureInfo> PublicTransportAppletPrivate::mergedDepartureList( bool includeFiltered,
                                                                  int max ) const
{
    QList< DepartureInfo > ret;

    for( int n = stopIndexToSourceName.count() - 1; n >= 0; --n ) {
        QString sourceName = stripDateAndTimeValues( stopIndexToSourceName[n] );
        if ( departureInfos.contains( sourceName ) ) {
            foreach( const DepartureInfo & departureInfo, departureInfos[sourceName] ) {
                // Only add not filtered items
                if ( !departureInfo.isFilteredOut() || includeFiltered ) {
                    ret << departureInfo;
                }
            }
        }
    }

    qSort( ret.begin(), ret.end() );
    return max == -1 ? ret.mid( 0, settings.maximalNumberOfDepartures() ) : ret.mid( 0, max );
}

void PublicTransportAppletPrivate::reconnectSource()
{
    Q_Q( PublicTransportApplet );

    // Get current stop data
    StopSettings stopSettings = settings.currentStop();
    if ( stopSettings.stopList().isEmpty() ) {
        // Currently no stops configured
        return;
    }

    // Build source names for each (combined) stop for the publictransport data engine
    const QString providerId = stopSettings.get< QString >( ServiceProviderSetting );
    const QString city = stopSettings.get< QString >( CitySetting );
    const FirstDepartureConfigMode firstDepartureMode = static_cast< FirstDepartureConfigMode >(
                stopSettings.get<int>(FirstDepartureConfigModeSetting) );
    QStringList sources;
    stopIndexToSourceName.clear();
    const StopList stopList = stopSettings.stopList();
    for ( int i = 0; i < stopList.count(); ++i ) {
        QString currentSource = QString( "%3 %1|count=%2" )
                .arg( providerId ).arg( settings.maximalNumberOfDepartures() )
                .arg( settings.departureArrivalListType() == ArrivalList
                        ? "Arrivals" : "Departures" );

        const Stop &stop = stopList[i];
        if ( stop.id.isEmpty() ) {
            currentSource += "|stop=" + stop.name;
        } else {
            currentSource += "|stopid=" + stop.id;
        }

        switch ( firstDepartureMode ) {
        case RelativeToCurrentTime:
            currentSource += "|timeoffset=" +
                    stopSettings.get<int>(TimeOffsetOfFirstDepartureSetting);
            break;
        case AtCustomTime:
            currentSource += "|time=" +
                    stopSettings.get<QTime>(TimeOfFirstDepartureSetting).toString("hh:mm");
            break;
        default:
            kWarning() << "Unknown FirstDepartureConfigMode" << firstDepartureMode;
            break;
        }

        if ( !city.isEmpty() ) {
            currentSource += "|city=" + city;
        }

        stopIndexToSourceName[ i ] = currentSource;
        sources << currentSource;
    }

    if ( sources == currentSources ) {
        // Sources did not change
        return;
    }

    QStringList previousSources = currentSources;
    currentSources.clear();

    // Notify that new departure/arrival data gets requested now
    emit q->requestedNewDepartureData();

    // Connect all data sources, normally this is only one source for departures/arrivals
    // from one stop, but departures/arrivals from multiple stops can be displayed combined
    // in the applet, to do so for each stop one data source gets connected
    foreach( const QString &currentSource, sources ) {
        currentSources << currentSource;

        // Do not connect with a polling interval, because this would cause updates to be received
        // later, ie. when the interval has finished. Instead let the data engine push new data
        // when available and let it decide itself when to update timetable data for connected
        // sources. Manual updates are possible through the timetable service.
        if ( !previousSources.removeOne(currentSource) ) {
            // Source is not connected
            kDebug() << "Connect data source" << currentSource;
            q->dataEngine( "publictransport" )->connectSource( currentSource, q );
        }
    }

    // Disconnect no longer used sources
    foreach( const QString &previousSource, previousSources ) {
        kDebug() << "Disconnect data source" << previousSource;
        q->dataEngine( "publictransport" )->disconnectSource( previousSource, q );
    }
}

void PublicTransportAppletPrivate::disconnectSources()
{
    Q_Q( PublicTransportApplet );
    if ( !currentSources.isEmpty() ) {
        foreach( const QString &currentSource, currentSources ) {
            kDebug() << "Disconnect data source" << currentSource;
            q->dataEngine( "publictransport" )->disconnectSource( currentSource, q );
        }
        currentSources.clear();
    }
}

void PublicTransportAppletPrivate::disconnectJourneySource()
{
    Q_Q( PublicTransportApplet );
    if ( !currentJourneySource.isEmpty() ) {
        kDebug() << "Disconnect journey data source" << currentJourneySource;
        q->dataEngine( "publictransport" )->disconnectSource( currentJourneySource, q );
    }
}

bool PublicTransportAppletPrivate::isStateActive( const QString &stateName ) const
{
    return states.contains( stateName )
           && stateMachine->configuration().contains( states[stateName] );
}

void PublicTransportAppletPrivate::reconnectJourneySource( const QString &targetStopName,
        const QDateTime &dateTime, bool stopIsTarget, bool timeIsDeparture,
        bool requestStopSuggestions )
{
    Q_Q( PublicTransportApplet );

    disconnectJourneySource();

    QString _targetStopName = targetStopName;
    QDateTime _dateTime = dateTime;
    if ( _targetStopName.isEmpty() ) {
        if ( lastSecondStopName.isEmpty() ) {
            return;
        }
        _targetStopName = lastSecondStopName;
    }
    if ( !_dateTime.isValid() ) {
        _dateTime = lastJourneyDateTime;
    }

    // Build a source name for the publictransport data engine
    if ( requestStopSuggestions ) {
        currentJourneySource = QString( "Stops %1|stop=%2" )
                               .arg( settings.currentStop().get<QString>(ServiceProviderSetting) )
                               .arg( _targetStopName );
    } else {
        // Get current stop data
        StopSettings stopSettings = settings.currentStop();
        if ( stopSettings.stopList().isEmpty() ) {
            // Currently no stops configured
            return;
        }

        currentJourneySource = QString( stopIsTarget ? "%4 %1|targetstop=%2|datetime=%3"
                                                     : "%4 %1|originstop=%2|datetime=%3" )
                               .arg( settings.currentStop().get<QString>(ServiceProviderSetting) )
                               .arg( _targetStopName )
                               .arg( _dateTime.toString(Qt::ISODate) )
                               .arg( timeIsDeparture ? "Journeys" : "JourneysArr" );

        const Stop stop = stopSettings.stop( 0 );
        if ( stop.id.isEmpty() ) {
            currentJourneySource += (stopIsTarget ? "|originstop=" : "|targetstop=") + stop.name;
        } else {
            currentJourneySource += (stopIsTarget ? "|originstopid=" : "|targetstopid=") + stop.id;
        }

        QString currentStop = settings.currentStop().stops().first();
        journeyTitleText = stopIsTarget
                           ? i18nc( "@info", "From %1<nl/>to <emphasis strong='1'>%2</emphasis>",
                                    currentStop, _targetStopName )
                           : i18nc( "@info", "From <emphasis strong='1'>%1</emphasis><nl/>to %2",
                                    _targetStopName, currentStop );
        if ( isStateActive( "journeyView" ) ) {
            titleWidget->setTitle( journeyTitleText );
        }
    }

    if ( !settings.currentStop().get<QString>(CitySetting).isEmpty() ) {
        currentJourneySource += QString( "|city=%1" ).arg(
                                    settings.currentStop().get<QString>(CitySetting) );
    }

    lastSecondStopName = _targetStopName;
    emit q->requestedNewJourneyData();
    q->dataEngine( "publictransport" )->connectSource( currentJourneySource, q );
}

void PublicTransportAppletPrivate::updateColorGroupSettings()
{
    Q_Q( PublicTransportApplet );
    if ( settings.colorize() ) {
        // Generate color groups from existing departure data
        settings.adjustColorGroupSettingsCount();
        ColorGroupSettingsList colorGroups = settings.currentColorGroups();
        ColorGroupSettingsList newColorGroups = ColorGroups::generateColorGroupSettingsFrom(
                mergedDepartureList(true, 40), settings.departureArrivalListType() );
        if ( colorGroups != newColorGroups ) {
            // Copy filterOut values from old color group settings
            for( int i = 0; i < newColorGroups.count(); ++i ) {
                ColorGroupSettings &newColorGroup = newColorGroups[i];
                if ( colorGroups.hasColor(newColorGroup.color) ) {
                    ColorGroupSettings colorGroup = colorGroups.byColor( newColorGroup.color );
                    newColorGroup.filterOut = colorGroup.filterOut;
                }
            }
            model->setColorGroups( newColorGroups );
            departureProcessor->setColorGroups( newColorGroups );

            // Change color group settings in a copy of the Settings object
            // Then write the changed settings
            Settings newSettings = settings;
            QList< ColorGroupSettingsList > colorGroups = newSettings.colorGroups();
            colorGroups[ newSettings.currentStopIndex() ] = newColorGroups;
            newSettings.setColorGroups( colorGroups );
            q->setSettings( newSettings );
        }
    } else {
        // Remove color groups if colorization was toggled off
        // or if stop/filter settings were changed (update color groups after data arrived)
        model->setColorGroups( ColorGroupSettingsList() );
        departureProcessor->setColorGroups( ColorGroupSettingsList() );
    }
}

void PublicTransportAppletPrivate::updateDepartureListIcon()
{
    if ( isStateActive( "intermediateDepartureView" ) ) {
        titleWidget->setIcon( GoBackIcon );
    } else {
        titleWidget->setIcon( isStateActive( "departureDataValid" )
                              ? DepartureListOkIcon : DepartureListErrorIcon );
    }
}

QString PublicTransportAppletPrivate::courtesyToolTip() const
{
    // Get courtesy information for the current service provider from the data engine
    QVariantHash data = currentProviderData;
    QString credit;
    if ( !data.isEmpty() ) {
        credit = data["credit"].toString();
    }

    if ( credit.isEmpty() ) {
        // No courtesy information given by the data engine
        return QString();
    } else {
        return credit;
    }
}

QString PublicTransportAppletPrivate::infoText()
{
    // Get information about the current service provider from the data engine
    const QVariantHash data = currentProviderData;
    const QString shortUrl = data.isEmpty() ? "-" : data["shortUrl"].toString();
    const QString url = data.isEmpty() ? "-" : data["url"].toString();
    QString credit = data["credit"].toString();
    const QSizeF size = graphicsWidget->size();
    const QFontMetrics fm( labelInfo->font() );

    if ( !credit.isEmpty() ) {
        // Credit string available, show it as link to the provider home page
        return QString("<a href='%1'>%2</a>")
                .arg(url, fm.elidedText(credit, Qt::ElideMiddle, size.width() - 2));
    } else {
        // No credit string available, show a link to the provider home page.
        // If there is enough space, also show a label.
        const QString labelText = i18nc( "@info/plain", "Data by" );
        const QString labelTextLong = i18nc( "@info/plain", "Timetable data by" );
        const QString linkHtml = QString( "<a href='%1'>%2</a>" ).arg( url, shortUrl );
        const QString html = labelText + ": " + linkHtml;
        const QString htmlLong = labelTextLong + ": " + linkHtml;
        const int width = fm.width( labelText + ": " + shortUrl );
        const int widthLong = fm.width( labelTextLong + ": " + shortUrl );
        if ( size.width() >= widthLong ) {
            // Enough horizontal space to show the longer label
            return htmlLong;
        } else if ( size.width() >= width ) {
            // Enough horizontal space to show the shorter label
            return html;
        } else {
            // Not enough horizontal space for the label, only show the link
            return linkHtml;
        }
    }
}

QString PublicTransportAppletPrivate::infoTooltip()
{
    QString tooltip = courtesyToolTip();
    if ( !nextAutomaticSourceUpdate.isValid() ) {
        return tooltip;
    }

    // Add information about the next automatic update time (with minute precision)
    qint64 msecs = QDateTime::currentDateTime().msecsTo( nextAutomaticSourceUpdate );
    if ( msecs > 0 ) {
        if ( !tooltip.isEmpty() ) {
            tooltip += ", ";
        }
        tooltip += i18nc("@info:tooltip %1 is a duration string with minute precision, "
                        "as returned by KLocale::prettyFormatDuration()",
                        "next automatic update in %1",
                        KGlobal::locale()->prettyFormatDuration(qCeil(msecs / 60000.0) * 60000));
    }

    // Add information about the minimal next (malnual) update time (with second precision)
    if ( minManualSourceUpdateTime.isValid() ) {
        qint64 minMsecs = QDateTime::currentDateTime().msecsTo( minManualSourceUpdateTime );
        if ( minMsecs > 0 ) {
            if ( !tooltip.isEmpty() ) {
                tooltip += ", ";
            }
            tooltip += i18nc("@info:tooltip %1 is a duration string with second precision, "
                             "as returned by KLocale::prettyFormatDuration()",
                             "updates blocked for %1",
                    KGlobal::locale()->prettyFormatDuration(qCeil(minMsecs / 1000.0) * 1000));
        }
    }

    QString sLastUpdate = lastSourceUpdate.toString( "hh:mm" );
    if ( sLastUpdate.isEmpty() ) {
        sLastUpdate = i18nc( "@info/plain This is used as 'last data update' "
                             "text when there hasn't been any updates yet.", "none" );
    }
    const QString dataByTextLocalized = i18nc( "@info/plain", "data by" );
    if ( !tooltip.isEmpty() ) {
        tooltip += ", ";
    }
    tooltip += i18nc("@info/plain", "last update: %1", sLastUpdate );
    return tooltip;
}

Plasma::Animation *PublicTransportAppletPrivate::fadeOutOldAppearance()
{
    Q_Q( PublicTransportApplet );

    if ( q->isVisible() && stateMachine ) {
        // Draw old appearance to pixmap
        QPixmap pixmap( mainGraphicsWidget->size().toSize() );
        pixmap.fill( Qt::transparent );
        QPainter p( &pixmap );
        QRect sourceRect = mainGraphicsWidget->mapToScene( mainGraphicsWidget->boundingRect() )
                           .boundingRect().toRect();
        QRectF rect( QPointF( 0, 0 ), mainGraphicsWidget->size() );
        titleWidget->scene()->render( &p, rect, sourceRect );

        // Fade from old to new appearance
        onOldItemAnimationFinished();
        oldItem = new GraphicsPixmapWidget( pixmap, graphicsWidget );
        oldItem->setPos( 0, 0 );
        oldItem->setZValue( 1000 );
        Plasma::Animation *animOut = Plasma::Animator::create( Plasma::Animator::FadeAnimation );
        animOut->setProperty( "startOpacity", 1 );
        animOut->setProperty( "targetOpacity", 0 );
        animOut->setTargetWidget( oldItem );
        q->connect( animOut, SIGNAL(finished()), q, SLOT(oldItemAnimationFinished()) );
        animOut->start( QAbstractAnimation::DeleteWhenStopped );
        return animOut;
    } else {
        return 0;
    }
}

KSelectAction *PublicTransportAppletPrivate::createSwitchStopAction( QObject *parent, bool destroyOverlayOnTrigger ) const
{
    Q_Q( const PublicTransportApplet );

    KSelectAction *switchStopAction = new KSelectAction(
        KIcon( "public-transport-stop" ), i18nc( "@action", "Switch Current Stop" ), parent );
    for( int i = 0; i < settings.stops().count(); ++i ) {
        QString stopList = settings.stop( i ).stops().join( ",\n" );
        QString stopListShort = settings.stop( i ).stops().join( ", " );
        if ( stopListShort.length() > 30 ) {
            stopListShort = stopListShort.left( 30 ).trimmed() + "...";
        }

        // Use a shortened stop name list as display text
        // and the complete version as tooltip (if it is different)
        QAction *stopAction = new QAction( stopListShort, parent );
        if ( stopList != stopListShort ) {
            stopAction->setToolTip( stopList );
        }
        stopAction->setData( i );
        if ( destroyOverlayOnTrigger ) {
            q->connect( stopAction, SIGNAL(triggered()), q->action("backToDepartures"),
                        SLOT(trigger()) );
        }

        stopAction->setCheckable( true );
        stopAction->setChecked( i == settings.currentStopIndex() );
        switchStopAction->addAction( stopAction );
    }

    q->connect( switchStopAction, SIGNAL(triggered(QAction*)),
                q, SLOT(setCurrentStopIndex(QAction*)) );
    return switchStopAction;
}

void PublicTransportAppletPrivate::onDepartureDataStateChanged()
{
    Q_Q( PublicTransportApplet );
    QString noItemsText;
    bool busy = false;

    if ( isStateActive("providerError") ) {
        // The used provider has an error or is not ready
        if ( currentProviderData["error"].toBool() ) {
            noItemsText = currentProviderData["errorMessage"].toString();
        } else if ( currentProviderData["state"].toString() != QLatin1String("ready") ) {
            noItemsText = currentProviderData["stateData"].toHash()
                    .value("statusMessage").toString();
        } else {
            // Unknown error, use the same string like for the departureDataInvalid state
            noItemsText = settings.departureArrivalListType() == ArrivalList
                    ? i18nc("@info/plain", "No arrivals due to an error.")
                    : i18nc("@info/plain", "No departures due to an error.");
        }
    } else if ( isStateActive("departureDataWaiting") ) {
        if ( settings.departureArrivalListType() == ArrivalList ) {
            noItemsText = i18nc("@info/plain", "Waiting for arrivals...");
        } else {
            noItemsText = i18nc("@info/plain", "Waiting for departures...");
        }
        busy = model->isEmpty();
    } else if ( isStateActive("departureDataInvalid") ) {
        noItemsText = settings.departureArrivalListType() == ArrivalList
                ? i18nc("@info/plain", "No arrivals due to an error.")
                : i18nc("@info/plain", "No departures due to an error.");
    } else if ( settings.departureArrivalListType() == ArrivalList ) {
        // Valid arrivals
        noItemsText = !settings.currentFilters().isEmpty()
                ? i18nc("@info/plain", "No unfiltered arrivals.<nl/>"
                        "You can disable filters to see all arrivals.")
                : i18nc("@info/plain", "No arrivals.");
    } else { // Valid departures
        noItemsText = !settings.currentFilters().isEmpty()
                ? i18nc("@info/plain", "No unfiltered departures.<nl/>"
                        "You can disable filters to see all departures.")
                : i18nc("@info/plain", "No departures.");
    }

    updateDepartureListIcon();
    timetable->setNoItemsText( noItemsText );
    q->setBusy( busy );
}

void PublicTransportAppletPrivate::onJourneyDataStateChanged()
{
    Q_Q( PublicTransportApplet );
    if ( isStateActive("journeyView") ) {
        MainIconDisplay icon;
        QString noItemsText;
        bool busy = false;

        if ( isStateActive("journeyDataWaiting") ) {
            icon = JourneyListErrorIcon;
            noItemsText = i18nc("@info/plain", "Waiting for journeys...");
            busy = modelJourneys->isEmpty();
        } else if ( isStateActive("journeyDataInvalid") ) {
            icon = JourneyListErrorIcon;
            noItemsText = i18nc("@info/plain", "No journeys due to an error.");
        } else {
            icon = JourneyListOkIcon;
            noItemsText = i18nc("@info/plain", "No journeys.");
        }

        titleWidget->setIcon( icon );
        journeyTimetable->setNoItemsText( noItemsText );
        q->setBusy( busy );
    }
}

StopDataConnection::StopDataConnection( Plasma::DataEngine *engine, const QString &providerId,
                                        const QString &stopName, QObject *parent )
        : QObject(parent)
{
    const QString sourceName = QString( "Stops %1|stop=%2" ).arg( providerId, stopName );
    engine->connectSource( sourceName, this );
}

void StopDataConnection::dataUpdated( const QString &sourceName,
                                      const Plasma::DataEngine::Data &data )
{
    Q_UNUSED( sourceName );
    if ( !data.contains("stops") ) {
        kWarning() << "Stop coordinates not found";
    } else {
        const QVariantHash stop = data[ "stops" ].toList().first().toHash(); // TODO Error handling
        const QString stopName = stop[ "StopName" ].toString();
        if ( stop.contains("StopLongitude") && stop.contains("StopLatitude") ) {
            const bool coordinatesAreValid = stop.contains("StopLongitude") &&
                                             stop.contains("StopLatitude");
            const qreal longitude = coordinatesAreValid ? stop["StopLongitude"].toReal() : 0.0;
            const qreal latitude = coordinatesAreValid ? stop["StopLatitude"].toReal() : 0.0;
            emit stopDataReceived( stopName, coordinatesAreValid, longitude, latitude );
        }

        emit stopDataReceived( stop );
    }

    // Automatically delete after data was received
    deleteLater();
}
