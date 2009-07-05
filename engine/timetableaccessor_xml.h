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

#ifndef TIMETABLEACCESSOR_XML_HEADER
#define TIMETABLEACCESSOR_XML_HEADER

#include "timetableaccessor.h"
#include "timetableaccessor_html.h"

class TimetableAccessorXml : public TimetableAccessor
{
    public:
	TimetableAccessorXml();
	TimetableAccessorXml( ServiceProvider serviceProvider );
	
	virtual bool useDifferentUrlForPossibleStops() const { return true; };

	virtual QString country() const { return m_info.country; };
	virtual QStringList cities() const { return m_info.cities; };
	virtual QStringList features() const;
	
    protected:
	// parses the contents of the document at the url
	virtual bool parseDocument( QList<DepartureInfo> *journeys );
	// Parses the contents of a document for a list of stop names
	virtual bool parseDocumentPossibleStops( QMap<QString,QString> *stops ) const;
	virtual QString rawUrl() const; // gets the "raw" url
	virtual QString differentRawUrl() const;
	virtual QByteArray charsetForUrlEncoding() const;

	TimetableAccessorHtml *m_accessorHTML;
};

#endif // TIMETABLEACCESSOR_XML_HEADER
