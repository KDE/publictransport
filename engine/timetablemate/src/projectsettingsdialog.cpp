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
#include "projectsettingsdialog.h"

// Own includes
#include "ui_timetablemateview_base.h"
#include "project.h"
#include "settings.h"
#include "changelogwidget.h"
#include "serviceproviderdatawriter.h"
#include "serviceproviderdatatester.h"

// PublicTransport engine includes
#include <engine/serviceproviderdatareader.h>
#include <engine/serviceprovider.h>
#include <engine/serviceproviderdata.h>
#include <engine/global.h>

// KDE includes
#include <KFileDialog>
#include <KMessageBox>
#include <KInputDialog>
#include <KColorScheme>
#include <KLocale>
#include <KMessageWidget>
#include <KLineEdit>
#include <KActionCollection>

// Qt includes
#include <QtGui/QLabel>
#include <QMenu>
#include <QBuffer>
#include <QFile>
#include <QSignalMapper>
#include <QTimer>
#include <QEvent>
#include <QScrollArea>
#include <QValidator>
#include <QLayout>
#include <QFormLayout>
#include <QToolBar>

ProjectSettingsDialog::ProjectSettingsDialog( QWidget *parent )
        : KDialog(parent), ui_provider(new Ui::timetablemateview_base()), m_providerData(0),
          m_newScriptTemplateType(Project::NoScriptTemplate),
          m_shortAuthorAutoFilled(false), m_shortUrlAutoFilled(false),
          m_cityName(0), m_cityReplacement(0), m_changelog(0),
          m_actions(new KActionCollection(this)), m_mapper(0)
{
    QWidget *widget = new QWidget( this );
    widget->setAutoFillBackground( false );
    ui_provider->setupUi( widget );
    setMainWidget( widget );
    setCaption( i18nc("@title:window", "Project Settings") );
    setButtons( KDialog::Ok | KDialog::Cancel | KDialog::User1 );
    setButtonIcon( KDialog::User1, KIcon("dialog-ok-apply") );
    setButtonText( KDialog::User1, i18nc("@info/plain", "Check") );

    QToolBar *notesToolBar = new QToolBar( "notesToolBar", ui_provider->tabNotes );
    QToolBar *notesToolBar2 = new QToolBar( "notesToolBar2", ui_provider->tabNotes );
    ui_provider->notesLayout->insertWidget( 0, notesToolBar );
    ui_provider->notesLayout->insertWidget( 1, notesToolBar2 );
    ui_provider->notes->createActions( m_actions );
    QAction *separator1 = new QAction( this ), *separator2 = new QAction( this );
    separator1->setSeparator( true );
    separator2->setSeparator( true );
    notesToolBar->addActions( QList<QAction*>()
            << m_actions->action("format_text_bold")
            << m_actions->action("format_text_italic")
            << m_actions->action("format_text_underline")
            << m_actions->action("format_text_strikeout")
            << separator1
            << m_actions->action("format_align_left")
            << m_actions->action("format_align_center")
            << m_actions->action("format_align_right")
            << m_actions->action("format_align_justify")
            << separator2
            << m_actions->action("insert_horizontal_rule")
            << m_actions->action("manage_link")
            << m_actions->action("format_painter") );
    notesToolBar2->addActions( QList<QAction*>()
            << m_actions->action("format_font_family")
            << m_actions->action("format_font_size")
            << m_actions->action("format_list_style") );

    settingsChanged();

    // Initialize script file buttons
    ui_provider->btnBrowseForScriptFile->setIcon( KIcon("document-open") );
    ui_provider->btnCreateScriptFile->setIcon( KIcon("document-new") );
    ui_provider->btnDetachScriptFile->setIcon( KIcon("list-remove") );
    ui_provider->btnDetachScriptFile->setVisible( false );
    connect( ui_provider->btnBrowseForScriptFile, SIGNAL(clicked(bool)),
             this, SLOT(browseForScriptFile()) );
    connect( ui_provider->btnCreateScriptFile, SIGNAL(clicked(bool)),
             this, SLOT(createScriptFile()) );
    connect( ui_provider->btnDetachScriptFile, SIGNAL(clicked(bool)),
             this, SLOT(detachScriptFile()) );

    // Initialize the language button
    ui_provider->currentLanguage->loadAllLanguages();
    ui_provider->currentLanguage->insertLanguage( "en", QString(), 0 );
    ui_provider->currentLanguage->insertSeparator( 1 );
    connect( ui_provider->currentLanguage, SIGNAL(activated(QString)),
             this, SLOT(languageActivated(QString)) );

    // Autofill short author/URL fields, if they are empty
    // while editing the full author/URL fields
    connect( ui_provider->author, SIGNAL(textEdited(QString)),
             this, SLOT(authorEdited(QString)) );
    connect( ui_provider->shortAuthor, SIGNAL(textEdited(QString)),
             this, SLOT(shortAuthorEdited(QString)) );
    connect( ui_provider->url, SIGNAL(textEdited(QString)),
             this, SLOT(urlEdited(QString)) );
    connect( ui_provider->shortUrl, SIGNAL(textEdited(QString)),
             this, SLOT(shortUrlEdited(QString)) );

    // Initialize the KEditListWidget for predefined cities
    QWidget *repWidget = new QWidget( this );
    QHBoxLayout *customEditorLayout = new QHBoxLayout( repWidget );
    m_cityName = new KLineEdit( this );
    m_cityReplacement = new KLineEdit( this );
    QLabel *lblCityReplacement = new QLabel( i18nc("@info", "Replace with:"), this );
    lblCityReplacement->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
    customEditorLayout->addWidget( m_cityName );
    customEditorLayout->addWidget( lblCityReplacement );
    customEditorLayout->addWidget( m_cityReplacement );
    KLineEdit *defaultLineEdit = new KLineEdit();
    m_predefinedCitiesCustomEditor.setLineEdit( defaultLineEdit );
    defaultLineEdit->hide();

    m_predefinedCitiesCustomEditor.setRepresentationWidget( repWidget );
    ui_provider->predefinedCities->setCustomEditor( m_predefinedCitiesCustomEditor );
    connect( m_cityName, SIGNAL(textChanged(QString)),
             this, SLOT(predefinedCityNameChanged(QString)) );
    connect( m_cityReplacement, SIGNAL(textChanged(QString)),
             this, SLOT(predefinedCityReplacementChanged(QString)) );
    connect( defaultLineEdit, SIGNAL(textChanged(QString)),
             this, SLOT(currentPredefinedCityChanged(QString)) );

    // Set a validator for version line edits, allow major, minor and path version
    QRegExpValidator *versionValidator =
            new QRegExpValidator( QRegExp("\\d+(\\.\\d+)?(\\.\\d+)?"), this );
    ui_provider->version->setValidator( versionValidator );
    ui_provider->fileVersion->setValidator( versionValidator );

    // Set a validator for the email line edit
    // The reg exp is "inspired" by http://www.regular-expressions.info/email.html
    QRegExp rx( "[a-z0-9!#$%&\\._-]+@(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.)+[a-z]{2,4}",
                Qt::CaseInsensitive );
    QRegExpValidator *emailValidator = new QRegExpValidator( rx, this );
    ui_provider->email->setValidator( emailValidator );

    // Install event filters to filter out focus out events
    // if the line edit's text cannot be validated
    ui_provider->version->installEventFilter( this );
    ui_provider->fileVersion->installEventFilter( this );
    ui_provider->name->installEventFilter( this );
    ui_provider->description->installEventFilter( this );
    ui_provider->author->installEventFilter( this );
    ui_provider->shortAuthor->installEventFilter( this );
    ui_provider->email->installEventFilter( this );
    ui_provider->url->installEventFilter( this );
    ui_provider->shortUrl->installEventFilter( this );

    // Set icons and connections for the "open url buttons"
    ui_provider->btnUrlOpen->setIcon( KIcon("document-open-remote") );
    connect( ui_provider->btnUrlOpen, SIGNAL(clicked(bool)),
             this, SLOT(openUrlClicked()) );

    // Add changelog widget into a scroll area
    QVBoxLayout *changelogAreaLayout = new QVBoxLayout( ui_provider->tabChangelog );
    QScrollArea *changelogArea = new QScrollArea( ui_provider->tabChangelog );
    changelogArea->setFrameStyle( QFrame::NoFrame );
    changelogArea->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    changelogArea->setWidgetResizable( true );
    changelogAreaLayout->addWidget( changelogArea );

    QWidget *changelogAreaWidget = new QWidget( changelogArea );
    changelogArea->setWidget( changelogAreaWidget );
    QVBoxLayout *changelogLayout = new QVBoxLayout( changelogAreaWidget );
    m_changelog = new ChangelogWidget( changelogAreaWidget );
    m_changelog->clear();
    changelogLayout->addWidget( m_changelog );
    changelogLayout->addStretch();

    connect( m_changelog, SIGNAL(changelogEntryWidgetAdded(ChangelogEntryWidget*)),
             this, SLOT(changelogEntryWidgetAdded(ChangelogEntryWidget*)) );

    // Add vehicle types with icons to the default vehicle type combo box
    ui_provider->defaultVehicleType->addItem(
        KIcon( "status_unknown" ), i18nc( "@item:listbox", "Unknown" ), "Unknown" );
    ui_provider->defaultVehicleType->addItem(
        KIcon( "vehicle_type_tram" ), i18nc( "@item:listbox", "Tram" ), "Tram" );
    ui_provider->defaultVehicleType->addItem(
        KIcon( "vehicle_type_bus" ), i18nc( "@item:listbox", "Bus" ), "Bus" );
    ui_provider->defaultVehicleType->addItem(
        KIcon( "vehicle_type_subway" ), i18nc( "@item:listbox", "Subway" ), "Subway" );
    ui_provider->defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_interurban" ),
        i18nc( "@item:listbox", "Interurban Train" ), "TrainInterurban" );
    ui_provider->defaultVehicleType->addItem(
        KIcon( "vehicle_type_metro" ), i18nc( "@item:listbox", "Metro" ), "Metro" );
    ui_provider->defaultVehicleType->addItem(
        KIcon( "vehicle_type_trolleybus" ),
        i18nc( "@item:listbox", "Trolley Bus" ), "TrolleyBus" );
    ui_provider->defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_regional" ), // TODO: Currently no special icon
        i18nc( "@item:listbox", "Regional Train" ), "TrainRegional" );
    ui_provider->defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_regional" ),
        i18nc( "@item:listbox", "Regional Express Train" ), "TrainRegionalExpress" );
    ui_provider->defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_interregional" ),
        i18nc( "@item:listbox", "Interregional Train" ), "TrainInterregio" );
    ui_provider->defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_intercity" ),
        i18nc( "@item:listbox", "Intercity/Eurocity Train" ), "TrainIntercityEurocity" );
    ui_provider->defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_highspeed" ),
        i18nc( "@item:listbox", "Intercity Express Train" ), "TrainIntercityExpress" );
    ui_provider->defaultVehicleType->addItem(
        KIcon( "vehicle_type_ferry" ), i18nc( "@item:listbox", "Ferry" ), "Ferry" );
    ui_provider->defaultVehicleType->addItem(
        KIcon( "vehicle_type_ferry" ), i18nc( "@item:listbox", "Ship" ), "Ship" );
    ui_provider->defaultVehicleType->addItem(
        KIcon( "vehicle_type_plane" ), i18nc( "@item:listbox", "Plane" ), "Plane" );

    // Connect all change signals of the widgets to the changed() signal
    m_mapper = new QSignalMapper( this );
    connect( ui_provider->name, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_provider->description, SIGNAL(textChanged()), m_mapper, SLOT(map()) );
    connect( ui_provider->version, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_provider->type, SIGNAL(currentIndexChanged(int)), m_mapper, SLOT(map()) );
    connect( ui_provider->useCityValue, SIGNAL(stateChanged(int)), m_mapper, SLOT(map()) );
    connect( ui_provider->onlyAllowPredefinedCities, SIGNAL(stateChanged(int)), m_mapper, SLOT(map()) );
    connect( ui_provider->url, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_provider->shortUrl, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_provider->minFetchWait, SIGNAL(valueChanged(int)), m_mapper, SLOT(map()) );
    connect( ui_provider->scriptFile, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_provider->author, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_provider->shortAuthor, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_provider->email, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_provider->defaultVehicleType, SIGNAL(currentIndexChanged(int)), m_mapper, SLOT(map()) );
    connect( ui_provider->fileVersion, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_provider->predefinedCities, SIGNAL(changed()), m_mapper, SLOT(map()) );
    connect( ui_provider->sampleStopNames, SIGNAL(changed()), m_mapper, SLOT(map()) );
    connect( ui_provider->sampleCity, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( m_changelog, SIGNAL(added(QWidget*)), m_mapper, SLOT(map()) );
    connect( m_changelog, SIGNAL(removed(QWidget*,int)), m_mapper, SLOT(map()) );
    connect( m_changelog, SIGNAL(changed()), m_mapper, SLOT(map()) );
    // TODO Map changes in the changelog, ie. changing the version or message text
    m_mapper->setMapping( ui_provider->name, ui_provider->name );
    m_mapper->setMapping( ui_provider->description, ui_provider->description );
    m_mapper->setMapping( ui_provider->version, ui_provider->version );
    m_mapper->setMapping( ui_provider->type, ui_provider->type );
    m_mapper->setMapping( ui_provider->useCityValue, ui_provider->useCityValue );
    m_mapper->setMapping( ui_provider->onlyAllowPredefinedCities, ui_provider->onlyAllowPredefinedCities );
    m_mapper->setMapping( ui_provider->url, ui_provider->url );
    m_mapper->setMapping( ui_provider->shortUrl, ui_provider->shortUrl );
    m_mapper->setMapping( ui_provider->minFetchWait, ui_provider->minFetchWait );
    m_mapper->setMapping( ui_provider->scriptFile, ui_provider->scriptFile );
    m_mapper->setMapping( ui_provider->author, ui_provider->author );
    m_mapper->setMapping( ui_provider->shortAuthor, ui_provider->shortAuthor );
    m_mapper->setMapping( ui_provider->email, ui_provider->email );
    m_mapper->setMapping( ui_provider->defaultVehicleType, ui_provider->defaultVehicleType );
    m_mapper->setMapping( ui_provider->fileVersion, ui_provider->fileVersion );
    m_mapper->setMapping( ui_provider->predefinedCities, ui_provider->predefinedCities );
    m_mapper->setMapping( ui_provider->sampleStopNames, ui_provider->sampleStopNames );
    m_mapper->setMapping( ui_provider->sampleCity, ui_provider->sampleCity );
    m_mapper->setMapping( m_changelog, m_changelog );
    connect( m_mapper, SIGNAL(mapped(QWidget*)), this, SLOT(slotChanged(QWidget*)) );
}

ProjectSettingsDialog::~ProjectSettingsDialog()
{
    delete ui_provider;
}

void ProjectSettingsDialog::slotChanged( QWidget *changedWidget )
{
    if( changedWidget == ui_provider->scriptFile ) {
        // Script file changed
        const QString fileName = ui_provider->scriptFile->text();
        ui_provider->btnCreateScriptFile->setVisible( fileName.isEmpty() );
        ui_provider->btnDetachScriptFile->setVisible( !fileName.isEmpty() );
        emit scriptFileChanged( fileName );
    } else if( changedWidget == ui_provider->url ) {
        // Home page URL changed
        ui_provider->btnUrlOpen->setDisabled( ui_provider->url->text().isEmpty() );
    } else if( changedWidget == ui_provider->shortAuthor ) {
        // Short author name changed, update changed log click messages
        QList<ChangelogEntryWidget *> entryWidgets = m_changelog->entryWidgets();
        foreach( const ChangelogEntryWidget * entryWidget, entryWidgets ) {
            entryWidget->authorLineEdit()->setClickMessage( m_providerData->shortAuthor() );
        }
    }

    fillValuesFromWidgets();
    emit changed();
}

bool ProjectSettingsDialog::check()
{
    bool result = testWidget( ui_provider->email );
    result = testWidget( ui_provider->version ) && result;
    result = testWidget( ui_provider->fileVersion ) && result;
    result = testWidget( ui_provider->name ) && result;
    result = testWidget( ui_provider->description ) && result;
    result = testWidget( ui_provider->author ) && result;
    result = testWidget( ui_provider->shortAuthor ) && result;
    result = testWidget( ui_provider->url ) && result;
    result = testWidget( ui_provider->shortUrl ) && result;
    return result;
}

void ProjectSettingsDialog::slotButtonClicked( int button )
{
    if ( button == KDialog::Ok ) {
        fillValuesFromWidgets();
        accept();
    } else if ( button == KDialog::User1 ) {
        if ( check() ) {
            KMessageWidget *messageWidget = new KMessageWidget(
                    i18nc("@info", "All settings are valid"), this );
            messageWidget->setMessageType( KMessageWidget::Positive );
            mainWidget()->layout()->addWidget( messageWidget );
            messageWidget->animatedShow();

            // Install an event filter to delete the message widget after the widget was hidden
            messageWidget->installEventFilter( this );

            // Hide after 4 seconds
            QTimer::singleShot( 4000, messageWidget, SLOT(animatedHide()) );
        }
    } else {
        KDialog::slotButtonClicked( button );
    }
}

void ProjectSettingsDialog::accept()
{
//     if ( check() ) {
        QDialog::accept();
//     }
}

bool ProjectSettingsDialog::eventFilter( QObject *object, QEvent *event )
{
    QWidget *widget = qobject_cast< QWidget* >( object );
    if ( widget && event->type() == QEvent::FocusOut ) {
        testWidget( widget );
    }
//     KLineEdit *lineEdit = qobject_cast< KLineEdit* >( object );
//     if ( lineEdit && lineEdit->validator() ) {
//         if ( event->type() == QEvent::FocusOut ) {
// //             QString errorMessage;
// //             if ( lineEdit == ui_provider->email ) {
// //                 errorMessage = i18nc("@info", "Invalid e-mail address");
// //             } else if ( lineEdit == ui_provider->version || lineEdit == ui_provider->fileVersion ) {
// //                 errorMessage = i18nc("@info", "Invalid version");
// //             }
// //
// //             const bool validated = errorMessage.isEmpty() ||
// //                     testLineEditValidator( lineEdit, errorMessage );
// //             kDebug() << "Validated?" << validated << lineEdit << lineEdit->text();
// //             if ( validated && lineEdit == ui_provider->fileVersion &&
// //                  lineEdit->text() != QLatin1String("1.0") )
// //             {
// //                 // TODO Global error message class for ServiceProviderData?
// //                 appendMessageWidgetAfter( lineEdit, i18nc("@info",
// //                         "<title>The PublicTransport data engine currently only supports version '1.0'</title>"
// //                         "<para>Specify version '1.0' as <interface>File Type Version</interface> "
// //                         "in the project settings.</para>") );
// //             }
//         }
//     } else if ( lineEdit && lineEdit->text().isEmpty() ) {
//         if ( event->type() == QEvent::FocusOut ) {
//             appendMessageWidgetAfter( lineEdit, i18nc("@info", "Cannot be empty") );
//         }
//     }

    KMessageWidget *messageWidget = qobject_cast< KMessageWidget*> ( object );
    if ( messageWidget && event->type() == QEvent::Hide ) {
        // Delete message widgets after they are hidden
        messageWidget->deleteLater();
    }

    return QDialog::eventFilter( object, event );
}

TestModel::Test testFromWidget( Ui::timetablemateview_base *ui, QWidget *widget )
{
    if ( widget == ui->email ) {
        return TestModel::ServiceProviderDataEmailTest;
    } else if ( widget == ui->name ) {
        return TestModel::ServiceProviderDataNameTest;
    } else if ( widget == ui->version ) {
        return TestModel::ServiceProviderDataVersionTest;
    } else if ( widget == ui->fileVersion ) {
        return TestModel::ServiceProviderDataFileFormatVersionTest;
    } else if ( widget == ui->author ) {
        return TestModel::ServiceProviderDataAuthorNameTest;
    } else if ( widget == ui->shortAuthor ) {
        return TestModel::ServiceProviderDataShortAuthorNameTest;
    } else if ( widget == ui->url ) {
        return TestModel::ServiceProviderDataUrlTest;
    } else if ( widget == ui->shortUrl ) {
        return TestModel::ServiceProviderDataShortUrlTest;
    } else if ( widget == ui->scriptFile ) {
        return TestModel::ServiceProviderDataScriptFileNameTest;
    } else if ( widget == ui->description ) {
        return TestModel::ServiceProviderDataDescriptionTest;
    } else {
        kWarning() << "Unknown widget";
        return TestModel::InvalidTest;
    }
}

bool ProjectSettingsDialog::testWidget( QWidget *widget )
{
    TestModel::Test test = testFromWidget( ui_provider, widget );
    if ( test == TestModel::InvalidTest ) {
        // Unknown widget
        return true;
    }

    QString text;
    KLineEdit *lineEdit = qobject_cast< KLineEdit* >( widget );
    if ( lineEdit ) {
        text = lineEdit->text();
    } else {
        KRichTextWidget *richText = qobject_cast< KRichTextWidget* >( widget );
        if ( richText ) {
            text = richText->textOrHtml();
        } else {
            kWarning() << "Unknown widget type";
            return false;
        }
    }

    QString errorMessage;
    if ( !ServiceProviderDataTester::runServiceProviderDataTest(test, text, &errorMessage) ) {
        appendMessageWidgetAfter( lineEdit, errorMessage );
        return false;
    }

    return true;
}

void ProjectSettingsDialog::appendMessageWidgetAfter( QWidget *after, const QString &errorMessage )
{
    QFormLayout *formLayout = qobject_cast< QFormLayout* >( after->parentWidget()->layout() );
    Q_ASSERT( formLayout );

    // Get the position of the QWidget after which the message widget should be inserted
    int row;
    QFormLayout::ItemRole role;
    formLayout->getWidgetPosition( after, &row, &role );

    // Check if there already is a KMessageWidget
    QLayoutItem *item = row == -1 ? 0 : formLayout->itemAt(row + 1, QFormLayout::FieldRole);
    KMessageWidget *messageWidget = !item ? 0 : qobject_cast< KMessageWidget* >( item->widget() );

    // Check if a message was already shown
    if ( messageWidget ) {
        // Found an existing KMessageWidget after the widget,
        // update it instead of creating a new one
        messageWidget->setText( errorMessage );
    } else {
        // Create a message widget showing where the error is
        messageWidget = new KMessageWidget( errorMessage, after->parentWidget() );
        messageWidget->setMessageType( KMessageWidget::Error );

        // Insert the message widget after the widget with the erroneous content
        formLayout->insertRow( row + 1, messageWidget );
        messageWidget->animatedShow();

        // Install an event filter to delete the message widget after the widget was hidden
        messageWidget->installEventFilter( this );

        // Hide after 4 seconds
        QTimer::singleShot( 4000, messageWidget, SLOT(animatedHide()) );
    }
}

void ProjectSettingsDialog::fillValuesFromWidgets()
{
//     if ( !m_providerData ) {
//         kDebug() << "No provider loaded to fill with values";
//         return;
//     }

    // Fill struct with current values of the widgets
    QString lang = ui_provider->currentLanguage->current();
    if( lang == QLatin1String("en_US") ) {
        lang = "en";
    }

    QHash< QString, QString > names = m_providerData->names();
    QHash< QString, QString > descriptions = m_providerData->descriptions();
    names[lang] = ui_provider->name->text();
    descriptions[lang] = ui_provider->description->toPlainText();
    const VehicleType defaultVehicleType = Global::vehicleTypeFromString(
            ui_provider->defaultVehicleType->itemData(
            ui_provider->defaultVehicleType->currentIndex()).toString() );

    QStringList cities;
    QHash< QString, QString > cityNameReplacements;
    const QStringList cityReplacements = ui_provider->predefinedCities->items();
    foreach( const QString & cityReplacement, cityReplacements ) {
        QStringList values = cityReplacement.split( "   ->   " );
        if( values.count() == 2 ) {
            cities << values.at( 0 );
            cityNameReplacements.insert( values.at(0).toLower(), values.at(1) );
        } else {
            cities << cityReplacement;
        }
    }

    // Update values that can be edited in this dialog
    m_providerData->setType( static_cast<ServiceProviderType>(ui_provider->type->currentIndex() + 1) );
    m_providerData->setNames( names );
    m_providerData->setDescriptions( descriptions );
    m_providerData->setVersion( ui_provider->version->text() );
    m_providerData->setFileFormatVersion( ui_provider->fileVersion->text() );
    m_providerData->setUseSeparateCityValue( ui_provider->useCityValue->isChecked() );
    m_providerData->setOnlyUseCitiesInList( ui_provider->onlyAllowPredefinedCities->isChecked() );
    m_providerData->setUrl( ui_provider->url->text(), ui_provider->shortUrl->text() );
    m_providerData->setMinFetchWait( ui_provider->minFetchWait->value() );
    m_providerData->setAuthor( ui_provider->author->text(), ui_provider->shortAuthor->text(),
                               ui_provider->email->text() );
    m_providerData->setDefaultVehicleType( defaultVehicleType );
    m_providerData->setChangelog( m_changelog->changelog() );
    m_providerData->setCities( cities );
    m_providerData->setCityNameToValueReplacementHash( cityNameReplacements );
    m_providerData->setSampleCity( ui_provider->sampleCity->text() );
    m_providerData->setSampleStops( ui_provider->sampleStopNames->items() );
    m_providerData->setNotes( ui_provider->notes->textOrHtml() );
}

void ProjectSettingsDialog::changelogEntryWidgetAdded( ChangelogEntryWidget *entryWidget )
{
    const int comparison = ServiceProviderData::compareVersions(
            entryWidget->version(), ui_provider->version->text());
    if ( comparison > 0 ) {
        int result = KMessageBox::questionYesNo( this,
                i18nc("@info", "The new changelog entry references a newer version than the "
                      "current project version. Do you want to update the project version to %1?",
                      entryWidget->version()) );
        if ( result == KMessageBox::Yes ) {
            // Yes clicked, update version value
            ui_provider->version->setText( entryWidget->version() );
        }
    }
}

void ProjectSettingsDialog::authorEdited( const QString &newAuthor )
{
    if ( ui_provider->shortAuthor->text().isEmpty() || m_shortAuthorAutoFilled ) {
        // Update short author value if it is empty
        m_shortAuthorAutoFilled = true; // Set to auto filled
        ui_provider->shortAuthor->setText( ServiceProviderData::shortAuthorFromAuthor(newAuthor) );
    }
}

void ProjectSettingsDialog::urlEdited( const QString &newUrl )
{
    if ( ui_provider->shortUrl->text().isEmpty() || m_shortUrlAutoFilled ) {
        // Update short URL value if it is empty
        m_shortUrlAutoFilled = true; // Set to auto filled
        ui_provider->shortUrl->setText( ServiceProviderData::shortUrlFromUrl(newUrl) );
    }
}

void ProjectSettingsDialog::shortAuthorEdited( const QString &shortAuthor )
{
    Q_UNUSED( shortAuthor );
    m_shortAuthorAutoFilled = false;
}

void ProjectSettingsDialog::shortUrlEdited( const QString &shortUrl )
{
    Q_UNUSED( shortUrl );
    m_shortUrlAutoFilled = false;
}

void ProjectSettingsDialog::currentPredefinedCityChanged( const QString &currentCityText )
{
    QStringList values = currentCityText.split( "   ->   " );
    if( values.count() == 2 ) {
        m_cityName->blockSignals( true );
        m_cityReplacement->blockSignals( true );

        m_cityName->setText( values.at( 0 ) );
        m_cityReplacement->setText( values.at( 1 ) );

        m_cityName->blockSignals( false );
        m_cityReplacement->blockSignals( false );
    } else {
        m_cityName->blockSignals( true );
        m_cityReplacement->blockSignals( true );

        m_cityName->setText( currentCityText );
        m_cityReplacement->setText( QString() );

        m_cityName->blockSignals( false );
        m_cityReplacement->blockSignals( false );
    }
}

void ProjectSettingsDialog::predefinedCityNameChanged( const QString &newCityName )
{
    QString text = newCityName;
    if( !m_cityReplacement->text().isEmpty() ) {
        text += "   ->   " + m_cityReplacement->text();
    }

    m_predefinedCitiesCustomEditor.lineEdit()->setText( text );
}

void ProjectSettingsDialog::predefinedCityReplacementChanged( const QString &newReplacement )
{
    QString text = m_cityName->text();
    if( !newReplacement.isEmpty() ) {
        text += "   ->   " + newReplacement;
    }

    m_predefinedCitiesCustomEditor.lineEdit()->setText( text );
}

void ProjectSettingsDialog::languageActivated( const QString &languageCode )
{
    QString code = languageCode == QLatin1String("en_US") ? "en" : languageCode;

    ui_provider->name->blockSignals( true );
    ui_provider->name->setText( m_providerData->names()[code] );
    ui_provider->name->blockSignals( false );

    ui_provider->description->blockSignals( true );
    ui_provider->description->setText( m_providerData->descriptions()[code] );
    ui_provider->description->blockSignals( false );
}

void ProjectSettingsDialog::openUrlClicked()
{
    emit urlShouldBeOpened( ui_provider->url->text() );
}

void ProjectSettingsDialog::createScriptFile()
{
    if( m_openedPath.isEmpty() ) {
        KMessageBox::information( this, i18nc("@info/plain", "Please save the XML file first. "
                                              "The script file needs to be in the same folder.") );
        return;
    }

    // Get a name for the new script file based on the current country code
    // and the current service provider ID
    QString scriptFile = m_currentServiceProviderID + ".js";

    // Get fileName for the new script file
    QString scriptFilePath = KUrl( m_openedPath ).directory( KUrl::AppendTrailingSlash ) + scriptFile;

    // Check if the file already exists
    QFile file( scriptFilePath );
    if( file.exists() ) {
        int result = KMessageBox::questionYesNoCancel( this,
                     i18nc("@info/plain", "The script file <filename>%1</filename> already exists.<nl/>"
                           "Do you want to overwrite it or open and use it as script file?", scriptFile),
                     i18nc("@title:window", "File Already Exists"),
                     KStandardGuiItem::overwrite(), KStandardGuiItem::open() );
        if( result == KMessageBox::No ) {  // open
            ui_provider->scriptFile->setText( scriptFile );
            return;
        } else if( result == KMessageBox::Cancel ) {
            return;
        }
    }

    // Create the file
    if( !file.open( QIODevice::WriteOnly ) ) {
        KMessageBox::information( this, i18nc("@info/plain", "A new script file with the name "
                                              "<filename>%1</filename> could not be created.",
                                              scriptFilePath) );
        return;
    }
    file.close();

    const QString scriptType = KInputDialog::getItem(
            i18nc("@title:window", "Choose Script Type"), i18nc("@info", "Script Type"),
            QStringList() << "JavaScript" << "Ruby" << "Python", 0, false, 0, this );
    if( scriptType == QLatin1String("JavaScript") ) {
        m_newScriptTemplateType = Project::ScriptQtScriptTemplate;
    } else if( scriptType == QLatin1String("Ruby") ) {
        m_newScriptTemplateType = Project::ScriptRubyTemplate;
    } else if( scriptType == QLatin1String("Python") ) {
        m_newScriptTemplateType = Project::ScriptPythonTemplate;
    } else {
        kWarning() << "Unexpected script type" << scriptType;
        return;
    }

    ui_provider->scriptFile->setText( scriptFile );
    emit scriptAdded( scriptFilePath );
}

void ProjectSettingsDialog::detachScriptFile()
{
    ui_provider->scriptFile->setText( QString() );
    m_newScriptTemplateType = Project::NoScriptTemplate;
}

void ProjectSettingsDialog::browseForScriptFile()
{
    if( m_openedPath.isEmpty() ) {
        KMessageBox::information( this, i18nc( "@info/plain", "Please save the "
                                               "XML file first. The script file needs to be in the same folder." ) );
        return;
    }

    KUrl openedUrl( m_openedPath );

    // Get a list of all script files in the directory of the XML file
    QStringList scriptFiles;
    int current = -1;
    QDir dir( openedUrl.directory() );
    QStringList fileNames = dir.entryList();
    for( int i = 0; i < fileNames.count(); ++i ) {
        QString fileName = fileNames.at( i );
        KMimeType::Ptr mimeType = KMimeType::findByUrl( KUrl( fileName ) );
        if( mimeType->is("application/javascript") ||
            mimeType->is("application/x-ruby") ||
            mimeType->is("text/x-python") )
        {
            scriptFiles << fileName;
            if( fileName == ui_provider->scriptFile->text() ) {
                current = i;
            }
        }
    }

    bool ok;
    QString selectedFile = KInputDialog::getItem( i18nc( "@title:window", "Choose Script File" ),
                           i18nc( "@info", "Script File for Parsing Documents" ),
                           scriptFiles, current, false, &ok, this );
    if( ok ) {
        ui_provider->scriptFile->setText( selectedFile );
    }
}

void ProjectSettingsDialog::setScriptFile( const QString &scriptFile )
{
    ui_provider->scriptFile->setText( scriptFile );
}

const ServiceProviderData *ProjectSettingsDialog::providerData( QObject *parent ) const
{
    m_providerData->setParent( parent );
    return m_providerData;
}

void ProjectSettingsDialog::setProviderData( const ServiceProviderData *info,
                                             const QString &fileName )
{
    // Disable changed signals from widgets while setting the read values
    m_mapper->blockSignals( true );

    m_shortAuthorAutoFilled = false;
    m_shortUrlAutoFilled = false;

    m_providerData = info->clone( info->parent() );
    m_openedPath = fileName;
    ui_provider->savePath->setText( fileName );
//     ui_provider->savePath->setToolTip( Project::savePathInfoStringFromFilePath(fileName) );
    ui_provider->currentLanguage->setCurrentItem( "en" );
    ui_provider->name->setText( info->names()["en"] );
    ui_provider->description->setText( info->descriptions()["en"] );
    ui_provider->version->setText( info->version() );
    ui_provider->type->setCurrentIndex( static_cast<int>(info->type()) - 1 );
    ui_provider->useCityValue->setChecked( info->useSeparateCityValue() );
    ui_provider->onlyAllowPredefinedCities->setChecked( info->onlyUseCitiesInList() );
    ui_provider->url->setText( info->url() );
    ui_provider->shortUrl->setText( info->shortUrl() );
    ui_provider->minFetchWait->setValue( info->minFetchWait() );
    ui_provider->scriptFile->setText( info->scriptFileName() );
    ui_provider->author->setText( info->author() );
    ui_provider->shortAuthor->setText( info->shortAuthor() );
    ui_provider->email->setText( info->email() );
    int defaultVehicleTypeIndex =
            ui_provider->defaultVehicleType->findData( info->defaultVehicleType() );
    ui_provider->defaultVehicleType->setCurrentIndex(
            defaultVehicleTypeIndex > 0 ? defaultVehicleTypeIndex : 0 );
    ui_provider->fileVersion->setText( info->fileFormatVersion() );
    m_changelog->clear();
    m_changelog->addChangelog( info->changelog(), info->shortAuthor() );

    ui_provider->predefinedCities->clear();
    foreach( const QString & city, info->cities() ) {
        QString lowerCity = city.toLower();
        if( info->cityNameToValueReplacementHash().contains( lowerCity ) ) {
            ui_provider->predefinedCities->insertItem( city + "   ->   " +
                    info->cityNameToValueReplacementHash()[lowerCity] );
        } else {
            ui_provider->predefinedCities->insertItem( city );
        }
    }

    ui_provider->sampleStopNames->setItems( info->sampleStopNames() );
    ui_provider->sampleCity->setText( info->sampleCity() );

    ui_provider->notes->setText( info->notes() );

    // Enable changed signals from widgets again and emit changed signals once
    m_mapper->blockSignals( false );
    emit changed();
    emit scriptFileChanged( fileName );
}

void ProjectSettingsDialog::settingsChanged()
{
    emit signalChangeStatusbar( i18n("Settings changed") );
}

#include "projectsettingsdialog.moc"
// kate: indent-mode cstyle; indent-width 4; replace-tabs on;
