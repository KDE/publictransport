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
* @brief This file contains the base class for all HTML based accessors using javascript files for parsing that are used by the public transport data engine.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TIMETABLEACCESSOR_HTML_JS_HEADER
#define TIMETABLEACCESSOR_HTML_JS_HEADER

#include "timetableaccessor.h"

#include <QScriptEngine>

/** @class TimetableAccessorHtmlJs
* @brief The base class for all HTML accessors using java script files for parsing.
*/
class TimetableAccessorHtmlJs : public TimetableAccessor {
    public:
	/** Creates a new TimetableAccessorHtmlJs object with the given information.
	* @param info Information about how to download and parse the documents
	* of a service provider.
	* @note Can be used if you have a custom TimetableAccessorInfo object.
	* TimetableAccessorXml uses this to create an HTML accessor for parsing 
	* of stop lists. */
	TimetableAccessorHtmlJs( TimetableAccessorInfo info = TimetableAccessorInfo() );

    protected:
	/** Parses the contents of a received document for a list of departures 
	* / arrivals and puts the results into @p journeys.
	* @param journeys A pointer to a list of departure/arrival or journey 
	* informations. The results of parsing the document is stored in @p journeys.
	* @param parseDocumentMode The mode of parsing, e.g. parse for 
	* departures/arrivals or journeys.
	* @return true, if there were no errors and the data in @p journeys is valid.
	* @return false, if there were an error parsing the document. */
	virtual bool parseDocument( QList<PublicTransportInfo*> *journeys,
				    ParseDocumentMode parseDocumentMode = ParseForDeparturesArrivals );

	virtual QString parseDocumentForLaterJourneysUrl();
	virtual QString parseDocumentForDetailedJourneysUrl();
	
	/** Parses the contents of the given document for a list of possible stop names
	* and puts the results into @p stops.
	* @param document A document to be parsed.
	* @param stops A pointer to a string list, where the stop names are stored.
	* @param stopToStopId A pointer to a map, where the keys are stop names
	* and the values are stop IDs.
	* @return true, if there were no errors.
	* @return false, if there were an error parsing the document.
	* @note Can be used if you have an html document containing a stop list.
	* TimetableAccessorXml uses this to let the HTML accessor parse a downloaded
	* document for stops.
	* @see parseDocumentPossibleStops(QHash<QString,QString>*) const */
	virtual bool parseDocumentPossibleStops( const QByteArray document,
						 QStringList *stops,
						 QHash<QString,QString> *stopToStopId );

	/** Parses the contents of a received document for a list of possible stop names
	* and puts the results into @p stops.
	* @param stops A pointer to a string list, where the stop names are stored.
	* @param stopToStopId A pointer to a map, where the keys are stop names 
	* and the values are stop IDs.
	* @return true, if there were no errors.
	* @return false, if there were an error parsing the document.
	* @see parseDocumentPossibleStops(const QByteArray, QHash<QString,QString>*) */
	virtual bool parseDocumentPossibleStops( QStringList *stops,
						 QHash<QString,QString> *stopToStopId ) const;

	bool loadScript(const QString& fileName);
	bool isScriptLoaded() { return m_scriptLoaded; };
	
    private:
	bool m_scriptLoaded;
	QScriptEngine m_engine;
	QScriptValue m_parser;
};

#endif // TIMETABLEACCESSOR_HTML_JS_HEADER
