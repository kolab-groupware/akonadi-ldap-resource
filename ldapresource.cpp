#include "ldapresource.h"
#include "retrieveitemsjob.h"
#include "retrieveitemjob.h"
#include "retrievegroupsjob.h"
#include "retrievegroupmembersjob.h"

#include "settings.h"
#include "settingsadaptor.h"

#include <QtDBus/QDBusConnection>

#include <Akonadi/CachePolicy>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>
#include <akonadi/kmime/messageparts.h>

#include <KABC/Addressee>
#include <KLDAP/LdapConfigWidget>
#include <KLDAP/LdapServer>
#include <kconfigdialog.h>
#include <klocalizedstring.h>
#include <kwindowsystem.h>
#include <Akonadi/ChangeRecorder>
using namespace Akonadi;


LDAPResource::LDAPResource( const QString &id )
    : ResourceBase( id )
{
    new SettingsAdaptor( Settings::self() );
    QDBusConnection::sessionBus().registerObject( QLatin1String( "/Settings" ),
                                Settings::self(), QDBusConnection::ExportAdaptors );

    setNeedsNetwork(true);
    loadConfig();
    
    changeRecorder()->itemFetchScope().fetchFullPayload(false);
    changeRecorder()->itemFetchScope().setAncestorRetrieval( ItemFetchScope::None );
    changeRecorder()->setChangeRecordingEnabled(false);
    
    //Ensure the root collection is immmediately created
    synchronizeCollectionTree();
}

LDAPResource::~LDAPResource()
{
}

void LDAPResource::loadConfig()
{
    mLdapConnection.close();
    const Settings *s = Settings::self();
    mLdapServer.setHost(s->ldaphost());
    mLdapServer.setBaseDn(KLDAP::LdapDN(s->ldapdn()));
    mLdapServer.setBindDn(s->ldapbinddn());
    mLdapServer.setPassword(s->ldappassword());
    mLdapServer.setAuth(KLDAP::LdapServer::Simple);
    mLdapServer.setSecurity(KLDAP::LdapServer::None);
    kDebug() << s->ldaphost();
    kDebug() << s->ldapdn();
    kDebug() << s->ldapbinddn();
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
    policy.setCacheTimeout(-1);
    policy.setIntervalCheckTime(-1);
    root.setCachePolicy(policy);

    QStringList mimeTypes;
    mimeTypes << Collection::mimeType();
    mimeTypes << KABC::Addressee::mimeType();
    root.setContentMimeTypes(mimeTypes);
    
    if (!connectToServer()) {
        cancelTask(i18n( "Failed to retrieve collections."));
        kWarning() << "Failed to connect";
        return;
    }
    RetrieveGroupsJob *retrieveJob = new RetrieveGroupsJob(mLdapServer.baseDn().toString(), root, mLdapConnection, this);
    connect(retrieveJob, SIGNAL(groupsRetrieved(Akonadi::Collection::List)), SLOT(groupsRetrieved(Akonadi::Collection::List)));
    connect(retrieveJob, SIGNAL(result(KJob*)), SLOT(slotGroupsRetrievalResult(KJob*)));
}

void LDAPResource::groupsRetrieved(const Collection::List &list)
{
    collectionsRetrieved(list);
}

void LDAPResource::slotGroupsRetrievalResult(KJob* job)
{
    if (job->error()) {
        cancelTask();
    }
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
    if (collection.parentCollection() == Collection::root()) {
        RetrieveItemsJob *job = new RetrieveItemsJob(mLdapServer.baseDn().toString(), collection, mLdapConnection, this);
        connect(job, SIGNAL(result(KJob*)), SLOT(slotItemsRetrievalResult(KJob*)));
    } else {
        //Groups
        RetrieveGroupMembersJob *job = new RetrieveGroupMembersJob(mLdapServer.baseDn().toString(), collection, mLdapConnection, this);
        connect(job, SIGNAL(result(KJob*)), SLOT(slotItemsRetrievalResult(KJob*)));
    }
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
    RetrieveItemJob *job = new RetrieveItemJob(mLdapServer.baseDn().toString(), item, mLdapConnection, this);
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
  
  KConfigDialog* dialog = KConfigDialog::exists( "settings" );

  if ( !dialog ) {
    // KConfigDialog didn't find an instance of this dialog, so lets
    // create it :
    dialog = new KConfigDialog( 0, "settings", Settings::self() );

    KLDAP::LdapConfigWidget::WinFlags featureFlags
      = KLDAP::LdapConfigWidget::W_BINDDN
      | KLDAP::LdapConfigWidget::W_PASS
      | KLDAP::LdapConfigWidget::W_HOST
      | KLDAP::LdapConfigWidget::W_PORT
      | KLDAP::LdapConfigWidget::W_DN;
    KLDAP::LdapConfigWidget *configWidget
      = new KLDAP::LdapConfigWidget( featureFlags, dialog );

    dialog->addPage( configWidget, i18n("LDAP Settings"), "settings" );

    connect( dialog, SIGNAL(okClicked()),
             this, SIGNAL(configurationDialogAccepted()) );
    connect( dialog, SIGNAL(cancelClicked()),
             this, SIGNAL(configurationDialogRejected()) );
  }

  if ( windowId ) {
    KWindowSystem::setMainWindow( dialog, windowId );
  }

  if (dialog->exec() == QDialog::Accepted) {
      kDebug() << "dialog accepted";
      loadConfig();
  }
}

AKONADI_RESOURCE_MAIN( LDAPResource )

#include "ldapresource.moc"
