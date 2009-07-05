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

TimetableAccessorHtml::TimetableAccessorHtml( TimetableAccessorInfo info )
    : m_preData(0) {
    m_info = info;
}

TimetableAccessorHtml::TimetableAccessorHtml( ServiceProvider serviceProvider )
    : TimetableAccessor (),
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

    QRegExp rx( m_info.searchJourneysPre->regExp(), Qt::CaseInsensitive );
    rx.setMinimal(true);
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

bool TimetableAccessorHtml::parseDocument( QList<DepartureInfo> *journeys ) {
    QString document = decodeHtml( m_document );

    // Preparse the document if there is a regexp to do so.
    // This preparsing gets a QMap from one DepartureInformation to another to get values from this QMap when parsing the document
    bool hasPreData = false;
    TimetableInformation keyInfo = Nothing, valueInfo = Nothing;
    if ( m_info.searchJourneysPre )
    {
	hasPreData = parseDocumentPre( document );
	QList< TimetableInformation > infos = m_info.searchJourneysPre->infos();
	Q_ASSERT( infos.length() >= 2 );
	keyInfo = infos[0];
	valueInfo = infos[1];
    }

    qDebug() << "TimetableAccessorHtml::parseDocument" << "Parsing...";
    QRegExp rx( m_info.searchJourneys.regExp(), Qt::CaseInsensitive );
    rx.setMinimal(true);
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

	QList< TimetableInformation > infos = m_info.searchJourneys.infos();
	QList< TimetableInformation > infosToGet;
	infosToGet << DepartureHour << DepartureMinute << TypeOfVehicle << TransportLine << Direction << Platform << Delay << DelayReason << JourneyNews << JourneyNewsOther << NoMatchOnSchedule << StopName << StopID;
	QMap< TimetableInformation, QString > data;
	data.insert( Delay, "-1" );
	foreach ( TimetableInformation info, infosToGet )
	{
	    if ( infos.contains(info) )
		postProcessMatchedData( info, rx.cap( infos.indexOf( info ) + 1 ).trimmed(), &data );
	}

	// Get missing data from preparsed matches
	if ( !data.contains( valueInfo ) && data.contains( keyInfo ) )
	{
	    qDebug() << "TimetableAccessorHtml::parseDocument" << "Get missing data from preparsed matches"<<m_preData->value( data[keyInfo] );
	    postProcessMatchedData( valueInfo, m_preData->value( data[keyInfo] ), &data );
	}

// 	qDebug() << " Info =" << data[TransportLine] << data[Direction] << QString("%1:%2").arg( data[DepartureHour] ).arg( data[DepartureMinute] ) << data[Platform] <<  data[Delay].toInt() << data[DelayReason];

	// Create DepartureInfo item and append it to the journey list
	journeys->append( DepartureInfo( data[TransportLine], DepartureInfo::getVehicleTypeFromString(data[TypeOfVehicle]), data[Direction], QTime::currentTime(), QTime(data[DepartureHour].toInt(), data[DepartureMinute].toInt()), data[TransportLine].isEmpty() ? false : data[TransportLine].at(0) == 'N', false, data[Platform], data[Delay].toInt(), data[DelayReason], data[JourneyNewsOther] ) );

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

void TimetableAccessorHtml::postProcessMatchedData ( TimetableInformation info, QString matchedData, QMap< TimetableInformation, QString > *data ) {
    QString sDelay, sDelayReason, sJourneyNews;
    switch ( info )
    {
	case DepartureHour:
	    data->insert( DepartureHour, matchedData );
	    break;
	case DepartureMinute:
	    data->insert( DepartureMinute, matchedData );
	    break;
	case TypeOfVehicle:
	    data->insert( TypeOfVehicle, matchedData );
	    break;
	case TransportLine:
	    matchedData = matchedData.replace( QRegExp("^(bus|str|s|u|ice|re|ic)\\s*", Qt::CaseInsensitive), "" );
	    matchedData = matchedData.replace( QRegExp("\\s{2,}", Qt::CaseInsensitive), " " );

	    data->insert( TransportLine, matchedData );
	    // TODO: if ( sType.isEmpty() ) getTypeOfVehicleFromLineString();
	    // TODO: strip away RB/RE/ICE/Tram from beginning and "S-Bahn"/"RegionalBahn"/"InterCity"/"RegionalExpress"/"InterCityExpress"
	    break;
	case Direction:
	    matchedData = TimetableAccessorHtml::decodeHtmlEntities(matchedData);
	    if ( serviceProvider() == DB && !m_curCity.isEmpty() ) // TODO: needed?
		matchedData = matchedData.remove( QRegExp(QString(",?\\s?%1$").arg(m_curCity)) );

	    data->insert( Direction, matchedData );
	    break;
	case Platform:
	    matchedData = TimetableAccessorHtml::decodeHtmlEntities(matchedData).trimmed();
	    data->insert( Platform, matchedData );
	    break;
	case Delay:
	    data->operator[](Delay) = matchedData;
	    break;
	case DelayReason:
	    matchedData = TimetableAccessorHtml::decodeHtmlEntities(matchedData);
	    data->insert( DelayReason, matchedData );
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
	case JourneyNewsOther:
	    matchedData = TimetableAccessorHtml::decodeHtmlEntities(matchedData);
	    data->insert( JourneyNewsOther, matchedData );
	    break;

	default:
	    break;
    }
}

bool TimetableAccessorHtml::parseJourneyNews( const QString sJourneyNews, QString *sDelay, QString *sDelayReason, QString *sJourneyNewsOther ) const {
    *sDelay = "-1";
    *sDelayReason = "";
    *sJourneyNewsOther = "";
    foreach ( TimetableRegExpSearch search, m_info.searchJourneyNews )
    {
// 	qDebug() << "     TimetableAccessorHtml::parseJourneyNews" << "Testing" << search.regExp();
	QRegExp rx( search.regExp(), Qt::CaseInsensitive );
	rx.setMinimal( true );
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
QString TimetableAccessorHtml::decodeHtmlEntities( QString sHtml ) {
    QRegExp rx("(?:&#)([0-9]+)(?:;)");
    rx.setMinimal(true);
    int pos = 0;
    while ((pos = rx.indexIn(sHtml, pos)) != -1)
    {
	int charCode = rx.cap(1).toInt();
	QChar ch(charCode);
	sHtml = sHtml.replace( QString("&#%1;").arg(charCode), ch );
    }

    sHtml = sHtml.replace( "&nbsp;", " " );
    sHtml = sHtml.replace( "&amp;", "&" );
    sHtml = sHtml.replace( "&lt;", "<" );
    sHtml = sHtml.replace( "&gt;", ">" );
    sHtml = sHtml.replace( "&szlig;", "ß" );
    sHtml = sHtml.replace( "&auml;", "ä" );
    sHtml = sHtml.replace( "&Auml;", "Ä" );
    sHtml = sHtml.replace( "&ouml;", "ö" );
    sHtml = sHtml.replace( "&Ouml;", "Ö" );
    sHtml = sHtml.replace( "&uuml;", "ü" );
    sHtml = sHtml.replace( "&Uuml;", "Ü" );

    return sHtml;
}

bool TimetableAccessorHtml::parseDocumentPossibleStops( const QByteArray document, QMap< QString, QString >* stops ) {
    m_document = document;
    return parseDocumentPossibleStops( stops );
}

bool TimetableAccessorHtml::parseDocumentPossibleStops( QMap<QString,QString>* stops ) const {
    if ( m_info.regExpSearchPossibleStopsRange.isEmpty() )
	return false; // possible stop lists not supported by accessor or service provider

    QString document = decodeHtml( m_document );
    qDebug() << "TimetableAccessorHtml::parseDocumentPossibleStops" << "Parsing for a list of possible stop names...";
    QRegExp rxRange(m_info.regExpSearchPossibleStopsRange, Qt::CaseInsensitive);
    rxRange.setMinimal( true );
    if ( rxRange.indexIn(document) != -1 && rxRange.isValid() )
	document = rxRange.cap(1);
    else
    {
	qDebug() << "TimetableAccessorHtml::parseDocumentPossibleStops" << "Possible stop range not matched ";
	return false;
    }
//     qDebug() << "TimetableAccessorHtml::parseDocumentPossibleStops" << "Possible stop range = " << document;

    QRegExp rx(m_info.searchPossibleStops.regExp(), Qt::CaseInsensitive);
    rx.setMinimal(true);
    int pos = 0;
    while ((pos = rx.indexIn(document, pos)) != -1) {
        if (!rx.isValid()) {
            qDebug() << "TimetableAccessorHtml::parseDocumentPossibleStops" << "Parse error";
            return false; // parse error
        }

	QList< TimetableInformation > infos = m_info.searchPossibleStops.infos();
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
    }

    return pos != 0;
}




