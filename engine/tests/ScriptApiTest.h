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

#ifndef SCRIPTAPITEST_H
#define SCRIPTAPITEST_H

#define QT_GUI_LIB

#include <QtCore/QObject>

/*
class TestVisualization : public QObject
{
    Q_OBJECT

public slots:
    void dataUpdated( const QString &, const Plasma::DataEngine::Data &_data ) {
        data = _data;
        emit completed();
    };

signals:
    void completed();

public:
    Plasma::DataEngine::Data data;
};*/

/** @brief Test classes which get exposed to scripts. */
class ScriptApiTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void cleanupTestCase();

    // Test Helper::addDaysToDate(), two overloads
    void helperAddDaysToDateTest_data();
    void helperAddDaysToDateTest();
    void helperAddDaysToDate2Test_data();
    void helperAddDaysToDate2Test();

    // Test Helper::addMinsToTime()
    void helperAddMinsToTimeTest_data();
    void helperAddMinsToTimeTest();

    // Test Helper::matchDate()
    void helperMatchDateTest_data();
    void helperMatchDateTest();

    // Test Helper::matchTime()
    void helperMatchTimeTest_data();
    void helperMatchTimeTest();

    // Test Helper::formatDate()
    void helperFormatDateTest_data();
    void helperFormatDateTest();

    // Test Helper::formatTime()
    void helperFormatTimeTest_data();
    void helperFormatTimeTest();

    // Test Helper::formatDateTime()
    void helperFormatDateTimeTest_data();
    void helperFormatDateTimeTest();

    // Test Helper::camelCase()
    void helperCamelCaseTest_data();
    void helperCamelCaseTest();

    // Test Helper::trim()
    void helperTrimTest_data();
    void helperTrimTest();

    // Test Helper::stripTags()
    void helpeStripTagsTest_data();
    void helpeStripTagsTest();

    // Test Helper::splitSkipEmptyParts()
    void helperSplitSkipEmptyPartsTest_data();
    void helperSplitSkipEmptyPartsTest();

    // Test Helper::decodeHtmlEntities()
    void helperDecodeHtmlEntitiesTest_data();
    void helperDecodeHtmlEntitiesTest();

    // Test Helper::duration()
    void helperDurationTest_data();
    void helperDurationTest();

    // Test Helper::findFirstHtmlTag()
    void helperFindFirstHtmlTagTest_data();
    void helperFindFirstHtmlTagTest();

    // Test Helper::findHtmlTags()
    void helperFindHtmlTagsTest_data();
    void helperFindHtmlTagsTest();

    // Test Helper::findNamedHtmlTags()
    void helperFindNamedHtmlTagsTest_data();
    void helperFindNamedHtmlTagsTest();

    // No testing of deprecated Helper::extractBlock()

    // Test Storage::read(), Storage::writePersistent(), Storage::remove() and Storage::hasData()
    void storageReadWriteTest_data();
    void storageReadWriteTest();

    // Test Storage::readPersistent(), Storage::writePersistent(), Storage::removePersistent(),
    // Storage::hasPersistentData() and Storage::lifetime()
    void storageReadWritePersistentTest_data();
    void storageReadWritePersistentTest();

    // TODO Test Storage::write/read[Persistent]( const QVariantMap &map );

    // Test ResultObject::features(), ResultObject::hints(), ResultObject::giveHint(),
    // ResultObject::enableFeature(), ResultObject::isHintGiven(), ResultObject::isFeatureEnabled()
    void resultFeaturesHintsTest();

    // Test ResultObject::addData(), ResultObject::clear(), ResultObject::hasData() and signal
    // ResultObject::publish()
    void resultDataTest();

    void networkSynchronousTest();
    void networkAsynchronousTest();
    void networkAsynchronousAbortTest();
    void networkAsynchronousMultipleTest();
};

#endif // SCRIPTAPITEST_H
