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

#ifndef TIMETABLEMATE_H
#define TIMETABLEMATE_H

// PublicTransport engine includes
#include <engine/enums.h> // For TimetableData

// KDE includes
#include <KParts/MainWindow>
#include <KMessageWidget> // For KMessageWidget::MessageType

// Qt includes
#include <QQueue>

class Project;
class ProjectModel;

class AbstractTab;
class OverviewTab;
class ProjectSourceTab;
class PlasmaPreviewTab;
class WebTab;
class ScriptTab;

class FixedToolBar;
class DockToolBar;

class BacktraceDockWidget;
class BreakpointDockWidget;
class OutputDockWidget;
class VariablesDockWidget;
class ConsoleDockWidget;
class DocumentationDockWidget;
class ProjectsDockWidget;
class TestDockWidget;
class WebInspectorDockWidget;
class NetworkMonitorDockWidget;

class TimetableAccessorInfo;
struct StopSuggestionRequestInfo;
struct JourneyRequestInfo;
struct DepartureRequestInfo;

namespace Ui {
    class preferences;
}
namespace Scripting {
    class ResultObject;
};
namespace Debugger {
    class Breakpoint;
    class ScriptRunData;
}

namespace KParts {
    class PartManager;
};
class KActionMenu;
class KToggleAction;
class KRecentFilesAction;
class KUrl;
class KTabWidget;
class KMessageWidget;

class QModelIndex;
class QVBoxLayout;
class QScriptValue;

using namespace Scripting;
using namespace Debugger; // Needed for slots to match signals, eg. using Breakpoint instead of Debugger::Breakpoint

/**
 * @brief Main window for TimetableMate.
 *
 * TimetableMate is a little IDE for creating scripts for the PublicTransport data engine.
 *
 * Uses QDockWidget's at the left, right and bottom dock areas. DockToolBar's are added to these
 * three areas to toggle the dock widgets at that area. Dock widgets can be freely moved between
 * the three areas, the associated toggle action gets moved to the DockToolBar at the new area.
 * At each area only one QDockWidget can be shown at a time.
 * This behaviour is similiar to what KDevelop does with it's tool views.
 *
 * Multiple projects can be opened in one TimetableMate window and are managed by a ProjectModel.
 * For each project a set of tabs can be opened: Edit the source project/accessor XML document
 * (AccessorTab), edit the script file (ScriptTab), preview the project in Plasma
 * (PlasmaPreviewTab) or show the service providers home page (WebTab). Instead of editing the XML
 * document of a project, it's settings can be edited using a ProjectSettingsDialog.
 * One project always is the "active project", if at least one project is opened. The active
 * project gets connected to the main TimetableMate actions and to the dock widgets. For example
 * the breakpoint dock widget shows the breakpoints of the active project.
 **/
class TimetableMate : public KParts::MainWindow
{
    Q_OBJECT

public:
    /** @brief Constructor */
    TimetableMate();

    /** @brief Destructor */
    virtual ~TimetableMate();

public slots:
    /** @brief Create a new project. */
    void fileNew();

    /** @brief Open project from @p url. */
    void open( const KUrl &url );

    /** @brief Open a file dialog to select a project to open. */
    void fileOpen();

    /** @brief Open a dialog to select an installed project to open. */
    void fileOpenInstalled();

    /** @brief Save all projects. */
    void fileSaveAll();

    /** @brief Close the currently active project if any. */
    void closeProject();

    void publish() {}; // TODO

    /**
     * @brief Close @p tab.
     * If @p tab contains unsaved content the user gets asked if it should be saved.
     **/
    void closeTab( AbstractTab *tab );

    /**
     * @brief Close the current tab.
     * If the current tab contains unsaved content the user gets asked if it should be saved.
     **/
    void closeCurrentTab();

    /**
     * @brief Close all tabs except for @p tab.
     *
     * If tabs with unsaved content are closed the user gets asked if they should be saved.
     * @returns True if all tabs could be closed, false otherwise (eg. cancelled by the user).
     **/
    bool closeAllTabsExcept( AbstractTab *tab, bool ask = true );

    /**
     * @brief Close all tabs. If a @p project is given only tabs of that project get closed.
     *
     * If tabs with unsaved content are closed the user gets asked if they should be saved.
     * @returns True if all tabs could be closed, false otherwise (eg. cancelled by the user).
     **/
    bool closeAllTabs( Project *project = 0, bool ask = true );

    /**
     * @brief Close @p project.
     * If @p project is modified the user gets asked if it should be saved.
     **/
    bool closeProject( Project *project );

    /**
     * @brief Close all projects.
     * If projects with modifications are closed the user gets asked if they should be saved.
     **/
    bool closeAllProjects();

protected slots:
    /** @brief Initialize after the TimetableMate instance is created. */
    void initialize();

    void optionsPreferences();
    void preferencesDialogFinished();

    void projectAdded( Project *project );
    void projectAboutToBeRemoved( Project *project );
    void projectCloseRequest();
    void activeProjectAboutToChange( Project *project, Project *previousProject );
    void infoMessage( const QString &message,
                      KMessageWidget::MessageType type = KMessageWidget::Information,
                      int timeout = 4000, QList<QAction*> actions = QList<QAction*>() );
    void removeAllMessageWidgets();
    void testActionTriggered();
    void testCaseActionTriggered();

    void tabTitleChanged( QWidget *tabWidget, const QString &title, const QIcon &icon );
    void dockLocationChanged( Qt::DockWidgetArea area );

    void currentTabChanged( int index );
    void tabCloseRequested( int index ) { closeTab(projectTabAt(index)); };
    void tabOpenRequest( AbstractTab *tab ) { showProjectTab(true, tab); };
    void tabGoToRequest( AbstractTab *tab ) { showProjectTab(false, tab); };
    void tabContextMenu( QWidget *widget, const QPoint &pos );
    void activePartChanged( KParts::Part *part );

    void scriptPreviousFunction();
    void scriptNextFunction();

    /** @brief A test has started in the currently active project. */
    void testStarted();

    /** @brief A test has finished in the currently active project. */
    void testFinished( bool success );

    /** @brief Script execution started in the currently active project. */
    void debugStarted();

    /** @brief Script execution stopped in the currently active project. */
    void debugStopped( const ScriptRunData &scriptRunData );

    void waitingForSignal();
    void wokeUpFromSignal( int time );

    /** @brief Script execution was aborted in the currently active project. */
    void debugAborted();

    /** @brief Script execution was interrupted in the currently active project. */
    void debugInterrupted();

    /** @brief Script execution was continued after an interrupt in the currently active project. */
    void debugContinued();

    /** @brief There was an uncaught execution in the script of the currently active project. */
    void uncaughtException( int lineNumber, const QString &errorMessage );

    /** @brief A @p breakpoint was reached in the currently active project. */
    void breakpointReached( const Breakpoint &breakpoint );

    /** @brief Toggle breakpoint at the current line in the script tab of the current project, if any. */
    void toggleBreakpoint();

    void functionCallResult( const QList< TimetableData > &timetableData,
                             const QScriptValue &returnValue );

    void removeTopMessageWidget();
    void tabNextActionTriggered();
    void tabPreviousActionTriggered();

    void projectSaveLocationChanged( const QString &newXmlFilePath, const QString &oldXmlFilePath );

protected:
    virtual bool queryClose();

    /** @brief Overridden to delete KMessageWidgets when they get hidden. */
    virtual bool eventFilter( QObject *object, QEvent *event );

    virtual void saveProperties( KConfigGroup &config );
    virtual void readProperties( const KConfigGroup &config );

    /** @brief Overridden to create custom menubar separator items. */
    virtual QAction *createCustomElement( QWidget *parent, int index, const QDomElement &element );

    /** @brief Overridden to change the context menu in empty menu bar space and main window splitters. */
    virtual void contextMenuEvent( QContextMenuEvent *event );

private:
    enum TabAction {
        MoveToTab,
        LeaveTab,
        CloseTab
    };

    void setupActions();
    void setupDockWidgets();
    void updateWindowTitle();
    void updateShownDocksAction();

    bool fixMenus();
    void populateTestMenu();
    void connectTestMenuWithProject( Project *project, bool doConnect = true );

    /** @brief Open project from @p fileName. */
    Project *openProject( const QString &fileName );

    /** @brief Get the current project, if any, ie. the project of the currently shown tab. */
    Project *currentProject();

    /** @brief Get the tab object at the given @p index */
    AbstractTab *projectTabAt( int index );

    AbstractTab *showProjectTab( bool addTab, AbstractTab *tab );

    bool closeAllTabsExcept( Project *project, AbstractTab *except = 0, bool ask = true );

    void overviewTabAction( OverviewTab *overviewTab, TabAction tabAction );
    void accessorTabAction( ProjectSourceTab *accessorTab, TabAction tabAction );
    void scriptTabAction( ScriptTab *scriptTab, TabAction tabAction );
    void plasmaPreviewTabAction( PlasmaPreviewTab *plasmaPreviewTab, TabAction tabAction );
    void webTabAction( WebTab *webTab, TabAction tabAction );

    bool hasHomePageURL( const TimetableAccessorInfo *info );

    Ui::preferences *ui_preferences;

    ProjectModel *m_projectModel; // Contains all opened projects
    KParts::PartManager *m_partManager;
    KTabWidget *m_tabWidget;

    // Fixed tool bars showing docks on the left/right/bottom dock area
    DockToolBar *m_leftDockBar;
    DockToolBar *m_rightDockBar;
    DockToolBar *m_bottomDockBar;

    // Dock widgets
    BacktraceDockWidget *m_backtraceDock;
    ConsoleDockWidget *m_consoleDock;
    OutputDockWidget *m_outputDock;
    BreakpointDockWidget *m_breakpointDock;
    VariablesDockWidget *m_variablesDock;
    DocumentationDockWidget *m_documentationDock;
    ProjectsDockWidget *m_projectsDock;
    TestDockWidget *m_testDock;
    WebInspectorDockWidget *m_webInspectorDock;
    NetworkMonitorDockWidget *m_networkMonitorDock;

    // Pointers to specific actions
    KActionMenu *m_showDocksAction;
    KToggleAction *m_toolbarAction;
    KToggleAction *m_statusbarAction;
    KRecentFilesAction *m_recentFilesAction;
    QList< QAction* > m_testCaseActions;

    AbstractTab *m_currentTab; // Stores a pointer to the current tab, if any
    QQueue< QPointer<KMessageWidget> > m_messageWidgets;
    QQueue< QPointer<KMessageWidget> > m_autoRemoveMessageWidgets;
    QVBoxLayout *m_messageWidgetLayout;
};

#endif // _TIMETABLEMATE_H_

// kate: indent-mode cstyle; indent-width 4; replace-tabs on;
