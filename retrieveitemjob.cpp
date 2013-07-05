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

RetrieveItemJob::RetrieveItemJob(const Akonadi::Item& item, KLDAP::LdapConnection& connection, QObject* parent)
:   Job(parent),
    mLdapSearch(connection),
    mItemToFetch(item)
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
    QString searchbase("dc=example,dc=org");
    const int ret = mLdapSearch.search( KLDAP::LdapDN(mItemToFetch.remoteId()), KLDAP::LdapUrl::Base, QLatin1String("objectClass=*"), LDAPMapper::requestedAttributes());
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
    KABC::Addressee addressee(LDAPMapper::getAddressee(obj));
    mItemToFetch.setPayload(addressee);
}

Akonadi::Item RetrieveItemJob::getItem() const
{
    return mItemToFetch;
}
