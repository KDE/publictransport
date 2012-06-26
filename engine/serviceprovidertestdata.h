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

#ifndef SERVICEPROVIDERTESTDATA_HEADER
#define SERVICEPROVIDERTESTDATA_HEADER

// Qt includes
#include <QString>

template<class T >
class QSharedPointer;
class KConfig;

/**
 * @brief Provides an interface to read/write provider test data to/from the cache.
 * Use read() to read test data for a provider plugin and write() to write test data.
 * The <em>cache</em> argument required by some functions, can be retrieved using
 * ServiceProviderGlobal::cache().
 * The Results flags can store for each test if it is passed/failed/pending.
 * The Status enumerable can be retrieved from Results flags using statusFromResults(). It gets
 * used to store information about the status of all tests (like in a test case) or about single
 * tests (eg. in setXmlStructureTestStatus()).
 **/
class ServiceProviderTestData {
public:
    /**
     * @brief Test status, can be retrieved from Results using statusFromResults().
     * @see Results
     * @see statusFromResults()
     **/
    enum Status {
        Pending = 0, /**< All completed tests passed but there are more tests pending. */
        Passed, /**< All tests were successfully completed. */
        Failed /**< At least one test failed. */
    };

    /**
     * @brief Results of service provider plugin tests.
     * The values of the enumerables get stored in the cache file (ServiceProviderGlobal::cache()).
     * @see Status
     * @see statusFromResults()
     **/
    enum Result {
        NoTestWasRun            = 0x0000, /**< No test has been run, all are pending. */

        XmlStructureTestPassed  = 0x0001, /**< The XML structure test has passed. */
        XmlStructureTestFailed  = 0x0002, /**< The XML structure test has failed. */

        SubTypeTestPassed       = 0x0004, /**< The sub-type test has passed, ie. tests run in
                                  * derived provider plugin types (eg. ServiceProviderScript). */
        SubTypeTestFailed       = 0x0008, /**< The sub-type test has failed, ie. tests run in
                                  * derived provider plugin types (eg. ServiceProviderScript). */

        /** All tests have passed. */
        AllTestsPassed = XmlStructureTestPassed | SubTypeTestPassed,

        /** All tests failed. */
        AllTestsFailed = XmlStructureTestFailed | SubTypeTestFailed,
    };
    Q_DECLARE_FLAGS( Results, Result );

    /** @brief Get the PluginTestStatus enumerable associated with the given @p testResults. */
    static Status statusFromResults( Results testResults );

    /**
     * @brief Read test data from the @p cache for the plugin with the given @p providerId.
     * @param providerId The ID of the provider plugin to return test data for.
     * @param cache A shared pointer to the KConfig object handling the provider cache file.
     * @see writeTestData()
     * @see ServiceProviderGlobal::cache()
     **/
    static ServiceProviderTestData read( const QString &providerId,
                                         const QSharedPointer<KConfig> &cache );

    /**
     * @brief Write @p testData to the @p cache for the plugin with the given @p providerId.
     *
     * The following entries get written: "testResults" (stored as type Results),
     * "modifiedTime" (to be able to check if the provider XML file has been modified),
     * "errorMessage" (if there is an error).
     *
     * @param providerId The ID of the provider plugin to write @p testData for.
     * @param testData The test data to write into the cache file.
     * @param cache A shared pointer to the KConfig object handling the provider cache file.
     * @see testData()
     * @see ServiceProviderGlobal::cache()
     **/
    static void write( const QString &providerId, const ServiceProviderTestData &testData,
                       const QSharedPointer<KConfig> &cache );

//     static void runPendingTests( const QString &providerId,
//                                  const ServiceProviderTestData &testData,
//                                  const QSharedPointer<KConfig> &cache );

    /** @brief Create a new ServiceProviderTestData object. */
    ServiceProviderTestData( Results results, const QString &errorMessage = QString() );

    /**
     * @brief Write this test data to the @p cache for the plugin with the given @p providerId.
     * This is a convenience function, it simply calls the static write() function with this
     * ServiceProviderTestData object as argument.
     */
    void write( const QString &providerId, const QSharedPointer<KConfig> &cache ) {
        write( providerId, *this, cache );
    };

    /**
     * @brief Set results to @p testResults.
     * @param testResults The new results.
     * @param errorMessage Only used if @p testResults contains one or more failed tests.
     */
    void setResults( Results testResults, const QString &errorMessage = QString() );

    /** @brief Whether or not at least one test has failed. */
    bool hasErrors() const {
        return m_results.testFlag(XmlStructureTestFailed) ||
               m_results.testFlag(SubTypeTestFailed);
    };

    /** @brief Whether or not there are pending tests. */
    bool hasPendingTests() const {
        return isXmlStructureTestPending() || isSubTypeTestPending();
    };

    /** @brief Get results. */
    inline Results results() const { return m_results; };

    /** @brief Get the global status of all tests. */
    inline Status status() const { return statusFromResults(m_results); };

    /** @brief Get an error message, if there is an error. */
    inline QString errorMessage() const { return m_errorMessage; };

    /** @brief Whether or not the XML structure test is pending, ie. did not pass or fail. */
    inline bool isXmlStructureTestPending() const {
        return !m_results.testFlag(XmlStructureTestPassed) &&
               !m_results.testFlag(XmlStructureTestFailed);
    };

    /** @brief Whether or not the sub-type test is pending, ie. did not pass or fail. */
    inline bool isSubTypeTestPending() const {
        return !m_results.testFlag(SubTypeTestPassed) &&
               !m_results.testFlag(SubTypeTestFailed);
    };

    /** @brief Set the status of the XML structure test to @p status. */
    void setXmlStructureTestStatus( Status status, const QString &errorMessage = QString() );

    /** @brief Set the status of the sub-type test to @p status. */
    void setSubTypeTestStatus( Status status, const QString &errorMessage = QString() );

private:
    void setErrorMessage( const QString &errorMessage );

    Results m_results;
    QString m_errorMessage;
};

#endif // Multiple inclusion guard