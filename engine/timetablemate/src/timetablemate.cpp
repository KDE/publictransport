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
#include "debugger.h"

// PublicTransport engine includes
#include <engine/scripting.h>
#include <engine/timetableaccessor.h>
#include <engine/timetableaccessor_info.h>
#include <engine/timetableaccessor_script.h>
#include <engine/script_thread.h>
#include <engine/global.h>

// Qt includes
#include <QtGui/QDropEvent>
#include <QtGui/QPainter>
#include <QtGui/QPrinter>
#include <QtGui/QSplitter>
#include <QtGui/QToolTip>
#include <QtGui/QSortFilterProxyModel>
#include <QtGui/QPlainTextEdit>
#include <QtGui/QFormLayout>
#include <QtGui/QApplication>
#include <QtGui/QDockWidget>
#include <QtGui/QTreeView>
#include <QtGui/QStandardItemModel>
#include <QtWebKit/QWebFrame>
#include <QtWebKit/QWebInspector>
#include <QtCore/QBuffer>
#include <QtCore/QTextCodec>
#include <QtCore/QTimer>
#include <QtScript/QScriptValueIterator>

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
#include <KTextEditor/ConfigInterface>
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
#include <KLineEdit>
#include <KColorScheme>
#include <KToggleAction>

#include <unistd.h>

const char *TimetableMate::SCRIPT_FUNCTION_USEDTIMETABLEINFORMATIONS = "usedTimetableInformations";
const char *TimetableMate::SCRIPT_FUNCTION_GETTIMETABLE = "getTimetable";
const char *TimetableMate::SCRIPT_FUNCTION_GETJOURNEYS = "getJourneys";
const char *TimetableMate::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS = "getStopSuggestions";

// QWidget *CheckboxDelegate::createEditor( QWidget *parent, const QStyleOptionViewItem &option,
//                                          const QModelIndex &index ) const
// {
//     return QAbstractItemDelegate::createEditor( parent, option, index );
// }

void CheckboxDelegate::paint( QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index ) const
{
    bool checked = index.model()->data( index, Qt::EditRole ).toBool();

    if( option.state & QStyle::State_Selected ) {
        painter->setPen( QPen( Qt::NoPen ) );
        if( option.state & QStyle::State_Active ) {
            painter->setBrush( QBrush( QPalette().highlight() ) );
        } else {
            painter->setBrush( QBrush( QPalette().color( QPalette::Inactive,
                                       QPalette::Highlight ) ) );
        }
        painter->drawRect( option.rect );
    }

    QStyleOptionButton checkBoxStyleOption;
    checkBoxStyleOption.state |= QStyle::State_Enabled;
    if( checked ) {
        checkBoxStyleOption.state |= QStyle::State_On;
    } else {
        checkBoxStyleOption.state |= QStyle::State_Off;
    }
    checkBoxStyleOption.rect = option.rect;

    QApplication::style()->drawControl( QStyle::CE_CheckBox, &checkBoxStyleOption, painter);
}

QSize CheckboxDelegate::sizeHint( const QStyleOptionViewItem &option,
                                  const QModelIndex &index ) const
{
    return QApplication::style()->subElementRect( QStyle::SE_CheckBoxLayoutItem, &option ).size();
}

TimetableMate::TimetableMate() : KParts::MainWindow( 0, Qt::WindowContextHelpButtonHint ),
        m_mainTabBar( new KTabWidget(this) ), m_view( new TimetableMateView(this) ),
        m_backtraceDock(0), m_outputDock(0), m_breakpointDock(0),
        m_backtraceModel(0), m_breakpointModel(0),
        m_backgroundParserTimer(0), m_engine(0), m_script(0), m_scriptNetwork(0),
        m_scriptHelper(0), m_scriptResult(0), m_scriptStorage(0), m_debugger(0)
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

    // Create URL bar
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
        if ( !m_accessorDocument || !m_scriptDocument ) {
            return;
        }
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

        KTextEditor::MarkInterface *markInterface =
                qobject_cast<KTextEditor::MarkInterface*>( m_scriptDocument );
        if ( markInterface ) {
            markInterface->setEditableMarks( KTextEditor::MarkInterface::Bookmark |
                                             KTextEditor::MarkInterface::BreakpointActive );
            markInterface->setMarkDescription( KTextEditor::MarkInterface::BreakpointActive,
                    i18nc("@info/plain", "Breakpoint") );
            markInterface->setMarkPixmap( KTextEditor::MarkInterface::BreakpointActive,
                    KIcon("tools-report-bug").pixmap(16, 16) );
            markInterface->setMarkDescription( KTextEditor::MarkInterface::Execution,
                    i18nc("@info/plain", "Execution Line") );
            markInterface->setMarkPixmap( KTextEditor::MarkInterface::Execution,
                    KIcon ("go-next").pixmap(16, 16) );
            connect( m_scriptDocument, SIGNAL(markChanged(KTextEditor::Document*,KTextEditor::Mark,KTextEditor::MarkInterface::MarkChangeAction)),
                     this, SLOT(markChanged(KTextEditor::Document*,KTextEditor::Mark,KTextEditor::MarkInterface::MarkChangeAction)) );
        }

        KTextEditor::View *accessorView = m_accessorDocument->views().first();
        KTextEditor::View *scriptView = m_scriptDocument->views().first();
        connect( accessorView, SIGNAL(informationMessage(KTextEditor::View*,QString)),
                 this, SLOT(informationMessage(KTextEditor::View*,QString)) );
        connect( scriptView, SIGNAL(informationMessage(KTextEditor::View*,QString)),
                 this, SLOT(informationMessage(KTextEditor::View*,QString)) );

        KTextEditor::ConfigInterface *configInterface =
                qobject_cast<KTextEditor::ConfigInterface*>( scriptView );
        if ( configInterface ) {
            configInterface->setConfigValue( "line-numbers", true );
            configInterface->setConfigValue( "icon-bar", true );
            configInterface->setConfigValue( "dynamic-word-wrap", true );
        } else {
            kDebug() << "No KTextEditor::ConfigInterface";
        }
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

    // Add backtrace dock
    m_backtraceDock = new QDockWidget( i18nc("@window:title Dock title", "Backtrace"), this );
    m_backtraceDock->setObjectName( "backtrace" );
    m_backtraceDock->setFeatures( m_backtraceDock->features() | QDockWidget::DockWidgetVerticalTitleBar );
    m_backtraceDock->setAllowedAreas( Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea );
    m_backtraceModel = new QStandardItemModel( this );
    m_backtraceModel->setColumnCount( 3 );
    m_backtraceModel->setHeaderData( 0, Qt::Horizontal, i18nc("@title:column", "Depth"), Qt::DisplayRole );
    m_backtraceModel->setHeaderData( 1, Qt::Horizontal, i18nc("@title:column", "Function"), Qt::DisplayRole );
    m_backtraceModel->setHeaderData( 2, Qt::Horizontal, i18nc("@title:column", "Source"), Qt::DisplayRole );
    QTreeView *backtraceWidget = new QTreeView( this );
    backtraceWidget->setModel( m_backtraceModel );
    backtraceWidget->setAllColumnsShowFocus( true );
    backtraceWidget->setRootIsDecorated( false );
    backtraceWidget->setEditTriggers( QAbstractItemView::NoEditTriggers );
    m_backtraceDock->setWidget( backtraceWidget );
    m_backtraceDock->hide();
    addDockWidget( Qt::BottomDockWidgetArea, m_backtraceDock );
    connect( backtraceWidget, SIGNAL(clicked(QModelIndex)),
             this, SLOT(clickedBacktraceItem(QModelIndex)) );

    // Add breakpoint dock
    m_breakpointDock = new QDockWidget( i18nc("@window:title Dock title", "Breakpoints"), this );
    m_breakpointDock->setObjectName( "breakpoints" );
    m_breakpointDock->setFeatures( m_backtraceDock->features() | QDockWidget::DockWidgetVerticalTitleBar );
    m_breakpointDock->setAllowedAreas( Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea );
    m_breakpointModel = new QStandardItemModel( this );
    m_breakpointModel->setColumnCount( 5 );
    m_breakpointModel->setHeaderData( 0, Qt::Horizontal, i18nc("@title:column", "Enabled"), Qt::DisplayRole );
    m_breakpointModel->setHeaderData( 1, Qt::Horizontal, i18nc("@title:column", "Address"), Qt::DisplayRole );
    m_breakpointModel->setHeaderData( 2, Qt::Horizontal, i18nc("@title:column", "Condition"), Qt::DisplayRole );
    m_breakpointModel->setHeaderData( 3, Qt::Horizontal, i18nc("@title:column", "Hits"), Qt::DisplayRole );
    m_breakpointModel->setHeaderData( 4, Qt::Horizontal, i18nc("@title:column", "Last Condition Result"), Qt::DisplayRole );
    QTreeView *breakpointWidget = new QTreeView( this );
    breakpointWidget->setModel( m_breakpointModel );
    breakpointWidget->setAllColumnsShowFocus( true );
    breakpointWidget->setRootIsDecorated( false );
    breakpointWidget->setItemDelegateForColumn( 0, new CheckboxDelegate(this) );
    m_breakpointDock->setWidget( breakpointWidget );
    m_breakpointDock->hide();
    addDockWidget( Qt::BottomDockWidgetArea, m_breakpointDock );
    connect( m_breakpointModel, SIGNAL(itemChanged(QStandardItem*)),
             this, SLOT(breakpointChangedInModel(QStandardItem*)) );
    connect( breakpointWidget, SIGNAL(clicked(QModelIndex)),
             this, SLOT(clickedBreakpointItem(QModelIndex)) );

    // Add output dock
    m_outputDock = new QDockWidget( i18nc("@window:title Dock title", "Output"), this );
    m_outputDock->setObjectName( "output" );
    m_outputDock->setFeatures( m_backtraceDock->features() | QDockWidget::DockWidgetVerticalTitleBar );
    m_outputDock->setAllowedAreas( Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea );
    m_outputWidget = new QPlainTextEdit( this );
    m_outputWidget->setReadOnly( true );
    m_outputDock->setWidget( m_outputWidget );
    m_outputDock->hide();
    addDockWidget( Qt::BottomDockWidgetArea, m_outputDock );

    // Add coonsole dock
    m_consoleDock = new QDockWidget( i18nc("@window:title Dock title", "Console"), this );
    m_consoleDock->setObjectName( "console" );
    m_consoleDock->setFeatures( m_backtraceDock->features() | QDockWidget::DockWidgetVerticalTitleBar );
    m_consoleDock->setAllowedAreas( Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea );
    QWidget *consoleContainer = new QWidget( this );
    m_consoleWidget = new QPlainTextEdit( consoleContainer );
    m_consoleWidget->setReadOnly( true );
    m_consoleWidget->setFont( KGlobalSettings::fixedFont() );
    m_consoleEdit = new KLineEdit( consoleContainer );
    m_consoleEdit->setClickMessage( i18nc("@info/plain", "Enter a command, eg. '.help'") );
    m_consoleEdit->setFont( KGlobalSettings::fixedFont() );
    QVBoxLayout *consoleLayout = new QVBoxLayout( consoleContainer );
    consoleLayout->setSpacing( 0 );
    consoleLayout->setContentsMargins( 0, 0, 0, 0 );
    consoleLayout->addWidget( m_consoleWidget );
    consoleLayout->addWidget( m_consoleEdit );
    m_consoleDock->setWidget( consoleContainer );
    m_consoleDock->hide();
    addDockWidget( Qt::BottomDockWidgetArea, m_consoleDock );
    connect( m_consoleEdit, SIGNAL(returnPressed(QString)),
             this, SLOT(sendCommandToConsole(QString)) );
    m_consoleHistoryIndex = -1;
    m_consoleEdit->installEventFilter( this );
    KCompletion *completion = m_consoleEdit->completionObject();
    completion->setItems( DebuggerCommand::defaultCompletions() );

    // Add variables dock
    m_variablesDock = new QDockWidget( i18nc("@window:title Dock title", "Variables"), this );
    m_variablesDock->setObjectName( "variables" );
    m_variablesDock->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea );
    m_variablesModel = new QStandardItemModel( this );
    m_variablesModel->setColumnCount( 2 );
    m_variablesModel->setSortRole( Qt::UserRole + 1 );
    m_variablesModel->setHeaderData( 0, Qt::Horizontal, i18nc("@title:column", "Name"), Qt::DisplayRole );
    m_variablesModel->setHeaderData( 1, Qt::Horizontal, i18nc("@title:column", "Value"), Qt::DisplayRole );
    QTreeView *variablesWidget = new QTreeView( this );
    variablesWidget->setAllColumnsShowFocus( true );
    variablesWidget->setModel( m_variablesModel );
    variablesWidget->setAnimated( true );
    m_variablesDock->setWidget( variablesWidget );
    m_variablesDock->hide();
    addDockWidget( Qt::LeftDockWidgetArea, m_variablesDock );

    // Create fixed toolbar to toggle dock widgets (at bottom)
    KToggleAction *toggleConsoleAction = new KToggleAction(
            KIcon("utilities-terminal"), i18nc("@info/plain", "Console"), this );
    KToggleAction *toggleBreakpointAction = new KToggleAction(
            KIcon("tools-report-bug"), i18nc("@info/plain", "Breakpoints"), this );
    KToggleAction *toggleBacktraceAction = new KToggleAction(
            KIcon("view-list-text"), i18nc("@info/plain", "Backtrace"), this );
    KToggleAction *toggleOutputAction = new KToggleAction(
            KIcon("system-run"), i18nc("@info/plain", "Output"), this );
    KToggleAction *toggleVariablesAction = new KToggleAction(
            KIcon("debugger"), i18nc("@info/plain", "Variables"), this );
    connect( toggleConsoleAction, SIGNAL(toggled(bool)), m_consoleDock, SLOT(setVisible(bool)) );
    connect( toggleBreakpointAction, SIGNAL(toggled(bool)), m_breakpointDock, SLOT(setVisible(bool)) );
    connect( toggleBacktraceAction, SIGNAL(toggled(bool)), m_backtraceDock, SLOT(setVisible(bool)) );
    connect( toggleOutputAction, SIGNAL(toggled(bool)), m_outputDock, SLOT(setVisible(bool)) );
    connect( toggleVariablesAction, SIGNAL(toggled(bool)), m_variablesDock, SLOT(setVisible(bool)) );
    connect( m_consoleDock, SIGNAL(visibilityChanged(bool)), toggleConsoleAction, SLOT(setChecked(bool)) );
    connect( m_breakpointDock, SIGNAL(visibilityChanged(bool)), toggleBreakpointAction, SLOT(setChecked(bool)) );
    connect( m_backtraceDock, SIGNAL(visibilityChanged(bool)), toggleBacktraceAction, SLOT(setChecked(bool)) );
    connect( m_outputDock, SIGNAL(visibilityChanged(bool)), toggleOutputAction, SLOT(setChecked(bool)) );
    connect( m_variablesDock, SIGNAL(visibilityChanged(bool)), toggleVariablesAction, SLOT(setChecked(bool)) );
    actionCollection()->addAction( "toggle_dock_console", toggleConsoleAction );
    actionCollection()->addAction( "toggle_dock_breakpoint", toggleBreakpointAction );
    actionCollection()->addAction( "toggle_dock_backtrace", toggleBacktraceAction );
    actionCollection()->addAction( "toggle_dock_output", toggleOutputAction );
    actionCollection()->addAction( "toggle_dock_variables", toggleVariablesAction );

    QActionGroup *bottomDockOverviewGroup = new QActionGroup( this );
    bottomDockOverviewGroup->addAction( toggleConsoleAction );
    bottomDockOverviewGroup->addAction( toggleBreakpointAction );
    bottomDockOverviewGroup->addAction( toggleBacktraceAction );
    bottomDockOverviewGroup->addAction( toggleOutputAction );
//     bottomDockOverviewGroup->addAction( toggleVariablesAction );
    bottomDockOverviewGroup->setExclusive( true );

    m_bottomDockOverview = createDockOverviewBar( Qt::BottomToolBarArea, "bottomDockOverview", this );
    m_bottomDockOverview->addActions( bottomDockOverviewGroup->actions() );
    m_bottomDockOverview->addAction( toggleVariablesAction );
    addToolBar( Qt::BottomToolBarArea, m_bottomDockOverview );

    KActionMenu *viewDocks = new KActionMenu( i18nc("@action", "&Shown Docks"), this );
    viewDocks->addAction( toggleConsoleAction );
    viewDocks->addAction( toggleBreakpointAction );
    viewDocks->addAction( toggleBacktraceAction );
    viewDocks->addAction( toggleOutputAction );
    viewDocks->addAction( toggleVariablesAction );
    actionCollection()->addAction( QLatin1String("view_docks"), viewDocks );

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

KToolBar *TimetableMate::createDockOverviewBar( Qt::ToolBarArea area, const QString &objectName,
                                                QWidget *parent )
{
    KToolBar *overviewBar = new KToolBar( parent );
    overviewBar->setObjectName( objectName );
    overviewBar->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
    overviewBar->setAllowedAreas( area );
    overviewBar->setMovable( false );
    overviewBar->setFloatable( false );
    overviewBar->setIconDimensions( 16 );
    overviewBar->hide();
    addToolBar( area, overviewBar );
    return overviewBar;
}

TimetableMate::~TimetableMate() {
    m_recentFilesAction->saveEntries( Settings::self()->config()->group(0) );
    delete m_script;

    if ( !m_engine ) {
        return;
    }
    m_scriptNetwork->abortAllRequests();

    m_debugger->abortDebugger();
    m_engine->abortEvaluation();
    m_engine->deleteLater();
}

bool TimetableMate::eventFilter( QObject *source, QEvent *event )
{
    if ( source == m_consoleEdit && event->type() == QEvent::KeyPress ) {
        QKeyEvent *keyEvent = dynamic_cast<QKeyEvent*>( event );
        if ( keyEvent ) {
            switch ( keyEvent->key() ) {
            case Qt::Key_Up:
                if ( m_consoleHistoryIndex + 1 < m_consoleHistory.count() ) {
                    ++m_consoleHistoryIndex;
                    m_consoleEdit->setText( m_consoleHistory[m_consoleHistoryIndex] );
                    return true;
                }
                break;
            case Qt::Key_Down:
                if ( m_consoleHistoryIndex >= 0 ) {
                    --m_consoleHistoryIndex;
                    if ( m_consoleHistoryIndex == -1 ) {
                        m_consoleEdit->clear();
                    } else {
                        m_consoleEdit->setText( m_consoleHistory[m_consoleHistoryIndex] );
                    }
                    return true;
                }
                break;
            }
        }
    }

    return QObject::eventFilter( source, event );
}

void TimetableMate::sendCommandToConsole( const QString &commandString )
{
    if ( commandString.isEmpty() ) {
        kDebug() << "No command given";
        return;
    }
    if ( !m_debugger ) {
        kDebug() << "Debugger not running";
        setupDebugger( InterruptOnExceptions );
        evaluateScript();
    }

    // Clear command input edit box
    m_consoleEdit->clear();

    // Write executed command to console
    m_consoleWidget->appendHtml( QString("<b> &gt; %1</b>").arg( commandString ) );

    // Store executed command in history (use up/down keys)
    if ( m_consoleHistory.isEmpty() || m_consoleHistory.first() != commandString ) {
        m_consoleHistory.prepend( commandString );
    }
    m_consoleHistoryIndex = -1;

    // Add executed command to completion object
    KCompletion *completion = m_consoleEdit->completionObject();
    if ( !completion->items().contains(commandString) ) {
        completion->addItem( commandString );
    }

    // Check if commandString contains a command of the form ".<command> ..."
    DebuggerCommand command = DebuggerCommand::fromString( commandString );
    if ( command.isValid() )  {
        QString returnValue;
        // Execute the command
        if ( m_debugger->executeCommand(command, &returnValue) ) {
            // Currently only the clear command cannot be executed in Debugger
            // (no access to the console widget)
            if ( !command.getsExecutedAutomatically() ) {
                switch ( command.command() ) {
                case DebuggerCommand::ClearCommand:
                    m_consoleWidget->clear();
                    break;
                default:
                    break;
                }
            }
        }

        // Write return value to console
        if ( !returnValue.isEmpty() ) {
            m_consoleWidget->appendHtml( returnValue );
        }
    } else {
        // Evaluate commandString in script context
        bool error;
        int errorLineNumber;
        QString errorMessage;
        QStringList backtrace;
        QScriptValue result = m_debugger->evaluateInContext( commandString,
                i18nc("@info/plain", "Console Command"), &error, &errorLineNumber,
                &errorMessage, &backtrace );
        if ( error ) {
            m_consoleWidget->appendHtml( // TODO KUIT <p>
                    i18nc("@info", "Error: <message>%1</message><nl />"
                                   "Backtrace: <message>%2</message>",
                        errorMessage, backtrace.join("<br />")) );
        } else {
            m_consoleWidget->appendHtml( result.toString() );
        }
    }
}

void TimetableMate::clickedBreakpointItem( const QModelIndex &breakpointItem )
{
    if ( !breakpointItem.isValid() ) {
        return;
    }

    const int lineNumber =
            m_breakpointModel->index(breakpointItem.row(), 0).data( Qt::UserRole + 1 ).toInt();
    const Breakpoint breakpoint = m_debugger->breakpointAt( lineNumber );
    if ( !breakpoint.isValid() ) {
        // Breakpoint does not exist, remove it from the model
        kDebug() << "No breakpoint found at" << lineNumber;
        m_breakpointModel->removeRow( breakpointItem.row() );
        return;
    }

    // Set cursor to breakpoint position
    KTextEditor::View *view = m_scriptDocument->activeView();
    view->blockSignals( true );
    view->setCursorPosition( KTextEditor::Cursor(lineNumber - 1, 0) );
    view->blockSignals( false );
}

void TimetableMate::clickedBacktraceItem( const QModelIndex &backtraceItem )
{
    if ( !backtraceItem.isValid() ) {
        return;
    }

    QScriptContext *context = m_engine->currentContext();
    int index = 0;
    while ( context ) {
        if ( backtraceItem.row() == index ) {
            break;
        }
        context = context->parentContext();
        ++index;
    }

    QScriptContextInfo info( context );
    KTextEditor::View *view = m_scriptDocument->activeView();
    view->blockSignals( true );
    if ( backtraceItem.row() == 0 ) {
        // Go to first line of current function if it is the current function
        view->setCursorPosition( KTextEditor::Cursor(info.functionStartLineNumber() - 1, 0) );
    } else {
        view->setCursorPosition( KTextEditor::Cursor(info.lineNumber() - 1, info.columnNumber()) );
    }
    view->blockSignals( false );
    updateVariableModel();
}

void TimetableMate::updateVariableModel()
{
    QScriptContext *context = m_engine->currentContext();
    m_variablesModel->removeRows( 0, m_variablesModel->rowCount() );
    addVariableChilds( context->activationObject() );

    QScriptContext *topContext = context;
    while ( topContext->parentContext() ) {
        topContext = topContext->parentContext();
    }
    if ( topContext != context ) {
        addVariableChilds( topContext->activationObject(), QModelIndex(), true );
    }
    m_variablesModel->sort( 0 );
}

void TimetableMate::markChanged( KTextEditor::Document *document, const KTextEditor::Mark &mark,
                                 KTextEditor::MarkInterface::MarkChangeAction action )
{
    if ( mark.type == KTextEditor::MarkInterface::BreakpointActive ) {
        if ( !m_debugger ) {
            loadScript();
        }

        kDebug() << (mark.line + 1);

        if ( action == KTextEditor::MarkInterface::MarkAdded ) {
            const int lineNumber = m_debugger->getNextBreakableLineNumber( mark.line + 1 );
            if ( mark.line + 1 != lineNumber ) {
                KTextEditor::MarkInterface *markInterface =
                        qobject_cast<KTextEditor::MarkInterface*>( m_scriptDocument );
                markInterface->removeMark( mark.line, mark.type );
            }

            if ( m_debugger->breakpointState(lineNumber) == Debugger::NoBreakpoint ) {
                toggleBreakpoint( lineNumber );
            }
        } else { // Mark removed
            if ( m_debugger->breakpointState(mark.line + 1) != Debugger::NoBreakpoint ) {
                toggleBreakpoint( mark.line + 1 );
            }
        }
    }
}

void TimetableMate::backtraceChanged( const BacktraceQueue &backtrace,
                                      Debugger::BacktraceChange change )
{
    Q_UNUSED( backtrace );
    Q_UNUSED( change );

    QScriptContext *context = m_engine->currentContext();
    int depth = 0;
    while ( context ) {
        ++depth;
        context = context->parentContext();
    }

    int index = -1;
    context = m_engine->currentContext();
    m_backtraceModel->removeRows( 0, m_backtraceModel->rowCount() );
    while ( context ) {
        ++index;
//         QString line = context->toString();
        QScriptContextInfo info( context );
        QString contextString;
        if ( !info.functionName().isEmpty() ) {
            contextString = info.functionName();
        } else {
            if ( context->thisObject().equals(m_engine->globalObject()) ) {
                contextString = "<global>";
            } else {
                contextString = "<anonymous>";
            }
        }

        QList< QStandardItem* > columns;
        columns << new QStandardItem( QString::number(depth--) ); // Depth
        columns << new QStandardItem( contextString ); // Function
        if ( info.lineNumber() == -1 ) {
            if ( info.fileName().isEmpty() ) {
                columns << new QStandardItem( "??" ); // Source
            } else {
                columns << new QStandardItem( info.fileName() ); // Source
            }
        } else {
            columns << new QStandardItem( QString("%0:%1").arg(info.fileName()).arg(info.lineNumber()) ); // Source
        }
        m_backtraceModel->appendRow( columns );
//         str << QString::fromLatin1("#%0 %1() @ %2:%3")
//                 .arg(index + 1)
//                 .arg(contextString)
//                 .arg(info.fileName())
//                 .arg(info.lineNumber());
//         str << QString::fromLatin1("#%0  %1").arg(index).arg(line);
        context = context->parentContext();
    }
//     m_backtraceModel->setStringList( str );
}

QStringList TimetableMate::allowedExtensions()
{
    return QStringList() << "kross" << "qt" << "qt.core" << "qt.xml";
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
    const QString currentTab = KGlobal::locale()->removeAcceleratorMarker(
                             m_mainTabBar->tabText(m_mainTabBar->currentIndex()) );
    QString caption = currentTab;
    if ( !m_currentServiceProviderID.isEmpty() ) {
        caption += " - " + m_currentServiceProviderID;
    }
    if ( m_debugger ) {
        if ( m_engine->hasUncaughtException() ) {
            caption += " - " + i18nc("@info/plain", "Debugging (exception in line %1)",
                                     m_engine->uncaughtExceptionLineNumber());
        } else if ( m_debugger->isInterrupted() ) {
            caption += " - " + i18nc("@info/plain", "Debugging (line %1)", m_debugger->lineNumber());
        }
    }
    setCaption( caption, m_changed );

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

    TimetableAccessor *accessor = m_view->accessor();

    // Enable/disable actions to open web pages
    action( "web_load_homepage" )->setEnabled( !accessor->info()->url().isEmpty() );

    QStringList functions = m_javaScriptModel->functionNames();
    action( "script_run_departures" )->setEnabled(
            functions.contains(SCRIPT_FUNCTION_GETTIMETABLE) );
    action( "script_run_stop_suggestions" )->setEnabled(
            functions.contains(SCRIPT_FUNCTION_GETSTOPSUGGESTIONS) );
    action( "script_run_journeys" )->setEnabled(
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
        int currentLine = m_scriptDocument->activeView()->cursorPosition().line() + 1;
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

// 	m_scriptDocument->views().first()->setCursorPosition( parser.errorCursor() - 1);
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
    action( "script_run_departures" )->setEnabled(
            functions.contains(SCRIPT_FUNCTION_GETTIMETABLE) );
    action( "script_run_stop_suggestions" )->setEnabled(
            functions.contains(SCRIPT_FUNCTION_GETSTOPSUGGESTIONS) );
    action( "script_run_journeys" )->setEnabled(
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
        action("debug_toggle_breakpoint")->setEnabled( false );
        action("debug_remove_all_breakpoints")->setEnabled( false );
//         m_breakpointDock->hide();
        m_variablesDock->hide();
        m_backtraceDock->hide();
        m_outputDock->hide();
        m_consoleDock->hide();
        m_bottomDockOverview->hide();
    } else if ( index == ScriptTab ) { // go to script tab
        action("script_next_function")->setVisible( true );
        action("script_previous_function")->setVisible( true );
        action("debug_toggle_breakpoint")->setEnabled( true );
        action("debug_remove_all_breakpoints")->setEnabled( true );
        m_bottomDockOverview->show();
        m_breakpointDock->show();
        if ( m_debugger ) {
            m_variablesDock->show();
            m_outputDock->show();
            m_consoleDock->show();
            if ( !m_debugger->isInterrupted() ) {
                m_backtraceDock->show();
            }
        }
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

    QAction *runDepartures = new KAction( KIcon("system-run"),
            i18nc("@action", "Run get&Timetable()"), this );
    runDepartures->setToolTip( i18nc("@info:tooltip",
                                          "Runs the <emphasis>getTimetable()</emphasis> function of the script.") );
    actionCollection()->addAction( QLatin1String("script_run_departures"), runDepartures );
    connect( runDepartures, SIGNAL(triggered(bool)), this, SLOT(scriptRunParseTimetable()) );

    QAction *runStopSuggestions = new KAction( KIcon("system-run"),
            i18nc("@action", "Run get&StopSuggestions()"), this );
    runStopSuggestions->setToolTip( i18nc("@info:tooltip",
                                          "Runs the <emphasis>getStopSuggestions()</emphasis> function of the script.") );
    actionCollection()->addAction( QLatin1String("script_run_stop_suggestions"),
                                   runStopSuggestions );
    connect( runStopSuggestions, SIGNAL(triggered(bool)),
             this, SLOT(scriptRunParseStopSuggestions()) );

    QAction *runJourneys = new KAction( KIcon("system-run"),
            i18nc("@action", "Run get&Journeys()"), this );
    runJourneys->setToolTip( i18nc("@info:tooltip",
                                         "Runs the <emphasis>getJourneys()</emphasis> function of the script.") );
    actionCollection()->addAction( QLatin1String("script_run_journeys"), runJourneys );
    connect( runJourneys, SIGNAL(triggered(bool)), this, SLOT(scriptRunParseJourneys()) );

    KActionMenu *runScript = new KActionMenu( KIcon("system-run"),
            i18nc("@action", "&Run Script"), this );
    runScript->setToolTip( i18nc("@info:tooltip", "Runs a function of the script.") );
    runScript->setDelayed( false );
    runScript->addAction( runDepartures );
    runScript->addAction( runStopSuggestions );
    runScript->addAction( runJourneys );
    actionCollection()->addAction( QLatin1String("script_run"), runScript );

    QAction *toolsCheck = new KAction( KIcon("dialog-ok-apply"), i18nc("@action", "&Check"), this );
    toolsCheck->setToolTip( i18nc("@info:tooltip", "Checks the accessor for error/features.") );
    actionCollection()->addAction( QLatin1String("tools_check"), toolsCheck );
    connect( toolsCheck, SIGNAL(triggered(bool)), this, SLOT(toolsCheck()) );

    QAction *debugDepartures = new KAction( KIcon("debug-run"), i18nc("@action", "&Debug getTimetable()"), this );
    debugDepartures->setToolTip( i18nc("@info:tooltip", "Runs the script function 'getTimetable()' in a debugger") );
    actionCollection()->addAction( QLatin1String("debug_departures"), debugDepartures );
    connect( debugDepartures, SIGNAL(triggered(bool)), this, SLOT(debugScriptDepartures()) );

    QAction *debugJourneys = new KAction( KIcon("debug-run"), i18nc("@action", "&Debug getJourneys()"), this );
    debugJourneys->setToolTip( i18nc("@info:tooltip", "Runs the script function 'getJourneys()' in a debugger") );
    actionCollection()->addAction( QLatin1String("debug_journeys"), debugJourneys );
    connect( debugJourneys, SIGNAL(triggered(bool)), this, SLOT(debugScriptJourneys()) );

    QAction *debugStopSuggestions = new KAction( KIcon("debug-run"), i18nc("@action", "&Debug getStopSuggestions()"), this );
    debugStopSuggestions->setToolTip( i18nc("@info:tooltip", "Runs the script function 'getStopSuggestions()' in a debugger") );
    actionCollection()->addAction( QLatin1String("debug_stop_suggestions"), debugStopSuggestions );
    connect( debugStopSuggestions, SIGNAL(triggered(bool)), this, SLOT(debugScriptStopSuggestions()) );

    KActionMenu *debugScript = new KActionMenu( KIcon("debug-run"),
            i18nc("@action", "&Debug Script"), this );
    debugScript->setToolTip( i18nc("@info:tooltip", "Runs a function of the script in a debugger.") );
    debugScript->setDelayed( false );
    debugScript->addAction( debugDepartures );
    debugScript->addAction( debugStopSuggestions );
    debugScript->addAction( debugJourneys );
    actionCollection()->addAction( QLatin1String("debug_run"), debugScript );

    QAction *debugStepInto = new KAction( KIcon("debug-step-into"), i18nc("@action", "Step &Into"), this );
    debugStepInto->setToolTip( i18nc("@info:tooltip", "Continue script execution until the next statement") );
    debugStepInto->setEnabled( false );
    actionCollection()->addAction( QLatin1String("debug_step_into"), debugStepInto );

    QAction *debugStepOver = new KAction( KIcon("debug-step-over"), i18nc("@action", "Step &Over"), this );
    debugStepOver->setToolTip( i18nc("@info:tooltip", "Continue script execution until the next statement in the same context.") );
    debugStepOver->setEnabled( false );
    actionCollection()->addAction( QLatin1String("debug_step_over"), debugStepOver );

    QAction *debugStepOut = new KAction( KIcon("debug-step-out"), i18nc("@action", "Step Ou&t"), this );
    debugStepOut->setToolTip( i18nc("@info:tooltip", "Continue script execution until the current function gets left.") );
    debugStepOut->setEnabled( false );
    actionCollection()->addAction( QLatin1String("debug_step_out"), debugStepOut );

    QAction *debugRunToCursor = new KAction( KIcon("debug-execute-to-cursor"),
                                             i18nc("@action", "Run to &Cursor"), this );
    debugRunToCursor->setToolTip( i18nc("@info:tooltip", "Continue script execution until the current cursor position is reached") );
    debugRunToCursor->setEnabled( false );
    actionCollection()->addAction( QLatin1String("debug_run_to_cursor"), debugRunToCursor );
    connect( debugRunToCursor, SIGNAL(triggered()), this, SLOT(runToCursor()) );

    QAction *debugInterrupt = new KAction( KIcon("media-playback-pause"), i18nc("@action", "&Interrupt"), this );
    debugInterrupt->setToolTip( i18nc("@info:tooltip", "Interrupt script execution.") );
    debugInterrupt->setEnabled( false );
    actionCollection()->addAction( QLatin1String("debug_interrupt"), debugInterrupt );

    QAction *debugContinue = new KAction( KIcon("media-playback-start"), i18nc("@action", "&Continue"), this );
    debugContinue->setToolTip( i18nc("@info:tooltip", "Continue script execution, only interrupt on breakpoints or uncaught exceptions.") );
    debugContinue->setEnabled( false );
    actionCollection()->addAction( QLatin1String("debug_continue"), debugContinue );

    QAction *abortDebugger = new KAction( KIcon("process-stop"), i18nc("@action", "&Abort"), this );
    abortDebugger->setToolTip( i18nc("@info:tooltip", "Abort script execution") );
    abortDebugger->setEnabled( false );
    actionCollection()->addAction( QLatin1String("debug_abort"), abortDebugger );

    QAction *toggleBreakpoint = new KAction( KIcon("tools-report-bug"), i18nc("@action", "Toggle &Breakpoint"), this );
    toggleBreakpoint->setToolTip( i18nc("@info:tooltip", "Toggle breakpoint for the current line") );
    toggleBreakpoint->setEnabled( false );
    actionCollection()->addAction( QLatin1String("debug_toggle_breakpoint"), toggleBreakpoint );
    connect( toggleBreakpoint, SIGNAL(triggered()), this, SLOT(toggleBreakpoint()) );

    QAction *removeAllBreakpoints = new KAction( KIcon("tools-report-bug"), i18nc("@action", "&Remove all Breakpoints"), this );
    removeAllBreakpoints->setToolTip( i18nc("@info:tooltip", "Removes all breakpoints") );
    removeAllBreakpoints->setEnabled( false );
    actionCollection()->addAction( QLatin1String("debug_remove_all_breakpoints"), removeAllBreakpoints );

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

    stateChanged( "debug_not_running" );
}

void TimetableMate::breakpointAdded( const Breakpoint &breakpoint )
{
    KTextEditor::MarkInterface *markInterface =
            qobject_cast<KTextEditor::MarkInterface*>( m_scriptDocument );
    if ( !markInterface ) {
        kDebug() << "Cannot mark breakpoint, no KTextEditor::MarkInterface";
        return;
    }

    disconnect( m_scriptDocument, SIGNAL(markChanged(KTextEditor::Document*,KTextEditor::Mark,KTextEditor::MarkInterface::MarkChangeAction)),
                this, SLOT(markChanged(KTextEditor::Document*,KTextEditor::Mark,KTextEditor::MarkInterface::MarkChangeAction)) );
    markInterface->setMark( breakpoint.lineNumber() - 1,
                            KTextEditor::MarkInterface::BreakpointActive );
    connect( m_scriptDocument, SIGNAL(markChanged(KTextEditor::Document*,KTextEditor::Mark,KTextEditor::MarkInterface::MarkChangeAction)),
             this, SLOT(markChanged(KTextEditor::Document*,KTextEditor::Mark,KTextEditor::MarkInterface::MarkChangeAction)) );

    statusBar()->showMessage( i18nc("@info:status", "Breakpoint in line %1 enabled",
                                    breakpoint.lineNumber()), 5000 );

    QStandardItem *enabledItem = new QStandardItem();
    enabledItem->setData( breakpoint.lineNumber(), Qt::UserRole + 1 );
    enabledItem->setData( breakpoint.isEnabled(), Qt::EditRole );
    enabledItem->setEditable( true );

    QStandardItem *addressItem = new QStandardItem( i18nc("@info/plain", "Line %1",
                                                          breakpoint.lineNumber()) );
    addressItem->setEditable( false );

    QStandardItem *conditionItem = new QStandardItem( breakpoint.condition() );
    conditionItem->setEditable( true );

    QStandardItem *hitsItem = new QStandardItem( QString::number(breakpoint.hitCount()) );
    hitsItem->setEditable( false );

    QStandardItem *lastResultItem = new QStandardItem( breakpoint.lastConditionResult().toString() );
    lastResultItem->setEditable( false );

    QList< QStandardItem* > columns;
    columns << enabledItem; // Enabled
    columns << addressItem; // Address
    columns << conditionItem; // Condition
    columns << hitsItem; // Hits
    columns << lastResultItem; // Last Condition Result

//     disconnect( m_breakpointModel, SIGNAL(itemChanged(QStandardItem*)),
//                 this, SLOT(breakpointChangedInModel(QStandardItem*)) );
    m_breakpointModel->appendRow( columns );
//     connect( m_breakpointModel, SIGNAL(itemChanged(QStandardItem*)),
//              this, SLOT(breakpointChangedInModel(QStandardItem*)) );
}

void TimetableMate::breakpointRemoved( const Breakpoint &breakpoint )
{
    KTextEditor::MarkInterface *markInterface =
            qobject_cast<KTextEditor::MarkInterface*>( m_scriptDocument );
    if ( !markInterface ) {
        kDebug() << "Cannot mark breakpoint, no KTextEditor::MarkInterface";
        return;
    }

    disconnect( m_scriptDocument, SIGNAL(markChanged(KTextEditor::Document*,KTextEditor::Mark,KTextEditor::MarkInterface::MarkChangeAction)),
                this, SLOT(markChanged(KTextEditor::Document*,KTextEditor::Mark,KTextEditor::MarkInterface::MarkChangeAction)) );
    markInterface->removeMark( breakpoint.lineNumber() - 1,
                               KTextEditor::MarkInterface::BreakpointActive );
    connect( m_scriptDocument, SIGNAL(markChanged(KTextEditor::Document*,KTextEditor::Mark,KTextEditor::MarkInterface::MarkChangeAction)),
             this, SLOT(markChanged(KTextEditor::Document*,KTextEditor::Mark,KTextEditor::MarkInterface::MarkChangeAction)) );

    statusBar()->showMessage( i18nc("@info:status", "Removed breakpoint in line %1",
                                    breakpoint.lineNumber()), 5000 );

    for ( int row = 0; row < m_breakpointModel->rowCount(); ++row ) {
        const QModelIndex index = m_breakpointModel->index( row, 0 );
        if ( index.data(Qt::UserRole + 1).toInt() == breakpoint.lineNumber() ) {
//             disconnect( m_breakpointModel, SIGNAL(itemChanged(QStandardItem*)),
//                         this, SLOT(breakpointChangedInModel(QStandardItem*)) );
            m_breakpointModel->removeRow( row );
//             connect( m_breakpointModel, SIGNAL(itemChanged(QStandardItem*)),
//                      this, SLOT(breakpointChangedInModel(QStandardItem*)) );
            break;
        }
    }
}

void TimetableMate::breakpointChangedInModel( QStandardItem *item )
{
    if ( item->column() != 1 ) {
        return; // Not an item in the condition column
    }

    const QString newCondition = item->text();
    const QModelIndex index = m_breakpointModel->index( item->row(), 0 );
    const int lineNumber = index.data( Qt::UserRole + 1 ).toInt();
    Breakpoint breakpoint = m_debugger->breakpointAt( lineNumber );
    if ( !breakpoint.isValid() ) {
        kDebug() << "Breakpoint changed in model, but is no longer existent" << lineNumber << m_debugger->breakpoints();
        return;
    }

    const bool enabled = index.data( Qt::UserRole + 1 ).toBool();
    breakpoint.setEnabled( enabled );

    if ( !m_engine->canEvaluate(newCondition) ) {
        // TODO Update item?
        kDebug() << "Cannot evaluate" << newCondition;
        item->setText( breakpoint.condition() );
    } else {
        breakpoint.setCondition( newCondition );
        m_debugger->addBreakpoint( breakpoint ); // Overwrite the old breakpoint
    }
}

void TimetableMate::toggleBreakpoint( int lineNumber ) {
    if ( !m_debugger ) {
        loadScript();
//     } else if ( m_scriptDocument->isModified() ) {
//
    }

    if ( lineNumber <= 0 ) {
        lineNumber = m_scriptDocument->views().first()->cursorPosition().line() + 1;
    }
    m_debugger->toggleBreakpoint( lineNumber );
}

void TimetableMate::breakpointReached( const Breakpoint &breakpoint )
{
    statusBar()->showMessage( i18nc("@info/plain", "Reached breakpoint at %1",
                                    breakpoint.lineNumber()) );

    for ( int row = 0; row < m_breakpointModel->rowCount(); ++row ) {
        const QModelIndex index = m_breakpointModel->index( row, 0 );
        if ( index.data(Qt::UserRole + 1).toInt() == breakpoint.lineNumber() ) {
            disconnect( m_breakpointModel, SIGNAL(itemChanged(QStandardItem*)),
                        this, SLOT(breakpointChangedInModel(QStandardItem*)) );
            m_breakpointModel->setData( m_breakpointModel->index(row, 3), // 3 => Hit count column
                                        QString::number(breakpoint.hitCount()) );
            connect( m_breakpointModel, SIGNAL(itemChanged(QStandardItem*)),
                     this, SLOT(breakpointChangedInModel(QStandardItem*)) );
            break;
        }
    }
}

void TimetableMate::abortDebugger()
{
    m_debugger->engine()->abortEvaluation();
}

void TimetableMate::scriptPositionAboutToChange( int oldLineNumber, int oldColumnNumber,
                                                 int lineNumber, int columnNumber )
{
    if ( m_debugger->isInterrupted() && lineNumber != -1 ) {
        KTextEditor::View *view = m_scriptDocument->activeView();
        view->blockSignals( true );
        view->setCursorPosition( KTextEditor::Cursor(lineNumber - 1, columnNumber) );
        view->blockSignals( false );
    }

    KTextEditor::MarkInterface *iface =
            qobject_cast<KTextEditor::MarkInterface*>( m_scriptDocument );
    if ( !iface ) {
        kDebug() << "Cannot mark breakpoint, no KTextEditor::MarkInterface";
    } else {
        iface->removeMark( oldLineNumber - 1, KTextEditor::MarkInterface::Execution );
        if ( lineNumber != -1 ) {
            iface->addMark( lineNumber - 1, KTextEditor::MarkInterface::Execution );
        }
    }
}

void TimetableMate::setupDebugger( /* TODO Move that Enum to Debugger class ==> */ DebugType debug )
{
#ifndef QT_NO_SCRIPTTOOLS
    if ( debug == NoDebugging ) {
        m_debugger->debugContinue();
    } else {
        // Create debugger if it is not already created
        if ( !m_debugger ) {
            if ( !m_engine ) {
                // Create script engine, the function then calls setupDebugger and creates m_debugger
                createScriptEngine();
            } else {
                kDebug() << "Create debugger";
                m_debugger = new Debugger( m_engine );
                connect( m_debugger, SIGNAL(positionAboutToChanged(int,int,int,int)),
                        this, SLOT(scriptPositionAboutToChange(int,int,int,int)) );
                connect( action("debug_step_into"), SIGNAL(triggered()),
                         m_debugger, SLOT(debugStepInto()) );
                connect( action("debug_step_over"), SIGNAL(triggered()),
                         m_debugger, SLOT(debugStepOver()) );
                connect( action("debug_step_out"), SIGNAL(triggered()),
                         m_debugger, SLOT(debugStepOut()) );
                connect( action("debug_interrupt"), SIGNAL(triggered()),
                         m_debugger, SLOT(debugInterrupt()) );
                connect( action("debug_continue"), SIGNAL(triggered()),
                         m_debugger, SLOT(debugContinue()) );
                connect( action("debug_abort"), SIGNAL(triggered()),
                         m_debugger, SLOT(abortDebugger()) );
                connect( action("debug_remove_all_breakpoints"), SIGNAL(triggered()),
                         m_debugger, SLOT(removeAllBreakpoints()) );
                connect( m_debugger, SIGNAL(interrupted()), this, SLOT(debugInterrupted()) );
                connect( m_debugger, SIGNAL(continued()), this, SLOT(debugContinued()) );
                connect( m_debugger, SIGNAL(scriptStarted()), this, SLOT(debugStarted()) );
                connect( m_debugger, SIGNAL(scriptFinished()), this, SLOT(debugStopped()) );
                connect( m_debugger, SIGNAL(exception(int,QString)),
                         this, SLOT(uncaughtException(int,QString)) );
                connect( m_debugger, SIGNAL(backtraceChanged(BacktraceQueue,Debugger::BacktraceChange)),
                        this, SLOT(backtraceChanged(BacktraceQueue,Debugger::BacktraceChange)) );
                connect( m_debugger, SIGNAL(breakpointReached(Breakpoint)),
                        this, SLOT(breakpointReached(Breakpoint)) );
                connect( m_debugger, SIGNAL(breakpointAdded(Breakpoint)),
                        this, SLOT(breakpointAdded(Breakpoint)) );
                connect( m_debugger, SIGNAL(breakpointRemoved(Breakpoint)),
                        this, SLOT(breakpointRemoved(Breakpoint)) );
                connect( m_debugger, SIGNAL(output(QString,QScriptContext*)),
                         this, SLOT(scriptOutput(QString,QScriptContext*)) );
            }
        }
        kDebug() << "Attach debugger";
        m_engine->setAgent( m_debugger );
        Q_ASSERT( m_engine == m_debugger->engine() );

        if ( debug == InterruptAtStart ) {
            m_debugger->debugInterrupt();
        } else if ( debug == InterruptOnExceptions ) {
            m_debugger->debugContinue();
        }
    }
#else
    Q_UNUSED( debug );
#endif
}

void TimetableMate::runToCursor()
{
    const KTextEditor::View *view = m_scriptDocument->activeView();
    m_debugger->debugRunUntilLineNumber( view->cursorPosition().line() + 1 );
}

void TimetableMate::debugInterrupted()
{
    KTextEditor::View *view = m_scriptDocument->activeView();
    view->blockSignals( true );
    view->setCursorPosition( KTextEditor::Cursor(m_debugger->lineNumber() - 1, 0) );
    view->blockSignals( false );

    stateChanged( "debug_interrupted" );
    statusBar()->showMessage( i18nc("@info:status", "Script interrupted"), 5000 );
    updateWindowTitle();
    updateVariableModel();
}

void TimetableMate::debugContinued()
{
    stateChanged( "debug_running" );
    statusBar()->showMessage( i18nc("@info:status", "Script continued"), 5000 );
    setWindowTitle( i18nc("@window:title", "TimetableMate - %1 [*]",
                          m_view->accessor()->serviceProvider()) );
    updateWindowTitle();
}

void TimetableMate::debugStarted()
{
    stateChanged( "debug_running" );
    m_scriptResult->clear();
    m_scriptErrors.clear();
    m_scriptNetwork->clear();
    debugContinued();

//     m_backtraceDock->show();
//     m_outputDock->show();
//     m_variablesDock->show();
//     m_consoleDock->show();
    statusBar()->showMessage( i18nc("@info:status", "Script started"), 5000 );
    m_outputWidget->appendHtml( i18nc("@info", "<emphasis strong='1'>Execution started at %1</emphasis>",
                                      QTime::currentTime().toString()) );
    updateWindowTitle();
}

void TimetableMate::debugStopped()
{
    stateChanged( "debug_not_running" );

//     m_backtraceDock->hide();
    statusBar()->showMessage( i18nc("@info:status", "Script finished"), 5000 );
    m_outputWidget->appendHtml( i18nc("@info", "<emphasis strong='1'>Execution ended at %1</emphasis>",
                                      QTime::currentTime().toString()) + "<br />---------------------<br />" );
    updateWindowTitle();
}

void TimetableMate::uncaughtException( int lineNumber, const QString &errorMessage )
{
    statusBar()->showMessage(
            i18nc("@info:status", "Uncaught exception at %1: <message>%2</message>",
                  lineNumber, errorMessage) );
    m_outputWidget->appendHtml(
            i18nc("@info", "<emphasis strong='1'>Uncaught exception at %1:</emphasis> "
                  "<message>%2</message>", lineNumber, errorMessage) );
}

ScriptVariableRow TimetableMate::addVariableRow( QStandardItem *item, const QString &name,
        const QString &value, const KIcon &icon, bool encodeValue, const QChar &endCharacter )
{
    QStandardItem *newNameItem = new QStandardItem( icon, name );
    QStandardItem *newValueItem = new QStandardItem( value );
    newNameItem->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable );
    newValueItem->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable );
    newValueItem->setToolTip( variableValueTooltip(value, encodeValue, endCharacter) );
    item->appendRow( QList<QStandardItem*>() << newNameItem << newValueItem );
    return ScriptVariableRow( newNameItem, newValueItem );
}

QString TimetableMate::variableValueTooltip( const QString &completeValueString,
                                             bool encodeHtml, const QChar &endCharacter ) const
{
    if ( completeValueString.isEmpty() ) {
        return QString();
    }

    QString tooltip = completeValueString.left( 1000 );
    if ( encodeHtml ) {
        if ( !endCharacter.isNull() ) {
            tooltip += endCharacter; // Add end character (eg. a quotation mark), which got cut off
        }
        tooltip = Global::encodeHtmlEntities( tooltip );
    }
    if ( tooltip.length() < completeValueString.length() ) {
        tooltip.prepend( i18nc("@info Always plural",
                         "<emphasis strong='1'>First %1 characters:</emphasis><nl />", 1000) )
               .append( QLatin1String("...") );
    }
    return tooltip.prepend( QLatin1String("<p>") ).append( QLatin1String("</p>") );
}

QPair<QBrush, QBrush> checkTimetableInformation( TimetableInformation info, const QVariant &value ) {
    bool correct = value.isValid();
    if ( correct ) {
        switch ( info ) {
        case DepartureDateTime:
        case ArrivalDateTime:
            correct = value.toDateTime().isValid();
            break;
        case DepartureDate:
        case ArrivalDate:
            correct = value.toDate().isValid();
            break;
        case DepartureTime:
        case ArrivalTime:
            correct = value.toTime().isValid();
            break;
        case TypeOfVehicle:
            correct = PublicTransportInfo::getVehicleTypeFromString( value.toString() ) != Unknown;
            break;
        case TransportLine:
        case Target:
        case TargetShortened:
        case Platform:
        case DelayReason:
        case JourneyNews:
        case JourneyNewsOther:
        case JourneyNewsLink:
        case Operator:
        case Status:
        case StartStopName:
        case StartStopID:
        case StopCity:
        case StopCountryCode:
        case TargetStopName:
        case TargetStopID:
        case Pricing:
        case StopName:
        case StopID:
            correct = !value.toString().trimmed().isEmpty();
            break;
        case Delay:
            correct = value.canConvert( QVariant::Int ) && value.toInt() >= -1;
            break;
        case Duration:
        case StopWeight:
        case Changes:
        case RouteExactStops:
            correct = value.canConvert( QVariant::Int ) && value.toInt() >= 0;
            break;
        case TypesOfVehicleInJourney:
        case RouteTimes:
        case RouteTimesDeparture:
        case RouteTimesArrival:
        case RouteTypesOfVehicles:
        case RouteTimesDepartureDelay:
        case RouteTimesArrivalDelay:
            correct = !value.toList().isEmpty();
            break;
        case IsNightLine:
            correct = value.canConvert( QVariant::Bool );
            break;
        case RouteStops:
        case RouteStopsShortened:
        case RouteTransportLines:
        case RoutePlatformsDeparture:
        case RoutePlatformsArrival:
            correct = !value.toStringList().isEmpty();
            break;

        default:
            correct = true;;
        }
    }

    const KColorScheme scheme( QPalette::Active );
    return QPair<QBrush, QBrush>(
            scheme.background(correct ? KColorScheme::PositiveBackground : KColorScheme::NegativeBackground),
            scheme.foreground(correct ? KColorScheme::PositiveText : KColorScheme::NegativeText) );
}

void TimetableMate::addVariableChilds( const QScriptValue &value, const QModelIndex &parent,
                                       bool onlyImportantObjects ) {
    QScriptValueIterator it( value );
    while ( it.hasNext() ) {
        it.next();
        if ( (onlyImportantObjects && !it.value().isQObject()) ||
             it.value().isError() || it.flags().testFlag(QScriptValue::SkipInEnumeration) ||
             it.name() == "NaN" || it.name() == "undefined" || it.name() == "Infinity" )
        {
            continue;
        }

        const bool isHelperObject = it.name() == "helper" || it.name() == "network" ||
                it.name() == "storage" || it.name() == "result" || it.name() == "accessor";
        if ( onlyImportantObjects && !isHelperObject ) {
            continue;
        }

        QString valueString;
        bool encodeValue = false;
        QChar endCharacter;
        if ( it.value().isArray() ) {
            valueString = QString("[%0]").arg( it.value().toVariant().toStringList().join(", ") );
            endCharacter = ']';
        } else if ( it.value().isString() ) {
            valueString = QString("\"%1\"").arg( it.value().toString() );
            encodeValue = true;
            endCharacter = '\"';
        } else if ( it.value().isRegExp() ) {
            valueString = QString("/%1/%2").arg( it.value().toRegExp().pattern() )
                    .arg( it.value().toRegExp().caseSensitivity() == Qt::CaseSensitive ? "" : "i" );
            encodeValue = true;
        } else if ( it.value().isFunction() ) {
            valueString = QString("function %1()").arg( it.name() ); // it.value() is the function definition
        } else {
            valueString = it.value().toString();
        }

        const QString completeValueString = valueString;
        const int cutPos = valueString.indexOf( '\n' );
        if ( cutPos != -1 ) {
            valueString = valueString.left( cutPos ) + " ...";
        }

        QStandardItem *newParentItem = new QStandardItem( it.name() );
        QStandardItem *newValueItem = new QStandardItem( valueString );
        newParentItem->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable );
        newValueItem->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable );
        newValueItem->setToolTip(
                variableValueTooltip(completeValueString, encodeValue, endCharacter) );

        QList< QStandardItem* > columns;
        columns << newParentItem;
        columns << newValueItem;

        if ( it.value().isRegExp() ) {
            newParentItem->setIcon( KIcon("code-variable") );
        } else if ( it.value().isFunction() ) {
            newParentItem->setIcon( KIcon("code-function") );
        } else if ( it.value().isArray() || it.value().isBool() || it.value().isBoolean() ||
                    it.value().isDate() || it.value().isNull() || it.value().isNumber() ||
                    it.value().isDate() || it.value().isString() || it.value().isUndefined() )
        {
            newParentItem->setIcon( KIcon("code-variable") );
        } else if ( it.value().isObject() || it.value().isQObject() || it.value().isQMetaObject() ) {
            newParentItem->setIcon( KIcon("code-class") );
        } else {
            newParentItem->setIcon( KIcon("code-context") );
        }
        if ( isHelperObject ) {
            QFont font = newParentItem->font();
            font.setBold( true );
            newParentItem->setFont( font );
        }

        const int row = m_variablesModel->rowCount( parent );
        int sortValue = 9999;
        if ( !it.value().isQObject() && !it.value().isQMetaObject() ) {
            // Sort to the end
            sortValue = 10000;
        } else if ( it.name() == TimetableAccessorScript::SCRIPT_FUNCTION_GETTIMETABLE ||
                    it.name() == TimetableAccessorScript::SCRIPT_FUNCTION_GETJOURNEYS ||
                    it.name() == TimetableAccessorScript::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS ||
                    it.name() == TimetableAccessorScript::SCRIPT_FUNCTION_USEDTIMETABLEINFORMATIONS )
        {
            // Sort to the beginning
            sortValue = 0;
        } else if ( isHelperObject ) {
            sortValue = 1;
        }
        newParentItem->setData( sortValue, Qt::UserRole + 1 );

        if ( parent.isValid() ) {
            m_variablesModel->itemFromIndex( parent )->insertRow( row, columns );
        } else {
            m_variablesModel->insertRow( row, columns );
        }
        m_variablesModel->setData( m_variablesModel->index(row, 0, parent),
                                   it.name(), Qt::DisplayRole );
        m_variablesModel->setData( m_variablesModel->index(row, 1, parent),
                                   valueString, Qt::DisplayRole );

        if ( it.name() == "result" ) {
            // Add special items for the "result" script object, which is an exposed ResultObject object
            ResultObject *result = qobject_cast< ResultObject* >( it.value().toQObject() );
            Q_ASSERT( result );
            const QString valueString = i18ncp("@info/plain", "%1 result", "%1 results",
                                               result->count());
            newValueItem->setText( valueString );
            newValueItem->setToolTip( valueString );

            ScriptVariableRow resultsItem = addVariableRow( newParentItem,
                    i18nc("@info/plain", "Data"), valueString, KIcon("documentinfo") );
            int i = 1;
            QList<TimetableInformation> shortInfoTypes = QList<TimetableInformation>()
                    << Target << TargetStopName << DepartureDateTime << DepartureTime << StopName;
            foreach ( const TimetableData &data, result->data() ) {
                QString shortInfo;
                foreach ( TimetableInformation infoType, shortInfoTypes) {
                    if ( data.contains(infoType) ) {
                        shortInfo = data[ infoType ].toString();
                        break;
                    }
                }
                ScriptVariableRow resultItem = addVariableRow( resultsItem.first,
                        i18nc("@info/plain", "Result %1", i),
                        QString("<%1>").arg(shortInfo), KIcon("code-class") );
                resultItem.first->setData( i, Qt::UserRole + 1 );
                for ( TimetableData::ConstIterator it = data.constBegin();
                      it != data.constEnd(); ++it )
                {
                    QString valueString;
                    const bool isList = it.value().isValid() &&
                                        it.value().canConvert( QVariant::List );
                    if ( isList ) {
                        const QVariantList list = it.value().toList();
                        QStringList stringList;
                        int count = 0;
                        for ( int i = 0; i < list.count(); ++i ) {
                            const QString str = list[i].toString();
                            count += str.length();
                            if ( count > 100 ) {
                                stringList << "...";
                                break;
                            }
                            stringList << str;
                        }
                        valueString = '[' + stringList.join(", ") + ']';
                    } else {
                        valueString = it.value().toString();
                    }
                    ScriptVariableRow item = addVariableRow( resultItem.first,
                                    Global::timetableInformationToString(it.key()),
                                    valueString, KIcon("code-variable") );
                    QPair<QBrush, QBrush> colors = checkTimetableInformation( it.key(), it.value() );
                    item.first->setBackground( colors.first );
                    item.first->setForeground( colors.second );
                    item.second->setBackground( colors.first );
                    item.second->setForeground( colors.second );

                    if ( isList ) {
                        const QVariantList list = it.value().toList();
                        for ( int i = 0; i < list.count(); ++i ) {
                            ScriptVariableRow listItem = addVariableRow(
                                    item.first, QString::number(i + 1), list[i].toString(),
                                    KIcon("code-variable") );
                            listItem.first->setData( i, Qt::UserRole + 1 );
                        }
                    }
                }
                ++i;
            }
        } else if ( it.name() == "network" ) {
            // Add special items for the "network" script object, which is an exposed Network object
            Network *network = qobject_cast< Network* >( it.value().toQObject() );
            Q_ASSERT( network );
            const QString valueString = i18ncp("@info/plain", "%1 request", "%1 requests",
                                               network->runningRequests().count());
            newValueItem->setText( valueString );
            newValueItem->setToolTip( valueString );

            ScriptVariableRow requestsItem = addVariableRow( newParentItem,
                    i18nc("@info/plain", "Running Requests"), valueString, KIcon("documentinfo") );
            int i = 1;
            foreach ( NetworkRequest *networkRequest, network->runningRequests() ) {
                ScriptVariableRow resultItem = addVariableRow( requestsItem.first,
                        i18nc("@info/plain", "Request %1", i),
                        networkRequest->url(), KIcon("code-class") );
                resultItem.first->setData( i, Qt::UserRole + 1 );
                ++i;
            }
        } else if ( it.name() == "storage" ) {
            // Add special items for the "storage" script object, which is an exposed Storage object
            Storage *storage = qobject_cast< Storage* >( it.value().toQObject() );
            Q_ASSERT( storage );
            const QVariantMap memory = storage->read();
            const QString valueString = i18ncp("@info/plain", "%1 value", "%1 values",
                                               memory.count());
            newValueItem->setText( valueString );
            newValueItem->setToolTip( valueString );

            ScriptVariableRow storageItem = addVariableRow( newParentItem,
                    i18nc("@info/plain", "Memory"), valueString, KIcon("documentinfo") );
            int i = 1;
            for ( QVariantMap::ConstIterator it = memory.constBegin();
                  it != memory.constEnd(); ++it )
            {
                ScriptVariableRow valueItem = addVariableRow( storageItem.first,
                        it.key(), it.value().toString(), KIcon("code-variable"), true );
                valueItem.first->setData( i, Qt::UserRole + 1 );
                ++i;
            }
        } else if ( it.name() == "helper" ) {
            const QString valueString = i18nc("@info/plain", "Offers helper functions to scripts");
            newValueItem->setText( valueString );
            newValueItem->setToolTip( valueString );
        } else if ( it.name() == "accessor" ) {
            const QString valueString = i18nc("@info/plain", "Exposes accessor information to "
                                              "scripts, which got read from the XML file");
            newValueItem->setText( valueString );
            newValueItem->setToolTip( valueString );
        }

        // Recursively add children, not for functions, max 1000 children
        if ( m_variablesModel->rowCount(parent) < 1000 && !it.value().isFunction() ) {
            addVariableChilds( it.value(), newParentItem->index() );
        }
    }
}

void TimetableMate::scriptOutput( const QString &outputString, QScriptContext *context )
{
    const QScriptContextInfo info( context );
    m_outputWidget->appendHtml( i18nc("@info", "<emphasis strong='1'>Line %1:</emphasis> <message>%2</message>",
                                      info.lineNumber(), outputString) );
}

void TimetableMate::debugScriptDepartures()
{
    // Go to script tab
    m_mainTabBar->setCurrentIndex( ScriptTab );

    setupDebugger( InterruptOnExceptions );
    evaluateScript();

    setupDebugger( InterruptAtStart );
    scriptRunParseTimetable();
}

void TimetableMate::debugScriptJourneys()
{
    // Go to script tab
    m_mainTabBar->setCurrentIndex( ScriptTab );

    setupDebugger( InterruptOnExceptions );
    evaluateScript();

    setupDebugger( InterruptAtStart );
    scriptRunParseJourneys();
}

void TimetableMate::debugScriptStopSuggestions()
{
    // Go to script tab
    m_mainTabBar->setCurrentIndex( ScriptTab );

    setupDebugger( InterruptOnExceptions );
    evaluateScript();

    setupDebugger( InterruptAtStart );
    scriptRunParseStopSuggestions();
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
    if ( !m_view->accessor() ) {
        kDebug() << "No accessor loaded";
        return;
    }

    const TimetableAccessorInfo *info = m_view->accessor()->info();
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
    const TimetableAccessorInfo *info = m_view->accessor()->info();
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
    const TimetableAccessorInfo *info = m_view->accessor()->info();

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
    const QString filePath = QFileInfo( _fileName ).path();
    if ( filePath.isEmpty() ) {
        kDebug() << "Cannot open script files when the path isn't given. "
                    "Save the accessor XML file first.";
        m_mainTabBar->setTabEnabled( ScriptTab, false ); // Disable script tab
    } else if ( !loadScriptForCurrentAccessor(false) ) {
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
    if ( !loadScriptForCurrentAccessor() ) {
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

void TimetableMate::scriptFileChanged( const QString &scriptFile ) {
    if ( scriptFile.isEmpty() ) {
        kDebug() << "Cannot open script files when the path isn't given. "
                    "Save the accessor XML file first.";
        m_mainTabBar->setTabEnabled( ScriptTab, false ); // Disable script tab
    } else {
        loadScriptForCurrentAccessor();
    }
}

void TimetableMate::plasmaPreviewLoaded() {
    m_preview->setSettings( m_currentServiceProviderID, QString() );
}

bool TimetableMate::loadScriptForCurrentAccessor( bool openFile ) {
    m_scriptDocument->closeUrl( false );
    m_scriptDocument->setModified( false );

    QString text = m_accessorDocument->text();
    QString scriptFile = m_view->accessor()->info()->scriptFileName();
    if ( scriptFile.isEmpty() ) {
        m_mainTabBar->setTabEnabled( ScriptTab, false ); // Disable script tab
        return false;
    } else {
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
// //     TODO
//     // The preference dialog is derived from prefs_base.ui
//     //
//     // compare the names of the widgets in the .ui file
//     // to the names of the variables in the .kcfg file
//     //avoid to have 2 dialogs shown
//     if ( KConfigDialog::showDialog( "settings" ) )  {
//         return;
//     }
//     KConfigDialog *dialog = new KConfigDialog( this, "settings", Settings::self() );
//     QWidget *generalSettingsDlg = new QWidget;
//     ui_prefs_base.setupUi(generalSettingsDlg);
//     dialog->addPage(generalSettingsDlg, i18n("General"), "package_setting");
//     connect(dialog, SIGNAL(settingsChanged(QString)), m_view, SLOT(settingsChanged()));
//     dialog->setAttribute( Qt::WA_DeleteOnClose );
//     dialog->show();
}

void TimetableMate::toolsCheck() {
    const TimetableAccessorInfo *info = m_view->accessor()->info();
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

    kDebug() << "Check script" << scriptOk;
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

        QScriptSyntaxCheckResult syntax = QScriptEngine::checkSyntax( m_scriptDocument->text() );
        kDebug() << "SYNTAX:" << syntax.state() << syntax.errorLineNumber() << syntax.errorMessage();
        if ( syntax.state() == QScriptSyntaxCheckResult::Intermediate ||
             syntax.state() == QScriptSyntaxCheckResult::Error )
        {
            // Go to error line
            m_scriptDocument->activeView()->setCursorPosition(
                    KTextEditor::Cursor(syntax.errorLineNumber() - 1, syntax.errorColumnNumber()) );
            const QString errorString = i18nc("@info", "<emphasis>Error in script:</emphasis> "
                            "<message>%1</message>", syntax.errorMessage());
            if ( syntax.state() == QScriptSyntaxCheckResult::Intermediate ) {
                inelegants << errorString;
            } else if ( syntax.state() == QScriptSyntaxCheckResult::Error ) {
                errors << errorString;
            }
            scriptOk = false;
        }

        evaluateScript();
        if ( m_lastScriptError == NoScriptError ) {
            // Use function names found by own JavaScript parser
//             if ( m_context ) {
//                 QScriptValueIterator it( m_context->popScope() );
//                 while ( it.hasNext() ) {
//                     it.next();
//                     scriptFunctions << it.name();
//                 } // TODO
//             } else {
                scriptFunctions = m_javaScriptModel->functionNames();
//             }
            kDebug() << "Successfully loaded the script" << scriptFunctions;
        } else if ( m_debugger && m_debugger->engine()->isEvaluating() ) { // TODO state() == QScriptEngineDebugger::RunningState ) {
//             inelegants << i18nc("@info", "Script is running the debugger, there may be an error",
//                                 m_engine->uncaughtException().toString());
//             scriptOk = false;
            return;
        } else if ( m_engine->hasUncaughtException() ) {
            // Go to error line
            m_scriptDocument->activeView()->setCursorPosition(
                    KTextEditor::Cursor(m_engine->uncaughtExceptionLineNumber() - 1, 0) );
            errors << i18nc("@info", "<emphasis>Error in script:</emphasis> "
                            "<message>%1</message>", m_engine->uncaughtException().toString());
            scriptOk = false;
        } else {
            errors << i18nc("@info", "<emphasis>Error loading the script</emphasis>");
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
                                     m_scriptDocument->activeView()->cursorPosition().line() + 1, Function) );
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
                                     m_scriptDocument->activeView()->cursorPosition().line() + 1, Function) );
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
    const TimetableAccessorInfo *info = m_view->accessor()->info();
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
    DepartureRequestInfo requestInfo = getDepartureRequestInfo();
    TimetableAccessor *accessor = m_view->accessor();
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
    StopSuggestionRequestInfo requestInfo = getStopSuggestionRequestInfo();
    TimetableAccessor *accessor = m_view->accessor();
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
    JourneyRequestInfo requestInfo = getJourneyRequestInfo();
    TimetableAccessor *accessor = m_view->accessor();
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

bool TimetableMate::loadScript()
{
    // Do not load the script again if it was already loaded and was not changed since then
    if ( m_lastScriptError == NoScriptError && m_script && m_engine &&
         !m_scriptDocument->isModified() )
    {
        kDebug() << "Script was not modified";
        return true;
    }
    m_lastScriptError = NoScriptError;

    // Initialize the script
    const TimetableAccessorInfo *info = m_view->accessor()->info();
    kDebug() << "Reload script text" << info->scriptFileName();
    m_script = new QScriptProgram( m_scriptDocument->text(), info->scriptFileName() );

    createScriptEngine();

    // Process events every 100ms
//     m_engine->setProcessEventsInterval( 100 );

    // Register NetworkRequest class for use in the script
    qScriptRegisterMetaType< NetworkRequestPtr >( m_engine,
            networkRequestToScript, networkRequestFromScript );

    // Create objects for the script
    if ( !m_scriptHelper ) {
        m_scriptHelper = new Helper( info->serviceProvider(), m_engine );
        connect( m_scriptHelper, SIGNAL(errorReceived(QString,QString)),
                 this, SLOT(scriptErrorReceived(QString,QString)) );
    }

    if ( !m_scriptResult ) {
        m_scriptResult = new ResultObject( m_engine );
        connect( m_scriptResult, SIGNAL(publish()), this, SLOT(publish()) );
    } else {
        m_scriptResult->clear();
    }

    if ( !m_scriptNetwork ) {
        m_scriptNetwork = new Network( info->fallbackCharset(), m_engine );
    } else {
        m_scriptNetwork->clear();
    }

    if ( !m_scriptStorage ) {
        m_scriptStorage = new Storage( info->serviceProvider(), this );
    }

    // Expose objects to the script engine
    if ( !m_engine->globalObject().property("accessor").isValid() ) {
        m_engine->globalObject().setProperty( "accessor", m_engine->newQObject(
                const_cast<TimetableAccessorInfo*>(m_view->accessor()->info())) );
    }
    if ( !m_engine->globalObject().property("helper").isValid() ) {
        m_engine->globalObject().setProperty( "helper", m_engine->newQObject(m_scriptHelper) );
    }
    if ( !m_engine->globalObject().property("network").isValid() ) {
        m_engine->globalObject().setProperty( "network", m_engine->newQObject(m_scriptNetwork) );
    }
    if ( !m_engine->globalObject().property("storage").isValid() ) {
        m_engine->globalObject().setProperty( "storage", m_engine->newQObject(m_scriptStorage) );
    }
    if ( !m_engine->globalObject().property("result").isValid() ) {
        m_engine->globalObject().setProperty( "result", m_engine->newQObject(m_scriptResult) );
    }
    if ( !m_engine->globalObject().property("enum").isValid() ) {
        m_engine->globalObject().setProperty( "enum",
                m_engine->newQMetaObject(&ResultObject::staticMetaObject) );
    }

    // Import extensions (from XML file, <script extensions="...">)
    foreach ( const QString &extension, info->scriptExtensions() ) {
        if ( !importExtension(m_engine, extension) ) {
            m_lastScriptError = ScriptLoadFailed;
            return false;
        }
    }

    // Load the script program in another context
//     m_context = m_engine->pushContext();
    m_engine->clearExceptions();
    m_engine->evaluate( *m_script/*->sourceCode(), info->fileName()*/ );

    const QString functionName = TimetableAccessorScript::SCRIPT_FUNCTION_GETTIMETABLE;
    QScriptValue function = m_engine->globalObject().property( functionName );
    if ( !function.isFunction() ) {
        kDebug() << "Did not find" << functionName << "function in the script!";
    }

    if ( m_engine->hasUncaughtException() ) {
        kDebug() << "Error in the script" << m_engine->uncaughtExceptionLineNumber()
                 << m_engine->uncaughtException().toString();
        kDebug() << "Backtrace:" << m_engine->uncaughtExceptionBacktrace().join("\n");
        m_lastError = i18nc("@info/plain", "Error in the script: "
                "<message>%1</message>.", m_engine->uncaughtException().toString());
        m_lastScriptError = ScriptLoadFailed;
        return false;
    } else {
        m_lastScriptError = NoScriptError;
        return true;
    }
}

void TimetableMate::createScriptEngine()
{
    if ( m_engine ) {
//         if ( m_debugger ) {
//             m_debugger->detach();
//             m_debugger->standardWindow()->close();
//             delete m_debugger;
//             m_debugger = 0;
//         }

//         m_engine->abortEvaluation();
//         delete m_engine;
        kDebug() << "Use existing QScriptEngine";
    } else {
        kDebug() << "Create QScriptEngine";
        m_engine = new QScriptEngine( this );

        // Setup debugger
        setupDebugger();
    }
}

void TimetableMate::evaluateScript()
{
    loadScript();
}

void TimetableMate::scriptErrorReceived( const QString &message, const QString &failedParseText )
{
    m_scriptErrors << message;
}

bool TimetableMate::scriptRun( const QString &functionToRun, const RequestInfo *requestInfo,
                               const TimetableAccessorInfo *info,
                               ResultObject *resultObject, QVariant *result ) {
//     if ( !m_engine ) {
        evaluateScript();
//     }
    if ( m_lastScriptError != NoScriptError ) {
        kDebug() << "Script could not be loaded correctly";
        return false;
    }
    kDebug() << "Run script job";
    kDebug() << "Values:" << requestInfo->stop << requestInfo->dateTime << m_scriptNetwork;
//     emit begin( m_sourceName );

    // Store start time of the script
    QTime time;
    time.start();

    // Add call to the appropriate function
    QString functionName;
    QScriptValueList arguments = QScriptValueList() << requestInfo->toScriptValue( m_engine );
    switch ( requestInfo->parseMode ) {
    case ParseForDeparturesArrivals:
        functionName = TimetableAccessorScript::SCRIPT_FUNCTION_GETTIMETABLE;
        break;
    case ParseForJourneys:
        functionName = TimetableAccessorScript::SCRIPT_FUNCTION_GETJOURNEYS;
        break;
    case ParseForStopSuggestions:
        functionName = TimetableAccessorScript::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS;
        break;
    default:
        kDebug() << "Parse mode unsupported:" << requestInfo->parseMode;
        break;
    }

    if ( functionName.isEmpty() ) {
        // This should never happen, therefore no i18n
        m_lastError = "Unknown parse mode";
        m_lastScriptError = ScriptRunError;
    } else {
        m_scriptErrors.clear();

//         setupDebugger( InterruptOnExceptions );

        m_engine->evaluate( *m_script );
//         kDebug() << "TEST" << functionName << m_engine->globalObject().property( functionName ).toString();
        QScriptValue function = m_engine->globalObject().property( functionName );
        if ( !function.isFunction() ) {
            kDebug() << "Did not find" << functionName << "function in the script!";
//             return false;
        }
//         m_debugger->action( QScriptEngineDebugger::InterruptAction )->trigger(); // TODO

        // Call script function
//         setupDebugger( InterruptAtStart );
        kDebug() << "Call script function";
        QScriptValue result = function.call( QScriptValue(), arguments );
        kDebug() << "calling done..";
        if ( m_engine->hasUncaughtException() ) {
            kDebug() << "Error in the script when calling function" << functionName
                        << m_engine->uncaughtExceptionLineNumber()
                        << m_engine->uncaughtException().toString();
            kDebug() << "Backtrace:" << m_engine->uncaughtExceptionBacktrace().join("\n");
            m_lastError = i18nc("@info/plain", "Error in the script when calling function '%1': "
                    "<message>%2</message>.", functionName, m_engine->uncaughtException().toString());
            m_scriptResult->clear();
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
//             } TODO REMOVE
//         }

//        TODO Only if no debugger is attached
//         while ( m_scriptNetwork->hasRunningRequests() || m_engine->isEvaluating() ) {
//             // Wait for running requests to finish
//             // TODO Blocking the GUI?x
//             QEventLoop eventLoop;
// //             ScriptAgent agent( m_engine ); Cannot because of the debugger
//             QTimer::singleShot( 5000, &eventLoop, SLOT(quit()) );
//             connect( this, SIGNAL(destroyed(QObject*)), &eventLoop, SLOT(quit()) );
// //             connect( &agent, SIGNAL(scriptFinished()), &eventLoop, SLOT(quit()) );
//             kDebug() << "m_scriptNetwork:" << m_scriptNetwork;
//             connect( m_scriptNetwork, SIGNAL(requestFinished()), &eventLoop, SLOT(quit()) );
//
//             kDebug() << "Waiting for script to finish..." << thread();
//             eventLoop.exec();
//         }
        kDebug() << " > Script evaluated after" << (time.elapsed() / 1000.0)
                    << "seconds: " << /*m_scriptFileName <<*/ requestInfo->parseMode;
        QApplication::processEvents();
        kDebug() << " > Processed events";

        // Inform about script run time

//         // If data for the current job has already been published, do not emit
//         // completed with an empty resultset
// //         if ( m_published == 0 || m_scriptResult->count() > m_published ) {
// //             const bool couldNeedForcedUpdate = m_published > 0;
// //             emitReady(
//             switch ( requestInfo->parseMode ) {
//             case ParseForDeparturesArrivals:
//                 emit departuresReady( m_scriptResult->data(),//.mid(m_published),
//                         m_scriptResult->features(), m_scriptResult->hints(),
//                         m_scriptNetwork->lastUrl(), globalInfo,
//                         *dynamic_cast<const DepartureRequestInfo*>(requestInfo) );//,
// //                         couldNeedForcedUpdate );
//                 break;
//             case ParseForJourneys:
//                 emit journeysReady( m_scriptResult->data(),//.mid(m_published),
//                         m_scriptResult->features(), m_scriptResult->hints(),
//                         m_scriptNetwork->lastUrl(), globalInfo,
//                         *dynamic_cast<const JourneyRequestInfo*>(requestInfo) );//,
// //                         couldNeedForcedUpdate );
//                 break;
//             case ParseForStopSuggestions:
//                 emit stopSuggestionsReady( m_scriptResult->data(),//.mid(m_published),
//                         m_scriptResult->features(), m_scriptResult->hints(),
//                         m_scriptNetwork->lastUrl(), globalInfo,
//                         *dynamic_cast<const StopSuggestionRequestInfo*>(requestInfo) );//,
// //                         couldNeedForcedUpdate );
//                 break;
//
//             default:
//                 kDebug() << "Parse mode unsupported:" << requestInfo->parseMode;
//                 // TODO
//                 break;
//             }
// //             emit dataReady( m_scriptResult->data().mid(m_published),
// //                             m_scriptResult->features(), m_scriptResult->hints(),
// //                             m_scriptNetwork->lastUrl(), globalInfo,
// //                             requestInfo, couldNeedForcedUpdate );
// //         }
// //         emit end( m_sourceName );

        // Cleanup
//         m_scriptResult->clear();
        m_scriptStorage->checkLifetime();

        if ( m_engine->hasUncaughtException() ) {
            kDebug() << "Error in the script when calling function" << functionName
                        << m_engine->uncaughtExceptionLineNumber()
                        << m_engine->uncaughtException().toString();
            kDebug() << "Backtrace:" << m_engine->uncaughtExceptionBacktrace().join("\n");
            m_lastError = i18nc("@info/plain", "Error in the script when calling function '%1': "
                    "<message>%2</message>.", functionName, m_engine->uncaughtException().toString());
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

DepartureRequestInfo TimetableMate::getDepartureRequestInfo() {
    TimetableAccessor *accessor = m_view->accessor();
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

    DepartureRequestInfo info;
    if ( dialog->exec() == KDialog::Accepted ) {
        info.city = city ? city->text() : QString();
        info.stop = stop->text();
        info.dateTime = dateTime->dateTime();
        info.dataType = dataType->itemData( dataType->currentIndex() ).toString();
    }
    delete dialog;
    return info;
}

JourneyRequestInfo TimetableMate::getJourneyRequestInfo()
{
    TimetableAccessor *accessor = m_view->accessor();
    QPointer<KDialog> dialog = new KDialog( this );
    QWidget *w = new QWidget( dialog );
    QFormLayout *l = new QFormLayout( w );
    KLineEdit *city = NULL;
    KLineEdit *originStop = new KLineEdit( w );
    KLineEdit *targetStop = new KLineEdit( w );
    KComboBox *dataType = new KComboBox( w );
    KDateTimeWidget *dateTime = new KDateTimeWidget( QDateTime::currentDateTime(), w );
    dataType->addItem( i18nc("@info/plain", "Departing at Given Time"), "dep" );
    dataType->addItem( i18nc("@info/plain", "Arriving at Given Time"), "arr" );
    if ( accessor->info()->useSeparateCityValue() ) {
        city = new KLineEdit( w );
        l->addRow( i18nc("@info", "City:"), city );
    }
    l->addRow( i18nc("@info", "Start Stop Name:"), originStop );
    l->addRow( i18nc("@info", "Target Stop Name:"), targetStop );
    l->addRow( i18nc("@info", "Time:"), dateTime );
    l->addRow( i18nc("@info", "Meaning of Time:"), dataType );
    dialog->setMainWidget( w );
    originStop->setFocus();

    JourneyRequestInfo info;
    if ( dialog->exec() == KDialog::Accepted ) {
        info.city = city ? city->text() : QString();
        info.stop = originStop->text();
        info.targetStop = targetStop->text();
        info.dateTime = dateTime->dateTime();
        info.dataType = dataType->itemData( dataType->currentIndex() ).toString();
    }
    delete dialog;

    return info;
}

StopSuggestionRequestInfo TimetableMate::getStopSuggestionRequestInfo()
{
    TimetableAccessor *accessor = m_view->accessor();
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

    StopSuggestionRequestInfo info;
    if ( dialog->exec() == KDialog::Accepted ) {
        info.city = city ? city->text() : QString();
        info.stop = stop->text();
    }
    delete dialog;

    return info;
}

#include "timetablemate.moc"
