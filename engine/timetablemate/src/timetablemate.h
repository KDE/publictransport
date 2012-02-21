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
#include <engine/departureinfo.h>
#include <engine/scripting.h>
#include <engine/timetableaccessor.h>

// KDE includes
#include <KParts/MainWindow>
#include <KDebug>

class Storage;
class Helper;
class Network;
class TimetableAccessorInfo;
class JavaScriptModel;
class PublicTransportPreview;
class TimetableMateView;
class ResultObject;
class TimetableAccessor;
struct RequestInfo;

namespace KTextEditor
{
    class Document;
    class View;
    class Cursor;
};
namespace KParts
{
    class PartManager;
};
class KWebView;
class KUrlComboBox;
class KToggleAction;
class KRecentFilesAction;
class KUrl;
class KTabWidget;
class KComboBox;

class QPrinter;
class QScriptProgram;
class QScriptProgram;
class QScriptEngine;
class QSortFilterProxyModel;

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

    void scriptNextFunction();
    void scriptPreviousFunction();

    /** Return pressed in the url bar. */
    void urlBarReturn( const QString &text );
    void webUrlChanged( const QUrl &url );

protected:
    virtual void closeEvent( QCloseEvent *event );

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

    bool scriptRun( const QString &functionToRun, const RequestInfo *requestInfo,
                    const TimetableAccessorInfo *info,
                    ResultObject *resultObject, QVariant *result );

    bool hasHomePageURL( const TimetableAccessorInfo *info );

    /** Decodes the given HTML document. First it tries QTextCodec::codecForHtml().
        * If that doesn't work, it parses the document for the charset in a meta-tag. */
    static QString decodeHtml( const QByteArray &document,
                               const QByteArray &fallbackCharset = QByteArray() );

    KUrl getDepartureUrl();
    KUrl getDepartureUrl( const TimetableAccessor &accessor, const QString &city,
                          const QString &stop, const QDateTime &dateTime,
                          const QString &dataType, bool useDifferentUrl = false ) const;

    KUrl getStopSuggestionUrl();
    KUrl getStopSuggestionUrl( const TimetableAccessor &accessor,
                               const QString &city, const QString &stop );

    KUrl getJourneyUrl();
    KUrl getJourneyUrl( const TimetableAccessor &accessor, const QString &city,
                        const QString &startStopName, const QString &targetStopName,
                        const QDateTime &dateTime, const QString &dataType ) const;

    /** Encodes the url in @p str using the charset in @p charset. Then it is
    * percent encoded.
    * @see charsetForUrlEncoding() */
    static QString toPercentEncoding( const QString &str, const QByteArray &charset );

    static QString gethex( ushort decimal );

    bool loadScript();
    bool lazyLoadScript();

private:
    KTabWidget *m_mainTabBar;
    KParts::PartManager *m_partManager;
    TimetableMateView *m_view;
    KTextEditor::Document *m_accessorDocument;
    KTextEditor::Document *m_scriptDocument;
    PublicTransportPreview *m_preview;
    KWebView *m_webview;

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
};

#endif // _TIMETABLEMATE_H_

struct GlobalTimetableInfo;
// kate: indent-mode cstyle; indent-width 4; replace-tabs on;
