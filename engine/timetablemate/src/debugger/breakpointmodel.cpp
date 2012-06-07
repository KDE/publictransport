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
#include "breakpointmodel.h"

// Own includes
#include "debuggerstructures.h"
#include "debugger.h"

// KDE includes
#include <KDebug>
#include <KLocalizedString>

// Qt includes
#include <QCheckBox>
#include <QApplication>
#include <QAbstractItemView>
#include <QTreeView>
#include <QHeaderView>

using namespace Debugger;

BreakpointModel::BreakpointModel( QObject *parent )
        : QAbstractListModel( parent )
{
}

BreakpointModel::~BreakpointModel()
{
    qDeleteAll( m_breakpoints );
}

void BreakpointModel::slotBreakpointChanged( Breakpoint *breakpoint )
{
    const QModelIndex index = indexFromBreakpoint( breakpoint );
    if ( index.isValid() ) {
        emit dataChanged( index, index );
        emit breakpointModified( *breakpoint );
    } else {
        kDebug() << "Could not find breakpoint at line" << breakpoint->lineNumber() << "in the model";
    }
}

QVariant BreakpointModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
    if ( orientation == Qt::Horizontal && role == Qt::DisplayRole ) {
        switch ( static_cast<Column>(section) ) {
        case EnableColumn:
            return i18nc("@title:column", "Enabled");
        case SourceColumn:
            return i18nc("@title:column", "Address");
        case ConditionColumn:
            return i18nc("@title:column", "Condition");
        case HitCountColumn:
            return i18nc("@title:column", "Hits");
        case LastConditionResultColumn:
            return i18nc("@title:column", "Last Condition Result");
        default:
            return QVariant();
        }
    }

    return QVariant();
}

QVariant BreakpointModel::data( const QModelIndex &index, int role ) const
{
    if ( !index.isValid() || index.row() >= m_breakpoints.count() ) {
        return QVariant();
    }

    Breakpoint *breakpoint = m_breakpoints[ index.row() ];
    switch ( role ) {
    case Qt::DisplayRole:
        switch ( index.column() ) {
        case EnableColumn:
            return breakpoint->isEnabled();
        case SourceColumn:
            return i18nc("@info/plain", "Line %1", breakpoint->lineNumber());
        case HitCountColumn:
            return breakpoint->hitCount();
        case ConditionColumn:
            return breakpoint->condition();
        case LastConditionResultColumn:
            return breakpoint->lastConditionResult().toString();
        default:
            kWarning() << "Unknown breakpoint model column" << index.column();
            break;
        }

    case Qt::EditRole:
        switch ( index.column() ) {
        case EnableColumn:
            return breakpoint->isEnabled();
        case ConditionColumn:
            return breakpoint->condition();
        default:
            break;
        }
    }

    return QVariant();
}

bool BreakpointModel::setData( const QModelIndex &index, const QVariant &value, int role )
{
    if ( !index.isValid() || index.row() >= m_breakpoints.count() ) {
        return false;
    }

    Breakpoint *breakpoint = m_breakpoints[ index.row() ];
    switch ( role ) {
    case Qt::EditRole:
        switch ( index.column() ) {
        case EnableColumn:
            kDebug() << "Update enabled state from" << breakpoint->isEnabled() << "to" << value.toBool();
            breakpoint->setEnabled( value.toBool() ); // Automatically emits dataChanged() / breakpointModified()
            return true;
        case ConditionColumn:
            breakpoint->setCondition( value.toString() );
            return true;
        }
    }

    return false;
}

Qt::ItemFlags BreakpointModel::flags( const QModelIndex &index ) const
{
    if ( !index.isValid() ) {
        return Qt::NoItemFlags;
    }

    const Column column = static_cast< Column >( index.column() );
    if ( column == EnableColumn ||  column == ConditionColumn ) {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    } else {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }
}

QList< Breakpoint * > BreakpointModel::breakpoints() const
{
    return m_breakpoints;
}

void BreakpointModel::applyChange( const BreakpointChange &change )
{
    switch ( change.type ) {
    case AddBreakpoint:
        addBreakpoint( new Breakpoint(change.breakpoint) );
        break;
    case RemoveBreakpoint:
        removeBreakpoint( breakpointFromLineNumber(change.breakpoint.lineNumber()) );
        break;
    case UpdateBreakpoint:
        updateBreakpoint( change.breakpoint );
        break;

    case NoOpBreakpointChange:
    default:
        break;
    }
}

void BreakpointModel::updateBreakpoint( const Breakpoint &breakpoint )
{
    if ( !breakpoint.isValid() ) {
        kDebug() << "Breakpoint is invalid" << breakpoint.lineNumber() << breakpoint.condition();
        return;
    }

    // Check if there already is a Breakpoint object for the line number
    if ( !m_breakpointsByLineNumber.contains(breakpoint.lineNumber()) ) {
        kDebug() << "No breakpoint found to update at line" << breakpoint.lineNumber();
        return;
    }

    Breakpoint *foundBreakpoint = m_breakpointsByLineNumber[ breakpoint.lineNumber() ];
    const QModelIndex index = indexFromBreakpoint( foundBreakpoint );
    foundBreakpoint->setValuesOf( breakpoint );
    emit dataChanged( index, index );
    emit breakpointModified( *foundBreakpoint );
}

void BreakpointModel::addBreakpoint( Breakpoint *breakpoint )
{
    if ( !breakpoint->isValid() ) {
        kDebug() << "Breakpoint is invalid" << breakpoint->lineNumber() << breakpoint->condition();
        return;
    }

    // Check if there already is a Breakpoint object for the line number
    if ( m_breakpointsByLineNumber.contains(breakpoint->lineNumber()) ) {
        updateBreakpoint( *breakpoint );
        return;
    }

    const int count = m_breakpoints.count();
    beginInsertRows( QModelIndex(), count, count );
    if ( breakpoint->model() ) {
        kWarning() << "Breakpoint already used in another model";
    }
    breakpoint->setModel( this );
    m_breakpoints << breakpoint;
    m_breakpointsByLineNumber.insert( breakpoint->lineNumber(), breakpoint );
    endInsertRows();

    emit breakpointAdded( *breakpoint );
    if ( count == 0 ) {
        emit emptinessChanged( false );
    }
}

void BreakpointModel::removeBreakpoint( Breakpoint *breakpoint )
{
    const int row = m_breakpoints.indexOf( breakpoint );

    if ( row == -1 ) {
        kWarning() << "Breakpoint not found";
        return;
    }

    removeRow( row );
}

bool BreakpointModel::removeRows( int row, int count, const QModelIndex &parent )
{
    // Collect breakpoints to be removed
    const bool wasEmpty = m_breakpoints.isEmpty();
    QList< Breakpoint* > removeBreakpoints;
    for ( int i = row; i < row + count; ++i ) {
        removeBreakpoints << m_breakpoints[i];
    }

    // Emit signals for breakpoints that get removed (before they get deleted)
    foreach ( Breakpoint *breakpoint, removeBreakpoints ) {
        emit breakpointAboutToBeRemoved( *breakpoint );
    }

    // Remove the breakpoints from the model
    beginRemoveRows( parent, row, row + count - 1 );
    for ( int i = row + count - 1; i >= row; --i ) {
        // Remove breakpoint from both collections and delete it
        Breakpoint *breakpoint = m_breakpoints.takeAt( i );
        m_breakpointsByLineNumber.remove( breakpoint->lineNumber() );
        delete breakpoint;
    }
    const bool isEmpty = m_breakpoints.isEmpty();
    endRemoveRows();

    if ( !wasEmpty && isEmpty ) {
        emit emptinessChanged( true );
    }
    return true;
}

void BreakpointModel::clear()
{
    removeRows( 0, m_breakpoints.count() );
}

QModelIndex BreakpointModel::index( int row, int column, const QModelIndex &parent ) const
{
    if ( parent.isValid() || row < 0 || row >= m_breakpoints.count() ||
         column < 0 || column >= ColumnCount )
    {
        return QModelIndex();
    } else {
        return createIndex( row, column, m_breakpoints[row] );
    }
}

QModelIndex BreakpointModel::indexFromBreakpoint( Breakpoint *breakpoint ) const
{
    const int row = m_breakpoints.indexOf( breakpoint );
    return row == -1 ? QModelIndex() : createIndex( row, 0, m_breakpoints[row] );
}


QModelIndex BreakpointModel::indexFromRow( int row ) const
{
    if ( row < 0 || row >= m_breakpoints.count() ) {
        return QModelIndex();
    } else {
        return createIndex( row, 0, m_breakpoints[row] );
    }
}

Breakpoint *BreakpointModel::breakpointFromLineNumber( int lineNumber ) const
{
    if ( m_breakpointsByLineNumber.contains(lineNumber) ) {
        return m_breakpointsByLineNumber[ lineNumber ];
    } else {
        return 0;
    }
}

Breakpoint *BreakpointModel::breakpointFromRow( int row ) const
{
    return m_breakpoints[ row ];
}

Breakpoint *BreakpointModel::breakpointFromIndex( const QModelIndex &index ) const
{
    return static_cast< Breakpoint* >( index.internalPointer() );
}

QList< uint > BreakpointModel::breakpointLineNumbers() const
{
    QList< uint > lineNumbers;
    foreach ( const Breakpoint *breakpoint, m_breakpoints ) {
        lineNumbers << breakpoint->lineNumber();
    }
    return lineNumbers;
}

Breakpoint *BreakpointModel::setBreakpoint( int lineNumber, bool enable )
{
    kDebug() << "setBreakpoint(" << lineNumber << "," << enable << ")";
    if ( lineNumber < 0 ) {
        return 0;
    }

    // Find a valid breakpoint line number near lineNumber (may be lineNumber itself)
//     lineNumber = getNextBreakableLineNumber( lineNumber );

    Breakpoint *breakpoint = 0;
    Breakpoint *foundBreakpoint = breakpointFromLineNumber( lineNumber );
    if ( foundBreakpoint && !enable ) {
        kDebug() << "Remove breakpoint at line" << lineNumber;
        removeBreakpoint( foundBreakpoint );
    } else if ( !foundBreakpoint && enable ) {
        kDebug() << "Add breakpoint at line" << lineNumber;
        breakpoint = new Breakpoint( lineNumber, enable );
        addBreakpoint( breakpoint );
    }

    return breakpoint;
}

Breakpoint *BreakpointModel::toggleBreakpoint( int lineNumber )
{
    kDebug() << "toggleBreakpoint(" << lineNumber << ")";
    Breakpoint::State breakpoint = breakpointState( lineNumber );
    return setBreakpoint( lineNumber, breakpoint == Breakpoint::NoBreakpoint );
}

Breakpoint::State BreakpointModel::breakpointState( int lineNumber ) const
{
    Breakpoint *foundBreakpoint = breakpointFromLineNumber( lineNumber );
    return !foundBreakpoint ? Breakpoint::NoBreakpoint
           : ( foundBreakpoint->isEnabled() ? Breakpoint::EnabledBreakpoint
                                            : Breakpoint::DisabledBreakpoint );
}

int BreakpointModel::rowCount( const QModelIndex &parent ) const
{
    return parent.isValid() ? 0 : m_breakpoints.count();
}

int BreakpointModel::columnCount( const QModelIndex &parent ) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

CheckboxDelegate::CheckboxDelegate( QAbstractItemView *itemView, QObject *parent )
        : KWidgetItemDelegate( itemView, parent )
{
    QTreeView *treeView = qobject_cast< QTreeView* >( itemView );
    if ( treeView ) {
        connect( treeView->header(), SIGNAL(sectionResized(int,int,int)),
                 this, SLOT(updateGeometry()) );
        connect( treeView->header(), SIGNAL(sectionMoved(int,int,int)),
                 this, SLOT(updateGeometry()) );
    }
}

CheckboxDelegate::~CheckboxDelegate()
{
}

void CheckboxDelegate::updateGeometry()
{
    // Make KWidgetItemDelegate call updateItemWidgets() when a section was resized or moved
    // by making it process resize events (the size needs to really change,
    // sending a QResizeEvent with the same size does not work)
    const QSize size = itemView()->size();
    itemView()->resize( size.width() + 1, size.height() );
    itemView()->resize( size );
}

QList< QWidget * > CheckboxDelegate::createItemWidgets() const
{
    QCheckBox *checkbox = new QCheckBox();
    setBlockedEventTypes( checkbox, QList<QEvent::Type>() << QEvent::MouseButtonPress
                            << QEvent::MouseButtonRelease << QEvent::MouseButtonDblClick );
    connect( checkbox, SIGNAL(toggled(bool)), this, SLOT(checkboxToggled(bool)) );
    return QList<QWidget*>() << checkbox;
}

void CheckboxDelegate::checkboxToggled( bool checked )
{
    const QModelIndex index = focusedIndex();
    if ( index.isValid() ) {
        emit checkedStateChanged( index, checked );
    }
}

void CheckboxDelegate::updateItemWidgets( const QList< QWidget * > widgets,
                                          const QStyleOptionViewItem &option,
                                          const QPersistentModelIndex &index ) const
{
    if ( widgets.isEmpty() ) {
        kDebug() << "No widgets";
        return;
    }

    QCheckBox *checkbox = qobject_cast< QCheckBox* >( widgets.first() );
    if ( itemView()->itemDelegate(index) != this ) {
        checkbox->hide();
        return;
    }

    const QVariant editData = index.data( Qt::EditRole );
    if ( !option.rect.isEmpty() && editData.isValid() && editData.canConvert(QVariant::Bool) ) {
        // Initialize checkbox
        checkbox->setChecked( editData.toBool() );

        // Align checkbox on the left and in the middle
        checkbox->move( (option.rect.width() - checkbox->width()) / 2,
                        (option.rect.height() - checkbox->height()) / 2 );

        // Resize checkbox to fit into option.rect and have maximally the size hint of the checkbox as size
        const QSize size = checkbox->sizeHint();
        checkbox->resize( qMin(size.width(), option.rect.width()),
                          qMin(size.height(), option.rect.height()) );

        checkbox->show();
    } else {
        checkbox->hide();
    }
}

QSize CheckboxDelegate::sizeHint( const QStyleOptionViewItem &option, const QModelIndex &index ) const
{
    Q_UNUSED( index );
    return QApplication::style()->subElementRect( QStyle::SE_CheckBoxIndicator, &option ).size();
}

void CheckboxDelegate::paint( QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index ) const
{
    QStyle* style = QApplication::style();
    const QVariant background = index.data( Qt::BackgroundRole );
    const QVariant foreground = index.data( Qt::ForegroundRole );
    QStyleOptionViewItemV4 opt( option );
    if ( background.isValid() ) {
        opt.backgroundBrush = background.value<QBrush>();
    }
    if ( foreground.isValid() ) {
        opt.palette.setColor( QPalette::Text, foreground.value<QBrush>().color() );
    }
    style->drawPrimitive( QStyle::PE_PanelItemViewItem, &opt, painter );
}
