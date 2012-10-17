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
#include "debug_config.h"
#include "projectmodel.h"
#include "projectsettingsdialog.h"
#include "serviceproviderdatawriter.h"
#include "serviceproviderdatatester.h"
#include "testmodel.h"
#include "tabs/abstracttab.h"
#include "tabs/dashboardtab.h"
#include "tabs/projectsourcetab.h"
#include "tabs/webtab.h"
#include "tabs/plasmapreviewtab.h"
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    #include "tabs/scripttab.h"
#endif
#include "debugger/debugger.h"
#include "debugger/backtracemodel.h"
#include "debugger/breakpointmodel.h"
#include "debugger/debuggerjobs.h"
#include "debugger/timetabledatarequestjob.h"

// PublicTransport engine includes
#include <engine/serviceprovider.h>
#include <engine/serviceproviderdata.h>
#include <engine/serviceproviderglobal.h>
#include <engine/script/serviceproviderscript.h>
#include <engine/request.h>
#include <engine/global.h>

// KDE includes
#include <KUrlComboBox>
#include <KLineEdit>
#include <KDoubleNumInput>
#include <KDateTimeWidget>
#include <KWebView>
#include <KMessageBox>
#include <KFileDialog>
#include <KInputDialog>
#include <KIcon>
#include <KMenu>
#include <KAction>
#include <KActionMenu>
#include <KStandardDirs>
#include <KColorScheme>
#include <KLocale>
#include <KLocalizedString>
#include <KAuth/ActionReply>
#include <KAuth/Action>
#include <KTextEditor/Document>
#include <KTextEditor/View>
#include <KTextEditor/TemplateInterface>
#include <KTextEditor/MarkInterface>
#include <ThreadWeaver/WeaverInterface>

#ifdef MARBLE_FOUND
    #include <marble/LatLonEdit.h>
#endif

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
        AutoUpdateEnabledState  = 0x0001,  /**< If this flag is set, the enabled state of the
                * connected project action gets updated in updateProjectActions().
                * Do not use this flag, if the action is always enabled or if its enabled state
                * gets updated in another way, eg. by connecting to its setEnabled()/setDisabled()
                * slots. */
        UseQueuedConnection     = 0x0002
    };
    Q_DECLARE_FLAGS( ConnectProjectActionFlags, ConnectProjectActionFlag );

    enum TestState {
        NoTestRunning,
        TestsRunning,
        TestsGetAborted
    };

    enum ScriptState {
        ScriptNotLoaded = 0,
        ScriptLoaded
    };

    ProjectPrivate( const WeaverInterfacePointer &weaver, Project *project )
        : state(Project::Uninitialized),
          projectModel(0), projectSourceBufferModified(false),
          dashboardTab(0), projectSourceTab(0), plasmaPreviewTab(0), webTab(0),
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
          scriptState(ScriptNotLoaded), scriptTab(0),
          debugger(new Debugger::Debugger(weaver, project)),
#endif
          provider(ServiceProvider::createInvalidProvider(project)),
          testModel(new TestModel(project)), testState(NoTestRunning),
          suppressMessages(false), enableQuestions(true), q_ptr(project)
    {
    };

    static inline QString serviceProviderIdFromProjectFileName( const QString &fileName ) {
        return fileName.left( fileName.lastIndexOf('.') );
    };

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    static QString scriptTemplateText( Project::ScriptTemplateType templateType
                                       = Project::DefaultScriptTemplate )
    {
        QString templateText = QString::fromUtf8(
                "/** Service provider plugin for ${Service Provider}\n"
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
            // TODO
                    "\n// This function gets called to determine the features of the service provider\n"
                    "function features() {\n" // TODO
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
#endif // BUILD_PROVIDER_TYPE_SCRIPT

    void enableDebuggerInformationMessages( bool enable = true ) {
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        Q_Q( Project );
        if ( enable ) {
            q->connect( debugger, SIGNAL(informationMessage(QString)), q, SIGNAL(informationMessage(QString)) );
            q->connect( debugger, SIGNAL(errorMessage(QString)), q, SLOT(emitErrorMessage(QString)) );
        } else {
            q->disconnect( debugger, SIGNAL(informationMessage(QString)), q, SIGNAL(informationMessage(QString)) );
            q->disconnect( debugger, SIGNAL(errorMessage(QString)), q, SLOT(emitErrorMessage(QString)) );
        }
#endif // BUILD_PROVIDER_TYPE_SCRIPT
    };

    // Initialize member variables, connect slots
    bool initialize()
    {
        Q_ASSERT( state == Project::Uninitialized );
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        Q_Q( Project );

        // Connect to signals of the debugger
        q->connect( debugger, SIGNAL(interrupted(int,QString,QDateTime)),
                    q, SLOT(debugInterrupted(int,QString,QDateTime)) );
        q->connect( debugger, SIGNAL(continued(QDateTime,bool)), q, SLOT(debugContinued()) );
        q->connect( debugger, SIGNAL(started()), q, SLOT(debugStarted()) );
        q->connect( debugger, SIGNAL(stopped(ScriptRunData)), q, SLOT(debugStopped(ScriptRunData)) );
        q->connect( debugger, SIGNAL(aborted()), q, SLOT(debugAborted()) );
        enableDebuggerInformationMessages();

        q->connect( debugger, SIGNAL(jobStarted(JobType,QString,QString)),
                    q, SLOT(jobStarted(JobType,QString,QString)) );
        q->connect( debugger, SIGNAL(jobDone(JobType,QString,QString,DebuggerJobResult)),
                    q, SLOT(jobDone(JobType,QString,QString,DebuggerJobResult)) );
        q->connect( debugger, SIGNAL(loadScriptResult(ScriptErrorType,QString,QStringList,QStringList)),
                    q, SLOT(loadScriptResult(ScriptErrorType,QString,QStringList,QStringList)) );
        q->connect( debugger, SIGNAL(requestTimetableDataResult(QSharedPointer<AbstractRequest>,bool,QString,QList<TimetableData>,QVariant)),
                    q, SLOT(functionCallResult(QSharedPointer<AbstractRequest>,bool,QString,QList<TimetableData>,QVariant)) );

        q->connect( debugger, SIGNAL(output(QString,QScriptContextInfo)),
                    q, SLOT(scriptOutput(QString,QScriptContextInfo)) );
        q->connect( debugger, SIGNAL(scriptMessageReceived(QString,QScriptContextInfo,QString,Helper::ErrorSeverity)),
                    q, SLOT(scriptMessageReceived(QString,QScriptContextInfo,QString,Helper::ErrorSeverity)) );
        q->connect( debugger, SIGNAL(exception(int,QString,QString)),
                    q, SLOT(scriptException(int,QString,QString)) );
        q->connect( debugger, SIGNAL(evaluationResult(EvaluationResult)),
                    q, SLOT(evaluationResult(EvaluationResult)) );
        q->connect( debugger, SIGNAL(commandExecutionResult(QString)),
                    q, SLOT(commandExecutionResult(QString)) );
        q->connect( debugger, SIGNAL(waitingForSignal()), q, SLOT(waitingForSignal()) );
        q->connect( debugger, SIGNAL(wokeUpFromSignal(int)), q, SLOT(wokeUpFromSignal(int)) );
#endif

        state = Project::NoProjectLoaded;
        return true;
    };

    // Load project from service provider XML document at @p projectSourceFile
    bool loadProject( const QString &projectSourceFile )
    {
        Q_Q( Project );

        // Try to open the XML in the Kate part in the "Project Source" tab
        if ( !QFile::exists(projectSourceFile) ) {
            // Project file not found, create a new one from template
            errorHappened( Project::ProjectFileNotFound, i18nc("@info", "The project file <filename>"
                        "%1</filename> could not be found.", projectSourceFile) );
            insertProjectSourceTemplate();
            return false;
        }

        if ( isModified() ) {
            kWarning() << "Loading another project, discarding changes in the previously "
                          "loaded project";
        }

        // Cleanup
        if ( projectSourceTab ) {
            projectSourceTab->document()->closeUrl( false );
        }
        lastError.clear();
        output.clear();
        consoleText.clear();
        projectSourceBufferModified = false;
        filePath.clear();
        serviceProviderID.clear();
        abortTests();
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        unsavedScriptContents.clear();
        if ( scriptTab ) {
            scriptTab->document()->closeUrl( false );
            scriptTab->setExecutionPosition( -1 );
        }
        debugger->abortDebugger();
#endif
        testModel->clear();
        q->emit outputCleared();
        q->emit outputChanged();
        q->emit consoleTextChanged( QString() );

        KUrl url( projectSourceFile );
        if ( projectSourceTab ) {
            if ( !projectSourceTab->document()->openUrl(url) ) {
                errorHappened( Project::ProjectFileNotReadable,
                               i18nc("@info", "Could not open project source document "
                                     "<filename>%1</filename>.", url.url()) );
            }
            projectSourceTab->document()->setModified( false );
        }

        if ( !readProjectSourceDocumentFromTabOrFile(projectSourceFile) ) {
            insertProjectSourceTemplate();
            return false;
        }

        setXmlFilePath( projectSourceFile );
        state = Project::ProjectSuccessfullyLoaded;

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        scriptState = ScriptNotLoaded;

        // Load script file referenced by the XML
        loadScript();

        q->scriptSaved();
#endif

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
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        return (scriptTab && scriptTab->isModified()) || !unsavedScriptContents.isEmpty();
#else
        return false; // No script support
#endif
    };

    bool isModified() const
    {
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        foreach ( ScriptTab *tab, externalScriptTabs ) {
            if ( tab->isModified() ) {
                return true;
            }
        }
#endif

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
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        return debugger->isRunning();
#else
        return false; // No script support, no debugger
#endif
    };

    QString projectName() const
    {
        QString name = provider->data()->names()[ KGlobal::locale()->country() ];
        if ( name.isEmpty() ) {
            // No translated name
            name = provider->data()->name();
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

    inline const ServiceProviderData *data()
    {
        return provider->data();
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
        if ( filePath.isEmpty() ) {
            return false;
        }
        const QString localSaveDir = KGlobal::dirs()->saveLocation( "data",
                ServiceProviderGlobal::installationSubDirectory() );
        const QString fileName = QFileInfo( filePath ).fileName();
        return QFile::exists( localSaveDir + '/' + fileName );
    };

    bool isInstalledGlobally() const
    {
        if ( filePath.isEmpty() ) {
            return false;
        }
        const QString globalSaveDir = KGlobal::dirs()->findDirs( "data",
                ServiceProviderGlobal::installationSubDirectory() ).last();
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

    // Read service provider plugin XML document from file or from opened project source document tab
    bool readProjectSourceDocumentFromTabOrFile( const QString &xmlFilePath )
    {
        Q_Q( Project );
        if ( xmlFilePath.isEmpty() ) {
            kDebug() << "No xml file path given, insert template";
            insertProjectSourceTemplate();
            return true;
        }

        // Try to read the XML contents
        if ( projectSourceTab ) {
            // Use text in already loaded project source document
            QTextCodec *codec = QTextCodec::codecForName( projectSourceTab->document()->encoding().isEmpty()
                                ? "UTF-8" : projectSourceTab->document()->encoding().toLatin1() );
            QByteArray text = codec->fromUnicode( projectSourceTab->document()->text() );
            QBuffer buffer( &text, q );
            return readProjectSourceDocument( &buffer, xmlFilePath );
        } else {
            // Read text from file, service provider document not loaded
            QFile file( xmlFilePath );
            return readProjectSourceDocument( &file, xmlFilePath );
        }
    };

    // Read project source XML document from file
    bool readProjectSourceDocument( const QString &fileName )
    {
        QFile file( fileName );
        return readProjectSourceDocument( &file, fileName );
    };

    // Read service provider plugin XML document from @p device, set file name to @p fileName
    bool readProjectSourceDocument( QIODevice *device, const QString &fileName )
    {
        Q_Q( Project );

        // Recreate service provider from the contents of device
        delete provider;
        provider = 0;
        xmlComments.clear();

        ServiceProviderDataReader reader;
        ServiceProviderData *readData = reader.read(
                device, fileName, ServiceProviderDataReader::ReadErrorneousFiles, q, &xmlComments );
        if ( readData ) {
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
            if ( readData->type() == Enums::ScriptedProvider ) {
                provider = new ServiceProviderScript( readData, q );
            } else
#endif
            {
                // Do not create sub class instance for unknown types
                provider = new ServiceProvider( readData, q );
            }
        } else {
            kDebug() << "Service provider plugin is invalid" << reader.errorString() << fileName;
            errorHappened( Project::ErrorWhileLoadingProject, reader.errorString() );
            insertProjectSourceTemplate();
            return false;
        }

        if ( provider /*&& provider->type() != InvalidProvider*/ ) {
            q->emit nameChanged( projectName() );
            q->emit iconNameChanged( iconName() );
            q->emit iconChanged( projectIcon() );
            q->emit dataChanged( data() );
            return true;
        } else {
            kDebug() << "Service provider plugin has invalid type" << fileName;
            errorHappened( Project::ErrorWhileLoadingProject,
                           i18nc("@info", "The provider plugin file <filename>%1</filename> "
                                 "has an invalid type.", fileName) );
            insertProjectSourceTemplate();
            return false;
        }
    };

    // Write service provider plugin XML document to @p fileName
    bool writeProjectSourceDocument( const QString &fileName )
    {
        if ( !provider ) {
            kDebug() << "No service provider loaded";
            return false;
        }

        ServiceProviderDataWriter writer;
        QFile file( fileName );
        return writer.write( &file, provider, xmlComments );
    };

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    // Load the script into the script tab,
    // if no script has been created yet the given @p templateType gets inserted
    bool loadScript( Project::ScriptTemplateType templateType = Project::DefaultScriptTemplate )
    {
        Q_Q( Project );
        if ( !scriptTab ) {
            // No script tab opened
            return true;
        }

        scriptTab->document()->closeUrl( false );
        scriptTab->document()->setModified( false );

        const QString scriptFile = provider->data()->scriptFileName();
        if ( scriptFile.isEmpty() ) {
            insertScriptTemplate( templateType );
            scriptTab->document()->setReadWrite( true );
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
#endif

    // Set the contents of the service provider plugin XML document to @p text
    // in the project source document tab
    bool setProjectSourceDocumentText( const QString &text )
    {
        if ( !projectSourceTab ) {
            kDebug() << "No project source tab opened";
            return true;
        }

        projectSourceTab->document()->closeUrl( false );
        projectSourceTab->document()->setModified( false );

        if ( text.isEmpty() ) {
            insertProjectSourceTemplate();
            return false;
        } else {
            // Open file if already stored to have the correct url set in KTextEditor::Document
            if ( !filePath.isEmpty() &&
                !projectSourceTab->document()->openUrl(KUrl(filePath)) )
            {
                errorHappened( Project::ProjectFileNotReadable,
                               i18nc("@info", "Could not open project source document "
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
                q->emit dataChanged( data() );
            }
        }
    };

    void insertProjectSourceTemplate()
    {
        Q_Q( Project );
        delete provider;
        provider = ServiceProvider::createInvalidProvider( q );
        xmlComments.clear();
        q->emit nameChanged( projectName() );
        q->emit iconNameChanged( iconName() );
        q->emit iconChanged( projectIcon() );
        q->emit dataChanged( data() );
    };

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
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
#endif

    void errorHappened( Project::Error error, const QString &errorString )
    {
        Q_Q( Project );
        if ( !errorString.isEmpty() ) {
            // Store last error message
            lastError = errorString;
        }
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
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        case Project::ShowScript:
        case Project::ShowExternalScript:
#endif
        case Project::ShowProjectSource:
        case Project::ShowPlasmaPreview:
            // Always enabled actions
            return true;

        case Project::Save:
            // Enable save action only when the project is modified
            return isModified();

        case Project::ShowHomepage:
            // Only enable "Open Homepage" action if an URL is available
            return !provider->data()->url().isEmpty();

        case Project::SetAsActiveProject:
            // Only enable "Set as Active Project" action if the project isn't already active
            return !isActiveProject();

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
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
            return debugger->state() != Running &&
                    debugger->scriptState() == Debugger::Debugger::ScriptLoaded;

        case Project::AbortDebugger:
            // Only enabled if the debugger is running or interrupted
            return debugger->state() != NotRunning;

        case Project::ToggleBreakpoint:
            // Only enabled if a script tab is opened
            return scriptTab;

        case Project::RemoveAllBreakpoints:
            // Only enabled if the breakpoint model isn't empty
            return debugger->breakpointModel()->rowCount() > 0;
#endif

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
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        case Project::RunMenuAction:
        case Project::RunGetTimetable:
        case Project::RunGetStopSuggestions:
        case Project::RunGetStopsByGeoPosition:
        case Project::RunGetJourneys:
        case Project::DebugMenuAction:
        case Project::DebugGetTimetable:
        case Project::DebugGetStopSuggestions:
        case Project::DebugGetStopsByGeoPosition:
        case Project::DebugGetJourneys:
            // Only enabled if the debugger and the test are both currently not running
            return !isTestRunning() && !isDebuggerRunning() &&
                    debugger->scriptState() == Debugger::Debugger::ScriptLoaded;
#else
            // Only enabled if the debugger and the test are both currently not running
            return !isTestRunning() && !isDebuggerRunning();
#endif

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
                q->connect( action, SIGNAL(triggered(bool)), receiver, slot,
                            flags.testFlag(UseQueuedConnection)
                            ? Qt::QueuedConnection : Qt::AutoConnection );
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
            QList< QPointer<QAction> > actions = externProjectActions.values( actionType );
            foreach ( QAction *action, actions ) {
                if ( action ) {
                    action->setEnabled( enabled );
                }
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

    // Asks if the project should be activated (if disableQuestions is false)
    bool askForProjectActivation( ProjectActivationReason reason )
    {
        Q_Q( Project );
        if ( isActiveProject() || !enableQuestions ) {
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
                            "only show data for the active project. Toolbar/menu actions only "
                            "control the active project, but the project context menu can be used "
                            "to eg. control the debugger of an inactive project.<nl />"
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

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    // Call script function @p functionName using @p request in the given @p debugMode
    void callScriptFunction( AbstractRequest *request,
                             Debugger::DebugFlags debugFlags = Debugger::InterruptOnExceptions )
    {
        if ( askForProjectActivation(ActivateProjectForDebugging) ) {
            debugger->requestTimetableData( request, QString(), debugFlags );
        }
    };

    // Call script function getTimetable() in the given @p debugMode
    void callGetTimetable( Debugger::DebugFlags debugFlags = Debugger::InterruptOnExceptions )
    {
        Q_Q( Project );
        bool cancelled = false;
        DepartureRequest request = q->getDepartureRequest( parentWidget(), &cancelled );
        if ( !cancelled ) {
            callScriptFunction( &request, debugFlags );
        }
    };

    // Call script function getStopSuggestions() in the given @p debugMode
    void callGetStopSuggestions( Debugger::DebugFlags debugFlags = Debugger::InterruptOnExceptions )
    {
        Q_Q( Project );
        bool cancelled = false;
        StopSuggestionRequest request = q->getStopSuggestionRequest( parentWidget(), &cancelled );
        if ( !cancelled ) {
            callScriptFunction( &request, debugFlags );
        }
    };

    // Call script function getStopSuggestions() in the given @p debugMode with a geo position
    void callGetStopsByGeoPosition(
            Debugger::DebugFlags debugFlags = Debugger::InterruptOnExceptions )
    {
        Q_Q( Project );
        bool cancelled = false;
        StopsByGeoPositionRequest request =
                q->getStopsByGeoPositionRequest( parentWidget(), &cancelled );
        if ( !cancelled ) {
            callScriptFunction( &request, debugFlags );
        }
    };

    // Call script function getJourneys() in the given @p debugMode
    void callGetJourneys( Debugger::DebugFlags debugFlags = Debugger::InterruptOnExceptions )
    {
        Q_Q( Project );
        bool cancelled = false;
        JourneyRequest request = q->getJourneyRequest( parentWidget(), &cancelled );
        if ( !cancelled ) {
            callScriptFunction( &request, debugFlags );
        }
    };
#endif // BUILD_PROVIDER_TYPE_SCRIPT

    // Called before testing starts
    bool beginTesting( const QList< TestModel::Test > &tests )
    {
        Q_Q( Project );
        if ( isTestRunning() ) {
            // Test is already running
            kWarning() << "Test is already running" << data()->id();
            return true;
        }

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        if ( !checkSyntax(q->scriptText()) ) {
            // Do not start the test if the syntax is invalid
            // TODO extra test for the syntax check? enough to test in LoadScriptJob?
            return false;
        }

        DEBUGGER_JOB_SYNCHRONIZATION("Testing begins" << data()->id());
#endif

        pendingTests.clear();
        testState = TestsRunning;
        finishedTests.clear();
        startedTests = tests;
        updateProjectActions( QList<Project::ProjectActionGroup>() << Project::TestActionGroup
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
                                                                   << Project::RunActionGroup,
                              QList<Project::ProjectAction>() << Project::RunToCursor
#endif
                            );

        // Disable inforamtion messages while testing, results are shown in the test tab
        enableDebuggerInformationMessages( false );

        q->emit testStarted();
        q->emit testRunningChanged( true );
        q->emit informationMessage( i18nc("@info", "Test started") );
        return true;
    };

    // Called after testing has ended
    void endTesting()
    {
        Q_Q( Project );
        if ( !isTestRunning() ) {
            return;
        }

        const TestModel::TestState state = testModel->completeState();
        DEBUGGER_JOB_SYNCHRONIZATION("Testing finished" << data()->id());
        pendingTests.clear();
        testState = NoTestRunning;
        updateProjectActions( QList<Project::ProjectActionGroup>() << Project::TestActionGroup
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
                                                                   << Project::RunActionGroup,
                              QList<Project::ProjectAction>() << Project::RunToCursor
#endif
                            );

        // Re-enable information messages from the debugger
        enableDebuggerInformationMessages();

        switch ( state ) {
        case TestModel::TestFinishedSuccessfully:
            q->emit informationMessage( i18nc("@info", "Test of %1 finished successfully",
                                              data()->id()),
                    KMessageWidget::Positive, 4000,
                    QList<QAction*>() << q->projectAction(Project::ShowPlasmaPreview) );
            break;
        case TestModel::TestFinishedWithErrors:
            q->emit informationMessage( i18nc("@info", "Test of %1 finished with errors",
                                              data()->id()),
                                        KMessageWidget::Error, 4000 );
            break;
        case TestModel::TestFinishedWithWarnings:
            q->emit informationMessage( i18nc("@info", "Test of %1 finished with warnings",
                                              data()->id()),
                                        KMessageWidget::Warning, 4000 );
            break;
        case TestModel::TestCouldNotBeStarted:
            q->emit informationMessage( i18nc("@info", "Test of %1 could not be started",
                                              data()->id()),
                                        KMessageWidget::Error, 4000 );
            break;
        case TestModel::TestAborted:
            q->emit informationMessage( i18nc("@info", "Test of %1 was aborted", data()->id()),
                                        KMessageWidget::Error, 4000 );
            break;
        case TestModel::TestCaseNotFinished:
            q->emit informationMessage( i18nc("@info", "Test of %1 case not finished",
                                              data()->id()),
                                        KMessageWidget::Information, 4000 );
            break;
        default:
            kWarning() << "Unexpected test state" << state;
            break;
        }
        q->emit testRunningChanged( false );
        q->emit testFinished( state == TestModel::TestFinishedSuccessfully );

        finishedTests.clear();
        startedTests.clear();
    };

    // Cancels all running/pending tests
    void abortTests()
    {
        Q_Q( Project );
        if ( !isTestRunning() ) {
            return;
        }

        testState = TestsGetAborted;
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        DEBUGGER_JOB_SYNCHRONIZATION("Abort tests" << data()->id());
        for ( QHash<TestModel::Test, ThreadWeaver::Job*>::ConstIterator it = pendingTests.constBegin();
              it != pendingTests.constEnd(); ++it )
        {
            if ( !debugger->weaver()->dequeue(*it) ) {
                (*it)->requestAbort();
            }
        }

        debugger->abortDebugger();
        debugger->finish();
#endif
        QList< TestModel::Test >::ConstIterator it = dependendTests.constBegin();
        while ( it != dependendTests.constEnd() ) {
            testModel->setTestState( *it,
                    TestModel::TestAborted,
                    i18nc("@info/plain", "Test was aborted"),
                    i18nc("@info", "<title>Test was aborted</title> "
                        "<para>The test was aborted while it was running, no results available.</para>"),
                    q->projectAction(Project::ShowProjectSettings) );
            ++it;
        }
        dependendTests.clear();

        endTesting();
    };

    bool testForCoordinatesSampleData()
    {
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        Q_Q( Project );
        const ServiceProviderData *data = provider->data();
        if ( !data->hasSampleCoordinates() ) {
            testModel->setTestState( TestModel::StopsByGeoPositionTest,
                    TestModel::TestCouldNotBeStarted,
                    i18nc("@info/plain", "Missing sample coordinates"),
                    i18nc("@info", "<title>Missing sample stop coordinates</title> "
                        "<para>Cannot run script execution tests for stops by geo "
                        "position. Open the project settings and add one or more "
                        "<interface>Sample Stop Coordinates</interface></para>"),
                    q->projectAction(Project::ShowProjectSettings) );
            testFinished( TestModel::StopsByGeoPositionTest );
            return false;
        }
#endif

        return true;
    };

    bool testForSampleData()
    {
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        Q_Q( Project );
        const ServiceProviderData *data = provider->data();
        if ( data->sampleStopNames().isEmpty() ) {
            testModel->markTestCaseAsUnstartable( TestModel::ScriptExecutionTestCase,
                    i18nc("@info/plain", "Missing sample stop name"),
                    i18nc("@info", "<title>Missing sample stop name</title> "
                        "<para>Cannot run script execution tests. Open the project settings and add "
                        "one or more <interface>Sample Stop Names</interface></para>"),
                    q->projectAction(Project::ShowProjectSettings) );
            return false;
        } else if ( data->useSeparateCityValue() && data->sampleCity().isEmpty() ) {
            testModel->markTestCaseAsUnstartable( TestModel::ScriptExecutionTestCase,
                    i18nc("@info/plain", "Missing sample city"),
                    i18nc("@info", "<title>Missing sample city</title> "
                        "<para>Cannot run script execution tests. Open the project settings and add "
                        "a <interface>Sample City</interface></para>"),
                    q->projectAction(Project::ShowProjectSettings) );
            return false;
        }
#endif

        return true;
    };

    bool testForJourneySampleData()
    {
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        Q_Q( Project );
        const ServiceProviderData *data = provider->data();
        if ( data->sampleStopNames().count() < 2 ) {
            testModel->setTestState( TestModel::JourneyTest, TestModel::TestCouldNotBeStarted,
                    i18nc("@info/plain", "To test journeys at least two sample stop names are needed"),
                    i18nc("@info", "<title>To test journeys at least two sample stop names are needed</title> "
                        "<para>Cannot run journey test. Open the project settings and add "
                        "another stop name to the <interface>Sample Stop Names</interface></para>"),
                    q->projectAction(Project::ShowProjectSettings) );
            testFinished( TestModel::JourneyTest );
            return false;
        }
#endif

        return true;
    };

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    bool startScriptExecutionTest( TestModel::Test test )
    {
        Q_Q( Project );

        const QList< TestModel::Test > requiredTests = TestModel::testIsDependedOf( test );
        foreach ( TestModel::Test requiredTest, requiredTests ) {
            if ( !testModel->isTestFinished(requiredTest) ) {
                // A required test is not finished, add it to the dependend test list
                // and start it when all required tests are done
                dependendTests << test;

                // Required test is not finished
                testModel->setTestState( test, TestModel::TestDelegated,
                        i18nc("@info/plain", "Waiting for required test \"%1\"",
                              TestModel::nameForTest(requiredTest)),
                        QString(), q->projectAction(Project::ShowScript) );
                return true;
            } else if ( testModel->testState(requiredTest) == TestModel::TestFinishedWithErrors ) {
                testModel->setTestState( test, TestModel::TestCouldNotBeStarted,
                        i18nc("@info/plain", "Required test \"%1\" was not successful",
                              TestModel::nameForTest(requiredTest)),
                        i18nc("@info", "<title>Dependency not met</title> "
                              "<para>This test depends on the \"%1\" test, but it was not "
                              "successful.</para>", TestModel::nameForTest(requiredTest)),
                        q->projectAction(Project::ShowScript) );
                testFinished( test );
                return false;
            }
        }

        // Test if enough sample data is available
        // and get the name of the script function to run
        QString function, message, shortMessage;
        switch ( test ) {
        case TestModel::DepartureTest:
        case TestModel::ArrivalTest:
            if ( !testForSampleData() ) {
                return false;
            }
            function = ServiceProviderScript::SCRIPT_FUNCTION_GETTIMETABLE;
            shortMessage = i18nc("@info/plain", "You need to implement a '%1' script function", function);
            message = i18nc("@info", "<title>You need to implement a '%1' script function</title> "
                            "<para>Service provider plugins that only support journeys are "
                            "currently not accepted by the data engine, but that may change."
                            "</para>", function);
            break;
        case TestModel::AdditionalDataTest:
            if ( !testForSampleData() ) {
                return false;
            }
            function = ServiceProviderScript::SCRIPT_FUNCTION_GETADDITIONALDATA;
            shortMessage = i18nc("@info/plain", "'%1' script function not implemented", function);
            message = i18nc("@info", "<title>You can implement a '%1' script function</title> "
                            "<para>This can be used to load additional data for single departures "
                            "or arrivals.</para>", function);
            break;
        case TestModel::StopSuggestionTest:
        case TestModel::StopsByGeoPositionTest:
            if ( test == TestModel::StopSuggestionTest ? !testForSampleData()
                                                       : !testForCoordinatesSampleData() )
            {
                return false;
            }
            function = ServiceProviderScript::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS;
            shortMessage = i18nc("@info/plain", "You need to implement a '%1' script function", function);
            message = i18nc("@info", "<title>You need to implement a '%1' script function</title> "
                            "<para>Without stop suggestions it can be very hard for users to find a "
                            "valid stop name. Therefore this function is needed.</para>", function);
            break;
        case TestModel::JourneyTest:
            if ( !testForJourneySampleData() ) {
                return false;
            }
            function = ServiceProviderScript::SCRIPT_FUNCTION_GETJOURNEYS;
            shortMessage = i18nc("@info/plain", "For journeys, you need to implement a '%1' script function", function);
            message = i18nc("@info", "<title>For journeys, you need to implement a '%1' script function</title> "
                            "<para>If you do not implement the function, journeys will not work with "
                            "the plugin.</para>", function);
            break;
        case TestModel::FeaturesTest:
            function = ServiceProviderScript::SCRIPT_FUNCTION_FEATURES;
            shortMessage = i18nc("@info/plain", "You should implement a '%1' script function", function);
            message = i18nc("@info", "<title>You should implement a '%1' script function</title> "
                            "<para>This function is used to know what information the plugin parses "
                            "from documents. Without adding the appropriate TimetableInformation names "
                            "to the return value of this function, the associated data will be unused "
                            "or associated features will be disabled.</para>"
                            "<para>If, for example, the script can parse Arrivals, 'Arrivals' should "
                            "be added to the return value. If the script parses route stops or "
                            "stop IDs, add 'RouteStops' or 'StopID' to the return value, etc.</para>",
                            function);
            break;
        case TestModel::LoadScriptTest:
            break;
        default:
            kWarning() << "Invalid test" << test;
            return false;
        }

        // Check if the function that should be run is implemented in the script
        const QStringList functions = q->scriptFunctions();
        const bool hasRequiredFunction = function.isEmpty() || functions.contains(function);
        if ( !hasRequiredFunction ) {
            // Function is not implemented
            TestModel::TestState state = test == TestModel::DepartureTest
                    ? TestModel::TestCouldNotBeStarted : TestModel::TestDisabled;
            testModel->setTestState( test, state, shortMessage, message,
                                     q->projectAction(Project::ShowScript) );
            testFinished( test );
            return false;
        } else {
            // Create job
            DebuggerJob *job;
            if ( test == TestModel::LoadScriptTest ) {
                job = debugger->getLoadScriptJob( q->scriptText(), data() );
                if ( !job ) {
                    // Script already loaded and not changed
                    testModel->setTestState( test, TestModel::TestFinishedSuccessfully,
                                             i18nc("@info/plain", "Script successfully loaded") );
                    testFinished( test );
                    startTests( takeStartableDependentTests(test) );
                    return true;
                } else if ( debugger->isLoadScriptJobRunning() && !job->isFinished() ) {
                    job->setObjectName( "TEST_LOAD" );

                    // The started signal of the LoadScriptJob was already sent, update test model
                    pendingTests.insert( test, job );
                    job->setUseCase( TestModel::nameForTest(test) );
                    q->testJobStarted( test, job->type(), job->useCase() );
                    return true;
                } else {
                    job->setObjectName( "TEST_LOAD" );
                }
            } else if ( test == TestModel::FeaturesTest ) {
                job = debugger->createTestFeaturesJob( QString(), NeverInterrupt );
                job->setObjectName( "TEST_FEATURES" );
            } else {
                // The number of items to request for testing, lower values mean higher performance,
                // higher values can mean better test results, eg. showing rare errors
                const int testItemCount = 30;

                // Create request object
                AbstractRequest *request = 0;
                switch ( test ) {
                case TestModel::DepartureTest:
                    request = new DepartureRequest( "TEST_DEPARTURES",
                            data()->sampleStopNames().first(), QDateTime::currentDateTime(),
                            testItemCount, data()->sampleCity() );
                    break;
                case TestModel::ArrivalTest: {
                    if ( !isTestFinishedOrPending(TestModel::FeaturesTest) ) {
                        kWarning() << "First start the features test";
                        return false;
                    }
                    if ( !hasFeature(test, Enums::ProvidesArrivals) ) {
                        return false;
                    }

                    request = new ArrivalRequest( "TEST_ARRIVALS",
                            data()->sampleStopNames().first(), QDateTime::currentDateTime(),
                            testItemCount, data()->sampleCity() );
                    break;
                }
                case TestModel::AdditionalDataTest: {
                    if ( !isTestFinishedOrPending(TestModel::DepartureTest) )
                    {
                        kWarning() << "First start the departure test";
                        return false;
                    }

                    const QList< TimetableData > results =
                            testModel->testResults( TestModel::DepartureTest );
                    if ( results.isEmpty() ) {
                        kWarning() << "No results in departure test";
                        return false;
                    }

                    QSharedPointer< AbstractRequest > departureRequest =
                            testModel->testRequest( TestModel::DepartureTest );
                    const TimetableData result = results.first();
                    request = new AdditionalDataRequest( "TEST_ADDITIONAL_DATA",
                            0, departureRequest->stop,
                            result[Enums::DepartureDateTime].toDateTime(),
                            result[Enums::TransportLine].toString(),
                            result[Enums::Target].toString(),
                            departureRequest->city, result[Enums::RouteDataUrl].toString() );
                }   break;
                case TestModel::StopSuggestionTest:
                    request = new StopSuggestionRequest( "TEST_STOP_SUGGESTIONS",
                            data()->sampleStopNames().first().left(4), testItemCount,
                            data()->sampleCity() );
                    break;
                case TestModel::StopsByGeoPositionTest:
                    if ( !isTestFinishedOrPending(TestModel::FeaturesTest) ) {
                        kWarning() << "First start the features test";
                        return false;
                    }
                    if ( !hasFeature(test, Enums::ProvidesStopsByGeoPosition) ) {
                        return false;
                    }

                    request = new StopsByGeoPositionRequest(
                            "TEST_STOP_SUGGESTIONS_BYGEOPOSITION",
                            data()->sampleLongitude(), data()->sampleLatitude(), testItemCount );
                    break;
                case TestModel::JourneyTest:
                    request = new JourneyRequest( "TEST_JOURNEYS",
                            data()->sampleStopNames().first(), data()->sampleStopNames()[1],
                            QDateTime::currentDateTime(), testItemCount, QString(),
                            data()->sampleCity() );
                    break;
                default:
                    kWarning() << "Invalid test" << test;
                    return false;
                }

                // Create job
                job = debugger->createTimetableDataRequestJob( request, QString(), NeverInterrupt );
                job->setObjectName( request->sourceName );
                delete request;
            }

            job->setUseCase( TestModel::nameForTest(test) );

            // Try to enqueue the job
            if ( !debugger->enqueueJob(job) ) {
                // The job could not be enqueued
                delete job;
                testModel->markTestCaseAsUnstartable( TestModel::ScriptExecutionTestCase );
                endTesting();
                return false;
            } else {
                // The job was successfully enqueued
                pendingTests.insert( test, job );
                return true;
            }
        }
    };
#endif // BUILD_PROVIDER_TYPE_SCRIPT

    void startTests( const QList< TestModel::Test > &tests )
    {
        foreach ( TestModel::Test test, tests ) {
            startTest( test );
            if ( testState == ProjectPrivate::TestsGetAborted ) {
                break;
            }
        }
    };

    bool startTest( TestModel::Test test )
    {
        bool finishedAfterThisTest = false;
        if ( !isTestRunning() ) {
            if ( !beginTesting(QList<TestModel::Test>() << test) ) {
                // Test could not be started
                return false;
            }

            // Test started, only running one test
            finishedAfterThisTest = true;
        }

        bool success;
        const TestModel::TestCase testCase = TestModel::testCaseOfTest( test );
        bool scriptExecutionTestCase = false;
        switch ( testCase ) {
        case TestModel::ServiceProviderDataTestCase: {
            testModel->markTestAsStarted( test );

            QString errorMessage, tooltip;
            success = ServiceProviderDataTester::runServiceProviderDataTest(
                    test, provider->data(), &errorMessage, &tooltip );
            testModel->setTestState( test, success ? TestModel::TestFinishedSuccessfully
                                                   : TestModel::TestFinishedWithErrors,
                                     errorMessage, tooltip/*, projectAction(ShowProjectSettings)*/ );
            testFinished( test );
        } break;

    #ifdef BUILD_PROVIDER_TYPE_SCRIPT
        case TestModel::ScriptExecutionTestCase:
            success = startScriptExecutionTest( test );
            scriptExecutionTestCase = true;
            break;
    #endif

        default:
            kWarning() << "Unknown test" << test;
            success = false;
            break;
        }

        if ( finishedAfterThisTest && !scriptExecutionTestCase ) {
            endTesting();
        } else if ( !success ) {
            // Test could not be started
            startTests( takeStartableDependentTests(test) );
        }

        return success;
    };

    bool save( QWidget *parent, const QString &xmlFilePath, bool useAsNewSavePath = true )
    {
        Q_Q( Project );
        parent = parentWidget( parent );
        const QString _filePath = xmlFilePath.isEmpty() ? filePath : xmlFilePath;
        if ( _filePath.isEmpty() ) {
            return q->saveAs( parent );
        }

        // Save the project
        kDebug() << "Save to" << _filePath;
        if ( !writeProjectSourceDocument(_filePath) ) {
            return false;
        }

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        QString scriptFile = provider->data()->scriptFileName();
        if ( !scriptFile.isEmpty() && isScriptModified() ) {
            const QString scriptFilePath =
                    QFileInfo(_filePath).absolutePath() + '/' + QFileInfo(scriptFile).fileName();
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

        foreach ( ScriptTab *tab, externalScriptTabs ) {
            tab->save();
        }
#endif

        if ( useAsNewSavePath ) {
            const bool wasModified = isModified();
            const bool wasProjectSourceModified = isProjectSourceModified();
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
            const bool wasScriptModified = isScriptModified();
            unsavedScriptContents.clear();
#endif

            projectSourceBufferModified = false;
            updateProjectActions( QList<Project::ProjectAction>() << Project::Save );
            setXmlFilePath( _filePath );

            if ( projectSourceTab ) {
                projectSourceTab->document()->setModified( false );
            }
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
            if ( scriptTab ) {
                scriptTab->document()->setModified( false );
            }
#endif
            if ( wasModified ) {
                q->emit modifiedStateChanged( false );
                if ( wasProjectSourceModified ) {
                    q->emit projectSourceModifiedStateChanged( false );
                }
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
                if ( wasScriptModified ) {
                    q->emit scriptModifiedStateChanged( false );
                }
#endif
            }
        }
        return true;
    };

    bool saveAs( QWidget *parent )
    {
        parent = parentWidget( parent );
        KFileDialog saveDialog( filePath.isEmpty() ? KGlobalSettings::documentPath() : filePath,
                                QString(), parent );
        saveDialog.setOperationMode( KFileDialog::Saving );
        saveDialog.setWindowTitle( i18nc("@title:window", "Save Project") );
        saveDialog.setMimeFilter( QStringList() << "application/x-publictransport-serviceprovider"
                                                << "application/xml",
                                  "application/x-publictransport-serviceprovider" );
        if ( saveDialog.exec() != KFileDialog::Accepted || saveDialog.selectedFile().isEmpty() ) {
            return false; // Cancel clicked
        }

        // Got a file name, save the project
        return save( parent, saveDialog.selectedFile() );
    };

    bool install( QWidget *parent, bool install, Project::InstallType installType )
    {
        Q_Q( Project );
        const QString xmlFileName = provider->data()->id() + ".pts";
        if ( installType == Project::LocalInstallation ) {
            // Local installation, find a writable location for Public Transport engine plugins
            const QString saveDir = KGlobal::dirs()->saveLocation( "data",
                    ServiceProviderGlobal::installationSubDirectory() );
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
            // Global insallation, find all directories for Public Transport engine plugins
            const QStringList saveDirs = KGlobal::dirs()->findDirs( "data",
                    ServiceProviderGlobal::installationSubDirectory() );
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
            args["filenameProvider"] = xmlFileName;
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
            args["filenameScript"] = provider->data()->scriptFileName();
#endif
            if ( install ) {
                args["contentsProvider"] = q->projectSourceText();
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
                args["contentsScript"] = q->scriptText();
#endif
            }
            action.setArguments( args );
            KAuth::ActionReply reply = action.execute();

            // Check if the installation was successful
            if ( reply.failed() ) {
                kDebug() << reply.type() << reply.data();
                kDebug() << reply.errorCode() << reply.errorDescription();
                if ( reply.type() == KAuth::ActionReply::HelperError ) {
                    KMessageBox::error( parent,
                            install ? i18nc("@info", "Service provider plugin could not be installed globally "
                                            "in <filename>%1</filename>: %2 <message>%3</message>",
                                            saveDir, reply.errorCode(), reply.errorDescription())
                                    : i18nc("@info", "Service provider plugin could not be uninstalled globally "
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
                q->emit informationMessage( i18nc("@info", "Service provider plugin successfully installed globally"),
                                            KMessageWidget::Positive );
                q->emit savePathInfoStringChanged( savePathInfoString() );
                q->emit globalInstallationStateChanged( true );
            } else {
                // Uninstallation successful
                q->emit informationMessage( i18nc("@info", "Service provider plugin successfully uninstalled globally"),
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

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    // Get the currently shown script tab, if any.
    // Otherwise any of the currently opened script tabs gets returned.
    ScriptTab *currentScriptTab()
    {
        // First try to find an active tab
        if ( scriptTab && scriptTab->isVisible() ) {
            return scriptTab;
        }
        foreach ( ScriptTab *tab, externalScriptTabs ) {
            if ( tab->isVisible() ) {
                return tab;
            }
        }

        // No active script tab, find any script tab
        if ( scriptTab ) {
            return scriptTab;
        } else if ( !externalScriptTabs.isEmpty() ) {
            return externalScriptTabs.first();
        } else {
            return 0;
        }
    };
#endif

    // Check if the features test result contains all given features,
    // and add error test results for the given test if a feature is not available
    bool hasFeatures( TestModel::Test test, const QList< Enums::ProviderFeature > &features ) {
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        Q_Q( Project );
        const QList< TimetableData > featuresResults =
                testModel->testResults( TestModel::FeaturesTest );
        if ( featuresResults.isEmpty() ) {
            kWarning() << "No results in features test";
            QStringList featureStrings;
            foreach ( Enums::ProviderFeature feature, features ) {
                featureStrings << Enums::toString( feature );
            }
            testModel->setTestState( test, TestModel::TestFinishedWithWarnings,
                    i18nc("@info/plain", "Feature function needed"),
                    i18nc("@info", "<title>Feature function needed</title> "
                    "<para>The <icode>features()</icode> function is needed to indicate support "
                    "for this test. It should return at least these features for this test to run: "
                    "%1.</para>", featureStrings.join(", ")),
                    q->projectAction(Project::ShowScript) );
            return false;
        }

        // Read features from the test result
        // and remove available features from the list of features to check
        QList< Enums::ProviderFeature > featuresToCheck = features;
        const QVariantList featuresList =
                featuresResults.first()[Enums::Nothing].toList();
        foreach ( const QVariant &featureVariant, featuresList ) {
            const Enums::ProviderFeature feature =
                    static_cast< Enums::ProviderFeature >( featureVariant.toInt() );
            if ( featuresToCheck.removeOne(feature) && featuresToCheck.isEmpty() ) {
                break;
            }
        }

        // Remaining features are not available
        if ( !featuresToCheck.isEmpty() ) {
            const Enums::ProviderFeature feature = featuresToCheck.first();
            testModel->setTestState( test, TestModel::TestDisabled,
                    i18nc("@info/plain", "Feature \"%1\" not supported", Enums::toString(feature)),
                    i18nc("@info", "<title>Feature \"%1\" not supported</title> "
                    "<para>The <icode>features()</icode> function did not return "
                    "<icode>PublicTransport.%1</icode>.</para>", Enums::toString(feature)),
                    q->projectAction(Project::ShowScript) );
            testFinished( test );
            return false;
        }

        return true;
#else
        return false;
#endif
    };

    inline bool hasFeature( TestModel::Test test, Enums::ProviderFeature feature ) {
        return hasFeatures( test, QList<Enums::ProviderFeature>() << feature );
    };

    bool isTestFinishedOrPending( TestModel::Test test ) const {
        return testModel->isTestFinished(test) || // Test is finished
               testModel->testState(test) == TestModel::TestIsRunning || // Test is running
               pendingTests.contains(test); // Test waits to be started
    };

    QList< TestModel::Test > takeStartableDependentTests(
            TestModel::Test finishedTest = TestModel::InvalidTest )
    {
        QList< TestModel::Test > tests;
        QList<TestModel::Test>::Iterator it = dependendTests.begin();
        while ( it != dependendTests.end() ) {
            const QList< TestModel::Test > requiredTests = TestModel::testIsDependedOf( *it );
            if ( requiredTests.contains(finishedTest) ) {
                // A required test was finished
                bool allFinished = true;
                foreach ( TestModel::Test requiredTest, requiredTests ) {
                    if ( !testModel->isTestFinished(requiredTest) && finishedTest != requiredTest ) {
                        // A required test in the list is not finished
                        // and it's not the job that has just finished
                        allFinished = false;
                        break;
                    }
                }
                if ( allFinished ) {
                    tests << *it;
                    it = dependendTests.erase( it );
                    continue;
                }
            }
            ++it;
        }
        if ( !tests.isEmpty() ) {
            DEBUGGER_JOB_SYNCHRONIZATION("All requirements for tests" << tests
                                         << "are finished with test" << finishedTest);
        }
        return tests;
    };

    void testFinished( TestModel::Test test )
    {
    #ifdef BUILD_PROVIDER_TYPE_SCRIPT
        Q_Q( Project );
        finishedTests << test;
        q->emit testProgress( finishedTests, startedTests );

        if ( !debugger->isRunning() && finishedTests.count() >= startedTests.count() &&
             pendingTests.isEmpty() && dependendTests.isEmpty() )
        {
            // The last pending test has finished
            DEBUGGER_JOB_SYNCHRONIZATION("The last pending test has finished");
            endTesting();
        }

        startTests( takeStartableDependentTests(test) );
    #endif
    };

    Project::State state;
    ProjectModel *projectModel;

    // This is needed to know when the project source was updated with new settings using
    // setProviderData() but no ProjectSourceTab is opened
    bool projectSourceBufferModified;

    QString filePath;
    QString serviceProviderID;

    DashboardTab *dashboardTab;
    ProjectSourceTab *projectSourceTab;
    PlasmaPreviewTab *plasmaPreviewTab;
    WebTab *webTab;
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    ScriptState scriptState;
    ScriptTab *scriptTab;
    QList< ScriptTab* > externalScriptTabs;

    QString unsavedScriptContents;
    Debugger::Debugger *debugger;
#endif

    ServiceProvider *provider;
    QString xmlComments;

    // Get created when needed, multi for actions of the same type with different data
    QMultiHash< Project::ProjectAction, QAction* > projectActions;

    // Store pointers to project actions, to update their enabled state on changes
    QMultiHash< Project::ProjectAction, QPointer<QAction> > externProjectActions;

    TestModel *testModel;
    TestState testState;
    QHash< TestModel::Test, ThreadWeaver::Job* > pendingTests;
    QList< TestModel::Test > dependendTests;
    QList< TestModel::Test > finishedTests;
    QList< TestModel::Test > startedTests;

    // Collects output/console text for the project
    QString output;
    QString consoleText;

    QString lastError;
    QStringList globalFunctions;
    QStringList includedFiles;
    bool suppressMessages;
    bool enableQuestions;

private:
    Project *q_ptr;
    Q_DECLARE_PUBLIC( Project )
};

Project::Project( const WeaverInterfacePointer &weaver, QWidget *parent )
        : QObject( parent ), d_ptr(new ProjectPrivate(weaver, this))
{
    qRegisterMetaType< ProjectActionData >( "ProjectActionData" );
    d_ptr->initialize();
}

Project::~Project()
{
    Q_D( Project );
    if ( isModified() ) {
        kWarning() << "Destroying project with modifications";
    }

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    disconnect( d->debugger, 0, this, 0 );
    d->debugger->weaver()->requestAbort();
    d->debugger->abortDebugger();
    d->debugger->finish();
    delete d_ptr;
#endif
}

bool Project::loadProject( const QString &projectSoureFile )
{
    Q_D( Project );
    if ( projectSoureFile.isEmpty() ) {
        d->insertProjectSourceTemplate();
        return true;
    } else {
        return d->loadProject( projectSoureFile );
    }
}

QString Project::output() const
{
    Q_D( const Project );
    return d->output;
}

void Project::clearOutput()
{
    Q_D( Project );
    d->output.clear();
    emit outputCleared();
    emit outputChanged();
}

void Project::appendOutput( const QString &output, const QColor &color )
{
    Q_D( Project );
    if ( output.isEmpty() ) {
        return;
    }
    if ( !d->output.isEmpty() ) {
        d->output.append( "<br />" );
    }

    const QString colorString = QString("rgb(%1,%2,%3)")
            .arg(color.red()).arg(color.green()).arg(color.blue());
    if ( color.isValid() ) {
        QString colorizedOutput = output;
        colorizedOutput.prepend("<span style='color:" + colorString + ";'>").append("</span>");
        d->output.append( colorizedOutput );
        emit outputAppended( colorizedOutput );
    } else {
        d->output.append( output );
        emit outputAppended( output );
    }
    emit outputChanged();
}

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
void Project::scriptOutput( const QString &message, const QScriptContextInfo &context )
{
    if ( context.fileName() != scriptFileName() ) {
        appendOutput( i18nc("@info %2 is the script file name",
                            "<emphasis strong='1'>Line %1 (%2):</emphasis> <message>%3</message>",
                            context.lineNumber(), QFileInfo(context.fileName()).fileName(),
                            message),
                      KColorScheme(QPalette::Active).foreground(KColorScheme::InactiveText).color() );
    } else {
        appendOutput( i18nc("@info", "<emphasis strong='1'>Line %1:</emphasis> <message>%2</message>",
                            context.lineNumber(), message),
                      KColorScheme(QPalette::Active).foreground(KColorScheme::InactiveText).color() );
    }
}

void Project::scriptMessageReceived( const QString &errorMessage,
                                     const QScriptContextInfo &context,
                                     const QString &failedParseText,
                                     Helper::ErrorSeverity severity )
{
    Q_UNUSED( failedParseText );
    QColor color;
    QString type;
    switch ( severity ) {
    case Helper::Warning:
        color = KColorScheme( QPalette::Active ).foreground( KColorScheme::NeutralText ).color();
        type = i18nc("@info/plain", "Warning");
        break;
    case Helper::Fatal:
        color = KColorScheme( QPalette::Active ).foreground( KColorScheme::NegativeText ).color();
        type = i18nc("@info/plain", "Error");
        break;
    case Helper::Information:
    default:
        type = i18nc("@info/plain", "Information");
        color = KColorScheme( QPalette::Active ).foreground( KColorScheme::InactiveText ).color();
        break;
    }
    if ( context.lineNumber() < 0 ) {
        // Received a message about repeated messages
        appendOutput( errorMessage, color );
    } else if ( !context.fileName().isEmpty() && context.fileName() != scriptFileName() ) {
        appendOutput( i18nc("@info %1 is the translation of 'Error'/'Warning'/'Information', "
                            "%3 is the script file name",
                            "<emphasis strong='1'>%1 in line %2 (%3):</emphasis> <message>%4</message>",
                            type, context.lineNumber(), QFileInfo(context.fileName()).fileName(),
                            errorMessage), color );
    } else {
        appendOutput( i18nc("@info %1 is the translation of 'Error'/'Warning'/'Information'",
                            "<emphasis strong='1'>%1 in line %2:</emphasis> <message>%3</message>",
                            type, context.lineNumber(), errorMessage), color );
    }
}
#endif

QString Project::consoleText() const
{
    Q_D( const Project );
    return d->consoleText;
}

void Project::clearConsoleText()
{
    Q_D( Project );
    d->consoleText.clear();
    emit consoleTextChanged( QString() );
}

void Project::appendToConsole( const QString &text )
{
    Q_D( Project );
    if ( text.isEmpty() ) {
        return;
    }
    if ( !d->consoleText.isEmpty() ) {
        d->consoleText.append( "<br />" );
    }
    d->consoleText.append( text );
    emit consoleTextChanged( d->consoleText );
}

Project::State Project::state() const
{
    Q_D( const Project );
    return d->state;
}

QString Project::lastError() const
{
    Q_D( const Project );
    return d->lastError;
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

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
ScriptTab *Project::scriptTab() const
{
    Q_D( const Project );
    return d->scriptTab;
}

ScriptTab *Project::scriptTab( const QString &filePath ) const
{
    Q_D( const Project );
    foreach ( ScriptTab *externalScriptTab, d->externalScriptTabs ) {
        if ( externalScriptTab->fileName() == filePath ) {
            return externalScriptTab;
        }
    }

    // External script is not opened in a tab
    return 0;
}

QList< ScriptTab * > Project::externalScriptTabs() const
{
    Q_D( const Project );
    return d->externalScriptTabs;
}

ScriptTab *Project::externalScriptTab( const QString &filePath ) const
{
    Q_D( const Project );
    foreach ( ScriptTab *tab, d->externalScriptTabs ) {
        if ( tab->fileName() == filePath ) {
            return tab;
        }
    }

    // No script tab with the given filePath found
    return 0;
}
#endif

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

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
Debugger::Debugger *Project::debugger() const
{
    Q_D( const Project );
    return d->debugger;
}
#endif

QString Project::path() const
{
    Q_D( const Project );
    return QFileInfo( d->filePath ).path();
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

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    scriptSaved();
#endif
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
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case ShowScript:
        return "project_show_script";
    case ShowExternalScript:
        return "project_show_external_script";
#endif
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

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
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
    case RunGetStopsByGeoPosition:
        return "run_stop_suggestions_geo_position";
    case RunGetJourneys:
        return "run_journeys";

    case DebugMenuAction:
        return "debug_menu_action";
    case DebugGetTimetable:
        return "debug_departures";
    case DebugGetStopSuggestions:
        return "debug_stop_suggestions";
    case DebugGetStopsByGeoPosition:
        return "debug_stops_by_geo_position";
    case DebugGetJourneys:
        return "debug_journeys";
#endif

    default:
        kWarning() << "Unknown project action" << actionType;
        return "";
    }
}

Project::ProjectActionData Project::projectActionData( QAction *action )
{
    return action->data().value< ProjectActionData >();
}

void Project::setProjectActionData( QAction *projectAction, const QVariant &data )
{
    ProjectActionData actionData = projectActionData( projectAction );
    actionData.data = data;
    projectAction->setData( QVariant::fromValue(actionData) );
    projectAction->setText( projectActionText(actionData.actionType, data) );
}

bool Project::isProjectAction( QAction *action )
{
    return projectActionData( action ).isValid();
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
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
            << debuggerSubMenuAction(parent)
#endif
            << testSubMenuAction(parent)
            << separator2
            << projectAction(ShowProjectSettings)
            << projectAction(Close);
    return actions;
}

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
QPointer< KActionMenu > Project::debuggerSubMenuAction( QWidget *parent )
{
    Q_D( Project );
    parent = d->parentWidget( parent );

    // Create a KActionMenu containing debug actions
    QPointer<KActionMenu> debuggerMenuAction( new KActionMenu(KIcon("debugger"),
                                              i18nc("@action", "Run"), parent) );
    debuggerMenuAction->setObjectName( "debuggerMenuAction" );
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
#endif

QPointer< KActionMenu > Project::testSubMenuAction( QWidget *parent )
{
    Q_D( Project );
    parent = d->parentWidget( parent );

    // Create a KActionMenu containing test actions
    QPointer<KActionMenu> testMenuAction( new KActionMenu(KIcon("task-complete"),
                                          i18nc("@action", "Test"), parent) );
    testMenuAction->setObjectName( "testMenuAction" );
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
    projectMenuAction->setObjectName( "projectMenuAction" );
    QList< QAction* > projectActions = contextMenuActions( parent );
    foreach ( QAction *projectAction, projectActions ) {
        projectMenuAction->addAction( projectAction );
    }
    return projectMenuAction;
}

void Project::testActionTriggered()
{
    QAction *action = qobject_cast< QAction* >( sender() );
    const ProjectActionData data = projectActionData( action );
    startTest( static_cast<TestModel::Test>(data.data.toInt()) );
}

void Project::testCaseActionTriggered()
{
    QAction *action = qobject_cast< QAction* >( sender() );
    const ProjectActionData data = projectActionData( action );
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

        d->updateProjectActions( QList<ProjectActionGroup>()
                << TestActionGroup << FileActionGroup << OtherActionGroup
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
                << RunActionGroup << DebuggerActionGroup
#endif
                );
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
            const ProjectActionData actionData = projectActionData( currentAction );
            if ( !actionData.data.isValid() && !data.isValid() ) {
                // No data wanted and an action without data was found
                action = currentAction;
                break;
            } else if ( actionData.data == data ) {
                // An action with the given data was found
                action = currentAction;
                break;
            }
        }
    }

    if ( !action ) {
        // Create and connect action and store it in d->projectActions
        action = createAndConnectProjectAction( actionType, data, this );
        d->projectActions.insert( actionType, action );
    }
    return action;
}

void Project::connectProjectAction( Project::ProjectAction actionType, QAction *action,
                                    bool doConnect, bool useQueuedConnection )
{
    Q_D( Project );
    ProjectPrivate::ConnectProjectActionFlags flags = useQueuedConnection
            ? ProjectPrivate::UseQueuedConnection : ProjectPrivate::NoConnectionFlags;
    switch ( actionType ) {
    case Save:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(save()), flags );
        if ( doConnect ) {
            connect( this, SIGNAL(modifiedStateChanged(bool)), action, SLOT(setEnabled(bool)) );
        } else {
            disconnect( this, SIGNAL(modifiedStateChanged(bool)), action, SLOT(setEnabled(bool)) );
        }
        break;
    case SaveAs:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(saveAs()), flags );
        break;
    case Install:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(installLocally()), flags );
        break;
    case Uninstall:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(uninstallLocally()), flags );
        break;
    case InstallGlobally:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(installGlobally()), flags );
        break;
    case UninstallGlobally:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(uninstallGlobally()), flags );
        break;
    case Close:
        d->connectProjectAction( actionType, action, doConnect, this, SIGNAL(closeRequest()), flags );
        break;
    case ShowProjectSettings:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(showSettingsDialog()), flags );
        break;
    case ShowDashboard:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(showDashboardTab()), flags );
        break;
    case ShowHomepage:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(showWebTab()), flags );
        break;
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case ShowScript:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(showScriptTab()), flags );
        break;
    case ShowExternalScript:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(showExternalScriptActionTriggered()), flags );
        break;
#endif
    case ShowProjectSource:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(showProjectSourceTab()), flags );
        break;
    case ShowPlasmaPreview:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(showPlasmaPreviewTab()), flags );
        break;

    case RunAllTests:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(testProject()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
    case AbortRunningTests:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(abortTests()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
    case ClearTestResults:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(clearTestResults()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
    case RunSpecificTest:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(testActionTriggered()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
    case RunSpecificTestCase:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(testCaseActionTriggered()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;

    case SetAsActiveProject:
        d->connectProjectAction( actionType, action, doConnect,
                                 this, SIGNAL(setAsActiveProjectRequest()), flags );
        if ( doConnect ) {
            connect( this, SIGNAL(activeProjectStateChanged(bool)), action, SLOT(setDisabled(bool)) );
            connect( this, SIGNAL(activeProjectStateChanged(bool)), action, SLOT(setChecked(bool)) );
            action->setChecked( isActiveProject() );
        } else {
            disconnect( this, SIGNAL(activeProjectStateChanged(bool)), action, SLOT(setDisabled(bool)) );
            disconnect( this, SIGNAL(activeProjectStateChanged(bool)), action, SLOT(setChecked(bool)) );
        }
        break;
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case StepInto:
        d->connectProjectAction( actionType, action, doConnect, d->debugger, SLOT(debugStepInto()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
    case StepOver:
        d->connectProjectAction( actionType, action, doConnect, d->debugger, SLOT(debugStepOver()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
    case StepOut:
        d->connectProjectAction( actionType, action, doConnect, d->debugger, SLOT(debugStepOut()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
    case Interrupt:
        d->connectProjectAction( actionType, action, doConnect, d->debugger, SLOT(debugInterrupt()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
    case Continue:
        d->connectProjectAction( actionType, action, doConnect, d->debugger, SLOT(debugContinue()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
    case AbortDebugger:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(abortDebugger()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
    case ToggleBreakpoint:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(toggleBreakpoint()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
    case RemoveAllBreakpoints:
        d->connectProjectAction( actionType, action, doConnect,
                                 d->debugger, SLOT(removeAllBreakpoints()), flags );
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
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
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
            connectProjectAction( projectActionData(action).actionType, action, doConnect );
        }
        d->connectProjectAction( actionType, action, doConnect, 0, "",
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
    } break;

    case RunGetTimetable:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(runGetTimetable()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
    case RunGetStopSuggestions:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(runGetStopSuggestions()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
    case RunGetStopsByGeoPosition:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(runGetStopsByGeoPosition()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
    case RunGetJourneys:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(runGetJourneys()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;

    case DebugGetTimetable:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(debugGetTimetable()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
    case DebugGetStopSuggestions:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(debugGetStopSuggestions()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
    case DebugGetStopsByGeoPosition:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(debugGetStopsByGeoPosition()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
    case DebugGetJourneys:
        d->connectProjectAction( actionType, action, doConnect, this, SLOT(debugGetJourneys()),
                                 flags | ProjectPrivate::AutoUpdateEnabledState );
        break;
#endif

    default:
        kWarning() << "Unknown project action" << actionType;
        return;
    }
}

QString Project::projectActionText( Project::ProjectAction actionType, const QVariant &data )
{
    switch ( actionType ) {
    case Save:
        return i18nc("@action", "Save Project");
    case SaveAs:
        return i18nc("@action", "Save Project As...");
    case Install:
        return i18nc("@action", "&Install");
    case Uninstall:
        return i18nc("@action", "&Uninstall");
    case InstallGlobally:
        return i18nc("@action", "Install &Globally");
    case UninstallGlobally:
        return i18nc("@action", "Uninstall &Globally");
    case Close:
        return i18nc("@action", "Close Project");
    case ShowProjectSettings:
        return i18nc("@action", "Project Settings...");
    case ShowDashboard:
        return i18nc("@action", "Show &Dashboard");
    case ShowHomepage:
        return i18nc("@action", "Show &Web Page");
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case ShowScript:
        return i18nc("@action", "Open &Script");
    case ShowExternalScript: {
        const QString filePath = data.toString();
        return filePath.isEmpty() ? i18nc("@action", "Open External Script...")
                                  : i18nc("@action", "Open External Script <filename>%1</filename>",
                                          QFileInfo(filePath).fileName());
    }
#endif
    case ShowProjectSource:
        return i18nc("@action", "Open Project &Source");
    case ShowPlasmaPreview:
        return i18nc("@action", "Show &Plasma Preview");

    case RunAllTests:
        return i18nc("@action", "&Run All Tests");
    case AbortRunningTests:
        return i18nc("@action", "&Abort Running Tests");
    case ClearTestResults:
        return i18nc("@action", "&Clear All Test Results");
    case RunSpecificTest: {
        TestModel::Test test = static_cast< TestModel::Test >( data.toInt() );
        if ( test == TestModel::InvalidTest ) {
            kWarning() << "No test specified for project action RunSpecificTest";
            return QString();
        }
        return i18nc("@action", "Run %1", TestModel::nameForTest(test));
    }
    case RunSpecificTestCase:
        return i18nc("@action", "&Run Complete Test Case");
    case SpecificTestCaseMenuAction: {
        TestModel::TestCase testCase = static_cast< TestModel::TestCase >( data.toInt() );
        if ( testCase == TestModel::InvalidTestCase ) {
            kWarning() << "No test case specified for project action SpecificTestCaseMenuAction";
            return QString();
        }

        // Create menu action
        return TestModel::nameForTestCase( testCase );
    }
    case SetAsActiveProject:
        return i18nc("@action", "Set as Active Project");

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case StepInto:
        return i18nc("@action", "Step &Into");
    case StepOver:
        return i18nc("@action", "Step &Over");
    case StepOut:
        return i18nc("@action", "Step Ou&t");
    case Interrupt:
        return i18nc("@action", "&Interrupt");
    case RunToCursor:
        return i18nc("@action", "Run to &Cursor");
    case Continue:
        return i18nc("@action", "&Continue");
    case AbortDebugger:
        return i18nc("@action", "&Abort Debugger");
    case ToggleBreakpoint:
        return i18nc("@action", "Toggle &Breakpoint");
    case RemoveAllBreakpoints:
        return i18nc("@action", "&Remove all Breakpoints");

    case RunMenuAction:
        return i18nc("@action", "&Run");
    case RunGetTimetable:
        return i18nc("@action", "Run get&Timetable()");
    case RunGetStopSuggestions:
        return i18nc("@action", "Run get&StopSuggestions()");
    case RunGetStopsByGeoPosition:
        return i18nc("@action", "Run get&StopSuggestions(), Geo Position");
    case RunGetJourneys:
        return i18nc("@action", "Run get&Journeys()");

    case DebugMenuAction:
        return i18nc("@action", "&Debug");
    case DebugGetTimetable:
        return i18nc("@action", "Debug get&Timetable()");
    case DebugGetStopSuggestions:
        return i18nc("@action", "Debug get&StopSuggestions()");
    case DebugGetStopsByGeoPosition:
        return i18nc("@action", "Debug get&StopSuggestions(), Geo Position");
    case DebugGetJourneys:
        return i18nc("@action", "Debug get&Journeys()");
#endif

    default:
        kDebug() << "Unknown project action" << actionType;
        return QString();
    }
}

QAction *Project::createProjectAction( Project::ProjectAction actionType, const QVariant &data,
                                       QObject *parent )
{
    KAction *action = 0;
    const QString text = projectActionText( actionType, data );
    switch ( actionType ) {
    case Save:
        action = new KAction( KIcon("document-save"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Save changes in the project") );
        action->setEnabled( false );
        break;
    case SaveAs:
        action = new KAction( KIcon("document-save-as"), text, parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Save changes in the project under a new file name") );
        break;
    case Install:
        action = new KAction( KIcon("run-build-install"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Install the project locally") );
        break;
    case Uninstall:
        action = new KAction( KIcon("edit-delete"), text, parent );
        action->setToolTip( i18nc("@info:tooltip",
                                  "Uninstall a locally installed version of the project") );
        break;
    case InstallGlobally:
        action = new KAction( KIcon("run-build-install-root"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Install the project globally") );
        break;
    case UninstallGlobally:
        action = new KAction( KIcon("edit-delete"), text, parent );
        action->setToolTip( i18nc("@info:tooltip",
                                  "Uninstall a globally installed version of the project") );
        break;
    case Close:
        action = new KAction( KIcon("project-development-close"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Close this project") );
        break;
    case ShowProjectSettings:
        action = new KAction( KIcon("configure"), text, parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Opens a dialog to modify the projects settings") );
        break;
    case ShowDashboard:
        action = new KAction( KIcon("dashboard-show"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Shows the dashboard tab of the project.") );
        break;
    case ShowHomepage:
        action = new KAction( KIcon("document-open-remote"), text, parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Opens the <emphasis>home page</emphasis> of the service provider in a tab.") );
        break;
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case ShowScript:
        action = new KAction( KIcon("document-open"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Opens the main <emphasis>script</emphasis> in a tab.") );
        break;
    case ShowExternalScript:
        action = new KAction( KIcon("document-open"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Opens an external <emphasis>script</emphasis> in a tab.") );
        break;
#endif
    case ShowProjectSource:
        action = new KAction( KIcon("document-open"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Opens the <emphasis>project source</emphasis> "
                                  "document in a tab.") );
        break;
    case ShowPlasmaPreview:
        action = new KAction( KIcon("plasma"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Opens the project in a PublicTransport applet "
                                  "in a <emphasis>Plasma preview</emphasis> tab.") );
        break;

    case RunAllTests:
        action = new KAction( KIcon("task-complete"), text, parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Runs all tests for the active project, eg. syntax errors, correct results.") );
        break;
    case AbortRunningTests:
        action = new KAction( KIcon("dialog-cancel"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Aborts all currently running tests.") );
        break;
    case ClearTestResults:
        action = new KAction( KIcon("edit-clear"), text, parent );
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
        action = new KAction( KIcon("edit-select"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Use this project as the active project") );
        action->setCheckable( true );
        action->setEnabled( false );
        break;

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case StepInto:
        action = new KAction( KIcon("debug-step-into"), text, parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Continue script execution until the next statement") );
        action->setEnabled( false );
        action->setPriority( QAction::LowPriority );
        break;
    case StepOver:
        action = new KAction( KIcon("debug-step-over"), text, parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Continue script execution until the next statement in the same context.") );
        action->setEnabled( false );
        action->setPriority( QAction::LowPriority );
        break;
    case StepOut:
        action = new KAction( KIcon("debug-step-out"), text, parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Continue script execution until the current function gets left.") );
        action->setEnabled( false );
        action->setPriority( QAction::LowPriority );
        break;
    case Interrupt:
        action = new KAction( KIcon("media-playback-pause"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Interrupt script execution.") );
        action->setEnabled( false );
        action->setEnabled( false );
        action->setPriority( QAction::LowPriority );
        break;
    case RunToCursor:
        action = new KAction( KIcon("debug-execute-to-cursor"), text, parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Continue script execution until the current cursor position is reached") );
        action->setEnabled( false );
        action->setPriority( QAction::LowPriority );
        break;
    case Continue:
        action = new KAction( KIcon("media-playback-start"), text, parent );
        action->setToolTip( i18nc("@info:tooltip",
                "Continue script execution, only interrupt on breakpoints or uncaught exceptions.") );
        action->setEnabled( false );
        action->setPriority( QAction::LowPriority );
        break;
    case AbortDebugger:
        action = new KAction( KIcon("process-stop"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Abort script execution") );
        action->setEnabled( false );
        action->setPriority( QAction::LowPriority );
        break;
    case ToggleBreakpoint:
        action = new KAction( KIcon("tools-report-bug"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Toggle breakpoint for the current line") );
        action->setEnabled( false );
        break;
    case RemoveAllBreakpoints:
        action = new KAction( KIcon("tools-report-bug"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Removes all breakpoints") );
        action->setEnabled( false );
        break;

    case RunMenuAction: {
        KActionMenu *debugScript = new KActionMenu( KIcon("system-run"), text, parent );
        debugScript->setToolTip( i18nc("@info:tooltip", "Runs a function of the script.") );
        debugScript->setDelayed( false );
        debugScript->addAction( createProjectAction(Project::RunGetTimetable, parent) );
        debugScript->addAction( createProjectAction(Project::RunGetStopSuggestions, parent) );
        debugScript->addAction( createProjectAction(Project::RunGetStopsByGeoPosition, parent) );
        debugScript->addAction( createProjectAction(Project::RunGetJourneys, parent) );
        action = debugScript;
    } break;
    case RunGetTimetable:
        action = new KAction( KIcon("system-run"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Runs the script function 'getTimetable()'") );
        action->setEnabled( false );
        break;
    case RunGetStopSuggestions:
        action = new KAction( KIcon("system-run"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Runs the script function 'getStopSuggestions()'") );
        action->setEnabled( false );
        break;
    case RunGetStopsByGeoPosition:
        action = new KAction( KIcon("system-run"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Runs the script function 'getStopSuggestions()' "
                                                   "with a geo position as argument") );
        action->setEnabled( false );
        break;
    case RunGetJourneys:
        action = new KAction( KIcon("system-run"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Runs the script function 'getJourneys()'") );
        action->setEnabled( false );
        break;

    case DebugMenuAction: {
        KActionMenu *debugScript = new KActionMenu( KIcon("debug-run"), text, parent );
        debugScript->setToolTip( i18nc("@info:tooltip", "Runs a function of the script in a debugger.") );
        debugScript->setDelayed( false );
        debugScript->addAction( createProjectAction(Project::DebugGetTimetable, parent) );
        debugScript->addAction( createProjectAction(Project::DebugGetStopSuggestions, parent) );
        debugScript->addAction( createProjectAction(Project::DebugGetStopsByGeoPosition, parent) );
        debugScript->addAction( createProjectAction(Project::DebugGetJourneys, parent) );
        action = debugScript;
    } break;
    case DebugGetTimetable:
        action = new KAction( KIcon("debug-run"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Runs the script function 'getTimetable()' "
                                                   "in a debugger") );
        action->setEnabled( false );
        break;
    case DebugGetStopSuggestions:
        action = new KAction( KIcon("debug-run"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Runs the script function 'getStopSuggestions()' "
                                                   "in a debugger") );
        action->setEnabled( false );
        break;
    case DebugGetStopsByGeoPosition:
        action = new KAction( KIcon("debug-run"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Runs the script function 'getStopSuggestions()' "
                                                   "in a debugger with a geo position as argument") );
        action->setEnabled( false );
        break;
    case DebugGetJourneys:
        action = new KAction( KIcon("debug-run"), text, parent );
        action->setToolTip( i18nc("@info:tooltip", "Runs the script function 'getJourneys()' "
                                                   "in a debugger") );
        action->setEnabled( false );
        break;
#endif

    default:
        kDebug() << "Unknown project action" << actionType;
        break;
    }

    // Store action type
    action->setData( QVariant::fromValue(ProjectActionData(actionType, data)) );
    return action;
}

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
void Project::showScriptLineNumber( int lineNumber )
{
    Q_D( Project );
    if ( lineNumber < 0 ) {
        return;
    }

    showScriptTab();
    d->scriptTab->document()->views().first()->setCursorPosition(
            KTextEditor::Cursor(lineNumber - 1, 0) );
}
#endif

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

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
ScriptTab *Project::showScriptTab( QWidget *parent )
{
    Q_D( Project );
    if ( d->scriptTab ) {
        emit tabGoToRequest( d->scriptTab );
    } else {
        d->scriptTab = createScriptTab( d->parentWidget(parent) );
        if ( d->scriptTab ) {
            connect( d->scriptTab->document(), SIGNAL(documentSavedOrUploaded(KTextEditor::Document*,bool)),
                     this, SLOT(scriptSaved()) );
            emit tabOpenRequest( d->scriptTab );
        }
    }
    return d->scriptTab;
}

ScriptTab *Project::showExternalScriptTab( const QString &_filePath, QWidget *parent )
{
    Q_D( Project );
    QString filePath = _filePath;
    if ( filePath.isEmpty() ) {
        // Get external script file name (from the same directory)
        QScopedPointer<KFileDialog> dialog( new KFileDialog(path(), QString(), parent) );
        dialog->setMimeFilter( QStringList() << "application/javascript" );
        if ( dialog->exec() == KFileDialog::Accepted ) {
            filePath = dialog->selectedFile();
        } else {
            return 0;
        }
    } else if ( !filePath.contains('/') ) {
        filePath.prepend( path() + '/' );
    }

    ScriptTab *tab = externalScriptTab( filePath );
    if ( tab ) {
        emit tabGoToRequest( tab );
    } else {
        tab = createExternalScriptTab( filePath, d->parentWidget(parent) );
        if ( tab ) {
            connect( tab->document(), SIGNAL(documentSavedOrUploaded(KTextEditor::Document*,bool)),
                     this, SLOT(scriptSaved()) );
            d->externalScriptTabs << tab;
            emit tabOpenRequest( tab );
        }
    }
    return tab;
}

void Project::scriptSaved()
{
    Q_D( Project );
    if ( d->data()->isValid() ) {
        d->debugger->loadScript( scriptText(), d->data() );
        d->testModel->markTestsAsOutdated(
                TestModel::testsOfTestCase(TestModel::ScriptExecutionTestCase) );
    }
}

ScriptTab *Project::showExternalScriptActionTriggered( QWidget *parent )
{
    QAction *action = qobject_cast< QAction* >( sender() );
    const QString filePath = projectActionData(action).data.toString();
    return showExternalScriptTab( filePath, parent );
}
#endif

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
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case ShowScript:
    case ShowExternalScript:
#endif
    case ShowProjectSource:
    case ShowPlasmaPreview:
        return UiActionGroup;

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
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
    case RunGetStopsByGeoPosition:
    case RunGetJourneys:
    case DebugMenuAction:
    case DebugGetTimetable:
    case DebugGetStopSuggestions:
    case DebugGetStopsByGeoPosition:
    case DebugGetJourneys:
        return RunActionGroup;
#endif

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
                    << ShowProjectSource << ShowPlasmaPreview
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
                    << ShowScript << ShowExternalScript
#endif
                    ;
        break;

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case DebuggerActionGroup:
        actionTypes << Interrupt << Continue << AbortDebugger << RunToCursor << StepInto << StepOver
                    << StepOut << ToggleBreakpoint << RemoveAllBreakpoints;
        break;
    case RunActionGroup:
        actionTypes << RunMenuAction << RunGetTimetable << RunGetStopSuggestions
                    << RunGetStopsByGeoPosition << RunGetJourneys
                    << DebugMenuAction << DebugGetTimetable << DebugGetStopSuggestions
                    << DebugGetStopsByGeoPosition << DebugGetJourneys;
        break;
#endif

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

QStringList Project::scriptFunctions()
{
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    Q_D( Project );
    return d->globalFunctions;
#else
    return QStringList();
#endif
}

QStringList Project::includedFiles()
{
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    Q_D( Project );
    return d->includedFiles;
#else
    return QStringList();
#endif
}

void Project::startTests( const QList< TestModel::Test > &tests )
{
    Q_D( Project );
    return d->startTests( tests );
}

bool Project::startTest( TestModel::Test test )
{
    Q_D( Project );
    return d->startTest( test );
}

bool Project::startTestCase( TestModel::TestCase testCase )
{
    Q_D( Project );
    bool finishedAfterThisTestCase = false;
    if ( !isTestRunning() ) {
        if ( !d->beginTesting(TestModel::testsOfTestCase(testCase)) ) {
            // Test could not be started
            return false;
        } else {
            // Test started, only running one test
            finishedAfterThisTestCase = true;
        }
    }

    const QList< TestModel::Test > tests = TestModel::testsOfTestCase( testCase );
    bool success = tests.isEmpty();
    foreach ( TestModel::Test test, tests ) {
        if ( startTest(test)  ) {
            success = true;
        }

        if ( d->testState == ProjectPrivate::TestsGetAborted ) {
            break;
        }
    }

    if ( finishedAfterThisTestCase
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        && testCase != TestModel::ScriptExecutionTestCase
#endif
    ) {
        d->endTesting();
    }
    return success;
}

void Project::testProject()
{
    Q_D( Project );
    if ( !d->askForProjectActivation(ProjectPrivate::ActivateProjectForTests) ||
         !d->beginTesting(TestModel::allTests()) )
    {
        return;
    }

    d->testModel->clear();
    startTestCase( TestModel::ServiceProviderDataTestCase ); // This test case runs synchronously

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    // Run the script and check the results
    if ( !startTestCase(TestModel::ScriptExecutionTestCase) ||
         d->testState == ProjectPrivate::TestsGetAborted )
    {
        d->endTesting();
        return;
    }
#endif
}

QList< TestModel::Test > Project::startedTests() const
{
    Q_D( const Project );
    return d->startedTests;
}

QList< TestModel::Test > Project::finishedTests() const
{
    Q_D( const Project );
    return d->finishedTests;
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

TestModel::Test Project::testFromObjectName( const QString &objectName )
{
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    if ( objectName == QLatin1String("TEST_LOAD") ) {
        return TestModel::LoadScriptTest;
    } else if ( objectName == QLatin1String("TEST_DEPARTURES") ) {
        return TestModel::DepartureTest;
    } else if ( objectName == QLatin1String("TEST_ARRIVALS") ) {
        return TestModel::ArrivalTest;
    } else if ( objectName == QLatin1String("TEST_ADDITIONAL_DATA") ) {
        return TestModel::AdditionalDataTest;
    } else if ( objectName == QLatin1String("TEST_STOP_SUGGESTIONS") ) {
        return TestModel::StopSuggestionTest;
    } else if ( objectName == QLatin1String("TEST_STOP_SUGGESTIONS_BYGEOPOSITION") ) {
        return TestModel::StopsByGeoPositionTest;
    } else if ( objectName == QLatin1String("TEST_JOURNEYS") ) {
        return TestModel::JourneyTest;
    } else if ( objectName == QLatin1String("TEST_FEATURES") ) {
        return TestModel::FeaturesTest;
    } else
#endif
    {
        return TestModel::InvalidTest;
    }
}

void Project::testJobStarted( TestModel::Test test, JobType type, const QString &useCase )
{
    Q_UNUSED(type)
    Q_UNUSED(useCase);
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    Q_D( Project );
    if ( test == TestModel::InvalidTest ) {
        kWarning() << "Unknown test job was started";
        return;
    }

    d->testModel->markTestAsStarted( test );
#else
    Q_UNUSED(test);
#endif
}

void Project::testJobDone( TestModel::Test test, JobType type, const QString &useCase,
                           const DebuggerJobResult &result )
{
    Q_UNUSED(type);
    Q_UNUSED(useCase);
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    Q_D( Project );
    if ( test == TestModel::InvalidTest ) {
        kWarning() << "Unknown test" << test;
    } else {
        d->pendingTests.remove( test );
    }

    TestModel::TestState testState = d->testModel->setTestState( test,
            TestModel::testStateFromBool(result.success),
            result.explanation, QString(), projectAction(ShowScript),
            result.messages, result.resultData, result.request );
    if ( result.aborted ) {
        appendOutput( i18nc("@info", "Test \"%1\" was aborted for %2.",
                            TestModel::nameForTest(test), d->data()->id()),
                KColorScheme(QPalette::Active).foreground(KColorScheme::NeutralText).color() );
    } else if ( result.success && testState != TestModel::TestFinishedWithErrors ) {
        if ( testState == TestModel::TestFinishedWithWarnings ) {
            appendOutput( i18nc("@info", "Test \"%1\" finished with warnings for %2.",
                                TestModel::nameForTest(test), d->data()->id()),
                    KColorScheme(QPalette::Active).foreground(KColorScheme::NeutralText).color() );
        } else {
            appendOutput( i18nc("@info", "Test \"%1\" was successful for %2.",
                                TestModel::nameForTest(test), d->data()->id()),
                    KColorScheme(QPalette::Active).foreground(KColorScheme::PositiveText).color() );
        }
    } else {
        appendOutput( i18nc("@info", "Test \"%1\" failed for %2.",
                            TestModel::nameForTest(test), d->data()->id()),
                KColorScheme(QPalette::Active).foreground(KColorScheme::NegativeText).color() );
    }

    d->testFinished( test );
#endif
}

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
void Project::jobStarted( JobType type, const QString &useCase,
                          const QString &objectName )
{
    Q_D( Project );
    if ( type == Debugger::EvaluateInContext && d->debugger->runningJobs().count() > 1 ) {
        appendOutput( i18nc("@info",
                "<para><emphasis>Begin child job:</emphasis> %1 (%2)</para>",
                useCase, QTime::currentTime().toString()) );
    } else {
        appendOutput( (d->output.isEmpty() ? QString() : "<div style='height:10px;'>&nbsp;</div>")
                + i18nc("@info", "<para><emphasis strong='1'>Begin:</emphasis> %1 (%2)</para>",
                        useCase, QTime::currentTime().toString()) );
    }

    TestModel::Test test = testFromObjectName( objectName );
    if ( test != TestModel::InvalidTest ) {
        testJobStarted( test, type, useCase );
    }
}

void Project::jobDone( JobType type, const QString &useCase, const QString &objectName,
                       const DebuggerJobResult &result )
{
    Q_D( Project );
    QString returnValue = Global::encodeHtmlEntities( result.returnValue.toString(),
                                                      Global::EncodeLessAndGreaterThan );
    if ( returnValue.isEmpty() ) {
        returnValue = "undefined";
    }
    TestModel::Test test = testFromObjectName( objectName );
    if ( test != TestModel::InvalidTest ) {
        testJobDone( test, type, useCase, result );
    }
    if ( type == Debugger::EvaluateInContext && d->debugger->hasRunningJobs() ) {
        appendOutput( i18nc("@info",
                "<para><emphasis>End child job:</emphasis> %1, result: %2 (%3)</para>",
                useCase, returnValue, QTime::currentTime().toString()) );
    } else {
        appendOutput( i18nc("@info",
                "<para><emphasis strong='1'>End:</emphasis> %1, result: %2 (%3)</para>",
                useCase, returnValue, QTime::currentTime().toString()) );
    }
}

void Project::evaluationResult( const EvaluationResult &result )
{
    if ( result.error ) {
        if ( result.backtrace.isEmpty() ) {
            appendToConsole( i18nc("@info", "Error: <message>%1</message>", result.errorMessage) );
        } else {
            appendToConsole( i18nc("@info", "Error: <message>%1</message><nl />"
                                   "Backtrace: <message>%2</message>",
                                   result.errorMessage, result.backtrace.join("<br />")) );
        }
    } else {
        appendToConsole( Global::encodeHtmlEntities(result.returnValue,
                                                    Global::EncodeLessAndGreaterThan) );
    }
}

void Project::commandExecutionResult( const QString &returnValue, bool error )
{
    Q_UNUSED( error );
    appendToConsole( returnValue );
}

void Project::functionCallResult( const QSharedPointer< AbstractRequest > &request,
                                  bool success, const QString &explanation,
                                  const QList< TimetableData >& timetableData,
                                  const QVariant &returnValue )
{
    Q_UNUSED( request );
    if ( timetableData.isEmpty() ) {
        if ( !returnValue.toString().isEmpty() ) {
            appendOutput( i18nc("@info", "Script execution has finished without results and "
                                "returned <icode>%1</icode>.", returnValue.toString()),
                    KColorScheme(QPalette::Active).foreground(KColorScheme::NegativeText).color() );
        } else {
            appendOutput( i18nc("@info", "Script execution has finished without results."),
                    KColorScheme(QPalette::Active).foreground(KColorScheme::NegativeText).color() );
        }
    } else if ( !returnValue.toString().isEmpty() ) {
        appendOutput( i18ncp("@info",
                "Script execution has finished with %1 result and returned <icode>%2</icode>.",
                "Script execution has finished with %1 results and returned <icode>%2</icode>.",
                timetableData.count(), returnValue.toString()),
                KColorScheme(QPalette::Active).foreground(
                    success ? KColorScheme::PositiveText : KColorScheme::NegativeText).color() );
    } else {
        appendOutput( i18ncp("@info", "Script execution has finished with %1 result.",
                "Script execution has finished with %1 results.", timetableData.count()),
                KColorScheme(QPalette::Active).foreground(
                    success ? KColorScheme::PositiveText : KColorScheme::NegativeText).color() );
    }

    if ( !success ) {
        // Emit an information message about the error (no syntax errors here)
        emit informationMessage( explanation, KMessageWidget::Error, 10000 );
    }
}

bool Project::suppressMessages() const
{
    Q_D( const Project );
    return d->suppressMessages || d->isTestRunning();
}

void Project::setQuestionsEnabled( bool enable )
{
    Q_D( Project );
    d->enableQuestions = enable;
}

void Project::loadScriptResult( ScriptErrorType lastScriptError,
                                const QString &lastScriptErrorString,
                                const QStringList &globalFunctions,
                                const QStringList &includedFiles )
{
    Q_D( Project );
    d->suppressMessages = false;
    if ( lastScriptError != NoScriptError ) {
        // Emit an information message about the error (eg. a syntax error)
        d->globalFunctions.clear();
        d->includedFiles.clear();
        d->scriptState = ProjectPrivate::ScriptNotLoaded;
        if ( !d->suppressMessages ) {
            emit informationMessage( lastScriptErrorString, KMessageWidget::Error, 10000 );
        }
    } else {
        d->globalFunctions = globalFunctions;
        d->includedFiles = includedFiles;
        d->scriptState = ProjectPrivate::ScriptLoaded;
    }
    d->updateProjectActions( QList<ProjectActionGroup>() << RunActionGroup << TestActionGroup
                                                         << DebuggerActionGroup );

    emit debuggerReady();
}

void Project::runGetTimetable()
{
    Q_D( Project );
    d->callGetTimetable( Debugger::InterruptOnExceptions );
}

void Project::debugGetTimetable()
{
    Q_D( Project );
    d->callGetTimetable( InterruptOnExceptionsAndBreakpoints );
}

void Project::runGetStopSuggestions()
{
    Q_D( Project );
    d->callGetStopSuggestions( Debugger::InterruptOnExceptions );
}

void Project::runGetStopsByGeoPosition()
{
    Q_D( Project );
    d->callGetStopsByGeoPosition( Debugger::InterruptOnExceptions );
}

void Project::debugGetStopSuggestions()
{
    Q_D( Project );
    d->callGetStopSuggestions( InterruptOnExceptionsAndBreakpoints );
}

void Project::debugGetStopsByGeoPosition()
{
    Q_D( Project );
    d->callGetStopsByGeoPosition( InterruptOnExceptionsAndBreakpoints );
}

void Project::runGetJourneys()
{
    Q_D( Project );
    d->callGetJourneys( Debugger::InterruptOnExceptions );
}

void Project::debugGetJourneys()
{
    Q_D( Project );
    d->callGetJourneys( InterruptOnExceptionsAndBreakpoints );
}
#endif

DepartureRequest Project::getDepartureRequest( QWidget *parent, bool* cancelled ) const
{
    Q_D( const Project );
    parent = d->parentWidget( parent );

    QPointer<KDialog> dialog = new KDialog( parent );
    QWidget *w = new QWidget( dialog );
    QFormLayout *l = new QFormLayout( w );
    KLineEdit *city = 0;
    const QStringList stops = d->provider->data()->sampleStopNames();
    KLineEdit *stop = new KLineEdit( stops.isEmpty() ? QString() : stops.first(), w );
    KComboBox *dataType = new KComboBox( w );
    KDateTimeWidget *dateTime = new KDateTimeWidget( QDateTime::currentDateTime(), w );
    dataType->addItem( i18nc("@info/plain", "Departures"), "departures" );
    dataType->addItem( i18nc("@info/plain", "Arrivals"), "arrivals" );
    if ( d->provider->data()->useSeparateCityValue() ) {
        city = new KLineEdit( w );
        l->addRow( i18nc("@info", "City:"), city );
    }
    l->addRow( i18nc("@info", "Stop Name:"), stop );
    l->addRow( i18nc("@info", "Data Type:"), dataType );
    l->addRow( i18nc("@info", "Time:"), dateTime );
    dialog->setMainWidget( w );
    if ( d->provider && !d->provider->data()->sampleStopNames().isEmpty() ) {
        // Use first sample stop name by default
        stop->setText( d->provider->data()->sampleStopNames().first() );
        if ( city ) {
            city->setText( d->provider->data()->sampleCity() );
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
        request.parseMode = dataType->itemData( dataType->currentIndex() ).toString()
                == QLatin1String("arrivals") ? ParseForArrivals : ParseForDepartures;
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
    if ( d->provider->data()->useSeparateCityValue() ) {
        city = new KLineEdit( w );
        l->addRow( i18nc("@info", "City:"), city );
    }
    l->addRow( i18nc("@info", "Partial Stop Name:"), stop );
    dialog->setMainWidget( w );
    stop->setFocus();

    StopSuggestionRequest request;
    int result = dialog->exec();
    if ( result == KDialog::Accepted ) {
        request.city = city ? city->text() : QString();
        request.stop = stop->text();
    }
    if ( cancelled ) {
        *cancelled = result != KDialog::Accepted;
    }

    delete dialog;
    return request;
}

StopsByGeoPositionRequest Project::getStopsByGeoPositionRequest(
        QWidget *parent, bool *cancelled ) const
{
    Q_D( const Project );
    parent = d->parentWidget( parent );

#ifdef MARBLE_FOUND
    QPointer<KDialog> dialog = new KDialog( parent );
    QWidget *w = new QWidget( dialog );
    QFormLayout *l = new QFormLayout( w );
    Marble::LatLonEdit *longitude = new Marble::LatLonEdit( w, Marble::Longitude );
    Marble::LatLonEdit *latitude = new Marble::LatLonEdit( w, Marble::Latitude );
    KIntSpinBox *distance = new KIntSpinBox( 500, 50000, 1, 5000, w );
    distance->setSuffix( "m" ); // meters
    longitude->setValue( d->provider->data()->sampleLongitude() );
    latitude->setValue( d->provider->data()->sampleLatitude() );
    l->addRow( i18nc("@info", "Longitude:"), longitude );
    l->addRow( i18nc("@info", "Latitude:"), latitude );
    l->addRow( i18nc("@info", "Distance:"), distance );
    dialog->setMainWidget( w );
    longitude->setFocus();

    StopsByGeoPositionRequest request;
    int result = dialog->exec();
    if ( result == KDialog::Accepted ) {
        request.longitude = longitude->value();
        request.latitude = latitude->value();
        request.distance = distance->value();
    }
    if ( cancelled ) {
        *cancelled = result != KDialog::Accepted;
    }

    delete dialog;
    return request;
#else
    // Marble was not found
    KMessageBox::information( parent, i18nc("@info", "Cannot use Marble widgets for "
                                                     "latitude/longitude input.") );
    return StopsByGeoPositionRequest();
#endif
}

JourneyRequest Project::getJourneyRequest( QWidget *parent, bool* cancelled ) const
{
    Q_D( const Project );
    parent = d->parentWidget( parent );
    QPointer<KDialog> dialog = new KDialog( parent );
    QWidget *w = new QWidget( dialog );
    QFormLayout *l = new QFormLayout( w );
    KLineEdit *city = 0;
    const QStringList stops = d->provider->data()->sampleStopNames();
    KLineEdit *originStop = new KLineEdit( stops.isEmpty() ? QString() : stops.first(), w );
    KLineEdit *targetStop = new KLineEdit( stops.count() < 2 ? QString() : stops[1], w );
    KComboBox *dataType = new KComboBox( w );
    KDateTimeWidget *dateTime = new KDateTimeWidget( QDateTime::currentDateTime(), w );
    dataType->addItem( i18nc("@info/plain", "Departing at Given Time"), "dep" );
    dataType->addItem( i18nc("@info/plain", "Arriving at Given Time"), "arr" );
    if ( d->provider->data()->useSeparateCityValue() ) {
        city = new KLineEdit( w );
        l->addRow( i18nc("@info", "City:"), city );
    }
    l->addRow( i18nc("@info", "Start Stop Name:"), originStop );
    l->addRow( i18nc("@info", "Target Stop Name:"), targetStop );
    l->addRow( i18nc("@info", "Time:"), dateTime );
    l->addRow( i18nc("@info", "Meaning of Time:"), dataType );
    dialog->setMainWidget( w );
    if ( d->provider && !d->provider->data()->sampleStopNames().isEmpty() ) {
        // Use sample stop names by default
        originStop->setText( d->provider->data()->sampleStopNames().first() );
        if ( d->provider->data()->sampleStopNames().count() >= 2 ) {
            targetStop->setText( d->provider->data()->sampleStopNames()[1] );
        }
        if ( city ) {
            city->setText( d->provider->data()->sampleCity() );
        }
    }
    originStop->setFocus();

    JourneyRequest request;
    int result = dialog->exec();
    if ( result == KDialog::Accepted ) {
        request.city = city ? city->text() : QString();
        request.stop = originStop->text();
        request.targetStop = targetStop->text();
        request.dateTime = dateTime->dateTime();
        request.parseMode = dataType->itemData( dataType->currentIndex() ).toString()
                == QLatin1String("arr") ? ParseForJourneysByArrivalTime
                                        : ParseForJourneysByDepartureTime;
    }
    if ( cancelled ) {
        *cancelled = result != KDialog::Accepted;
    }

    delete dialog;
    return request;
}

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
void Project::abortDebugger()
{
    Q_D( Project );
    if ( !d->debugger->isRunning() && !d->debugger->isInterrupted() ) {
        // The abort action should have been disabled,
        // no stopped signal received? Update UI state to debugger state
        kDebug() << "Internal error, debugger not running, update UI state";
        d->updateProjectActions( QList<ProjectActionGroup>() << RunActionGroup << TestActionGroup
                                                             << DebuggerActionGroup );
    } else {
        d->debugger->abortDebugger();
    }
}

void Project::toggleBreakpoint( int lineNumber )
{
    Q_D( Project );
    ScriptTab *scriptTab = d->currentScriptTab();
    if ( !scriptTab ) {
        kDebug() << "No script tab opened";
        return;
    }

    scriptTab->toggleBreakpoint( lineNumber );
}

void Project::runToCursor()
{
    Q_D( Project );
    ScriptTab *scriptTab = d->currentScriptTab();
    if ( !scriptTab ) {
        kError() << "No script tab opened";
        return;
    }

    const KTextEditor::View *view = scriptTab->document()->activeView();
    d->debugger->debugRunUntilLineNumber( scriptTab->fileName(), view->cursorPosition().line() + 1 );
}

void Project::debugInterrupted( int lineNumber, const QString &fileName,
                                const QDateTime &timestamp )
{
    Q_UNUSED( timestamp );
    Q_D( Project );
    if ( !d->debugger->hasUncaughtException() ) {
        // Show script tab and ask to activate the project if it's not already active
        ScriptTab *tab = fileName == scriptFileName() || fileName.isEmpty()
                ? showScriptTab() : showExternalScriptTab(fileName);
        d->askForProjectActivation( ProjectPrivate::ActivateProjectForDebugging );
        d->updateProjectActions( QList<ProjectActionGroup>() << RunActionGroup << TestActionGroup
                                                            << DebuggerActionGroup );

        tab->setExecutionPosition( lineNumber );
    }

    // Update title of all script tabs
    if ( d->scriptTab ) {
        d->scriptTab->slotTitleChanged();
    }
    foreach ( ScriptTab *externalTab, d->externalScriptTabs ) {
        externalTab->slotTitleChanged();
    }
}

void Project::debugContinued()
{
    Q_D( Project );
    d->updateProjectActions( QList<ProjectActionGroup>() << RunActionGroup << TestActionGroup
                                                      << DebuggerActionGroup );
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

void Project::debugStopped( const ScriptRunData &scriptRunData )
{
    Q_D( Project );
    d->updateProjectActions( QList<ProjectActionGroup>() << RunActionGroup << TestActionGroup
                                                         << DebuggerActionGroup );

    QString message = i18nc("@info Shown in project output, %1: Current time",
                            "<emphasis>Statistics:</emphasis>");
    message.append( "<br />" );
    message.append( i18nc("@info %1 is a formatted duration string",
                            "- %1 spent for script execution",
                            KGlobal::locale()->formatDuration(scriptRunData.executionTime())) );
    if ( scriptRunData.interruptTime() > 0 ) {
        message.append( "<br />" );
        message.append( i18nc("@info %1 is a formatted duration string",
                              "- %1 interrupted",
                              KGlobal::locale()->formatDuration(scriptRunData.interruptTime())) );
    }
    if ( scriptRunData.signalWaitingTime() > 0 ) {
        message.append( "<br />" );
        message.append( i18nc("@info %1 is a formatted duration string",
                              "- %1 spent waiting for signals (eg. asynchronous network requests)",
                              KGlobal::locale()->formatDuration(scriptRunData.signalWaitingTime())) );
    }
    if ( scriptRunData.synchronousDownloadTime() > 0 ) {
        message.append( "<br />" );
        message.append( i18nc("@info %1 is a formatted duration string",
                              "- %1 spent for synchronous downloads",
                              KGlobal::locale()->formatDuration(scriptRunData.synchronousDownloadTime())) );
    }
    if ( scriptRunData.asynchronousDownloadSize() > 0 ||
         scriptRunData.synchronousDownloadSize() > 0 )
    {
        message.append( "<br />" );
        if ( scriptRunData.asynchronousDownloadSize() == scriptRunData.totalDownloadSize() ) {
            message.append( i18nc("@info %1 is a formatted byte size string",
                                "- %1 downloaded in asynchronous requests",
                                KGlobal::locale()->formatByteSize(scriptRunData.asynchronousDownloadSize())) );
        } else if ( scriptRunData.synchronousDownloadSize() == scriptRunData.totalDownloadSize() ) {
            message.append( i18nc("@info %1 is a formatted byte size string",
                                "- %1 downloaded in synchronous requests",
                                KGlobal::locale()->formatByteSize(scriptRunData.synchronousDownloadSize())) );
        } else {
            QString details;
            if ( scriptRunData.asynchronousDownloadSize() > 0 ) {
                details.append( i18nc("@info %1 is a formatted byte size string",
                        "%1 in asynchronous requests",
                        KGlobal::locale()->formatByteSize(scriptRunData.asynchronousDownloadSize())) );
            }
            if ( scriptRunData.synchronousDownloadSize() > 0 ) {
                if ( !details.isEmpty() ) {
                    details.append( ", " );
                }
                details.append( i18nc("@info 1 is a formatted byte size string",
                        "%1 in synchronous requests",
                        KGlobal::locale()->formatByteSize(scriptRunData.synchronousDownloadSize())) );
            }
            message.append( i18nc("@info %1 is a formatted byte size string, %2 a translated details "
                                "string (how much was downloaded in synchronous/asynchronous requests)",
                                "- %1 downloaded (%2)",
                                KGlobal::locale()->formatByteSize(scriptRunData.asynchronousDownloadSize()),
                                details) );
        }
    }
    appendOutput( message );

    if ( d->scriptTab ) {
        d->scriptTab->slotTitleChanged();
    }

    emit debuggerRunningChanged( false );
    emit debuggerReady();
}

void Project::debugAborted()
{
    appendOutput( i18nc("@info", "(Debugger aborted)"),
                  KColorScheme(QPalette::Active).foreground(KColorScheme::NegativeText).color() );
}

void Project::waitingForSignal()
{
    appendOutput( i18nc("@info", "Waiting for a signal (%1)",
                        QTime::currentTime().toString()) );
}

void Project::wokeUpFromSignal( int time )
{
    appendOutput( i18nc("@info", "Signal received, waiting time: %1 (%2)",
                        KGlobal::locale()->formatDuration(time), QTime::currentTime().toString()) );
}

void Project::scriptException( int lineNumber, const QString &errorMessage, const QString &fileName )
{
    Q_D( Project );
    ScriptTab *tab;
    if ( fileName == d->provider->data()->scriptFileName() || fileName.isEmpty() ) {
        appendOutput( i18nc("@info For the script output dock",
                            "<emphasis strong='1'>Uncaught exception at %1:</emphasis> <message>%2</message>",
                            lineNumber, errorMessage),
                      KColorScheme(QPalette::Active).foreground(KColorScheme::NegativeText).color() );
        tab = showScriptTab();
    } else {
        appendOutput( i18nc("@info For the script output dock",
                            "<emphasis strong='1'>Uncaught exception in script <filename>%1</filename> "
                            "at %2:</emphasis> <message>%3</message>",
                            QFileInfo(fileName).fileName(), lineNumber, errorMessage),
                      KColorScheme(QPalette::Active).foreground(KColorScheme::NegativeText).color() );
        tab = showExternalScriptTab( fileName );
    }

    if ( !d->suppressMessages ) {
        tab->document()->views().first()->setCursorPosition(
                KTextEditor::Cursor(lineNumber - 1, 0) );
    }
}
#endif // BUILD_PROVIDER_TYPE_SCRIPT

QString Project::scriptFileName() const
{
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    Q_D( const Project );
    return d->provider->data()->scriptFileName();
#else
    return QString();
#endif
}

QIcon Project::scriptIcon() const
{
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    Q_D( const Project );
    if ( d->scriptTab ) {
        return KIcon( d->scriptTab->document()->mimeType().replace('/', '-') );
    } else
#endif
    {
        return KIcon("application-javascript");
    }
}

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
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
#endif

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
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case Tabs::Script:
        return scriptTab();
#endif
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
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case Tabs::Script:
        return showScriptTab( parent );
#endif
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
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case Tabs::Script:
        return d->scriptTab;
#endif
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
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case Tabs::Script:
        return createScriptTab( parent );
#endif
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

void Project::closeTab( AbstractTab *tab )
{
    emit tabCloseRequest( tab );
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
        d->webTab->webView()->setUrl( d->provider->data()->url() );
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

    // Get project source text
    const QString text = projectSourceText( ReadProjectDocumentFromBuffer );

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
    KTextEditor::View *projectSourceView = document->views().first();
    connect( projectSourceView, SIGNAL(informationMessage(KTextEditor::View*,QString)),
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

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
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

ScriptTab *Project::createExternalScriptTab( const QString &filePath, QWidget *parent )
{
    Q_D( Project );
    foreach ( ScriptTab *externalScriptTab, d->externalScriptTabs ) {
        if ( externalScriptTab->fileName() == filePath ) {
            kWarning() << "Script tab already created";
            return externalScriptTab;
        }
    }

    // Create script tab
    parent = d->parentWidget( parent );
    ScriptTab *externalScriptTab = ScriptTab::create( this, parent );
    if ( !externalScriptTab ) {
        d->errorHappened( KatePartError, i18nc("@info", "Service katepart.desktop not found") );
        return 0;
    } else if ( !QFile::exists(filePath) ) {
        d->errorHappened( Project::ScriptFileNotFound,
                          i18nc("@info", "The external script file <filename>%1</filename> "
                                "could not be found.", filePath) );
        delete externalScriptTab;
        return 0;
    } else if ( !externalScriptTab->document()->openUrl(KUrl(filePath)) ) {
        d->errorHappened( Project::ScriptFileNotFound,
                          i18nc("@info", "The external script file <filename>%1</filename> "
                                "could not be opened.", filePath) );
        delete externalScriptTab;
        return 0;
    }
    externalScriptTab->document()->setModified( false );

    emit tabTitleChanged( externalScriptTab, externalScriptTab->title(), externalScriptTab->icon() );

    d->updateProjectActions( QList< ProjectAction >() << ToggleBreakpoint );

    // Connect default tab slots with the tab
    d->connectTab( externalScriptTab );
    connect( externalScriptTab, SIGNAL(destroyed(QObject*)),
             this, SLOT(externalScriptTabDestroyed(QObject*)) );
//     connect( externalScriptTab, SIGNAL(modifiedStatusChanged(bool)),
//              this, SIGNAL(externalScriptModifiedStateChanged(bool)) );
    return externalScriptTab;
}
#endif

ServiceProvider *Project::provider() const
{
    Q_D( const Project );
    Q_ASSERT( d->provider );
    return d->provider;
}

void Project::setProviderData( const ServiceProviderData *providerData )
{
    Q_D( Project );

    // Recreate service provider plugin with new info
    delete d->provider;
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    if ( providerData->type() == Enums::ScriptedProvider ) {
        d->provider = new ServiceProviderScript( providerData, this );
    } else
#endif
    {
        d->provider = new ServiceProvider( providerData, this );
    }
    emit nameChanged( projectName() );
    emit iconNameChanged( iconName() );
    emit iconChanged( projectIcon() );
    emit dataChanged( data() );
    d->testModel->markTestsAsOutdated(
            TestModel::testsOfTestCase(TestModel::ServiceProviderDataTestCase) );

    if ( d->projectSourceTab ) {
        // Update service provider plugin document
        d->projectSourceTab->document()->setText( projectSourceText(ReadProjectDocumentFromBuffer) );
    } else {
        const bool wasModified = isModified();
        const bool wasProjectSourceModified = isProjectSourceModified();
        d->projectSourceBufferModified = true;
        if ( !wasModified ) {
            d->updateProjectActions( QList<ProjectAction>() << Save );
            emit modifiedStateChanged( true );
        }
        if ( !wasProjectSourceModified ) {
            emit projectSourceModifiedStateChanged( true );
        }
    }
}

void Project::showSettingsDialog( QWidget *parent )
{
    Q_D( Project );

    // Check if a modified project source tab is opened and ask to save it before
    // editing the file in the settings dialog
    parent = d->parentWidget( parent );

    // Create settings dialog
    QPointer< ProjectSettingsDialog > dialog( new ProjectSettingsDialog(parent) );
    dialog->setProviderData( d->provider->data(), d->filePath );
    if ( dialog->exec() == KDialog::Accepted ) {
        setProviderData( dialog->providerData(this) );

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
        if ( dialog->newScriptTemplateType() != Project::NoScriptTemplate ) {
            // A new script file was set in the dialog
            // Load the chosen template
            setScriptText( Project::scriptTemplateText(dialog->newScriptTemplateType()) );
        }
#endif
    }
    delete dialog.data();
}

void Project::projectSourceDocumentChanged( KTextEditor::Document *projectSourceDocument )
{
    Q_D( Project );
    Q_UNUSED( projectSourceDocument );

    // Recreate service provider plugin with new XML content
    d->readProjectSourceDocumentFromTabOrFile( d->filePath );

    // Update other tabs
    if ( d->webTab ) {
        d->webTab->webView()->setUrl( provider()->data()->url() );
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

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
void Project::scriptTabDestroyed()
{
    Q_D( Project );
    d->scriptTab = 0;
    d->updateProjectActions( QList< ProjectAction >() << ToggleBreakpoint );
}

void Project::externalScriptTabDestroyed( QObject *tab )
{
    Q_D( Project );
    // qobject_cast does not work here, but only the address gets compared in removeOne(),
    // which needs to be casted to the list element type
    for ( int i = 0; i < d->externalScriptTabs.count(); ++i ) {
        if ( qobject_cast<QObject*>(d->externalScriptTabs[i]) == tab ) {
            d->externalScriptTabs.removeAt( i );
            return;
        }
    }
    qWarning() << "Internal error: Script tab destroyed but not found in the list";
}
#endif

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

QString Project::projectSourceText( ProjectDocumentSource source) const
{
    Q_D( const Project );
    if ( !d->provider ) {
        kDebug() << "No service provider plugin loaded";
        return QString();
    }

    if ( d->projectSourceTab && (source == ReadProjectDocumentFromTab ||
                            source == ReadProjectDocumentFromTabIfOpened) )
    {
        // Service provider plugin XML file opened in a tab
        return d->projectSourceTab->document()->text();
    } else if ( source == ReadProjectDocumentFromBuffer ||
                source == ReadProjectDocumentFromTabIfOpened )
    {
        // No project source tab opened, read XML text from file to buffer
        ServiceProviderDataWriter writer;
        QBuffer buffer;
        if( writer.write(&buffer, d->provider, d->xmlComments) ) {
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

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
QString Project::scriptText( const QString &includedScriptFilePath ) const
{
    Q_D( const Project );
    if ( includedScriptFilePath.isEmpty() ) {
        if ( d->scriptTab ) {
            // Script file opened in a tab
            return d->scriptTab->document()->text();
        } else if ( !d->unsavedScriptContents.isEmpty() ) {
            // Unsaved script contents available
            return d->unsavedScriptContents;
        }
    }

    // No script tab opened, read script text from file
    const QString fileName = !includedScriptFilePath.isEmpty()
            ? includedScriptFilePath : d->provider->data()->scriptFileName();
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
#endif

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
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    Q_D( const Project );
    KTextEditor::View *activeView = d->scriptTab->document()->activeView();
    const QPoint pointInView = activeView->cursorToCoordinate( position );
    const QPoint pointGlobal = activeView->mapToGlobal( pointInView );
    QToolTip::showText( pointGlobal, text );
#else
    Q_UNUSED( position )
    Q_UNUSED( text )
#endif
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

ServiceProviderData *Project::data()
{
    Q_D( Project );
    // Return as non const, because QML cannot use it otherwise
    return const_cast< ServiceProviderData* >( d->data() );
}

Project::InstallType Project::Project::installationTypeFromFilePath( const QString &filePath )
{
    if ( filePath.isEmpty() ) {
        return NoInstallation;
    }

    const QString saveDir = QFileInfo( filePath ).path() + '/';
    const QString localSaveDir = KGlobal::dirs()->saveLocation( "data",
            ServiceProviderGlobal::installationSubDirectory() );
    if ( saveDir == localSaveDir ) {
        return LocalInstallation;
    }

    const QStringList allSaveDirs = KGlobal::dirs()->findDirs( "data",
            ServiceProviderGlobal::installationSubDirectory() );
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
