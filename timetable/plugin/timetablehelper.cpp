#include "timetablehelper.h"

#include <QString>
#include <QLocale>

#include <KDialog>
#include <KNewStuff3/kns3/downloaddialog.h>


namespace Timetable {

using namespace KNS3;

TimetableHelper::TimetableHelper(QWidget* parent)
    : KDialog(parent),
      m_download(new KDialog())
{}

TimetableHelper::~TimetableHelper()
{
    delete m_download;
}

void TimetableHelper::downloadProviders()
{
    m_download = new DownloadDialog("timetable.knsrc", this);
    m_download->setAttribute(Qt::WA_DeleteOnClose);
    m_download->show();
}

QString TimetableHelper::countryName(QString countryCode) const
{
    QLocale locale(countryCode);
    return QLocale::countryToString(locale.country());
}

}
