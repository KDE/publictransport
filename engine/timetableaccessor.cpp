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

#include "timetableaccessor.h"
#include "timetableaccessor_html.h"
#include "timetableaccessor_xml.h"
#include "timetableaccessor_htmlinfo.h"

#include <KIO/NetAccess>
#include <KDebug>
#include <qtextcodec.h>

TimetableAccessor::TimetableAccessor()
{

}

TimetableAccessor::~TimetableAccessor()
{

}

TimetableAccessor* TimetableAccessor::getSpecificAccessor ( ServiceProvider serviceProvider ) {
    switch (serviceProvider)
    {
	case RMV:
 	    return new TimetableAccessorXml( serviceProvider );

	default:
 	    return new TimetableAccessorHtml( serviceProvider );
    }
}

QStringList TimetableAccessor::features() const {
    return m_info.features();
}

KIO::TransferJob *TimetableAccessor::requestJourneys ( const QString &sourceName, const QString &city, const QString &stop, int maxDeps, int timeOffset, const QString &dataType, bool useDifferentUrl ) {
    qDebug() << "TimetableAccessor::requestJourneys" << "URL =" << getUrl(city, stop, maxDeps, timeOffset, dataType, useDifferentUrl).url();

    // Creating a kioslave
    if ( useDifferentUrl )
	qDebug() << "using a different url";
    KIO::TransferJob *job = KIO::get( getUrl(city, stop, maxDeps, timeOffset, dataType, useDifferentUrl), KIO::NoReload, KIO::HideProgressInfo );
    m_jobInfos.insert( job, QList<QVariant>() << sourceName << city << stop << maxDeps << timeOffset << dataType << useDifferentUrl );
    m_document = "";

    connect( job, SIGNAL(data(KIO::Job*,QByteArray)), this, SLOT(dataReceived(KIO::Job*,QByteArray)) );
    connect( job, SIGNAL(finished(KJob*)), this, SLOT(finished(KJob*)) );

    return job;
}

void TimetableAccessor::dataReceived ( KIO::Job*, const QByteArray& data ) {
    m_document += data;
}

void TimetableAccessor::finished(KJob* job) {
    QList<QVariant> jobInfo = m_jobInfos.value(job);
    m_jobInfos.remove(job);

    QString sourceName = jobInfo.at(0).toString();
    QString city = jobInfo.at(1).toString();
    QString stop = jobInfo.at(2).toString();
    int maxDeps = jobInfo.at(3).toInt();
    int timeOffset = jobInfo.at(4).toInt();
    QString dataType = jobInfo.at(5).toString();
    bool usedDifferentUrl = jobInfo.at(6).toBool();
    m_curCity = city;
    QList< DepartureInfo > *journeys;
    QMap< QString, QString > *stops;

    // TODO: less confusing?
    if ( !usedDifferentUrl )
    {
	if ( (journeys = new QList< DepartureInfo >()) != NULL && parseDocument(journeys) ){
	    emit journeyListReceived( this, *journeys, serviceProvider(), sourceName, city, stop, dataType );
	    qDebug() << "emit journeyListReceived";
	}
	else if ( useDifferentUrlForPossibleStops() ) {
	    requestJourneys( sourceName, m_curCity, stop, maxDeps, timeOffset, dataType, true );
	    qDebug() << "request possible stop list";
	}
	else if ( (stops = new QMap<QString, QString>()) != NULL && parseDocumentPossibleStops(stops) ) {
	    emit stopListReceived( this, *stops, serviceProvider(), sourceName, city, stop, dataType );
	    qDebug() << "possible stop list";
	}
	else
	    emit errorParsing( this, serviceProvider(), sourceName, city, stop, dataType );
    }
    else if ( (stops = new QMap<QString, QString>()) != NULL && parseDocumentPossibleStops(stops) ) {
	emit stopListReceived( this, *stops, serviceProvider(), sourceName, city, stop, dataType );
	qDebug() << "stop list";
    }
    else
	emit errorParsing( this, serviceProvider(), city, sourceName, stop, dataType );
}

KUrl TimetableAccessor::getUrl ( const QString &city, const QString &stop, int maxDeps, int timeOffset, const QString &dataType, bool useDifferentUrl ) const {
    QString sRawUrl = useDifferentUrl ? differentRawUrl() : rawUrl();
    QString sTime = QTime::currentTime().addSecs(timeOffset * 60).toString("hh:mm");
    QString sDataType = dataType == "arrivals" ? "arr" : "dep";

    // Construct the url from the "raw" url by replacing values
    if ( useSeperateCityValue() ) {
	sRawUrl = sRawUrl.replace( "%3", QString("%1").arg(maxDeps) ).replace( "%4", sTime ).replace( "%1", QString::fromAscii(QUrl::toPercentEncoding(city.toLower())) ).replace( "%2", QString::fromAscii(QUrl::toPercentEncoding(stop.toLower())) ).replace( "%5", sDataType );
    } else {
	if ( charsetForUrlEncoding().isEmpty() )
	    sRawUrl = sRawUrl.replace("%2", sTime ).replace( "%3", QString("%1").arg(maxDeps) ).replace( "%1", QString::fromAscii(QUrl::toPercentEncoding(stop.toLower())) ).replace( "%5", sDataType );
	else {
	    sRawUrl = sRawUrl.replace( "%2", sTime ).replace( "%3", QString("%1").arg(maxDeps) ).replace( "%1", toPercentEncoding(stop.toLower(), charsetForUrlEncoding()) ).replace( "%5", sDataType );
	}
    }

    return KUrl( sRawUrl );
}

QString TimetableAccessor::gethex ( ushort decimal ) {
    QString hexchars = "0123456789ABCDEFabcdef";
    return "%" + hexchars[decimal >> 4] + hexchars[decimal & 0xF];
}

QString TimetableAccessor::toPercentEncoding ( QString str, QByteArray charset ) {
    QString unreserved = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_.~";
//     QString reserved = "!*'();:@&=+$,/?%#[]";
    QString encoded;

    QByteArray ba = QTextCodec::codecForName(charset)->fromUnicode(str);
    for ( int i = 0; i < ba.length(); ++i )
    {
	char ch = ba[i];
	if ( unreserved.indexOf(ch) != -1 )
	{
	    encoded += ch;
// 	    qDebug() << "  Unreserved char" << encoded;
	}
	else if ( ch < 0 )
	{
	    encoded += gethex(256 + ch);
// 	    qDebug() << "  Encoding char < 0" << encoded;
	}
	else
	{
	    encoded += gethex(ch);
// 	    qDebug() << "  Encoding char >= 0" << encoded;
	}
    }

    return encoded;
}

bool TimetableAccessor::parseDocument( QList<DepartureInfo> *journeys ) {
    Q_UNUSED( journeys );
    return false;
}

bool TimetableAccessor::parseDocumentPossibleStops ( QMap<QString,QString> *stops ) const {
    Q_UNUSED( stops );
    return false;
}

QString TimetableAccessor::rawUrl() const {
    return m_info.rawUrl;
}

QString TimetableAccessor::differentRawUrl() const {
    return m_info.rawUrlXml();
}

QByteArray TimetableAccessor::charsetForUrlEncoding() const {
    return m_info.charsetForUrlEncoding();
}
/*
QString TimetableAccessor::regExpSearch() const
{
    return m_info.searchJourneys.regExp();
}

QList< TimetableInformation > TimetableAccessor::regExpInfos() const
{
    return m_info.searchJourneys.infos();
}

QString TimetableAccessor::regExpSearchPossibleStopsRange() const
{
    return m_info.regExpSearchPossibleStopsRange;
}

QString TimetableAccessor::regExpSearchPossibleStops() const
{
    return m_info.searchPossibleStops.regExp();
}

QList< TimetableInformation > TimetableAccessor::regExpInfosPossibleStops() const
{
    return m_info.searchPossibleStops.infos();
}*/

TimetableAccessorInfo TimetableAccessor::timetableAccessorInfo() const {
    return m_info;
}




