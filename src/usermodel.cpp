/*
 * Copyright (C) 2020 Open Mobile Platform LLC.
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

#include "usermodel.h"
#include "logging_p.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QString>
#include <functional>
#include <grp.h>
#include <pwd.h>
#include <sailfishaccesscontrol.h>
#include <sailfishusermanagerinterface.h>
#include <sys/types.h>

namespace {
const auto UserManagerService = QStringLiteral(SAILFISH_USERMANAGER_DBUS_INTERFACE);
const auto UserManagerPath = QStringLiteral(SAILFISH_USERMANAGER_DBUS_OBJECT_PATH);
const auto UserManagerInterface = QStringLiteral(SAILFISH_USERMANAGER_DBUS_INTERFACE);

const QHash<const QString, int> errorTypeMap = {
    { QStringLiteral(SailfishUserManagerErrorBusy), UserModel::Busy },
    { QStringLiteral(SailfishUserManagerErrorHomeCreateFailed), UserModel::HomeCreateFailed },
    { QStringLiteral(SailfishUserManagerErrorHomeRemoveFailed), UserModel::HomeRemoveFailed },
    { QStringLiteral(SailfishUserManagerErrorGroupCreateFailed), UserModel::GroupCreateFailed },
    { QStringLiteral(SailfishUserManagerErrorUserAddFailed), UserModel::UserAddFailed },
    { QStringLiteral(SailfishUserManagerErrorMaxUsersReached), UserModel::MaximumNumberOfUsersReached },
    { QStringLiteral(SailfishUserManagerErrorUserModifyFailed), UserModel::UserModifyFailed },
    { QStringLiteral(SailfishUserManagerErrorUserRemoveFailed), UserModel::UserRemoveFailed },
    { QStringLiteral(SailfishUserManagerErrorGetUidFailed), UserModel::GetUidFailed },
    { QStringLiteral(SailfishUserManagerErrorUserNotFound), UserModel::UserNotFound },
    { QStringLiteral(SailfishUserManagerErrorAddToGroupFailed), UserModel::AddToGroupFailed },
    { QStringLiteral(SailfishUserManagerErrorRemoveFromGroupFailed), UserModel::RemoveFromGroupFailed },
};

int getErrorType(const QDBusError &error)
{
    if (error.type() != QDBusError::Other)
        return error.type();

    return errorTypeMap.value(error.name(), UserModel::OtherError);
}
}

UserModel::UserModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_dBusInterface(nullptr)
    , m_dBusWatcher(new QDBusServiceWatcher(UserManagerService, QDBusConnection::systemBus(),
                    QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration, this))
    , m_guestEnabled(getpwuid((uid_t)SAILFISH_USERMANAGER_GUEST_UID))
{
    connect(this, &UserModel::guestEnabledChanged,
            this, &UserModel::maximumCountChanged);
    qDBusRegisterMetaType<SailfishUserManagerEntry>();
    connect(m_dBusWatcher, &QDBusServiceWatcher::serviceRegistered,
            this, &UserModel::createInterface);
    connect(m_dBusWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &UserModel::destroyInterface);
    if (QDBusConnection::systemBus().interface()->isServiceRegistered(UserManagerService))
        createInterface();
    struct group *grp = getgrnam("users");
    if (!grp) {
        qCWarning(lcUsersLog) << "Could not read users group:" << strerror(errno);
    } else {
        for (int i = 0; grp->gr_mem[i] != nullptr; ++i) {
            UserInfo user(QString(grp->gr_mem[i]));
            if (user.isValid()) { // Skip invalid users here
                m_users.append(user);
                m_uidsToRows.insert(user.uid(), m_users.count()-1);
            }
        }
    }
    // grp must not be free'd
}

UserModel::~UserModel()
{
    delete m_dBusInterface;
}

bool UserModel::placeholder() const
{
    // Placeholder is always last and the only item that can be invalid
    if (m_users.count() == 0)
        return false;
    return !m_users.last().isValid();
}

void UserModel::setPlaceholder(bool value)
{
    if (placeholder() == value)
        return;

    if (value) {
        int row = m_users.count();
        beginInsertRows(QModelIndex(), row, row);
        m_users.append(UserInfo::placeholder());
        endInsertRows();
    } else {
        int row = m_users.count()-1;
        beginRemoveRows(QModelIndex(), row, row);
        m_users.remove(row);
        endRemoveRows();
    }
    emit placeholderChanged();
}

/*
 * Number of existing users
 *
 * If placeholder = false, then this is the same as rowCount.
 */
int UserModel::count() const
{
    return (placeholder()) ? m_users.count()-1 : m_users.count();
}

/*
 * Maximum number of users that can be created
 *
 * If more users are created after count reaches this,
 * MaximumNumberOfUsersReached may be thrown and user creation fails.
 */
int UserModel::maximumCount() const
{
    return m_guestEnabled ? SAILFISH_USERMANAGER_MAX_USERS+1 : SAILFISH_USERMANAGER_MAX_USERS;
}

QHash<int, QByteArray> UserModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { Qt::DisplayRole, "displayName" },
        { UsernameRole, "username" },
        { NameRole, "name" },
        { TypeRole, "type" },
        { UidRole, "uid" },
        { CurrentRole, "current" },
        { PlaceholderRole, "placeholder" },
        { TransitioningRole, "transitioning" },
    };
    return roles;
}

int UserModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_users.count();
}

QVariant UserModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_users.count() || index.column() != 0)
        return QVariant();

    const UserInfo &user = m_users.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return user.displayName();
    case UsernameRole:
        return user.username();
    case NameRole:
        return user.name();
    case TypeRole:
        return user.type();
    case UidRole:
        return user.uid();
    case CurrentRole:
        return user.current();
    case PlaceholderRole:
        return !user.isValid();
    case TransitioningRole:
        return m_transitioning.contains(user.uid());
    default:
        return QVariant();
    }
}

bool UserModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.row() < 0 || index.row() >= m_users.count() || index.column() != 0)
        return false;

    UserInfo &user = m_users[index.row()];

    if (user.type() == UserInfo::Guest)
        return false;

    switch (role) {
    case NameRole: {
        QString name = value.toString();
        if (name.isEmpty() || name == user.name())
            return false;
        user.setName(name);
        if (user.isValid()) {
            createInterface();
            const int uid = user.uid();
            auto response = m_dBusInterface->call(QStringLiteral("modifyUser"), (uint)uid, name);
            response->onError([this, uid](const QDBusError &error) {
                int row = m_uidsToRows.value(uid);
                emit userModifyFailed(row, getErrorType(error));
                qCWarning(lcUsersLog) << "Modifying user with usermanager failed:" << error;
                reset(row);
            });
        }
        emit dataChanged(index, index, QVector<int>() << role);
        return true;
    }
    case Qt::DisplayRole:
    case UsernameRole:
    case TypeRole:
    case UidRole:
    case CurrentRole:
    case PlaceholderRole:
    case TransitioningRole:
    default:
        return false;
    }
}

QModelIndex UserModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (row < 0 || row >= m_users.count() || column != 0)
        return QModelIndex();

    return createIndex(row, 0, row);
}

/*
 * Creates new user from a placeholder user.
 *
 * Does nothing if there is no placeholder or user's name is not set.
 */
void UserModel::createUser()
{
    if (!placeholder())
        return;

    auto user = m_users.last();
    if (user.name().isEmpty())
        return;

    m_transitioning.insert(user.uid());
    auto idx = index(m_users.count()-1, 0);
    emit dataChanged(idx, idx, QVector<int>() << TransitioningRole);

    createInterface();
    auto response = m_dBusInterface->call(QStringLiteral("addUser"), user.name());
    response->onError([this](const QDBusError &error) {
        emit userAddFailed(getErrorType(error));
        qCWarning(lcUsersLog) << "Adding user with usermanager failed:" << error;
    });
    response->onFinished<uint>([this](uint uid) {
        // Check that this was not just added to the list by onUserAdded
        if (!m_uidsToRows.contains(uid)) {
            UserInfo user(uid);
            add(user);
        }
    });
}

void UserModel::removeUser(int row)
{
    if (row < 0 || row >= m_users.count())
        return;

    auto user = m_users.at(row);
    if (!user.isValid())
        return;

    m_transitioning.insert(user.uid());
    auto idx = index(row, 0);
    emit dataChanged(idx, idx, QVector<int>() << TransitioningRole);

    createInterface();
    const int uid = user.uid();
    auto response = m_dBusInterface->call(QStringLiteral("removeUser"), (uint)uid);
    response->onError([this, uid](const QDBusError &error) {
        int row = m_uidsToRows.value(uid);
        emit userRemoveFailed(row, getErrorType(error));
        qCWarning(lcUsersLog) << "Removing user with usermanager failed:" << error;
        m_transitioning.remove(uid);
        auto idx = index(row, 0);
        emit dataChanged(idx, idx, QVector<int>() << TransitioningRole);
    });
}

void UserModel::setCurrentUser(int row)
{
    if (row < 0 || row >= m_users.count())
        return;

    auto user = m_users.at(row);
    if (!user.isValid())
        return;

    createInterface();
    const int uid = user.uid();
    auto response = m_dBusInterface->call(QStringLiteral("setCurrentUser"), (uint)uid);
    response->onError([this, uid](const QDBusError &error) {
        emit setCurrentUserFailed(m_uidsToRows.value(uid), getErrorType(error));
        qCWarning(lcUsersLog) << "Switching user with usermanager failed:" << error;
    });
}

void UserModel::reset(int row)
{
    if (row < 0 || row >= m_users.count())
        return;

    m_users[row].reset();
    auto idx = index(row, 0);
    emit dataChanged(idx, idx, QVector<int>());
}

UserInfo * UserModel::getCurrentUser() const
{
    return new UserInfo();
}

bool UserModel::hasGroup(int row, const QString &group) const
{
    if (row < 0 || row >= m_users.count())
        return false;

    auto user = m_users.at(row);
    if (!user.isValid())
        return false;

    return sailfish_access_control_hasgroup(user.uid(), group.toUtf8().constData());
}

void UserModel::addGroups(int row, const QStringList &groups)
{
    if (row < 0 || row >= m_users.count())
        return;

    auto user = m_users.at(row);
    if (!user.isValid())
        return;

    createInterface();
    const int uid = user.uid();
    auto response = m_dBusInterface->call(QStringLiteral("addToGroups"), (uint)uid, groups);
    response->onError([this, uid](const QDBusError &error) {
        emit addGroupsFailed(m_uidsToRows.value(uid), getErrorType(error));
        qCWarning(lcUsersLog) << "Adding user to groups failed:" << error;
    });
    response->onFinished([this, uid]() {
        emit userGroupsChanged(m_uidsToRows.value(uid));
    });
}

void UserModel::removeGroups(int row, const QStringList &groups)
{
    if (row < 0 || row >= m_users.count())
        return;

    auto user = m_users.at(row);
    if (!user.isValid())
        return;

    createInterface();
    const int uid = user.uid();
    auto response = m_dBusInterface->call(QStringLiteral("removeFromGroups"), (uint)uid, groups);
    response->onError([this, uid](const QDBusError &error) {
        emit removeGroupsFailed(m_uidsToRows.value(uid), getErrorType(error));
        qCWarning(lcUsersLog) << "Removing user from groups failed:" << error;
    });
    response->onFinished([this, uid]() {
        emit userGroupsChanged(m_uidsToRows.value(uid));
    });
}

void UserModel::onUserAdded(const SailfishUserManagerEntry &entry)
{
    if (m_uidsToRows.contains(entry.uid))
        return;

    // Not found already, appending
    auto user = UserInfo(entry.uid);
    if (user.isValid())
        add(user);
}

void UserModel::onUserModified(uint uid, const QString &newName)
{
    if (!m_uidsToRows.contains(uid))
        return;

    int row = m_uidsToRows.value(uid);
    UserInfo &user = m_users[row];
    if (user.name() != newName) {
        user.setName(newName);
        auto idx = index(row, 0);
        dataChanged(idx, idx, QVector<int>() << NameRole);
    }
}

void UserModel::onUserRemoved(uint uid)
{
    if (!m_uidsToRows.contains(uid))
        return;

    int row = m_uidsToRows.value(uid);
    beginRemoveRows(QModelIndex(), row, row);
    m_transitioning.remove(uid);
    m_users.remove(row);
    // It is slightly costly to remove users since some row numbers may need to be updated
    m_uidsToRows.remove(uid);
    for (auto iter = m_uidsToRows.begin(); iter != m_uidsToRows.end(); ++iter) {
        if (iter.value() > row)
            iter.value() -= 1;
    }
    endRemoveRows();
    emit countChanged();
}

void UserModel::onCurrentUserChanged(uint uid)
{
    UserInfo *previous = getCurrentUser();
    if (previous) {
        if (previous->updateCurrent()) {
            auto idx = index(m_uidsToRows.value(previous->uid()), 0);
            emit dataChanged(idx, idx, QVector<int>() << CurrentRole);
        }
        delete previous;
    }
    if (m_uidsToRows.contains(uid) && m_users[m_uidsToRows.value(uid)].updateCurrent()) {
        auto idx = index(m_uidsToRows.value(uid), 0);
        emit dataChanged(idx, idx, QVector<int>() << CurrentRole);
    }
}

void UserModel::onCurrentUserChangeFailed(uint uid)
{
    if (m_uidsToRows.contains(uid)) {
        emit setCurrentUserFailed(m_uidsToRows.value(uid), Failure);
    }
}

void UserModel::onGuestUserEnabled(bool enabled)
{
    if (enabled != m_guestEnabled) {
        m_guestEnabled = enabled;
        emit guestEnabledChanged();
    }
}

bool UserModel::guestEnabled() const
{
    return m_guestEnabled;
}

void UserModel::setGuestEnabled(bool enabled)
{
    if (enabled == m_guestEnabled)
        return;

    if (m_guestEnabled) {
        m_transitioning.insert(SAILFISH_USERMANAGER_GUEST_UID);
        auto idx = index(m_uidsToRows.value(SAILFISH_USERMANAGER_GUEST_UID), 0);
        emit dataChanged(idx, idx, QVector<int>() << TransitioningRole);
    }

    createInterface();
    auto response = m_dBusInterface->call(QStringLiteral("enableGuestUser"), enabled);
    response->onError([this, enabled](const QDBusError &error) {
        emit setGuestEnabledFailed(enabled, getErrorType(error));
        qCWarning(lcUsersLog) << ((enabled) ? "Enabling" : "Disabling") << "guest user failed:" << error;
        if (!enabled) {
            m_transitioning.remove(SAILFISH_USERMANAGER_GUEST_UID);
            auto idx = index(m_uidsToRows.value(SAILFISH_USERMANAGER_GUEST_UID), 0);
            emit dataChanged(idx, idx, QVector<int>() << TransitioningRole);
        }
    });
}

void UserModel::createInterface()
{
    if (!m_dBusInterface) {
        qCDebug(lcUsersLog) << "Creating interface to user-managerd";
        m_dBusInterface = new NemoDBus::Interface(
                this, QDBusConnection::systemBus(),
                UserManagerService, UserManagerPath, UserManagerInterface);
        m_dBusInterface->connectToSignal(
                QStringLiteral("userAdded"),
                SLOT(onUserAdded(const SailfishUserManagerEntry &)));
        m_dBusInterface->connectToSignal(
                QStringLiteral("userModified"),
                SLOT(onUserModified(uint, const QString &)));
        m_dBusInterface->connectToSignal(
                QStringLiteral("userRemoved"),
                SLOT(onUserRemoved(uint)));
        m_dBusInterface->connectToSignal(
                QStringLiteral("currentUserChanged"),
                SLOT(onCurrentUserChanged(uint)));
        m_dBusInterface->connectToSignal(
                QStringLiteral("currentUserChangeFailed"),
                SLOT(onCurrentUserChangeFailed(uint)));
        m_dBusInterface->connectToSignal(
                QStringLiteral("guestUserEnabled"),
                SLOT(onGuestUserEnabled(bool)));
    }
}

void UserModel::destroyInterface()
{
    if (m_dBusInterface) {
        qCDebug(lcUsersLog) << "Destroying interface to user-managerd";
        m_dBusInterface->connection().disconnect(
                UserManagerService, UserManagerPath, UserManagerInterface,
                QStringLiteral("userAdded"),
                this, SLOT(onUserAdded(const SailfishUserManagerEntry &)));
        m_dBusInterface->connection().disconnect(
                UserManagerService, UserManagerPath, UserManagerInterface,
                QStringLiteral("userModified"),
                this, SLOT(onUserModified(uint, const QString &)));
        m_dBusInterface->connection().disconnect(
                UserManagerService, UserManagerPath, UserManagerInterface,
                QStringLiteral("userRemoved"),
                this, SLOT(onUserRemoved(uint)));
        m_dBusInterface->connection().disconnect(
                UserManagerService, UserManagerPath, UserManagerInterface,
                QStringLiteral("currentUserChanged"),
                this, SLOT(onCurrentUserChanged(uint)));
        m_dBusInterface->connection().disconnect(
                UserManagerService, UserManagerPath, UserManagerInterface,
                QStringLiteral("currentUserChangeFailed"),
                this, SLOT(onCurrentUserChangeFailed(uint)));
        m_dBusInterface->connection().disconnect(
                UserManagerService, UserManagerPath, UserManagerInterface,
                QStringLiteral("guestUserEnabled"),
                this, SLOT(onGuestUserEnabled(bool)));
        delete m_dBusInterface;
        m_dBusInterface = nullptr;
    }
}

void UserModel::add(UserInfo &user)
{
    if (placeholder() && m_transitioning.contains(m_users.last().uid())
            && m_users.last().name() == user.name()) {
        // This is the placeholder we were adding, "change" that
        int row = m_users.count()-1;
        m_users.insert(row, user);
        m_uidsToRows.insert(user.uid(), row);
        auto idx = index(row, 0);
        emit dataChanged(idx, idx, QVector<int>());
        // And then "add" the placeholder back to its position
        beginInsertRows(QModelIndex(), row+1, row+1);
        m_users[row+1].reset();
        m_transitioning.remove(m_users[row+1].uid());
        endInsertRows();
    } else {
        int row = placeholder() ? m_users.count() - 1 : m_users.count();
        beginInsertRows(QModelIndex(), row, row);
        m_users.insert(row, user);
        m_uidsToRows.insert(user.uid(), row);
        m_transitioning.remove(user.uid());
        endInsertRows();
    }
    emit countChanged();
}
