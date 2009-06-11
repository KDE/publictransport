#ifndef TIMETABLEACCESSOR_EFA_HEADER
#define TIMETABLEACCESSOR_EFA_HEADER

#include "timetableaccessor.h"

class TimetableAccessorEfa : public TimetableAccessor
{
    public:
	virtual ServiceProvider serviceProvider() { return None; };

    protected:
	QList<DepartureInfo> parseDocument(QString document); // parses the contents of the document at the url
	
	virtual QString rawUrl(); // gets the "raw" url
	virtual QString regExpSearch(); // the regexp string to use
	virtual DepartureInfo getInfo(QRegExp rx);
};

#endif // TIMETABLEACCESSOR_EFA_HEADER
