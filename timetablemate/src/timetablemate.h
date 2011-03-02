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

#ifndef TIMETABLEMATE_H
#define TIMETABLEMATE_H

#include "ui_prefs_base.h"
#include "enums.h"

#include <KParts/MainWindow>
#include <KDebug>

class QSortFilterProxyModel;
class JavaScriptModel;
class KComboBox;
class PublicTransportPreview;
class TimetableMateView;
class TimetableData;
class ResultObject;
struct TimetableAccessor;

class KWebKitPart;
class KUrlComboBox;
class KToggleAction;
class KRecentFilesAction;
class KUrl;
class KTabWidget;
namespace KTextEditor {
    class Document;
	class View;
	class Cursor;
};
namespace KParts {
    class PartManager;
};
class QPrinter;

class DebugMessage;
typedef QList<DebugMessage> DebugMessageList;

/**
 * This class serves as the main window for TimetableMate. It handles the
 * menus, toolbars, and status bars.
 *
 * @short Main window class
 * @author Friedrich Pülz <fpuelz@gmx.de>
 * @version 0.2
 */
class TimetableMate : public KParts::MainWindow { //KXmlGuiWindow {
    Q_OBJECT
    public:
	enum Tabs {
	    AccessorTab = 0,
	    AccessorSourceTab,
	    ScriptTab,
	    PlasmaPreviewTab,
	    WebTab
	};
	
	/** Default Constructor */
	TimetableMate();

	/** Default Destructor */
	virtual ~TimetableMate();

    public slots:
	void fileNew();
	void open( const KUrl &url );
	void fileOpen();
	void fileOpenInstalled();
	void fileSave();
	void fileSaveAs();

	void install();
	void installGlobal();

    private slots:
	void optionsPreferences();

	void showScriptTab( bool loadTemplateIfEmpty = true );
	void showWebTab( const QString &url, RawUrl rawUrl = NormalUrl );
	void currentTabChanged( int index );
	void activePartChanged( KParts::Part *part );
	void informationMessage( KTextEditor::View*, const QString &message );
	
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
	bool loadScriptForCurrentAccessor( const QString &path, bool openFile = true );
	
	void setChanged( bool changed = true );
	void syncAccessor();

	bool scriptRun( const QString &functionToRun, TimetableData *timetableData,
			ResultObject *resultObject, QVariant *result, 
			DebugMessageList *debugMessageList = 0 );

	bool hasHomePageURL( const TimetableAccessor &accessor );
	bool hasRawDepartureURL( const TimetableAccessor &accessor );
	bool hasRawStopSuggestionURL( const TimetableAccessor &accessor );
	bool hasRawJourneyURL( const TimetableAccessor &accessor );

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
				    const QString &city, const QString& stop );

	KUrl getJourneyUrl();
	KUrl getJourneyUrl( const TimetableAccessor &accessor, const QString& city,
			    const QString& startStopName, const QString& targetStopName,
			    const QDateTime &dateTime, const QString& dataType ) const;

	/** Encodes the url in @p str using the charset in @p charset. Then it is
	* percent encoded.
	* @see charsetForUrlEncoding() */
	static QString toPercentEncoding( const QString &str, const QByteArray &charset );

	static QString gethex( ushort decimal );
	
    private:
	Ui::prefs_base ui_prefs_base;
	
	KTabWidget *m_mainTabBar;
	KParts::PartManager *m_partManager;
	TimetableMateView *m_view;
	KTextEditor::Document *m_accessorDocument;
	KTextEditor::Document *m_scriptDocument;
	PublicTransportPreview *m_preview;
	KWebKitPart *m_webview;
	
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
};

#endif // _TIMETABLEMATE_H_
