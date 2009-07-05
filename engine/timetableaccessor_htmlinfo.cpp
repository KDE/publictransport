#include "timetableaccessor_htmlinfo.h"
#include <KLocalizedString>

TimetableRegExpSearch::TimetableRegExpSearch()
{
}

TimetableRegExpSearch::TimetableRegExpSearch ( QString regExpSearch, QList< TimetableInformation > regExpInfos )
{
    this->regExpSearch = regExpSearch;
    this->regExpInfos = regExpInfos;
}

TimetableAccessorInfo::TimetableAccessorInfo ( const QString& name, const QString& shortUrl, const QString &author, const QString &email, const QString &version, const ServiceProvider& serviceProvider, const AccessorType& accessorType )
    : searchJourneysPre ( 0 )
{
    m_name = name;
    m_shortUrl = shortUrl;
    m_author = author;
    m_email = email;
    m_version = version;
    this->serviceProvider = serviceProvider;
    m_accessorType = accessorType;
}

void TimetableAccessorInfo::setRegExpJourneys ( QString regExpSearch, QList< TimetableInformation > regExpInfos, QString regExpSearchPre, TimetableInformation regExpInfoKeyPre, TimetableInformation regExpInfoValuePre )
{
    searchJourneys = TimetableRegExpSearch ( regExpSearch, regExpInfos );
    if ( !regExpSearchPre.isEmpty() )
    {
        searchJourneysPre = new TimetableRegExpSearch ( regExpSearchPre, QList< TimetableInformation >() << regExpInfoKeyPre << regExpInfoValuePre );
    }
}

void TimetableAccessorInfo::setRegExpPossibleStops ( QString regExpRange, QString regExpSearch, QList< TimetableInformation > regExpInfos )
{
    regExpSearchPossibleStopsRange = regExpRange;
    searchPossibleStops = TimetableRegExpSearch ( regExpSearch, regExpInfos );
}

void TimetableAccessorInfo::addRegExpJouneyNews ( QString regExpSearch, QList< TimetableInformation > regExpInfos )
{
    searchJourneyNews.append ( TimetableRegExpSearch ( regExpSearch, regExpInfos ) );
}

bool TimetableAccessorInfo::supportsStopAutocompletion() const
{
    return !searchPossibleStops.regExp().isEmpty();
}

bool TimetableAccessorInfo::supportsTimetableAccessorInfo ( const TimetableInformation& info ) const
{
    return searchJourneys.infos().contains(info) ||
	(searchJourneysPre != NULL && searchJourneys.infos().contains( searchJourneysPre->infos().at(0) ) && searchJourneysPre->infos().at(1) == info) ||
	searchPossibleStops.infos().contains(info) ||
	supportsByJourneyNewsParsing(info);
}

bool TimetableAccessorInfo::supportsByJourneyNewsParsing( const TimetableInformation &info ) const
{
    foreach ( TimetableRegExpSearch search, searchJourneyNews )
    {
	if ( search.infos().contains( info ) )
	    return true;
    }

    return false;
}

QString TimetableAccessorInfo::name() const
{
    return m_name;
}

void TimetableAccessorInfo::setName ( const QString& name )
{
    m_name = name;
}

QByteArray TimetableAccessorInfo::charsetForUrlEncoding() const
{
    return m_charsetForUrlEncoding;
}

void TimetableAccessorInfo::setCharsetForUrlEncoding ( const QByteArray charsetForUrlEncoding )
{
    m_charsetForUrlEncoding = charsetForUrlEncoding;
}

AccessorType TimetableAccessorInfo::accessorType() const
{
    return m_accessorType;
}

QString TimetableAccessorInfo::rawUrlXml() const
{
    return m_rawUrlXml;
}

void TimetableAccessorInfo::setRawUrlXml ( const QString& rawUrlXml )
{
    m_rawUrlXml = rawUrlXml;
}

QString TimetableAccessorInfo::author() const
{
    return m_author;
}

void TimetableAccessorInfo::setAuthor ( const QString& author )
{
    m_author = author;
}

QString TimetableAccessorInfo::description() const
{
    return m_description;
}

void TimetableAccessorInfo::setDescription ( const QString& description )
{
    m_description = description;
}

QString TimetableAccessorInfo::version() const
{
    return m_version;
}

void TimetableAccessorInfo::setVersion ( const QString& version )
{
    m_version = version;
}

QString TimetableAccessorInfo::email() const
{
    return m_email;
}

void TimetableAccessorInfo::setEmail ( const QString& email )
{
    m_email = email;
}

QStringList TimetableAccessorInfo::features() const
{
    QStringList list;

    if ( supportsStopAutocompletion() )
	list << i18nc("Autocompletion for names of public transport stops", "Autocompletion");
    if ( supportsTimetableAccessorInfo(Delay) )
	list << i18nc("Support for getting delay information. This string is used in a feature list, should be short.", "Delay");
    if ( supportsTimetableAccessorInfo(DelayReason) )
	list << i18nc("Support for getting the reason of a delay. This string is used in a feature list, should be short.", "Delay reason");
    if ( supportsTimetableAccessorInfo(Platform) )
	list << i18nc("Support for getting the information from which platform a public transport vehicle departs / at which it arrives. This string is used in a feature list, should be short.", "Platform");
    if ( supportsTimetableAccessorInfo(JourneyNews) || supportsTimetableAccessorInfo(JourneyNewsOther) )
	list << i18nc("Support for getting the news about a journey with public transport, such as a platform change. This string is used in a feature list, should be short.", "Journey news");
    if ( supportsTimetableAccessorInfo(TypeOfVehicle) )
	list << i18nc("Support for getting information about the type of vehicle of a journey with public transport. This string is used in a feature list, should be short.", "Type of vehicle");
    if ( supportsTimetableAccessorInfo(StopID) )
	list << i18nc("Support for getting the id of a stop of public transport. This string is used in a feature list, should be short.", "Stop ID");
    if ( rawUrl.contains( "%5" ) )
	list << i18nc("Support for getting arrivals for a stop of public transport. This string is used in a feature list, should be short.", "Arrivals");

    return list;
}

void TimetableAccessorInfo::setUrl ( const QString& url )
{
    m_url = url;
}

QString TimetableAccessorInfo::url() const
{
    return m_url;
}

void TimetableAccessorInfo::setShortUrl ( const QString& shortUrl )
{
    m_shortUrl = shortUrl;
}

QString TimetableAccessorInfo::shortUrl() const
{
    return m_shortUrl;
}




