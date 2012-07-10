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
#include "backtracemodel.h"

// Own includes
#include "debuggerstructures.h"
#include "debugger.h"

// KDE includes
#include <KDebug>
#include <KLocalizedString>

using namespace Debugger;

BacktraceModel::BacktraceModel( QObject *parent )
        : QAbstractListModel( parent )
{
}

BacktraceModel::~BacktraceModel()
{
    qDeleteAll( m_frames );
}

QVariant BacktraceModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
    if ( orientation == Qt::Horizontal && role == Qt::DisplayRole ) {
        switch ( static_cast<Column>(section) ) {
        case DepthColumn:
            return i18nc("@title:column", "Depth");
        case FunctionColumn:
            return i18nc("@title:column", "Function");
        case SourceColumn:
            return i18nc("@title:column", "Source");
        default:
            return QVariant();
        }
    }

    return QVariant();
}

QVariant BacktraceModel::data( const QModelIndex &index, int role ) const
{
    if ( !index.isValid() || index.row() >= m_frames.count() ) {
        return QVariant();
    }

    Frame *frame = m_frames[ rowToIndex(index.row()) ];
    switch ( role ) {
    case Qt::DisplayRole:
        switch ( index.column() ) {
        case DepthColumn:
            return rowToIndex( m_frames.indexOf(frame) );
        case FunctionColumn:
            return frame->contextString();
        case SourceColumn: {
            const int lineNumber = frame->lineNumber() == -1
                    ? frame->contextInfo().functionStartLineNumber() : frame->lineNumber();
            if ( lineNumber == -1 ) {
                if ( frame->fileName().isEmpty() ) {
                    return "??";
                } else {
                    return frame->fileName();
                }
            } else {
                return QString("%0: %1").arg(frame->fileName()).arg(lineNumber);
            }
        } break;
        default:
            kWarning() << "Unknown backtrace model column" << index.column();
            break;
        }
    }

    return QVariant();
}

Qt::ItemFlags BacktraceModel::flags( const QModelIndex &index ) const
{
//     const Column column = static_cast< Column >( index.column() );
    return !index.isValid() ? Qt::NoItemFlags : Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void BacktraceModel::applyChange( const BacktraceChange &change )
{
    switch ( change.type ) {
    case PushBacktraceFrame:
        pushFrame( new Frame() );
        break;
    case PopBacktraceFrame:
        popFrame();
        break;
    case UpdateBacktraceFrame: {
        if( m_frames.isEmpty() ) {
            kWarning() << "Trying to update current backtrace frame, but the model is empty";
            pushFrame( new Frame() );
        }

        m_frames.top()->setValuesOf( change.frame );
    } break;
    default:
        kWarning() << "BacktraceChange type not implemented" << change.type;
        break;
    }
}

void BacktraceModel::frameChanged( Frame *frame )
{
    QModelIndex frameIndex = indexFromFrame( frame );
    if ( !frameIndex.isValid() ) {
        kWarning() << "Frame not found" << frame;
        return;
    }

    dataChanged( frameIndex, index(frameIndex.row(), ColumnCount - 1) );
}

FrameStack BacktraceModel::frameStack() const
{
    return m_frames;
}

void BacktraceModel::pushFrame( Frame *frame )
{
    beginInsertRows( QModelIndex(), m_frames.count(), m_frames.count() );
    frame->setModel( this );
    m_frames.push( frame );
    endInsertRows();
}

void BacktraceModel::popFrame()
{
    if ( m_frames.isEmpty() ) {
        kDebug() << "Model is empty!";
        return;
    }

    removeRow( 0 );
}

Frame *Debugger::BacktraceModel::topFrame()
{
    return m_frames.isEmpty() ? 0 : m_frames.top();
}

bool BacktraceModel::removeRows( int row, int count, const QModelIndex &parent )
{
    // Remove the frames from the model
    row = rowToIndex( row );
    beginRemoveRows( parent, row, row + count - 1 );
    if ( row == 0 ) {
        for ( int i = 0; i < count; ++i ) {
            delete m_frames.pop();
        }
    } else {
        // Remove frames from the middle or beginning of the backtrace
        for ( int i = row - count + 1; i >= row; --i ) {
            // Remove frame and delete it
            Frame *frame = m_frames[ i ];
            m_frames.remove( i );
            delete frame;
        }
    }
    endRemoveRows();
    return true;
}

void BacktraceModel::clear()
{
    removeRows( 0, m_frames.count() );
}

bool BacktraceModel::isEmpty()
{
    return m_frames.isEmpty();
}

QModelIndex BacktraceModel::index( int row, int column, const QModelIndex &parent ) const
{
    if ( parent.isValid() || row < 0 || row >= m_frames.count() ||
         column < 0 || column >= ColumnCount )
    {
        return QModelIndex();
    } else {
        return createIndex( row, column, m_frames[rowToIndex(row)] );
    }
}

QModelIndex BacktraceModel::indexFromFrame( Frame *frame ) const
{
    const int row = m_frames.indexOf( frame );
    return row == -1 ? QModelIndex() : createIndex( row, 0, m_frames[rowToIndex(row)] );
}


QModelIndex BacktraceModel::indexFromRow( int row ) const
{
    if ( row < 0 || row >= m_frames.count() ) {
        return QModelIndex();
    } else {
        return createIndex( row, 0, m_frames[rowToIndex(row)] );
    }
}

Frame *BacktraceModel::frameFromRow( int row ) const
{
    return m_frames[ rowToIndex(row) ];
}

Frame *BacktraceModel::frameFromIndex( const QModelIndex &index ) const
{
    return static_cast< Frame* >( index.internalPointer() );
}

int BacktraceModel::rowCount( const QModelIndex &parent ) const
{
    return parent.isValid() ? 0 : m_frames.count();
}

int BacktraceModel::columnCount( const QModelIndex &parent ) const
{
    return parent.isValid() ? 0 : ColumnCount;
}
