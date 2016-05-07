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
#include "serviceprovidertestdata.h"

// Own includes
#include "serviceprovider.h"
#include "serviceproviderglobal.h"

// KDE includes
#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KDebug>

// Qt includes
#include <QSharedPointer>
#include <QFileInfo>


ServiceProviderTestData::ServiceProviderTestData( Results results, const QString &errorMessage )
        : m_results(results)
{
    setErrorMessage( errorMessage );
}

void ServiceProviderTestData::setResults( Results testResults, const QString &errorMessage )
{
    m_results = testResults;
    setErrorMessage( errorMessage );
}

void ServiceProviderTestData::setErrorMessage( const QString &errorMessage )
{
    if ( m_results == AllTestsPassed ) {
        // Do not store error message if there is no error
        m_errorMessage.clear();
    } else if ( errorMessage.isEmpty() ) {
        const Status status = statusFromResults( m_results );
        switch ( status ) {
        case Failed:
            m_errorMessage = i18nc("@info/plain", "Provider plugin is invalid");
            break;
        case Passed:
            m_errorMessage.clear();
            break;
        case Pending:
            m_errorMessage = i18nc("@info/plain", "Provider plugin test is pending");
            break;
        }
    } else {
        m_errorMessage = errorMessage;
    }
}

ServiceProviderTestData::Status ServiceProviderTestData::statusFromResults( Results testResults )
{
    if ( testResults == AllTestsPassed ) {
        // All tests passed
        return Passed;
    } else if ( testResults.testFlag(XmlStructureTestFailed) ||
                testResults.testFlag(SubTypeTestFailed) )
    {
        // A test failed
        return Failed;
    } else {
        // Not all tests were run (no test failed and not all tests passed)
        return Pending;
    }
}

ServiceProviderTestData ServiceProviderTestData::read( const QString &providerId,
                                                       const QSharedPointer< KConfig > &cache )
{
    // Check if the source XML file was modified since the cache was last updated
    if ( ServiceProviderGlobal::isSourceFileModified(providerId, cache) ) {
        // Source file modified, all tests need to be rerun
        return ServiceProviderTestData( NoTestWasRun );
    } else {
        // Source file not modified, read test data from config or use defaults
        const KConfigGroup group = cache->group( providerId );
        const Results testResults = static_cast<Results>(
                group.readEntry("testResults", static_cast<int>(NoTestWasRun)) );
        return ServiceProviderTestData( testResults, group.readEntry("errorMessage", QString()) );
    }
}

void ServiceProviderTestData::write( const QString &providerId,
                                     const ServiceProviderTestData &testData,
                                     const QSharedPointer<KConfig> &cache )
{
    if ( providerId.isEmpty() ) {
        qWarning() << "No provider plugin ID given!";
        return;
    }

    KConfigGroup group = cache->group( providerId );
    const QString fileName = ServiceProviderGlobal::fileNameFromId(providerId);
    group.writeEntry( "modifiedTime", QFileInfo(fileName).lastModified() );
    group.writeEntry( "testResults", static_cast<int>(testData.results()) );
    if ( !testData.errorMessage().isEmpty() ) {
        group.writeEntry( "errorMessage", testData.errorMessage() );
    } else if ( group.hasKey("errorMessage") ) {
        // Do not keep old error messages in the cache
        group.deleteEntry( "errorMessage" );
    }
    group.sync();
}

void ServiceProviderTestData::setXmlStructureTestStatus( Status status, const QString &errorMessage )
{
    switch ( status ) {
    case Pending:
        setResults( m_results & ~(XmlStructureTestPassed | XmlStructureTestFailed) );
        break;
    case Passed:
        setResults( (m_results | XmlStructureTestPassed ) & (~XmlStructureTestFailed) );
        break;
    case Failed:
        setResults( (m_results | XmlStructureTestFailed) & (~XmlStructureTestPassed),
                    errorMessage );
        break;
    }
}

void ServiceProviderTestData::setSubTypeTestStatus( Status status, const QString &errorMessage )
{
    switch ( status ) {
    case Pending:
        setResults( m_results & ~(SubTypeTestPassed | SubTypeTestFailed) );
        break;
    case Passed:
        setResults( (m_results | SubTypeTestPassed) & (~SubTypeTestFailed) );
        break;
    case Failed:
        setResults( (m_results | SubTypeTestFailed) & (~SubTypeTestPassed),
                    errorMessage );
        break;
    }
}
