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

// Own includes
#include "project.h"

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

Project *TestModel::project() const
{
    return qobject_cast< Project* >( QObject::parent() );
}

void TestModel::markTestCaseAsUnstartable( TestModel::TestCase testCase,
                                           const QString &errorMessage, const QString &tooltip,
                                           QAction *solution )
{
    m_unstartableTestCases[ testCase ] = TestCaseData( errorMessage, tooltip, solution );
    emit dataChanged( indexFromTestCase(testCase, 0), indexFromTestCase(testCase, ColumnCount - 1) );
    emit testResultsChanged();
}

void TestModel::markTestAsStarted( TestModel::Test test )
{
    removeTestChildren( test );
    m_testData[ test ] = TestData( TestIsRunning );
    testChanged( test );
}

void TestModel::markTestsAsOutdated( const QList< TestModel::Test >& tests )
{
    foreach ( Test test, tests ) {
        if ( m_testData.contains(test) ) {
            m_testData[ test ].flags |= TestIsOutdated;
            testChanged( test );
        }
    }
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

TestModel::TestState TestModel::setTestState( TestModel::Test test, TestState state,
        const QString &explanation, const QString &tooltip, QAction *solution,
        const QList< TimetableDataRequestMessage > &childrenExplanations,
        const QList< TimetableData > &results, const QSharedPointer<AbstractRequest> &request )
{
    const QModelIndex testIndex = indexFromTest( test );
    removeTestChildren( test );

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
    return state;
}

void TestModel::testChanged( TestModel::Test test )
{
    emit dataChanged( indexFromTest(test, 0), indexFromTest(test, ColumnCount - 1) );

    TestCase testCase = testCaseOfTest( test );
    emit dataChanged( indexFromTestCase(testCase, 0), indexFromTestCase(testCase, ColumnCount - 1) );

    emit testResultsChanged();
}

void TestModel::clear()
{
    for ( int i = 0; i< TestCount; ++i ) {
        removeTestChildren( static_cast<Test>(i) );
    }
    m_unstartableTestCases.clear();
    m_testData.clear();
    emit dataChanged( index(0, 0), index(TestCaseCount - 1, ColumnCount - 1) );

    emit testResultsChanged();
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
                // Parent is a test item child, no more children
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
        // Do not show test cases without applicable tests
        int count = TestCaseCount;
        if ( !isTestCaseApplicableTo(ScriptExecutionTestCase, project()->data()) ) {
            --count;
        }
        if ( !isTestCaseApplicableTo(GtfsTestCase, project()->data()) ) {
            --count;
        }
        return count;
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
        // Do not show test cases without applicable tests
        int testCase = testCaseIndex.row();
        if ( testCase >= ScriptExecutionTestCase &&
            !isTestCaseApplicableTo(ScriptExecutionTestCase, project()->data()) )
        {
            ++testCase;
        }
        if ( testCase >= GtfsTestCase &&
            !isTestCaseApplicableTo(GtfsTestCase, project()->data()) )
        {
            ++testCase;
        }
        const TestCase enumTestCase = static_cast< TestCase >( testCase );
        return enumTestCase == TestCaseCount ? InvalidTestCase : enumTestCase;
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
    if ( testCase >= TestCaseCount ) {
        return QModelIndex();
    } else {
        int row = static_cast< int >( testCase );
        if ( row >= ScriptExecutionTestCase &&
            !isTestCaseApplicableTo(ScriptExecutionTestCase, project()->data()) )
        {
            --row;
        }
        if ( testCase >= GtfsTestCase &&
            !isTestCaseApplicableTo(GtfsTestCase, project()->data()) )
        {
            --row;
        }
        return createIndex(row, column, -1);
    }
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

QString TestModel::nameForState( TestModel::TestState state )
{
    switch ( state ) {
    case TestNotStarted:
        return i18nc("@info/plain", "Not Started");
    case TestCaseNotFinished:
        return i18nc("@info/plain", "Not Finished");
    case TestDelegated:
        return i18nc("@info/plain", "Delegated");
    case TestDisabled:
        return i18nc("@info/plain", "Disabled");
    case TestNotApplicable:
        return i18nc("@info/plain", "Not Applicable");
    case TestIsRunning:
        return i18nc("@info/plain", "Running");
    case TestFinishedSuccessfully:
        return i18nc("@info/plain", "Success");
    case TestFinishedWithWarnings:
        return i18nc("@info/plain", "Warnings");
    case TestAborted:
        return i18nc("@info/plain", "Aborted");
    case TestFinishedWithErrors:
    case TestCouldNotBeStarted:
        return i18nc("@info/plain", "Failed");
    default:
        kDebug() << "Unexpected test state" << state;
        return "Unknown";
    }
}

QVariant TestModel::testCaseData( TestModel::TestCase testCase, const QModelIndex &index, int role ) const
{
    switch ( role ) {
    case Qt::DisplayRole:
        switch ( index.column() ) {
        case NameColumn:
            return nameForTestCase( testCase );
        case StateColumn:
            return nameForState( testCaseState(testCase) );
        case ExplanationColumn: {
            const TestState state = testCaseState( testCase );
            if ( (isFinishedState(state) || state == TestDelegated || state == TestAborted) &&
                 !m_unstartableTestCases[testCase].explanation.isEmpty() )
            {
                return m_unstartableTestCases[ testCase ].explanation;
            }
            return descriptionForTestCase( testCase );
        } break;
        }
        break;

    case Qt::DecorationRole:
        if ( index.column() == 0 ) {
            const TestState state = testCaseState( testCase );
            switch ( state ) {
            case TestNotStarted:
            case TestCaseNotFinished:
                return KIcon("arrow-right");
            case TestDisabled:
            case TestNotApplicable:
                return KIcon("dialog-cancel");
            case TestDelegated:
                return KIcon("task-delegate");
            case TestIsRunning:
                return KIcon("task-ongoing");
            case TestFinishedSuccessfully:
                return KIcon("task-complete");
            case TestFinishedWithWarnings:
                return KIcon("dialog-warning");
            case TestAborted:
                return KIcon("process-stop");
            case TestFinishedWithErrors:
            case TestCouldNotBeStarted:
                return KIcon("task-reject");
            }
        }
        break;

    case Qt::ToolTipRole:
        if ( index.column() == ExplanationColumn ) {
            const TestState state = testCaseState( testCase );
            if ( (isFinishedState(state) || state == TestDelegated) &&
                 !m_unstartableTestCases[testCase].tooltip.isEmpty() )
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
        case StateColumn:
            if ( m_testData.contains(test) ) {
                if ( m_testData[test].flags.testFlag(TestIsOutdated) ) {
                    return i18nc("@info/plain", "Outdated");
                } else {
                    return nameForState( m_testData[test].state );
                }
            }
            return nameForState( TestNotStarted );
        case ExplanationColumn:
            if ( m_testData.contains(test) ) {
                const TestData testData = m_testData[ test ];
                if ( (testData.isFinished() || testData.isDelegated() || testData.isAborted()) &&
                     !testData.explanation.isEmpty() )
                {
                    return testData.explanation;
                }
            }
            return descriptionForTest( test );
        }
        break;

    case Qt::ToolTipRole:
        if ( index.column() == ExplanationColumn ) {
            if ( m_testData.contains(test) ) {
                const TestData testData = m_testData[ test ];
                if ( testData.isFinished() || testData.isDelegated() ) {
                    return !testData.tooltip.isEmpty() ? testData.tooltip : testData.explanation;
                }
            }
            return descriptionForTest( test );
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
            case TestCaseNotFinished:
                return KIcon("arrow-right");
            case TestDisabled:
            case TestNotApplicable:
                return KIcon("dialog-cancel");
            case TestDelegated:
                return KIcon("task-delegate");
            case TestIsRunning:
                return KIcon("task-ongoing");
            case TestFinishedSuccessfully:
                return KIcon("task-complete");
            case TestFinishedWithWarnings:
                return KIcon("dialog-warning");
            case TestAborted:
                return KIcon("process-stop");
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

    case Qt::BackgroundRole: {
        const QVariant color = backgroundFromTestState( testState(test) );
        if ( color.isValid() && m_testData.contains(test) &&
             m_testData[test].flags.testFlag(TestIsOutdated) )
        {
            return KColorUtils::mix( color.value<QBrush>().color(),
                                     KColorScheme(QPalette::Active).background().color() );
        }
        return color;
    }
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
                        const TimetableDataRequestMessage message =
                                testData.childrenExplanations[ index.row() ];
                        return message.repetitions > 0
                                ? i18nc("@info/plain Always plural", "%1 times: %2",
                                        message.repetitions + 1, message.message)
                                : message.message;
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
                case FileNameRole: {
                    // Return the associated file name, if any
                    const TestData &testData = m_testData[ test ];
                    return index.row() >= testData.childrenExplanations.count()
                            ? QVariant() : testData.childrenExplanations[index.row()].fileName;
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
        return testCaseData( testCaseFromIndex(index), index, role );
    }

    return QVariant();
}

QVariant TestModel::backgroundFromTestState( TestModel::TestState state ) const
{
    switch ( state ) {
    case TestFinishedSuccessfully:
        return KColorScheme(QPalette::Active).background(KColorScheme::PositiveBackground);
    case TestFinishedWithWarnings:
        return KColorScheme(QPalette::Active).background(KColorScheme::NeutralBackground);
    case TestCouldNotBeStarted:
    case TestFinishedWithErrors:
    case TestAborted:
        return KColorScheme(QPalette::Active).background(KColorScheme::NegativeBackground);
    case TestDisabled:
    case TestNotApplicable:
    case TestDelegated:
    case TestIsRunning:
    case TestNotStarted:
    case TestCaseNotFinished:
    default:
        return QVariant();
    }
}

QVariant TestModel::foregroundFromTestState( TestModel::TestState state ) const
{
    switch ( state ) {
    case TestFinishedSuccessfully:
        return KColorScheme(QPalette::Active).foreground(KColorScheme::PositiveText);
    case TestFinishedWithWarnings:
        return KColorScheme(QPalette::Active).foreground(KColorScheme::NeutralText);
    case TestCouldNotBeStarted:
    case TestFinishedWithErrors:
    case TestAborted:
        return KColorScheme(QPalette::Active).foreground(KColorScheme::NegativeText);
    case TestDelegated:
    case TestIsRunning:
    case TestNotStarted:
    case TestCaseNotFinished:
    default:
        return QVariant();
    }
}

QVariant TestModel::backgroundFromMessageType( TimetableDataRequestMessage::Type messageType ) const
{
    switch ( messageType ) {
    case TimetableDataRequestMessage::Warning:
        return KColorScheme(QPalette::Active).background(KColorScheme::NeutralBackground);
    case TimetableDataRequestMessage::Error:
        return KColorScheme(QPalette::Active).background(KColorScheme::NegativeBackground);
    case TimetableDataRequestMessage::Information:
    default:
        return QVariant();
    }
}

QVariant TestModel::foregroundFromMessageType( TimetableDataRequestMessage::Type messageType ) const
{
    switch ( messageType ) {
    case TimetableDataRequestMessage::Warning:
        return KColorScheme(QPalette::Active).foreground(KColorScheme::NeutralText);
    case TimetableDataRequestMessage::Error:
        return KColorScheme(QPalette::Active).foreground(KColorScheme::NegativeText);
    case TimetableDataRequestMessage::Information:
    default:
        return QVariant();
    }
}

Qt::ItemFlags TestModel::flags( const QModelIndex &index ) const
{
    if ( !index.isValid() ) {
        return Qt::NoItemFlags;
    }

    // Get the test of the index, if any
    const Test test = testFromIndex( index );
    if ( test != InvalidTest ) {
        // The item at index is a test item
        const TestState state = testState( test );
        if ( state == TestDisabled || state == TestNotApplicable ||
             (m_testData.contains(test) && m_testData[test].flags.testFlag(TestIsOutdated)) )
        {
            return Qt::ItemIsSelectable;
        }
    } else {
        // The item at index is a test case item
        const TestCase testCase = testCaseFromIndex( index );
        const TestState state = testCaseState( testCase );
        if ( state == TestDisabled || state == TestNotApplicable ) {
            return Qt::ItemIsSelectable;
        }
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
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
    if ( isErroneousTestState(testCaseState(ScriptExecutionTestCase)) ) {
        return true;
    }
    if ( isErroneousTestState(testCaseState(GtfsTestCase)) ) {
        return true;
    }
    return isErroneousTestState( testCaseState(ServiceProviderDataTestCase) );
}

float TestModel::progress() const
{
    return float(finishedTests().count()) / float(TestCount);
}

QList< TestModel::Test > TestModel::finishedTests() const
{
    QList< Test > tests;
    for ( int i = 0; i < TestCount; ++i ) {
        const Test test = static_cast< Test >( i );
        if ( isTestFinished(test) ) {
            tests << test;
        }
    }
    return tests;
}

QList< TestModel::Test > TestModel::startedTests() const
{
    return project()->startedTests();
}

QList< TestModel::Test > TestModel::allTests()
{
    QList< TestModel::Test > tests;
    for ( int i = 0; i < TestCount; ++i ) {
        tests << static_cast< Test >( i );
    }
    return tests;
}

TestModel::TestState TestModel::completeState() const
{
    TestState completeState = TestNotStarted;
    for ( int i = 0; i < TestCaseCount; ++i ) {
        const TestCase testCase = static_cast< TestCase >( i );
        const TestState state = testCaseState( testCase );
        if ( !isFinishedState(state) && state != TestIsRunning && state != TestAborted ) {
            return TestCaseNotFinished;
        } else if ( state > completeState ) {
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
        if ( !isFinishedState(state) && state != TestIsRunning && state != TestAborted ) {
            return TestCaseNotFinished;
        } else if ( state > caseState ) {
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

bool TestModel::isTestApplicableTo( Test test, const ServiceProviderData *data,
                                    QString *errorMessage, QString *tooltip )
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
    case ServiceProviderDataDescriptionTest:
        return true;

    case LoadScriptTest:
    case DepartureTest:
    case ArrivalTest:
    case AdditionalDataTest:
    case StopSuggestionTest:
    case StopsByGeoPositionTest:
    case JourneyTest:
    case FeaturesTest:
    case ServiceProviderDataScriptFileNameTest:
        if ( data->type() == Enums::ScriptedProvider ) {
            return true;
        }
        if ( errorMessage ) {
            *errorMessage = i18nc("@info/plain", "Only for scripted providers");
        }
        if ( tooltip ) {
            *tooltip = i18nc("@info", "<title>Test not Applicable</title> "
                            "<para>This test is only applicable for scripted provider plugins.</para>");
        }
        return false;

    case GtfsFeedExistsTest:
    case GtfsRealtimeUpdatesTest:
    case GtfsRealtimeAlertsTest:
    case ServiceProviderDataGtfsFeedUrlTest:
    case ServiceProviderDataGtfsRealtimeUpdatesUrlTest:
    case ServiceProviderDataGtfsRealtimeAlertsTest:
        if ( data->type() == Enums::GtfsProvider ) {
            return true;
        }
        if ( errorMessage ) {
            *errorMessage = i18nc("@info/plain", "Only for GTFS providers");
        }
        if ( tooltip ) {
            *tooltip = i18nc("@info", "<title>Test not Applicable</title> "
                            "<para>This test is only applicable for GTFS provider plugins.</para>");
        }
        return false;

    default:
        kDebug() << "Unknown test" << test;
        return false;
    }
}

bool TestModel::isTestCaseApplicableTo( TestModel::TestCase testCase,
        const ServiceProviderData *data, QString *errorMessage, QString *tooltip )
{
    const QList< TestModel::Test > tests = testsOfTestCase( testCase );
    foreach ( TestModel::Test test, tests ) {
        if ( isTestApplicableTo(test, data, errorMessage, tooltip) ) {
            // Found an applicable test of the test case,
            // which is therefore also applicable
            return true;
        }
    }

    // The test case contains no applicable tests
    return false;
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
              << ServiceProviderDataShortUrlTest << ServiceProviderDataDescriptionTest
              << ServiceProviderDataScriptFileNameTest << ServiceProviderDataGtfsFeedUrlTest
              << ServiceProviderDataGtfsRealtimeUpdatesUrlTest
              << ServiceProviderDataGtfsRealtimeAlertsTest;
        break;
    case ScriptExecutionTestCase:
        tests << LoadScriptTest << DepartureTest << ArrivalTest << AdditionalDataTest
              << StopSuggestionTest << StopsByGeoPositionTest << JourneyTest << FeaturesTest;
        break;
    case GtfsTestCase:
        tests << GtfsFeedExistsTest << GtfsRealtimeUpdatesTest << GtfsRealtimeAlertsTest;
        break;
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
    case ServiceProviderDataDescriptionTest:
    case ServiceProviderDataScriptFileNameTest:
    case ServiceProviderDataGtfsFeedUrlTest:
    case ServiceProviderDataGtfsRealtimeUpdatesUrlTest:
    case ServiceProviderDataGtfsRealtimeAlertsTest:
        return ServiceProviderDataTestCase;

    case LoadScriptTest:
    case DepartureTest:
    case ArrivalTest:
    case AdditionalDataTest:
    case StopSuggestionTest:
    case StopsByGeoPositionTest:
    case JourneyTest:
    case FeaturesTest:
        return ScriptExecutionTestCase;

    case GtfsFeedExistsTest:
    case GtfsRealtimeUpdatesTest:
    case GtfsRealtimeAlertsTest:
        return GtfsTestCase;

    default:
        kDebug() << "Unknown test" << test;
        return InvalidTestCase;
    }
}

QList< TestModel::Test > TestModel::testIsDependedOf( TestModel::Test test )
{
    switch ( test ) {
    case GtfsFeedExistsTest:
        return QList< TestModel::Test >() << ServiceProviderDataGtfsFeedUrlTest;
    case GtfsRealtimeUpdatesTest:
        return QList< TestModel::Test >() << ServiceProviderDataGtfsRealtimeUpdatesUrlTest;
    case GtfsRealtimeAlertsTest:
        return QList< TestModel::Test >() << ServiceProviderDataGtfsRealtimeAlertsTest;

    case AdditionalDataTest:
        return QList< TestModel::Test >() << ServiceProviderDataScriptFileNameTest
                << LoadScriptTest << FeaturesTest << DepartureTest;

    case ArrivalTest:
    case StopsByGeoPositionTest:
        return QList< TestModel::Test >() << ServiceProviderDataScriptFileNameTest
                << LoadScriptTest << FeaturesTest;

    case DepartureTest:
    case StopSuggestionTest:
    case JourneyTest:
    case FeaturesTest:
        return QList< TestModel::Test >() << ServiceProviderDataScriptFileNameTest
                << LoadScriptTest;

    case LoadScriptTest:
        return QList< TestModel::Test >() << ServiceProviderDataScriptFileNameTest;

    case ServiceProviderDataNameTest:
    case ServiceProviderDataVersionTest:
    case ServiceProviderDataFileFormatVersionTest:
    case ServiceProviderDataAuthorNameTest:
    case ServiceProviderDataShortAuthorNameTest:
    case ServiceProviderDataEmailTest:
    case ServiceProviderDataUrlTest:
    case ServiceProviderDataShortUrlTest:
    case ServiceProviderDataDescriptionTest:
    case ServiceProviderDataScriptFileNameTest:
    case ServiceProviderDataGtfsFeedUrlTest:
    case ServiceProviderDataGtfsRealtimeUpdatesUrlTest:
    case ServiceProviderDataGtfsRealtimeAlertsTest:
    default:
        return QList< TestModel::Test >();
    }
}

QString TestModel::nameForTestCase( TestModel::TestCase testCase )
{
    switch ( testCase ) {
    case ServiceProviderDataTestCase:
        return i18nc("@info/plain", "Project Settings Test Case" );
    case ScriptExecutionTestCase:
        return i18nc("@info/plain", "Script Execution Test Case" );
    case GtfsTestCase:
        return i18nc("@info/plain", "GTFS Test Case" );
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
    case ServiceProviderDataDescriptionTest:
        return i18nc("@info/plain", "Description Test" );
    case ServiceProviderDataScriptFileNameTest:
        return i18nc("@info/plain", "Script File Test" );
    case ServiceProviderDataGtfsFeedUrlTest:
        return i18nc("@info/plain", "GTFS Feed URL Test" );
    case ServiceProviderDataGtfsRealtimeUpdatesUrlTest:
        return i18nc("@info/plain", "GTFS-realtime Updates URL Test" );
    case ServiceProviderDataGtfsRealtimeAlertsTest:
        return i18nc("@info/plain", "GTFS-realtime Alerts URL Test" );

    case LoadScriptTest:
        return i18nc("@info/plain", "Load Script Test" );
    case DepartureTest:
        return i18nc("@info/plain", "Departure Test" );
    case ArrivalTest:
        return i18nc("@info/plain", "Arrival Test" );
    case AdditionalDataTest:
        return i18nc("@info/plain", "Additional Data Test" );
    case StopSuggestionTest:
        return i18nc("@info/plain", "Stop Suggestion Test" );
    case StopsByGeoPositionTest:
        return i18nc("@info/plain", "Stops by Geo Position Test" );
    case JourneyTest:
        return i18nc("@info/plain", "Journey Test" );
    case FeaturesTest:
        return i18nc("@info/plain", "Features Test" );

    case GtfsFeedExistsTest:
        return i18nc("@info/plain", "GTFS Feed Exists Test" );
    case GtfsRealtimeUpdatesTest:
        return i18nc("@info/plain", "GTFS-realtime Updates Test" );
    case GtfsRealtimeAlertsTest:
        return i18nc("@info/plain", "GTFS-realtime Alerts Test" );

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
    case ScriptExecutionTestCase:
        return i18nc("@info/plain", "Runs script functions and tests collected data" );
    case GtfsTestCase:
        return i18nc("@info/plain", "Test GTFS feeds" );
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
    case ServiceProviderDataDescriptionTest:
        return i18nc("@info/plain", "Tests for a valid description" );
    case ServiceProviderDataScriptFileNameTest:
        return i18nc("@info/plain", "Tests for a valid script file" );
    case ServiceProviderDataGtfsFeedUrlTest:
        return i18nc("@info/plain", "Tests for a valid GTFS feed URL" );
    case ServiceProviderDataGtfsRealtimeUpdatesUrlTest:
        return i18nc("@info/plain", "Tests for a valid GTFS-realtime updates URL" );
    case ServiceProviderDataGtfsRealtimeAlertsTest:
        return i18nc("@info/plain", "Tests for a valid GTFS-realtime alerts URL" );

#ifdef BUILD_PROVIDER_TYPE_SCRIPT
    case LoadScriptTest:
        return i18nc("@info/plain", "Tries to load the script, fails on syntax errors" );
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
    case StopsByGeoPositionTest:
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
#else
    case LoadScriptTest:
    case DepartureTest:
    case ArrivalTest:
    case AdditionalDataTest:
    case StopSuggestionTest:
    case StopsByGeoPositionTest:
    case JourneyTest:
    case FeaturesTest:
        return QString();
#endif // BUILD_PROVIDER_TYPE_SCRIPT

    case GtfsFeedExistsTest:
        return i18nc("@info/plain", "Checks if the feed at the given URL exists" );
    case GtfsRealtimeUpdatesTest:
        return i18nc("@info/plain", "Tests GTFS-realtime updates, if used" );
    case GtfsRealtimeAlertsTest:
        return i18nc("@info/plain", "Tests GTFS-realtime alerts, if used" );

    default:
        kDebug() << "Unknown test" << test;
        return QString();
    }
}

QAction *TestModel::actionFromIndex( const QModelIndex &index )
{
    return index.data(SolutionActionRole).value< QAction* >();
}
