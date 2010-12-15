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

// Own includes
#include "timetableaccessor.h"
#include "accessorinfoxmlreader.h"

// Qt includes
#include <QTextCodec>
#include <QFile>

// KDE includes
#include <KIO/NetAccess>
#include <KStandardDirs>
#include <KLocale>
#include <KDebug>

TimetableAccessor* TimetableAccessor::getSpecificAccessor( const QString &serviceProvider ) {
    QString filePath;
    QString country = "international";
    QString sp = serviceProvider;
    if ( sp.isEmpty() ) {
	// No service provider ID given, use the default one for the users country
	country = KGlobal::locale()->country();
	QStringList dirs = KGlobal::dirs()->findDirs("data",
		"plasma_engine_publictransport/accessorInfos");
	QString fileName = QString("%1_default.xml").arg(country);
	foreach ( const QString &dir, dirs ) {
	    if ( QFile::exists(dir + fileName) ) {
		filePath = dir + fileName;
		break;
	    }
	}
	
	// Get the real filename the "xx_default.xml"-symlink links to
	filePath = KGlobal::dirs()->realFilePath(filePath);
	if ( filePath.isEmpty() ) {
	    kDebug() << "Couldn't find the default service provider information XML for country" << country;
	    return NULL;
	}

	// Extract service provider ID from real filename
	int pos = filePath.lastIndexOf('/');
	sp = filePath.mid( pos + 1, filePath.length() - pos - 5 );
	kDebug() << "No service provider ID given, using the default one for country"
		<< country << "which is" << sp;
    } else {
	filePath = KGlobal::dirs()->findResource("data",
		QString("plasma_engine_publictransport/accessorInfos/%1.xml").arg(sp));
	if ( filePath.isEmpty() ) {
	    kDebug() << "Couldn't find a service provider information XML named" << sp;
	    return NULL;
	}

	// Get country code from filename
	QRegExp rx( "^([^_]+)" );
	if ( rx.indexIn(sp) != -1
		&& KGlobal::locale()->allCountriesList().contains(rx.cap()) ) {
	    country = rx.cap();
	}
    }

    QFile file( filePath );
    AccessorInfoXmlReader reader;
    TimetableAccessor *ret = reader.read( &file, sp, filePath, country );
    if ( !ret )
	kDebug() << "Error while reading accessor info xml" << filePath << reader.errorString();
    return ret;
}

AccessorType TimetableAccessor::accessorTypeFromString( const QString &sAccessorType ) {
    QString s = sAccessorType.toLower();
    if ( s == "html" )
	return HTML;
    else if ( s == "xml" )
	return XML;
    else
	return NoAccessor;
}

VehicleType TimetableAccessor::vehicleTypeFromString( QString sVehicleType ) {
    if ( sVehicleType == "Unknown" )
	 return Unknown;
    else if ( sVehicleType == "Tram" )
	return Tram;
    else if ( sVehicleType == "Bus" )
	return Bus;
    else if ( sVehicleType == "Subway" )
	return Subway;
    else if ( sVehicleType == "TrainInterurban" )
	return TrainInterurban;
    else if ( sVehicleType == "Metro" )
	return Metro;
    else if ( sVehicleType == "TrolleyBus" )
	return TrolleyBus;
    else if ( sVehicleType == "TrainRegional" )
	return TrainRegional;
    else if ( sVehicleType == "TrainRegionalExpress" )
	return TrainRegionalExpress;
    else if ( sVehicleType == "TrainInterregio" )
	return TrainInterregio;
    else if ( sVehicleType == "TrainIntercityEurocity" )
	return TrainIntercityEurocity;
    else if ( sVehicleType == "TrainIntercityExpress" )
	return TrainIntercityExpress;
    else if ( sVehicleType == "Feet" )
	return Feet;
    else if ( sVehicleType == "Ferry" )
	return Ferry;
    else if ( sVehicleType == "Ship" )
	return Ship;
    else if ( sVehicleType == "Plane" )
	return Plane;
    else
	return Unknown;
}

TimetableInformation TimetableAccessor::timetableInformationFromString(
			const QString& sTimetableInformation ) {
    QString sInfo = sTimetableInformation.toLower();
    if ( sInfo == "nothing" )
	return Nothing;
    else if ( sInfo == "departuredate" )
	return DepartureDate;
    else if ( sInfo == "departurehour" )
	return DepartureHour;
    else if ( sInfo == "departureminute" )
	return DepartureMinute;
    else if ( sInfo == "typeofvehicle" )
	return TypeOfVehicle;
    else if ( sInfo == "transportline" )
	return TransportLine;
    else if ( sInfo == "flightnumber" )
	return FlightNumber;
    else if ( sInfo == "target" )
	return Target;
    else if ( sInfo == "platform" )
	return Platform;
    else if ( sInfo == "delay" )
	return Delay;
    else if ( sInfo == "delayreason" )
	return DelayReason;
    else if ( sInfo == "journeynews" )
	return JourneyNews;
    else if ( sInfo == "journeynewsother" )
	return JourneyNewsOther;
    else if ( sInfo == "journeynewslink" )
	return JourneyNewsLink;
    else if ( sInfo == "departurehourprognosis" )
	return DepartureHourPrognosis;
    else if ( sInfo == "departureminuteprognosis" )
	return DepartureMinutePrognosis;
    else if ( sInfo == "status" )
	return Status;
    else if ( sInfo == "departureyear" )
	return DepartureYear;
    else if ( sInfo == "routestops" )
	return RouteStops;
    else if ( sInfo == "routetimes" )
	return RouteTimes;
    else if ( sInfo == "routetimesdeparture" )
	return RouteTimesDeparture;
    else if ( sInfo == "routetimesarrival" )
	return RouteTimesArrival;
    else if ( sInfo == "routeexactstops" )
	return RouteExactStops;
    else if ( sInfo == "routetypesofvehicles" )
	return RouteTypesOfVehicles;
    else if ( sInfo == "routetransportlines" )
	return RouteTransportLines;
    else if ( sInfo == "routeplatformsdeparture" )
	return RoutePlatformsDeparture;
    else if ( sInfo == "routeplatformsarrival" )
	return RoutePlatformsArrival;
    else if ( sInfo == "routetimesdeparturedelay" )
	return RouteTimesDepartureDelay;
    else if ( sInfo == "routetimesarrivaldelay" )
	return RouteTimesArrivalDelay;
    else if ( sInfo == "operator" )
	return Operator;
    else if ( sInfo == "departureamorpm" )
	return DepartureAMorPM;
    else if ( sInfo == "departureamorpmprognosis" )
	return DepartureAMorPMPrognosis;
    else if ( sInfo == "arrivalamorpm" )
	return ArrivalAMorPM;
    else if ( sInfo == "duration" )
	return Duration;
    else if ( sInfo == "startstopname" )
	return StartStopName;
    else if ( sInfo == "startstopid" )
	return StartStopID;
    else if ( sInfo == "targetstopname" )
	return TargetStopName;
    else if ( sInfo == "targetstopid" )
	return TargetStopID;
    else if ( sInfo == "arrivaldate" )
	return ArrivalDate;
    else if ( sInfo == "arrivalhour" )
	return ArrivalHour;
    else if ( sInfo == "arrivalminute" )
	return ArrivalMinute;
    else if ( sInfo == "changes" )
	return Changes;
    else if ( sInfo == "typesofvehicleinjourney" )
	return TypesOfVehicleInJourney;
    else if ( sInfo == "pricing" )
	return Pricing;
    else if ( sInfo == "nomatchonschedule" )
	return NoMatchOnSchedule;
    else if ( sInfo == "stopname" )
	return StopName;
    else if ( sInfo == "stopid" )
	return StopID;
    else if ( sInfo == "stopweight" )
	return StopWeight;
    else {
	kDebug() << sTimetableInformation
		 << "is an unknown timetable information value! Assuming value Nothing.";
	return Nothing;
    }
}

QStringList TimetableAccessor::features() const {
    QStringList list;
    
    if ( m_info.departureRawUrl().contains( "{dataType}" ) )
	list << "Arrivals";

    if ( m_info.scriptFileName().isEmpty() ) {
	if ( m_info.supportsStopAutocompletion() )
	    list << "Autocompletion";
	if ( !m_info.searchJourneys().regExp().isEmpty() )
	    list << "JourneySearch";
	if ( m_info.supportsTimetableAccessorInfo(Delay) )
	    list << "Delay";
	if ( m_info.supportsTimetableAccessorInfo(DelayReason) )
	    list << "DelayReason";
	if ( m_info.supportsTimetableAccessorInfo(Platform) )
	    list << "Platform";
	if ( m_info.supportsTimetableAccessorInfo(JourneyNews)
		    || m_info.supportsTimetableAccessorInfo(JourneyNewsOther)
		    || m_info.supportsTimetableAccessorInfo(JourneyNewsLink) )
	    list << "JourneyNews";
	if ( m_info.supportsTimetableAccessorInfo(TypeOfVehicle) )
	    list << "TypeOfVehicle";
	if ( m_info.supportsTimetableAccessorInfo(Status) )
	    list << "Status";
	if ( m_info.supportsTimetableAccessorInfo(Operator) )
	    list << "Operator";
	if ( m_info.supportsTimetableAccessorInfo(StopID) )
	    list << "StopID";
    } else
	list << scriptFeatures();

    list.removeDuplicates();
    return list;
}

QStringList TimetableAccessor::featuresLocalized() const {
    QStringList featuresl10n;
    QStringList featureList = features();

    if ( featureList.contains("Arrivals") )
	featuresl10n << i18nc("Support for getting arrivals for a stop of public "
			      "transport. This string is used in a feature list, "
			      "should be short.", "Arrivals");
    if ( featureList.contains("Autocompletion") )
	featuresl10n << i18nc("Autocompletion for names of public transport stops",
			      "Autocompletion");
    if ( featureList.contains("JourneySearch") )
	featuresl10n << i18nc("Support for getting journeys from one stop to another. "
			      "This string is used in a feature list, should be short.",
			      "Journey search");
    if ( featureList.contains("Delay") )
	featuresl10n << i18nc("Support for getting delay information. This string is "
			      "used in a feature list, should be short.", "Delay");
    if ( featureList.contains("DelayReason") )
	featuresl10n << i18nc("Support for getting the reason of a delay. This string "
			      "is used in a feature list, should be short.",
			      "Delay reason");
    if ( featureList.contains("Platform") )
	featuresl10n << i18nc("Support for getting the information from which platform "
			      "a public transport vehicle departs / at which it "
			      "arrives. This string is used in a feature list, "
			      "should be short.", "Platform");
    if ( featureList.contains("JourneyNews") )
	featuresl10n << i18nc("Support for getting the news about a journey with public "
			      "transport, such as a platform change. This string is "
			      "used in a feature list, should be short.", "Journey news");
    if ( featureList.contains("TypeOfVehicle") )
	featuresl10n << i18nc("Support for getting information about the type of "
			      "vehicle of a journey with public transport. This string "
			      "is used in a feature list, should be short.",
			      "Type of vehicle");
    if ( featureList.contains("Status") )
	featuresl10n << i18nc("Support for getting information about the status of a "
			      "journey with public transport or an aeroplane. This "
			      "string is used in a feature list, should be short.",
			      "Status");
    if ( featureList.contains("Operator") )
	featuresl10n << i18nc("Support for getting the operator of a journey with public "
			      "transport or an aeroplane. This string is used in a "
			      "feature list, should be short.", "Operator");
    if ( featureList.contains("StopID") )
	featuresl10n << i18nc("Support for getting the id of a stop of public transport. "
			      "This string is used in a feature list, should be short.",
			      "Stop ID");
    return featuresl10n;
}

KIO::StoredTransferJob *TimetableAccessor::requestDepartures( const QString &sourceName,
		const QString &city, const QString &stop, int maxCount,
		const QDateTime &dateTime, const QString &dataType,
		bool usedDifferentUrl ) {
    KUrl url = getUrl( city, stop, maxCount, dateTime, dataType, usedDifferentUrl );
//     kDebug() << url;
    
    KIO::StoredTransferJob *job = KIO::storedGet( url, KIO::NoReload, KIO::HideProgressInfo );
    ParseDocumentMode parseType = maxCount == -1
	    ? ParseForStopSuggestions : ParseForDeparturesArrivals;
    m_jobInfos.insert( job, JobInfos(parseType, sourceName, city, stop, url,
				     dataType, maxCount, dateTime, usedDifferentUrl) );

    connect( job, SIGNAL(result(KJob*)), this, SLOT(result(KJob*)) );

    return job;
}

KIO::StoredTransferJob* TimetableAccessor::requestStopSuggestions(
		const QString &sourceName, const QString &city, const QString &stop ) {
    if ( hasSpecialUrlForStopSuggestions() ) {
	KUrl url = getStopSuggestionsUrl( city, stop );
	KIO::StoredTransferJob *job = KIO::storedGet( url, KIO::NoReload, KIO::HideProgressInfo );
	m_jobInfos.insert( job, JobInfos(ParseForStopSuggestions, sourceName, city, stop, url) );
	
	connect( job, SIGNAL(result(KJob*)), this, SLOT(result(KJob*)) );
	
	return job;
    } else
	return requestDepartures( sourceName, city, stop, -1,
				  QDateTime::currentDateTime() );
}

KIO::StoredTransferJob *TimetableAccessor::requestJourneys( const QString &sourceName,
		const QString &city, const QString &startStopName,
		const QString &targetStopName, int maxCount,
		const QDateTime &dateTime, const QString &dataType,
		bool usedDifferentUrl ) {
    // Creating a kioslave
    KUrl url = getJourneyUrl( city, startStopName, targetStopName, maxCount,
			      dateTime, dataType, usedDifferentUrl );
    KIO::StoredTransferJob *job = requestJourneys( url );
    m_jobInfos.insert( job, JobInfos(ParseForJourneys, sourceName, city, startStopName,
		url, dataType, maxCount, dateTime, usedDifferentUrl, targetStopName) );

    return job;
}

KIO::StoredTransferJob* TimetableAccessor::requestJourneys( const KUrl& url ) {
//     kDebug() << url;

    KIO::StoredTransferJob *job = KIO::storedGet( url, KIO::NoReload, KIO::HideProgressInfo );
    connect( job, SIGNAL(result(KJob*)), this, SLOT(result(KJob*)) );
    
    return job;
}

void TimetableAccessor::result( KJob* job ) {
    JobInfos jobInfo = m_jobInfos.value( job );
    m_jobInfos.remove( job );
    KIO::StoredTransferJob *storedJob = static_cast< KIO::StoredTransferJob* >( job );
    QByteArray document = storedJob->data();
    
    QList< PublicTransportInfo* > dataList;
    GlobalTimetableInfo globalInfo;
    QStringList stops;
    QHash<QString, QString> stopToStopId;
    QHash<QString, int> stopToStopWeight;
    ParseDocumentMode parseDocumentMode = jobInfo.parseDocumentMode;
    kDebug() << "FINISHED:" << parseDocumentMode;
    
    QString sourceName = jobInfo.sourceName;
    QString city = jobInfo.city;
    QString stop = jobInfo.stop;
    QString dataType = jobInfo.dataType;
    QUrl url = jobInfo.url;
    if ( storedJob->error() != 0 ) {
	kDebug() << "Error in job" << storedJob->error() << storedJob->errorString();
	emit errorParsing( this, ErrorDownloadFailed, storedJob->errorString(),
			   jobInfo.url, serviceProvider(), jobInfo.sourceName, jobInfo.city,
			   stop, dataType, parseDocumentMode );
    }
    
    if ( parseDocumentMode == ParseForStopSuggestions ) {
	kDebug() << "Stop suggestions request finished" << sourceName << city << stop;
	if ( parseDocumentPossibleStops(document, &stops, &stopToStopId, &stopToStopWeight) ) {
	    kDebug() << "finished parsing for stop suggestions";
	    emit stopListReceived( this, url, stops, stopToStopId, stopToStopWeight,
				   serviceProvider(), sourceName, city, stop, QString(),
				   parseDocumentMode );
	    kDebug() << "emit stopListReceived finished";
	} else {
	    kDebug() << "error parsing for stop suggestions" << sourceName;
	    emit errorParsing( this, ErrorParsingFailed, i18n("Error while parsing "
			       "the timetable document."),
			       url, serviceProvider(), sourceName, city,
			       stop, QString(), parseDocumentMode );
	}

	return;
    }
    
    int maxDeps = jobInfo.maxDeps;
    QDateTime dateTime = jobInfo.dateTime;
    bool usedDifferentUrl = jobInfo.usedDifferentUrl;
    QString targetStop = jobInfo.targetStop;
    int roundTrips = jobInfo.roundTrips;
    if ( parseDocumentMode == ParseForJourneys ) {
	kDebug() << "    FINISHED JOURNEY SEARCH" << roundTrips;
    }
    m_curCity = city;
    
    kDebug() << "usedDifferentUrl" << usedDifferentUrl;
    if ( !usedDifferentUrl ) {
	QString sNextUrl;
	if ( parseDocumentMode == ParseForJourneys ) {
	    if ( roundTrips < 2 )
		sNextUrl = parseDocumentForLaterJourneysUrl( document );
	    else if ( roundTrips == 2 )
		sNextUrl = parseDocumentForDetailedJourneysUrl( document );
	}
	kDebug() << "Parse results" << parseDocumentMode;

	// Try to parse the document
	if ( parseDocument(document, &dataList, &globalInfo, parseDocumentMode) ) {
	    if ( parseDocumentMode == ParseForDeparturesArrivals ) {
		QList<DepartureInfo*> departures;
		foreach( PublicTransportInfo *info, dataList )
		    departures << dynamic_cast< DepartureInfo* >( info );
		emit departureListReceived( this, url, departures, globalInfo,
					    serviceProvider(), sourceName,
					    city, stop, dataType, parseDocumentMode );
	    } else if ( parseDocumentMode == ParseForJourneys ) {
		QList<JourneyInfo*> journeys;
		foreach( PublicTransportInfo *info, dataList )
		    journeys << dynamic_cast< JourneyInfo* >( info );
		emit journeyListReceived( this, url, journeys, globalInfo,
					  serviceProvider(), sourceName,
					  city, stop, dataType, parseDocumentMode );
	    }
	// Parsing has failed, try to parse stop suggestions.
	// First request departures using a different url if that is a special
	// url for stop suggestions.
	} else if ( hasSpecialUrlForStopSuggestions() ) {
// 	    kDebug() << "request possible stop list";
	    requestDepartures( sourceName, m_curCity, stop, maxDeps, dateTime,
			       dataType, true );
	} else if ( parseDocumentPossibleStops(document, &stops, &stopToStopId,
		    &stopToStopWeight) ) {
	    kDebug() << "Stop suggestion list received" << parseDocumentMode;
	    emit stopListReceived( this, url, stops, stopToStopId, stopToStopWeight,
				   serviceProvider(), sourceName, city, stop, dataType,
				   parseDocumentMode );
	} else {
	    kDebug() << "Error parsing for stop suggestions B" << sourceName;
	    emit errorParsing( this, ErrorParsingFailed, i18n("Error while parsing "
			       "the stop suggestions document."),
			       url, serviceProvider(), sourceName, city,
			       stop, dataType, parseDocumentMode );
	}

	if ( parseDocumentMode == ParseForJourneys ) {
	    if ( !sNextUrl.isNull() && !sNextUrl.isEmpty() ) {
		kDebug() << "\n\n     REQUEST PARSED URL:   " << sNextUrl << "\n\n";
		++roundTrips;
		KIO::StoredTransferJob *job = requestJourneys( KUrl(sNextUrl) );
		m_jobInfos.insert( job, JobInfos(ParseForJourneys, sourceName,
			    city, stop, url, dataType, maxDeps, dateTime,
			    usedDifferentUrl, targetStop, roundTrips) );
// 		return;
	    }
	}
    // Used a different url for requesting data, the data contains stop suggestions
    } else if ( parseDocumentPossibleStops(document, &stops, &stopToStopId, &stopToStopWeight) ) {
// 	kDebug() << "possible stop list received ok";
	emit stopListReceived( this, url, stops, stopToStopId, stopToStopWeight,
			       serviceProvider(), sourceName, city, stop, dataType,
			       parseDocumentMode );
    } else {
	kDebug() << "Error parsing for stop suggestions C" << sourceName;
	emit errorParsing( this, ErrorParsingFailed, i18n("Error while parsing "
			   "the stop suggestions document."),
			   url, serviceProvider(), sourceName, city,
			   stop, dataType, parseDocumentMode );
    }
}

KUrl TimetableAccessor::getUrl( const QString &city, const QString &stop,
				int maxCount, const QDateTime &dateTime,
				const QString &dataType, bool useDifferentUrl ) const {
    QString sRawUrl = useDifferentUrl ? stopSuggestionsRawUrl() : departuresRawUrl();
    QString sTime = dateTime.time().toString("hh:mm");
    QString sDataType;
    QString sCity = city.toLower(), sStop = stop.toLower();
    if ( dataType == "arrivals" )
	sDataType = "arr";
    else if ( dataType == "departures" || dataType == "journeys" )
	sDataType = "dep";

    sCity = timetableAccessorInfo().mapCityNameToValue( sCity );

    // Encode city and stop
    if ( charsetForUrlEncoding().isEmpty() ) {
	sCity = QString::fromAscii(QUrl::toPercentEncoding(sCity));
	sStop = QString::fromAscii(QUrl::toPercentEncoding(sStop));
    } else {
	sCity = toPercentEncoding( sCity, charsetForUrlEncoding() );
	sStop = toPercentEncoding( sStop, charsetForUrlEncoding() );
    }

    // Construct the url from the "raw" url by replacing values
    if ( useSeparateCityValue() )
	sRawUrl = sRawUrl.replace( "{city}", sCity );
    sRawUrl = sRawUrl.replace( "{time}", sTime )
		     .replace( "{maxCount}", QString("%1").arg(maxCount) )
		     .replace( "{stop}", sStop )
		     .replace( "{dataType}", sDataType );

    QRegExp rx = QRegExp("\\{date:([^\\}]*)\\}", Qt::CaseInsensitive);
    if ( rx.indexIn(sRawUrl) != -1 )
	sRawUrl.replace( rx, dateTime.date().toString(rx.cap(1)) );

    return KUrl( sRawUrl );
}

KUrl TimetableAccessor::getStopSuggestionsUrl( const QString &city,
					       const QString& stop ) {
    QString sRawUrl = stopSuggestionsRawUrl();
    QString sCity = city.toLower(), sStop = stop.toLower();
    
    // Encode stop
    if ( charsetForUrlEncoding().isEmpty() ) {
	sCity = QString::fromAscii(QUrl::toPercentEncoding(sCity));
	sStop = QString::fromAscii(QUrl::toPercentEncoding(sStop));
    } else {
	sCity = toPercentEncoding( sCity, charsetForUrlEncoding() );
	sStop = toPercentEncoding( sStop, charsetForUrlEncoding() );
    }
    
    if ( useSeparateCityValue() )
	sRawUrl = sRawUrl.replace( "{city}", sCity );
    sRawUrl = sRawUrl.replace( "{stop}", sStop );

    return KUrl( sRawUrl );
}

KUrl TimetableAccessor::getJourneyUrl( const QString& city,
				       const QString& startStopName,
				       const QString& targetStopName,
				       int maxCount, const QDateTime &dateTime,
				       const QString& dataType,
				       bool useDifferentUrl ) const {
    Q_UNUSED( useDifferentUrl );

    QString sRawUrl = m_info.journeyRawUrl();
    QString sTime = dateTime.time().toString("hh:mm");
    QString sDataType;
    QString sCity = city.toLower(), sStartStopName = startStopName.toLower(),
	    sTargetStopName = targetStopName.toLower();
    if ( dataType == "arrivals" || dataType == "journeysArr" )
	sDataType = "arr";
    else if ( dataType == "departures" || dataType == "journeysDep" )
	sDataType = "dep";

    sCity = timetableAccessorInfo().mapCityNameToValue(sCity);

    // Encode city and stop
    if ( charsetForUrlEncoding().isEmpty() ) {
	sCity = QString::fromAscii(QUrl::toPercentEncoding(sCity));
	sStartStopName = QString::fromAscii( QUrl::toPercentEncoding(sStartStopName) );
	sTargetStopName = QString::fromAscii( QUrl::toPercentEncoding(sTargetStopName) );
    } else {
	sCity = toPercentEncoding( sCity, charsetForUrlEncoding() );
	sStartStopName = toPercentEncoding( sStartStopName, charsetForUrlEncoding() );
	sTargetStopName = toPercentEncoding( sTargetStopName, charsetForUrlEncoding() );
    }

    // Construct the url from the "raw" url by replacing values
    if ( useSeparateCityValue() )
	sRawUrl = sRawUrl.replace( "{city}", sCity );

    sRawUrl = sRawUrl.replace( "{time}", sTime )
		     .replace( "{maxCount}", QString("%1").arg(maxCount) )
		     .replace( "{startStop}", sStartStopName )
		     .replace( "{targetStop}", sTargetStopName )
		     .replace( "{dataType}", sDataType );

    QRegExp rx = QRegExp("\\{date:([^\\}]*)\\}", Qt::CaseInsensitive);
    if ( rx.indexIn(sRawUrl) != -1 )
	sRawUrl.replace( rx, dateTime.date().toString(rx.cap(1)) );
    
    rx = QRegExp("\\{dep=([^\\|]*)\\|arr=([^\\}]*)\\}", Qt::CaseInsensitive);
    if ( rx.indexIn(sRawUrl) != -1 ) {
	if ( sDataType == "arr" )
	    sRawUrl.replace( rx, rx.cap(2) );
	else
	    sRawUrl.replace( rx, rx.cap(1) );
    }
    
    return KUrl( sRawUrl );
}

QString TimetableAccessor::gethex( ushort decimal ) {
    QString hexchars = "0123456789ABCDEFabcdef";
    return QChar('%') + hexchars[decimal >> 4] + hexchars[decimal & 0xF];
}

QString TimetableAccessor::toPercentEncoding( const QString &str, const QByteArray &charset ) {
    QString unreserved = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_.~";
//     QString reserved = "!*'();:@&=+$,/?%#[]";
    QString encoded;

    QByteArray ba = QTextCodec::codecForName(charset)->fromUnicode(str);
    for ( int i = 0; i < ba.length(); ++i ) {
	char ch = ba[i];
	if ( unreserved.indexOf(ch) != -1 ) {
	    encoded += ch;
// 	    qDebug() << "  Unreserved char" << encoded;
	} else if ( ch < 0 ) {
	    encoded += gethex(256 + ch);
// 	    qDebug() << "  Encoding char < 0" << encoded;
	} else {
	    encoded += gethex(ch);
// 	    qDebug() << "  Encoding char >= 0" << encoded;
	}
    }

    return encoded;
}

bool TimetableAccessor::parseDocument( const QByteArray &document,
				       QList<PublicTransportInfo*> *journeys,
				       GlobalTimetableInfo *globalInfo,
				       ParseDocumentMode parseDocumentMode ) {
    Q_UNUSED( document );
    Q_UNUSED( journeys );
    Q_UNUSED( globalInfo );
    Q_UNUSED( parseDocumentMode );
    return false;
}

bool TimetableAccessor::parseDocumentPossibleStops( const QByteArray &document,
						    QStringList *stops,
						    QHash<QString,QString> *stopToStopId,
						    QHash<QString,int> *stopToStopWeight ) {
    Q_UNUSED( document );
    Q_UNUSED( stops );
    Q_UNUSED( stopToStopId );
    Q_UNUSED( stopToStopWeight );
    return false;
}

QString TimetableAccessor::departuresRawUrl() const {
    return m_info.departureRawUrl();
}

QString TimetableAccessor::stopSuggestionsRawUrl() const {
    return m_info.stopSuggestionsRawUrl();
}

QByteArray TimetableAccessor::charsetForUrlEncoding() const {
    return m_info.charsetForUrlEncoding();
}




