/*
 *   Copyright 2011 Friedrich Pülz <fieti1983@gmx.de>
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

#ifndef PARSERENUMS_HEADER
#define PARSERENUMS_HEADER

#include <QtCore/qnamespace.h>

/** @brief Different types of code nodes. */
enum NodeType {
    NoNodeType      = 0x0000, /**< For example an empty line in global space,
                                 * not associated with any node. */

    Block           = 0x0001, /**< A code block, enclosed by '{' and '}'. */
    Function        = 0x0002, /**< A function definition. */
    Argument        = 0x0004, /**< An argument of a function definition. */
    Statement       = 0x0008, /**< An unknown statement. */
    Comment         = 0x0010, /**< A comment (single or multiline). */
    String          = 0x0020, /**< A string (' or ") or regular expression. */
    FunctionCall    = 0x0040, /**< A function call. */
    Bracketed       = 0x0080, /**< A node containing a list of child nodes that have been read
                                 * inside a pair of brackets ('(' or '[').. */
    UnknownNodeType = 0x0100, /**< An unknown node. */

    AllNodeTypes    = Block | Function | Argument | Statement | Comment | String |
                      FunctionCall | Bracketed | UnknownNodeType
};
Q_DECLARE_FLAGS( NodeTypes, NodeType );
Q_DECLARE_OPERATORS_FOR_FLAGS( NodeTypes );

#endif // Multiple inclusion guard
