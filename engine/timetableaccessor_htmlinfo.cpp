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

#include "timetableaccessor_htmlinfo.h"
#include <KLocalizedString>
#include <KDebug>

TimetableRegExpSearch::TimetableRegExpSearch( const QString &regExpSearch,
	    const QList< TimetableInformation > &regExpInfos ) {
    m_regExpSearch = QRegExp( regExpSearch, Qt::CaseInsensitive );
    m_regExpSearch.setMinimal( true );

    m_regExpInfos = regExpInfos;
}


TimetableAccessorInfo::TimetableAccessorInfo( const QString& name,
	    const QString& shortUrl, const QString &author, const QString &email,
	    const QString &version, const QString& serviceProviderID,
	    const AccessorType& accessorType ) {
    m_name = name;
    m_shortUrl = shortUrl;
    m_author = author;
    m_email = email;
    m_version = version;
    m_serviceProviderID = serviceProviderID;
    m_accessorType = accessorType;
    m_onlyUseCitiesInList = false;
    m_defaultVehicleType = Unknown;
    m_minFetchWait = 0;
}

TimetableAccessorInfo::~TimetableAccessorInfo() {
}

void TimetableAccessorInfo::setRegExpDepartures( const QString &regExpSearch,
	const QList< TimetableInformation > &regExpInfos,
	const QString &regExpSearchPre, TimetableInformation regExpInfoKeyPre,
	TimetableInformation regExpInfoValuePre )
{
    m_regExps.searchDepartures = TimetableRegExpSearch( regExpSearch, regExpInfos );
    if ( !regExpSearchPre.isEmpty() ) {
	m_regExps.searchDeparturesPre = TimetableRegExpSearch( regExpSearchPre,
		QList< TimetableInformation >() << regExpInfoKeyPre << regExpInfoValuePre );
    }
}

void TimetableAccessorInfo::addRegExpPossibleStops( const QString &regExpRange,
	const QString &regExpSearch, const QList< TimetableInformation > &regExpInfos )
{
    m_regExps.regExpSearchPossibleStopsRanges << regExpRange;
    m_regExps.searchPossibleStops << TimetableRegExpSearch( regExpSearch, regExpInfos );
}

bool TimetableAccessorInfo::supportsTimetableAccessorInfo(
				const TimetableInformation& info ) const {
    if ( m_regExps.searchDepartures.info().contains(info) ||
	    (!m_regExps.searchDeparturesPre.regExp().isEmpty()
	    && m_regExps.searchDepartures.info().contains(
	    m_regExps.searchDeparturesPre.info().at(0) )
	    && m_regExps.searchDeparturesPre.info().at(1) == info) )
	return true;
	
    bool supportedByPossibleStopRegExps = false;
    foreach ( const TimetableRegExpSearch &searchPossibleStops, m_regExps.searchPossibleStops ) {
	if ( searchPossibleStops.info().contains(info) ) {
	    supportedByPossibleStopRegExps = true;
	    break;
	}
    }

    return supportedByPossibleStopRegExps || supportsByJourneyNewsParsing( info );
}

bool TimetableAccessorInfo::supportsByJourneyNewsParsing(
				const TimetableInformation &info ) const {
    foreach ( const TimetableRegExpSearch &search, m_regExps.searchJourneyNews ) {
	if ( search.info().contains( info ) )
	    return true;
    }

    return false;
}

void TimetableAccessorInfo::setAuthor( const QString& author, const QString &email ) {
    m_author = author;
    m_email = email;
}

QString TimetableAccessorInfo::mapCityNameToValue( const QString &city ) const {
    if ( m_hashCityNameToValue.contains( city.toLower() ) )
	return m_hashCityNameToValue[city.toLower()];
    else
	return city;
}
