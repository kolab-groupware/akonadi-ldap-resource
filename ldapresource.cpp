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
#include "ldapresource.h"

#include "incrementalupdatejob.h"
#include "retrieveitemsjob.h"
#include "retrieveitemjob.h"
#include "retrievegroupsjob.h"
#include "retrievegroupmembersjob.h"

#include "settings.h"
#include "settingsadaptor.h"
#include "settingswidget.h"

#include <QtDBus/QDBusConnection>

#include <Akonadi/CachePolicy>
#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemFetchScope>
#include <akonadi/kmime/messageparts.h>

#include <KABC/Addressee>
#include <KLDAP/LdapServer>
#include <kconfigdialog.h>
#include <klocalizedstring.h>
#include <kwindowsystem.h>
#include <Akonadi/ChangeRecorder>
using namespace Akonadi;


LDAPResource::LDAPResource( const QString &id )
    : ResourceBase( id ),
      mIncrementalUpdateTimer(new QTimer(this))
{
    new SettingsAdaptor( Settings::self() );
    QDBusConnection::sessionBus().registerObject( QLatin1String( "/Settings" ),
                                Settings::self(), QDBusConnection::ExportAdaptors );

    setNeedsNetwork(true);
    loadConfig();
    
    changeRecorder()->itemFetchScope().fetchFullPayload(false);
    changeRecorder()->itemFetchScope().setAncestorRetrieval( ItemFetchScope::None );
    changeRecorder()->setChangeRecordingEnabled(false);
    
    //Due to groups
    setHierarchicalRemoteIdentifiersEnabled(true);
    
    //Ensure the root collection is immmediately created
    synchronizeCollectionTree();

    mIncrementalUpdateTimer->setSingleShot(true);
    connect(mIncrementalUpdateTimer, SIGNAL(timeout()), this, SLOT(scheduleIncrementalUpdateTask()));
    mIncrementalUpdateTimer->start();
}

LDAPResource::~LDAPResource()
{
}

void LDAPResource::loadConfig()
{
    mLdapConnection.close();
    const Settings *s = Settings::self();
    mLdapServer.setHost(s->ldaphost());
    mLdapServer.setPort(s->ldapport());
    mLdapServer.setBaseDn(KLDAP::LdapDN(s->ldapdn()));
    mLdapServer.setBindDn(s->ldapbinddn());
    mLdapServer.setPassword(s->ldappassword());
    mLdapServer.setAuth(KLDAP::LdapServer::Simple);
    mLdapServer.setSecurity(KLDAP::LdapServer::None);
    kDebug() << s->ldaphost();
    kDebug() << s->ldapdn();
    kDebug() << s->ldapbinddn();

    mIncrementalUpdateTimer->setInterval(s->incrementalupdateinterval() * 60 * 1000);
    setName(s->name());
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
    kDebug();
    Collection root;
    root.setParentCollection(Collection::root());
    root.setRemoteId(mLdapServer.host());
    root.setName(name());
    root.setRights(Collection::ReadOnly);

    CachePolicy policy;
    policy.setInheritFromParent(false);
    policy.setSyncOnDemand(true);
    policy.setCacheTimeout(-1);

    // cache policy interval is in minutes, config in hours
    const int fullUpdateInterval = Settings::self()->fullupdateinterval() * 60;
    policy.setIntervalCheckTime(fullUpdateInterval == 0 ? -1 : fullUpdateInterval);

    root.setCachePolicy(policy);

    QStringList mimeTypes;
    mimeTypes << Collection::mimeType();
    mimeTypes << KABC::Addressee::mimeType();
    root.setContentMimeTypes(mimeTypes);
    
    if (mLdapServer.host().isEmpty()) {
        emit error( QLatin1String("No host configured.") );
        //We want a dummy root to be able to access the config
        collectionsRetrieved( Collection::List() << root );
        return;
    }
    if (!connectToServer()) {
        emit error( QLatin1String("Failed to retrieve collections.") );
        collectionsRetrieved( Collection::List() << root );
        kWarning() << "Failed to connect";
        return;
    }
    RetrieveGroupsJob *retrieveJob = new RetrieveGroupsJob(mLdapServer.baseDn().toString(), root, mLdapConnection, this);
    retrieveJob->setProperty("root", QVariant::fromValue(root));
    connect(retrieveJob, SIGNAL(result(KJob*)), SLOT(slotGroupsRetrievalResult(KJob*)));
}

void LDAPResource::slotGroupsRetrievalResult(KJob* job)
{
    const Collection root = job->property("root").value<Collection>();
    if (job->error()) {
        emit error( QLatin1String("Failed to retrieve collections.") );
        collectionsRetrieved( Collection::List() << root );
    } else {
        RetrieveGroupsJob *retrieveJob = static_cast<RetrieveGroupsJob*>(job);
        Collection::List collections;
        collections << root;
        collections << retrieveJob->retrievedCollections();
        collectionsRetrieved( collections );
    }
}

void LDAPResource::retrieveItems( const Akonadi::Collection &collection )
{
    kWarning() << collection.remoteId();
    Q_UNUSED( collection );
    //Make an item synch also trigger a refetch of the collections
    synchronizeCollectionTree();

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
        mIncrementalUpdateTimer->start();
    }
}

bool LDAPResource::retrieveItem( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
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

void LDAPResource::scheduleIncrementalUpdateTask()
{
    scheduleCustomTask(this, "incrementalUpdateTask", QVariant());
}

void LDAPResource::incrementalUpdateTask(const QVariant &params)
{
    Q_UNUSED(params);

    IncrementalUpdateJob *job = new IncrementalUpdateJob(identifier(), mLdapServer.baseDn().toString(), mLdapConnection, this);
    connect(job, SIGNAL(result(KJob*)), this, SLOT(incrementalUpdateResult(KJob*)));

    // TODO progress reporting
}

void LDAPResource::incrementalUpdateResult(KJob *job)
{
    Q_UNUSED(job);

    taskDone();
    mIncrementalUpdateTimer->start();
}

void LDAPResource::aboutToQuit()
{
    // TODO: any cleanup you need to do while there is still an active
    // event loop. The resource will terminate after this method returns
    mLdapConnection.close();
}

void LDAPResource::configure( WId windowId )
{
    KConfigDialog* dialog = KConfigDialog::exists( "settings" );

    if ( !dialog ) {
        // KConfigDialog didn't find an instance of this dialog, so lets
        // create it :
        dialog = new KConfigDialog( 0, "settings", Settings::self() );
        dialog->setFaceType( KPageDialog::Plain );

        SettingsWidget *configWidget = new SettingsWidget( dialog );

        dialog->addPage( configWidget, i18n("Settings"), "settings" );

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
        synchronizeCollectionTree();
    }
}

AKONADI_RESOURCE_MAIN( LDAPResource )

#include "ldapresource.moc"
