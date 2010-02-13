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

#ifndef OPENSTREETMAPDATAENGINE_HEADER
#define OPENSTREETMAPDATAENGINE_HEADER

#include <Plasma/DataEngine>

#include "osmreader.h"

class KJob;
namespace KIO {
    class Job;
}
class QSizeF;
class QBuffer;

/** @brief This engine searches for information from OpenStreetMap.
* The source name is:
*   "[longitude],[latitude] ([mapArea]) ([element] [filter]|[short-filter])".
* For example: "53.069,8.8 theatre"
* 		"53.069,8.8 0.1 theatre" (search in a bigger area)
* 		"53.069,8.8 publictransportstops"
* 		"53.069,8.8 node amenity=theatre" (custom search)
*
* TODO: Could use libweb from Project Silk, the base class for REST apis? */
class OpenStreetMapEngine : public Plasma::DataEngine {
    Q_OBJECT
    public:
        // Basic Create/Destroy
        OpenStreetMapEngine( QObject *parent, const QVariantList &args );
	
	static const int maxResults = 50;
	static const double maxAreaSize = 0.5;
	static const double defautAreaSize = 0.02;
	
    protected:
	virtual bool sourceRequestEvent( const QString& source );
	virtual bool updateSourceEvent( const QString& source );

	const QString baseUrl() const {
		return "http://xapi.openstreetmap.org/api/0.6/"; };

    public slots:
	/** Download has finished. */
	void finished( KJob *job );
	
	/** More downloaded data is available. */
	void data( KIO::Job *job, const QByteArray &ba );

	/** Reading an XML document has finished (reached the end of the document).
	* @Note @p data only contains the last chunk of data. */
	void osmFinishedReading( OsmReader *osmReader, const Plasma::DataEngine::Data &data );
	
	/** A new chunk of the XML document has been read. */
	void osmChunkRead( OsmReader *osmReader, const Plasma::DataEngine::Data &data );
	
    private:
	enum Element {
	    Node, Relation, Way
	};
	
	struct Filter {
	    Element element;
	    QString filter;

	    Filter() {};
	    Filter( Element element, const QString &filter ) {
		this->element = element;
		this->filter = filter; };
	};

	/** Data stored for each download job. */
	struct JobInfo {
	    QString sourceName;
	    OsmReader *osmReader;
	    bool readStarted;

	    JobInfo() {};
	    JobInfo( const QString &sourceName, OsmReader *osmReader ) {
		this->sourceName = sourceName;
		this->osmReader = osmReader;
		this->readStarted = false;
	    };
	};

	QString elementToString( Element element ) const;

	
	QHash< KJob*, JobInfo > m_jobInfos;
	QHash< QString, Filter > m_shortFilter;
};

#endif // Multiple include guard
