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

#ifndef PROJECT_H
#define PROJECT_H

// Own includes
#include "config.h"
#include "lib_config.h"
#include "tabs/tabs.h" // For TabType
#include "testmodel.h" // For TestModel::Test, TestModel::TestCase
#include "debugger/debuggerstructures.h"
#include "debugger/debuggerjobs.h"

// Public Transport engine includes
#include <engine/enums.h> // For TimetableData, TimetableInformation
#include <engine/serviceproviderdata.h>

// KDE includes
#include <KMessageWidget> // For KMessageWidget::MessageType

// Qt includes
#include <QPointer>

namespace ThreadWeaver
{
    class Job;
}
typedef QSharedPointer< ThreadWeaver::WeaverInterface > WeaverInterfacePointer;

class ProjectPrivate;
class ProjectModel;
class TestModel;

class AbstractTab;
class DashboardTab;
class ProjectSourceTab;
class ScriptTab;
class WebTab;
class PlasmaPreviewTab;

class ServiceProvider;
class ServiceProviderData;

struct AbstractRequest;
struct DepartureRequest;
struct StopSuggestionRequest;
struct StopsByGeoPositionRequest;
struct JourneyRequest;

namespace Debugger {
    class Debugger;
    class BreakpointModel;
    class Breakpoint;
    class ScriptRunData;
    class TimetableDataRequestJob;
    class DebuggerJob;
}

class KActionMenu;
class KIcon;
namespace KTextEditor
{
    class Document;
    class Cursor;
    class View;
}

class QAction;
class QMenu;
class QPoint;
class QIODevice;

using namespace Debugger;

/**
 * @brief A TimetableMate project.
 *
 * Manages one TimetableMate project, which gets used to develop/edit/test/fix a PublicTransport
 * engine service provider plugin. Each project has an XML document describing the service provider
 * and how to get timetable data. There is a special mime type
 * "application-x-publictransport-serviceprovider" and the extension "*.pts" for these XML
 * documents, but "*.xml" also works. Use install()/uninstall() to install/uninstall a project.
 * Installed projects get recognized by the PublicTransport engine. Locally installed versions are
 * preferred over globally installed ones.
 * Each project can also have a script file, which gets used to request/parse timetable data.
 * Currently only scripted service provider plugins are supported by this class.
 *
 * A set of tabs gets provided, ie. DashboardTab, ScriptTab, ProjectSourceTab, PlasmaPreviewTab and
 * WebTab. Project settings can be changed using the ProjectSourceTab or the ProjectSettingsDialog
 * (showSettingsDialog()).
 * Each project has a projectName() and a projectIcon(). It can be saved using save() or saveAs()
 * and opened by creating a new Project instance with the file path. Check if the project was
 * opened from an installation directory using saveType(). Use installationTypes() to check if the
 * project is installed locally and/or globally.
 *
 * Each project also provides a set of actions, accessible with projectAction(). Use
 * createProjectAction() to create an external action with the same properties as the internal one
 * returned by projectAction(). The created action needs to be connected with the project using
 * connectProjectAction(). It then triggers the associated action in the connected project and
 * it's enabled state gets updated when needed.
 * This can be used to have an external set of project actions that can be connected with different
 * projects, eg. in a ProjectModel. When these external actions get stored in a KActionCollection,
 * you can use projectActionName() to get names for the actions.
 *
 * This class provides many properties with notify signals and invokable methods for easy usage
 * in QML. For example the projectAction() method can be used to get an action, which can then be
 * connected to eg. a Plasma PushButton.
 **/
class Project : public QObject {
    Q_OBJECT
    Q_PROPERTY( bool isActiveProject READ isActiveProject NOTIFY activeProjectStateChanged )
    Q_PROPERTY( bool isModified READ isModified NOTIFY modifiedStateChanged )
    Q_PROPERTY( bool isProjectSourceModified READ isProjectSourceModified NOTIFY projectSourceModifiedStateChanged )
    Q_PROPERTY( bool isScriptModified READ isScriptModified NOTIFY scriptModifiedStateChanged )
    Q_PROPERTY( bool isInstalledLocally READ isInstalledLocally NOTIFY localInstallationStateChanged )
    Q_PROPERTY( bool isInstalledGlobally READ isInstalledGlobally NOTIFY globalInstallationStateChanged )
    Q_PROPERTY( bool isDebuggerRunning READ isDebuggerRunning NOTIFY debuggerRunningChanged )
    Q_PROPERTY( bool isTestRunning READ isTestRunning NOTIFY testRunningChanged )
    Q_PROPERTY( QString name READ projectName NOTIFY nameChanged )
    Q_PROPERTY( ServiceProviderData *data READ data NOTIFY dataChanged )
    Q_PROPERTY( QString iconName READ iconName NOTIFY iconNameChanged )
    Q_PROPERTY( QIcon icon READ projectIcon NOTIFY iconChanged )
    Q_PROPERTY( InstallType saveType READ saveType NOTIFY saveTypeChanged )
    Q_PROPERTY( QString savePathInfoString READ savePathInfoString NOTIFY savePathInfoStringChanged )
    Q_PROPERTY( QString output READ output NOTIFY outputChanged )
    Q_PROPERTY( QString consoleText READ consoleText NOTIFY consoleTextChanged )
    Q_ENUMS( State Error ScriptTemplateType ProjectAction ProjectActionGroup
             ProjectDocumentSource InstallType )
    friend class ProjectSourceTab;
    friend class ScriptTab;
    friend class WebTab;
    friend class PlasmaPreviewTab;
    friend class ProjectModel;

public:
    /** @brief Project states. */
    enum State {
        Uninitialized = 0, /**< Project object not initialized. */
        NoProjectLoaded, /**< No project is loaded, a template gets used, waiting for save. */
        ProjectSuccessfullyLoaded, /**< Project was successfully loaded from file. */
        ProjectError /**< There was an error. @see Error. */
    };

    /** @brief Error types. */
    enum Error {
        NoError = 0, /**< No error. */
        ProjectFileNotFound, /**< The project file (ie. the service provider plugin XML document) was not found. */
        ProjectFileNotReadable, /**< The project file (ie. the service provider plugin XML document) is not readable. */
        ScriptFileNotFound, /**< The script file was not found. */
        ScriptSyntaxError, /**< There is a syntax error in the script. */
        ErrorWhileLoadingProject, /**< There was an error while loading the project. */
        KatePartNotFound, /**< The Kate part was not found. */
        KatePartError, /**< There was an error with the Kate part. */
        PlasmaPreviewError, /**< There was an error with the plasma preview. */
        WebError /**< There was an error with the web widget. */
    };

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    /** @brief Types of script templates. */
    enum ScriptTemplateType {
        NoScriptTemplate = 0,
        ScriptQtScriptTemplate, /**< A template for script content written in QtScript. */
        ScriptQtScriptHafasTemplate, /**< A template for script content written in QtScript,
                * using the base HAFAS script. */ // TODO
        ScriptRubyTemplate, /**< A template for script content written in ruby. */
        ScriptPythonTemplate, /**< A template for script content written in python. */

        DefaultScriptTemplate = ScriptQtScriptTemplate /**< Default script template type. */
    };
#endif

    /**
     * @brief Types of project actions.
     * Actions can be created using createProjectAction() and connected using connectProjectAction().
     * Internally managed actions are available using projectAction().
     **/
    enum ProjectAction {
        InvalidProjectAction = -1, /**< Invalid project action. */

        // FileActionGroup
        Save = 0, /**< Save the project. */
        SaveAs, /**< Save the project under a new filename. */
        Install, /**< Install the project locally. */
        Uninstall, /**< Uninstall a locally installed version of the project. */
        InstallGlobally, /**< Install the project globally. */
        UninstallGlobally, /**< Uninstall a globally installed version of the project. */

        // UiActionGroup
        ShowProjectSettings, /**< Show a settings dialog for the project. */
        ShowDashboard, /**< Show the dashboard tab. */
        ShowHomepage, /**< Show the web tab. */
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        ShowScript, /**< Show/open the main script in a tab. */
        ShowExternalScript, /**< Show/open a script tab with an external script (included into the
                               * main script). The file path to the external script gets specified
                               * as data of type QString in calls to projectAction() or
                               * createProjectAction(). If no file path is given an open file
                               * dialog gets shown to select the file to open. That file could
                               * later be included into the main script. */
        OpenScript = ShowScript,
        OpenExternalScript = ShowExternalScript,
#endif
        ShowProjectSource, /**< Show the project source XML document tab. */
        OpenProjectSource = ShowProjectSource,
        ShowPlasmaPreview, /**< Show the plasma preview tab. */

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        // DebuggerActionGroup
        Interrupt, /**< Interrupt the debugger. */
        Continue, /**< Continue the debugger. */
        AbortDebugger, /**< Abort the debugger. */
        RunToCursor, /**< Run until the current cursor position in an opened script tab. */
        StepInto, /**< Continue script execution until the next statement. */
        StepOver, /**< Continue script execution until the next statement in the same level. */
        StepOut, /**< Continue script execution until the first statement
                * outside the current function. */
        ToggleBreakpoint, /**< Toggle breakpoint at the current cursor position in an opened
                * script tab. */
        RemoveAllBreakpoints, /**< Remove all breakpoints. */

        // RunActionGroup
        RunMenuAction, /**< A KMenuAction which contains the other RunXXX actions. */
        RunGetTimetable, /**< Run the getTimetable() script function, interrupt on exceptions. */
        RunGetStopSuggestions, /**< Run the getStopSuggestions() script function, interrupt on exceptions. */
        RunGetStopsByGeoPosition, /**< Run the getStopSuggestions() script function
                * with a geo position as argument, interrupt on exceptions. */
        RunGetJourneys, /**< Run the getJourneys() script function, interrupt on exceptions. */

        DebugMenuAction, /**< A KMenuAction which contains the other DebugXXX actions. */
        DebugGetTimetable, /**< Run the getTimetable() script function, interrupt at start. */
        DebugGetStopSuggestions, /**< Run the getStopSuggestions() script function with a stop name
                * part as argument, interrupt at start. */
        DebugGetStopsByGeoPosition, /**< Run the getStopSuggestions() script function
                * with a geo position as argument, interrupt at start. */
        DebugGetJourneys, /**< Run the getJourneys() script function, interrupt at start. */
#endif

        // TestActionGroup
        RunAllTests, /**< Test the project, eg. syntax errors in the script. */
        AbortRunningTests, /**< Test the project, eg. syntax errors in the script. */
        ClearTestResults, /**< Clears all test results. */
        RunSpecificTest, /**< Runs one specific test. The test gets specified as data of type
                * TestModel::Test (ie. int) in calls to projectAction() or createProjectAction(). */
        RunSpecificTestCase, /**< Runs one specific test case. The test case gets specified
                * as data of type TestModel::TestCase (ie. int) in calls to projectAction()
                * or createProjectAction(). */
        SpecificTestCaseMenuAction, /**< A KMenuAction which contains actions for a specific
                * test case, eg. an action for each test of the test case. The test case gets
                * specified as data of type TestModel::TestCase (ie. int) in calls to
                * projectAction() or createProjectAction(). */

        // OtherActionGroup
        Close, /**< Close the project. */
        SetAsActiveProject, /**< Set the project as active project. */
    };

    /** @brief Groups of project actions, each ProjectAction is associated with one group. */
    enum ProjectActionGroup {
        InvalidProjectActionGroup = -1,

        FileActionGroup,
        UiActionGroup,
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        DebuggerActionGroup,
        RunActionGroup,
#endif
        TestActionGroup,
        OtherActionGroup
    };

    /** @brief Sources of project XML document text. */
    enum ProjectDocumentSource {
        ReadProjectDocumentFromBuffer, /**< Read project XML document from a buffer, which always
                * contains the newest version (eg. from a modified ProjectSourceTab or with changes
                * from a ProjectSettingsDialog). */
        ReadProjectDocumentFromFile, /**< Read project XML document from file, if a file name was
                * specified. @see Project::xmlFilePath */
        ReadProjectDocumentFromTab, /**< Read project XML document from an opened ProjectSourceTab.
                * If no such tab is opened nothing gets read. */
        ReadProjectDocumentFromTabIfOpened /**< Read project XML document from an opened
                * ProjectSourceTab, if any. If no such tab is opened the document gets read like with
                * ReadProjectDocumentFromBuffer. */
    };

    /** @brief Different types of installations. */
    enum InstallType {
        NoInstallation      = 0x0000, /**< Not installed. */

        LocalInstallation   = 0x0001, /**< Locally installed in the current users home directory. */
        GlobalInstallation  = 0x0002, /**< Globally installed. */

        DefaultInstallType = LocalInstallation /**< The default installation type. */
    };
    Q_DECLARE_FLAGS( InstallTypes, InstallType );

    /** @brief . */
    struct ProjectActionData {
        ProjectActionData( ProjectAction actionType = InvalidProjectAction,
                            const QVariant &data = QVariant() )
                : actionType(actionType), data(data) {};
        bool isValid() const { return actionType != InvalidProjectAction; };

        ProjectAction actionType;
        QVariant data;
    };

    /**
     * @brief Create a new project from @p xmlFilePath.
     *
     * @param parent Used as parent for dialogs created inside Project, eg. showSettingsDialog()
     *   uses this as parent for the settings dialog.
     **/
    explicit Project( const WeaverInterfacePointer &weaver, QWidget *parent = 0 );

    /** @brief Destructor. */
    virtual ~Project();

    /**
     * @brief Loads the project from the given @p projectSoureFile.
     * @warning If another project was loaded previously, all changes made in the project
     *   get discarded. Better create a new Project object for each project to load.
     * @param projectSourceFile The file path to the provider XML file. If this is empty a template
     *   project gets loaded.
     **/
    bool loadProject( const QString &projectSoureFile = QString() );

    /** @brief Whether or not messages for this project should currently be suppressed. */
    bool suppressMessages() const;

    /** @brief Whether or not question message boxes should be shown. */
    void setQuestionsEnabled( bool enable = true );

    /** @brief Get the current state of this project. */
    Q_INVOKABLE static QString nameFromIcon( const QIcon &icon ) { return icon.name(); };

    /** @brief Get the current state of this project. */
    Q_INVOKABLE State state() const;

    /** @brief Get the last error message for this project. */
    Q_INVOKABLE QString lastError() const;

    /** @brief Get the model which contains this project, if any. */
    ProjectModel *projectModel() const;

    /**
     * @brief Get all collected output for this project.
     * @see appendOutput
     * @see clearOutput
     **/
    QString output() const;

    /**
     * @brief Get all collected console text for this project.
     * @see appendOutput
     * @see clearConsoleText
     **/
    QString consoleText() const;

    /** @brief Whether or not the project is modified. */
    Q_INVOKABLE bool isModified() const;

    /**
     * @brief Whether or not the service provider plugin XML document was modified.
     *
     * The service provider plugin XML document can be modified in an ProjectSourceTab or in a
     * ProjectSettingsDialog. It can be modified although no ProjectSourceTab is currently opened.
     **/
    Q_INVOKABLE bool isProjectSourceModified() const;

    /**
     * @brief Whether or not the script was modified.
     *
     * The script can be modified in a ScriptTab.
     **/
    Q_INVOKABLE bool isScriptModified() const;

    /**
     * @brief Whether or not this project is currently the active one.
     * @note If this project is not added to a ProjectModel, this function always returns false.
     **/
    Q_INVOKABLE bool isActiveProject() const;

    /**
     * @brief Show a context menu for this project at @p globalPos.
     * The actions returned by contextMenuActions() are shown in the context menu.
     **/
    Q_INVOKABLE void showProjectContextMenu( const QPoint &globalPos );

    /** @brief Get a list of actions to be shown in the projects context menu. */
    Q_INVOKABLE QList< QAction* > contextMenuActions( QWidget *parent = 0 );

    /** @brief Get a KActionMenu which contains the actions from contextMenuActions(). */
    Q_INVOKABLE QPointer< KActionMenu > projectSubMenuAction( QWidget *parent = 0 );

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    /** @brief Get a KActionMenu which contains actions related to the debugger. */
    Q_INVOKABLE QPointer< KActionMenu > debuggerSubMenuAction( QWidget *parent = 0 );
#endif

    /** @brief Get a KActionMenu which contains actions related to tests. */
    Q_INVOKABLE QPointer< KActionMenu > testSubMenuAction( QWidget *parent = 0 );

    /**
     * @brief Get the name for project actions of the given @p actionType.
     *
     * Can be useful when actions get stored in a KActionCollection.
     **/
    Q_INVOKABLE static const char *projectActionName( ProjectAction actionType );

    /**
     * @brief Get the text for project actions of the given @p actionType with @p data.
     **/
    Q_INVOKABLE static QString projectActionText( ProjectAction actionType,
                                                  const QVariant &data = QVariant() );

    /**
     * @brief Get the project action of the given @p actionType.
     *
     * Creates the action if not done already using createProjectAction().
     * If you need a QAction which is not bound to this Project instance, use createProjectAction()
     * and connect it to custom projects using connectProjectAction().
     *
     * @param actionType The type of the action to create.
     * @param data Data needed for the action, if any. Most actions need no data.
     *
     * @see createProjectAction
     **/
    Q_INVOKABLE QAction *projectAction( ProjectAction actionType, const QVariant &data = QVariant() );

    /**
     * @brief Create a project action of the given @p actionType and with the given @p parent.
     *
     * @note The created action is initially @em not connected to a project.
     *   Use connectProjectAction() to connect it to a project.
     *
     * Use projectActionName() to get a name for the created action, eg. when used in a
     * KActionCollection.
     * Do not modify the data of the created action (QAction::setData()). It contains internally
     * used data, which can be obtained using projectActionData(). The @p data parameter can
     * @em not be used to store custom data.
     *
     * @param actionType The type of the action to create.
     * @param data Data needed for the action, if any. Most actions do not need any data
     *   (see ProjectAction). A warning message gets printed if data is missing for @p actionType.
     *   Do @em not use this to store custom data. The @p actionType and the @p data of a project
     *   action are used to distinguish between different actions.
     * @param parent The parent QObject for the new action.
     * @return The created action.
     * @see connectProjectAction
     **/
    Q_INVOKABLE static QAction *createProjectAction( ProjectAction actionType,
                                                     const QVariant &data,
                                                     QObject *parent = 0 );

    /** @brief Overload without data argument. */
    Q_INVOKABLE static inline QAction *createProjectAction( ProjectAction actionType,
                                                            QObject *parent = 0 )
            { return createProjectAction(actionType, QVariant(), parent); };

    /** @brief Get the group of actions of @p actionType. */
    Q_INVOKABLE static ProjectActionGroup actionGroupFromType( ProjectAction actionType );

    /**
     * @brief Convenience method wich calls createProjectAction() and connectProjectAction().
     **/
    Q_INVOKABLE inline QAction* createAndConnectProjectAction(
            ProjectAction actionType, const QVariant &data, QObject *parent = 0,
            bool useQueuedConnection = false )
    {
        QAction *action = createProjectAction( actionType, data, parent );
        connectProjectAction( actionType, action, true, useQueuedConnection );
        return action;
    };

    /**
     * @brief Convenience method wich calls createProjectAction() and connectProjectAction().
     **/
    Q_INVOKABLE inline QAction* createAndConnectProjectAction(
            ProjectAction actionType, QObject *parent = 0, bool useQueuedConnection = false )
    {
        QAction *action = createProjectAction( actionType, parent );
        connectProjectAction( actionType, action, true, useQueuedConnection );
        return action;
    };

    /**
     * @brief Connects/disconnects an @p action according to the given @p actionType.
     *
     * @param actionType The type of the @p action, controls which connections to make.
     * @param action The action to connect/disconnect.
     * @param doConnect If true, the @p action gets connected.
     *   Otherwise, the @p action gets disconnected.
     * @param useQueuedConnection Whether or not to use a queued connection, ie. Qt::QueuedConnection.
     * @see createProjectAction
     **/
    Q_INVOKABLE void connectProjectAction( ProjectAction actionType, QAction *action,
                                           bool doConnect = true, bool useQueuedConnection = false );

    /** @brief Calls connectProjectAction() with the @em doConnect argument set to false. */
    Q_INVOKABLE inline void disconnectProjectAction( ProjectAction actionType, QAction *action ) {
        connectProjectAction( actionType, action, false );
    };

    /** @brief Get a list of all project actions in @p group. */
    static QList< Project::ProjectAction > actionsFromGroup( ProjectActionGroup group );

    /** @brief Get data stored for @p projectAction. */
    static ProjectActionData projectActionData( QAction *projectAction );

    /** @brief Get data stored for @p projectAction. */
    static void setProjectActionData( QAction *projectAction, const QVariant &data );

    /** @brief Whether or not @p action is a valid project action. */
    static bool isProjectAction( QAction *action );

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    /** @brief Uses KInputDialog to let the user choose a script template type. */
    ScriptTemplateType getScriptTemplateTypeInput( QWidget *parent = 0 ) const;
#endif

    /** @brief Get a pointer to the tab of the given @p type. */
    AbstractTab *tab( TabType type ) const;

    /** @brief Whether or not a tab of the given @p type is opened. */
    Q_INVOKABLE bool isTabOpened( TabType type ) const;

    /** @brief Create a tab of the given @p type or return an already created one. */
    Q_INVOKABLE AbstractTab *createTab( TabType type, QWidget *parent = 0 );

    /** @brief Close the tab(s) of the given @p type, if any. */
    Q_INVOKABLE void closeTab( TabType type );

    /** @brief Close @p tab. */
    Q_INVOKABLE void closeTab( AbstractTab *tab );

    /**
     * @brief Get a pointer to the dashboard tab, if it was created.
     * @see createDashboardTab
     * @see showDashboardTab
     **/
    DashboardTab *dashboardTab() const;

    /**
     * @brief Get a pointer to the project source document tab, if it was created.
     * @see createProjectSourceTab
     * @see showProjectSourceTab
     **/
    ProjectSourceTab *projectSourceTab() const;

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    /**
     * @brief Get a pointer to the script document tab, if it was created.
     * @see createScriptTab
     * @see showScriptTab
     **/
    ScriptTab *scriptTab() const;

    /**
     * @brief Get a pointer to the included script document tab for @p filePath, if it was created.
     **/
    ScriptTab *scriptTab( const QString &filePath ) const;

    /** @brief Get a list of pointers to opened script document tabs for external scripts. */
    QList< ScriptTab* > externalScriptTabs() const;

    /** @brief Get a pointer to an opened script tab for the external script at @p filePath. */
    ScriptTab *externalScriptTab( const QString &filePath ) const;
#endif

    /**
     * @brief Get a pointer to the plasma preview tab, if it was created.
     * @see createPlasmaPreviewTab
     * @see showPlasmaPreviewTab
     **/
    PlasmaPreviewTab *plasmaPreviewTab() const;

    /**
     * @brief Get a pointer to the web tab, if it was created.
     * @see createWebTab
     * @see showWebTab
     **/
    WebTab *webTab() const;

    /**
     * @brief Create a dashboard tab or return an already created one.
     * @see dashboardTab
     **/
    DashboardTab *createDashboardTab( QWidget *parent = 0 );

    /**
     * @brief Create a project source document tab or return an already created one.
     * @see projectSourceTab
     **/
    ProjectSourceTab *createProjectSourceTab( QWidget *parent = 0 );

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    /**
     * @brief Create a script document tab or return an already created one.
     * @see scriptTab
     **/
    ScriptTab *createScriptTab( QWidget *parent = 0 );

    /**
     * @brief Create an external script document tab or return an already created one.
     * @see externalScriptTabs
     **/
    ScriptTab *createExternalScriptTab( const QString &filePath, QWidget *parent = 0 );
#endif

    /**
     * @brief Create a plasma preview tab or return an already created one.
     * @see plasmaPreviewTab
     **/
    PlasmaPreviewTab *createPlasmaPreviewTab( QWidget *parent = 0 );

    /**
     * @brief Create a web tab or return an already created one.
     * @see webTab
     **/
    WebTab *createWebTab( QWidget *parent = 0 );

    /**
     * @brief Get a pointer to the ServiceProvider object of this project.
     *
     * This function always returns a valid pointer.
     **/
    ServiceProvider *provider() const;

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    /** @brief Get the debugger used by this project. */
    Debugger::Debugger *debugger() const;
#endif

    /**
     * @brief Set service provider data values used for this project to @p providerData.
     * @note Comments read from the XML source file will not be cleared.
     **/
    void setProviderData( const ServiceProviderData *providerData );

    /** @brief Get the path to the project files. */
    Q_INVOKABLE QString path() const;

    /** @brief Get the file path to the project source XML file. */
    Q_INVOKABLE QString filePath() const;

    /** @brief Get the script file name. */
    Q_INVOKABLE QString scriptFileName() const;

    /** @brief Get the icon for the used script. */
    Q_INVOKABLE QIcon scriptIcon() const;

    /** @brief Get the ID of the service provider of this project. */
    Q_INVOKABLE QString serviceProviderId() const;

    /** @brief Get the contents of the service provider plugin XML document. */
    Q_INVOKABLE QString projectSourceText( ProjectDocumentSource source = ReadProjectDocumentFromTabIfOpened ) const;

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    /**
     * @brief Get the contents of the script document or an included script.
     * @param includedScriptFilePath File path to the included script file to read or an empty
     *   string to get the contents of the main script file.
     **/
    Q_INVOKABLE QString scriptText( const QString &includedScriptFilePath = QString() ) const;

    /** @brief Set the contents of the script document to @p text. */
    Q_INVOKABLE void setScriptText( const QString &text );
#endif

    /** @brief Get an icon for the project. */
    Q_INVOKABLE QIcon projectIcon() const;

    /** @brief Get an icon name for the project. */
    Q_INVOKABLE QString iconName() const;

    /** @brief Get a name for the project. */
    Q_INVOKABLE QString projectName() const;

    /** @brief Get data for the service provider plugin project. */
    ServiceProviderData *data();

    /** @brief Check if @p filePath specifies a local or global installation path. */
    Q_INVOKABLE static InstallType installationTypeFromFilePath( const QString &filePath );

    /**
     * @brief Get a string to be displayed to users, which explains the save path.
     *
     * If the save path of the project is in an installation directory, this gets expressed in the
     * returned string.
     **/
    Q_INVOKABLE static QString savePathInfoStringFromFilePath( const QString &filePath );

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    /** @brief Get the script template text for @p templateType. */
    Q_INVOKABLE static QString scriptTemplateText(
            ScriptTemplateType templateType = DefaultScriptTemplate );
#endif

    /**
     * @brief Check if the current save location gets used by the Public Transport engine.
     * @note This does not check if the project is also installed, it only checks if it is saved
     *   in an installation directory. To do so use installationTypes().
     **/
    Q_INVOKABLE InstallType saveType() const;

    /** @brief Check if the project is installed locally and/or globally. */
    Q_INVOKABLE InstallTypes installationTypes() const;

    /**
     * @brief Return true, if the project is installed locally.
     * If true gets returned, the project may be opened from another path but is also installed
     * in the installation directory.
     **/
    Q_INVOKABLE bool isInstalledLocally() const;

    /**
     * @brief Return true, if the project is installed globally.
     * If true gets returned, the project may be opened from another path but is also installed
     * in the global installation directory.
     **/
    Q_INVOKABLE bool isInstalledGlobally() const;

    /** @brief Whether or not a test is currently running. */
    Q_INVOKABLE bool isTestRunning() const;

    /** @brief Whether or not the debugger is currently running. */
    Q_INVOKABLE bool isDebuggerRunning() const;

    /** @brief Get an info string describing the current save location of the project. */
    Q_INVOKABLE QString savePathInfoString() const;

    /** @brief Get a list of all functions that are implemented in the script. */
    Q_INVOKABLE QStringList scriptFunctions();

    /** @brief Get a list of file paths to all included scripts. */
    Q_INVOKABLE QStringList includedFiles();

    DepartureRequest getDepartureRequest( QWidget *parent = 0, bool* cancelled = 0 ) const;
    StopSuggestionRequest getStopSuggestionRequest( QWidget *parent = 0, bool* cancelled = 0 ) const;
    StopsByGeoPositionRequest getStopsByGeoPositionRequest(
            QWidget *parent = 0, bool* cancelled = 0 ) const;
    JourneyRequest getJourneyRequest( QWidget *parent = 0, bool* cancelled = 0 ) const;

    /** @brief Return the model for tests. */
    TestModel *testModel() const;

    QList< TestModel::Test > finishedTests() const;
    QList< TestModel::Test > startedTests() const;

signals:
    void nameChanged( const QString &newName );
    void dataChanged( const ServiceProviderData *newData );
    void iconNameChanged( const QString &newIconName );
    void iconChanged( const QIcon &newIcon );
    void saveTypeChanged( InstallType newInstallType );
    void savePathInfoStringChanged( const QString &newSavePathInfoString );
    void debuggerRunningChanged( bool debuggerRunning );
    void testRunningChanged( bool testRunning );
    void outputChanged();
    void consoleTextChanged( const QString &consoleText );

    void debuggerReady();

    void outputCleared();
    void outputAppended( const QString &newOutput );

    /** @brief Emitted when the @p title and/or @p icon for @p tabWidget has changed. */
    void tabTitleChanged( QWidget *tabWidget, const QString &title, const QIcon &icon );

    /** @brief Emitted when the modified status of the project has changed. */
    void modifiedStateChanged( bool modified );

    void saveLocationChanged( const QString &newXmlFilePath, const QString &oldXmlFilePath );

    /** @brief Emitted when the modified status of the project source document changes. */
    void projectSourceModifiedStateChanged( bool modified );

    /** @brief Emitted when the modified status of the script changes. */
    void scriptModifiedStateChanged( bool modified );

    /** @brief Emitted when @p message should be shown eg. in the status bar. */
    void informationMessage( const QString &message,
                             KMessageWidget::MessageType type = KMessageWidget::Information,
                             int timeout = 4000, QList<QAction*> actions = QList<QAction*>() );

    /** @brief Emitted when this project gets actived/deactived. */
    void activeProjectStateChanged( bool isActiveProject );

    /** @brief Emitted when the local installation state of this project changed. */
    void localInstallationStateChanged( bool isInstalledLocally );

    /** @brief Emitted when the global installation state of this project changed. */
    void globalInstallationStateChanged( bool isInstalledGlobally );

    /** @brief Emitted when this project should be closed. */
    void closeRequest();

    /** @brief Emitted when @p tab should be closed. */
    void tabCloseRequest( AbstractTab *tab );

    /** @brief Emitted when all tabs except @p tab should be closed. */
    void otherTabsCloseRequest( AbstractTab *tab );

    /** @brief Emitted when @p tab should be opened. */
    void tabOpenRequest( AbstractTab *tab );

    /** @brief Emitted when @p tab should be made the current tab. */
    void tabGoToRequest( AbstractTab *tab );

    /**
     * @brief Emitted when this project should be set as active project.
     * ProjectModel automatically connects to this signal.
     **/
    void setAsActiveProjectRequest();

    /** @brief Emitted when a test gets started, after a call to testProject(). */
    void testStarted();

    /** @brief Emitted when a test has finished, after a call to testProject(). */
    void testFinished( bool success );

    /** @brief Emitted when testing progresses, @p progress is between 0 and 1. */
    void testProgress( const QList< TestModel::Test > &finishedTests,
                       const QList< TestModel::Test > &startedTests );

public slots:
    /** @brief Save this project to @p xmlFilePath. */
    bool save( QWidget *parent = 0, const QString &xmlFilePath = QString() );

    /** @brief Ask the user where to save this project. */
    bool saveAs( QWidget *parent = 0 );

    /** @brief Install the project with the given @p installType. */
    bool install( QWidget *parent = 0, InstallType installType = DefaultInstallType );

    /** @brief Install the project locally for the current user. */
    void installLocally( QWidget *parent = 0 ) { install(parent, LocalInstallation); };

    /**
     * @brief Install the project globally for all users.
     * @note Needs root password using KAuth.
     **/
    void installGlobally( QWidget *parent = 0 ) { install(parent, GlobalInstallation); };

    /** @brief Uninstall the project with the given @p installType. */
    bool uninstall( QWidget *parent = 0, InstallType installType = DefaultInstallType );

    /** @brief Uninstall a locally installed version of the project for the current user. */
    void uninstallLocally( QWidget *parent = 0 ) { uninstall(parent, LocalInstallation); };

    /**
     * @brief Uninstall a globally installed version of the project for all users.
     * @note Needs root password using KAuth.
     **/
    void uninstallGlobally( QWidget *parent = 0 ) { uninstall(parent, GlobalInstallation); };

    /** @brief Emit a request to make this project active. */
    void setAsActiveProject() { emit setAsActiveProjectRequest(); };

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    /** @brief Aborts the debugger if it is running or updates the UI state to the debugger state. */
    void abortDebugger();
#endif

    /**
     * @brief Test the project, eg. for syntax errors in the script, correct results.
     * @see RunAllTests
     **/
    void testProject();

    /**
     * @brief Aborts all currently running tests, if any.
     * @see AbortRunningTests
     **/
    void abortTests();

    /**
     * @brief Clear all test results of a previous test run.
     * @see ClearTestResults
     **/
    void clearTestResults();

    /** @brief Start @p test. */
    bool startTest( TestModel::Test test );

    /** @brief Start @p tests. */
    void startTests( const QList< TestModel::Test > &tests );

    /** @brief Start all tests in @p testCase. */
    bool startTestCase( TestModel::TestCase testCase );

    /** @brief Show the project settings dialog. */
    void showSettingsDialog( QWidget *parent = 0 );

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    /** @brief Open the script tab if not done already and sets the cursor position to @p lineNumber. */
    void showScriptLineNumber( int lineNumber );

    /**
     * @brief Toggle breakpoint at @p lineNumber.
     * @param lineNumber The line number at which the breakpoint should be toggled. If this is -1,
     *   the current cursor position in the script tab gets used (if the script tab is opened).
     **/
    void toggleBreakpoint( int lineNumber = -1 );

    /** @brief Continue script execution until the current cursor position is reached. */
    void runToCursor();

    /** @brief Run the getTimetable() script function. */
    void runGetTimetable();

    /** @brief Run the getStopSuggestions() script function with a stop name part as argument. */
    void runGetStopSuggestions();

    /** @brief Run the getStopSuggestions() script function with a geo position as argument. */
    void runGetStopsByGeoPosition();

    /** @brief Run the getJourneys() script function. */
    void runGetJourneys();

    /** @brief Run the getTimetable() script function and interrupt at breakpoints and executions. */
    void debugGetTimetable();

    /** @brief Run the getStopSuggestions() script function and interrupt at breakpoints and executions. */
    void debugGetStopSuggestions();

    /** @brief Run the getStopSuggestions() script function and interrupt at breakpoints and executions. */
    void debugGetStopsByGeoPosition();

    /** @brief Run the getJourneys() script function and interrupt at breakpoints and executions. */
    void debugGetJourneys();

    /** @brief Show the main script tab. */
    ScriptTab *showScriptTab( QWidget *parent = 0 );

    /** @brief Show a script tab containing the external script at @p filePath. */
    ScriptTab *showExternalScriptTab( const QString &filePath, QWidget *parent = 0 );
#endif // BUILD_PROVIDER_TYPE_SCRIPT

    /** @brief Show the project dashboard tab. */
    DashboardTab *showDashboardTab( QWidget *parent = 0 );

    /** @brief Show the project source tab. */
    ProjectSourceTab *showProjectSourceTab( QWidget *parent = 0 );

    /** @brief Show the plasma preview tab. */
    PlasmaPreviewTab *showPlasmaPreviewTab( QWidget *parent = 0 );

    /** @brief Show the web tab. */
    WebTab *showWebTab( QWidget *parent = 0 );

    /** @brief Show the tab of the given @p tabType. */
    AbstractTab *showTab( TabType tabType, QWidget *parent = 0 );

    /**
     * @brief Add @p output to the projects output.
     * @see output
     **/
    void appendOutput( const QString &output, const QColor &color = QColor() );

    /**
     * @brief Clear collected output.
     * @see output
     **/
    void clearOutput();

    /**
     * @brief Appends @p text to the console.
     * @see consoleText
     **/
    void appendToConsole( const QString &text );

    /**
     * @brief Clear collected console text.
     * @see consoleText
     **/
    void clearConsoleText();

protected slots:
    void slotTabTitleChanged( const QString &title );
    void slotTabCloseRequest();
    void slotOtherTabsCloseRequest();
    void slotModifiedStateChanged();

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    void jobStarted( JobType type, const QString &useCase, const QString &objectName );
    void jobDone( JobType type, const QString &useCase, const QString &objectName,
                  const DebuggerJobResult &result );
    void scriptSaved();

    void loadScriptResult( ScriptErrorType lastScriptError,
                           const QString &lastScriptErrorString,
                           const QStringList &globalFunctions,
                           const QStringList &includedFiles );
    void functionCallResult( const QSharedPointer< AbstractRequest > &request,
                             bool success, const QString &explanation,
                             const QList< TimetableData > &timetableData,
                             const QVariant &returnValue );
    void scriptOutput( const QString &message, const QScriptContextInfo &context );
    void scriptMessageReceived( const QString &errorMessage, const QScriptContextInfo &context,
                                const QString &failedParseText,
                                Helper::ErrorSeverity severity = Helper::Information );

    /** @brief Show a script tab containing an external script, included into the main script. */
    ScriptTab *showExternalScriptActionTriggered( QWidget *parent = 0 );
#endif // BUILD_PROVIDER_TYPE_SCRIPT

    /**
     * @brief The active project has changed from @p previousProject to @p project.
     *
     * This slot emits the activeProjectStateChanged() signal if @p project or @p previousProject
     * is this project.
     **/
    void slotActiveProjectChanged( Project *project, Project *previousProject );

    /** @brief Simply emit the given @p message with the informationMessage() signal.
     * The view argument is ignored. */
    void slotInformationMessage( KTextEditor::View*, const QString &message ) {
        emit informationMessage( message );
    };

    void emitErrorMessage( const QString &message ) {
        emit informationMessage( message, KMessageWidget::Error, -1 );
    };

    /** @brief Simply emit the given @p message with the informationMessage() signal. */
    void emitInformationMessage( const QString &message,
                                 KMessageWidget::MessageType type = KMessageWidget::Information,
                                 int timeout = 4000 ) {
        emit informationMessage(message, type, timeout);
    };

    void showTextHint( const KTextEditor::Cursor &position, QString &text );

    void projectSourceDocumentChanged( KTextEditor::Document *projectSourceDocument );

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    void scriptAdded( const QString &fileName );
    void scriptFileChanged( const QString &fileName );

    void debugInterrupted( int lineNumber, const QString &fileName, const QDateTime &timestamp );
    void debugContinued();
    void debugStarted();
    void debugStopped( const ScriptRunData &scriptRunData );
    void debugAborted();
    void waitingForSignal();
    void wokeUpFromSignal( int time );

    /** @brief An uncaught exception occured in the script at @p lineNumber. */
    void scriptException( int lineNumber, const QString &errorMessage,
                          const QString &fileName = QString() );

    void scriptTabDestroyed();
    void externalScriptTabDestroyed( QObject *tab );

    void evaluationResult( const EvaluationResult &result );
    void commandExecutionResult( const QString &returnValue, bool error = false );
#endif

    void dashboardTabDestroyed();
    void projectSourceTabDestroyed();
    void plasmaPreviewTabDestroyed();
    void webTabDestroyed();

    void testJobStarted( TestModel::Test test, JobType type, const QString &useCase );
    void testJobDone( TestModel::Test test, JobType type, const QString &useCase,
                      const DebuggerJobResult &result );
    TestModel::Test testFromObjectName( const QString &objectName );

    void testActionTriggered();
    void testCaseActionTriggered();

protected:
    void setProjectModel( ProjectModel *projectModel );

private:
    Q_DECLARE_PRIVATE( Project )
    Q_DISABLE_COPY( Project )

    ProjectPrivate *d_ptr;
};

Q_DECLARE_METATYPE( Project::ProjectActionData );

#endif // Multiple inclusion guard
