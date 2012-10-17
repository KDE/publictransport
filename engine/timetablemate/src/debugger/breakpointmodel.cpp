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
#include <QFileInfo>

using namespace Debugger;

BreakpointModel::BreakpointModel( QObject *parent )
        : QAbstractListModel( parent )
{
}

BreakpointModel::~BreakpointModel()
{
    for ( QHash<QString, BreakpointData>::Iterator it = m_breakpointsByFile.begin();
          it != m_breakpointsByFile.end(); ++it )
    {
        qDeleteAll( it->breakpoints );
    }
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
    if ( !index.isValid() || index.row() >= rowCount(index.parent()) ) {
        return QVariant();
    }

    Breakpoint *breakpoint = breakpointFromRow( index.row() );
    switch ( role ) {
    case Qt::DisplayRole:
        switch ( index.column() ) {
        case EnableColumn:
            return breakpoint->isEnabled();
        case SourceColumn:
            return QString("%1:%2").arg( QFileInfo(breakpoint->fileName()).fileName() )
                                   .arg( breakpoint->lineNumber() );
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
    if ( !index.isValid() || index.row() >= rowCount(index.parent()) ) {
        return false;
    }

    Breakpoint *breakpoint = breakpointFromRow( index.row() );
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

QList< Breakpoint * > BreakpointModel::breakpoints( const QString &fileName ) const
{
    return m_breakpointsByFile[ fileName ].breakpoints;
}

QHash< uint, Breakpoint * > BreakpointModel::breakpointsByLineNumber( const QString &fileName ) const
{
    return m_breakpointsByFile[ fileName ].breakpointsByLineNumber;
}

void BreakpointModel::applyChange( const BreakpointChange &change )
{
    switch ( change.type ) {
    case AddBreakpoint:
        addBreakpoint( new Breakpoint(change.breakpoint) );
        break;
    case RemoveBreakpoint:
        removeBreakpoint( breakpointFromLineNumber(change.breakpoint.fileName(),
                                                   change.breakpoint.lineNumber()) );
        break;
    case UpdateBreakpoint:
        updateBreakpoint( change.breakpoint );
        break;

    case NoOpBreakpointChange:
    default:
        break;
    }
}

bool BreakpointModel::hasBreakpoint( int lineNumber ) const
{
    for ( QHash<QString, BreakpointData>::ConstIterator it = m_breakpointsByFile.constBegin();
          it != m_breakpointsByFile.constEnd(); ++it )
    {
        if ( it->breakpointsByLineNumber.contains(lineNumber) ) {
            return true;
        }
    }
    return false;
}

bool BreakpointModel::hasBreakpoint( const QString &fileName, int lineNumber ) const
{
    return m_breakpointsByFile[ fileName ].breakpointsByLineNumber.contains( lineNumber );
}

void BreakpointModel::updateBreakpoint( const Breakpoint &breakpoint )
{
    if ( !breakpoint.isValid() ) {
        kDebug() << "Breakpoint is invalid" << breakpoint.lineNumber() << breakpoint.condition();
        return;
    }

    // Check if there already is a Breakpoint object for the line number
    if ( !hasBreakpoint(breakpoint.fileName(), breakpoint.lineNumber()) ) {
        kDebug() << "No breakpoint found to update at line" << breakpoint.lineNumber();
        return;
    }

    Breakpoint *foundBreakpoint = breakpointFromLineNumber( breakpoint.fileName(),
                                                            breakpoint.lineNumber() );
    const QModelIndex index = indexFromBreakpoint( foundBreakpoint );
    foundBreakpoint->setValuesOf( breakpoint );
    emit dataChanged( index, index );
    emit breakpointModified( *foundBreakpoint );
}

void BreakpointModel::addBreakpoint( Breakpoint *breakpoint )
{
    kDebug() << "Add breakpoint in" << QFileInfo(breakpoint->fileName()).fileName()
             << "at line" << breakpoint->lineNumber();
    if ( !breakpoint->isValid() ) {
        kDebug() << "Breakpoint is invalid" << breakpoint->lineNumber() << breakpoint->condition();
        return;
    }

    // Check if there already is a Breakpoint object for the line number
    if ( hasBreakpoint(breakpoint->fileName(), breakpoint->lineNumber()) ) {
        updateBreakpoint( *breakpoint );
        return;
    }

    const int count = rowCount();
    beginInsertRows( QModelIndex(), count, count );
    if ( breakpoint->model() ) {
        kWarning() << "Breakpoint already used in another model";
    }
    breakpoint->setModel( this );
    BreakpointData &data = m_breakpointsByFile[ breakpoint->fileName() ];
    data.breakpoints << breakpoint;
    data.breakpointsByLineNumber.insert( breakpoint->lineNumber(), breakpoint );
    endInsertRows();

    emit breakpointAdded( *breakpoint );
    if ( count == 0 ) {
        emit emptinessChanged( false );
    }
}

void BreakpointModel::removeBreakpoint( Breakpoint *breakpoint )
{
    kDebug() << "Remove breakpoint in" << QFileInfo(breakpoint->fileName()).fileName()
             << "at line" << breakpoint->lineNumber();
    int row = indexFromBreakpoint( breakpoint ).row();
    if ( row == -1 ) {
        kWarning() << "Breakpoint not found";
        return;
    }

    removeRow( row );
}

bool BreakpointModel::removeRows( int row, int count, const QModelIndex &parent )
{
    if ( isEmpty() || parent.isValid() ) {
        return false;
    }

    // Collect breakpoints to be removed
    QList< Breakpoint* > removeBreakpoints;
    int _row = row;
    int _count = count;
    for ( QHash<QString, BreakpointData>::ConstIterator it = m_breakpointsByFile.constBegin();
          it != m_breakpointsByFile.constEnd(); ++it )
    {
        if ( _row < it->breakpoints.count() ) {
            QList< Breakpoint* > newRemoveBreakpoints = it->breakpoints.mid( _row, _count );
            _count -= newRemoveBreakpoints.count();
            _row = 0;
            removeBreakpoints << newRemoveBreakpoints;

            if ( _count == 0 ) {
                break;
            }
        } else if ( _row > 0 ) {
            _row -= it->breakpoints.count();
        }
    }

    // Emit signals for breakpoints that get removed (before they get deleted)
    foreach ( Breakpoint *breakpoint, removeBreakpoints ) {
        emit breakpointAboutToBeRemoved( *breakpoint );
    }

    // Remove the breakpoints from the model
    beginRemoveRows( parent, row, row + count - 1 );
    BreakpointData *data = 0;
    foreach ( Breakpoint *breakpoint, removeBreakpoints ) {
        if ( !data || data->breakpoints.first()->fileName() != breakpoint->fileName() ) {
            data = &m_breakpointsByFile[ breakpoint->fileName() ];
        }

        data->breakpoints.removeOne( breakpoint );
        data->breakpointsByLineNumber.remove( breakpoint->lineNumber() );
        delete breakpoint;
    }
    endRemoveRows();

    if ( isEmpty() ) {
        emit emptinessChanged( true );
    }
    return true;
}

void BreakpointModel::clear()
{
    removeRows( 0, rowCount() );
}

QModelIndex BreakpointModel::index( int row, int column, const QModelIndex &parent ) const
{
    if ( parent.isValid() || row < 0 || row >= rowCount(parent) ||
         column < 0 || column >= ColumnCount )
    {
        return QModelIndex();
    } else {
        return createIndex( row, column, breakpointFromRow(row) );
    }
}

QModelIndex BreakpointModel::indexFromBreakpoint( Breakpoint *breakpoint ) const
{
    int row = 0;
    for ( QHash<QString, BreakpointData>::ConstIterator it = m_breakpointsByFile.constBegin();
          it != m_breakpointsByFile.constEnd(); ++it )
    {
        int r = it->breakpoints.indexOf( breakpoint );
        if ( r != -1 ) {
            row += r;
            break;
        } else {
            row += it->breakpoints.count();
        }
    }

    return row == -1 ? QModelIndex() : createIndex( row, 0, breakpoint );
}


QModelIndex BreakpointModel::indexFromRow( int row ) const
{
    Breakpoint *breakpoint = breakpointFromRow( row );
    if ( !breakpoint ) {
        return QModelIndex();
    } else {
        return createIndex( row, 0, breakpoint );
    }
}

Breakpoint *BreakpointModel::breakpointFromLineNumber( const QString &fileName, int lineNumber ) const
{
    if ( !m_breakpointsByFile.contains(fileName) ) {
        return 0;
    }

    const BreakpointData &data = m_breakpointsByFile[ fileName ];
    if ( data.breakpointsByLineNumber.contains(lineNumber) ) {
        return data.breakpointsByLineNumber[ lineNumber ];
    } else {
        return 0;
    }
}

Breakpoint *BreakpointModel::breakpointFromRow( int row ) const
{
    for ( QHash<QString, BreakpointData>::ConstIterator it = m_breakpointsByFile.constBegin();
          it != m_breakpointsByFile.constEnd(); ++it )
    {
        if ( row < it->breakpoints.count() ) {
            return it->breakpoints[ row ];
        } else {
            row -= it->breakpoints.count();
        }
    }
    return 0;
}

Breakpoint *BreakpointModel::breakpointFromIndex( const QModelIndex &index ) const
{
    return static_cast< Breakpoint* >( index.internalPointer() );
}

QList< uint > BreakpointModel::breakpointLineNumbers( const QString &fileName ) const
{
    return m_breakpointsByFile[ fileName ].breakpointsByLineNumber.keys();
}

Breakpoint *BreakpointModel::setBreakpoint( const QString &fileName, int lineNumber, bool enable )
{
    kDebug() << "setBreakpoint(" << QFileInfo(fileName).fileName() << ","
             << lineNumber << "," << enable << ")";
    if ( lineNumber < 0 ) {
        return 0;
    }

    // Find a valid breakpoint line number near lineNumber (may be lineNumber itself)
//     lineNumber = getNextBreakableLineNumber( lineNumber );

    Breakpoint *breakpoint = 0;
    Breakpoint *foundBreakpoint = breakpointFromLineNumber( fileName, lineNumber );
    if ( foundBreakpoint && !enable ) {
        removeBreakpoint( foundBreakpoint );
    } else if ( !foundBreakpoint && enable ) {
        breakpoint = new Breakpoint( fileName, lineNumber, enable );
        addBreakpoint( breakpoint );
    }

    return breakpoint;
}

Breakpoint *BreakpointModel::toggleBreakpoint( const QString &fileName, int lineNumber )
{
    kDebug() << "toggleBreakpoint(" << QFileInfo(fileName).fileName() << "," << lineNumber << ")";
    Breakpoint::State breakpoint = breakpointState( fileName, lineNumber );
    return setBreakpoint( fileName, lineNumber, breakpoint == Breakpoint::NoBreakpoint );
}

Breakpoint::State BreakpointModel::breakpointState( const QString &fileName, int lineNumber ) const
{
    Breakpoint *foundBreakpoint = breakpointFromLineNumber( fileName, lineNumber );
    return !foundBreakpoint ? Breakpoint::NoBreakpoint
           : ( foundBreakpoint->isEnabled() ? Breakpoint::EnabledBreakpoint
                                            : Breakpoint::DisabledBreakpoint );
}

int BreakpointModel::rowCount( const QModelIndex &parent ) const
{
    if ( parent.isValid() ) {
        return 0;
    } else {
        int count = 0;
        for ( QHash<QString, BreakpointData>::ConstIterator it = m_breakpointsByFile.constBegin();
            it != m_breakpointsByFile.constEnd(); ++it )
        {
            count += it->breakpoints.count();
        }
        return count;
    }
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
