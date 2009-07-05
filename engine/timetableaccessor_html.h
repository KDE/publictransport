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

class TimetableAccessorHtml : public TimetableAccessor
{
    public:
	TimetableAccessorHtml( TimetableAccessorInfo info = TimetableAccessorInfo() );
	TimetableAccessorHtml( ServiceProvider serviceProvider );

	bool parseDocumentPossibleStops( const QByteArray document, QMap<QString,QString> *stops );

	static QString decodeHtmlEntities( QString sHtml );
	static QString decodeHtml( QByteArray document );

    protected:
	// Parses the contents of a received document for a list of departures and puts the results into journeys
	virtual bool parseDocument( QList<DepartureInfo> *journeys );
	// Exceuted before parseDocument if there is a regexp to use before starting parseDocument. It collects data matched by the regexp to be used in parseDocument
	virtual bool parseDocumentPre( QString document );
	// Parses the contents of a received document for a list of possible stop names and puts the results into stops (stop name => stop id)
	virtual bool parseDocumentPossibleStops( QMap<QString,QString> *stops ) const;
	// Parses a journey news string
	virtual bool parseJourneyNews( const QString sJourneyNews, QString *sDelay, QString *sDelayReason, QString *sJourneyNewsOther ) const;

    private:
	void postProcessMatchedData( TimetableInformation info, QString matchedData, QMap< TimetableInformation, QString > *data );

	// Data collected by parseDocumentPre
	QMap< QString, QString > *m_preData;
};

#endif // TIMETABLEACCESSOR_HTML_HEADER
