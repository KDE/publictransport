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
#include "serviceproviderdatatester.h"

// Own includes
#include "testmodel.h"
#include "project.h"

// Public Transport engine includes
#include <engine/serviceproviderdata.h>

// KDE includes
#include <KLocalizedString>
#include <KDebug>

TestModel::TestState  ServiceProviderDataTester::runServiceProviderDataTest( TestModel::Test test,
        const QString &text, QString *errorMessage, QString *tooltip )
{
    switch ( test ) {
    case TestModel::ServiceProviderDataNameTest:
        return isNameValid( text, errorMessage, tooltip );
    case TestModel::ServiceProviderDataVersionTest:
        return isVersionValid( text, errorMessage, tooltip );
    case TestModel::ServiceProviderDataFileFormatVersionTest:
        return isFileVersionValid( text, errorMessage, tooltip );
    case TestModel::ServiceProviderDataAuthorNameTest:
        return isAuthorNameValid( text, errorMessage, tooltip );
    case TestModel::ServiceProviderDataShortAuthorNameTest:
        return isShortAuthorNameValid( text, errorMessage, tooltip );
    case TestModel::ServiceProviderDataEmailTest:
        return isEmailValid( text, errorMessage, tooltip );
    case TestModel::ServiceProviderDataUrlTest:
        return isUrlValid( text, errorMessage, tooltip );
    case TestModel::ServiceProviderDataShortUrlTest:
        return isShortUrlValid( text, errorMessage, tooltip );
    case TestModel::ServiceProviderDataScriptFileNameTest:
        return isScriptFileNameValid( text, errorMessage, tooltip );
    case TestModel::ServiceProviderDataDescriptionTest:
        return isDescriptionValid( text, errorMessage, tooltip );

    default:
        kWarning() << "Unknown test";
        return TestModel::TestFinishedSuccessfully;
    }
}

TestModel::TestState ServiceProviderDataTester::runServiceProviderDataTest( TestModel::Test test,
        const ServiceProviderData *data, QString *errorMessage, QString *tooltip )
{
    switch ( test ) {
    case TestModel::ServiceProviderDataNameTest:
        return isNameValid( data->name(), errorMessage, tooltip );
    case TestModel::ServiceProviderDataVersionTest:
        return isVersionValid( data->version(), errorMessage, tooltip );
    case TestModel::ServiceProviderDataFileFormatVersionTest:
        return isFileVersionValid( data->fileFormatVersion(), errorMessage, tooltip );
    case TestModel::ServiceProviderDataAuthorNameTest:
        return isAuthorNameValid( data->author(), errorMessage, tooltip );
    case TestModel::ServiceProviderDataShortAuthorNameTest:
        return isShortAuthorNameValid( data->shortAuthor(), errorMessage, tooltip );
    case TestModel::ServiceProviderDataEmailTest:
        return isEmailValid( data->email(), errorMessage, tooltip );
    case TestModel::ServiceProviderDataUrlTest:
        return isUrlValid( data->url(), errorMessage, tooltip );
    case TestModel::ServiceProviderDataShortUrlTest:
        return isShortUrlValid( data->shortUrl(), errorMessage, tooltip );
    case TestModel::ServiceProviderDataScriptFileNameTest:
        if ( data->type() != Enums::ScriptedProvider ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info/plain", "Only for scripted providers");
            }
            if ( tooltip ) {
                *tooltip = i18nc("@info", "<title>Test not Applicable</title> "
                                 "<para>This test is only applicable for "
                                 "scripted provider plugins.</para>");
            }
            return TestModel::TestNotApplicable;
        } else {
            return isScriptFileNameValid( data->scriptFileName(), errorMessage, tooltip );
        }
    case TestModel::ServiceProviderDataDescriptionTest:
        return isDescriptionValid( data->description(), errorMessage, tooltip );

    default:
        kWarning() << "Unknown test";
        return TestModel::TestFinishedSuccessfully;
    }
}

TestModel::TestState ServiceProviderDataTester::isNameValid( const QString &name, QString *errorMessage, QString *tooltip )
{
    if ( name.isEmpty() ) {
        if ( errorMessage ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info/plain", "You need to specify a name for your project");
            }
            if ( tooltip ) {
                *tooltip = i18nc("@info",
                        "<title>You need to specify a name for your project in the project settings</title> "
                        "<para>Applets show this name in a service provider selector widget.</para>");
            }
        }
        return TestModel::TestFinishedWithErrors;
    }

    return TestModel::TestFinishedSuccessfully;
}

TestModel::TestState ServiceProviderDataTester::isVersionValid( const QString &version,
        QString *errorMessage, QString *tooltip )
{
    if ( version.isEmpty() ) {
        if ( errorMessage ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info/plain", "You should specify a version of your project");
            }
            if ( tooltip ) {
                *tooltip = i18nc("@info", "<title>Project Version Missing</title>"
                        "<para>This helps to distinguish between different versions and makes it "
                        "possible to say for example: \"You need at least version 1.3 of that "
                        "service provider plugin for that feature to work\". "
                        "Open the project settings to add a <interface>Version</interface>.</para>");
            }
        }
        return TestModel::TestFinishedWithErrors;
    }

    // Check if version contains a valid version string
    QRegExp rx( "\\d+(\\.\\d+)?(\\.\\d+)?" );
    if ( rx.indexIn(version) == -1 ) {
        if ( errorMessage ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info/plain", "The version string is invalid");
            }
            if ( tooltip ) {
                *tooltip = i18nc("@info", "<title>The version string is invalid</title>");
            }
        }
        return TestModel::TestFinishedWithErrors;
    }

    return TestModel::TestFinishedSuccessfully;
}

TestModel::TestState ServiceProviderDataTester::isFileVersionValid( const QString &fileVersion,
        QString *errorMessage, QString *tooltip )
{
    if ( fileVersion.isEmpty() || fileVersion != "1.0" ) {
        if ( errorMessage ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info/plain", "The PublicTransport data engine currently "
                        "only supports version '1.0'");
            }
            if ( tooltip ) {
                *tooltip = i18nc("@info",
                        "<title>The PublicTransport data engine currently only supports version '1.0'</title>"
                        "<para>Specify version '1.0' as <interface>File Type Version</interface> "
                        "in the project settings.</para>");
            }
        }
        return TestModel::TestFinishedWithErrors;
    }

    // Check if fileVersion contains a valid version string
    QRegExp rx( "\\d+(\\.\\d+)?(\\.\\d+)?" );
    if ( rx.indexIn(fileVersion) == -1 ) {
        if ( errorMessage ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info/plain", "The version string is invalid");
            }
            if ( tooltip ) {
                *tooltip = i18nc("@info", "<title>The version string is invalid</title>");
            }
        }
        return TestModel::TestFinishedWithErrors;
    }

    return TestModel::TestFinishedSuccessfully;
}

TestModel::TestState ServiceProviderDataTester::isEmailValid( const QString &email,
        QString *errorMessage, QString *tooltip )
{
    if ( email.isEmpty() ) {
        if ( errorMessage ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info/plain", "You should provide your email address as author "
                        "of the project");
            }
            if ( tooltip ) {
                *tooltip = i18nc("@info",
                        "<title>You should provide your email address as author of the project</title> "
                        "<para>You may create a new address if you do not want to use your private one. "
                        "Without an email address, no one can contact you if something is wrong "
                        "with your project. Open the project settings and specify your "
                        "<interface>E-Mail</interface> address.</para>");
            }
        }
        return TestModel::TestFinishedWithErrors;
    }

    // Check if email contains a valid email address
    QRegExp rx( "^[a-z0-9!#$%&\\._-]+@(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.)+[a-z]{2,4}$",
                Qt::CaseInsensitive );
    if ( rx.indexIn(email) == -1 ) {
        if ( errorMessage ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info/plain", "The email address is invalid");
            }
            if ( tooltip ) {
                *tooltip = i18nc("@info", "<title>The email address is invalid</title>");
            }
        }
        return TestModel::TestFinishedWithErrors;
    }

    return TestModel::TestFinishedSuccessfully;
}

TestModel::TestState ServiceProviderDataTester::isAuthorNameValid( const QString &authorName,
        QString *errorMessage, QString *tooltip )
{
    if ( authorName.isEmpty() ) {
        if ( errorMessage ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info/plain", "You should provide your name as author "
                        "of the project");
            }
            if ( tooltip ) {
                *tooltip = i18nc("@info",
                        "<title>You should provide your name as author of the project</title> "
                        "<para>Open the project settings and specify an "
                        "<interface>Author</interface>.</para>");
            }
        }
        return TestModel::TestFinishedWithErrors;
    }

    return TestModel::TestFinishedSuccessfully;
}

TestModel::TestState ServiceProviderDataTester::isShortAuthorNameValid(
        const QString &shortAuthorName, QString *errorMessage, QString *tooltip )
{
    if ( shortAuthorName.isEmpty() ) {
        if ( errorMessage ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info/plain", "You should provide a short version of your name "
                        "as author of the project");
            }
            if ( tooltip ) {
                *tooltip = i18nc("@info",
                        "<title>You should provide a short version of your name as author of the project"
                        "</title> <para>Open the project settings and specify a "
                        "<interface>Short Author Name</interface>.</para>");
            }
        }
        return TestModel::TestFinishedWithErrors;
    }
    return TestModel::TestFinishedSuccessfully;
}

TestModel::TestState ServiceProviderDataTester::isUrlValid( const QString &url,
        QString *errorMessage, QString *tooltip )
{
    if ( url.isEmpty() ) {
        if ( errorMessage ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info/plain", "You should provide the URL to the home page "
                        "of the service provider");
            }
            if ( tooltip ) {
                *tooltip = i18nc("@info",
                        "<title>You should provide the URL to the home page of the service provider</title> "
                        "<para>Since the service providers are running servers for the timetable service "
                        "they will want to get some credit. Applets should show a link to the home page. "
                        "Open the project settings and add a <interface>Home Page URL</interface>.</para>");
            }
        }
        return TestModel::TestFinishedWithErrors;
    }
    return TestModel::TestFinishedSuccessfully;
}

TestModel::TestState ServiceProviderDataTester::isShortUrlValid( const QString &shortUrl,
        QString *errorMessage, QString *tooltip )
{
    if ( shortUrl.isEmpty() ) {
        if ( errorMessage ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info/plain", "You should provide a short version of the URL "
                        "to the home page of the service provider");
            }
            if ( tooltip ) {
                *tooltip = i18nc("@info",
                        "<title>You should provide a short version of the URL to the home page of the "
                        "service provider</title> "
                        "<para>Applets may want to show the short URL as display text for the home "
                        "page link, to save space. The result would be that nothing is shown. "
                        "Open the project settings to add a <interface>Short URL</interface>.</para>");
            }
        }
        return TestModel::TestFinishedWithErrors;
    }
    return TestModel::TestFinishedSuccessfully;
}

TestModel::TestState ServiceProviderDataTester::isDescriptionValid( const QString &description,
        QString *errorMessage, QString *tooltip )
{
    if ( description.isEmpty() ) {
        if ( errorMessage ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info/plain", "You should give a description for your project");
            }
            if ( tooltip ) {
                *tooltip = i18nc("@info/plain",
                        "<title>You should give a description for your project</title> "
                        "<para>Describe what cities/countries/vehicles are supported and what "
                        "limitations there possibly are when using the service provider. Open the "
                        "project settings to add a <interface>Description</interface>.</para>");
            }
        }
        return TestModel::TestFinishedWithErrors;
    }
    return TestModel::TestFinishedSuccessfully;
}

TestModel::TestState ServiceProviderDataTester::isScriptFileNameValid(
        const QString &scriptFileName, QString *errorMessage, QString *tooltip )
{
    if ( scriptFileName.isEmpty() ) {
        if ( errorMessage ) {
            if ( errorMessage ) {
                *errorMessage = i18nc("@info/plain", "No script file created for the project");
            }
            if ( tooltip ) {
                *tooltip = i18nc("@info/plain",
                        "<title>No script file created for the project</title> "
                        "<para>The script does the actual work of the project, ie. it requests and parses "
                        "documents from the service provider. Open the script tab to create a new script "
                        "from a template, implement the functions and save it.</para>");
            }
        }
        return TestModel::TestFinishedWithErrors;
    }
    // TODO Test if the script file exists
    return TestModel::TestFinishedSuccessfully;
}
