/*
 *   Copyright 2012 Friedrich Pülz <fpuelz@gmx.de>
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
#include <Plasma/DataEngineManager>

/** @brief Namespace for the publictransport helper library. */
namespace PublicTransport {

class ServiceProviderDataWidgetPrivate {
public:
    ServiceProviderDataWidgetPrivate( const QString &providerId,
                                      ServiceProviderDataWidget::Options _options,
                                      ServiceProviderDataWidget *q )
            : q(q), providerId(providerId), options(_options),
              importFinished(false), feedSizeInBytes(0)
    {
        uiProviderData.setupUi( q );
    };

    void updateIcon( const QIcon &icon ) {
        uiProviderData.icon->setPixmap( icon.pixmap( 32 ) );
    }

    void update( const Plasma::DataEngine::Data &data ) {
        uiProviderData.type->setText( data["type"].toString() );

        const QString state = data["state"].toString();
        const QVariantHash stateData = data["stateData"].toHash();
        uiProviderData.state->setText( stateData["statusMessage"].toString() );

        const QString type = data["type"].toString();
        if ( type != QLatin1String("GTFS") ) {
            uiProviderData.lblGtfsFeed->hide();
            uiProviderData.gtfsFeed->hide();
            uiProviderData.deleteGtfsDatabaseButton->hide();
        } else {
            // Is a GTFS provider
            const QString feedUrl = data["feedUrl"].toString();
            uiProviderData.lblGtfsFeed->show();
            uiProviderData.gtfsFeed->show();
            if ( state == QLatin1String("ready") ) {
                uiProviderData.deleteGtfsDatabaseButton->setEnabled( true );
                feedSizeInBytes = stateData["gtfsDatabaseSize"].toInt();
                importFinished = true;
                uiProviderData.gtfsFeed->setText( i18nc("@info:label",
                        "<a href='%1'>%1</a>,<nl/>%2 disk space used",
                        feedUrl, KGlobal::locale()->formatByteSize(feedSizeInBytes)) );
            } else {
                uiProviderData.deleteGtfsDatabaseButton->setEnabled( false );
                uiProviderData.gtfsFeed->setText(
                        i18nc("@info:label", "<a href='%1'>%1</a>", feedUrl) );
            }
        }

        // Set the tooltip for the "Delete GTFS Database" button also if it gets hidden,
        // because ServiceProviderDataDialog may use this tooltip for it's own (dialog) button
        uiProviderData.deleteGtfsDatabaseButton->setToolTip(
                i18nc("@info:tooltip", "<title>Delete GTFS Database</title>"
                    "<para>The GTFS database contains all data imported from the GTFS "
                    "feed. If you delete the database now the GTFS feed needs to be "
                    "imported again to make this service provider usable again.</para>"
                    "<para>By deleting the database %1 disk space get freed.</para>",
                KGlobal::locale()->formatByteSize(feedSizeInBytes) ) );
        if ( !importFinished ||
             !options.testFlag(ServiceProviderDataWidget::ShowDeleteGtfsDatabaseButton) )
        {
            uiProviderData.lblOperations->hide();
            uiProviderData.deleteGtfsDatabaseButton->hide();
        }

        uiProviderData.serviceProviderName->setText(data["name"].toString() );
        uiProviderData.version->setText( i18nc("@info/plain", "Version %1",
                data["version"].toString()) );
        uiProviderData.url->setUrl( data["url"].toString() );
        uiProviderData.url->setText( QString("<a href='%1'>%1</a>").arg(
                data["url"].toString()) );

        uiProviderData.fileName->setUrl( data["fileName"].toString() );
        uiProviderData.fileName->setText( QString("<a href='%1'>%1</a>").arg(
                data["fileName"].toString()) );

        QString scriptFileName = data["scriptFileName"].toString();
        if ( scriptFileName.isEmpty() ) {
            uiProviderData.lblScriptFileName->setVisible( false );
            uiProviderData.scriptFileName->setVisible( false );
        } else {
            uiProviderData.lblScriptFileName->setVisible( true );
            uiProviderData.scriptFileName->setVisible( true );
            uiProviderData.scriptFileName->setUrl( scriptFileName );
            uiProviderData.scriptFileName->setText( QString("<a href='%1'>%1</a>")
                    .arg(scriptFileName) );
        }

        if ( data["email"].toString().isEmpty() ) {
            uiProviderData.author->setText( data["author"].toString() );
        } else {
            uiProviderData.author->setText( QString("<a href='mailto:%2'>%1</a> (%3)")
                    .arg(data["author"].toString())
                    .arg(data["email"].toString())
                    .arg(data["shortAuthor"].toString()) );
            uiProviderData.author->setToolTip( i18nc("@info",
                    "Write an email to <email address='%2'>%1</email> (%3)",
                    data["author"].toString(),
                    data["email"].toString(),
                    data["shortAuthor"].toString()) );
        }
        uiProviderData.description->setText( data["description"].toString() );
        uiProviderData.features->setText( data["featureNames"].toStringList().join(", ") );

        QStringList changelogEntries = data["changelog"].toStringList();
        if ( changelogEntries.isEmpty() ) {
            uiProviderData.lblChangelog->hide();
            uiProviderData.changelog->hide();
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
            uiProviderData.changelog->setHtml( changelog );
        }

        q->emit providerStateChanged( state, stateData );
    };

    QString providerFileName() const {
        return uiProviderData.fileName->url();
    };

    ServiceProviderDataWidget *q;
    Ui::providerData uiProviderData;
    QString providerId;
    ServiceProviderDataWidget::Options options;
    bool importFinished;
    qint64 feedSizeInBytes;
};

ServiceProviderDataWidget::ServiceProviderDataWidget( const QString &providerId,
        ServiceProviderDataWidget::Options options, QWidget *parent )
        : QWidget(parent),
          d_ptr(new ServiceProviderDataWidgetPrivate(providerId, options, this))
{
    Plasma::DataEngineManager::self()->loadEngine( "publictransport" );
    Plasma::DataEngineManager::self()->loadEngine( "favicons" );
    Plasma::DataEngine *engine = Plasma::DataEngineManager::self()->engine( "publictransport" );
    engine->connectSource( "ServiceProvider " + providerId, this );
}

ServiceProviderDataWidget::~ServiceProviderDataWidget()
{
    delete d_ptr;
    Plasma::DataEngineManager::self()->unloadEngine( "publictransport" );
    Plasma::DataEngineManager::self()->unloadEngine( "favicons" );
}

void ServiceProviderDataWidget::dataUpdated( const QString &sourceName,
                                             const Plasma::DataEngine::Data &data )
{
    Q_D( ServiceProviderDataWidget );
    if ( sourceName == "ServiceProvider " + d->providerId ) {
        d->update( data );

        // Request favicon
        Plasma::DataEngine *favIconEngine = Plasma::DataEngineManager::self()->engine( "favicons" );
        if ( favIconEngine ) {
            QString favIconSource = data["url"].toString();
            favIconEngine->connectSource( favIconSource, this );
        }
    } else {
        // Favicon of a service provider arrived
        QPixmap favicon( QPixmap::fromImage( data["Icon"].value<QImage>() ) );
        if ( favicon.isNull() ) {
            // No favicon found for sourceName;
            favicon = QPixmap( 16, 16 );
            favicon.fill( Qt::transparent );
        }

        d->updateIcon( KIcon(favicon) );
    }
}

bool ServiceProviderDataWidget::isImportFinished() const
{
    Q_D( const ServiceProviderDataWidget );
    return d->importFinished;
}

class ServiceProviderDataDialogPrivate {
public:
    ServiceProviderDataDialogPrivate() : widget(0) {};
    ServiceProviderDataWidget *widget;
};

ServiceProviderDataDialog::ServiceProviderDataDialog( const QString &providerId,
        ServiceProviderDataDialog::Options options, QWidget *parent )
        : KDialog(parent), d_ptr(new ServiceProviderDataDialogPrivate())
{
    Q_D( ServiceProviderDataDialog );

    d->widget = new ServiceProviderDataWidget(
            providerId, ServiceProviderDataWidget::NoOption, this );
    connect( d->widget, SIGNAL(providerStateChanged(QString,QVariantHash)),
             this, SLOT(providerStateChanged(QString,QVariantHash)) );

    setModal( true );
    setButtons( KDialog::Ok );
    setMainWidget( d->widget );
    setWindowTitle( i18nc("@title:window", "Service Provider Information") );
    setWindowIcon( KIcon("help-about") );

    ButtonCodes buttonCodes = Ok;
    if ( options.testFlag(ShowOpenInTimetableMateButton) ) {
        buttonCodes |= User1; // Add "Open in TimetableMate..." button
    }
    if ( options.testFlag(ShowDeleteGtfsDatabaseButton) ) {
        buttonCodes |= User2; // Add "Delete GTFS database" button
    }

    setButtons( buttonCodes );

    if ( options.testFlag(ShowOpenInTimetableMateButton) ) {
        setButtonIcon( User1, KIcon("document-open") );
        setButtonText( User1, i18nc("@action:button", "Open in TimetableMate...") );
    }

    if ( buttonCodes.testFlag(User2) ) {
        KPushButton *button = d->widget->d_ptr->uiProviderData.deleteGtfsDatabaseButton;
        setButtonIcon( User2, KIcon(button->icon()) );
        setButtonText( User2, button->text() );
        setButtonToolTip( User2, button->toolTip() );
        enableButton( User2, d->widget->isImportFinished() );
    }
}

ServiceProviderDataDialog::~ServiceProviderDataDialog()
{
    delete d_ptr;
}

ServiceProviderDataWidget *ServiceProviderDataDialog::providerDataWidget() const
{
    Q_D( const ServiceProviderDataDialog );
    return d->widget;
}

void ServiceProviderDataDialog::providerStateChanged( const QString &state,
                                                      const QVariantHash &stateData )
{
    Q_UNUSED( stateData )

    // Enable the "Delete GTFS Database" button only when the provider is ready,
    // ie. the GTFS feed was imported
    enableButton( User2, state == QLatin1String("ready") );
}

void ServiceProviderDataDialog::slotButtonClicked( int button )
{
    Q_D( ServiceProviderDataDialog );
    switch ( button ) {
    case User1:
        openInTimetableMate();
        break;
    case User2:
        d->widget->deleteGtfsDatabase();
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
            d->widget->d_ptr->providerFileName(), &error );
    if ( result != 0 ) {
        KMessageBox::error( this, i18nc("@info",
                "TimetableMate couldn't be started, error message was: '%1'", error) );
    }
}

void ServiceProviderDataWidget::deleteGtfsDatabase()
{
    Q_D( ServiceProviderDataWidget );

    if ( KMessageBox::warningContinueCancel(this, i18nc("@info",
            "<title>Delete GTFS database</title>"
            "<para>Do you really want to delete the GTFS database? You will need to import the "
            "GTFS feed again to use this service provider again.</para>"
            "<para>By deleting the database %1 disk space get freed.</para>",
            KGlobal::locale()->formatByteSize(d->feedSizeInBytes))) == KMessageBox::Continue )
    {
        Plasma::DataEngine *engine = Plasma::DataEngineManager::self()->engine("publictransport");
        Plasma::Service *gtfsService = engine->serviceForSource("GTFS");
        KConfigGroup op = gtfsService->operationDescription("deleteGtfsDatabase");
        op.writeEntry( "serviceProviderId", d->providerId );
        Plasma::ServiceJob *deleteJob = gtfsService->startOperationCall( op );
        kDebug() << gtfsService << "call deleteGtfsDatabase" << deleteJob;
        connect( deleteJob, SIGNAL(result(KJob*)), this, SLOT(deletionFinished(KJob*)) );
        connect( deleteJob, SIGNAL(finished(KJob*)), gtfsService, SLOT(deleteLater()) );
    }
}

void ServiceProviderDataWidget::deletionFinished( KJob *job )
{
    if ( job->error() != 0 ) {
        KMessageBox::information( this, i18nc("@info", "Deleting the GTFS database failed: "
                                              "<message>%1</message>", job->errorString()) );
    }
}

void ServiceProviderDataDialog::gtfsDatabaseDeletionFinished() {
    // Disable "Delete GTFS database" button
    enableButton( KDialog::User2, false );
}

} // namespace Timetable
