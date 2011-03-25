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

#include "timetablematehelper.h"

#include <KAuth/HelperSupport>
#include <QFile>
#include <KDebug>

KAuth::ActionReply TimetableMateHelper::install( const QVariantMap& map ) {
    kDebug() << "BEGIN" << map;
    KAuth::ActionReply reply;
    const QString saveDir = map["path"].toString();
    const QString accessorFileName = map["filenameAccessor"].toString();
    const QString accessorDocument = map["contentsAccessor"].toString();
    const QString scriptFileName = map["filenameScript"].toString();
    const QString scriptDocument = map["contentsScript"].toString();

    QFile accessorFile( accessorFileName );
    if ( !accessorFile.open(QIODevice::WriteOnly) ) {
	reply = ActionReply::HelperErrorReply;
	reply.setErrorCode( accessorFile.error() );
	reply.setErrorDescription( accessorFile.errorString() );
	return reply;
    }
    if ( accessorFile.write(accessorDocument.toUtf8()) == -1 ) {
	reply = ActionReply::HelperErrorReply;
	reply.setErrorCode( accessorFile.error() );
	reply.setErrorDescription( accessorFile.errorString() );
	return reply;
    }
    accessorFile.close();

    QFile scriptFile( scriptFileName );
    if ( !scriptFile.open(QIODevice::WriteOnly) ) {
	reply = ActionReply::HelperErrorReply;
	reply.setErrorCode( scriptFile.error() );
	reply.setErrorDescription( scriptFile.errorString() );
	return reply;
    }
    if ( scriptFile.write(scriptDocument.toUtf8()) == -1 ) {
	reply = ActionReply::HelperErrorReply;
	reply.setErrorCode( scriptFile.error() );
	reply.setErrorDescription( scriptFile.errorString() );
	return reply;
    }
    scriptFile.close();

    kDebug() << "END";
    return reply;
}

KDE4_AUTH_HELPER_MAIN( "org.kde.timetablemate", TimetableMateHelper )