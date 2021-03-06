#ifndef _JBOT_H_
#define _JBOT_H_

#include <gloox/client.h>
#include <gloox/messagesessionhandler.h>
#include <gloox/messagehandler.h>
#include <gloox/messageeventhandler.h>
#include <gloox/messageeventfilter.h>
#include <gloox/chatstatehandler.h>
#include <gloox/chatstatefilter.h>
#include <gloox/connectionlistener.h>
#include <gloox/disco.h>
#include <gloox/message.h>
#include <gloox/discohandler.h>
#include <gloox/stanza.h>
#include <gloox/gloox.h>
#include <gloox/lastactivity.h>
#include <gloox/loghandler.h>
#include <gloox/logsink.h>
#include <gloox/connectiontcpclient.h>
#include <gloox/connectionsocks5proxy.h>
#include <gloox/connectionhttpproxy.h>
#include <gloox/messagehandler.h>
#include <gloox/rostermanager.h>
#include <gloox/siprofileft.h>
#include <gloox/siprofilefthandler.h>
#include <gloox/bytestreamdatahandler.h>
#include <gloox/error.h>
#include <gloox/presence.h>
#include <gloox/rosteritem.h>

#include <boost/function.hpp>
#include <boost/thread/mutex.hpp>

#include "json_spirit/json_spirit.h"

#ifndef _WIN32
# include <unistd.h>
#endif

#include <stdio.h>
#include <locale.h>
#include <string>
#include <vector>
#include <set>

#if defined( WIN32 ) || defined( _WIN32 )
# include <windows.h>
#endif
/// Basic jabber bot using gloox, that detects other nodes that have 
/// playdar:resolver capabilities using disco and makes it easy to msg them all.
/// This class doesn't know playdar (except declaring an XMPP feature called "playdar:resolver") and playdar doesnt know any gloox objects. just json and
/// strings pass back and forth

namespace playdar{ namespace resolvers{ 
class greynet;
} }


class jbot 
 :  public gloox::RosterListener, 
    public gloox::DiscoHandler,
    gloox::MessageHandler, 
    gloox::ConnectionListener, 
    gloox::LogHandler
{
  public:
    jbot(std::string jid, std::string pass, std::string server, unsigned short port, playdar::resolvers::greynet* g);
    virtual ~jbot() {}
    
    /// Our bot api used in the resolver (note, no gloox types passed in/out)
    void start(const std::string& loglevel);
    void stop();
    void send_to( const std::string& to, const std::string& msg );
    void broadcast_msg( const std::string& msg );
    void set_msg_received_callback( boost::function<void(const std::string&, const std::string&)> cb); 
    void set_new_peer_callback( boost::function<void(const std::string& jid)> cb);
    void clear_msg_received_callback();
    const std::string& jid() const { return m_jid; }
    json_spirit::Array get_roster();

    /// GLOOX IMPLEMENTATION STUFF FOLLOWS
    
    virtual void onConnect();
    virtual void onDisconnect( gloox::ConnectionError e );
    virtual bool onTLSConnect( const gloox::CertInfo& info );
    
    virtual void handleMessage( const gloox::Message& msg, gloox::MessageSession * /*session*/ );
    virtual void handleLog( gloox::LogLevel level, gloox::LogArea area, const std::string& message );
    
    /// ROSTER STUFF
    virtual void onResourceBindError( gloox::ResourceBindError error );
    virtual void onSessionCreateError( gloox::SessionCreateError error );
    
    virtual void handleItemSubscribed( const gloox::JID& jid );
    virtual void handleItemAdded( const gloox::JID& jid );
    virtual void handleItemUnsubscribed( const gloox::JID& jid );
    virtual void handleItemRemoved( const gloox::JID& jid );
    virtual void handleItemUpdated( const gloox::JID& jid );
    
    virtual void handleRoster( const gloox::Roster& roster );
    virtual void handleRosterError( const gloox::IQ& /*iq*/ );
    virtual void handleRosterPresence( const gloox::RosterItem& item, const std::string& resource,
                                       gloox::Presence::PresenceType presence, const std::string& /*msg*/ );
    virtual void handleSelfPresence( const gloox::RosterItem& item, const std::string& resource,
                                       gloox::Presence::PresenceType presence, const std::string& msg );
    virtual bool handleSubscriptionRequest( const gloox::JID& jid, const std::string& /*msg*/ );
    virtual bool handleUnsubscriptionRequest( const gloox::JID& jid, const std::string& /*msg*/ );
    virtual void handleNonrosterPresence( const gloox::Presence& presence );
    /// END ROSTER STUFF
    
    /// DISCO STUFF
    virtual void handleDiscoInfo( const gloox::JID& from, const gloox::Disco::Info& info, int context);
    virtual void handleDiscoItems( const gloox::JID& /*iq*/, const gloox::Disco::Items&, int /*context*/ );
    virtual void handleDiscoError( const gloox::JID& /*iq*/, const gloox::Error*, int /*context*/ );
    /// END DISCO STUFF

  private:
    
    gloox::Client *j;
    std::string m_jid, m_pass, m_server;
    unsigned short m_port;
    // presence type to string helper:
    std::map< gloox::Presence::PresenceType , std::string > m_presences;
    
    // who is online with playdar capabilities:
    std::set<std::string> m_playdarpeers;
    boost::mutex m_playdarpeers_mut;
    
    boost::function<void(const std::string&, const std::string&)> m_msg_received_callback;
    boost::function<void(const std::string& jid)> m_new_peer_cb;

    playdar::resolvers::greynet* m_greynet;
};
#endif
