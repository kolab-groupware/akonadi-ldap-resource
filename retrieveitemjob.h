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


#ifndef RETRIEVEITEMJOB_H
#define RETRIEVEITEMJOB_H

#include <kjob.h>
#include <akonadi/job.h>
#include <KLDAP/LdapSearch>
#include <akonadi/collection.h>
#include <akonadi/item.h>

class RetrieveItemJob :  public Akonadi::Job
{
    Q_OBJECT
public:
    explicit RetrieveItemJob(const QString &searchbase, const Akonadi::Item &item, KLDAP::LdapConnection &connection, QObject* parent = 0);
    virtual void doStart();
    Akonadi::Item getItem() const;
    
private Q_SLOTS:
    void gotSearchResult(KLDAP::LdapSearch *search);
    void gotSearchData(KLDAP::LdapSearch *search, const KLDAP::LdapObject &obj);
    
private:
    void search();
    KLDAP::LdapSearch mLdapSearch;
    Akonadi::Item mItemToFetch;
    QString mSearchbase;
};

#endif // RETRIEVEITEMJOB_H
