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

#include "retrievegroupsjob.h"
#include "ldapmapper.h"
#include <KABC/Addressee>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>
#include <Akonadi/ItemCreateJob>
#include <Akonadi/ItemModifyJob>
#include <Akonadi/ItemDeleteJob>
#include <kldap/ldapdefs.h>
#include <quuid.h>

RetrieveGroupsJob::RetrieveGroupsJob(const QString &searchbase, const Akonadi::Collection& col, KLDAP::LdapConnection& connection, QObject* parent)
:   Job(parent),
    mLdapSearch(connection),
    mParentCollection(col),
    mSearchbase(searchbase)
{
    Q_ASSERT(connection.handle());
    connect( &mLdapSearch, SIGNAL(result(KLDAP::LdapSearch*)),
           this, SLOT(gotSearchResult(KLDAP::LdapSearch*)) );
    connect( &mLdapSearch, SIGNAL(data(KLDAP::LdapSearch*,KLDAP::LdapObject)),
           this, SLOT(gotSearchData(KLDAP::LdapSearch*,KLDAP::LdapObject)) );
}

void RetrieveGroupsJob::doStart()
{
    search();
}

void RetrieveGroupsJob::search()
{
    kDebug();
    const int ret = mLdapSearch.search( KLDAP::LdapDN(mSearchbase), KLDAP::LdapUrl::Sub,
                                        QLatin1String("(|(objectClass=groupofuniquenames)(objectClass=kolabgroupofuniquenames))"),
                                        QStringList() << "cn" << "nsuniqueid");
    if (!ret) {
        kWarning() << mLdapSearch.errorString();
        kWarning() << "retrieval failed";
        setError(KJob::UserDefinedError);
        emitResult();
    }
}

void RetrieveGroupsJob::gotSearchResult(KLDAP::LdapSearch *search)
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
    } else {
        kDebug() << mRetrievedCollections.size() << " groups retrieved";
    }
    emitResult();
}

void RetrieveGroupsJob::gotSearchData(KLDAP::LdapSearch *search, const KLDAP::LdapObject &obj)
{
    Q_UNUSED( search );
    kWarning();
    kDebug() << "Object:";
    kDebug() << obj.toString();
    kDebug() << "got group: " << obj.dn().toString() << obj.value("nsuniqueid");
    Akonadi::Collection col;
    col.setRemoteId(LDAPMapper::getStableIdentifier(obj));
    col.setContentMimeTypes(QStringList() << KABC::Addressee::mimeType() << Akonadi::Collection::mimeType());
    col.setParentCollection(mParentCollection);
    col.setName(obj.value("cn"));
    mRetrievedCollections << col;
}

Akonadi::Collection::List RetrieveGroupsJob::retrievedCollections() const
{
    return mRetrievedCollections;
}
