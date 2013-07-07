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
    void groupsRetrieved(const Akonadi::Collection::List &);
    void slotGroupsRetrievalResult (KJob* job);
    void contactsRetrieved(const Akonadi::Item::List &);
    void slotItemsRetrievalResult (KJob* job);
    void slotItemRetrievalResult (KJob* job);
    

private:
    bool connectToServer();
    KLDAP::LdapServer mLdapServer;
    KLDAP::LdapConnection mLdapConnection;
};

#endif
