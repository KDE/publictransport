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

#ifndef JAVASCRIPTPARSER_HEADER
#define JAVASCRIPTPARSER_HEADER

// Own includes
#include "parserenums.h"

// Qt includes
#include <QList>
#include <QSharedPointer>
#include <KDebug>

class ChildListNode;
class FunctionNode;
namespace KTextEditor {
    class Cursor;
};
class QString;
class QStringList;

/** @brief Base class of all code nodes. */
class CodeNode {
    // Friends, to set m_parent
    friend class ChildListNode;
    friend class FunctionNode;
    friend class FunctionCallNode;
public:
    typedef QSharedPointer<CodeNode> Ptr;

    CodeNode( const QString &text, int line, int colStart, int colEnd );
    virtual ~CodeNode() {};

    /**
     * @returns the ID of this code node.
     *
     * May be used for @ref JavaScriptCompletionModel::completionItemFromId.
     **/
    virtual QString id() const { return m_id; };

    /**
     * @returns a list of all child nodes. The default implementation returns an empty list.
     *
     * @note All children that get returned by this function on destruction get deleted automatically.
     **/
    virtual QList< CodeNode::Ptr > children() const { return QList< CodeNode::Ptr >(); };

    /**
     * @brief Finds the child node at the given @p lineNumber and @p column.
     *
     * @returns the found child node. If no child node was found, this node is returned if the
     *   given @p lineNumber and @p column are in it's range. Otherwise NULL is returned.
     *
     * @see isInRange
     **/
    CodeNode *childFromPosition( int lineNumber, int column ) const;

    /**
     * @returns whether or not the given @p lineNumber and @p column are in the range of this node.
     **/
    bool isInRange( int lineNumber, int column = -1 ) const;

    /** @returns the parent node of this node, if any. Otherwise NULL is returned. */
    virtual CodeNode *parent() const { return m_parent; };

    /**
     * @brief Searches up the hierarchy for a code node of the template type, if any.
     * The returned code node can be this code node, it's direct parent or a parent further up
     * in the hierarchy.
     * @see searchDown
     **/
    template< class T >
    T *searchUp( int maxLevels = -1 ) const {
        CodeNode *node = const_cast<CodeNode*>( this );
        T *castedNode = dynamic_cast< T* >( node );
        return castedNode ? castedNode
                : (parent() && maxLevels != 0
                   ? parent()->searchUp<T>(maxLevels == -1 ? -1 : --maxLevels) : 0);
    };

    /** @returns the top level parent node of this node. If this node has no parent it is
     * returned itself. */
    CodeNode *topLevelParent() const;

    virtual QString toString( bool shortString = false ) const {
        Q_UNUSED( shortString ); return QString("CodeNode Type %1").arg(type()); };

    /** @brief The type of this node. */
    virtual NodeType type() const = 0;

    QString text() const { return m_text; };

    /** @brief The first line of this node. */
    int line() const { return m_line; };

    /**
     * @brief The last line of this node.
     *
     * The default implementation returns the same as @ref line().
     **/
    virtual int endLine() const { return m_line; };

    /** @returns true, if this node is spanned over multiple lines. */
    bool isMultiline() const { return m_line != endLine(); };

    /** @brief The first column of this node in it's first line. */
    int column() const { return m_col; };

    /** @brief The last column of this node in it's last line. */
    int columnEnd() const { return m_colEnd; };

protected:
    QString m_id;
    QString m_text;
    int m_line;
    int m_col;
    int m_colEnd;
    CodeNode *m_parent;
};

/** @brief Base class for all nodes that may span multiple lines. */
class MultilineNode : public CodeNode {
public:
    typedef QSharedPointer<MultilineNode> Ptr;

    MultilineNode( const QString &text, int line, int colStart, int lineEnd, int colEnd );

    /** @brief The last line of this node. */
    virtual int endLine() const { return m_endLine; };

protected:
    int m_endLine;
};

/** @brief Base class for all nodes that have use a single list of child nodes. */
class ChildListNode : public MultilineNode {
    friend class CodeNode; // Friends, to set m_parent
public:
    typedef QSharedPointer<ChildListNode> Ptr;

    ChildListNode( const QString &text, int line, int colStart,
                   int lineEnd, int colEnd, const QList< CodeNode::Ptr > &children );

    /** @returns a list of all child nodes. */
    virtual QList< CodeNode::Ptr > children() const { return m_children; };

protected:
    QList< CodeNode::Ptr > m_children;
};

/**
 * @brief A node representing an empty part in code, eg. only whitespace.
 **/
class EmptyNode : public CodeNode {
public:
    typedef QSharedPointer<EmptyNode> Ptr;

    EmptyNode();

    virtual NodeType type() const { return NoNodeType; };
    virtual QString id() const { return QString(); };
    void setText( const QString &text ) { m_text = text; };
};

/**
 * @brief An unknown code node.
 **/
class UnknownNode : public CodeNode {
public:
    typedef QSharedPointer<UnknownNode> Ptr;

    UnknownNode( const QString &text, int line, int colStart = 0, int colEnd = 0 )
        : CodeNode( text, line, colStart, colEnd ) {
    };
    virtual NodeType type() const { return UnknownNodeType; };
};

/**
 * @brief A node representing a comment in code.
 *
 * A comment can be spanned across multiple lines. Get the comment string (without comment markers)
 * using @ref content.
 **/
class CommentNode : public MultilineNode {
public:
    typedef QSharedPointer<CommentNode> Ptr;

    CommentNode( const QString &text, int line, int colStart, int lineEnd, int colEnd )
        : MultilineNode( text, line, colStart, lineEnd, colEnd ) {
    };

    /** @brief Gets the contents of the comment (without comment markers). */
    QString content() const { return m_text; };

    virtual NodeType type() const { return Comment; };
    virtual QString toString( bool shortString = false ) const {
        Q_UNUSED( shortString );
        return isMultiline() ? "/*" + m_text + "*/" : "//" + m_text;
    };
};

/** @brief A string or regular expression. */
class StringNode : public CodeNode {
public:
    typedef QSharedPointer<StringNode> Ptr;

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

/** @brief An unknown statement. */
class StatementNode : public ChildListNode {
public:
    typedef QSharedPointer<StatementNode> Ptr;

    StatementNode( const QString &text, int line, int colStart, int lineEnd, int colEnd,
                   const QList< CodeNode::Ptr > &children );

    virtual NodeType type() const { return Statement; };
    virtual QString toString( bool shortString = false ) const {
        Q_UNUSED( shortString );
        return m_text;
    };
};

/** @brief A node containing a list of child nodes that have been read inside a pair of brackets
 * ('(' or '['). */
class BracketedNode : public ChildListNode {
public:
    typedef QSharedPointer<BracketedNode> Ptr;

    BracketedNode( const QChar &openingBracketChar, const QString &text, int line, int colStart,
                   int lineEnd, int colEnd, const QList< CodeNode::Ptr > &children );

    QString content() const { return m_text; };

    virtual NodeType type() const { return Bracketed; };
    virtual QString toString( bool shortString = false ) const {
        Q_UNUSED( shortString );
        return m_bracketChar + m_text + closingBracketChar();
    };
    QList< CodeNode::Ptr > commaSeparated( int pos ) const;
    int commaSeparatedCount() const;

    /** @returns the opening bracket character. */
    QChar openingBracketChar() const { return m_bracketChar; };

    /** @returns the closing bracket character. */
    QChar closingBracketChar() const;

private:
    QChar m_bracketChar;
};

/** @brief A function call in this form: "object.function(...)". */
class FunctionCallNode : public CodeNode {
    friend class CodeNode; // Friends, to set m_parent
public:
    typedef QSharedPointer<FunctionCallNode> Ptr;

    FunctionCallNode( const QString &object, const QString &function,
                      int line, int colStart, int colEnd, const BracketedNode::Ptr &arguments );
    virtual NodeType type() const { return FunctionCall; };
    virtual QString id() const { return "call:" + m_text; };
    virtual QString toString( bool shortString = false ) const {
        return QString( "%1(%2)" ).arg( m_text, m_arguments->toString(shortString) );
    };

    virtual QList< CodeNode::Ptr > children() const {
            return QList< CodeNode::Ptr >() << m_arguments.dynamicCast<CodeNode>(); };
    BracketedNode::Ptr arguments() const { return m_arguments; };
    QString object() const { return m_object; };
    QString function() const { return m_function; };

private:
    BracketedNode::Ptr m_arguments;
    QString m_object;
    QString m_function;
};

/** @brief A code block, enclosed by '{' and '}'. */
class BlockNode : public ChildListNode {
public:
    typedef QSharedPointer<BlockNode> Ptr;

    BlockNode( int line, int colStart, int lineEnd, int colEnd,
               const QList< CodeNode::Ptr > &children );

    virtual NodeType type() const { return Block; };
    virtual QString toString( bool shortString = false ) const;
    virtual QString content() const;
};

/** @brief An argument of a function definition. */
class ArgumentNode : public CodeNode {
public:
    typedef QSharedPointer<ArgumentNode> Ptr;

    ArgumentNode( const QString &text, int line, int colStart, int colEnd )
            : CodeNode( text, line, colStart, colEnd ) {};

    virtual NodeType type() const { return Argument; };
    virtual QString id() const { return "arg:" + m_text; };
    virtual QString toString( bool shortString = false ) const {
        Q_UNUSED( shortString );
        return m_text;
    };
};

// TODO allow argument definitions like "var argument" and "var argument = 3"
/** @brief A function definition. */
class FunctionNode : public MultilineNode {
    friend class CodeNode; // Friends, to set m_parent
public:
    typedef QSharedPointer<FunctionNode> Ptr;

    FunctionNode( const QString &text, int line, int colStart, int colEnd,
                  const QList< ArgumentNode::Ptr > &arguments, const BlockNode::Ptr &definition );

    virtual NodeType type() const { return Function; };
    virtual QString id() const;
    virtual QString toString( bool shortString = false ) const;
    QString toStringSignature() const;
    virtual QList< CodeNode::Ptr > children() const;
    virtual QList< ArgumentNode::Ptr > arguments() const { return m_arguments; };
    virtual BlockNode::Ptr definition() const { return m_definition; };
    inline QString name() const { return text(); };

private:
    QList< ArgumentNode::Ptr > m_arguments;
    BlockNode::Ptr m_definition;
};

/** @brief Parses java script code. */
class JavaScriptParser {
public:
    /** @brief Creates a new parser object and parses the given @p code.
     * @see nodes
     * @see hasError */
    JavaScriptParser( const QString &code );

    virtual ~JavaScriptParser();

    /** @returns the code given in the constructor. */
    QString code() const { return m_code; };

    /** @returns the parsed list of nodes. */
    QList< CodeNode::Ptr > nodes() const { return m_nodes; };

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

    /** @brief Whether or not @p text names a java script keyword. */
    bool isKeyword( const QString &text );

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

    QList< CodeNode::Ptr > parse();
    CodeNode::Ptr parseComment();
    CodeNode::Ptr parseString();
    CodeNode::Ptr parseBracketed();
    CodeNode::Ptr parseFunction(); // Parse a function declaration (not a function call)
    BlockNode::Ptr parseBlock();
    CodeNode::Ptr parseStatement();

    inline bool atEnd() const { return m_it == m_token.constEnd(); };
    inline Token *currentToken() const {
        Q_ASSERT( !atEnd() );
        return *m_it;
    };
    inline QString whitespaceSinceLastToken() const {
        return Token::whitespacesBetween( m_lastToken, currentToken() );
    };
    bool tryMoveToNextToken();
    void moveToNextToken();

    void clearError();
    void setErrorState( const QString &errorMessage, int errorLine = -1, int m_errorColumn = 0,
                int affectedLine = -1 );
    void checkFunctionCall( const QString &object, const QString &function,
                const BracketedNode::Ptr &bracketedNode, int line, int column );

private:
    QString m_code;
    QList< CodeNode::Ptr > m_nodes;

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
