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

#ifndef LDAPRESOURCE_H
#define LDAPRESOURCE_H

#include <akonadi/resourcebase.h>
#include <KLDAP/LdapServer>
#include <KLDAP/LdapSearch>

class LDAPResource: public Akonadi::ResourceBase,
                    public Akonadi::AgentBase::Observer
{
Q_OBJECT

public:
    LDAPResource( const QString &id );
    ~LDAPResource();

public Q_SLOTS:
    virtual void configure( WId windowId );

protected Q_SLOTS:
    void retrieveCollections();
    void retrieveItems( const Akonadi::Collection &col );
    bool retrieveItem( const Akonadi::Item &item, const QSet<QByteArray> &parts );

protected:
    virtual void aboutToQuit();
    
private Q_SLOTS:
    void slotGroupsRetrievalResult (KJob* job);
    void slotItemsRetrievalResult (KJob* job);
    void slotItemRetrievalResult (KJob* job);
    void scheduleIncrementalUpdateTask();
    void incrementalUpdateTask(const QVariant &params);
    void incrementalUpdateResult(KJob *job);

private:
    void loadConfig();
    bool connectToServer();
    KLDAP::LdapServer mLdapServer;
    KLDAP::LdapConnection mLdapConnection;
    QTimer *mIncrementalUpdateTimer;
};

#endif
