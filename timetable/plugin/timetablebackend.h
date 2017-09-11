#ifndef TIMETABLEBACKEND_H
#define TIMETABLEBACKEND_H

#include "timetablehelper.h"

#include <QObject>

class TimetableBackend : public QObject
{
    Q_OBJECT

public:
    TimetableBackend(QObject *parent=0);
    ~TimetableBackend();

public slots:
    Q_INVOKABLE void ghnsDialogRequested();

private:
    TimetableHelper *m_helper;
};

#endif // TIMETABLEBACKEND_H