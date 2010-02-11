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

#include "timetableaccessor_html_script.h"
#include "timetableaccessor_html.h"

#include <KDebug>
#include <QFile>
#include <QScriptValueIterator>
#include "scripting.h"
#include <kross/core/action.h>
#include <kross/core/manager.h>
#include <KLocalizedString>

TimetableAccessorHtmlScript::TimetableAccessorHtmlScript( const TimetableAccessorInfo &info )
	: TimetableAccessor() {
    m_info = info;

    // Create the Kross::Action instance
    m_script = new Kross::Action( this, "TimetableParser" ); // TODO delete in destructor
    
    TimetableData *timetableData = new TimetableData( m_script );
    m_resultObject = new ResultObject( m_script );
    m_script->addQObject( new Helper(m_script), "helper" );
    m_script->addQObject( timetableData, "timetableData" );
    m_script->addQObject( m_resultObject, "result"/*,
			  Kross::ChildrenInterface::AutoConnectSignals*/ );
    
    m_scriptLoaded = m_script->setFile( m_info.scriptFileName() );

    if ( m_scriptLoaded ) {
	m_script->trigger();
	m_scriptLoaded = !m_script->hadError();
    }
    
//     loadScript( m_info.scriptFileName() );
}

TimetableAccessorHtmlScript::~TimetableAccessorHtmlScript() {
    delete m_script;
}

QStringList TimetableAccessorHtmlScript::scriptFeatures() const {
    if ( !m_scriptLoaded )
	return QStringList();
    
    QStringList functions = m_script->functionNames();
    QStringList features;
    
    if ( functions.contains("parsePossibleStops") )
	features << "Autocompletion";
    if ( functions.contains("parseJourneys") )
	features << "JourneySearch";

    if ( !m_scriptLoaded ) {
	kDebug() << "Script couldn't be loaded" << m_info.scriptFileName();
	return features;
    }
    if ( !m_script->functionNames().contains("usedTimetableInformations") ) {
	kDebug() << "The script has no 'usedTimetableInformations' function";
	kDebug() << "Functions in the script:" << m_script->functionNames();
	return features;
    }

    QStringList usedTimetableInformations = m_script->callFunction(
	    "usedTimetableInformations" ).toStringList();
    
    if ( usedTimetableInformations.contains("Delay", Qt::CaseInsensitive) )
	features << "Delay";
    if ( usedTimetableInformations.contains("DelayReason", Qt::CaseInsensitive) )
	features << "DelayReason";
    if ( usedTimetableInformations.contains("Platform", Qt::CaseInsensitive) )
	features << "Platform";
    if ( usedTimetableInformations.contains("JourneyNews", Qt::CaseInsensitive)
		|| usedTimetableInformations.contains("JourneyNewsOther", Qt::CaseInsensitive)
		|| usedTimetableInformations.contains("JourneyNewsLink", Qt::CaseInsensitive) )
	features << "JourneyNews";
    if ( usedTimetableInformations.contains("TypeOfVehicle", Qt::CaseInsensitive) )
	features << "TypeOfVehicle";
    if ( usedTimetableInformations.contains("Status", Qt::CaseInsensitive) )
	features << "Status";
    if ( usedTimetableInformations.contains("Operator", Qt::CaseInsensitive) )
	features << "Operator";
    if ( usedTimetableInformations.contains("StopID", Qt::CaseInsensitive) )
	features << "StopID";
    
    return features;
}

bool TimetableAccessorHtmlScript::parseDocument( const QByteArray &document,
						 QList<PublicTransportInfo*> *journeys,
						 GlobalTimetableInfo *globalInfo,
						 ParseDocumentMode parseDocumentMode ) {
    if ( !m_scriptLoaded ) {
	kDebug() << "Script couldn't be loaded" << m_info.scriptFileName();
	return false;
    }
    QString functionName = parseDocumentMode == ParseForJourneys
	    ? "parseJourneys" : "parseTimetable";
    if ( !m_script->functionNames().contains(functionName) ) {
	kDebug() << "The script has no '" << functionName << "' function";
	kDebug() << "Functions in the script:" << m_script->functionNames();
	return false;
    }
    
    QString doc = TimetableAccessorHtml::decodeHtml( document );
    // Performance(?): Cut everything before "<body>" from the document
    doc = doc.mid( doc.indexOf("<body>", 0, Qt::CaseInsensitive) );

    kDebug() << "Parsing..." << parseDocumentMode;

    // Call script using Kross
    m_resultObject->clear();
    QVariant result = m_script->callFunction( functionName, QVariantList() << doc );

    if ( result.isValid() && result.canConvert(QVariant::StringList) ) {
	QStringList globalInfos = result.toStringList();
	if ( globalInfos.contains("no delays", Qt::CaseInsensitive) ) {
	    // No delay information available for the given stop
	    globalInfo->delayInfoAvailable = false;
	}
    }
    
    QList<TimetableData> data = m_resultObject->data();
    int count = 0;
    QDate curDate;
    QTime lastTime;
    foreach ( TimetableData timetableData, data ) {
	QDate date = timetableData.value( DepartureDate ).toDate();
	QTime departureTime = QTime( timetableData.value(DepartureHour).toInt(),
					timetableData.value(DepartureMinute).toInt() );
	if ( !date.isValid() ) {
	    if ( curDate.isNull() ) {
		// First departure
		if ( QTime::currentTime().hour() < 3 && departureTime.hour() > 21 )
		    date = QDate::currentDate().addDays( -1 );
		else if ( QTime::currentTime().hour() > 21 && departureTime.hour() < 3 )
		    date = QDate::currentDate().addDays( 1 );
		else
		    date = QDate::currentDate();
	    } else if ( lastTime.secsTo(departureTime) < -5 * 60 ) {
		// Time too much ealier than last time, estimate it's tomorrow
		date = curDate.addDays( 1 );
	    } else {
		date = curDate;
	    }

	    timetableData.set( DepartureDate, date );
	}
	
	curDate = date;
	lastTime = departureTime;
	
	PublicTransportInfo *info;
	if ( parseDocumentMode == ParseForJourneys )
	    info = new JourneyInfo( timetableData.values() );
	else
	    info = new DepartureInfo( timetableData.values() );
	
	if ( !info->isValid() ) {
	    delete info;
	    continue;
	}
	
	journeys->append( info );
	++count;
    }
    
    if ( count == 0 )
	kDebug() << "The script didn't find anything";
    return count > 0;
}

QString TimetableAccessorHtmlScript::parseDocumentForLaterJourneysUrl(
	    const QByteArray &document ) {
    if ( !m_scriptLoaded ) {
	kDebug() << "Script couldn't be loaded" << m_info.scriptFileName();
	return QString();
    }
    if ( !m_script->functionNames().contains("getUrlForLaterJourneyResults") ) {
	kDebug() << "The script has no 'getUrlForLaterJourneyResults' function";
	kDebug() << "Functions in the script:" << m_script->functionNames();
	return QString();
    }
    
    QString doc = TimetableAccessorHtml::decodeHtml( document );
    // Performance(?): Cut everything before "<body>" from the document
    doc = doc.mid( doc.indexOf("<body>", 0, Qt::CaseInsensitive) );
    
    // Call script
    QString result = m_script->callFunction( "getUrlForLaterJourneyResults",
					     QVariantList() << doc ).toString();
    if ( result.isEmpty() || result == "null" )
	return QString();
    else
	return TimetableAccessorHtml::decodeHtmlEntities( result );
}

QString TimetableAccessorHtmlScript::parseDocumentForDetailedJourneysUrl(
		const QByteArray &document ) {
    if ( !m_scriptLoaded ) {
	kDebug() << "Script couldn't be loaded" << m_info.scriptFileName();
	return QString();
    }
    if ( !m_script->functionNames().contains("getUrlForDetailedJourneyResults") ) {
	kDebug() << "The script has no 'getUrlForDetailedJourneyResults' function";
	kDebug() << "Functions in the script:" << m_script->functionNames();
	return QString();
    }
    
    QString doc = TimetableAccessorHtml::decodeHtml( document );
    // Performance(?): Cut everything before "<body>" from the document
    doc = doc.mid( doc.indexOf("<body>", 0, Qt::CaseInsensitive) );

    QString result = m_script->callFunction( "getUrlForDetailedJourneyResults",
					     QVariantList() << doc ).toString();
    if ( result.isEmpty() || result == "null" )
	return QString();
    else
	return TimetableAccessorHtml::decodeHtmlEntities( result );
}

bool TimetableAccessorHtmlScript::parseDocumentPossibleStops( const QByteArray &document,
				    QStringList *stops,
				    QHash<QString,QString> *stopToStopId,
				    QHash<QString,int> *stopToStopWeight ) {
    if ( !m_scriptLoaded ) {
	kDebug() << "Script couldn't be loaded" << m_info.scriptFileName();
	return false;
    }
    if ( !m_script->functionNames().contains("parsePossibleStops") ) {
	kDebug() << "The script has no 'parsePossibleStops' function";
	kDebug() << "Functions in the script:" << m_script->functionNames();
	return false;
    }

    QString doc = TimetableAccessorHtml::decodeHtml( document, m_info.fallbackCharset() );

    // Call script
    m_resultObject->clear();
    QVariant result = m_script->callFunction( "parsePossibleStops", QVariantList() << doc );
    QList<TimetableData> data = m_resultObject->data();

    int count = 0;
    foreach ( TimetableData timetableData, data ) {
	QString stopName = timetableData.value( StopName ).toString();
	QString stopID;
	int stopWeight = -1;
	
	if ( stopName.isEmpty() )
	    continue;
	
	if ( timetableData.values().contains(StopID) )
	    stopID = timetableData.value( StopID ).toString();
	if ( timetableData.values().contains(StopWeight) )
	    stopWeight = timetableData.value( StopWeight ).toInt();
	
	stops->append( stopName );
// 	if ( !stopID.isEmpty() )
	    stopToStopId->insert( stopName, stopID );
	if ( stopWeight != -1 )
	    stopToStopWeight->insert( stopName, stopWeight );
	++count;
    }

    if ( count == 0 )
	kDebug() << "No stops found";
    return count > 0;
}




