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

#include <iostream>
#include <QMetaMethod>
#include <QFile>
#include <KDebug>

#include "../../engine/scripting.h"

QString cToQtScriptTypeName( const QString &cTypeName ) {
    if ( cTypeName == "QString" ) {
        return "string";
    } else if ( cTypeName == "QVariantMap" ) {
        return "object";
    } else if ( cTypeName == "QVariantList" || cTypeName == "QStringList" ||
                cTypeName.startsWith("QList") )
    {
        return "list";
    } else if ( cTypeName == "QDateTime" || cTypeName == "QDate" || cTypeName == "QTime" ) {
        return "date";
    } else if ( cTypeName == "QVariant" ) {
        return "any";
    } else if ( cTypeName == "NetworkRequest*" ) {
        return "NetworkRequest";
    } else if ( cTypeName == "Feature" ) {
        return "enum.feature"; // Enums are available under the enum object
    } else if ( cTypeName == "Hint" ) {
        return "enum.hint";
    } else if ( cTypeName == "int" || cTypeName == "uint" || cTypeName == "bool" ) {
        return cTypeName; // unchanged
    } else if ( cTypeName == "void" || cTypeName.isEmpty() ) {
        return "void";
    } else {
        kDebug() << "Type unknown" << cTypeName;
        return cTypeName;
    }
};

bool checkMethod( const QMetaMethod &method ) {
    return (method.access() != QMetaMethod::Public && method.methodType() == QMetaMethod::Method) ||
           method.methodType() == QMetaMethod::Constructor;
}

bool readUntil( QIODevice *dev, const QString &str ) {
    while ( !dev->atEnd() ) {
        const QString line = QString( dev->readLine() ).trimmed();
        if ( line.startsWith(str) ) {
            return true; // str found
        }
    }
    return false; // str not found
}

// Stores method names (lower case) as keys and method comments as values,
// the first string in the pair is the brief comment, the second string (list) are normal
// comment lines
typedef QHash< QString, QPair<QString, QStringList> > MethodComments;
MethodComments parseMethodDescriptions( const QString &className, const QString &sourceFilePath )
{
    QHash< QString, QPair<QString, QStringList> > methodDescriptions;
    if ( sourceFilePath.isEmpty() ) {
        return methodDescriptions;
    }

    QFile sourceFile( sourceFilePath );
    if ( !sourceFile.open(QIODevice::ReadOnly) ) {
        kDebug() << "Could not open source file" << sourceFilePath;
        return methodDescriptions;
    }

    bool found = readUntil( &sourceFile, QString("class %1 ").arg(className) );
    found = found && readUntil( &sourceFile, "public:" );
    if ( !found ) {
        kDebug() << "Did not find 'public:' line in class declaration of" << className;
        return methodDescriptions;
    }

    QRegExp returnsRegExp( "@returns?" );
    QRegExp paramRegExp( "@param (\\w+)" );
    QRegExp boldRegExp( "@b (\\w+)" );
    QRegExp italicsRegExp( "@em (\\w+)" );
    QRegExp commentRegExp( "@brief (.*)" );
    QRegExp commentStarCleanerRegExp( "^(\\*\\s*|/\\*\\*)" );
    QRegExp commentCleanerRegExp( "@p\\s+(\\w+)" );
    QRegExp methodSignatureRegExp( "(?:Q_INVOKABLE\\s+)?\\w+\\*?\\s*\\*?(\\w+)\\s*\\(" );
    while ( !sourceFile.atEnd() ) {
        // Read next line
        QString line = sourceFile.readLine();

        bool foundComment = false;
        while ( !sourceFile.atEnd() ) {
            if ( line.startsWith("};") ) {
                // End of class declaration found (important not to trim line before here)
                break;
            } else if ( line.trimmed().startsWith("/**") ) {
                foundComment = true;
                break;
            }

            // Read next line
            line = sourceFile.readLine();
        }
        if ( !foundComment ) {
            // Comment not found or class declaration closed
            break;
        }

        QString briefComment;
        QStringList comment;
        bool preserveLineBreaks = false;
        while ( !sourceFile.atEnd() ) {
            // Read one comment block
            line = line.trimmed();
//             if ( line.startsWith("@param") || line.startsWith("* @param") ) {
                // Beginning of first @param found, read until end of comment
//                         kDebug() << "Beginning of first @param found, read until end of comment";
//                 found = readUntil( &sourceFile, "**/" );
//                 break;
//             }
            if ( line.endsWith("*/") ) {
                // End of multiline comment found
//                         kDebug() << "End of multiline comment found";
                break;
            }

            if ( briefComment.isEmpty() && commentRegExp.indexIn(line) != -1 ) {
                briefComment = commentRegExp.cap(1)
                        .replace( commentCleanerRegExp, "<i>\\1</i>" )
                        .replace( "\"", "\\\"" );
            } else {
                QString commentLine = line.remove( commentStarCleanerRegExp );
                if ( commentLine.startsWith("@code") ) {
                    preserveLineBreaks = true;
                    comment << "<br><b>Code example:</b><br>";
                } else if ( commentLine.startsWith("@endcode") ) {
                    preserveLineBreaks = false;
                    comment << "<br>";
                } else if ( !commentLine.startsWith("@ingroup") &&
                            !commentLine.startsWith("@overload") )
                {
                    commentLine.replace( "@note", "<br><b>Note:</b> " )
                               .replace( "@since", "<br><b>Since:</b> " )
                               .replace( "@see", "<br><b>See also:</b> " )
                               .replace( "@li", "<br> <b>&bull;</b> " )
                               .replace( "@verbatim", "<pre>" )
                               .replace( "@endverbatim", "</pre>" )
                               .replace( returnsRegExp, "<br><b>Returns:</b>" )
                               .replace( boldRegExp, "<b>\\1</b>" )
                               .replace( italicsRegExp, "<i>\\1</i>" )
                               .replace( paramRegExp, "<br><b>Parameter <i>\\1</i>:</b>" )
                               .replace( commentCleanerRegExp, "<i>\\1</i>" );
                    if ( !commentLine.isEmpty() ) {
                        // Found a usable comment
                        // Escape quotation marks
                        commentLine.replace( "\"", "\\\"" );
                        if ( preserveLineBreaks ) {
                            commentLine.append( "<br>" );
                        }
                        comment << commentLine;
                    }
                }
            }

            // Read next line
            line = sourceFile.readLine();
        }

        if ( found ) {
            // Find method name directly after last found comment block
            line = sourceFile.readLine().trimmed();
            if ( methodSignatureRegExp.indexIn(line) != -1 ) {
                const QString methodName = methodSignatureRegExp.cap(1).toLower();
                if ( methodDescriptions.contains(methodName) ) {
                    kDebug() << "Method overload already added" << methodName;
                } else {
                    methodDescriptions.insert( methodName,
                            QPair<QString, QStringList>(briefComment, comment) );
                }
            } else {
                kDebug() << "Method signature not found after comment block in line" << line;
            }
        } else {
            kDebug() << "Error while parsing method comments";
            break;
        }
    }

    sourceFile.close();
    return methodDescriptions;
}

void writeMethods( QIODevice *dev, const QMetaObject &obj, const MethodComments &comments,
                   const QString &overwriteClassName = QString() )
{
    QStringList foundMethods;
    const QString className = overwriteClassName.isEmpty()
            ? QString(obj.className()).toLower() : overwriteClassName;
    for ( int i = obj.methodOffset(); i < obj.methodCount(); ++i ) {
        QMetaMethod method = obj.method( i );
        if ( checkMethod(method) ) {
            // Normal method is not public or method is a constructor
            continue;
        }
        const QString signature = method.signature();
        const QString name = signature.left( signature.indexOf('(') );
        if ( foundMethods.contains(name) ) {
            continue;
        }
        foundMethods << name;

        QString returnType = cToQtScriptTypeName( method.typeName() );
        QStringList typedParameters, templatedParameters;
        for ( int p = 0; p < method.parameterNames().count(); ++p ) {
            const QString type = cToQtScriptTypeName( method.parameterTypes()[p] );
            const QString name = method.parameterNames()[p];
            typedParameters << QString("%1 %2").arg(type).arg(name);
            templatedParameters << QString("${%1}").arg(name);
        }

        const QPair< QString, QStringList > comment = comments[ name.toLower() ];
        const QString indentation = "    ";
        const QString description = comment.second.isEmpty()
                ? QString("<b>Brief:</b> %1").arg(comment.first)
                : QString("<b>Brief:</b> %1\"\n%3        \"%2")
                  .arg(comment.first)
                  .arg(comment.second.join(QString(" \"\n%1        \"").arg(indentation)))
                  .arg(indentation);
        const QString newOutput = QString(
            "%7// Completion for %1.%2()\n"
            "%7completions->operator[](\"%1\").insert( \"call:%2()\", CompletionItem(\n"
            "%7        KTextEditor::CodeCompletionModel::Function,\n"
            "%7        \"%2(%3)\",\n"
            "%7        \"%6\",\n"
            "%7        \"%2(%4);\", true, \"%5\") );\n" )
            .arg( className ).arg( name ).arg( typedParameters.join(", ") )
            .arg( templatedParameters.join(", ") ).arg( returnType )
            .arg( description ).arg( indentation );
        dev->write( newOutput.toUtf8() );
    }

    dev->write( "\n" );
    kDebug() << "Found" << foundMethods.count() << "methods in" << className;
}

void writeMethodList( QIODevice *dev, const QMetaObject &obj,
                      const QString &overwriteClassName = QString() ) {
    const QString className = overwriteClassName.isEmpty()
            ? QString(obj.className()).toLower() : overwriteClassName;
    QStringList foundMethods;
    for ( int i = obj.methodOffset(); i < obj.methodCount(); ++i ) {
        QMetaMethod method = obj.method( i );
        if ( checkMethod(method) ) {
            // Normal method is not public or method is a constructor
            continue;
        }
        const QString signature = method.signature();
        const QString name = signature.left( signature.indexOf('(') );
        if ( foundMethods.contains(name) ) {
            continue;
        }
        foundMethods << name;
    }

    QString str;
    QString line = QString("    methods[\"%1\"] = QStringList()").arg(className);
    foreach ( const QString &method, foundMethods ) {
        if ( line.length() + method.length() + 16 > 100 ) {
            if ( !str.isEmpty() ) {
                str += "\n            ";
            }
            str += line;
            line.clear();
        }
        line += QString(" << \"%1\"").arg(method);
    }
    if ( !line.isEmpty() ) {
        if ( !str.isEmpty() ) {
            str += "\n            ";
        }
        str += line;
    }
    str += ";\n";
    dev->write( str.toUtf8() );
}

void parseDescriptionsAndWriteMethods( QIODevice *dev, const QMetaObject &obj,
        const QString &sourceFilePath = QString(), const QString &overwriteClassName = QString() )
{
    MethodComments comments = parseMethodDescriptions( obj.className(), sourceFilePath );
    writeMethods( dev, obj, comments, overwriteClassName );
}

int main( int argc, char **argv )
{
    const QString path = argc > 0 ? argv[1] : QString();
    kDebug() << path;
    const QString outputFileNameHeader( QString("%1/javascriptcompletiongeneric.h").arg(path) );
    QFile outputHeader( outputFileNameHeader );
    if ( !outputHeader.open(QIODevice::WriteOnly | QIODevice::Truncate) ) {
        kDebug() << "Could not open output header file";
        return -1;
    }
    outputHeader.write( "// This file was generated by TimetableMateCompletionGenerator\n\n"
            "#include <QHash>\n"
            "#include \"javascriptcompletionmodel.h\"\n"
            "\n"
            "class JavaScriptCompletionGeneric {\n"
            "public:\n"
            "    /**\n"
            "     * @brief Adds automatically generated completions.\n"
            "     **/\n"
            "    static void addCompletions( QHash< QString, QHash<QString, CompletionItem> > *completions );\n"
            "};" );
    outputHeader.close();

    const QString outputFileName( QString("%1/javascriptcompletiongeneric.cpp").arg(path) );
    QFile output( outputFileName );
    if ( !output.open(QIODevice::WriteOnly | QIODevice::Truncate) ) {
        kDebug() << "Could not open output file";
        return -1;
    }

    output.write( "// This file was generated by TimetableMateCompletionGenerator\n\n"
            "#include \"javascriptcompletiongeneric.h\"\n"
            "#include <QStringList>\n"
            "\n"
            "void JavaScriptCompletionGeneric::addCompletions( "
            "QHash< QString, QHash<QString, CompletionItem> > *completions ) {\n" );

    const QString pre = "../../../engine/";
    parseDescriptionsAndWriteMethods( &output, Helper::staticMetaObject, pre + "scripting.h" );
    parseDescriptionsAndWriteMethods( &output, Network::staticMetaObject, pre + "scripting.h" );
//     parseDescriptionsAndWriteMethods( &output, NetworkRequest::staticMetaObject, pre + "scripting.h" ); // no static name, gets dynamically generated in the script by using the 'network' object
    parseDescriptionsAndWriteMethods( &output, ResultObject::staticMetaObject, pre + "scripting.h",
                                      "result" );
    parseDescriptionsAndWriteMethods( &output, Storage::staticMetaObject, pre + "scripting.h" );

    output.write( "    // Methods supported per object:\n" );
    output.write( "    QHash< QString, QStringList > methods;\n" );
    writeMethodList( &output, Helper::staticMetaObject );
    writeMethodList( &output, Network::staticMetaObject );
    writeMethodList( &output, ResultObject::staticMetaObject, "result" );
    writeMethodList( &output, Storage::staticMetaObject );

    output.write ( "\n}" );

    output.close();
    return 0;
}
