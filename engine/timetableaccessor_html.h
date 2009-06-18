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

#ifndef TIMETABLEACCESSOR_HTML_HEADER
#define TIMETABLEACCESSOR_HTML_HEADER

#include "timetableaccessor.h"
#include "timetableaccessor_html_infos.h"

class TimetableAccessorHtml : public TimetableAccessor
{
    public:
	TimetableAccessorHtml();
	TimetableAccessorHtml( ServiceProvider serviceProvider );
	
	virtual ServiceProvider serviceProvider() { return m_info.serviceProvider; };
	virtual QString country() const { return m_info.country; };
	virtual QStringList cities() const { return m_info.cities; };
	virtual bool useSeperateCityValue() const { return m_info.useSeperateCityValue; };

	static QString decodeHtml( QString sHtml );

    protected:
	// Parses the contents of the document at the url
	virtual bool parseDocument( QList<DepartureInfo> *journeys );
	virtual bool parseDocumentPossibleStops( QMap<QString,QString> *stops );

	// Gets the "raw" url
	virtual QString rawUrl();
	// The regexp string to use
	virtual QString regExpSearch();
	// The regexp string to use to find the range where possible stops are listed
	virtual QString regExpSearchPossibleStopsRange();
	// The regexp string to use to find the stops from the list of possible stops
	virtual QString regExpSearchPossibleStops();
	// The meanings of matches of the regexp
	virtual QList< TimetableInformation > regExpInfos();
	// The meanings of matches of the regexp for possible stops
	virtual QList< TimetableInformation > regExpInfosPossibleStops();
	virtual DepartureInfo getInfo(QRegExp rx);

    private:
	TimetableAccessorInfo m_info;
};

#endif // TIMETABLEACCESSOR_HTML_HEADER
