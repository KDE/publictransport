#include "timetableaccessor_htmlinfo.h"
#include <KLocalizedString>

TimetableRegExpSearch::TimetableRegExpSearch() {
}

TimetableRegExpSearch::TimetableRegExpSearch ( QString regExpSearch, QList< TimetableInformation > regExpInfos ) {
    m_regExpSearch = QRegExp(regExpSearch, Qt::CaseInsensitive );
    m_regExpSearch.setMinimal( true );

    m_regExpInfos = regExpInfos;
}


TimetableAccessorInfo::TimetableAccessorInfo ( const QString& name, const QString& shortUrl, const QString &author, const QString &email, const QString &version, const QString& serviceProviderID, const AccessorType& accessorType )
    : m_searchDeparturesPre ( 0 ) {
    m_name = name;
    m_shortUrl = shortUrl;
    m_author = author;
    m_email = email;
    m_version = version;
    m_serviceProviderID = serviceProviderID;
    m_accessorType = accessorType;
    m_onlyUseCitiesInList = false;
    m_defaultVehicleType = Unknown;
}

void TimetableAccessorInfo::setRegExpDepartures( const QString &regExpSearch, const QList< TimetableInformation > &regExpInfos, const QString &regExpSearchPre, TimetableInformation regExpInfoKeyPre, TimetableInformation regExpInfoValuePre ) {
    m_searchDepartures = TimetableRegExpSearch( regExpSearch, regExpInfos );
    if ( !regExpSearchPre.isEmpty() )
    {
	m_searchDeparturesPre = new TimetableRegExpSearch( regExpSearchPre, QList< TimetableInformation >() << regExpInfoKeyPre << regExpInfoValuePre );
    }
}

void TimetableAccessorInfo::setRegExpJourneys ( const QString &regExpSearch, const QList< TimetableInformation > &regExpInfos ) {
    m_searchJourneys = TimetableRegExpSearch( regExpSearch, regExpInfos );
}

void TimetableAccessorInfo::addRegExpPossibleStops( const QString &regExpRange, const QString &regExpSearch, const QList< TimetableInformation > &regExpInfos ) {
    m_regExpSearchPossibleStopsRanges << regExpRange;
    m_searchPossibleStops << TimetableRegExpSearch( regExpSearch, regExpInfos );
}

void TimetableAccessorInfo::addRegExpJouneyNews ( const QString &regExpSearch, const QList< TimetableInformation > &regExpInfos ) {
    m_searchJourneyNews.append( TimetableRegExpSearch( regExpSearch, regExpInfos ) );
}

void TimetableAccessorInfo::addCityNameToValueReplacement ( const QString& city, const QString& value ) {
    m_hashCityNameToValue.insert( city, value );
}

void TimetableAccessorInfo::setCityNameToValueReplacementHash ( const QHash<QString, QString>& hash ) {
    m_hashCityNameToValue = hash;
}

bool TimetableAccessorInfo::supportsStopAutocompletion() const {
    return !m_searchPossibleStops.isEmpty();
}

bool TimetableAccessorInfo::supportsTimetableAccessorInfo ( const TimetableInformation& info ) const {
    return m_searchDepartures.infos().contains(info) ||
	(m_searchDeparturesPre != NULL && m_searchDepartures.infos().contains( m_searchDeparturesPre->infos().at(0) ) && m_searchDeparturesPre->infos().at(1) == info) ||
	(!m_searchPossibleStops.isEmpty() && m_searchPossibleStops.first().infos().contains(info)) || // test all possible stop searches?
	supportsByJourneyNewsParsing(info);
}

bool TimetableAccessorInfo::supportsByJourneyNewsParsing( const TimetableInformation &info ) const {
    foreach ( TimetableRegExpSearch search, m_searchJourneyNews )
    {
	if ( search.infos().contains( info ) )
	    return true;
    }

    return false;
}

QString TimetableAccessorInfo::fileName() const {
    return m_fileName;
}

void TimetableAccessorInfo::setFileName ( const QString& fileName ) {
    m_fileName = fileName;
}

QString TimetableAccessorInfo::name() const {
    return m_name;
}

void TimetableAccessorInfo::setName ( const QString& name ) {
    m_name = name;
}

QByteArray TimetableAccessorInfo::charsetForUrlEncoding() const {
    return m_charsetForUrlEncoding;
}

void TimetableAccessorInfo::setCharsetForUrlEncoding ( const QByteArray &charsetForUrlEncoding ) {
    m_charsetForUrlEncoding = charsetForUrlEncoding;
}

AccessorType TimetableAccessorInfo::accessorType() const {
    return m_accessorType;
}

QString TimetableAccessorInfo::stopSuggestionsRawUrl() const {
    return m_stopSuggestionsRawUrl;
}

void TimetableAccessorInfo::setStopSuggestionsRawUrl ( const QString& rawUrlXml ) {
    m_stopSuggestionsRawUrl = rawUrlXml;
}

QString TimetableAccessorInfo::author() const {
    return m_author;
}

void TimetableAccessorInfo::setAuthor ( const QString& author, const QString &email ) {
    m_author = author;
    m_email = email;
}

QString TimetableAccessorInfo::description() const {
    return m_description;
}

void TimetableAccessorInfo::setDescription ( const QString& description ) {
    m_description = description;
}

QString TimetableAccessorInfo::version() const {
    return m_version;
}

void TimetableAccessorInfo::setVersion ( const QString& version ) {
    m_version = version;
}

QString TimetableAccessorInfo::email() const {
    return m_email;
}

QStringList TimetableAccessorInfo::features() const {
    QStringList list;

    if ( supportsStopAutocompletion() )
	list << i18nc("Autocompletion for names of public transport stops", "Autocompletion");
    if ( supportsTimetableAccessorInfo(Delay) )
	list << i18nc("Support for getting delay information. This string is used in a feature list, should be short.", "Delay");
    if ( supportsTimetableAccessorInfo(DelayReason) )
	list << i18nc("Support for getting the reason of a delay. This string is used in a feature list, should be short.", "Delay reason");
    if ( supportsTimetableAccessorInfo(Platform) )
	list << i18nc("Support for getting the information from which platform a public transport vehicle departs / at which it arrives. This string is used in a feature list, should be short.", "Platform");
    if ( supportsTimetableAccessorInfo(JourneyNews) || supportsTimetableAccessorInfo(JourneyNewsOther) || supportsTimetableAccessorInfo(JourneyNewsLink) )
	list << i18nc("Support for getting the news about a journey with public transport, such as a platform change. This string is used in a feature list, should be short.", "Journey news");
    if ( supportsTimetableAccessorInfo(TypeOfVehicle) )
	list << i18nc("Support for getting information about the type of vehicle of a journey with public transport. This string is used in a feature list, should be short.", "Type of vehicle");
    if ( supportsTimetableAccessorInfo(Status) )
	list << i18nc("Support for getting information about the status of a journey with public transport or an aeroplane. This string is used in a feature list, should be short.", "Status");
    if ( supportsTimetableAccessorInfo(Operator) )
	list << i18nc("Support for getting the operator of a journey with public transport or an aeroplane. This string is used in a feature list, should be short.", "Operator");
    if ( supportsTimetableAccessorInfo(StopID) )
	list << i18nc("Support for getting the id of a stop of public transport. This string is used in a feature list, should be short.", "Stop ID");
    if ( m_departureRawUrl.contains( "{dataType}" ) )
	list << i18nc("Support for getting arrivals for a stop of public transport. This string is used in a feature list, should be short.", "Arrivals");
    if ( !m_searchJourneys.regExp().isEmpty() )
	list << i18nc("Support for getting journeys from one stop to another. This string is used in a feature list, should be short.", "Journey search");

    return list;
}

void TimetableAccessorInfo::setUrl ( const QString& url ) {
    m_url = url;
}

QString TimetableAccessorInfo::url() const {
    return m_url;
}

void TimetableAccessorInfo::setShortUrl ( const QString& shortUrl ) {
    m_shortUrl = shortUrl;
}

QString TimetableAccessorInfo::shortUrl() const {
    return m_shortUrl;
}

void TimetableAccessorInfo::setDefaultVehicleType ( VehicleType vehicleType ) {
    m_defaultVehicleType = vehicleType;
}

VehicleType TimetableAccessorInfo::defaultVehicleType() const {
    return m_defaultVehicleType;
}

void TimetableAccessorInfo::setDepartureRawUrl ( const QString& departureRawUrl ) {
    m_departureRawUrl = departureRawUrl;
}

void TimetableAccessorInfo::setJourneyRawUrl ( const QString& journeyRawUrl ) {
    m_journeyRawUrl = journeyRawUrl;
}

QString TimetableAccessorInfo::journeyRawUrl() const {
    return m_journeyRawUrl;
}

QString TimetableAccessorInfo::serviceProvider() const {
    return m_serviceProviderID;
}

QString TimetableAccessorInfo::departureRawUrl() const {
    return m_departureRawUrl;
}

QString TimetableAccessorInfo::country() const {
    return m_country;
}

TimetableRegExpSearch TimetableAccessorInfo::searchDepartures() const {
    return m_searchDepartures;
}

TimetableRegExpSearch TimetableAccessorInfo::searchJourneys() const {
    return m_searchJourneys;
}

TimetableRegExpSearch* TimetableAccessorInfo::searchDeparturesPre() const {
    return m_searchDeparturesPre;
}

QStringList TimetableAccessorInfo::regExpSearchPossibleStopsRanges() const {
    return m_regExpSearchPossibleStopsRanges;
}

QList< TimetableRegExpSearch > TimetableAccessorInfo::searchPossibleStops() const {
    return m_searchPossibleStops;
}

QList< TimetableRegExpSearch > TimetableAccessorInfo::searchJourneyNews() const {
    return m_searchJourneyNews;
}

void TimetableAccessorInfo::setCountry ( const QString& country ) {
    m_country = country;
}

QStringList TimetableAccessorInfo::cities() const {
    return m_cities;
}

bool TimetableAccessorInfo::useSeperateCityValue() const {
    return m_useSeperateCityValue;
}

bool TimetableAccessorInfo::onlyUseCitiesInList() const {
    return m_onlyUseCitiesInList;
}

QString TimetableAccessorInfo::mapCityNameToValue( const QString &city ) const {
    if ( m_hashCityNameToValue.contains( city.toLower() ) )
	return m_hashCityNameToValue[city.toLower()];
    else
	return city;
}

void TimetableAccessorInfo::setOnlyUseCitiesInList ( bool onlyUseCitiesInList ) {
    m_onlyUseCitiesInList = onlyUseCitiesInList;
}

void TimetableAccessorInfo::setUseSeperateCityValue ( bool useSeperateCityValue ) {
    m_useSeperateCityValue = useSeperateCityValue;
}

void TimetableAccessorInfo::setCities ( const QStringList& cities ) {
    m_cities = cities;
}

