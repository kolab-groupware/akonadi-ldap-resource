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

#include "incrementalupdatejob.h"

#include "retrieveupdatesjob.h"
#include "updateitemjob.h"
#include "updategroupjob.h"

#include <KABC/Addressee>
#include <KABC/ContactGroup>

#include <akonadi/collectioncreatejob.h>
#include <akonadi/collectionfetchjob.h>
#include <akonadi/collectionfetchscope.h>
#include <akonadi/collectionmodifyjob.h>

IncrementalUpdateJob::IncrementalUpdateJob(const QString &resourceId, const QString &searchBase, KLDAP::LdapConnection &connection, QObject *parent)
:   KJob(parent),
    mResourceId(resourceId),
    mSearchbase(searchBase),
    mConnection(connection)
{
    // autostart like an Akonadi::Job
    QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
}

void IncrementalUpdateJob::start()
{
    kDebug() << "Starting incremental update";
    mProcessingTime.start();

    // start by fetching all our collections
    Akonadi::CollectionFetchJob *fetchJob =
        new Akonadi::CollectionFetchJob(Akonadi::Collection::root(), Akonadi::CollectionFetchJob::Recursive, this);
    fetchJob->fetchScope().setResource(mResourceId);
    fetchJob->fetchScope().setAncestorRetrieval(Akonadi::CollectionFetchScope::All);
    connect(fetchJob, SIGNAL(result(KJob*)), this, SLOT(localFetchDone(KJob*)));
}

void IncrementalUpdateJob::localFetchDone(KJob *job)
{
    if (job->error()) {
        kWarning() << job->errorString();
        setError(KJob::UserDefinedError);
        emitResult();
        return;
    }

    Akonadi::CollectionFetchJob *fetchJob = static_cast<Akonadi::CollectionFetchJob*>(job);
    mCollections = fetchJob->collections();
    Q_ASSERT(mCollections.count() > 0);

    // determine timestamp of last update
    // should be the remote revision of the top level collection
    foreach (const Akonadi::Collection &collection, mCollections) {
        if (collection.parentCollection() == Akonadi::Collection::root()) {
            mInitialTimestamp = collection.remoteRevision();
            break;
        }
    }

    if (mInitialTimestamp.isEmpty()) {
        kWarning() << "No timestamp for incremental update available";
        setError(KJob::UserDefinedError);
        emitResult();
        return;
    }

    kDebug() << "Checking for updates since" << mInitialTimestamp;

    RetrieveUpdatesJob *updateJob = new RetrieveUpdatesJob(mInitialTimestamp, mSearchbase, mConnection, this);
    connect(updateJob, SIGNAL(result(KJob*)), this, SLOT(retrieveUpdatesDone(KJob*)));
}

void IncrementalUpdateJob::retrieveUpdatesDone(KJob *job)
{
    if (job->error()) {
        kWarning() << job->errorString();
        setError(KJob::UserDefinedError);
        emitResult();
        return;
    }

    RetrieveUpdatesJob *updateJob = static_cast<RetrieveUpdatesJob*>(job);
    mUpdatedItems = updateJob->items();
    mUpdatedGroups = updateJob->groups();
    mNextTimestamp = updateJob->nextTimestamp();

    processNextGroup();
}

void IncrementalUpdateJob::updateGroupDone(KJob *job)
{
    if (job->error()) {
        kWarning() << job->errorString();

        // try to proceed as far as possible
    }

    processNextGroup();
}

void IncrementalUpdateJob::updateItemDone(KJob *job)
{
    if (job->error()) {
        kWarning() << job->errorString();

        // try to proceed as far as possible
    }

    processNextItem();
}

void IncrementalUpdateJob::createGroupDone(KJob *job)
{
    if (job->error()) {
        kWarning() << job->errorString();
        processNextGroup();
        return;
    }

    Akonadi::CollectionCreateJob *createJob = static_cast<Akonadi::CollectionCreateJob*>(job);

    const Akonadi::Collection collection = createJob->collection();

    UpdateGroupJob *updateJob = new UpdateGroupJob(mSearchbase, mConnection, collection, this);
    connect(updateJob, SIGNAL(result(KJob*)), this, SLOT(updateGroupDone(KJob*)));
}

void IncrementalUpdateJob::updateTimestampDone(KJob *job)
{
    if (job->error()) {
        kWarning() << job->errorString();
        setError(KJob::UserDefinedError);
    }

    done();
}

void IncrementalUpdateJob::processNextGroup()
{
    if (mUpdatedGroups.isEmpty()) {
        processNextItem();
        return;
    }

    const GroupUpdate groupUpdate = mUpdatedGroups.takeFirst();
    const QString groupId = groupUpdate.id;
    const QString groupName = groupUpdate.name;

    // check if this is a new collection or an update to an existing one
    foreach (const Akonadi::Collection &collection, mCollections) {
        if (collection.remoteId() == groupId) {
            if (collection.remoteRevision().isEmpty() || collection.remoteRevision() != groupUpdate.timestamp) {
                UpdateGroupJob *updateJob = new UpdateGroupJob(groupUpdate, mSearchbase, mConnection, collection, this);
                connect(updateJob, SIGNAL(result(KJob*)), this, SLOT(updateGroupDone(KJob*)));
            } else {
                // already up to date
                processNextGroup();
            }
            return;
        }
    }

    // find the top level collection
    foreach (const Akonadi::Collection &collection, mCollections) {
        if (collection.parentCollection() == Akonadi::Collection::root()) {
            Akonadi::Collection groupCollection;
            groupCollection.setName(groupName);
            groupCollection.setRemoteId(groupId);
            groupCollection.setContentMimeTypes(QStringList() << KABC::Addressee::mimeType() << Akonadi::Collection::mimeType() << KABC::ContactGroup::mimeType());
            groupCollection.setParentCollection(collection);
            groupCollection.setRemoteRevision(groupUpdate.timestamp);

            Akonadi::CollectionCreateJob *createJob = new Akonadi::CollectionCreateJob(groupCollection, this);
            connect(createJob, SIGNAL(result(KJob*)), this, SLOT(createGroupDone(KJob*)));
            return;
        }
    }
}

void IncrementalUpdateJob::processNextItem()
{
    if (mUpdatedItems.isEmpty()) {
        updateTimestamp();
        return;
    }

    const QString itemId = mUpdatedItems.takeFirst();

    UpdateItemJob *updateJob = new UpdateItemJob(itemId, mSearchbase, mConnection, mCollections, this);
    connect(updateJob, SIGNAL(result(KJob*)), this, SLOT(updateItemDone(KJob*)));
}

void IncrementalUpdateJob::updateTimestamp()
{
    if (mNextTimestamp.isEmpty() || mInitialTimestamp == mNextTimestamp) {
        done();
        return;
    }

    // find the top level collection
    foreach (const Akonadi::Collection &collection, mCollections) {
        if (collection.parentCollection() == Akonadi::Collection::root()) {
            Akonadi::Collection col = collection;
            col.setRemoteRevision(mNextTimestamp);

            Akonadi::CollectionModifyJob *modifyJob = new Akonadi::CollectionModifyJob(col, this);
            connect(modifyJob, SIGNAL(result(KJob*)), this, SLOT(updateTimestampDone(KJob*)));
            return;
        }
    }

    Q_ASSERT("List of collections must include the top level collection" == 0);
}

void IncrementalUpdateJob::done()
{
    kDebug() << "Total Time elapsed:" << mProcessingTime.elapsed() << "ms";

    emitResult();
}
