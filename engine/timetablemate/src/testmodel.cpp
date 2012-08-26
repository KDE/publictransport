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
#include "testmodel.h"

// Public Transport engine includes
#include <engine/script/serviceproviderscript.h>

// KDE includes
#include <KDebug>
#include <KLocalizedString>
#include <KColorScheme>
#include <KIcon>
#include <KGlobalSettings>
#include <KColorUtils>

// Qt includes
#include <QToolButton>
#include <QAction>
#include <QAbstractItemView>
#include <QTreeView>
#include <QHeaderView>
#include <QApplication>
#include <QPainter>

TestModel::TestModel( QObject *parent ) : QAbstractItemModel( parent )
{
}

void TestModel::markTestCaseAsUnstartable( TestModel::TestCase testCase,
                                           const QString &errorMessage, const QString &tooltip,
                                           QAction *solution )
{
    m_unstartableTestCases[ testCase ] = TestCaseData( errorMessage, tooltip, solution );
    emit dataChanged( indexFromTestCase(testCase, 0), indexFromTestCase(testCase, ColumnCount - 1) );
}

void TestModel::markTestAsStarted( TestModel::Test test )
{
    removeTestChildren( test );
    m_testData[ test ] = TestData( TestIsRunning );
    testChanged( test );
}

void TestModel::removeTestChildren( TestModel::Test test )
{
    const QModelIndex testIndex = indexFromTest( test );
    if ( m_testData.contains(test) ) {
        TestData &testData = m_testData[ test ];
        if ( !testData.childrenExplanations.isEmpty() ) {
            beginRemoveRows( testIndex, 0, testData.childrenExplanations.count() - 1 );
            testData.childrenExplanations.clear();
            endRemoveRows();
        }
    }
}

void TestModel::addTestResult( TestModel::Test test, TestState state, const QString &explanation,
                               const QString &tooltip, QAction *solution,
                               const QList< TimetableDataRequestMessage > &childrenExplanations,
                               const QList< TimetableData > &results,
                               const QSharedPointer<AbstractRequest> &request )
{
    const QModelIndex testIndex = indexFromTest( test );
    removeTestChildren( test );
//     if ( m_testData.contains(test) ) {
//         TestData &testData = m_testData[ test ];
//         if ( !testData.childrenExplanations.isEmpty() ) {
//             beginRemoveRows( testIndex, 0, testData.childrenExplanations.count() - 1 );
//             testData.childrenExplanations.clear();
//             endRemoveRows();
//         }
//     }

    // Check for warnings/errors in children explanations
    if ( state == TestFinishedSuccessfully ) {
        foreach ( const TimetableDataRequestMessage &message, childrenExplanations ) {
            if ( message.type == TimetableDataRequestMessage::Error ) {
                state = TestFinishedWithErrors;
            } else if ( state != TestFinishedWithErrors &&
                        message.type == TimetableDataRequestMessage::Warning )
            {
                state = TestFinishedWithWarnings;
            }
        }
    }

    if ( childrenExplanations.isEmpty() ) {
        m_testData[ test ] = TestData( state, explanation, tooltip, solution,
                                       childrenExplanations, results, request );
    } else {
        beginInsertRows( testIndex, 0, childrenExplanations.count() - 1 );
        m_testData[ test ] = TestData( state, explanation, tooltip, solution,
                                       childrenExplanations, results, request );
        endInsertRows();
    }

    testChanged( test );
}

void TestModel::testChanged( TestModel::Test test )
{
    emit dataChanged( indexFromTest(test, 0), indexFromTest(test, ColumnCount - 1) );

    TestCase testCase = testCaseOfTest( test );
    emit dataChanged( indexFromTestCase(testCase, 0), indexFromTestCase(testCase, ColumnCount - 1) );
}

void TestModel::clear()
{
    for ( int i = 0; i< TestCount; ++i ) {
        removeTestChildren( static_cast<Test>(i) );
    }
    m_unstartableTestCases.clear();
    m_testData.clear();
    emit dataChanged( index(0, 0), index(TestCaseCount - 1, ColumnCount - 1) );
}

bool TestModel::isEmpty() const
{
    return m_testData.isEmpty();
}

int TestModel::rowCount( const QModelIndex &parent ) const
{
    if ( parent.isValid() ) {
        if ( parent.column() != 0 ) {
            // Only the first column has children
            return 0;
        } else if ( parent.parent().isValid() ) {
            if ( testCaseFromIndex(parent.parent()) == InvalidTestCase ) {
                return 0;
            } else {
                // Parent is a test item
                const Test test = testFromIndex( parent );
                return m_testData.contains(test)
                        ? m_testData[test].childrenExplanations.count() : 0;
            }
        } else {
            // Parent is a test case item
            TestCase testCase = testCaseFromIndex( parent );
            return testsOfTestCase( testCase ).count();
        }
    } else {
        return TestCaseCount;
    }
}

TestModel::TestCase TestModel::testCaseFromIndex( const QModelIndex &testCaseIndex ) const
{
    if ( !testCaseIndex.isValid() || testCaseIndex.parent().isValid() ||
         testCaseIndex.internalId() != -1 )
    {
        // Not a test case item
        return InvalidTestCase;
    } else {
        return static_cast< TestCase >( testCaseIndex.row() );
    }
}

TestModel::Test TestModel::testFromIndex( const QModelIndex &testIndex ) const
{
    if ( !testIndex.isValid() || testIndex.parent().internalId() != -1 ) {
        // Not a test item
        return InvalidTest;
    } else {
        return static_cast< Test >( testIndex.internalId() );
    }
}

QModelIndex TestModel::indexFromTestCase( TestModel::TestCase testCase, int column ) const
{
    return testCase >= TestCaseCount ? QModelIndex()
            : createIndex(static_cast<int>(testCase), column, -1);
}

QModelIndex TestModel::indexFromTest( TestModel::Test test, int column ) const
{
    TestCase testCase = testCaseOfTest( test );
    QList< Test > tests = testsOfTestCase( testCase );
    return test >= TestCount ? QModelIndex()
            : createIndex(tests.indexOf(test), column, static_cast<int>(test));
}

QModelIndex TestModel::index( int row, int column, const QModelIndex &parent ) const
{
    if ( column < 0 || column >= ColumnCount || row < 0 ) {
        return QModelIndex();
    }

    if ( parent.isValid() ) {
        if ( parent.column() != 0 ) {
            // Only the first column has children
            return QModelIndex();
        }

        if ( parent.parent().isValid() ) {
            const QModelIndex testCaseIndex = parent.parent();
            if ( testCaseIndex.parent().isValid() ) {
                // Only three levels: Test case items > Test items > Children explanations for Test items
                kDebug() << "Only three levels: Test case items > Test items > Children explanations for Test items";
                return QModelIndex();
            }

            Test test = testFromIndex( parent );
            if ( m_testData.contains(test) ) {
                const TestData &testData = m_testData[ test ];
                // Use the sum of the Test enumerable and a buffer for the normal test items
                // as internal id for children of test items
                // Everything with an internal id greater or equal to TestCount + 100 is a child
                // of that test item
                return row >= testData.childrenExplanations.count() ? QModelIndex()
                        : createIndex(row, column, TestCount + 100 + static_cast<int>(test));
            } else {
                return QModelIndex();
            }
        } else {
            TestCase testCase = testCaseFromIndex( parent );
            QList< Test > tests = testsOfTestCase( testCase );
            // Use values of Test enumerables as internal id for test items
            return row >= tests.count() ? QModelIndex()
                    : createIndex(row, column, static_cast<int>(tests[row]));
        }
    } else {
        // Use -1 as internal id for top level test case items
        return row < 0 || row >= TestCaseCount ? QModelIndex() : createIndex(row, column, -1);
    }
}

QModelIndex TestModel::parent( const QModelIndex &child ) const
{
    if ( !child.isValid() ) {
        // child is invalid
        return QModelIndex();
    }

    const qint64 id = child.internalId();
    if ( id >= TestCount + 100 ) {
        // child has a test item as parent
        const Test test = static_cast< Test >( id - TestCount - 100 );
        return indexFromTest( test );
    } else if ( id >= 0 ) {
        // child is a test item, a test case item is it's parent
        const Test test = static_cast< Test >( id );
        return indexFromTestCase( testCaseOfTest(test) );
    } else {
        // child is a top level test case item, it has no parent
        return QModelIndex();
    }
}

QFont TestModel::getFont( const QVariant &fontData ) const
{
    return fontData.isValid() && fontData.canConvert(QVariant::Font)
            ? fontData.value<QFont>() : KGlobalSettings::generalFont();
}

QVariant TestModel::testCaseData( TestModel::TestCase testCase, const QModelIndex &index, int role ) const
{
    switch ( role ) {
    case Qt::DisplayRole:
        switch ( index.column() ) {
        case NameColumn:
            return nameForTestCase( testCase );
        case StateColumn: {
            const TestState state = testCaseState( testCase );
            switch ( state ) {
            case TestNotStarted:
                return i18nc("@info/plain", "Not Started");
            case TestIsRunning:
                return i18nc("@info/plain", "Running");
            case TestFinishedSuccessfully:
                return i18nc("@info/plain", "Success");
            case TestFinishedWithWarnings:
                return i18nc("@info/plain", "Warnings");
            case TestFinishedWithErrors:
            case TestCouldNotBeStarted:
                return i18nc("@info/plain", "Failed");
            }
        } break;
        case ExplanationColumn: {
            const TestState state = testCaseState( testCase );
            if ( (state == TestCouldNotBeStarted || state == TestFinishedWithErrors ||
                  state == TestFinishedSuccessfully || state == TestFinishedWithWarnings) &&
                 !m_unstartableTestCases[testCase].explanation.isEmpty() )
            {
                return m_unstartableTestCases[ testCase ].explanation;
            } else {
                return descriptionForTestCase( testCase );
            }
        } break;
        }
        break;

    case Qt::DecorationRole:
        if ( index.column() == 0 ) {
            const TestState state = testCaseState( testCase );
            switch ( state ) {
            case TestNotStarted:
                return KIcon("arrow-right");
            case TestIsRunning:
                return KIcon("task-ongoing");
            case TestFinishedSuccessfully:
                return KIcon("task-complete");
            case TestFinishedWithWarnings:
                return KIcon("dialog-warning");
            case TestFinishedWithErrors:
            case TestCouldNotBeStarted:
                return KIcon("task-reject");
            }
        }
        break;

    case Qt::ToolTipRole:
        if ( index.column() == ExplanationColumn ) {
            const TestState state = testCaseState( testCase );
            if ( state == TestCouldNotBeStarted || state == TestFinishedWithErrors ||
                 state == TestFinishedWithWarnings || state == TestFinishedSuccessfully )
            {
                return m_unstartableTestCases[ testCase ].tooltip;
            } else {
                return descriptionForTestCase( testCase );
            }
        } else if ( index.column() == NameColumn ) {
            return descriptionForTestCase( testCase );
        }
        break;

    case Qt::SizeHintRole:
        if ( index.column() == ExplanationColumn ) {
            // Make test case items two lines high
            const QString text = index.data().toString();
            const QFontMetrics fontMetrics( getFont(index.data(Qt::FontRole)) );
            return QSize( fontMetrics.width(text) / 2, fontMetrics.lineSpacing() * 2 );
        }
        break;

    case SolutionActionRole:
        if ( index.column() == ExplanationColumn ) {
            const TestState state = testCaseState( testCase );
            if ( state == TestCouldNotBeStarted ) {
                QVariant variant;
                variant.setValue< QAction* >( m_unstartableTestCases.contains(testCase)
                                            ? m_unstartableTestCases[testCase].solution : 0 );
                return variant;
            }
        }
        break;

    case Qt::TextAlignmentRole:
        return int( Qt::AlignLeft | Qt::AlignVCenter );

    case Qt::BackgroundRole:
        return backgroundFromTestState( testCaseState(testCase) );

    case Qt::ForegroundRole:
        return foregroundFromTestState( testCaseState(testCase) );
    }

    return QVariant();
}

QVariant TestModel::testData( TestModel::Test test, const QModelIndex &index, int role ) const
{
    switch ( role ) {
    case Qt::DisplayRole:
        switch ( index.column() ) {
        case NameColumn:
            return nameForTest( test );
        case StateColumn: {
            TestState state = m_testData.contains(test)
                    ? m_testData[test].state : TestNotStarted;
            switch ( state ) {
            case TestNotStarted:
                return i18nc("@info/plain", "Not Started");
            case TestIsRunning:
                return i18nc("@info/plain", "Running");
            case TestFinishedSuccessfully:
                return i18nc("@info/plain", "Success");
            case TestFinishedWithWarnings:
                return i18nc("@info/plain", "Warnings");
            case TestFinishedWithErrors:
            case TestCouldNotBeStarted:
                return i18nc("@info/plain", "Failed");
            }
        } break;
        case ExplanationColumn:
            return m_testData.contains(test) && m_testData[test].isFinished() &&
                   !m_testData[test].explanation.isEmpty()
                    ? m_testData[test].explanation : descriptionForTest(test);
        }
        break;

    case Qt::ToolTipRole:
        if ( index.column() == ExplanationColumn ) {
            return m_testData.contains(test) && m_testData[test].isFinished()
                   ? (!m_testData[test].tooltip.isEmpty()
                      ? m_testData[test].tooltip : m_testData[test].explanation)
                   : descriptionForTest(test);
        } else if ( index.column() == NameColumn ) {
            return descriptionForTest( test );
        }
        break;

    case Qt::DecorationRole:
        if ( index.column() == 0 ) {
            TestState state = m_testData.contains(test)
                    ? m_testData[test].state : TestNotStarted;
            switch ( state ) {
            case TestNotStarted:
                return KIcon("arrow-right");
            case TestIsRunning:
                return KIcon("task-ongoing");
            case TestFinishedSuccessfully:
                return KIcon("task-complete");
            case TestFinishedWithWarnings:
                return KIcon("dialog-warning");
            case TestFinishedWithErrors:
            case TestCouldNotBeStarted:
                return KIcon("task-reject");
            }
        }
        break;

    case Qt::TextAlignmentRole:
        return int( Qt::AlignLeft | Qt::AlignVCenter );

    case Qt::SizeHintRole:
        if ( index.column() == ExplanationColumn ) {
            if ( m_testData.contains(test) && m_testData[test].isFinished() ) {
                const QFont font = getFont( index.data(Qt::FontRole) );
                const QFontMetrics fontMetrics( font );
                const QString text = index.data().toString();
                return m_testData[test].isFinishedSuccessfully()
                        ? QSize(fontMetrics.width(text), fontMetrics.lineSpacing())
                        : QSize(fontMetrics.width(text) / 2, fontMetrics.lineSpacing() * 2);
            }
        }
        break;

    case SolutionActionRole:
        if ( index.column() == ExplanationColumn ) {
            QVariant variant;
            variant.setValue< QAction* >( m_testData.contains(test) &&
                    m_testData[test].isFinishedWithErrors() ? m_testData[test].solution : 0 );
            return variant;
        }
        break;

    case Qt::BackgroundRole:
        return backgroundFromTestState( testState(test) );

    case Qt::ForegroundRole:
        return foregroundFromTestState( testState(test) );
    }
    return QVariant();
}

QVariant TestModel::data( const QModelIndex &index, int role ) const
{
    if ( !index.isValid() ) {
        return QVariant();
    }

    if ( index.parent().isValid() ) {
        const Test test = testFromIndex( index.parent() );
        if ( test != InvalidTest ) {
            // Get data for a child item of a test parent item, ie. data for additional explanations
            if ( index.column() != 0 ) {
                return QVariant();
            } else if ( m_testData.contains(test) ) {
                switch ( role ) {
                case Qt::DisplayRole:
                case Qt::ToolTipRole: {
                    const TestData &testData = m_testData[ test ];
                    if ( index.row() < testData.childrenExplanations.count() ) {
                        return testData.childrenExplanations[index.row()].message;
                    }
                }
                case Qt::DecorationRole:
                    if ( index.column() == 0 ) {
                        const TestData &testData = m_testData[ test ];
                        if ( index.row() < testData.childrenExplanations.count() ) {
                            switch ( testData.childrenExplanations[index.row()].type ) {
                            case TimetableDataRequestMessage::Error:
                                return KIcon("task-reject");
                            case TimetableDataRequestMessage::Warning:
                                return KIcon("dialog-warning");
                            case TimetableDataRequestMessage::Information:
                            default:
                                return KIcon("documentinfo");
                            }
                        }
                    }
                    break;
                case LineNumberRole: {
                    // Return the associated line number, if any
                    const TestData &testData = m_testData[ test ];
                    return index.row() >= testData.childrenExplanations.count()
                            ? QVariant() : testData.childrenExplanations[index.row()].lineNumber;
                }
                case FeatureRole: {
                    // Return the associated features, if any
                    const TestData &testData = m_testData[ test ];
                    return index.row() >= testData.childrenExplanations.count()
                            ? TimetableDataRequestMessage::NoFeature
                            : static_cast<int>(testData.childrenExplanations[index.row()].features);
                }
                case UrlRole: {
                    // Return the associated URL, if any
                    const TestData &testData = m_testData[ test ];
                    return index.row() >= testData.childrenExplanations.count()
                            ? QVariant() : testData.childrenExplanations[index.row()].data.toString();
                }

                case Qt::BackgroundRole:
                    if ( m_testData.contains(test) ) {
                        const TestData &testData = m_testData[ test ];
                        return backgroundFromMessageType(
                                testData.childrenExplanations[index.row()].type );
                    }

                case Qt::ForegroundRole:
                    if ( m_testData.contains(test) ) {
                        const TestData &testData = m_testData[ test ];
                        return foregroundFromMessageType(
                                testData.childrenExplanations[index.row()].type );
                    }
                default:
                    return QVariant();
                }
            }
        } else {
            // Get data for a child item of a test case parent item, ie. data for a test item
            const TestCase testCase = testCaseFromIndex( index.parent() );
            const QList< Test > tests = testsOfTestCase( testCase );
            if ( index.row() >= tests.count() ) {
                return QVariant();
            }
            return testData( tests[index.row()], index, role );
        }
    } else {
        // Get data for a top level item, ie. a test case item
        return testCaseData( static_cast<TestCase>(index.row()), index, role );
    }

    return QVariant();
}

QBrush TestModel::backgroundFromTestState( TestModel::TestState state ) const
{
    switch ( state ) {
    case TestFinishedSuccessfully:
        return KColorScheme(QPalette::Active).background(KColorScheme::PositiveBackground);
    case TestFinishedWithWarnings:
        return KColorScheme(QPalette::Active).background(KColorScheme::NeutralBackground);
    case TestCouldNotBeStarted:
    case TestFinishedWithErrors:
        return KColorScheme(QPalette::Active).background(KColorScheme::NegativeBackground);
    case TestIsRunning:
    case TestNotStarted:
    default:
        return KColorScheme(QPalette::Active).background(KColorScheme::NormalBackground);
    }
}

QBrush TestModel::foregroundFromTestState( TestModel::TestState state ) const
{
    switch ( state ) {
    case TestFinishedSuccessfully:
        return KColorScheme(QPalette::Active).foreground(KColorScheme::PositiveText);
    case TestFinishedWithWarnings:
        return KColorScheme(QPalette::Active).foreground(KColorScheme::NeutralText);
    case TestCouldNotBeStarted:
    case TestFinishedWithErrors:
        return KColorScheme(QPalette::Active).foreground(KColorScheme::NegativeText);
    case TestIsRunning:
    case TestNotStarted:
    default:
        return KColorScheme(QPalette::Active).foreground(KColorScheme::NormalText);
    }
}

QBrush TestModel::backgroundFromMessageType( TimetableDataRequestMessage::Type messageType ) const
{
    switch ( messageType ) {
    case TimetableDataRequestMessage::Warning:
        return KColorScheme(QPalette::Active).background(KColorScheme::NeutralBackground);
    case TimetableDataRequestMessage::Error:
        return KColorScheme(QPalette::Active).background(KColorScheme::NegativeBackground);
    case TimetableDataRequestMessage::Information:
    default:
        return KColorScheme(QPalette::Active).background(KColorScheme::NormalBackground);
    }
}

QBrush TestModel::foregroundFromMessageType( TimetableDataRequestMessage::Type messageType ) const
{
    switch ( messageType ) {
    case TimetableDataRequestMessage::Warning:
        return KColorScheme(QPalette::Active).foreground(KColorScheme::NeutralText);
    case TimetableDataRequestMessage::Error:
        return KColorScheme(QPalette::Active).foreground(KColorScheme::NegativeText);
    case TimetableDataRequestMessage::Information:
    default:
        return KColorScheme(QPalette::Active).foreground(KColorScheme::NormalText);
    }
}

Qt::ItemFlags TestModel::flags( const QModelIndex &index ) const
{
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
}

QVariant TestModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
    if ( orientation == Qt::Horizontal && role == Qt::DisplayRole ) {
        switch ( static_cast<Column>(section) ) {
        case NameColumn:
            return i18nc("@title:column", "Name");
        case StateColumn:
            return i18nc("@title:column", "State");
        case ExplanationColumn:
            return i18nc("@title:column", "Explanation");
        default:
            return QVariant();
        }
    }

    return QVariant();
}

bool TestModel::hasErroneousTests() const
{
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    if ( isErroneousTestState(testCaseState(ScriptExecutionTestCase)) ) {
        return true;
    }
#endif
    return isErroneousTestState( testCaseState(ServiceProviderDataTestCase) );
}

TestModel::TestState TestModel::completeState() const
{
    TestState completeState = TestNotStarted;
    for ( int i = 0; i < TestCaseCount; ++i ) {
        const TestCase testCase = static_cast< TestCase >( i );
        const TestState state = testCaseState( testCase );
        if ( state > completeState ) {
            completeState = state;
        }
    }

    return completeState;
}

TestModel::TestState TestModel::testCaseState( TestModel::TestCase testCase ) const
{
    if ( m_unstartableTestCases.contains(testCase) ) {
        return TestCouldNotBeStarted;
    }

    QList< Test > tests = testsOfTestCase( testCase );
    TestState caseState = TestNotStarted;
    foreach ( Test test, tests ) {
        // Get the state of the current test
        const TestState state = testState( test );

        // Overwrite the test case state with the state of the current test if it has a
        // higher value then the value of the current test case state
        if ( state > caseState ) {
            caseState = state;
        }
    }

    return caseState;
}

TestModel::TestState TestModel::testState( TestModel::Test test ) const
{
    return m_testData.contains(test) ? m_testData[test].state : TestNotStarted;
}

QList< TimetableData > TestModel::testResults( TestModel::Test test ) const
{
    return m_testData.contains(test) ? m_testData[test].results : QList< TimetableData >();
}

QSharedPointer<AbstractRequest> TestModel::testRequest( TestModel::Test test ) const
{
    return m_testData.contains(test) ? m_testData[test].request : QSharedPointer<AbstractRequest>();
}

QList< TestModel::Test > TestModel::testsOfTestCase( TestModel::TestCase testCase )
{
    QList< TestModel::Test > tests;
    switch ( testCase ) {
    case ServiceProviderDataTestCase:
        tests << ServiceProviderDataNameTest << ServiceProviderDataVersionTest
              << ServiceProviderDataFileFormatVersionTest
              << ServiceProviderDataAuthorNameTest << ServiceProviderDataShortAuthorNameTest
              << ServiceProviderDataEmailTest << ServiceProviderDataUrlTest
              << ServiceProviderDataShortUrlTest
              << ServiceProviderDataScriptFileNameTest << ServiceProviderDataDescriptionTest;
        break;
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case ScriptExecutionTestCase:
        tests << DepartureTest << ArrivalTest << AdditionalDataTest
              << StopSuggestionTest << StopSuggestionFromGeoPositionTest
              << JourneyTest << FeaturesTest;
        break;
#endif
    default:
        kDebug() << "Unknown test case" << testCase;
        break;
    }

    return tests;
}

TestModel::TestCase TestModel::testCaseOfTest( TestModel::Test test )
{
    switch ( test ) {
    case ServiceProviderDataNameTest:
    case ServiceProviderDataVersionTest:
    case ServiceProviderDataFileFormatVersionTest:
    case ServiceProviderDataAuthorNameTest:
    case ServiceProviderDataShortAuthorNameTest:
    case ServiceProviderDataEmailTest:
    case ServiceProviderDataUrlTest:
    case ServiceProviderDataShortUrlTest:
    case ServiceProviderDataScriptFileNameTest:
    case ServiceProviderDataDescriptionTest:
        return ServiceProviderDataTestCase;

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case DepartureTest:
    case ArrivalTest:
    case AdditionalDataTest:
    case StopSuggestionTest:
    case StopSuggestionFromGeoPositionTest:
    case JourneyTest:
    case FeaturesTest:
        return ScriptExecutionTestCase;
#endif

    default:
        kDebug() << "Unknown test" << test;
        return InvalidTestCase;
    }
}

QList< TestModel::Test > TestModel::testIsDependedOf( TestModel::Test test )
{
    switch ( test ) {
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case AdditionalDataTest:
        return QList< TestModel::Test >() << DepartureTest;

    case DepartureTest:
    case ArrivalTest:
    case StopSuggestionTest:
    case StopSuggestionFromGeoPositionTest:
    case JourneyTest:
    case FeaturesTest:
#endif

    case ServiceProviderDataNameTest:
    case ServiceProviderDataVersionTest:
    case ServiceProviderDataFileFormatVersionTest:
    case ServiceProviderDataAuthorNameTest:
    case ServiceProviderDataShortAuthorNameTest:
    case ServiceProviderDataEmailTest:
    case ServiceProviderDataUrlTest:
    case ServiceProviderDataShortUrlTest:
    case ServiceProviderDataScriptFileNameTest:
    case ServiceProviderDataDescriptionTest:
    default:
        return QList< TestModel::Test >();
    }
}

QString TestModel::nameForTestCase( TestModel::TestCase testCase )
{
    switch ( testCase ) {
    case ServiceProviderDataTestCase:
        return i18nc("@info/plain", "Project Settings Test Case" );
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case ScriptExecutionTestCase:
        return i18nc("@info/plain", "Script Execution Test Case" );
#endif
    default:
        kDebug() << "Unknown test case" << testCase;
        return QString();
    }
}

QString TestModel::nameForTest( TestModel::Test test )
{
    switch ( test ) {
    case ServiceProviderDataNameTest:
        return i18nc("@info/plain", "Name Test" );
    case ServiceProviderDataVersionTest:
        return i18nc("@info/plain", "Version Test" );
    case ServiceProviderDataFileFormatVersionTest:
        return i18nc("@info/plain", "File Version Test" );
    case ServiceProviderDataAuthorNameTest:
        return i18nc("@info/plain", "Author Name Test" );
    case ServiceProviderDataShortAuthorNameTest:
        return i18nc("@info/plain", "Short Author Name Test" );
    case ServiceProviderDataEmailTest:
        return i18nc("@info/plain", "Email Test" );
    case ServiceProviderDataUrlTest:
        return i18nc("@info/plain", "URL Test" );
    case ServiceProviderDataShortUrlTest:
        return i18nc("@info/plain", "Short URL Test" );
    case ServiceProviderDataScriptFileNameTest:
        return i18nc("@info/plain", "Script File Test" );
    case ServiceProviderDataDescriptionTest:
        return i18nc("@info/plain", "Description Test" );

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case DepartureTest:
        return i18nc("@info/plain", "Departure Test" );
    case ArrivalTest:
        return i18nc("@info/plain", "Arrival Test" );
    case AdditionalDataTest:
        return i18nc("@info/plain", "Additional Data Test" );
    case StopSuggestionTest:
        return i18nc("@info/plain", "Stop Suggestion Test" );
    case StopSuggestionFromGeoPositionTest:
        return i18nc("@info/plain", "Stop Suggestion From Geo Position Test" );
    case JourneyTest:
        return i18nc("@info/plain", "Journey Test" );
    case FeaturesTest:
        return i18nc("@info/plain", "Features Test" );
#endif
    default:
        kDebug() << "Unknown test" << test;
        return QString();
    }
}

QString TestModel::descriptionForTestCase( TestModel::TestCase testCase )
{
    switch ( testCase ) {
    case ServiceProviderDataTestCase:
        return i18nc("@info/plain", "Tests project settings for validity" );
#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case ScriptExecutionTestCase:
        return i18nc("@info/plain", "Runs script functions and tests collected data" );
#endif
    default:
        kDebug() << "Unknown test case" << testCase;
        return QString();
    }
}

QString TestModel::descriptionForTest( TestModel::Test test )
{
    switch ( test ) {
    case ServiceProviderDataNameTest:
        return i18nc("@info/plain", "Tests for a valid name");
    case ServiceProviderDataVersionTest:
        return i18nc("@info/plain", "Tests for a valid version string" );
    case ServiceProviderDataFileFormatVersionTest:
        return i18nc("@info/plain", "Tests for a valid engine plugin format version string" );
    case ServiceProviderDataAuthorNameTest:
        return i18nc("@info/plain", "Tests for a valid author name" );
    case ServiceProviderDataShortAuthorNameTest:
        return i18nc("@info/plain", "Tests for a valid short author name string" );
    case ServiceProviderDataEmailTest:
        return i18nc("@info/plain", "Tests for a valid email address" );
    case ServiceProviderDataUrlTest:
        return i18nc("@info/plain", "Tests for a valid URL to the homepage of the service provider" );
    case ServiceProviderDataShortUrlTest:
        return i18nc("@info/plain", "Tests for a valid short version of URL" );
    case ServiceProviderDataScriptFileNameTest:
        return i18nc("@info/plain", "Tests for a valid script file" );
    case ServiceProviderDataDescriptionTest:
        return i18nc("@info/plain", "Tests for a valid description" );

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case DepartureTest:
        return i18nc("@info/plain", "Runs the %1() script function and tests collected departure data",
                     ServiceProviderScript::SCRIPT_FUNCTION_GETTIMETABLE );
    case ArrivalTest:
        return i18nc("@info/plain", "Runs the %1() script function and tests collected arrival data",
                     ServiceProviderScript::SCRIPT_FUNCTION_GETTIMETABLE );
    case AdditionalDataTest:
        return i18nc("@info/plain", "Runs the %1() script function and tests collected additional data",
                     ServiceProviderScript::SCRIPT_FUNCTION_GETADDITIONALDATA );
    case StopSuggestionTest:
        return i18nc("@info/plain", "Runs the %1() script function with a stop name part as argument "
                                    "and tests collected stop suggestions",
                     ServiceProviderScript::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS );
    case StopSuggestionFromGeoPositionTest:
        return i18nc("@info/plain", "Runs the %1() script function with a geo position as argument "
                                    "and tests collected stop suggestions",
                     ServiceProviderScript::SCRIPT_FUNCTION_GETSTOPSUGGESTIONS );
    case JourneyTest:
        return i18nc("@info/plain", "Runs the %1() script function and tests collected journey data",
                     ServiceProviderScript::SCRIPT_FUNCTION_GETJOURNEYS );
    case FeaturesTest:
        return i18nc("@info/plain", "Runs the %1() script function and tests the returned list of "
                     "strings, which should name TimetableInformation enumerables",
                     ServiceProviderScript::SCRIPT_FUNCTION_FEATURES );
#endif

    default:
        kDebug() << "Unknown test" << test;
        return QString();
    }
}

QAction *TestModel::actionFromIndex( const QModelIndex &index )
{
    return index.data(SolutionActionRole).value< QAction* >();
}

ActionDelegate::ActionDelegate( QAbstractItemView *itemView, QObject *parent )
            : KWidgetItemDelegate(itemView, parent)
{
    QTreeView *treeView = qobject_cast< QTreeView* >( itemView );
    if ( treeView ) {
        connect( treeView->header(), SIGNAL(sectionResized(int,int,int)),
                this, SLOT(updateGeometry()) );
        connect( treeView->header(), SIGNAL(sectionMoved(int,int,int)),
                this, SLOT(updateGeometry()) );
    }
}

ActionDelegate::~ActionDelegate()
{
}

void ActionDelegate::updateGeometry()
{
    // Make KWidgetItemDelegate call updateItemWidgets() when a section was resized or moved
    // by making it process resize events (the size needs to really change,
    // sending a QResizeEvent with the same size does not work)
    const QSize size = itemView()->size();
    itemView()->resize( size.width() + 1, size.height() );
    itemView()->resize( size );
}

QList< QWidget * > ActionDelegate::createItemWidgets() const
{
    QToolButton *button = new QToolButton();
    setBlockedEventTypes( button, QList<QEvent::Type>() << QEvent::MouseButtonPress
                                  << QEvent::MouseButtonRelease << QEvent::MouseButtonDblClick );
    return QList<QWidget*>() << button;
}

void ActionDelegate::updateItemWidgets( const QList< QWidget * > widgets,
                                        const QStyleOptionViewItem &option,
                                        const QPersistentModelIndex &index ) const
{
    if ( widgets.isEmpty() ) {
        kDebug() << "No widgets";
        return;
    }

    QToolButton *button = qobject_cast< QToolButton* >( widgets.first() );
    button->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::MinimumExpanding );

    QAction *action = TestModel::actionFromIndex( index );
    if ( action && !option.rect.isEmpty() ) {
        // Initialize button
        button->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
        button->setDefaultAction( action );

        // Align button on the left and in the middle
        const QVariant alignmentVariant = index.data( Qt::TextAlignmentRole );
        Qt::Alignment alignment = Qt::AlignRight | Qt::AlignVCenter;
        QPoint point( 0, 0 );
        if ( alignment.testFlag(Qt::AlignRight) ) {
            point.setX( option.rect.width() - button->width() );
        } else if ( alignment.testFlag(Qt::AlignHCenter) ) {
            point.setX( (option.rect.width() - button->width()) / 2 );
        }
        if ( alignment.testFlag(Qt::AlignBottom) ) {
            point.setY( option.rect.height() - button->height() );
        } else if ( alignment.testFlag(Qt::AlignVCenter) ) {
            point.setY( (option.rect.height() - button->height()) / 2 );
        }
        button->move( point );

        // Resize button to fit into option.rect and have maximally the buttons size hint as size
        const QSize size = button->sizeHint();
        button->resize( qMin(size.width(), option.rect.width()),
                        qMin(size.height(), option.rect.height()) );

        // Show the button
        button->show();
    } else {
        // Hide button, if there is no action set in the model for index
        // or if the target rectangle is empty (eg. because it is a child item of a collapsed
        // parent in a QTreeView)
        button->hide();
    }
}

QSize ActionDelegate::toolButtonSize( const QStyleOptionViewItem &option,
                                      const QModelIndex &index ) const
{
    Q_UNUSED( option );
    QAction *action = TestModel::actionFromIndex( index );
    if ( !action ) {
        return QSize();
    }

    QStyleOptionToolButton buttonOption;
    buttonOption.toolButtonStyle = Qt::ToolButtonTextBesideIcon;
    buttonOption.text = action->text();
    buttonOption.icon = action->icon();
    buttonOption.iconSize = QSize( 22, 22 ); // a dummy icon size
    const QVariant &fontData = index.data( Qt::FontRole );
    const QFont font = fontData.isValid() && fontData.canConvert(QVariant::Font)
            ? index.data(Qt::FontRole).value<QFont>() : KGlobalSettings::generalFont();
    const QFontMetrics fontMetrics( font );
    const int buttonTextWidth = fontMetrics.width( buttonOption.text );
    return QApplication::style()->sizeFromContents(
            QStyle::CT_ToolButton, &buttonOption, QSize(qMax(24 + buttonTextWidth, 22), 22) );
}

QSize ActionDelegate::sizeHint( const QStyleOptionViewItem &option,
                                const QModelIndex &index ) const
{
    const QVariant &fontData = index.data( Qt::FontRole );
    const QFont font = fontData.isValid() && fontData.canConvert(QVariant::Font)
            ? index.data(Qt::FontRole).value<QFont>() : KGlobalSettings::generalFont();
    const QFontMetrics fontMetrics( font );
    const int textWidth = fontMetrics.width( index.data().toString() );
    const int textLines = qMax( 1, option.rect.height() / fontMetrics.lineSpacing() );
    const QSize buttonSize = toolButtonSize( option, index );
    return QSize( (textWidth / textLines) + 2 + buttonSize.width(),
                  qMax(fontMetrics.lineSpacing(), buttonSize.height()) );
}

void ActionDelegate::paint( QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index ) const
{
    const bool isSelected = option.state.testFlag( QStyle::State_Selected );
    QStyle* style = QApplication::style();
    const QVariant background = index.data( Qt::BackgroundRole );
    const QVariant foreground = index.data( Qt::ForegroundRole );
    QStyleOptionViewItemV4 opt( option );
    if ( background.isValid() ) {
        opt.backgroundBrush = background.value<QBrush>();
    }
    if ( foreground.isValid() ) {
        opt.palette.setColor( QPalette::Text, foreground.value<QBrush>().color() );
    }
    style->drawPrimitive( QStyle::PE_PanelItemViewItem, &opt, painter );

    const QVariant &fontData = index.data( Qt::FontRole );
    const QFont font = fontData.isValid() && fontData.canConvert(QVariant::Font)
            ? index.data(Qt::FontRole).value<QFont>() : KGlobalSettings::generalFont();
    const QFontMetrics fontMetrics( font );
    const QVariant alignment = index.data(Qt::TextAlignmentRole);

    painter->save();
    const QColor textColor = opt.palette.color( QPalette::Active, isSelected
                                                ? QPalette::BrightText : QPalette::Text );
    painter->setPen( textColor );
    const QRect textRect = opt.rect.adjusted( 2, 0, -toolButtonSize(option, index).width() + 2, 0 );
    const QString text = fontMetrics.elidedText( index.data().toString(),
                                                 Qt::ElideRight, textRect.width() );
    style->drawItemText( painter, textRect, alignment.isValid()
                         ? alignment.toInt() : int(Qt::AlignRight | Qt::AlignVCenter),
                         option.palette, option.state.testFlag(QStyle::State_Enabled), text );
    painter->restore();
}
