#include "ldapresource.h"

#include "settings.h"
#include "settingsadaptor.h"

#include <QtDBus/QDBusConnection>

#include <Akonadi/CachePolicy>
#include <akonadi/kmime/messageparts.h>

#include <KABC/Addressee>

using namespace Akonadi;

LDAPResource::LDAPResource( const QString &id )
  : ResourceBase( id )
{
  new SettingsAdaptor( Settings::self() );
  QDBusConnection::sessionBus().registerObject( QLatin1String( "/Settings" ),
                            Settings::self(), QDBusConnection::ExportAdaptors );

  // TODO: you can put any resource specific initialization code here.
}

LDAPResource::~LDAPResource()
{
}

void LDAPResource::retrieveCollections()
{
  // TODO: this method is called when Akonadi wants to have all the
  // collections your resource provides.
  // Be sure to set the remote ID and the content MIME types
    QString mServerUrl("ldap://192.168.122.88");

    Collection root;
    root.setParentCollection( Collection::root() );
    root.setRemoteId( mServerUrl );
    root.setName( name() );
    root.setRights( Collection::ReadOnly );

    CachePolicy policy;
    policy.setInheritFromParent( false );
    policy.setSyncOnDemand( true );
    policy.setLocalParts( QStringList() << MessagePart::Envelope );
    policy.setCacheTimeout( 1 );
    policy.setIntervalCheckTime( -1 );
    root.setCachePolicy( policy );

    QStringList mimeTypes;
    mimeTypes << Collection::mimeType();
    mimeTypes << KABC::Addressee::mimeType();
    root.setContentMimeTypes( mimeTypes );


    Collection::List list;
    list << root;
    collectionsRetrieved( list );
}

void LDAPResource::retrieveItems( const Akonadi::Collection &collection )
{
  Q_UNUSED( collection );

  // TODO: this method is called when Akonadi wants to know about all the
  // items in the given collection. You can but don't have to provide all the
  // data for each item, remote ID and MIME type are enough at this stage.
  // Depending on how your resource accesses the data, there are several
  // different ways to tell Akonadi when you are done.
}

bool LDAPResource::retrieveItem( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED( item );
  Q_UNUSED( parts );

  // TODO: this method is called when Akonadi wants more data for a given item.
  // You can only provide the parts that have been requested but you are allowed
  // to provide all in one go

  return true;
}

void LDAPResource::aboutToQuit()
{
  // TODO: any cleanup you need to do while there is still an active
  // event loop. The resource will terminate after this method returns
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

void LDAPResource::itemAdded( const Akonadi::Item &item, const Akonadi::Collection &collection )
{
  Q_UNUSED( item );
  Q_UNUSED( collection );

  // TODO: this method is called when somebody else, e.g. a client application,
  // has created an item in a collection managed by your resource.

  // NOTE: There is an equivalent method for collections, but it isn't part
  // of this template code to keep it simple
}

void LDAPResource::itemChanged( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED( item );
  Q_UNUSED( parts );

  // TODO: this method is called when somebody else, e.g. a client application,
  // has changed an item managed by your resource.

  // NOTE: There is an equivalent method for collections, but it isn't part
  // of this template code to keep it simple
}

void LDAPResource::itemRemoved( const Akonadi::Item &item )
{
  Q_UNUSED( item );

  // TODO: this method is called when somebody else, e.g. a client application,
  // has deleted an item managed by your resource.

  // NOTE: There is an equivalent method for collections, but it isn't part
  // of this template code to keep it simple
}

AKONADI_RESOURCE_MAIN( LDAPResource )

#include "ldapresource.moc"
