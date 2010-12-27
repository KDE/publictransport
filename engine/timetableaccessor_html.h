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
* @brief This file contains the base class for all HTML based accessors used by the public transport data engine.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef TIMETABLEACCESSOR_HTML_HEADER
#define TIMETABLEACCESSOR_HTML_HEADER

#include "timetableaccessor.h"


/** @class TimetableAccessorHtml
* @brief The base class for all HTML accessors.
*/
class TimetableAccessorHtml : public TimetableAccessor
{
	// Because the XML accessor uses TimetableAccessorHtml::parseDocumentPossibleStops().
	friend class TimetableAccessorXml;

public:
	/**  
	 * @brief Creates a new TimetableAccessorHtml object with the given information.
	 * 
	 * @param info Information about how to download and parse the documents of a service provider.
	 * @note Can be used if you have a custom TimetableAccessorInfo object.
	 *   TimetableAccessorXml uses this to create an HTML accessor for parsing of stop lists. */
	TimetableAccessorHtml( TimetableAccessorInfoRegExp *info = new TimetableAccessorInfoRegExp() );

	/** @brief Decodes HTML entities in @p html, e.g. "&nbsp;" is replaced by " ". */
	static QString decodeHtmlEntities( const QString &html );

	/** 
	 * @brief Decodes the given HTML document. 
	 * 
	 * First it tries QTextCodec::codecForHtml().
	 * If that doesn't work, it parses the document for the charset in a meta-tag. */
	static QString decodeHtml( const QByteArray &document,
							   const QByteArray &fallbackCharset = QByteArray() );

protected:
	/** 
	 * @brief Parses the contents of a received document for a list of departures/arrivals
	 *   or journeys (depending on @p parseDocumentMode) and puts the results into @p journeys.
	 * 
	 * @param journeys A pointer to a list of departure/arrival or journey information.
	 *   The results of parsing the document is stored in @p journeys.
	 * @param parseDocumentMode The mode of parsing, e.g. parse for departures/arrivals or journeys.
	 * @return true, if there were no errors and the data in @p journeys is valid.
	 * @return false, if there were an error parsing the document. */
	virtual bool parseDocument( const QByteArray &document,
			QList<PublicTransportInfo*> *journeys, GlobalTimetableInfo *globalInfo,
			ParseDocumentMode parseDocumentMode = ParseForDeparturesArrivals );

	/** 
	 * @brief Exceuted before parseDocument() if there is a regexp to use before starting
	 *   parseDocument. It collects data matched by the regexp to be used in parseDocument.
	 * 
	 * @param document A string containing the whole document from the service provider.
	 * @return true, if there were no errors.
	 * @return false, if there were an error parsing the document. */
	virtual bool parseDocumentPre( const QString &document );

	/** 
	 * @brief Parses the contents of the given document for a list of possible stop names
	 *   and puts the results into @p stops.
	 * 
	 * @param document A document to be parsed.
	 * @param stops A pointer to a string list, where the stop names are stored.
	 * @param stopToStopId A pointer to a map, where the keys are stop names 
	 *   and the values are stop IDs.
	 * @return true, if there were no errors.
	 * @return false, if there were an error parsing the document.
	 * @note Can be used if you have an html document containing a stop list.
	 *   TimetableAccessorXml uses this to let the HTML accessor parse a downloaded
	 *   document for stops.
	 * @see parseDocumentPossibleStops(QHash<QString,QString>*) */
	virtual bool parseDocumentPossibleStops( const QByteArray &document,
			QStringList *stops, QHash<QString,QString> *stopToStopId,
			QHash<QString,int> *stopToStopWeight );

	/** 
	 * @brief Parses a journey news string using regular expressions from the accessor xml file. 
	 * 
	 * @param sJourneyNews A sub-string of the HTML document, that contains journey news.
	 * @param sDelay[out] The parsed delay gets stored here.
	 * @param sDelayReason[out] The parsed reason string for a delay gets stored here.
	 * @param sJourneyNewsOther[out] Other found information gets stored here. 
	 * @return True, if the journey news could be parsed. False, Otherwise. */
	virtual bool parseJourneyNews( const QString &sJourneyNews, QString *sDelay,
			QString *sDelayReason, QString *sJourneyNewsOther ) const;

private:
	void postProcessMatchedData( TimetableInformation info,
			const QString &matchedData, QHash< TimetableInformation, QVariant > *data );

	QHash< QString, QString > *m_preData; // Data collected by parseDocumentPre
};

#endif // TIMETABLEACCESSOR_HTML_HEADER
