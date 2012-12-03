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

// Own include
#include "documentationparser.h"
#include "outputgenerator.h"

// KDE includes
#include <KCmdLineArgs>
#include <KAboutData>

// Qt includes
#include <QBuffer>

static const char description[] =
    I18N_NOOP("A helper application to parse documentation from source files with classes exposed "
              "to scripts and generate HTML documentation and source files for code completion "
              "in TimetableMate");

static const char version[] = "0.2";

void silentMessageOutput( QtMsgType type, const char *msg );
void normalMessageOutput( QtMsgType type, const char *msg );

int main( int argc, char **argv )
{
    // Use KCmdLineOptions to easily parse arguments
    KAboutData about("completiongenerator", 0, ki18n("CompletionGenerator"), version,
                     ki18n(description), KAboutData::License_GPL_V2,
                     ki18n("(C) 2010 Friedrich Pülz"), KLocalizedString(), 0, "fpuelz@gmx.de");
    about.addAuthor( ki18n("Friedrich Pülz"), KLocalizedString(), "fpuelz@gmx.de" );
    KCmdLineArgs::init( argc, argv, &about );

    // Add options
    KCmdLineOptions options;
    options.add( "out_completion <argument>",
                 ki18n("Output path for code completion source files.") );
    options.add( "out_doc <argument>",
                 ki18n("Output path for HTML documentation files.") );
    options.add( "completion_class_name <argument>",
                 ki18n("Name of the generated class for code completion"),
                 "JavaScriptCompletionGeneric" );
    options.add( "input_script <argument>", ki18n("Script API input file path."),
                 "../../../../engine/script/scriptapi.h" );
    options.add( "input_script_doc <argument>", ki18n("Global script API documentation input file path."),
                 "../../../../engine/script/scriptapi-doc.h" );
    options.add( "input_engine <argument>", ki18n("Engine input file path."),
                 "../../../../engine/engine-doc.h" );
    options.add( "input_enum <argument>", ki18n("Engine enum file path."),
                 "../../../../engine/enums.h" );
    options.add( "silent", ki18n("Print out no warnings/debug messages.") );
    options.add( "verbose", ki18n("Print out all debug messages / warnings.") );
    KCmdLineArgs::addCmdLineOptions( options );

    // Get output paths from options
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    const QString completionOutputPath = args->isSet("out_completion")
            ? args->getOption("out_completion") : QDir::currentPath();
    const QString documentationOutputPath = args->isSet("out_doc")
            ? args->getOption("out_doc") : QDir::currentPath();
    const QString inputScriptFilePath = args->getOption("input_script");
    const QString inputScriptDocFilePath = args->getOption("input_script_doc");
    const QString inputEngineFilePath = args->getOption("input_engine");
    const QString inputEnumFilePath = args->getOption("input_enum");
    const QString completionClassName = args->getOption("completion_class_name");
    const bool silent = args->isSet("silent");
    const bool verbose = args->isSet("verbose");
    args->clear();

    // Check arguments
    if ( !QDir(completionOutputPath).exists() ) {
        qFatal( "The completion output path (--out_completion) does not exist: %s",
                completionOutputPath.toUtf8().data() );
    }
    if ( !QDir(documentationOutputPath).exists() ) {
        qFatal( "The documentation output path (--out_doc) does not exist: %s",
                documentationOutputPath.toUtf8().data() );
    }
    if ( !QFile(inputScriptFilePath).exists() ) {
        qFatal( "The input file (--input_script) does not exist: %s",
                inputScriptFilePath.toUtf8().data() );
    }
    if ( !QFile(inputScriptDocFilePath).exists() ) {
        qFatal( "The input file (--input_script_doc) does not exist: %s",
                inputScriptDocFilePath.toUtf8().data() );
    }
    if ( !QFile(inputEngineFilePath).exists() ) {
        qFatal( "The input file (--input_engine) does not exist: %s",
                inputEngineFilePath.toUtf8().data() );
    }
    if ( !QFile(inputEnumFilePath).exists() ) {
        qFatal( "The input file (--input_enum) does not exist: %s",
                inputEnumFilePath.toUtf8().data() );
    }
    if ( completionClassName.isEmpty() ) {
        qFatal( "The completion class name cannot be empty (--completion_class_name)" );
    }
    if ( silent && verbose ) {
        qFatal( "Cannot combine silent and verbose (--silent and --verbose)" );
    }

    // Install muting message handler
    if ( silent ) {
        qInstallMsgHandler( silentMessageOutput );
    } else if ( !verbose ) {
        qInstallMsgHandler( normalMessageOutput );
    }

    // Create information objects
    DocumentationParser parser( inputScriptFilePath );
    parser.addClass( Helper::staticMetaObject, "helper" );
    parser.addClass( ResultObject::staticMetaObject, "result" );
    parser.addClass( Network::staticMetaObject, "network" );
    parser.addClass( NetworkRequest::staticMetaObject );
    parser.addClass( Storage::staticMetaObject, "storage" );
    parser.addClass( DataStreamPrototype::staticMetaObject, "DataStream" );
    parser.parse();

    // Parse global script API documentation
    Comments scriptApiDocumentation;
    QFile scriptDocFile( inputScriptDocFilePath );
    if ( !scriptDocFile.open(QIODevice::ReadOnly) ) {
        qWarning() << "Could not open engine source file" << scriptDocFile.errorString();
    } else {
        scriptApiDocumentation << DocumentationParser::parseGlobalDocumentation( &scriptDocFile );
        scriptDocFile.close();
    }

    EnumCommentList enumDocumentation;
    DocumentationParser enumParser( inputEnumFilePath );
    enumParser.addClass( Enums::staticMetaObject, "PublicTransport" );
    enumParser.parse();
    foreach ( const ClassInformation &classInformation, enumParser.classInformations() ) {
        if ( classInformation.className == QLatin1String("Enums") ) {
            enumDocumentation = classInformation.sortedEnums;
        }
    }

    // Extract "provider_infos_xml" section from the global documentation
    QFile engineFile( inputEngineFilePath );
    Comments engineComments;
    if ( !engineFile.open(QIODevice::ReadOnly) ) {
        qWarning() << "Could not open engine source file" << engineFile.errorString();
    } else {
        QByteArray data = engineFile.readAll();
        engineFile.close();

        const int startPos = data.indexOf( "@section provider_plugin_pts" );
        int endPos = data.indexOf( "@section", startPos + 27 );
        if ( startPos == -1 ) {
            qWarning() << "Did not find the section with the ID 'provider_plugin_pts'";
        } else {
            if ( endPos == -1 ) {
                endPos = data.length();
            }

            data = "/**\n" + data.mid( startPos, endPos - startPos - 1 ) + "\n*/\n";
            QBuffer buffer( &data );
            engineComments << DocumentationParser::parseGlobalDocumentation( &buffer );
        }
    }

    DocumentationParser parserEnum( inputEnumFilePath );
    parserEnum.addClass( Enums::staticMetaObject, "PublicTransport" );
    parserEnum.parse();

    // Initialize generator
    qDebug() << "Initialize generators";
    OutputGenerator completionGenerator( new CompletionOutputGenerator() );
    OutputGenerator documentationGenerator( new DocumentationOutputGenerator() );

    // Write C++ source files for code completion
    qDebug() << "Write C++ source files for code completion";  // TODO no NetworkRequest here?
    completionGenerator.writeCompletionSource( parser.classInformations(), completionOutputPath,
                                               completionClassName );

    // Write HTML documentation files
    qDebug() << "Write HTML documentation files";
    documentationGenerator.writeDocumentation( parser.classInformations(),
                                               Comments() << scriptApiDocumentation << engineComments,
                                               enumDocumentation, documentationOutputPath );
    return 0;
}

void normalMessageOutput( QtMsgType type, const char *msg )
{
    switch( type ) {
    case QtDebugMsg:
        // Mute qDebug()
        break;
    case QtWarningMsg:
        fprintf( stderr, "Warning: %s\n", msg );
        break;
    case QtCriticalMsg:
        fprintf( stderr, "Critical: %s\n", msg );
        break;
    case QtFatalMsg:
        fprintf( stderr, "Fatal: %s\n", msg );
        abort();
    }
}

void silentMessageOutput( QtMsgType type, const char *msg )
{
    switch( type ) {
    case QtDebugMsg:
    case QtWarningMsg:
        // Mute qDebug() and qWarning()
        break;
    case QtCriticalMsg:
        fprintf( stderr, "Critical: %s\n", msg );
        break;
    case QtFatalMsg:
        fprintf( stderr, "Fatal: %s\n", msg );
        abort();
    }
}
