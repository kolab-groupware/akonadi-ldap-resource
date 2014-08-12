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

#include "retrieveupdatesjob.h"

#include "ldapmapper.h"

#include <kldap/ldapdefs.h>

RetrieveUpdatesJob::RetrieveUpdatesJob(const QString &timestamp, const QString &searchBase, KLDAP::LdapConnection &connection, QObject *parent)
:   KJob(parent),
    mTimeQuery(QString::fromUtf8("(&(modifyTimestamp>=%1)(!(modifyTimestamp=%1)))").arg(timestamp)),
    mSearchbase(searchBase),
    mLdapSearch(connection),
    mPhase(RetrieveItemUpdates)
{
    Q_ASSERT(connection.handle());
    connect(&mLdapSearch, SIGNAL(result(KLDAP::LdapSearch*)),
            this, SLOT(gotSearchResult(KLDAP::LdapSearch*)));
    connect(&mLdapSearch, SIGNAL(data(KLDAP::LdapSearch*,KLDAP::LdapObject)),
            this, SLOT(gotSearchData(KLDAP::LdapSearch*,KLDAP::LdapObject)));

    // autostart like an Akonadi::Job
    QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
}

QStringList RetrieveUpdatesJob::items() const
{
    return mItems;
}

GroupUpdateList RetrieveUpdatesJob::groups() const
{
    return mGroups;
}

QString RetrieveUpdatesJob::nextTimestamp() const
{
    return mNextTimestamp;
}

void RetrieveUpdatesJob::start()
{
    retrieveItemUpdates();
}

void RetrieveUpdatesJob::gotSearchResult(KLDAP::LdapSearch *search)
{
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
        emitResult();
        return;
    }

    switch (mPhase) {
        case RetrieveItemUpdates:
            retrieveGroupUpdates();
            break;

        case RetrieveGroupUpdates:
            emitResult();
            return;
    }
}

void RetrieveUpdatesJob::gotSearchData(KLDAP::LdapSearch *search, const KLDAP::LdapObject &obj)
{
    Q_UNUSED(search);

    kDebug() << "Object:";
    kDebug() << obj.toString();

    const QString id = LDAPMapper::getStableIdentifier(obj);

    switch (mPhase) {
        case RetrieveItemUpdates:
            kDebug() << "got person update";
            mItems << id;
            updateNextTimestamp(LDAPMapper::getTimestamp(obj));
            break;
        case RetrieveGroupUpdates:
            kDebug() << "got group update";
            mGroups << GroupUpdate(obj);

            // only update next timestamp if we did not have an update from items.
            // group updates is a separated query so there might have been item updates in between
            // which we don't want to miss next time
            if (mItems.isEmpty()) {
                updateNextTimestamp(LDAPMapper::getTimestamp(obj));
            }
            break;
    }
}

void RetrieveUpdatesJob::retrieveItemUpdates()
{
    Q_ASSERT(mPhase == RetrieveItemUpdates);

    const QString query = QLatin1String("(objectClass=inetorgperson)"); //+ mTimeQuery + QLatin1String(")");

    const int ret = mLdapSearch.search(KLDAP::LdapDN(mSearchbase), KLDAP::LdapUrl::Sub, query,
                                       QStringList() << LDAPMapper::getAttribute(LDAPMapper::UniqueIdentifier)
                                                     << "modifyTimestamp");
    if (!ret) {
        kWarning() << mLdapSearch.errorString();
        kWarning() << "retrieval failed";
        setError(KJob::UserDefinedError);
        emitResult();
    }
}

void RetrieveUpdatesJob::retrieveGroupUpdates()
{
    mPhase = RetrieveGroupUpdates;

    const QString query = QLatin1String("(&(|(objectClass=groupofuniquenames)(objectClass=kolabgroupofuniquenames))") + mTimeQuery + QLatin1String(")");

    const int ret = mLdapSearch.search(KLDAP::LdapDN(mSearchbase), KLDAP::LdapUrl::Sub, query,
                                       QStringList() << LDAPMapper::getAttribute(LDAPMapper::UniqueIdentifier) << "cn" << "modifyTimestamp");
    if (!ret) {
        kWarning() << mLdapSearch.errorString();
        kWarning() << "retrieval failed";
        setError(KJob::UserDefinedError);
        emitResult();
    }
}

void RetrieveUpdatesJob::updateNextTimestamp(const QString &itemTimestamp)
{
    if (!itemTimestamp.isEmpty()) {
        if (mNextTimestamp.isEmpty() || mNextTimestamp < itemTimestamp) {
            mNextTimestamp = itemTimestamp;
        }
    }
}
