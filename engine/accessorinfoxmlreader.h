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
* @brief This file contains an xml reader that reads accessor info xml files.
* @author Friedrich Pülz <fpuelz@gmx.de> */

#ifndef ACCESSORINFOXMLREADER_HEADER
#define ACCESSORINFOXMLREADER_HEADER

#include <QXmlStreamReader>
#include <QHash>
#include "enums.h"

class TimetableAccessor;
class AccessorInfoXmlReader : public QXmlStreamReader {
	friend class TimetableAccessorInfo; // Because AccessorInfoXmlReader needs to set values in TimetableAccessorInfo when reading xml files

public:
	AccessorInfoXmlReader() : QXmlStreamReader() {};

	TimetableAccessor* read( QIODevice *device, const QString &serviceProvider,
							 const QString &fileName, const QString &country  );

private:
	void readUnknownElement();
	TimetableAccessor* readAccessorInfo( const QString &serviceProvider,
			const QString &fileName, const QString &country  );
	QString readLocalizedTextElement( QString *lang );
	bool readBooleanElement();
	void readAuthor( QString *fullname, QString *email );
	void readCities( QStringList *cities, QHash<QString, QString> *cityNameReplacements );
	void readRawUrls( QString *rawUrlDepartures, QString *rawUrlStopSuggestions,
					  QString *rawUrlJourneys );
	bool readRegExps( QString* regExpDepartures,
					  QList< TimetableInformation >* infosDepartures,
					  QString* regExpDeparturesPre,
					  TimetableInformation* infoPreKey, TimetableInformation* infoPreValue,
					  QString* regExpJourneys, QList< TimetableInformation >* infosJourneys,
					  QString* regExpDepartureGroupTitles,
					  QList< TimetableInformation >* infosDepartureGroupTitles,
					  QStringList* regExpListPossibleStopsRange,
					  QStringList* regExpListPossibleStops,
					  QList< QList< TimetableInformation > >* infosListPossibleStops,
					  QStringList* regExpListJourneyNews,
					  QList< QList< TimetableInformation > >* infosListJourneyNews );
	bool readRegExp( QString *regExp, QList<TimetableInformation> *info,
					 QString *regExpPreOrRanges = 0, TimetableInformation *infoPreKey = 0,
					 TimetableInformation *infoPreValue = 0 );
	void readRegExpInfos( QList<TimetableInformation> *info );
	bool readRegExpPre( QString *regExpPre, TimetableInformation *infoPreKey,
						TimetableInformation *infoPreValue );
	bool readRegExpItems( QStringList *regExps, QList< QList< TimetableInformation > >* infosList,
						  QStringList *regExpsRanges = 0 );
};

#endif // ACCESSORINFOXMLREADER_HEADER
