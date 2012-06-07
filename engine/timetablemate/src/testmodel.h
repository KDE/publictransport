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

#ifndef TESTMODEL_H
#define TESTMODEL_H

// KDE includes
#include <KWidgetItemDelegate>

// Qt inlcudes
#include <QAbstractItemModel>

class QAction;

Q_DECLARE_METATYPE( QAction* );
Q_DECLARE_METATYPE( Qt::AlignmentFlag );

struct TimetableDataRequestMessage {
    enum Type {
        Information = 0,
        Warning,
        Error
    };

    enum Feature {
        NoFeature = 0x0000,
        OpenLink  = 0x0001
    };
    Q_DECLARE_FLAGS( Features, Feature );

    TimetableDataRequestMessage( const QString &message, Type type = Information,
                                 int lineNumber = -1, Features features = NoFeature,
                                 const QVariant &data = QVariant() )
            : message(message), type(type), lineNumber(lineNumber),
              features(features), data(data) {};

    QString message;
    Type type;
    int lineNumber;
    Features features;
    QVariant data;
};

/** @brief Model for tests and it's results. */
class TestModel : public QAbstractItemModel {
    Q_OBJECT
    friend class ActionDelegate;

    Q_ENUMS( Column Role TestCase Test TestState )

public:
    /** @brief Available columns. */
    enum Column {
        NameColumn = 0,
        StateColumn,
        ExplanationColumn,

        ColumnCount /**< @internal */
    };

    /** @brief Additional roles for this model. */
    enum Role {
        LineNumberRole = Qt::UserRole, /**< Stores an associated line number, if any. */
        SolutionActionRole = Qt::UserRole + 1, /**< Stores a pointer to a QAction, which can be
                * used to solve a problem with a test. */
        FeatureRole = Qt::UserRole + 2, /**< Additional features of an index, eg. open an url
                * (stored in UrlRole). Can be used to determine which context menu entries to show.
                * Stored as TimetableDataRequestMessage::Features flags. */
        UrlRole = Qt::UserRole + 3 /**< An URL associated with an index as QString. */
    };

    /**
     * @brief Available test cases.
     * For each test case this model provides a top level item.
     **/
    enum TestCase {
        AccessorInfoTestCase = 0,
        ScriptExecutionTestCase,

        TestCaseCount, /**< @internal */
        InvalidTestCase
    };

    /**
     * @brief Available tests.
     * For each test this model provides a child item of the associated test case.
     **/
    enum Test {
        AccessorInfoNameTest = 0,
        AccessorInfoVersionTest,
        AccessorInfoFileVersionTest,
        AccessorInfoAuthorNameTest,
        AccessorInfoShortAuthorNameTest,
        AccessorInfoEmailTest,
        AccessorInfoUrlTest,
        AccessorInfoShortUrlTest,
        AccessorInfoScriptFileNameTest,
        AccessorInfoDescriptionTest,

        DepartureTest, /**< Tests for an implemented getTimetable() function and for valid
                * departure results. */
        ArrivalTest, /**< Tests for an implemented getTimetable() function and for valid
                * arrival results. */
        StopSuggestionTest, /**< Tests for an implemented getStopSuggestions() function and
                * for valid stop suggestion results. */
        JourneyTest, /**< Tests for an implemented getJourneys() function and for valid
                * journey results. */
        UsedTimetableInformationsTest, /**< Tests for an implemented usedTimetableInformations()
                * function and for valid results (a list of strings naming TimetableInformation). */

        TestCount, /**< @internal */
        InvalidTest // Should be the last enumerable
    };

    /**
     * @brief States of test cases.
     *
     * @warning The order of the enumarables is important. Enumarables with higher values will
     *   overwrite others with lower values, when the state of a test case gets checked.
     **/
    enum TestState {
        TestNotStarted = 0, /**< No test of the test case has been started. */
        TestFinishedSuccessfully, /**< The test or all tests of a test case finished successfully. */
        TestFinishedWithWarnings, /**< The test or all tests of a test case are finished,
                * at least one test had a warning, no test had an error. */
        TestFinishedWithErrors, /**< The test or all tests of a test case are finished
                * and at least one test had an error. */
        TestCouldNotBeStarted, /**< The test could not be started or at least one test of a test
                * case could not be started. */
        TestIsRunning /**< The test or at least one test of a test case is still running. */
    };

    /** @brief Constructor. */
    explicit TestModel( QObject *parent = 0 );

    Q_INVOKABLE virtual int columnCount( const QModelIndex &parent = QModelIndex() ) const {
            Q_UNUSED(parent); return ColumnCount; };
    Q_INVOKABLE virtual int rowCount( const QModelIndex &parent = QModelIndex() ) const;
    Q_INVOKABLE virtual QModelIndex index( int row, int column,
                                           const QModelIndex &parent = QModelIndex() ) const;
    Q_INVOKABLE virtual QModelIndex parent( const QModelIndex &child ) const;
    Q_INVOKABLE virtual QVariant data( const QModelIndex &index, int role = Qt::DisplayRole ) const;
    Q_INVOKABLE virtual QVariant headerData( int section, Qt::Orientation orientation,
                                             int role = Qt::DisplayRole ) const;
    virtual Qt::ItemFlags flags( const QModelIndex &index ) const;

    /** @brief Clear all test results. */
    Q_INVOKABLE void clear();

    /**
     * @brief Whether or not there are any test results in this model.
     *
     * @note If the model is empty, ie. contains no test results, rowCount() will still not return 0.
     *   The model still has valid indexes, but without any data stored (the test results).
     **/
    Q_INVOKABLE bool isEmpty() const;

    void markTestCaseAsUnstartable( TestCase testCase, const QString &errorMessage = QString(),
                                    const QString &tooltip = QString(), QAction *solution = 0 );
    void markTestAsStarted( Test test );
    void addTestResult( Test test, TestState state, const QString &explanation = QString(),
                        const QString &tooltip = QString(), QAction *solution = 0,
                        const QList< TimetableDataRequestMessage > &childrenExplanations =
                                QList< TimetableDataRequestMessage >() );

    /** @brief Get the QModelIndex for @p testCase. */
    Q_INVOKABLE QModelIndex indexFromTestCase( TestCase testCase, int column = 0 ) const;

    /** @brief Get the QModelIndex for @p test. */
    Q_INVOKABLE QModelIndex indexFromTest( Test test, int column = 0 ) const;

    Q_INVOKABLE TestCase testCaseFromIndex( const QModelIndex &testCaseIndex ) const;
    Q_INVOKABLE Test testFromIndex( const QModelIndex &testIndex ) const;

    /**
     * @brief Get the current state of @p testCase.
     * @see TestState
     **/
    Q_INVOKABLE TestState testCaseState( TestCase testCase ) const;

    /**
     * @brief Get the current state of @p test.
     * @see TestState
     **/
    Q_INVOKABLE TestState testState( Test test ) const;

    /**
     * @brief Whether or not there are erroneous tests.
     * Uses testCaseState() for all available test cases to check for errors.
     **/
    Q_INVOKABLE bool hasErroneousTests() const;

    /** @brief Get a list of all tests in @p testCase. */
    static QList< Test > testsOfTestCase( TestCase testCase );

    /** @brief Get the test case to which @p test belongs. */
    Q_INVOKABLE static TestCase testCaseOfTest( Test test );

    inline static TestState testStateFromBool( bool success ) {
           return success ? TestFinishedSuccessfully : TestFinishedWithErrors; };

    /** @brief Get a name for @p testCase. */
    Q_INVOKABLE static QString nameForTestCase( TestCase testCase );

    /** @brief Get a name for @p test. */
    Q_INVOKABLE static QString nameForTest( Test test );

    /** @brief Get a description for @p testCase. */
    Q_INVOKABLE static QString descriptionForTestCase( TestCase testCase );

    /** @brief Get a description for @p test. */
    Q_INVOKABLE static QString descriptionForTest( Test test );

protected:
    static QAction *actionFromIndex( const QModelIndex &index );

private:
    struct TestCaseData {
        TestCaseData( const QString &explanation = QString(), const QString &tooltip = QString(),
                      QAction *solution = 0 )
                : explanation(explanation), tooltip(tooltip), solution(solution) {};

        QString explanation;
        QString tooltip;
        QAction *solution;
    };

    struct TestData : public TestCaseData {
        TestData( TestState state = TestNotStarted,
                  const QString &explanation = QString(),
                  const QString &tooltip = QString(), QAction *solution = 0,
                  const QList< TimetableDataRequestMessage > &childrenExplanations =
                        QList< TimetableDataRequestMessage >() )
                : TestCaseData(explanation, tooltip, solution), state(state),
                  childrenExplanations(childrenExplanations) {};

        TestState state;
        QList< TimetableDataRequestMessage > childrenExplanations;

        inline bool isNotStarted() const { return state == TestNotStarted; };
        inline bool isRunning() const { return state == TestIsRunning; };
        inline bool isUnstartable() const { return state == TestCouldNotBeStarted; };
        inline bool isFinishedSuccessfully() const { return state == TestFinishedSuccessfully; };
        inline bool isFinishedWithErrors() const { return state == TestFinishedWithErrors; };
        inline bool isFinishedWithWarnings() const { return state == TestFinishedWithWarnings; };
        inline bool isFinished() const {
                return isFinishedSuccessfully() || isFinishedWithErrors() ||
                       isFinishedWithWarnings() || isUnstartable();
        };
    };

    void testChanged( Test test );

    QVariant testCaseData( TestCase testCase,
                           const QModelIndex &index, int role = Qt::DisplayRole ) const;
    QVariant testData( Test test, const QModelIndex &index, int role = Qt::DisplayRole ) const;

    void removeTestChildren( Test test );

    QFont getFont( const QVariant &fontData ) const;
    QBrush backgroundFromTestState( TestState testState ) const;
    QBrush foregroundFromTestState( TestState testState ) const;
    QBrush backgroundFromMessageType( TimetableDataRequestMessage::Type messageType ) const;
    QBrush foregroundFromMessageType( TimetableDataRequestMessage::Type messageType ) const;

    QHash< Test, TestData > m_testData;
    QHash< TestCase, TestCaseData > m_unstartableTestCases;
};

class ActionDelegate : public KWidgetItemDelegate {
    Q_OBJECT
    friend class TestModel;

public:
    explicit ActionDelegate( QAbstractItemView *itemView, QObject *parent = 0 );
    virtual ~ActionDelegate();

protected slots:
    void updateGeometry();

protected:
    virtual QList< QWidget* > createItemWidgets() const;
    virtual void updateItemWidgets( const QList< QWidget* > widgets,
                                    const QStyleOptionViewItem &option,
                                    const QPersistentModelIndex &index ) const;
    virtual QSize sizeHint( const QStyleOptionViewItem &option, const QModelIndex &index ) const;
    virtual void paint( QPainter *painter, const QStyleOptionViewItem &option,
                        const QModelIndex &index ) const;

    QSize toolButtonSize( const QStyleOptionViewItem &option, const QModelIndex &index ) const;
};

#endif // Multiple inclusion guard