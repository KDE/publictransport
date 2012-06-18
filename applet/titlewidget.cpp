/*
 *   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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

// Header
#include "titlewidget.h"

// Own includes
#include "settings.h"
#include "journeysearchlineedit.h"
#include "journeysearchmodel.h"
#include "journeysearchlistview.h"

// KDE+Plasma includes
#include <KAction>
#include <KActionMenu>
#include <KMenu>
#include <KIconEffect>
#include <Plasma/Label>
#include <Plasma/IconWidget>
#include <Plasma/LineEdit>
#include <Plasma/ToolButton>

// Qt includes
#include <QToolButton>
#include <QLabel>
#include <QGraphicsLinearLayout>
#include <QVBoxLayout>
#include <qmath.h>

TitleWidget::TitleWidget( TitleType titleType, Settings *settings, bool journeysSupported,
                          QGraphicsItem *parent )
        : QGraphicsWidget(parent), m_icon(0), m_filterWidget(0), m_journeysWidget(0),
            m_layout(new QGraphicsLinearLayout(Qt::Horizontal, this)), m_settings(settings),
            m_journeysSupported(journeysSupported), m_journeysAction(0), m_filtersAction(0)
{
    m_type = titleType;
    m_layout->setContentsMargins( 1, 1, 0, 0 );
    m_layout->setSpacing( 1 );
    m_layout->setItemSpacing( 0, 4 );

    // Initialize icon widget
    int iconExtend = 26 * settings->sizeFactor();
    Plasma::IconWidget *icon = new Plasma::IconWidget;
    icon->setIcon( "public-transport-stop" );
    icon->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    icon->setMinimumSize( iconExtend, iconExtend );
    icon->setMaximumSize( iconExtend, iconExtend );
    setIcon( icon );

    // Add a title label
    Plasma::Label *title = new Plasma::Label( this );
    title->setAlignment( Qt::AlignVCenter | Qt::AlignLeft );
    QLabel *_title = title->nativeWidget();
    _title->setTextInteractionFlags( Qt::LinksAccessibleByMouse );
    addWidget( title, WidgetTitle );

    if ( m_journeysSupported ) {
        // Add a quick journey search widget
        createAndAddWidget( WidgetQuickJourneySearch );
    }

    // Add a filter widget
    createAndAddWidget( WidgetFilter );

    // Adjust to settings
    settingsChanged();
}

void TitleWidget::setJourneysSupported( bool supported )
{
    if ( m_journeysSupported == supported ) {
        return;
    }

    m_journeysSupported = supported;
    if ( supported ) {
        createAndAddWidget( WidgetQuickJourneySearch );
    } else {
        removeWidget( WidgetQuickJourneySearch );
        delete m_journeysWidget;
        m_journeysWidget = 0;
    }
}

QString TitleWidget::title() const
{
    return m_title->text();
}

Plasma::Label* TitleWidget::titleWidget() const
{
    return m_title;
}

void TitleWidget::setTitleType( TitleType titleType,
                                bool validDepartureData, bool validJourneyData )
{
    // Remove old additional widgets
    clearWidgets();

    // New type
    m_type = titleType;
    switch ( titleType ) {
        case ShowDepartureArrivalListTitle:
            // Default state, a departure/arrival board is shown
            setIcon( validDepartureData ? DepartureListOkIcon : DepartureListErrorIcon );
            m_icon->setToolTip( i18nc("@info:tooltip", "Search journeys to or from the home stop") );
            setTitle( titleText() );

            // Show a title (with the stop name) and the filter and quick journey search widgets
            addWidget( m_title, WidgetTitle );
            if ( m_journeysSupported ) {
                addWidget( m_journeysWidget, WidgetQuickJourneySearch );
            }
            addWidget( m_filterWidget, WidgetFilter );
            break;

        case ShowIntermediateDepartureListTitle:
            // An intermediate deparure list is shown
            setIcon( GoBackIcon );
            m_icon->setToolTip( i18nc("@info:tooltip", "Go back to original stop") );
            setTitle( titleText() );

            // Same as for normal departure/arrival boards
            addWidget( m_title, WidgetTitle );
            if ( m_journeysSupported ) {
                addWidget( m_journeysWidget, WidgetQuickJourneySearch );
            }
            addWidget( m_filterWidget, WidgetFilter );
            break;

        case ShowSearchJourneyLineEdit: {
            // The journey search UI is shown
            setIcon( AbortJourneySearchIcon );
            m_icon->setToolTip( i18nc("@info:tooltip", "Abort search for journeys "
                                                       "to or from the home stop" ) );

            // Add widgets
            addJourneySearchWidgets();
            Plasma::LineEdit *journeySearchLine =
                    castedWidget<Plasma::LineEdit>(WidgetJourneySearchLine);
            journeySearchLine->setEnabled( true );
            journeySearchLine->setFocus();
            journeySearchLine->nativeWidget()->selectAll();
            break;
        }
        case ShowSearchJourneyLineEditDisabled:
            // The journey search UI is shown,
            // but the currently used service provider does not support journeys
            setIcon( AbortJourneySearchIcon );
            m_icon->setToolTip( i18nc("@info:tooltip", "Abort search for journeys "
                                                       "to or from the home stop") );

            // Add widgets
            addJourneySearchWidgets();

            // Disable all widgets, because journeys are not supported by the currently used
            // service provider
            castedWidget<Plasma::LineEdit>(WidgetJourneySearchLine)->setEnabled( false );
            castedWidget<Plasma::LineEdit>(WidgetFillJourneySearchLineButton)->setEnabled( false );
            castedWidget<Plasma::LineEdit>(WidgetStartJourneySearchButton)->setEnabled( false );
            break;

        case ShowJourneyListTitle: {
            // A list of journeys is shown
            setIcon( validJourneyData ? JourneyListOkIcon : JourneyListErrorIcon );
            m_icon->setToolTip( i18nc("@info:tooltip", "Show available action in the applet") );

            // Add a close icon to close the journey view
            int iconExtend = 26 * m_settings->sizeFactor();
            Plasma::IconWidget *closeIcon = new Plasma::IconWidget;
            closeIcon->setIcon( "window-close" );
            closeIcon->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
            closeIcon->setMinimumSize( iconExtend, iconExtend );
            closeIcon->setMaximumSize( iconExtend, iconExtend );
            closeIcon->setToolTip( i18nc("@info:tooltip", "Show departures / arrivals") );
            connect( closeIcon, SIGNAL(clicked()), this, SIGNAL(closeIconClicked()) );
            addWidget( closeIcon, WidgetCloseIcon );

            // Add a title label
            addWidget( m_title, WidgetTitle );
            break;
        }
    }
}

QString TitleWidget::titleText() const
{
    QString sStops = m_settings->currentStop().stops().join( ", " );
    if ( !m_settings->currentStop().get<QString>(CitySetting).isEmpty() ) {
        return QString( "%1, %2" ).arg( sStops )
                .arg( m_settings->currentStop().get<QString>(CitySetting) );
    } else {
        return QString( "%1" ).arg( sStops );
    }
}

void TitleWidget::slotJourneySearchInputFinished()
{
    Plasma::LineEdit *journeySearch =
            castedWidget<Plasma::LineEdit>( TitleWidget::WidgetJourneySearchLine );
    Q_ASSERT( journeySearch );

    emit journeySearchInputFinished( journeySearch->text() );
}

void TitleWidget::addJourneySearchWidgets()
{
    // Add recent journeys button
    Plasma::ToolButton *recentJourneysButton = new Plasma::ToolButton;
    recentJourneysButton->setIcon( KIcon("document-open-recent") );
    recentJourneysButton->setToolTip( i18nc("@info:tooltip", "Use a favorite/recent journey search") );
    recentJourneysButton->nativeWidget()->setPopupMode( QToolButton::InstantPopup );
    // This is needed, to have the popup menu drawn above other widgets
    recentJourneysButton->setZValue( 9999 );
    connect( recentJourneysButton, SIGNAL(clicked()), this, SLOT(slotJourneysIconClicked()) );

    // Add button to start the journey search
    Plasma::ToolButton *journeySearchButton = new Plasma::ToolButton;
    journeySearchButton->setIcon( KIcon("edit-find") );
    journeySearchButton->setToolTip( i18nc("@info:tooltip", "Find journeys") );
    journeySearchButton->setEnabled( false );
    connect( journeySearchButton, SIGNAL(clicked()), this, SLOT(slotJourneySearchInputFinished()) );

    // Add journey search query input field
    Plasma::LineEdit *journeySearchLineEdit = new Plasma::LineEdit;
    #if KDE_VERSION >= KDE_MAKE_VERSION(4,4,0)
        journeySearchLineEdit->setNativeWidget( new JourneySearchLineEdit );
    #endif
    journeySearchLineEdit->setToolTip(
            i18nc("@info:tooltip This should match the localized keywords.",
            "<para>Type a <emphasis strong='1'>target stop</emphasis> or "
            "<emphasis strong='1'>journey request</emphasis>.</para>"
            "<para><emphasis strong='1'>Samples:</emphasis><list>"
            "<item><emphasis>To target in 15 mins</emphasis></item>"
            "<item><emphasis>From origin arriving tomorrow at 18:00</emphasis></item>"
            "<item><emphasis>Target at 6:00 2010-03-07</emphasis></item>"
            "</list></para>") );
    journeySearchLineEdit->installEventFilter( this ); // Handle up/down keys (selecting stop suggestions)
    journeySearchLineEdit->setClearButtonShown( true );
    journeySearchLineEdit->nativeWidget()->setCompletionMode( KGlobalSettings::CompletionAuto );
    journeySearchLineEdit->nativeWidget()->setCompletionModeDisabled(
            KGlobalSettings::CompletionMan );
    journeySearchLineEdit->nativeWidget()->setCompletionModeDisabled(
            KGlobalSettings::CompletionPopup );
    journeySearchLineEdit->nativeWidget()->setCompletionModeDisabled(
            KGlobalSettings::CompletionPopupAuto );
    journeySearchLineEdit->nativeWidget()->setCompletionModeDisabled(
            KGlobalSettings::CompletionShell );
    journeySearchLineEdit->setEnabled( true );

    KLineEdit *journeySearch = journeySearchLineEdit->nativeWidget();
    journeySearch->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    journeySearch->setClickMessage( i18nc("@info/plain", "Target stop name or journey request") );
    KCompletion *completion = journeySearch->completionObject( false );
    completion->setIgnoreCase( true );
    journeySearchLineEdit->setFont( m_settings->sizedFont() );
    connect( journeySearchLineEdit, SIGNAL(returnPressed()),
             this, SLOT(slotJourneySearchInputFinished()) );
    connect( journeySearchLineEdit, SIGNAL(textEdited(QString)),
             this, SIGNAL(journeySearchInputEdited(QString)) );
    connect( journeySearchLineEdit, SIGNAL(textChanged(QString)),
             this, SLOT(slotJourneySearchInputChanged(QString)) );

    // Add widgets
    addWidget( journeySearchLineEdit, WidgetJourneySearchLine );
    addWidget( recentJourneysButton, WidgetFillJourneySearchLineButton );
    addWidget( journeySearchButton, WidgetStartJourneySearchButton );
}

void TitleWidget::slotFilterIconClicked()
{
    KActionMenu *filtersAction = qobject_cast<KActionMenu*>( m_filtersAction );
    Q_ASSERT( filtersAction );

    // Show the journeys menu under the journey search icon
    filtersAction->menu()->exec( QCursor::pos() );
}

void TitleWidget::slotJourneysIconClicked()
{
    KActionMenu *journeysAction = qobject_cast<KActionMenu*>( m_journeysAction );
    Q_ASSERT( journeysAction );

    // Show the journeys menu under the journey search icon
    journeysAction->menu()->exec( QCursor::pos() );
}

void TitleWidget::setJourneySearch( const QString &journeySearch )
{
    Plasma::LineEdit* journeySearchLine = castedWidget<Plasma::LineEdit>( WidgetJourneySearchLine );
    if ( journeySearchLine ) {
        journeySearchLine->setText( journeySearch );
        journeySearchLine->setFocus();
    }
}

void TitleWidget::removeJourneySearchWidgets()
{
    removeWidget( WidgetStartJourneySearchButton );
    removeWidget( WidgetJourneySearchLine );
    removeWidget( WidgetFillJourneySearchLineButton );
}

void TitleWidget::setTitle( const QString &title )
{
    m_titleText = title;
    updateTitle();
}

void TitleWidget::setIcon( Plasma::IconWidget *icon )
{
    if ( m_icon ) {
        m_layout->removeItem(m_icon);
        delete m_icon;
    }

    m_icon = icon;
    connect( icon, SIGNAL(clicked()), this, SIGNAL(iconClicked()) );
    m_layout->insertItem( 0, m_icon );
}

void TitleWidget::setIcon( MainIconDisplay iconType )
{
    Q_ASSERT( m_icon );

    KIcon icon;
    KIconEffect iconEffect;
    QPixmap pixmap;
    int iconExtend = m_icon->size().width();

    // Create an icon of the given type.
    switch ( iconType ) {
    case DepartureListErrorIcon: {
        // Create an icon to be shown on errors with a departure/arrival board
        QList<KIcon> overlays;
        if ( m_settings->departureArrivalListType() == DepartureList ) {
            // Use a public transport stop icon with a house and an arrow away from it
            // to indicate that a departure list is shown
            overlays << KIcon("go-home") << KIcon("go-next");
        } else {
            // Use a public transport stop icon with a house and an arrow towards it
            // to indicate that an arrival list is shown
            overlays << KIcon("go-next") << KIcon("go-home");
        }
        icon = GlobalApplet::makeOverlayIcon( KIcon("public-transport-stop"),
                overlays, QSize(iconExtend / 2, iconExtend / 2), iconExtend );
        pixmap = icon.pixmap( iconExtend );
        pixmap = iconEffect.apply( pixmap, KIconLoader::Small, KIconLoader::DisabledState );
        icon = KIcon();
        icon.addPixmap( pixmap, QIcon::Normal );
        break;
    }
    case DepartureListOkIcon: {
        // Create an icon to be shown for departure/arrival boards without errors.
        // This icon is the same as the departure error icon, but without the icon effect
        QList<KIcon> overlays;
        if ( m_settings->departureArrivalListType() == DepartureList ) {
            overlays << KIcon("go-home") << KIcon("go-next");
        } else {
            overlays << KIcon("go-next") << KIcon("go-home");
        }
        icon = GlobalApplet::makeOverlayIcon( KIcon("public-transport-stop"), overlays,
                QSize( iconExtend / 2, iconExtend / 2 ), iconExtend );
        break;
    }
    case JourneyListOkIcon:
        // Create an icon to be shown for journey lists without errors.
        // This icon is the same as the journey error icon, but without the icon effect
        icon = GlobalApplet::makeOverlayIcon( KIcon("public-transport-stop"),
                QList<KIcon>() << KIcon("go-home")
                    << KIcon("go-next-view") << KIcon("public-transport-stop"),
                QSize(iconExtend / 3, iconExtend / 3), iconExtend );
        break;

    case JourneyListErrorIcon:
        // Create an icon to be shown on errors with a journey list
        icon = GlobalApplet::makeOverlayIcon( KIcon("public-transport-stop"),
                QList<KIcon>() << KIcon("go-home")
                    << KIcon("go-next-view") << KIcon("public-transport-stop"),
                QSize(iconExtend / 3, iconExtend / 3), iconExtend );
        pixmap = icon.pixmap( iconExtend );
        pixmap = iconEffect.apply( pixmap, KIconLoader::Small, KIconLoader::DisabledState );
        icon = KIcon();
        icon.addPixmap( pixmap, QIcon::Normal );
        break;

    case AbortJourneySearchIcon:
        // Create an icon to be shown for the journey search
        icon = KIcon("edit-delete");
        break;

    case GoBackIcon:
        // Create an icon to be used for "go back" functionality
        icon = KIcon("arrow-left");
        break;
    }

    m_icon->setIcon( icon );
}

QGraphicsWidget* TitleWidget::createAndAddWidget( TitleWidget::WidgetType widgetType )
{
    switch ( widgetType ) {
    case WidgetFilter:
        if ( !m_filterWidget ) {
            // Create the filter widget showing the currently active filters
            m_filterWidget = new Plasma::ToolButton( this );
            m_filterWidget->setIcon( KIcon("view-filter") );
            m_filterWidget->setToolTip( i18nc("@info:tooltip",
                    "Shows a menu that allows to toggle filters / color groups") );
            m_filterWidget->nativeWidget()->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
            connect( m_filterWidget, SIGNAL(clicked()), this, SLOT(slotFilterIconClicked()) );
            addWidget( m_filterWidget, WidgetFilter );
        }

        updateFilterWidget();
        return m_filterWidget;

    case WidgetQuickJourneySearch:
        if ( !m_journeysWidget ) {
            // Create the filter widget showing the currently active filters
            m_journeysWidget = new Plasma::ToolButton( this );
            m_journeysWidget->setIcon( KIcon("edit-find") );
            m_journeysWidget->setText( i18nc("@action:button", "Quick Journey Search") );
            m_journeysWidget->setToolTip( i18nc("@info:tooltip",
                    "Shows a menu with favorite/recent journey search items") );
            m_journeysWidget->nativeWidget()->setToolButtonStyle( Qt::ToolButtonIconOnly );
            m_journeysWidget->setMaximumWidth( m_journeysWidget->size().height() );
            connect( m_journeysWidget, SIGNAL(clicked()),
                     this, SLOT(slotJourneysIconClicked()) );
            addWidget( m_journeysWidget, WidgetQuickJourneySearch );
        }

        return m_journeysWidget;

    default:
        Q_ASSERT( false );
        return 0;
    }
}

void TitleWidget::addWidget( QGraphicsWidget *widget, WidgetType widgetType )
{
    if ( m_widgets.contains(widgetType) ) {
        // Widget is already created and in the widget list
        widget->show();
        return;
    }

    if ( widgetType == WidgetTitle ) {
        m_title = qgraphicsitem_cast<Plasma::Label*>( widget );
        m_layout->insertItem( 1, widget );
    } else if ( widgetType == WidgetQuickJourneySearch && m_filterWidget ) {
        m_layout->insertItem( 2, widget );
        m_layout->setAlignment( widget, Qt::AlignVCenter | Qt::AlignLeft );
    } else {
        m_layout->addItem( widget );
        m_layout->setAlignment( widget, Qt::AlignVCenter | Qt::AlignLeft );
    }
    m_widgets.insert( widgetType, widget );
    widget->show();
}

bool TitleWidget::removeWidget( TitleWidget::WidgetType widgetType, RemoveWidgetOptions options )
{
    if ( !m_widgets.contains(widgetType) ) {
        return false;
    }

    if ( widgetType == WidgetFilter || widgetType == WidgetQuickJourneySearch ||
         widgetType == WidgetTitle )
    {
        // Don't delete widgets that are also stored in a member variable
        // (m_filterWidget, m_journeySearchWidget, m_title)
        options ^= DeleteWidget;
        options |= HideAndRemoveWidget;
    }

    QGraphicsWidget *widget;
    if ( options.testFlag(RemoveWidget) || options.testFlag(DeleteWidget) ) {
        widget = m_widgets.take(widgetType);
        m_layout->removeItem(widget);
    } else {
        widget = m_widgets[widgetType];
    }
    if ( !widget ) {
        // A null value is stored in m_widgets, can happen if a default value gets constructed
        kDebug() << "Null value stored for widget type" << widgetType << "This can happen if a "
                "default value gets constructed for that widget type, ie. when a widget of that "
                "type gets requested from TitleWidget::m_widgets without checking if it is contained.";
        m_widgets.remove( widgetType );
        return false;
    }

    if ( options == DeleteWidget ) {
        widget->deleteLater();
    } else if ( options.testFlag(HideWidget) ) {
        widget->hide();
    }
    return true;
}

void TitleWidget::clearWidgets()
{
    while ( !m_widgets.isEmpty() ) {
        removeWidget( m_widgets.keys().first() );
    }
}

void TitleWidget::updateFilterWidget()
{
    FilterSettingsList filters = m_settings->currentFilters();
    ColorGroupSettingsList colorGroups = m_settings->currentColorGroups();
    ColorGroupSettingsList disabledColorGroups;
    foreach ( const ColorGroupSettings &colorGroup, colorGroups ) {
        if ( colorGroup.filterOut ) {
            disabledColorGroups << colorGroup;
        }
    }
    if ( filters.isEmpty() && disabledColorGroups.isEmpty() ) {
        m_filterWidget->setOpacity( 0.6 );
        m_filterWidget->setText( i18nc("@info/plain Shown in the applet to indicate that no "
                "filters are currently active", "(No active filter)") );
        // Do not show any text, if no filter is active
        m_filterWidget->nativeWidget()->setToolButtonStyle( Qt::ToolButtonIconOnly );
        m_filterWidget->setIcon( KIcon("view-filter") );
    } else {
        QFontMetrics fm( m_filterWidget->font() );
        QString text;
        m_filterWidget->nativeWidget()->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
        if ( filters.count() == 1 && disabledColorGroups.isEmpty() ) {
            text = fm.elidedText( filters.first().name,
                                  Qt::ElideRight, boundingRect().width() * 0.45 );
            m_filterWidget->setIcon( KIcon("view-filter") );
        } else if ( filters.count() > 1 && disabledColorGroups.isEmpty() ) {
            text = fm.elidedText( i18ncp("@info/plain", "%1 active filter", "%1 active filters",
                                         filters.count()),
                                  Qt::ElideRight, boundingRect().width() * 0.45 );
            m_filterWidget->setIcon( KIcon("object-group") );
        } else if ( filters.isEmpty() && disabledColorGroups.count() >= 1 ) {
            text = fm.elidedText( i18ncp("@info/plain", "%1 disabled color group",
                                         "%1 disabled color groups", disabledColorGroups.count()),
                                  Qt::ElideRight, boundingRect().width() * 0.45 );
            m_filterWidget->setIcon( KIcon("object-group") );
        } else {
            text = fm.elidedText( i18ncp("@info/plain", "%1 active (color) filter",
                                         "%1 active (color) filters",
                                         filters.count() + disabledColorGroups.count()),
                                  Qt::ElideRight, boundingRect().width() * 0.45 );
            m_filterWidget->setIcon( KIcon("view-filter") );
        }

        m_filterWidget->setOpacity( 1 );
        m_filterWidget->setText( text );
    }
}

void TitleWidget::slotJourneySearchInputChanged( const QString &text )
{
    // Disable start search button if the journey search line is empty
    Plasma::ToolButton *startJourneySearchButton =
            castedWidget<Plasma::ToolButton>( WidgetStartJourneySearchButton );
    if ( startJourneySearchButton ) {
        startJourneySearchButton->setEnabled( !text.isEmpty() );
    }
}

void TitleWidget::settingsChanged()
{
    int mainIconExtend = qCeil(26 * m_settings->sizeFactor());
    m_icon->setMinimumSize( mainIconExtend, mainIconExtend );
    m_icon->setMaximumSize( mainIconExtend, mainIconExtend );

    QFont font = m_settings->sizedFont();
    QFont boldFont = font;
    boldFont.setBold( true );
    m_title->setFont( boldFont );

    if ( m_filterWidget ) {
        m_filterWidget->setFont( font );
    }
    if ( m_journeysWidget ) {
        m_journeysWidget->setFont( font );
    }

    if ( m_type == ShowDepartureArrivalListTitle
      || m_type == ShowIntermediateDepartureListTitle )
    {
        setTitle( titleText() );
    }
}

void TitleWidget::resizeEvent( QGraphicsSceneResizeEvent *event )
{
    QGraphicsWidget::resizeEvent( event );
    updateTitle();
}

void TitleWidget::updateTitle()
{
    QFontMetrics fm( m_title->font() );
    qreal maxStopNameWidth = contentsRect().width() - m_icon->boundingRect().right() - 10;
    if ( m_filterWidget ) {
        maxStopNameWidth -= m_filterWidget->boundingRect().width();
    }
    if ( m_journeysWidget ) {
        maxStopNameWidth -= m_journeysWidget->boundingRect().width();
    }

    if ( !m_titleText.contains(QRegExp("<\\/?[^>]+>")) ) {
        m_title->setText( fm.elidedText(m_titleText, Qt::ElideRight, maxStopNameWidth * 2.0) );
    } else {
        m_title->setText( m_titleText );
    }
}
