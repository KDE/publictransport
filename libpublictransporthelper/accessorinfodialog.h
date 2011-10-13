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

#ifndef ACCESSORINFODIALOG_H
#define ACCESSORINFODIALOG_H

/** @file
 * @brief Contains the AccessorInfoDialog.
 *
 * @author Friedrich Pülz <fpuelz@gmx.de>
 **/

#include "publictransporthelper_export.h"
#include <KDialog>

namespace Plasma {
class DataEngine;
}

class KJob;
/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

class AccessorInfoDialogPrivate;
/**
 * @brief This dialog shows information about an accessor for a public transport service provider.
 **/
class PUBLICTRANSPORTHELPER_EXPORT AccessorInfoDialog : public KDialog
{
	Q_OBJECT

public:
	/**
	 * @brief Options for the accessor info dialog.
	 **/
	enum Option {
		NoOption = 0x0000, /**< Don't use any option. */
		ShowOpenInTimetableMateButton = 0x0001, /**< Show a button to open the accessor sources in
				* TimetableMate, a little IDE for editing public transport accessors. */
		DefaultOptions = ShowOpenInTimetableMateButton /**< Default options. */
	};
	Q_DECLARE_FLAGS(Options, Option);

	/**
	 * @brief Creates a dialog, that shows information about a public transport accessor.
	 *
	 * @param serviceProviderData The data object for the service provider from the
	 *   publictransport data engine. You can get it by querying for eg.
	 *   "ServiceProvider <em>id</em>" (id replaced by the service provider ID).
	 * @param icon The icon to show for the service provider. You can use the favicon of the
	 *   service providers home page from the <em>favicons</em> data engine. The <em>"url"</em>
	 *   key of the data object for the service provider from the publictransport data engine
	 *   contains an URL, that should give you a favicon, if one is available for the service
	 *   provider.
     * @param publicTransportEngine A pointer to the Public Transport data engine.
	 * @param options Options for the accessor info dialog.
	 * @param parent The parent widget of the dialog.
	 **/
	AccessorInfoDialog( const QVariantHash& serviceProviderData, const QIcon& icon,
                        Plasma::DataEngine *publicTransportEngine,
						AccessorInfoDialog::Options options = DefaultOptions,
						QWidget* parent = 0 );

    virtual ~AccessorInfoDialog();

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
	AccessorInfoDialogPrivate* const d_ptr;

private:
	Q_DECLARE_PRIVATE( AccessorInfoDialog )
	Q_DISABLE_COPY( AccessorInfoDialog )
};
Q_DECLARE_OPERATORS_FOR_FLAGS(AccessorInfoDialog::Options);

}; // namespace Timetable

#endif // ACCESSORINFODIALOG_H
