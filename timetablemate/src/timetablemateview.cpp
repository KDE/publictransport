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
#include "timetablemateview.h"

// Own includes
#include "settings.h"
#include "changelogwidget.h"
#include "accessorinfoxmlwriter.h"

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

// Qt includes
#include <QtGui/QLabel>
#include <QMenu>
#include <QBuffer>
#include <QFile>
#include <QSignalMapper>
#include <QScrollArea>

TimetableMateView::TimetableMateView( QWidget *parent )
        : QWidget( parent ), m_accessor(0), m_cityName(0), m_cityReplacement(0), m_changelog(0),
          m_mapper(0)
{
    ui_accessor.setupUi( this );
    settingsChanged();

    // Initialize script file buttons
    ui_accessor.btnBrowseForScriptFile->setIcon( KIcon("document-open") );
    ui_accessor.btnCreateScriptFile->setIcon( KIcon("document-new") );
    ui_accessor.btnDetachScriptFile->setIcon( KIcon("list-remove") );
    ui_accessor.btnDetachScriptFile->setVisible( false );
    connect( ui_accessor.btnBrowseForScriptFile, SIGNAL(clicked(bool)),
             this, SLOT(browseForScriptFile()) );
    connect( ui_accessor.btnCreateScriptFile, SIGNAL(clicked(bool)),
             this, SLOT(createScriptFile()) );
    connect( ui_accessor.btnDetachScriptFile, SIGNAL(clicked(bool)),
             this, SLOT(detachScriptFile()) );

    // Initialize the language button
    ui_accessor.currentLanguage->loadAllLanguages();
    ui_accessor.currentLanguage->insertLanguage( "en", QString(), 0 );
    ui_accessor.currentLanguage->insertSeparator( 1 );
    connect( ui_accessor.currentLanguage, SIGNAL(activated(QString)),
             this, SLOT(languageActivated(QString)) );

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
    ui_accessor.predefinedCities->setCustomEditor( m_predefinedCitiesCustomEditor );
    connect( m_cityName, SIGNAL(textChanged(QString)),
             this, SLOT(predefinedCityNameChanged(QString)) );
    connect( m_cityReplacement, SIGNAL(textChanged(QString)),
             this, SLOT(predefinedCityReplacementChanged(QString)) );
    connect( defaultLineEdit, SIGNAL(textChanged(QString)),
             this, SLOT(currentPredefinedCityChanged(QString)) );

    // Use negative text color for the warning labels
    ui_accessor.lblFileVersionWarning->hide();
    QPalette pal = ui_accessor.lblFileVersionWarning->palette();
    KColorScheme::adjustForeground( pal, KColorScheme::NegativeText,
                                    QPalette::WindowText, KColorScheme::Window );
    ui_accessor.lblFileVersionWarning->setPalette( pal );

    // Set a validator for version line edits
    QRegExpValidator *versionValidator = new QRegExpValidator( QRegExp( "\\d*\\.\\d*" ), this );
    ui_accessor.version->setValidator( versionValidator );
    ui_accessor.fileVersion->setValidator( versionValidator );

    // Set a validator for the email line edit
    // The reg exp is "inspired" by http://www.regular-expressions.info/email.html
    QRegExp rx( "[a-z0-9!#$%&\\._-]+@(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.)+[a-z]{2,4}", Qt::CaseInsensitive );
    QRegExpValidator *emailValidator = new QRegExpValidator( rx, this );
    ui_accessor.email->setValidator( emailValidator );

    // Set icons and connections for the "open url buttons"
    ui_accessor.btnUrlOpen->setIcon( KIcon("document-open-remote") );
    connect( ui_accessor.btnUrlOpen, SIGNAL(clicked(bool)),
             this, SLOT(openUrlClicked()) );

    // Add changelog widget into a scroll area
    QVBoxLayout *changelogAreaLayout = new QVBoxLayout( ui_accessor.tabChangelog );
    QScrollArea *changelogArea = new QScrollArea( ui_accessor.tabChangelog );
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

    // Add vehicle types with icons to the default vehicle type combo box
    ui_accessor.defaultVehicleType->addItem(
        KIcon( "status_unknown" ), i18nc( "@item:listbox", "Unknown" ), "Unknown" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon( "vehicle_type_tram" ), i18nc( "@item:listbox", "Tram" ), "Tram" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon( "vehicle_type_bus" ), i18nc( "@item:listbox", "Bus" ), "Bus" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon( "vehicle_type_subway" ), i18nc( "@item:listbox", "Subway" ), "Subway" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_interurban" ),
        i18nc( "@item:listbox", "Interurban Train" ), "TrainInterurban" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon( "vehicle_type_metro" ), i18nc( "@item:listbox", "Metro" ), "Metro" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon( "vehicle_type_trolleybus" ),
        i18nc( "@item:listbox", "Trolley Bus" ), "TrolleyBus" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_regional" ), // TODO: Currently no special icon
        i18nc( "@item:listbox", "Regional Train" ), "TrainRegional" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_regional" ),
        i18nc( "@item:listbox", "Regional Express Train" ), "TrainRegionalExpress" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_interregional" ),
        i18nc( "@item:listbox", "Interregional Train" ), "TrainInterregio" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_intercity" ),
        i18nc( "@item:listbox", "Intercity/Eurocity Train" ), "TrainIntercityEurocity" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon( "vehicle_type_train_highspeed" ),
        i18nc( "@item:listbox", "Intercity Express Train" ), "TrainIntercityExpress" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon( "vehicle_type_ferry" ), i18nc( "@item:listbox", "Ferry" ), "Ferry" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon( "vehicle_type_ferry" ), i18nc( "@item:listbox", "Ship" ), "Ship" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon( "vehicle_type_plane" ), i18nc( "@item:listbox", "Plane" ), "Plane" );

    // Connect all change signals of the widgets to the changed() signal
    m_mapper = new QSignalMapper( this );
    connect( ui_accessor.name, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor.description, SIGNAL(textChanged()), m_mapper, SLOT(map()) );
    connect( ui_accessor.version, SIGNAL(textChanged(QString )), m_mapper, SLOT(map()) );
    connect( ui_accessor.type, SIGNAL(currentIndexChanged(int )), m_mapper, SLOT(map()) );
    connect( ui_accessor.useCityValue, SIGNAL(stateChanged(int )), m_mapper, SLOT(map()) );
    connect( ui_accessor.onlyAllowPredefinedCities, SIGNAL(stateChanged(int )), m_mapper, SLOT(map()) );
    connect( ui_accessor.url, SIGNAL(textChanged(QString )), m_mapper, SLOT(map()) );
    connect( ui_accessor.shortUrl, SIGNAL(textChanged(QString )), m_mapper, SLOT(map()) );
    connect( ui_accessor.minFetchWait, SIGNAL(valueChanged(int )), m_mapper, SLOT(map()) );
    connect( ui_accessor.scriptFile, SIGNAL(textChanged(QString )), m_mapper, SLOT(map()) );
    connect( ui_accessor.author, SIGNAL(textChanged(QString )), m_mapper, SLOT(map()) );
    connect( ui_accessor.shortAuthor, SIGNAL(textChanged(QString )), m_mapper, SLOT(map()) );
    connect( ui_accessor.email, SIGNAL(textChanged(QString )), m_mapper, SLOT(map()) );
    connect( ui_accessor.defaultVehicleType, SIGNAL(currentIndexChanged(int )), m_mapper, SLOT(map()) );
    connect( ui_accessor.fileVersion, SIGNAL(textChanged(QString )), m_mapper, SLOT(map()) );
    connect( ui_accessor.predefinedCities, SIGNAL(changed()), m_mapper, SLOT(map()) );
    connect( m_changelog, SIGNAL(added(QWidget*)), m_mapper, SLOT(map()) );
    connect( m_changelog, SIGNAL(removed(QWidget*,int)), m_mapper, SLOT(map()) );
    connect( m_changelog, SIGNAL(changed()), m_mapper, SLOT(map()) );
    // TODO Map changes in the changelog
    m_mapper->setMapping( ui_accessor.name, ui_accessor.name );
    m_mapper->setMapping( ui_accessor.description, ui_accessor.description );
    m_mapper->setMapping( ui_accessor.version, ui_accessor.version );
    m_mapper->setMapping( ui_accessor.type, ui_accessor.type );
    m_mapper->setMapping( ui_accessor.useCityValue, ui_accessor.useCityValue );
    m_mapper->setMapping( ui_accessor.onlyAllowPredefinedCities, ui_accessor.onlyAllowPredefinedCities );
    m_mapper->setMapping( ui_accessor.url, ui_accessor.url );
    m_mapper->setMapping( ui_accessor.shortUrl, ui_accessor.shortUrl );
    m_mapper->setMapping( ui_accessor.minFetchWait, ui_accessor.minFetchWait );
    m_mapper->setMapping( ui_accessor.scriptFile, ui_accessor.scriptFile );
    m_mapper->setMapping( ui_accessor.author, ui_accessor.author );
    m_mapper->setMapping( ui_accessor.shortAuthor, ui_accessor.shortAuthor );
    m_mapper->setMapping( ui_accessor.email, ui_accessor.email );
    m_mapper->setMapping( ui_accessor.defaultVehicleType, ui_accessor.defaultVehicleType );
    m_mapper->setMapping( ui_accessor.fileVersion, ui_accessor.fileVersion );
    m_mapper->setMapping( ui_accessor.predefinedCities, ui_accessor.predefinedCities );
    m_mapper->setMapping( m_changelog, m_changelog );
    connect( m_mapper, SIGNAL(mapped(QWidget*)), this, SLOT(slotChanged(QWidget*)) );

    fillValuesFromWidgets();
}

TimetableMateView::~TimetableMateView()
{
}

void TimetableMateView::slotChanged( QWidget *changedWidget )
{
    if( changedWidget == ui_accessor.scriptFile ) {
        // Script file changed
        const QString fileName = ui_accessor.scriptFile->text();
        ui_accessor.btnCreateScriptFile->setVisible( fileName.isEmpty() );
        ui_accessor.btnDetachScriptFile->setVisible( !fileName.isEmpty() );
        emit scriptFileChanged( fileName );
    } else if( changedWidget == ui_accessor.fileVersion ) {
        // File version changed
        if( ui_accessor.fileVersion->text() != "1.0" ) {
            ui_accessor.lblFileVersionWarning->setText( i18nc( "@info",
                    "The PublicTransport data engine currently only supports version '1.0'." ) );
            ui_accessor.lblFileVersionWarning->show();
        } else {
            ui_accessor.lblFileVersionWarning->hide();
        }
    } else if( changedWidget == ui_accessor.url ) {
        // Home page URL changed
        ui_accessor.btnUrlOpen->setDisabled( ui_accessor.url->text().isEmpty() );
    } else if( changedWidget == ui_accessor.shortAuthor ) {
        // Short author name changed, update changed log click messages
        QList<ChangelogEntryWidget *> entryWidgets = m_changelog->entryWidgets();
        foreach( const ChangelogEntryWidget * entryWidget, entryWidgets ) {
            entryWidget->authorLineEdit()->setClickMessage( m_accessor->info()->shortAuthor() );
        }
    }

    fillValuesFromWidgets();
    emit changed();
}

void TimetableMateView::fillValuesFromWidgets()
{
    if ( !m_accessor ) {
        kDebug() << "No accessor loaded to fill with values";
        return;
    }

    // Fill struct with current values of the widgets
    QString lang = ui_accessor.currentLanguage->current();
    if( lang == "en_US" ) {
        lang = "en";
    }

    QHash< QString, QString > names = m_accessor->info()->names();
    QHash< QString, QString > descriptions = m_accessor->info()->descriptions();
    names[lang] = ui_accessor.name->text();
    descriptions[lang] = ui_accessor.description->toPlainText();
    const VehicleType defaultVehicleType = Global::vehicleTypeFromString(
            ui_accessor.defaultVehicleType->itemData(
            ui_accessor.defaultVehicleType->currentIndex()).toString() );

    QStringList cities;
    QHash< QString, QString > cityNameReplacements;
    const QStringList cityReplacements = ui_accessor.predefinedCities->items();
    foreach( const QString & cityReplacement, cityReplacements ) {
        QStringList values = cityReplacement.split( "   ->   " );
        if( values.count() == 2 ) {
            cities << values.at( 0 );
            cityNameReplacements.insert( values.at(0).toLower(), values.at(1) );
        } else {
            cities << cityReplacement;
        }
    }

    // Create new info object, can't access setters, only AccessorInfoXmlReader can,
    // therefore a new TimetableAccessor objects gets created with the new info object
    TimetableAccessorInfo *info = new TimetableAccessorInfo(
            static_cast<AccessorType>(ui_accessor.type->currentIndex() + 1),
            m_accessor->serviceProvider(), names, descriptions, ui_accessor.version->text(),
            ui_accessor.fileVersion->text(), ui_accessor.useCityValue->isChecked(),
            ui_accessor.onlyAllowPredefinedCities->isChecked(), ui_accessor.url->text(),
            ui_accessor.shortUrl->text(), ui_accessor.minFetchWait->value(),
            ui_accessor.author->text(), ui_accessor.email->text(), defaultVehicleType,
            m_changelog->changelog(),
            cities, cityNameReplacements );
    delete m_accessor;
    m_accessor = new TimetableAccessor( info );
}

void TimetableMateView::currentPredefinedCityChanged( const QString &currentCityText )
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

void TimetableMateView::predefinedCityNameChanged( const QString &newCityName )
{
    QString text = newCityName;
    if( !m_cityReplacement->text().isEmpty() ) {
        text += "   ->   " + m_cityReplacement->text();
    }

    m_predefinedCitiesCustomEditor.lineEdit()->setText( text );
}

void TimetableMateView::predefinedCityReplacementChanged( const QString &newReplacement )
{
    QString text = m_cityName->text();
    if( !newReplacement.isEmpty() ) {
        text += "   ->   " + newReplacement;
    }

    m_predefinedCitiesCustomEditor.lineEdit()->setText( text );
}

void TimetableMateView::languageActivated( const QString &languageCode )
{
    QString code = languageCode == "en_US" ? "en" : languageCode;

    ui_accessor.name->blockSignals( true );
    ui_accessor.name->setText( m_accessor->info()->names()[code] );
    ui_accessor.name->blockSignals( false );

    ui_accessor.description->blockSignals( true );
    ui_accessor.description->setText( m_accessor->info()->descriptions()[code] );
    ui_accessor.description->blockSignals( false );
}

void TimetableMateView::openUrlClicked()
{
    emit urlShouldBeOpened( ui_accessor.url->text() );
}

void TimetableMateView::createScriptFile()
{
    if( m_openedPath.isEmpty() ) {
        KMessageBox::information( this, i18nc( "@info/plain", "Please save the "
                                               "XML file first. The script file needs to be in the same folder." ) );
        return;
    }

    // Get a name for the new script file based on the current country code
    // and the current service provider ID
    const QString scriptType = KInputDialog::getItem(
                                   i18nc( "@title:window", "Choose Script Type" ), i18nc( "@info", "Script Type" ),
                                   QStringList() << "JavaScript" << "Ruby" << "Python", 0, false, 0, this );
    QString scriptFile = m_currentServiceProviderID;
    if( scriptType == "JavaScript" ) {
        scriptFile += ".js";
    } else if( scriptType == "Ruby" ) {
        scriptFile += ".rb";
    } else if( scriptType == "Python" ) {
        scriptFile += ".py";
    }

    // Get fileName for the new script file
    QString fullScriptFile = KUrl( m_openedPath ).directory( KUrl::AppendTrailingSlash ) + scriptFile;

    // Check if the file already exists
    QFile file( fullScriptFile );
    if( file.exists() ) {
        int result = KMessageBox::questionYesNoCancel( this,
                     i18nc( "@info/plain", "The script file <filename>%1</filename> already exists.<nl/>"
                            "Do you want to overwrite it or open and use it as script file?", scriptFile ),
                     i18nc( "@title:window", "File Already Exists" ),
                     KStandardGuiItem::overwrite(), KStandardGuiItem::open() );
        if( result == KMessageBox::No ) {  // open
            ui_accessor.scriptFile->setText( scriptFile );
            return;
        } else if( result == KMessageBox::Cancel ) {
            return;
        }
    }

    // Create the file
    if( !file.open( QIODevice::WriteOnly ) ) {
        KMessageBox::information( this, i18nc( "@info/plain", "A new script file "
                                               "with the name <filename>%1</filename> could not be created.",
                                               fullScriptFile ) );
        return;
    }
    file.close();

    ui_accessor.scriptFile->setText( scriptFile );
    emit scriptAdded( fullScriptFile );
}

void TimetableMateView::detachScriptFile()
{
    ui_accessor.scriptFile->setText( QString() );
}

void TimetableMateView::browseForScriptFile()
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
        if( mimeType->is( "application/javascript" )
                || mimeType->is( "application/x-ruby" ) || mimeType->is( "text/x-python" ) ) {
            scriptFiles << fileName;
            if( fileName == ui_accessor.scriptFile->text() ) {
                current = i;
            }
        }
    }

    bool ok;
    QString selectedFile = KInputDialog::getItem( i18nc( "@title:window", "Choose Script File" ),
                           i18nc( "@info", "Script File for Parsing Documents" ),
                           scriptFiles, current, false, &ok, this );
    if( ok ) {
        ui_accessor.scriptFile->setText( selectedFile );
    }
}

TimetableAccessor *TimetableMateView::accessorInfo() const
{
    return m_accessor;
}

void TimetableMateView::setScriptFile( const QString &scriptFile )
{
    ui_accessor.scriptFile->setText( scriptFile );
}

bool TimetableMateView::readAccessorInfoXml( const QString &fileName, QString *error )
{
    QFile file( fileName );
    return readAccessorInfoXml( &file, error, fileName );
}

bool TimetableMateView::readAccessorInfoXml( QIODevice *device, QString *error,
        const QString &fileName )
{
    const QString serviceProvider = QFileInfo(fileName).baseName();
    QString country = "international";

    // Get country code from filename
    QRegExp rx( "^([^_]+)" );
    if ( rx.indexIn(serviceProvider) != -1 &&
         KGlobal::locale()->allCountriesList().contains(rx.cap()) )
    {
        country = rx.cap();
    }

    AccessorInfoXmlReader reader;
    m_accessor = reader.read( device, serviceProvider, fileName, country );
    if( !m_accessor ) {
        kDebug() << "Accessor is invalid" << reader.errorString() << fileName;
        if( error ) {
            *error = reader.errorString();
        }
        return false;
    }

    // Disable changed signals from widgets while setting the read values
    m_mapper->blockSignals( true );

    const TimetableAccessorInfo *info = m_accessor->info();
    m_openedPath = fileName;
    ui_accessor.currentLanguage->setCurrentItem( "en" );
    ui_accessor.name->setText( info->names()["en"] );
    ui_accessor.description->setText( info->descriptions()["en"] );
    ui_accessor.version->setText( info->version() );
    ui_accessor.type->setCurrentIndex( static_cast<int>(info->accessorType()) - 1 );
    ui_accessor.useCityValue->setChecked( info->useSeparateCityValue() );
    ui_accessor.onlyAllowPredefinedCities->setChecked( info->onlyUseCitiesInList() );
    ui_accessor.url->setText( info->url() );
    ui_accessor.shortUrl->setText( info->shortUrl() );
    ui_accessor.minFetchWait->setValue( info->minFetchWait() );
    ui_accessor.scriptFile->setText( info->scriptFileName() );
    ui_accessor.author->setText( info->author() );
    ui_accessor.shortAuthor->setText( info->shortAuthor() );
    ui_accessor.email->setText( info->email() );
    int defaultVehicleTypeIndex =
            ui_accessor.defaultVehicleType->findData( info->defaultVehicleType() );
    ui_accessor.defaultVehicleType->setCurrentIndex(
            defaultVehicleTypeIndex > 0 ? defaultVehicleTypeIndex : 0 );
    ui_accessor.fileVersion->setText( info->fileVersion() );
    m_changelog->clear();
    m_changelog->addChangelog( info->changelog(), info->shortAuthor() );

    ui_accessor.predefinedCities->clear();
    foreach( const QString & city, info->cities() ) {
        QString lowerCity = city.toLower();
        if( info->cityNameToValueReplacementHash().contains( lowerCity ) ) {
            ui_accessor.predefinedCities->insertItem( city + "   ->   " +
                    info->cityNameToValueReplacementHash()[lowerCity] );
        } else {
            ui_accessor.predefinedCities->insertItem( city );
        }
    }

    // Enable changed signals from widgets again and emit changed signals once
    m_mapper->blockSignals( false );
    emit changed();
    emit scriptFileChanged( fileName );

    if( error ) {
        error->clear();
    }
    return true;
}

bool TimetableMateView::writeAccessorInfoXml( const QString &fileName )
{
    if ( !m_accessor ) {
        kDebug() << "No accessor loaded";
        return false;
    }

    AccessorInfoXmlWriter writer;
    QFile file( fileName );
    bool ret = writer.write( &file, m_accessor );
    if( ret ) {
        m_openedPath = fileName;
    }
    return ret;
}

QString TimetableMateView::writeAccessorInfoXml()
{
    if ( !m_accessor ) {
        kDebug() << "No accessor loaded";
        return false;
    }

    AccessorInfoXmlWriter writer;
    QBuffer buffer;
    if( writer.write( &buffer, m_accessor ) ) {
        return QString::fromUtf8( buffer.data() );
    } else {
        return QString();
    }
}

void TimetableMateView::settingsChanged()
{
    emit signalChangeStatusbar( i18n("Settings changed") );
}

#include "timetablemateview.moc"
// kate: indent-mode cstyle; indent-width 4; replace-tabs on;
