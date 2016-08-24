#ifndef TIMETABLEHELPER_H
#define TIMETABLEHELPER_H

#include <QObject>
#include <QWidget>

#include <KDialog>

class TimetableHelper : public KDialog
{
    Q_OBJECT

public:
    TimetableHelper(QWidget *parent=0);
    ~TimetableHelper();

    Q_INVOKABLE void displayDownloadDialog();
};

#endif // TIMETABLEHELPER_H