#ifndef __RS_GREYNET_H__
#define __RS_GREYNET_H__

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/thread.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <map>

#include "libf2f/router.h"
#include "libf2f/protocol.h"


#include "greynet_messages.hpp"
#include "playdar/playdar_plugin_include.h"
#include "ss_greynet.h"

#include "jbot.h"

#include "portfwd/portfwd.h"

namespace playdar { 
namespace resolvers { 

class ss_greynet;

class greynet 
:   public ResolverPlugin<greynet>, 
    public libf2f::Protocol
{
public:

    virtual bool init(pa_ptr pap);
    void connect_to_peer(const std::string& remote_ip, unsigned short remote_port);
    void connect_to_peer(const std::string& remote_ip, unsigned short remote_port, std::map<std::string,std::string> props);
    
    /// jabber stuff
    void jabber_start(const std::string& jid, const std::string& pass, const std::string& server, unsigned short port);
    void jabber_msg_received(const std::string& msg, const std::string& jid);
    void jabber_new_peer(const std::string& jid);
    /// end jabber stuff
   
    virtual std::string name() const { return "greynet"; }

    virtual void start_resolving(boost::shared_ptr<ResolverQuery> rq);
    
    /// max time in milliseconds we'd expect to have results in.
    virtual unsigned int target_time() const
    {
        return 3000;
    }
    
    /// highest weighted resolverservices are queried first.
    virtual unsigned short weight() const
    {
        return 51;
    }
    
    bool new_incoming_connection( libf2f::connection_ptr conn );
    void new_outgoing_connection( libf2f::connection_ptr conn );
    void new_connection_watchdog( libf2f::connection_ptr conn, size_t timeout );
    void new_connection_timeout(const boost::system::error_code& e,
                            libf2f::connection_ptr conn,
                            boost::shared_ptr<boost::asio::deadline_timer> t);
    void expect_ident( libf2f::message_ptr msgp, libf2f::connection_ptr conn, bool incoming );
    void send_ident( libf2f::connection_ptr conn );
    void connection_terminated(libf2f::connection_ptr conn);
    void message_received( libf2f::message_ptr msgp, libf2f::connection_ptr conn );
    void handle_query(libf2f::connection_ptr conn, libf2f::message_ptr msgp);
    void fwd_search(const boost::system::error_code& e,
                     libf2f::connection_ptr conn, libf2f::message_ptr msgp,
                     boost::shared_ptr<boost::asio::deadline_timer> t,
                     query_uid qid);
    void send_response( query_uid qid, boost::shared_ptr<ResolvedItem> rip);
    void handle_queryresult(libf2f::connection_ptr conn, libf2f::message_ptr msgp);
    bool handle_sidrequest(libf2f::connection_ptr conn, libf2f::message_ptr msg);
    bool handle_siddata(libf2f::connection_ptr conn, libf2f::message_ptr msg);
    void handle_sidheaders(libf2f::connection_ptr conn, libf2f::message_ptr msgp);
    void handle_sidfail(libf2f::connection_ptr conn, libf2f::message_ptr msgp);
    void handle_querystop(libf2f::connection_ptr conn, libf2f::message_ptr msgp);

    void start_sidrequest(libf2f::connection_ptr conn, source_uid sid, boost::shared_ptr<ss_greynet> ss);
    
    void unregister_sidtransfer( libf2f::connection_ptr conn, const source_uid &sid );
    void register_sidtransfer( libf2f::connection_ptr conn, const source_uid &sid, boost::shared_ptr<ss_greynet> ss );
    
    void set_query_origin(query_uid qid, libf2f::connection_ptr conn)
    {
        assert( m_qidorigins.find(qid) == m_qidorigins.end() );
        try
        {
            m_qidorigins[qid] = libf2f::connection_ptr_weak(conn);
        }
        catch(...){}
    }
    
    libf2f::connection_ptr get_conn( const std::string & name );
    
    virtual std::map< std::string, boost::function<ss_ptr(std::string)> >
    get_ss_factories();
    
    
    libf2f::connection_ptr get_query_origin(query_uid qid)
    {
        libf2f::connection_ptr conn;
        if(m_qidorigins.find(qid) == m_qidorigins.end())
        {
            // not found
            return conn;
        }
        try
        {
            libf2f::connection_ptr_weak connw = m_qidorigins[qid];
            return libf2f::connection_ptr(connw);
        }catch(...)
        { return conn; }
    }
    
    bool anon_http_handler(const playdar_request&, playdar_response&, playdar::auth&);
    bool authed_http_handler(const playdar_request& req, playdar_response& resp, playdar::auth& pauth);

    boost::shared_ptr<boost::asio::io_service> get_io_service() const
    {
        return m_io_service;
    }
    
    libf2f::Router * router() { return m_router; }
protected:
    virtual ~greynet() throw();
    
private:

    void detect_ip();
    const std::string& public_ip() const { return m_publicip; }

    libf2f::Router * m_router;
    unsigned short m_port;
    pa_ptr m_pap;

    boost::shared_ptr<jbot> m_jbot;
    boost::shared_ptr<boost::thread> m_jbot_thread;
    std::map<std::string, std::string> m_peer_cookies; // cookie->jid

    boost::shared_ptr<Portfwd> m_pf;

    boost::thread_group m_threads;
    boost::shared_ptr<boost::asio::io_service> m_io_service;

    // source of queries, so we know how to reply.
    std::map< query_uid, libf2f::connection_ptr_weak > m_qidorigins;
    
    // soure id to streaming strats
    std::map< source_uid,  boost::shared_ptr<ss_greynet> > m_sid2ss;
    
    // so we can reject all GUIDs we've already seen (dupe msgs)
    std::set< std::string > m_seen_guids;
    
    // track connection -> sids that are actively being transferred so that
    // if a connection dies, we can cancel the sid transfers immediately
    std::multimap< libf2f::connection_ptr, source_uid > m_conn2sid;
    
    // do we have a public internet IP for incoming connections?
    std::string m_publicip;
    
    
};

}} //namespaces

#endif
