#include "timetableapplet.h"

#include <QString>

#include <KNewStuff3/kns3/downloaddialog.h>

TimetableHelper::TimetableHelper(QObject* parent, const QVariantList& data)
: Plasma::Applet(parent, data)
{
}

TimetableHelper::~TimetableHelper()
{
}

void TimetableHelper::getNewProviders()
{
    KNS3::DownloadDialog *dialog = new KNS3::DownloadDialog(QString::fromLatin1("publictransport.knsrc"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

K_EXPORT_PLASMA_APPLET_WITH_JSON(timetablehelper, TimetableHelper, "metadata.json")

#include "timetableapplet.moc"
