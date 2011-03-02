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

#ifndef JAVASCRIPTPARSER_HEADER
#define JAVASCRIPTPARSER_HEADER

#include <QString>
#include <QList>
#include "parserenums.h"

class ChildListNode;
class FunctionNode;
/** Base class of all code nodes. */
class CodeNode {
    friend class ChildListNode; // Friends, to set m_parent
    friend class FunctionNode; // Friends, to set m_parent
    public:
	CodeNode( const QString &text, int line, int colStart, int colEnd );
	virtual ~CodeNode();

	/** @returns the ID of this code node.
	* May be used for @ref JavaScriptCompletionModel::completionItemFromId. */
	virtual QString id() const { return m_text; };

	/** @returns a list of all child nodes. The default implementation returns an empty list.
	* All childs get deleted automatically on destruction. */
	virtual QList<CodeNode*> children() const { return QList<CodeNode*>(); };

	/** Finds the child node at the given @p lineNumber and @p column.
	* @returns the found child node. If no child node was found, this node is returned if the
	* given @p lineNumber and @p column are in it's range. Otherwise NULL is returned. 
	* @see isInRange */
	CodeNode *childFromPosition( int lineNumber, int column ) const;
	/** @returns whether or not the given @p lineNumber and @p column are in the range
	* of this node. */
	bool isInRange( int lineNumber, int column = -1 ) const;

	/** @returns the parent node of this node, if any. Otherwise NULL is returned. */
	virtual CodeNode *parent() const { return m_parent; };
	/** @returns the top level parent node of this node. If this node has no parent it is 
	* returned itself. */
	CodeNode *topLevelParent() const;
	virtual QString toString( bool shortString = false ) const {
	    Q_UNUSED( shortString ); return QString("CodeNode Type %1").arg(type()); };

	/** The type of this node. */
	virtual NodeType type() const = 0;
	QString text() const { return m_text; };
	/** The first line of this node. */
	int line() const { return m_line; };
	/** The last line of this node. 
	* The default implementation returns the same as @ref line(). */
	virtual int endLine() const { return m_line; };
	/** @returns true, if this node is spanned over multiple lines. */
	bool isMultiline() const { return m_line != endLine(); };
	/** The first column of this node in it's first line. */
	int column() const { return m_col; };
	/** The last column of this node in it's last line. */
	int columnEnd() const { return m_colEnd; };

    protected:
	QString m_text;
	int m_line;
	int m_col;
	int m_colEnd;
	CodeNode *m_parent;
};

/** Base class for all nodes that may span multiple lines. */
class MultilineNode : public CodeNode {
    public:
	MultilineNode( const QString &text, int line, int colStart,
		       int lineEnd, int colEnd );
	
	/** The last line of this node. */
	virtual int endLine() const { return m_endLine; };

    protected:
	int m_endLine;
};

/** Base class for all nodes that have use a single list of child nodes. */
class ChildListNode : public MultilineNode {
    friend class CodeNode; // Friends, to set m_parent
    public:
	ChildListNode( const QString &text, int line, int colStart,
		       int lineEnd, int colEnd, const QList<CodeNode*> &children );

	/** @returns a list of all child nodes. */
	virtual QList<CodeNode*> children() const { return m_children; };

    protected:
	QList<CodeNode*> m_children;
};

class EmptyNode : public CodeNode {
    public:
	EmptyNode();
	
	virtual NodeType type() const { return NoNodeType; };
	virtual QString id() const { return QString(); };
	void setText( const QString &text ) { m_text = text; };
};

class UnknownNode : public CodeNode {
    public:
	UnknownNode( const QString &text, int line, int colStart = 0, int colEnd = 0 )
		: CodeNode( text, line, colStart, colEnd ) {
	};
	virtual NodeType type() const { return Unknown; };
};

/** A comment. */
class CommentNode : public MultilineNode {
    public:
	CommentNode( const QString &text, int line, int colStart, int lineEnd, int colEnd )
		: MultilineNode( text, line, colStart, lineEnd, colEnd ) {
	};

	QString content() const { return m_text; };

	virtual NodeType type() const { return Comment; };
	virtual QString toString( bool shortString = false ) const {
	    Q_UNUSED( shortString );
	    return isMultiline() ? "/*" + m_text + "*/" : "//" + m_text; };
};

/** A string or regular expression. */
class StringNode : public CodeNode {
    public:
	StringNode( const QString &text, int line, int colStart, int colEnd )
		: CodeNode( text, line, colStart, colEnd ) {
	};

	QString content() const { return m_text; };

	virtual NodeType type() const { return String; };
	virtual QString id() const { return "str:" + m_text; };
	virtual QString toString( bool shortString = false ) const {
	    Q_UNUSED( shortString );
	    return "\"" + m_text + "\"";
	};
};

/** An unknown statement. */
class StatementNode : public ChildListNode {
    public:
	StatementNode( const QString &text, int line, int colStart, int lineEnd, int colEnd,
		       const QList<CodeNode*> &children );

	virtual NodeType type() const { return Statement; };
	virtual QString toString( bool shortString = false ) const {
	    Q_UNUSED( shortString );
	    return "Statement: " + m_text;
	};
};

/** A node containing a list of child nodes that have been read inside a pair of brackets 
* ('(' or '['). */
class BracketedNode : public ChildListNode {
    public:
	BracketedNode( const QChar &openingBracketChar, const QString &text,
		       int line, int colStart, int lineEnd, int colEnd,
		       const QList<CodeNode*> &children );

	QString content() const { return m_text; };

	virtual NodeType type() const { return Bracketed; };
	virtual QString toString( bool shortString = false ) const {
	    Q_UNUSED( shortString );
	    return m_bracketChar + m_text + closingBracketChar();
	};
	QList<CodeNode*> commaSeparated( int pos ) const;
	int commaSeparatedCount() const;

	/** @returns the opening bracket character. */
	QChar openingBracketChar() const { return m_bracketChar; };
	/** @returns the closing bracket character. */
	QChar closingBracketChar() const;

    private:
	QChar m_bracketChar;
};

/** A function call in this form: "object.function(...)". */
class FunctionCallNode : public CodeNode {
    public:
	FunctionCallNode( const QString &object, const QString &function,
			  int line, int colStart, int colEnd, BracketedNode *arguments );

	virtual NodeType type() const { return FunctionCall; };
	virtual QString id() const { return "call:" + m_text; };
	virtual QString toString( bool shortString = false ) const {
	    return QString( "%1(%2)" ).arg( m_text, m_arguments->toString(shortString) );
	};

	virtual QList<CodeNode*> children() const { return QList<CodeNode*>() << m_arguments; };
	BracketedNode *arguments() const { return m_arguments; };
	QString object() const { return m_object; };
	QString function() const { return m_function; };

    private:
	BracketedNode *m_arguments;
	QString m_object;
	QString m_function;
};

/** A code block, enclosed by '{' and '}'. */
class BlockNode : public ChildListNode {
    public:
	BlockNode( int line, int colStart, int lineEnd, int colEnd,
		   const QList<CodeNode*> &children );

	virtual NodeType type() const { return Block; };
	virtual QString toString( bool shortString = false ) const;
};

/** An argument of a function definition. */
class ArgumentNode : public CodeNode {
    public:
	ArgumentNode( const QString &text, int line, int colStart, int colEnd )
		    : CodeNode( text, line, colStart, colEnd ) {};

	virtual NodeType type() const { return Argument; };
	virtual QString id() const { return "arg:" + m_text; };
	virtual QString toString( bool shortString = false ) const {
	    Q_UNUSED( shortString );
	    return m_text;
	};
};

/** A function definition. */
class FunctionNode : public MultilineNode {
    friend class CodeNode; // Friends, to set m_parent
    public:
	FunctionNode( const QString &text, int line, int colStart, int colEnd,
		      const QList<ArgumentNode*> &arguments, BlockNode *definition );

	virtual NodeType type() const { return Function; };
	virtual QString id() const;
	virtual QString toString( bool shortString = false ) const;
	QString toStringSignature() const;
	virtual QList<CodeNode*> children() const;
	virtual QList<ArgumentNode*> arguments() const { return m_arguments; };
	virtual BlockNode* definition() const { return m_definition; };

    private:
	QList<ArgumentNode*> m_arguments;
	BlockNode *m_definition;
};

namespace KTextEditor {
    class Cursor;
};
/** Parses java script code. */
class JavaScriptParser {
    public:
	/** Creates a new parser object and parses the given @p code.
	* @see nodes
	* @see hasError */
	JavaScriptParser( const QString &code ) {
	    m_code = code;
	    m_hasError = false;
	    m_errorLine = -1;
	    m_errorColumn = 0;
	    m_nodes = parse();
	};

	/** @returns the code given in the constructor. */
	QString code() const { return m_code; };
	/** @returns the parsed list of nodes. */
	QList<CodeNode*> nodes() const { return m_nodes; };

	/** Wheather or not there was an error while parsing. */
	bool hasError() const { return m_hasError; };
	/** @returns a message for the error, if any.
	* @see hasError */
	QString errorMessage() const { return m_errorMessage; };
	/** @returns the line of the error, if any.
	* @see hasError */
	int errorLine() const { return m_errorLine; };
	/** @returns a second affected line of the error, if any.
	* @see hasError */
	int errorAffectedLine() const { return m_errorAffectedLine; };
	/** @returns the column of the error, if any.
	* @see hasError */
	int errorColumn() const { return m_errorColumn; };
	/** @returns the cursor of the error, if any.
	* @see hasError */
	KTextEditor::Cursor errorCursor() const;

    private:
	struct Token {
	    // Constructs an invalid Token
	    Token() { };

	    Token( const QString &text, int line, int posStart, int posEnd, bool isName = false ) {
		this->text = text;
		this->isName = isName;
		this->line = line;
		this->posStart = posStart;
		this->posEnd = posEnd;
	    };

	    bool isValid() const { return !text.isEmpty(); };
	    bool isChar( const QChar &ch ) const { return text.length() == 1 && text.at(0) == ch; };
	    static QString whitespacesBetween( const Token *token1, const Token *token2 );

	    QString text;
	    bool isName;
	    int line;
	    int posStart, posEnd;
	};

	QList<CodeNode*> parse();
	CommentNode *parseComment();
	StringNode *parseString();
	BracketedNode *parseBracketed();
	FunctionNode *parseFunction();
	BlockNode *parseBlock();
	StatementNode *parseStatement();

	inline bool atEnd() const { return m_it == m_token.constEnd(); };
	inline Token *currentToken() const {
	    Q_ASSERT( !atEnd() );
	    return *m_it; };
	inline QString whitespaceSinceLastToken() const {
	    return Token::whitespacesBetween( m_lastToken, currentToken() );
	};
	bool tryMoveToNextToken();
	void moveToNextToken();

	void clearError();
	void setErrorState( const QString &errorMessage, int errorLine = -1, int m_errorColumn = 0,
			    int affectedLine = -1 );
	void checkFunctionCall( const QString &object, const QString &function,
				BracketedNode *bracketedNode, int line, int column );

    private:
	QString m_code;
	QList<CodeNode*> m_nodes;

	QList<Token*> m_token;
	QList<Token*>::const_iterator m_it;
	Token *m_lastToken;

	bool m_hasError;
	QString m_errorMessage;
	int m_errorLine;
	int m_errorAffectedLine;
	int m_errorColumn;
};

#endif // Multiple inclusion guard
