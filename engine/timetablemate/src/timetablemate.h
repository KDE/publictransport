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

// Own includes
#include "debuggeragent.h"

// PublicTransport engine includes
#include <engine/departureinfo.h>
#include <engine/scripting.h>
#include <engine/timetableaccessor.h>

// KDE includes
#include <KParts/MainWindow>
#include <KDebug>
#include <KTextEditor/MarkInterface>

// Qt includes
#include <QScriptContextInfo>
#include <QModelIndex>
#include <QAbstractItemDelegate>

class TimetableAccessorInfo;
class JavaScriptModel;
class PublicTransportPreview;
class TimetableMateView;
namespace Scripting {
    class ResultObject;
};
namespace Debugger {
    class Debugger;
    struct EvaluationResult;
};
namespace KTextEditor
{
    class Document;
    class View;
    class Cursor;
    class Mark;
};
namespace KParts
{
    class PartManager;
};
class KLineEdit;
class KTextBrowser;
class KWebView;
class KUrlComboBox;
class KToggleAction;
class KRecentFilesAction;
class KUrl;
class KTabWidget;
class KComboBox;

class QPlainTextEdit;
class QModelIndex;
class QDockWidget;
class QPrinter;
class QSortFilterProxyModel;
class QStandardItem;
class QStandardItemModel;
class QScriptProgram;
class QScriptEngine;

using namespace Scripting;
using namespace Debugger;

typedef QPair<QStandardItem*, QStandardItem*> ScriptVariableRow;

// TODO
class CheckboxDelegate : public QAbstractItemDelegate {
public:
    explicit CheckboxDelegate( QObject *parent = 0 ) : QAbstractItemDelegate(parent) {};

    virtual void paint( QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index ) const;
    virtual QSize sizeHint( const QStyleOptionViewItem &option, const QModelIndex &index ) const;

};

/**
 * This class serves as the main window for TimetableMate. It handles the
 * menus, toolbars, and status bars.
 *
 * @short Main window class
 * @author Friedrich Pülz <fpuelz@gmx.de>
 * @version 0.3
 */
class TimetableMate : public KParts::MainWindow   //KXmlGuiWindow {
{
    Q_OBJECT
public:
    enum Tabs {
        AccessorTab = 0,
        AccessorSourceTab,
        ScriptTab,
        PlasmaPreviewTab,
        WebTab
    };

    enum ScriptError {
        NoScriptError = 0,
        ScriptLoadFailed,
        ScriptParseError,
        ScriptRunError
    };

    enum DebugType {
        NoDebugging = 0,
        InterruptOnExceptions,
        InterruptAtStart
    };

    /** @brief The name of the script function to get a list of used TimetableInformation's. */
    static const char *SCRIPT_FUNCTION_USEDTIMETABLEINFORMATIONS;

    /** @brief The name of the script function to download and parse departures/arrivals. */
    static const char *SCRIPT_FUNCTION_GETTIMETABLE;

    /** @brief The name of the script function to download and parse journeys. */
    static const char *SCRIPT_FUNCTION_GETJOURNEYS;

    /** @brief The name of the script function to download and parse stop suggestions. */
    static const char *SCRIPT_FUNCTION_GETSTOPSUGGESTIONS;

    /** @brief Gets a list of extensions that are allowed to be imported by scripts. */
    static QStringList allowedExtensions();

    /** Default Constructor */
    TimetableMate();

    /** Default Destructor */
    virtual ~TimetableMate();

signals:
    /** @brief Signals ready TimetableData items. */
    void departuresReady( const QList<TimetableData> &departures,
                          ResultObject::Features features, ResultObject::Hints hints,
                          const QString &url, const GlobalTimetableInfo &globalInfo,
                          const DepartureRequestInfo &info, bool couldNeedForcedUpdate = false );

    /** @brief Signals ready TimetableData items. */
    void journeysReady( const QList<TimetableData> &departures,
                        ResultObject::Features features, ResultObject::Hints hints,
                        const QString &url, const GlobalTimetableInfo &globalInfo,
                        const JourneyRequestInfo &info, bool couldNeedForcedUpdate = false );

    /** @brief Signals ready TimetableData items. */
    void stopSuggestionsReady( const QList<TimetableData> &departures,
                               ResultObject::Features features, ResultObject::Hints hints,
                               const QString &url, const GlobalTimetableInfo &globalInfo,
                               const StopSuggestionRequestInfo &info,
                               bool couldNeedForcedUpdate = false );

public slots:
    void fileNew();
    void open( const KUrl &url );
    void fileOpen();
    void fileOpenInstalled();
    void fileSave();
    void fileSaveAs();

    void install();
    void installGlobal();

    void publish() {}; // TODO

    /**
     * @brief An error was received from the script.
     *
     * @param message The error message.
     * @param failedParseText The text in the source document where parsing failed.
     **/
    void scriptErrorReceived( const QString &message,
                              const QString &failedParseText = QString() );
    void markChanged( KTextEditor::Document *document, const KTextEditor::Mark &mark,
                      KTextEditor::MarkInterface::MarkChangeAction action );
    void sendCommandToConsole( const QString &command );

protected slots:
    void optionsPreferences();

    void showScriptTab( bool loadTemplateIfEmpty = true );
    void showWebTab( const QString &url );
    void currentTabChanged( int index );
    void activePartChanged( KParts::Part *part );
    void informationMessage( KTextEditor::View *, const QString &message );

    void accessorDocumentChanged( KTextEditor::Document *document );
    void scriptDocumentChanged( KTextEditor::Document *document );
    void accessorWidgetsChanged();
    void scriptFileChanged( const QString &scriptFile );
    void plasmaPreviewLoaded();

    void beginScriptParsing();
    void currentFunctionChanged( int index );
    void scriptCursorPositionChanged( KTextEditor::View *view,
                                      const KTextEditor::Cursor &cursor );
    void showTextHint( const KTextEditor::Cursor &position, QString &text );

    void scriptRunParseTimetable();
    void scriptRunParseStopSuggestions();
    void scriptRunParseJourneys();

    void webLoadHomePage();
    void webLoadDepartures();
    void webLoadStopSuggestions();
    void webLoadJourneys();

    void toolsCheck();
    void toggleBreakpoint( int lineNumber = -1 );

    /** @brief Start debugger and call 'getTimetable' script function. */
    void debugScriptDepartures();

    /** @brief Start debugger and call 'getJourneys' script function. */
    void debugScriptJourneys();

    /** @brief Start debugger and call 'getStopSUggestions' script function. */
    void debugScriptStopSuggestions();

    /** @brief Abort debugging. */
    void abortDebugger();

    /** @brief Script execution started. */
    void debugStarted();

    /** @brief Script execution stopped. */
    void debugStopped();

    /** @brief Script execution was interrupted. */
    void debugInterrupted();

    /** @brief Script execution was continued after an interrupt. */
    void debugContinued();

    /** @brief Execute script until the line number at the cursor gets hit. */
    void runToCursor();

    /** @brief There was an uncaught execution in the script. */
    void uncaughtException( int lineNumber, const QString &errorMessage );

    /** @brief The current script backtrace changed. */
    void backtraceChanged( const FrameStack &backtrace, BacktraceChange change );

    /** @brief A @p breakpoint was added. */
    void breakpointAdded( const Breakpoint &breakpoint );

    /** @brief A @p breakpoint was removed. */
    void breakpointRemoved( const Breakpoint &breakpoint );

    /** @brief A @p breakpoint was reached. */
    void breakpointReached( const Breakpoint &breakpoint );

    /** @brief The breakpoint represented by @p item was changed. */
    void breakpointChangedInModel( QStandardItem *item );

    /** @brief The script produced output at @p context. */
    void scriptOutput( const QString &outputString, const QScriptContextInfo &contextInfo );

    /** @brief An item in the backtrace widget was clicked. */
    void clickedBacktraceItem( const QModelIndex &backtraceItem );

    /** @brief An item in the breakpoint widget was clicked. */
    void clickedBreakpointItem( const QModelIndex &breakpointItem );

    void scriptNextFunction();
    void scriptPreviousFunction();

    /** Return pressed in the url bar. */
    void urlBarReturn( const QString &text );
    void webUrlChanged( const QUrl &url );

    void documentationAnchorClicked( const QUrl &url );
    void documentationUrlChanged( const QUrl &url );
    void documentationChosen( int index );

    void appendToConsole( const QString &text );
    void consoleEvaluationResult( const EvaluationResult &result );
    void functionCallResult( const QList< TimetableData > &timetableData,
                             const QScriptValue &returnValue );

protected:
    virtual void closeEvent( QCloseEvent *event );
    virtual bool eventFilter( QObject *source, QEvent *event );

private:
    void setupActions();
    void updateWindowTitle();
    void updateNextPreviousFunctionActions();

    void writeScriptTemplate();
    bool loadTemplate( const QString &fileName = QString() );
    bool loadAccessor( const QString &fileName );
    bool setAccessorValues( QByteArray *text, QString *error = 0,
                            const QString &fileName = QString() );
    bool loadScriptForCurrentAccessor( bool openFile = true );

    void setChanged( bool changed = true );
    void syncAccessor();

    bool hasHomePageURL( const TimetableAccessorInfo *info );

    /** Decodes the given HTML document. First it tries QTextCodec::codecForHtml().
        * If that doesn't work, it parses the document for the charset in a meta-tag. */
    static QString decodeHtml( const QByteArray &document,
                               const QByteArray &fallbackCharset = QByteArray() );

    DepartureRequestInfo getDepartureRequestInfo();
    JourneyRequestInfo getJourneyRequestInfo();
    StopSuggestionRequestInfo getStopSuggestionRequestInfo();

    /** Encodes the url in @p str using the charset in @p charset. Then it is
    * percent encoded.
    * @see charsetForUrlEncoding() */
    static QString toPercentEncoding( const QString &str, const QByteArray &charset );

    static QString gethex( ushort decimal );

    bool loadScript();

    void updateVariableModel();
    void addVariables( const Variables &variables, const QModelIndex &parent = QModelIndex(),
                       bool onlyImportantObjects = false );
    KToolBar *createDockOverviewBar( Qt::ToolBarArea area, const QString &objectName,
                                     QWidget *parent = 0 );

    KTabWidget *m_mainTabBar;
    KParts::PartManager *m_partManager;
    TimetableMateView *m_view;
    KTextEditor::Document *m_accessorDocument;
    KTextEditor::Document *m_scriptDocument;
    PublicTransportPreview *m_preview;
    KWebView *m_webview;
    QDockWidget *m_backtraceDock;
    QDockWidget *m_consoleDock;
    QDockWidget *m_outputDock;
    QDockWidget *m_breakpointDock;
    QDockWidget *m_variablesDock;
    QDockWidget *m_documentationDock;
    QPlainTextEdit *m_outputWidget;
    QPlainTextEdit *m_consoleWidget;
    KComboBox *m_documentationChooser;
    KWebView *m_documentationWidget;
    KLineEdit *m_consoleEdit;
    QStandardItemModel *m_backtraceModel;
    QStandardItemModel *m_breakpointModel;
    QStandardItemModel *m_variablesModel;
    KToolBar *m_bottomDockOverview;

    KUrlComboBox *m_urlBar;
    KComboBox *m_functions;
    JavaScriptModel *m_javaScriptModel;
    QSortFilterProxyModel *m_functionsModel;
    QTimer *m_backgroundParserTimer;

    KToggleAction *m_toolbarAction;
    KToggleAction *m_statusbarAction;
    KRecentFilesAction *m_recentFilesAction;

    QString m_currentServiceProviderID;
    QString m_openedPath;

    int m_currentTab;
    bool m_changed;
    bool m_accessorDocumentChanged;
    bool m_accessorWidgetsChanged;

    QScriptEngine *m_engine;
    QScriptProgram *m_script;
    Network *m_scriptNetwork;
    Helper *m_scriptHelper;
    ResultObject *m_scriptResult;
    Storage *m_scriptStorage;
    QString m_lastError;
    ScriptError m_lastScriptError;
    QStringList m_scriptErrors;
    QStringList m_consoleHistory;
    int m_consoleHistoryIndex;

    Debugger::Debugger *m_debugger;
    int m_executionLine;
};

#endif // _TIMETABLEMATE_H_

struct GlobalTimetableInfo;
// kate: indent-mode cstyle; indent-width 4; replace-tabs on;
