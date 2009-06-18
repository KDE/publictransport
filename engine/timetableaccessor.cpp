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
#include "timetableaccessor_rmv.h"

#include <KIO/NetAccess>
#include <KDebug>

TimetableAccessor::TimetableAccessor()
{

}

TimetableAccessor::~TimetableAccessor()
{

}

TimetableAccessor* TimetableAccessor::getSpecificAccessor ( ServiceProvider serviceProvider )
{
    switch (serviceProvider)
    {
	case RMV:
 	    return new TimetableAccessorRmv();

	default:
 	    return new TimetableAccessorHtml( serviceProvider );
    }
}

KIO::TransferJob *TimetableAccessor::requestJourneys ( QString city, QString stop )
{
    qDebug() << "TimetableAccessor::requestJourneys" << "URL =" << getUrl(city, stop).pathOrUrl();
    
    // Creating a kioslave
    KIO::TransferJob *job = KIO::get( getUrl(city, stop), KIO::NoReload, KIO::HideProgressInfo );
    m_jobInfos.insert( job, QStringList() << city << stop );
    m_document = "";

    connect( job, SIGNAL(data(KIO::Job*,QByteArray)), this, SLOT(dataReceived(KIO::Job*,QByteArray)) );
    connect( job, SIGNAL(finished(KJob*)), this, SLOT(finished(KJob*)) );

    return job;
}

QList< DepartureInfo > TimetableAccessor::getJourneys ( QString city, QString stop )
{
    qDebug() << "TimetableAccessor::getJourneys" << "URL =" << getUrl(city, stop).pathOrUrl();
    
    KIO::TransferJob *job = KIO::get( getUrl(city, stop), KIO::NoReload, KIO::HideProgressInfo );
    QByteArray data;

    // this crashes plasma (doesn't crash in plasmoidviewer)
    if (KIO::NetAccess::synchronousRun(job, 0, &data))
    {
	m_document = data;
	QList< DepartureInfo > journeys;
	parseDocument( &journeys );
	return journeys;
    }

    return QList< DepartureInfo >();
}

void TimetableAccessor::dataReceived ( KIO::Job*, const QByteArray& data )
{
    m_document += data;
}

void TimetableAccessor::finished(KJob* job)
{
    QStringList jobInfo = m_jobInfos.value(job);
    m_jobInfos.remove(job);

    m_curCity = jobInfo.at(0);
    QList< DepartureInfo > *journeys;
    QMap< QString, QString > *stops;
    if ( (journeys = new QList< DepartureInfo >()) != NULL && parseDocument(journeys) )
	emit journeyListReceived( this, *journeys, serviceProvider(), jobInfo.at(0), jobInfo.at(1) );
    else if ( (stops = new QMap<QString, QString>()) != NULL && parseDocumentPossibleStops(stops) )
	emit stopListReceived( this, *stops, serviceProvider(), jobInfo.at(0), jobInfo.at(1) );
    else
	emit errorParsing( this, serviceProvider(), jobInfo.at(0), jobInfo.at(1) );
}

KUrl TimetableAccessor::getUrl ( QString city, QString stop )
{
    // Construct the url from the "raw" url by inserting the city and the stop
    if ( useSeperateCityValue() )
	return rawUrl().arg(city).arg(stop);
    else
	return rawUrl().arg(stop);
}

bool TimetableAccessor::parseDocument( QList<DepartureInfo> *journeys )
{
    return false;
}

bool TimetableAccessor::parseDocumentPossibleStops ( QMap<QString,QString> *stops )
{
    return false;
}

QString TimetableAccessor::rawUrl()
{
    return "";
}

