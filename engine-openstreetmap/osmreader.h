/*
*   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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
	enum ResultFlag {
	    AllResults = 0x0000,
	    OnlyResultsWithNameAttribute = 0x0001
	};
	Q_DECLARE_FLAGS( ResultFlags, ResultFlag );

	OsmReader( const QString &associatedSourceName, const QString &sourceUrl,
               ResultFlags resultFlags = ResultFlags(AllResults) )
            : QXmlStreamReader()
    {
	    m_associatedSourceName = associatedSourceName;
        m_sourceUrl = sourceUrl;
	    m_resultFlags = resultFlags;
    };

	void read();
	Plasma::DataEngine::Data data() const { return m_data; };

	void resumeReading() { m_loop.quit(); };
    QString associatedSourceName() const { return m_associatedSourceName; };
    QString sourceUrl() const { return m_sourceUrl; };

signals:
	/**
     * @brief Reading an XML document has finished (reached the end of the document).
     *
	 * @Note @p lastDataChunk only contains the last chunk of data.
     **/
	void finishedReading( QPointer<OsmReader> reader, const Plasma::DataEngine::Data &lastDataChunk );

	/** @brief A new chunk of the XML document has been read. */
	void chunkRead( QPointer<OsmReader> reader, const Plasma::DataEngine::Data &dataChunk );

private:
	bool isResultValid( const QVariantHash &data ) const;

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
	ResultFlags m_resultFlags;
    QString m_sourceUrl;
};
Q_DECLARE_OPERATORS_FOR_FLAGS( OsmReader::ResultFlags );

#endif // OSMREADER_HEADER
