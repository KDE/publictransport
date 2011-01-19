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
#include <KStandardDirs>

TimetableRegExpSearch::TimetableRegExpSearch( const QString &regExpSearch,
        const QList< TimetableInformation > &regExpInfos )
{
	m_regExpSearch = QRegExp( regExpSearch, Qt::CaseInsensitive );
	m_regExpSearch.setMinimal( true );

	m_regExpInfos = regExpInfos;
}


TimetableAccessorInfo::TimetableAccessorInfo( const QString& name,
        const QString& shortUrl, const QString &author, const QString &email,
        const QString &version, const QString& serviceProviderID,
        const AccessorType& accessorType )
{
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

TimetableAccessorInfo::~TimetableAccessorInfo()
{
}

void TimetableAccessorInfo::setAuthor( const QString& author, const QString &email )
{
	m_author = author;
	m_email = email;
}

void TimetableAccessorInfo::setFileName(const QString& fileName)
{
	m_fileName = KStandardDirs::realFilePath( fileName );
}

QString TimetableAccessorInfo::mapCityNameToValue( const QString &city ) const
{
	if ( m_hashCityNameToValue.contains( city.toLower() ) ) {
		return m_hashCityNameToValue[city.toLower()];
	} else {
		return city;
	}
}

TimetableAccessorInfoRegExp::TimetableAccessorInfoRegExp( const QString& name, 
		const QString& shortUrl, const QString& author, const QString& email, 
		const QString& version, const QString& serviceProviderID, const AccessorType& accessorType )
		: TimetableAccessorInfo(name, shortUrl, author, email, version, serviceProviderID, accessorType)
{
}

TimetableAccessorInfoRegExp::TimetableAccessorInfoRegExp( const TimetableAccessorInfo& info )
		: TimetableAccessorInfo( info.name(), info.shortUrl(), info.author(), info.email(),
								 info.version(), info.serviceProvider(), info.accessorType() )
{
	setFileName( info.fileName() );
	setCountry( info.country() );
	setCities( info.cities() );
	setCredit( info.credit() );
	setCityNameToValueReplacementHash( info.cityNameToValueReplacementHash() );
	setUseSeparateCityValue( info.useSeparateCityValue() );
	setOnlyUseCitiesInList( info.onlyUseCitiesInList() );
	setDescription( info.description() );
	setDefaultVehicleType( info.defaultVehicleType() );
	setUrl( info.url() );
	setShortUrl( info.shortUrl() );
	setMinFetchWait( info.minFetchWait() );
	setDepartureRawUrl( info.departureRawUrl() );
	setJourneyRawUrl( info.journeyRawUrl() );
	setStopSuggestionsRawUrl( info.stopSuggestionsRawUrl() );
	setFallbackCharset( info.fallbackCharset() );
	setCharsetForUrlEncoding( info.charsetForUrlEncoding() );
	setAttributesForDepatures( info.attributesForDepatures() );
	setAttributesForJourneys( info.attributesForJourneys() );
	setAttributesForStopSuggestions( info.attributesForStopSuggestions() );
}

void TimetableAccessorInfoRegExp::setRegExpDepartures( const QString &regExpSearch,
        const QList< TimetableInformation > &regExpInfos,
        const QString &regExpSearchPre, TimetableInformation regExpInfoKeyPre,
        TimetableInformation regExpInfoValuePre )
{
	m_searchDepartures = TimetableRegExpSearch( regExpSearch, regExpInfos );
	if ( !regExpSearchPre.isEmpty() ) {
		m_searchDeparturesPre = TimetableRegExpSearch( regExpSearchPre,
				QList< TimetableInformation >() << regExpInfoKeyPre << regExpInfoValuePre );
	}
}

void TimetableAccessorInfoRegExp::addRegExpPossibleStops( const QString &regExpRange,
        const QString &regExpSearch, const QList< TimetableInformation > &regExpInfos )
{
	m_regExpSearchPossibleStopsRanges << regExpRange;
	m_searchPossibleStops << TimetableRegExpSearch( regExpSearch, regExpInfos );
}

bool TimetableAccessorInfoRegExp::supportsTimetableAccessorInfo( const TimetableInformation& info ) const
{
	if ( m_searchDepartures.info().contains(info) ||
			(!m_searchDeparturesPre.regExp().isEmpty()
			&& m_searchDepartures.info().contains(m_searchDeparturesPre.info().at(0))
			&& m_searchDeparturesPre.info().at(1) == info) ) {
		return true;
	}

	bool supportedByPossibleStopRegExps = false;
	foreach( const TimetableRegExpSearch &searchPossibleStops, m_searchPossibleStops ) {
		if ( searchPossibleStops.info().contains( info ) ) {
			supportedByPossibleStopRegExps = true;
			break;
		}
	}

	return supportedByPossibleStopRegExps || supportsByJourneyNewsParsing( info );
}

bool TimetableAccessorInfoRegExp::supportsByJourneyNewsParsing( const TimetableInformation &info ) const
{
	foreach( const TimetableRegExpSearch &search, m_searchJourneyNews ) {
		if ( search.info().contains( info ) ) {
			return true;
		}
	}

	return false;
}