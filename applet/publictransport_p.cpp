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
#include <QList>

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

PublicTransportPrivate::PublicTransportPrivate( PublicTransport *q )
        : graphicsWidget(0), mainGraphicsWidget(0),
        oldItem(0), titleWidget(0), labelInfo(0), timetable(0), journeyTimetable(0),
        labelJourneysNotSupported(0), listStopSuggestions(0), overlay(0), model(0),
        popupIcon(0), titleToggleAnimation(0), modelJourneys(0),
        filtersGroup(0), colorFiltersGroup(0), departureProcessor(0), departurePainter(0),
        stateMachine(0), journeySearchTransition1(0), journeySearchTransition2(0),
        journeySearchTransition3(0), marble(0), q_ptr( q )
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

void PublicTransportPrivate::onSettingsChanged( const Settings &_settings, SettingsIO::ChangedFlags changed )
{
    Q_Q( PublicTransport );

    if ( !changed.testFlag(SettingsIO::IsChanged) ) {
        kDebug() << "No changes made in the settings";
        return;
    }

    // Copy new settings
    settings = _settings;

    QVariantHash serviceProviderData = currentServiceProviderData();
    currentServiceProviderFeatures = serviceProviderData.isEmpty()
            ? QStringList() : serviceProviderData["features"].toStringList();
    emit q->configNeedsSaving();
    emit q->settingsChanged();

    // First update the departure processor
    if ( changed.testFlag(SettingsIO::ChangedDepartureArrivalListType) ) {
        departureProcessor->setDepartureArrivalListType( settings.departureArrivalListType );
    }
    if ( changed.testFlag(SettingsIO::ChangedFilterSettings) ) {
        departureProcessor->setFilterSettings( settings.currentFilterSettings() );
    }
    if ( changed.testFlag(SettingsIO::ChangedColorGroupSettings) ) {
        departureProcessor->setColorGroups( settings.currentColorGroupSettings() );
    }

    // If stop settings have changed the whole model gets cleared and refilled.
    // Therefore the other change flags can be in 'else' parts
    if ( changed.testFlag(SettingsIO::ChangedServiceProvider) ||
         changed.testFlag(SettingsIO::ChangedCurrentStopSettings) ||
         changed.testFlag(SettingsIO::ChangedCurrentStop) )
    {
        if ( changed.testFlag(SettingsIO::ChangedCurrentStopSettings) ) {
            // Apply first departure settings to the worker thread
            const StopSettings stopSettings = settings.currentStopSettings();
            departureProcessor->setFirstDepartureSettings(
                    static_cast<FirstDepartureConfigMode>(stopSettings.get<int>(
                                FirstDepartureConfigModeSetting)),
                    stopSettings.get<QTime>(TimeOfFirstDepartureSetting),
                    stopSettings.get<int>(TimeOffsetOfFirstDepartureSetting) );
        }

        labelInfo->setToolTip( courtesyToolTip() );
        labelInfo->setText( infoText() );

        settings.adjustColorGroupSettingsCount();
        clearDepartures();
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
        model->setAlarmSettings( settings.alarmSettingsList );
        if ( modelJourneys ) {
            modelJourneys->setAlarmSettings( settings.alarmSettingsList );
        }
        departureProcessor->setAlarmSettings( settings.alarmSettingsList );
    }

    // Apply show departures/arrivals setting
    if ( changed.testFlag(SettingsIO::ChangedDepartureArrivalListType) ) {
        model->setDepartureArrivalListType( settings.departureArrivalListType );

        // Update text in the departure/arrival view that gets shown when the model is empty
        onDepartureDataStateChanged();
    }

    // Apply font / size factor
    if ( changed.testFlag(SettingsIO::ChangedFont) ||
         changed.testFlag(SettingsIO::ChangedSizeFactor) )
    {
        // Get fonts
        QFont font = settings.sizedFont();
        int smallPointSize = KGlobalSettings::smallestReadableFont().pointSize() * settings.sizeFactor;
        QFont smallFont = font;
        smallFont.setPointSize( smallPointSize > 0 ? smallPointSize : 1 );

        // Apply fonts, update indentation and icon sizes to size factor, update options
        labelInfo->setFont( smallFont );
        timetable->setFont( font );

        if ( changed.testFlag(SettingsIO::ChangedSizeFactor) ) {
            timetable->setZoomFactor( settings.sizeFactor );
        }
        if ( journeyTimetable && isStateActive("journeyView") ) {
            journeyTimetable->setFont( font );
            if ( changed.testFlag(SettingsIO::ChangedSizeFactor) ) {
                journeyTimetable->setZoomFactor( settings.sizeFactor );
            }
        }
    }

    // Apply shadow settings
    if ( changed.testFlag(SettingsIO::ChangedShadows) ) {
        timetable->setOption( PublicTransportWidget::DrawShadowsOrHalos, settings.drawShadows );
        if ( journeyTimetable && isStateActive("journeyView") ) {
            journeyTimetable->setOption( PublicTransportWidget::DrawShadowsOrHalos,
                                         settings.drawShadows );
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
        timetable->setTargetHidden( settings.hideColumnTarget );
        timetable->updateItemLayouts();
    }
}

void PublicTransportPrivate::onUnknownSettingsChanged()
{

    // Apply other settings to the model
    timetable->setMaxLineCount( settings.linesPerRow );
    model->setLinesPerRow( settings.linesPerRow );
    model->setSizeFactor( settings.sizeFactor );
    model->setDepartureColumnSettings( settings.displayTimeBold,
                                       settings.showRemainingMinutes, settings.showDepartureTime );

    int alarmMinsBeforeDeparture = settings.currentStopSettings().get<int>( AlarmTimeSetting );
    model->setAlarmMinsBeforeDeparture( alarmMinsBeforeDeparture );
    modelJourneys->setAlarmMinsBeforeDeparture( alarmMinsBeforeDeparture );

    //     modelJourneys->setHomeStop( settings.currentStopSettings().stop(0).name ); DONE IN WRITESETTINGS

    // Limit model item count to the maximal number of departures setting
    if ( model->rowCount() > settings.maximalNumberOfDepartures ) {
        model->removeRows( settings.maximalNumberOfDepartures,
                           model->rowCount() - settings.maximalNumberOfDepartures );
    }
}

void PublicTransportPrivate::onDepartureArrivalListTypeChanged()
{
    Q_Q( PublicTransport );

    model->setDepartureArrivalListType( settings.departureArrivalListType );
    timetable->updateItemLayouts();

    // Adjust action texts to departure / arrival list
    q->action( "removeAlarmForDeparture" )->setText(
        settings.departureArrivalListType == DepartureList
        ? i18nc( "@action", "Remove &Alarm for This Departure" )
        : i18nc( "@action", "Remove &Alarm for This Arrival" ) );
    q->action( "createAlarmForDeparture" )->setText(
        settings.departureArrivalListType == DepartureList
        ? i18nc( "@action", "Set &Alarm for This Departure" )
        : i18nc( "@action", "Set &Alarm for This Arrival" ) );
    q->action( "backToDepartures" )->setText(
        settings.departureArrivalListType == DepartureList
        ? i18nc( "@action", "Back to &Departure List" )
        : i18nc( "@action", "Back to &Arrival List" ) );
}

void PublicTransportPrivate::onCurrentStopSettingsChanged()
{
    model->setHomeStop( settings.currentStopSettings().stop(0).name );
    model->setCurrentStopIndex( settings.currentStopSettingsIndex );

    if ( modelJourneys ) {
        modelJourneys->setHomeStop( settings.currentStopSettings().stop(0).name );
        modelJourneys->setCurrentStopIndex( settings.currentStopSettingsIndex );
    }
}

void PublicTransportPrivate::onServiceProviderSettingsChanged()
{
    Q_Q( PublicTransport );

    if ( settings.checkConfig() ) {
        // Configuration is valid
        q->setConfigurationRequired( false );

        // Only use the default target state (journey search) if journeys
        // are supported by the used service provider. Otherwise go to the
        // alternative target state (journeys not supported).
        const bool journeysSupported = currentServiceProviderFeatures.contains( "JourneySearch" );
        QAbstractState *target = journeysSupported
                                 ? states["journeySearch"] : states["journeysUnsupportedView"];
        journeySearchTransition1->setTargetState( target );
        journeySearchTransition2->setTargetState( target );
        journeySearchTransition3->setTargetState( target );

        q->action( "journeys" )->setEnabled( journeysSupported );
        titleWidget->setJourneysSupported( journeysSupported );

        // Reconnect with new settings
        reconnectSource();
        if ( !currentJourneySource.isEmpty() ) {
            reconnectJourneySource();
        }
    } else {
        // Missing configuration, eg. no home stop
        q->setConfigurationRequired( true, i18nc( "@info/plain", "Please check your configuration." ) );

        q->action( "journeys" )->setEnabled( false );
        titleWidget->setJourneysSupported( false );
    }
}

void PublicTransportPrivate::onResized()
{
    Q_Q( PublicTransport );

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

            q->connect( titleToggleAnimation, SIGNAL( finished() ),
                        q, SLOT( titleToggleAnimationFinished() ) );
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

            q->connect( titleToggleAnimation, SIGNAL( finished() ),
                        q, SLOT( titleToggleAnimationFinished() ) );
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

void PublicTransportPrivate::onOldItemAnimationFinished()
{
    if ( oldItem && oldItem->scene() ) {
        oldItem->scene()->removeItem( oldItem );
    }
    delete oldItem;
    oldItem = 0;
}

void PublicTransportPrivate::updateInfoText()
{
    labelInfo->setToolTip( courtesyToolTip() );
    labelInfo->setText( infoText() );
}

void PublicTransportPrivate::applyTheme()
{
    Q_Q( PublicTransport );
    if ( settings.useDefaultFont ) {
        q->configChanged();
    }

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
    model->setDepartureArrivalListType( settings.departureArrivalListType );
    timetable->updateItemLayouts();
}

void PublicTransportPrivate::createTooltip()
{
    Q_Q( PublicTransport );

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

        if ( settings.departureArrivalListType ==  DepartureList ) {
            // Showing a departure list
            foreach( const DepartureItem * item, currentGroup ) {
                infoStrings << i18nc( "@info Text for one departure for the tooltip (%1: line string, "
                                      "%2: target)",
                                      "Line <emphasis strong='1'>%1<emphasis> "
                                      "to <emphasis strong='1'>%2<emphasis>",
                                      item->departureInfo()->lineString(),
                                      item->departureInfo()->target() );
            }
            if ( isAlarmGroup ) {
                data.setSubText( i18ncp( "@info %2 is the translated duration text (e.g. in 3 minutes), "
                                         "%4 contains texts for a list of departures",
                                         "Alarm (%2) for a departure from '%3':<nl/>%4",
                                         "%1 Alarms (%2) for departures from '%3':<nl/>%4",
                                         currentGroup.count(),
                                         groupDurationString, settings.currentStopSettings().stops().join( ", " ),
                                         infoStrings.join( ",<nl/>" ) ) );
            } else {
                data.setSubText( i18ncp( "@info %2 is the translated duration text (e.g. in 3 minutes), "
                                         "%4 contains texts for a list of departures",
                                         "Departure (%2) from '%3':<nl/>%4",
                                         "%1 Departures (%2) from '%3':<nl/>%4",
                                         currentGroup.count(),
                                         groupDurationString, settings.currentStopSettings().stops().join( ", " ),
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
                                         groupDurationString, settings.currentStopSettings().stops().join( ", " ),
                                         infoStrings.join( ",<nl/>" ) ) );
            } else {
                data.setSubText( i18ncp( "@info %2 is the translated duration text (e.g. in 3 minutes), "
                                         "%4 contains texts for a list of arrivals",
                                         "Arrival (%2) at '%3':<nl/>%4",
                                         "%1 Arrivals (%2) at '%3':<nl/>%4",
                                         currentGroup.count(),
                                         groupDurationString, settings.currentStopSettings().stops().join( ", " ),
                                         infoStrings.join( ",<nl/>" ) ) );
            }
        }
    }

    data.setImage( KIcon( "public-transport-stop" ).pixmap( IconSize( KIconLoader::Desktop ) ) );
    Plasma::ToolTipManager::self()->setContent( q, data );
}

QString PublicTransportPrivate::stripDateAndTimeValues( const QString &sourceName ) const
{
    QString ret = sourceName;
    QRegExp rx( "(time=[^\\|]*|datetime=[^\\|]*)", Qt::CaseInsensitive );
    rx.setMinimal( true );
    ret.replace( rx, QChar() );
    return ret;
}

void PublicTransportPrivate::fillModel( const QList<DepartureInfo> &departures )
{
    bool modelFilled = model->rowCount() >= settings.maximalNumberOfDepartures;
    foreach( const DepartureInfo & departureInfo, departures ) {
        QModelIndex index = model->indexFromInfo( departureInfo );
        if ( !index.isValid() ) {
            // Departure wasn't in the model
            if ( !modelFilled && !departureInfo.isFilteredOut() ) {
                // Departure doesn't get filtered out and the model isn't full => Add departure
                model->addItem( departureInfo );
                modelFilled = model->rowCount() >= settings.maximalNumberOfDepartures;
            }
        } else if ( departureInfo.isFilteredOut() ) {
            // Departure has been marked as "filtered out" in the DepartureProcessor => Remove departure
            model->removeItem( model->itemFromInfo( departureInfo ) );
        } else {
            // Departure isn't filtered out => Update associated item in the model
            DepartureItem *item = dynamic_cast<DepartureItem *>( model->itemFromIndex( index ) );
            model->updateItem( item, departureInfo );
        }
    }

    // Sort departures in the model.
    // They are most probably already sorted, but sometimes they are not
    model->sort( ColumnDeparture );
}

void PublicTransportPrivate::fillModelJourney( const QList<JourneyInfo> &journeys )
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

void PublicTransportPrivate::updateFilterMenu()
{
    Q_Q( PublicTransport );

    KActionMenu *actionFilter = qobject_cast< KActionMenu * >( q->action( "filterConfiguration" ) );
    KMenu *menu = actionFilter->menu();
    menu->clear();

    QList< QAction * > oldActions = filtersGroup->actions();
    foreach( QAction * oldAction, oldActions ) {
        filtersGroup->removeAction( oldAction );
        delete oldAction;
    }

    bool showColorGrous = settings.colorize && !settings.colorGroupSettingsList.isEmpty();
    if ( settings.filterSettingsList.isEmpty() && !showColorGrous ) {
        return; // Nothing to show in the filter menu
    }

    if ( !settings.filterSettingsList.isEmpty() ) {
        menu->addTitle( KIcon( "view-filter" ), i18nc( "@title This is a menu title",
                        "Filters (reducing)" ) );
        foreach( const FilterSettings & filterSettings, settings.filterSettingsList ) {
            QAction *action = new QAction( filterSettings.name, filtersGroup );
            action->setCheckable( true );
            if ( filterSettings.affectedStops.contains( settings.currentStopSettingsIndex ) ) {
                action->setChecked( true );
            }

            menu->addAction( action );
        }
    }

    if ( showColorGrous ) {
        // Add checkbox entries to toggle color groups
        if ( settings.departureArrivalListType == ArrivalList ) {
            menu->addTitle( KIcon( "object-group" ), i18nc( "@title This is a menu title",
                            "Arrival Groups (extending)" ) );
        } else {
            menu->addTitle( KIcon( "object-group" ), i18nc( "@title This is a menu title",
                            "Departure Groups (extending)" ) );
        }
        foreach( const ColorGroupSettings & colorGroupSettings,
                 settings.currentColorGroupSettings() ) {
            // Create action for current color group
            QAction *action = new QAction( colorGroupSettings.displayText, colorFiltersGroup );
            action->setCheckable( true );
            if ( !colorGroupSettings.filterOut ) {
                action->setChecked( true );
            }
            action->setData( QVariant::fromValue( colorGroupSettings.color ) );

            // Draw a color patch with the color of the color group
            QPixmap pixmap( QSize( 16, 16 ) );
            pixmap.fill( Qt::transparent );
            QPainter p( &pixmap );
            p.setRenderHints( QPainter::Antialiasing );
            p.setBrush( colorGroupSettings.color );
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

void PublicTransportPrivate::updateJourneyMenu()
{
    Q_Q( PublicTransport );

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

QList<DepartureInfo> PublicTransportPrivate::mergedDepartureList( bool includeFiltered,
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
    return max == -1 ? ret.mid( 0, settings.maximalNumberOfDepartures ) : ret.mid( 0, max );
}

void PublicTransportPrivate::reconnectSource()
{
    Q_Q( PublicTransport );
    disconnectSources();

    // Get a list of stops (or stop IDs if available) which results are currently shown
    StopSettings curStopSettings = settings.currentStopSettings();
    QStringList stops = curStopSettings.stops();
    QStringList stopIDs = curStopSettings.stopIDs();
    if ( stopIDs.isEmpty() ) {
        if ( stops.isEmpty() ) {
            // Currently no stops configured
            return;
        }
        stopIDs = stops;
    }

    // Build source names for each (combined) stop for the publictransport data engine
    kDebug() << "Connect" << settings.currentStopSettingsIndex << stops;
    QStringList sources;
    stopIndexToSourceName.clear();
    for( int i = 0; i < stops.count(); ++i ) {
        QString stopValue = stopIDs[i].isEmpty() ? stops[i] : stopIDs[i];
        QString currentSource = QString( "%4 %1|stop=%2" )
                                .arg( settings.currentStopSettings().get<QString>( ServiceProviderSetting ) )
                                .arg( stopValue )
                                .arg( settings.departureArrivalListType == ArrivalList
                                      ? "Arrivals" : "Departures" );
        if ( static_cast<FirstDepartureConfigMode>( curStopSettings.get<int>(
                    FirstDepartureConfigModeSetting ) ) == RelativeToCurrentTime ) {
            currentSource += QString( "|timeOffset=%1" ).arg(
                                 curStopSettings.get<int>( TimeOffsetOfFirstDepartureSetting ) );
        } else {
            currentSource += QString( "|time=%1" ).arg(
                                 curStopSettings.get<QTime>( TimeOfFirstDepartureSetting ).toString( "hh:mm" ) );
        }
        if ( !curStopSettings.get<QString>( CitySetting ).isEmpty() ) {
            currentSource += QString( "|city=%1" ).arg( curStopSettings.get<QString>( CitySetting ) );
        }

        stopIndexToSourceName[ i ] = currentSource;
        sources << currentSource;
    }

    emit q->requestedNewDepartureData();

    foreach( const QString & currentSource, sources ) {
        kDebug() << "Connect data source" << currentSource
                 << "Autoupdate" << settings.autoUpdate;
        currentSources << currentSource;
        if ( settings.autoUpdate ) {
            // Update once a minute
            q->dataEngine( "publictransport" )->connectSource( currentSource, q,
                    60000, Plasma::AlignToMinute );
        } else {
            q->dataEngine( "publictransport" )->connectSource( currentSource, q );
        }
    }
}

void PublicTransportPrivate::disconnectSources()
{
    Q_Q( PublicTransport );
    if ( !currentSources.isEmpty() ) {
        foreach( const QString & currentSource, currentSources ) {
            kDebug() << "Disconnect data source" << currentSource;
            q->dataEngine( "publictransport" )->disconnectSource( currentSource, q );
        }
        currentSources.clear();
    }
}

void PublicTransportPrivate::disconnectJourneySource()
{
    Q_Q( PublicTransport );
    if ( !currentJourneySource.isEmpty() ) {
        kDebug() << "Disconnect journey data source" << currentJourneySource;
        q->dataEngine( "publictransport" )->disconnectSource( currentJourneySource, q );
    }
}

bool PublicTransportPrivate::isStateActive( const QString &stateName ) const
{
    return states.contains( stateName )
           && stateMachine->configuration().contains( states[stateName] );
}

void PublicTransportPrivate::reconnectJourneySource( const QString &targetStopName,
        const QDateTime &dateTime, bool stopIsTarget, bool timeIsDeparture,
        bool requestStopSuggestions )
{
    Q_Q( PublicTransport );

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
                               .arg( settings.currentStopSettings().get<QString>(ServiceProviderSetting) )
                               .arg( _targetStopName );
    } else {
        currentJourneySource = QString( stopIsTarget
                                        ? "%6 %1|originStop=%2|targetStop=%3|maxCount=%4|datetime=%5"
                                        : "%6 %1|originStop=%3|targetStop=%2|maxCount=%4|datetime=%5" )
                               .arg( settings.currentStopSettings().get<QString>(ServiceProviderSetting) )
                               .arg( settings.currentStopSettings().stop(0).nameOrId() )
                               .arg( _targetStopName )
                               .arg( settings.maximalNumberOfDepartures )
                               .arg( _dateTime.toString() )
                               .arg( timeIsDeparture ? "Journeys" : "JourneysArr" );
        QString currentStop = settings.currentStopSettings().stops().first();
        journeyTitleText = stopIsTarget
                           ? i18nc( "@info", "From %1<nl/>to <emphasis strong='1'>%2</emphasis>",
                                    currentStop, _targetStopName )
                           : i18nc( "@info", "From <emphasis strong='1'>%1</emphasis><nl/>to %2",
                                    _targetStopName, currentStop );
        if ( isStateActive( "journeyView" ) ) {
            titleWidget->setTitle( journeyTitleText );
        }
    }

    if ( !settings.currentStopSettings().get<QString>(CitySetting).isEmpty() ) {
        currentJourneySource += QString( "|city=%1" ).arg(
                                    settings.currentStopSettings().get<QString>(CitySetting) );
    }

    lastSecondStopName = _targetStopName;
    emit q->requestedNewJourneyData();
    q->dataEngine( "publictransport" )->connectSource( currentJourneySource, q );
}

void PublicTransportPrivate::updateColorGroupSettings()
{
    Q_Q( PublicTransport );
    if ( settings.colorize ) {
        // Generate color groups from existing departure data
        settings.adjustColorGroupSettingsCount();
        ColorGroupSettingsList colorGroups = settings.currentColorGroupSettings();
        ColorGroupSettingsList newColorGroups = ColorGroups::generateColorGroupSettingsFrom(
                mergedDepartureList(true, 40), settings.departureArrivalListType );
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
            newSettings.colorGroupSettingsList[ newSettings.currentStopSettingsIndex ] = newColorGroups;
            q->setSettings( newSettings );
        }
    } else {
        // Remove color groups if colorization was toggled off
        // or if stop/filter settings were changed (update color groups after data arrived)
        model->setColorGroups( ColorGroupSettingsList() );
        departureProcessor->setColorGroups( ColorGroupSettingsList() );
    }
}

void PublicTransportPrivate::updateDepartureListIcon()
{
    if ( isStateActive( "intermediateDepartureView" ) ) {
        titleWidget->setIcon( GoBackIcon );
    } else {
        titleWidget->setIcon( isStateActive( "departureDataValid" )
                              ? DepartureListOkIcon : DepartureListErrorIcon );
    }
}

QString PublicTransportPrivate::courtesyToolTip() const
{
    // Get courtesy information for the current service provider from the data engine
    QVariantHash data = currentServiceProviderData();
    QString credit, url;
    if ( !data.isEmpty() ) {
        credit = data["credit"].toString();
        url = data["url"].toString();
    }

    if ( credit.isEmpty() || url.isEmpty() ) {
        // No courtesy information given by the data engine
        return QString();
    } else {
        return i18nc( "@info/plain", "By courtesy of %1 (%2)", credit, url );
    }
}

QString PublicTransportPrivate::infoText()
{
    // Get information about the current service provider from the data engine
    const QVariantHash data = currentServiceProviderData();
    const QString shortUrl = data.isEmpty() ? "-" : data["shortUrl"].toString();
    const QString url = data.isEmpty() ? "-" : data["url"].toString();
    QString sLastUpdate = lastSourceUpdate.toString( "hh:mm" );
    if ( sLastUpdate.isEmpty() ) {
        sLastUpdate = i18nc( "@info/plain This is used as 'last data update' "
                             "text when there hasn't been any updates yet.", "none" );
    }

    // Plasma::Label sets it's height as if the label would show the HTML source.
    // Therefore the <nobr>'s are inserted to prevent using multiple lines unnecessarily
    const qreal minHeightForTwoLines = 250.0;
    const QString dataByTextLocalized = i18nc( "@info/plain", "data by" );
    const QString textNoHtml1 = QString( "%1: %2" )
                                .arg( i18nc( "@info/plain", "last update" ), sLastUpdate );
    const QString dataByLinkHtml = QString( "<a href='%1'>%2</a>" ).arg( url, shortUrl );
    const QString textHtml2 = dataByTextLocalized + ": " + dataByLinkHtml;
    QFontMetrics fm( labelInfo->font() );
    const int widthLine1 = fm.width( textNoHtml1 );
    const int widthLine2 = fm.width( dataByTextLocalized + ": " + shortUrl );
    const int width = widthLine1 + fm.width( ", " ) + widthLine2;
    QSizeF size = graphicsWidget->size();
    if ( size.width() >= width ) {
        // Enough horizontal space to show the complete info text in one line
        return "<nobr>" + textNoHtml1 + ", " + textHtml2 + "</nobr>";
    } else if ( size.height() >= minHeightForTwoLines &&
                size.width() >= widthLine1 && size.width() >= widthLine2 ) {
        // Not enough horizontal space to show the complete info text in one line,
        // but enough vertical space to break it into two lines, which both fit horizontally
        return "<nobr>" + textNoHtml1 + ",<br />" + textHtml2 + "</nobr>";
    } else if ( size.width() >= widthLine2 ) {
        // Do not show "last update" text, but credits info
        return "<nobr>" + textHtml2 + "</nobr>";
    } else {
        // Do not show "last update" text, but credits info, without "data by:" label
        return "<nobr>" + dataByLinkHtml + "</nobr>";
    }
}

Plasma::Animation *PublicTransportPrivate::fadeOutOldAppearance()
{
    Q_Q( PublicTransport );

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
        q->connect( animOut, SIGNAL( finished() ), q, SLOT( oldItemAnimationFinished() ) );
        animOut->start( QAbstractAnimation::DeleteWhenStopped );
        return animOut;
    } else {
        return 0;
    }
}

KSelectAction *PublicTransportPrivate::createSwitchStopAction( QObject *parent, bool destroyOverlayOnTrigger ) const
{
    Q_Q( const PublicTransport );

    KSelectAction *switchStopAction = new KSelectAction(
        KIcon( "public-transport-stop" ), i18nc( "@action", "Switch Current Stop" ), parent );
    for( int i = 0; i < settings.stopSettingsList.count(); ++i ) {
        QString stopList = settings.stopSettingsList[ i ].stops().join( ",\n" );
        QString stopListShort = settings.stopSettingsList[ i ].stops().join( ", " );
        if ( stopListShort.length() > 30 ) {
            stopListShort = stopListShort.left( 30 ).trimmed() + "...";
        }

        // Use a shortened stop name list as display text
        // and the complete version as tooltip (if it is different)
        QAction *stopAction = settings.departureArrivalListType == DepartureList
                              ? new QAction( i18nc( "@action", "Show Departures For '%1'", stopListShort ), parent )
                              : new QAction( i18nc( "@action", "Show Arrivals For '%1'", stopListShort ), parent );
        if ( stopList != stopListShort ) {
            stopAction->setToolTip( stopList );
        }
        stopAction->setData( i );
        if ( destroyOverlayOnTrigger ) {
            q->connect( stopAction, SIGNAL( triggered() ), q->action( "backToDepartures" ),
                        SLOT( trigger() ) );
        }

        stopAction->setCheckable( true );
        stopAction->setChecked( i == settings.currentStopSettingsIndex );
        switchStopAction->addAction( stopAction );
    }

    q->connect( switchStopAction, SIGNAL( triggered( QAction * ) ),
                q, SLOT( setCurrentStopIndex( QAction * ) ) );
    return switchStopAction;
}

void PublicTransportPrivate::onDepartureDataStateChanged()
{
    Q_Q( PublicTransport );
    QString noItemsText;
    bool busy = false;

    if ( isStateActive("departureDataWaiting") ) {
        if ( settings.departureArrivalListType == ArrivalList ) {
            noItemsText = i18nc("@info/plain", "Waiting for arrivals...");
        } else {
            noItemsText = i18nc("@info/plain", "Waiting for depatures...");
        }
        busy = model->isEmpty();
    } else if ( isStateActive("departureDataInvalid") ) {
        noItemsText = settings.departureArrivalListType == ArrivalList
                ? i18nc("@info/plain", "No arrivals due to an error.")
                : i18nc("@info/plain", "No departures due to an error.");
    } else if ( settings.departureArrivalListType == ArrivalList ) {
        // Valid arrivals
        noItemsText = !settings.currentFilterSettings().isEmpty()
                ? i18nc("@info/plain", "No unfiltered arrivals.<nl/>"
                        "You can disable filters to see all arrivals.")
                : i18nc("@info/plain", "No arrivals.");
    } else { // Valid departures
        noItemsText = !settings.currentFilterSettings().isEmpty()
                ? i18nc("@info/plain", "No unfiltered departures.<nl/>"
                        "You can disable filters to see all departures.")
                : i18nc("@info/plain", "No departures.");
    }

    updateDepartureListIcon();
    timetable->setNoItemsText( noItemsText );
    q->setBusy( busy );
}

void PublicTransportPrivate::onJourneyDataStateChanged()
{
    Q_Q( PublicTransport );
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
