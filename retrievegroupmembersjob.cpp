/*
 * Copyright (C) 2013  Christian Mollekopf <mollekopf@kolabsys.com>
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

#include "retrievegroupmembersjob.h"
#include "ldapmapper.h"
#include "settings.h"

#include <KABC/Addressee>
#include <KABC/ContactGroup>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>
#include <Akonadi/ItemCreateJob>
#include <Akonadi/ItemModifyJob>
#include <Akonadi/ItemDeleteJob>
#include <kldap/ldapdefs.h>
#include <quuid.h>

RetrieveGroupMembersJob::RetrieveGroupMembersJob(const QString &searchbase, const Akonadi::Collection& col, KLDAP::LdapConnection& connection, QObject* parent)
:   Job(parent),
    mFetchScope(LookupPayload),
    mLdapSearch(connection),
    mParentCollection(col),
    mTransaction(0),
    mSearchbase(searchbase),
    mSaveContactGroup(false)
{
    Q_ASSERT(connection.handle());
    connect( &mLdapSearch, SIGNAL(result(KLDAP::LdapSearch*)),
           this, SLOT(gotSearchResult(KLDAP::LdapSearch*)) );
    connect( &mLdapSearch, SIGNAL(data(KLDAP::LdapSearch*,KLDAP::LdapObject)),
           this, SLOT(gotSearchData(KLDAP::LdapSearch*,KLDAP::LdapObject)) );
}

void RetrieveGroupMembersJob::doStart()
{
    kDebug();
    Akonadi::ItemFetchJob *job = new Akonadi::ItemFetchJob(mParentCollection, this);
    job->fetchScope().setFetchModificationTime(false);
    job->fetchScope().setCacheOnly(true);
    job->fetchScope().fetchFullPayload(false);
    connect(job, SIGNAL(itemsReceived(Akonadi::Item::List)), this, SLOT(localItemsReceived(Akonadi::Item::List)));
    connect(job, SIGNAL(result(KJob*)), this, SLOT(localFetchDone(KJob*)));
    mTime.start();
}

void RetrieveGroupMembersJob::setFetchScope(RetrieveGroupMembersJob::FetchScope fetchScope)
{
    mFetchScope = fetchScope;
}

void RetrieveGroupMembersJob::localItemsReceived(const Akonadi::Item::List &items)
{
    kDebug() << items.size();
    foreach (const Akonadi::Item &item, items) {
        kDebug() << item.remoteId() << item.remoteRevision();
        if (mLocalItems.contains(item.remoteId())) {
            Akonadi::ItemDeleteJob *job = new Akonadi::ItemDeleteJob(item, transaction());
            transaction()->setIgnoreJobFailure(job);
            continue;
        }
        mLocalItems.insert(item.remoteId(), item.remoteRevision());
        mRemoteLocalIds.insert(item.remoteId(), item.id());
    }
}

void RetrieveGroupMembersJob::localFetchDone(KJob *job)
{
    kDebug();
    if (job->error()) {
        kWarning() << "retrieval failed";
        setError(KJob::UserDefinedError);
        emitResult();
        return;
    }
    searchForGroup();
}

void RetrieveGroupMembersJob::searchForGroup()
{
    kDebug();
    const int ret = mLdapSearch.search( KLDAP::LdapDN(mSearchbase), KLDAP::LdapUrl::Sub, QString("%1=%2").arg(LDAPMapper::getAttribute(LDAPMapper::UniqueIdentifier)).arg(mParentCollection.remoteId()), QStringList() << "nsuniqueid" << "uniqueMember" << "cn");
    if (!ret) {
        kWarning() << mLdapSearch.errorString();
        kWarning() << "retrieval failed";
        setError(KJob::UserDefinedError);
        emitResult();
    }
}

void RetrieveGroupMembersJob::searchForMember(const QString &memberDn)
{
    kDebug();
    const QStringList attributes = mFetchScope == FullPayload ? LDAPMapper::requestedFullPayloadAttributes()
                                                              : LDAPMapper::requestedLookupPayloadAttributes();
    const int ret = mLdapSearch.search( KLDAP::LdapDN(memberDn), KLDAP::LdapUrl::Base, QString(), attributes);
    if (!ret) {
        kWarning() << mLdapSearch.errorString();
        kWarning() << "retrieval failed";
        setError(KJob::UserDefinedError);
        emitResult();
    }
}

bool RetrieveGroupMembersJob::getNextMember()
{
    if (mGroupMembers.isEmpty()) {
        return false;
    }
    const QString member = mGroupMembers.takeFirst();
    searchForMember(member);
    return true;
}

void RetrieveGroupMembersJob::gotSearchResult(KLDAP::LdapSearch *search)
{
    Q_UNUSED( search );
    kDebug() << search->isFinished(); 
    if (search->error()) {
        kWarning() << search->error() << search->errorString(); 
        switch (search->error()) {
            case KLDAP_SIZELIMIT_EXCEEDED:
                kWarning() << "Sizelimit exceeded";
                break;
            default:
                kWarning() << "Unknown error";
        }
        setError(KJob::UserDefinedError);
        done();
        return;
    }

    if (getNextMember()) {
        return;
    }

    //only do the removal if we got all entires without anything missing
    Akonadi::Item::List toRemove;
    toRemove.reserve(mLocalItems.size());
    QHash<QString, QString>::const_iterator it = mLocalItems.constBegin();
    for (; it != mLocalItems.constEnd(); it++) {
        kDebug() << mParentCollection.name() <<  "deleted " << it.key();
        Akonadi::Item item;
        item.setRemoteId(it.key());
        toRemove << item;
    }
    if (!toRemove.isEmpty()) {
        Akonadi::ItemDeleteJob *job = new Akonadi::ItemDeleteJob(toRemove, transaction());
        transaction()->setIgnoreJobFailure(job);
    }

    if (!mTransaction) { // no jobs created here -> savegroup
        if (mSaveContactGroup) {
            saveContactGroup();
        } else {
            done();
        }
    } else {
        mTransaction->commit();
    }
}

void RetrieveGroupMembersJob::gotSearchData(KLDAP::LdapSearch *search, const KLDAP::LdapObject &obj)
{
    Q_UNUSED( search );
    kWarning();
    kDebug() << "Object:";
    kDebug() << obj.toString();
    if (obj.value("nsuniqueid") == mParentCollection.remoteId()) {
        foreach (const QByteArray &val, obj.values("uniqueMember")) {
            mGroupMembers << val;
        }
        kDebug() << "found members: " << mGroupMembers;

        KABC::ContactGroup group;
        group.setName(obj.value("cn"));
        //group.setEmail(obj.value("email"));
        mGroup = group;
        mGroupItem = Akonadi::Item();
        mGroupItem.setRemoteId(LDAPMapper::getStableIdentifier(obj));
        mGroupItem.setMimeType(KABC::ContactGroup::mimeType());
        const QHash<QString, QString>::iterator it = mLocalItems.find(mGroupItem.remoteId());
        mSaveContactGroup = true;
        if (it != mLocalItems.end()) {
            const QHash<QString, Akonadi::Entity::Id>::iterator uid = mRemoteLocalIds.find(mGroupItem.remoteId());
            mGroupItem.setId(*uid);
            kDebug() <<  mGroupItem.remoteId() <<  mGroupItem.id();
            if (*it == LDAPMapper::getTimestamp(obj)) {
                mSaveContactGroup = false;
                kDebug() << "skipping " << mGroupItem.remoteId();
            }
            mLocalItems.erase(it);
            return;
        }
    } else {
        kDebug() << "got person: " << obj.dn().toString() << obj.value("nsuniqueid") << obj.value("modifyTimestamp");
        Akonadi::Item item;
        item.setRemoteId(LDAPMapper::getStableIdentifier(obj));
        item.setPayload(LDAPMapper::getAddressee(obj));
        item.setMimeType(KABC::Addressee::mimeType());
        item.setParentCollection(mParentCollection);
        item.setRemoteRevision(LDAPMapper::getTimestamp(obj));

        const QHash<QString, QString>::iterator it = mLocalItems.find(item.remoteId());
        if (it != mLocalItems.end()) {
            const QHash<QString, Akonadi::Entity::Id>::iterator uid = mRemoteLocalIds.find(item.remoteId());
            KABC::ContactGroup::ContactReference reference;
            reference.setUid(QString::number(*uid));
            mGroup.append(reference);
            if (*it == LDAPMapper::getTimestamp(obj)) {
                kDebug() << "skipping " << item.remoteId();
            } else {
                kDebug() << "modification";
                new Akonadi::ItemModifyJob(item, transaction());
            }
            mLocalItems.erase(it);
            return;
        }
        //new item
        Akonadi::ItemCreateJob *job = new Akonadi::ItemCreateJob(item, mParentCollection, transaction());
        connect(job, SIGNAL(result(KJob*)), SLOT(createdItem(KJob*)));
    }
}

void RetrieveGroupMembersJob::createdItem(KJob* job) {
    Akonadi::ItemCreateJob *itemjob =static_cast<Akonadi::ItemCreateJob*>(job);
    if (!job->error()) {
        mSaveContactGroup = true;
        KABC::ContactGroup::ContactReference reference;
        reference.setUid(QString::number(itemjob->item().id()));
        mGroup.append(reference);
    }
}

Akonadi::TransactionSequence* RetrieveGroupMembersJob::transaction()
{
    if ( !mTransaction ) {
        mTransaction= new Akonadi::TransactionSequence( this );
        mTransaction->setAutomaticCommittingEnabled( false );
        connect(mTransaction, SIGNAL(result(KJob*)), SLOT(transactionDone(KJob*)) );
    }
    return mTransaction;
}

void RetrieveGroupMembersJob::transactionDone (KJob* job)
{
    if ( job->error() ) {
        return; // handled by base class
    }

    if (mSaveContactGroup) {
        saveContactGroup();
    } else {
        done();
    }
}

void RetrieveGroupMembersJob::saveContactGroup()
{
    mGroupItem.setPayload(mGroup);
    if (mGroupItem.isValid()) {
        kDebug() << "modify";
        Akonadi::ItemModifyJob *job = new Akonadi::ItemModifyJob(mGroupItem, this);
        connect(job, SIGNAL(result(KJob*)), SLOT(savedContactGroup(KJob*)));
    } else {
        kDebug() << "new";
        Akonadi::ItemCreateJob::Job *job = new Akonadi::ItemCreateJob(mGroupItem, mParentCollection, this);
        connect(job, SIGNAL(result(KJob*)), SLOT(savedContactGroup(KJob*)));
    }
}

void RetrieveGroupMembersJob::savedContactGroup(KJob *job)
{
    if ( job->error() ) {
        return; // handled by base class
    }
    done();
}


void RetrieveGroupMembersJob::done()
{
    kDebug() << "Done. Took " << mTime.elapsed()/1000.0 << " s";
    emitResult();
}

