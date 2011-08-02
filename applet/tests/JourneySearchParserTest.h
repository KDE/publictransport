/*
 *   Copyright 2011 Friedrich Pülz <fpuelz@gmx.de>
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

#ifndef JourneySearchParserTest_H
#define JourneySearchParserTest_H

#define QT_GUI_LIB

#include <QtCore/QObject>

class JourneySearchKeywords;
class JourneySearchParserTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void cleanupTestCase();

    void journeySearchParserTest_data();
    void journeySearchParserTest();

    void benchmarkLexicalTest();
    void benchmarkSyntacticalTest();
    void benchmarkContextualTest();
    void benchmarkTest();
//     void benchmarkTestOld();

private:
    JourneySearchKeywords *m_keywords;
};

#endif // Multiple inclusion guard
