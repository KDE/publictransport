#ifndef TIMETABLEACCESSOR_HTML_HEADER
#define TIMETABLEACCESSOR_HTML_HEADER

#include "timetableaccessor.h"
#include "timetableaccessor_html_infos.h"

class TimetableAccessorHtml : public TimetableAccessor
{
    public:
	TimetableAccessorHtml();
	TimetableAccessorHtml( ServiceProvider serviceProvider );
	
	virtual ServiceProvider serviceProvider() { return m_info.serviceProvider; };
	virtual QString country() const { return m_info.country; };
	virtual QStringList cities() const { return m_info.cities; };
	virtual bool putCityIntoUrl() const { return m_info.putCityIntoUrl; };

    protected:
	QList<DepartureInfo> parseDocument( const QString &document ); // parses the contents of the document at the url
	
	virtual QString rawUrl(); // gets the "raw" url
	virtual QString regExpSearch(); // the regexp string to use
	virtual QList< TimetableInformation > regExpInfos(); // The meanings of matches of the regexp
	virtual DepartureInfo getInfo(QRegExp rx);

    private:
	TimetableAccessorInfo m_info;
// 	ServiceProvider m_serviceProvider;
// 	QString m_rawUrl, m_regExpSearch, m_country;
// 	QStringList m_cities;
// 	QList< TimetableInformation > m_regExpInfos;
// 	bool m_putCityIntoUrl;
};

#endif // TIMETABLEACCESSOR_HTML_HEADER
