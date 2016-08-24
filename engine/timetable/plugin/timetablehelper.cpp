#include "timetablehelper.h"

#include <KNewStuff3/kns3/downloaddialog.h>

TimetableHelper::TimetableHelper(QWidget* parent)
    : KDialog(parent)
{
}

TimetableHelper::~TimetableHelper()
{
}

void TimetableHelper::displayDownloadDialog()
{
    KNS3::DownloadDialog *dialog = new KNS3::DownloadDialog("publictransport.knsrc", this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
