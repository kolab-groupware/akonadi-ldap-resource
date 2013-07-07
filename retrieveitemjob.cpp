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

#include "retrieveitemjob.h"
#include "ldapmapper.h"
#include <KABC/Addressee>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>
#include <quuid.h>

RetrieveItemJob::RetrieveItemJob(const QString &searchbase, const Akonadi::Item& item, KLDAP::LdapConnection& connection, QObject* parent)
:   Job(parent),
    mLdapSearch(connection),
    mItemToFetch(item),
    mSearchbase(searchbase)
{
    Q_ASSERT(connection.handle());
    connect( &mLdapSearch, SIGNAL(result(KLDAP::LdapSearch*)),
           this, SLOT(gotSearchResult(KLDAP::LdapSearch*)) );
    connect( &mLdapSearch, SIGNAL(data(KLDAP::LdapSearch*,KLDAP::LdapObject)),
           this, SLOT(gotSearchData(KLDAP::LdapSearch*,KLDAP::LdapObject)) );
}


void RetrieveItemJob::doStart()
{
    kDebug();
    search();
}

void RetrieveItemJob::search()
{
    kDebug();
    const int ret = mLdapSearch.search( KLDAP::LdapDN(mSearchbase), KLDAP::LdapUrl::Sub, QString("%1=%2").arg(LDAPMapper::getAttribute(LDAPMapper::UniqueIdentifier)).arg(mItemToFetch.remoteId()), LDAPMapper::requestedAttributes());
    if (!ret) {
        kWarning() << mLdapSearch.errorString();
        kWarning() << "retrieval failed";
        setError(KJob::UserDefinedError);
        emitResult();
    }
}

void RetrieveItemJob::gotSearchResult(KLDAP::LdapSearch *search)
{
    Q_UNUSED( search );
    if (!search->error()) {
        kWarning() << "not found";
        setError(KJob::UserDefinedError);
        setErrorText(search->errorString());
    }
    emitResult();
}

void RetrieveItemJob::gotSearchData(KLDAP::LdapSearch *search, const KLDAP::LdapObject &obj)
{
    Q_UNUSED( search );
    kWarning();
    kDebug() << "Object:";
    kDebug() << obj.toString();
    kDebug() << "got person: " << obj.dn().toString();
    mItemToFetch.setPayload(LDAPMapper::getAddressee(obj));
    mItemToFetch.setRemoteRevision(LDAPMapper::getTimestamp(obj));
}

Akonadi::Item RetrieveItemJob::getItem() const
{
    return mItemToFetch;
}
