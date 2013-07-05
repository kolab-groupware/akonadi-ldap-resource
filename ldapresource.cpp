#include "ldapresource.h"
#include "retrieveitemsjob.h"
#include "retrieveitemjob.h"

#include "settings.h"
#include "settingsadaptor.h"

#include <QtDBus/QDBusConnection>

#include <Akonadi/CachePolicy>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>
#include <akonadi/kmime/messageparts.h>

#include <KABC/Addressee>
#include <KLDAP/LdapServer>
#include <klocalizedstring.h>

using namespace Akonadi;


LDAPResource::LDAPResource( const QString &id )
    : ResourceBase( id )
{
    new SettingsAdaptor( Settings::self() );
    QDBusConnection::sessionBus().registerObject( QLatin1String( "/Settings" ),
                                Settings::self(), QDBusConnection::ExportAdaptors );

    setNeedsNetwork(true);
    mLdapServer.setHost("192.168.122.231");
    mLdapServer.setBaseDn(KLDAP::LdapDN("dc=example,dc=org"));
    mLdapServer.setBindDn("uid=doe,ou=People,dc=example,dc=org");
    mLdapServer.setPassword("Welcome2KolabSystems");
    mLdapServer.setAuth(KLDAP::LdapServer::Simple);
    mLdapServer.setSecurity(KLDAP::LdapServer::None);
}

LDAPResource::~LDAPResource()
{
}

bool LDAPResource::connectToServer()
{
    mLdapConnection.setServer(mLdapServer);
    if (mLdapConnection.handle()) {
        kWarning() << "already connected";
        return true;
    }
    
    //This doesn't really open a connection, so we have to test ourselves if the server is available
    if (mLdapConnection.connect()) {
        kWarning() << mLdapConnection.connectionError();
        kWarning() << "failed to connect to server";
        return false;
    }
    Q_ASSERT(mLdapConnection.handle());
    kWarning() << "Connected";
    return true;
}


void LDAPResource::retrieveCollections()
{
    // TODO: this method is called when Akonadi wants to have all the
    // collections your resource provides.
    // Be sure to set the remote ID and the content MIME types
    
//     Maildir dir( mSettings->path(), mSettings->topLevelIsContainer() );
    if (mLdapServer.host().isEmpty()) {
        emit error( QLatin1String("No ldap host configured") );
        collectionsRetrieved( Collection::List() );
        return;
    }
    Collection root;
    root.setParentCollection(Collection::root());
    root.setRemoteId(mLdapServer.host());
    root.setName(name());
    root.setRights(Collection::ReadOnly);

    CachePolicy policy;
    policy.setInheritFromParent(false);
    policy.setSyncOnDemand(true);
    policy.setCacheTimeout(1);
    policy.setIntervalCheckTime(-1);
    root.setCachePolicy(policy);

    QStringList mimeTypes;
    mimeTypes << Collection::mimeType();
    mimeTypes << KABC::Addressee::mimeType();
    root.setContentMimeTypes(mimeTypes);

    Collection::List list;
    list << root;
//     list += listRecursive( root, dir );
    collectionsRetrieved(list);
}

void LDAPResource::retrieveItems( const Akonadi::Collection &collection )
{
    kWarning() << collection.remoteId();
    Q_UNUSED( collection );

    // TODO: this method is called when Akonadi wants to know about all the
    // items in the given collection. You can but don't have to provide all the
    // data for each item, remote ID and MIME type are enough at this stage.
    // Depending on how your resource accesses the data, there are several
    // different ways to tell Akonadi when you are done.
    if (!connectToServer()) {
        cancelTask(i18n( "Failed to retrieve collection '%1' is invalid.", collection.remoteId()));
        kWarning() << "Failed to connect";
        return;
    }
    RetrieveItemsJob *job = new RetrieveItemsJob( collection, mLdapConnection, this );
    connect(job, SIGNAL(contactsRetrieved(Akonadi::Item::List)), SLOT(contactsRetrieved(Akonadi::Item::List)));
    connect(job, SIGNAL(result(KJob*)), SLOT(slotItemsRetrievalResult(KJob*)));
}

void LDAPResource::contactsRetrieved(const Item::List &list)
{
    setItemStreamingEnabled(true);
    kDebug() << list.size();
    itemsRetrievedIncremental(list, Item::List());
}

void LDAPResource::slotItemsRetrievalResult (KJob* job)
{
    kDebug() << "item retrieval done";
    if ( job->error() ) {
        cancelTask(job->errorString());
    } else {
        itemsRetrievalDone();
    }
}

bool LDAPResource::retrieveItem( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
    Q_UNUSED( item );
    Q_UNUSED( parts );
    kDebug() << parts << item.remoteId();
    if (!connectToServer()) {
        kWarning() << "Failed to connect";
        return false;
    }

    // TODO: this method is called when Akonadi wants more data for a given item.
    // You can only provide the parts that have been requested but you are allowed
    // to provide all in one go
    RetrieveItemJob *job = new RetrieveItemJob(item, mLdapConnection, this);
    connect(job, SIGNAL(result(KJob*)), SLOT(slotItemRetrievalResult(KJob*)));
    return true;
}

void LDAPResource::slotItemRetrievalResult (KJob* job)
{
    kDebug() << "item retrieval done";
    if ( job->error() ) {
        cancelTask( job->errorString() );
        return;
    }
    itemRetrieved(static_cast<RetrieveItemJob*>(job)->getItem());
}

void LDAPResource::aboutToQuit()
{
    // TODO: any cleanup you need to do while there is still an active
    // event loop. The resource will terminate after this method returns
    mLdapConnection.close();
}

void LDAPResource::configure( WId windowId )
{
  Q_UNUSED( windowId );

  // TODO: this method is usually called when a new resource is being
  // added to the Akonadi setup. You can do any kind of user interaction here,
  // e.g. showing dialogs.
  // The given window ID is usually useful to get the correct
  // "on top of parent" behavior if the running window manager applies any kind
  // of focus stealing prevention technique
  //
  // If the configuration dialog has been accepted by the user by clicking Ok,
  // the signal configurationDialogAccepted() has to be emitted, otherwise, if
  // the user canceled the dialog, configurationDialogRejected() has to be emitted.
}

AKONADI_RESOURCE_MAIN( LDAPResource )

#include "ldapresource.moc"
