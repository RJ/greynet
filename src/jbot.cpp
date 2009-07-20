#include "jbot.h"
#include <boost/foreach.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include "greynet.h"

using namespace gloox;
using namespace std;

jbot::jbot(std::string jid, std::string pass, std::string server, unsigned short port, playdar::resolvers::greynet* g)
        : m_jid(jid), m_pass(pass), m_server(server), m_port(port),
        m_greynet(g)
{
    m_presences[Presence::Available]  = "available";
    m_presences[Presence::Chat]   ="chat";
    m_presences[Presence::Away]   ="away";
    m_presences[Presence::DND]   ="dnd";
    m_presences[Presence::XA]   ="xa";
    m_presences[Presence::Unavailable]   ="unavailable";
    m_presences[Presence::Probe]   ="probe";
    m_presences[Presence::Error]   ="error";
    m_presences[Presence::Invalid]   ="invalid";
    
}

void
jbot::start(const std::string& loglevel)
{
    JID jid( m_jid );
    
    // the google hack, because they filter disco features they don't know.
    if( jid.server().find("googlemail.") != string::npos 
        || jid.server().find("gmail.") != string::npos
        || jid.server().find("gtalk.") != string::npos )
    {
        if( jid.resource().find("playdar") == string::npos )
        {
            cout << "Forcing your /resource to contain 'playdar' (the google workaround)" << endl;
            jid.setResource( string("playdar_") + jid.resource() );
            m_jid = jid.full();
        }
    }
    
    j = new Client( jid, m_pass );
    if( m_server != "" ) j->setServer(m_server);
    j->setPort(m_port);
    j->registerConnectionListener( this );
    j->registerMessageHandler( this );
    j->rosterManager()->registerRosterListener( this );
    j->disco()->registerDiscoHandler( this );

    j->disco()->setVersion( "gloox_playdar", GLOOX_VERSION, "cross-platform" );
    j->disco()->setIdentity( "client", "bot" );
    j->disco()->addFeature( "playdar:resolver" );

    LogLevel ll = LogLevelDebug;
    if( loglevel == "warning" ) ll = LogLevelWarning;
    if( loglevel == "error"   ) ll = LogLevelError;
    j->logInstance().registerLogHandler( ll, LogAreaAll, this );
    // mark ourselves as "extended away" lowest priority:
    // there is no "invisible" in the spec. XA is the lowest?
    j->setPresence( Presence::XA, -128, "Daemon not human." );
    
    if ( j->connect( false ) )
    {
        cout << "********** Resource: " << j->resource() << endl ;
        ConnectionError ce = ConnNoError;
        while ( ce == ConnNoError )
        {
            ce = j->recv();
        }
        printf( "gloox box disconnected, ce: %d\n", ce );
    }
    delete( j );
}

void 
jbot::stop()
{
    j->setPresence( Presence::Unavailable, -128 );
    j->disco()->removeDiscoHandler( this );
    j->rosterManager()->removeRosterListener();
    j->removeMessageHandler(this);
    j->removeConnectionListener(this);
    
    if( j ) j->disconnect();
}

/// send msg to specific jid
void
jbot::send_to( const string& to, const string& msg )
{
    Message m(Message::Chat, JID(to), msg, "");
    j->send( m ); //FIXME no idea if this is threadsafe. need to RTFM
}

/// send msg to all playdarpeers
void 
jbot::broadcast_msg( const std::string& msg )
{
    const string subject = "";
    boost::mutex::scoped_lock lk(m_playdarpeers_mut);
    BOOST_FOREACH( const string& jidstr, m_playdarpeers )
    {
        printf("Dispatching query to: %s\n", jidstr.c_str() );
        JID jid(jidstr);
        Message m(Message::Chat, jid, msg, subject);
        j->send( m );
    }
}

void 
jbot::set_msg_received_callback( boost::function<void(const std::string&, const std::string&)> cb)
{
    m_msg_received_callback = cb;
}

void 
jbot::set_new_peer_callback( boost::function<void(const std::string& jid)> cb)
{
    m_new_peer_cb = cb;
}

void 
jbot::clear_msg_received_callback()
{
    m_msg_received_callback = 0;
}

json_spirit::Array
jbot::get_roster()
{
    using namespace json_spirit;
    Array a;
    Roster * roster = j->rosterManager()->roster();
    typedef std::pair<const std::string, RosterItem*> pair_t;
    typedef std::pair<std::string, Resource*> rp_t;
    BOOST_FOREACH( pair_t ri, *roster )
    {
        if( ri.second->subscription() != S10nBoth ) continue;
        json_spirit::Object o;
        o.push_back( Pair("jid", ri.first) );
        //o.push_back( Pair("jid_bare", ri.second->jid()) );
        o.push_back( Pair("name", ri.second->name()) );
        o.push_back( Pair("online",  ri.second->online()) );
        json_spirit::Object oresources;
        // Resources for this jid:
        //ResourceMap rm = ri.second->resources();
        JID jid( ri.first );
        std::map<std::string, Resource*> rm = ri.second->resources();
        BOOST_FOREACH( rp_t rr, rm )
        {
            jid.setResource( rr.first );
            json_spirit::Object ores;
            ores.push_back( Pair("jid_full", jid.full()) );
            bool cap = false;
            {
                boost::mutex::scoped_lock lk(m_playdarpeers_mut);
                cap = m_playdarpeers.count(jid.full()) == 1;
            }
            ores.push_back( Pair("playdar-capable", cap) );
            bool cond = (bool)(m_greynet->router()->get_connection_by_name(jid.full()));
            ores.push_back( Pair("playdar-connected", cond) );
            ores.push_back( Pair("priority", rr.second->priority()) );
            ores.push_back( Pair("message", rr.second->message()) );
            string presence = "";
            if(m_presences.find(rr.second->presence()) != m_presences.end())
                presence = m_presences[rr.second->presence()];
            ores.push_back( Pair("presence", presence) );
            // extensions:
            StanzaExtensionList sexlist = rr.second->extensions();
            Array sexArray;
            BOOST_FOREACH( const StanzaExtension* sex, sexlist )
            {
                sexArray.push_back( sex->tag()->xml() );
            }
            
            ores.push_back( Pair("extensions", sexArray) );
            oresources.push_back( Pair( rr.first, ores ) );
        }
        
        o.push_back( Pair("resources", oresources) );
        a.push_back( o );
    }
    return a;
}

/// GLOOXY CALLBACKS FOLLOW

void 
jbot::onConnect()
{
    
    // update jid resource, servers like gtalk use resource binding and may
    // have changed our requested /resource
    JID jid( m_jid );
    jid.setResource( j->resource() );
    m_jid = jid.full();
    printf( "connected as: %s\n", m_jid.c_str() );
}

void 
jbot::onDisconnect( ConnectionError e )
{
    printf( "jbot: disconnected: %d\n", e );
    if ( e == ConnAuthenticationFailed )
        printf( "auth failed. reason: %d\n", j->authError() );
}

bool 
jbot::onTLSConnect( const CertInfo& info )
{
    printf( "status: %d\nissuer: %s\npeer: %s\nprotocol: %s\nmac: %s\ncipher: %s\ncompression: %s\n"
            "from: %s\nto: %s\n",
            info.status, info.issuer.c_str(), info.server.c_str(),
            info.protocol.c_str(), info.mac.c_str(), info.cipher.c_str(),
            info.compression.c_str(), ctime( (const time_t*)&info.date_from ),
            ctime( (const time_t*)&info.date_to ) );
    onConnect();
    return true;
}

void 
jbot::handleMessage( const Message& msg, MessageSession * /*session*/ )
{
    printf( "from: %s, type: %d, subject: %s, message: %s, thread id: %s\n",
            msg.from().full().c_str(), msg.subtype(),
            msg.subject().c_str(), msg.body().c_str(), msg.thread().c_str() );

    if( msg.from().bare() == JID(m_jid).bare() )
    {
        printf("Message is from ourselves/considered safe\n");
        //TODO admin interface using text commands? stats?
        if ( msg.body() == "quit" ) { stop(); return; }
    }
    
    if( m_msg_received_callback )
    {
        m_msg_received_callback( msg.body(), msg.from().full() );
    }

/*
    std::string re = "You said:\n> " + msg.body() + "\nI like that statement.";
    std::string sub;
    if ( !msg.subject().empty() )
        sub = "Re: " +  msg.subject();

    m_messageEventFilter->raiseMessageEvent( MessageEventDisplayed );
#if defined( WIN32 ) || defined( _WIN32 )
    Sleep( 1000 );
#else
    sleep( 1 );
#endif
    m_messageEventFilter->raiseMessageEvent( MessageEventComposing );
    m_chatStateFilter->setChatState( ChatStateComposing );
#if defined( WIN32 ) || defined( _WIN32 )
    Sleep( 2000 );
#else
    sleep( 2 );
#endif
    m_session->send( re, sub );

    if ( msg.body() == "quit" )
        j->disconnect();
*/
}

void 
jbot::handleLog( LogLevel level, LogArea area, const std::string& message )
{
    printf("log: level: %d, area: %d, %s\n", level, area, message.c_str() );
}

/// ROSTER STUFF
// {{{
void 
jbot::onResourceBindError( ResourceBindError error )
{
    printf( "onResourceBindError: %d\n", error );
}

void 
jbot::onSessionCreateError( SessionCreateError error )
{
    printf( "onSessionCreateError: %d\n", error );
}

void 
jbot::handleItemSubscribed( const JID& jid )
{
    printf( "subscribed %s\n", jid.bare().c_str() );
}

void 
jbot::handleItemAdded( const JID& jid )
{
    printf( "added %s\n", jid.bare().c_str() );
}

void 
jbot::handleItemUnsubscribed( const JID& jid )
{
    printf( "unsubscribed %s\n", jid.bare().c_str() );
}

void 
jbot::handleItemRemoved( const JID& jid )
{
    printf( "removed %s\n", jid.bare().c_str() );
}

void 
jbot::handleItemUpdated( const JID& jid )
{
    printf( "updated %s\n", jid.bare().c_str() );
}
// }}}
void 
jbot::handleRoster( const Roster& roster )
{
    printf( "roster arriving: \n" );
    Roster::const_iterator it = roster.begin();
    for ( ; it != roster.end(); ++it )
    {
        if ( (*it).second->subscription() != S10nBoth ) continue;
        printf("JID: %s\n", (*it).second->jid().c_str());
        
        /*
        // only disco after getting presence:
        JID jid = (*it).second->jid();
        // disco query all jid's resources:
        RosterItem::ResourceMap::const_iterator rit;
        for ( rit = (*it).second->resources().begin() ; 
              rit != (*it).second->resources().end(); 
              ++rit )
        {
            jid.setResource((*rit).first);
            printf( "* DISCO: %s \n", jid.full().c_str() );
            j->disco()->getDiscoInfo( jid, "", this, 0 );
        }
        */
        /*
        printf( "jid: %s, name: %s, subscription: %d\n",
                (*it).second->jid().c_str(), (*it).second->name().c_str(),
                (*it).second->subscription() );
        */
        
        //StringList g = (*it).second->groups();
        //StringList::const_iterator it_g = g.begin();
        //for ( ; it_g != g.end(); ++it_g )
        //    printf( "\tgroup: %s\n", (*it_g).c_str() );
        
    }
    printf("\n");
}

void 
jbot::handleRosterError( const IQ& /*iq*/ )
{
    printf( "a roster-related error occured\n" );
}

void 
jbot::handleRosterPresence( const RosterItem& item, const std::string& resource,
        Presence::PresenceType presence, const std::string& /*msg*/ )
{
    // does this presence mean they are offline/un-queryable:
    if ( presence == Presence::Unavailable ||
            presence == Presence::Error ||
            presence == Presence::Invalid )
    {
        printf( "//////presence received (->OFFLINE): %s/%s\n", item.jid().c_str(), resource.c_str() );
        // remove peer from list, if he exists
        boost::mutex::scoped_lock lk(m_playdarpeers_mut);
        if( m_playdarpeers.erase(item.jid()) )
        {
            printf("Removed %s from playdarpeers\n", item.jid().c_str());
        }
        return;
    }
    // If they just became available, disco them, otherwise ignore - they may
    // have just changed their status to Away or something asinine.
    {
        boost::mutex::scoped_lock lk(m_playdarpeers_mut);
        if( m_playdarpeers.find(item.jid()) != m_playdarpeers.end() )
        {
            return; // already online and in our list.
        }
    }
    printf( "//////presence received (->ONLINE) %s/%s -- %d\n",
     item.jid().c_str(),resource.c_str(), presence );
     
    // Disco them to check if they are playdar-capable
    JID jid( item.jid() );
    jid.setResource( resource );
    //printf( "DISCOing: %s \n", jid.full().c_str() );
    j->disco()->getDiscoInfo( jid, "", this, 0 );
}

void 
jbot::handleSelfPresence( const RosterItem& item, const std::string& resource,
                                       Presence::PresenceType presence, const std::string& msg )
{
    printf( "self presence received: %s/%s -- %d\n", item.jid().c_str(), resource.c_str(), presence );
    handleRosterPresence( item, resource, presence, msg );
}

bool 
jbot::handleSubscriptionRequest( const JID& jid, const std::string& /*msg*/ )
{
    printf( "subscription: %s\n", jid.bare().c_str() );
    StringList groups;
    JID id( jid );
    j->rosterManager()->subscribe( id, "", groups, "" );
    return true;
}

bool 
jbot::handleUnsubscriptionRequest( const JID& jid, const std::string& /*msg*/ )
{
    printf( "unsubscription: %s\n", jid.bare().c_str() );
    return true;
}

void 
jbot::handleNonrosterPresence( const Presence& presence )
{
    printf( "received presence from entity not in the roster: %s\n", presence.from().full().c_str() );
}
/// END ROSTER STUFF


/// DISCO STUFF
void 
jbot::handleDiscoInfo( const JID& from, const Disco::Info& info, int context)
{
    if ( info.hasFeature("playdar:resolver") 
         || from.resource().find( "playdar") != string::npos ) 
         // /resource HACK: gtalk filters unrecognised disco features :( 
    {
        boost::mutex::scoped_lock lk(m_playdarpeers_mut);
        if( m_playdarpeers.count(from.full()) == 0 )
        {
            printf("DISCO-response '%s' playdar = YES\n", from.full().c_str());
            printf("Adding '%s' to playdarpeers\n", from.full().c_str());
            m_playdarpeers.insert( from.full() );
            if( m_new_peer_cb ) m_new_peer_cb( from.full() );
        }else{
            printf("DISCO-response '%s' playdar = YES, already found\n", from.full().c_str());
        }
        
    }
    else
    {
        printf("DISCO-response '%s' playdar = NO\n", from.full().c_str());
    }
}

void 
jbot::handleDiscoItems( const JID& /*iq*/, const Disco::Items&, int /*context*/ )
{
    printf( "handleDiscoItemsResult\n" );
}

void 
jbot::handleDiscoError( const JID& j, const Error* e, int /*context*/ )
{
    printf( "handleDiscoError for jid: %s reason: %s\n", j.full().c_str(), e->text().c_str() );
}
/// END DISCO STUFF

