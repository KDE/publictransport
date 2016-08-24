#ifndef TIMETABLEHELPER_H
#define TIMETABLEHELPER_H

#include <QObject>
#include <QWidget>

#include <KDialog>
#include <Plasma/Applet>

/**
 * Class that aims to display the GHNS dialog
 * Enables the user to download new service providers
 */
class TimetableHelper : public KDialog
{
    Q_OBJECT

public:
    TimetableHelper(QWidget *parent=0);
    ~TimetableHelper();

    Q_INVOKABLE void displayDownloadDialog();
};

/**
 * Class to export the desktop file to json format
 */
class TimetableExporter : public Plasma::Applet
{
    Q_OBJECT

public:
    TimetableExporter(QObject *parent, const QVariantList &data);
    ~TimetableExporter() override;
};

#endif // TIMETABLEHELPER_H