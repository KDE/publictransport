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

/** @file
 * @brief This file contains the TitleWidget class that is used for the title of the public transport applet.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TITLEWIDGET_H
#define TITLEWIDGET_H

// Own includes
#include "global.h"
#include "journeysearchitem.h"

// Qt includes
#include <QGraphicsWidget>

class JourneySearchModel;
class KMenu;
class Settings;
class QGraphicsLinearLayout;
class QMenu;
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
        WidgetFilter = 0x0002, /**< The filter widget, a Plasma::ToolButton. */
        WidgetQuickJourneySearch = 0x0004, /**< The quick journey search button,
                * a Plasma::ToolButton. */
        WidgetJourneySearchLine = 0x0010, /**< The journey search edit box, a Plasma::LineEdit. */
        WidgetFillJourneySearchLineButton = 0x0020, /**< The button to fill the journey search
                * line with a favorite/recent journey search, a Plasma::ToolButton. */
        WidgetStartJourneySearchButton = 0x0040, /**< The button to start a journey search,
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
     * @brief Creates a new title widget.
     *
     * @param titleType The initial type of this title widget.
     * @param settings A pointer to the settings object of the applet.
     * @param journeysSupported Whether or not journeys are supported by the current
     *   service provider.
     * @param parent The parent QGraphicsItem. Defaults to 0.
     **/
    TitleWidget( TitleType titleType, Settings *settings, bool journeysSupported = false,
                 QGraphicsItem* parent = 0 );

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
     * @param widgetType The type of the additional widget to get.
     * @return The additional widget of type @p widgetType, casted to type T*. It uses
     *   qgraphicsitem_cast, so it returns 0, if the widget isn't of the given type.
     **/
    template <typename T>
    T* castedWidget( WidgetType widgetType ) const;

    /**
     * @brief Sets the type of this title widget to @p titleType.
     *
     * @param titleType The new title type.
     * @param validDepartureData Whether or not valid departure data has been received.
     * @param validJourneyData Whether or not valid journey data has been received.
     **/
    void setTitleType( TitleType titleType, bool validDepartureData = false,
                       bool validJourneyData = false );

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

    /**
     * @brief Sets the filter action to use for the filter widget.
     *
     * It is expected that @p filterAction is of type KActionMenu, it's menu gets used.
     **/
    void setFiltersAction( QAction *filtersAction ) {
        m_filtersAction = filtersAction;
    };

    /**
     * @brief Sets the journey action to use for the journey widget.
     *
     * It is expected that @p journeysAction is of type KActionMenu, it's menu gets used.
     **/
    void setJourneysAction( QAction *journeysAction ) {
        m_journeysAction = journeysAction;
    };

    /**
     * @brief Creates/deletes widgets for journeys.
     *
     * If @p supported is false, all widgets associated with journey functionality will not be used.
     * @param supported Whether or not journeys are supported. Default is true.
     **/
    void setJourneysSupported( bool supported = true );

    /**
     * @brief Sets the current journey search input to @p journeySearch.
     *
     * This only works when in journey search mode. Otherwise this function does nothing.
     * @param journeySearch The new journey search string.
     **/
    void setJourneySearch( const QString &journeySearch );

signals:
    /** @brief The icon widget was clicked. */
    void iconClicked();

    /** @brief The widget in the additional widget list with type @ref WidgetCloseIcon was clicked. */
    void closeIconClicked();

    /** @brief The widget in the additional widget list with type @ref WidgetFilter was clicked. */
    void filterIconClicked();

    /** @brief The widget in the additional widget list with type @ref WidgetJourneySearch was clicked. */
    void journeySearchIconClicked();

    /** @brief The recent journeys button in journey search view was clicked. */
    void recentJourneysIconClicked();

    /**
     * @brief The widget of type WidgetJourneySearchButton was clicked or enter was pressed
     *   in widget of type @ref WidgetJourneySearchLine.
     *
     * @param text The finished journey search text.
     **/
    void journeySearchInputFinished( const QString &text );

    /**
     * @brief The text of the widget of type @ref WidgetJourneySearchLine has changed.
     *
     * @param text The new text.
     **/
    void journeySearchInputEdited( const QString &text );

    void journeySearchListUpdated( const QList<JourneySearchItem> &newJourneySearches );

public slots:
    /** @brief Updates the filter widget based on the current applet settings. */
    void updateFilterWidget();

    /** @brief Call this when the applet settings have changed. */
    void settingsChanged();

protected slots:
    void slotJourneySearchInputChanged( const QString &text );
    void slotJourneySearchInputFinished();
    void slotFilterIconClicked();
    void slotJourneysIconClicked();

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

    virtual void resizeEvent( QGraphicsSceneResizeEvent* event );
    void updateTitle();

    TitleType m_type;
    Plasma::IconWidget *m_icon;
    Plasma::Label *m_title;
    Plasma::ToolButton *m_filterWidget;
    Plasma::ToolButton *m_journeysWidget;
    QHash< WidgetType, QGraphicsWidget* > m_widgets;
    QGraphicsLinearLayout *m_layout;
    Settings *m_settings;
    QString m_titleText;
    bool m_journeysSupported;
    QAction *m_journeysAction;
    QAction *m_filtersAction;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(TitleWidget::WidgetTypes)
Q_DECLARE_OPERATORS_FOR_FLAGS(TitleWidget::RemoveWidgetOptions)

#endif // TITLEWIDGET_H
