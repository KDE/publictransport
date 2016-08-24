#include "enginehelper.h"

#include <KNewStuff3/KNS3/DownloadDialog>

TimetableHelper::TimetableHelper(QObject* parent)
    : QObject(parent)
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
