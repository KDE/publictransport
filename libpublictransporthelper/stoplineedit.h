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
* @brief This file contains the StopLineEdit and StopLineEditList classes.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef STOPLINEEDIT_H
#define STOPLINEEDIT_H

#include "dynamicwidget.h" // For StopLineEditList
#include <KLineEdit> // Base class of StopLineEdit
#include <Plasma/DataEngine> // For Plasma::DataEngine::Data in StopLineEdit::dataUpdated()

class KJob;

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

class StopLineEditPrivate;

/**
 * @brief A KLineEdit with some additional functionality for Public Transport.
 *
 * This KLineEdit amongst others provides stop name completion and an easy way to start and monitor
 * the first import of the GTFS feed. Errors in the Public Transport data engine or service are
 * also shown.
 *
 * This class uses the publictransport data engine to get stop name suggestions. The service
 * provider to be used for suggestions must be specified in the constructor or via
 * @ref setServiceProvider. If the service provider requires a city to be set you need to also
 * set a city name using @ref setCity.
 *
 * Besides stop name completion there are three situations in which this line edit behaves and
 * looks differently than KLineEdit.
 * @li If there is an error getting stop suggestions using the current service provider,
 *     the line edit gets put into read only mode, showing the error string. Negative colors are
 *     used from KColorScheme.
 * @li If a GTFS accessor is used, but it's GTFS feed has never been imported completely, a
 *     question gets shown: "Start GTFS feed import?". By clicking the ok button shown inside the
 *     line edit, an import service job gets created and started. This leads to the following
 *     situation:
 * @li If the GTFS feed currently gets imported, a progress gets shown. There is also a cancel
 *     button, which allows to cancel the import service job.
 *
 * Shown buttons are no real QWidgets, they are only drawn in StopLineEdit::paintEvent() and
 * controlled by overriding other event handlers. Hover and pressed states of the buttons get
 * visualized.
 **/
class PUBLICTRANSPORTHELPER_EXPORT StopLineEdit : public KLineEdit
{
	Q_OBJECT

public:
    /** @brief Creates a new StopLineEdit object, using the given @p serviceProvider. */
	explicit StopLineEdit( QWidget* parent = 0, const QString &serviceProvider = QString(),
						   KGlobalSettings::Completion completion = KGlobalSettings::CompletionPopup );

    /** @brief Simple destructor. */
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

public Q_SLOTS:
    /**
     * @brief Updates the state of this StopLineEdit, eg. after a database was deleted.
     *
     * This slot should be called eg. after deleting a GTFS database using the Public Transport
     * service.
     * If data is available (eg. GTFS database completely imported) this StopLineEdit becomes
     * editable when calling this function. If no data is available an error string or a question
     * if data import should be started gets shown. A start button gets shown to start the import,
     * progress gets visualized in this StopLineEdit and can be suspended/resumed.
     **/
    void updateToDataEngineState();

protected Q_SLOTS:
    /** @brief Stop suggestion data arrived from the data engine. */
    void dataUpdated( const QString& sourceName, const Plasma::DataEngine::Data &data );

    /** @brief The stop name was edited. */
    void edited( const QString &newText );

    /** @brief The GTFS feed import has finished. */
    void importFinished( KJob *job );

    /** @brief The GTFS feed import has progressed. */
    void importProgress( KJob *job, ulong percent );

    /** @brief Info message from the GTFS feed import job. */
    void importInfoMessage( KJob *job, const QString &plain, const QString &rich = QString() );

    /** @brief Ask if timetable data import should be started, show a start button. */
    void askToImportTimetableData();

protected:
    StopLineEditPrivate* const d_ptr;

    /** @brief Overwritten to draw error strings, questions with buttons, a progress bar. */
    virtual void paintEvent( QPaintEvent *ev );

    /** @brief Overwritten to update the layout of the custom additional buttons. */
    virtual void resizeEvent( QResizeEvent *ev );

    /** @brief Overwritten for functionality of the custom additional buttons. */
    virtual void mousePressEvent( QMouseEvent *ev );

    /**
     * @brief Overwritten for functionality of the custom additional buttons.
     *
     * When the mouse gets released on the same button it was pressed on, this is a click and
     * triggers the buttons action.
     **/
    virtual void mouseReleaseEvent( QMouseEvent *ev );

    /** @brief Overwritten for functionality of the custom additional buttons. */
    virtual void mouseMoveEvent( QMouseEvent *ev );

    /** @brief Overwritten to provide custom tooltips for the custom additional buttons. */
    virtual bool event( QEvent *ev );

    /** @brief Starts a new service job to download and import the GTFS feed into the database. */
    void importGtfsFeed();

    /** @brief Cancel a running GTFS feed import. */
    bool cancelImport();

private:
    Q_DECLARE_PRIVATE( StopLineEdit )
    Q_DISABLE_COPY( StopLineEdit )
};

/**
 * @brief A dynamic list of StopLineEdit widgets.
 **/
class PUBLICTRANSPORTHELPER_EXPORT StopLineEditList : public DynamicLabeledLineEditList
{
    Q_OBJECT

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

public Q_SLOTS:
    /**
     * @brief Updates the state of the contained StopLineEdits, eg. after a database was deleted.
     *
     * This slot simply calls @ref StopLineEdit::updateToDataEngineState for all contained
     * StopLineEdits.
     **/
    void updateToDataEngineState();
};

}; // namespace Timetable

#endif // STOPLINEEDIT_H
