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

#ifndef BREAKPOINTMODEL_H
#define BREAKPOINTMODEL_H

// Own includes
#include "debuggerstructures.h"

// Qt includes
#include <QAbstractListModel>

// KDE includes
#include <KWidgetItemDelegate>

class QMutex;
class Project;

namespace Debugger {

enum BreakpointChangeType {
    NoOpBreakpointChange = 0,

    AddBreakpoint,
    RemoveBreakpoint,
    UpdateBreakpoint
};

struct BreakpointChange {
    BreakpointChange( BreakpointChangeType type = NoOpBreakpointChange,
                      const Breakpoint &breakpoint = Breakpoint() )
            : type(type), breakpoint(breakpoint) {};

    BreakpointChangeType type;
    Breakpoint breakpoint;
};

/**
 * @brief A model for breakpoints.
 *
 * Each Debugger uses a BreakpointModel to store and check for breakpoints.
 *
 * Breakpoints can have a maximum hit count, after which they will be disabled. They can also have
 * a condition written in JavaScript, which gets executed in the current script context and should
 * return a boolean. If the condition returns true, the breakpoint matches.
 * To get a list of all line numbers which have a breakpoint, use breakpointLines().
 **/
class BreakpointModel : public QAbstractListModel
{
    Q_OBJECT
    friend class Breakpoint;

public:
    /**< Columns available in this model. */
    enum Column {
        EnableColumn = 0, /**< Shows whether or not a breakpoint is enabled. */
        SourceColumn,
        ConditionColumn,
        HitCountColumn,
        LastConditionResultColumn,

        ColumnCount /**< @internal */
    };

    BreakpointModel( QObject *parent = 0 );
    virtual ~BreakpointModel();

    virtual int columnCount( const QModelIndex &parent = QModelIndex() ) const;
    virtual int rowCount( const QModelIndex &parent = QModelIndex() ) const;
    virtual QModelIndex index( int row, int column,
                               const QModelIndex &parent = QModelIndex() ) const;
    virtual QVariant headerData( int section, Qt::Orientation orientation,
                                 int role = Qt::DisplayRole ) const;
    virtual QVariant data( const QModelIndex &index, int role = Qt::DisplayRole ) const;
    virtual bool setData( const QModelIndex &index, const QVariant &value, int role = Qt::EditRole );
    virtual Qt::ItemFlags flags( const QModelIndex &index ) const;
    virtual bool removeRows( int row, int count, const QModelIndex &parent = QModelIndex() );
    void clear();
    bool isEmpty() const { return rowCount() == 0; };

    bool hasBreakpoint( int lineNumber ) const;
    bool hasBreakpoint( const QString &fileName, int lineNumber ) const;

    QModelIndex indexFromRow( int row ) const;
    QModelIndex indexFromBreakpoint( Breakpoint *breakpoint ) const;

    Breakpoint *breakpointFromRow( int row ) const;
    Breakpoint *breakpointFromIndex( const QModelIndex &index ) const;
    Breakpoint *breakpointFromLineNumber( const QString &fileName, int lineNumber ) const;

    Breakpoint *setBreakpoint( const QString &fileName, int lineNumber, bool enable = true );
    Breakpoint *toggleBreakpoint( const QString &fileName, int lineNumber );

    /** @brief Get the state of the breakpoint at @p lineNumber or NoBreakpoint if there is none. */
    Breakpoint::State breakpointState( const QString &fileName, int lineNumber ) const;

    /** @brief Get a list of all Breakpoint objects in this model. */
    QList< Breakpoint* > breakpoints( const QString &fileName ) const;

    /** @brief Get a list of all line numbers with an associated breakpoint in this model. */
    QList< uint > breakpointLineNumbers( const QString &fileName ) const;

    /** @brief Get a hash with all Breakpoints in this model keyed by their line numbers. */
    QHash< uint, Breakpoint* > breakpointsByLineNumber( const QString &fileName ) const;

signals:
    /** @brief Emitted when @p breakpoint gets added to the model. */
    void breakpointAdded( Breakpoint *breakpoint );

    /** @brief Emitted when @p breakpoint gets added to the model. */
    void breakpointAdded( const Breakpoint &breakpoint );

    /** @brief Emitted before @p breakpoint gets removed from the model. */
    void breakpointAboutToBeRemoved( const Breakpoint &breakpoint );

    /** @brief Emitted when @p breakpoint gets modified. */
    void breakpointModified( const Breakpoint &breakpoint );

    /**
     * @brief Emitted after the last breakpoint was removed or if one gets added to an empty model.
     *
     * Can be used for example to update the enabled state of a "Remove All Breakpoints" action.
     **/
    void emptinessChanged( bool isEmpty );

public slots:
    void applyChange( const BreakpointChange &change );

    /** @brief Append @p breakpoint to the list of breakpoints in this model. */
    void addBreakpoint( Breakpoint *breakpoint );

    /** @brief Remove @p breakpoint from this model. */
    void removeBreakpoint( Breakpoint *breakpoint );

    void updateBreakpoint( const Breakpoint &breakpoint );

protected slots:
    void slotBreakpointChanged( Breakpoint *breakpoint );

private:
    struct BreakpointData {
        QList< Breakpoint* > breakpoints;
        QHash< uint, Breakpoint* > breakpointsByLineNumber;
    };

    QHash< QString, BreakpointData > m_breakpointsByFile;
    QString m_fileName;
};

class CheckboxDelegate : public KWidgetItemDelegate {
    Q_OBJECT

public:
    explicit CheckboxDelegate( QAbstractItemView *itemView, QObject *parent = 0 );
    virtual ~CheckboxDelegate();

signals:
    void checkedStateChanged( const QModelIndex &index, bool checked );

protected slots:
    void updateGeometry();
    void checkboxToggled( bool checked );

protected:
    virtual QList< QWidget* > createItemWidgets() const;
    virtual void updateItemWidgets( const QList< QWidget* > widgets,
                                    const QStyleOptionViewItem &option,
                                    const QPersistentModelIndex &index ) const;
    virtual QSize sizeHint( const QStyleOptionViewItem &option, const QModelIndex &index ) const;
    virtual void paint( QPainter *painter, const QStyleOptionViewItem &option,
                        const QModelIndex &index ) const;
};

}; // namespace Debugger

#endif // Multiple inclusion guard
