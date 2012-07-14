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

#ifndef SCRIPTTAB_H
#define SCRIPTTAB_H

// Own includes
#include "abstracttab.h"

// KDE includes
#include <KTextEditor/MarkInterface>

class JavaScriptModel;
namespace Debugger
{
    class Breakpoint;
}
using namespace Debugger;

namespace KTextEditor
{
    class View;
    class Cursor;
}

/** @brief Represents a script document tab. */
class ScriptTab : public AbstractDocumentTab {
    Q_OBJECT
public:
    static ScriptTab *create( Project *project, QWidget *parent );
    virtual inline TabType type() const { return Tabs::Script; };
    virtual QIcon icon() const;

    /**
     * @brief Saves modifications, if any.
     *
     * If no file name is specified the user gets asked for it.
     **/
    virtual bool save();

    inline JavaScriptModel *scriptModel() const { return m_scriptModel; };
    inline JavaScriptCompletionModel *completionModel() const { return m_completionModel; };
    inline QSortFilterProxyModel *functionsModel() const { return m_functionsModel; };
    inline KComboBox *functionsWidget() const { return m_functionsWidget; };
    inline KAction *previousFunctionAction() const { return m_previousFunctionAction; };
    inline KAction *nextFunctionAction() const { return m_nextFunctionAction; };
    inline int executionLine() const { return m_executionLine; };

    static KAction *createNextFunctionAction( QObject *parent = 0 );
    static KAction *createPreviousFunctionAction( QObject *parent = 0 );

signals:
    void syntaxErrorFound( const QString &errorString );
    void canGoToPreviousFunctionChanged( bool canGo );
    void canGoToNextFunctionChanged( bool canGo );

public slots:
    void parseScript();
    void goToPreviousFunction();
    void goToNextFunction();
    void toggleBreakpoint( int lineNumber = -1 );
    void setExecutionPosition( int executionLine = -1, int column = 0 );
    void removeExecutionMarker();

protected slots:
    void documentChanged( KTextEditor::Document *document );
    void scriptCursorPositionChanged( KTextEditor::View *view, const KTextEditor::Cursor &cursor );
    void showTextHint( const KTextEditor::Cursor &position, QString &text );
    void currentFunctionChanged( int index );
    void breakpointAdded( const Breakpoint &breakpoint );
    void breakpointAboutToBeRemoved( const Breakpoint &breakpoint );
    void markChanged( KTextEditor::Document *document, const KTextEditor::Mark &mark,
                      KTextEditor::MarkInterface::MarkChangeAction action );
    void informationMessage( KTextEditor::View*, const QString &message );
    void slotSetStatusBarText( const QString &message );
    void contextMenuAboutToShow( KTextEditor::View *view, QMenu* menu );

private:
    ScriptTab( Project *project, KTextEditor::Document *document, QWidget *parent = 0 );
    void updateNextPreviousFunctionActions();

    JavaScriptModel *m_scriptModel;
    JavaScriptCompletionModel *m_completionModel;
    QSortFilterProxyModel *m_functionsModel;
    KComboBox *m_functionsWidget;
    KAction *m_previousFunctionAction;
    KAction *m_nextFunctionAction;
    QTimer *m_backgroundParserTimer;
    int m_executionLine;
};

#endif // Multiple inclusion guard
