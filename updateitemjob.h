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

#ifndef UPDATEITEMJOB_H
#define UPDATEITEMJOB_H

#include <KLDAP/LdapSearch>

#include <akonadi/collection.h>
#include <akonadi/item.h>

#include <kjob.h>

class UpdateItemJob : public KJob
{
    Q_OBJECT
public:
    UpdateItemJob(const QString &ldapItemId, const QString &searchBase, KLDAP::LdapConnection &connection,
                  const Akonadi::Collection::List &parentCollections, QObject *parent = 0);

public Q_SLOTS:
    virtual void start();

private Q_SLOTS:
    void gotSearchResult(KLDAP::LdapSearch *search);
    void gotSearchData(KLDAP::LdapSearch *search, const KLDAP::LdapObject &obj);
    void localFetchDone(KJob *job);
    void createJobDone(KJob *job);
    void modifyJobDone(KJob *job);

private:
    void processNextParentCollection();

    const QString mLdapItemId;
    const QString mSearchbase;
    KLDAP::LdapSearch mLdapSearch;

    Akonadi::Collection::List mParentCollections;
    Akonadi::Item mItem;
};

#endif // UPDATEITEMJOB_H
