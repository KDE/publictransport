/*
*   Copyright 2010 Friedrich Pülz <fpuelz@gmx.de>
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

#include "timetablemateview.h"
#include "settings.h"
#include "accessorinfoxmlreader.h"

#include <QtGui/QLabel>
#include <QMenu>
#include <QBuffer>
#include <QFile>

#include <KFileDialog>
#include <KMessageBox>
#include <KInputDialog>
#include <KColorScheme>
#include <KLocale>
#include <QSignalMapper>
#include "changelogwidget.h"
#include <QScrollArea>

TimetableMateView::TimetableMateView( QWidget *parent ) : QWidget( parent ) {
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
    QRegExpValidator *versionValidator = new QRegExpValidator( QRegExp("\\d*\\.\\d*"), this );
    ui_accessor.version->setValidator( versionValidator );
    ui_accessor.fileVersion->setValidator( versionValidator );
    
    // Set a validator for the email line edit
    // The reg exp is "inspired" by http://www.regular-expressions.info/email.html
    QRegExp rx( "[a-z0-9!#$%&\\._-]+@(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.)+[a-z]{2,4}", Qt::CaseInsensitive );
    QRegExpValidator *emailValidator = new QRegExpValidator( rx, this );
    ui_accessor.email->setValidator( emailValidator );

    // Set icons and connections for the "open url buttons"
    ui_accessor.btnUrlOpen->setIcon( KIcon("document-open-remote") );
    ui_accessor.btnDepartureUrlOpen->setIcon( KIcon("document-open-remote") );
    ui_accessor.btnStopUrlOpen->setIcon( KIcon("document-open-remote") );
    ui_accessor.btnJourneyUrlOpen->setIcon( KIcon("document-open-remote") );
    connect( ui_accessor.btnUrlOpen, SIGNAL(clicked(bool)),
         this, SLOT(openUrlClicked()) );
    connect( ui_accessor.btnDepartureUrlOpen, SIGNAL(clicked(bool)),
         this, SLOT(openDepartureUrlClicked()) );
    connect( ui_accessor.btnStopUrlOpen, SIGNAL(clicked(bool)),
         this, SLOT(openStopUrlClicked()) );
    connect( ui_accessor.btnJourneyUrlOpen, SIGNAL(clicked(bool)),
         this, SLOT(openJourneyUrlClicked()) );
    
    // Set icons for "insert placeholder buttons"
    ui_accessor.btnDepartureUrlInsertPlaceHolder->setIcon( KIcon("tools-wizard") );
    ui_accessor.btnJourneyUrlInsertPlaceHolder->setIcon( KIcon("tools-wizard") );
    ui_accessor.btnStopUrlInsertPlaceHolder->setIcon( KIcon("tools-wizard") );

    // Create "Add ... Placeholder" actions for the departure raw url
    QMenu *departureMenu = new QMenu( this );
    QAction *action;
    action = new QAction( KIcon("public-transport-stop"),
              i18n("Add &Stop Name Placeholder"), this );
    action->setData( "{stop}" );
    departureMenu->addAction( action );
    
    action = new QAction( i18n("Add &City Name Placeholder"), this );
    action->setData( "{city}" );
    departureMenu->addAction( action );
    
    action = new QAction( i18n("Add &Departure Date Placeholder"), this );
    action->setData( "{date:dd.MM.yy}" );
    departureMenu->addAction( action );
    
    action = new QAction( KIcon("chronometer"), i18n("Add &Departure Time Placeholder"), this );
    action->setData( "{time}" );
    departureMenu->addAction( action );
    
    action = new QAction( i18n("Add Departure/&Arrival Placeholder"), this );
    action->setData( "{dataType}" );
    departureMenu->addAction( action );
    connect( departureMenu, SIGNAL(triggered(QAction*)),
         this, SLOT(departurePlaceHolder(QAction*)) );
    ui_accessor.btnDepartureUrlInsertPlaceHolder->setMenu( departureMenu );
    
    // Create "Add ... Placeholder" actions for the journey raw url
    QMenu *journeyMenu = new QMenu( this );
    action = new QAction( KIcon("flag-green"), i18n("Add &Start Stop Name Placeholder"), this );
    action->setData( "{startStop}" );
    journeyMenu->addAction( action );
    
    action = new QAction( KIcon("flag-red"), i18n("Add &Target Stop Name Placeholder"), this );
    action->setData( "{targetStop}" );
    journeyMenu->addAction( action );

    action = new QAction( KIcon("chronometer"), i18n("Add &Departure Time Placeholder"), this );
    action->setData( "{time}" );
    journeyMenu->addAction( action );
    connect( journeyMenu, SIGNAL(triggered(QAction*)), this, SLOT(journeyPlaceHolder(QAction*)) );
    ui_accessor.btnJourneyUrlInsertPlaceHolder->setMenu( journeyMenu );
    
    // Create "Add ... Placeholder" actions for the stop suggestions raw url
    QMenu *stopMenu = new QMenu( this );
    action = new QAction( KIcon("public-transport-stop"),
              i18n("Add &Stop Name Placeholder"), this );
    action->setData( "{stop}" );
    stopMenu->addAction( action );
    connect( stopMenu, SIGNAL(triggered(QAction*)),
             this, SLOT(stopSuggestionsPlaceHolder(QAction*)) );
    ui_accessor.btnStopUrlInsertPlaceHolder->setMenu( stopMenu );
    
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
        KIcon("status_unknown"), i18nc("@item:listbox", "Unknown"), "Unknown" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon("vehicle_type_tram"), i18nc("@item:listbox", "Tram"), "Tram" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon("vehicle_type_bus"), i18nc("@item:listbox", "Bus"), "Bus" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon("vehicle_type_subway"), i18nc("@item:listbox", "Subway"), "Subway" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon("vehicle_type_train_interurban"),
        i18nc("@item:listbox", "Interurban Train"), "TrainInterurban" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon("vehicle_type_metro"), i18nc("@item:listbox", "Metro"), "Metro" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon("vehicle_type_trolleybus"),
        i18nc("@item:listbox", "Trolley Bus"), "TrolleyBus" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon("vehicle_type_train_regional"), // TODO: Currently no special icon
        i18nc("@item:listbox", "Regional Train"), "TrainRegional" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon("vehicle_type_train_regional"),
        i18nc("@item:listbox", "Regional Express Train"), "TrainRegionalExpress" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon("vehicle_type_train_interregional"),
        i18nc("@item:listbox", "Interregional Train"), "TrainInterregio" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon("vehicle_type_train_intercity"),
        i18nc("@item:listbox", "Intercity/Eurocity Train"), "TrainIntercityEurocity" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon("vehicle_type_train_highspeed"),
        i18nc("@item:listbox", "Intercity Express Train"), "TrainIntercityExpress" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon("vehicle_type_ferry"), i18nc("@item:listbox", "Ferry"), "Ferry" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon("vehicle_type_ferry"), i18nc("@item:listbox", "Ship"), "Ship" );
    ui_accessor.defaultVehicleType->addItem(
        KIcon("vehicle_type_plane"), i18nc("@item:listbox", "Plane"), "Plane" );

    // Connect all change signals of the widgets to the changed() signal
    m_mapper = new QSignalMapper( this );
    connect( ui_accessor.name, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor.description, SIGNAL(textChanged()), m_mapper, SLOT(map()) );
    connect( ui_accessor.version, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor.type, SIGNAL(currentIndexChanged(int)), m_mapper, SLOT(map()) );
    connect( ui_accessor.useCityValue, SIGNAL(stateChanged(int)), m_mapper, SLOT(map()) );
    connect( ui_accessor.onlyAllowPredefinedCities, SIGNAL(stateChanged(int)), m_mapper, SLOT(map()) );
    connect( ui_accessor.url, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor.shortUrl, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor.rawDepartureUrl, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor.rawJourneyUrl, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor.rawStopSuggestionsUrl, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor.minFetchWait, SIGNAL(valueChanged(int)), m_mapper, SLOT(map()) );
    connect( ui_accessor.scriptFile, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor.author, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor.shortAuthor, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor.email, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
    connect( ui_accessor.defaultVehicleType, SIGNAL(currentIndexChanged(int)), m_mapper, SLOT(map()) );
    connect( ui_accessor.fileVersion, SIGNAL(textChanged(QString)), m_mapper, SLOT(map()) );
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
    m_mapper->setMapping( ui_accessor.rawDepartureUrl, ui_accessor.rawDepartureUrl );
    m_mapper->setMapping( ui_accessor.rawJourneyUrl, ui_accessor.rawJourneyUrl );
    m_mapper->setMapping( ui_accessor.rawStopSuggestionsUrl, ui_accessor.rawStopSuggestionsUrl );
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

TimetableMateView::~TimetableMateView() {

}

void TimetableMateView::slotChanged( QWidget* changedWidget ) {
    if ( changedWidget == ui_accessor.scriptFile ) {
        // Script file changed
        const QString fileName = ui_accessor.scriptFile->text();
        ui_accessor.btnCreateScriptFile->setVisible( fileName.isEmpty() );
        ui_accessor.btnDetachScriptFile->setVisible( !fileName.isEmpty() );
        emit scriptFileChanged( fileName );
    } else if ( changedWidget == ui_accessor.fileVersion ) {
        // File version changed
        if ( ui_accessor.fileVersion->text() != "1.0" ) {
            ui_accessor.lblFileVersionWarning->setText( i18nc("@info",
                "The PublicTransport data engine currently only supports version '1.0'.") );
            ui_accessor.lblFileVersionWarning->show();
        } else {
            ui_accessor.lblFileVersionWarning->hide();
        }
    } else if ( changedWidget == ui_accessor.url ) {
        // Home page URL changed
        ui_accessor.btnUrlOpen->setDisabled( ui_accessor.url->text().isEmpty() );
    } else if ( changedWidget == ui_accessor.rawDepartureUrl ) {
        // Raw departure URL changed
        QString newUrl = ui_accessor.rawDepartureUrl->text();
        ui_accessor.btnDepartureUrlOpen->setDisabled( newUrl.isEmpty() );

        bool hasCityPlaceholder = newUrl.contains("{city}")
            || ui_accessor.rawJourneyUrl->text().contains("{city}");
        ui_accessor.useCityValue->setChecked( hasCityPlaceholder );
        ui_accessor.predefinedCities->setEnabled( hasCityPlaceholder );
    } else if ( changedWidget == ui_accessor.rawStopSuggestionsUrl ) {
        // Raw stop suggestions URL changed
        ui_accessor.btnStopUrlOpen->setDisabled( ui_accessor.rawStopSuggestionsUrl->text().isEmpty() );
    } else if ( changedWidget == ui_accessor.rawJourneyUrl ) {
        // Raw journey URL changed
        QString newUrl = ui_accessor.rawJourneyUrl->text();
        ui_accessor.btnJourneyUrlOpen->setDisabled( newUrl.isEmpty() );

        bool hasCityPlaceholder = newUrl.contains("{city}")
            || ui_accessor.rawDepartureUrl->text().contains("{city}");
        ui_accessor.useCityValue->setChecked( hasCityPlaceholder );
        ui_accessor.predefinedCities->setEnabled( hasCityPlaceholder );
    } else if ( changedWidget == ui_accessor.shortAuthor ) {
        // Short author name changed, update changed log click messages
        QList<ChangelogEntryWidget*> entryWidgets = m_changelog->entryWidgets();
        foreach ( const ChangelogEntryWidget *entryWidget, entryWidgets ) {
            entryWidget->authorLineEdit()->setClickMessage( m_accessor.shortAuthor );
        }
    }
    
    fillValuesFromWidgets();
    emit changed();
}

void TimetableMateView::fillValuesFromWidgets() {
    // Fill struct with current values of the widgets
    QString lang = ui_accessor.currentLanguage->current();
    if ( lang == "en_US" ) {
        lang = "en";
    }
    m_accessor.name[lang] = ui_accessor.name->text();
    m_accessor.description[lang] = ui_accessor.description->toPlainText();
    m_accessor.version = ui_accessor.version->text();
    m_accessor.type = static_cast<AccessorType>( ui_accessor.type->currentIndex() + 1 );
    m_accessor.useCityValue = ui_accessor.useCityValue->isChecked();
    m_accessor.onlyUseCitiesInList = ui_accessor.onlyAllowPredefinedCities->isChecked();
    m_accessor.url = ui_accessor.url->text();
    m_accessor.shortUrl = ui_accessor.shortUrl->text();
    m_accessor.rawDepartureUrl = ui_accessor.rawDepartureUrl->text();
    m_accessor.rawJourneyUrl = ui_accessor.rawJourneyUrl->text();
    m_accessor.rawStopSuggestionsUrl = ui_accessor.rawStopSuggestionsUrl->text();
    m_accessor.minFetchWait = ui_accessor.minFetchWait->value();
    m_accessor.scriptFile = ui_accessor.scriptFile->text();
    m_accessor.author = ui_accessor.author->text();
    m_accessor.shortAuthor = ui_accessor.shortAuthor->text();
    m_accessor.email = ui_accessor.email->text();
    m_accessor.defaultVehicleType = ui_accessor.defaultVehicleType->itemData(
        ui_accessor.defaultVehicleType->currentIndex() ).toString();
    m_accessor.fileVersion = ui_accessor.fileVersion->text();
    m_accessor.changelog = m_changelog->changelog();

    m_accessor.cities.clear();
    m_accessor.cityNameReplacements.clear();
    const QStringList cityReplacements = ui_accessor.predefinedCities->items();
    foreach ( const QString &cityReplacement, cityReplacements ) {
        QStringList values = cityReplacement.split( "   ->   " );
        if ( values.count() == 2 ) {
            m_accessor.cities << values.at( 0 );
            m_accessor.cityNameReplacements.insert( values.at(0).toLower(), values.at(1) );
        } else {
            m_accessor.cities << cityReplacement;
        }
    }
}

void TimetableMateView::currentPredefinedCityChanged( const QString& currentCityText ) {
    QStringList values = currentCityText.split( "   ->   " );
    if ( values.count() == 2 ) {
        m_cityName->blockSignals( true );
        m_cityReplacement->blockSignals( true );
        
        m_cityName->setText( values.at(0) );
        m_cityReplacement->setText( values.at(1) );
        
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

void TimetableMateView::predefinedCityNameChanged( const QString& newCityName ) {
    QString text = newCityName;
    if ( !m_cityReplacement->text().isEmpty() ) {
        text += "   ->   " + m_cityReplacement->text();
    }
    
    m_predefinedCitiesCustomEditor.lineEdit()->setText( text );
}

void TimetableMateView::predefinedCityReplacementChanged( const QString& newReplacement ) {
    QString text = m_cityName->text();
    if ( !newReplacement.isEmpty() ) {
        text += "   ->   " + newReplacement;
    }

    m_predefinedCitiesCustomEditor.lineEdit()->setText( text );
}

void TimetableMateView::languageActivated( const QString& languageCode ) {
    QString code = languageCode == "en_US" ? "en" : languageCode;
    
    ui_accessor.name->blockSignals( true );
    ui_accessor.name->setText( m_accessor.name.value(code) );
    ui_accessor.name->blockSignals( false );

    ui_accessor.description->blockSignals( true );
    ui_accessor.description->setText( m_accessor.description.value(code) );
    ui_accessor.description->blockSignals( false );
}

void TimetableMateView::openUrlClicked() {
    emit urlShouldBeOpened( ui_accessor.url->text() );
}

void TimetableMateView::openDepartureUrlClicked() {
    emit urlShouldBeOpened( ui_accessor.rawDepartureUrl->text(), RawDepartureUrl );
}

void TimetableMateView::openStopUrlClicked() {
    emit urlShouldBeOpened( ui_accessor.rawStopSuggestionsUrl->text(),
                RawStopSuggestionsUrl );
}

void TimetableMateView::openJourneyUrlClicked() {
    emit urlShouldBeOpened( ui_accessor.rawJourneyUrl->text(), RawJourneyUrl );
}

void TimetableMateView::createScriptFile() {
    if ( m_openedPath.isEmpty() ) {
        KMessageBox::information( this, i18nc("@info/plain", "Please save the "
            "XML file first. The script file needs to be in the same folder.") );
        return;
    }

    // Get a name for the new script file based on the current country code 
    // and the current service provider ID
    const QString scriptType = KInputDialog::getItem(
        i18nc("@title:window", "Choose Script Type"), i18nc("@info", "Script Type"),
        QStringList() << "JavaScript" << "Ruby" << "Python", 0, false, 0, this );
    QString scriptFile = m_currentServiceProviderID;
    if ( scriptType == "JavaScript" ) {
        scriptFile += ".js";
    } else if ( scriptType == "Ruby" ) {
        scriptFile += ".rb";
    } else if ( scriptType == "Python" ) {
        scriptFile += ".py";
    }

    // Get fileName for the new script file
    QString fullScriptFile = KUrl(m_openedPath).directory(KUrl::AppendTrailingSlash) + scriptFile;

    // Check if the file already exists
    QFile file( fullScriptFile );
    if ( file.exists() ) {
        int result = KMessageBox::questionYesNoCancel( this,
            i18nc("@info/plain", "The script file <filename>%1</filename> already exists.<nl/>"
                "Do you want to overwrite it or open and use it as script file?", scriptFile),
            i18nc("@title:window", "File Already Exists"),
            KStandardGuiItem::overwrite(), KStandardGuiItem::open() );
        if ( result == KMessageBox::No ) { // open
            ui_accessor.scriptFile->setText( scriptFile );
            return;
        } else if ( result == KMessageBox::Cancel ) {
            return;
        }
    }
    
    // Create the file    
    if ( !file.open(QIODevice::WriteOnly) ) {
        KMessageBox::information( this, i18nc("@info/plain", "A new script file "
            "with the name <filename>%1</filename> could not be created.",
            fullScriptFile) );
        return;
    }
    file.close();
    
    ui_accessor.scriptFile->setText( scriptFile );
    emit scriptAdded( fullScriptFile );
}

void TimetableMateView::detachScriptFile() {
    ui_accessor.scriptFile->setText( QString() );
}

void TimetableMateView::browseForScriptFile() {
    if ( m_openedPath.isEmpty() ) {
        KMessageBox::information( this, i18nc("@info/plain", "Please save the "
            "XML file first. The script file needs to be in the same folder.") );
        return;
    }
    
    KUrl openedUrl( m_openedPath );

    // Get a list of all script files in the directory of the XML file
    QStringList scriptFiles;
    int current = -1;
    QDir dir( openedUrl.directory() );
    QStringList fileNames = dir.entryList();
    for ( int i = 0; i < fileNames.count(); ++i ) {
        QString fileName = fileNames.at( i );
        KMimeType::Ptr mimeType = KMimeType::findByUrl( KUrl(fileName) );
        if ( mimeType->is("application/javascript")
            || mimeType->is("application/x-ruby") || mimeType->is("text/x-python") )
        {
            scriptFiles << fileName;
            if ( fileName == ui_accessor.scriptFile->text() ) {
                current = i;
            }
        }
    }

    bool ok;
    QString selectedFile = KInputDialog::getItem( i18nc("@title:window", "Choose Script File"),
               i18nc("@info", "Script File for Parsing Documents"),
               scriptFiles, current, false, &ok, this );
    if ( ok ) {
        ui_accessor.scriptFile->setText( selectedFile );
    }
}

void TimetableMateView::departurePlaceHolder( QAction *action ) {
    QString placeholder = action->data().toString();
    ui_accessor.rawDepartureUrl->insert( placeholder );
}

void TimetableMateView::journeyPlaceHolder( QAction *action ) {
    QString placeholder = action->data().toString();
    ui_accessor.rawJourneyUrl->insert( placeholder );
}

void TimetableMateView::stopSuggestionsPlaceHolder( QAction *action ) {
    QString placeholder = action->data().toString();
    ui_accessor.rawStopSuggestionsUrl->insert( placeholder );
}

TimetableAccessor TimetableMateView::accessorInfo() const {
    return m_accessor;
}

void TimetableMateView::setScriptFile( const QString& scriptFile ) {
    ui_accessor.scriptFile->setText( scriptFile );
}

bool TimetableMateView::readAccessorInfoXml( const QString& fileName, QString *error ) {
    QFile file( fileName );
    return readAccessorInfoXml( &file, error, fileName );
}

bool TimetableMateView::readAccessorInfoXml( QIODevice* device, QString *error,
                         const QString& fileName ) {
    AccessorInfoXmlReader reader;
    m_accessor = reader.read( device );
    if ( !m_accessor.isValid() ) {
        kDebug() << "Accessor is invalid" << reader.errorString() << fileName;
        if ( error ) {
            *error = reader.errorString();
        }
        return false;
    }

    // Disable changed signals from widgets while setting the read values
    m_mapper->blockSignals( true );
    
    m_openedPath = fileName;
    ui_accessor.currentLanguage->setCurrentItem( "en" );
    ui_accessor.name->setText( m_accessor.name["en"] );
    ui_accessor.description->setText( m_accessor.description["en"] );
    ui_accessor.version->setText( m_accessor.version );
    ui_accessor.type->setCurrentIndex( static_cast<int>(m_accessor.type) - 1 );
    ui_accessor.useCityValue->setChecked( m_accessor.useCityValue );
    ui_accessor.onlyAllowPredefinedCities->setChecked( m_accessor.onlyUseCitiesInList );
    ui_accessor.url->setText( m_accessor.url );
    ui_accessor.shortUrl->setText( m_accessor.shortUrl );
    ui_accessor.rawDepartureUrl->setText( m_accessor.rawDepartureUrl );
    ui_accessor.rawJourneyUrl->setText( m_accessor.rawJourneyUrl );
    ui_accessor.rawStopSuggestionsUrl->setText( m_accessor.rawStopSuggestionsUrl );
    ui_accessor.minFetchWait->setValue( m_accessor.minFetchWait );
    ui_accessor.scriptFile->setText( m_accessor.scriptFile );
    ui_accessor.author->setText( m_accessor.author );
    ui_accessor.shortAuthor->setText( m_accessor.shortAuthor );
    ui_accessor.email->setText( m_accessor.email );
    int defaultVehicleTypeIndex =
        ui_accessor.defaultVehicleType->findData( m_accessor.defaultVehicleType );
    ui_accessor.defaultVehicleType->setCurrentIndex(
        defaultVehicleTypeIndex > 0 ? defaultVehicleTypeIndex : 0 );
    ui_accessor.fileVersion->setText( m_accessor.fileVersion );
    m_changelog->clear();
    m_changelog->addChangelog( m_accessor.changelog, m_accessor.shortAuthor );

    ui_accessor.predefinedCities->clear();
    foreach ( const QString &city, m_accessor.cities ) {
        QString lowerCity = city.toLower();
        if ( m_accessor.cityNameReplacements.contains(lowerCity) ) {
            ui_accessor.predefinedCities->insertItem( city + "   ->   " +
                m_accessor.cityNameReplacements[lowerCity] );
        } else {
            ui_accessor.predefinedCities->insertItem( city );
        }
    }

    // Enable changed signals from widgets again and emit changed signals once
    m_mapper->blockSignals( false );
    emit changed();
    emit scriptFileChanged( fileName );
    
    if ( error ) {
        error->clear();
    }
    return true;
}

bool TimetableMateView::writeAccessorInfoXml( const QString& fileName ) {
    AccessorInfoXmlWriter writer;
    QFile file( fileName );
    bool ret = writer.write( &file, m_accessor );
    if ( ret ) {
        m_openedPath = fileName;
    }
    return ret;
}

QString TimetableMateView::writeAccessorInfoXml() {
    AccessorInfoXmlWriter writer;
    QBuffer buffer;
    if ( writer.write(&buffer, m_accessor) ) {
        return QString::fromUtf8( buffer.data() );
    } else {
        return QString();
    }
}

void TimetableMateView::settingsChanged() {
    emit signalChangeStatusbar( i18n("Settings changed") );
}

#include "timetablemateview.moc"
