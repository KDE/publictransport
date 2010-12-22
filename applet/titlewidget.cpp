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

#include "titlewidget.h"

#include "settings.h"
#include "journeysearchlineedit.h"

#include <QGraphicsLinearLayout>
#include <KIconEffect>
#include <Plasma/Label>
#include <Plasma/IconWidget>
#include <Plasma/LineEdit>
#include <Plasma/ToolButton>
#include <QMenu>

TitleWidget::TitleWidget(TitleType titleType, Settings *settings, QGraphicsItem* parent)
		: QGraphicsWidget(parent), m_icon(0), m_filterWidget(0),
			m_layout(new QGraphicsLinearLayout(Qt::Horizontal, this)), m_settings(settings)
{
	m_type = titleType;

	int iconExtend = 32 * settings->sizeFactor;
	Plasma::IconWidget *icon = new Plasma::IconWidget;
	icon->setIcon( "public-transport-stop" );
	icon->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
	icon->setMinimumSize( iconExtend, iconExtend );
	icon->setMaximumSize( iconExtend, iconExtend );
	setIcon( icon );

	Plasma::Label *title = new Plasma::Label( this );
	title->setAlignment( Qt::AlignVCenter | Qt::AlignLeft );
	QLabel *_title = title->nativeWidget();
	_title->setTextInteractionFlags( Qt::LinksAccessibleByMouse );
// 	m_layout->addItem( m_title );
	addWidget( title, WidgetTitle );

	createAndAddWidget( WidgetFilter );
}

template <typename T>
T* TitleWidget::castedWidget(WidgetType widgetType) const
{
	return qobject_cast<T*>( m_widgets[widgetType] );
}

QString TitleWidget::title() const
{
	return m_title->text();
}

Plasma::Label* TitleWidget::titleWidget() const
{
	return m_title;
}

void TitleWidget::setTitleType(TitleType titleType, AppletStates appletStates)
{
	kDebug() << titleType << appletStates;

	// Remove old additional widgets
	clearWidgets();

	// New type
	m_type = titleType;
	switch ( titleType ) {
		case ShowDepartureArrivalListTitle:
			setIcon( appletStates.testFlag(ReceivedValidDepartureData)
					? DepartureListOkIcon : DepartureListErrorIcon );
			m_icon->setToolTip( i18nc("@info:tooltip", "Search journeys to or from the home stop") );
			setTitle( titleText() );

// 			m_title->show();
			addWidget(m_title, WidgetTitle);
			m_icon->show(); // TEST is this needed?
			addWidget( m_filterWidget, WidgetFilter );

// 			m_titleInfo->setToolTip( courtesyToolTip() );
// 			m_titleInfo->setText( infoText() );
// 			widget = m_treeView;
			break;

		case ShowSearchJourneyLineEdit: {
			setIcon( AbortJourneySearchIcon );
			m_icon->setToolTip( i18nc("@info:tooltip", "Abort search for journeys "
													   "to or from the home stop" ) );

// 			m_title->hide();
			removeWidget( WidgetTitle, HideAndRemoveWidget );
			m_icon->show(); // TEST is this needed?
			addJourneySearchWidgets();
			Plasma::LineEdit *journeySearchLine = castedWidget<Plasma::LineEdit>(WidgetJourneySearchLine);
			journeySearchLine->setEnabled( true );
			journeySearchLine->setFocus();
			journeySearchLine->nativeWidget()->selectAll();

			updateRecentJourneysMenu();

// 			m_listStopSuggestions = new Plasma::TreeView();
// 			m_listStopSuggestions->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
// 			QTreeView *treeView = m_listStopSuggestions->nativeWidget();
// 			treeView->setRootIsDecorated( false );
// 			treeView->setHeaderHidden( true );
// 			treeView->setAlternatingRowColors( true );
// 			treeView->setEditTriggers( QAbstractItemView::NoEditTriggers );
// 			treeView->setAutoFillBackground( false );
// 			treeView->setAttribute( Qt::WA_NoSystemBackground );
// 			treeView->setItemDelegate( new PublicTransportDelegate( m_listStopSuggestions ) );
// 			treeView->setPalette( m_treeView->nativeWidget()->palette() );
// 			m_listStopSuggestions->setFont( m_settings.sizedFont() );

// 			addJourneySearchCompletions();

// 			connect( treeView, SIGNAL(clicked(QModelIndex)),
// 					 this, SLOT(possibleStopClicked(QModelIndex)) );
// 			connect( treeView, SIGNAL(doubleClicked(QModelIndex)),
// 					 this, SLOT(possibleStopDoubleClicked(QModelIndex)) );
// 			widget = m_listStopSuggestions;
			break;
		}
		case ShowSearchJourneyLineEditDisabled:
			setIcon( AbortJourneySearchIcon );
			m_icon->setToolTip( i18nc("@info:tooltip", "Abort search for journeys "
													   "to or from the home stop") );
			m_icon->show(); // TEST is this needed?

			addJourneySearchWidgets();
// 			m_labelJourneysNotSupported = new Plasma::Label;
// 			m_labelJourneysNotSupported->setAlignment( Qt::AlignCenter );
// 			m_labelJourneysNotSupported->setSizePolicy( QSizePolicy::Expanding,
// 														QSizePolicy::Expanding, QSizePolicy::Label );
// 			m_labelJourneysNotSupported->setText( i18nc( "@info/plain",
// 														 "Journey searches aren't supported by the currently used "
// 														 "service provider or it's accessor." ) );
// 			m_labelJourneysNotSupported->nativeWidget()->setWordWrap( true );

			castedWidget<Plasma::LineEdit>(WidgetJourneySearchLine)->setEnabled( false );
			castedWidget<Plasma::LineEdit>(WidgetRecentJourneysButton)->setEnabled( false );
			castedWidget<Plasma::LineEdit>(WidgetJourneySearchButton)->setEnabled( false );
// 			widget = m_labelJourneysNotSupported;
			break;

		case ShowJourneyListTitle: {
			setIcon( appletStates.testFlag(ReceivedValidJourneyData)
					? JourneyListOkIcon : JourneyListErrorIcon );
			m_icon->setToolTip( i18nc("@info:tooltip", "Search journeys to or from the home stop") );
			// 		m_title->setText( m_journeyTitleText.isEmpty()
			// 		? i18nc( "@info", "<emphasis strong='1'>Journeys</emphasis>" )
			// 		: m_journeyTitleText );
// 			setTitle( m_journeyTitleText.isEmpty()
// 					? i18nc( "@info", "<emphasis strong='1'>Journeys</emphasis>" )
// 					: m_journeyTitleText ); // TODO

			int iconExtend = 32 * m_settings->sizeFactor;
			Plasma::IconWidget *closeIcon = new Plasma::IconWidget;
			closeIcon->setIcon( "window-close" );
			closeIcon->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
			closeIcon->setMinimumSize( iconExtend, iconExtend );
			closeIcon->setMaximumSize( iconExtend, iconExtend );
			closeIcon->setToolTip( i18nc("@info:tooltip", "Show departures / arrivals") );
			connect( closeIcon, SIGNAL(clicked()), this, SIGNAL(closeIconClicked()) );
			addWidget( closeIcon, WidgetCloseIcon );

			m_icon->show(); // TEST is this needed?
			addWidget(m_title, WidgetTitle);

// 			m_treeViewJourney = new Plasma::TreeView;
// 			initTreeView( m_treeViewJourney );
// 			QTreeView *treeViewJourneys = m_treeViewJourney->nativeWidget();
// 			treeViewJourneys->setPalette( m_treeView->nativeWidget()->palette() );
// 			treeViewJourneys->header()->setPalette( m_treeView->nativeWidget()->header()->palette() );
// 			treeViewJourneys->setIconSize( QSize( 32 * m_settings.sizeFactor, 32 * m_settings.sizeFactor ) );
// 			m_treeViewJourney->setModel( m_modelJourneys );
// 			m_treeViewJourney->setFont( m_settings.sizedFont() );
// 			treeViewJourneys->header()->resizeSection( 1, 120 );

// 			widget = m_treeViewJourney;
			break;
		}
	}
}

QString TitleWidget::titleText() const
{
	QString sStops = m_settings->currentStopSettings().stops.join( ", " );
	if ( !m_settings->currentStopSettings().city.isEmpty() ) {
		return QString( "%1, %2" ).arg( sStops ).arg( m_settings->currentStopSettings().city );
	} else {
		return QString( "%1" ).arg( sStops );
	}
}

void TitleWidget::addJourneySearchWidgets()
{
	kDebug() << "";
	// Add recent journeys button
	Plasma::ToolButton *recentJourneysButton = new Plasma::ToolButton;
	recentJourneysButton->setIcon( KIcon("document-open-recent") );
	recentJourneysButton->setToolTip( i18nc("@info:tooltip", "Use a recent journey search") );
	recentJourneysButton->nativeWidget()->setPopupMode( QToolButton::InstantPopup );
	// This is needed, to have the popup menu drawn above other widgets
	recentJourneysButton->setZValue( 999 );

	// Add button to start the journey search
	Plasma::ToolButton *journeySearchButton = new Plasma::ToolButton;
	journeySearchButton->setIcon( KIcon("edit-find") );
	journeySearchButton->setToolTip( i18nc("@info:tooltip", "Find journeys") );
	journeySearchButton->setEnabled( false );
	connect( journeySearchButton, SIGNAL(clicked()), this, SIGNAL(journeySearchInputFinished()) );

	// Add journey search query input field
	Plasma::LineEdit *journeySearchLineEdit = new Plasma::LineEdit;
	#if KDE_VERSION >= KDE_MAKE_VERSION(4,4,0)
		journeySearchLineEdit->setNativeWidget( new JourneySearchLineEdit );
	#endif
	journeySearchLineEdit->setToolTip( i18nc("@info:tooltip This should match the localized keywords.",
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
			 this, SIGNAL(journeySearchInputFinished()) );
	connect( journeySearchLineEdit, SIGNAL(textEdited(QString)),
			 this, SIGNAL(journeySearchInputEdited(QString)) );
	connect( journeySearchLineEdit, SIGNAL(textChanged(QString)),
			 this, SLOT(slotJourneySearchInputChanged(QString)) );

	// Add widgets
	addWidget( journeySearchLineEdit, WidgetJourneySearchLine );
	addWidget( recentJourneysButton, WidgetRecentJourneysButton );
	addWidget( journeySearchButton, WidgetJourneySearchButton );
}

void TitleWidget::removeJourneySearchWidgets()
{
	removeWidget( WidgetJourneySearchButton );
	removeWidget( WidgetJourneySearchLine );
	removeWidget( WidgetRecentJourneysButton );
}

void TitleWidget::setTitle(const QString& title)
{
	m_title->setText( title );
}

void TitleWidget::setIcon(Plasma::IconWidget* icon)
{
	if ( m_icon ) {
		m_layout->removeItem(m_icon);
		delete m_icon;
	}

	m_icon = icon;
	connect( icon, SIGNAL(clicked()), this, SIGNAL(iconClicked()) );
	m_layout->insertItem( 0, m_icon );
}

void TitleWidget::setIcon(MainIconDisplay iconType)
{
	Q_ASSERT( m_icon );

	KIcon icon;
	KIconEffect iconEffect;
	QPixmap pixmap;
	int iconExtend = m_icon->size().width();

	// Make disabled icon
	switch ( iconType ) {
		case DepartureListErrorIcon:
			if ( m_settings->departureArrivalListType == DepartureList ) {
				icon = Global::makeOverlayIcon( KIcon("public-transport-stop"),
						QList<KIcon>() << KIcon("go-home") << KIcon("go-next"),
						QSize(iconExtend / 2, iconExtend / 2), iconExtend );
			} else {
				icon = Global::makeOverlayIcon( KIcon( "public-transport-stop" ),
						QList<KIcon>() << KIcon("go-next") << KIcon("go-home"),
						QSize(iconExtend / 2, iconExtend / 2), iconExtend );
			}
			pixmap = icon.pixmap( iconExtend );
			pixmap = iconEffect.apply( pixmap, KIconLoader::Small, KIconLoader::DisabledState );
			icon = KIcon();
			icon.addPixmap( pixmap, QIcon::Normal );
			break;

		case DepartureListOkIcon: {
			QList<KIcon> overlays;
			if ( m_settings->departureArrivalListType == DepartureList ) {
				overlays << KIcon("go-home") << KIcon("go-next");
			} else {
				overlays << KIcon("go-next") << KIcon("go-home");
			}
			icon = Global::makeOverlayIcon( KIcon( "public-transport-stop" ), overlays,
					QSize( iconExtend / 2, iconExtend / 2 ), iconExtend );
			break;
		}
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
			icon = KIcon("edit-delete");
			break;

		case GoBackIcon:
			icon = KIcon("arrow-left");
			break;
	}

	m_icon->setIcon( icon );
}

QGraphicsWidget* TitleWidget::createAndAddWidget(TitleWidget::WidgetType widgetType)
{
	switch ( widgetType ) {
	case WidgetFilter:
		if ( !m_filterWidget ) {
			// Create the filter widget showing the currently active filters
			m_filterWidget = new QGraphicsWidget( this );
			m_filterWidget->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Preferred );

			Plasma::IconWidget *filterIcon = new Plasma::IconWidget( m_filterWidget );
			filterIcon->setIcon( "view-filter" );
			int fitlerIconExtend = 24 * m_settings->sizeFactor;
			filterIcon->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
			filterIcon->setMinimumSize( fitlerIconExtend, fitlerIconExtend );
			filterIcon->setMaximumSize( fitlerIconExtend, fitlerIconExtend );

			Plasma::Label *filterLabel = new Plasma::Label( m_filterWidget );
			filterLabel->setMaximumWidth( 100 * m_settings->sizeFactor );
			filterLabel->setAlignment( Qt::AlignVCenter | Qt::AlignLeft );
			#if KDE_VERSION >= KDE_MAKE_VERSION(4,5,0)
				filterLabel->setWordWrap( true );
			#endif

			QGraphicsLinearLayout *filterLayout = new QGraphicsLinearLayout(
					Qt::Horizontal, m_filterWidget );
			filterLayout->setContentsMargins( 4, 0, 4, 0 );
			filterLayout->addItem( filterIcon );
			filterLayout->addItem( filterLabel );
			m_filterWidget->setLayout( filterLayout );

			connect( filterIcon, SIGNAL(clicked()), this, SIGNAL(filterIconClicked()) );
		}
		updateFilterWidget();
		addWidget( m_filterWidget, WidgetFilter );
		return m_filterWidget;

	default:
		Q_ASSERT(false);
		return NULL;
	}
}

void TitleWidget::addWidget(QGraphicsWidget* widget, WidgetType widgetType)
{
	if ( m_widgets.contains(widgetType) ) {
		widget->show();
		return;
	}

	if ( widgetType == WidgetTitle ) {
		m_title = qgraphicsitem_cast<Plasma::Label*>( widget );
		m_layout->insertItem( 1, widget );
	} else {
		m_layout->addItem( widget );
	}
	m_widgets.insert( widgetType, widget );
	widget->show();
}

bool TitleWidget::removeWidget(TitleWidget::WidgetType widgetType, RemoveWidgetOptions options)
{
	if ( !m_widgets.contains(widgetType) ) {
		return false;
	}

	if ( widgetType == WidgetFilter || widgetType == WidgetTitle ) {
		// Don't delete widgets that are also stored in a member variable (m_filterWidget, m_title)
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
	Q_ASSERT( widget );

	if ( options == DeleteWidget ) {
		widget->deleteLater();
	} else if ( options.testFlag(HideWidget) ) {
		widget->hide();
	}
	return true;
}

void TitleWidget::clearWidgets()
{
// 	for ( QHash<WidgetType, QGraphicsWidget*>::const_iterator it = m_widgets.constBegin();
// 			it != m_widgets.constEnd(); ++it )
	while ( !m_widgets.isEmpty() ) {
		removeWidget( m_widgets.keys().first() );
	}
}

void TitleWidget::updateFilterWidget()
{
	Plasma::Label *filterLabel = qgraphicsitem_cast<Plasma::Label*>(
			m_filterWidget->layout()->itemAt(1)->graphicsItem() );
	Q_ASSERT( filterLabel );
	if ( m_settings->filtersEnabled ) {
		m_filterWidget->setOpacity( 1 );
		QFontMetrics fm( m_filterWidget->font() );
		filterLabel->setText( fm.elidedText(SettingsUiManager::translateKey(
				m_settings->currentStopSettings().filterConfiguration),
				Qt::ElideRight, filterLabel->maximumWidth() * 1.8) );
	} else {
		m_filterWidget->setOpacity( 0.6 );
		filterLabel->setText( i18n("(No active filter)") );
	}
}

void TitleWidget::updateRecentJourneysMenu()
{
	Plasma::ToolButton *recentJourneysButton =
			castedWidget<Plasma::ToolButton>( WidgetRecentJourneysButton );
	Q_ASSERT( recentJourneysButton );

	if ( m_settings->recentJourneySearches.isEmpty() ) {
		recentJourneysButton->setEnabled( false );
	} else {
		QMenu *menu = new QMenu( recentJourneysButton->nativeWidget() );
		foreach( const QString &recent, m_settings->recentJourneySearches ) {
			menu->addAction( recent );
		}
		menu->addSeparator();
		menu->addAction( KIcon("edit-clear-list"),
						 i18nc("@action:button", "&Clear list") )->setData( true );
		connect( menu, SIGNAL(triggered(QAction*)),
				 this, SLOT(slotRecentJourneyActionTriggered(QAction*)) );
		recentJourneysButton->nativeWidget()->setMenu( menu );
		recentJourneysButton->setEnabled( true );
	}
}

void TitleWidget::slotRecentJourneyActionTriggered(QAction* action)
{
	Plasma::LineEdit* journeySearchLine = castedWidget<Plasma::LineEdit>( WidgetJourneySearchLine );

	if ( action->data().isValid() && action->data().toBool() ) {
		// First let the settings object be updated, the update the menu based on the new settings
		emit recentJourneyActionTriggered( ActionClearRecentJourneys );
		updateRecentJourneysMenu();
	} else {
		journeySearchLine->setText( action->text() );
		emit recentJourneyActionTriggered( ActionClearRecentJourneys, action->text() );
	}

	journeySearchLine->setFocus();
}

void TitleWidget::slotJourneySearchInputChanged(const QString& text)
{
	// Disable start search button if the journey search line is empty
	castedWidget<Plasma::ToolButton>(WidgetJourneySearchButton)->setEnabled( !text.isEmpty() );
}

void TitleWidget::settingsChanged()
{
	int mainIconExtend = 32 * m_settings->sizeFactor;
	m_icon->setMinimumSize( mainIconExtend, mainIconExtend );
	m_icon->setMaximumSize( mainIconExtend, mainIconExtend );

	QFont font = m_settings->sizedFont();
	QFont boldFont = font;
	boldFont.setBold( true );
	m_title->setFont( boldFont );

	if ( m_filterWidget ) {
		Plasma::Label *filterLabel = qgraphicsitem_cast<Plasma::Label*>(
			m_filterWidget->layout()->itemAt(1)->graphicsItem() );
		if ( filterLabel ) {
			filterLabel->setFont( font ); // TEST
		}
	}

	if ( m_type == ShowDepartureArrivalListTitle ) {
		setTitle( titleText() );
	}
}
