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

#include "updateitemjob.h"

#include "ldapmapper.h"

#include <kldap/ldapdefs.h>

#include <akonadi/itemcreatejob.h>
#include <akonadi/itemfetchjob.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/itemmodifyjob.h>

UpdateItemJob::UpdateItemJob(const QString &ldapItemId, const QString &searchBase, KLDAP::LdapConnection &connection,
                             const Akonadi::Collection::List &parentCollections, QObject *parent)
:   KJob(parent),
    mLdapItemId(ldapItemId),
    mSearchbase(searchBase),
    mLdapSearch(connection),
    mParentCollections(parentCollections)
{
    Q_ASSERT(connection.handle());
    connect(&mLdapSearch, SIGNAL(result(KLDAP::LdapSearch*)),
            this, SLOT(gotSearchResult(KLDAP::LdapSearch*)));
    connect(&mLdapSearch, SIGNAL(data(KLDAP::LdapSearch*,KLDAP::LdapObject)),
            this, SLOT(gotSearchData(KLDAP::LdapSearch*,KLDAP::LdapObject)));

    // autostart like an Akonadi::Job
    QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
}

void UpdateItemJob::start()
{
    // TODO have IncrementalUpdateJob provide DN for item instead of searchbase
    const int ret = mLdapSearch.search(KLDAP::LdapDN(mSearchbase), KLDAP::LdapUrl::Sub,
                                       QLatin1String("nsuniqueid=") + mLdapItemId,
                                       LDAPMapper::requestedFullPayloadAttributes());
    if (!ret) {
        kWarning() << mLdapSearch.errorString();
        kWarning() << "retrieval failed";
        setError(KJob::UserDefinedError);
        emitResult();
    }
}

void UpdateItemJob::gotSearchResult(KLDAP::LdapSearch *search)
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
    } else {
        processNextParentCollection();
    }
}

void UpdateItemJob::gotSearchData(KLDAP::LdapSearch *search, const KLDAP::LdapObject &obj)
{
    Q_UNUSED( search );
    kWarning();

    kDebug() << "Object:";
    kDebug() << obj.toString();
    kDebug() << "got person: " << obj.dn().toString() << obj.value("nsuniqueid") << obj.value("modifyTimestamp");

    mItem.setRemoteId(LDAPMapper::getStableIdentifier(obj));
    mItem.setPayload(LDAPMapper::getAddressee(obj));
    mItem.setMimeType(KABC::Addressee::mimeType());
    mItem.setRemoteRevision(LDAPMapper::getTimestamp(obj));
}

void UpdateItemJob::localFetchDone(KJob *job)
{
    const Akonadi::Collection parentCollection = job->property("parentCollection").value<Akonadi::Collection>();
    const Akonadi::Item::List items = static_cast<Akonadi::ItemFetchJob*>(job)->items();

    if (job->error() || items.isEmpty()) {
        // if there is no such item we are OK unless this is the top level collection.
        // in this case an update could mean a new item, so we create it

        if (parentCollection.parentCollection() == Akonadi::Collection::root()) {
            Akonadi::Item item = mItem;
            Akonadi::ItemCreateJob *createJob = new Akonadi::ItemCreateJob(item, parentCollection, this);
            connect(createJob, SIGNAL(result(KJob*)), this, SLOT(createJobDone(KJob*)));
        } else {
            processNextParentCollection();
        }
    } else {
        Akonadi::Item item = mItem;
        item.setId(items.at(0).id());

        Akonadi::ItemModifyJob *modifyJob = new Akonadi::ItemModifyJob(item, this);
        connect(modifyJob, SIGNAL(result(KJob*)), this, SLOT(modifyJobDone(KJob*)));
    }
}

void UpdateItemJob::createJobDone(KJob *job)
{
    if (job->error()) {
        kWarning() << job->errorString();

        // try to proceed as far as possible
    }

    processNextParentCollection();
}

void UpdateItemJob::modifyJobDone(KJob *job)
{
    if (job->error()) {
        kWarning() << job->errorString();

        // try to proceed as far as possible
    }

    processNextParentCollection();
}

void UpdateItemJob::processNextParentCollection()
{
    if (mParentCollections.isEmpty()) {
        emitResult();
        return;
    }

    const Akonadi::Collection parentCollection = mParentCollections.takeFirst();

    Akonadi::Item item = mItem;
    item.setParentCollection(parentCollection);

    Akonadi::ItemFetchJob *fetchJob = new Akonadi::ItemFetchJob(item, this);
    fetchJob->fetchScope().setCacheOnly(true);
    fetchJob->setProperty("parentCollection", QVariant::fromValue(parentCollection));
    connect(fetchJob, SIGNAL(result(KJob*)), this, SLOT(localFetchDone(KJob*)));
}
