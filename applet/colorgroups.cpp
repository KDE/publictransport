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
#include "colorgroups.h"

// Qt includes
#include <QColor>

struct TargetCounter {
    QString target;
    QString displayText;
    int usedCount;

    TargetCounter() {};
    TargetCounter( const QString &target, const QString &displayText, int occurenceCount = 1 )
            : target(target), displayText(displayText), usedCount(occurenceCount) {};
    TargetCounter &operator ++() { ++usedCount; return *this; };
};

class TargetCounterGreaterThan {
public:
    inline TargetCounterGreaterThan() {};
    inline bool operator()( const TargetCounter &l, const TargetCounter &r ) const {
        return l.usedCount > r.usedCount;
    };
};

ColorGroupSettingsList ColorGroups::generateColorGroupSettingsFrom(
        const QList< DepartureInfo >& infoList, DepartureArrivalListType departureArrivalListType )
{
    // Maximal number of groups
    const int maxGroupCount = 10;

    const int opacity = 128;
    const int colors = 82;
    static const QColor oxygenColors[colors] = {
        QColor(56,   37,   9, opacity), //wood brown6
//         QColor(87,   64,  30, opacity), // wood brown5
        QColor(117,  81,  26, opacity), // wood brown4
        QColor(143, 107,  50, opacity), // wood brown3
        QColor(179, 146,  93, opacity), // wood brown2
//         QColor(222, 188, 133, opacity), // wood brown1
        QColor(156,  15,  15, opacity), // brick red6
//         QColor(191,   3,   3, opacity), // brick red5
        QColor(226,   8,   0, opacity), // brick red4
        QColor(232,  87,  82, opacity), // brick red3
        QColor(240, 134, 130, opacity), // brick red2
//         QColor(249, 204, 202, opacity), // brick red1
        QColor(156,  15,  86, opacity), // raspberry pink6
//         QColor(191,   3,  97, opacity), // raspberry pink5
        QColor(226,   0, 113, opacity), // raspberry pink4
        QColor(232,  82, 144, opacity), // raspberry pink3
        QColor(240, 130, 176, opacity), // raspberry pink2
//         QColor(249, 202, 222, opacity), // raspberry pink1
        QColor(106,   0,  86, opacity), // burgundy purple6
//         QColor(133,   2, 108, opacity), // burgundy purple5
        QColor(160,  39, 134, opacity), // burgundy purple4
        QColor(177,  79, 154, opacity), // burgundy purple3
        QColor(193, 115, 176, opacity), // burgundy purple2
//         QColor(232, 183, 215, opacity), // burgundy purple1
        QColor(29,   10,  85, opacity), // grape violet6
//         QColor(52,   23, 110, opacity), // grape violet5
        QColor(70,   40, 134, opacity), // grape violet4
        QColor(100,  74, 155, opacity), // grape violet3
        QColor(142, 121, 165, opacity), // grape violet2
//         QColor(195, 180, 218, opacity), // grape violet1
        QColor(0,    49, 110, opacity), // skyblue6
//         QColor(0,    67, 138, opacity), // skyblue5
        QColor(0,    87, 174, opacity), // skyblue4
        QColor(44,  114, 199, opacity), // skyblue3
        QColor(97,  147, 207, opacity), // skyblue2
//         QColor(164, 192, 228, opacity), // skyblue1
        QColor(0,    72,  77, opacity), // sea blue6
//         QColor(0,    96, 102, opacity), // sea blue5
        QColor(0,   120, 128, opacity), // sea blue4
        QColor(0,   167, 179, opacity), // sea blue3
        QColor(0,   196, 204, opacity), // sea blue2
//         QColor(168, 221, 224, opacity), // sea blue1
        QColor(0,    88,  63, opacity), // emerald green6
//         QColor(0,   115,  77, opacity), // emerald green5
        QColor(0,   153, 102, opacity), // emerald green4
        QColor(0,   179, 119, opacity), // emerald green3
        QColor(0,   204, 136, opacity), // emerald green2
//         QColor(153, 220, 198, opacity), // emerald green1
        QColor(0,   110,  41, opacity), // forest green6
//         QColor(0,   137,  44, opacity), // forest green5
        QColor(55,  164,  44, opacity), // forest green4
        QColor(119, 183,  83, opacity), // forest green3
        QColor(177, 210, 143, opacity), // forest green2
//         QColor(216, 232, 194, opacity), // forest green1
        QColor(227, 173,   0, opacity), // sun yellow6
//         QColor(243, 195,   0, opacity), // sun yellow5
        QColor(255, 221,   0, opacity), // sun yellow4
        QColor(255, 235,  85, opacity), // sun yellow3
        QColor(255, 242, 153, opacity), // sun yellow2
//         QColor(255, 246, 200, opacity), // sun yellow1
        QColor(172,  67,  17, opacity), // hot orange6
//         QColor(207,  73,  19, opacity), // hot orange5
        QColor(235, 115,  49, opacity), // hot orange4
        QColor(242, 155, 104, opacity), // hot orange3
        QColor(242, 187, 136, opacity), // hot orange2
//         QColor(255, 217, 176, opacity), // hot orange1
        QColor(46,   52,  54, opacity), // aluminum gray6
//         QColor(85,   87,  83, opacity), // aluminum gray5
        QColor(136, 138, 133, opacity), // aluminum gray4
        QColor(186, 189, 182, opacity), // aluminum gray3
        QColor(211, 215, 207, opacity), // aluminum gray2
//         QColor(238, 238, 236, opacity), // aluminum gray1
        QColor(77,   38,   0, opacity), // brown orange6
//         QColor(128,  63,   0, opacity), // brown orange5
        QColor(191,  94,   0, opacity), // brown orange4
        QColor(255, 126,   0, opacity), // brown orange3
        QColor(255, 191, 128, opacity), // brown orange2
//         QColor(255, 223, 191, opacity), // brown orange1
        QColor(89,    0,   0, opacity), // red6
//         QColor(140,   0,   0, opacity), // red5
        QColor(191,   0,   0, opacity), // red4
        QColor(255,   0,   0, opacity), // red3
        QColor(255, 128, 128, opacity), // red2
//         QColor(255, 191, 191, opacity), // red1
        QColor(115,   0,  85, opacity), // pink6
//         QColor(163,   0, 123, opacity), // pink5
        QColor(204,   0, 154, opacity), // pink4
        QColor(255,   0, 191, opacity), // pink3
        QColor(255, 128, 223, opacity), // pink2
//         QColor(255, 191, 240, opacity), // pink1
        QColor(44,    0,  89, opacity), // purple6
//         QColor(64,    0, 128, opacity), // purple5
        QColor(90,    0, 179, opacity), // purple4
        QColor(128,   0, 255, opacity), // purple3
        QColor(192, 128, 255, opacity), // purple2
//         QColor(223, 191, 255, opacity), // purple1
        QColor(0,     0, 128, opacity), // blue6
//         QColor(0,     0, 191, opacity), // blue5
        QColor(0,     0, 255, opacity), // blue4
        QColor(0,   102, 255, opacity), // blue3
        QColor(128, 179, 255, opacity), // blue2
//         QColor(191, 217, 255, opacity), // blue1
        QColor(0,    77,   0, opacity), // green6
//         QColor(0,   140,   0, opacity), // green5
        QColor(0,   191,   0, opacity), // green4
        QColor(0,   255,   0, opacity), // green3
        QColor(128, 255, 128, opacity), // green2
//         QColor(191, 255, 191, opacity), // green1
        QColor(99,  128,   0, opacity), // lime6
//         QColor(139, 179,   0, opacity), // lime5
        QColor(191, 245,   0, opacity), // lime4
        QColor(229, 255,   0, opacity), // lime3
        QColor(240, 255, 128, opacity), // lime2
//         QColor(248, 255, 191, opacity), // lime1
        QColor(255, 170,   0, opacity), // yellow6
//         QColor(255, 191,   0, opacity), // yellow5
        QColor(255, 213,   0, opacity), // yellow4
        QColor(255, 255,   0, opacity), // yellow3
        QColor(255, 255, 153, opacity), // yellow2
//         QColor(255, 255, 191, opacity), // yellow1
        QColor(50,   50,  50, opacity), // gray6
//         QColor(85,   85,  85, opacity) // gray5
        QColor(136, 136, 136, opacity), // gray4
//         QColor(187, 187, 187, opacity), // gray3
//         QColor(221, 221, 221, opacity), // gray2
//         QColor(238, 238, 238, opacity)  // gray1
    };

    // Count target usages
    QHash< QString, TargetCounter > targetUsedInformation;
    foreach ( const DepartureInfo &info, infoList ) {
        const QString target = info.target();
        const QString displayText = info.targetShortened();

        // Check if the target was already counted
        if ( targetUsedInformation.contains(target) ) {
            // Increment the used count of the target by one
            ++targetUsedInformation[target];
        } else {
            // Add new target with count 1
            targetUsedInformation.insert( target, TargetCounter(target, displayText) );
        }
    }

    // Sort list of targets by used count
    QList< TargetCounter > targetCount = targetUsedInformation.values();
    qStableSort( targetCount.begin(), targetCount.end(), TargetCounterGreaterThan() );

    // Create the color groups
    ColorGroupSettingsList colorGroups;
    for ( int i = 0; i < maxGroupCount && i < targetCount.count(); ++i ) {
        const TargetCounter &routeCount = targetCount[i];
        QString hashString = routeCount.target;
        while ( hashString.length() < 3 ) {
            hashString += 'z';
        }
        const int color = qHash(hashString) % colors;

        // Create filter for the new color group
        Filter groupFilter;
        groupFilter << Constraint( FilterByTarget, FilterEquals, routeCount.target );

        // Create the new color group
        ColorGroupSettings group;
        group.filters << groupFilter;
        group.color = oxygenColors[color];
        group.target = routeCount.target;
        group.displayText = routeCount.displayText;

        colorGroups << group;
    }

    return colorGroups;
}
