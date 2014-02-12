/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * LXDE-Qt - a lightweight, Qt based, desktop toolset
 * http://razor-qt.org
 *
 * Copyright: 2012 Razor team
 * Authors:
 *   Christian Surlykke <christian@surlykke.dk>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QCoreApplication>
#include <lxqt/lxqtautostartentry.h>

#include "batterywatcherd.h"
#include "../config/common.h"

BatteryWatcherd::BatteryWatcherd(QObject *parent) :
    QObject(parent),
    mBatteryInfo(),
    mBattery(),
    mTrayIcon(0),
    mLxQtPower(),
    mLxQtNotification(tr("Power low"), this),
    mActionTime(),
    mSettings("lxqt-autosuspend")
{
    if (!mBattery.haveBattery())
    {
        LxQt::Notification::notify(tr("No battery!"),
                                  tr("LxQt autosuspend could not find data about any battery - actions on power low will not work"),
                                  "lxqt-autosuspend");
    }

    mLxQtNotification.setIcon("lxqt-autosuspend"); // FIXME should be a battery icon
    mLxQtNotification.setUrgencyHint(LxQt::Notification::UrgencyCritical);
    mLxQtNotification.setTimeout(2000);

    connect(&mBattery, SIGNAL(batteryChanged()), this, SLOT(batteryChanged()));
    connect(&mSettings, SIGNAL(settingsChanged()), this, SLOT(settingsChanged()));
    connect(LxQt::Settings::globalSettings(), SIGNAL(iconThemeChanged()), this, SLOT(settingsChanged()));
    settingsChanged();
    batteryChanged();
}

BatteryWatcherd::~BatteryWatcherd()
{
    if (mTrayIcon)
    {
        delete mTrayIcon;
    }
}

void BatteryWatcherd::batteryChanged()
{
    qDebug() <<  "BatteryChanged"
             <<  "discharging:"  << mBattery.discharging() 
             << "chargeLevel:" << mBattery.chargeLevel() 
             << "powerlow:"    << mBattery.powerLow() 
             << "actionTime:"  << mActionTime;

    if (mBattery.powerLow() && mActionTime.isNull() && powerLowAction() > 0)
    {
        int warningTimeMsecs = mSettings.value(POWERLOWWARNING_KEY, 30).toInt()*1000;
        mActionTime = QTime::currentTime().addMSecs(warningTimeMsecs);
        startTimer(100);
        // From here everything is handled by timerEvent below
    }

    mBatteryInfo.updateInfo(&mBattery);

    mTrayIcon->update(mBattery.discharging(), mBattery.chargeLevel(), mSettings.value(POWERLOWLEVEL_KEY, 0.05).toDouble());
}

void BatteryWatcherd::timerEvent(QTimerEvent *event)
{
    if (mActionTime.isNull() || powerLowAction() == 0 || ! mBattery.powerLow())
    {
            killTimer(event->timerId());
            mActionTime = QTime();
    }
    else if (QTime::currentTime().msecsTo(mActionTime) > 0)
    {
        QString notificationMsg;
        switch (powerLowAction())
        {
        case SLEEP:
            notificationMsg = tr("Sleeping in %1 seconds");
            break;
        case HIBERNATE:
            notificationMsg = tr("Hibernating in %1 seconds");
            break;
        case POWEROFF:
            notificationMsg = tr("Shutting down in %1 seconds");
            break;
        }

        mLxQtNotification.setBody(notificationMsg.arg(QTime::currentTime().msecsTo(mActionTime)/1000));
        mLxQtNotification.update();
    }
    else
    {
        doAction(powerLowAction());
        mActionTime = QTime();
        killTimer(event->timerId());
    }
}

void BatteryWatcherd::doAction(int action)
{
    switch (action)
    {
    case SLEEP:
        mLxQtPower.suspend();
        break;
    case HIBERNATE:
        mLxQtPower.hibernate();
        break;
    case POWEROFF:
        mLxQtPower.shutdown();
        break;
    }
}

int BatteryWatcherd::powerLowAction()
{
    return mSettings.value(POWERLOWACTION_KEY).toInt();
}


void BatteryWatcherd::settingsChanged()
{
    bool useThemeIcons = mSettings.value(USETHEMEICONS_KEY, false).toBool();

    if (mTrayIcon != 0 && !mTrayIcon->isProperForCurrentSettings(useThemeIcons))
    {
        mTrayIcon->hide(); 
        mTrayIcon->deleteLater();
        mTrayIcon = 0;
    }
   
    if (mTrayIcon == 0) 
    {
        IconNamingScheme *iconNamingScheme = 0;
        if (useThemeIcons && (iconNamingScheme = IconNamingScheme::getNamingSchemeForCurrentIconTheme()))
        {
            mTrayIcon = new TrayIconTheme(iconNamingScheme, this);
        }
        else 
        {
            mTrayIcon = new TrayIconBuiltIn(this);
        }
    
        bool discharging = mBattery.state() == 2;
        qDebug() << "updating trayicon: " << discharging << mBattery.chargeLevel() << mSettings.value(POWERLOWLEVEL_KEY, 0.05).toDouble();
        
        connect(mTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(showBatteryInfo()));
        mTrayIcon->update(discharging, mBattery.chargeLevel(), mSettings.value(POWERLOWLEVEL_KEY, 0.05).toDouble());
        mTrayIcon->show();
    }
}

void BatteryWatcherd::showBatteryInfo()
{
    if (mBatteryInfo.isVisible())
    {
        mBatteryInfo.close();
    }
    else
    {
        mBatteryInfo.open();
    }
}