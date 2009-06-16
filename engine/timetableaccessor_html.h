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
	// Parses the contents of the document at the url
	QList<DepartureInfo> parseDocument();

	// Gets the "raw" url
	virtual QString rawUrl();
	// The regexp string to use
	virtual QString regExpSearch();
	// The meanings of matches of the regexp
	virtual QList< TimetableInformation > regExpInfos();
	virtual DepartureInfo getInfo(QRegExp rx);

    private:
	TimetableAccessorInfo m_info;
};

#endif // TIMETABLEACCESSOR_HTML_HEADER
