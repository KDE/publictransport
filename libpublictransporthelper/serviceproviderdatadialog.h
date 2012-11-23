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

#ifndef SERVICEPROVIDERDATADIALOG_H
#define SERVICEPROVIDERDATADIALOG_H

/** @file
 * @brief Contains the ServiceProviderDataDialog.
 *
 * @author Friedrich Pülz <fpuelz@gmx.de>
 **/

#include "publictransporthelper_export.h"

// KDE includes
#include <KDialog> // Base class
#include <Plasma/DataEngine>

class ServiceProviderData;
namespace Plasma {
    class DataEngine;
}
class KJob;

/** @brief Namespace for the publictransport helper library. */
namespace PublicTransport {

class ServiceProviderDataDialog;
class ServiceProviderDataWidgetPrivate;

/**
 * @brief This dialog shows information about a service provider (plugin).
 * @see ServiceProviderDataDialog
 **/
class PUBLICTRANSPORTHELPER_EXPORT ServiceProviderDataWidget : public QWidget
{
    Q_OBJECT
    friend ServiceProviderDataDialog;

public:
    /**
     * @brief Options for the provider data widget.
     **/
    enum Option {
        NoOption = 0x0000, /**< Don't use any option. */
        ShowDeleteGtfsDatabaseButton = 0x0001, /**< Show a button to delete the Gtfs database. */
        DefaultOptions = ShowDeleteGtfsDatabaseButton /**< Default options. */
    };
    Q_DECLARE_FLAGS(Options, Option)

    /**
     * @brief Create a widget, that shows information about a service provider (plugin).
     *
     * @param providerId The ID of the service provider plugin to show information for.
     * @param options Options for the provider data widget.
     * @param parent The parent widget of the widget.
     **/
    ServiceProviderDataWidget( const QString &providerId, Options options = DefaultOptions,
                               QWidget *parent = 0 );

    virtual ~ServiceProviderDataWidget();

    bool isImportFinished() const;

Q_SIGNALS:
    /**
     * @brief The GTFS database for the service provider was deleted manually.
     *
     * A warning message box was shown, the user clicked "Continue" and the deletion job has
     * finished.
     **/
    void gtfsDatabaseDeleted();

protected Q_SLOTS:
    /** @brief The data from the favicons data engine was updated. */
    void dataUpdated( const QString &sourceName, const Plasma::DataEngine::Data &data );

    /**
     * @brief The button to delete the GTFS database has been clicked.
     **/
    void deleteGtfsDatabase();

    /**
     * @brief Deletion of the GTFS database has finished.
     **/
    void deletionFinished( KJob *job );

protected:
    ServiceProviderDataWidgetPrivate* const d_ptr;

private:
    Q_DECLARE_PRIVATE( ServiceProviderDataWidget )
    Q_DISABLE_COPY( ServiceProviderDataWidget )
};
Q_DECLARE_OPERATORS_FOR_FLAGS(ServiceProviderDataWidget::Options)

class ServiceProviderDataDialogPrivate;

/**
 * @brief This dialog shows information about a service provider (plugin).
 * @see ServiceProviderDataWidget The main widget of this dialog.
 **/
class PUBLICTRANSPORTHELPER_EXPORT ServiceProviderDataDialog : public KDialog
{
    Q_OBJECT
    friend ServiceProviderDataWidget;

public:
    /**
     * @brief Options for the provider data dialog.
     **/
    enum Option {
        NoOption = 0x0000, /**< Don't use any option. */
        ShowOpenInTimetableMateButton = 0x0001, /**< Show a button to open the provider plugin
                * sources in TimetableMate, a little IDE to edit PublicTransport engine service
                * provider plugins. */
        ShowDeleteGtfsDatabaseButton  = 0x0002, /**< Show a button to delete the Gtfs database. */
        DefaultOptions = ShowOpenInTimetableMateButton | ShowDeleteGtfsDatabaseButton
                /**< Default options. */
    };
    Q_DECLARE_FLAGS(Options, Option)

    /**
     * @brief Create a dialog, that shows information about a service provider (plugin).
     *
     * @param providerId The ID of the service provider plugin to show information for.
     * @param options Options for the provider data dialog.
     * @param parent The parent widget of the dialog.
     **/
    ServiceProviderDataDialog( const QString &providerId, Options options = DefaultOptions,
                               QWidget *parent = 0 );

    virtual ~ServiceProviderDataDialog();

    /**
     * @brief Get the used ServiceProviderDataWidget.
     **/
    ServiceProviderDataWidget *providerDataWidget() const;

protected Q_SLOTS:
    /**
     * @brief The button to open the service provider in TimetableMate was clicked.
     **/
    void openInTimetableMate();

    /**
     * @brief Deletion of the GTFS database has finished.
     **/
    void gtfsDatabaseDeletionFinished();

    /**
     * @brief The @p button of this dialog was clicked.
     **/
    virtual void slotButtonClicked( int button );

protected:
    ServiceProviderDataDialogPrivate* const d_ptr;

private:
    Q_DECLARE_PRIVATE( ServiceProviderDataDialog )
    Q_DISABLE_COPY( ServiceProviderDataDialog )
};
Q_DECLARE_OPERATORS_FOR_FLAGS(ServiceProviderDataDialog::Options)

} // namespace Timetable

#endif // Multiple inclusion guard
