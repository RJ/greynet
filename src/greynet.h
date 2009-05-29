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

#include "playdar/playdar_plugin_include.h"

#include "libf2f/router.h"
#include "libf2f/protocol.h"

using namespace libf2f; // pff

namespace playdar { 
namespace resolvers { 

class greynet : public ResolverPlugin<greynet>, public libf2f::Protocol
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
    bool new_outgoing_connection( connection_ptr conn, boost::asio::ip::tcp::endpoint &endpoint );
    void expect_ident( message_ptr msgp, connection_ptr conn, bool incoming );
    void connection_terminated(connection_ptr conn);
    void message_received( message_ptr msgp, connection_ptr conn );
    void handle_query(connection_ptr conn, message_ptr msgp);
    void fwd_search(const boost::system::error_code& e,
                     connection_ptr conn, message_ptr msgp,
                     boost::shared_ptr<boost::asio::deadline_timer> t,
                     query_uid qid);
    void send_response( query_uid qid, boost::shared_ptr<ResolvedItem> rip);
    void handle_queryresult(connection_ptr conn, message_ptr msgp);
    
    void set_query_origin(query_uid qid, connection_ptr conn)
    {
        assert( m_qidorigins.find(qid) == m_qidorigins.end() );
        try
        {
            m_qidorigins[qid] = connection_ptr_weak(conn);
        }
        catch(...){}
    }
    /*
    virtual std::map< std::string, boost::function<ss_ptr(std::string)> >
    get_ss_factories()
    {
        // return our darknet ss
        std::map< std::string, boost::function<ss_ptr(std::string)> > facts;
        facts[ "darknet" ] = boost::bind( &DarknetStreamingStrategy::factory, _1, this );
        return facts;
    }
    */
    
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
                           
protected:
    virtual ~greynet() throw();
    
private:

    Router * m_router;

    pa_ptr m_pap;

    boost::thread_group m_threads;
    boost::shared_ptr<boost::asio::io_service> m_io_service;

    // source of queries, so we know how to reply.
    std::map< query_uid, connection_ptr_weak > m_qidorigins;
    std::map< source_uid,  boost::function<bool (const char * payload, size_t len)> > m_sidhandlers;
    
};

}} //namespaces

#endif
