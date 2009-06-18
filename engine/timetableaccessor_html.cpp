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

TimetableAccessorHtml::TimetableAccessorHtml()
{
    m_info = TimetableAccessorInfo();
}

TimetableAccessorHtml::TimetableAccessorHtml ( ServiceProvider serviceProvider )
    : TimetableAccessor ()
{
    switch ( serviceProvider )
    {
	case BVG:
	    m_info = TimetableAccessorInfo_Bvg();
	    break;

	case DVB:
	    m_info = TimetableAccessorInfo_Dvb();
	    break;

	case Fahrplaner:
	    m_info = TimetableAccessorInfo_Fahrplaner();
	    break;

	case IMHD:
	    m_info = TimetableAccessorInfo_Imhd();
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

	case DB:
	    m_info = TimetableAccessorInfo_Db();
	    break;

	case SBB:
	    m_info = TimetableAccessorInfo_Sbb();
	    break;

	default:
	    qDebug() << "Not an HTML accessor?" << serviceProvider;
    }
}

bool TimetableAccessorHtml::parseDocument( QList<DepartureInfo> *journeys )
{
    QString document = QString(m_document);

    // Get charset of the received document and convert it to a unicode QString
    QRegExp rxCharset("(?:<head>\\s*<meta http-equiv=\"Content-Type\" content=\"text/html; charset=)(.*)(?:\">)", Qt::CaseInsensitive);
    rxCharset.setMinimal( true );
    if ( rxCharset.indexIn(document) != -1 && rxCharset.isValid() )
	document = QTextCodec::codecForName(rxCharset.cap(1).toAscii())->toUnicode(m_document);
    
    qDebug() << "TimetableAccessorHtml::parseDocument" << "Parsing...";
    QRegExp rx( regExpSearch(), Qt::CaseInsensitive );
    rx.setMinimal(true);
    int pos = 0, count = 0;
    while ((pos = rx.indexIn(document, pos)) != -1) {
        if (!rx.isValid()) {
            qDebug() << "TimetableAccessorHtml::parseDocument" << "Parse error";
            return false; // parse error
        }

	QList< TimetableInformation > infos = regExpInfos();
	if ( infos.isEmpty() )
	    journeys->append( getInfo(rx) );
	else
	{
	    QString sDepHour, sDepMinute, sType, sLine, sDirection;
	    if ( infos.contains(DepartureHour) )
		sDepHour = rx.cap( infos.indexOf( DepartureHour ) + 1 );
	    if ( infos.contains(DepartureMinute) )
		sDepMinute = rx.cap( infos.indexOf( DepartureMinute ) + 1 );
	    if ( infos.contains(TypeOfVehicle) )
		sType = rx.cap( infos.indexOf( TypeOfVehicle ) + 1 );
	    if ( infos.contains(TransportLine) )
	    {
		sLine = rx.cap( infos.indexOf( TransportLine ) + 1 ).trimmed();
		sLine = sLine.replace( QRegExp("^(bus|str|s|u)\\s*", Qt::CaseInsensitive), "" );
		sLine = sLine.replace( QRegExp("\\s{2,}", Qt::CaseInsensitive), " " );
	    }
	    if ( infos.contains(Direction) )
		sDirection = rx.cap( infos.indexOf( Direction ) + 1 ).trimmed();
// 	    qDebug() << sDepHour << ":" << sDepMinute;

	    sDirection = TimetableAccessorHtml::decodeHtml(sDirection);
	    if ( serviceProvider() == DB && !m_curCity.isEmpty() ) // TODO: needed?
		sDirection = sDirection.remove( QRegExp(QString(",?\\s?%1$").arg(m_curCity)) );

// 	    qDebug() << sLine << sDirection << sDepHour << ":" << sDepMinute;
	    journeys->append( DepartureInfo( sLine, DepartureInfo::getLineTypeFromString(sType), sDirection, QTime(sDepHour.toInt(), sDepMinute.toInt()), sLine.isEmpty() ? false : sLine.at(0) == 'N' ) );
	}
        pos += rx.matchedLength();
	++count;
    }

    if ( count == 0 )
	qDebug() << "TimetableAccessorHtml::parseDocument" << "The regular expression didn't match anything";
    return count > 0;
}

// Couldn't find such a function anywhere in Qt or KDE (but it must be there somewhere...)
QString TimetableAccessorHtml::decodeHtml( QString sHtml )
{
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

bool TimetableAccessorHtml::parseDocumentPossibleStops ( QMap<QString,QString>* stops )
{
    if ( regExpSearchPossibleStopsRange().isEmpty() )
	return false; // possible stop lists not supported by accessor or service provider
    
    QString document = QString(m_document);
    
    // Get charset of the received document and convert it to a unicode QString
    // TODO: use QTextCodec::codecForHtml?
    QRegExp rxCharset("(?:<head>\\s*<meta http-equiv=\"Content-Type\" content=\"text/html; charset=)(.*)(?:\">)", Qt::CaseInsensitive);
    rxCharset.setMinimal( true );
    if ( rxCharset.indexIn(document) != -1 && rxCharset.isValid() )
	document = QTextCodec::codecForName(rxCharset.cap(1).toAscii())->toUnicode(m_document);

    qDebug() << "TimetableAccessorHtml::parseDocumentPossibleStops" << "Parsing for a list of possible stop names...";
    QRegExp rxRange(regExpSearchPossibleStopsRange(), Qt::CaseInsensitive);
    rxRange.setMinimal( true );
    if ( rxRange.indexIn(document) != -1 && rxRange.isValid() )
	document = rxRange.cap(1);
    else
	return false;
//     qDebug() << "TimetableAccessorHtml::parseDocumentPossibleStops" << "Possible stop range = " << document;
    
    QRegExp rx(regExpSearchPossibleStops(), Qt::CaseInsensitive);
    rx.setMinimal(true);
    int pos = 0;
    while ((pos = rx.indexIn(document, pos)) != -1) {
        if (!rx.isValid()) {
            qDebug() << "TimetableAccessorHtml::parseDocumentPossibleStops" << "Parse error";
            return false; // parse error
        }

	QList< TimetableInformation > infos = regExpInfosPossibleStops();
	if ( infos.isEmpty() )
	    return false;
	else
	{
	    QString sStopName, sStopID;
	    if ( infos.contains(StopName) )
	    {
		sStopName = rx.cap( infos.indexOf( StopName ) + 1 );
		sStopName = TimetableAccessorHtml::decodeHtml( sStopName );
	    }
	    if ( infos.contains(StopID) )
		sStopID = rx.cap( infos.indexOf( StopID ) + 1 );

	    stops->insert( sStopName, sStopID );
	}
        pos += rx.matchedLength();
    }

    return pos != 0;
}

QString TimetableAccessorHtml::rawUrl()
{
    return m_info.rawUrl;
}

QString TimetableAccessorHtml::regExpSearch()
{
    return m_info.regExpSearch;
}

QString TimetableAccessorHtml::regExpSearchPossibleStopsRange()
{
    return m_info.regExpSearchPossibleStopsRange;
}

QString TimetableAccessorHtml::regExpSearchPossibleStops()
{
    return m_info.regExpSearchPossibleStops;
}

QList< TimetableInformation > TimetableAccessorHtml::regExpInfos()
{
    return m_info.regExpInfos;
}

QList< TimetableInformation > TimetableAccessorHtml::regExpInfosPossibleStops()
{
    return m_info.regExpInfosPossibleStops;
}

DepartureInfo TimetableAccessorHtml::getInfo(QRegExp)
{
    return DepartureInfo();
}
