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

#ifndef ACCESSORINFOTESTER_H
#define ACCESSORINFOTESTER_H

// Own includes
#include "testmodel.h"

// Qt includes
#include <QHash>

class Project;
class TimetableAccessorInfo;
class AccessorInfoTester;
class QString;

/**
 * @brief Tests accessor info objects for validity.
 * @see AccessorInfoTesterResult
 **/
class AccessorInfoTester {
    friend struct AccessorInfoTesterResult;

public:
    static bool runAccessorInfoTest( TestModel::Test test, const QString &text,
            QString *errorMessage = 0, QString *tooltip = 0 );
    static bool runAccessorInfoTest( TestModel::Test test, const TimetableAccessorInfo *info,
            QString *errorMessage = 0, QString *tooltip = 0 );

    static bool isNameValid( const QString &name,
            QString *errorMessage = 0, QString *tooltip = 0 );
    static bool isVersionValid( const QString &version,
            QString *errorMessage = 0, QString *tooltip = 0  );
    static bool isFileVersionValid( const QString &fileVersion,
            QString *errorMessage = 0, QString *tooltip = 0 );
    static bool isAuthorNameValid( const QString &authorName,
            QString *errorMessage = 0, QString *tooltip = 0 );
    static bool isShortAuthorNameValid( const QString &shortAuthorName,
            QString *errorMessage = 0, QString *tooltip = 0 );
    static bool isEmailValid( const QString &email,
            QString *errorMessage = 0, QString *tooltip = 0 );
    static bool isUrlValid( const QString &url,
            QString *errorMessage = 0, QString *tooltip = 0 );
    static bool isShortUrlValid( const QString &shortUrl,
            QString *errorMessage = 0, QString *tooltip = 0 );
    static bool isScriptFileNameValid( const QString &scriptFileName,
            QString *errorMessage = 0, QString *tooltip = 0 );
    static bool isDescriptionValid( const QString &description,
            QString *errorMessage = 0, QString *tooltip = 0 );
};

#endif // Multiple inclusion guard
