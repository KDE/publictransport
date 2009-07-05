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

#ifndef TIMETABLEACCESSOR_HTMLINFO_HEADER
#define TIMETABLEACCESSOR_HTMLINFO_HEADER

#include "enums.h"
#include <QList>
#include <QString>
#include <QStringList>

class TimetableRegExpSearch
{
    public:
	TimetableRegExpSearch();

	TimetableRegExpSearch( QString regExpSearch, QList< TimetableInformation > regExpInfos );

	// Gets the regular expression of this search
	QString regExp() const { return regExpSearch; }; // TODO: return QRegExp with setMinimal(true) and caseinsensitive

	// Gets a list of meanings for each matched string of the regular expression
	QList< TimetableInformation > infos () const { return regExpInfos; };

    private:
	QString regExpSearch;
	QList< TimetableInformation > regExpInfos;
};

class TimetableAccessorInfo
{
    public:
	// TODO: make private, m_[variable], create getters
	ServiceProvider serviceProvider;
	QString rawUrl, regExpSearchPossibleStopsRange, country;
	TimetableRegExpSearch searchJourneys, searchPossibleStops;
	TimetableRegExpSearch *searchJourneysPre;
	QList< TimetableRegExpSearch > searchJourneyNews;
	QStringList cities;
	bool useSeperateCityValue;

	TimetableAccessorInfo( const QString& name = "", const QString& shortUrl = "", const QString& author = "", const QString& email = "", const QString& version = "", const ServiceProvider& serviceProvider = NoServiceProvider, const AccessorType& accessorType = NoAccessor );

	virtual ~TimetableAccessorInfo() {
// 	    if ( searchJourneysPre )
// 	    {
// 		delete searchJourneysPre;
// 		searchJourneysPre = NULL;
// 	    }
	};

	// Sets a regular expression for parsing an html document for a list of journeys. A list of TimetableInformation-values is needed to know what's the meaning of each matched string of the regular expression. You can also set another regular expression to be executed before executing the actual regular expression. The data matched by this one is then used if the actual one doesn't provide it. It needs two TimetableInformation-values, one is used as the key and one as the value, to create a map where the keys (known from regExpSearch) point to values (not known from regExpSearch). That map is then used to fill in missing data.
	void setRegExpJourneys( QString regExpSearch, QList< TimetableInformation > regExpInfos, QString regExpSearchPre = "", TimetableInformation regExpInfoKeyPre = Nothing, TimetableInformation regExpInfoValuePre = Nothing );

	// Sets a regular expression for parsing an html document for a list of possible stops. You need to specify a regular expression for getting a subset of the html document containing the list and one for getting the stop information from that list.
	void setRegExpPossibleStops( QString regExpRange, QString regExpSearch, QList< TimetableInformation > regExpInfos = QList< TimetableInformation >() << StopName );

	// Adds a regular expression for parsing a string matched by the regular expression set by setRegExpJourneys() that is assocciated with the TimetableInformation-value JourneyNews. You can add more such regular expressions for different meanings.
	void addRegExpJouneyNews( QString regExpSearch, QList< TimetableInformation > regExpInfos );

	QString name() const;
	void setName( const QString &name );
	QByteArray charsetForUrlEncoding() const;
	void setCharsetForUrlEncoding( const QByteArray charsetForUrlEncoding );
	AccessorType accessorType() const;
	QString rawUrlXml() const;
	void setRawUrlXml( const QString &rawUrlXml );
	QString description() const;
	void setDescription( const QString &description );
	QString author() const;
	void setAuthor( const QString &author );
	QString version() const;
	void setVersion( const QString &version );
	QString email() const;
	void setEmail( const QString &email );
	QString url() const;
	void setUrl( const QString &url );
	QString shortUrl() const;
	void setShortUrl( const QString &shortUrl );

	QStringList features() const;
	bool supportsStopAutocompletion() const;
	bool supportsTimetableAccessorInfo( const TimetableInformation &info ) const;

    private:
	bool supportsByJourneyNewsParsing( const TimetableInformation &info ) const;

	// The name of this accessor, which can be displayed by the visualization
	QString m_name;
	// A short version of the url without protocol or "www"  to be displayed in links
	QString m_shortUrl;
	// A description of the service provider
	QString m_description;
	// The author of the accessor information to be used by the accessor
	QString m_author;
	// The email address of the author
	QString m_email;
	// The version of the accessor information
	QString m_version;
	// The main/home url of the service provider
	QString m_url;
	// If empty, use unicode (QUrl::toPercentEncoding()), otherwise use own toPercentEncoding() with this charset
	QByteArray m_charsetForUrlEncoding;
	// Raw url to an xml file for xml accessors
	QString m_rawUrlXml;
	// Type of the accessor (HTML, XML)
	AccessorType m_accessorType;
};

#endif // TIMETABLEACCESSOR_HTMLINFO_HEADER
