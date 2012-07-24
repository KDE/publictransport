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

namespace Plasma {
class DataEngine;
}

class KJob;
/** @brief Namespace for the publictransport helper library. */
namespace PublicTransport {

class ServiceProviderDataDialogPrivate;

/**
 * @brief This dialog shows information about a service provider (plugin).
 **/
class PUBLICTRANSPORTHELPER_EXPORT ServiceProviderDataDialog : public KDialog
{
    Q_OBJECT

public:
    /**
     * @brief Options for the provider data dialog.
     **/
    enum Option {
        NoOption = 0x0000, /**< Don't use any option. */
        ShowOpenInTimetableMateButton = 0x0001, /**< Show a button to open the provider plugin
                * sources in TimetableMate, a little IDE to edit PublicTransport engine service
                * provider plugins. */
        DefaultOptions = ShowOpenInTimetableMateButton /**< Default options. */
    };
    Q_DECLARE_FLAGS(Options, Option)

    /**
     * @brief Create a dialog, that shows information about a service provider (plugin).
     *
     * @param serviceProviderData The data object for the service provider from the
     *   publictransport data engine. You can get it by querying for eg.
     *   "ServiceProvider <em>id</em>" (id replaced by the service provider ID).
     * @param icon The icon to show for the service provider. You can use the favicon of the
     *   service providers home page from the <em>favicons</em> data engine. The <em>"url"</em>
     *   key of the data object for the service provider from the publictransport data engine
     *   contains an URL, that should give you a favicon, if one is available for the service
     *   provider.
     *
     * @param options Options for the provider data dialog.
     * @param parent The parent widget of the dialog.
     **/
    ServiceProviderDataDialog( const QVariantHash& serviceProviderData, const QIcon& icon,
                        ServiceProviderDataDialog::Options options = DefaultOptions,
                        QWidget* parent = 0 );

    virtual ~ServiceProviderDataDialog();

Q_SIGNALS:
    /**
     * @brief The GTFS database for the service provider was deleted manually.
     *
     * A warning message box was shown, the user clicked "Continue" and the deletion job has
     * finished.
     **/
    void gtfsDatabaseDeleted();

protected Q_SLOTS:
    /**
     * @brief The button to open the service provider in TimetableMate was clicked.
     **/
    void openInTimetableMate();

    /**
     * @brief The button to delete the GTFS database has been clicked.
     **/
    void deleteGtfsDatabase();

    /**
     * @brief Deletion of the GTFS database has finished.
     **/
    void deletionFinished( KJob *job );

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
