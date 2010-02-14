/*
*   Copyright 2010 Friedrich Pülz <fpuelz@gmx.de>
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

#include "osmreader.h"


void OsmReader::read() {
    m_data.clear();

    while ( !atEnd() || waitOnRecoverableError() ) {
	readNext();
	
	if ( isStartElement() ) {
	    if ( name().compare("osm", Qt::CaseInsensitive) == 0 ) {
		readOsm();
		break;
	    }
	}
    }

    kDebug() << "Read complete:" << errorString();
    emit finishedReading( this, m_data );
}

bool OsmReader::waitOnRecoverableError() {
    if ( error() == PrematureEndOfDocumentError ) {
	if ( !m_data.isEmpty() )
	    emit chunkRead( this, m_data );
	m_data.clear(); // Clear old data
	m_loop.exec(); // Wait for more data
	return true;
    } else
	return false;
}

void OsmReader::readUnknownElement() {
    Q_ASSERT( isStartElement() );
    
    while ( !atEnd() || waitOnRecoverableError() ) {
	readNext();
	
	if ( isEndElement() )
	    break;
	
	if ( isStartElement() )
	    readUnknownElement();
    }
}

void OsmReader::readOsm() {
    while ( !atEnd() || waitOnRecoverableError() ) {
	readNext();
	
	if ( isEndElement() && name().compare("osm", Qt::CaseInsensitive) == 0 )
	    break;

	if ( isStartElement() ) {
	    if ( name().compare("node", Qt::CaseInsensitive) == 0 ) {
		readNode();
	    } else if ( name().compare("way", Qt::CaseInsensitive) == 0 ) {
		readWay();
	    } else if ( name().compare("relation", Qt::CaseInsensitive) == 0 ) {
		readRelation();
	    } else
		readUnknownElement();
	}
    }
}

void OsmReader::readNode() {
    QString id = attributes().value( "id" ).toString();
    double longitude = attributes().value( "lon" ).toString().toDouble();
    double latitude = attributes().value( "lat" ).toString().toDouble();
    // Could read more infos from attributes (user, uid, timestamp, version, changeset)
    
    QHash< QString, QVariant > nodeData;
    nodeData.insert( "longitude", longitude );
    nodeData.insert( "latitude", latitude );
    nodeData.insert( "type", "node" );
    
    while ( !atEnd() || waitOnRecoverableError() ) {
	readNext();
	
	if ( isEndElement() && name().compare("node", Qt::CaseInsensitive) == 0 )
	    break;
	
	if ( isStartElement() ) {
	    if ( name().compare("tag", Qt::CaseInsensitive) == 0 ) {
		readTag( &nodeData );
	    } else
		readUnknownElement();
	}
    }

    m_data.insert( id, nodeData );
}

void OsmReader::readWay() {
    QString id = attributes().value( "id" ).toString();
    // Could read more infos from attributes (user, uid, timestamp, version, changeset)
    QHash< QString, QVariant > nodeData;
    QStringList nodes;
    nodeData.insert( "type", "way" );
    
    while ( !atEnd() || waitOnRecoverableError() ) {
	readNext();
	
	if ( isEndElement() && name().compare("way", Qt::CaseInsensitive) == 0 )
	    break;
	
	if ( isStartElement() ) {
	    if ( name().compare("tag", Qt::CaseInsensitive) == 0 ) {
		readTag( &nodeData );
	    } else if ( name().compare("nd", Qt::CaseInsensitive) == 0 ) {
		QString node = attributes().value( "ref" ).toString();
		if ( !node.isEmpty() )
		    nodes << node;
	    } else
		readUnknownElement();
	}
    }
    
    nodeData.insert( "nodes", nodes ); // IDs of associated nodes
    m_data.insert( id, nodeData );
}

void OsmReader::readRelation() {
    QString id = attributes().value( "id" ).toString();
    // Could read more infos from attributes (user, uid, timestamp, version, changeset)
    QHash< QString, QVariant > nodeData;
    QStringList nodes, ways;
    nodeData.insert( "type", "relation" );
    
    while ( !atEnd() || waitOnRecoverableError() ) {
	readNext();
	
	if ( isEndElement() && name().compare("way", Qt::CaseInsensitive) == 0 )
	    break;
	
	if ( isStartElement() ) {
	    if ( name().compare("tag", Qt::CaseInsensitive) == 0 ) {
		readTag( &nodeData );
	    } else if ( name().compare("member", Qt::CaseInsensitive) == 0 ) {
		QString nodeOrWay = attributes().value( "ref" ).toString();
		if ( !nodeOrWay.isEmpty() ) {
		    QString type = attributes().value( "type" ).toString();
		    if ( type == "node" )
			nodes << nodeOrWay;
		    else if ( type == "way" )
			ways << nodeOrWay;
		    else
			kDebug() << "Unknown member type" << type
				 << "of relation" << id;
		}
	    } else
		readUnknownElement();
	}
    }
    
    nodeData.insert( "nodes", nodes ); // IDs of associated nodes
    nodeData.insert( "ways", ways ); // IDs of associated ways
    m_data.insert( id, nodeData );
}

void OsmReader::readTag( QHash< QString, QVariant > *nodeData ) {
    if ( !attributes().hasAttribute("k") || !attributes().hasAttribute("v") )
	kDebug() << "Key or value attribute not found for <tag>";

    // Simply use the keys from OpenStreetMap. Maybe it's better to translate
    // them ("addr:street" => "street", then maybe combined with "addr:housenumber).
    nodeData->insert( attributes().value("k").toString(),
		      attributes().value("v").toString() );
}