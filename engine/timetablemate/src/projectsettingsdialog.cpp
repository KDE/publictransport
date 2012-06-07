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
#include "accessorinfoxmlwriter.h"
#include "accessorinfotester.h"

// PublicTransport engine includes
#include <engine/accessorinfoxmlreader.h>
#include <engine/timetableaccessor.h>
#include <engine/timetableaccessor_info.h>
#include <engine/global.h>

// KDE includes
#include <KFileDialog>
#include <KMessageBox>
#include <KInputDialog>
#include <KColorScheme>
#include <KLocale>
#include <KMessageWidget>
#include <KLineEdit>

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

ProjectSettingsDialog::ProjectSettingsDialog( QWidget *parent )
        : KDialog(parent), ui_accessor(new Ui::timetablemateview_base()), m_accessorInfo(0),
          m_newScriptTemplateType(Project::NoScriptTemplate),
          m_shortAuthorAutoFilled(false), m_shortUrlAutoFilled(false),
          m_cityName(0), m_cityReplacement(0), m_changelog(0), m_mapper(0)
{
    QWidget *widget = new QWidget( this );
    widget->setAutoFillBackground( false );
    ui_accessor->setupUi( widget );
    setMainWidget( widget );
    setCaption( i18nc("@title:window", "Project Settings") );
    setButtons( KDialog::Ok | KDialog::Cancel | KDialog::User1 );
    setButtonIcon( KDialog::User1, KIcon("dialog-ok-apply") );
    setButtonText( KDialog::User1, i18nc("@info/plain", "Check") );

    settingsChanged();

    // Initialize script file buttons
    ui_accessor->btnBrowseForScriptFile->setIcon( KIcon("document-open") );
    ui_accessor->btnCreateScriptFile->setIcon( KIcon("document-new") );
    ui_accessor->btnDetachScriptFile->setIcon( KIcon("list-remove") );
    ui_accessor->btnDetachScriptFile->setVisible( false );
    connect( ui_accessor->btnBrowseForScriptFile, SIGNAL(clicked(bool)),
             this, SLOT(browseForScriptFile()) );
    connect( ui_accessor->btnCreateScriptFile, SIGNAL(clicked(bool)),
             this, SLOT(createScriptFile()) );
    connect( ui_accessor->btnDetachScriptFile, SIGNAL(clicked(bool)),
             this, SLOT(detachScriptFile()) );

    // Initialize the language button
    ui_accessor->currentLanguage->loadAllLanguages();
    ui_accessor->currentLanguage->insertLanguage( "en", QString(), 0 );
    ui_accessor->currentLanguage->insertSeparator( 1 );
    connect( ui_accessor->currentLanguage, SIGNAL(activated(QString)),
             this, SLOT(languageActivated(QString)) );

    // Autofill short author/URL fields, if they are empty
    // while editing the full author/URL fields
    connect( ui_accessor->author, SIGNAL(textEdited(QString)),
             this, SLOT(authorEdited(QString)) );
    connect( ui_accessor->shortAuthor, SIGNAL(textEdited(QString)),
             this, SLOT(shortAuthorEdited(QString)) );
    connect( ui_accessor->url, SIGNAL(textEdited(QString)),
             this, SLOT(urlEdited(QString)) );
    connect( ui_accessor->shortUrl, SIGNAL(textEdited(QString)),
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
    ui_accessor->predefinedCities->setCustomEditor( m_predefinedCitiesCustomEditor );
    connect( m_cityName, SIGNAL(textChanged(QString)),
             this, SLOT(predefinedCityNameChanged(QString)) );
    connect( m_cityReplacement, SIGNAL(textChanged(QString)),
             this, SLOT(predefinedCityReplacementChanged(QString)) );
    connect( defaultLineEdit, SIGNAL(textChanged(QString)),
             this, SLOT(currentPredefinedCityChanged(QString)) );

    // Set a validator for version line edits, allow major, minor and path version
    QRegExpValidator *versionValidator =
            new QRegExpValidator( QRegExp("\\d+(\\.\\d+)?(\\.\\d+)?"), this );
    ui_accessor->version->setValidator( versionValidator );
    ui_accessor->fileVersion->setValidator( versionValidator );

    // Set a validator for the email line edit
    // The reg exp is "inspired" by http://www.regular-expressions.info/email.html
    QRegExp rx( "[a-z0-9!#$%&\\._-]+@(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.)+[a-z]{2,4}",
                Qt::CaseInsensitive );
    QRegExpValidator *emailValidator = new QRegExpValidator( rx, this );
    ui_accessor->email->setValidator( emailValidator );

    // Install event filters to filter out focus out events
    // if the line edit's text cannot be validated
    ui_accessor->version->installEventFilter( this );
    ui_accessor->fileVersion->installEventFilter( this );
    ui_accessor->name->installEventFilter( this );
    ui_accessor->description->installEventFilter( this );
    ui_accessor->author->installEventFilter( this );
    ui_accessor->shortAuthor->installEventFilter( this );
    ui_accessor->email->installEventFilter( this );
    ui_accessor->url->installEventFilter( this );
    ui_accessor->shortUrl->installEventFilter( this );

    // Set icons and connections for the "open url buttons"
    ui_accessor->btnUrlOpen->setIcon( KIcon("document-open-remote") );
    connect( ui_accessor->btnUrlOpen, SIGNAL(clicked(bool)),
             this, SLOT(openUrlClicked()) );

    // Add changelog widget into a scroll area
    QVBoxLayout *changelogAreaLayout = new QVBoxLayout( ui_accessor->tabChangelog );
    QScrollArea *changelogArea = new QScrollArea( ui_accessor->tabChangelog );
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
    ui_accessor->defaultVehicleType->addItem(
        KIcon( "status_unknown" ), i18nc( "@item:listbox", "Unknown" ), "Unknown" );
    ui_accessor->defaultVehicleType->addItem(
        KIcon( "vehicle_type_tram" ), i18nc( "@item:listbox", "Tram" ), "Tram" );
    ui_accessor->defaultVehicleType->addItem(
        KIcon( "vehicle_type_bus" ), i18nc( "@item:listbox", "Bus" ), "Bus" );
    ui_accessor->defaultVehicleType->addItem(
        KIcon( "vehicle_type_subway" ), i18nc( "@item:listbox", "Subway" ), "Subway" );
    ui_accessor->defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_interurban" ),
        i18nc( "@item:listbox", "Interurban Train" ), "TrainInterurban" );
    ui_accessor->defaultVehicleType->addItem(
        KIcon( "vehicle_type_metro" ), i18nc( "@item:listbox", "Metro" ), "Metro" );
    ui_accessor->defaultVehicleType->addItem(
        KIcon( "vehicle_type_trolleybus" ),
        i18nc( "@item:listbox", "Trolley Bus" ), "TrolleyBus" );
    ui_accessor->defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_regional" ), // TODO: Currently no special icon
        i18nc( "@item:listbox", "Regional Train" ), "TrainRegional" );
    ui_accessor->defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_regional" ),
        i18nc( "@item:listbox", "Regional Express Train" ), "TrainRegionalExpress" );
    ui_accessor->defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_interregional" ),
        i18nc( "@item:listbox", "Interregional Train" ), "TrainInterregio" );
    ui_accessor->defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_intercity" ),
        i18nc( "@item:listbox", "Intercity/Eurocity Train" ), "TrainIntercityEurocity" );
    ui_accessor->defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_highspeed" ),
        i18nc( "@item:listbox", "Intercity Express Train" ), "TrainIntercityExpress" );
    ui_accessor->defaultVehicleType->addItem(
        KIcon( "vehicle_type_ferry" ), i18nc( "@item:listbox", "Ferry" ), "Ferry" );
    ui_accessor->defaultVehicleType->addItem(
        KIcon( "vehicle_type_ferry" ), i18nc( "@item:listbox", "Ship" ), "Ship" );
    ui_accessor->defaultVehicleType->addItem(
        KIcon( "vehicle_type_plane" ), i18nc( "@item:listbox", "Plane" ), "Plane" );

    // Connect all change signals of the widgets to the changed() signal
    m_mapper = new QSignalMapper( this );
    connect( ui_accessor->name, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor->description, SIGNAL(textChanged()), m_mapper, SLOT(map()) );
    connect( ui_accessor->version, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor->type, SIGNAL(currentIndexChanged(int)), m_mapper, SLOT(map()) );
    connect( ui_accessor->useCityValue, SIGNAL(stateChanged(int)), m_mapper, SLOT(map()) );
    connect( ui_accessor->onlyAllowPredefinedCities, SIGNAL(stateChanged(int)), m_mapper, SLOT(map()) );
    connect( ui_accessor->url, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor->shortUrl, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor->minFetchWait, SIGNAL(valueChanged(int)), m_mapper, SLOT(map()) );
    connect( ui_accessor->scriptFile, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor->author, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor->shortAuthor, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor->email, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor->defaultVehicleType, SIGNAL(currentIndexChanged(int)), m_mapper, SLOT(map()) );
    connect( ui_accessor->fileVersion, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor->predefinedCities, SIGNAL(changed()), m_mapper, SLOT(map()) );
    connect( ui_accessor->sampleStopNames, SIGNAL(changed()), m_mapper, SLOT(map()) );
    connect( ui_accessor->sampleCity, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( m_changelog, SIGNAL(added(QWidget*)), m_mapper, SLOT(map()) );
    connect( m_changelog, SIGNAL(removed(QWidget*,int)), m_mapper, SLOT(map()) );
    connect( m_changelog, SIGNAL(changed()), m_mapper, SLOT(map()) );
    // TODO Map changes in the changelog, ie. changing the version or message text
    m_mapper->setMapping( ui_accessor->name, ui_accessor->name );
    m_mapper->setMapping( ui_accessor->description, ui_accessor->description );
    m_mapper->setMapping( ui_accessor->version, ui_accessor->version );
    m_mapper->setMapping( ui_accessor->type, ui_accessor->type );
    m_mapper->setMapping( ui_accessor->useCityValue, ui_accessor->useCityValue );
    m_mapper->setMapping( ui_accessor->onlyAllowPredefinedCities, ui_accessor->onlyAllowPredefinedCities );
    m_mapper->setMapping( ui_accessor->url, ui_accessor->url );
    m_mapper->setMapping( ui_accessor->shortUrl, ui_accessor->shortUrl );
    m_mapper->setMapping( ui_accessor->minFetchWait, ui_accessor->minFetchWait );
    m_mapper->setMapping( ui_accessor->scriptFile, ui_accessor->scriptFile );
    m_mapper->setMapping( ui_accessor->author, ui_accessor->author );
    m_mapper->setMapping( ui_accessor->shortAuthor, ui_accessor->shortAuthor );
    m_mapper->setMapping( ui_accessor->email, ui_accessor->email );
    m_mapper->setMapping( ui_accessor->defaultVehicleType, ui_accessor->defaultVehicleType );
    m_mapper->setMapping( ui_accessor->fileVersion, ui_accessor->fileVersion );
    m_mapper->setMapping( ui_accessor->predefinedCities, ui_accessor->predefinedCities );
    m_mapper->setMapping( ui_accessor->sampleStopNames, ui_accessor->sampleStopNames );
    m_mapper->setMapping( ui_accessor->sampleCity, ui_accessor->sampleCity );
    m_mapper->setMapping( m_changelog, m_changelog );
    connect( m_mapper, SIGNAL(mapped(QWidget*)), this, SLOT(slotChanged(QWidget*)) );
}

ProjectSettingsDialog::~ProjectSettingsDialog()
{
    delete ui_accessor;
}

void ProjectSettingsDialog::slotChanged( QWidget *changedWidget )
{
    if( changedWidget == ui_accessor->scriptFile ) {
        // Script file changed
        const QString fileName = ui_accessor->scriptFile->text();
        ui_accessor->btnCreateScriptFile->setVisible( fileName.isEmpty() );
        ui_accessor->btnDetachScriptFile->setVisible( !fileName.isEmpty() );
        emit scriptFileChanged( fileName );
    } else if( changedWidget == ui_accessor->url ) {
        // Home page URL changed
        ui_accessor->btnUrlOpen->setDisabled( ui_accessor->url->text().isEmpty() );
    } else if( changedWidget == ui_accessor->shortAuthor ) {
        // Short author name changed, update changed log click messages
        QList<ChangelogEntryWidget *> entryWidgets = m_changelog->entryWidgets();
        foreach( const ChangelogEntryWidget * entryWidget, entryWidgets ) {
            entryWidget->authorLineEdit()->setClickMessage( m_accessorInfo->shortAuthor() );
        }
    }

    fillValuesFromWidgets();
    emit changed();
}

bool ProjectSettingsDialog::check()
{
    bool result = testWidget( ui_accessor->email );
    result = testWidget( ui_accessor->version ) && result;
    result = testWidget( ui_accessor->fileVersion ) && result;
    result = testWidget( ui_accessor->name ) && result;
    result = testWidget( ui_accessor->description ) && result;
    result = testWidget( ui_accessor->author ) && result;
    result = testWidget( ui_accessor->shortAuthor ) && result;
    result = testWidget( ui_accessor->url ) && result;
    result = testWidget( ui_accessor->shortUrl ) && result;
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
// //             if ( lineEdit == ui_accessor->email ) {
// //                 errorMessage = i18nc("@info", "Invalid e-mail address");
// //             } else if ( lineEdit == ui_accessor->version || lineEdit == ui_accessor->fileVersion ) {
// //                 errorMessage = i18nc("@info", "Invalid version");
// //             }
// //
// //             const bool validated = errorMessage.isEmpty() ||
// //                     testLineEditValidator( lineEdit, errorMessage );
// //             kDebug() << "Validated?" << validated << lineEdit << lineEdit->text();
// //             if ( validated && lineEdit == ui_accessor->fileVersion &&
// //                  lineEdit->text() != QLatin1String("1.0") )
// //             {
// //                 // TODO Global error message class for TimetableAccessorInfo?
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
        return TestModel::AccessorInfoEmailTest;
    } else if ( widget == ui->name ) {
        return TestModel::AccessorInfoNameTest;
    } else if ( widget == ui->version ) {
        return TestModel::AccessorInfoVersionTest;
    } else if ( widget == ui->fileVersion ) {
        return TestModel::AccessorInfoFileVersionTest;
    } else if ( widget == ui->author ) {
        return TestModel::AccessorInfoAuthorNameTest;
    } else if ( widget == ui->shortAuthor ) {
        return TestModel::AccessorInfoShortAuthorNameTest;
    } else if ( widget == ui->url ) {
        return TestModel::AccessorInfoUrlTest;
    } else if ( widget == ui->shortUrl ) {
        return TestModel::AccessorInfoShortUrlTest;
    } else if ( widget == ui->scriptFile ) {
        return TestModel::AccessorInfoScriptFileNameTest;
    } else if ( widget == ui->description ) {
        return TestModel::AccessorInfoDescriptionTest;
    } else {
        kWarning() << "Unknown widget";
        return TestModel::InvalidTest;
    }
}

bool ProjectSettingsDialog::testWidget( QWidget *widget )
{
    TestModel::Test test = testFromWidget( ui_accessor, widget );
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
    if ( !AccessorInfoTester::runAccessorInfoTest(test, text, &errorMessage) ) {
        appendMessageWidgetAfter( lineEdit, errorMessage );
        return false;
    }

    return true;
}

void ProjectSettingsDialog::appendMessageWidgetAfter( QWidget *after, const QString &errorMessage )
{
    QFormLayout *formLayout = qobject_cast< QFormLayout* >( after->parentWidget()->layout() );
        // ui_accessor->tabGeneral->layout() );
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
//     if ( !m_accessorInfo ) {
//         kDebug() << "No accessor loaded to fill with values";
//         return;
//     }

    // Fill struct with current values of the widgets
    QString lang = ui_accessor->currentLanguage->current();
    if( lang == QLatin1String("en_US") ) {
        lang = "en";
    }

    QHash< QString, QString > names = m_accessorInfo->names();
    QHash< QString, QString > descriptions = m_accessorInfo->descriptions();
    names[lang] = ui_accessor->name->text();
    descriptions[lang] = ui_accessor->description->toPlainText();
    const VehicleType defaultVehicleType = Global::vehicleTypeFromString(
            ui_accessor->defaultVehicleType->itemData(
            ui_accessor->defaultVehicleType->currentIndex()).toString() );

    QStringList cities;
    QHash< QString, QString > cityNameReplacements;
    const QStringList cityReplacements = ui_accessor->predefinedCities->items();
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
    m_accessorInfo->setType( static_cast<AccessorType>(ui_accessor->type->currentIndex() + 1) );
    m_accessorInfo->setNames( names );
    m_accessorInfo->setDescriptions( descriptions );
    m_accessorInfo->setVersion( ui_accessor->version->text() );
    m_accessorInfo->setFileVersion( ui_accessor->fileVersion->text() );
    m_accessorInfo->setUseSeparateCityValue( ui_accessor->useCityValue->isChecked() );
    m_accessorInfo->setOnlyUseCitiesInList( ui_accessor->onlyAllowPredefinedCities->isChecked() );
    m_accessorInfo->setUrl( ui_accessor->url->text(), ui_accessor->shortUrl->text() );
    m_accessorInfo->setMinFetchWait( ui_accessor->minFetchWait->value() );
    m_accessorInfo->setAuthor( ui_accessor->author->text(), ui_accessor->shortAuthor->text(),
                               ui_accessor->email->text() );
    m_accessorInfo->setDefaultVehicleType( defaultVehicleType );
    m_accessorInfo->setChangelog( m_changelog->changelog() );
    m_accessorInfo->setCities( cities );
    m_accessorInfo->setCityNameToValueReplacementHash( cityNameReplacements );
    m_accessorInfo->setSampleCity( ui_accessor->sampleCity->text() );
    m_accessorInfo->setSampleStops( ui_accessor->sampleStopNames->items() );
}

void ProjectSettingsDialog::changelogEntryWidgetAdded( ChangelogEntryWidget *entryWidget )
{
    const int comparison = TimetableAccessorInfo::compareVersions(
            entryWidget->version(), ui_accessor->version->text());
    if ( comparison > 0 ) {
        int result = KMessageBox::questionYesNo( this,
                i18nc("@info", "The new changelog entry references a newer version than the "
                      "current project version. Do you want to update the project version to %1?",
                      entryWidget->version()) );
        if ( result == KMessageBox::Yes ) {
            // Yes clicked, update version value
            ui_accessor->version->setText( entryWidget->version() );
        }
    }
}

void ProjectSettingsDialog::authorEdited( const QString &newAuthor )
{
    if ( ui_accessor->shortAuthor->text().isEmpty() || m_shortAuthorAutoFilled ) {
        // Update short author value if it is empty
        m_shortAuthorAutoFilled = true; // Set to auto filled
        ui_accessor->shortAuthor->setText( TimetableAccessorInfo::shortAuthorFromAuthor(newAuthor) );
    }
}

void ProjectSettingsDialog::urlEdited( const QString &newUrl )
{
    if ( ui_accessor->shortUrl->text().isEmpty() || m_shortUrlAutoFilled ) {
        // Update short URL value if it is empty
        m_shortUrlAutoFilled = true; // Set to auto filled
        ui_accessor->shortUrl->setText( TimetableAccessorInfo::shortUrlFromUrl(newUrl) );
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

    ui_accessor->name->blockSignals( true );
    ui_accessor->name->setText( m_accessorInfo->names()[code] );
    ui_accessor->name->blockSignals( false );

    ui_accessor->description->blockSignals( true );
    ui_accessor->description->setText( m_accessorInfo->descriptions()[code] );
    ui_accessor->description->blockSignals( false );
}

void ProjectSettingsDialog::openUrlClicked()
{
    emit urlShouldBeOpened( ui_accessor->url->text() );
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
            ui_accessor->scriptFile->setText( scriptFile );
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

    ui_accessor->scriptFile->setText( scriptFile );
    emit scriptAdded( scriptFilePath );
}

void ProjectSettingsDialog::detachScriptFile()
{
    ui_accessor->scriptFile->setText( QString() );
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
            if( fileName == ui_accessor->scriptFile->text() ) {
                current = i;
            }
        }
    }

    bool ok;
    QString selectedFile = KInputDialog::getItem( i18nc( "@title:window", "Choose Script File" ),
                           i18nc( "@info", "Script File for Parsing Documents" ),
                           scriptFiles, current, false, &ok, this );
    if( ok ) {
        ui_accessor->scriptFile->setText( selectedFile );
    }
}

void ProjectSettingsDialog::setScriptFile( const QString &scriptFile )
{
    ui_accessor->scriptFile->setText( scriptFile );
}

const TimetableAccessorInfo *ProjectSettingsDialog::accessorInfo( QObject *parent ) const
{
    m_accessorInfo->setParent( parent );
    return m_accessorInfo;
}

void ProjectSettingsDialog::setAccessorInfo( const TimetableAccessorInfo *info,
                                             const QString &fileName )
{
    // Disable changed signals from widgets while setting the read values
    m_mapper->blockSignals( true );

    m_shortAuthorAutoFilled = false;
    m_shortUrlAutoFilled = false;

    m_accessorInfo = info->clone( info->parent() );
    m_openedPath = fileName;
    ui_accessor->savePath->setText( fileName );
//     ui_accessor->savePath->setToolTip( Project::savePathInfoStringFromFilePath(fileName) );
    ui_accessor->currentLanguage->setCurrentItem( "en" );
    ui_accessor->name->setText( info->names()["en"] );
    ui_accessor->description->setText( info->descriptions()["en"] );
    ui_accessor->version->setText( info->version() );
    ui_accessor->type->setCurrentIndex( static_cast<int>(info->accessorType()) - 1 );
    ui_accessor->useCityValue->setChecked( info->useSeparateCityValue() );
    ui_accessor->onlyAllowPredefinedCities->setChecked( info->onlyUseCitiesInList() );
    ui_accessor->url->setText( info->url() );
    ui_accessor->shortUrl->setText( info->shortUrl() );
    ui_accessor->minFetchWait->setValue( info->minFetchWait() );
    ui_accessor->scriptFile->setText( info->scriptFileName() );
    ui_accessor->author->setText( info->author() );
    ui_accessor->shortAuthor->setText( info->shortAuthor() );
    ui_accessor->email->setText( info->email() );
    int defaultVehicleTypeIndex =
            ui_accessor->defaultVehicleType->findData( info->defaultVehicleType() );
    ui_accessor->defaultVehicleType->setCurrentIndex(
            defaultVehicleTypeIndex > 0 ? defaultVehicleTypeIndex : 0 );
    ui_accessor->fileVersion->setText( info->fileVersion() );
    m_changelog->clear();
    m_changelog->addChangelog( info->changelog(), info->shortAuthor() );

    ui_accessor->predefinedCities->clear();
    foreach( const QString & city, info->cities() ) {
        QString lowerCity = city.toLower();
        if( info->cityNameToValueReplacementHash().contains( lowerCity ) ) {
            ui_accessor->predefinedCities->insertItem( city + "   ->   " +
                    info->cityNameToValueReplacementHash()[lowerCity] );
        } else {
            ui_accessor->predefinedCities->insertItem( city );
        }
    }

    ui_accessor->sampleStopNames->setItems( info->sampleStopNames() );
    ui_accessor->sampleCity->setText( info->sampleCity() );

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
