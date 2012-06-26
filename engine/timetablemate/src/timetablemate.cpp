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
#include "timetablemate.h"

// Own includes
#include "project.h"
#include "projectmodel.h"
#include "ui_preferences.h"
#include "settings.h"

// Debugger includes
#include "debugger/debugger.h"
#include "debugger/variablemodel.h"

// Tab widgets
#include "tabs/abstracttab.h"
#include "tabs/dashboardtab.h"
#include "tabs/projectsourcetab.h"
#include "tabs/scripttab.h"
#include "tabs/webtab.h"
#include "tabs/plasmapreviewtab.h"

// Dock widgets
#include "docks/docktoolbar.h"
#include "docks/projectsdockwidget.h"
#include "docks/consoledockwidget.h"
#include "docks/documentationdockwidget.h"
#include "docks/variablesdockwidget.h"
#include "docks/outputdockwidget.h"
#include "docks/breakpointdockwidget.h"
#include "docks/backtracedockwidget.h"
#include "docks/consoledockwidget.h"
#include "docks/testdockwidget.h"
#include "docks/webinspectordockwidget.h"
#include "docks/networkmonitordockwidget.h"

// PublicTransport engine includes
#include <engine/serviceprovider.h>
#include <engine/serviceproviderdata.h>
#include <engine/serviceproviderglobal.h>

// KDE includes
#include <KGlobalSettings>
#include <KStandardDirs>
#include <KMenu>
#include <KMenuBar>
#include <KToolBar>
#include <KStatusBar>
#include <KFileDialog>
#include <KInputDialog>
#include <KConfigDialog>
#include <KMessageBox>
#include <KMessageWidget>
#include <KTabWidget>
#include <KWebView>
#include <KUrlComboBox>
#include <KLocale>
#include <KLocalizedString>
#include <KStandardShortcut>
#include <KActionCollection>
#include <KAction>
#include <KActionMenu>
#include <KStandardAction>
#include <KRecentFilesAction>
#include <KToggleAction>
#include <KParts/PartManager>
#include <KParts/MainWindow>
#include <KTextEditor/Document>
#include <KTextEditor/View>

// Qt includes
#include <QtGui/QFormLayout>
#include <QtGui/QTreeView>
#include <QtGui/QBoxLayout>
#include <QtGui/QKeyEvent>
#include <QtCore/QTimer>
#include <QtWebKit/QWebInspector>

#include <unistd.h> // For KStandardDirs::checkAccess(), W_OK, in TimetableMate::fileOpenInstalled()

// This function returns all actions that get connected to the currently active project
// in TimetableMate::activeProjectAboutToChange(). These actions are proxy actions for the actions
// inside the different projects and are added to the main TimetableMate UI (extern to the
// projects). They are stored in the KActionCollection as Project::projectActionName().
const QList< Project::ProjectAction > externProjectActions() {
    return QList< Project::ProjectAction >()
            << Project::Save << Project::SaveAs << Project::Install << Project::InstallGlobally
            << Project::ShowProjectSettings << Project::Close << Project::ShowHomepage
            << Project::RunAllTests << Project::AbortRunningTests << Project::ClearTestResults
            << Project::RunMenuAction << Project::DebugMenuAction << Project::StepInto
            << Project::StepOver << Project::StepOut << Project::RunToCursor << Project::Interrupt
            << Project::Continue << Project::AbortDebugger
            << Project::ToggleBreakpoint << Project::RemoveAllBreakpoints;
}

void moveContainer( KXMLGUIClient *client, const QString &tagname, const QString &name,
                    const QString &to_name, bool recursive )
{
    QDomDocument doc = client->xmlguiBuildDocument ();
    if  (doc.documentElement ().isNull ()) doc = client->domDocument ();

    // find the given elements
    QDomElement e = doc.documentElement ();

    QDomElement from_elem;
    QDomElement to_elem;

    QDomNodeList list = e.elementsByTagName (tagname);
    int count = list.count ();
    for (int i = 0; i < count; ++i) {
        QDomElement elem = list.item (i).toElement ();
        if (elem.isNull ()) continue;
        if (elem.attribute ("name") == name) {
            from_elem = elem;
        } else if (elem.attribute ("name") == to_name) {
            to_elem = elem;
        }
    }

    // move
    from_elem.parentNode ().removeChild (from_elem);
    to_elem.appendChild (from_elem);

    // set result
    client->setXMLGUIBuildDocument (doc);

    // recurse
    if (recursive) {
        QList<KXMLGUIClient*> children = client->childClients ();
        QList<KXMLGUIClient*>::const_iterator it;
        for (it = children.constBegin (); it != children.constEnd (); ++it) {
            moveContainer (*it, tagname, name, to_name, true);
        }
    }
}

TimetableMate::TimetableMate() : KParts::MainWindow( 0, Qt::WindowContextHelpButtonHint ),
        ui_preferences(0), m_projectModel(0), m_partManager(0),
        m_tabWidget(new KTabWidget(this)),
        m_leftDockBar(0), m_rightDockBar(0), m_bottomDockBar(0),
        m_backtraceDock(0), m_consoleDock(0), m_outputDock(0), m_breakpointDock(0),
        m_variablesDock(0), m_documentationDock(0), m_projectsDock(0), m_testDock(0),
        m_webInspectorDock(0), m_networkMonitorDock(0), m_showDocksAction(0),
        m_toolbarAction(0), m_statusbarAction(0), m_recentFilesAction(0), m_currentTab(0),
        m_messageWidgetLayout(new QVBoxLayout())
{
    m_partManager = new KParts::PartManager( this );
    m_tabWidget->setDocumentMode( true );
    m_tabWidget->setAutomaticResizeTabs( true );
    m_tabWidget->setMovable( true );
    m_tabWidget->setTabsClosable( true );

    QWidget *widget = new QWidget( this );
    widget->setMinimumSize( 220, 200 );
    QVBoxLayout *mainLayout = new QVBoxLayout( widget );
    mainLayout->setContentsMargins( 0, 0, 0, 0 );
    m_messageWidgetLayout->setContentsMargins( 0, 0, 0, 0 );
    mainLayout->addWidget( m_tabWidget );
    mainLayout->addLayout( m_messageWidgetLayout );
    setCentralWidget( widget );

    // Connect signals
    connect( m_partManager, SIGNAL(activePartChanged(KParts::Part*)),
             this, SLOT(activePartChanged(KParts::Part*)) );
    connect( m_tabWidget, SIGNAL(tabCloseRequested(int)),
             this, SLOT(tabCloseRequested(int)) );
    connect( m_tabWidget, SIGNAL(currentChanged(int)),
             this, SLOT(currentTabChanged(int)) );
    connect( m_tabWidget, SIGNAL(contextMenu(QWidget*,QPoint)),
             this, SLOT(tabContextMenu(QWidget*,QPoint)) );

    // Create project model
    m_projectModel = new ProjectModel( this );
    connect( m_projectModel, SIGNAL(activeProjectAboutToChange(Project*,Project*)),
             this, SLOT(activeProjectAboutToChange(Project*,Project*)) );
    connect( m_projectModel, SIGNAL(projectAdded(Project*)), this, SLOT(projectAdded(Project*)) );
    connect( m_projectModel, SIGNAL(projectAboutToBeRemoved(Project*)),
             this, SLOT(projectAboutToBeRemoved(Project*)) );

    Settings::self()->readConfig();
    setupActions();
    setupDockWidgets();
    setupGUI();
    if ( !fixMenus() ) {
        int result = KMessageBox::warningContinueCancel( this,
                i18nc("@info", "<title>Initialization Error</title>"
                      "<para>There seems to be a problem with your installation. The UI will not "
                      "be complete and there may be errors if you continue now.</para>"
                      "<para><emphasis strong='1'>Possible Solution:</emphasis> "
                      "Please reinstall TimetableMate and try again.</para>"),
                i18nc("@title:window", "Error"), KStandardGuiItem::cont(), KStandardGuiItem::quit(),
                QString(), KMessageBox::Notify | KMessageBox::Dangerous );
        if ( result != KMessageBox::Continue ) {
            QApplication::quit();
            deleteLater();
            return;
        }
    }
    populateTestMenu();

    // Create fixed dock overview toolbars after setupGUI()
    m_leftDockBar = new DockToolBar(
            Qt::LeftDockWidgetArea, "leftDockBar", m_showDocksAction, this );
    m_rightDockBar = new DockToolBar(
            Qt::RightDockWidgetArea, "rightDockBar", m_showDocksAction, this );
    m_bottomDockBar = new DockToolBar(
            Qt::BottomDockWidgetArea, "bottomDockBar", m_showDocksAction, this );

    QList<QAction*> dockToggleActions;
    dockToggleActions << action("toggle_dock_projects")     << action("toggle_dock_variables")
                      << action("toggle_dock_test")         << action("toggle_dock_console")
                      << action("toggle_dock_breakpoints")  << action("toggle_dock_backtrace")
                      << action("toggle_dock_output")       << action("toggle_dock_documentation")
                      << action("toggle_dock_webinspector") << action("toggle_dock_networkmonitor");
    foreach ( QAction *action, dockToggleActions ) {
        DockToolButtonAction *dockAction = qobject_cast< DockToolButtonAction* >( action );
        if ( !dockAction ) {
            continue;
        }

        Qt::DockWidgetArea area = dockWidgetArea( dockAction->dockWidget() );
        switch ( area ) {
        case Qt::LeftDockWidgetArea:
            m_leftDockBar->addAction( dockAction );
            break;
        case Qt::RightDockWidgetArea:
            m_rightDockBar->addAction( dockAction );
            break;
        case Qt::BottomDockWidgetArea:
            m_bottomDockBar->addAction( dockAction );
            break;
        default:
            kWarning() << "Top dock widget area is not supported";
            break;
        }

        connect( dockAction->dockWidget(), SIGNAL(dockLocationChanged(Qt::DockWidgetArea)),
                 this, SLOT(dockLocationChanged(Qt::DockWidgetArea)) );
    }

    addToolBar( Qt::LeftToolBarArea, m_leftDockBar );
    addToolBar( Qt::RightToolBarArea, m_rightDockBar );
    addToolBar( Qt::BottomToolBarArea, m_bottomDockBar );

    // Ensure the projects dock is visible on program start (if it was created)
    if ( m_projectsDock && !m_projectsDock->isVisible() ) {
        m_projectsDock->show();
    }

    // Set initial states
    stateChanged( "script_tab_is_active", StateReverse );

    QTimer::singleShot( 0, this, SLOT(initialize()) );
}

TimetableMate::~TimetableMate() {
    delete ui_preferences;
}

void TimetableMate::saveProperties( KConfigGroup &config )
{
    m_recentFilesAction->saveEntries( config );

    QStringList openedProjects;
    for ( int row = 0; row < m_projectModel->rowCount(); ++ row ) {
        Project *project = m_projectModel->projectItemFromRow( row )->project();
        const QString filePath = project->filePath();
        if ( !filePath.isEmpty() ) {
            QStringList openedTabs;
            const QList< TabType > allTabs = QList< TabType >()
                    << Tabs::Dashboard << Tabs::Script << Tabs::ProjectSource
                    << Tabs::PlasmaPreview << Tabs::Web;
            foreach ( TabType tab, allTabs ) {
                if ( project->isTabOpened(tab) ) {
                    openedTabs << QString::number(static_cast<int>(tab));
                }
            }
            QString projectString = QString("%1 ::%2").arg( filePath ).arg( openedTabs.join(",") );
            openedProjects << projectString;
        }
    }
//     Settings::setLastOpenedProjects( openedProjects );
//     Settings::self()->writeConfig();
    config.writeEntry( "lastOpenedProjects", openedProjects );
}

void TimetableMate::readProperties( const KConfigGroup &config )
{
    m_recentFilesAction->loadEntries( config );

    const QStringList lastOpenedProjects = config.readEntry( "lastOpenedProjects", QStringList() );
    QStringList failedToOpenProjects;
    foreach ( const QString &lastOpenedProject, lastOpenedProjects ) {
        const int pos = lastOpenedProject.indexOf( QLatin1String(" ::") );
        QString xmlFilePath;
        QList< TabType > openedTabs;
        if ( pos == -1 ) {
            xmlFilePath = lastOpenedProject;
        } else {
            xmlFilePath = lastOpenedProject.left( pos );
            const QStringList openedTabStrings = lastOpenedProject.mid( pos + 3 ).split(',');
            foreach ( const QString &openedTabString, openedTabStrings ) {
                openedTabs << static_cast< TabType >( openedTabString.toInt() );
            }
        }
        Project *project = openProject( xmlFilePath );
        if ( project ) {
            foreach ( TabType tab, openedTabs ) {
                project->showTab( tab );
            }
        } else {
            failedToOpenProjects << xmlFilePath;
        }
    }

    // Show an information message box, if projects failed to open
    if ( !failedToOpenProjects.isEmpty() ) {
        KMessageBox::informationList( this,
                i18nc("@info", "The following projects could not be opened"),
                failedToOpenProjects, i18nc("@title:window", "Failed to Open"),
                "couldNotOpenLastProjects" );
    }

    if ( m_projectModel->rowCount() == 0 ) {
        // Add a new template project if no project was opened
        fileNew();
    } else if ( m_tabWidget->count() == 0 ) {
        // Show dashboard of the active project, if no tabs were restored
        m_projectModel->activeProject()->showDashboardTab();
    }
}

void TimetableMate::initialize()
{
    updateShownDocksAction();

    if ( Settings::self()->restoreProjects() &&
         Settings::self()->config()->hasGroup("last_session") )
    {
        KConfigGroup config = Settings::self()->config()->group("last_session");
        readProperties( config );
    }
}

void TimetableMate::populateTestMenu()
{
    // Fill test action list with menu actions for each test case
    m_testCaseActions.clear();
    for ( int i = 0; i < TestModel::TestCaseCount; ++i ) {
        const TestModel::TestCase testCase = static_cast<TestModel::TestCase>( i );
        QAction *action = Project::createProjectAction( Project::SpecificTestCaseMenuAction,
                QVariant::fromValue(static_cast<int>(testCase)), this );
        m_testCaseActions << action;
    }
    plugActionList( "test_list", m_testCaseActions );
}

void TimetableMate::connectTestMenuWithProject( Project *project, bool doConnect )
{
    if ( !project ) {
        return;
    }

    foreach ( QAction *action, m_testCaseActions ) {
        const Project::ProjectActionData data = project->projectActionData( action );
        project->connectProjectAction( data.actionType, action, doConnect );
    }
}

QAction *TimetableMate::createCustomElement( QWidget *parent, int index, const QDomElement &element )
{
    QAction* before = 0L;
    if ( index > 0 && index < parent->actions().count() ) {
        before = parent->actions().at( index );
    }

    // Copied from KDevelop, menubar separators need to be defined as <Separator style="visible" />
    // and to be always shown in the menubar. For those, we create special disabled actions
    // instead of calling QMenuBar::addSeparator() because menubar separators are ignored
    if ( element.tagName().toLower() == QLatin1String("separator") &&
         element.attribute("style") == QLatin1String("visible") )
    {
        if ( QMenuBar* bar = qobject_cast<QMenuBar*>(parent) ) {
            QAction *separatorAction = new QAction( "|", this );
            bar->insertAction( before, separatorAction );
            separatorAction->setDisabled( true );
            separatorAction->setObjectName( element.attribute("name") );
            return separatorAction;
        }
    }

    return KXMLGUIBuilder::createCustomElement( parent, index, element );
}

bool TimetableMate::fixMenus()
{
    const QList< QAction* > menuBarActions = menuBar()->actions();
    QHash< QString, QAction* > menus;
    foreach ( QAction *menuBarAction, menuBarActions ) {
        menus.insert( menuBarAction->objectName(), menuBarAction );
    }

    // Show the file menu only when it is not empty
    QAction *fileMenu = menus["file"];
    if ( fileMenu ) {
        fileMenu->setVisible( !fileMenu->menu()->isEmpty() );
    }

    // Show the separator after the part menus only when part menus are there
    QAction *separatorPartMenusEnd = menus["separator_part_menus_end"];
    if ( separatorPartMenusEnd ) {
        separatorPartMenusEnd->setVisible( fileMenu->isVisible() && menus["edit"] &&
                                           menus["view"] && menus["tools"] && menus["bookmarks"] );
    } else {
        kWarning() << "Missing separator_part_menus_end, timetablemateui.rc not installed?";
    }

    QAction *editMenu = menus["edit"];
    if ( editMenu ) {
        const QList< QAction* > actions = editMenu->menu()->actions();
        foreach ( QAction *action, actions ) {
            if ( action->objectName() == QLatin1String("edit_undo") ||
                 action->objectName() == QLatin1String("edit_redo") )
            {
                action->setPriority( QAction::LowPriority );
            }
        }
    }

    // If the "separator_part_menus_end" menu bar item cannot be found, assume that
    // timetablemateui.rc was not installed and return false
    return separatorPartMenusEnd;
}

void TimetableMate::testActionTriggered()
{
    if ( m_projectModel->activeProject() ) {
        QAction *action = qobject_cast< QAction* >( sender() );
        const TestModel::Test test = static_cast< TestModel::Test >( action->data().toInt() );
        m_projectModel->activeProject()->startTest( test );
    }
}

void TimetableMate::testCaseActionTriggered()
{
    if ( m_projectModel->activeProject() ) {
        QAction *action = qobject_cast< QAction* >( sender() );
        const TestModel::TestCase testCase = static_cast< TestModel::TestCase >( action->data().toInt() );
        m_projectModel->activeProject()->startTestCase( testCase );
    }
}

void TimetableMate::dockLocationChanged( Qt::DockWidgetArea area )
{
    QDockWidget *dockWidget = qobject_cast< QDockWidget* >( sender() );
    Q_ASSERT( dockWidget );

    // Find the action to toggle the dock widget in one of the three dock bars and remove it
    QAction *toggleAction = m_leftDockBar->actionForDockWidget( dockWidget );
    if ( toggleAction ) {
        m_leftDockBar->removeAction( toggleAction );
    } else {
        toggleAction = m_rightDockBar->actionForDockWidget( dockWidget );
        if ( toggleAction ) {
            m_rightDockBar->removeAction( toggleAction );
        } else {
            toggleAction = m_bottomDockBar->actionForDockWidget( dockWidget );
            if ( toggleAction ) {
                m_bottomDockBar->removeAction( toggleAction );
            } else {
                kDebug() << "Action not found for dock widget" << dockWidget;
                return;
            }
        }
    }

    // Add the found dock widget toggle action to the dock bar for the new area
    if ( area == Qt::LeftDockWidgetArea ) {
        m_leftDockBar->addAction( toggleAction );
    } else if ( area == Qt::RightDockWidgetArea ) {
        m_rightDockBar->addAction( toggleAction );
    } else if ( area == Qt::BottomDockWidgetArea ) {
        m_bottomDockBar->addAction( toggleAction );
    } else {
        kDebug() << "Area is not allowed" << area;
    }

    updateShownDocksAction();
}

void TimetableMate::updateShownDocksAction()
{
    // Remove all actions, they will be inserted in new order below
    m_showDocksAction->removeAction( action("toggle_dock_projects") );
    m_showDocksAction->removeAction( action("toggle_dock_variables") );
    m_showDocksAction->removeAction( action("toggle_dock_documentation") );
    m_showDocksAction->removeAction( action("toggle_dock_console") );
    m_showDocksAction->removeAction( action("toggle_dock_breakpoints") );
    m_showDocksAction->removeAction( action("toggle_dock_backtrace") );
    m_showDocksAction->removeAction( action("toggle_dock_output") );
    m_showDocksAction->removeAction( action("toggle_dock_test") );
    m_showDocksAction->removeAction( action("toggle_dock_webinspector") );
    m_showDocksAction->removeAction( action("toggle_dock_networkmonitor") );

    // Delete remaining actions (titles, separators, hide actions)
    QList< QAction* > separators = m_showDocksAction->menu()->actions();
    foreach ( QAction *action, separators ) {
        m_showDocksAction->removeAction( action );
        delete action;
    }

    // Insert actions for the left dock area
    KMenu *menu = m_showDocksAction->menu();
    if ( !m_leftDockBar->actions().isEmpty() ) {
        menu->addTitle( i18nc("@title:menu In-menu title", "Left Dock Area") );
        menu->addActions( m_leftDockBar->actions() );

        // Add another action to the radio group to hide the dock area
        QAction *hideDockAction = menu->addAction( KIcon("edit-clear"),
                                                   i18nc("@action:inmenu", "&Hide Left Dock"),
                                                   m_leftDockBar, SLOT(hideCurrentDock()) );
        hideDockAction->setCheckable( true );
        if ( !m_leftDockBar->actionGroup()->checkedAction() ) {
            hideDockAction->setChecked( true );
        }
        m_leftDockBar->actionGroup()->addAction( hideDockAction );
    }

    // Insert actions for the bottom dock area
    if ( !m_bottomDockBar->actions().isEmpty() ) {
        menu->addTitle( i18nc("@title:menu In-menu title", "Bottom Dock Area") );
        menu->addActions( m_bottomDockBar->actions() );

        // Add another action to the radio group to hide the dock area
        QAction *hideDockAction = menu->addAction( KIcon("edit-clear"),
                                                   i18nc("@action:inmenu", "&Hide Bottom Dock"),
                                                   m_bottomDockBar, SLOT(hideCurrentDock()) );
        hideDockAction->setCheckable( true );
        if ( !m_bottomDockBar->actionGroup()->checkedAction() ) {
            hideDockAction->setChecked( true );
        }
        m_bottomDockBar->actionGroup()->addAction( hideDockAction );
    }

    // Insert actions for the right dock area (after a separator)
    if ( !m_rightDockBar->actions().isEmpty() ) {
        menu->addTitle( i18nc("@title:menu In-menu title", "Right Dock Area") );
        menu->addActions( m_rightDockBar->actions() );

        // Add another action to the radio group to hide the dock area
        QAction *hideDockAction = menu->addAction( KIcon("edit-clear"),
                                                   i18nc("@action:inmenu", "&Hide Right Dock"),
                                                   m_rightDockBar, SLOT(hideCurrentDock()) );
        hideDockAction->setCheckable( true );
        if ( !m_rightDockBar->actionGroup()->checkedAction() ) {
            hideDockAction->setChecked( true );
        }
        m_rightDockBar->actionGroup()->addAction( hideDockAction );
    }
}

void TimetableMate::setupDockWidgets()
{
    m_showDocksAction = new KActionMenu( i18nc("@action", "&Docks Shown"), this );
    actionCollection()->addAction( QLatin1String("options_show_docks"), m_showDocksAction );

    // Create dock widgets
    m_projectsDock = new ProjectsDockWidget( m_projectModel, m_showDocksAction, this );
    m_backtraceDock = new BacktraceDockWidget( m_projectModel, m_showDocksAction, this );
    m_breakpointDock = new BreakpointDockWidget( m_projectModel, m_showDocksAction, this );
    m_outputDock = new OutputDockWidget( m_projectModel, m_showDocksAction, this );
    m_consoleDock = new ConsoleDockWidget( m_projectModel, m_showDocksAction, this );
    m_variablesDock = new VariablesDockWidget( m_projectModel, m_showDocksAction, this );
    m_testDock = new TestDockWidget( m_projectModel, m_showDocksAction, this );
    m_documentationDock = new DocumentationDockWidget( m_showDocksAction, this );
    m_webInspectorDock = new WebInspectorDockWidget( m_showDocksAction, this );
    m_networkMonitorDock = new NetworkMonitorDockWidget( m_projectModel, m_showDocksAction, this );

    const QList< AbstractDockWidget* > allDockWidgets = QList< AbstractDockWidget* >()
            << m_projectsDock << m_backtraceDock << m_breakpointDock << m_outputDock
            << m_consoleDock << m_variablesDock << m_testDock << m_documentationDock
            << m_webInspectorDock << m_networkMonitorDock;
    foreach ( AbstractDockWidget *dockWidget, allDockWidgets ) {
        DockToolButtonAction *toggleAction = new DockToolButtonAction(
                dockWidget, dockWidget->icon(), dockWidget->windowTitle(), this );
        actionCollection()->addAction( "toggle_dock_" + dockWidget->objectName(), toggleAction );

        // Add dock widgets to default areas (stored changes to the areas are restored later)
        addDockWidget( dockWidget->defaultDockArea(), dockWidget );
    }
}

void TimetableMate::activeProjectAboutToChange( Project *project, Project *previousProject )
{
    // Enable "Save All" action only when at least one project is opened
    action("project_save_all")->setEnabled( project );

    if ( previousProject ) {
        // Disconnect previously active project
        foreach ( Project::ProjectAction projectAction, externProjectActions() ) {
            QAction *qaction = action( Project::projectActionName(projectAction) );
            previousProject->connectProjectAction( projectAction, qaction, false );
        }

        connectTestMenuWithProject( previousProject, false );

        disconnect( previousProject, SIGNAL(testStarted()), this, SLOT(testStarted()) );
        disconnect( previousProject, SIGNAL(testFinished(bool)), this, SLOT(testFinished(bool)) );

        Debugger::Debugger *debugger = previousProject->debugger();
        disconnect( debugger, SIGNAL(aborted()), this, SLOT(debugAborted()) );
        disconnect( debugger, SIGNAL(interrupted()), this, SLOT(debugInterrupted()) );
        disconnect( debugger, SIGNAL(continued()), this, SLOT(debugContinued()) );
        disconnect( debugger, SIGNAL(started()), this, SLOT(debugStarted()) );
        disconnect( debugger, SIGNAL(stopped()), this, SLOT(debugStopped()) );
        disconnect( debugger, SIGNAL(exception(int,QString)),
                    this, SLOT(uncaughtException(int,QString)) );
        disconnect( debugger, SIGNAL(breakpointReached(Breakpoint)),
                    this, SLOT(breakpointReached(Breakpoint)) );
        if ( m_testDock ) {
            disconnect( m_testDock, SIGNAL(clickedTestErrorItem(int,QString)),
                        previousProject, SLOT(showScriptLineNumber(int)) );
        }
        if ( m_backtraceDock ) {
            disconnect( m_backtraceDock, SIGNAL(activeFrameDepthChanged(int)),
                        debugger->variableModel(), SLOT(switchToVariableStack(int)) );
        }
    }

    if ( project ) {
        // Connect the new active project
        foreach ( Project::ProjectAction projectAction, externProjectActions() ) {
            QAction *qaction = action( Project::projectActionName(projectAction) );
            project->connectProjectAction( projectAction, qaction );
        }

        connectTestMenuWithProject( project );

        connect( project, SIGNAL(testStarted()), this, SLOT(testStarted()) );
        connect( project, SIGNAL(testFinished(bool)), this, SLOT(testFinished(bool)) );

        Debugger::Debugger *debugger = project->debugger();
        connect( debugger, SIGNAL(aborted()), this, SLOT(debugAborted()) );
        connect( debugger, SIGNAL(interrupted()), this, SLOT(debugInterrupted()) );
        connect( debugger, SIGNAL(continued(bool)), this, SLOT(debugContinued()) );
        connect( debugger, SIGNAL(started()), this, SLOT(debugStarted()) );
        connect( debugger, SIGNAL(stopped()), this, SLOT(debugStopped()) );
        connect( debugger, SIGNAL(exception(int,QString)),
                 this, SLOT(uncaughtException(int,QString)) );
        connect( debugger, SIGNAL(breakpointReached(Breakpoint)),
                 this, SLOT(breakpointReached(Breakpoint)) );
        if ( m_testDock ) {
            connect( m_testDock, SIGNAL(clickedTestErrorItem(int,QString)),
                     project, SLOT(showScriptLineNumber(int)) );
        }
        if ( m_backtraceDock ) {
            connect( m_backtraceDock, SIGNAL(activeFrameDepthChanged(int)),
                     debugger->variableModel(), SLOT(switchToVariableStack(int)) );
        }

        stateChanged( "project_opened" );
    } else {
        stateChanged( "no_project_opened" );
        stateChanged( "project_opened", StateReverse );
    }
}

bool TimetableMate::queryClose()
{
    // Save session properties into a special group in the configuration
    KConfigGroup config = Settings::self()->config()->group("last_session");
    saveProperties( config );

    // Close projects and ask to save if modified
    return closeAllProjects();
}

Project *TimetableMate::currentProject()
{
    // Get project of the currently shown tab
    AbstractTab *tab = projectTabAt( m_tabWidget->currentIndex() );
    return tab ? tab->project() : 0;
}

void TimetableMate::updateWindowTitle() {
    AbstractTab *tab = 0;
    QString caption;
    Project *project = m_projectModel->activeProject();

    // Start caption with the name of the current tab, if any
    if ( m_tabWidget->currentIndex() != -1 ) {
        tab = projectTabAt( m_tabWidget->currentIndex() );
        const ProjectModelItem::Type type =
                ProjectModelItem::projectItemTypeFromTabType( tab->type() );
        caption = m_projectModel->projectItemChildFromProject( project, type )->text();

        // Add project name
        caption += " - " + tab->project()->projectName();
    }

    // Add information about the test state
    if ( project->isTestRunning() ) {
        caption += " - " + i18nc("@info/plain", "Testing");
    }

    // Add information about the debugger state
    Debugger::Debugger *debugger = project->debugger();
    if ( debugger ) {
        if ( debugger->hasUncaughtException() ) { //m_engine->hasUncaughtException() ) {
            caption += " - " + i18nc("@info/plain", "Debugging (Exception in Line %1)",
                                     debugger->uncaughtExceptionLineNumber());
        } else if ( debugger->isInterrupted() ) {
            caption += " - " + i18nc("@info/plain", "Debugger Interrupted at Line %1", debugger->lineNumber());
        } else if ( debugger->isRunning() ) {
            caption += " - " + i18nc("@info/plain", "Debugger Running");
        }
    }

    setCaption( caption, tab ? tab->isModified() : false );
}

void TimetableMate::activePartChanged( KParts::Part *part ) {
    // Merge the GUI of the part, do not update while merging to avoid flicker
    setUpdatesEnabled( false );
    createGUI( part );
    setUpdatesEnabled( true );

    if ( part ) {
        // Manually hide actions of the part
        QStringList actionsToHide;
        actionsToHide << "tools_mode" << "tools_highlighting" << "tools_indentation";
        foreach ( QAction *action, menuBar()->actions() ) {
            KActionMenu *menuAction = static_cast<KActionMenu*>( action );
            const QList< QAction* > actions = menuAction->menu()->actions();
            for ( int i = actions.count() - 1; i >= 0; --i ) {
                QAction *curAction = actions.at( i );
                if ( curAction->parent() == actionCollection() )
                    continue; // Don't hide own actions

                if ( actionsToHide.contains(curAction->objectName()) ) {
                    curAction->setVisible( false );

                    actionsToHide.removeAt( i );
                    if ( actionsToHide.isEmpty() )
                        break;
                }
            }

            if ( actionsToHide.isEmpty() ) {
                break;
            }
        }
    }

    fixMenus();
}

AbstractTab *TimetableMate::projectTabAt( int index )
{
    return qobject_cast< AbstractTab* >( m_tabWidget->widget(index) );
}

bool TimetableMate::closeProject( Project *project )
{
    if ( closeAllTabs(project) ) {
        if ( project->isModified() ) {
            const QString message = i18nc("@info", "The project '%1' was modified. "
                                          "Do you want to save it now?");
            const int result = KMessageBox::warningYesNoCancel( this, message, QString(),
                    KStandardGuiItem::save(), KStandardGuiItem::close() );
            if ( result == KMessageBox::Yes ) {
                // Save clicked
                project->save( this );
                m_projectModel->removeProject( project );
                return !project->isModified();
            } else if ( result == KMessageBox::No ) {
                // Close clicked
                m_projectModel->removeProject( project );
                return true;
            } else {
                // Cancel clicked
                return false;
            }
        } else {
            m_projectModel->removeProject( project );
        }
        return true;
    } else {
        return false;
    }
}

bool TimetableMate::closeAllProjects()
{
    QStringList modifiedProjects;
    QList<ProjectModelItem*> projects = m_projectModel->projectItems();
    foreach ( const ProjectModelItem *project, projects ) {
        if ( project->project()->isModified() ) {
            modifiedProjects << project->project()->projectName();
        }
    }

    if ( modifiedProjects.isEmpty() ) {
        // No modified projects
        return true;
    }

    const QString message = i18nc("@info", "The following projects were modified. "
                                  "Do you want to save them now?");
    const int result = KMessageBox::warningYesNoCancelList( this, message, modifiedProjects,
            i18nc("@title:window", "Modified Projects"),
            KStandardGuiItem::save(), KStandardGuiItem::close() );
    if ( result == KMessageBox::Yes ) {
        // Save clicked
        foreach ( const ProjectModelItem *projectItem, projects ) {
            if ( projectItem->project()->isModified() ) {
                projectItem->project()->save( this );
                if ( projectItem->project()->isModified() ) {
                    // Still modified, error while saving
                    return false;
                }
            }
            closeAllTabs( projectItem->project(), false );
            m_projectModel->removeProject( projectItem->project() );
        }
        return true;
    } else if ( result == KMessageBox::No ) {
        // Close clicked
        closeAllTabs( 0, false );
        foreach ( const ProjectModelItem *projectItem, projects ) {
            m_projectModel->removeProject( projectItem->project() );
        }
        return true;
    } else {
        // Cancel clicked
        return false;
    }
}

void TimetableMate::contextMenuEvent( QContextMenuEvent *event )
{
    // Show "Shown Docks" action menu for context menus in empty menu bar space
    // and in main window splitters
    m_showDocksAction->menu()->exec( event->globalPos() );
}

void TimetableMate::tabContextMenu( QWidget *widget, const QPoint &pos )
{
    AbstractTab *tab = qobject_cast< AbstractTab* >( widget );
    tab->showTabContextMenu( pos );
}

void TimetableMate::closeCurrentTab()
{
    closeTab( projectTabAt(m_tabWidget->currentIndex()) );
}

void TimetableMate::closeTab( AbstractTab *tab )
{
    if ( !tab ) {
        kDebug() << "Tab was already closed";
        return;
    }

    if ( tab->isModified() ) {
        // Ask if modifications should be saved
        AbstractDocumentTab *documentTab = qobject_cast< AbstractDocumentTab* >( tab );
        const QString message = documentTab
                ? i18nc("@info", "The document was modified. Do you want to save it now?")
                : i18nc("@info", "Tab contents were modified. Do you want to save it now?");
        int result = KMessageBox::warningYesNoCancel( this, message, QString(),
                KStandardGuiItem::save(),
                documentTab ? KStandardGuiItem::closeDocument() : KStandardGuiItem::close() );
        if ( result == KMessageBox::Yes ) {
            if ( !tab->save() ) {
                // Do not close the tab if modifications could not be saved
                return;
            }
        } else if ( result == KMessageBox::Cancel ) {
            // Cancel clicked, do not close the tab
            return;
        } // else: No clicked, ie. do not save, but close the tab
    }

    switch ( tab->type() ) {
    case Tabs::Dashboard:
        dashboardTabAction( qobject_cast<DashboardTab*>(tab), CloseTab );
        break;
    case Tabs::ProjectSource:
        projectSourceTabAction( qobject_cast<ProjectSourceTab*>(tab), CloseTab );
        break;
    case Tabs::Script:
        scriptTabAction( qobject_cast<ScriptTab*>(tab), CloseTab );
        break;
    case Tabs::PlasmaPreview:
        plasmaPreviewTabAction( qobject_cast<PlasmaPreviewTab*>(tab), CloseTab );
        break;
    case Tabs::Web:
        webTabAction( qobject_cast<WebTab*>(tab), CloseTab );
        break;
    case Tabs::NoTab:
    default:
        break;
    }

    // Close the tab
    m_tabWidget->removeTab( m_tabWidget->indexOf(tab) );
//     currentTabChanged( m_tabWidget->currentIndex() );
    tab->deleteLater();
}

bool TimetableMate::closeAllTabsExcept( AbstractTab *tab, bool ask )
{
    return closeAllTabsExcept( 0, tab, ask );
}

bool TimetableMate::closeAllTabs( Project *project, bool ask )
{
    return closeAllTabsExcept( project, 0, ask );
}

bool TimetableMate::closeAllTabsExcept( Project *project, AbstractTab *except, bool ask )
{
    // Check for tabs with modified content documents
    QHash< QString, AbstractTab* > modifiedDocuments;
    for ( int i = 0; i < m_tabWidget->count(); ++i ) {
        AbstractTab *tab = projectTabAt( i );
        if ( (project && tab->project() != project) || (except && except == tab) ) {
            // If a project is given as argument only close tabs of that project and skip others
            // If a tab is given do not close that close
            continue;
        }

        if ( tab->isModified() ) {
            // Tab contents are modified, get a unique name
            const QString baseDocumentName = tab->fileName();
            QString documentName = baseDocumentName;
            int i = 1;
            if ( modifiedDocuments.contains(documentName) ) {
                documentName = QString("%1 (%2)").arg(baseDocumentName).arg(i);
            }
            modifiedDocuments.insert( documentName, tab );
        } else {
            // Tab contents unchanged, just close it
            m_tabWidget->removeTab( i );

            // Use deleteLater() because otherwise closing a tab from inside itself would cause
            // crash (also closing the project from inside a tab, because that closes the tab, too)
            tab->deleteLater();
            --i;
        }
    }

    // Unmodified tabs are now closed, check if modified tabs were found
    if ( modifiedDocuments.isEmpty() ) {
        return true;
    } else if ( ask ) {
        // Ask the user if modified documents should be saved
        const QString message = i18nc("@info", "The following documents were modified. "
                                      "Do you want to save them now?");
        KGuiItem saveAllItem( i18nc("@info/plain", "Save All"), KIcon("document-save-all") );
        KGuiItem doNotSaveItem( i18nc("@info/plain", "Do not Save"), KIcon("user-trash") );
        KGuiItem doNotCloseItem( i18nc("@info/plain", "Do not Close"), KIcon("dialog-cancel") );
        KDialog dialog( this );
        int result = KMessageBox::warningYesNoCancelList( this, message, modifiedDocuments.keys(),
                QString(), saveAllItem, doNotSaveItem, doNotCloseItem );

        if ( result == KMessageBox::Cancel ) {
            // "Do not Close" clicked
            return false;
        } else if ( result == KMessageBox::Yes ) {
            // "Save All" clicked
            bool allTabsClosed = true;
            for ( QHash<QString, AbstractTab*>::ConstIterator it = modifiedDocuments.constBegin();
                  it != modifiedDocuments.constEnd(); ++it )
            {
                if ( !(*it)->save() ) {
                    // Document could not be saved (eg. cancelled by the user),
                    // do not close the associated tab and return false at the end
                    allTabsClosed = false;
                    continue;
                }

                // Document successfully saved, close the tab
                m_tabWidget->removeTab( m_tabWidget->indexOf(*it) );
                delete *it;
            }

            // Return if all tabs are now closed
            return allTabsClosed;
        } else {
            // "Do not Save" clicked, close all tabs without saving
            for ( QHash<QString, AbstractTab*>::ConstIterator it = modifiedDocuments.constBegin();
                  it != modifiedDocuments.constEnd(); ++it )
            {
                // Close the tab
                m_tabWidget->removeTab( m_tabWidget->indexOf(*it) );
                delete *it;
            }
            return true;
        }
    } else {
        return false;
    }
}

void TimetableMate::currentTabChanged( int index ) {
    // Clear status bar messages
    statusBar()->showMessage( QString() );

    AbstractTab *tab = index == -1 ? 0 : qobject_cast<AbstractTab*>(m_tabWidget->widget(index));
    if ( tab && (tab->isProjectSourceTab() || tab->isScriptTab()) ) { // go to project source or script tab
        AbstractDocumentTab *documentTab = qobject_cast< AbstractDocumentTab* >( tab );
        Q_ASSERT( documentTab );
        if ( documentTab ) {
            if ( !m_partManager->parts().contains(documentTab->document()) ) {
                m_partManager->addPart( documentTab->document() );
            }
            documentTab->document()->activeView()->setFocus();
        }
    } else {
        m_partManager->setActivePart( 0 );
    }

    // Adjust if a dashboard tab was left or newly shown
    const bool leftDashboardTab = m_currentTab && m_currentTab->isDashboardTab();
    const bool movedToDashboardTab = tab && tab->isDashboardTab();
    if ( leftDashboardTab && !movedToDashboardTab ) {
        dashboardTabAction( qobject_cast<DashboardTab*>(m_currentTab), LeaveTab );
    } else if ( movedToDashboardTab && !leftDashboardTab ) {
        dashboardTabAction( qobject_cast<DashboardTab*>(tab), MoveToTab );
    }

    // Adjust if a project source tab was left or newly shown
    const bool leftProjectSourceTab = m_currentTab && m_currentTab->isProjectSourceTab();
    const bool movedToProjectSourceTab = tab && tab->isProjectSourceTab();
    if ( leftProjectSourceTab && !movedToProjectSourceTab ) {
        projectSourceTabAction( qobject_cast<ProjectSourceTab*>(m_currentTab), LeaveTab );
    } else if ( movedToProjectSourceTab && !leftProjectSourceTab ) {
        projectSourceTabAction( qobject_cast<ProjectSourceTab*>(tab), MoveToTab );
    }

    // Adjust if a plasma preview tab was left or newly shown
    const bool leftPlasmaPreviewTab = m_currentTab && m_currentTab->isPlasmaPreviewTab();
    const bool movedToPlasmaPreviewTab = tab && tab->isPlasmaPreviewTab();
    if ( leftPlasmaPreviewTab && !movedToPlasmaPreviewTab ) {
        plasmaPreviewTabAction( qobject_cast<PlasmaPreviewTab*>(m_currentTab), LeaveTab );
    } else if ( movedToPlasmaPreviewTab && !leftPlasmaPreviewTab ) {
        plasmaPreviewTabAction( qobject_cast<PlasmaPreviewTab*>(tab), MoveToTab );
    }

    // Adjust if a script tab was left or newly shown
    const bool leftScriptTab = m_currentTab && m_currentTab->isScriptTab();
    const bool movedToScriptTab = tab && tab->isScriptTab();
    if ( leftScriptTab && !movedToScriptTab ) {
        scriptTabAction( qobject_cast<ScriptTab*>(m_currentTab), LeaveTab );
    } else if ( movedToScriptTab && !leftScriptTab ) {
        scriptTabAction( qobject_cast<ScriptTab*>(tab), MoveToTab );
    }

    // Adjust if a web tab was left or newly shown
    const bool leftWebTab = m_currentTab && m_currentTab->isWebTab();
    const bool movedToWebTab = tab && tab->isWebTab();
    if ( leftWebTab && !movedToWebTab ) {
        webTabAction( qobject_cast<WebTab*>(m_currentTab), LeaveTab );
    } else if ( movedToWebTab && !leftWebTab ) {
        webTabAction( qobject_cast<WebTab*>(tab), MoveToTab );
    }

    if ( movedToWebTab ) {
        WebTab *webTab = qobject_cast< WebTab* >( tab );
        if ( m_webInspectorDock ) {
            m_webInspectorDock->setWebTab( webTab );
        }
    } else if ( leftWebTab ) {
        WebTab *webTab = qobject_cast< WebTab* >( m_currentTab );
        if ( m_webInspectorDock &&
             m_webInspectorDock->webInspector()->page() == webTab->webView()->page() )
        {
            // The web tab that was closed was connected to the web inspector dock widget
            m_webInspectorDock->setWebTab( 0 );
        }
    }

    // Store new tab and update window title
    m_currentTab = tab;
    updateWindowTitle();
}

void TimetableMate::dashboardTabAction( DashboardTab *dashboardTab, TimetableMate::TabAction tabAction )
{
    Q_UNUSED( dashboardTab );
    Q_UNUSED( tabAction );
}

void TimetableMate::projectSourceTabAction( ProjectSourceTab *projectSourceTab,
                                            TimetableMate::TabAction tabAction )
{
    if ( tabAction == CloseTab ) {
        m_partManager->removePart( projectSourceTab->document() );
    }
}

void TimetableMate::scriptTabAction( ScriptTab *scriptTab, TimetableMate::TabAction tabAction )
{
    if ( tabAction == MoveToTab ) {
        stateChanged( "script_tab_is_active" );
        connect( scriptTab, SIGNAL(canGoToPreviousFunctionChanged(bool)),
                 action("script_previous_function"), SLOT(setEnabled(bool)) );
        connect( scriptTab, SIGNAL(canGoToNextFunctionChanged(bool)),
                 action("script_next_function"), SLOT(setEnabled(bool)) );
    } else if ( tabAction == LeaveTab ) {
        stateChanged( "script_tab_is_active", StateReverse );
        disconnect( scriptTab, SIGNAL(canGoToPreviousFunctionChanged(bool)),
                    action("script_previous_function"), SLOT(setEnabled(bool)) );
        disconnect( scriptTab, SIGNAL(canGoToNextFunctionChanged(bool)),
                    action("script_next_function"), SLOT(setEnabled(bool)) );
    } else if ( tabAction == CloseTab ) {
        m_partManager->removePart( scriptTab->document() );
    }
}

void TimetableMate::plasmaPreviewTabAction( PlasmaPreviewTab *plasmaPreviewTab,
                                            TimetableMate::TabAction tabAction )
{
    Q_UNUSED( tabAction );
    Q_ASSERT( plasmaPreviewTab );
}

void TimetableMate::webTabAction( WebTab *webTab, TimetableMate::TabAction tabAction )
{
    if ( m_webInspectorDock ) {
        if ( tabAction == MoveToTab ) {
            m_webInspectorDock->setWebTab( webTab );
            webTab->urlBar()->setFocus();
        } else if ( tabAction == CloseTab ) {
            if ( m_webInspectorDock->webInspector()->page() == webTab->webView()->page() ) {
                // The web tab that was closed was connected to the web inspector dock widget
                m_webInspectorDock->setWebTab( 0 );
            }
        }
    }
}

void TimetableMate::setupActions() {
    KAction *newProject = new KAction( KIcon("project-development-new-template"),
                                       i18nc("@action", "New Project"), this );
    newProject->setPriority( QAction::LowPriority );
    KAction *openProject = new KAction( KIcon("project-open"),
                                        i18nc("@action", "Open Project"), this );
    openProject->setPriority( QAction::LowPriority );
    actionCollection()->addAction( QLatin1String("project_new"), newProject );
    actionCollection()->addAction( QLatin1String("project_open"), openProject );
    connect( newProject, SIGNAL(triggered(bool)), this, SLOT(fileNew()) );
    connect( openProject, SIGNAL(triggered(bool)), this, SLOT(fileOpen()) );

    KAction *saveAllProjects = new KAction( KIcon("document-save-all"),
                                            i18nc("@action", "Save All"), this );
    actionCollection()->addAction( QLatin1String("project_save_all"), saveAllProjects );
    connect( saveAllProjects, SIGNAL(triggered(bool)), this, SLOT(fileSaveAll()) );

    KStandardAction::quit( qApp, SLOT(closeAllWindows()), actionCollection() );
    KStandardAction::preferences( this, SLOT(optionsPreferences()), actionCollection() );
    m_recentFilesAction = KStandardAction::openRecent( this, SLOT(open(KUrl)), actionCollection() );
    actionCollection()->addAction( QLatin1String("project_open_recent"), m_recentFilesAction );
//     m_recentFilesAction->loadEntries( Settings::self()->config()->group(0) );

    KAction *openInstalled = new KAction( KIcon("document-open"), i18nc("@action",
                                          "Open I&nstalled..."), this );
    actionCollection()->addAction( QLatin1String("project_open_installed"), openInstalled );
    connect( openInstalled, SIGNAL(triggered(bool)), this, SLOT(fileOpenInstalled()) );

    KSelectAction *chooseActiveProject = new KSelectAction(
            KIcon("edit-select"), i18nc("@action", "&Active Project"), this );
    actionCollection()->addAction( QLatin1String("project_choose_active"), chooseActiveProject );

    // TODO Move to Project? => Project::ProjectAction
    KAction *scriptNextFunction = ScriptTab::createNextFunctionAction( this );
    actionCollection()->addAction( QLatin1String("script_next_function"), scriptNextFunction );
    scriptNextFunction->setVisible( false );
    connect( scriptNextFunction, SIGNAL(triggered(bool)),
             this, SLOT(scriptNextFunction()) );

    KAction *scriptPreviousFunction = ScriptTab::createPreviousFunctionAction( this );
    actionCollection()->addAction( QLatin1String("script_previous_function"), scriptPreviousFunction );
    scriptPreviousFunction->setVisible( false );
    connect( scriptPreviousFunction, SIGNAL(triggered(bool)),
             this, SLOT(scriptPreviousFunction()) );

    // Add project actions, they get connected to the currently active project
    // in activeProjectAboutToChange()
    foreach ( Project::ProjectAction projectAction, externProjectActions() ) {
        actionCollection()->addAction( Project::projectActionName(projectAction),
                                       Project::createProjectAction(projectAction, this) );
    }

    KAction *tabNext = new KAction( KIcon("go-next"), i18nc("@action", "Go to &Next Tab"), this );
    tabNext->setShortcut( KStandardShortcut::tabNext() );
    connect( tabNext, SIGNAL(triggered()), this, SLOT(tabNextActionTriggered()) );
    actionCollection()->addAction( QLatin1String("tab_next"), tabNext );

    KAction *tabPrevious = new KAction( KIcon("go-previous"), i18nc("@action", "Go to &Previous Tab"), this );
    tabPrevious->setShortcut( KStandardShortcut::tabPrev() );
    connect( tabPrevious, SIGNAL(triggered()), this, SLOT(tabPreviousActionTriggered()) );
    actionCollection()->addAction( QLatin1String("tab_previous"), tabPrevious );

    KAction *tabClose = new KAction( KIcon("tab-close"), i18nc("@action", "&Close Tab"), this );
    tabClose->setShortcut( KStandardShortcut::close() );
    connect( tabClose, SIGNAL(triggered()), this, SLOT(closeCurrentTab()) );
    actionCollection()->addAction( QLatin1String("tab_close"), tabClose );
}

void TimetableMate::tabNextActionTriggered()
{
    if ( m_tabWidget->currentIndex() + 1 < m_tabWidget->count() ) {
        m_tabWidget->setCurrentIndex( m_tabWidget->currentIndex() + 1 );
    } else if ( m_tabWidget->count() > 1 ) {
        // Was at last tab, go to the first tab
        m_tabWidget->setCurrentIndex( 0 );
    }
}

void TimetableMate::tabPreviousActionTriggered()
{
    if ( m_tabWidget->currentIndex() - 1 >= 0 ) {
        m_tabWidget->setCurrentIndex( m_tabWidget->currentIndex() - 1 );
    } else if ( m_tabWidget->count() > 1 ) {
        // Was at first tab, go to the last tab
        m_tabWidget->setCurrentIndex( m_tabWidget->count() - 1 );
    }
}

void TimetableMate::toggleBreakpoint()
{
    ScriptTab *scriptTab = qobject_cast< ScriptTab* >( m_currentTab );
    if ( scriptTab ) {
        scriptTab->toggleBreakpoint();
    }
}

void TimetableMate::scriptPreviousFunction()
{
    ScriptTab *scriptTab = qobject_cast< ScriptTab* >( m_currentTab );
    if ( scriptTab ) {
        scriptTab->goToPreviousFunction();
    }
}

void TimetableMate::scriptNextFunction()
{
    ScriptTab *scriptTab = qobject_cast< ScriptTab* >( m_currentTab );
    if ( scriptTab ) {
        scriptTab->goToNextFunction();
    }
}

void TimetableMate::breakpointReached( const Breakpoint &breakpoint )
{
    infoMessage( i18nc("@info/plain", "Reached breakpoint at %1", breakpoint.lineNumber()),
                 KMessageWidget::Information );
}

void TimetableMate::debugInterrupted()
{
    updateWindowTitle();
}

void TimetableMate::debugContinued()
{
//     Project *project = m_projectModel->activeProject();
//     Q_ASSERT( project ); // debugContinued is connected to the currently active project
//     setWindowTitle( i18nc("@window:title", "TimetableMate - %1 [*]",
//                           project->provider()->serviceProvider()) );
    updateWindowTitle();
}

void TimetableMate::debugAborted()
{
    updateWindowTitle();
}

void TimetableMate::debugStarted()
{
    updateWindowTitle();
}

void TimetableMate::debugStopped()
{
    updateWindowTitle();
}

void TimetableMate::testStarted()
{
    if ( m_testDock ) {
        m_testDock->show();
    }
    updateWindowTitle();
}

void TimetableMate::testFinished( bool success )
{
    Q_UNUSED( success );
    updateWindowTitle();
}

void TimetableMate::uncaughtException( int lineNumber, const QString &errorMessage )
{
    infoMessage( i18nc("@info", "Uncaught exception at %1: <message>%2</message>",
                       lineNumber, errorMessage), KMessageWidget::Error, -1 );
}

void TimetableMate::fileNew()
{
    Project *newProject = new Project( this );
    newProject->loadProject();
    m_projectModel->appendProject( newProject );
    newProject->showDashboardTab();
}

void TimetableMate::projectAdded( Project *project )
{
    // Expand project item
    if ( m_projectsDock ) {
        m_projectsDock->projectsWidget()->expand( m_projectModel->indexFromProject(project) );
    }

    // Connect new project
    connect( project, SIGNAL(tabTitleChanged(QWidget*,QString,QIcon)),
             this, SLOT(tabTitleChanged(QWidget*,QString,QIcon)) );
    connect( project, SIGNAL(testStarted()), this, SLOT(removeAllMessageWidgets()) );
    connect( project, SIGNAL(informationMessage(QString,KMessageWidget::MessageType,int,QList<QAction*>)),
             this, SLOT(infoMessage(QString,KMessageWidget::MessageType,int,QList<QAction*>)) );
    connect( project, SIGNAL(closeRequest()), this, SLOT(projectCloseRequest()) );
    connect( project, SIGNAL(tabCloseRequest(AbstractTab*)), this, SLOT(closeTab(AbstractTab*)) );
    connect( project, SIGNAL(otherTabsCloseRequest(AbstractTab*)),
             this, SLOT(closeAllTabsExcept(AbstractTab*)) );
    connect( project, SIGNAL(tabOpenRequest(AbstractTab*)), this, SLOT(tabOpenRequest(AbstractTab*)) );
    connect( project, SIGNAL(tabGoToRequest(AbstractTab*)), this, SLOT(tabGoToRequest(AbstractTab*)) );
    connect( project, SIGNAL(saveLocationChanged(QString,QString)),
             this, SLOT(projectSaveLocationChanged(QString,QString)) );

    KSelectAction *chooseActiveProject =
            qobject_cast< KSelectAction* >( action("project_choose_active") );
    if ( chooseActiveProject ) {
        // Create "Set as Active Project" action and use the project name/icon for it
        // instead of the default, which would mean that the chooseActiveProject action would
        // contain multiple actions with the same text/icon
        QAction *action = project->createProjectAction(
                Project::SetAsActiveProject, chooseActiveProject );
        action->setText( project->projectName() );
        action->setIcon( project->projectIcon() );

        // Store a pointer to the project in the action,
        // to be able to find the action for a specific project in the select action
        action->setData( QVariant::fromValue(static_cast<void*>(project)) );

        // Connect action with the project
        project->connectProjectAction( Project::SetAsActiveProject, action );

        // Add action to make the project active to the select action
        chooseActiveProject->addAction( action );
    }
}

void TimetableMate::projectAboutToBeRemoved( Project *project )
{
    KSelectAction *chooseActiveProject =
            qobject_cast< KSelectAction* >( action("project_choose_active") );
    if ( chooseActiveProject ) {
        // Search for the action associated with the given project
        foreach ( QAction *action, chooseActiveProject->actions() ) {
            // Read pointer to the associated project from the action's data
            Project *associatedProject = static_cast< Project* >( action->data().value<void*>() );
            if ( associatedProject == project ) {
                // Found the action associated with the given project, remove it
                chooseActiveProject->removeAction( action );
                break;
            }
        }
    }
}

void TimetableMate::projectSaveLocationChanged( const QString &newXmlFilePath,
                                                const QString &oldXmlFilePath )
{
    Q_UNUSED( oldXmlFilePath );
    if ( newXmlFilePath.isEmpty() ) {
        m_recentFilesAction->addUrl( newXmlFilePath );
    }
}

void TimetableMate::removeAllMessageWidgets()
{
    // Hide the widget and then delete it (give 1 second for the hide animation)
    while ( !m_messageWidgets.isEmpty() ) {
        QPointer< KMessageWidget > messageWidget = m_messageWidgets.dequeue();
        m_autoRemoveMessageWidgets.removeOne( messageWidget );
        if ( messageWidget.data() ) {
            messageWidget->animatedHide();
            QTimer::singleShot( 1000, messageWidget, SLOT(deleteLater()) );
        }
    }
}

void TimetableMate::infoMessage( const QString &message, KMessageWidget::MessageType type,
                                 int timeout, QList<QAction*> actions )
{
    if ( statusBar()->isVisible() ) {
        statusBar()->showMessage( message, timeout );
    } else {
        if ( !m_messageWidgets.isEmpty() ) {
            QPointer< KMessageWidget > messageWidget = m_messageWidgets.last();
            while ( !messageWidget.data() ) {
                m_messageWidgets.removeOne( messageWidget );
                if ( m_messageWidgets.isEmpty() ) {
                    messageWidget = 0;
                    break;
                }
                messageWidget = m_messageWidgets.last();
            }

            if ( messageWidget && messageWidget->messageType() == type &&
                 messageWidget->text() == message )
            {
                // The same message was just added
                return;
            }
        }

        // Create a new KMessageWidget
        QPointer< KMessageWidget > messageWidget( new KMessageWidget(message, this) );
        messageWidget->hide();
        messageWidget->setCloseButtonVisible( true );
        messageWidget->setMessageType( type );
        messageWidget->addActions( actions );
        if ( message.length() > 60 ) {
            messageWidget->setWordWrap( true );
        }

        // Install event filter to delete the message widget when it gets hidden
        messageWidget->installEventFilter( this );

        // Add new message widget
        m_messageWidgetLayout->addWidget( messageWidget );
        m_messageWidgets.enqueue( messageWidget );
        messageWidget->animatedShow();

        // Add a timer to remove the message widget again
        if ( timeout > 0 ) {
            m_autoRemoveMessageWidgets.enqueue( messageWidget );
            QTimer::singleShot( timeout, this, SLOT(removeTopMessageWidget()) );
        }

        // Clear up the message widget queue, if there are too many messages shown
        const int maxMessageWidgetCount = 3;
        while ( m_messageWidgets.length() > maxMessageWidgetCount ) {
            QPointer< KMessageWidget > messageWidget = m_messageWidgets.dequeue();
            if ( messageWidget.data() ) {
                messageWidget.data()->deleteLater();
            }
        }
    }
}

void TimetableMate::removeTopMessageWidget()
{
    if ( m_autoRemoveMessageWidgets.isEmpty() ) {
        return;
    }

    // Hide the widget and then delete it (give 1 second for the hide animation)
    QPointer<KMessageWidget> messageWidget = m_autoRemoveMessageWidgets.dequeue();
    m_messageWidgets.removeOne( messageWidget );
    if ( messageWidget.data() ) {
        messageWidget->animatedHide();
    }
}

bool TimetableMate::eventFilter( QObject *object, QEvent *event )
{
    KMessageWidget *messageWidget = qobject_cast< KMessageWidget*> ( object );
    if ( messageWidget && event->type() == QEvent::Hide ) {
        // Delete message widgets after they are hidden
        messageWidget->deleteLater();
    }

    return QObject::eventFilter( object, event );
}

void TimetableMate::projectCloseRequest() {
    Project *project = qobject_cast< Project* >( sender() );
    if ( !project ) {
        kWarning() << "Slot projectCloseRequest() called from wrong sender, "
                      "only class Project is allowed";
        return;
    }

    closeProject( project );
}

void TimetableMate::tabTitleChanged( QWidget *tabWidget, const QString &title, const QIcon &icon )
{
    const int index = m_tabWidget->indexOf( tabWidget );
    if ( index != -1 ) {
        // Tab widget was already inserted into the main tab bar
        m_tabWidget->setTabText( index, title );
        m_tabWidget->setTabIcon( index, icon );
    }
}

void TimetableMate::closeProject()
{
    closeProject( m_projectModel->activeProject() );
}

AbstractTab *TimetableMate::showProjectTab( bool addTab, AbstractTab *tab )
{
    if ( !tab ) {
        kDebug() << "No tab object";
        return 0;
    }

    if ( addTab ) {
        // Add the tab
        m_tabWidget->addTab( tab, tab->icon(), tab->title() );
    }

    // Switch to the tab
    m_tabWidget->setCurrentWidget( tab );
    return tab;
}

void TimetableMate::open( const KUrl &url ) {
//     foreach ( ProjectModelItem *projectItem, m_projectModel->projectItems() ) {
//         if ( url.equals(projectItem->project()->xmlFilePath(), KUrl::CompareWithoutTrailingSlash) ) {
//             KMessageBox::information( this, i18nc("@info", "The project %1 is already opened.",
//                                                   projectItem->project()->projectName()) );
//             return;
//         }
//     }

    Project *project = openProject( url.path() );
    if ( project ) {
        project->showDashboardTab( this );
    }
}

Project *TimetableMate::openProject( const QString &filePath )
{
    Project *openedProject = m_projectModel->projectFromFilePath( filePath );
    if ( openedProject ) {
        return openedProject;
    }

    Project *project = new Project( this );
    project->loadProject( filePath );
    if ( project->state() == Project::ProjectSuccessfullyLoaded ) {
        if ( !project->filePath().isEmpty() ) {
            m_recentFilesAction->addUrl( project->filePath() );
        }
        m_projectModel->appendProject( project );
        return project;
    } else if ( project->state() == Project::ProjectError ) {
        // The error message was emitted from the constructor of Project
        infoMessage( project->lastError(), KMessageWidget::Error );
        delete project;
        return 0;
    }

    return project;
}

void TimetableMate::fileOpen() {
    QString fileName = KFileDialog::getOpenFileName( KUrl("kfiledialog:///serviceprovider"),
            "application/x-publictransport-serviceprovider application/xml",
            this, i18nc("@title:window", "Open Service Provider Plugin") );
    if ( fileName.isNull() )
        return; // Cancel clicked

    open( KUrl(fileName) );
}

void TimetableMate::fileOpenInstalled() {
    // Get a list of all service provider plugin files in the directory of the XML file
    QStringList pluginFiles = ServiceProviderGlobal::installedProviders();
    if ( pluginFiles.isEmpty() ) {
        KMessageBox::information( this, i18nc("@info/plain", "There are no installed "
                "service provider plugins. You need to install the PublicTransport data engine.") );
        return;
    }
    qSort( pluginFiles );

    // Make filenames more pretty and create a hash to map from the pretty names to the full paths
    QHash< QString, QString > map;
    for ( QStringList::iterator it = pluginFiles.begin(); it != pluginFiles.end(); ++it ) {
        QString prettyName;
        if ( KStandardDirs::checkAccess(*it, W_OK) ) {
            // File is writable, ie. locally installed
            prettyName = KUrl( *it ).fileName();
        } else {
            // File isn't writable, ie. globally installed
            prettyName = i18nc("@info/plain This string is displayed instead of the full path for "
                               "globally installed service provider plugins.",
                               "Global: %1", KUrl(*it).fileName());
        }

        map.insert( prettyName, *it );
        *it = prettyName;
    }

    bool ok;
    QString selectedPrettyName = KInputDialog::getItem(
            i18nc("@title:window", "Open Installed Service Provider Plugin"),
            i18nc("@info", "Installed service provider plugin"),
            pluginFiles, 0, false, &ok, this );
    if ( ok ) {
        QString selectedFilePath = map[ selectedPrettyName ];
        Project *project = openProject( selectedFilePath );
        if ( project ) {
            project->showDashboardTab( this );
        }
    }
}

void TimetableMate::fileSaveAll() {
    for ( int row = 0; row < m_projectModel->rowCount(); ++row ) {
        Project *project = m_projectModel->projectItemFromRow( row )->project();
        project->save( this );
    }
}

void TimetableMate::optionsPreferences() {
    // Avoid to have two dialogs shown
    if ( !KConfigDialog::showDialog("settings") )  {
        // Create a new preferences dialog and show it
        KConfigDialog *dialog = new KConfigDialog( this, "settings", Settings::self() );
        QWidget *generalSettings = new QWidget( dialog );
        ui_preferences = new Ui::preferences;
        ui_preferences->setupUi( generalSettings );
        dialog->addPage( generalSettings, i18n("General"), "package_settings" );
        dialog->setAttribute( Qt::WA_DeleteOnClose );
        connect( dialog, SIGNAL(finished()), this, SLOT(preferencesDialogFinished()) );
        dialog->show();
    }
}

void TimetableMate::preferencesDialogFinished()
{
    delete ui_preferences;
    ui_preferences = 0;
}

bool TimetableMate::hasHomePageURL( const ServiceProviderData *info ) {
    if ( info->url().isEmpty() ) {
        KMessageBox::information( this, i18nc("@info",
                "The <interface>Home Page URL</interface> is empty.<nl/>"
                "Please set it in the project settings dialog first.") );
        return false;
    } else
        return true;
}

#include "timetablemate.moc"
