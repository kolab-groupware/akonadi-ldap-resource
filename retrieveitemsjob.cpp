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
#include <KABC/Addressee>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>

RetrieveItemsJob::RetrieveItemsJob(const Akonadi::Collection& col, KLDAP::LdapConnection& connection, QObject* parent)
:   Job(parent),
    mLdapSearch(connection),
    mParentCollection(col)
{
    Q_ASSERT(connection.handle());
    Q_UNUSED(col);
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
    connect(job, SIGNAL(itemsReceived(Akonadi::Item::List)), this, SLOT(localItemsReceived(Akonadi::Item::List)));
    connect(job, SIGNAL(result(KJob*)), this, SLOT(localFetchDone(KJob*)));
}

void RetrieveItemsJob::localItemsReceived(const Akonadi::Item::List &items)
{
    kDebug() << items.size();
    foreach (const Akonadi::Item &item, items) {
        mLocalItemRemoteIds.insert(item.remoteId());
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
    if (!search()) {
        kWarning() << mLdapSearch.errorString();
        kWarning() << "retrieval failed";
        setError(KJob::UserDefinedError);
        emitResult();
    }
}


bool RetrieveItemsJob::search()
{
    kDebug();
    QString searchbase("dc=example,dc=org");
    return mLdapSearch.search( KLDAP::LdapDN(searchbase), KLDAP::LdapUrl::Sub, QString("objectClass=inetorgperson"), QStringList() << "dn" << "objectClass" << "uid");

}

void RetrieveItemsJob::gotSearchResult(KLDAP::LdapSearch *search)
{
  Q_UNUSED( search );
  kWarning();
  emit contactsRetrieved(mRetrievedItems);
  emitResult();
}

void RetrieveItemsJob::gotSearchData(KLDAP::LdapSearch *search, const KLDAP::LdapObject &obj)
{
    Q_UNUSED( search );
    kWarning();
    kDebug() << "Object:";
    kDebug() << obj.toString();
    kDebug() << "got person: " << obj.dn().toString();
    Akonadi::Item item;
    item.setRemoteId(obj.dn().toString());
    if (mLocalItemRemoteIds.contains(item.remoteId())) {
        //TODO detect updates
        return;
    }
    KABC::Addressee addressee;
    addressee.setName(obj.dn().toString());
    item.setPayload(addressee);
    item.setMimeType(KABC::Addressee::mimeType());
    item.setParentCollection(mParentCollection);
    mRetrievedItems.append(item);
}

