/*
 * Copyright (C) 2017 Jolla Ltd.
 * Contact: Raine Makelainen <raine.makelainen@jolla.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#ifndef BATTERYSTATUS_P_H
#define BATTERYSTATUS_P_H

#include "batterystatus.h"

class QDBusPendingCallWatcher;

class BatteryStatusPrivate : public QObject
{
    Q_OBJECT

public:
    BatteryStatusPrivate(BatteryStatus *batteryInfo);
    ~BatteryStatusPrivate();

    void registerSignals();

    BatteryStatus::ChargerStatus parseChargerStatus(const QString &state);
    BatteryStatus::Status parseBatteryStatus(const QString &status);

    BatteryStatus *q;
    BatteryStatus::Status status;
    BatteryStatus::ChargerStatus chargerStatus;
    int chargePercentage;

public slots:
    void mceRegistered();
    void mceUnregistered();

    void chargerStatusChanged(const QString &status);
    void statusChanged(const QString &s);
    void chargePercentageChanged(int percentage);

    void initialChargerState(QDBusPendingCallWatcher *watcher);
    void initialBatteryStatus(QDBusPendingCallWatcher *watcher);
    void initialChargePercentage(QDBusPendingCallWatcher *watcher);
};

#endif // BATTERYSTATUS_P_H
