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
    if ( m_regExps.searchDepartures.infos().contains(info) ||
	    (!m_regExps.searchDeparturesPre.regExp().isEmpty()
	    && m_regExps.searchDepartures.infos().contains(
	    m_regExps.searchDeparturesPre.infos().at(0) )
	    && m_regExps.searchDeparturesPre.infos().at(1) == info) )
	return true;
	
    bool supportedByPossibleStopRegExps = false;
    foreach ( TimetableRegExpSearch searchPossibleStops, m_regExps.searchPossibleStops ) {
	if ( searchPossibleStops.infos().contains(info) ) {
	    supportedByPossibleStopRegExps = true;
	    break;
	}
    }

    return supportedByPossibleStopRegExps || supportsByJourneyNewsParsing( info );
}

bool TimetableAccessorInfo::supportsByJourneyNewsParsing(
				const TimetableInformation &info ) const {
    foreach ( TimetableRegExpSearch search, m_regExps.searchJourneyNews ) {
	if ( search.infos().contains( info ) )
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
