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

#ifndef TIMETABLEACCESSOR_HEADER
#define TIMETABLEACCESSOR_HEADER

#include <KUrl>
#include <QMap>
#include "departureinfo.h"
#include "kio/scheduler.h"
#include "kio/jobclasses.h"

enum ServiceProvider
{
    None = -1,
    Fahrplaner = 0, // Niedersachsen/Bremen
    RMV = 1, // Rhein-Main
    VVS = 2, // Stuttgart
    VRN = 3, // Rhein-Neckar
    BVG = 4, // Berlin
    IMHD = 100 // Bratislava
};

class TimetableAccessor : public QObject
{
    Q_OBJECT
    public:
	TimetableAccessor();
        ~TimetableAccessor();

	virtual ServiceProvider serviceProvider() { return None; };
	// Gets the timetable accessor that is associated with the given service provider and can parse it's replies
	static TimetableAccessor *getSpecificAccessor(ServiceProvider serviceProvider);

	virtual QString country() { return ""; };
	virtual QStringList cities() { return QStringList(); };
	KIO::TransferJob *requestJourneys( QString city, QString stop );
	QList< DepartureInfo > getJourneys( QString city, QString stop );

    protected:
	QString m_document;

	virtual QList<DepartureInfo> parseDocument(const QString& document); // parses the contents of a document that was requested using requestJourneys()
	virtual QString rawUrl(); // gets the "raw" url with placeholders for the city ("%1") and the stop ("%2")
	virtual bool putCityIntoUrl() { return true; };
	KUrl getUrl(QString city, QString stop); // constructs an url by combining the "raw" url with the needed information

    private:
	QMap<KJob*, QStringList> m_jobInfos;

    signals:
	void journeyListReceived(QList< DepartureInfo > journeys, ServiceProvider serviceProvider, QString city, QString stop);

    public slots:
	void dataReceived(KIO::Job *job, const QByteArray &data);
	void finished(KJob* job);
};

#endif // TIMETABLEACCESSOR_HEADER
