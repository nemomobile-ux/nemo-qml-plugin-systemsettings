/*
 * Copyright (C) 2017 Jolla Ltd. <simo.piiroinen@jollamobile.com>
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

#include "deviceinfo.h"

#include <QSet>
#include <QFile>
#include <QDebug>
#include <QSettings>

#ifdef HAS_SSUSYSINFO
#include <ssusysinfo.h>
#endif

class DeviceInfoPrivate
{
public:
    DeviceInfoPrivate();
    ~DeviceInfoPrivate();

    QSet<DeviceInfo::Feature> m_features;
    QSet<Qt::Key> m_keys;
    QString m_model;
    QString m_baseModel;
    QString m_designation;
    QString m_manufacturer;
    QString m_prettyName;
};

DeviceInfoPrivate::DeviceInfoPrivate()
{
#ifdef HAS_SSUSYSINFO
    ssusysinfo_t *si = ssusysinfo_create();

    hw_feature_t *features = ssusysinfo_get_hw_features(si);
    if (features) {
        for (size_t i = 0; features[i]; ++i) {
            m_features.insert(static_cast<DeviceInfo::Feature>(features[i]));
        }
        free(features);
    }

    hw_key_t *keys = ssusysinfo_get_hw_keys(si);
    if (keys) {
        for (size_t i = 0; keys[i]; ++i) {
            m_keys.insert(static_cast<Qt::Key>(keys[i]));
        }
        free(keys);
    }

    /* Note: These queries always return non-null C string */
    m_model = ssusysinfo_device_model(si);
    m_baseModel = ssusysinfo_device_base_model(si);
    m_designation = ssusysinfo_device_designation(si);
    m_manufacturer = ssusysinfo_device_manufacturer(si);
    m_prettyName = ssusysinfo_device_pretty_name(si);

    ssusysinfo_delete(si);
#else
    if(QFile::exists("/etc/hw-release")) {
        QSettings* devSettings = new QSettings("/etc/device.ini", QSettings::IniFormat);
        m_model = devSettings->value("MODEL", "Unknown model").toString();
        m_baseModel = devSettings->value("BASE_MODEL", "Unknown base model").toString();
        m_designation = devSettings->value("DESIGNATION", "Unknown designation").toString();
        m_manufacturer = devSettings->value("MANUFACTURER", "Unknown anufacturer").toString();
        m_prettyName = devSettings->value("PRETTY_NAME", "Unknown pretty name").toString();
    } else {
        qWarning() << "SSU and /etc/hw-release file not found";
        m_model = "Unknown";
        m_baseModel = "Unknown";
        m_designation = "Unknown";
        m_manufacturer = "Unknown";
        m_prettyName = "Unknown";
    }
#endif
}

DeviceInfoPrivate::~DeviceInfoPrivate()
{
}

DeviceInfo::DeviceInfo(QObject *parent)
    : QObject(parent)
    , d_ptr(new DeviceInfoPrivate())
{
}

DeviceInfo::~DeviceInfo()
{
    delete d_ptr;
    d_ptr = 0;
}

bool DeviceInfo::hasFeature(DeviceInfo::Feature feature) const
{
    Q_D(const DeviceInfo);
    return d->m_features.contains(feature);
}

bool DeviceInfo::hasHardwareKey(Qt::Key key) const
{
    Q_D(const DeviceInfo);
    return d->m_keys.contains(key);
}

QString DeviceInfo::model() const
{
    Q_D(const DeviceInfo);
    return d->m_model;
}

QString DeviceInfo::baseModel() const
{
    Q_D(const DeviceInfo);
    return d->m_baseModel;
}

QString DeviceInfo::designation() const
{
    Q_D(const DeviceInfo);
    return d->m_designation;
}

QString DeviceInfo::manufacturer() const
{
    Q_D(const DeviceInfo);
    return d->m_manufacturer;
}

QString DeviceInfo::prettyName() const
{
    Q_D(const DeviceInfo);
    return d->m_prettyName;
}
