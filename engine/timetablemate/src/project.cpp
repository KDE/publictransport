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
#include "project.h"

// Own includes
#include "projectmodel.h"
#include "projectsettingsdialog.h"
#include "javascriptmodel.h"
#include "javascriptcompletionmodel.h"
#include "javascriptparser.h"
#include "accessorinfoxmlwriter.h"
#include "accessorinfotester.h"
#include "publictransportpreview.h"
#include "testmodel.h"
#include "tabs/abstracttab.h"
#include "tabs/dashboardtab.h"
#include "tabs/projectsourcetab.h"
#include "tabs/scripttab.h"
#include "tabs/webtab.h"
#include "tabs/plasmapreviewtab.h"
#include "docks/breakpointdockwidget.h"
#include "debugger/debugger.h"
#include "debugger/breakpointmodel.h"
#include "debugger/debuggerjobs.h"
#include "debugger/timetabledatarequestjob.h"

// PublicTransport engine includes
#include <engine/timetableaccessor.h>
#include <engine/timetableaccessor_info.h>
#include <engine/timetableaccessor_script.h>
#include <engine/request.h>

// KDE includes
#include <KUrlComboBox>
#include <KLineEdit>
#include <KWebView>
#include <KAction>
#include <KActionMenu>
#include <KMenu>
#include <KMessageBox>
#include <KFileDialog>
#include <KInputDialog>
#include <KIcon>
#include <KStandardDirs>
#include <KDateTimeWidget>
#include <KLocale>
#include <KLocalizedString>
#include <KAuth/ActionReply>
#include <KAuth/Action>
#include <KTextEditor/Document>
#include <KTextEditor/View>
#include <KTextEditor/TemplateInterface>
#include <KTextEditor/MarkInterface>
#include <ThreadWeaver/WeaverInterface>

// Qt includes
#include <QWidget>
#include <QToolTip>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QMenu>
#include <QFormLayout>
#include <QApplication>
#include <QFile>
#include <QTextCodec>
#include <QBuffer>
#include <QFileInfo>

class ProjectPrivate
{
public:
    enum ConnectProjectActionFlag {
        NoConnectionFlags       = 0x0000,
        AutoUpdateEnabledState  = 0x0001  /**< If this flag is set, the enabled state of the
                * connected project action gets updated in updateProjectActions().
                * Do not use this flag, if the action is always enabled or if its enabled state
                * gets updated in another way, eg. by connecting to its setEnabled()/setDisabled()
                * slots. */
    };
    Q_DECLARE_FLAGS( ConnectProjectActionFlags, ConnectProjectActionFlag );

    enum TestState {
        NoTestRunning,
        TestsRunning,
        TestsGetAborted
    };

    ProjectPrivate( Project *project ) : state(Project::Uninitialized),
          projectModel(0), projectSourceBufferModified(false),
          dashboardTab(0), projectSourceTab(0), scriptTab(0), plasmaPreviewTab(0), webTab(0),
          accessor(new TimetableAccessor(0, project)), debugger(new Debugger::Debugger(project)),
          executionLine(-1), testModel(new TestModel(project)), testState(NoTestRunning),
          q_ptr(project)
    {
    };

    static inline QString serviceProviderIdFromProjectFileName( const QString &fileName ) {
        return fileName.left( fileName.lastIndexOf('.') );
    };

    static QString scriptTemplateText( Project::ScriptTemplateType templateType
                                       = Project::DefaultScriptTemplate )
    {
        QString templateText = QString::fromUtf8(
                "/** Accessor for ${Service Provider}\n"
                "  * © ${year}, ${Author} */\n"
                "\n" );

        switch ( templateType ) {
        case Project::ScriptRubyTemplate:
            templateText +=
                    "\n// Create Kross action"
                    "var action = Kross.action( \"RubyScript\" );\n"
                    "\n"
                    "// Propagate action to the Python script\n"
                    "action.addQObject( action, \"MyAction\" );\n"
                    "\n"
                    "// Set the interpreter to use, eg. \"python\", \"ruby\"\n"
                    "action.setInterpreter( \"ruby\" );\n"
                    "\n"
                    "// Set the code to execute and trigger execution\n"
                    "action.setCode( \"${cursor}\" /* TODO: Insert ruby code here */ );\n"
                    "action.trigger();\n";
            break;
        case Project::ScriptPythonTemplate:
            templateText +=
                    "\n// Create Kross action"
                    "var action = Kross.action( \"PythonScript\" );\n"
                    "\n"
                    "// Propagate action to the Python script\n"
                    "action.addQObject( action, \"MyAction\" );\n"
                    "\n"
                    "// Set the interpreter to use, eg. \"python\", \"ruby\"\n"
                    "action.setInterpreter( \"python\" );\n"
                    "\n"
                    "// Set the code to execute and trigger execution\n"
                    "action.setCode( \"import MyAction; print 'This is Python. name=>', MyAction.interpreter()\"${cursor} );\n"
                    "action.trigger();\n";
            break;
        case Project::ScriptQtScriptTemplate:
            templateText +=
                    "\n// This function gets called to determine the features of this accessor\n"
                    "function usedTimetableInformations() {\n"
                    "    // Return a list of TimetableInformation values, that are used by this script.\n"
                    "    // Required values like DepartureDateTime/DepartureTime or TypeOfVehicle\n"
                    "    // are not needed here\n"
                    "    return [ 'Arrivals', 'StopID', 'RouteStops' ];\n"
                    "}\n"
                    "\n"
                    "// This function gets called when departures/arrivals are requested\n"
                    "function getTimetable( values ) {\n"
                    "    // Construct an URL from the given values\n"
                    "    var url = \"http://www.page.com\" +\n"
                    "            \"?stop=\" + values.stop + \"!\" +\n"
                    "            \"&boardType=\" + (values.dataType == \"arrivals\" ? \"arr\" : \"dep\") +\n"
                    "            \"&date=\" + helper.formatDateTime(values.dateTime, \"dd.MM.yy\") +\n"
                    "            \"&time=\" + helper.formatDateTime(values.dateTime, \"hh:mm\") +\n"
                    "            \"&maxJourneys=\" + values.maxCount;\n"
                    "\n"
                    "    // Create a NetworkRequest object for the URL\n"
                    "    var request = network.createRequest( url );\n"
                    "\n"
                    "    // Connect to the finished signal,\n"
                    "    // an alternative is the readyRead signal to parse iteratively\n"
                    "    request.finished.connect( parseTimetable );\n"
                    "\n"
                    "    // Start the download,\n"
                    "    // the parseTimetable() function will be called when it is finished\n"
                    "    network.get( request );\n"
                    "}\n"
                    "\n"
                    "// This function is connected to the finished signal of network requests\n"
                    "// started in getTimetable()\n"
                    "function parseTimetable( html ) {\n"
                    "    // TODO: Parse the contents of the received document and add results \n"
                    "    // using result.addData()\n"
                    "    // Use helper.findHtmlTags(), helper.findFirstHtmlTag() or \n"
                    "    // helper.findNamedHtmlTags() to parse HTML documents (see documentation)\n"
                    "    ${cursor}\n"
                    "}\n"
                    "\n"
                    "// This function gets called when stop suggestions are requested\n"
                    "function getStopSuggestions( values  ) {\n"
                    "    // Construct an URL from the given values\n"
                    "    var url = \"http://www.page.com?stop=\" + values.stop;\n"
                    "\n"
                    "    // Download the document synchronously\n"
                    "    var json = network.getSynchronous( url );\n"
                    "\n"
                    "    // Check if the download was completed successfully\n"
                    "    if ( !network.lastDownloadAborted ) {\n"
                    "        // TODO: Find all stop suggestions\n"
                    "        result.addData({ StopName: \"Test-Stop\",\n"
                    "                         StopID: \"123\",\n"
                    "                         StopWeight: stop[3] });\n"
                    "        return result.hasData();\n"
                    "    } else {\n"
                    "        return false;\n"
                    "    }\n"
                    "}\n"
                    "\n"
                    "// TODO: To parse journeys implement getJourneys()\n";
        default:
            break;
        }

        return templateText;
    };

    // Initialize member variables, connect slots
    bool initialize()
    {
        Q_Q( Project );
        Q_ASSERT( state == Project::Uninitialized );

        // Connect to signals of the debugger
        q->connect( debugger, SIGNAL(interrupted()), q, SLOT(debugInterrupted()) );
        q->connect( debugger, SIGNAL(continued(bool)), q, SLOT(debugContinued()) );
        q->connect( debugger, SIGNAL(started()), q, SLOT(debugStarted()) );
        q->connect( debugger, SIGNAL(stopped()), q, SLOT(debugStopped()) );
        q->connect( debugger, SIGNAL(informationMessage(QString)), q, SIGNAL(informationMessage(QString)) );
        q->connect( debugger, SIGNAL(errorMessage(QString)), q, SLOT(emitErrorMessage(QString)) );
        q->connect( debugger, SIGNAL(loadScriptResult(ScriptErrorType,QString)),
                    q, SLOT(loadScriptResult(ScriptErrorType,QString)) );
        q->connect( debugger, SIGNAL(requestTimetableDataResult(QSharedPointer<AbstractRequest>,bool,QString,QList<TimetableData>,QScriptValue)),
                    q, SLOT(functionCallResult(QSharedPointer<AbstractRequest>,bool,QString,QList<TimetableData>,QScriptValue)) );

        state = Project::NoProjectLoaded;
        return true;
    };

    // Load project from accessor XML document at @p xmlFilePath
    bool loadProject( const QString &xmlFilePath )
    {
        if ( projectSourceTab ) {
            projectSourceTab->document()->closeUrl( false );
        }
        if ( scriptTab ) {
            scriptTab->document()->closeUrl( false );
        }

        // Try to open the XML in the Kate part in the "Accessor Source" tab
        if ( !QFile::exists(xmlFilePath) ) {
            // Project file not found, create a new one from template
            errorHappened( Project::ProjectFileNotFound, i18nc("@info", "The project file <filename>"
                        "%1</filename> could not be found.", xmlFilePath) );
            insertAccessorTemplate();
            return false;
        }

        KUrl url( xmlFilePath );
        if ( projectSourceTab ) {
            if ( !projectSourceTab->document()->openUrl(url) ) {
                errorHappened( Project::ProjectFileNotReadable,
                               i18nc("@info", "Could not open accessor document "
                                     "<filename>%1</filename>.", url.url()) );
            }
            projectSourceTab->document()->setModified( false );
        }

        if ( !readProjectSourceDocument(xmlFilePath) ) {
            insertAccessorTemplate();
            return false;
        }

        // Set read only mode of the kate parts if the files aren't writable
        QFile test( url.path() );
        if ( test.open(QIODevice::ReadWrite) ) {
            if ( projectSourceTab ) {
                projectSourceTab->document()->setReadWrite( true );
            }
            if ( scriptTab ) {
                scriptTab->document()->setReadWrite( true );
            }
            test.close();
        } else {
            if ( projectSourceTab ) {
                projectSourceTab->document()->setReadWrite( false );
            }
            if ( scriptTab ) {
                scriptTab->document()->setReadWrite( false );
            }
        }

        // Load script file referenced by the XML
        if ( !loadScript() ) {
            // Could not load, eg. script file not found
            return false;
        }

        setXmlFilePath( xmlFilePath );
        state = Project::ProjectSuccessfullyLoaded;
        return true;
    };

    bool isActiveProject() const
    {
        Q_Q( const Project );
        return projectModel ? projectModel->activeProject() == q : false;
    };

    bool isProjectSourceModified() const
    {
        return (projectSourceTab && projectSourceTab->isModified()) || projectSourceBufferModified;
    };

    bool isScriptModified() const
    {
        return (scriptTab && scriptTab->isModified()) || !unsavedScriptContents.isEmpty();
    };

    bool isModified() const
    {
        return isScriptModified() || isProjectSourceModified() ||
            (plasmaPreviewTab && plasmaPreviewTab->isModified()) ||
            (webTab && webTab->isModified());
    };

    inline bool isTestRunning() const
    {
        return testState != NoTestRunning;
    };

    inline bool isDebuggerRunning() const
    {
        return debugger->isRunning();
    };

    QString projectName() const
    {
        QString name = accessor->info()->names()[ KGlobal::locale()->country() ];
        if ( name.isEmpty() ) {
            // No translated name
            name = accessor->info()->name();
        }

        if ( name.isEmpty()) {
            // No name given, use service provider ID if available
            name = serviceProviderID.isEmpty()
                    ? i18nc("@info/plain", "New Project") : serviceProviderID;
        } else {
            // Add service provider ID to the name
            name += " (" + serviceProviderID + ')';
        }
        return name;
    };

    inline const TimetableAccessorInfo *info()
    {
        return accessor->info();
    };

    QString iconName() const
    {
        if ( serviceProviderID.isEmpty() ) {
            // New unsaved project
            return "folder-development";
        } else {
            // Project file is stored on disk
            Project::InstallType installType = saveType();
            switch ( installType ) {
            case Project::LocalInstallation:
                return "folder-orange";
            case Project::GlobalInstallation:
                return "folder-red";
            case Project::NoInstallation:
            default:
                return "folder-development";
            }
        }
    };

    inline QIcon projectIcon() const
    {
        return KIcon( iconName() );
    };

    bool isInstalledLocally() const
    {
        const QString localSaveDir = KGlobal::dirs()->saveLocation( "data",
                "plasma_engine_publictransport/accessorInfos/" );
        const QString fileName = QFileInfo( filePath ).fileName();
        return QFile::exists( localSaveDir + '/' + fileName );
    };

    bool isInstalledGlobally() const
    {
        const QString globalSaveDir = KGlobal::dirs()->findDirs( "data",
                    "plasma_engine_publictransport/accessorInfos/" ).last();
        const QString fileName = QFileInfo( filePath ).fileName();
        return QFile::exists( globalSaveDir + '/' + fileName );
    };

    inline Project::InstallType saveType() const
    {
        return Project::installationTypeFromFilePath( filePath );
    };

    Project::InstallTypes installationTypes() const
    {
        Project::InstallTypes ret = Project::NoInstallation;
        if ( isInstalledLocally() ) {
            ret |= Project::LocalInstallation;
        }
        if ( isInstalledGlobally() ) {
            ret |= Project::GlobalInstallation;
        }
        return ret;
    };

    inline QString savePathInfoString() const
    {
        QString message = Project::savePathInfoStringFromFilePath( filePath );
        const Project::InstallType installType = saveType();
        switch ( installType ) {
        case Project::LocalInstallation:
            if ( isInstalledGlobally() ) {
                message += ", " + i18nc("@info:tooltip", "also installed globally");
            }
            break;
        case Project::GlobalInstallation:
            if ( isInstalledLocally() ) {
                message += ", " + i18nc("@info:tooltip", "also installed locally");
            }
            break;
        case Project::NoInstallation:
        default:
            if ( isInstalledLocally() && isInstalledGlobally() ) {
                message += ", " + i18nc("@info:tooltip", "installed locally and globally");
            } else if ( isInstalledLocally() ) {
                message += ", " + i18nc("@info:tooltip", "installed locally");
            } else if ( isInstalledGlobally() ) {
                message += ", " + i18nc("@info:tooltip", "installed globally");
            }
            break;
        }
        return message;
    };

    // Read accessor XML document from file or from opened accessor document tab
    bool readProjectSourceDocument( const QString &xmlFilePath )
    {
        Q_Q( Project );
        if ( xmlFilePath.isEmpty() ) {
            kDebug() << "No xml file path given, insert template";
            insertAccessorTemplate();
            return true;
        }

        // Try to read the XML contents
        bool success = false;
        if ( projectSourceTab ) {
            // Use text in already loaded accessor document
            QTextCodec *codec = QTextCodec::codecForName( projectSourceTab->document()->encoding().isEmpty()
                                ? "UTF-8" : projectSourceTab->document()->encoding().toLatin1() );
            QByteArray text = codec->fromUnicode( projectSourceTab->document()->text() );
            QBuffer buffer( &text, q );
            success = readAccessorInfoXml( &buffer, xmlFilePath );
        } else {
            // Read text from file, accessor document not loaded
            QFile file( xmlFilePath );
            success = readAccessorInfoXml( &file, xmlFilePath );
        }

        if ( success ) {
            return true;
        } else {
            errorHappened( Project::ErrorWhileLoadingProject,
                           i18nc("@info", "The XML file <filename>%1</filename> "
                                 "could not be read.", xmlFilePath) );
            return false;
        }
    };

    // Read accessor XML document from file
    bool readAccessorInfoXml( const QString &fileName )
    {
        QFile file( fileName );
        return readAccessorInfoXml( &file, fileName );
    };

    // Read accessor XML document from @p device, set file name to @p fileName
    bool readAccessorInfoXml( QIODevice *device, const QString &fileName )
    {
        Q_Q( Project );

        // Recreate accessor from the contents of device
        AccessorInfoXmlReader reader;
        delete accessor;
        accessor = reader.read( device, fileName, AccessorInfoXmlReader::ReadErrorneousFiles, q );
        if( accessor ) {
            q->emit nameChanged( projectName() );
            q->emit iconNameChanged( iconName() );
            q->emit iconChanged( projectIcon() );
            q->emit infoChanged( info() );
            return true;
        } else {
            kDebug() << "Accessor is invalid" << reader.errorString() << fileName;
            errorHappened( Project::ErrorWhileLoadingProject, reader.errorString() );

            insertAccessorTemplate();
            return false;
        }
    };

    // Write accessor XML document to @p fileName
    bool writeAccessorInfoXml( const QString &fileName )
    {
        if ( !accessor ) {
            kDebug() << "No accessor loaded";
            return false;
        }

        AccessorInfoXmlWriter writer;
        QFile file( fileName );
        return writer.write( &file, accessor );
    };

    // Load the script into the script tab,
    // if no script has been created yet the given @p templateType gets inserted
    bool loadScript( Project::ScriptTemplateType templateType = Project::DefaultScriptTemplate )
    {
        Q_Q( Project );
        if ( !scriptTab ) {
            kDebug() << "No script tab opened";
            return true;
        }

        scriptTab->document()->closeUrl( false );
        scriptTab->document()->setModified( false );

        const QString scriptFile = accessor->info()->scriptFileName();
        if ( scriptFile.isEmpty() ) {
            insertScriptTemplate( templateType );
            return false;
        } else {
            if ( !QFile::exists(scriptFile) ) {
                errorHappened( Project::ScriptFileNotFound,
                               i18nc("@info", "The script file <filename>%1</filename> "
                                     "could not be found.", scriptFile) );
                return false;
            }
            if ( !scriptTab->document()->openUrl(KUrl(scriptFile)) ) {
                return false;
            }
            scriptTab->document()->setModified( false );
        }

        q->emit tabTitleChanged( scriptTab, scriptTab->title(), scriptTab->icon() );
        return true;
    };

    bool checkSyntax( const QString &scriptText )
    {
        Q_Q( Project );
        QScriptSyntaxCheckResult syntax = QScriptEngine::checkSyntax( scriptText );
        if ( syntax.state() == QScriptSyntaxCheckResult::Error ) {
            // Open script tab and set the cursor position to the error position
            ScriptTab *tab = q->showScriptTab();
            tab->document()->views().first()->setCursorPosition(
                    KTextEditor::Cursor(syntax.errorLineNumber() - 1, syntax.errorColumnNumber()) );

            // Emit an information message about the syntax error
            q->emit informationMessage(
                    i18nc("@info", "Syntax error at line %1: <message>%2</message>",
                    syntax.errorLineNumber(), syntax.errorMessage()),
                    KMessageWidget::Error, 10000 );

            return false;
        } else {
            return true;
        }
    };

    // Set the contents of the accessor XML document to text in the accessor document tab
    bool setProjectSourceDocumentText( const QString &text )
    {
        if ( !projectSourceTab ) {
            kDebug() << "No accessor tab opened";
            return true;
        }

        projectSourceTab->document()->closeUrl( false );
        projectSourceTab->document()->setModified( false );

        if ( text.isEmpty() ) {
            insertAccessorTemplate();
            return false;
        } else {
            // Open file if already stored to have the correct url set in KTextEditor::Document
            if ( !filePath.isEmpty() &&
                !projectSourceTab->document()->openUrl(KUrl(filePath)) )
            {
                errorHappened( Project::ProjectFileNotReadable,
                               i18nc("@info", "Could not open accessor XML document "
                                     "<filename>%1</filename> could not be found.", filePath) );
                return false;
            }

            // Update document contents with current project settings
            if ( !projectSourceTab->document()->setText(text) ) {
                return false;
            }
        }

        return true;
    };

    // Set the xml file path to a canonical version of @p fileName
    // Should always be used instead of setting the value of xmlFilePath directly
    void setXmlFilePath( const QString &fileName )
    {
        Q_Q( Project );
        const QString oldXmlFilePath = filePath;
        filePath = QFileInfo( fileName ).canonicalFilePath();

        if ( oldXmlFilePath != filePath ) {
            // Update member variables
            KUrl url( filePath );
            const QString oldServiceProviderId = serviceProviderID;
            serviceProviderID = serviceProviderIdFromProjectFileName( url.fileName() );
            openedPath = url.directory();

            // Notify about changes
            q->emit saveLocationChanged( filePath, oldXmlFilePath );

            const Project::InstallType oldInstallType =
                    Project::installationTypeFromFilePath( oldXmlFilePath );
            const Project::InstallType newInstallType = saveType();
            if ( oldInstallType != newInstallType ) {
                // The "save path info string" changes with the installation type
                q->emit saveTypeChanged( newInstallType );
                q->emit savePathInfoStringChanged( savePathInfoString() );
            }

            if ( oldServiceProviderId != serviceProviderID ) {
                q->emit nameChanged( projectName() );
                q->emit iconNameChanged( iconName() );
                q->emit iconChanged( projectIcon() );
                q->emit infoChanged( info() );
            }
        }
    };

    void insertAccessorTemplate()
    {
        Q_Q( Project );
        delete accessor;
        accessor = new TimetableAccessor( 0, q );
        q->emit nameChanged( projectName() );
        q->emit iconNameChanged( iconName() );
        q->emit iconChanged( projectIcon() );
        q->emit infoChanged( info() );
    };

    void insertScriptTemplate( Project::ScriptTemplateType templateType
                               = Project::DefaultScriptTemplate )
    {
        if ( templateType == Project::NoScriptTemplate ) {
            // Do nothing
            return;
        }
        if ( !scriptTab ) {
            kWarning() << "No script tab created";
            return;
        }

        // Get the template interface
        KTextEditor::View *scriptView = scriptTab->document()->views().first();
        KTextEditor::TemplateInterface *templateInterface =
                qobject_cast<KTextEditor::TemplateInterface*>( scriptView );

        if ( templateInterface ) {
            // Insert a template with author information
            templateInterface->insertTemplateText( KTextEditor::Cursor(),
                    scriptTemplateText(templateType), QMap<QString, QString>() );
        }
    };

    void errorHappened( Project::Error error, const QString &errorString )
    {
        Q_Q( Project );
        if ( state == Project::ProjectError ) {
            kDebug() << "Following Error:" << error << errorString;
            return;
        }

        kDebug() << "Error:" << error << errorString;
        state = Project::ProjectError;
        q->emitInformationMessage( errorString, KMessageWidget::Error, 10000 );
    };

    void connectTab( AbstractTab *tab )
    {
        Q_Q( Project );
        q->connect( tab, SIGNAL(titleChanged(QString)), q, SLOT(slotTabTitleChanged(QString)) );
        q->connect( tab, SIGNAL(modifiedStatusChanged(bool)), q, SLOT(slotModifiedStateChanged()) );
    };

    inline QWidget *parentWidget( QWidget *parentToUse = 0 ) const
    {
        Q_Q( const Project );
        return parentToUse ? parentToUse : qobject_cast<QWidget*>(q->parent());
    };

    bool isActionEnabled( Project::ProjectAction projectAction ) const
    {
        switch ( projectAction ) {
        case Project::SaveAs:
        case Project::Install:
        case Project::Uninstall:
        case Project::InstallGlobally:
        case Project::UninstallGlobally:
        case Project::Close:
        case Project::ShowProjectSettings:
        case Project::ShowDashboard:
        case Project::ShowScript:
        case Project::ShowProjectSource:
        case Project::ShowPlasmaPreview:
            // Always enabled actions
            return true;

        case Project::Save:
            // Enable save action only when the project is modified
            return isModified();

        case Project::ShowHomepage:
            // Only enable "Open Homepage" action if an URL is available
            return !accessor->info()->url().isEmpty();

        case Project::SetAsActiveProject:
            // Only enable "Set as Active Project" action if the project isn't already active
            return !isActiveProject();

        case Project::StepInto:
        case Project::StepOver:
        case Project::StepOut:
        case Project::Continue:
            // Only enabled if the debugger is interrupted
            return debugger->isInterrupted();

        case Project::Interrupt:
            // Only enabled if the debugger is running, but not interrupted
            return debugger->state() == Running;

        case Project::RunToCursor:
            // Only enabled if the debugger is interrupted or not running
            return debugger->state() != Running;

        case Project::AbortDebugger:
            // Only enabled if the debugger is running or interrupted
            return debugger->state() != NotRunning;

        case Project::ToggleBreakpoint:
            // Only enabled if a script tab is opened
            return scriptTab;

        case Project::RemoveAllBreakpoints:
            // Only enabled if the breakpoint model isn't empty
            return debugger->breakpointModel()->rowCount() > 0;

        case Project::ClearTestResults:
            // Only enabled if there are test results
            // and the debugger and the test are both currently not running
            return !testModel->isEmpty() && !isTestRunning() && !isDebuggerRunning();

        case Project::AbortRunningTests:
            // Only enabled if tests are currently running
            return isTestRunning();

        case Project::RunAllTests:
        case Project::RunSpecificTest:
        case Project::RunSpecificTestCase:
        case Project::SpecificTestCaseMenuAction:
        case Project::RunMenuAction:
        case Project::RunGetTimetable:
        case Project::RunGetStopSuggestions:
        case Project::RunGetJourneys:
        case Project::DebugMenuAction:
        case Project::DebugGetTimetable:
        case Project::DebugGetStopSuggestions:
        case Project::DebugGetJourneys:
            // Only enabled if the debugger and the test are both currently not running
            return !isTestRunning() && !isDebuggerRunning();

        default:
            kDebug() << "Unknown project action" << projectAction;
            return false;
        }
    };

    // @param autoUpdateEnabledState Set to true, if the enabled state of the action should be
    //   updated in updateProjectActions(). Leave false, if the action is always enabled or if
    //   its enabled state gets updated in another way, eg. by connecting to the
    //   setEnabled()/setDisabled() slots
    void connectProjectAction( Project::ProjectAction actionType, QAction *action,
                               bool doConnect, QObject *receiver, const char *slot,
                               ConnectProjectActionFlags flags = NoConnectionFlags )
    {
        Q_Q( Project );
        if ( doConnect ) {
            action->setEnabled( isActionEnabled(actionType) );
            if ( receiver ) {
                q->connect( action, SIGNAL(triggered(bool)), receiver, slot );
            }
            if ( flags.testFlag(ProjectPrivate::AutoUpdateEnabledState) &&
                !(externProjectActions.contains(actionType) &&
                externProjectActions.values(actionType).contains(action)) )
            {
                externProjectActions.insert( actionType, action );
            }
        } else {
            action->setEnabled( false );
            if ( receiver ) {
                q->disconnect( action, SIGNAL(triggered(bool)), receiver, slot );
            }
            if ( flags.testFlag(ProjectPrivate::AutoUpdateEnabledState) ) {
                externProjectActions.remove( actionType, action );
            }
        }
    };

    // Enable/disable project actions of the given @p actionTypes (also external actions)
    void updateProjectActions( const QList< Project::ProjectAction > &actionTypes )
    {
        foreach ( Project::ProjectAction actionType, actionTypes ) {
            const bool enabled = isActionEnabled( actionType );
            QList< QAction* > actions = externProjectActions.values( actionType );
            foreach ( QAction *action, actions ) {
                action->setEnabled( enabled );
            }
        }
    };

    inline void updateProjectActions( Project::ProjectActionGroup group,
                                      const QList< Project::ProjectAction > &additionalActionTypes
                                            = QList< Project::ProjectAction >() )
    {
        updateProjectActions(Project::actionsFromGroup(group) << additionalActionTypes);
    };

    inline void updateProjectActions( const QList< Project::ProjectActionGroup > &groups,
                                      const QList< Project::ProjectAction > &additionalActionTypes
                                            = QList< Project::ProjectAction >() )
    {
        updateProjectActions(actionsFromGroups(groups) << additionalActionTypes);
    };

    QList< Project::ProjectAction > actionsFromGroups(
            const QList<Project::ProjectActionGroup> &groups )
    {
        QList< Project::ProjectAction > actionTypes;
        foreach ( Project::ProjectActionGroup group, groups ) {
            actionTypes << Project::actionsFromGroup( group );
        }
        return actionTypes;
    };

    enum ProjectActivationReason {
        ActivateProjectForTests,
        ActivateProjectForDebugging
    };

    // Asks if the project should be activated
    bool askForProjectActivation( ProjectActivationReason reason )
    {
        Q_Q( Project );
        if ( isActiveProject() ) {
            return true;
        }

        QString message, dontAskAgainName;
        switch ( reason ) {
        case ActivateProjectForTests:
            message = i18nc("@info", "Test results cannot be seen for non-active projects.<nl />"
                            "Do you want to make this project active now?");
            dontAskAgainName = "make_project_active_for_tests";
            break;
        case ActivateProjectForDebugging:
            message = i18nc("@info", "Docks like <interface>Variables</interface>, "
                            "<interface>Backtrace</interface> or <interface>Output</interface> "
                            "only show data for the active project.<nl />"
                            "Do you want to make this project active now?");
            dontAskAgainName = "make_project_active_for_debugging";
            break;
        default:
            kWarning() << "Unknown project activation reason";
            return false;
        }

        const int result = KMessageBox::questionYesNoCancel( parentWidget(),
                message, i18nc("@title:window", "Activate Project?"), KStandardGuiItem::yes(),
                KStandardGuiItem::no(), KStandardGuiItem::cancel(), dontAskAgainName );
        if ( result == KMessageBox::Yes ) {
            q->setAsActiveProject();
        } else if ( result == KMessageBox::Cancel ) {
            return false;
        }
        return true;
    };

    // Call script function @p functionName using @p request in the given @p debugMode
    void callScriptFunction( AbstractRequest *request,
                             Debugger::DebugFlag debugMode = Debugger::InterruptOnExceptions )
    {
        Q_Q( Project );
        if ( !askForProjectActivation(ActivateProjectForDebugging) ) {
            return;
        }

        q->showScriptTab();

        const QString &text = q->scriptText();
        debugger->loadScript( text, accessor->info() );
        debugger->requestTimetableData( request, debugMode );
    };

    // Call script function getTimetable() in the given @p debugMode
    void callGetTimetable( Debugger::DebugFlag debugMode = Debugger::InterruptOnExceptions )
    {
        Q_Q( Project );
        bool cancelled = false;
        DepartureRequest request = q->getDepartureRequest( parentWidget(), &cancelled );
        if ( !cancelled ) {
            callScriptFunction( &request, debugMode );
        }
    };

    // Call script function getStopSuggestions() in the given @p debugMode
    void callGetStopSuggestions( Debugger::DebugFlag debugMode = Debugger::InterruptOnExceptions )
    {
        Q_Q( Project );
        bool cancelled = false;
        StopSuggestionRequest request = q->getStopSuggestionRequest( parentWidget(), &cancelled );
        if ( !cancelled ) {
            callScriptFunction( &request, debugMode );
        }
    };

    // Call script function getJourneys() in the given @p debugMode
    void callGetJourneys( Debugger::DebugFlag debugMode = Debugger::InterruptOnExceptions )
    {
        Q_Q( Project );
        bool cancelled = false;
        JourneyRequest request = q->getJourneyRequest( parentWidget(), &cancelled );
        if ( !cancelled ) {
            callScriptFunction( &request, debugMode );
        }
    };

    // Called before testing starts
    bool beginTesting()
    {
        Q_Q( Project );
        if ( isTestRunning() ) {
            // Test is already running
            return true;
        }

        if ( !checkSyntax(q->scriptText()) ) {
            // Do not start the test if the syntax is invalid
            // TODO extra test for the syntax check?
            return false;
        }

        pendingTests.clear();
        testState = TestsRunning;
        updateProjectActions( QList<Project::ProjectActionGroup>() << Project::RunActionGroup
                                                                   << Project::TestActionGroup,
                              QList<Project::ProjectAction>() << Project::RunToCursor );
        q->emit informationMessage( i18nc("@info", "Test started") );
        q->emit testStarted();
        q->emit testRunningChanged( true );
        return true;
    };

    // Called after testing has ended
    void endTesting()
    {
        Q_Q( Project );
        const bool success = !testModel->hasErroneousTests();
        pendingTests.clear();
        testState = NoTestRunning;
        updateProjectActions( QList<Project::ProjectActionGroup>() << Project::RunActionGroup
                                                                   << Project::TestActionGroup,
                              QList<Project::ProjectAction>() << Project::RunToCursor );
        if ( success ) {
            q->emit informationMessage( i18nc("@info", "Test finished successfully"),
                    KMessageWidget::Positive, 4000,
                    QList<QAction*>() << q->projectAction(Project::ShowPlasmaPreview) );
        } else {
            q->emit informationMessage( i18nc("@info", "Test finished with errors"),
                                        KMessageWidget::Error, 4000 );
        }
        q->emit testFinished( success );
        q->emit testRunningChanged( false );
    };

    // Cancels all running tests
    void abortTests()
    {
        testState = TestsGetAborted;
        foreach ( ThreadWeaver::Job *testJob, pendingTests ) {
            if ( !debugger->weaver()->dequeue(testJob) ) {
                testJob->requestAbort();
            }
        }

        if ( !debugger->weaver()->isIdle() ) {
            while ( !pendingTests.isEmpty() ) {
                DebuggerJob *job = qobject_cast< DebuggerJob* >( pendingTests.takeFirst() );
                job->debugger()->engine()->abortEvaluation();
//                 deleteLater();
            }
        }
        endTesting();
    };

    bool testForSampleData()
    {
        Q_Q( Project );
        const TimetableAccessorInfo *info = accessor->info();
        if ( info->sampleStopNames().isEmpty() ) {
            testModel->markTestCaseAsUnstartable( TestModel::ScriptExecutionTestCase,
                    i18nc("@info/plain", "Missing sample stop name"),
                    i18nc("@info", "<title>Missing sample stop name</title> "
                        "<para>Cannot run script execution tests. Open the project settings and add "
                        "one or more <interface>Sample Stop Names</interface></para>"),
                    q->projectAction(Project::ShowProjectSettings) );
            return false;
        } else if ( info->useSeparateCityValue() && info->sampleCity().isEmpty() ) {
            testModel->markTestCaseAsUnstartable( TestModel::ScriptExecutionTestCase,
                    i18nc("@info/plain", "Missing sample city"),
                    i18nc("@info", "<title>Missing sample city</title> "
                        "<para>Cannot run script execution tests. Open the project settings and add "
                        "a <interface>Sample City</interface></para>"),
                    q->projectAction(Project::ShowProjectSettings) );
            return false;
        }

        return true;
    };

    bool testForJourneySampleData()
    {
        Q_Q( Project );
        const TimetableAccessorInfo *info = accessor->info();
        if ( info->sampleStopNames().count() < 2 ) {
            testModel->addTestResult( TestModel::JourneyTest, TestModel::TestCouldNotBeStarted,
                    i18nc("@info/plain", "To test journeys at least two sample stop names are needed"),
                    i18nc("@info", "<title>To test journeys at least two sample stop names are needed</title> "
                        "<para>Cannot run journey test. Open the project settings and add "
                        "another stop name to the <interface>Sample Stop Names</interface></para>"),
                    q->projectAction(Project::ShowProjectSettings) );
            return false;
        }

        return true;
    };

    bool startScriptExecutionTest( TestModel::Test test )
    {
        Q_Q( Project );

        // Test if enough sample data is available
        // and get the name of the script function to run
        QString function, message, shortMessage;
        switch ( test ) {
        case TestModel::DepartureTest:
        case TestModel::ArrivalTest:
            if ( !testForSampleData() ) {
                return false;
            }
            function = TimetableAccessorScript::SCRIPT_FUNCTION_GETTIMETABLE;
            shortMessage = i18nc("@info/plain", "You need to implement a '%1' script function", function);
            message = i18nc("@info", "<title>You need to implement a '%1' script function</title> "
                            "<para>Accessors that only support journeys are currently not accepted by "
                            "the data engine, but that may change.</para>", function);
            break;
        case TestModel::StopSuggestionTest:
            if ( !testForSampleData() ) {
                return false;
            }
            function = TimetableAccessorScript::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS;
            shortMessage = i18nc("@info/plain", "You need to implement a '%1' script function", function);
            message = i18nc("@info", "<title>You need to implement a '%1' script function</title> "
                            "<para>Without stop suggestions it can be very hard for users to find a "
                            "valid stop name. Therefore this function is needed.</para>", function);
            break;
        case TestModel::JourneyTest:
            if ( !testForJourneySampleData() ) {
                return false;
            }
            function = TimetableAccessorScript::SCRIPT_FUNCTION_GETJOURNEYS;
            shortMessage = i18nc("@info/plain", "For journeys, you need to implement a '%1' script function", function);
            message = i18nc("@info", "<title>For journeys, you need to implement a '%1' script function</title> "
                            "<para>If you do not implement the function, journeys will not work with "
                            "your accessor.</para>", function);
            break;
        case TestModel::UsedTimetableInformationsTest:
            function = TimetableAccessorScript::SCRIPT_FUNCTION_USEDTIMETABLEINFORMATIONS;
            shortMessage = i18nc("@info/plain", "You should implement a '%1' script function", function);
            message = i18nc("@info", "<title>You should implement a '%1' script function</title> "
                            "<para>This function is used to know what information the accessor parses "
                            "from documents. Without adding the appropriate TimetableInformation names "
                            "to the return value of this function, the associated data will be unused "
                            "or associated features will be disabled.</para>"
                            "<para>If, for example, the script can parse Arrivals, 'Arrivals' should "
                            "be added to the return value. If the script parses route stops or "
                            "stop IDs, add 'RouteStops' or 'StopID' to the return value, etc.</para>",
                            function);
            break;
        default:
            kWarning() << "Invalid test" << test;
            return false;
        }

        // Check if the function that should be run is implemented in the script
        const QStringList functions = q->scriptFunctions();
        const bool hasRequiredFunction = functions.contains( function );
        if ( !hasRequiredFunction ) {
            // Function is not implemented
            testModel->addTestResult( test, TestModel::TestCouldNotBeStarted, shortMessage,
                                      message, q->projectAction(Project::ShowScript) );
            return false;
        } else {
            // Function is implemented, ensure, that the current version of the script is loaded
            const TimetableAccessorInfo *info = accessor->info();
            debugger->loadScript( q->scriptText(), info );

            // Create job
            DebuggerJob *job;
            QString testName;
            if ( test == TestModel::UsedTimetableInformationsTest ) {
                job = debugger->createTestUsedTimetableInformationsJob( InterruptOnExceptions );
                testName = "TEST_USEDTIMETABLEINFORMATIONS";
            } else {
                // The number of items to request for testing, lower values mean higher performance,
                // higher values can mean better test results, eg. showing rare errors
                const int testItemCount = 30;

                // Create request object
                AbstractRequest *request = 0;
                switch ( test ) {
                case TestModel::DepartureTest:
                    request = new DepartureRequest( "TEST_DEPARTURES",
                            info->sampleStopNames().first(), QDateTime::currentDateTime(),
                            testItemCount, info->sampleCity() );
                    break;
                case TestModel::ArrivalTest:
                    request = new ArrivalRequest( "TEST_ARRIVALS",
                            info->sampleStopNames().first(), QDateTime::currentDateTime(),
                            testItemCount, info->sampleCity() );
                    break;
                case TestModel::StopSuggestionTest:
                    request = new StopSuggestionRequest( "TEST_STOP_SUGGESTIONS",
                            info->sampleStopNames().first(), testItemCount, info->sampleCity() );
                    break;
                case TestModel::JourneyTest:
                    request = new JourneyRequest( "TEST_JOURNEYS",
                            info->sampleStopNames().first(), info->sampleStopNames()[1],
                            QDateTime::currentDateTime(), testItemCount, QString(),
                            info->sampleCity() );
                    break;
                default:
                    kWarning() << "Invalid test" << test;
                    return false;
                }

                // Create job
                job = debugger->createTimetableDataRequestJob( request, InterruptOnExceptions );
                testName = request->sourceName;
                delete request;
            }

            // Connect job and try to enqueue it
            q->connect( job, SIGNAL(started(ThreadWeaver::Job*)),
                        q, SLOT(testJobStarted(ThreadWeaver::Job*)) );
            q->connect( job, SIGNAL(done(ThreadWeaver::Job*)),
                        q, SLOT(testJobDone(ThreadWeaver::Job*)) );
            if ( !debugger->enqueueJob(job, false) ) {
                // The job could not be enqueued
                delete job;
                testModel->markTestCaseAsUnstartable( TestModel::ScriptExecutionTestCase );
                endTesting();
                return false;
            } else {
                // The job was successfully enqueued
                pendingTests << job;
                return true;
            }
        }
    };

    bool save( QWidget *parent, const QString &_xmlFilePath, bool useAsNewSavePath = true )
    {
        Q_Q( Project );
        parent = parentWidget( parent );
        const QString filePath = _xmlFilePath.isEmpty() ? filePath : _xmlFilePath;
        if ( filePath.isEmpty() ) {
            return q->saveAs( parent );
        }

        // Save the project
        kDebug() << "Save to" << filePath;
        if ( !writeAccessorInfoXml(filePath) ) {
            return false;
        }

        QString scriptFile = accessor->info()->scriptFileName();
        if ( !scriptFile.isEmpty() ) {
            const QString scriptFilePath =
                    QFileInfo(filePath).absolutePath() + '/' + QFileInfo(scriptFile).fileName();
            QFile file( scriptFilePath );
            if ( !file.open(QIODevice::WriteOnly) ) {
                q->emit informationMessage( i18nc("@info", "Could not write the script file to "
                                                  "<filename>%1</filename>: <message>%2</message>",
                                                  scriptFilePath, file.errorString() ),
                                            KMessageWidget::Error );
//                 KMessageBox::error( parent, i18nc("@info", "Could not write the script file to "
//                                                   "<filename>%1</filename>.", scriptFilePath) );
                return false;
            }

            file.write( q->scriptText().toUtf8() );
            file.close();
        }

        if ( useAsNewSavePath ) {
            const bool wasModified = isModified();
            const bool wasScriptModified = isScriptModified();
            const bool wasProjectSourceModified = isProjectSourceModified();

            projectSourceBufferModified = false;
            unsavedScriptContents.clear();
            updateProjectActions( QList<Project::ProjectAction>() << Project::Save );
            setXmlFilePath( filePath );

            if ( projectSourceTab ) {
                projectSourceTab->document()->setModified( false );
            }
            if ( scriptTab ) {
                scriptTab->document()->setModified( false );
            }
            if ( wasModified ) {
                q->emit modifiedStateChanged( false );
                if ( wasScriptModified ) {
                    q->emit scriptModifiedStateChanged( false );
                }
                if ( wasProjectSourceModified ) {
                    q->emit projectSourceModifiedStateChanged( false );
                }
            }
        }
        return true;
    };

    bool saveAs( QWidget *parent )
    {
        parent = parentWidget( parent );
        QString fileName = KFileDialog::getSaveFileName(
                openedPath.isEmpty() ? KGlobalSettings::documentPath() : openedPath,
                "application/x-publictransport-serviceprovider application/xml",
                parent, i18nc("@title:window", "Save Project") );
        //  "*.cpp *.cc *.C|C++ Source Files\n*.h *.H|Header files");
        if ( fileName.isEmpty() ) {
            return false; // Cancel clicked
        }

        // Got a file name, save the project
        return save( parent, fileName );
    };

    bool install( QWidget *parent, bool install, Project::InstallType installType )
    {
        Q_Q( Project );
        const QString xmlFileName = accessor->info()->serviceProvider() + ".xml";
        if ( installType == Project::LocalInstallation ) {
            // Local installation, find a writable location for Public Transport engine accessors/plugins
            const QString saveDir = KGlobal::dirs()->saveLocation( "data",
                    "plasma_engine_publictransport/accessorInfos/" );
            const QString savePath = saveDir + '/' + xmlFileName;

            if ( install ) {
                // Install by saving into the found writable location
                if ( save(parent, savePath, false) ) {
                    // Installation successful
                    q->emit informationMessage( i18nc("@info", "Project successfully installed locally"),
                                                KMessageWidget::Positive );
                    q->emit savePathInfoStringChanged( savePathInfoString() );
                    q->emit localInstallationStateChanged( true );
                } else {
                    // Could not install
                    q->emit informationMessage( i18nc("@info", "Project could not be installed locally "
                                                      "as <filename>%1</filename>", savePath),
                                                KMessageWidget::Error, 10000 );
                    return false;
                }
            } else if ( QFile::exists(savePath) ) {
                // Uninstall by deleting the project files from the found writable location
                const QString scriptSavePath = saveDir + '/' + QFileInfo(q->scriptFileName()).fileName();
                if ( QFile::exists(scriptSavePath) && !QFile::remove(scriptSavePath) ) {
                    // Could not uninstall script file
                    q->emit informationMessage( i18nc("@info", "Project could not be uninstalled locally, "
                                                      "file <filename>%1</filename>", scriptSavePath),
                                                KMessageWidget::Error, 10000 );
                    return false;
                } else if ( !QFile::remove(savePath) ) {
                    // Could not uninstall project XML file
                    q->emit informationMessage( i18nc("@info", "Project could not be uninstalled locally, "
                                                      "file <filename>%1</filename>", saveDir),
                                                KMessageWidget::Error, 10000 );
                    return false;
                } else {
                    // Uninstallation successful
                    q->emit informationMessage( i18nc("@info", "Project successfully uninstalled locally"),
                                                KMessageWidget::Positive );
                    if ( filePath == savePath ) {
                        // The project was opened from a local install path, which was just deleted
                        setXmlFilePath( QString() );
                    } else {
                        q->emit savePathInfoStringChanged( savePathInfoString() );
                    }
                    q->emit localInstallationStateChanged( false );
                }
            } else {
                q->emit informationMessage( i18nc("@info", "Project is not installed locally"),
                                            KMessageWidget::Information );
            }
        } else if ( installType == Project::GlobalInstallation ) {
            // Global insallation, find all directories for Public Transport engine accessors/plugins
            const QStringList saveDirs = KGlobal::dirs()->findDirs( "data",
                    "plasma_engine_publictransport/accessorInfos/" );
            if ( saveDirs.isEmpty() ) {
                kDebug() << "No save directory found. Is the PublicTransport data engine installed?";
                return false;
            }

            // Use the most global directory (see KStandardDirs::findDirs()
            const QString saveDir = saveDirs.last();
            const QString savePath = saveDir + '/' + xmlFileName;

            // Use KAuth for installation
            KAuth::Action action( "org.kde.timetablemate.install" );
            action.setHelperID( "org.kde.timetablemate" );
            QVariantMap args;
            args["path"] = saveDir;
            args["operation"] = install ? "install" : "uninstall";
            args["filenameAccessor"] = xmlFileName;
            args["filenameScript"] = accessor->info()->scriptFileName();
            if ( install ) {
                args["contentsAccessor"] = q->accessorText();
                args["contentsScript"] = q->scriptText();
            }
            action.setArguments( args );
            KAuth::ActionReply reply = action.execute();

            // Check if the installation was successful
            if ( reply.failed() ) {
                kDebug() << reply.type() << reply.data();
                kDebug() << reply.errorCode() << reply.errorDescription();
                if ( reply.type() == KAuth::ActionReply::HelperError ) {
                    KMessageBox::error( parent,
                            install ? i18nc("@info", "Accessor could not be installed globally "
                                            "in <filename>%1</filename>: %2 <message>%3</message>",
                                            saveDir, reply.errorCode(), reply.errorDescription())
                                    : i18nc("@info", "Accessor could not be uninstalled globally "
                                            "from <filename>%1</filename>: %2 <message>%3</message>",
                                            saveDir, reply.errorCode(), reply.errorDescription()) );
                } else {
                    switch ( reply.errorCode() ) {
                    case KAuth::ActionReply::UserCancelled:
                    case KAuth::ActionReply::AuthorizationDenied:
                        // Do nothing
                        break;

                    case KAuth::ActionReply::NoSuchAction:
                        KMessageBox::error( parent, i18nc("@info", "Could not find the authentication "
                                "action. If you just installed TimetableMate, you might need to "
                                "restart D-Bus.") );
                        break;

                    case KAuth::ActionReply::HelperBusy:
                        KMessageBox::error( parent, i18nc("@info", "The action is currently being "
                                "performed. Please try again later.") );
                        break;

                    default:
                        KMessageBox::error( parent, i18nc("@info", "Unable to authenticate the action: "
                                "%1 <message>%2</message>", reply.errorCode(), reply.errorDescription()) );
                        break;
                    }
                }
                return false;
            } else if ( install ) {
                // Installation successful
                q->emit informationMessage( i18nc("@info", "Accessor successfully installed globally"),
                                            KMessageWidget::Positive );
                q->emit savePathInfoStringChanged( savePathInfoString() );
                q->emit globalInstallationStateChanged( true );
            } else {
                // Uninstallation successful
                q->emit informationMessage( i18nc("@info", "Accessor successfully uninstalled globally"),
                                            KMessageWidget::Positive );
                if ( xmlFileName == savePath ) {
                    // The project was opened from a global install path, which was just deleted
                    setXmlFilePath( QString() );
                } else {
                    q->emit savePathInfoStringChanged( savePathInfoString() );
                }
                q->emit globalInstallationStateChanged( false );
            }
        } else {
            kDebug() << "Install type not implemented:" << installType;
            return false;
        }

        return true;
    };

    Project::State state;
    ProjectModel *projectModel;

    // This is needed to know when accessor was updated with new settings using setAccessorInfo()
    // but no ProjectSourceTab is opened
    bool projectSourceBufferModified;

    QString openedPath;
    QString filePath;
    QString serviceProviderID;

    DashboardTab *dashboardTab;
    ProjectSourceTab *projectSourceTab;
    ScriptTab *scriptTab;
    PlasmaPreviewTab *plasmaPreviewTab;
    WebTab *webTab;

    TimetableAccessor *accessor;
    QString unsavedScriptContents;
    Debugger::Debugger *debugger;
    int executionLine;

    // Get created when needed, multi for actions of the same type with different data
    QMultiHash< Project::ProjectAction, QAction* > projectActions;

    // Store pointers to project actions, to update their enabled state on changes
    QMultiHash< Project::ProjectAction, QAction* > externProjectActions;

    TestModel *testModel;
    TestState testState;
    QList< ThreadWeaver::Job* > pendingTests;

private:
    Project *q_ptr;
    Q_DECLARE_PUBLIC( Project )
};

Project::Project( const QString &xmlFilePath, QWidget *parent )
        : QObject( parent ), d_ptr(new ProjectPrivate(this))
{
    qRegisterMetaType< ProjectActionData >( "ProjectActionData" );
    d_ptr->initialize();
    d_ptr->loadProject( xmlFilePath );
}

Project::~Project()
{
    if ( isModified() ) {
        kWarning() << "Destroying project with modifications";
    }

    delete d_ptr;
}

Project::State Project::state() const
{
    Q_D( const Project );
    return d->state;
}

ProjectModel *Project::projectModel() const
{
    Q_D( const Project );
    return d->projectModel;
}

DashboardTab *Project::dashboardTab() const
{
    Q_D( const Project );
    return d->dashboardTab;
}

ProjectSourceTab *Project::projectSourceTab() const
{
    Q_D( const Project );
    return d->projectSourceTab;
}

ScriptTab *Project::scriptTab() const
{
    Q_D( const Project );
    return d->scriptTab;
}

PlasmaPreviewTab *Project::plasmaPreviewTab() const
{
    Q_D( const Project );
    return d->plasmaPreviewTab;
}

WebTab *Project::webTab() const
{
    Q_D( const Project );
    return d->webTab;
}

Debugger::Debugger *Project::debugger() const
{
    Q_D( const Project );
    return d->debugger;
}

QString Project::filePath() const
{
    Q_D( const Project );
    return d->filePath;
}

QString Project::serviceProviderId() const
{
    Q_D( const Project );
    return d->serviceProviderID;
}

TestModel *Project::testModel() const
{
    Q_D( const Project );
    return d->testModel;
}

Project::InstallType Project::saveType() const
{
    Q_D( const Project );
    return d->saveType();
}

Project::InstallTypes Project::installationTypes() const
{
    Q_D( const Project );
    return d->installationTypes();
}

QString Project::savePathInfoString() const
{
    Q_D( const Project );
    return d->savePathInfoString();
}

void Project::setProjectModel( ProjectModel *projectModel )
{
    Q_D( Project );
    d->projectModel = projectModel;
}

const char *Project::projectActionName( Project::ProjectAction actionType )
{
    switch ( actionType ) {
    case Save:
        return "project_save";
    case SaveAs:
        return "project_save_as";
    case Install:
        return "project_install";
    case Uninstall:
        return "project_uninstall";
    case InstallGlobally:
        return "project_install_global";
    case UninstallGlobally:
        return "project_uninstall_global";
    case Close:
        return "project_close";
    case ShowProjectSettings:
        return "project_settings";
    case ShowDashboard:
        return "project_show_dashboard";
    case ShowHomepage:
        return "project_show_homepage";
    case ShowScript:
        return "project_show_script";
    case ShowProjectSource:
        return "project_show_source";
    case ShowPlasmaPreview:
        return "view_plasma_preview_show";
    case RunAllTests:
        return "test_all";
    case AbortRunningTests:
        return "test_abort";
    case ClearTestResults:
        return "test_clear";
    case RunSpecificTest:
        return "test_specific_test";
    case RunSpecificTestCase:
        return "test_specific_testcase";
    case SpecificTestCaseMenuAction:
        return "test_specific_testcase_menu";
    case SetAsActiveProject:
        return "project_set_active";
    case StepInto:
        return "debug_step_into";
    case StepOver:
        return "debug_step_over";
    case StepOut:
        return "debug_step_out";
    case Interrupt:
        return "debug_interrupt";
    case RunToCursor:
        return "debug_run_to_cursor";
    case Continue:
        return "debug_continue";
    case AbortDebugger:
        return "debug_abort";
    case ToggleBreakpoint:
        return "debug_toggle_breakpoint";
    case RemoveAllBreakpoints:
        return "debug_remove_all_breakpoints";

    case RunMenuAction:
        return "run_menu_action";
    case RunGetTimetable:
        return "run_departures";
    case RunGetStopSuggestions:
        return "run_stop_suggestions";
    case RunGetJourneys:
        return "run_journeys";

    case DebugMenuAction:
        return "debug_menu_action";
    case DebugGetTimetable:
        return "debug_departures";
    case DebugGetStopSuggestions:
        return "debug_stop_suggestions";
    case DebugGetJourneys:
        return "debug_journeys";

    default:
        kWarning() << "Unknown project action" << actionType;
        return "";
    }
}

Project::ProjectActionData Project::projectActionData( QAction *action )
{
    return action->data().value< ProjectActionData >();
}

void Project::showProjectContextMenu( const QPoint &globalPos )
{
    Q_D( Project );
    // Show context menu for this tab
    QScopedPointer<QMenu> contextMenu( new QMenu(d->parentWidget()) );
    contextMenu->addActions( contextMenuActions(contextMenu.data()) );
    contextMenu->exec( globalPos );
}

QList< QAction* > Project::contextMenuActions( QWidget *parent )
{
    Q_D( Project );
    parent = d->parentWidget( parent );

    // Create a list of actions that should be used for context menus for the project
    QList< QAction* > actions;
    KAction *separator1 = new KAction( parent );
    separator1->setSeparator( true );
    KAction *separator2 = new KAction( parent );
    separator2->setSeparator( true );
    actions << projectAction(Save) << projectAction(SaveAs)
            << projectAction(Install) << projectAction(InstallGlobally)
            << projectAction(Uninstall) << projectAction(UninstallGlobally)
            << separator1
            << projectAction(SetAsActiveProject)
            << projectAction(ShowDashboard)
            << debuggerSubMenuAction( parent )
            << testSubMenuAction( parent )
            << separator2
            << projectAction(ShowProjectSettings)
            << projectAction(Close);
    return actions;
}

QPointer< KActionMenu > Project::debuggerSubMenuAction( QWidget *parent )
{
    Q_D( Project );
    parent = d->parentWidget( parent );

    // Create a KActionMenu containing debug actions
    QPointer<KActionMenu> debuggerMenuAction( new KActionMenu(KIcon("debugger"),
                                              i18nc("@action", "Run"), parent) );
    debuggerMenuAction->addAction( projectAction(RunMenuAction)  );
    debuggerMenuAction->addAction( projectAction(DebugMenuAction)  );
    debuggerMenuAction->addAction( projectAction(RunToCursor)  );
    debuggerMenuAction->addAction( projectAction(Continue)  );
    debuggerMenuAction->addAction( projectAction(Interrupt)  );
    debuggerMenuAction->addAction( projectAction(AbortDebugger)  );
    debuggerMenuAction->addSeparator();
    debuggerMenuAction->addAction( projectAction(StepOver)  );
    debuggerMenuAction->addAction( projectAction(StepInto)  );
    debuggerMenuAction->addAction( projectAction(StepOut)  );
    return debuggerMenuAction;
}

QPointer< KActionMenu > Project::testSubMenuAction( QWidget *parent )
{
    Q_D( Project );
    parent = d->parentWidget( parent );

    // Create a KActionMenu containing test actions
    QPointer<KActionMenu> testMenuAction( new KActionMenu(KIcon("task-complete"),
                                          i18nc("@action", "Test"), parent) );
    testMenuAction->addAction( projectAction(RunAllTests) );
    testMenuAction->addAction( projectAction(AbortRunningTests) );
    testMenuAction->addAction( projectAction(ClearTestResults) );
    testMenuAction->addSeparator();

    // Fill test action list
    for ( int i = 0; i < TestModel::TestCaseCount; ++i ) {
        const TestModel::TestCase testCase = static_cast<TestModel::TestCase>( i );
        testMenuAction->addAction( projectAction(SpecificTestCaseMenuAction,
                                                 QVariant::fromValue(static_cast<int>(testCase))) );
    }
    return testMenuAction;
}

QPointer< KActionMenu > Project::projectSubMenuAction( QWidget *parent )
{
    Q_D( Project );
    parent = d->parentWidget( parent );

    // Create a KActionMenu containing all context menu actions for the project
    QPointer<KActionMenu> projectMenuAction( new KActionMenu(KIcon("project-development"),
                                       i18nc("@action", "Project"), parent) );
    QList< QAction* > projectActions = contextMenuActions( parent );
    foreach ( QAction *projectAction, projectActions ) {
        projectMenuAction->addAction( projectAction );
    }
    return projectMenuAction;
}

void Project::testActionTriggered()
{
    QAction *action = qobject_cast< QAction* >( sender() );
    const ProjectActionData data = action->data().value< ProjectActionData >();
    startTest( static_cast<TestModel::Test>(data.data.toInt()) );
}

void Project::testCaseActionTriggered()
{
    QAction *action = qobject_cast< QAction* >( sender() );
    const ProjectActionData data = action->data().value< ProjectActionData >();
    startTestCase( static_cast<TestModel::TestCase>(data.data.toInt()) );
}

bool Project::isActiveProject() const
{
    Q_D( const Project );
    return d->isActiveProject();
}

void Project::slotActiveProjectChanged( Project *project, Project *previousProject )
{
    Q_D( Project );
    if ( project == this ) {
        emit activeProjectStateChanged( true );

        d->updateProjectActions( QList<ProjectActionGroup>() << RunActionGroup << TestActionGroup
                                 << DebuggerActionGroup << FileActionGroup << OtherActionGroup );
    } else if ( previousProject == this ) {
        emit activeProjectStateChanged( false );
    }
}

QAction *Project::projectAction( Project::ProjectAction actionType, const QVariant &data )
{
    Q_D( Project );
    QAction *action = 0;
    if ( d->projectActions.contains(actionType) ) {
        // Find action in d->projectActions
        QList< QAction* > actions = d->projectActions.values( actionType );
        foreach ( QAction *currentAction, actions ) {
            if ( !currentAction->data().isValid() && !data.isValid() ) {
                // No data wanted and an action without data was found
                action = currentAction;
                break;
            } else if ( currentAction->data() == data ) {
                // An action with the given data was found
                action = currentAction;
                break;
            }
        }
    }

    if ( !action ) {
        // Create and connect action and store it in d->projectActions
        action = createProjectAction( actionType, data, this );
        connectProjectAction( actionType, action );
        d->projectActions.insert( actionType, action );
    }
    return action;
}

void Project::connectProjectAction( Project::ProjectAction actionType, QAction *action,
                                    bool doConnect )
{
    Q_D( Project );
    switch ( actionType ) {
    case Save:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(save()) );
        if ( doConnect ) {
            connect( this, SIGNAL(modifiedStateChanged(bool)), action, SLOT(setEnabled(bool)) );
        } else {
            disconnect( this, SIGNAL(modifiedStateChanged(bool)), action, SLOT(setEnabled(bool)) );
        }
        break;
    case SaveAs:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(saveAs()) );
        break;
    case Install:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(installLocally()) );
        break;
    case Uninstall:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(uninstallLocally()) );
        break;
    case InstallGlobally:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(installGlobally()) );
        break;
    case UninstallGlobally:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(uninstallGlobally()) );
        break;
    case Close:
        d->connectProjectAction( actionType, action, doConnect, this, SIGNAL(closeRequest()) );
        break;
    case ShowProjectSettings:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(showSettingsDialog()) );
        break;
    case ShowDashboard:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(showDashboardTab()) );
        break;
    case ShowHomepage:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(showWebTab()) );
        break;
    case ShowScript:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(showScriptTab()) );
        break;
    case ShowProjectSource:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(showProjectSourceTab()) );
        break;
    case ShowPlasmaPreview:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(showPlasmaPreviewTab()) );
        break;

    case RunAllTests:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(testProject()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;
    case AbortRunningTests:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(abortTests()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;
    case ClearTestResults:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(clearTestResults()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;
    case RunSpecificTest:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(testActionTriggered()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;
    case RunSpecificTestCase:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(testCaseActionTriggered()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;

    case SetAsActiveProject:
        d->connectProjectAction( actionType, action, doConnect,
                                 this, SIGNAL(setAsActiveProjectRequest()) );
        if ( doConnect ) {
            connect( this, SIGNAL(activeProjectStateChanged(bool)), action, SLOT(setDisabled(bool)) );
            connect( this, SIGNAL(activeProjectStateChanged(bool)), action, SLOT(setChecked(bool)) );
            action->setChecked( isActiveProject() );
        } else {
            disconnect( this, SIGNAL(activeProjectStateChanged(bool)), action, SLOT(setDisabled(bool)) );
            disconnect( this, SIGNAL(activeProjectStateChanged(bool)), action, SLOT(setChecked(bool)) );
        }
        break;
    case StepInto:
        d->connectProjectAction( actionType, action, doConnect, d->debugger, SLOT(debugStepInto()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;
    case StepOver:
        d->connectProjectAction( actionType, action, doConnect, d->debugger, SLOT(debugStepOver()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;
    case StepOut:
        d->connectProjectAction( actionType, action, doConnect, d->debugger, SLOT(debugStepOut()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;
    case Interrupt:
        d->connectProjectAction( actionType, action, doConnect, d->debugger, SLOT(debugInterrupt()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;
    case Continue:
        d->connectProjectAction( actionType, action, doConnect, d->debugger, SLOT(debugContinue()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;
    case AbortDebugger:
        d->connectProjectAction( actionType, action, doConnect, d->debugger, SLOT(abortDebugger()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;
    case ToggleBreakpoint:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(toggleBreakpoint()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;
    case RemoveAllBreakpoints:
        d->connectProjectAction( actionType, action, doConnect,
                                 d->debugger, SLOT(removeAllBreakpoints()) );
        if ( doConnect ) {
            connect( d->debugger->breakpointModel(), SIGNAL(emptinessChanged(bool)),
                     action, SLOT(setDisabled(bool)) );
        } else {
            disconnect( d->debugger->breakpointModel(), SIGNAL(emptinessChanged(bool)),
                        action, SLOT(setDisabled(bool)) );
        }
        break;
    case RunToCursor:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(runToCursor()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;

    case SpecificTestCaseMenuAction:
    case RunMenuAction:
    case DebugMenuAction: {
        // Nothing to do for this action, it opens a menu with other actions
        // Connect these other actions instead
        KActionMenu *menuAction = qobject_cast< KActionMenu* >( action );
        Q_ASSERT( menuAction );
        QList<QAction*> actions = menuAction->menu()->actions();
        foreach ( QAction *action, actions ) {
            if ( action->isSeparator() ) {
                continue;
            }
            const ProjectActionData data = action->data().value< ProjectActionData >();
            connectProjectAction( data.actionType, action, doConnect );
        }
        d->connectProjectAction( actionType, action, doConnect, 0, "", ProjectPrivate::AutoUpdateEnabledState );
    } break;

    case RunGetTimetable:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(runGetTimetable()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;
    case RunGetStopSuggestions:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(runGetStopSuggestions()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;
    case RunGetJourneys:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(runGetJourneys()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;

    case DebugGetTimetable:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(debugGetTimetable()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;
    case DebugGetStopSuggestions:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(debugGetStopSuggestions()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;
    case DebugGetJourneys:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(debugGetJourneys()),
                                 ProjectPrivate::AutoUpdateEnabledState );
        break;

    default:
        kWarning() << "Unknown project action" << actionType;
        return;
    }
}

QAction *Project::createProjectAction( Project::ProjectAction actionType, const QVariant &data,
                                       QObject *parent )
{
    KAction *action = 0;
    switch ( actionType ) {
    case Save:
        action = new KAction( KIcon("document-save"),
                              i18nc("@item:inmenu", "Save Project"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Save changes in the project") );
        action->setEnabled( false );
        break;
    case SaveAs:
        action = new KAction( KIcon("document-save-as"),
                              i18nc("@action", "Save Project As..."), parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Save changes in the project under a new file name") );
        break;
    case Install:
        action = new KAction( KIcon("run-build-install"), i18nc("@action", "&Install"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Install the project locally") );
        break;
    case Uninstall:
        action = new KAction( KIcon("edit-delete"), i18nc("@action", "&Uninstall"), parent );
        action->setToolTip( i18nc("@info:tooltip",
                                  "Uninstall a locally installed version of the project") );
        break;
    case InstallGlobally:
        action = new KAction( KIcon("run-build-install-root"),
                              i18nc("@action", "Install &Globally"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Install the project globally") );
        break;
    case UninstallGlobally:
        action = new KAction( KIcon("edit-delete"),
                              i18nc("@action", "Uninstall &Globally"), parent );
        action->setToolTip( i18nc("@info:tooltip",
                                  "Uninstall a globally installed version of the project") );
        break;
    case Close:
        action = new KAction( KIcon("project-development-close"),
                              i18nc("@action", "Close Project"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Close this project") );
        break;
    case ShowProjectSettings:
        action = new KAction( KIcon("configure"), i18nc("@action", "Project Settings..."), parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Opens a dialog to modify the projects settings") );
        break;
    case ShowDashboard:
        action = new KAction( KIcon("dashboard-show"), i18nc("@action", "Show &Dashboard"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Shows the dashboard tab of the project.") );
        break;
    case ShowHomepage:
        action = new KAction( KIcon("document-open-remote"),
                              i18nc("@action", "Show &Web Page"), parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Opens the <emphasis>home page</emphasis> of the service provider in a tab.") );
        break;
    case ShowScript:
        action = new KAction( KIcon("application-javascript"),
                              i18nc("@action", "Show &Script"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Opens the <emphasis>script</emphasis> in a tab.") );
        break;
    case ShowProjectSource:
        action = new KAction( KIcon("application-x-publictransport-serviceprovider"),
                              i18nc("@action", "Show Project &Source"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Opens the <emphasis>project source</emphasis> "
                                  "document in a tab.") );
        break;
    case ShowPlasmaPreview:
        action = new KAction( KIcon("plasma"), i18nc("@action", "Show &Plasma Preview"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Opens the project in a PublicTransport applet "
                                  "in a <emphasis>Plasma preview</emphasis> tab.") );
        break;

    case RunAllTests:
        action = new KAction( KIcon("task-complete"), i18nc("@action", "&Run All Tests"), parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Runs all tests for the active project, eg. syntax errors, correct results.") );
        break;
    case AbortRunningTests:
        action = new KAction( KIcon("dialog-cancel"), i18nc("@action", "&Abort Running Tests"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Aborts all currently running tests.") );
        break;
    case ClearTestResults:
        action = new KAction( KIcon("edit-clear"), i18nc("@action", "&Clear All Test Results"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Clears all results of a previous test run.") );
        break;
    case RunSpecificTest: {
        TestModel::Test test = static_cast< TestModel::Test >( data.toInt() );
        if ( test == TestModel::InvalidTest ) {
            kWarning() << "No test specified for project action RunSpecificTest";
            return 0;
        }
        action = new KAction( KIcon("arrow-right"), i18nc("@action:inmenu", "Run %1",
                                                          TestModel::nameForTest(test)), parent );
        action->setToolTip( TestModel::descriptionForTest(test) );
    } break;
    case RunSpecificTestCase: {
        TestModel::TestCase testCase = static_cast< TestModel::TestCase >( data.toInt() );
        if ( testCase == TestModel::InvalidTestCase ) {
            kWarning() << "No test case specified for project action RunSpecificTestCase";
            return 0;
        }
        action = new KAction( KIcon("arrow-right-double"),
                              i18nc("@action:inmenu", "&Run Complete Test Case"), parent );
        action->setToolTip( TestModel::descriptionForTestCase(testCase) );
    } break;
    case SpecificTestCaseMenuAction: {
        TestModel::TestCase testCase = static_cast< TestModel::TestCase >( data.toInt() );
        if ( testCase == TestModel::InvalidTestCase ) {
            kWarning() << "No test case specified for project action SpecificTestCaseMenuAction";
            return 0;
        }

        // Create menu action
        KActionMenu *runTestCase = new KActionMenu( TestModel::nameForTestCase(testCase), parent );
        runTestCase->setToolTip( TestModel::descriptionForTestCase(testCase) );
        runTestCase->setDelayed( false );

        // Add RunSpecificTestCase action, a separator
        // and RunSpeficTest actions for each test of the test case
        runTestCase->addAction( createProjectAction(RunSpecificTestCase, testCase, parent) );
        runTestCase->addSeparator();
        const QList< TestModel::Test > tests = TestModel::testsOfTestCase( testCase );
        foreach ( TestModel::Test test, tests ) {
            runTestCase->addAction( createProjectAction(RunSpecificTest, test, parent) );
        }
        action = runTestCase;
    } break;

    case SetAsActiveProject:
        action = new KAction( KIcon("edit-select"),
                              i18nc("@action", "Set as Active Project"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Use this project as the active project") );
        action->setCheckable( true );
        action->setEnabled( false );
        break;
    case StepInto:
        action = new KAction( KIcon("debug-step-into"), i18nc("@action", "Step &Into"), parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Continue script execution until the next statement") );
        action->setEnabled( false );
        action->setPriority( QAction::LowPriority );
        break;
    case StepOver:
        action = new KAction( KIcon("debug-step-over"), i18nc("@action", "Step &Over"), parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Continue script execution until the next statement in the same context.") );
        action->setEnabled( false );
        action->setPriority( QAction::LowPriority );
        break;
    case StepOut:
        action = new KAction( KIcon("debug-step-out"), i18nc("@action", "Step Ou&t"), parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Continue script execution until the current function gets left.") );
        action->setEnabled( false );
        action->setPriority( QAction::LowPriority );
        break;
    case Interrupt:
        action = new KAction( KIcon("media-playback-pause"),
                              i18nc("@action", "&Interrupt"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Interrupt script execution.") );
        action->setEnabled( false );
        action->setEnabled( false );
        action->setPriority( QAction::LowPriority );
        break;
    case RunToCursor:
        action = new KAction( KIcon("debug-execute-to-cursor"),
                              i18nc("@action", "Run to &Cursor"), parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Continue script execution until the current cursor position is reached") );
        action->setEnabled( false );
        action->setPriority( QAction::LowPriority );
        break;
    case Continue:
        action = new KAction( KIcon("media-playback-start"),
                              i18nc("@action", "&Continue"), parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Continue script execution, only interrupt on breakpoints or uncaught exceptions.") );
        action->setEnabled( false );
        action->setPriority( QAction::LowPriority );
        break;
    case AbortDebugger:
        action = new KAction( KIcon("process-stop"), i18nc("@action", "&Abort Debugger"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Abort script execution") );
        action->setEnabled( false );
        action->setPriority( QAction::LowPriority );
        break;
    case ToggleBreakpoint:
        action = new KAction( KIcon("tools-report-bug"),
                i18nc("@action", "Toggle &Breakpoint"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Toggle breakpoint for the current line") );
        action->setEnabled( false );
        break;
    case RemoveAllBreakpoints:
        action = new KAction( KIcon("tools-report-bug"),
                i18nc("@action", "&Remove all Breakpoints"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Removes all breakpoints") );
        action->setEnabled( false );
        break;

    case RunMenuAction: {
        KActionMenu *debugScript = new KActionMenu( KIcon("system-run"),
                i18nc("@action", "&Run"), parent );
        debugScript->setToolTip( i18nc("@info:tooltip", "Runs a function of the script.") );
        debugScript->setDelayed( false );
        debugScript->addAction( createProjectAction(Project::RunGetTimetable, parent) );
        debugScript->addAction( createProjectAction(Project::RunGetStopSuggestions, parent) );
        debugScript->addAction( createProjectAction(Project::RunGetJourneys, parent) );
        action = debugScript;
    } break;
    case RunGetTimetable:
        action = new KAction( KIcon("system-run"), i18nc("@action", "Run get&Timetable()"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Runs the script function 'getTimetable()'") );
        action->setEnabled( false );
        break;
    case RunGetStopSuggestions:
        action = new KAction( KIcon("system-run"), i18nc("@action", "Run get&StopSuggestions()"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Runs the script function 'getStopSuggestions()'") );
        action->setEnabled( false );
        break;
    case RunGetJourneys:
        action = new KAction( KIcon("system-run"), i18nc("@action", "Run get&Journeys()"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Runs the script function 'getJourneys()'") );
        action->setEnabled( false );
        break;

    case DebugMenuAction: {
        KActionMenu *debugScript = new KActionMenu( KIcon("debug-run"),
                i18nc("@action", "&Debug"), parent );
        debugScript->setToolTip( i18nc("@info:tooltip", "Runs a function of the script in a debugger.") );
        debugScript->setDelayed( false );
        debugScript->addAction( createProjectAction(Project::DebugGetTimetable, parent) );
        debugScript->addAction( createProjectAction(Project::DebugGetStopSuggestions, parent) );
        debugScript->addAction( createProjectAction(Project::DebugGetJourneys, parent) );
        action = debugScript;
    } break;
    case DebugGetTimetable:
        action = new KAction( KIcon("debug-run"), i18nc("@action", "Debug get&Timetable()"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Runs the script function 'getTimetable()' "
                                                   "in a debugger") );
        action->setEnabled( false );
        break;
    case DebugGetStopSuggestions:
        action = new KAction( KIcon("debug-run"), i18nc("@action", "Debug get&StopSuggestions()"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Runs the script function 'getStopSuggestions()' "
                                                   "in a debugger") );
        action->setEnabled( false );
        break;
    case DebugGetJourneys:
        action = new KAction( KIcon("debug-run"), i18nc("@action", "Debug get&Journeys()"), parent );
        action->setToolTip( i18nc("@info:tooltip", "Runs the script function 'getJourneys()' "
                                                   "in a debugger") );
        action->setEnabled( false );
        break;

    default:
        kDebug() << "Unknown project action" << actionType;
        break;
    }

    // Store action type
    action->setData( QVariant::fromValue(ProjectActionData(actionType, data)) );
    return action;
}

void Project::showScriptLineNumber( int lineNumber )
{
    Q_D( Project );
    if ( lineNumber < 0 ) {
        return;
    }

    if ( !d->scriptTab ) {
        showScriptTab();
    }
    d->scriptTab->document()->views().first()->setCursorPosition(
            KTextEditor::Cursor(lineNumber - 1, 0) );
}

DashboardTab *Project::showDashboardTab( QWidget *parent )
{
    Q_D( Project );
    if ( d->dashboardTab ) {
        emit tabGoToRequest( d->dashboardTab );
    } else {
        d->dashboardTab = createDashboardTab( d->parentWidget(parent) );
        if ( d->dashboardTab ) {
            emit tabOpenRequest( d->dashboardTab );
        }
    }
    return d->dashboardTab;
}

ScriptTab *Project::showScriptTab( QWidget *parent )
{
    Q_D( Project );
    if ( d->scriptTab ) {
        emit tabGoToRequest( d->scriptTab );
    } else {
        d->scriptTab = createScriptTab( d->parentWidget(parent) );
        if ( d->scriptTab ) {
            emit tabOpenRequest( d->scriptTab );
        }
    }
    return d->scriptTab;
}

ProjectSourceTab *Project::showProjectSourceTab( QWidget *parent )
{
    Q_D( Project );
    if ( d->projectSourceTab ) {
        emit tabGoToRequest( d->projectSourceTab );
    } else {
        d->projectSourceTab = createProjectSourceTab( d->parentWidget(parent) );
        if ( d->projectSourceTab ) {
            emit tabOpenRequest( d->projectSourceTab );
        }
    }
    return d->projectSourceTab;
}

PlasmaPreviewTab *Project::showPlasmaPreviewTab( QWidget *parent )
{
    Q_D( Project );
    if ( d->plasmaPreviewTab ) {
        emit tabGoToRequest( d->plasmaPreviewTab );
    } else {
        d->plasmaPreviewTab = createPlasmaPreviewTab( d->parentWidget(parent) );
        if ( d->plasmaPreviewTab ) {
            emit tabOpenRequest( d->plasmaPreviewTab );
        }
    }
    return d->plasmaPreviewTab;
}

WebTab *Project::showWebTab( QWidget *parent )
{
    Q_D( Project );
    if ( d->webTab ) {
        emit tabGoToRequest( d->webTab );
    } else {
        d->webTab = createWebTab( d->parentWidget(parent) );
        if ( d->webTab ) {
            emit tabOpenRequest( d->webTab );
        }
    }
    return d->webTab;
}

Project::ProjectActionGroup Project::actionGroupFromType( Project::ProjectAction actionType )
{
    switch ( actionType ) {
    case Save:
    case SaveAs:
    case Install:
    case Uninstall:
    case InstallGlobally:
    case UninstallGlobally:
        return FileActionGroup;

    case ShowProjectSettings:
    case ShowDashboard:
    case ShowHomepage:
    case ShowScript:
    case ShowProjectSource:
    case ShowPlasmaPreview:
        return UiActionGroup;

    case Interrupt:
    case Continue:
    case AbortDebugger:
    case RunToCursor:
    case StepInto:
    case StepOver:
    case StepOut:
    case ToggleBreakpoint:
    case RemoveAllBreakpoints:
        return DebuggerActionGroup;

    case RunMenuAction:
    case RunGetTimetable:
    case RunGetStopSuggestions:
    case RunGetJourneys:
    case DebugMenuAction:
    case DebugGetTimetable:
    case DebugGetStopSuggestions:
    case DebugGetJourneys:
        return RunActionGroup;

    case RunAllTests:
    case AbortRunningTests:
    case ClearTestResults:
    case RunSpecificTest:
    case RunSpecificTestCase:
    case SpecificTestCaseMenuAction:
        return TestActionGroup;

    case Close:
    case SetAsActiveProject:
        return OtherActionGroup;

    default:
        return InvalidProjectActionGroup;
    }
}

QList< Project::ProjectAction > Project::actionsFromGroup( Project::ProjectActionGroup group )
{
    QList< Project::ProjectAction > actionTypes;
    switch ( group ) {
    case FileActionGroup:
        actionTypes << Save << SaveAs << Install << Uninstall
                    << InstallGlobally << UninstallGlobally;
        break;
    case UiActionGroup:
        actionTypes << ShowProjectSettings << ShowDashboard << ShowHomepage
                    << ShowScript << ShowProjectSource << ShowPlasmaPreview;
        break;
    case DebuggerActionGroup:
        actionTypes << Interrupt << Continue << AbortDebugger << RunToCursor << StepInto << StepOver
                    << StepOut << ToggleBreakpoint << RemoveAllBreakpoints;
        break;
    case RunActionGroup:
        actionTypes << RunMenuAction << RunGetTimetable << RunGetStopSuggestions << RunGetJourneys
                    << DebugMenuAction << DebugGetTimetable << DebugGetStopSuggestions
                    << DebugGetJourneys;
        break;
    case TestActionGroup:
        actionTypes << RunAllTests << AbortRunningTests << ClearTestResults << RunSpecificTest
                    << RunSpecificTestCase << SpecificTestCaseMenuAction;
        break;
    case OtherActionGroup:
        actionTypes << Close << SetAsActiveProject;
        break;
    case InvalidProjectActionGroup:
    default:
        kWarning() << "Invalid group" << group;
        break;
    }
    return actionTypes;
}

bool Project::isTestRunning() const
{
    Q_D( const Project );
    return d->isTestRunning();
}

bool Project::isDebuggerRunning() const
{
    Q_D( const Project );
    return d->isDebuggerRunning();
}

QStringList Project::scriptFunctions() const
{
    Q_D( const Project );
    if ( d->scriptTab ) {
        return d->scriptTab->scriptModel()->functionNames();
    } else {
        JavaScriptParser parser( scriptText() );
        JavaScriptModel scriptModel;
        scriptModel.setNodes( parser.nodes() );
        return scriptModel.functionNames();
    }
}

bool Project::startTest( TestModel::Test test )
{
    Q_D( Project );
    bool finishedAfterThisTest = false;
    if ( !isTestRunning() ) {
        if ( !d->beginTesting() ) {
            // Test could not be started
            return false;
        } else {
            // Test started, only running one test
            finishedAfterThisTest = true;
        }
    }

    bool success;
    const TestModel::TestCase testCase = TestModel::testCaseOfTest( test );
    switch ( testCase ) {
    case TestModel::AccessorInfoTestCase: {
        d->testModel->markTestAsStarted( test );

        QString errorMessage, tooltip;
        success = AccessorInfoTester::runAccessorInfoTest(
                test, d->accessor->info(), &errorMessage, &tooltip );
        d->testModel->addTestResult( test, success ? TestModel::TestFinishedSuccessfully
                                                  : TestModel::TestFinishedWithErrors,
                                    errorMessage, tooltip, projectAction(ShowProjectSettings) );
    } break;

    case TestModel::ScriptExecutionTestCase:
        success = d->startScriptExecutionTest( test );
        break;

    default:
        kWarning() << "Unknown test" << test;
        success = false;
        break;
    }

    if ( finishedAfterThisTest ) {
        d->endTesting();
    }
    return success;
}

bool Project::startTestCase( TestModel::TestCase testCase )
{
    Q_D( Project );
    bool finishedAfterThisTestCase = false;
    if ( !isTestRunning() ) {
        if ( !d->beginTesting() ) {
            // Test could not be started
            return false;
        } else {
            // Test started, only running one test
            finishedAfterThisTestCase = true;
        }
    }

    bool success = true;
    const QList< TestModel::Test > tests = TestModel::testsOfTestCase( testCase );
    foreach ( TestModel::Test test, tests ) {
        if ( !startTest(test)  ) {
            success = false;
        }

        if ( d->testState == ProjectPrivate::TestsGetAborted ) {
            break;
        }
    }

    if ( finishedAfterThisTestCase ) {
        d->endTesting();
    }
    return success;
}

void Project::testProject()
{
    Q_D( Project );
    if ( !d->askForProjectActivation(ProjectPrivate::ActivateProjectForTests) || !d->beginTesting() ) {
        return;
    }

    startTestCase( TestModel::AccessorInfoTestCase ); // This test case runs synchronously

    // Get a list of all functions that are implemented in the script
    const QStringList functions = scriptFunctions();
    if ( !functions.contains(TimetableAccessorScript::SCRIPT_FUNCTION_GETTIMETABLE) ) {
        d->testModel->markTestCaseAsUnstartable( TestModel::ScriptExecutionTestCase,
                i18nc("@info/plain", "You need to implement a 'getTimetable' script function"),
                i18nc("@info", "<title>You need to implement a 'getTimetable' script function</title> "
                      "<para>Accessors that only support journeys are currently not accepted by the "
                      "data engine, but that may change.</para>"),
                projectAction(ShowScript) );
        d->endTesting();
        return;
    }

    // Run the script and check the results
    if ( !startTestCase(TestModel::ScriptExecutionTestCase) ||
         d->testState == ProjectPrivate::TestsGetAborted )
    {
        d->endTesting();
        return;
    }
}

void Project::abortTests()
{
    Q_D( Project );
    d->abortTests();
}

void Project::clearTestResults()
{
    Q_D( Project );
    d->testModel->clear();
    d->updateProjectActions( QList< ProjectAction >() << ClearTestResults );
}

void Project::testJobStarted( ThreadWeaver::Job *job )
{
    Q_D( Project );
    CallScriptFunctionJob *callFunctionJob = qobject_cast< CallScriptFunctionJob* >( job );
    if ( callFunctionJob ) {
        TestModel::Test test = TestModel::InvalidTest;
        TimetableDataRequestJob *requestJob = qobject_cast< TimetableDataRequestJob* >( job );
        if ( requestJob ) {
            const QString sourceName = requestJob->request()->sourceName;
            if ( sourceName == QLatin1String("TEST_DEPARTURES") ) {
                test = TestModel::DepartureTest;
            } else if ( sourceName == QLatin1String("TEST_ARRIVALS") ) {
                test = TestModel::ArrivalTest;
            } else if ( sourceName == QLatin1String("TEST_STOP_SUGGESTIONS") ) {
                test = TestModel::StopSuggestionTest;
            } else if ( sourceName == QLatin1String("TEST_JOURNEYS") ) {
                test = TestModel::JourneyTest;
            } else if ( sourceName == QLatin1String("TEST_USEDTIMETABLEINFORMATIONS") ) {
                test = TestModel::UsedTimetableInformationsTest;
            }
        } else if ( callFunctionJob->functionName() ==
                    TimetableAccessorScript::SCRIPT_FUNCTION_USEDTIMETABLEINFORMATIONS )
        {
            test = TestModel::UsedTimetableInformationsTest;
        }

        if ( test == TestModel::InvalidTest ) {
            kDebug() << "Unknown test job was started";
            return;
        } else {
            d->testModel->markTestAsStarted( test );
        }
    }
}

void Project::testJobDone( ThreadWeaver::Job *job )
{
    Q_D( Project );
    CallScriptFunctionJob *callFunctionJob = qobject_cast< CallScriptFunctionJob* >( job );
    if ( callFunctionJob ) {
        TestModel::Test test = TestModel::InvalidTest;
        TimetableDataRequestJob *requestJob = qobject_cast< TimetableDataRequestJob* >( job );
        if ( requestJob ) {
            const QString sourceName = requestJob->request()->sourceName;
            if ( sourceName == QLatin1String("TEST_DEPARTURES") ) {
                test = TestModel::DepartureTest;
            } else if ( sourceName == QLatin1String("TEST_ARRIVALS") ) {
                test = TestModel::ArrivalTest;
            } else if ( sourceName == QLatin1String("TEST_STOP_SUGGESTIONS") ) {
                test = TestModel::StopSuggestionTest;
            } else if ( sourceName == QLatin1String("TEST_JOURNEYS") ) {
                test = TestModel::JourneyTest;
            } else if ( sourceName == QLatin1String("TEST_USEDTIMETABLEINFORMATIONS") ) {
                test = TestModel::UsedTimetableInformationsTest;
            }
            if ( test != TestModel::InvalidTest ) {
                d->pendingTests.removeOne( requestJob );
            }
        } else if ( callFunctionJob->functionName() ==
                    TimetableAccessorScript::SCRIPT_FUNCTION_USEDTIMETABLEINFORMATIONS )
        {
            test = TestModel::UsedTimetableInformationsTest;
            d->pendingTests.removeOne( requestJob );
        }

        if ( test == TestModel::InvalidTest ) {
            kDebug() << "Unknown test job was done";
            return;
        } else {
            d->testModel->addTestResult( test,
                                         TestModel::testStateFromBool(callFunctionJob->success()),
                                         callFunctionJob->explanation(), QString(),
                                         projectAction(ShowScript),
                                         callFunctionJob->additionalMessages() );
        }

        if ( d->pendingTests.isEmpty() ) {
            // The last pending test has finished
            d->endTesting();
        } else if ( !job->success() ) {
            // The job was not successful, therefore following test jobs will not be executed
            d->endTesting();
        }
    }

    delete job;
}

void Project::functionCallResult( const QSharedPointer< AbstractRequest > &request,
                                  bool success, const QString &explanation,
                                  const QList< TimetableData >& timetableData,
                                  const QScriptValue &returnValue )
{
    Q_UNUSED( request );
    Q_UNUSED( timetableData );
    Q_UNUSED( returnValue );
    if ( !success ) {
        // Emit an information message about the error (no syntax errors here)
        emit informationMessage( explanation, KMessageWidget::Error, 10000 );
    }
}

void Project::loadScriptResult( ScriptErrorType lastScriptError,
                                const QString &lastScriptErrorString )
{
    if ( lastScriptError != NoScriptError ) {
        // Emit an information message about the error (eg. a syntax error)
        emit informationMessage( lastScriptErrorString, KMessageWidget::Error, 10000 );
    }
}

void Project::runGetTimetable()
{
    Q_D( Project );
    d->callGetTimetable( Debugger::InterruptOnExceptions );
}

void Project::debugGetTimetable()
{
    Q_D( Project );
    d->callGetTimetable( Debugger::InterruptAtStart );
}

void Project::runGetStopSuggestions()
{
    Q_D( Project );
    d->callGetStopSuggestions( Debugger::InterruptOnExceptions );
}

void Project::debugGetStopSuggestions()
{
    Q_D( Project );
    d->callGetStopSuggestions( Debugger::InterruptAtStart );
}

void Project::runGetJourneys()
{
    Q_D( Project );
    d->callGetJourneys( Debugger::InterruptOnExceptions );
}

void Project::debugGetJourneys()
{
    Q_D( Project );
    d->callGetJourneys( Debugger::InterruptAtStart );
}

DepartureRequest Project::getDepartureRequest( QWidget *parent, bool* cancelled ) const
{
    Q_D( const Project );
    parent = d->parentWidget( parent );

    QPointer<KDialog> dialog = new KDialog( parent );
    QWidget *w = new QWidget( dialog );
    QFormLayout *l = new QFormLayout( w );
    KLineEdit *city = 0;
    KLineEdit *stop = new KLineEdit( w );
    KComboBox *dataType = new KComboBox( w );
    KDateTimeWidget *dateTime = new KDateTimeWidget( QDateTime::currentDateTime(), w );
    dataType->addItem( i18nc("@info/plain", "Departures"), "departures" );
    dataType->addItem( i18nc("@info/plain", "Arrivals"), "arrivals" );
    if ( d->accessor->info()->useSeparateCityValue() ) {
        city = new KLineEdit( w );
        l->addRow( i18nc("@info", "City:"), city );
    }
    l->addRow( i18nc("@info", "Stop Name:"), stop );
    l->addRow( i18nc("@info", "Data Type:"), dataType );
    l->addRow( i18nc("@info", "Time:"), dateTime );
    dialog->setMainWidget( w );
    if ( d->accessor && !d->accessor->info()->sampleStopNames().isEmpty() ) {
        // Use first sample stop name by default
        stop->setText( d->accessor->info()->sampleStopNames().first() );
        if ( city ) {
            city->setText( d->accessor->info()->sampleCity() );
        }
    }
    stop->setFocus();

    // Show the dialog
    int result = dialog->exec();
    DepartureRequest request;
    if ( result == KDialog::Accepted ) {
        request.city = city ? city->text() : QString();
        request.stop = stop->text();
        request.dateTime = dateTime->dateTime();
        request.dataType = dataType->itemData( dataType->currentIndex() ).toString();
    }
    if ( cancelled ) {
        *cancelled = result != KDialog::Accepted;
    }

    delete dialog;
    return request;
}

StopSuggestionRequest Project::getStopSuggestionRequest( QWidget *parent,
                                                                 bool* cancelled ) const
{
    Q_D( const Project );
    parent = d->parentWidget( parent );

    QPointer<KDialog> dialog = new KDialog( parent );
    QWidget *w = new QWidget( dialog );
    QFormLayout *l = new QFormLayout( w );
    KLineEdit *city = 0;
    KLineEdit *stop = new KLineEdit( w );
    if ( d->accessor->info()->useSeparateCityValue() ) {
        city = new KLineEdit( w );
        l->addRow( i18nc("@info", "City:"), city );
    }
    l->addRow( i18nc("@info", "Partial Stop Name:"), stop );
    dialog->setMainWidget( w );
    stop->setFocus();

    StopSuggestionRequest info;
    int result = dialog->exec();
    if ( result == KDialog::Accepted ) {
        info.city = city ? city->text() : QString();
        info.stop = stop->text();
    }
    if ( cancelled ) {
        *cancelled = result != KDialog::Accepted;
    }

    delete dialog;
    return info;
}

JourneyRequest Project::getJourneyRequest( QWidget *parent, bool* cancelled ) const
{
    Q_D( const Project );
    parent = d->parentWidget( parent );
    QPointer<KDialog> dialog = new KDialog( parent );
    QWidget *w = new QWidget( dialog );
    QFormLayout *l = new QFormLayout( w );
    KLineEdit *city = 0;
    KLineEdit *originStop = new KLineEdit( w );
    KLineEdit *targetStop = new KLineEdit( w );
    KComboBox *dataType = new KComboBox( w );
    KDateTimeWidget *dateTime = new KDateTimeWidget( QDateTime::currentDateTime(), w );
    dataType->addItem( i18nc("@info/plain", "Departing at Given Time"), "dep" );
    dataType->addItem( i18nc("@info/plain", "Arriving at Given Time"), "arr" );
    if ( d->accessor->info()->useSeparateCityValue() ) {
        city = new KLineEdit( w );
        l->addRow( i18nc("@info", "City:"), city );
    }
    l->addRow( i18nc("@info", "Start Stop Name:"), originStop );
    l->addRow( i18nc("@info", "Target Stop Name:"), targetStop );
    l->addRow( i18nc("@info", "Time:"), dateTime );
    l->addRow( i18nc("@info", "Meaning of Time:"), dataType );
    dialog->setMainWidget( w );
    if ( d->accessor && !d->accessor->info()->sampleStopNames().isEmpty() ) {
        // Use sample stop names by default
        originStop->setText( d->accessor->info()->sampleStopNames().first() );
        if ( d->accessor->info()->sampleStopNames().count() >= 2 ) {
            targetStop->setText( d->accessor->info()->sampleStopNames()[1] );
        }
        if ( city ) {
            city->setText( d->accessor->info()->sampleCity() );
        }
    }
    originStop->setFocus();

    JourneyRequest info;
    int result = dialog->exec();
    if ( result == KDialog::Accepted ) {
        info.city = city ? city->text() : QString();
        info.stop = originStop->text();
        info.targetStop = targetStop->text();
        info.dateTime = dateTime->dateTime();
        info.dataType = dataType->itemData( dataType->currentIndex() ).toString();
    }
    if ( cancelled ) {
        *cancelled = result != KDialog::Accepted;
    }

    delete dialog;
    return info;
}

void Project::toggleBreakpoint( int lineNumber )
{
    Q_D( Project );
    if ( !d->scriptTab ) {
        kDebug() << "No script tab opened";
        return;
    }

    d->scriptTab->toggleBreakpoint( lineNumber );
}

void Project::runToCursor()
{
    Q_D( Project );
    if ( !d->scriptTab ) {
        kError() << "No script tab opened";
        return;
    }

    const KTextEditor::View *view = d->scriptTab->document()->activeView();
    d->debugger->debugRunUntilLineNumber( view->cursorPosition().line() + 1 );
}

void Project::debugInterrupted()
{
    Q_D( Project );
    kDebug() << "Interrupted";

    // Move cursor position
    ScriptTab *tab = scriptTab();

    if ( tab ) {
        KTextEditor::View *view = tab->document()->activeView();
        view->blockSignals( true );
        view->setCursorPosition( KTextEditor::Cursor(d->debugger->lineNumber() - 1,
                                                     d->debugger->columnNumber()) );
        view->blockSignals( false );

        // Move execution mark
        KTextEditor::MarkInterface *markInterface =
                qobject_cast<KTextEditor::MarkInterface*>( tab->document() );
        if ( !markInterface ) {
            kDebug() << "Cannot mark current execution line, no KTextEditor::MarkInterface";
        } else if ( d->executionLine != d->debugger->lineNumber() - 1 ) {
            if ( d->executionLine != -1 ) {
                markInterface->removeMark( d->executionLine, KTextEditor::MarkInterface::Execution );
            }
            if ( d->debugger->lineNumber() != -1 ) {
                d->executionLine = d->debugger->lineNumber() - 1;
                markInterface->addMark( d->executionLine, KTextEditor::MarkInterface::Execution );
            }
        }
    }

    d->updateProjectActions( QList<ProjectActionGroup>() << RunActionGroup << TestActionGroup
                                                         << DebuggerActionGroup );

    if ( d->scriptTab ) {
        d->scriptTab->slotTitleChanged();
    }
}

void Project::debugContinued()
{
    Q_D( Project );

    // Remove execution mark
    if ( scriptTab() ) {
        KTextEditor::MarkInterface *iface =
                qobject_cast<KTextEditor::MarkInterface*>( scriptTab()->document() );
        if ( !iface ) {
            kDebug() << "Cannot remove execution mark, no KTextEditor::MarkInterface";
        } else if ( d->executionLine != -1 ) {
            iface->removeMark( d->executionLine, KTextEditor::MarkInterface::Execution );
            d->executionLine = -1;
        }
    }

    d->updateProjectActions( QList<ProjectActionGroup>() << RunActionGroup << TestActionGroup
                                                      << DebuggerActionGroup );

    if ( d->scriptTab ) {
        d->scriptTab->slotTitleChanged();
    }
}

void Project::debugStarted()
{
    Q_D( Project );
    d->updateProjectActions( QList<ProjectActionGroup>() << RunActionGroup << TestActionGroup
                                                      << DebuggerActionGroup );

    if ( d->scriptTab ) {
        d->scriptTab->slotTitleChanged();
    }
    emit debuggerRunningChanged( true );
}

void Project::debugStopped()
{
    Q_D( Project );
    d->updateProjectActions( QList<ProjectActionGroup>() << RunActionGroup << TestActionGroup
                                                         << DebuggerActionGroup );

    if ( d->scriptTab ) {
        d->scriptTab->slotTitleChanged();
    }

    // Remove execution mark
    ScriptTab *tab = scriptTab();
    if ( tab ) {
        KTextEditor::MarkInterface *markInterface =
                qobject_cast<KTextEditor::MarkInterface*>( tab->document() );
        if ( markInterface && d->executionLine != -1 ) {
            markInterface->removeMark( d->executionLine, KTextEditor::MarkInterface::Execution );
            d->executionLine = -1;
        }
    }
    emit debuggerRunningChanged( false );
}

QString Project::scriptFileName() const
{
    Q_D( const Project );
    return d->accessor->info()->scriptFileName();
}

QIcon Project::scriptIcon() const
{
    Q_D( const Project );
    if ( d->scriptTab ) {
        return KIcon( d->scriptTab->document()->mimeType().replace('/', '-') );
    } else {
        return KIcon("application-javascript");
    }
}

QString Project::scriptTemplateText( ScriptTemplateType templateType )
{
    return ProjectPrivate::scriptTemplateText( templateType );
}

void Project::scriptAdded( const QString &fileName )
{
    kDebug() << fileName;
}

void Project::scriptFileChanged( const QString &fileName )
{
    kDebug() << fileName;
}

void Project::slotTabTitleChanged( const QString &title )
{
    AbstractTab *tab = qobject_cast< AbstractTab* >( sender() );
    Q_ASSERT( tab );

    emit tabTitleChanged( tab, title, tab->icon() );
}

void Project::slotModifiedStateChanged()
{
    Q_D( Project );
    const bool modified = isModified();
    d->updateProjectActions( QList<ProjectAction>() << Save );
    emit modifiedStateChanged( modified );
}

void Project::slotTabCloseRequest()
{
    AbstractTab *tab = qobject_cast< AbstractTab* >( sender() );
    Q_ASSERT( tab );
    emit tabCloseRequest( tab );
}

void Project::slotOtherTabsCloseRequest()
{
    AbstractTab *tab = qobject_cast< AbstractTab* >( sender() );
    Q_ASSERT( tab );
    emit otherTabsCloseRequest( tab );
}

AbstractTab *Project::tab( TabType type ) const
{
    switch ( type ) {
    case Tabs::Dashboard:
        return dashboardTab();
    case Tabs::ProjectSource:
        return projectSourceTab();
    case Tabs::Script:
        return scriptTab();
    case Tabs::Web:
        return webTab();
    case Tabs::PlasmaPreview:
        return plasmaPreviewTab();
    default:
        kWarning() << "Unknown tab type" << type;
        return 0;
    }
}

AbstractTab *Project::showTab( TabType type, QWidget *parent )
{
    switch ( type ) {
    case Tabs::Dashboard:
        return showDashboardTab( parent );
    case Tabs::ProjectSource:
        return showProjectSourceTab( parent );
    case Tabs::Script:
        return showScriptTab( parent );
    case Tabs::Web:
        return showWebTab( parent );
    case Tabs::PlasmaPreview:
        return showPlasmaPreviewTab( parent );
    default:
        kWarning() << "Unknown tab type" << type;
        return 0;
    }
}

bool Project::isTabOpened( TabType type ) const
{
    Q_D( const Project );
    switch ( type ) {
    case Tabs::Dashboard:
        return d->dashboardTab;
    case Tabs::ProjectSource:
        return d->projectSourceTab;
    case Tabs::Script:
        return d->scriptTab;
    case Tabs::Web:
        return d->webTab;
    case Tabs::PlasmaPreview:
        return d->plasmaPreviewTab;
    default:
        return false;
    }
}

AbstractTab *Project::createTab( TabType type, QWidget *parent )
{
    Q_D( Project );
    parent = d->parentWidget( parent );
    switch ( type ) {
    case Tabs::Dashboard:
        return createDashboardTab( parent );
    case Tabs::ProjectSource:
        return createProjectSourceTab( parent );
    case Tabs::Script:
        return createScriptTab( parent );
    case Tabs::Web:
        return createWebTab( parent );
    case Tabs::PlasmaPreview:
        return createPlasmaPreviewTab( parent );
    default:
        return 0;
    }
}

void Project::closeTab( TabType type )
{
    AbstractTab *tabOfType = tab( type );
    if ( tabOfType ) {
        emit tabCloseRequest( tabOfType );
    }
}

PlasmaPreviewTab *Project::createPlasmaPreviewTab( QWidget *parent )
{
    Q_D( Project );

    // Create plasma preview tab
    parent = d->parentWidget( parent );
    d->plasmaPreviewTab = PlasmaPreviewTab::create( this, parent );
    if ( d->plasmaPreviewTab ) {
        // Connect default tab slots with the tab
        connect( d->plasmaPreviewTab, SIGNAL(destroyed(QObject*)),
                 this, SLOT(plasmaPreviewTabDestroyed()) );
        d->connectTab( d->plasmaPreviewTab );
        return d->plasmaPreviewTab;
    } else {
        d->errorHappened( PlasmaPreviewError, i18nc("@info", "Cannot create Plasma preview") );
        return 0;
    }
}

WebTab *Project::createWebTab( QWidget *parent )
{
    Q_D( Project );

    // Create web widget
    parent = d->parentWidget( parent );
    d->webTab = WebTab::create( this, parent );
    if ( d->webTab ) {
        // Connect default tab slots with the tab
        connect( d->webTab, SIGNAL(destroyed(QObject*)),
                 this, SLOT(webTabDestroyed()) );
        d->connectTab( d->webTab );

        // Load the service providers home page
        d->webTab->webView()->setUrl( d->accessor->info()->url() );
        return d->webTab;
    } else {
        d->errorHappened( WebError, i18nc("@info", "Cannot create web widget") );
        return 0;
    }
}

DashboardTab *Project::createDashboardTab( QWidget *parent )
{
    Q_D( Project );

    // Create dashboard widget
    parent = d->parentWidget( parent );
    d->dashboardTab = DashboardTab::create( this, parent );
    if ( d->dashboardTab ) {
        // Connect default tab slots with the tab
        connect( d->dashboardTab, SIGNAL(destroyed(QObject*)),
                 this, SLOT(dashboardTabDestroyed()) );
        d->connectTab( d->dashboardTab );

        return d->dashboardTab;
    } else {
        d->errorHappened( WebError, i18nc("@info", "Cannot create dashboard widget") );
        return 0;
    }
}

ProjectSourceTab *Project::createProjectSourceTab( QWidget *parent )
{
    Q_D( Project );

    if ( d->projectSourceTab ) {
        kWarning() << "Project source tab already created";
        return d->projectSourceTab;
    }

    // Get accessor text from d->accessor
    const QString text = accessorText( ReadProjectDocumentFromBuffer );

    // Try to create an project source document tab
    parent = d->parentWidget( parent );
    d->projectSourceTab = ProjectSourceTab::create( this, parent );
    if ( !d->projectSourceTab ) {
        d->errorHappened( KatePartError, i18nc("@info", "service katepart.desktop not found") );
        return 0;
    }

    // Connect slots with the document
    KTextEditor::Document *document = d->projectSourceTab->document();
    connect( document, SIGNAL(setStatusBarText(QString)),
             this, SIGNAL(informationMessage(QString)) );
    connect( document, SIGNAL(textChanged(KTextEditor::Document*)),
             this, SLOT(projectSourceDocumentChanged(KTextEditor::Document*)));

    // Connect slots with the view
    KTextEditor::View *accessorView = document->views().first();
    connect( accessorView, SIGNAL(informationMessage(KTextEditor::View*,QString)),
             this, SLOT(slotInformationMessage(KTextEditor::View*,QString)) );

    // Connect default tab slots with the tab
    d->connectTab( d->projectSourceTab );
    connect( d->projectSourceTab, SIGNAL(destroyed(QObject*)),
             this, SLOT(projectSourceTabDestroyed()) );
    connect( d->projectSourceTab, SIGNAL(modifiedStatusChanged(bool)),
             this, SIGNAL(projectSourceModifiedStateChanged(bool)) );

    if ( d->setProjectSourceDocumentText(text) ) {
        d->projectSourceTab->document()->setModified( false );
    }
    return d->projectSourceTab;
}

ScriptTab *Project::createScriptTab( QWidget *parent )
{
    Q_D( Project );

    if ( d->scriptTab ) {
        kWarning() << "Script tab already created";
        return d->scriptTab;
    }

    // Create script tab
    parent = d->parentWidget( parent );
    d->scriptTab = ScriptTab::create( this, parent );
    if ( !d->scriptTab ) {
        d->errorHappened( KatePartError, i18nc("@info", "Service katepart.desktop not found") );
        return 0;
    }

    // Try to load the script
    if ( !d->loadScript(NoScriptTemplate) ) {
        // Script not created yet, create the script from a template,
        // ask the user which template to use
        ScriptTemplateType templateType = getScriptTemplateTypeInput( parent );
        if ( templateType == NoScriptTemplate ) {
            delete d->scriptTab;
            d->scriptTab = 0;
            return 0;
        }

        d->insertScriptTemplate( templateType );
    }

    d->updateProjectActions( QList< ProjectAction >() << ToggleBreakpoint );

    // Connect default tab slots with the tab
    d->connectTab( d->scriptTab );
    connect( d->scriptTab, SIGNAL(destroyed(QObject*)),
             this, SLOT(scriptTabDestroyed()) );
    connect( d->scriptTab, SIGNAL(modifiedStatusChanged(bool)),
             this, SIGNAL(scriptModifiedStateChanged(bool)) );
    return d->scriptTab;
}

TimetableAccessor *Project::accessor() const
{
    Q_D( const Project );
    Q_ASSERT( d->accessor );
    return d->accessor;
}

void Project::setAccessorInfo( const TimetableAccessorInfo *accessorInfo )
{
    Q_D( Project );

    // Recreate accessor with new info
    delete d->accessor;
    d->accessor = new TimetableAccessor( accessorInfo, this );
    emit nameChanged( projectName() );
    emit iconNameChanged( iconName() );
    emit iconChanged( projectIcon() );
    emit infoChanged( info() );

    if ( d->projectSourceTab ) {
        // Update accessor document
        d->projectSourceTab->document()->setText( accessorText(ReadProjectDocumentFromBuffer) );
    } else {
        const bool wasModified = isModified();
        const bool wasAccessorModified = isProjectSourceModified();
        d->projectSourceBufferModified = true;
        if ( !wasModified ) {
            d->updateProjectActions( QList<ProjectAction>() << Save );
            emit modifiedStateChanged( true );
        }
        if ( !wasAccessorModified ) {
            emit projectSourceModifiedStateChanged( true );
        }
    }
}

void Project::showSettingsDialog( QWidget *parent )
{
    Q_D( Project );

    // Check if a modified accessor tab is opened and ask to save it before
    // editing the file in the settings dialog
    parent = d->parentWidget( parent );
    if ( d->projectSourceTab && d->projectSourceTab->isModified() ) {
        int result = KMessageBox::warningContinueCancel( parent,
                i18nc("@info", "The project XML file was modified. Please save it first."),
                QString(), KStandardGuiItem::save() );
        if ( result == KMessageBox::Continue ) {
            // Save clicked, save project XML file
            d->projectSourceTab->save();
        } else {
            // Cancel clicked, do not save and do not open settings dialog
            return;
        }
    }

    // Create settings dialog
    QPointer< ProjectSettingsDialog > dialog( new ProjectSettingsDialog(parent) );
    dialog->setAccessorInfo( d->accessor->info(), d->filePath );
    if ( dialog->exec() == KDialog::Accepted ) {
        setAccessorInfo( dialog->accessorInfo(this) );

        if ( dialog->newScriptTemplateType() != Project::NoScriptTemplate ) {
            // A new script file was set in the dialog
            // Load the chosen template
            setScriptText( Project::scriptTemplateText(dialog->newScriptTemplateType()) );
        }
    }
    delete dialog.data();
}

void Project::projectSourceDocumentChanged( KTextEditor::Document *projectSourceDocument )
{
    Q_D( Project );
    Q_UNUSED( projectSourceDocument );

    // Recreate accessor with new XML content
    d->readProjectSourceDocument( d->filePath );

    // Update other tabs
    if ( d->webTab ) {
        d->webTab->webView()->setUrl( accessor()->info()->url() );
    }
}

void Project::dashboardTabDestroyed()
{
    Q_D( Project );
    d->dashboardTab = 0;
}

void Project::projectSourceTabDestroyed()
{
    Q_D( Project );
    d->projectSourceTab = 0;
}

void Project::scriptTabDestroyed()
{
    Q_D( Project );
    d->scriptTab = 0;
    d->updateProjectActions( QList< ProjectAction >() << ToggleBreakpoint );
}

void Project::plasmaPreviewTabDestroyed()
{
    Q_D( Project );
    d->plasmaPreviewTab = 0;
}

void Project::webTabDestroyed()
{
    Q_D( Project );
    d->webTab = 0;
}

QString Project::accessorText( ProjectDocumentSource source) const
{
    Q_D( const Project );
    if ( !d->accessor ) {
        kDebug() << "No accessor loaded";
        return QString();
    }

    if ( d->projectSourceTab && (source == ReadProjectDocumentFromTab ||
                            source == ReadProjectDocumentFromTabIfOpened) )
    {
        // Accessor XML file opened in a tab
        return d->projectSourceTab->document()->text();
    } else if ( source == ReadProjectDocumentFromBuffer ||
                source == ReadProjectDocumentFromTabIfOpened )
    {
        // No accessor tab opened, read XML text from file to buffer
        AccessorInfoXmlWriter writer;
        QBuffer buffer;
        if( writer.write(&buffer, d->accessor) ) {
            return QString::fromUtf8( buffer.data() );
        }
    } else if ( source == ReadProjectDocumentFromFile ) {
        if ( d->filePath.isEmpty() ) {
            return QString();
        }

        // Open project file
        QFile file( d->filePath );
        if ( !file.open(QIODevice::ReadOnly) ) {
            kDebug() << "Could not open project file" << d->filePath;
            return QString();
        }

        // Read and close project file
        const QByteArray ba = file.readAll();
        file.close();
        return QString( ba );
    }

    return QString();
}

QString Project::scriptText() const
{
    Q_D( const Project );
    if ( d->scriptTab ) {
        // Script file opened in a tab
        return d->scriptTab->document()->text();
    } else if ( !d->unsavedScriptContents.isEmpty() ) {
        // Unsaved script contents available
        return d->unsavedScriptContents;
    } else {
        // No script tab opened, read script text from file
        const QString fileName = d->accessor->info()->scriptFileName();
        if ( !QFile::exists(fileName) ) {
            return QString();
        }

        // Open script file
        QFile file( fileName );
        if ( !file.open(QIODevice::ReadOnly) ) {
            return QString();
        }

        // Read and close script file
        const QByteArray ba = file.readAll();
        file.close();
        return QString( ba );
    }
}

void Project::setScriptText( const QString &text )
{
    Q_D( Project );
    if ( d->scriptTab ) {
        d->unsavedScriptContents.clear();
        d->scriptTab->document()->setText( text );
    } else {
        const bool wasModified = isModified();
        const bool wasScriptModified = isScriptModified();

        d->unsavedScriptContents = text;
        d->updateProjectActions( QList<ProjectAction>() << Save );

        if ( !wasModified ) {
            emit modifiedStateChanged( true );
        }
        if ( !wasScriptModified ) {
            emit scriptModifiedStateChanged( true );
        }
    }
}

Project::ScriptTemplateType Project::getScriptTemplateTypeInput( QWidget *parent ) const
{
    Q_D( const Project );
    bool ok;
    QStringList templates;
    parent = d->parentWidget( parent );
    templates << i18nc("@info/plain", "Complete JavaScript Template")
              << i18nc("@info/plain", "Simple Ruby Template")
              << i18nc("@info/plain", "Simple Python Template");
    const QString scriptType = KInputDialog::getItem(
            i18nc("@title:window", "Script Template"),
            i18nc("@info", "Choose a template for the new script"),
            templates, 0, false, &ok, parent );
    if ( !ok ) {
        return NoScriptTemplate;
    }

    const int selectedTemplate = templates.indexOf( scriptType );
    if( selectedTemplate == 0 ) {
        return ScriptQtScriptTemplate;
    } else if( selectedTemplate == 1 ) {
        return ScriptRubyTemplate;
    } else if( selectedTemplate == 2 ) {
        return ScriptPythonTemplate;
    } else {
        kWarning() << "Unexpected script type" << scriptType;
        return NoScriptTemplate;
    }
}

bool Project::isProjectSourceModified() const
{
    Q_D( const Project );
    return d->isProjectSourceModified();
}

bool Project::isScriptModified() const
{
    Q_D( const Project );
    return d->isScriptModified();
}

bool Project::isModified() const
{
    Q_D( const Project );
    return d->isModified();
}

void Project::showTextHint( const KTextEditor::Cursor &position, QString &text ) {
    Q_D( const Project );
    KTextEditor::View *activeView = d->scriptTab->document()->activeView();
    const QPoint pointInView = activeView->cursorToCoordinate( position );
    const QPoint pointGlobal = activeView->mapToGlobal( pointInView );
    QToolTip::showText( pointGlobal, text );
}

bool Project::save( QWidget *parent, const QString &xmlFilePath )
{
    Q_D( Project );
    return d->save( parent, xmlFilePath );
}

bool Project::saveAs( QWidget *parent )
{
    Q_D( Project );
    return d->saveAs( parent );
}

bool Project::install( QWidget *parent, InstallType installType )
{
    Q_D( Project );
    return d->install( parent, true, installType );
}

bool Project::uninstall( QWidget *parent, Project::InstallType installType )
{
    Q_D( Project );
    return d->install( parent, false, installType );
}

bool Project::isInstalledLocally() const
{
    Q_D( const Project );
    return d->isInstalledLocally();
}

bool Project::isInstalledGlobally() const
{
    Q_D( const Project );
    return d->isInstalledGlobally();
}

QString Project::iconName() const
{
    Q_D( const Project );
    return d->iconName();
}

QIcon Project::projectIcon() const
{
    Q_D( const Project );
    return d->projectIcon();
}

QString Project::projectName() const
{
    Q_D( const Project );
    return d->projectName();
}

const TimetableAccessorInfo *Project::info()
{
    Q_D( Project );
    return d->info();
}

Project::InstallType Project::Project::installationTypeFromFilePath( const QString &filePath )
{
    if ( filePath.isEmpty() ) {
        return NoInstallation;
    }

    const QString saveDir = QFileInfo( filePath ).path() + '/';
    const QString localSaveDir = KGlobal::dirs()->saveLocation( "data",
            "plasma_engine_publictransport/accessorInfos/" );
    if ( saveDir == localSaveDir ) {
        return LocalInstallation;
    }

    const QStringList allSaveDirs = KGlobal::dirs()->findDirs( "data",
                "plasma_engine_publictransport/accessorInfos/" );
    if ( allSaveDirs.contains(saveDir) ) {
        return GlobalInstallation;
    }

    return NoInstallation;
}

QString Project::savePathInfoStringFromFilePath( const QString &filePath )
{
    if ( filePath.isEmpty() ) {
        // Project not saved
        return i18nc("@info:tooltip", "Project not saved");
    } else {
        // Project is saved
        InstallType installType = installationTypeFromFilePath( filePath );
        switch ( installType ) {
        case Project::LocalInstallation:
            return i18nc("@info:tooltip", "Project is opened from local installation directory at "
                         "<filename>%1</filename>", filePath);
        case Project::GlobalInstallation:
            return i18nc("@info:tooltip", "Project is opened from global installation directory at "
                         "<filename>%1</filename>", filePath);
        case Project::NoInstallation:
        default:
            return i18nc("@info:tooltip", "Project saved at <filename>%1</filename>", filePath);
        }
    }
}
