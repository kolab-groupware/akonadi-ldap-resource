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

#ifndef UPDATEGROUPJOB_H
#define UPDATEGROUPJOB_H

#include <akonadi/collection.h>
#include <akonadi/item.h>

#include <KLDAP/LdapSearch>

#include <kjob.h>

namespace Akonadi {
    class TransactionSequence;
}

struct GroupUpdate;

class UpdateGroupJob : public KJob
{
    Q_OBJECT
public:
    UpdateGroupJob(const QString &searchBase, KLDAP::LdapConnection &connection, const Akonadi::Collection &collection, QObject *parent = 0);
    UpdateGroupJob(const GroupUpdate &updateData, const QString &searchBase, KLDAP::LdapConnection &connection, const Akonadi::Collection &collection, QObject *parent = 0);

public Q_SLOTS:
    virtual void start();

private Q_SLOTS:
    void gotSearchResult(KLDAP::LdapSearch *search);
    void gotSearchData(KLDAP::LdapSearch *search, const KLDAP::LdapObject &obj);
    void collectionModifyDone(KJob *job);
    void retrieveMembersDone(KJob *job);
    void localFetchDone(KJob*job);
    void transactionDone(KJob* job);

private:
    Akonadi::TransactionSequence *transaction();
    void fetchLocalItems();
    void searchForAllMembers();
    void searchForMember(const QString &memberDn);
    void processMembers();
    void processNewMember();

    Akonadi::TransactionSequence *mTransaction;

    const QString mName;
    const QString mTimestamp;
    const QString mSearchbase;
    KLDAP::LdapConnection &mConnection;
    KLDAP::LdapSearch mLdapSearch;

    Akonadi::Collection mCollection;
    QHash<QString, Akonadi::Item> mLocalItems;

    QStringList mNewMembers;

    enum Phase {
        ListMembers,
        FetchMembers
    };
    Phase mPhase;
};

#endif // UPDATEGROUPJOB_H
