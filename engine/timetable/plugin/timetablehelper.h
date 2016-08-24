#ifndef TIMETABLEHELPER_H
#define TIMETABLEHELPER_H

#include <QObject>
#include <QHash>
#include <QVariantMap>

class TimetableHelper : public QObject
{
    Q_OBJECT

public:
    TimetableHelper(QObject *parent=0);
    ~TimetableHelper();

    Q_INVOKABLE void displayDownloadDialog();
};

#endif // TIMETABLEHELPER_H