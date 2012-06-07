/*
*   Copyright 2012 Friedrich Pülz <fieti1983@gmx.de>
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
    clear();
}

void JavaScriptModel::needTextHint( const KTextEditor::Cursor &position, QString &text )
{
    if ( !m_javaScriptCompletionModel ) {
        kDebug() << "No completion model created";
        return;
    }

    CodeNode::Ptr node = nodeFromLineNumber( position.line() + 1, position.column(), MatchChildren );
    if ( !node || node->line() != position.line() + 1 ) {
        return;
    }

    CompletionItem item = m_javaScriptCompletionModel->completionItemFromId( node->id() );
    if ( !item.isValid() || item.description.isEmpty() ) {
        kDebug() << "No completion item found for" << node->id();
        return;
    }

    text = QString("<table style='margin: 3px;'><tr><td style='font-size:large;'>"
            "<nobr>%1<b>%2</b></nobr><hr></td></tr><tr><td>%3</td></tr>")
            .arg(node->type() == Function ? i18n("Function: ") : QString(),
            item.name, item.description);

    // The KatePart only prints a debug message...
    emit showTextHint( position, text );
}

QModelIndex JavaScriptModel::indexFromNode( const CodeNode::Ptr &node ) const
{
    if ( !m_nodes.contains(node) ) {
        return QModelIndex();
    } else {
        return createIndex( m_nodes.indexOf(node), 0, node.data() );
    }
}

QModelIndex JavaScriptModel::indexFromNodePointer( CodeNode *nodePointer,
                                                   const CodeNode::Ptr &parent ) const
{
    if ( parent ) {
        const QList< CodeNode::Ptr > children = parent->children();
        for ( int row = 0; row < children.count(); ++row ) {
            const CodeNode::Ptr &node = children[ row ];
            if ( node.data() == nodePointer ) {
                return createIndex( row, 0, nodePointer );
            }

            const QModelIndex index = indexFromNodePointer( nodePointer, node );
            if ( index.isValid() ) {
                return index;
            }
        }
    } else {
        for ( int row = 0; row < m_nodes.count(); ++row ) {
            const CodeNode::Ptr &node = m_nodes[ row ];
            if ( node.data() == nodePointer ) {
                return createIndex( row, 0, nodePointer );
            }

            const QModelIndex index = indexFromNodePointer( nodePointer, node );
            if ( index.isValid() ) {
                return index;
            }
        }
    }

    // Node not found
    return QModelIndex();
}

CodeNode::Ptr JavaScriptModel::nodeFromNodePointer( CodeNode *nodePointer,
                                                    const CodeNode::Ptr &parent ) const
{
    if ( parent ) {
        const QList< CodeNode::Ptr > children = parent->children();
        foreach ( const CodeNode::Ptr &node, children ) {
            if ( node.data() == nodePointer ) {
                return node;
            }

            const CodeNode::Ptr childNode = nodeFromNodePointer( nodePointer, node );
            if ( childNode ) {
                return childNode;
            }
        }
    } else {
        foreach ( const CodeNode::Ptr &node, m_nodes ) {
            if ( node.data() == nodePointer ) {
                return node;
            }

            const CodeNode::Ptr childNode = nodeFromNodePointer( nodePointer, node );
            if ( childNode ) {
                return childNode;
            }
        }
    }

    // Node not found
    return CodeNode::Ptr( 0 );
}

CodeNode::Ptr JavaScriptModel::nodeFromIndex( const QModelIndex &index ) const
{
    if ( !index.isValid() ) {
        return CodeNode::Ptr( 0 );
    }

    if ( index.parent().isValid() ) {
        CodeNode *nodePointer = nodePointerFromIndex( index );
        CodeNode *parent = nodePointerFromIndex( index.parent() );
        const QList< CodeNode::Ptr > children = parent->children();
        for ( int row = 0; row < children.count(); ++row ) {
            const CodeNode::Ptr &node = children[ row ];
            if ( node.data() == nodePointer ) {
                return node;
            }
        }
        // Invalid index
        return CodeNode::Ptr( 0 );
    } else if ( index.row() >= m_nodes.count() ) {
        // Invalid index
        return CodeNode::Ptr( 0 );
    } else {
        return m_nodes[ index.row() ];
    }
}

CodeNode *JavaScriptModel::nodePointerFromIndex( const QModelIndex& index ) const
{
    return static_cast< CodeNode* >( index.internalPointer() );
}

CodeNode::Ptr JavaScriptModel::nodeFromRow( int row ) const
{
    return m_nodes.at( row );
}

CodeNode::Ptr JavaScriptModel::nodeFromLineNumber( int lineNumber, int column,
                                               MatchOptions matchOptions )
{
    foreach ( const CodeNode::Ptr &node, m_nodes ) {
        if ( /* !matchOptions.testFlag(MatchOnlyFirstRowOfSpanned) TODO Remove flag
            && */ node->isInRange(lineNumber, column) )
        {
            if ( matchOptions.testFlag(MatchChildren) ) {
                return nodeFromNodePointer( node->childFromPosition(lineNumber, column) );
            } else {
                return node;
            }
        }
    }

    return createAndAddEmptyNode();
}

CodeNode::Ptr JavaScriptModel::nodeBeforeLineNumber( int lineNumber, NodeTypes nodeTypes )
{
    CodeNode::Ptr lastFoundNode( 0 );
    foreach ( const CodeNode::Ptr &node, m_nodes ) {
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

    return lastFoundNode ? lastFoundNode : createAndAddEmptyNode().dynamicCast<CodeNode>();
}

CodeNode::Ptr JavaScriptModel::nodeAfterLineNumber( int lineNumber, NodeTypes nodeTypes )
{
    CodeNode::Ptr lastFoundNode( 0 );
    for ( int i = m_nodes.count() - 1; i >= 0; --i ) {
        CodeNode::Ptr node = m_nodes.at( i );
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
    return lastFoundNode ? lastFoundNode : createAndAddEmptyNode().dynamicCast<CodeNode>();
}

EmptyNode::Ptr JavaScriptModel::createAndAddEmptyNode()
{
    EmptyNode::Ptr node( 0 );
    if ( !m_nodes.isEmpty() ) {
        node = m_nodes.first().dynamicCast<EmptyNode>();
    }
    if ( !node ) {
        node = EmptyNode::Ptr( new EmptyNode );
        beginInsertRows( QModelIndex(), 0, 0 );
        m_nodes.prepend( node );
        updateFirstEmptyNodeName();
        endInsertRows();
    }

    return node;
}

QModelIndex JavaScriptModel::index( int row, int column, const QModelIndex& parent ) const
{
    if ( row < 0 || column < 0 ) {
        return QModelIndex();
    }

    if ( parent.isValid() ) {
        CodeNode *parentNode = nodePointerFromIndex( parent );
        if ( row >= parentNode->children().count() ) {
            return QModelIndex();
        }
        return createIndex( row, column, parentNode );
    } else {
        if ( row >= m_nodes.count() ) {
            return QModelIndex();
        }
        return createIndex( row, column, m_nodes[row].data() );
    }
}

int JavaScriptModel::rowCount( const QModelIndex &parent ) const
{
    if ( parent.isValid() ) {
        CodeNode *node = nodePointerFromIndex( parent );
        return node->children().count();
    } else {
        return m_nodes.count();
    }
}

QModelIndex JavaScriptModel::parent( const QModelIndex &child ) const
{
    CodeNode *childNode = nodePointerFromIndex( child );
    if ( childNode->parent() ) {
        return indexFromNodePointer( childNode->parent() );
    } else {
        return QModelIndex();
    }
}

QVariant JavaScriptModel::data( const QModelIndex& index, int role ) const
{
    CodeNode *item = nodePointerFromIndex( index );

    if ( role == Qt::UserRole ) {
        return item->type();
    }

    FunctionNode *function = dynamic_cast< FunctionNode* >( item );
    if ( function ) {
        switch ( role ) {
        case Qt::DisplayRole:
            return function->toStringSignature();
        case Qt::DecorationRole:
            return KIcon("code-function");
        }
    } else {
        EmptyNode *empty = dynamic_cast< EmptyNode* >( item );
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
        delete m_nodes.takeAt( row ).data();
    }
    endRemoveRows();

    updateFirstEmptyNodeName();
    return true;
}

void JavaScriptModel::clear()
{
    beginRemoveRows( QModelIndex(), 0, m_nodes.count() - 1 );
    foreach ( const CodeNode::Ptr &node, m_nodes ) {
        if ( node->parent() ) {
            kDebug() << "Toplevel node had a parent set!" << node << node->parent();
        }
    }
//     qDeleteAll( m_nodes );
    m_nodes.clear();
    endRemoveRows();
}

void JavaScriptModel::appendNodes( const QList< CodeNode::Ptr > nodes )
{
    beginInsertRows( QModelIndex(), m_nodes.count(), m_nodes.count() + nodes.count() - 1 );
    m_nodes << nodes;
    endInsertRows();

    updateFirstEmptyNodeName();
}

void JavaScriptModel::setNodes( const QList< CodeNode::Ptr > nodes )
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
    foreach ( const CodeNode::Ptr &node, m_nodes ) {
        FunctionNode::Ptr function = node.dynamicCast< FunctionNode >();
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

    EmptyNode::Ptr node = m_nodes.first().dynamicCast< EmptyNode >();
    if ( node ) {
        if ( m_nodes.count() == 1 ) {
            node->setText( i18nc("@info/plain", "(no functions)") );
        } else {
            node->setText( i18ncp("@info/plain", "%1 function:",
                                  "%1 functions:", m_nodes.count() - 1) );
        }
    }
}
