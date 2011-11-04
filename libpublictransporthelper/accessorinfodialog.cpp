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

#include "accessorinfodialog.h"
#include "ui_accessorInfo.h"

#include <KToolInvocation>
#include <KMessageBox>
#include <KDebug>

#include <Plasma/Service>
#include <Plasma/ServiceJob>
#include <Plasma/DataEngine>

/** @brief Namespace for the publictransport helper library. */
namespace Timetable {

class AccessorInfoDialogPrivate {
public:
    AccessorInfoDialogPrivate( const QVariantHash &_serviceProviderData,
            Plasma::DataEngine *publicTransportEngine, AccessorInfoDialog::Options _options )
            : publicTransportEngine(publicTransportEngine), service(0)
    {
        serviceProviderData = _serviceProviderData;
        options = _options;
    };

    Ui::accessorInfo uiAccessorInfo;
    QVariantHash serviceProviderData;
    AccessorInfoDialog::Options options;
    Plasma::DataEngine *publicTransportEngine;
    Plasma::Service *service;
};

AccessorInfoDialog::AccessorInfoDialog( const QVariantHash &serviceProviderData, const QIcon &icon,
        Plasma::DataEngine *publicTransportEngine, AccessorInfoDialog::Options options,
        QWidget* parent )
        : KDialog(parent), d_ptr(new AccessorInfoDialogPrivate(serviceProviderData,
                                                               publicTransportEngine, options))
{

    QWidget *widget = new QWidget;
    d_ptr->uiAccessorInfo.setupUi( widget );

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
        d_ptr->uiAccessorInfo.lblGtfsFeed->hide();
        d_ptr->uiAccessorInfo.gtfsFeed->hide();
    } else {
        // Is a GTFS accessor
        feedSizeInBytes = serviceProviderData["gtfsDatabaseSize"].toInt();
        d_ptr->uiAccessorInfo.lblGtfsFeed->show();
        d_ptr->uiAccessorInfo.gtfsFeed->show();
        if ( feedSizeInBytes <= 0 ) {
            d_ptr->uiAccessorInfo.gtfsFeed->setText( i18nc("@info:label",
                    "<a href='%1'>%1</a>,<nl/>not imported", feedUrl) );
        } else {
            d_ptr->uiAccessorInfo.gtfsFeed->setText( i18nc("@info:label",
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

    d_ptr->uiAccessorInfo.icon->setPixmap( icon.pixmap( 32 ) );
    d_ptr->uiAccessorInfo.serviceProviderName->setText(serviceProviderData["name"].toString() );
    d_ptr->uiAccessorInfo.version->setText( i18nc("@info/plain", "Version %1",
            serviceProviderData["version"].toString()) );
    d_ptr->uiAccessorInfo.url->setUrl( serviceProviderData["url"].toString() );
    d_ptr->uiAccessorInfo.url->setText( QString("<a href='%1'>%1</a>").arg(
            serviceProviderData["url"].toString()) );

    d_ptr->uiAccessorInfo.fileName->setUrl( serviceProviderData["fileName"].toString() );
    d_ptr->uiAccessorInfo.fileName->setText( QString("<a href='%1'>%1</a>").arg(
            serviceProviderData["fileName"].toString()) );

    QString scriptFileName = serviceProviderData["scriptFileName"].toString();
    if ( scriptFileName.isEmpty() ) {
        d_ptr->uiAccessorInfo.lblScriptFileName->hide();
        d_ptr->uiAccessorInfo.scriptFileName->hide();
    } else {
        d_ptr->uiAccessorInfo.lblScriptFileName->show();
        d_ptr->uiAccessorInfo.scriptFileName->show();
        d_ptr->uiAccessorInfo.scriptFileName->setUrl( scriptFileName );
        d_ptr->uiAccessorInfo.scriptFileName->setText( QString("<a href='%1'>%1</a>")
                .arg(scriptFileName) );
    }

    if ( serviceProviderData["email"].toString().isEmpty() ) {
        d_ptr->uiAccessorInfo.author->setText( serviceProviderData["author"].toString() );
    } else {
        d_ptr->uiAccessorInfo.author->setText( QString("<a href='mailto:%2'>%1</a> (%3)")
                .arg(serviceProviderData["author"].toString())
                .arg(serviceProviderData["email"].toString())
                .arg(serviceProviderData["shortAuthor"].toString()) );
        d_ptr->uiAccessorInfo.author->setToolTip( i18nc("@info",
                "Write an email to <email address='%2'>%1</email> (%3)",
                serviceProviderData["author"].toString(),
                serviceProviderData["email"].toString(),
                serviceProviderData["shortAuthor"].toString()) );
    }
    d_ptr->uiAccessorInfo.description->setText( serviceProviderData["description"].toString() );
    d_ptr->uiAccessorInfo.features->setText( serviceProviderData["featuresLocalized"].toStringList().join(", ") );

    QStringList changelogEntries = serviceProviderData["changelog"].toStringList();
    if ( changelogEntries.isEmpty() ) {
        d_ptr->uiAccessorInfo.lblChangelog->hide();
        d_ptr->uiAccessorInfo.changelog->hide();
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
        d_ptr->uiAccessorInfo.changelog->setHtml( changelog );
    }
}

AccessorInfoDialog::~AccessorInfoDialog()
{
    delete d_ptr;
}

void AccessorInfoDialog::slotButtonClicked( int button )
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

void AccessorInfoDialog::openInTimetableMate()
{
    Q_D( AccessorInfoDialog );
    QString error;
    int result = KToolInvocation::startServiceByDesktopName( "timetablemate",
            d->serviceProviderData["fileName"].toString(), &error );
    if ( result != 0 ) {
        KMessageBox::error( this, i18nc("@info",
                "TimetableMate couldn't be started, error message was: '%1'", error) );
    }
}

void AccessorInfoDialog::deleteGtfsDatabase()
{
    Q_D( AccessorInfoDialog );

    qint64 feedSizeInBytes = d->serviceProviderData["gtfsDatabaseSize"].toInt();
    if ( KMessageBox::warningContinueCancel(this, i18nc("@info",
            "<title>Delete GTFS database</title>"
            "<para>Do you really want to delete the GTFS database? You will need to import the "
            "GTFS feed again to use this service provider again.</para>"
            "<para>By deleting the database %1 disk space get freed.</para>",
            KGlobal::locale()->formatByteSize(feedSizeInBytes))) == KMessageBox::Continue )
    {
        if ( !d->service ) {
            d->service = d->publicTransportEngine->serviceForSource( QString() );
            d->service->setParent( this );
        }
        KConfigGroup op = d->service->operationDescription("deleteGtfsDatabase");
        op.writeEntry( "serviceProviderId", d->serviceProviderData["id"] );
        Plasma::ServiceJob *deleteJob = d->service->startOperationCall( op );
        connect( deleteJob, SIGNAL(result(KJob*)), this, SLOT(deletionFinished(KJob*)) );
    }
}

void AccessorInfoDialog::deletionFinished( KJob *job )
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
