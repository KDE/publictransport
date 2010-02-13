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

#ifndef OSMREADER_HEADER
#define OSMREADER_HEADER

#include <Plasma/DataEngine>

#include <QXmlStreamReader>
#include <QEventLoop>

class TimetableAccessor;

class OsmReader : public QObject, public QXmlStreamReader {
    Q_OBJECT;
    
    public:
	OsmReader( const QString &associatedSourceName ) : QXmlStreamReader() {
	    m_associatedSourceName = associatedSourceName; };

	void read();
	Plasma::DataEngine::Data data() const { return m_data; };
	
	void resumeReading() { m_loop.quit(); };
	QString associatedSourceName() const { return m_associatedSourceName; };

    signals:
	/** Reading an XML document has finished (reached the end of the document).
	* @Note @p lastDataChunk only contains the last chunk of data. */
	void finishedReading( OsmReader *reader,
			      const Plasma::DataEngine::Data &lastDataChunk );
			      
	/** A new chunk of the XML document has been read. */
	void chunkRead( OsmReader *reader, const Plasma::DataEngine::Data &dataChunk );
	
    private:
	void readUnknownElement();
	void readOsm();
	void readNode();
	void readWay();
	void readRelation();
	void readTag( QHash< QString, QVariant > *nodeData );

	bool waitOnRecoverableError();

	Plasma::DataEngine::Data m_data;
	QEventLoop m_loop;
	QString m_associatedSourceName;
};

#endif // OSMREADER_HEADER