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
#include <quuid.h>

RetrieveItemsJob::RetrieveItemsJob(const Akonadi::Collection& col, KLDAP::LdapConnection& connection, QObject* parent)
:   Job(parent),
    mLdapSearch(connection),
    mParentCollection(col)
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
    QStringList requestedAttributes;
    requestedAttributes << "dn" << "uid" << "cn" << "givenName" << "sn" << "mail" << "alias" << "displayName" << "nsuniqueid" << "modifyTimestamp";
    const int ret = mLdapSearch.search( KLDAP::LdapDN(searchbase), KLDAP::LdapUrl::Sub, QLatin1String("objectClass=inetorgperson"), requestedAttributes);
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
    kWarning() << mRetrievedItems.size();
    if (!mRetrievedItems.isEmpty()) {
        emit contactsRetrieved(mRetrievedItems);
    }
    emitResult();
}

void RetrieveItemsJob::gotSearchData(KLDAP::LdapSearch *search, const KLDAP::LdapObject &obj)
{
    Q_UNUSED( search );
    kWarning();
    kDebug() << "Object:";
    kDebug() << obj.toString();
    kDebug() << "got person: " << obj.dn().toString() << obj.value("nsuniqueid") << obj.value("modifyTimestamp");
    Akonadi::Item item;
    item.setRemoteId(obj.dn().toString());
    if (mLocalItems.contains(item.remoteId())) {
        if (mLocalItems.value(item.remoteId()) == obj.value("modifyTimestamp")) {
            kDebug() << "skipping " << item.remoteId();
            return;
        } else {
            kDebug() << "modification";
            //get uid and reuse
        }
    }
    
    //new item
    KABC::Addressee addressee;
    addressee.setName(obj.value("cn"));
    addressee.setGivenName(obj.value("givenName"));
    addressee.setFamilyName(obj.value("sn"));
    addressee.setFormattedName(obj.value("displayName"));
    QStringList email(obj.value("mail"));
    foreach(const QByteArray &e, obj.values("alias")) {
        email << e;
    }
    addressee.setEmails(email);
    item.setPayload(addressee);
    item.setMimeType(KABC::Addressee::mimeType());
    item.setParentCollection(mParentCollection);
    item.setRemoteRevision(obj.value("modifyTimestamp"));
    mRetrievedItems.append(item);
//     Akonadi::ItemFetchJob *fetchJob = new Akonadi::ItemFetchJob(item, this);
//     fetchJob->fetchScope().setFetchModificationTime(false);
//     fetchJob->fetchScope().setCacheOnly(true);
//     fetchJob->fetchScope().fetchFullPayload(false);
// //     connect(fetchJob, SIGNAL(itemsReceived(Akonadi::Item::List)), this, SLOT(localItemReceived(Akonadi::Item::List)));
//     connect(fetchJob, SIGNAL(result(KJob*)), this, SLOT(localItemFetchDone(KJob*)));
}

void RetrieveItemsJob::localItemReceived(const Akonadi::Item::List &list)
{

}

void RetrieveItemsJob::localItemFetchDone(KJob *job)
{
    kDebug();
    if (job->error()) {
        //new item
    kDebug() << "new item";
    }
//     Akonadi::ItemFetchJob *fetchJob = static_cast<Akonadi::ItemFetchJob>(job);
//     const Akonadi::Item &item = fetchJob->items().first();
    //modify or same as before
    kDebug() << "modify";
}


