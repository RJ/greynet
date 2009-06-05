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

using namespace libf2f; // pff

namespace playdar { 
namespace resolvers { 

class ss_greynet;

class greynet 
:   public ResolverPlugin<greynet>, 
    public libf2f::Protocol
{
public:

    virtual bool init(pa_ptr pap);
   
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
    
    bool new_incoming_connection( connection_ptr conn );
    void new_outgoing_connection( connection_ptr conn );
    void expect_ident( message_ptr msgp, connection_ptr conn, bool incoming );
    void send_ident( connection_ptr conn );
    void connection_terminated(connection_ptr conn);
    void message_received( message_ptr msgp, connection_ptr conn );
    void handle_query(connection_ptr conn, message_ptr msgp);
    void fwd_search(const boost::system::error_code& e,
                     connection_ptr conn, message_ptr msgp,
                     boost::shared_ptr<boost::asio::deadline_timer> t,
                     query_uid qid);
    void send_response( query_uid qid, boost::shared_ptr<ResolvedItem> rip);
    void handle_queryresult(connection_ptr conn, message_ptr msgp);
    bool handle_sidrequest(connection_ptr conn, message_ptr msg);
    bool handle_siddata(connection_ptr conn, message_ptr msg);
    void handle_sidheaders(connection_ptr conn, message_ptr msgp);
    void handle_sidfail(connection_ptr conn, message_ptr msgp);
    
    void start_sidrequest(connection_ptr conn, source_uid sid, boost::shared_ptr<ss_greynet> ss);
    
    void unregister_sidtransfer( connection_ptr conn, const source_uid &sid );
    void register_sidtransfer( connection_ptr conn, const source_uid &sid );
    
    void set_query_origin(query_uid qid, connection_ptr conn)
    {
        assert( m_qidorigins.find(qid) == m_qidorigins.end() );
        try
        {
            m_qidorigins[qid] = connection_ptr_weak(conn);
        }
        catch(...){}
    }
    
    connection_ptr get_conn( const std::string & name );
    
    virtual std::map< std::string, boost::function<ss_ptr(std::string)> >
    get_ss_factories();
    
    
    connection_ptr get_query_origin(query_uid qid)
    {
        connection_ptr conn;
        if(m_qidorigins.find(qid) == m_qidorigins.end())
        {
            // not found
            return conn;
        }
        try
        {
            connection_ptr_weak connw = m_qidorigins[qid];
            return connection_ptr(connw);
        }catch(...)
        { return conn; }
    }
    
    bool anon_http_handler(const playdar_request&, playdar_response&, playdar::auth&);
                           
    boost::shared_ptr<boost::asio::io_service> get_io_service() const
    {
        return m_io_service;
    }
protected:
    virtual ~greynet() throw();
    
private:

    Router * m_router;

    pa_ptr m_pap;

    boost::thread_group m_threads;
    boost::shared_ptr<boost::asio::io_service> m_io_service;

    // source of queries, so we know how to reply.
    std::map< query_uid, connection_ptr_weak > m_qidorigins;
    
    std::map< source_uid,  boost::shared_ptr<ss_greynet> > m_sid2ss;
    
    std::set< std::string > m_seen_guids;
    
    // track connection -> sids that are actively being transferred so that
    // if a connection dies, we can cancel the sid transfers immediately
    std::multimap< connection_ptr, source_uid > m_conn2sid;
    
};

}} //namespaces

#endif
