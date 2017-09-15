#ifndef TIMETABLEHELPER_H
#define TIMETABLEHELPER_H

#include <KDialog>

class QWidget;

namespace Timetable {

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

    Q_INVOKABLE void downloadProviders();
    Q_INVOKABLE QString countryName(QString ) const;

private:
    KDialog *m_download;
};

}


#endif // TIMETABLEHELPER_H
