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

#ifndef OVERLAYWIDGET_HEADER
#define OVERLAYWIDGET_HEADER

/** @file
 * @brief This file contains the OverlayWidget class.
 * @author Friedrich Pülz <fpuelz@gmx.de> */

#include <QGraphicsWidget> // Base class

#if QT_VERSION >= 0x040600
class QGraphicsBlurEffect;
#endif

/**
 * @brief A widget which gets laid over the whole applet.
 *
 * Used for the action button view.
 * Mostly copied from Plasma::Applet (the AppletOverlayWidget displayed when calling
 * Plasma::Applet::setConfigurationRequired(true)). But with a blur effect ;)
 **/
class OverlayWidget : public QGraphicsWidget {
    Q_OBJECT

public:
    explicit OverlayWidget( QGraphicsWidget* parent = 0, QGraphicsWidget* under = 0 );
    void destroy();

protected:
    virtual void paint( QPainter* painter, const QStyleOptionGraphicsItem* option,
                        QWidget* widget = 0 );

public slots:
    void overlayAnimationComplete();

private:
    qreal opacity;
    QGraphicsWidget* m_under; // TODO Better name...

    #if QT_VERSION >= 0x040600
    QGraphicsBlurEffect *m_blur;
    #endif
};

#endif // Multiple inclusion guard
