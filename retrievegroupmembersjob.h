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


#ifndef RETRIEVEGROUPMEMBERS_H
#define RETRIEVEGROUPMEMBERS_H

#include <kjob.h>
#include <akonadi/job.h>
#include <KLDAP/LdapSearch>
#include <akonadi/collection.h>
#include <akonadi/item.h>
#include <akonadi/transactionsequence.h>
#include <QDateTime>

class RetrieveGroupMembersJob:  public Akonadi::Job
{
    Q_OBJECT
public:
    enum FetchScope {
        LookupPayload,
        FullPayload
    };

    explicit RetrieveGroupMembersJob(const QString &searchbase, const Akonadi::Collection &col, KLDAP::LdapConnection &connection, QObject* parent = 0);
    virtual void doStart();
    
    void setFetchScope(FetchScope fetchScope);

signals:
    void contactsRetrieved(const Akonadi::Item::List &);
    
private Q_SLOTS:
    void gotSearchResult(KLDAP::LdapSearch *search);
    void gotSearchData(KLDAP::LdapSearch *search, const KLDAP::LdapObject &obj);
    void localFetchDone(KJob*);
    void localItemsReceived(const Akonadi::Item::List &);
    void transactionDone(KJob* job);
    
private:
    Akonadi::TransactionSequence *transaction();
    void searchForGroup();
    void searchForMember(const QString &memberDn);
    void done();
    bool getNextMember();

    FetchScope mFetchScope;
    KLDAP::LdapSearch mLdapSearch;
    Akonadi::Collection mParentCollection;
    QHash<QString, QString> mLocalItems;
    Akonadi::TransactionSequence *mTransaction;
    QString mSearchbase;
    QTime mTime;
    QStringList mGroupMembers;
};

#endif // RETRIEVEITEMSJOB_H
