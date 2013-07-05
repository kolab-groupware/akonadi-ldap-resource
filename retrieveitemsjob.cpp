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

#include "retrieveitemsjob.h"
#include "ldapmapper.h"
#include <KABC/Addressee>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>
#include <Akonadi/ItemCreateJob>
#include <Akonadi/ItemModifyJob>
#include <quuid.h>

RetrieveItemsJob::RetrieveItemsJob(const Akonadi::Collection& col, KLDAP::LdapConnection& connection, QObject* parent)
:   Job(parent),
    mLdapSearch(connection),
    mParentCollection(col),
    mTransaction(0)
{
    Q_ASSERT(connection.handle());
    connect( &mLdapSearch, SIGNAL(result(KLDAP::LdapSearch*)),
           this, SLOT(gotSearchResult(KLDAP::LdapSearch*)) );
    connect( &mLdapSearch, SIGNAL(data(KLDAP::LdapSearch*,KLDAP::LdapObject)),
           this, SLOT(gotSearchData(KLDAP::LdapSearch*,KLDAP::LdapObject)) );
}

void RetrieveItemsJob::doStart()
{
    kDebug();
    Akonadi::ItemFetchJob *job = new Akonadi::ItemFetchJob(mParentCollection, this);
    job->fetchScope().setFetchModificationTime(false);
    job->fetchScope().setCacheOnly(true);
    job->fetchScope().fetchFullPayload(false);
    connect(job, SIGNAL(itemsReceived(Akonadi::Item::List)), this, SLOT(localItemsReceived(Akonadi::Item::List)));
    connect(job, SIGNAL(result(KJob*)), this, SLOT(localFetchDone(KJob*)));
}

void RetrieveItemsJob::localItemsReceived(const Akonadi::Item::List &items)
{
    kDebug() << items.size();
    foreach (const Akonadi::Item &item, items) {
        kDebug() << item.remoteId() << item.remoteRevision();
        mLocalItems.insert(item.remoteId(), item.remoteRevision());
    }
}

void RetrieveItemsJob::localFetchDone(KJob *job)
{
    kDebug();
    if (job->error()) {
        kWarning() << "retrieval failed";
        setError(KJob::UserDefinedError);
        emitResult();
        return;
    }
    search();
}

void RetrieveItemsJob::search()
{
    kDebug();
    QString searchbase("dc=example,dc=org");
    const int ret = mLdapSearch.search( KLDAP::LdapDN(searchbase), KLDAP::LdapUrl::Sub, QLatin1String("objectClass=inetorgperson"), LDAPMapper::requestedAttributes());
    if (!ret) {
        kWarning() << mLdapSearch.errorString();
        kWarning() << "retrieval failed";
        setError(KJob::UserDefinedError);
        emitResult();
    }
}

void RetrieveItemsJob::gotSearchResult(KLDAP::LdapSearch *search)
{
    Q_UNUSED( search );
    if (!mTransaction) { // no jobs created here -> done
        emitResult();
    } else {
        mTransaction->commit();
    }
}

void RetrieveItemsJob::gotSearchData(KLDAP::LdapSearch *search, const KLDAP::LdapObject &obj)
{
    Q_UNUSED( search );
    kWarning();
    kDebug() << "Object:";
    kDebug() << obj.toString();
    kDebug() << "got person: " << obj.dn().toString() << obj.value("nsuniqueid") << obj.value("modifyTimestamp");
    Akonadi::Item item;
    item.setRemoteId(LDAPMapper::getStableIdentifier(obj));
    item.setPayload(LDAPMapper::getAddressee(obj));
    item.setMimeType(KABC::Addressee::mimeType());
    item.setParentCollection(mParentCollection);
    item.setRemoteRevision(LDAPMapper::getTimestamp(obj));
    
    if (mLocalItems.contains(item.remoteId())) {
        if (mLocalItems.value(item.remoteId()) == obj.value("modifyTimestamp")) {
            kDebug() << "skipping " << item.remoteId();
            return;
        } else {
            kDebug() << "modification";
            new Akonadi::ItemModifyJob(item, transaction());
            return;
        }
    }
    //new item
    new Akonadi::ItemCreateJob(item, mParentCollection, transaction());
}

Akonadi::TransactionSequence* RetrieveItemsJob::transaction()
{
    if ( !mTransaction ) {
        mTransaction= new Akonadi::TransactionSequence( this );
        mTransaction->setAutomaticCommittingEnabled( false );
        connect(mTransaction, SIGNAL(result(KJob*)), SLOT(transactionDone(KJob*)) );
    }
    return mTransaction;
}

void RetrieveItemsJob::transactionDone (KJob* job)
{
    if ( job->error() ) {
        return; // handled by base class
    }
    emitResult();
}        

