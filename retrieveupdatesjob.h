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

#ifndef RETRIEVEUPDATESJOB_H
#define RETRIEVEUPDATESJOB_H

#include "incrementalupdatedata.h"

#include <akonadi/collection.h>
#include <akonadi/item.h>

#include <KLDAP/LdapSearch>

#include <kjob.h>

class RetrieveUpdatesJob : public KJob
{
    Q_OBJECT
public:
    RetrieveUpdatesJob(const QString &timestamp, const QString &searchBase, KLDAP::LdapConnection &connection, QObject *parent = 0);

    QStringList items() const;
    GroupUpdateList groups() const;

    QString nextTimestamp() const;

public Q_SLOTS:
    virtual void start();

private Q_SLOTS:
    void gotSearchResult(KLDAP::LdapSearch *search);
    void gotSearchData(KLDAP::LdapSearch *search, const KLDAP::LdapObject &obj);

private:
    void retrieveItemUpdates();
    void retrieveGroupUpdates();
    void updateNextTimestamp(const QString &itemTimestamp);

    const QString mTimeQuery;
    const QString mSearchbase;
    KLDAP::LdapSearch mLdapSearch;

    enum Phase {
        RetrieveItemUpdates,
        RetrieveGroupUpdates
    };
    Phase mPhase;

    QStringList mItems;
    GroupUpdateList mGroups;
    QString mNextTimestamp;
};

#endif // RETRIEVEUPDATESJOB_H
