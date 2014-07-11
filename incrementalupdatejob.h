/*
 * Copyright (C) 2014 Klaralvdalens Datakonsult AB <info@kdab.com>
 *     Author: Kevin Krammer <kevin.krammer@kdab.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef INCREMENATLUPDATEJOB_H
#define INCREMENATLUPDATEJOB_H

#include "incrementalupdatedata.h"

#include <akonadi/collection.h>

#include <kjob.h>

#include <QStringList>

namespace KLDAP {
    class LdapConnection;
}

class IncrementalUpdateJob : public KJob
{
    Q_OBJECT
public:
    IncrementalUpdateJob(const QString &resourceId, const QString &searchBase, KLDAP::LdapConnection &connection, QObject *parent = 0);

public Q_SLOTS:
    virtual void start();

private Q_SLOTS:
    void localFetchDone(KJob *job);
    void retrieveUpdatesDone(KJob *job);
    void updateGroupDone(KJob *job);
    void updateItemDone(KJob* job);
    void createGroupDone(KJob *job);
    void updateTimestampDone(KJob *job);

private:
    void processNextGroup();
    void processNextItem();
    void updateTimestamp();
    void done();

    const QString mResourceId;

    const QString mSearchbase;
    KLDAP::LdapConnection &mConnection;

    Akonadi::Collection::List mCollections;
    QString mInitialTimestamp;

    QStringList mUpdatedItems;
    GroupUpdateList mUpdatedGroups;
    QString mNextTimestamp;

    QElapsedTimer mProcessingTime;
};

#endif // INCREMENATLUPDATEJOB_H
