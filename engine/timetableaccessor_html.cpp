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
#include "timetableaccessor_html_infos.h"
#include <qtextcodec.h>
#include <QMimeData>
#include <qmath.h>

TimetableAccessorHtml::TimetableAccessorHtml( TimetableAccessorInfo info )
    : TimetableAccessor(), m_preData(0) {
    m_info = info;
}

TimetableAccessorHtml::TimetableAccessorHtml( ServiceProvider serviceProvider )
    : TimetableAccessor(),
    m_preData(0) {
    switch ( serviceProvider )
    {
	case DB:
	    m_info = TimetableAccessorInfo_Db();
	    break;

	case BVG:
	    m_info = TimetableAccessorInfo_Bvg();
	    break;

	case DVB:
	    m_info = TimetableAccessorInfo_Dvb();
	    break;

	case Fahrplaner:
	    m_info = TimetableAccessorInfo_Fahrplaner();
	    break;

	case NASA:
	    m_info = TimetableAccessorInfo_Nasa();
	    break;

	case VRN:
	    m_info = TimetableAccessorInfo_Vrn();
	    break;

	case VVS:
	    m_info = TimetableAccessorInfo_Vvs();
	    break;

	case OEBB:
	    m_info = TimetableAccessorInfo_Oebb();
	    break;

	case SBB:
	    m_info = TimetableAccessorInfo_Sbb();
	    break;

	case IMHD:
	    m_info = TimetableAccessorInfo_Imhd();
	    break;

	case IDNES:
	    m_info = TimetableAccessorInfo_Idnes();
	    break;

	case PKP:
	    m_info = TimetableAccessorInfo_Pkp();
	    break;

	default:
	    qDebug() << "TimetableAccessorHtml::TimetableAccessorHtml" << "Not an HTML accessor?" << serviceProvider;
    }
}

bool TimetableAccessorHtml::parseDocumentPre( QString document ){
    qDebug() << "TimetableAccessorHtml::parseDocumentPre";
    m_preData = new QMap< QString, QString >();

    QRegExp rx = m_info.searchDeparturesPre()->regExp();
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

    // Preparse the document if there is a regexp to do so.
    // This preparsing gets a QMap from one DepartureInformation to another to get values from this QMap when parsing the document
    bool hasPreData = false;
    TimetableInformation keyInfo = Nothing, valueInfo = Nothing;
    if ( m_info.searchDeparturesPre() && (hasPreData = parseDocumentPre( document )) ) {
	QList< TimetableInformation > infos = m_info.searchDeparturesPre()->infos();
	Q_ASSERT( infos.length() >= 2 );
	keyInfo = infos[0];
	valueInfo = infos[1];
    }

    qDebug() << "TimetableAccessorHtml::parseDocument" << "Parsing..." << (parseDocumentMode == ParseForJourneys ? "searching for journeys" : "searching for departures / arrivels");

    QRegExp rx = parseDocumentMode == ParseForDeparturesArrivals
			    ? m_info.searchDepartures().regExp() : m_info.searchJourneys().regExp();
    QList<TimetableInformation> infos = parseDocumentMode == ParseForDeparturesArrivals
			    ? m_info.searchDepartures().infos() : m_info.searchJourneys().infos();
    int pos = 0, count = 0;
    while ((pos = rx.indexIn(document, pos)) != -1)
    {
        if (!rx.isValid()) {
            qDebug() << "TimetableAccessorHtml::parseDocument" << "Parse error";
	    if ( m_preData ) {
		delete m_preData;
		m_preData = NULL;
	    }
            return false; // parse error
        }

	QList< TimetableInformation > infosToGet;
	infosToGet << DepartureDate << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Target << Platform << Delay << DelayReason << JourneyNews << JourneyNewsOther << NoMatchOnSchedule << StopName << StopID << DepartureHourPrognosis<< DepartureMinutePrognosis << Duration << StartStopName<< StartStopID << TargetStopName << TargetStopID << ArrivalDate << ArrivalHour<< ArrivalMinute<< Changes<< TypesOfVehicleInJourney << Pricing;

	QMap< TimetableInformation, QVariant > data;
	data.insert( Delay, "-1" );
	foreach ( TimetableInformation info, infosToGet ) {
	    if ( infos.contains(info) )
		postProcessMatchedData( info, rx.cap( infos.indexOf( info ) + 1 ).trimmed(), &data );
	}

	// Get missing data from preparsed matches
	if ( !data.contains( valueInfo ) && data.contains( keyInfo ) ) {
	    qDebug() << "TimetableAccessorHtml::parseDocument" << "Get missing data from preparsed matches"<<m_preData->value( data[keyInfo].toString() );
	    postProcessMatchedData( valueInfo, m_preData->value( data[keyInfo].toString() ), &data );
	}

	// Compute delay from departure time and departure time prognosis
	if ( data[Delay] == -1 && data.contains(DepartureHourPrognosis) && data.contains(DepartureMinutePrognosis) && data.contains(DepartureHour) && data.contains( DepartureMinute) ) {
	    data[Delay] = qFloor( (float)QTime(data[DepartureHour].toInt(), data[DepartureMinute].toInt()).secsTo( QTime(data[DepartureHourPrognosis].toInt(), data[DepartureMinutePrognosis].toInt()) ) / 60.0f );
	}

	if ( !data.contains( DepartureDate ) )
	    data.insert( DepartureDate, QDate::currentDate() );

	if ( parseDocumentMode == ParseForJourneys ) {
	    // TODO: Correct date calculations (if data contains DepartureDate)?
// 	    qDebug() << "duration" << data.contains(Duration) << ", depHour" << data.contains(DepartureHour) << ", depMin" << data.contains(DepartureMinute);

	    if ( !data.contains(Duration) && data.contains(ArrivalHour) && data.contains(ArrivalMinute) && data.contains(DepartureHour) && data.contains( DepartureMinute) )
	    { // Compute duration from departure and arrival time
		data.insert( Duration, qFloor( (float)QTime(data[DepartureHour].toInt(), data[DepartureMinute].toInt()).secsTo( QTime(data[ArrivalHour].toInt(), data[ArrivalMinute].toInt()) ) / 60.0f ) );
	    } else if ( (!data.contains(ArrivalHour) || !data.contains(ArrivalMinute)) && data.contains(Duration) && data.contains(DepartureHour) && data.contains( DepartureMinute) )
	    { // Compute arrival time from departure time and duration
		QTime arrival = QTime( data[DepartureHour].toInt(), data[DepartureMinute].toInt() ).addSecs( data[Duration].toInt() * 60 );
		data.insert( ArrivalHour, arrival.hour() );
		data.insert( ArrivalMinute, arrival.minute() );
	    } else if ( (!data.contains(ArrivalHour) || !data.contains(ArrivalMinute)) && data.contains(Duration) && data.contains(DepartureHour) && data.contains( DepartureMinute) )
	    { // Compute departure time from arrival time and duration
		QTime departure = QTime( data[ArrivalHour].toInt(), data[ArrivalMinute].toInt() ).addSecs( -data[Duration].toInt() * 60 );
		data.insert( DepartureHour, departure.hour() );
		data.insert( DepartureMinute, departure.minute() );
	    }

	    if ( !data.contains( StartStopName ) )
		data.insert( StartStopName, "" );
	    if ( !data.contains( TargetStopName ) )
		data.insert( TargetStopName, "" );

	    if ( !data.contains( DepartureDate ) )
		data.insert( DepartureDate, QDate::currentDate() );
	    if ( !data.contains( ArrivalDate ) )
		data.insert( ArrivalDate, data[DepartureDate] );
	}

// 	qDebug() << " Info =" << data[TransportLine] << data[Direction] << QString("%1:%2").arg( data[DepartureHour] ).arg( data[DepartureMinute] ) << data[Platform] <<  data[Delay].toInt() << data[DelayReason];

	// Create DepartureInfo item and append it to the journey list
	if ( parseDocumentMode == ParseForDeparturesArrivals )
	    journeys->append( new DepartureInfo( data[TransportLine].toString(),
		static_cast<VehicleType>(data[TypeOfVehicle].toInt()), data[Target].toString(),
		QTime::currentTime(), QTime(data[DepartureHour].toInt(), data[DepartureMinute].toInt()),
		data[TransportLine].toString().isEmpty() ? false : data[TransportLine].toString().at(0) == 'N',
		false, data[Platform].toString(), data[Delay].toInt(), data[DelayReason].toString(), data[JourneyNewsOther].toString() ) );
	else /*if ( parseDocumentMode == ParseForJourneys )*/ {
	    QList<VehicleType> vehicleTypes;
	    QList<QVariant> variantList = data[TypesOfVehicleInJourney].toList();
	    foreach ( QVariant vehicleType, variantList )
		vehicleTypes.append( static_cast<VehicleType>(vehicleType.toInt()) );

// 	    qDebug() << "parsing journey list..." << QTime(data[DepartureHour].toInt(), data[DepartureMinute].toInt()) << QTime(data[ArrivalHour].toInt(), data[ArrivalMinute].toInt()) << "duration =" << data[Duration] << /*"pricing =" << data[Pricing] <<*/ "StartStopName =" << data[StartStopName] << "TargetStopName =" << data[TargetStopName] << " =" << data[TypesOfVehicleInJourney];

	    journeys->append( new JourneyInfo( vehicleTypes, data[StartStopName].toString(), data[TargetStopName].toString(),
		QDateTime( data[DepartureDate].toDate(), QTime(data[DepartureHour].toInt(), data[DepartureMinute].toInt()) ),
		QDateTime( data[ArrivalDate].toDate(), QTime(data[ArrivalHour].toInt(), data[ArrivalMinute].toInt()) ),
		data[Duration].toInt(), data[Changes].toInt(), data[Pricing].toString(), data[JourneyNewsOther].toString() ) );
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

void TimetableAccessorHtml::postProcessMatchedData( TimetableInformation info, QString matchedData, QMap< TimetableInformation, QVariant > *data ) {
    QString sDelay, sDelayReason, sJourneyNews;
    QStringList stringList;
    QList<QVariant> variantList;
    QTime time;
    QDate date;

//     qDebug() << "TimetableAccessorHtml::postProcessMatchedData" << info << matchedData;
    switch ( info )
    {
	case StartStopName:
	case StartStopID:
	case TargetStopName:
	case TargetStopID:
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
	    matchedData = TimetableAccessorHtml::decodeHtmlEntities(matchedData).trimmed();
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
	    data->insert( TypeOfVehicle, static_cast<int>(PublicTransportInfo::getVehicleTypeFromString(matchedData)) );
	    break;

	case TransportLine:
	    matchedData = matchedData.replace( QRegExp("^(bus|str|s|u|ice|re|ic)\\s*", Qt::CaseInsensitive), "" );
	    matchedData = matchedData.replace( QRegExp("\\s{2,}", Qt::CaseInsensitive), " " );

	    data->insert( TransportLine, matchedData );
	    // TODO: if ( sType.isEmpty() ) getTypeOfVehicleFromLineString();
	    // TODO: strip away RB/RE/ICE/Tram from beginning and "S-Bahn"/"RegionalBahn"/"InterCity"/"RegionalExpress"/"InterCityExpress"
	    break;

	case Target:
	    matchedData = TimetableAccessorHtml::decodeHtmlEntities(matchedData);
	    if ( serviceProvider() == DB && !m_curCity.isEmpty() ) // TODO: needed?
		matchedData = matchedData.remove( QRegExp(QString(",?\\s?%1$").arg(m_curCity)) );

	    data->insert( Target, matchedData );
	    break;

	case Delay:
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

	default:
	    break;
    }
}

bool TimetableAccessorHtml::parseJourneyNews( const QString sJourneyNews, QString *sDelay, QString *sDelayReason, QString *sJourneyNewsOther ) const {
    *sDelay = "-1";
    *sDelayReason = "";
    *sJourneyNewsOther = "";
    foreach ( TimetableRegExpSearch search, m_info.searchJourneyNews() )
    {
// 	qDebug() << "     TimetableAccessorHtml::parseJourneyNews" << "Testing" << search.regExp();
	QRegExp rx = search.regExp();
	if ( rx.indexIn(sJourneyNews) != -1 && rx.isValid() )
	{
// 	    qDebug() << "     TimetableAccessorHtml::parseJourneyNews" << "Matched!";
	    if ( search.infos().contains( NoMatchOnSchedule ) )
		*sDelay = "0";
	    else
	    {
		if ( search.infos().contains( Delay ) )
		    *sDelay = rx.cap( search.infos().indexOf( Delay ) + 1 ).trimmed();
		if ( search.infos().contains( DelayReason ) )
		{
		    *sDelayReason = rx.cap( search.infos().indexOf( DelayReason ) + 1 ).trimmed();
		    *sDelayReason = TimetableAccessorHtml::decodeHtmlEntities(*sDelayReason);
		}
	    }
	    if ( search.infos().contains( JourneyNewsOther ) )
	    {
		*sJourneyNewsOther = rx.cap( search.infos().indexOf( JourneyNewsOther ) + 1 ).trimmed();
		*sJourneyNewsOther = TimetableAccessorHtml::decodeHtmlEntities(*sJourneyNewsOther);
	    }
	}
    }
//     qDebug() << "     TimetableAccessorHtml::parseJourneyNews" << sJourneyNews << *iDelay<< *sDelayReason<< *sJourneyNewsOther;
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

bool TimetableAccessorHtml::parseDocumentPossibleStops( const QByteArray document, QMap< QString, QString >* stops ) {
    m_document = document;
    return parseDocumentPossibleStops( stops );
}

bool TimetableAccessorHtml::parseDocumentPossibleStops( QMap<QString,QString>* stops ) const {
    if ( m_info.regExpSearchPossibleStopsRanges().isEmpty() )
	return false; // possible stop lists not supported by accessor or service provider

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

	QRegExp rx = m_info.searchPossibleStops()[i].regExp();
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




