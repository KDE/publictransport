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

#ifndef STOPLINEEDIT_H
#define STOPLINEEDIT_H

#include "dynamicwidget.h"
#include <KLineEdit>
#include <Plasma/DataEngine>

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

class StopLineEditPrivate;

/**
 * @brief A KLineEdit that provides stop name autocompletion.
 *
 * Uses the publictransport data engine to get stop name suggestions. The service provider to be
 * used for suggestions must be specified in the constructor or via @ref setServiceProvider.
 * If the service provider requires a city to be set you need to also set a city name using
 * @ref setCity.
 **/
class PUBLICTRANSPORTHELPER_EXPORT StopLineEdit : public KLineEdit
{
    Q_OBJECT
public:
    explicit StopLineEdit( QWidget* parent = 0, const QString &serviceProvider = QString(),
                           KGlobalSettings::Completion completion = KGlobalSettings::CompletionPopup );
    virtual ~StopLineEdit();

    /**
     * @brief Sets the ID of the service provider to be used for stop name suggestions.
     *
     * @param serviceProvider The new service provider ID to be used for stop name suggestions.
     **/
    void setServiceProvider( const QString &serviceProvider );

    /** @brief Gets the ID of the service provider that is used for stop name suggestions. */
    QString serviceProvider() const;

    /**
     * @brief Sets the city to be used for stop name suggestions.
     *
     * @param city The new city to be used for stop name suggestions.
     **/
    void setCity( const QString &city );

    /** @brief Gets the city that is used for stop name suggestions. */
    QString city() const;

protected Q_SLOTS:
    /** @brief Stop suggestion data arrived from the data engine. */
    void dataUpdated( const QString& sourceName, const Plasma::DataEngine::Data &data );

    /** @brief The stop name was edited. */
    void edited( const QString &newText );

protected:
    StopLineEditPrivate* const d_ptr;

private:
    Q_DECLARE_PRIVATE( StopLineEdit )
    Q_DISABLE_COPY( StopLineEdit )
};

/**
 * @brief A dynamic list of StopLineEdit widgets.
 **/
class PUBLICTRANSPORTHELPER_EXPORT StopLineEditList : public DynamicLabeledLineEditList
{
public:
    StopLineEditList( QWidget* parent = 0,
                      RemoveButtonOptions removeButtonOptions = RemoveButtonsBesideWidgets,
                      AddButtonOptions addButtonOptions = AddButtonBesideFirstWidget,
                      SeparatorOptions separatorOptions = NoSeparator,
                      NewWidgetPosition newWidgetPosition = AddWidgetsAtBottom,
                      const QString& labelText = "Item %1:" );

    virtual KLineEdit* createLineEdit();

    /**
     * @brief Sets the service provider to be used for stop name suggestions
     *   in all StopLineEdit widgets.
     *
     * @param serviceProvider The new service provider ID to be used for stop name suggestions.
     **/
    void setServiceProvider( const QString &serviceProvider );

    /**
     * @brief Sets the city to be used for stop name suggestions in all StopLineEdit widgets.
     *
     * @param city The new city to be used for stop name suggestions.
     **/
    void setCity( const QString &city );
};

}; // namespace Timetable

#endif // STOPLINEEDIT_H
