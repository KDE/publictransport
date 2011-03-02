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

#ifndef JAVASCRIPTMODEL_HEADER
#define JAVASCRIPTMODEL_HEADER

#include <QAbstractItemModel>
#include <QStringList>
#include "parserenums.h"

namespace KTextEditor {
    class Cursor;
};
class EmptyNode;
class CodeNode;
class JavaScriptCompletionModel;
class JavaScriptModel : public QAbstractItemModel {
    Q_OBJECT
    public:
	enum MatchOption {
	    NoOptions = 0x0000,
	    MatchOnlyFirstRowOfSpanned = 0x0001, /**< Matches nodes that span multiple lines 
						    * only if the first line is searched. */
	    MatchChildren = 0x0002 /**< Goes down the hierarchy to search for nodes. */
	};
	Q_DECLARE_FLAGS( MatchOptions, MatchOption );
	
	JavaScriptModel( QObject *parent = 0 );
	~JavaScriptModel();

	void setJavaScriptCompletionModel( JavaScriptCompletionModel *javaScriptCompletionModel ) {
	    m_javaScriptCompletionModel = javaScriptCompletionModel;
	};
	
	void clear();
	void appendNodes( const QList<CodeNode*> nodes );
	void setNodes( const QList<CodeNode*> nodes );

	QStringList functionNames() const;

	virtual int columnCount( const QModelIndex& parent = QModelIndex() ) const {
	    Q_UNUSED( parent );
	    return 1; };
	virtual int rowCount( const QModelIndex& parent = QModelIndex() ) const {
	    Q_UNUSED( parent );
	    return m_nodes.count(); };
	virtual QModelIndex parent( const QModelIndex& child ) const {
	    Q_UNUSED( child );
	    return QModelIndex(); };
	virtual QModelIndex index( int row, int column,
				   const QModelIndex& parent = QModelIndex() ) const;
	virtual QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const;
	virtual bool removeRows( int row, int count, const QModelIndex& parent = QModelIndex() );

	QModelIndex indexFromNode( CodeNode *node ) const;
	
	CodeNode *nodeFromIndex( const QModelIndex &index ) const;
	CodeNode *nodeFromRow( int row ) const;
	/** @returns the node at the given @p lineNumber or NULL if there is no node at that line.
	* @param lineNumber The line number at which the node is.
	* @param matchOnlyFirstRowOfSpanned If false, nodes that span multiple lines are matched 
	*   if @p lineNumber is between the first line and the last line of that node, including 
	*   the first and last lines. If true, such nodes are only matched if @p lineNumber is 
	*   there first line, such as the signature of a function. */
	CodeNode *nodeFromLineNumber( int lineNumber, int column = -1,
				      MatchOptions matchOptions = NoOptions );
	/** @returns the first node before the given @p lineNumber.
	* @note This requires all nodes to be sorted by line number
	* as returned by @ref JavaScriptModel::parse. */
	CodeNode *nodeBeforeLineNumber( int lineNumber, NodeTypes nodeTypes = AllNodeTypes );
	/** @returns the first node after the given @p lineNumber.
	* @note This requires all nodes to be sorted by line number
	* as returned by @ref JavaScriptModel::parse. */
	CodeNode *nodeAfterLineNumber( int lineNumber, NodeTypes nodeTypes = AllNodeTypes );

    signals:
	void showTextHint( const KTextEditor::Cursor &position, QString &text );
	
    public slots:
	void needTextHint( const KTextEditor::Cursor &position, QString &text );

    private:
	// Sets the name of the first empty node (including the number of nodes), if any.
	void updateFirstEmptyNodeName();
	EmptyNode *createAndAddEmptyNode();
	
	QList< CodeNode* > m_nodes;
	JavaScriptCompletionModel *m_javaScriptCompletionModel;
};
Q_DECLARE_OPERATORS_FOR_FLAGS( JavaScriptModel::MatchOptions );

#endif // Multiple inclusion guard