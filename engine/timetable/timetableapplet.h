#ifndef TIMETABLEAPPLET_H
#define TIMETABLEAPPLET_H

#include <QObject>

#include <Plasma/Applet>

/**
 * Class to export the desktop file to json format
 * and display GHNS download dialog
 */
class TimetableHelper : public Plasma::Applet
{
    Q_OBJECT

public:
    TimetableHelper(QObject *parent, const QVariantList &data);
    ~TimetableHelper();

public slots:
    Q_INVOKABLE void getNewProviders();
};

#endif // TIMETABLEAPPLET_H
