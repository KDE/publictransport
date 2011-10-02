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
#include "timetableaccessor_script.h"


/** @class TimetableAccessorXml
* @brief This is the base class for all XML based accessors.
*/
class TimetableAccessorXml : public TimetableAccessor
{
	// Because the XML accessor uses TimetableAccessorScript::parseDocumentPossibleStops().
	friend class TimetableAccessorScript;

public:
	/** 
	 * @brief Creates a new TimetableAccessorXml object with the given information.
	 *
	 * @param info Information about how to download and parse the documents of a
	 *   service provider.
	 * 
	 * @note Can be used if you have a custom TimetableAccessorInfo object. TODO */
	TimetableAccessorXml( TimetableAccessorInfo *info = new TimetableAccessorInfo() );

	/** 
	 * @brief Destructor, destroys the used "sub-" @ref TimetableAccessorScript, which is used
	 *  to parse stop suggestion documents in HTML format. */
	virtual ~TimetableAccessorXml();

    virtual AccessorType type() const { return XmlAccessor; };

	/** @brief Gets a list of features that this accessor supports. */
	virtual QStringList features() const;
	
	/**
	 * @brief Returns a pointer to the script accessor object that is used 
	 *   to parse stop suggestion documents.
	 **/
	TimetableAccessorScript *stopSuggestionAccessor() const { return m_accessorScript; };

protected:
	/** 
	 * @brief Parses the contents of a document that was requested using requestJourneys()
	 *   and puts the results into @p journeys.
	 *
	 * @param document A QByteArray containing the document to be parsed.
	 * 
	 * @param[out] journeys A pointer to a list of departure/arrival or journey information.
	 *   The results of parsing the document is stored in @p journeys.
	 * 
	 * @param[out] globalInfo A pointer to a GlobalTimetableInfo object containing information
	 *   for all departures/arrivals or journeys.
	 * 
	 * @param parseDocumentMode The mode of parsing, e.g. parse for
	 *   departures/arrivals or journeys.
	 *
	 * @return true, if there were no errors and the data in @p journeys is valid.
	 * 
	 * @return false, if there were an error parsing the document. */
	virtual bool parseDocument( const QByteArray &document, QList<PublicTransportInfo*> *journeys,
			GlobalTimetableInfo *globalInfo,
			ParseDocumentMode parseDocumentMode = ParseForDeparturesArrivals );

	/** 
	 * @brief Parses the contents of a received document for a list of possible stop names
	 *   and puts the results into @p stops.
	 *
	 * @param document A QByteArray containing the document to be parsed.
	 * 
	 * @param[out] stops A pointer to a list of @ref StopInfo objects.
	 *
	 * @return true, if there were no errors.
	 * 
	 * @return false, if there were an error parsing the document. */
	virtual bool parseDocumentPossibleStops( const QByteArray &document, QList<StopInfo*> *stops );

	/** 
	 * @brief Gets the "raw" url with placeholders for the city ("%1") and the stop ("%2")
	 *   or only for the stop ("%1") if putCityIntoUrl() returns false. */
	virtual QString departuresRawUrl() const; // gets the "raw" url

	/** 
	 * @brief Gets a second "raw" url with placeholders for the city ("%1") and the stop ("%2")
	 *   or only for the stop ("%1") if putCityIntoUrl() returns false. */
	virtual QString stopSuggestionsRawUrl() const;

private:
	TimetableAccessorScript *m_accessorScript; // The scripted accessor used to parse stop suggestion documents
};

#endif // TIMETABLEACCESSOR_XML_HEADER
