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

// Own includes
#include "timetablemate.h"
#include "timetablemateview.h"
#include "settings.h"
#include "publictransportpreview.h"
#include "javascriptcompletionmodel.h"
#include "javascriptmodel.h"
#include "javascriptparser.h"

// PublicTransport engine includes
#include <engine/scripting.h>
#include <engine/timetableaccessor.h>
#include <engine/timetableaccessor_info.h>
#include <engine/timetableaccessor_script.h>
#include <engine/script_thread.h>

// Qt includes
#include <QtGui/QDropEvent>
#include <QtGui/QPainter>
#include <QtGui/QPrinter>
#include <QtGui/QSplitter>
#include <QtGui/QToolTip>
#include <QtGui/QSortFilterProxyModel>
#include <QtWebKit/QWebFrame>
#include <QtWebKit/QWebInspector>
#include <QtCore/QBuffer>
#include <QtCore/QTextCodec>
#include <QtCore/QTimer>

// KDE includes
#include <KGlobalSettings>
#include <KStandardDirs>
#include <KDateTimeWidget>
#include <KTabWidget>
#include <KUrlComboBox>
#include <KMenu>
#include <KMenuBar>
#include <KToolBar>
#include <KStatusBar>
#include <KConfigDialog>
#include <KFileDialog>
#include <KInputDialog>
#include <KMessageBox>
#include <KParts/PartManager>
#include <KParts/MainWindow>
#include <KWebView>
#include <KTextEditor/Document>
#include <KTextEditor/View>
#include <KTextEditor/CodeCompletionModel>
#include <KTextEditor/CodeCompletionInterface>
#include <KTextEditor/TemplateInterface>
#include <KTextEditor/TextHintInterface>
#include <KTextEditor/MarkInterface>
#include <KLibFactory>
#include <KLibLoader>
#include <KAction>
#include <KActionCollection>
#include <KActionMenu>
#include <KStandardAction>
#include <KRecentFilesAction>
#include <Kross/Action>
#include <KAuth/Action>
#include <KAuth/ActionReply>
#include <KIO/NetAccess>
#include <KDE/KLocale>

#include <unistd.h>

const char *TimetableMate::SCRIPT_FUNCTION_USEDTIMETABLEINFORMATIONS = "usedTimetableInformations";
const char *TimetableMate::SCRIPT_FUNCTION_GETTIMETABLE = "getTimetable";
const char *TimetableMate::SCRIPT_FUNCTION_GETJOURNEYS = "getJourneys";
const char *TimetableMate::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS = "getStopSuggestions";

// TODO Move class ScriptAgent to own file(s)
// #include <QScriptEngineAgent>
// /**
//  * @brief A QScriptEngineAgent that signals when a script finishes.
//  *
//  * After a function exit the agent waits a little bit and checks if the script is still executing
//  * using QScriptEngineAgent::isEvaluating().
//  **/
// class ScriptAgent : public QObject, public QScriptEngineAgent {
//     Q_OBJECT
//
// public:
//     /** @brief Creates a new ScriptAgent instance. */
//     ScriptAgent( QScriptEngine* engine = 0 );
//
//     /** Overwritten to get noticed when a script might have finished. */
//     virtual void functionExit( qint64 scriptId, const QScriptValue& returnValue );
//
// signals:
//     /** @brief Emitted, when the script is no longer running */
//     void scriptFinished();
//
// protected slots:
//     void checkExecution();
// };
//
// ScriptAgent::ScriptAgent(QScriptEngine* engine)
//         : QObject(engine), QScriptEngineAgent::QScriptEngineAgent(engine)
// {
// }
//
// void ScriptAgent::functionExit( qint64 scriptId, const QScriptValue& returnValue )
// {
//     QTimer::singleShot( 250, this, SLOT(checkExecution()) );
// }
//
// void ScriptAgent::checkExecution()
// {
//     if ( !engine()->isEvaluating() ) {
//         emit scriptFinished();
//     }
// }

TimetableMate::TimetableMate() : KParts::MainWindow( 0, Qt::WindowContextHelpButtonHint ),
        m_mainTabBar( new KTabWidget(this) ), m_view( new TimetableMateView(this) ),
        m_backgroundParserTimer(0), m_engine(0), m_script(0), m_scriptNetwork(0),
        m_scriptHelper(0), m_scriptResult(0), m_scriptStorage(0)
{
    m_partManager = new KParts::PartManager( this );
    m_accessorDocumentChanged = false;
    m_accessorWidgetsChanged = false;
    m_changed = false;
    m_lastScriptError = NoScriptError;
    m_currentTab = AccessorTab;
    m_mainTabBar->setDocumentMode( true );

    setCentralWidget( m_mainTabBar );

    // Create plasma preview widget
    m_preview = new PublicTransportPreview( this );
    m_preview->setWhatsThis( i18nc("@info:whatsthis", "<subtitle>Plasma Preview</subtitle>"
                                   "<para>This is a preview of the PublicTransport applet in a plasma desktop. "
                                   "The applet's settings are changed so that it always uses the currently opened "
                                   "timetable accessor.</para>"
                                   "<para><note>You have to install the accessor to use it in this preview. "
                                   "Use <interface>File -&gt; Install</interface> to install the accessor locally "
                                   "or <interface>File -&gt; Install Globally</interface> to install the accessor "
                                   "globally, ie. for all users.</note></para>") );
    connect( m_preview, SIGNAL(plasmaPreviewLoaded()), this, SLOT(plasmaPreviewLoaded()) );

    // Create web view widget
    m_webview = new KWebView( this );
    m_webview->settings()->setAttribute( QWebSettings::DeveloperExtrasEnabled, true );
    m_webview->pageAction( QWebPage::OpenLinkInNewWindow )->setVisible( false );
    m_webview->pageAction( QWebPage::OpenFrameInNewWindow )->setVisible( false );
    m_webview->pageAction( QWebPage::OpenImageInNewWindow )->setVisible( false );
    m_webview->setMinimumHeight( 150 );
    m_webview->setWhatsThis( i18nc("@info:whatsthis", "<subtitle>Web View</subtitle>"
                                   "<para>This is the web view. You can use it to check the URLs you have defined "
                                   "in the <interface>Accessor</interface> settings or to get information about the "
                                   "structure of the documents that get parsed by the script.</para>"
                                   "<para><note>You can select a web element in the <emphasis>inspector</emphasis> "
                                   "using the context menu.</note></para>") );

    // Create a web inspector
    QWebInspector *inspector = new QWebInspector( this );
    inspector->setPage( m_webview->page() );
    inspector->setMinimumHeight( 150 );

    QSplitter *webSplitter = new QSplitter( this );
    webSplitter->setOrientation( Qt::Vertical );
    webSplitter->addWidget( m_webview );
    webSplitter->addWidget( inspector );

    m_urlBar = new KUrlComboBox( KUrlComboBox::Both, true, this );
    connect( m_webview, SIGNAL(statusBarMessage(QString)), this, SLOT(slotSetStatusBarText(QString)) );
    connect( m_webview, SIGNAL(urlChanged(QUrl)), this, SLOT(webUrlChanged(QUrl)) );
    connect( m_urlBar, SIGNAL(returnPressed(QString)), this, SLOT(urlBarReturn(QString)) );

    QWidget *webWidget = new QWidget( this );
    QVBoxLayout *l = new QVBoxLayout( webWidget );
    l->addWidget( m_urlBar );
    l->addWidget( webSplitter );

    setupActions();

    // Add a status bar
    statusBar()->show();

    // a call to KXmlGuiWindow::setupGUI() populates the GUI
    // with actions, using KXMLGUI.
    // It also applies the saved mainwindow settings, if any, and ask the
    // mainwindow to automatically save settings if changed: window size,
    // toolbar position, icon size, etc.
    setupGUI();

    connect( m_view, SIGNAL(scriptAdded(QString)), this, SLOT(showScriptTab()) );
    connect( m_view, SIGNAL(urlShouldBeOpened(QString)),
             this, SLOT(showWebTab(QString)) );
    connect( m_view, SIGNAL(changed()), this, SLOT(accessorWidgetsChanged()) );
    connect( m_view, SIGNAL(scriptFileChanged(QString)),
             this, SLOT(scriptFileChanged(QString)) );

    // When the manager says the active part changes,
    // the builder updates (recreates) the GUI
    connect( m_partManager, SIGNAL(activePartChanged(KParts::Part*)),
             this, SLOT(activePartChanged(KParts::Part*)) );
    connect( m_mainTabBar, SIGNAL(currentChanged(int)),
             this, SLOT(currentTabChanged(int)) );

    // Query the .desktop file to load the requested Part
    QWidget *accessorSourceWidget = 0, *scriptWidget = 0;
    JavaScriptCompletionModel *completionModel = NULL;
    KService::Ptr service = KService::serviceByDesktopPath( "katepart.desktop" );
    if ( service ) {
        m_accessorDocument = static_cast<KTextEditor::Document*>(
                                 service->createInstance<KParts::ReadWritePart>(m_mainTabBar) );
        m_scriptDocument = static_cast<KTextEditor::Document*>(
                               service->createInstance<KParts::ReadWritePart>(m_mainTabBar) );

        if ( !m_accessorDocument || !m_scriptDocument )
            return;
        connect( m_accessorDocument, SIGNAL(setStatusBarText(QString)),
                 this, SLOT(slotSetStatusBarText(QString)) );
        connect( m_scriptDocument, SIGNAL(setStatusBarText(QString)),
                 this, SLOT(slotSetStatusBarText(QString)) );

        connect( m_accessorDocument, SIGNAL(textChanged(KTextEditor::Document*)),
                 this, SLOT(accessorDocumentChanged(KTextEditor::Document*)));
        connect( m_scriptDocument, SIGNAL(textChanged(KTextEditor::Document*)),
                 this, SLOT(scriptDocumentChanged(KTextEditor::Document*)));

        m_accessorDocument->setHighlightingMode( "XML" );
        m_scriptDocument->setHighlightingMode( "JavaScript" );

        accessorSourceWidget = m_accessorDocument->widget();
        scriptWidget = m_scriptDocument->widget();

        accessorSourceWidget->setWhatsThis( i18nc("@info:whatsthis",
                                            "<subtitle>Accessor Source</subtitle>"
                                            "<para>This shows the XML source of the accessor settings. Normally you will not need "
                                            "this, because you can setup everything in the <interface>Accessor</interface> "
                                            "settings.</para>"
                                            "<para><note>Changes to <interface>Accessor</interface> and "
                                            "<interface>Accessor Source</interface> are synchronized automatically. "
                                            "Comments and unknown content in the source is removed when synchronizing."
                                            "</note></para>") );
        scriptWidget->setWhatsThis( i18nc("@info:whatsthis",
                                          "<subtitle>Script File</subtitle>"
                                          "<para>This shows the script source code. Syntax completion is available for all "
                                          "functions and strings used by the data engine.</para>"
                                          "<para>To try out the script functions just click one of the "
                                          "<interface>Run '<placeholder>function</placeholder>'</interface> buttons.</para>") );

        KTextEditor::CodeCompletionInterface *iface =
            qobject_cast<KTextEditor::CodeCompletionInterface*>( m_scriptDocument->activeView() );
        if ( iface ) {
            // Get the completion shortcut string
            QString completionShortcut;
            if ( !m_accessorDocument->views().isEmpty() ) {
                KTextEditor::View *view = m_accessorDocument->views().first();
                QAction *completionAction = view->action("tools_invoke_code_completion");
                if ( completionAction ) {
                    completionShortcut = completionAction->shortcut().toString(
                                             QKeySequence::NativeText );
                }
            }
            if ( completionShortcut.isEmpty() ) {
                completionShortcut = "unknown"; // Should not happen
            }

            completionModel = new JavaScriptCompletionModel( completionShortcut, this );
            iface->registerCompletionModel( completionModel );
        }

        KTextEditor::View *accessorView = m_accessorDocument->views().first();
        KTextEditor::View *scriptView = m_scriptDocument->views().first();
        connect( accessorView, SIGNAL(informationMessage(KTextEditor::View*,QString)),
                 this, SLOT(informationMessage(KTextEditor::View*,QString)) );
        connect( scriptView, SIGNAL(informationMessage(KTextEditor::View*,QString)),
                 this, SLOT(informationMessage(KTextEditor::View*,QString)) );
    } else {
        // if we couldn't find our Part, we exit since the Shell by
        // itself can't do anything useful
        KMessageBox::error(this, "service katepart.desktop not found");
        qApp->quit();
        // we return here, cause qApp->quit() only means "exit the
        // next time we enter the event loop...
        return;
    }

    // Add parts
    m_partManager->addPart( m_accessorDocument, false );
    m_partManager->addPart( m_scriptDocument, false );
//     m_partManager->addPart( m_webview, false );

    // Create script widgets/models
    m_functions = new KComboBox( this );
    m_javaScriptModel = new JavaScriptModel( this );
    m_functionsModel = new QSortFilterProxyModel( this );
    m_functionsModel->setSourceModel( m_javaScriptModel );
    m_functionsModel->setFilterRole( Qt::UserRole ); // TODO
    m_functionsModel->setFilterFixedString( QString::number(Function) );
    m_functions->setModel( m_functionsModel );
    connect( m_javaScriptModel, SIGNAL(showTextHint(KTextEditor::Cursor,QString&)),
             this, SLOT(showTextHint(KTextEditor::Cursor,QString&)) );

    KTextEditor::TextHintInterface *iface =
        qobject_cast<KTextEditor::TextHintInterface*>( m_scriptDocument->activeView() );
    if ( iface ) {
        iface->enableTextHints( 250 );
        connect( m_scriptDocument->activeView(), SIGNAL(needTextHint(KTextEditor::Cursor,QString&)),
                 m_javaScriptModel, SLOT(needTextHint(KTextEditor::Cursor,QString&)) );
        m_javaScriptModel->setJavaScriptCompletionModel( completionModel );
    }

    QWidget *scriptTab = new QWidget( this );
    QVBoxLayout *layoutScript = new QVBoxLayout( scriptTab );
    QToolButton *btnPreviousFunction = new QToolButton( scriptTab );
    btnPreviousFunction->setDefaultAction( action("script_previous_function") );
    QToolButton *btnNextFunction = new QToolButton( scriptTab );
    btnNextFunction->setDefaultAction( action("script_next_function") );
    QHBoxLayout *layoutScriptTop = new QHBoxLayout();
    layoutScriptTop->setSpacing( 0 );
    layoutScriptTop->addWidget( btnPreviousFunction );
    layoutScriptTop->addWidget( btnNextFunction );
    layoutScriptTop->addWidget( m_functions );

    layoutScript->addLayout( layoutScriptTop );
    layoutScript->addWidget( scriptWidget );
    connect( m_functions, SIGNAL(currentIndexChanged(int)),
             this, SLOT(currentFunctionChanged(int)) );
    connect( m_scriptDocument->views().first(),
             SIGNAL(cursorPositionChanged(KTextEditor::View*,KTextEditor::Cursor)),
             this, SLOT(scriptCursorPositionChanged(KTextEditor::View*,KTextEditor::Cursor)) );

    // Add tabs
    m_mainTabBar->addTab( m_view, KIcon("public-transport-stop"),
                          i18nc("@title:tab", "&Accessor") );
    m_mainTabBar->addTab( accessorSourceWidget, KIcon("text-xml"),
                          i18nc("@title:tab", "A&ccessor Source") );
    m_mainTabBar->addTab( scriptTab, // The icon gets automatically set to the mime type of the script
                          i18nc("@title:tab", "&Script") );
    m_mainTabBar->addTab( m_preview, KIcon("plasma"), i18nc("@title:tab", "&Preview") );
    m_mainTabBar->addTab( webWidget, KIcon("applications-internet"),
                          i18nc("@title:tab", "&Web View") );

    m_mainTabBar->setTabEnabled( ScriptTab, false ); // Disable script tab

//     loadTemplate();
    writeScriptTemplate();

    // This creates an XML document in the Accessor Source tab.
    // First mark the Accessor tab changed and then sync with the Accessor Source tab.
    m_accessorWidgetsChanged = true;
    syncAccessor();

    // Accessor isn't modified (therefore file_save is disabled),
    // but it's also not saved, so enable file_save.
    action("file_save")->setEnabled( true );
}

TimetableMate::~TimetableMate() {
    m_recentFilesAction->saveEntries( Settings::self()->config()->group(0) );
    delete m_script;

    if ( !m_engine ) {
        return;
    }
    m_scriptNetwork->abortAllRequests();
    m_engine->abortEvaluation();
    m_engine->deleteLater();
}

QStringList TimetableMate::allowedExtensions()
{
    return QStringList() << "kross" << "qt" << "qt.core" << "qt.xml";
}

bool TimetableMate::lazyLoadScript( const TimetableAccessorInfo *info )
{
    if ( m_script ) {
        return true;
    }

    // Read script
    QFile scriptFile( info->scriptFileName() );
    if ( !scriptFile.open(QIODevice::ReadOnly) ) {
        kDebug() << "Script could not be opened for reading"
                 << info->scriptFileName() << scriptFile.errorString();
        return false;
    }
    QTextStream stream( &scriptFile );
    QString scriptContents = stream.readAll();
    scriptFile.close();

    // Initialize the script
    m_script = new QScriptProgram( scriptContents, info->scriptFileName() );

    return true;
}

void TimetableMate::closeEvent( QCloseEvent *event ) {
    if ( m_changed ) {
        int result;
        if ( m_openedPath.isEmpty() ) {
            result = KMessageBox::warningYesNoCancel( this, i18nc("@info/plain",
                     "The accessor <resource>%1</resource> has been changed.<nl/>"
                     "Do you want to save or discard the changes?",
                     m_currentServiceProviderID),
                     i18nc("@title:window", "Close Document"),
                     KStandardGuiItem::save(), KStandardGuiItem::discard() );
        } else {
            result = KMessageBox::warningYesNoCancel( this, i18nc("@info/plain",
                     "The accessor <resource>%1</resource> in <filename>%2</filename> "
                     "has been changed.<nl/>Do you want to save or discard the changes?",
                     m_currentServiceProviderID, m_openedPath),
                     i18nc("@title:window", "Close Document"),
                     KStandardGuiItem::save(), KStandardGuiItem::discard() );
        }
        if ( result == KMessageBox::Yes ) {
            // Save current document
            fileSave();
        } else if ( result == KMessageBox::Cancel ) {
            // Cancel closing
            event->setAccepted( false );
        }
    }
}

void TimetableMate::setChanged( bool changed ) {
    if ( m_changed == changed )
        return;

    m_changed = changed;
    action("file_save")->setEnabled( m_changed );
    if ( !changed ) {
        m_accessorDocument->setModified( false );
        m_scriptDocument->setModified( false );
    }

    updateWindowTitle();
}

void TimetableMate::showTextHint( const KTextEditor::Cursor &position, QString &text ) {
    QPoint pointInView = m_scriptDocument->activeView()->cursorToCoordinate( position );
    QPoint pointGlobal = m_scriptDocument->activeView()->mapToGlobal( pointInView );
    QToolTip::showText( pointGlobal, text );
}

void TimetableMate::updateWindowTitle() {
    QString currentTab = KGlobal::locale()->removeAcceleratorMarker(
                             m_mainTabBar->tabText(m_mainTabBar->currentIndex()) );
    if ( m_currentServiceProviderID.isEmpty() )
        setCaption( currentTab, m_changed );
    else
        setCaption( currentTab + " - " + m_currentServiceProviderID, m_changed );

    // Set preview tab disabled when the accessor isn't saved
    m_mainTabBar->setTabEnabled( PlasmaPreviewTab, !m_openedPath.isEmpty() );
    if ( m_mainTabBar->currentIndex() == PlasmaPreviewTab
            && !m_mainTabBar->isTabEnabled(PlasmaPreviewTab) )
    {
        m_mainTabBar->setCurrentIndex( AccessorTab );
    }
}

void TimetableMate::writeScriptTemplate() {
    // Get the template interface
    KTextEditor::View *scriptView = m_scriptDocument->views().first();
    KTextEditor::TemplateInterface *templateInterface =
        qobject_cast<KTextEditor::TemplateInterface*>( scriptView );
    if ( templateInterface ) {
        // Insert a template with author information
        templateInterface->insertTemplateText( KTextEditor::Cursor(),
                                               QString::fromUtf8("/** Accessor for ${Service Provider}\n"
                                                                 "  * © ${year}, ${Author} */\n\n"
                                                                 "// TODO: Implement parsing functions, use syntax completion\n"
                                                                 "${cursor}"),
                                               QMap<QString, QString>() );

        setChanged( false );
    }
}

void TimetableMate::activePartChanged( KParts::Part *part ) {
    createGUI( part );

    if ( part ) {
        // Manually hide actions of the part
        QStringList actionsToHide;
        actionsToHide << "file_save" << "file_save_as" << "tools_mode"
        << "tools_highlighting" << "tools_indentation";
        foreach ( QAction *action, menuBar()->actions() ) {
            KActionMenu *menuAction = static_cast<KActionMenu*>( action );
            for ( int i = menuAction->menu()->actions().count() - 1; i >= 0; --i ) {
                QAction *curAction = menuAction->menu()->actions().at( i );
                if ( curAction->parent() == actionCollection() )
                    continue; // Don't hide own actions

                if ( actionsToHide.contains(curAction->objectName()) ) {
                    curAction->setVisible( false );

                    actionsToHide.removeAt( i );
                    if ( actionsToHide.isEmpty() )
                        break;
                }
            }

            if ( actionsToHide.isEmpty() )
                break;
        }
    }
}

void TimetableMate::informationMessage( KTextEditor::View*, const QString &message ) {
    statusBar()->showMessage( message );
}

void TimetableMate::accessorWidgetsChanged() {
    m_accessorWidgetsChanged = true;
    setChanged( true );

    TimetableAccessor *accessor = m_view->accessorInfo();

    // Enable/disable actions to open web pages
    action( "web_load_homepage" )->setEnabled( !accessor->info()->url().isEmpty() );
//     action( "web_load_departures" )->setEnabled( !accessor.rawDepartureUrl.isEmpty() ); // TODO
//     action( "web_load_stopsuggestions" )->setEnabled( !accessor.rawStopSuggestionsUrl.isEmpty() );
//     action( "web_load_journeys" )->setEnabled( !accessor.rawJourneyUrl.isEmpty() );

    QStringList functions = m_javaScriptModel->functionNames();
    action( "script_runParseTimetable" )->setEnabled(
            functions.contains(SCRIPT_FUNCTION_GETTIMETABLE) );
    action( "script_runParseStopSuggestions" )->setEnabled(
            functions.contains(SCRIPT_FUNCTION_GETSTOPSUGGESTIONS) );
    action( "script_runParseJourneys" )->setEnabled(
            functions.contains(SCRIPT_FUNCTION_GETJOURNEYS) );
}

void TimetableMate::accessorDocumentChanged( KTextEditor::Document */*document*/ ) {
    m_accessorDocumentChanged = true;
    setChanged( true );
}

void TimetableMate::updateNextPreviousFunctionActions() {
    int count = m_functionsModel->rowCount();
    int functionIndex = m_functions->currentIndex();
    if ( functionIndex == -1 ) {
        int currentLine = m_scriptDocument->activeView()->cursorPosition().line();
        FunctionNode *previousNode = dynamic_cast<FunctionNode*>(
                                         m_javaScriptModel->nodeBeforeLineNumber(currentLine, Function) );
        FunctionNode *nextNode = dynamic_cast<FunctionNode*>(
                                     m_javaScriptModel->nodeAfterLineNumber(currentLine, Function) );
        action("script_previous_function")->setEnabled( previousNode );
        action("script_next_function")->setEnabled( nextNode );
    } else {
        action("script_previous_function")->setEnabled( count > 1 && functionIndex > 0 );
        action("script_next_function")->setEnabled( count > 1 && functionIndex != count - 1 );
    }
}

void TimetableMate::beginScriptParsing() {
    delete m_backgroundParserTimer;
    m_backgroundParserTimer = NULL;

    // Parse the script
    JavaScriptParser parser( m_scriptDocument->text() );

    KTextEditor::MarkInterface *iface =
        qobject_cast<KTextEditor::MarkInterface*>( m_scriptDocument );
    if ( iface )
        iface->clearMarks();

//     m_scriptDocument->activeView()->mar
    if ( parser.hasError() ) {
        if ( iface ) {
            iface->addMark( parser.errorLine() - 1, KTextEditor::MarkInterface::Error );
            if ( parser.errorAffectedLine() != -1 ) {
                iface->addMark( parser.errorAffectedLine() - 1,
                                KTextEditor::MarkInterface::Warning );
            }
        }

// 	m_scriptDocument->views().first()->setCursorPosition( parser.errorCursor() );
        statusBar()->showMessage( i18nc("@info:status", "Syntax error in line %1, column %2: "
                                        "<message>%3</message>",
                                        parser.errorLine(), parser.errorColumn(),
                                        parser.errorMessage()), 10000 );
    } else {
        statusBar()->showMessage( i18nc("@info:status", "No syntax errors found."), 5000 );
    }

    // Update the model with the parsed nodes
    bool wasBlocked = m_functions->blockSignals( true );
    m_javaScriptModel->setNodes( parser.nodes() );
    m_functions->blockSignals( wasBlocked );

    // Update selected function in the function combobox
    scriptCursorPositionChanged( m_scriptDocument->views().first(),
                                 m_scriptDocument->views().first()->cursorPosition() );
    // Update next/previous function actions enabled state
    updateNextPreviousFunctionActions();

    // Update script_run* action enabled state
    QStringList functions = m_javaScriptModel->functionNames();
    action( "script_runParseTimetable" )->setEnabled(
            functions.contains(SCRIPT_FUNCTION_GETTIMETABLE) );
    action( "script_runParseStopSuggestions" )->setEnabled(
            functions.contains(SCRIPT_FUNCTION_GETSTOPSUGGESTIONS) );
    action( "script_runParseJourneys" )->setEnabled(
            functions.contains(SCRIPT_FUNCTION_GETJOURNEYS) );
}

void TimetableMate::scriptDocumentChanged( KTextEditor::Document */*document*/ ) {
    setChanged( true );

    if ( !m_backgroundParserTimer ) {
        m_backgroundParserTimer = new QTimer( this );
        m_backgroundParserTimer->setSingleShot( true );
        connect( m_backgroundParserTimer, SIGNAL(timeout()), this, SLOT(beginScriptParsing()) );
    }

    // Begin parsing after delay
    m_backgroundParserTimer->start( 500 );
}

void TimetableMate::syncAccessor() {
    bool wasChanged = m_changed;

    if ( m_accessorDocumentChanged ) {
        QTextCodec *codec = QTextCodec::codecForName( m_accessorDocument->encoding().isEmpty()
                            ? "UTF-8" : m_accessorDocument->encoding().toLatin1() );
        QByteArray ba = codec->fromUnicode( m_accessorDocument->text() );
        setAccessorValues( &ba );
        m_accessorDocumentChanged = false;
        m_accessorWidgetsChanged = false;
    } else if ( m_accessorWidgetsChanged ) {
        m_accessorDocument->setText( m_view->writeAccessorInfoXml() );
        m_accessorDocument->setModified( false );
        m_accessorDocumentChanged = false;
        m_accessorWidgetsChanged = false;
    }

    if ( !wasChanged && m_changed )
        setChanged( false );
}

void TimetableMate::showScriptTab( bool loadTemplateIfEmpty ) {
    m_mainTabBar->setCurrentIndex( ScriptTab ); // go to script tab

    if ( loadTemplateIfEmpty && m_scriptDocument->isEmpty() )
        writeScriptTemplate();
}

void TimetableMate::showWebTab( const QString &url ) {
    if ( !url.isEmpty() ) {
        KUrl kurl( url );
        if ( kurl.isEmpty() )
            return;

        m_webview->setUrl( kurl );
    }
    m_mainTabBar->setCurrentIndex( WebTab ); // go to web tab
}

void TimetableMate::currentTabChanged( int index ) {
    // Clear status bar messages
    statusBar()->showMessage( QString() );

    // When leaving the "Accessor Source" tab with changes,
    // reload accessor values into the widgets in the "Accessor" tab
    syncAccessor();
//     if ( m_currentTab == AccessorSourceTab && m_accessorDocumentChanged ) {
// 	QTextCodec *codec = QTextCodec::codecForName( m_accessorDocument->encoding().isEmpty()
// 		? "UTF-8" : m_accessorDocument->encoding().toLatin1() );
// 	QByteArray ba = codec->fromUnicode( m_accessorDocument->text() );
// 	setAccessorValues( &ba );
// 	m_accessorDocumentChanged = false;
// 	m_accessorWidgetsChanged = false;
//     }
//     // When leaving the "Accessor" tab with changes,
//     // recreate accessor source and set it in the "Accessor Source" tab
//     else if ( m_currentTab == AccessorTab && m_accessorWidgetsChanged ) {
// 	m_accessorDocument->setText( m_view->writeAccessorInfoXml() );
// 	m_accessorDocumentChanged = false;
// 	m_accessorWidgetsChanged = false;
//     }

    // Don't flicker while changing the active part
    setUpdatesEnabled( false );

    if ( index == AccessorSourceTab ) { // go to accessor source tab
        m_partManager->setActivePart( m_accessorDocument, m_mainTabBar );
        m_accessorDocument->activeView()->setFocus();
    } else if ( index == ScriptTab ) { // go to script tab
        m_partManager->setActivePart( m_scriptDocument, m_mainTabBar );
        m_scriptDocument->activeView()->setFocus();
    } else if ( index == WebTab ) { // go to web tab
//         m_partManager->setActivePart( m_webview, m_mainTabBar );
        m_urlBar->setFocus();
    } else {
        m_partManager->setActivePart( 0 );
    }

    if ( m_currentTab == PlasmaPreviewTab ) { // left plasma preview tab
        m_preview->closePlasmaPreview();
    } else if ( m_currentTab == ScriptTab ) { // left script tab
        if ( statusBar()->hasItem(1) )
            statusBar()->removeItem( 1 );
    }

    if ( m_currentTab == ScriptTab ) { // left script tab
        action("script_next_function")->setVisible( false );
        action("script_previous_function")->setVisible( false );
    } else if ( index == ScriptTab ) { // go to script tab
        action("script_next_function")->setVisible( true );
        action("script_previous_function")->setVisible( true );
    }

    if ( m_currentTab == WebTab ) { // left web tab
        action("web_back")->setVisible( false ); // TODO: Make (in)visible with xmlgui-states
        action("web_forward")->setVisible( false );
        action("web_stop")->setVisible( false );
        action("web_reload")->setVisible( false );
    } else if ( index == WebTab ) { // go to web tab
        action("web_back")->setVisible( true );
        action("web_forward")->setVisible( true );
        action("web_stop")->setVisible( true );
        action("web_reload")->setVisible( true );
    }

    // Update caption
    updateWindowTitle();

    // Reset updates
    setUpdatesEnabled( true );

    // Store last tab
    m_currentTab = index;
}

void TimetableMate::setupActions() {
    KStandardAction::openNew( this, SLOT(fileNew()), actionCollection() );
    KStandardAction::open( this, SLOT(fileOpen()), actionCollection() );
    KStandardAction::save( this, SLOT(fileSave()), actionCollection() );
    KStandardAction::saveAs( this, SLOT(fileSaveAs()), actionCollection() );
    KStandardAction::quit( qApp, SLOT(closeAllWindows()), actionCollection() );
    KStandardAction::preferences( this, SLOT(optionsPreferences()), actionCollection() );
    m_recentFilesAction = KStandardAction::openRecent( this, SLOT(open(KUrl)), actionCollection() );
    m_recentFilesAction->loadEntries( Settings::self()->config()->group(0) );

    KAction *openInstalled = new KAction( KIcon("document-open"), i18nc("@action",
                                          "Open I&nstalled..."), this );
    actionCollection()->addAction( QLatin1String("file_open_installed"), openInstalled );
    connect( openInstalled, SIGNAL(triggered(bool)), this, SLOT(fileOpenInstalled()) );

    KAction *install = new KAction( KIcon("run-build-install"), i18nc("@action", "&Install"), this );
    actionCollection()->addAction( QLatin1String("file_install"), install );
    connect( install, SIGNAL(triggered(bool)), this, SLOT(install()) );

    KAction *installGlobal = new KAction( KIcon("run-build-install-root"),
                                          i18nc("@action", "Install &Globally"), this );
    actionCollection()->addAction( QLatin1String("file_install_global"), installGlobal );
//     installGlobal->setEnabled( false ); // TODO Enable only if a valid xml with script is opened
    connect( installGlobal, SIGNAL(triggered(bool)), this, SLOT(installGlobal()) );

    QAction *webBack = m_webview->pageAction( QWebPage::Back );
    webBack->setVisible( false );
    actionCollection()->addAction( QLatin1String("web_back"), webBack );

    QAction *webForward = m_webview->pageAction( QWebPage::Forward );
    webForward->setVisible( false );
    actionCollection()->addAction( QLatin1String("web_forward"), webForward );

    QAction *webStop = m_webview->pageAction( QWebPage::Stop );
    webStop->setVisible( false );
    actionCollection()->addAction( QLatin1String("web_stop"), webStop );

    QAction *webReload = m_webview->pageAction( QWebPage::Reload );
    webReload->setVisible( false );
    actionCollection()->addAction( QLatin1String("web_reload"), webReload );

    QAction *webLoadHomePage = new KAction( KIcon("document-open-remote"),
                                            i18nc("@action", "Open &Provider Home Page"), this );
    webLoadHomePage->setToolTip( i18nc("@info:tooltip",
                                       "Opens the <emphasis>home page</emphasis> of the service provider.") );
    webLoadHomePage->setEnabled( false );
    actionCollection()->addAction( QLatin1String("web_load_homepage"), webLoadHomePage );
    connect( webLoadHomePage, SIGNAL(triggered(bool)), this, SLOT(webLoadHomePage()) );

    QAction *webLoadDepartures = new KAction( KIcon("document-open-remote"),
            i18nc("@action", "Open &Departures Page"), this );
    webLoadDepartures->setToolTip( i18nc("@info:tooltip",
                                         "Opens the <emphasis>departures</emphasis> web page.") );
    webLoadDepartures->setEnabled( false );
    actionCollection()->addAction( QLatin1String("web_load_departures"), webLoadDepartures );
    connect( webLoadDepartures, SIGNAL(triggered(bool)), this, SLOT(webLoadDepartures()) );

    QAction *webLoadStopSuggestions = new KAction( KIcon("document-open-remote"),
            i18nc("@action", "Open &Stop Suggestions Page"), this );
    webLoadStopSuggestions->setToolTip( i18nc("@info:tooltip",
                                        "Opens the <emphasis>stop suggestions</emphasis> web page.") );
    webLoadStopSuggestions->setEnabled( false );
    actionCollection()->addAction( QLatin1String("web_load_stopsuggestions"), webLoadStopSuggestions );
    connect( webLoadStopSuggestions, SIGNAL(triggered(bool)), this, SLOT(webLoadStopSuggestions()) );

    QAction *webLoadJourneys = new KAction( KIcon("document-open-remote"),
                                            i18nc("@action", "Open &Journeys Page"), this );
    webLoadJourneys->setToolTip( i18nc("@info:tooltip",
                                       "Opens the <emphasis>journeys</emphasis> web page.") );
    webLoadJourneys->setEnabled( false );
    actionCollection()->addAction( QLatin1String("web_load_journeys"), webLoadJourneys );
    connect( webLoadJourneys, SIGNAL(triggered(bool)), this, SLOT(webLoadJourneys()) );

    KActionMenu *webLoadPage = new KActionMenu( KIcon("document-open-remote"),
            i18nc("@action", "Open &Page"), this );
    webLoadPage->setToolTip( i18nc("@info:tooltip",
                                   "Opens a web page defined in the <interface>Accessor</interface> tab.") );
    webLoadPage->setDelayed( false );
    webLoadPage->addAction( webLoadHomePage );
    webLoadPage->addSeparator();
    webLoadPage->addAction( webLoadDepartures );
    webLoadPage->addAction( webLoadStopSuggestions );
    webLoadPage->addAction( webLoadJourneys );
    actionCollection()->addAction( QLatin1String("web_load_page"), webLoadPage );

    QAction *runScriptTimetable = new KAction( KIcon("system-run"),
            i18nc("@action", "Run 'parse&Timetable'"), this );
    runScriptTimetable->setToolTip( i18nc("@info:tooltip",
                                          "Runs the <emphasis>parseTimetable()</emphasis> function of the script.") );
    actionCollection()->addAction( QLatin1String("script_runParseTimetable"), runScriptTimetable );
    connect( runScriptTimetable, SIGNAL(triggered(bool)), this, SLOT(scriptRunParseTimetable()) );

    QAction *runScriptStopSuggestions = new KAction( KIcon("system-run"),
            i18nc("@action", "Run 'parse&StopSuggestions'"), this );
    runScriptStopSuggestions->setToolTip( i18nc("@info:tooltip",
                                          "Runs the <emphasis>parseStopSuggestions()</emphasis> function of the script.") );
    actionCollection()->addAction( QLatin1String("script_runParseStopSuggestions"),
                                   runScriptStopSuggestions );
    connect( runScriptStopSuggestions, SIGNAL(triggered(bool)),
             this, SLOT(scriptRunParseStopSuggestions()) );

    QAction *runScriptJourneys = new KAction( KIcon("system-run"),
            i18nc("@action", "Run 'parse&Journeys'"), this );
    runScriptJourneys->setToolTip( i18nc("@info:tooltip",
                                         "Runs the <emphasis>parseJourneys()</emphasis> function of the script.") );
    actionCollection()->addAction( QLatin1String("script_runParseJourneys"), runScriptJourneys );
    connect( runScriptJourneys, SIGNAL(triggered(bool)), this, SLOT(scriptRunParseJourneys()) );

    KActionMenu *runScript = new KActionMenu( KIcon("system-run"),
            i18nc("@action", "&Run Script"), this );
    runScript->setToolTip( i18nc("@info:tooltip", "Runs a function of the script.") );
    runScript->setDelayed( false );
    runScript->addAction( runScriptTimetable );
    runScript->addAction( runScriptStopSuggestions );
    runScript->addAction( runScriptJourneys );
    actionCollection()->addAction( QLatin1String("script_run"), runScript );

    QAction *toolsCheck = new KAction( KIcon("dialog-ok-apply"), i18nc("@action", "&Check"), this );
    toolsCheck->setToolTip( i18nc("@info:tooltip", "Checks the accessor for error/features.") );
    actionCollection()->addAction( QLatin1String("tools_check"), toolsCheck );
    connect( toolsCheck, SIGNAL(triggered(bool)), this, SLOT(toolsCheck()) );

    KAction *scriptPreviousFunction = new KAction( KIcon("go-previous"), // no icon for this so far
            i18nc("@action", "&Previous Function"), this );
    scriptPreviousFunction->setToolTip( i18nc("@info:tooltip", "Selects the previous function.") );
    scriptPreviousFunction->setVisible( false );
    scriptPreviousFunction->setShortcut( KShortcut("Ctrl+Alt+PgUp") ); // Same as in KDevelop
    actionCollection()->addAction( QLatin1String("script_previous_function"),
                                   scriptPreviousFunction );
    connect( scriptPreviousFunction, SIGNAL(triggered(bool)),
             this, SLOT(scriptPreviousFunction()) );

    KAction *scriptNextFunction = new KAction( KIcon("go-next"), // no icon for this so far
            i18nc("@action", "&Next Function"), this );
    scriptNextFunction->setToolTip( i18nc("@info:tooltip", "Selects the next function.") );
    scriptNextFunction->setVisible( false );
    scriptNextFunction->setShortcut( KShortcut("Ctrl+Alt+PgDown") ); // Same as in KDevelop
    actionCollection()->addAction( QLatin1String("script_next_function"), scriptNextFunction );
    connect( scriptNextFunction, SIGNAL(triggered(bool)), this, SLOT(scriptNextFunction()) );
}

void TimetableMate::currentFunctionChanged( int index ) {
    QModelIndex functionIndex = m_functionsModel->index( index, 0 );
    CodeNode *node = m_javaScriptModel->nodeFromIndex( m_functionsModel->mapToSource(functionIndex) );
    FunctionNode *function = dynamic_cast<FunctionNode*>( node );
    if ( function ) {
        KTextEditor::View *view = m_scriptDocument->activeView();
        view->blockSignals( true );
        view->setCursorPosition( KTextEditor::Cursor(function->line() - 1, 0) );
        view->blockSignals( false );
    }

    updateNextPreviousFunctionActions();
}

void TimetableMate::scriptCursorPositionChanged( KTextEditor::View* view,
        const KTextEditor::Cursor& cursor ) {
    Q_UNUSED( view );
    bool wasBlocked = m_functions->blockSignals( true );
    CodeNode *node = m_javaScriptModel->nodeFromLineNumber( cursor.line() + 1 );
    if ( node ) {
        QModelIndex index = m_javaScriptModel->indexFromNode( node );
        QModelIndex functionIndex = m_functionsModel->mapFromSource( index );
        m_functions->setCurrentIndex( functionIndex.row() );
        updateNextPreviousFunctionActions();
    }
    m_functions->blockSignals( wasBlocked );

    QString posInfo = i18nc("@info:status", "Line: %1 Col: %2",
                            cursor.line() + 1, cursor.column() + 1);
    if ( statusBar()->hasItem(1) ) {
        statusBar()->changeItem( posInfo, 1 );
    } else {
        statusBar()->insertPermanentItem( posInfo, 1 );
    }
}

void TimetableMate::fileNew() {
    // This slot is called whenever the File->New menu is selected,
    // the New shortcut is pressed (usually CTRL+N) or the New toolbar
    // button is clicked

    // Create a new window
    (new TimetableMate)->show();
}

void TimetableMate::open( const KUrl &url ) {
    KUrl openedUrl( m_openedPath + '/' + m_currentServiceProviderID + ".xml" );
    if ( url.equals(openedUrl, KUrl::CompareWithoutTrailingSlash) ) {
        kDebug() << "The file" << m_openedPath << "was already opened";
        return;
    }

    if ( !m_openedPath.isEmpty() || m_changed ) {
        TimetableMate *newWindow = new TimetableMate;
        newWindow->open( url );
        newWindow->show();
    } else
        loadAccessor( url.path() );
}

void TimetableMate::fileOpen() {
    QString fileName = KFileDialog::getOpenFileName( KGlobalSettings::documentPath(),
                       "??*_*.xml", this, i18nc("@title:window", "Open Accessor") );
    if ( fileName.isNull() )
        return; // Cancel clicked

    open( KUrl(fileName) );
}

void TimetableMate::fileOpenInstalled() {
    // Get a list of all script files in the directory of the XML file
    QStringList accessorFiles = KGlobal::dirs()->findAllResources( "data",
                                "plasma_engine_publictransport/accessorInfos/*.xml" );
    if ( accessorFiles.isEmpty() ) {
        KMessageBox::information( this, i18nc("@info/plain", "There are no installed "
                                              "timetable accessors. You need to install the PublicTransport data engine.") );
        return;
    }

    // Make filenames more pretty and create a hash to map from the pretty names to the full paths
    QHash< QString, QString > map;
    for ( QStringList::iterator it = accessorFiles.begin(); it != accessorFiles.end(); ++it ) {
        QString prettyName;
        if ( KStandardDirs::checkAccess(*it, W_OK) ) {
            // File is writable, ie. locally installed
            prettyName = KUrl( *it ).fileName();
        } else {
            // File isn't writable, ie. globally installed
            prettyName = i18nc( "@info/plain This string is displayed instead of the full path for "
                                "globally installed timetable accessor xmls.",
                                "Global: %1", KUrl(*it).fileName() );
        }

        map.insert( prettyName, *it );
        *it = prettyName;
    }

    bool ok;
    QString selectedPrettyName = KInputDialog::getItem( i18nc("@title:window",
                                 "Open Installed Accessor"), i18nc("@info", "Installed timetable accessor"),
                                 accessorFiles, 0, false, &ok, this );
    if ( ok ) {
        QString selectedFilePath = map[ selectedPrettyName ];
        loadAccessor( selectedFilePath );
    }
}

void TimetableMate::fileSave() {
    if ( m_openedPath.isEmpty() ) {
        fileSaveAs();
    } else {
        syncAccessor();
        m_accessorDocument->documentSave();
        m_scriptDocument->documentSave();
        setChanged( false );
    }
}

void TimetableMate::fileSaveAs() {
    const TimetableAccessorInfo *info = m_view->accessorInfo()->info();
    QString fileName = KFileDialog::getSaveFileName(
                           m_openedPath.isEmpty() ? KGlobalSettings::documentPath() : m_openedPath,
                           "??*_*.xml", this, i18nc("@title:window", "Save Accessor") );
    if ( fileName.isNull() )
        return; // Cancel clicked

    KUrl url( fileName );
    m_currentServiceProviderID = url.fileName().left( url.fileName().lastIndexOf('.') );
    m_view->setCurrentServiceProviderID( m_currentServiceProviderID );
    m_openedPath = url.directory();
    syncAccessor();
    m_view->writeAccessorInfoXml( fileName );

    QString scriptFile = info->scriptFileName();
    if ( !scriptFile.isEmpty() ) {
        QString scriptFilePath = m_openedPath + '/' + scriptFile;
        if ( !m_scriptDocument->saveAs(scriptFilePath) ) {
            KMessageBox::information( this, i18nc("@info", "Could not write the script file to "
                                                  "<filename>%1</filename>.", scriptFilePath) );
        }
    }

    setChanged( false );
}

void TimetableMate::install() {
    const TimetableAccessorInfo *info = m_view->accessorInfo()->info();
    QString saveDir = KGlobal::dirs()->saveLocation( "data",
                      "plasma_engine_publictransport/accessorInfos/" );
    KUrl urlXml = KUrl( saveDir + m_currentServiceProviderID + ".xml" );
    KUrl urlScript = KUrl( saveDir + info->scriptFileName() ); // TODO .py, .rb

    syncAccessor();
    bool ok = false;
    ok = m_accessorDocument->saveAs( urlXml );
    ok = ok && m_scriptDocument->saveAs( urlScript );

    if ( ok ) {
        // Installation successful
        statusBar()->showMessage( i18nc("@info:status",
                                        "Accessor successfully installed locally"), 5000 );
    } else {
        KMessageBox::error( this, i18nc("@info", "Accessor could not be installed locally. Tried "
                                        "to save these files:<nl/>  %1<nl/>  %2",
                                        urlXml.prettyUrl(), urlScript.prettyUrl()) );
    }
}

void TimetableMate::installGlobal() {
    QStringList saveDirs = KGlobal::dirs()->findDirs( "data",
                           "plasma_engine_publictransport/accessorInfos/" );
    if ( saveDirs.isEmpty() ) {
        kDebug() << "No save directory found. Is the PublicTransport data engine installed?";
        return;
    }
    QString saveDir = saveDirs.last(); // Use the most global one
    syncAccessor();
    const TimetableAccessorInfo *info = m_view->accessorInfo()->info();

    KAuth::Action action( "org.kde.timetablemate.install" );
    action.setHelperID( "org.kde.timetablemate" );
    QVariantMap args;
    args["path"] = saveDir;
    args["filenameAccessor"] = m_currentServiceProviderID + ".xml";
    args["filenameScript"] = info->scriptFileName();
    args["contentsAccessor"] = m_accessorDocument->text();
    args["contentsScript"] = m_scriptDocument->text();
    action.setArguments( args );
    KAuth::ActionReply reply = action.execute();
    if ( reply.failed() ) {
        kDebug() << reply.type() << reply.data();
        kDebug() << reply.errorCode() << reply.errorDescription();
        if ( reply.type() == KAuth::ActionReply::HelperError ) {
            KMessageBox::error( this, i18nc("@info", "Accessor could nt be installed globally: "
                                            "%1 <message>%2</message>", reply.errorCode(), reply.errorDescription()) );
        } else {
            switch ( reply.errorCode() ) {
            case KAuth::ActionReply::UserCancelled:
            case KAuth::ActionReply::AuthorizationDenied:
// 		    UserCancelled is also AuthorizationDenied on X11 currently (PolicyKit limitation)
                // Do nothing
// 		    KMessageBox::error( this, i18nc("@info", "Authentication denied: "
// 			    "<message>%1</message>", reply.errorDescription()) );
                break;

            case KAuth::ActionReply::NoSuchAction:
                KMessageBox::error( this, i18nc("@info", "Could not find the authentication "
                                                "action. If you just installed TimetableMate, you might need to "
                                                "restart D-Bus.") );
                break;

            case KAuth::ActionReply::HelperBusy:
                KMessageBox::error( this, i18nc("@info", "The action is currently being "
                                                "performed. Please try again later.") );
                break;

            default:
                KMessageBox::error( this, i18nc("@info", "Unable to authenticate the action: "
                                                "%1 <message>%2</message>", reply.errorCode(), reply.errorDescription()) );
                break;
            }
        }
    } else {
        // Installation successful
        statusBar()->showMessage( i18nc("@info:status",
                                        "Accessor successfully installed globally"), 5000 );
    }
}

bool TimetableMate::loadTemplate( const QString &fileName ) {
    QString _fileName = fileName;
    if ( _fileName.isNull() ) {
        // Get a template file name
        QStringList fileNames = KGlobal::dirs()->findAllResources( "data",
                                QString("timetablemate/templates/*.xml") );
        if ( fileNames.isEmpty() ) {
            kDebug() << "Couldn't find a template";
            return false;
        }

        // Use the first template found
        _fileName = fileNames.first();
    }

    // Read template
    QFile file( _fileName );
    if ( !file.open(QIODevice::ReadOnly) ) {
        kDebug() << "Coulnd't open file" << _fileName;
        return false;
    }
    QByteArray ba = file.readAll();
    file.close();

    // Set template text to the Kate part in the "Accessor Source" tab
    m_accessorDocument->closeUrl( false );
    m_accessorDocument->setText( QString(ba) );
    m_accessorDocument->setModified( false );

    // Set values of widgets in the "Accessor" tab
    QString error;
    if ( !setAccessorValues(&ba, &error) ) {
        KMessageBox::information( this, i18nc("@info", "The XML file <filename>"
                                              "%1</filename> could not be read: <message>%2</message>",
                                              _fileName, error) );
        return false;
    }

    m_accessorDocumentChanged = false;
    m_accessorWidgetsChanged = false;
    m_currentServiceProviderID.clear(); // window title gets updated on setChanged() below
    m_view->setCurrentServiceProviderID( m_currentServiceProviderID );
    m_openedPath.clear();

    // Load script file referenced by the XML
    if ( !loadScriptForCurrentAccessor(QFileInfo(_fileName).path(), false) ) {
        syncAccessor();
        m_view->setScriptFile( QString() );
        syncAccessor();
        setChanged( false );
        return false;
    }

    setChanged( false );
    return true;
}

bool TimetableMate::setAccessorValues( QByteArray *text, QString *error,
                                       const QString &fileName ) {
    // Set values in the Accessor tab, text contains the XML document
    QBuffer buffer( text, this );
    return m_view->readAccessorInfoXml( &buffer, error, fileName );
}

bool TimetableMate::loadAccessor( const QString &fileName ) {
    // Close old files without asking to save them
    m_accessorDocument->closeUrl( false );
    m_scriptDocument->closeUrl( false );

    // Try to open the XML in the Kate part in the "Accessor Source" tab
    KUrl url( fileName );
    if ( !QFile::exists(fileName) ) {
        KMessageBox::information( this, i18nc("@info", "The XML file <filename>"
                                              "%1</filename> could not be found.", fileName) );
        return false;
    }
    if ( !m_accessorDocument->openUrl(url) )
        return false;
    m_accessorDocument->setModified( false );

    // Try to read the XML text into the widgets in the "Accessor" tab
    QString error;
    QTextCodec *codec = QTextCodec::codecForName( m_accessorDocument->encoding().isEmpty()
                        ? "UTF-8" : m_accessorDocument->encoding().toLatin1() );
    QByteArray ba = codec->fromUnicode( m_accessorDocument->text() );
    if ( !setAccessorValues(&ba, &error, fileName) ) {
        KMessageBox::information( this, i18nc("@info", "The XML file <filename>"
                                              "%1</filename> could not be read: <message>%2</message>",
                                              fileName, error) );
        return false;
    }
    m_openedPath = url.directory();

    // Set read only mode of the kate parts if the files aren't writable
    QFile test( url.path() );
    if ( test.open(QIODevice::ReadWrite) ) {
        m_accessorDocument->setReadWrite( true );
        m_scriptDocument->setReadWrite( true );
        test.close();
    } else {
        m_accessorDocument->setReadWrite( false );
        m_scriptDocument->setReadWrite( false );
    }

    m_accessorDocumentChanged = false;
    m_accessorWidgetsChanged = false;
    m_currentServiceProviderID = url.fileName().left( url.fileName().lastIndexOf('.') );
    m_view->setCurrentServiceProviderID( m_currentServiceProviderID );

    // Add to the recently used files action
    m_recentFilesAction->addUrl( url );

    // Load script file referenced by the XML
    if ( !loadScriptForCurrentAccessor(url.directory()) ) {
        // Could not load, eg. script file not found
        syncAccessor();
        m_view->setScriptFile( QString() );
        syncAccessor();
        setChanged( false );
        return false;
    }

    setChanged( false );
    return true;
}

void TimetableMate::scriptFileChanged( const QString &/*scriptFile*/ ) {
    loadScriptForCurrentAccessor( m_openedPath );
}

void TimetableMate::plasmaPreviewLoaded() {
    m_preview->setSettings( m_currentServiceProviderID, QString() );
}

bool TimetableMate::loadScriptForCurrentAccessor( const QString &path, bool openFile ) {
    m_scriptDocument->closeUrl( false );
    m_scriptDocument->setModified( false );
    if ( path.isEmpty() ) {
        kDebug() << "Cannot open script files when the path isn't given. "
                    "Save the accessor XML file first.";
        m_mainTabBar->setTabEnabled( ScriptTab, false ); // Disable script tab
        return false;
    }

    QString text = m_accessorDocument->text();
    QString scriptFile = m_view->accessorInfo()->info()->scriptFileName();
    if ( scriptFile.isEmpty() ) {
        m_mainTabBar->setTabEnabled( ScriptTab, false ); // Disable script tab
        return false;
    } else {
//         scriptFile = path + '/' + scriptFile;
        Q_ASSERT( scriptFile.startsWith(path) );

        if ( openFile ) {
            if ( !QFile::exists(scriptFile) ) {
                KMessageBox::information( this, i18nc("@info", "The script file <filename>"
                                                      "%1</filename> could not be found.", scriptFile) );
                m_mainTabBar->setTabEnabled( ScriptTab, false ); // Disable script tab
                return false;
            }
            if ( !m_scriptDocument->openUrl(KUrl(scriptFile)) ) {
                m_mainTabBar->setTabEnabled( ScriptTab, false ); // Disable script tab
                return false;
            }
        } else {
            QFile file( scriptFile );
            if ( !file.open(QIODevice::ReadOnly) ) {
                kDebug() << "Coulnd't open file" << scriptFile << "read only";
                m_mainTabBar->setTabEnabled( ScriptTab, false ); // Disable script tab
                return false;
            }
            QByteArray ba = file.readAll();
            file.close();
            m_scriptDocument->setText( QString(ba) );
            m_scriptDocument->setModified( false );
        }
    }

    m_mainTabBar->setTabEnabled( ScriptTab, true ); // Enable script tab
    m_mainTabBar->setTabIcon( ScriptTab, KIcon(m_scriptDocument->mimeType().replace('/', '-')) );
    return true;
}

void TimetableMate::optionsPreferences() {
    KMessageBox::information( this, "There are currently no settings... But maybe later ;)" );
    return;
//     TODO
    // The preference dialog is derived from prefs_base.ui
    //
    // compare the names of the widgets in the .ui file
    // to the names of the variables in the .kcfg file
    //avoid to have 2 dialogs shown
    if ( KConfigDialog::showDialog( "settings" ) )  {
        return;
    }
    KConfigDialog *dialog = new KConfigDialog( this, "settings", Settings::self() );
    QWidget *generalSettingsDlg = new QWidget;
    ui_prefs_base.setupUi(generalSettingsDlg);
    dialog->addPage(generalSettingsDlg, i18n("General"), "package_setting");
    connect(dialog, SIGNAL(settingsChanged(QString)), m_view, SLOT(settingsChanged()));
    dialog->setAttribute( Qt::WA_DeleteOnClose );
    dialog->show();
}

void TimetableMate::toolsCheck() {
    const TimetableAccessorInfo *info = m_view->accessorInfo()->info();
    QStringList errors, inelegants, working;

    const bool nameOk = !info->name().isEmpty();
    const bool descriptionOk = !info->description().isEmpty();
    const bool versionOk = !info->version().isEmpty(); // Correct format is validated
    const bool fileVersionOk = info->fileVersion() == "1.0"; // Correct format is validated
    const bool authorOk = !info->author().isEmpty();
    const bool emailOk = !info->email().isEmpty(); // Correct format is validated
    const bool urlOk = !info->url().isEmpty();
    const bool shortUrlOk = !info->shortUrl().isEmpty();
    bool scriptOk = !info->scriptFileName().isEmpty();
    QStringList scriptFunctions;

    if ( !nameOk ) {
        errors << i18nc("@info", "<emphasis>You need to specify a name for your accessor.</emphasis> "
                        "Applets show this name in a service provider selector widget.");
    }
    if ( !descriptionOk ) {
        inelegants << i18nc("@info", "<emphasis>You should give a description for your accessor."
                            "</emphasis> Describe what cities/countries/vehicles are supported and "
                            "what limitations there possibly are when using your accessor.");
    }
    if ( !versionOk ) {
        inelegants << i18nc("@info", "<emphasis>You should specify a version of your accessor."
                            "</emphasis> This helps to distinguish between different versions and "
                            "makes it possible to say for example: \"You need at least version "
                            "1.3 of that accessor for that feature to work\".");
    }
    if ( !fileVersionOk ) {
        errors << i18nc("@info", "<emphasis>The PublicTransport data engine currently "
                        "only supports version '1.0'.</emphasis>");
    }
    if ( !authorOk ) {
        inelegants << i18nc("@info", "<emphasis>You should give your name.</emphasis> "
                            "Applets may want to show the name as display text for email-links, "
                            "the result would be that nothing is shown.");
    }
    if ( !emailOk ) {
        inelegants << i18nc("@info", "<emphasis>You should give your email address.</emphasis> "
                            "You may create a new address if you do not want to use your private "
                            "one. Without an email address, no one can contact you if something "
                            "is wrong with your accessor.");
    }
    if ( !urlOk ) {
        inelegants << i18nc("@info", "<emphasis>You should give the URL to the home page of the "
                            "service provider.</emphasis> "
                            "Since the service providers are running servers for the timetable "
                            "service they will want to get some credit. Applets (should) show "
                            "a link to the home page.");
    }
    if ( !shortUrlOk ) {
        inelegants << i18nc("@info", "<emphasis>You should give a short version of the URL to "
                            "the home page of the service provider.</emphasis> "
                            "Applets may want to show the short URL as display text for the home "
                            "page link, to save space. The result would be that nothing is shown.");
    }

    if ( scriptOk ) {
        // First check the script using the own JavaScriptParser
        JavaScriptParser parser( m_scriptDocument->text() );
        if ( parser.hasError() ) {
            // Go to error line
            m_scriptDocument->activeView()->setCursorPosition(
                KTextEditor::Cursor(parser.errorLine() - 1, parser.errorColumn()) );
            errors << i18nc("@info", "<emphasis>Error in script (line %2, column %3):</emphasis> "
                            "<message>%1</message>", parser.errorMessage(), parser.errorLine(),
                            parser.errorColumn());
            scriptOk = false;
        }

        // Create a Kross::Action instance
        Kross::Action script( this, "TimetableParser" );

        // Set script code and type
        if ( info->scriptFileName().endsWith(QLatin1String(".py")) )
            script.setInterpreter( "python" );
        else if ( info->scriptFileName().endsWith(QLatin1String(".rb")) )
            script.setInterpreter( "ruby" );
        else if ( info->scriptFileName().endsWith(QLatin1String(".js")) )
            script.setInterpreter( "javascript" );
        else {
            const QString scriptType = KInputDialog::getItem(
                                           i18nc("@title:window", "Choose Script Type"),
                                           i18nc("@info", "Script type unknown, please choose one of these:"),
                                           QStringList() << "JavaScript" << "Ruby" << "Python", 0, false, 0, this );
            script.setInterpreter( scriptType.toLower() );
        }
        script.setCode( m_scriptDocument->text().toUtf8() );

        // Test the script
        script.trigger();
        if ( !script.hadError() ) {
            scriptFunctions = script.functionNames();
        } else {
            if ( script.errorLineNo() != -1 ) {
                // Go to error line
                m_scriptDocument->activeView()->setCursorPosition(
                    KTextEditor::Cursor(script.errorLineNo(), 0) );
            }
            errors << i18nc("@info", "<emphasis>Error in script:</emphasis> "
                            "<message>%1</message>", script.errorMessage());
            scriptOk = false;
        }
    } else {
        errors << i18nc("@info", "<emphasis>No script file specified in the "
                        "<interface>Accessor</interface> tab.</emphasis>");
    }

    if ( !scriptFunctions.contains(SCRIPT_FUNCTION_USEDTIMETABLEINFORMATIONS) ) {
        inelegants << i18nc("@info", "<emphasis>You should implement the "
                            "'usedTimetableInformations' script function.</emphasis> This is used "
                            "to get the features supported by the accessor.");
    }
    if ( !scriptFunctions.contains(SCRIPT_FUNCTION_GETTIMETABLE) ) {
        errors << i18nc("@info", "<emphasis>You need to specify a 'getTimetable' script function."
                        "</emphasis> <note>Accessors that only support journeys are currently not "
                        "accepted by the data engine, but that may change</note>.");
    }
    if ( !scriptFunctions.contains(SCRIPT_FUNCTION_GETSTOPSUGGESTIONS) ) {
        inelegants << i18nc("@info", "<emphasis>The script has no 'getStopSuggestions' function, "
                            "that can make it hard to find a correct stop name.</emphasis>");
    } else {
        working << i18nc("@info", "Stop suggestions should work.");
    }
    if ( !scriptFunctions.contains(SCRIPT_FUNCTION_GETJOURNEYS) ) {
        inelegants << i18nc("@info", "<emphasis>The script has no 'getJourneys' function, "
                            "journey functions will not work.</emphasis>");
    } else {
        working << i18nc("@info", "Journeys should work.");
    }

    QString msg;
    if ( errors.isEmpty() && inelegants.isEmpty() ) {
        msg = i18nc("@info", "<para><emphasis>No errors found.</emphasis></para>"
                    "<para>To ensure that your accessor is working correctly do these steps:<list>"
                    "<item>Check the home page link, eg. using <interface>View -> Open Page -> "
                    "Open Home Page</interface></item>"
                    "<item>Run each script function you have implemented using <interface>Tools -> "
                    "Run Script</interface></item>"
                    "<item>Try your accessor using the PublicTransport applet, eg. in the "
                    "<interface>Preview</interface> tab.</item>"
                    "<item>Try different stops, eg. with/without delay information.</item>"
                    "</list></para>");
    } else if ( errors.isEmpty() && !inelegants.isEmpty() ) {
        msg = i18nc("@info", "<para><emphasis>%1 errors found, but nothing severe.</emphasis> "
                    "You should try to fix these: <list>%2</list></para>"
                    "<para>To ensure that your accessor is working correctly do these steps:<list>"
                    "<item>Check the home page link, eg. using <interface>View -> Open Page -> "
                    "Open Home Page</interface></item>"
                    "<item>Run each script function you have implemented using <interface>Tools -> "
                    "Run Script</interface></item>"
                    "<item>Try your accessor using the PublicTransport applet, eg. in the "
                    "<interface>Preview</interface> tab.</item>"
                    "<item>Try different stops, eg. with/without delay information.</item>"
                    "</list></para>", inelegants.count(),
                    "<item>" + inelegants.join("</item><item>") + "</item>");
    } else if ( !errors.isEmpty() && inelegants.isEmpty() ) {
        msg = i18nc("@info", "<para><warning>%1 severe errors found.</warning> You should try "
                    "to fix these: <list>%2</list></para>", errors.count(),
                    "<item>" + errors.join("</item><item>") + "</item>");
    } else { // both not empty
        msg = i18nc("@info", "<para><warning>%1 errors found, %2 of them are severe.</warning> "
                    "You need to fix these: <list>%3</list><nl/>"
                    "You should also try to fix these: <list>%4</list></para>",
                    errors.count() + inelegants.count(), errors.count(),
                    "<item>" + errors.join("</item><item>") + "</item>",
                    "<item>" + inelegants.join("</item><item>") + "</item>");
    }

    KMessageBox::information( this, msg, i18nc("@title:window", "Error Report") );
}

void TimetableMate::scriptNextFunction() {
    if ( m_functions->currentIndex() == -1 ) {
        FunctionNode *node = dynamic_cast<FunctionNode*>(
                                 m_javaScriptModel->nodeAfterLineNumber(
                                     m_scriptDocument->activeView()->cursorPosition().line(), Function) );
        if ( node ) {
            QModelIndex index = m_javaScriptModel->indexFromNode( node );
            m_functions->setCurrentIndex( m_functionsModel->mapFromSource(index).row() );
            return;
        }
    }

    m_functions->setCurrentIndex( m_functions->currentIndex() + 1 );
}

void TimetableMate::scriptPreviousFunction() {
    if ( m_functions->currentIndex() == -1 ) {
        FunctionNode *node = dynamic_cast<FunctionNode*>(
                                 m_javaScriptModel->nodeBeforeLineNumber(
                                     m_scriptDocument->activeView()->cursorPosition().line(), Function) );
        if ( node ) {
            QModelIndex index = m_javaScriptModel->indexFromNode( node );
            m_functions->setCurrentIndex( m_functionsModel->mapFromSource(index).row() );
            return;
        }
    }

    m_functions->setCurrentIndex( m_functions->currentIndex() - 1 );
}

void TimetableMate::urlBarReturn( const QString &text ) {
    m_webview->setUrl( KUrl::fromUserInput(text) );
}

void TimetableMate::webUrlChanged( const QUrl& url ) {
    m_urlBar->setEditUrl( url );
    if ( !m_urlBar->contains(url.toString()) )
        m_urlBar->addUrl( QWebSettings::iconForUrl(url), url );
}

void TimetableMate::webLoadHomePage() {
    const TimetableAccessorInfo *info = m_view->accessorInfo()->info();
    if ( !hasHomePageURL(info) )
        return;

    // Open URL
    m_webview->setUrl( info->url() );

    // Go to web tab
    m_mainTabBar->setCurrentIndex( WebTab );
}

void TimetableMate::webLoadDepartures() {
//     TODO
//     TimetableAccessor accessor = m_view->accessorInfo();
//     if ( !hasRawDepartureURL(accessor) )
//         return;
//
//     // Open URL
//     KUrl url = getDepartureUrl();
//     m_webview->setUrl( url );
//
//     // Go to web tab
//     m_mainTabBar->setCurrentIndex( WebTab );
}

void TimetableMate::webLoadStopSuggestions() {
//     TODO
//     TimetableAccessor accessor = m_view->accessorInfo();
//     if ( !hasRawStopSuggestionURL(accessor) )
//         return;
//
//     // Open URL
//     KUrl url = getStopSuggestionUrl();
//     m_webview->setUrl( url );
//
//     // Go to web tab
//     m_mainTabBar->setCurrentIndex( WebTab );
}

void TimetableMate::webLoadJourneys() {
//     TODO
//     TimetableAccessor accessor = m_view->accessorInfo();
//     if ( !hasRawJourneyURL(accessor) )
//         return;
//
//     // Open URL
//     KUrl url = getJourneyUrl();
//     m_webview->setUrl( url );
//
//     // Go to web tab
//     m_mainTabBar->setCurrentIndex( WebTab );
}

void TimetableMate::scriptRunParseTimetable() {
    ResultObject resultObject;
    QVariant result;
    RequestInfo requestInfo; // TODO: Fill values
    TimetableAccessor *accessor = m_view->accessorInfo();
    if ( !scriptRun(TimetableAccessorScript::SCRIPT_FUNCTION_GETTIMETABLE,
                    &requestInfo, accessor->info(), &resultObject, &result) )
    {
        return;
    }

    // Get global information
    QStringList globalInfos;
    if ( result.isValid() && result.canConvert(QVariant::StringList) ) {
        globalInfos = result.toStringList();
    }

    // Get result set
    QList<TimetableData> data = resultObject.data();
    int count = 0, countInvalid = 0;
    QDate curDate;
    QTime lastTime;
    for ( int i = 0; i < data.count(); ++ i ) {
        TimetableData timetableData = data.at( i );
        QDateTime departureDateTime = timetableData.value( DepartureDateTime ).toDateTime();
        if ( !departureDateTime.isValid() ) {
            QDate date = timetableData.value( DepartureDate ).toDate();
            QTime departureTime;
            if ( timetableData.values().contains(departureTime) ) {
                QVariant timeValue = timetableData.value( DepartureTime );
                if ( timeValue.canConvert(QVariant::Time) ) {
                    departureTime = timeValue.toTime();
                } else {
                    departureTime = QTime::fromString( timeValue.toString(), "hh:mm:ss" );
                    if ( !departureTime.isValid() ) {
                        departureTime = QTime::fromString( timeValue.toString(), "hh:mm" );
                    }
                }
            }
            if ( !date.isValid() ) {
                if ( curDate.isNull() ) {
                    // First departure
                    if ( QTime::currentTime().hour() < 3 && departureTime.hour() > 21 )
                        date = QDate::currentDate().addDays( -1 );
                    else if ( QTime::currentTime().hour() > 21 && departureTime.hour() < 3 )
                        date = QDate::currentDate().addDays( 1 );
                    else
                        date = QDate::currentDate();
                } else if ( lastTime.secsTo(departureTime) < -5 * 60 ) {
                    // Time too much ealier than last time, estimate it's tomorrow
                    date = curDate.addDays( 1 );
                } else {
                    date = curDate;
                }
            }

            departureDateTime = QDateTime( date, departureTime );
            timetableData.insert( DepartureDateTime, departureDateTime );
        }

        curDate = departureDateTime.date();
        lastTime = departureDateTime.time();

        bool isValid = timetableData.contains(TransportLine) && timetableData.contains(Target) &&
                       timetableData.contains(DepartureDateTime);
        if ( isValid )
            ++count;
        else
            ++countInvalid;
    }

    QStringList departures;
    for ( int i = 0; i < data.count(); ++i ) {
        TimetableData values = data[i];
        QString departure;
        departure = QString("\"%1\" to \"%2\" at %3")
                .arg( values[TransportLine].toString(),
                      values[Target].toString(),
                      values[DepartureDateTime].toDateTime().toString() );
        if ( values.contains(DepartureDate) && !values[DepartureDate].toList().isEmpty() ) {
            QList<QVariant> date = values[DepartureDate].toList();
            if ( date.count() >= 3 ) {
                departure += QString(", %1").arg( QDate(date[0].toInt(), date[1].toInt(), date[2].toInt()).toString() );
            }
        }
        if ( values.contains(TypeOfVehicle) && !values[TypeOfVehicle].toString().isEmpty() )
            departure += QString(", %1").arg( values[TypeOfVehicle].toString() );
        if ( values.contains(Delay) && values[Delay].toInt() != -1 )
            departure += QString(", delay: %1").arg( values[Delay].toInt() );
        if ( values.contains(DelayReason) && !values[DelayReason].toString().isEmpty() )
            departure += QString(", delay reason: %1").arg( values[DelayReason].toString() );
        if ( values.contains(Platform) && !values[Platform].toString().isEmpty() )
            departure += QString(", platform: %1").arg( values[Platform].toString() );
        if ( values.contains(Operator) && !values[Operator].toString().isEmpty() )
            departure += QString(", operator: %1").arg( values[Operator].toString() );
        if ( values.contains(RouteStops) && !values[RouteStops].toStringList().isEmpty() ) {
            QStringList routeStops = values[RouteStops].toStringList();
            departure += QString(", %1 route stops").arg( routeStops.count() );

            // Check if RouteTimes has the same number of elements as RouteStops (if set)
            if ( values.contains(RouteTimes) && !values[RouteTimes].toStringList().isEmpty() ) {
                QStringList routeTimes = values[RouteTimes].toStringList();
                departure += QString(", %1 route times").arg( routeTimes.count() );

                if ( routeTimes.count() != routeStops.count() ) {
                    departure += QString(" - <emphasis strong='1'>'RouteTimes' should contain "
                                         "the same number of elements as 'RouteStops'</emphasis>");
                }
            }
        }

        departures << QString("<item><emphasis strong='1'>%1.</emphasis> %2</item>")
			.arg( i + 1 ).arg( departure );
    }

    QStringList unknownTimetableInformations; // TODO
//     QMultiHash<QString, QVariant> unknown = timetableData.unknownTimetableInformationStrings();
//     kDebug() << unknown;
//     for ( QMultiHash<QString, QVariant>::const_iterator it = unknown.constBegin();
//             it != unknown.constEnd(); ++it )
//     {
//         unknownTimetableInformations << "<item>" +
// 	    i18nc("@info", "'%1' with value '%2'", it.key(), it.value().toString()) + "</item>";
//     }

    // Show results
    QString resultItems = i18nc("@info", "Got %1 departures/arrivals.", data.count());
    if ( countInvalid > 0 ) {
        resultItems += "<br/>" + i18ncp("@info", "<warning>%1 departure/arrival is invalid</warning>",
                                        "<warning>%1 departures/arrivals are invalid</warning>", countInvalid);
    }
    if ( globalInfos.contains("no delays", Qt::CaseInsensitive) ) {
        // No delay information available for the given stop
        resultItems += "<br/>" + i18nc("@info", "Got the information from the script that there "
                                       "is no delay information available for the given stop.");
    }

    QString resultText = i18nc("@info", "No syntax errors.") + "<br/>" + resultItems;

	// Add departures
	if ( !departures.isEmpty() ) {
		resultText += i18nc("@info", "<para>Departures:<list>%1</list></para>",
							departures.join(QString()));
	}

    // Add debug messages
//     m_scriptHelper->
	if ( m_scriptErrors.isEmpty() ) {
		resultText += i18nc("@info", "<para>No messages from the script (helper.error)</para>");
	} else {
		QString debugMessagesString;
		foreach ( const QString &message, m_scriptErrors ) {
			debugMessagesString.append( QString("<item>%1</item>")
					.arg(QString(message).replace('<', "&lt;").replace('>', "&gt;")) ); //.arg(debugMessage.context.left(200)) );
		}
		resultText += i18nc("@info", "<para>Messages from the script (helper.error):<list>%1</list></para>",
							debugMessagesString);
	}

    if ( !unknownTimetableInformations.isEmpty() ) {
        resultText += i18nc("@info", "<para>There were unknown strings used for "
                            "<icode>timetableData.set( '<placeholder>unknown string</placeholder>', "
                            "<placeholder>value</placeholder> );</icode>"
                            "<list>%1</list></para>",
                            unknownTimetableInformations.join(QString()));
    }
    KMessageBox::information( this, resultText, i18nc("@title:window", "Result") );
}

void TimetableMate::scriptRunParseStopSuggestions() {
    ResultObject resultObject;
    QVariant result;
    RequestInfo requestInfo; // TODO: Fill values
    TimetableAccessor *accessor = m_view->accessorInfo();
    if ( !scriptRun(TimetableAccessorScript::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS,
                    &requestInfo, accessor->info(), &resultObject, &result) )
    {
        return;
    }

    // Get global information
    QStringList globalInfos;
    if ( result.isValid() && result.canConvert(QVariant::StringList) ) {
        globalInfos = result.toStringList();
    }

    // Get result set
    QStringList stops;
    QHash<QString, QString> stopToStopId;
    QHash<QString, int> stopToStopWeight;

    QList<TimetableData> data = resultObject.data();
    int count = 0, countInvalid = 0;
    foreach ( const TimetableData &timetableData, data ) {
        QString stopName = timetableData[ StopName ].toString();
        QString stopID;
        int stopWeight = -1;

        if ( stopName.isEmpty() ) {
            ++countInvalid;
            continue;
        }

        stops << stopName;

        if ( timetableData.contains(StopID) ) {
            stopID = timetableData[ StopID ].toString();
            stopToStopId.insert( stopName, stopID );
        }

        if ( timetableData.contains(StopWeight) )
            stopWeight = timetableData[ StopWeight ].toInt();

        if ( stopWeight != -1 )
            stopToStopWeight.insert( stopName, stopWeight );
        ++count;
    }

    QStringList stopInfo;
    for ( int i = 0; i < stops.count(); ++i ) {
        QString stop = stops.at( i );
        QString stopItem = QString("\"%1\"").arg( stop );

        if ( stopToStopId.contains(stop) )
            stopItem += QString(", ID: %1").arg( stopToStopId[stop] );
        if ( stopToStopWeight.contains(stop) )
            stopItem += QString(", weight: %1").arg( stopToStopWeight[stop] );

        stopInfo << QString("<item><emphasis strong='1'>%1.</emphasis> %2</item>")
                .arg( i + 1 ).arg( stopItem );
    }

    QStringList unknownTimetableInformations;
//     QMultiHash<QString, QVariant> unknown = timetableData.unknownTimetableInformationStrings();
//     kDebug() << unknown;
//     for ( QMultiHash<QString, QVariant>::const_iterator it = unknown.constBegin();
//             it != unknown.constEnd(); ++it )
//     {
//         unknownTimetableInformations << "<item>" +
//         i18nc("@info", "'%1' with value '%2'", it.key(), it.value().toString()) + "</item>";
//     }

    // Show results
    QString resultItems = i18nc("@info", "Got %1 stop suggestions.", data.count());
    if ( countInvalid > 0 ) {
        resultItems += "<br/>" + i18ncp("@info", "<warning>%1 stop suggestion is invalid</warning>",
                                        "<warning>%1 stop suggestions are invalid</warning>", countInvalid);
    }

    QString resultText = i18nc("@info", "No syntax errors.") + "<br/>" + resultItems;
    resultText += i18nc("@info", "<para>Stop suggestions:<list>%1</list></para>",
                        stopInfo.join(QString()));

    // Add debug messages
	if ( m_scriptErrors.isEmpty() ) {
		resultText += i18nc("@info", "<para>No messages from the script (helper.error)</para>");
	} else {
		QString debugMessagesString;
		foreach ( const QString &message, m_scriptErrors ) {
			debugMessagesString.append( QString("<item>%1</item>")
					.arg(QString(message).replace('<', "&lt;").replace('>', "&gt;")) ); //.arg(debugMessage.context.left(200)) );
		}
		resultText += i18nc("@info", "<para>Messages from the script (helper.error):<list>%1</list></para>",
							debugMessagesString);
	}

    if ( !unknownTimetableInformations.isEmpty() ) {
        resultText += i18nc("@info", "<para>There were unknown strings used for "
                            "<icode>timetableData.set( '<placeholder>unknown string</placeholder>', "
                            "<placeholder>value</placeholder> );</icode>"
                            "<list>%1</list></para>",
                            unknownTimetableInformations.join(QString()));
    }
    KMessageBox::information( this, resultText, i18nc("@title:window", "Result") );
}

void TimetableMate::scriptRunParseJourneys() {
    ResultObject resultObject;
    QVariant result;
    JourneyRequestInfo requestInfo; // TODO: Fill values
    TimetableAccessor *accessor = m_view->accessorInfo();
    if ( !scriptRun(TimetableAccessorScript::SCRIPT_FUNCTION_GETJOURNEYS,
                    &requestInfo, accessor->info(), &resultObject, &result) )
    {
        return;
    }

    // Get global information
    QStringList globalInfos;
    if ( result.isValid() && result.canConvert(QVariant::StringList) ) {
        globalInfos = result.toStringList();
    }

    // Get result set
    QList<TimetableData> data = resultObject.data();
    int count = 0, countInvalid = 0;
    QDate curDate;
    QTime lastTime;
    for ( int i = 0; i < data.count(); ++ i ) {
        TimetableData timetableData = data.at( i );
        QDateTime departureDateTime = timetableData.value( DepartureDateTime ).toDateTime();
        if ( !departureDateTime.isValid() ) {
            QDate date = timetableData.value( DepartureDate ).toDate();
            QTime departureTime;
            if ( timetableData.values().contains(DepartureTime) ) {
                QVariant timeValue = timetableData[ DepartureTime ];
                if ( timeValue.canConvert(QVariant::Time) ) {
                    departureTime = timeValue.toTime();
                } else {
                    departureTime = QTime::fromString( timeValue.toString(), "hh:mm:ss" );
                    if ( !departureTime.isValid() ) {
                        departureTime = QTime::fromString( timeValue.toString(), "hh:mm" );
                    }
                }
            }
            if ( !date.isValid() ) {
                if ( curDate.isNull() ) {
                    // First departure
                    if ( QTime::currentTime().hour() < 3 && departureTime.hour() > 21 )
                        date = QDate::currentDate().addDays( -1 );
                    else if ( QTime::currentTime().hour() > 21 && departureTime.hour() < 3 )
                        date = QDate::currentDate().addDays( 1 );
                    else
                        date = QDate::currentDate();
                } else if ( lastTime.secsTo(departureTime) < -5 * 60 ) {
                    // Time too much ealier than last time, estimate it's tomorrow
                    date = curDate.addDays( 1 );
                } else {
                    date = curDate;
                }
            }

            departureDateTime = QDateTime( date, departureTime );
            timetableData.insert( DepartureDateTime, departureDateTime );
        }

        QDateTime arrivalDateTime = timetableData[ ArrivalDateTime ].toDateTime();
        if ( !departureDateTime.isValid() ) {
            QDate date = timetableData[ ArrivalDate ].toDate();
            QTime arrivalTime;
            if ( timetableData.contains(ArrivalTime) ) {
                QVariant timeValue = timetableData[ ArrivalTime ];
                if ( timeValue.canConvert(QVariant::Time) ) {
                    arrivalTime = timeValue.toTime();
                } else {
                    arrivalTime = QTime::fromString( timeValue.toString(), "hh:mm:ss" );
                    if ( !arrivalTime.isValid() ) {
                        arrivalTime = QTime::fromString( timeValue.toString(), "hh:mm" );
                    }
                }
            }
            if ( !date.isValid() ) {
                date = departureDateTime.date();
            }

            arrivalDateTime = QDateTime( date, arrivalTime );
            if ( arrivalDateTime < departureDateTime ) {
                arrivalDateTime = arrivalDateTime.addDays( 1 );
            }
            timetableData.insert( ArrivalDateTime, arrivalDateTime );
        }

        curDate = departureDateTime.date();
        lastTime = departureDateTime.time();

        bool isValid = timetableData.contains(StartStopName) &&
                       timetableData.contains(TargetStopName) &&
                       timetableData.contains(DepartureDateTime) &&
                       timetableData.contains(ArrivalDateTime);
        if ( isValid )
            ++count;
        else
            ++countInvalid;
    }

    QStringList journeys;
    for ( int i = 0; i < data.count(); ++i ) {
        TimetableData values = data[i];
        QString journey;
        journey = QString("From \"%1\" (%3) to \"%2\" (%4)")
                  .arg( values[StartStopName].toString(),
                        values[TargetStopName].toString(),
                        values[DepartureDateTime].toDateTime().toString(),
                        values[ArrivalDateTime].toDateTime().toString() );
        if ( values.contains(Changes) && !values[Changes].toString().isEmpty() ) {
            journey += QString(",<br> changes: %1").arg( values[Changes].toString() );
        }
        if ( values.contains(TypeOfVehicle) && !values[TypeOfVehicle].toString().isEmpty() ) {
            journey += QString(",<br> %1").arg( values[TypeOfVehicle].toString() );
        }
        if ( values.contains(Operator) && !values[Operator].toString().isEmpty() ) {
            journey += QString(",<br> operator: %1").arg( values[Operator].toString() );
        }
        if ( values.contains(RouteStops) && !values[RouteStops].toStringList().isEmpty() ) {
            QStringList routeStops = values[RouteStops].toStringList();
            journey += QString(",<br> %1 route stops: %2")
                       .arg( routeStops.count() ).arg( routeStops.join(", ") );

            // Check if RouteTimesDeparture has one element less than RouteStops
            // and if RouteTimesDepartureDelay has the same number of elements as RouteStops (if set)
            if ( values.contains(RouteTimesDeparture)
                    && !values[RouteTimesDeparture].toStringList().isEmpty() )
            {
                QStringList routeTimesDeparture = values[RouteTimesDeparture].toStringList();
                journey += QString(",<br> %1 route departure times: %2")
                           .arg( routeTimesDeparture.count() ).arg( routeTimesDeparture.join(", ") );

                if ( routeTimesDeparture.count() != routeStops.count() - 1 ) {
                    journey += QString("<br> - <emphasis strong='1'>'RouteTimesDeparture' should "
                                       "contain one element less than 'RouteStops', because the last stop "
                                       "has no departure, only an arrival time</emphasis>");
                }

                if ( values.contains(RouteTimesDepartureDelay)
                        && !values[RouteTimesDepartureDelay].toStringList().isEmpty() )
                {
                    QStringList routeTimesDepartureDelay =
                        values[RouteTimesDepartureDelay].toStringList();
                    journey += QString(",<br> %1 route departure delays: %2")
                               .arg( routeTimesDepartureDelay.count() ).arg( routeTimesDepartureDelay.join(", ") );

                    if ( routeTimesDepartureDelay.count() != routeTimesDeparture.count() ) {
                        journey += QString("<br> - <emphasis strong='1'>'RouteTimesDepartureDelay' "
                                           "should contain the same number of elements as "
                                           "'RouteTimesDeparture'</emphasis>");
                    }
                }
            }

            // Check if RoutePlatformsDeparture has one element less than RouteStops
            if ( values.contains(RoutePlatformsDeparture)
                    && !values[RoutePlatformsDeparture].toStringList().isEmpty() )
            {
                QStringList routePlatformsArrival = values[RoutePlatformsDeparture].toStringList();
                journey += QString(",<br> %1 route departure platforms: %2")
                           .arg( routePlatformsArrival.count() ).arg( routePlatformsArrival.join(", ") );

                if ( routePlatformsArrival.count() != routeStops.count() - 1 ) {
                    journey += QString("<br> - <emphasis strong='1'>'RoutePlatformsDeparture' should "
                                       "contain one element less than 'RouteStops', because the last stop has "
                                       "no departure, only an arrival platform</emphasis>");
                }
            }

            // Check if RouteTimesArrival has one element less than RouteStops
            // and if RouteTimesArrivalDelay has the same number of elements as RouteStops (if set)
            if ( values.contains(RouteTimesArrival)
                    && !values[RouteTimesArrival].toStringList().isEmpty() )
            {
                QStringList routeTimesArrival = values[RouteTimesArrival].toStringList();
                journey += QString(",<br> %1 route arrival times: %2")
                           .arg( routeTimesArrival.count() ).arg( routeTimesArrival.join(", ") );

                if ( routeTimesArrival.count() != routeStops.count() - 1 ) {
                    journey += QString("<br> - <emphasis strong='1'>'RouteTimesArrival' should "
                                       "contain one element less than 'RouteStops', because the first stop "
                                       "has no arrival, only a departure time</emphasis>");
                }

                if ( values.contains(RouteTimesArrivalDelay)
                        && !values[RouteTimesArrivalDelay].toStringList().isEmpty() )
                {
                    QStringList routeTimesArrivalDelay =
                        values[RouteTimesArrivalDelay].toStringList();
                    journey += QString(",<br> %1 route arrival delays: %2")
                               .arg( routeTimesArrivalDelay.count() ).arg( routeTimesArrivalDelay.join(", ") );

                    if ( routeTimesArrivalDelay.count() != routeTimesArrival.count() ) {
                        journey += QString("<br> - <emphasis strong='1'>'RouteTimesArrivalDelay' "
                                           "should contain the same number of elements as "
                                           "'RouteTimesArrival'</emphasis>");
                    }
                }
            }

            // Check if RoutePlatformsArrival has one element less than RouteStops
            if ( values.contains(RoutePlatformsArrival)
                    && !values[RoutePlatformsArrival].toStringList().isEmpty() )
            {
                QStringList routePlatformsArrival = values[RoutePlatformsArrival].toStringList();
                journey += QString(",<br> %1 route arrival platforms: %2")
                           .arg( routePlatformsArrival.count() ).arg( routePlatformsArrival.join(", ") );

                if ( routePlatformsArrival.count() != routeStops.count() - 1 ) {
                    journey += QString("<br> - <emphasis strong='1'>'RoutePlatformsArrival' should "
                                       "contain one element less than 'RouteStops', because the first stop has "
                                       "no arrival, only a departure platform</emphasis>");
                }
            }
        }

        journeys << QString("<item><emphasis strong='1'>%1.</emphasis> %2</item>")
        .arg( i + 1 ).arg( journey );
    }

    QStringList unknownTimetableInformations;
//     QMultiHash<QString, QVariant> unknown = timetableData.unknownTimetableInformationStrings();
//     kDebug() << unknown;
//     for ( QMultiHash<QString, QVariant>::const_iterator it = unknown.constBegin();
//             it != unknown.constEnd(); ++it )
//     {
//         unknownTimetableInformations << "<item>" +
//         i18nc("@info", "'%1' with value '%2'", it.key(), it.value().toString()) + "</item>";
//     }

    // Show results
    QString resultItems = i18nc("@info", "Got %1 journeys.", data.count());
    if ( countInvalid > 0 ) {
        resultItems += "<br/>" + i18ncp("@info", "<warning>%1 journey is invalid</warning>",
                                        "<warning>%1 journeys are invalid</warning>", countInvalid);
    }
    QString resultText = i18nc("@info", "No syntax errors.") + "<br/>" + resultItems;
    resultText += i18nc("@info", "<para>Journeys:<list>%1</list></para>",
                        journeys.join(QString()));

    // Add debug messages
	if ( m_scriptErrors.isEmpty() ) {
		resultText += i18nc("@info", "<para>No messages from the script (helper.error)</para>");
	} else {
		QString debugMessagesString;
		foreach ( const QString &message, m_scriptErrors ) {
			debugMessagesString.append( QString("<item>%1</item>")
					.arg(QString(message).replace('<', "&lt;").replace('>', "&gt;")) ); //.arg(debugMessage.context.left(200)) );
		}
		resultText += i18nc("@info", "<para>Messages from the script (helper.error):<list>%1</list></para>",
							debugMessagesString);
	}

    if ( !unknownTimetableInformations.isEmpty() ) {
        resultText += i18nc("@info", "<para>There were unknown strings used for "
                            "<icode>timetableData.set( '<placeholder>unknown string</placeholder>', "
                            "<placeholder>value</placeholder> );</icode>"
                            "<list>%1</list></para>",
                            unknownTimetableInformations.join(QString()));
    }
    KMessageBox::information( this, resultText, i18nc("@title:window", "Result") );
}

bool TimetableMate::hasHomePageURL( const TimetableAccessorInfo *info ) {
    if ( info->url().isEmpty() ) {
        KMessageBox::information( this, i18nc("@info",
                "The <interface>Home Page URL</interface> is empty.<nl/>"
                "Please set it in the <interface>Accessor</interface> tab first.") );
        return false;
    } else
        return true;
}

bool TimetableMate::loadScript( QScriptProgram *script, const TimetableAccessorInfo *info )
{
    // Create script engine
    kDebug() << "Create QScriptEngine";
    m_engine = new QScriptEngine( this );
//     m_scriptStorage->mutex.lock();
//     kDebug() << "LOAD qt.core" << thread() << m_parseMode;
//     m_engine->importExtension("qt.core");

//     TODO Load script
//     foreach ( const QString &extension, m_scriptExtensions ) {
//         if ( !importExtension(m_engine, extension) ) {
//             m_lastError = i18nc("@info/plain", "Could not load script extension "
//                                 "<resource>%1</resource>.", extension);
//             m_lastScriptError = ScriptLoadFailed;
//             m_engine->deleteLater();
//             m_engine = 0;
//             return false;
//         }
//     }

    m_engine->globalObject().setProperty( "accessor", m_engine->newQObject(parent()) );

    // Add "importExtension()" function to import extensions
    // Importing Kross not from the GUI thread causes some warnings about pixmaps being used
    // outside the GUI thread...
//     m_engine->globalObject().setProperty( "importExtension",
//                                           m_engine->newFunction(importExtension, 1) );

    // Process events every 100ms
//     m_engine->setProcessEventsInterval( 100 );

    // Register NetworkRequest class for use in the script
    qScriptRegisterMetaType< NetworkRequestPtr >( m_engine,
            networkRequestToScript, networkRequestFromScript );

    // Create objects for the script
    m_scriptHelper = new Helper( info->serviceProvider(), m_engine );
    m_scriptNetwork = new Network( info->fallbackCharset(), this );
    m_scriptResult = new ResultObject( this );
    m_scriptStorage = new Storage( info->serviceProvider(), this );
    connect( m_scriptResult, SIGNAL(publish()), this, SLOT(publish()) );
    connect( m_scriptHelper, SIGNAL(errorReceived(QString,QString)),
             this, SLOT(scriptErrorReceived(QString,QString)) );

    // Make the objects available to the script
    m_engine->globalObject().setProperty( "helper", m_engine->newQObject(m_scriptHelper) );
    m_engine->globalObject().setProperty( "network", m_engine->newQObject(m_scriptNetwork) );
    m_engine->globalObject().setProperty( "storage", m_engine->newQObject(m_scriptStorage) );
    m_engine->globalObject().setProperty( "result", m_engine->newQObject(m_scriptResult) );
    m_engine->globalObject().setProperty( "enum",
            m_engine->newQMetaObject(&ResultObject::staticMetaObject) );

    // Load the script program
    m_engine->evaluate( *script );
    if ( m_engine->hasUncaughtException() ) {
        kDebug() << "Error in the script" << m_engine->uncaughtExceptionLineNumber()
                 << m_engine->uncaughtException().toString();
        kDebug() << "Backtrace:" << m_engine->uncaughtExceptionBacktrace().join("\n");
        m_lastError = i18nc("@info/plain", "Error in the script: "
                "<message>%1</message>.", m_engine->uncaughtException().toString());
        m_engine->deleteLater();
        m_engine = 0;
        m_scriptNetwork->clear();
//         m_scriptNetwork->deleteLater();
//         m_scriptNetwork = 0;
//         m_scriptResult->deleteLater();
//         m_scriptResult = 0;
        m_lastScriptError = ScriptLoadFailed;
        return false;
    } else {
        m_lastScriptError = NoScriptError;
        return true;
    }
}

void TimetableMate::scriptErrorReceived( const QString &message, const QString &failedParseText )
{
    m_scriptErrors << message;
}

bool TimetableMate::scriptRun( const QString &functionToRun, const RequestInfo *requestInfo,
                               const TimetableAccessorInfo *info,
                               ResultObject *resultObject, QVariant *result ) {
    // Create the Kross::Action instance
//     Kross::Action script( this, "TimetableParser" );

//     // Add global script objects
//     if ( functionToRun == SCRIPT_FUNCTION_GETTIMETABLE ) {
//         timetableData->setMode( "departures" );
//     } else if ( functionToRun == SCRIPT_FUNCTION_GETSTOPSUGGESTIONS ) {
//         timetableData->setMode( "stopsuggestions" );
//     } else if ( functionToRun == SCRIPT_FUNCTION_GETJOURNEYS ) {
//         timetableData->setMode( "journeys" );
//     } else  {
//         kDebug() << "Unknown function to run:" << functionToRun;
//     }

//     // NOTE Update documentation in scripting.h if the names change here,
//     // eg. update the name in the documentation of the Helper class if the name "helper" changes.
//     Helper *helper = new Helper(&script);
//     script.addQObject( helper, "helper" );
//     script.addQObject( resultObject, "result" );
//
//     // Set script code and type
//     const TimetableAccessorInfo *info = m_view->accessorInfo()->info();
//     if ( info->scriptFileName().endsWith(QLatin1String(".py")) ) {
//         script.setInterpreter( "python" );
//     } else if ( info->scriptFileName().endsWith(QLatin1String(".rb")) ) {
//         script.setInterpreter( "ruby" );
//     } else if ( info->scriptFileName().endsWith(QLatin1String(".js")) ) {
//         script.setInterpreter( "javascript" );
//     } else {
//         const QString scriptType = KInputDialog::getItem(
// 				i18nc("@title:window", "Choose Script Type"),
// 				i18nc("@info", "Script type unknown, please choose one of these:"),
// 				QStringList() << "JavaScript" << "Ruby" << "Python", 0, false, 0, this );
//         script.setInterpreter( scriptType.toLower() );
//     }
//     script.setCode( m_scriptDocument->text().toUtf8() );
//
//     // Run the script
//     script.trigger();
//     if ( script.hadError() ) {
//         if ( script.errorLineNo() != -1 ) {
//             // Go to error line
//             m_scriptDocument->activeView()->setCursorPosition(
//                 KTextEditor::Cursor(script.errorLineNo(), 0) );
//         }
//         KMessageBox::information( this, i18nc("@info",
//                 "Error in script:<nl/><message>%1</message>", script.errorMessage()) );
//         return false;
//     }
//
//     // Check if the function is implemented by the script
//     if ( !script.functionNames().contains(functionToRun) ) {
//         KMessageBox::information( this, i18nc("@info/plain",
//                 "The script does not implement <emphasis>%1</emphasis>.", functionToRun) );
//         return false;
//     }
//
//     // Call script using Kross
//     *result = script.callFunction( functionToRun, QVariantList() << doc );
//
//     if ( script.hadError() || script.isFinalized() ) {
//         if ( script.errorLineNo() != -1 ) {
//             // Go to error line
//             m_scriptDocument->activeView()->setCursorPosition(
//                 KTextEditor::Cursor(script.errorLineNo(), 0) );
//         }
//
//         KMessageBox::information( this, i18nc("@info",
// 				"An error occurred in the script:<nl/><message>%1</message>", script.errorMessage()) );
//         return false;
//     }
//
//     if ( debugMessageList ) {
// 		debugMessageList->append( helper->debugMessages );
//     }
// NEW ***********************************************************************
    if ( !lazyLoadScript(info) || !loadScript(m_script, info) ) {
        kDebug() << "Script could not be loaded correctly";
        return false;
    }
    kDebug() << "Run script job";
    kDebug() << "JOB:" << requestInfo->stop << requestInfo->dateTime << m_scriptNetwork;
//     emit begin( m_sourceName );

    // Store start time of the script
    QTime time;
    time.start();

    // Add call to the appropriate function
    QString functionName;
    QScriptValueList arguments;
    switch ( requestInfo->parseMode ) {
    case ParseForDeparturesArrivals:
        functionName = TimetableAccessorScript::SCRIPT_FUNCTION_GETTIMETABLE;
        arguments << requestInfo->stop << m_engine->newDate(requestInfo->dateTime)
                  << requestInfo->maxCount << requestInfo->dataType << requestInfo->city;
        break;
    case ParseForJourneys:
        functionName = TimetableAccessorScript::SCRIPT_FUNCTION_GETJOURNEYS;
        arguments << requestInfo->stop
                  << dynamic_cast<const JourneyRequestInfo*>(requestInfo)->targetStop
                  << m_engine->newDate(requestInfo->dateTime)
                  << requestInfo->maxCount << requestInfo->dataType << requestInfo->city;
        break;
    case ParseForStopSuggestions:
        functionName = TimetableAccessorScript::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS;
        arguments << requestInfo->stop << requestInfo->maxCount << requestInfo->city;
        break;

    default:
        kDebug() << "Parse mode unsupported:" << requestInfo->parseMode;
        // TODO
        break;
    }

    if ( functionName.isEmpty() ) {
        // This should never happen, therefore no i18n
        m_lastError = "Unknown parse mode";
        m_lastScriptError = ScriptRunError;
    } else {
        kDebug() << "Call script function" << m_scriptNetwork;
        if ( !m_scriptNetwork ) {
            kDebug() << "Deleted ------------------------------------------------";
            m_engine->deleteLater();
            m_engine = 0;
            m_scriptNetwork->clear();
//             m_scriptResult.clear();
            m_lastScriptError = ScriptRunError;
            return false;
        }

        // Call script function
        m_scriptErrors.clear();
        QScriptValue function = m_engine->globalObject().property( functionName );
        QScriptValue result = function.call( QScriptValue(), arguments );
        if ( m_engine->hasUncaughtException() ) {
            kDebug() << "Error in the script when calling function" << functionName
                        << m_engine->uncaughtExceptionLineNumber()
                        << m_engine->uncaughtException().toString();
            kDebug() << "Backtrace:" << m_engine->uncaughtExceptionBacktrace().join("\n");
            m_lastError = i18nc("@info/plain", "Error in the script when calling function '%1': "
                    "<message>%2</message>.", functionName, m_engine->uncaughtException().toString());
            m_engine->deleteLater();
            m_engine = 0;
            m_scriptNetwork->clear();
//             m_scriptResult.clear();
//             m_scriptNetwork->deleteLater();
//             m_scriptNetwork = 0;
//             m_scriptResult->deleteLater();
//             m_scriptResult = 0;
            m_lastScriptError = ScriptRunError;
            return false;
        }
        m_lastScriptError = NoScriptError;

        GlobalTimetableInfo globalInfo;
        globalInfo.requestDate = QDate::currentDate();
        globalInfo.delayInfoAvailable =
                !m_scriptResult->isHintGiven( ResultObject::NoDelaysForStop );
//         if ( result.isValid() && result.isArray() ) {
//             QStringList results = m_engine->fromScriptValue<QStringList>( result );
//             if ( results.contains(QLatin1String("no delays"), Qt::CaseInsensitive) ) {
//                 // No delay information available for the given stop
//                 globalInfo.delayInfoAvailable = false;
//             }
//             if ( results.contains(QLatin1String("dates need adjustment"), Qt::CaseInsensitive) ) {
//                 globalInfo.datesNeedAdjustment = true;
//             } TODO REMOVE TODO REMOVE TODO REMOVE TODO REMOVE TODO REMOVE TODO REMOVE TODO
//         }

        while ( m_scriptNetwork->hasRunningRequests() || m_engine->isEvaluating() ) {
            // Wait for running requests to finish
            // TODO: Blocking the GUI?x
            QEventLoop eventLoop;
            ScriptAgent agent( m_engine );
            QTimer::singleShot( 30000, &eventLoop, SLOT(quit()) );
            connect( this, SIGNAL(destroyed(QObject*)), &eventLoop, SLOT(quit()) );
            connect( &agent, SIGNAL(scriptFinished()), &eventLoop, SLOT(quit()) );
            connect( m_scriptNetwork, SIGNAL(requestFinished()), &eventLoop, SLOT(quit()) );

            kDebug() << "Waiting for script to finish..." << thread();
            eventLoop.exec();
        }

        // Inform about script run time
        kDebug() << " > Script finished after" << (time.elapsed() / 1000.0)
                    << "seconds: " << /*m_scriptFileName <<*/ requestInfo->parseMode;

        // If data for the current job has already been published, do not emit
        // completed with an empty resultset
//         if ( m_published == 0 || m_scriptResult->count() > m_published ) {
//             const bool couldNeedForcedUpdate = m_published > 0;
//             emitReady(
            switch ( requestInfo->parseMode ) {
            case ParseForDeparturesArrivals:
                emit departuresReady( m_scriptResult->data(),//.mid(m_published),
                        m_scriptResult->features(), m_scriptResult->hints(),
                        m_scriptNetwork->lastUrl(), globalInfo,
                        *dynamic_cast<const DepartureRequestInfo*>(requestInfo) );//,
//                         couldNeedForcedUpdate );
                break;
            case ParseForJourneys:
                emit journeysReady( m_scriptResult->data(),//.mid(m_published),
                        m_scriptResult->features(), m_scriptResult->hints(),
                        m_scriptNetwork->lastUrl(), globalInfo,
                        *dynamic_cast<const JourneyRequestInfo*>(requestInfo) );//,
//                         couldNeedForcedUpdate );
                break;
            case ParseForStopSuggestions:
                emit stopSuggestionsReady( m_scriptResult->data(),//.mid(m_published),
                        m_scriptResult->features(), m_scriptResult->hints(),
                        m_scriptNetwork->lastUrl(), globalInfo,
                        *dynamic_cast<const StopSuggestionRequestInfo*>(requestInfo) );//,
//                         couldNeedForcedUpdate );
                break;

            default:
                kDebug() << "Parse mode unsupported:" << requestInfo->parseMode;
                // TODO
                break;
            }
//             emit dataReady( m_scriptResult->data().mid(m_published),
//                             m_scriptResult->features(), m_scriptResult->hints(),
//                             m_scriptNetwork->lastUrl(), globalInfo,
//                             requestInfo, couldNeedForcedUpdate );
//         }
//         emit end( m_sourceName );

        // Cleanup
        m_scriptResult->clear();
        m_scriptStorage->checkLifetime();

        if ( m_engine->hasUncaughtException() ) {
            kDebug() << "Error in the script when calling function" << functionName
                        << m_engine->uncaughtExceptionLineNumber()
                        << m_engine->uncaughtException().toString();
            kDebug() << "Backtrace:" << m_engine->uncaughtExceptionBacktrace().join("\n");
            m_lastError = i18nc("@info/plain", "Error in the script when calling function '%1': "
                    "<message>%2</message>.", functionName, m_engine->uncaughtException().toString());
            m_engine->deleteLater();
            m_engine = 0;
            m_scriptNetwork->clear();
//             m_scriptNetwork->deleteLater();
//             m_scriptNetwork = 0;
//             m_scriptResult->deleteLater();
//             m_scriptResult = 0;
            m_lastScriptError = ScriptRunError;
            return false;
        }
    }

    return true;
}

QString TimetableMate::decodeHtml( const QByteArray& document, const QByteArray& fallbackCharset ) {
    // Get charset of the received document and convert it to a unicode QString
    // First parse the charset with a regexp to get a fallback charset
    // if QTextCodec::codecForHtml doesn't find the charset
    QString sDocument = QString( document );
    QTextCodec *textCodec;
    QRegExp rxCharset( "(?:<head>.*<meta http-equiv=\"Content-Type\" "
                       "content=\"text/html; charset=)([^\"]*)(?:\"[^>]*>)", Qt::CaseInsensitive );
    rxCharset.setMinimal( true );
    if ( rxCharset.indexIn(sDocument) != -1 && rxCharset.isValid() ) {
        textCodec = QTextCodec::codecForName( rxCharset.cap(1).trimmed().toUtf8() );
    } else if ( !fallbackCharset.isEmpty() ) {
        textCodec = QTextCodec::codecForName( fallbackCharset );
    } else {
        textCodec = QTextCodec::codecForName( "UTF-8" );
    }
    sDocument = QTextCodec::codecForHtml( document, textCodec )->toUnicode( document );

    return sDocument;
}

QString TimetableMate::gethex( ushort decimal ) {
    QString hexchars = "0123456789ABCDEFabcdef";
    return QChar('%') + hexchars[decimal >> 4] + hexchars[decimal & 0xF];
}

QString TimetableMate::toPercentEncoding( const QString &str, const QByteArray &charset ) {
    QString unreserved = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_.~";
    QString encoded;

    QByteArray ba = QTextCodec::codecForName(charset)->fromUnicode(str);
    for ( int i = 0; i < ba.length(); ++i ) {
        char ch = ba[i];
        if ( unreserved.indexOf(ch) != -1 ) {
            encoded += ch;
        } else if ( ch < 0 ) {
            encoded += gethex(256 + ch);
        } else {
            encoded += gethex(ch);
        }
    }

    return encoded;
}

KUrl TimetableMate::getDepartureUrl() {
    TimetableAccessor *accessor = m_view->accessorInfo();

    QPointer<KDialog> dialog = new KDialog( this );
    QWidget *w = new QWidget( dialog );
    QFormLayout *l = new QFormLayout( w );
    KLineEdit *city = NULL;
    KLineEdit *stop = new KLineEdit( w );
    KComboBox *dataType = new KComboBox( w );
    KDateTimeWidget *dateTime = new KDateTimeWidget( QDateTime::currentDateTime(), w );
    dataType->addItem( i18nc("@info/plain", "Departures"), "departures" );
    dataType->addItem( i18nc("@info/plain", "Arrivals"), "arrivals" );
    if ( accessor->info()->useSeparateCityValue() ) {
        city = new KLineEdit( w );
        l->addRow( i18nc("@info", "City:"), city );
    }
    l->addRow( i18nc("@info", "Stop Name:"), stop );
    l->addRow( i18nc("@info", "Data Type:"), dataType );
    l->addRow( i18nc("@info", "Time:"), dateTime );
    dialog->setMainWidget( w );
    stop->setFocus();

    KUrl url;
    if ( dialog->exec() == KDialog::Accepted ) {
        url = getDepartureUrl(/* accessor, city ? city->text() : QString(), stop->text(),
                               dateTime->dateTime(), dataType->itemData(dataType->currentIndex()).toString()*/ );
    }
    delete dialog;

    return url;
}

KUrl TimetableMate::getStopSuggestionUrl() {
    TimetableAccessor *accessor = m_view->accessorInfo();

    QPointer<KDialog> dialog = new KDialog( this );
    QWidget *w = new QWidget( dialog );
    QFormLayout *l = new QFormLayout( w );
    KLineEdit *city = NULL;
    KLineEdit *stop = new KLineEdit( w );
    if ( accessor->info()->useSeparateCityValue() ) {
        city = new KLineEdit( w );
        l->addRow( i18nc("@info", "City:"), city );
    }
    l->addRow( i18nc("@info", "Partial Stop Name:"), stop );
    dialog->setMainWidget( w );
    stop->setFocus();

    KUrl url;
    if ( dialog->exec() == KDialog::Accepted ) {
        url = getStopSuggestionUrl( /*accessor, city ? city->text() : QString(), stop->text()*/ );
    }
    delete dialog;

    return url;
}

KUrl TimetableMate::getJourneyUrl() {
    TimetableAccessor *accessor = m_view->accessorInfo();

    QPointer<KDialog> dialog = new KDialog( this );
    QWidget *w = new QWidget( dialog );
    QFormLayout *l = new QFormLayout( w );
    KLineEdit *city = NULL;
    KLineEdit *startStop = new KLineEdit( w );
    KLineEdit *targetStop = new KLineEdit( w );
    KComboBox *dataType = new KComboBox( w );
    KDateTimeWidget *dateTime = new KDateTimeWidget( QDateTime::currentDateTime(), w );
    dataType->addItem( i18nc("@info/plain", "Departing at Given Time"), "dep" );
    dataType->addItem( i18nc("@info/plain", "Arriving at Given Time"), "arr" );
    if ( accessor->info()->useSeparateCityValue() ) {
        city = new KLineEdit( w );
        l->addRow( i18nc("@info", "City:"), city );
    }
    l->addRow( i18nc("@info", "Start Stop Name:"), startStop );
    l->addRow( i18nc("@info", "Target Stop Name:"), targetStop );
    l->addRow( i18nc("@info", "Time:"), dateTime );
    l->addRow( i18nc("@info", "Meaning of Time:"), dataType );
    dialog->setMainWidget( w );
    startStop->setFocus();

    KUrl url;
    if ( dialog->exec() == KDialog::Accepted ) {
        url = getJourneyUrl( /*accessor, city ? city->text() : QString(),
                             startStop->text(), targetStop->text(), dateTime->dateTime(),
                             dataType->itemData(dataType->currentIndex()).toString()*/ );
    }
    delete dialog;

    return url;
}

KUrl TimetableMate::getDepartureUrl( const TimetableAccessor &accessor, const QString& city,
                                     const QString& stop, const QDateTime& dateTime,
                                     const QString& dataType, bool useDifferentUrl ) const {
    return KUrl(); // TODO
//     int maxCount = 20;
// //     QString sRawUrl = useDifferentUrl ? accessor.rawStopSuggestionsUrl : accessor.rawDepartureUrl;
//     QString sTime = dateTime.time().toString("hh:mm");
//     QString sDataType;
//     QString sCity = city.toLower(), sStop = stop.toLower();
//     if ( dataType == "arrivals" ) {
//         sDataType = "arr";
//     } else if ( dataType == "departures" ) {
//         sDataType = "dep";
//     }
//
// //     sCity = accessor.mapCityNameToValue( sCity ); // TODO
//
//     // Encode city and stop
//     if ( accessor.charsetForUrlEncoding.isEmpty() ) {
//         sCity = QString::fromAscii(KUrl::toPercentEncoding(sCity));
//         sStop = QString::fromAscii(KUrl::toPercentEncoding(sStop));
//     } else {
//         sCity = toPercentEncoding( sCity, accessor.charsetForUrlEncoding.toUtf8() );
//         sStop = toPercentEncoding( sStop, accessor.charsetForUrlEncoding.toUtf8() );
//     }
//
//     // Construct the url from the "raw" url by replacing values
//     if ( accessor.useCityValue )
//         sRawUrl = sRawUrl.replace( "{city}", sCity );
//     sRawUrl = sRawUrl.replace( "{time}", sTime )
//               .replace( "{maxCount}", QString("%1").arg(maxCount) )
//               .replace( "{stop}", sStop )
//               .replace( "{dataType}", sDataType );
//
//     QRegExp rx = QRegExp("\\{date:([^\\}]*)\\}", Qt::CaseInsensitive);
//     if ( rx.indexIn(sRawUrl) != -1 ) {
//         sRawUrl.replace( rx, dateTime.date().toString(rx.cap(1)) );
//     }
//
//     return KUrl( sRawUrl );
}

KUrl TimetableMate::getStopSuggestionUrl( const TimetableAccessor &accessor,
        const QString &city, const QString& stop ) {
    return KUrl(); // TODO
/*    QString sRawUrl = accessor.rawStopSuggestionsUrl;
    QString sCity = city.toLower(), sStop = stop.toLower();

    // Encode stop
    if ( accessor.charsetForUrlEncoding.isEmpty() ) {
        sCity = QString::fromAscii(KUrl::toPercentEncoding(sCity));
        sStop = QString::fromAscii(KUrl::toPercentEncoding(sStop));
    } else {
        sCity = toPercentEncoding( sCity, accessor.charsetForUrlEncoding.toUtf8() );
        sStop = toPercentEncoding( sStop, accessor.charsetForUrlEncoding.toUtf8() );
    }

    if ( accessor.useCityValue ) {
        sRawUrl = sRawUrl.replace( "{city}", sCity );
    }
    sRawUrl = sRawUrl.replace( "{stop}", sStop );

    return KUrl( sRawUrl )*/;
}

KUrl TimetableMate::getJourneyUrl( const TimetableAccessor &accessor, const QString& city,
                                   const QString& startStopName, const QString& targetStopName,
                                   const QDateTime &dateTime, const QString& dataType ) const {
    return KUrl(); // TODO
//     int maxCount = 20;
//     QString sRawUrl = accessor.rawJourneyUrl;
//     QString sTime = dateTime.time().toString("hh:mm");
//     QString sDataType;
//     QString sCity = city.toLower(), sStartStopName = startStopName.toLower(),
//                     sTargetStopName = targetStopName.toLower();
//     if ( dataType == "arrivals" || dataType == "journeysArr" )
//         sDataType = "arr";
//     else if ( dataType == "departures" || dataType == "journeysDep" )
//         sDataType = "dep";
//
// //     sCity = accessor.mapCityNameToValue(sCity); TODO
//
//     // Encode city and stop
//     if ( accessor.charsetForUrlEncoding.isEmpty() ) {
//         sCity = QString::fromAscii(KUrl::toPercentEncoding(sCity));
//         sStartStopName = QString::fromAscii( KUrl::toPercentEncoding(sStartStopName) );
//         sTargetStopName = QString::fromAscii( KUrl::toPercentEncoding(sTargetStopName) );
//     } else {
//         sCity = toPercentEncoding( sCity, accessor.charsetForUrlEncoding.toUtf8() );
//         sStartStopName = toPercentEncoding( sStartStopName, accessor.charsetForUrlEncoding.toUtf8() );
//         sTargetStopName = toPercentEncoding( sTargetStopName, accessor.charsetForUrlEncoding.toUtf8() );
//     }
//
//     // Construct the url from the "raw" url by replacing values
//     if ( accessor.useCityValue ) {
//         sRawUrl = sRawUrl.replace( "{city}", sCity );
//     }
//     sRawUrl = sRawUrl.replace( "{time}", sTime )
//               .replace( "{maxCount}", QString("%1").arg(maxCount) )
//               .replace( "{startStop}", sStartStopName )
//               .replace( "{targetStop}", sTargetStopName )
//               .replace( "{dataType}", sDataType );
//
//     // Fill in date with the given format
//     QRegExp rx = QRegExp( "\\{date:([^\\}]*)\\}", Qt::CaseInsensitive );
//     if ( rx.indexIn(sRawUrl) != -1 ) {
//         sRawUrl.replace( rx, dateTime.date().toString(rx.cap(1)) );
//     }
//
//     rx = QRegExp( "\\{dep=([^\\|]*)\\|arr=([^\\}]*)\\}", Qt::CaseInsensitive );
//     if ( rx.indexIn(sRawUrl) != -1 ) {
//         if ( sDataType == "arr" ) {
//             sRawUrl.replace( rx, rx.cap(2) );
//         } else {
//             sRawUrl.replace( rx, rx.cap(1) );
//         }
//     }
//
//     return KUrl( sRawUrl );
}

#include "timetablemate.moc"
