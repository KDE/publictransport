/*
*   Copyright 2012 Friedrich Pülz <fpuelz@gmx.de>
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
            if ( name().compare(QLatin1String("osm"), Qt::CaseInsensitive) == 0 ) {
                readOsm();
                break;
            }
        }
    }

    kDebug() << "Read complete:" << (hasError() ? "No error." : errorString() );
    emit finishedReading( this, m_data );
}

bool OsmReader::waitOnRecoverableError() {
    if ( error() == PrematureEndOfDocumentError ) {
        if ( !m_data.isEmpty() ) {
            emit chunkRead( this, m_data );
        }
        m_data.clear(); // Clear old data
        m_loop.exec(); // Wait for more data
        return true;
    } else {
        return false;
    }
}

void OsmReader::readUnknownElement() {
    Q_ASSERT( isStartElement() );

    while ( !atEnd() || waitOnRecoverableError() ) {
        readNext();

        if ( isEndElement() ) {
            break;
        }

        if ( isStartElement() ) {
            readUnknownElement();
        }
    }
}

void OsmReader::readOsm() {
    while ( !atEnd() || waitOnRecoverableError() ) {
        readNext();

        if ( isEndElement() && name().compare(QLatin1String("osm"), Qt::CaseInsensitive) == 0 ) {
            kDebug() << "Closing </osm> tag read";
            break;
        }

        if ( isStartElement() ) {
            if ( name().compare(QLatin1String("node"), Qt::CaseInsensitive) == 0 ) {
                readNode();
            } else if ( name().compare(QLatin1String("way"), Qt::CaseInsensitive) == 0 ) {
                readWay();
            } else if ( name().compare(QLatin1String("relation"), Qt::CaseInsensitive) == 0 ) {
                readRelation();
            } else {
                readUnknownElement();
            }
        }
    }

    kDebug() << "Finished reading the <osm> tag";
}

bool OsmReader::isResultValid( const QVariantHash& data ) const {
    return !m_resultFlags.testFlag(OnlyResultsWithNameAttribute)
        || data.contains("name");
}

void OsmReader::readNode() {
    QString id = attributes().value( QLatin1String("id") ).toString();
    double longitude = attributes().value( QLatin1String("lon") ).toString().toDouble();
    double latitude = attributes().value( QLatin1String("lat") ).toString().toDouble();
    // Could read more information from attributes (user, uid, timestamp, version, changeset)

    QVariantHash nodeData;
    nodeData.insert( "longitude", longitude );
    nodeData.insert( "latitude", latitude );
    nodeData.insert( "type", "node" );

    while ( !atEnd() || waitOnRecoverableError() ) {
        readNext();

        if ( isEndElement() && name().compare(QLatin1String("node"), Qt::CaseInsensitive) == 0 ) {
            break;
        }

        if ( isStartElement() ) {
            if ( name().compare(QLatin1String("tag"), Qt::CaseInsensitive) == 0 ) {
                readTag( &nodeData );
            } else {
                readUnknownElement();
            }
        }
    }

    if ( isResultValid(nodeData) ) {
        m_data.insert( id, nodeData );
    }
}

void OsmReader::readWay() {
    QString id = attributes().value( QLatin1String("id") ).toString();
    // Could read more information from attributes (user, uid, timestamp, version, changeset)
    QVariantHash nodeData;
    QStringList nodes;
    nodeData.insert( "type", "way" );

    while ( !atEnd() || waitOnRecoverableError() ) {
        readNext();

        if ( isEndElement() && name().compare(QLatin1String("way"), Qt::CaseInsensitive) == 0 )
            break;

        if ( isStartElement() ) {
            if ( name().compare(QLatin1String("tag"), Qt::CaseInsensitive) == 0 ) {
                readTag( &nodeData );
            } else if ( name().compare(QLatin1String("nd"), Qt::CaseInsensitive) == 0 ) {
                QString node = attributes().value( QLatin1String("ref") ).toString();
                if ( !node.isEmpty() ) {
                    nodes << node;
                }
            } else {
                readUnknownElement();
            }
        }
    }

    if ( isResultValid(nodeData) ) {
        if ( !nodes.isEmpty() ) {
            nodeData.insert( "nodes", nodes ); // IDs of associated nodes
        }
        m_data.insert( id, nodeData );
    }
}

void OsmReader::readRelation() {
    QString id = attributes().value( QLatin1String("id") ).toString();
    // Could read more information from attributes (user, uid, timestamp, version, changeset)
    QVariantHash nodeData;
    QStringList nodes, ways;
    nodeData.insert( "type", "relation" );

    while ( !atEnd() || waitOnRecoverableError() ) {
        readNext();

        if ( isEndElement() && name().compare(QLatin1String("relation"), Qt::CaseInsensitive) == 0 ) {
            break;
        }

        if ( isStartElement() ) {
            if ( name().compare(QLatin1String("tag"), Qt::CaseInsensitive) == 0 ) {
                readTag( &nodeData );
            } else if ( name().compare(QLatin1String("member"), Qt::CaseInsensitive) == 0 ) {
                QString nodeOrWay = attributes().value( QLatin1String("ref") ).toString();
                if ( !nodeOrWay.isEmpty() ) {
                    QString type = attributes().value( QLatin1String("type") ).toString();
                    if ( type == QLatin1String("node") ) {
                        nodes << nodeOrWay;
                    } else if ( type == QLatin1String("way") ) {
                        ways << nodeOrWay;
                    } else {
                        kDebug() << "Unknown member type" << type
                                 << "of relation" << id;
                    }
                }
            } else {
                readUnknownElement();
            }
        }
    }

    if ( isResultValid(nodeData) ) {
        if ( !nodes.isEmpty() ) {
            nodeData.insert( "nodes", nodes ); // IDs of associated nodes
        }
        if ( !ways.isEmpty() ) {
            nodeData.insert( "ways", ways ); // IDs of associated ways
        }
        m_data.insert( id, nodeData );
    }
}

void OsmReader::readTag( QVariantHash *nodeData ) {
    if ( !attributes().hasAttribute("k") || !attributes().hasAttribute("v") ) {
        kDebug() << "Key or value attribute not found for <tag>";
    }

    // Simply use the keys from OpenStreetMap. Maybe it's better to translate
    // them ("addr:street" => "street", then maybe combined with "addr:housenumber").
    nodeData->insert( attributes().value("k").toString(), attributes().value("v").toString() );
}
