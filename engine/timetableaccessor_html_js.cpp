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

#include "timetableaccessor_html_js.h"
#include "timetableaccessor_html.h"

#include <KDebug>
#include <QFile>
#include <QScriptValueIterator>

TimetableAccessorHtmlJs::TimetableAccessorHtmlJs( TimetableAccessorInfo info )
	: TimetableAccessor() {
    m_info = info;
    loadScript( m_info.scriptFileName() );
}

bool TimetableAccessorHtmlJs::loadScript( const QString& fileName ) {
    // Reset loaded state
    m_scriptLoaded = false;
    
    // Open file
    if ( fileName.isEmpty() )
	return false;
    
    QFile file( fileName );
    if( !file.open(QFile::ReadOnly) ) {
	kDebug() << "Couldn't open script file" << fileName;
	return false;
    }
    
    // Load content
    kDebug() << "Load script: " << fileName;
    QScriptValue cls = m_engine.evaluate( file.readAll(), fileName );
    m_scriptLoaded = !m_engine.hasUncaughtException();
    file.close();
    
    if ( m_scriptLoaded )
	m_parser = m_engine.globalObject().property( "Parser" );
    else {
	kDebug() << "Failed to load, backtrace: ";
	foreach ( const QString& line, m_engine.uncaughtExceptionBacktrace() )
	    kDebug() << "  # " << line;
    }
    
    return m_scriptLoaded;
}

bool TimetableAccessorHtmlJs::parseDocument( QList<PublicTransportInfo*> *journeys,
					     ParseDocumentMode parseDocumentMode ) {
    QString document = TimetableAccessorHtml::decodeHtml( m_document );
    // Performance(?): Cut everything before "<body>" from the document
    document = document.mid( document.indexOf("<body>", 0, Qt::CaseInsensitive) );

    kDebug() << "Parsing..." << (parseDocumentMode == ParseForJourneys
	    ? "searching for journeys" : "searching for departures / arrivals");

    // Call script
    QScriptValue parseTimetableScript = m_parser.property(
	    (parseDocumentMode == ParseForJourneys ? "parseJourneys" : "parseTimetable") );
    parseTimetableScript.call( m_parser, QScriptValueList() << document );
    // TODO: The script could hang here making the data engine to hang also
    
    // Evaluate results
    QScriptValue result = m_parser.property( "result" );
    QScriptValueIterator itDepartures( result );

    int count = 0;
    while ( itDepartures.hasNext() ) {
	itDepartures.next();
	
	QHash< TimetableInformation, QVariant > data;
	QScriptValueIterator itValues( itDepartures.value() );
	while ( itValues.hasNext() ) {
	    itValues.next();
	    TimetableInformation info = timetableInformationFromString( itValues.name() );
	    if ( info == Nothing ) {
		kDebug() << "Unknown timetable information" << itValues.name()
		    << "with value" << itValues.value().toString();
	    } else {
		data[ info ] = itValues.value().toVariant();
	    }
	}

	PublicTransportInfo *info;
	if ( parseDocumentMode == ParseForJourneys ) {
// 	    kDebug() << "\n   DATA:\n     " << data;
// 	    kDebug() << "\n   DATA (RouteStops:\n     " << data[ RouteStops ];
// 	    kDebug() << "\n   DATA (RouteStops:\n     " << data[ RouteTypesOfVehicles ];
// 	    kDebug() << "\n   DATA (RouteTimes:\n     " << data[ RouteTimes ];
	    info = new JourneyInfo( data );
	} else
	    info = new DepartureInfo( data );
	if ( !info->isValid() )
	    continue;
	journeys->append( info );
	++count;
    }
    
    if ( count == 0 )
	kDebug() << "The script didn't find anything";
    return count > 0;
}

QString TimetableAccessorHtmlJs::parseDocumentForLaterJourneysUrl() {
    QString document = TimetableAccessorHtml::decodeHtml( m_document );
    // Performance(?): Cut everything before "<body>" from the document
    document = document.mid( document.indexOf("<body>", 0, Qt::CaseInsensitive) );
    
    // Call script
    QScriptValue parseLaterUrlScript = m_parser.property( "getUrlForLaterJourneyResults" );
    if ( !parseLaterUrlScript.isValid() ) {
	kDebug() << "No 'getUrlForLaterJourneyResults'-function in script";
	return QString();
    }
    parseLaterUrlScript.call( m_parser, QScriptValueList() << document );
    // TODO: The script could hang here making the data engine to hang also
    
    // Evaluate results
    QScriptValue result = m_parser.property( "result" );
    QString laterUrl = result.toString();
    if ( laterUrl.isEmpty() || laterUrl == "null" )
	return QString();
    else
	return TimetableAccessorHtml::decodeHtmlEntities( laterUrl );
}

QString TimetableAccessorHtmlJs::parseDocumentForDetailedJourneysUrl() {
    QString document = TimetableAccessorHtml::decodeHtml( m_document );
    // Performance(?): Cut everything before "<body>" from the document
    document = document.mid( document.indexOf("<body>", 0, Qt::CaseInsensitive) );
    
    // Call script
    QScriptValue parseDetailedUrlScript = m_parser.property( "getUrlForDetailedJourneyResults" );
    if ( !parseDetailedUrlScript.isValid() ) {
	kDebug() << "No 'getUrlForDetailedJourneyResults'-function in script";
	return QString();
    }
    parseDetailedUrlScript.call( m_parser, QScriptValueList() << document );
    // TODO: The script could hang here making the data engine to hang also
    
    // Evaluate results
    QScriptValue result = m_parser.property( "result" );
    QString detailedUrl = result.toString();
    if ( detailedUrl.isEmpty() || detailedUrl == "null" )
	return QString();
    else
	return TimetableAccessorHtml::decodeHtmlEntities( detailedUrl );
}

bool TimetableAccessorHtmlJs::parseDocumentPossibleStops( const QByteArray document,
						QStringList* stops,
						QHash<QString,QString>* stopToStopId ) {
    m_document = document;
    return parseDocumentPossibleStops( stops, stopToStopId );
}

bool TimetableAccessorHtmlJs::parseDocumentPossibleStops( QStringList *stops,
				    QHash<QString,QString> *stopToStopId ) const {
    QScriptValue parsePossibleStopsScript = m_parser.property( "parsePossibleStops" );
    
    if ( !parsePossibleStopsScript.isValid() ) {
	kDebug() << "Possible stop lists not supported by accessor or service provider";
	return false;
    }

    QString document = TimetableAccessorHtml::decodeHtml( m_document );
//     kDebug() << document;

    // Call script
    parsePossibleStopsScript.call( m_parser, QScriptValueList() << document );
    // TODO: The script could hang here making the data engine to hang also
    
    // Evaluate results
    QScriptValue result = m_parser.property( "result" );
    QScriptValueIterator itStops( result );
    
    int count = 0;
    while ( itStops.hasNext() ) {
	itStops.next();

	QString stopName, stopID;
	QScriptValueIterator itValues( itStops.value() );
	while ( itValues.hasNext() ) {
	    itValues.next();
	    
	    TimetableInformation info = timetableInformationFromString( itValues.name() );
	    if ( info == StopName )
		stopName = TimetableAccessorHtml::decodeHtmlEntities( itValues.value().toString() );
	    else if ( info == StopID )
		stopID = itValues.value().toString();
	}

	if ( stopName == "" )
	    continue;

	stops->append( stopName );
	stopToStopId->insert( stopName, stopID );
	++count;
    }

    if ( count == 0 )
	kDebug() << "No stops found";
    return count > 0;
}




