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
#include "timetableaccessor_fahrplaner.h"
#include "timetableaccessor_rmv.h"
#include "timetableaccessor_vvs.h"
#include "timetableaccessor_bvg.h"
#include "timetableaccessor_vrn.h"
#include "timetableaccessor_imhd.h"

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
	case Fahrplaner:
	    return new TimetableAccessorFahrplaner();

	case RMV:
 	    return new TimetableAccessorRmv();

	case VVS:
 	    return new TimetableAccessorVvs();

	case BVG:
 	    return new TimetableAccessorBvg();

	case VRN:
 	    return new TimetableAccessorVrn();

	case IMHD:
	    return new TimetableAccessorImhd();

	default:
	    return 0;
    }
}

KIO::TransferJob *TimetableAccessor::requestJourneys ( QString city, QString stop )
{
    qDebug() << "URL =" << getUrl(city, stop);
    
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
    qDebug() << "URL =" << getUrl(city, stop);
// return QList< DepartureInfo >(); // FIXME: synchronousRun causes Crash in plasma (but not in plasmoidviewer...)
    
    KIO::TransferJob *job = KIO::get( getUrl(city, stop), KIO::NoReload, KIO::HideProgressInfo );
    QByteArray data;

//     try
//     {
	if (KIO::NetAccess::synchronousRun(job, 0, &data))
	    return parseDocument( QString(data) );
//     }
//     catch( e )
//     {
// 	qDebug() << "Error while trying to call KIO::NetAccess::synchronousRun";
//     }

    return QList< DepartureInfo >();
}

void TimetableAccessor::dataReceived ( KIO::Job*, const QByteArray& data )
{
    m_document += QString(data);
}

void TimetableAccessor::finished(KJob* job)
{
    QStringList jobInfo = m_jobInfos.value(job);
    m_jobInfos.remove(job);
    
//     qDebug() << "\nFinished downloading:\n" << m_document << "\n";
// , ServiceProvider serviceProvider, QString city, QString stop

    emit journeyListReceived( parseDocument(m_document), serviceProvider(), jobInfo.at(0), jobInfo.at(1) );
}

KUrl TimetableAccessor::getUrl ( QString city, QString stop )
{
    // Construct the url from the "raw" url by inserting the city and the stop
    if ( putCityIntoUrl() )
	return rawUrl().arg(city).arg(stop);
    else
	return rawUrl().arg(stop);
}

QList< DepartureInfo > TimetableAccessor::parseDocument(QString)
{
    return QList<DepartureInfo>();
}

QString TimetableAccessor::rawUrl()
{
    return "";
}

