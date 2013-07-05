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
    search();
}

bool RetrieveItemsJob::search()
{
    kDebug();
    QString searchbase("dc=example,dc=org");
    const int ret = mLdapSearch.search( KLDAP::LdapDN(searchbase), KLDAP::LdapUrl::Sub, QString("objectClass=inetorgperson"), QStringList() << "dn" << "objectClass" << "uid");
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
    KABC::Addressee addressee;
    addressee.setName(obj.dn().toString());
    item.setPayload(addressee);
    item.setMimeType(KABC::Addressee::mimeType());
    item.setParentCollection(mParentCollection);
    mRetrievedItems.append(item);
}

