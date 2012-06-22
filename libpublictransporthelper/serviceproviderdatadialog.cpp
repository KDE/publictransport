/*
 *   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

// Header
#include "serviceproviderdatadialog.h"

// Own includes
#include "ui_providerData.h"

// KDE includes
#include <KToolInvocation>
#include <KMessageBox>
#include <KDebug>

#include <Plasma/Service>
#include <Plasma/ServiceJob>
#include <Plasma/DataEngine>

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

class ServiceProviderDataDialogPrivate {
public:
    ServiceProviderDataDialogPrivate( const QVariantHash &_serviceProviderData,
                               ServiceProviderDataDialog::Options _options ) {
        serviceProviderData = _serviceProviderData;
        options = _options;
    };

    Ui::providerData uiProviderData;
    QVariantHash serviceProviderData;
    ServiceProviderDataDialog::Options options;
};

ServiceProviderDataDialog::ServiceProviderDataDialog( const QVariantHash &serviceProviderData, const QIcon &icon,
        ServiceProviderDataDialog::Options options, QWidget* parent )
        : KDialog(parent), d_ptr(new ServiceProviderDataDialogPrivate(serviceProviderData, options))
{

    QWidget *widget = new QWidget;
    d_ptr->uiProviderData.setupUi( widget );

    setModal( true );
    setButtons( KDialog::Ok );
    setMainWidget( widget );
    setWindowTitle( i18nc("@title:window", "Service Provider Information") );
    setWindowIcon( KIcon("help-about") );

    KDialog::ButtonCodes buttonCodes = KDialog::Ok;
    if ( options.testFlag(ShowOpenInTimetableMateButton) ) {
        buttonCodes |= KDialog::User1; // Add "Open in TimetableMate..." button
    }
    QString feedUrl = serviceProviderData["feedUrl"].toString();
    qint64 feedSizeInBytes = 0;
    if ( feedUrl.isEmpty() ) {
        d_ptr->uiProviderData.lblGtfsFeed->hide();
        d_ptr->uiProviderData.gtfsFeed->hide();
    } else {
        // Is a GTFS provider
        feedSizeInBytes = serviceProviderData["gtfsDatabaseSize"].toInt();
        d_ptr->uiProviderData.lblGtfsFeed->show();
        d_ptr->uiProviderData.gtfsFeed->show();
        if ( feedSizeInBytes <= 0 ) {
            d_ptr->uiProviderData.gtfsFeed->setText( i18nc("@info:label",
                    "<a href='%1'>%1</a>,<nl/>not imported", feedUrl) );
        } else {
            d_ptr->uiProviderData.gtfsFeed->setText( i18nc("@info:label",
                    "<a href='%1'>%1</a>,<nl/>%2 disk space used",
                    feedUrl, KGlobal::locale()->formatByteSize(feedSizeInBytes)) );
            buttonCodes |= KDialog::User2; // Add "Delete GTFS database" button
        }
    }

    setButtons( buttonCodes );

    if ( options.testFlag(ShowOpenInTimetableMateButton) ) {
        setButtonIcon( KDialog::User1, KIcon("document-open") );
        setButtonText( KDialog::User1, i18nc("@action:button", "Open in TimetableMate...") );
    }

    if ( feedSizeInBytes > 0 ) {
        setButtonIcon( KDialog::User2, KIcon("edit-delete") );
        setButtonText( KDialog::User2, i18nc("@action:button", "Delete GTFS Database") );
        setButtonToolTip( KDialog::User2, i18nc("@info:tooltip",
                "<title>Delete GTFS Database</title>"
                "<para>The GTFS database contains all data imported from the GTFS feed. "
                "If you delete the database now the GTFS feed needs to be imported again to make "
                "this service provider usable again.</para>"
                "<para>By deleting the database %1 disk space get freed.</para>",
                KGlobal::locale()->formatByteSize(feedSizeInBytes) ) );
    }

    d_ptr->uiProviderData.icon->setPixmap( icon.pixmap( 32 ) );
    d_ptr->uiProviderData.serviceProviderName->setText(serviceProviderData["name"].toString() );
    d_ptr->uiProviderData.version->setText( i18nc("@info/plain", "Version %1",
            serviceProviderData["version"].toString()) );
    d_ptr->uiProviderData.url->setUrl( serviceProviderData["url"].toString() );
    d_ptr->uiProviderData.url->setText( QString("<a href='%1'>%1</a>").arg(
            serviceProviderData["url"].toString()) );

    d_ptr->uiProviderData.fileName->setUrl( serviceProviderData["fileName"].toString() );
    d_ptr->uiProviderData.fileName->setText( QString("<a href='%1'>%1</a>").arg(
            serviceProviderData["fileName"].toString()) );

    QString scriptFileName = serviceProviderData["scriptFileName"].toString();
    if ( scriptFileName.isEmpty() ) {
        d_ptr->uiProviderData.lblScriptFileName->setVisible( false );
        d_ptr->uiProviderData.scriptFileName->setVisible( false );
    } else {
        d_ptr->uiProviderData.lblScriptFileName->setVisible( true );
        d_ptr->uiProviderData.scriptFileName->setVisible( true );
        d_ptr->uiProviderData.scriptFileName->setUrl( scriptFileName );
        d_ptr->uiProviderData.scriptFileName->setText( QString("<a href='%1'>%1</a>")
                .arg(scriptFileName) );
    }

    if ( serviceProviderData["email"].toString().isEmpty() ) {
        d_ptr->uiProviderData.author->setText( serviceProviderData["author"].toString() );
    } else {
        d_ptr->uiProviderData.author->setText( QString("<a href='mailto:%2'>%1</a> (%3)")
                .arg(serviceProviderData["author"].toString())
                .arg(serviceProviderData["email"].toString())
                .arg(serviceProviderData["shortAuthor"].toString()) );
        d_ptr->uiProviderData.author->setToolTip( i18nc("@info",
                "Write an email to <email address='%2'>%1</email> (%3)",
                serviceProviderData["author"].toString(),
                serviceProviderData["email"].toString(),
                serviceProviderData["shortAuthor"].toString()) );
    }
    d_ptr->uiProviderData.description->setText( serviceProviderData["description"].toString() );
    d_ptr->uiProviderData.features->setText( serviceProviderData["featuresLocalized"].toStringList().join(", ") );

    QStringList changelogEntries = serviceProviderData["changelog"].toStringList();
    if ( changelogEntries.isEmpty() ) {
        d_ptr->uiProviderData.lblChangelog->hide();
        d_ptr->uiProviderData.changelog->hide();
    } else {
        QString changelog("<ul style='margin-left:-20;'>");
        foreach ( const QString &entry, changelogEntries ) {
            int pos = entry.indexOf(':');
            if ( pos == -1 ) {
                changelog.append( QString("<li>%1</li>").arg(entry) );
            } else {
                QString e = entry;
                changelog.append( QString("<li><span style='font-style: italic;'>%1</li>")
                        .arg(e.insert(pos + 1, QLatin1String("</span>"))) );
            }
        }
        changelog.append( QLatin1String("</ul>") );
        d_ptr->uiProviderData.changelog->setHtml( changelog );
    }
}

ServiceProviderDataDialog::~ServiceProviderDataDialog()
{
    delete d_ptr;
}

void ServiceProviderDataDialog::slotButtonClicked( int button )
{
    switch ( button ) {
    case KDialog::User1:
        openInTimetableMate();
        break;
    case KDialog::User2:
        deleteGtfsDatabase();
        break;
    default:
        KDialog::slotButtonClicked( button );
        break;
    }
}

void ServiceProviderDataDialog::openInTimetableMate()
{
    Q_D( ServiceProviderDataDialog );
    QString error;
    int result = KToolInvocation::startServiceByDesktopName( "timetablemate",
            d->serviceProviderData["fileName"].toString(), &error );
    if ( result != 0 ) {
        KMessageBox::error( this, i18nc("@info",
                "TimetableMate couldn't be started, error message was: '%1'", error) );
    }
}

void ServiceProviderDataDialog::deleteGtfsDatabase()
{
    Q_D( ServiceProviderDataDialog );

    qint64 feedSizeInBytes = d->serviceProviderData["gtfsDatabaseSize"].toInt();
    if ( KMessageBox::warningContinueCancel(this, i18nc("@info",
            "<title>Delete GTFS database</title>"
            "<para>Do you really want to delete the GTFS database? You will need to import the "
            "GTFS feed again to use this service provider again.</para>"
            "<para>By deleting the database %1 disk space get freed.</para>",
            KGlobal::locale()->formatByteSize(feedSizeInBytes))) == KMessageBox::Continue )
    {
//         TODO TODO TODO
//         if ( !d->service ) {
//             d->service = d->publicTransportEngine->serviceForSource( QString() );
//             d->service->setParent( this );
//         }
//         KConfigGroup op = d->service->operationDescription("deleteGtfsDatabase");
//         op.writeEntry( "serviceProviderId", d->serviceProviderData["id"] );
//         Plasma::ServiceJob *deleteJob = d->service->startOperationCall( op );
//         connect( deleteJob, SIGNAL(result(KJob*)), this, SLOT(deletionFinished(KJob*)) );
    }
}

void ServiceProviderDataDialog::deletionFinished( KJob *job )
{
    if ( job->error() != 0 ) {
        KMessageBox::information( this, i18nc("@info", "Deleting the GTFS database failed") );
    } else {
        // Finished successfully
        emit gtfsDatabaseDeleted();
    }

    // Disable "Delete GTFS database" button
    enableButton( KDialog::User2, false );
}

} // namespace Timetable
