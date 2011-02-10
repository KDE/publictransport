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

/** @file
 * @brief This file contains the widget that is used for the title of the public transport applet.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TITLEWIDGET_H
#define TITLEWIDGET_H

#include "global.h"

#include <QGraphicsWidget>

class Settings;
class QGraphicsLinearLayout;
namespace Plasma {
	class IconWidget;
	class Label;
	class ToolButton;
}

/**
 * @brief Represents the widget used as title for the publictransport applet.
 *
 * It uses a linear horizontal layout. The first item is always a Plasma::IconWidget. After that
 * a title widget is shown (Plasma::Label). The title can easily be changed using @ref setTitle.
 * Additional widgets are added using @ref addWidget and removed with @ref removeWidget.
 * You can also clear all additional widgets using @ref clearWidgets.
 * The widget can switch between different layouts (different sets of widgets shown) using
 * @ref setTitleType.
 *
 * @since 0.9.1
 **/
class TitleWidget : public QGraphicsWidget
{
	Q_OBJECT

public:
	/**
	 * @brief The types of additional widgets.
	 * @note The main icon widget isn't contained in this list and handled separately, because
	 *   it's always visible.
	 **/
	enum WidgetType {
		WidgetTitle = 0x0001, /**< The title widget, a Plasma::Label. */
		WidgetFilter = 0x0002, /**< The filter widget, a QGraphicsWidget, that contains
				* a Plasma::IconWidget and a Plasma::Label. */
		WidgetJourneySearchLine = 0x0010, /**< The journey search edit box, a Plasma::LineEdit. */
		WidgetRecentJourneysButton = 0x0020, /**< The button to show actions for recent journeys,
				* a Plasma::ToolButton. @see RecentJourneyAction */
		WidgetJourneySearchButton = 0x0040, /**< The button to start a journey search,
				* a Plasma::ToolButton. */
		WidgetCloseIcon = 0x0080, /**< The icon to close the current view (eg. the journey search),
				* a Plasma::IconWidget. */
		WidgetUser = 0x1000 /**< The first unused number in the Widget enum. Can be used for
				* custom types (should be of type QGraphicsWidget). */
	};
	Q_DECLARE_FLAGS(WidgetTypes, WidgetType)

	/**
	 * @brief Options for removing widgets.
	 * @see removeWidget
	 **/
	enum RemoveWidgetOption {
		DeleteWidget = 0x0000, /**< Delete and remove the widget. */
		HideWidget = 0x0001, /**< Hide the widget. */
		RemoveWidget = 0x0002, /**< Only remove the widget without deleting or hiding it. */
		HideAndRemoveWidget = HideWidget | RemoveWidget /**< Combination of HideWidget and RemoveWidget. */
	};
	Q_DECLARE_FLAGS(RemoveWidgetOptions, RemoveWidgetOption)

	/**
	 * @brief The types of actions associated with recent journeys.
	 **/
	enum RecentJourneyAction {
		ActionClearRecentJourneys = 0, /**< The action to clear the list of recent journeys. */
		ActionUseRecentJourney = 1 /**< An action to use a specific recent journey. */
	};

	/**
	 * @brief Creates a new title widget.
	 *
	 * @param titleType The initial type of this title widget.
	 * @param settings A pointer to the settings object of the applet.
	 * @param parent The parent QGraphicsItem. Defaults to 0.
	 **/
	TitleWidget(TitleType titleType, Settings *settings, QGraphicsItem* parent = 0);

	/** @brief Gets the current type of this title widget. */
	TitleType titleType() const { return m_type; };
	/** @brief Gets the main icon widget. */
	Plasma::IconWidget* icon() const { return m_icon; };
	/** @brief Gets the title string. */
	QString title() const;
	/** @brief Gets the title widget. */
	Plasma::Label *titleWidget() const;
	/** @brief Gets additional widgets used for the current @ref titleType. */
	QList<QGraphicsWidget*> widgets() const { return m_widgets.values(); };

	/**
	 * @brief Returns the additional widget at @p index, which is of type T*.
	 *
	 * @param index The index of the additional widget to get.
	 * @return The additional widget at @p index, casted to type T*. It uses q_objectcast,
	 *   so it returns NULL, if the widget isn't of the given type.
	 **/
	template <typename T>
	T* castedWidget( WidgetType widgetType ) const;

	/**
	 * @brief Sets the type of this title widget to @p titleType.
	 *
	 * @param titleType The new title type.
	 * @param appletStates The current states of the applet.
	 **/
	void setTitleType( TitleType titleType, AppletStates appletStates );

	/**
	 * @brief Sets the title.
	 *
	 * @param title The new title string.
	 **/
	void setTitle( const QString &title );
	/**
	 * @brief Changes the icon of the icon widget to the given @p iconType.
	 *
	 * @param iconType The new icon type for the icon widget.
	 **/
	void setIcon( MainIconDisplay iconType );

	/** @brief Adds a new @p widget of the given @p widgetType to this title item. */
	void addWidget( QGraphicsWidget *widget, WidgetType widgetType );
	/** @brief Removes the widget of the given @p widgetType.
	 *
	 * @param widgetType The type of the widget to be removed.
	 * @param options Options for removing the widget, ie. whether or not to delete the widget
	 *   after removing it from the layout. Widget of type @ref WidgetFilter and @ref WidgetTitle
	 *   will never be deleted.
	 * @returns True on success. Otherwise, false (eg. if there is no widget of the
	 *   given @p widgetType). */
	bool removeWidget( WidgetType widgetType, RemoveWidgetOptions options = DeleteWidget );
	/** @brief Removes and deletes all additional widgets, ie. not the icon and title widgets. */
	void clearWidgets();
	QGraphicsWidget *createAndAddWidget( WidgetType widgetType );

signals:
	/** @brief The icon widget was clicked. */
	void iconClicked();

	/** @brief The widget in the additional widget list with type @ref WidgetCloseIcon was clicked. */
	void closeIconClicked();

	/** @brief The widget in the additional widget list with type @ref WidgetFilter was clicked. */
	void filterIconClicked();

	/** @brief The widget of type @ref WidgetJourneySearchButton was clicked or enter was pressed
	 * in widget of type @ref WidgetJourneySearchLine. */
	void journeySearchInputFinished();

	/** @brief The text of the widget of type @ref WidgetJourneySearchLine has changed.
	 *
	 * @param text The new text. */
	void journeySearchInputEdited( const QString &text );

	/**
	 * @brief An action of the recent journey menu has been triggered.
	 *
	 * @param recentJourneyAction The type of the triggered action.
	 * @param recentJourney The search string of the triggered recent journey action,
	 *   if @p recentJourneyAction is @ref ActionUseRecentJourney. Otherwise an empty QString.
	 *
	 * @note If @p recentJourneyAction is @ref ActionClearRecentJourneys, the settings object
	 *   should be updated to execute the action. On @ref ActionUseRecentJourney the action
	 *   is automatically executed by setting the search string to @p recentJourney.
	 **/
	void recentJourneyActionTriggered( TitleWidget::RecentJourneyAction recentJourneyAction,
									   const QString &recentJourney = QString() );

public slots:
	/** @brief Updates the filter widget based on the current applet settings. */
	void updateFilterWidget();
	/** @brief Updates the recent journeys menu based on the current applet settigns. */
	void updateRecentJourneysMenu();
	/** @brief Call this when the applet settings have changed. */
	void settingsChanged();

protected slots:
	void slotRecentJourneyActionTriggered( QAction *action );
	void slotJourneySearchInputChanged( const QString &text );

protected:
	/**
	 * @brief Sets the icon widget and deletes the old one.
	 *
	 * @param icon The new icon widget.
	 **/
	void setIcon( Plasma::IconWidget *icon );

	/** @brief Adds widgets used for the journey search title type. */
	void addJourneySearchWidgets();
	/** @brief Remove widgets used for the journey search title type. */
	void removeJourneySearchWidgets();
	/** @brief Gets a new title string based on the current applet settings. */
	QString titleText() const;

	TitleType m_type;
	Plasma::IconWidget *m_icon;
	Plasma::Label *m_title;
	Plasma::ToolButton *m_filterWidget;
	QHash< WidgetType, QGraphicsWidget* > m_widgets;
	QGraphicsLinearLayout *m_layout;
	Settings *m_settings;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(TitleWidget::WidgetTypes)
Q_DECLARE_OPERATORS_FOR_FLAGS(TitleWidget::RemoveWidgetOptions)

#endif // TITLEWIDGET_H
