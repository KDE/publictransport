/*
*   Copyright 2010 Friedrich Pülz <fieti1983@gmx.de>
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

#include "javascriptmodel.h"
#include "javascriptparser.h"
#include "javascriptcompletionmodel.h"

#include <KDebug>
#include <KLocalizedString>
#include <KIcon>
#include <KTextEditor/Cursor>

JavaScriptModel::JavaScriptModel( QObject* parent ) : QAbstractItemModel( parent ),
        m_javaScriptCompletionModel(0)
{
}

JavaScriptModel::~JavaScriptModel()
{
    qDeleteAll( m_nodes );
}

void JavaScriptModel::needTextHint( const KTextEditor::Cursor &position, QString &text )
{
    if ( !m_javaScriptCompletionModel ) {
        return;
    }

    CodeNode *node = nodeFromLineNumber( position.line() + 1, position.column(), MatchChildren );
    if ( !node || node->line() != position.line() + 1 ) {
        return;
    }

    CompletionItem item = m_javaScriptCompletionModel->completionItemFromId( node->id() );
    if ( !item.isValid() || item.description.isEmpty() ) {
        return;
    }

    text = QString("<table style='margin: 3px;'><tr><td style='font-size:large;'>"
            "<nobr>%1<b>%2</b></nobr><hr></td></tr><tr><td>%3</td></tr>")
            .arg(node->type() == Function ? i18n("Function: ") : QString(),
            item.name, item.description);

    // The KatePart only prints a debug message...
    emit showTextHint( position, text );
}

QModelIndex JavaScriptModel::indexFromNode( CodeNode* node ) const
{
    if ( !m_nodes.contains(node) ) {
        return QModelIndex();
    } else {
        return createIndex( m_nodes.indexOf(node), 0, node );
    }
}

CodeNode *JavaScriptModel::nodeFromIndex( const QModelIndex& index ) const
{
    return static_cast<CodeNode*>( index.internalPointer() );
}

CodeNode *JavaScriptModel::nodeFromRow( int row ) const
{
    return m_nodes.at( row );
}

CodeNode *JavaScriptModel::nodeFromLineNumber( int lineNumber, int column,
                                               MatchOptions matchOptions )
{
    foreach ( CodeNode *node, m_nodes ) {
        if ( /* !matchOptions.testFlag(MatchOnlyFirstRowOfSpanned) TODO REMOVE FLAG
            && */ node->isInRange(lineNumber, column) )
        {
            if ( matchOptions.testFlag(MatchChildren) ) {
                return node->childFromPosition( lineNumber, column );
            } else {
                return node;
            }
        }
    }

    return createAndAddEmptyNode();
}

CodeNode *JavaScriptModel::nodeBeforeLineNumber( int lineNumber, NodeTypes nodeTypes )
{
    CodeNode *lastFoundNode = NULL;
    foreach ( CodeNode *node, m_nodes ) {
        if ( node->type() == NoNodeType || !nodeTypes.testFlag(node->type()) ) {
            continue;
        }

        if ( node->line() < lineNumber ) {
            lastFoundNode = node;
        } else if ( node->line() > lineNumber ) {
            break;
        }

        if ( lineNumber >= node->line() && lineNumber <= node->endLine() ) {
            return node;
        }
    }

    return lastFoundNode ? lastFoundNode : createAndAddEmptyNode();
}

CodeNode *JavaScriptModel::nodeAfterLineNumber( int lineNumber, NodeTypes nodeTypes )
{
    CodeNode *lastFoundNode = NULL;
    for ( int i = m_nodes.count() - 1; i >= 0; --i ) {
        CodeNode *node = m_nodes.at( i );
        if ( node->type() == NoNodeType || !nodeTypes.testFlag(node->type()) ) {
            continue;
        }

        if ( node->line() > lineNumber ) {
            lastFoundNode = node;
        } else if ( node->line() < lineNumber ) {
            break;
        }

        if ( lineNumber >= node->line() && lineNumber <= node->endLine() ) {
            return node;
        }
    }
    return lastFoundNode ? lastFoundNode : createAndAddEmptyNode();
}

EmptyNode *JavaScriptModel::createAndAddEmptyNode()
{
    EmptyNode *node = NULL;
    if ( !m_nodes.isEmpty() ) {
        node = dynamic_cast<EmptyNode*>( m_nodes.first() );
    }
    if ( !node ) {
        node = new EmptyNode;
        beginInsertRows( QModelIndex(), 0, 0 );
        m_nodes.insert( 0, node );
        updateFirstEmptyNodeName();
        endInsertRows();
    }

    return node;
}

QModelIndex JavaScriptModel::index( int row, int column, const QModelIndex& parent ) const
{
    Q_UNUSED( parent );
    if ( !hasIndex(row, column, QModelIndex()) ) {
        return QModelIndex();
    }

    if ( row >= 0 && row < m_nodes.count() ) {
        return createIndex( row, column, m_nodes[row] );
    } else {
        return QModelIndex();
    }
}

QVariant JavaScriptModel::data( const QModelIndex& index, int role ) const
{
    CodeNode *item = static_cast<CodeNode*>( index.internalPointer() );

    if ( role == Qt::UserRole ) {
        return item->type();
    }

    FunctionNode *function = dynamic_cast<FunctionNode*>( item );
    if ( function ) {
        switch ( role ) {
        case Qt::DisplayRole:
            return function->toStringSignature();
        case Qt::DecorationRole:
            return KIcon("code-function");
        }
    } else {
        EmptyNode *empty = dynamic_cast<EmptyNode*>( item );
        if ( empty && role == Qt::DisplayRole ) {
            return empty->text();
        }
    }

    return QVariant();
}

bool JavaScriptModel::removeRows( int row, int count, const QModelIndex& parent )
{
    beginRemoveRows( parent, row, row + count - 1 );
    for ( int i = 0; i < count; ++i ) {
        CodeNode *item = m_nodes.takeAt( row );
        delete item;
    }
    endRemoveRows();

    updateFirstEmptyNodeName();
    return true;
}

void JavaScriptModel::clear()
{
    beginRemoveRows( QModelIndex(), 0, m_nodes.count() );
    qDeleteAll( m_nodes );
    m_nodes.clear();
    endRemoveRows();
}

void JavaScriptModel::appendNodes( const QList< CodeNode* > nodes )
{
    beginInsertRows( QModelIndex(), m_nodes.count(), m_nodes.count() + nodes.count() - 1 );
    m_nodes << nodes;
    endInsertRows();

    updateFirstEmptyNodeName();
}

void JavaScriptModel::setNodes( const QList< CodeNode* > nodes )
{
    clear();

    beginInsertRows( QModelIndex(), 0, nodes.count() - 1 );
    m_nodes = nodes;
    endInsertRows();

    updateFirstEmptyNodeName();
}

QStringList JavaScriptModel::functionNames() const
{
    QStringList functions;
    foreach ( CodeNode *node, m_nodes ) {
        FunctionNode *function = dynamic_cast<FunctionNode*>( node );
        if ( function ) {
            functions << function->text();
        }
    }
    return functions;
}

void JavaScriptModel::updateFirstEmptyNodeName()
{
    if ( m_nodes.isEmpty() ) { 
        return;
    }

    EmptyNode *node = dynamic_cast<EmptyNode*>( m_nodes.first() );
    if ( node ) {
        if ( m_nodes.count() == 1 ) {
            node->setText( i18nc("@info/plain", "(no functions)") );
        } else {
            node->setText( i18ncp("@info/plain", "%1 function:",
                                  "%1 functions:", m_nodes.count() - 1) );
        }
    }
}
