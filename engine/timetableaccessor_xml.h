/*
 *   Copyright 2009 Friedrich Pülz <fpuelz@gmx.de>
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
* @brief This file contains the base class for all XML based accessors used by the public transport data engine.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TIMETABLEACCESSOR_XML_HEADER
#define TIMETABLEACCESSOR_XML_HEADER

#include "timetableaccessor.h"
#include "timetableaccessor_html.h"


/** @class TimetableAccessorXml
* @brief This is the base class for all XML based accessors.
*/
class TimetableAccessorXml : public TimetableAccessor
{
    // Because the XML accessor uses TimetableAccessorHtml::parseDocumentPossibleStops().
    friend class TimetableAccessorHtml;

    public:
	/** Creates a new TimetableAccessorXml object with the given information.
	* @param info Information about how to download and parse the documents of a
	* service provider.
	* @note Can be used if you have a custom TimetableAccessorInfo object. TODO */
	TimetableAccessorXml( TimetableAccessorInfo info = TimetableAccessorInfo() );

	/** Gets a list of features that this accessor supports. */
	virtual QStringList features() const;

    protected:
	/** Parses the contents of a document that was requested using requestJourneys()
	* and puts the results into @p journeys..
	* @param journeys A pointer to a list of departure/arrival or journey informations.
	* The results of parsing the document is stored in @p journeys.
	* @param parseDocumentMode The mode of parsing, e.g. parse for
	* departures/arrivals or journeys.
	* @return true, if there were no errors and the data in @p journeys is valid.
	* @return false, if there were an error parsing the document. */
	virtual bool parseDocument( QList<PublicTransportInfo*> *journeys,
				    ParseDocumentMode parseDocumentMode = ParseForDeparturesArrivals );

	/** Parses the contents of a received document for a list of possible stop names
	* and puts the results into @p stops.
	* @param stops A pointer to a string list, where the stop names are stored.
	* @param stopToStopId A pointer to a map, where the keys are stop names
	* and the values are stop IDs.
	* @return true, if there were no errors.
	* @return false, if there were an error parsing the document. */
	virtual bool parseDocumentPossibleStops( QStringList *stops,
						 QHash<QString,QString> *stopToStopId,
						 QHash<QString,int> *stopToStopWeight );

	/** Gets the "raw" url with placeholders for the city ("%1") and the stop ("%2")
	* or only for the stop ("%1") if putCityIntoUrl() returns false. */
	virtual QString departuresRawUrl() const; // gets the "raw" url

	/** Gets a second "raw" url with placeholders for the city ("%1") and the stop ("%2")
	* or only for the stop ("%1") if putCityIntoUrl() returns false. */
	virtual QString stopSuggestionsRawUrl() const;

    private:
	TimetableAccessorHtml *m_accessorHTML; // The HTML accessor used to parse possible stop lists
};

#endif // TIMETABLEACCESSOR_XML_HEADER
