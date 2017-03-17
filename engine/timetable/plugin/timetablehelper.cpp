#include "timetablehelper.h"

#include <KNewStuff3/kns3/downloaddialog.h>

TimetableHelper::TimetableHelper(QWidget* parent)
    : KDialog(parent)
{
}

TimetableHelper::~TimetableHelper()
{
}

void TimetableHelper::showDialog()
{
    KNS3::DownloadDialog *dialog = new KNS3::DownloadDialog("timetable.knsrc", this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
