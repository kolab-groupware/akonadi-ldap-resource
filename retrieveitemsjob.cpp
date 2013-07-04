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

RetrieveItemsJob::RetrieveItemsJob(const Akonadi::Collection& col, KLDAP::LdapConnection& connection, QObject* parent)
:   KJob(parent),
    mLdapSearch(connection)
{
    Q_ASSERT(connection.handle());
    Q_UNUSED(col);
    connect( &mLdapSearch, SIGNAL(result(KLDAP::LdapSearch*)),
           this, SLOT(gotSearchResult(KLDAP::LdapSearch*)) );
    connect( &mLdapSearch, SIGNAL(data(KLDAP::LdapSearch*,KLDAP::LdapObject)),
           this, SLOT(gotSearchData(KLDAP::LdapSearch*,KLDAP::LdapObject)) );

}

void RetrieveItemsJob::start()
{
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
    return mLdapSearch.search( KLDAP::LdapDN(searchbase), KLDAP::LdapUrl::Sub, QString(), QStringList() << "dn" << "objectClass" << "uid");

}

void RetrieveItemsJob::gotSearchResult(KLDAP::LdapSearch *search)
{
  Q_UNUSED( search );
  kWarning();
  emitResult();
}

void RetrieveItemsJob::gotSearchData(KLDAP::LdapSearch *search, const KLDAP::LdapObject &obj)
{
    Q_UNUSED( search );
    kWarning();
    kDebug() << "Object:";
    kDebug() << obj.toString();
    if (obj.values("objectClass").contains("inetorgperson")) {
        kDebug() << "got person: " << obj.dn().toString();
    }
    if (obj.values("objectClass").contains("groupofuniquenames")) {
        kDebug() << "got group: " << obj.dn().toString();
    }
}

