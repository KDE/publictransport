#ifndef TIMETABLEACCESSOR_VRN_HEADER
#define TIMETABLEACCESSOR_VRN_HEADER

#include "timetableaccessor.h"

class TimetableAccessorVrn : public TimetableAccessor
{
    public:
	virtual ServiceProvider serviceProvider() { return VRN; };
	
	virtual QString country() const { return "Germany"; };
// 	virtual QStringList cities() const { return QStringList() << "?"; };

    protected:
// 	QList<DepartureInfo> parseDocument(QString document); // parses the contents of the document at the url
	virtual QString rawUrl(); // gets the "raw" url
	virtual QString regExpSearch(); // the regexp string to use
	virtual DepartureInfo getInfo(QRegExp rx);
};

#endif // TIMETABLEACCESSOR_VRN_HEADER
