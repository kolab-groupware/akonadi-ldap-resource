#include <kldap/ldapserver.h>
#include <kldap/ldapconnection.h>
#include <kdebug.h>
#include <qcoreapplication.h>
#include "retrieveitemsjob.h"



int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    KLDAP::LdapServer mLdapServer;
    KLDAP::LdapConnection mLdapConnection;
    mLdapServer.setHost("192.168.122.151");
    mLdapServer.setBaseDn(KLDAP::LdapDN("dc=example,dc=org"));
    mLdapServer.setBindDn("uid=doe,ou=People,dc=example,dc=org");
    mLdapServer.setPassword("kolab");
    mLdapServer.setAuth(KLDAP::LdapServer::Simple);
    mLdapServer.setSecurity(KLDAP::LdapServer::None);
//     mLdapServer.setTimeout(3);
//     mLdapServer.setTimeLimit(10);
    
    kDebug() << mLdapServer.host();
    mLdapConnection.setServer(mLdapServer);
    kDebug() << mLdapConnection.server().host();
    if (mLdapConnection.handle()) {
        kWarning() << "already connected";
    }
    if (mLdapConnection.connect()) {
        kWarning() << mLdapConnection.connectionError();
        kWarning() << "failed to connect to server";
        return -1;
    }
    kWarning() << "Connected";
    Q_ASSERT(mLdapConnection.handle());
//     KLDAP::LdapSearch mLdapSearch(mLdapConnection);
//     QString baseDN("dc=example,dc=org");
//     mLdapSearch.search( KLDAP::LdapDN(baseDN), KLDAP::LdapUrl::Base, QString(), QStringList() << "dn" << "objectClass" );
    
    RetrieveItemsJob *job = new RetrieveItemsJob(mLdapServer.baseDn().toString(), Akonadi::Collection(), mLdapConnection);
    job->exec();
}