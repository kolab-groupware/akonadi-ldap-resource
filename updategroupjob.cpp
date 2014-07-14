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

#include "updategroupjob.h"

#include "incrementalupdatedata.h"
#include "ldapmapper.h"

#include <kldap/ldapdefs.h>

#include <akonadi/collectionmodifyjob.h>
#include <akonadi/itemcreatejob.h>
#include <akonadi/itemdeletejob.h>
#include <akonadi/itemfetchjob.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/transactionsequence.h>

UpdateGroupJob::UpdateGroupJob(const QString &searchBase, KLDAP::LdapConnection &connection, const Akonadi::Collection &collection, QObject *parent)
:   KJob(parent),
    mTransaction(0),
    mSearchbase(searchBase),
    mConnection(connection),
    mLdapSearch(connection),
    mCollection(collection),
    mPhase(ListMembers)
{
    Q_ASSERT(connection.handle());
    connect(&mLdapSearch, SIGNAL(result(KLDAP::LdapSearch*)),
            this, SLOT(gotSearchResult(KLDAP::LdapSearch*)));
    connect(&mLdapSearch, SIGNAL(data(KLDAP::LdapSearch*,KLDAP::LdapObject)),
            this, SLOT(gotSearchData(KLDAP::LdapSearch*,KLDAP::LdapObject)));

    // autostart like an Akonadi::Job
    QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
}

UpdateGroupJob::UpdateGroupJob(const GroupUpdate &updateData, const QString &searchBase, KLDAP::LdapConnection &connection, const Akonadi::Collection &collection, QObject *parent)
:   KJob(parent),
    mTransaction(0),
    mName(updateData.name),
    mTimestamp(updateData.timestamp),
    mSearchbase(searchBase),
    mConnection(connection),
    mLdapSearch(connection),
    mCollection(collection),
    mPhase(ListMembers)
{
    Q_ASSERT(connection.handle());
    connect(&mLdapSearch, SIGNAL(result(KLDAP::LdapSearch*)),
            this, SLOT(gotSearchResult(KLDAP::LdapSearch*)));
    connect(&mLdapSearch, SIGNAL(data(KLDAP::LdapSearch*,KLDAP::LdapObject)),
            this, SLOT(gotSearchData(KLDAP::LdapSearch*,KLDAP::LdapObject)));

    // autostart like an Akonadi::Job
    QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
}

void UpdateGroupJob::start()
{
    if (!mName.isEmpty() || !mTimestamp.isEmpty()) {
        if (!mName.isEmpty()) {
            mCollection.setName(mName);
        }
        if (!mTimestamp.isEmpty()) {
            mCollection.setRemoteRevision(mTimestamp);
        }

        Akonadi::CollectionModifyJob *modifyJob = new Akonadi::CollectionModifyJob(mCollection, this);
        connect(modifyJob, SIGNAL(result(KJob*)), this, SLOT(collectionModifyDone(KJob*)));
    } else {
        fetchLocalItems();
    }
}

void UpdateGroupJob::gotSearchResult(KLDAP::LdapSearch *search)
{
    if (search->error()) {
        kWarning() << search->error() << search->errorString();
        switch (search->error()) {
            case KLDAP_SIZELIMIT_EXCEEDED:
                kWarning() << "Sizelimit exceeded";
                break;
            case KLDAP_ADMINLIMIT_EXCEEDED:
                kWarning() << "Administrative limit exceeded";
                break;
            default:
                kWarning() << "Unknown error";
        }
        setError(KJob::UserDefinedError);
        emitResult();
        return;
    }

    switch (mPhase) {
        case ListMembers:
            processMembers();
            break;

        case FetchMembers:
            if (!mTransaction) { // no jobs created here -> done
                emitResult();
            } else {
                mTransaction->commit();
            }
            break;
    }
}

void UpdateGroupJob::gotSearchData(KLDAP::LdapSearch *search, const KLDAP::LdapObject &obj)
{
    Q_UNUSED(search);

    switch (mPhase) {
        case ListMembers: {
            // ignore all objects that are available as local items.
            // if they have an update, they will be updated by IncrementalUpdateJob
            // later on using UpdateItemJob on all collections

            foreach (const QByteArray &val, obj.values("uniqueMember")) {
                mNewMembers << val;
            }
            break;
        }

        case FetchMembers: {
            Akonadi::Item item;
            item.setRemoteId(LDAPMapper::getStableIdentifier(obj));
            item.setPayload(LDAPMapper::getAddressee(obj));
            item.setMimeType(KABC::Addressee::mimeType());
            item.setParentCollection(mCollection);
            item.setRemoteRevision(LDAPMapper::getTimestamp(obj));

            new Akonadi::ItemCreateJob(item, mCollection, transaction());
            break;
        }
    }

}

void UpdateGroupJob::collectionModifyDone(KJob *job)
{
    if (job->error()) {
        kWarning() << job->errorString();

        // just failed the rename, lets still try to update the member list
    }

    fetchLocalItems();
}

void UpdateGroupJob::retrieveMembersDone(KJob *job)
{
    if (job->error()) {
        setError(KJob::UserDefinedError);
    }

    emitResult();
}

void UpdateGroupJob::localFetchDone(KJob *job)
{
    Akonadi::ItemFetchJob *fetchJob = static_cast<Akonadi::ItemFetchJob*>(job);
    foreach (const Akonadi::Item &item, fetchJob->items()) {
        kDebug() << item.remoteId() << item.remoteRevision();
        mLocalItems.insert(item.remoteId(), item);
    }
    searchForAllMembers();
}

void UpdateGroupJob::transactionDone(KJob *job)
{
    if ( job->error() ) {
        return; // handled by base class
    }
    emitResult();
}

Akonadi::TransactionSequence *UpdateGroupJob::transaction()
{
    if ( !mTransaction ) {
        mTransaction= new Akonadi::TransactionSequence( this );
        mTransaction->setAutomaticCommittingEnabled( false );
        connect(mTransaction, SIGNAL(result(KJob*)), SLOT(transactionDone(KJob*)) );
    }
    return mTransaction;
}

void UpdateGroupJob::fetchLocalItems()
{
    Akonadi::ItemFetchJob *job = new Akonadi::ItemFetchJob(mCollection, this);
    job->fetchScope().setFetchModificationTime(false);
    job->fetchScope().setCacheOnly(true);
    job->fetchScope().fetchFullPayload(false);
    connect(job, SIGNAL(result(KJob*)), this, SLOT(localFetchDone(KJob*)));
}

void UpdateGroupJob::searchForAllMembers()
{
    Q_ASSERT(mPhase == ListMembers);

    const int ret = mLdapSearch.search(KLDAP::LdapDN(mSearchbase), KLDAP::LdapUrl::Sub,
                                       QString("%1=%2").arg(LDAPMapper::getAttribute(LDAPMapper::UniqueIdentifier)).arg(mCollection.remoteId()),
                                       QStringList() << "uniqueMember");
    if (!ret) {
        kWarning() << mLdapSearch.errorString();
        kWarning() << "retrieval failed";
        setError(KJob::UserDefinedError);
        emitResult();
    }
}

void UpdateGroupJob::searchForMember(const QString &memberDn)
{
    const int ret = mLdapSearch.search(KLDAP::LdapDN(memberDn), KLDAP::LdapUrl::Base, QString(),
                                       LDAPMapper::requestedFullPayloadAttributes());
    if (!ret) {
        kWarning() << mLdapSearch.errorString();
        kWarning() << "retrieval failed";
        setError(KJob::UserDefinedError);
        emitResult();
    }
}

void UpdateGroupJob::processMembers()
{
    // all remaining local items are no longer members
    const Akonadi::Item::List toRemove = mLocalItems.values();
    if (!toRemove.isEmpty()) {
        Akonadi::ItemDeleteJob *job = new Akonadi::ItemDeleteJob(toRemove, transaction());
        transaction()->setIgnoreJobFailure(job);
    }

    mPhase = FetchMembers;
    processNewMember();
}

void UpdateGroupJob::processNewMember()
{
    if (mNewMembers.isEmpty()) {
        if (!mTransaction) { // no jobs created here -> done
            emitResult();
        } else {
            mTransaction->commit();
        }
        return;
    }

    const QString memberDn = mNewMembers.takeFirst();
    searchForMember(memberDn);
}
