/*
 * Copyright (C) 2013 Jolla Ltd. <pekka.vuorela@jollamobile.com>
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

#include "aboutsettings.h"
#include "aboutsettings_p.h"

#include <QDebug>
#include <QStringList>

#include <QFile>
#include <QByteArray>
#include <QRegularExpression>
#include <QMap>
#include <QTextStream>
#include <QVariant>
#include <QSettings>
#include <QTimer>
#include <QLocale>

namespace
{

void parseReleaseFile(const QString &filename, QMap<QString, QString> *result)
{
    if (!result->isEmpty()) {
        return;
    }

    // Specification of the format:
    // http://www.freedesktop.org/software/systemd/man/os-release.html

    QFile release(filename);
    if (release.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&release);

        // "All strings should be in UTF-8 format, and non-printable characters
        // should not be used."
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        in.setCodec("UTF-8");
#else
        in.setEncoding(QStringConverter::Utf8);
#endif

        while (!in.atEnd()) {
            QString line = in.readLine();

            // "Lines beginning with "#" shall be ignored as comments."
            if (line.startsWith('#')) {
                continue;
            }

            QString key = line.section('=', 0, 0);
            QString value = line.section('=', 1);

            // Remove trailing whitespace in value
            value = value.trimmed();

            // POSIX.1-2001 says uppercase, digits and underscores.
            //
            // Bash uses "[a-zA-Z_]+[a-zA-Z0-9_]*", so we'll use that too,
            // as we can safely assume that "shell-compatible variable
            // assignments" means it should be compatible with bash.
            //
            // see http://stackoverflow.com/a/2821183
            // and http://stackoverflow.com/a/2821201
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            if (!QRegExp("[a-zA-Z_]+[a-zA-Z0-9_]*").exactMatch(key)) {
#else
            if (!QRegularExpression("[a-zA-Z_]+[a-zA-Z0-9_]*").match(key).hasMatch()) {
#endif
                qWarning("Invalid key in input line: '%s'", qPrintable(line));
                continue;
            }

            // "Variable assignment values should be enclosed in double or
            // single quotes if they include spaces, semicolons or other
            // special characters outside of A-Z, a-z, 0-9."
            if (((value.at(0) == '\'') || (value.at(0) == '"'))) {
                if (value.at(0) != value.at(value.size() - 1)) {
                    qWarning("Quoting error in input line: '%s'", qPrintable(line));
                    continue;
                }

                // Remove the quotes
                value = value.mid(1, value.size() - 2);
            }

            // "If double or single quotes or backslashes are to be used within
            // variable assignments, they should be escaped with backslashes,
            // following shell style."
            value = value.replace(QRegularExpression("\\\\(.)"), "\\1");

            (*result)[key] = value;
        }

        release.close();
    }
}

void parseLocalizationFile(const QString &filename, QMap<QString, QString> *result)
{
    if (!result->isEmpty()) {
        return;
    }

    if (!QFile(filename).exists()) {
        return;
    }

    QSettings localizations(filename, QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    localizations.setIniCodec("UTF-8");
#endif

    QStringList languages = QLocale::system().uiLanguages();
    QStringList availableLanguages;

    for (auto it = languages.crbegin(); it != languages.crend(); ++it) {
        const auto &lang = *it;
        if (localizations.childGroups().contains(lang)) {
            availableLanguages.append(lang);
        }
    }

    // Gradually load localizations, overridding least preferred with most preferred ones
    for (const auto &lang : availableLanguages) {
        localizations.beginGroup(lang);
        for (const auto &key : localizations.childKeys()) {
            result->insert(key, localizations.value(key).toString());
        }
        localizations.endGroup();
    }
}

}


AboutSettingsPrivate::AboutSettingsPrivate(QObject *parent)
    : QObject(parent)
    , deviceInfo(new DeviceInfo())
{
}

AboutSettingsPrivate::~AboutSettingsPrivate()
{
    delete deviceInfo;
    deviceInfo = nullptr;
}


AboutSettings::AboutSettings(QObject *parent)
    : QObject(parent)
    , d_ptr(new AboutSettingsPrivate(this))
{
    Q_D(AboutSettings);
    QSettings settings(QStringLiteral("/etc/os-release"), QSettings::IniFormat);
    d->vendorName = settings.value(QStringLiteral("NAME")).toString();
    d->vendorVersion = settings.value(QStringLiteral("BUILD_ID")).toString();
}

AboutSettings::~AboutSettings()
{
}

QString AboutSettings::wlanMacAddress() const
{
    Q_D(const AboutSettings);
    return d->deviceInfo->wlanMacAddress();
}

QString AboutSettings::serial() const
{
    QStringList serialFiles;

    serialFiles
        // Old location for serial number that was used by e.g. 
        // Jolla Tablet, that should not be used anymore.
        << "/config/serial/serial.txt"
        // Location for serialnumber file that should be preferred if no /sys 
        // node or something for it. The means how the serialnumber ends to 
        // this file are device specific.
        << "/run/config/serial"
        // usb-moded sets up the serial number here.
        << "/sys/class/android_usb/android0/iSerial"
        // Some devices have serialno in this path.
        << "/sys/firmware/devicetree/base/firmware/android/serialno";

    for (const QString &serialFile : serialFiles) {
        QFile serialTxt(serialFile);
        if (serialTxt.exists() && serialTxt.open(QIODevice::ReadOnly))
            return QString::fromUtf8(serialTxt.readAll()).trimmed();
    }

    return QString();
}

QString AboutSettings::localizedOperatingSystemName() const
{
    Q_D(const AboutSettings);
    parseLocalizationFile(QStringLiteral("/etc/os-release-l10n"), &d->osReleaseLocalization);

    return d->osReleaseLocalization.value("NAME", operatingSystemName());
}

QString AboutSettings::baseOperatingSystemName() const
{
    QString osName = operatingSystemName();
    if (osName.endsWith(QStringLiteral(" OS"))) {
        osName.chop(3);
    }
    return osName;
}

QString AboutSettings::operatingSystemName() const
{
    Q_D(const AboutSettings);
    parseReleaseFile(QStringLiteral("/etc/os-release"), &d->osRelease);

    return d->osRelease["NAME"];
}

QString AboutSettings::localizedSoftwareVersion() const
{
    Q_D(const AboutSettings);
    parseLocalizationFile(QStringLiteral("/etc/os-release-l10n"), &d->osReleaseLocalization);

    return d->osReleaseLocalization.value("VERSION", softwareVersion());
}

QString AboutSettings::softwareVersion() const
{
    Q_D(const AboutSettings);
    parseReleaseFile(QStringLiteral("/etc/os-release"), &d->osRelease);

    return d->osRelease["VERSION"];
}

QString AboutSettings::softwareVersionId() const
{
    Q_D(const AboutSettings);
    parseReleaseFile(QStringLiteral("/etc/os-release"), &d->osRelease);

    return d->osRelease["VERSION_ID"];
}

QString AboutSettings::adaptationVersion() const
{
    Q_D(const AboutSettings);
    parseReleaseFile(QStringLiteral("/etc/hw-release"), &d->hardwareRelease);

    return d->hardwareRelease["VERSION_ID"];
}

QString AboutSettings::vendorName() const
{
    Q_D(const AboutSettings);
    return d->vendorName;
}

QString AboutSettings::vendorVersion() const
{
    Q_D(const AboutSettings);
    return d->vendorVersion;
}
