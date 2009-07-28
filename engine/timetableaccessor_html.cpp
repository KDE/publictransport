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

#include "timetableaccessor_html.h"
#include <QRegExp>
#include <QDebug>
// #include "timetableaccessor_html_infos.h"
#include <qtextcodec.h>
#include <QMimeData>
#include <qmath.h>

TimetableAccessorHtml::TimetableAccessorHtml( TimetableAccessorInfo info )
    : TimetableAccessor(), m_preData(0)
{
    m_info = info;
}

bool TimetableAccessorHtml::parseDocumentPre( QString document ) {
    qDebug() << "TimetableAccessorHtml::parseDocumentPre";
    m_preData = new QHash< QString, QString >();

    const QRegExp rx = m_info.searchDeparturesPre()->regExp();
    int pos = 0, count = 0;
    while ((pos = rx.indexIn(document, pos)) != -1)
    {
        if (!rx.isValid()) {
            qDebug() << "TimetableAccessorHtml::parseDocumentPre" << "Parse error";
            return false; // parse error
        }

	QString sKey, sValue;
	sKey = rx.cap( 1 ).trimmed();
	sValue = rx.cap( 2 ).trimmed();

	m_preData->insert( sKey, sValue );

	++count;
        pos += rx.matchedLength();
    }

    return count > 0;
}

QString TimetableAccessorHtml::decodeHtml ( QByteArray document ) {
    // Get charset of the received document and convert it to a unicode QString
    // First parse the charset with a regexp to get a fallback charset if QTextCodec::codecForHtml doesn't find the charset
    QString sDocument = QString(document);
    QTextCodec *textCodec;
    QRegExp rxCharset("(?:<head>.*<meta http-equiv=\"Content-Type\" content=\"text/html; charset=)([^\"]*)(?:\"[^>]*>)", Qt::CaseInsensitive);
    rxCharset.setMinimal( true );
    if ( rxCharset.indexIn(sDocument) != -1 && rxCharset.isValid() )
	textCodec = QTextCodec::codecForName( rxCharset.cap(1).trimmed().toUtf8() );
    else
	textCodec = QTextCodec::codecForName( "UTF-8" );
    sDocument = QTextCodec::codecForHtml( document, textCodec )->toUnicode( document );

    return sDocument;
}

bool TimetableAccessorHtml::parseDocument( QList<PublicTransportInfo*> *journeys, ParseDocumentMode parseDocumentMode ) {
    QString document = decodeHtml( m_document );

    // Performance(?): Cut everything before "<body>" from the document
    document = document.mid( document.indexOf("<body>", 0, Qt::CaseInsensitive) );

    // Preparse the document if there is a regexp to do so.
    // This preparsing gets a QHash from one DepartureInformation to another to get values from this QHash when parsing the document
    bool hasPreData = false;
    TimetableInformation keyInfo = Nothing, valueInfo = Nothing;
    if ( m_info.searchDeparturesPre() && (hasPreData = parseDocumentPre( document )) ) {
	QList< TimetableInformation > infos = m_info.searchDeparturesPre()->infos();
	Q_ASSERT( infos.length() >= 2 );
	keyInfo = infos[0];
	valueInfo = infos[1];
    }

    qDebug() << "TimetableAccessorHtml::parseDocument" << "Parsing..." << (parseDocumentMode == ParseForJourneys ? "searching for journeys" : "searching for departures / arrivals");

    const QRegExp rx = parseDocumentMode == ParseForDeparturesArrivals
			    ? m_info.searchDepartures().regExp() : m_info.searchJourneys().regExp();
    QList<TimetableInformation> infos = parseDocumentMode == ParseForDeparturesArrivals
			    ? m_info.searchDepartures().infos() : m_info.searchJourneys().infos();
    int pos = 0, count = 0;
    QList<TimetableInformation> infosToGet = QList<TimetableInformation>() << DepartureDate << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Target << Platform << Delay << DelayReason << JourneyNews << JourneyNewsOther << JourneyNewsLink << NoMatchOnSchedule << StopName << StopID << DepartureHourPrognosis<< DepartureMinutePrognosis << Duration << StartStopName<< StartStopID << TargetStopName << TargetStopID << ArrivalDate << ArrivalHour<< ArrivalMinute<< Changes<< TypesOfVehicleInJourney << Pricing << Operator << DepartureAMorPM << DepartureAMorPMPrognosis << ArrivalAMorPM << Status;

    // TODO (performance): fill calculateMissingValues in TimetableAccessorInfo and store there
    QList<CalculateMissingValue> calculateMissingValues;
    if ( !infos.contains(Delay) && infos.contains(DepartureHourPrognosis) &&
	infos.contains(DepartureMinutePrognosis) && infos.contains(DepartureHour) &&
	infos.contains( DepartureMinute) )
    {
	calculateMissingValues << CalculateDelayFromDepartureAndPrognosis;
    }

    if ( !infos.contains( DepartureDate ) )
	calculateMissingValues << CalculateDepartureDate;
    if ( parseDocumentMode == ParseForJourneys ) {
	if ( !infos.contains(Duration) && infos.contains(ArrivalHour) &&
	    infos.contains(ArrivalMinute) && infos.contains(DepartureHour) &&
	    infos.contains( DepartureMinute) ) {
	    calculateMissingValues << CalculateDurationFromDepartureAndArrival;
	} else if ( (!infos.contains(ArrivalHour) || !infos.contains(ArrivalMinute)) &&
	    infos.contains(Duration) && infos.contains(DepartureHour) && infos.contains( DepartureMinute) ) {
	    calculateMissingValues << CalculateArrivalFromDepartureAndDuration;
	} else if ( (!infos.contains(DepartureHour) || !infos.contains(DepartureMinute)) &&
	    infos.contains(Duration) && infos.contains(ArrivalHour) && infos.contains( ArrivalMinute) ) {
	    calculateMissingValues << CalculateDepartureFromArrivalAndDuration;
	}

	if ( !infos.contains( ArrivalDate ) )
	    calculateMissingValues << CalculateArrivalDateFromDepartureDate;
    }

    // Process each match of the regular expression
    while ( (pos = rx.indexIn(document, pos)) != -1 )
    {
	QHash< TimetableInformation, QVariant > data;
	data.insert( Delay, "-1" );
	foreach ( TimetableInformation info, infosToGet ) {
	    if ( infos.contains(info) )
		postProcessMatchedData( info, rx.cap( infos.indexOf( info ) + 1 ).trimmed(), &data );
	}

	// Get missing data from preparsed matches
	if ( hasPreData && data.contains(keyInfo) &&
	    (!data.contains(valueInfo) || (valueInfo == TypeOfVehicle && data[valueInfo] == Unknown)) ) {
	    qDebug() << "TimetableAccessorHtml::parseDocument" << "Get missing data from preparsed matches"<<m_preData->value( data[keyInfo].toString() );
	    postProcessMatchedData( valueInfo, m_preData->value( data[keyInfo].toString() ), &data );
	}

	foreach( CalculateMissingValue calculation, calculateMissingValues ) {
	    QTime time;
	    switch( calculation ) {
		case CalculateDelayFromDepartureAndPrognosis:
		    qDebug() << "Calculating delay" << data[DepartureHourPrognosis] << data[DepartureMinutePrognosis];

		    data[Delay] = qFloor( (float)QTime(data[DepartureHour].toInt(), data[DepartureMinute].toInt()).secsTo( QTime(data[DepartureHourPrognosis].toInt(), data[DepartureMinutePrognosis].toInt()) ) / 60.0f );
		    break;
		case CalculateDepartureDate:
		    data.insert( DepartureDate, QDate::currentDate() ); // TODO: guess date if it's not parsed, not only currentDate
		    break;
		case CalculateArrivalDateFromDepartureDate:
		    data.insert( ArrivalDate, data[DepartureDate] );
		    break;
		case CalculateDurationFromDepartureAndArrival:
		    data.insert( Duration, qFloor( (float)QTime(data[DepartureHour].toInt(), data[DepartureMinute].toInt()).secsTo( QTime(data[ArrivalHour].toInt(), data[ArrivalMinute].toInt()) ) / 60.0f ) );
		    break;
		case CalculateArrivalFromDepartureAndDuration:
		    time = QTime( data[DepartureHour].toInt(), data[DepartureMinute].toInt() ).addSecs( data[Duration].toInt() * 60 );
		    data.insert( ArrivalHour, time.hour() );
		    data.insert( ArrivalMinute, time.minute() );
		    break;
		case CalculateDepartureFromArrivalAndDuration:
		    time = QTime( data[ArrivalHour].toInt(), data[ArrivalMinute].toInt() ).addSecs( -data[Duration].toInt() * 60 );
		    data.insert( DepartureHour, time.hour() );
		    data.insert( DepartureMinute, time.minute() );
		    break;
	    }
	}

	if ( parseDocumentMode == ParseForJourneys ) {
	    if ( !data.contains( StartStopName ) )
		data.insert( StartStopName, "" );
	    if ( !data.contains( TargetStopName ) )
		data.insert( TargetStopName, "" );
	    if ( data.contains(DepartureDate) ) {
		if ( !data.contains(ArrivalDate) )
		    data.insert( ArrivalDate, data[DepartureDate] );
		else if ( data[ArrivalDate].toString().isEmpty() )
		    data[ArrivalDate] = data[DepartureDate];
	    }
	}

	if ( data.contains(JourneyNewsLink) && !data.contains(JourneyNewsOther) )
	    data[JourneyNewsOther] = data[JourneyNewsLink];

	if ( !data.contains(TypeOfVehicle) && m_info.defaultVehicleType() != Unknown ) {
	    qDebug() << "TimetableAccessorHtml::parseDocument" << "Setting vehicle type to default" << m_info.defaultVehicleType();
	    data[TypeOfVehicle] = m_info.defaultVehicleType();
	}

// 	qDebug() << " Info =" << data[TransportLine] << data[Direction] << QString("%1:%2").arg( data[DepartureHour] ).arg( data[DepartureMinute] ) << data[Platform] <<  data[Delay].toInt() << data[DelayReason];

// TODO Status, AMorPM
	// Create DepartureInfo item and append it to the journey list
	if ( parseDocumentMode == ParseForDeparturesArrivals )
	    journeys->append( new DepartureInfo( data[TransportLine].toString(),
		static_cast<VehicleType>(data[TypeOfVehicle].toInt()), data[Target].toString(),
		QTime::currentTime(), QTime(data[DepartureHour].toInt(), data[DepartureMinute].toInt()),
		data[TransportLine].toString().isEmpty() ? false : data[TransportLine].toString().at(0) == 'N',
		false, data[Platform].toString(), data[Delay].toInt(), data[DelayReason].toString(), data[JourneyNewsOther].toString(), data[Operator].toString() ) );
	else /*if ( parseDocumentMode == ParseForJourneys )*/ {
	    QList<VehicleType> vehicleTypes;
	    QList<QVariant> variantList = data[TypesOfVehicleInJourney].toList();
	    foreach ( QVariant vehicleType, variantList )
		vehicleTypes.append( static_cast<VehicleType>(vehicleType.toInt()) );

// 	    qDebug() << "parsing journey list..." << QTime(data[DepartureHour].toInt(), data[DepartureMinute].toInt()) << QTime(data[ArrivalHour].toInt(), data[ArrivalMinute].toInt()) << "duration =" << data[Duration] << /*"pricing =" << data[Pricing] <<*/ "StartStopName =" << data[StartStopName] << "TargetStopName =" << data[TargetStopName] << " =" << data[TypesOfVehicleInJourney];

	    journeys->append( new JourneyInfo( vehicleTypes,
		data[StartStopName].toString(), data[TargetStopName].toString(),
		QDateTime( data[DepartureDate].toDate(), QTime(data[DepartureHour].toInt(), data[DepartureMinute].toInt()) ),
		QDateTime( data[ArrivalDate].toDate(), QTime(data[ArrivalHour].toInt(), data[ArrivalMinute].toInt()) ),
		data[Duration].toInt(), data[Changes].toInt(), data[Pricing].toString(),
		data[JourneyNewsOther].toString(), data[Operator].toString() ) );
	}

        pos += rx.matchedLength();
	++count;
    }

    // Delete preparsed data
    if ( m_preData ) {
	delete m_preData;
	m_preData = NULL;
    }
    if ( count == 0 )
	qDebug() << "TimetableAccessorHtml::parseDocument" << "The regular expression didn't match anything";
    return count > 0;
}

void TimetableAccessorHtml::postProcessMatchedData( TimetableInformation info, QString matchedData, QHash< TimetableInformation, QVariant > *data ) {
    QString sDelay, sDelayReason, sJourneyNews, sVehicleType;
    QStringList stringList;
    QList<QVariant> variantList;
    QTime time;
    QDate date;
    QRegExp rx;

//     qDebug() << "TimetableAccessorHtml::postProcessMatchedData" << info << matchedData;
    switch ( info ) {
	case StartStopName:
	case StartStopID:
	case TargetStopName:
	case TargetStopID:
	case DepartureAMorPM:
	case DepartureAMorPMPrognosis:
	case ArrivalAMorPM:
	    data->insert( info, matchedData );
	    break;

	case DepartureHourPrognosis:
	case DepartureMinutePrognosis:
	case ArrivalHour:
	case ArrivalMinute:
	case DepartureHour:
	case DepartureMinute:
	case Changes:
	    data->insert( info, matchedData.toInt() );
	    break;

	case Platform:
	case DelayReason:
	case JourneyNewsOther:
	case Pricing:
	case Operator:
	case Status:
	    matchedData = TimetableAccessorHtml::decodeHtmlEntities(matchedData).trimmed();
	    data->insert( info, matchedData );
	    break;

	case JourneyNewsLink:
	    matchedData = TimetableAccessorHtml::decodeHtmlEntities(matchedData).trimmed();
	    if ( matchedData.startsWith('/') )
		matchedData.prepend( m_info.url() );
	    data->insert( info, matchedData );
	    break;

	case Duration:
	    time = QTime::fromString(matchedData, "h:mm");
	    data->insert( info, time.hour() * 60 + time.minute() ); // TODO: autodetect time format
	    break;

	case DepartureDate:
	case ArrivalDate:
	    date = QDate::fromString(matchedData, "dd.MM.yy");
	    if ( date.year() == QDate::currentDate().year() - 100 )
		date.setDate( date.year() + 100, date.month(), date.day() );
	    data->insert( info, date ); // TODO: autodetect date format
	    break;

	case TypesOfVehicleInJourney:
	    stringList = matchedData.split(',', QString::SkipEmptyParts);
	    foreach ( QString sTypeOfVehicle, stringList )
		variantList.append( static_cast<int>(PublicTransportInfo::getVehicleTypeFromString(sTypeOfVehicle)) );
	    data->insert( TypesOfVehicleInJourney, variantList );
	    break;

	case TypeOfVehicle:
	    data->insert( TypeOfVehicle,
		static_cast<int>(PublicTransportInfo::getVehicleTypeFromString(matchedData)) );
	    break;

	case TransportLine:
	    rx = QRegExp("^(bus|tro|tram|str|s|u|m)\\s*", Qt::CaseInsensitive);
	    rx.indexIn(matchedData);
	    sVehicleType = rx.cap();
	    if ( !data->contains(TypeOfVehicle) ) {
		data->insert( TypeOfVehicle,
		    static_cast<int>(PublicTransportInfo::getVehicleTypeFromString(sVehicleType)) );
	    } else if ( data->value(TypeOfVehicle) == Unknown ) {
		data->operator[](TypeOfVehicle) =
		    static_cast<int>(PublicTransportInfo::getVehicleTypeFromString(sVehicleType));
	    }

	    matchedData = matchedData.trimmed();
	    matchedData = matchedData.remove( QRegExp("^(bus|tro|tram|str)", Qt::CaseInsensitive) );
	    matchedData = matchedData.replace( QRegExp("\\s{2,}"), " " );

	    if ( matchedData.contains( "F&#228;hre" ) )
		data->insert( TypeOfVehicle, Ferry );

	    data->insert( TransportLine, matchedData );
	    // TODO: strip away from beginning: "S-Bahn"/"RegionalBahn"/"InterCity"/"RegionalExpress"/"InterCityExpress"?
	    break;

	case Target:
	    matchedData = TimetableAccessorHtml::decodeHtmlEntities(matchedData);
	    data->insert( Target, matchedData );
	    break;

	case Delay:
	    if ( !matchedData.trimmed().isEmpty() )
		data->operator[](Delay) = matchedData;
	    break;

	case JourneyNews:
	    sJourneyNews = matchedData;
	    parseJourneyNews( sJourneyNews, &sDelay, &sDelayReason, &sJourneyNews );
	    data->operator[](Delay) = sDelay;
	    if ( !sJourneyNews.isEmpty() )
		data->insert( JourneyNewsOther, sJourneyNews );
	    if ( !sDelayReason.isEmpty() )
		data->insert( DelayReason, sDelayReason );
	    break;

	case NoMatchOnSchedule:
	    // Only if something matched and no delay was parsed previously
	    if ( !matchedData.isEmpty() && data->operator[](Delay).toString() == "-1" )
		data->operator[](Delay) = "0";
	    break;

	default:
	    qDebug() << "TimetableAccessorHtml::postProcessMatchedData" << "TimetableInformation with value" << info << "has no processing instructions and will be omitted! Matched data is" << matchedData;
	    break;
    }
}

bool TimetableAccessorHtml::parseJourneyNews( const QString sJourneyNews, QString *sDelay, QString *sDelayReason, QString *sJourneyNewsOther ) const {
//     if ( !sJourneyNews.isEmpty() )
// 	qDebug() << "TimetableAccessorHtml::parseJourneyNews" << "Journey news string:" << sJourneyNews;

    *sDelay = "-1";
    *sDelayReason = "";
    *sJourneyNewsOther = "";
    foreach ( TimetableRegExpSearch search, m_info.searchJourneyNews() ) {
// 	if ( !sJourneyNews.isEmpty() )
// 	    qDebug() << "     TimetableAccessorHtml::parseJourneyNews" << "Testing" << search.regExp();

	const QRegExp rx = search.regExp();
	if ( rx.indexIn(sJourneyNews) != -1 && rx.isValid() ) {
// 	    qDebug() << "     TimetableAccessorHtml::parseJourneyNews" << "Matched!" << "(" << rx.pattern() << ")";

	    if ( search.infos().contains( NoMatchOnSchedule ) ) {
// 		qDebug() << "NoMatchOnSchedule";
		*sDelay = "0";
	    } else {
		if ( search.infos().contains( Delay ) ) {
// 		    qDebug() << "Delay";
		    *sDelay = rx.cap( search.infos().indexOf( Delay ) + 1 ).trimmed();
		}
		if ( search.infos().contains( DelayReason ) ) {
// 		    qDebug() << "DelayReason";
		    *sDelayReason = rx.cap( search.infos().indexOf( DelayReason ) + 1 ).trimmed();
		    *sDelayReason = TimetableAccessorHtml::decodeHtmlEntities(*sDelayReason);
		}
	    }
	    if ( search.infos().contains( JourneyNewsOther ) ) {
		*sJourneyNewsOther = rx.cap( search.infos().indexOf( JourneyNewsOther ) + 1 ).trimmed();
		*sJourneyNewsOther = TimetableAccessorHtml::decodeHtmlEntities(*sJourneyNewsOther);
	    }
	} // if found and is valid
    } // foreach

//     if ( !sJourneyNews.isEmpty() )
// 	qDebug() << "     TimetableAccessorHtml::parseJourneyNews" << sJourneyNews << *sDelay<< *sDelayReason<< *sJourneyNewsOther;
    return true;
}

// Couldn't find such a function anywhere in Qt or KDE (but it must be there somewhere...)
QString TimetableAccessorHtml::decodeHtmlEntities( QString html ) {
    QRegExp rx("(?:&#)([0-9]+)(?:;)");
    rx.setMinimal(true);
    int pos = 0;
    while ((pos = rx.indexIn(html, pos)) != -1)
    {
	int charCode = rx.cap(1).toInt();
	QChar ch(charCode);
	html = html.replace( QString("&#%1;").arg(charCode), ch );
    }

    html = html.replace( "&nbsp;", " " );
    html = html.replace( "&amp;", "&" );
    html = html.replace( "&lt;", "<" );
    html = html.replace( "&gt;", ">" );
    html = html.replace( "&szlig;", "ß" );
    html = html.replace( "&auml;", "ä" );
    html = html.replace( "&Auml;", "Ä" );
    html = html.replace( "&ouml;", "ö" );
    html = html.replace( "&Ouml;", "Ö" );
    html = html.replace( "&uuml;", "ü" );
    html = html.replace( "&Uuml;", "Ü" );

    return html;
}

bool TimetableAccessorHtml::parseDocumentPossibleStops( const QByteArray document, QHash< QString, QString >* stops ) {
    m_document = document;
    return parseDocumentPossibleStops( stops );
}

bool TimetableAccessorHtml::parseDocumentPossibleStops( QHash<QString,QString>* stops ) const {
    if ( m_info.regExpSearchPossibleStopsRanges().isEmpty() ) {
	qDebug() << "TimetableAccessorHtml::parseDocumentPossibleStops" << "Possible stop lists not supported by accessor or service provider";
	return false;
    }

    QString document = decodeHtml( m_document );
    qDebug() << "TimetableAccessorHtml::parseDocumentPossibleStops" << "Parsing for a list of possible stop names...";

    bool matched = false;
    int pos = 0;
    for( int i = 0; i < m_info.regExpSearchPossibleStopsRanges().count(); ++i ) {
	QRegExp rxRange(m_info.regExpSearchPossibleStopsRanges()[i], Qt::CaseInsensitive);
	rxRange.setMinimal( true );
	if ( rxRange.indexIn(document) == -1 || !rxRange.isValid() )
	    continue;

	QString possibleStopRange = rxRange.cap(1);
	matched = true;
	//     qDebug() << "TimetableAccessorHtml::parseDocumentPossibleStops" << "Possible stop range = " << possibleStopRange;

	const QRegExp rx = m_info.searchPossibleStops()[i].regExp();
	while ( (pos = rx.indexIn(possibleStopRange, pos)) != -1 ) {
	    if (!rx.isValid()) {
		qDebug() << "TimetableAccessorHtml::parseDocumentPossibleStops" << "Parse error";
		return false; // parse error
	    }

	    QList< TimetableInformation > infos = m_info.searchPossibleStops()[i].infos();
	    if ( infos.isEmpty() )
		return false;
	    else
	    {
		QString sStopName, sStopID;
		if ( infos.contains(StopName) )
		{
		    sStopName = rx.cap( infos.indexOf( StopName ) + 1 );
		    sStopName = TimetableAccessorHtml::decodeHtmlEntities( sStopName );
		}
		if ( infos.contains(StopID) )
		    sStopID = rx.cap( infos.indexOf( StopID ) + 1 );

		stops->insert( sStopName, sStopID );
	    }
	    pos += rx.matchedLength();
	} // while ( (pos = rx.indexIn(document, pos)) != -1 ) {
    } // for( int i = 0; i < m_info.regExpSearchPossibleStopsRanges.count(); ++i ) {

    if ( !matched ) {
	qDebug() << "TimetableAccessorHtml::parseDocumentPossibleStops" << "Possible stop ranges not matched ";
	return false;
    }

    return pos != 0;
}




