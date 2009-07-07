#include "greynet.h"

#include <iostream>
#include <boost/lexical_cast.hpp>          
#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string.hpp> 
#include <boost/foreach.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <cassert>
#include <boost/lexical_cast.hpp>
#include <cassert>

#include "playdar/resolver_query.hpp"
#include "playdar/playdar_request.h"
#include "playdar/auth.h"

// default port for f2f mesh
#define GREYNET_PORT 60210

using namespace std;
using namespace json_spirit;
using namespace libf2f;

namespace playdar {
namespace resolvers {

bool greynet::init(pa_ptr pap)
{
    m_pap = pap;

    unsigned short port = pap->get("plugins.greynet.port", GREYNET_PORT);

    m_io_service = boost::shared_ptr<boost::asio::io_service>
                   (new boost::asio::io_service);
    boost::shared_ptr<boost::asio::ip::tcp::acceptor> accp(
        new boost::asio::ip::tcp::acceptor(
            *m_io_service, 
            boost::asio::ip::tcp::endpoint( boost::asio::ip::tcp::v4(), port) 
            )
    );
    
    m_router = new Router( accp, this, boost::bind(&PluginAdaptor::gen_uuid, m_pap) );
    
    // start io_services:
    cout << "Greynet router coming online on port " <<  port <<endl;
    cout << "Greynet build date: " << __DATE__ << " " << __TIME__ << endl;
    
    for (std::size_t i = 0; i < 1; ++i)
    {
        m_threads.create_thread( boost::bind(
            &boost::asio::io_service::run, m_io_service.get()));
    }
 
    // get peers: TODO support multiple/list from config
    string remote_ip = m_pap->get<string>("plugins.greynet.peerip","");
    if(remote_ip!="")
    {
        connect_to_peer( remote_ip,
                         m_pap->get<int>
                            ("plugins.greynet.peerport",GREYNET_PORT) 
                       );
    }
    
    jabber_start(   m_pap->get<string>("plugins.greynet.jid",""),
                    m_pap->get<string>("plugins.greynet.password","") );
    
    return true;
}

greynet::~greynet() throw()
{
    cout << "DTOR greynet" << endl;
    m_jbot->stop();
    m_jbot_thread->join();
    
    m_router->stop();
    m_io_service->stop();
    m_threads.join_all();
}

void
greynet::connect_to_peer(const string& remote_ip, unsigned short remote_port)
{
    map<string,string> props;
    connect_to_peer( remote_ip, remote_port, props );
}

void
greynet::connect_to_peer(const string& remote_ip, unsigned short remote_port, map<string,string> props)
{
    cout << "Attempting peer connect: " 
            << remote_ip << ":" << remote_port << endl;
    boost::asio::ip::address_v4 ipaddr =
        boost::asio::ip::address_v4::from_string(remote_ip);
    boost::asio::ip::tcp::endpoint ep(ipaddr, remote_port);
    m_router->connect_to_remote(ep, props);
}

/// begin jaber stuff
void
greynet::jabber_start(const string& jid, const string& pass)
{
    if( jid.empty() || pass.empty() )
    {
        cerr << "no jid+pass specified, jabber bot not starting.\n" << endl;
        return;
    }
    // start xmpp connection in new thread:
    m_jbot = boost::shared_ptr<jbot>(new jbot(jid, pass));
    m_jbot->set_msg_received_callback( boost::bind(&greynet::jabber_msg_received, this, _1, _2) );
    m_jbot->set_new_peer_callback( boost::bind(&greynet::jabber_new_peer, this, _1) );
    m_jbot_thread = boost::shared_ptr<boost::thread>
                        (new boost::thread(boost::bind(&jbot::start, m_jbot)));

}

void
greynet::jabber_msg_received(const string& msg, const string& jid)
{
    cout << "Msg from " << jid << " = " << msg << endl;
    // is this a new peer announcement?
    using namespace json_spirit;
    Value mv;
    if( !read(msg, mv) || mv.type() != obj_type )
    {
        cout << "Dropping msg, not valid json" << endl;
        return;
    }
    Object o = mv.get_obj();
    map<string,Value> m;
    obj_to_map(o,m);
    // TODO better validation here:
    if( m.find("playdar-greynet") == m.end() ||
        m.find("peer_ip") == m.end() ||
        m.find("cookie") == m.end() )
    {
        cout << "Missing fields, invalid." << endl;
        return;
    }
    string peerip = m["peer_ip"].get_str();
    int peerport = m["peer_port"].get_int();
    cout << "Jabber msg means we'll connect to: " << peerip << ":" << peerport << endl;
    map<string,string> props;
    props["cookie"] = m["cookie"].get_str();
    props["jid"] = jid;
    connect_to_peer( peerip, peerport, props );
}

void
greynet::jabber_new_peer(const string& jid)
{
    cout << "New jabber peer reported: " << jid << endl;
    if( jid == m_pap->get<string>("plugins.greynet.jid","") )
    {
        cout << "self" << endl;
        //return;
    }
    // tell them our ip/port
    string cookie = m_pap->gen_uuid();
    m_peer_cookies[cookie] = jid;
    using namespace json_spirit;
    Object o;
    o.push_back( Pair("playdar-greynet", "0.1") );
    o.push_back( Pair("peer_ip", m_pap->get<string>("plugins.greynet.ip","")) );
    o.push_back( Pair("peer_port", GREYNET_PORT) );
    o.push_back( Pair("cookie", cookie) );
    ostringstream os;
    write( o, os );
    m_jbot->send_to( jid, os.str() );
}

/// end jabber stuff

void
greynet::start_resolving(boost::shared_ptr<ResolverQuery> rq)
{
    cout << "greynet::start_resolving..." << endl;
    message_ptr msg( new QueryMessage(rq, m_router->gen_uuid()) );
    connection_ptr qorigin = get_query_origin( rq->id() );
    m_router->foreach_conns_except( boost::bind(&Connection::async_write, _1, msg), qorigin );
}

bool 
greynet::new_incoming_connection( connection_ptr conn )
{
    cout << "greynet::new_incoming_connection " << conn->str() << endl;
    // first thing to expect is an ident msg, so set the msg handler to one 
    // that expects it, and kills the connection otherwise.
    conn->push_message_received_cb( 
        boost::bind( &greynet::expect_ident, this, _1, _2, true ) );
    return true;
}

void 
greynet::new_outgoing_connection( connection_ptr conn )
{
    cout << "greynet::new_outgoing_connection " << conn->str() 
         << " jid: " << conn->get("jid") << endl;
    conn->push_message_received_cb( 
        boost::bind( &greynet::expect_ident, this, _1, _2, false) );
    send_ident( conn );
}

/// inserted as msg handler for new connection
void
greynet::expect_ident( message_ptr msgp, connection_ptr conn, bool incoming )
{
    if( msgp->type() != IDENT )
    {
        cout << "Expected ident from peer, but didn't get it. Disconnecting." 
             << endl;
        conn->fin();
        return;
    }
    cout << "Got IDENT from new connection: " 
         << msgp->payload_str() << endl;
    
    // now parse/validate the ident msg. TODO move to IdentMsg class?
    using namespace json_spirit;
    Value v;
    if( !read( msgp->payload_str(), v ) || v.type() != obj_type )
    {
        cout << "Invalid ident/auth from incoming connection. closing." << endl;
        conn->fin();
        return;
    }
    Object o = v.get_obj();
    map<string,Value> m;
    obj_to_map(o,m);
    if( m.find("name") == m.end() || m["name"].type() != str_type )
    {
        cout << "Invalid IDENT msg, goodbye." << endl;
        conn->fin();
        return;
    }
    const string name = m["name"].get_str();
    
    // if the other end initiated the connection, we validate their IDENT, then
    // send them our IDENT message:
    if( incoming )
    {
        cout << "New incoming connection from " << name << endl;
        if( m.find("cookie") == m.end() || m["cookie"].type() != str_type )
        {
            cout << "IDENT missing cookie, goodbye" << endl;
            conn->fin();
            return;
        }
        const string cookie = m["cookie"].get_str();
        if( cookie == "" 
            || m_peer_cookies.find(cookie) == m_peer_cookies.end()
            || m_peer_cookies[cookie] != name )
        {
            cout << "IDENT cookie or name mismatch, goodbye" << endl;
            conn->fin();
            return;
        }
        // they IDENTed correctly, send them our IDENT:
        cout << "IDENT cookie verified for " << name << endl;
        send_ident( conn );
    }
    else
    {
        cout << "New outgoing connection setup to " << name << endl;
    }
    
    // now the connection is considered ready to handle normal messages:
    conn->set_ready( true );
    conn->set_name( name );
    
    // remove our custom msg rcvd callback:
    conn->pop_message_received_cb();
    
    cout << "Connection ready to rock: " << conn->str() << endl;
}

void
greynet::send_ident( connection_ptr conn )
{
    cout << "Sending our IDENT.." << endl;
    using namespace json_spirit;
    Object o;
    o.push_back( Pair("name", m_pap->get<string>("plugins.greynet.jid","")) );
    if( conn->get("cookie") != "" ) o.push_back( Pair("cookie", conn->get("cookie")) );
    ostringstream os;
    write( o, os );
    message_ptr msg( new IdentMessage( os.str(), m_router->gen_uuid() ) );
    conn->async_write( msg );
}

void
greynet::connection_terminated(connection_ptr conn)
{
    cout << "Connection terminated: " << conn->str() << endl;
    // clear and cancel any active transfers on this connection.
    // gather a list of active sids first, then cancel them.
    // the cancel callback on the SS will unregister them with us.
    vector<source_uid> sids;
    multimap< connection_ptr, source_uid >::iterator it;
    for ( it=m_conn2sid.find(conn) ; it != m_conn2sid.end(); it++ )
        sids.push_back( (*it).second );
        
    if( sids.size() == 0 )
    {
        cout << "No active transfers on this connection." << endl;
        return;
    }
    cout << "Cancelling " << sids.size() << " active transfers." << endl;
    BOOST_FOREACH( source_uid s, sids )
    {
        cout << "* Cancelling transfer: " << s << endl;
        m_sid2ss[s]->cancel_handler();
    }
}

/// currently the ss_greynet calls this when transfer is over or cancelled.
void
greynet::unregister_sidtransfer( connection_ptr conn, const source_uid &sid )
{
    cout << "greynet::unregister_sidtransfer" << endl;
    multimap< connection_ptr, source_uid >::iterator it;
    while( (it = m_conn2sid.find(conn)) != m_conn2sid.end() )
    {
        if( (*it).second == sid )
        {        
            m_conn2sid.erase( it );
            return;
        } 
    }
}

void
greynet::register_sidtransfer( connection_ptr conn, const source_uid &sid )
{
    cout << "greynet::register_sidtransfer" << endl;
    // TODO assert it's not already registered?
    m_conn2sid.insert( pair<connection_ptr, source_uid>(conn,sid) );
}

/// Only called once auth/ident has completed
void
greynet::message_received( message_ptr msgp, connection_ptr conn )
{
    cout << "greynet::message_received from " << conn->str() 
         << " " << msgp->str() << endl;
             
    // ignore if the msg guid is a dupe
    // but allow dupes for streams (all msgs of one stream have same sid)
    switch( msgp->type() )
    {
        case SIDDATA:
        case SIDFAIL:
        case SIDHEADERS:
            break;
            
        default:
            if( m_seen_guids.count( msgp->guid() ) )
            {
                cout << "Dropping msg, dupe guid." << endl;
                return;
            }
            m_seen_guids.insert( msgp->guid() );
    }

    switch(msgp->type())
    {
        case PING:
            cout << "ponging." << endl;
            conn->async_write( message_ptr(new PongMessage(m_router->gen_uuid())) );
            break;
        case PONG:
            cout << "Got pong!" << endl;
            break;
            
        case QUERY:
            handle_query(conn,msgp);
            break;
            
        case QUERYRESULT:
            handle_queryresult(conn,msgp);
            break;
        
        case QUERYCANCEL:
            //handle_querycancel(conn,msgp);
            cout << "querycancel is not currently handled" << endl; //TODO
            break;
            
        case SIDREQUEST:
            handle_sidrequest(conn, msgp);
            break;
            
        case SIDDATA:
            handle_siddata(conn,msgp);
            break;
        
        case SIDHEADERS:
            handle_sidheaders(conn,msgp);
            break;
            
        case SIDFAIL:
            handle_sidfail(conn,msgp);
            break;   
        
        default:
            cout << "UNKNOWN MSG! " << msgp->str() << endl;
    }
}

void
greynet::handle_query(connection_ptr conn, message_ptr msgp)
{
    using namespace json_spirit;
    boost::shared_ptr<ResolverQuery> rq;
    try
    {
        Value mv;
        if(!read(msgp->payload_str(), mv)) 
        {
            cout << "Greynet: invalid JSON in this message, discarding." << endl;
            conn->fin(); // invalid json = disconnect them.
            return;
        }
        Object qo = mv.get_obj();
        rq = ResolverQuery::from_json(qo);
    } 
    catch (...) 
    {
        cout << "Greynet: invalid search json, discarding" << endl;
        conn->fin(); // too harsh?
        return;
    }
    
    if(m_pap->query_exists(rq->id()))
    {
        cout << "Greynet: discarding search message, QID already exists: " << rq->id() << endl;
        return;
    }

    cout << "greynet: handing search query:" << msgp->payload_str() << endl;
    // register source for this query, so we know where to 
    // send any replies to.
    set_query_origin(rq->id(), conn);

    // dispatch search with our callback handler:
    rq_callback_t cb = boost::bind(&greynet::send_response, this, _1, _2);
    query_uid qid = m_pap->dispatch(rq, cb);
    
    assert(rq->id() == qid);
    
    cout << "greynet: sending search to peers" << endl;
    /*
        schedule search to be fwded to our peers - this will abort if
        the query has been solved before it fires anyway.
          
        The 100ms delay is intentional - it means cancellation messages
        can reach the search frontier immediately (fwded with no delay)
    */
    boost::shared_ptr<boost::asio::deadline_timer> 
        t(new boost::asio::deadline_timer( *m_io_service ));
    t->expires_from_now(boost::posix_time::milliseconds(100));
    // pass the timer pointer to the handler so it doesnt autodestruct:
    t->async_wait(boost::bind(&greynet::fwd_search, this,
                                boost::asio::placeholders::error, 
                                conn, msgp, t, qid));
}

void
greynet::fwd_search(const boost::system::error_code& e,
                     connection_ptr conn, message_ptr msgp,
                     boost::shared_ptr<boost::asio::deadline_timer> t,
                     query_uid qid)
{
    if(e)
    {
        cout << "Error from timer, not fwding: "<< e.value() << " = " << e.message() << endl;
        return;
    }
    // bail if already solved (probably from our locallibrary resolver)
    if( m_pap->rq(qid)->solved() )
    {
        cout << "Greynet: not relaying solved search: " << qid << endl;
        return;
    }
    // TODO check search is still active
    cout << "Forwarding search.." << endl;
    m_router->foreach_conns_except( boost::bind(&Connection::async_write, _1, msgp), conn );
}

// fired when a new result is available for a running query:
void
greynet::send_response( query_uid qid, 
                        boost::shared_ptr<ResolvedItem> rip)
{
    connection_ptr origin_conn = get_query_origin(qid);
    // relay result if the originating connection still active:
    cout << "got send_response with qid:" << qid << "and url:" << rip->url() << endl;
    if(origin_conn)
    {
        message_ptr resp(new QueryResultMessage(qid, rip, m_router->gen_uuid()));
        origin_conn->async_write( resp );
    }
}

void
greynet::handle_queryresult(connection_ptr conn, message_ptr msgp)
{
    cout << "Got search result: " << msgp->str() << endl;
    // try and parse it as json:
    Value v;
    if(!read(msgp->payload_str(), v)) 
    {
        cout << "Greynet: invalid JSON in this message, discarding." << endl;
        conn->fin(); // invalid json = disconnect.
        return; 
    }
    Object o = v.get_obj();
    map<string,Value> r;
    obj_to_map(o,r);
    if(r.find("qid")==r.end() || r.find("result")==r.end())
    {
        cout << "Greynet, malformed search response, discarding." << endl;
        conn->fin(); // malformed = disconnect.
        return; 
    }
    query_uid qid = r["qid"].get_str();
    Object resobj = r["result"].get_obj();
    ri_ptr rip;
    try
    {
        rip = boost::shared_ptr<playdar::ResolvedItem>( new ResolvedItem( resobj ) );
    }
    catch (...)
    {
        cout << "Greynet: Missing fields in response json, discarding" << endl;
        conn->fin();
        return;
    }
    cout    << "INFO Result from '" << rip->source()
                            <<"' for '"<< write_formatted( rip->get_json())
                            << endl;
                            
    ostringstream rbs;
    rbs << "greynet://" << conn->name() << "/sid/" << rip->id();
    cout << "created new greynet url:" << rbs.str() << endl;
    
    rip->set_url( rbs.str() );
    vector< json_spirit::Object> res;
    res.push_back(rip->get_json());
    m_pap->report_results( qid, res );
}

connection_ptr 
greynet::get_conn( const std::string & name )
{
    return m_router->get_connection_by_name( name );
}


// asks remote host to start streaming us data for this sid
void
greynet::start_sidrequest(connection_ptr conn, source_uid sid, 
                          boost::shared_ptr<ss_greynet> ss)
{
    m_sid2ss[sid] = ss;
    register_sidtransfer( conn, sid );
    message_ptr msg(new GeneralMessage(SIDREQUEST, sid, m_pap->gen_uuid()));
    conn->async_write( msg );
    // the ss_greynet will get callbacks fired as data arrives.
}


// a peer has asked us to start streaming something to them:
bool
greynet::handle_sidrequest(connection_ptr conn, message_ptr msg)
{
    source_uid sid = msg->payload_str();
    cout << "Greynet request for sid: " << sid << " len: " << sid.length() << endl;
    
    ss_ptr ss = m_pap->get_ss( sid );
    if(!ss)
    {
        cout << "SID no longer valid" << endl;
        message_ptr msgp(new GeneralMessage(SIDHEADERS, "status\t404\n", sid));
        conn->async_write( msgp );
        return false;
    }
    ri_ptr ri = m_pap->get_ri( sid );
    cout << "-> " << ss->debug() << endl;
    
    // our adaptor is responsible for sending SIDDATA msgs down the connection
    AsyncAdaptor_ptr aptr( new greynet_asyncadaptor( conn, sid ) );
    ss->start_reply( aptr );
    return true;
}

bool
greynet::handle_siddata(connection_ptr conn, message_ptr msgp)
{
    source_uid sid = msgp->guid();
    if(m_sid2ss.find(sid) == m_sid2ss.end())
    {
        cout << "Data transfer received for invalid sid("<<sid<<"), discarding" << endl;
        // TODO send cancel message
        return true;
    }
    // TODO locking for sid2ss
    m_sid2ss[sid]->siddata_handler( msgp->payload(), msgp->length() );
    return false;
}

void
greynet::handle_sidheaders(connection_ptr conn, message_ptr msgp)
{
    source_uid sid = msgp->guid();
    if(m_sid2ss.find(sid) == m_sid2ss.end())
    {
        cout << "Headers transfered for invalid sid("<<sid<<"), discarding" << endl;
        return;
    }
    // TODO locking for sid2ss
    m_sid2ss[sid]->sidheaders_handler( msgp );
}

void
greynet::handle_sidfail(connection_ptr conn, message_ptr msgp)
{
    source_uid sid = msgp->guid();
    if(m_sid2ss.find(sid) == m_sid2ss.end())
    {
        return;
    }
    // TODO locking for sid2ss
    cout << "SIDFAIL received." << endl;
    m_sid2ss[sid]->cancel_handler();
}

std::map< std::string, boost::function<ss_ptr(std::string)> >
greynet::get_ss_factories()
{
    // return our greynet ss
    std::map< std::string, boost::function<ss_ptr(std::string)> > facts;
    facts[ "greynet" ] = boost::bind( &ss_greynet::factory, _1, this );
    return facts;
}
    
// web interface:
bool 
greynet::anon_http_handler(const playdar_request& req, playdar_response& resp,
                           playdar::auth& pauth)
{
    if( req.postvar_exists("formtoken") &&
        req.postvar_exists("newaddr") &&
        req.postvar_exists("newport") &&
        pauth.consume_formtoken(req.postvar("formtoken")) )
    {
        string addr = req.postvar("newaddr");
        unsigned short port = boost::lexical_cast<unsigned short>(req.postvar("newport"));
        boost::asio::ip::address_v4 ip = boost::asio::ip::address_v4::from_string(addr);
        boost::asio::ip::tcp::endpoint ep(ip, port);
        m_router->connect_to_remote(ep);
    }
    
    if( req.getvar_exists("pingall") )
    {
        cout <<" Pinging all.." << endl;
        m_router->send_all( message_ptr(new PingMessage(m_router->gen_uuid())) );
    }
    
    string formtoken = m_pap->gen_uuid();
    pauth.add_formtoken( formtoken );
    typedef pair<string, connection_ptr_weak> pair_t;
    ostringstream os;
    os  << "<h2>Greynet Settings</h2>"
        "<form method=\"post\" action=\"\">"
        "Connect "
        "IP: <input type=\"text\" name=\"newaddr\" />"
        "Port: <input type=\"text\" name=\"newport\" value=\"" 
        << GREYNET_PORT << "\"/>"
        "<input type=\"hidden\" name=\"formtoken\" value=\""
        << formtoken << "\"/>"
        " <input type=\"submit\" value=\"Connect to remote servent\" />"
        "</form>" 
        "<hr/><p>"
        "NB: You might want to <a href=\"/greynet/config\">refresh</a> this page if you just connected,"
        " it may take a couple of seconds to connect and display the connection below."
        "</p>"
        << endl
        ;
    os  << "<h3>Current Connections</h3>"    
        << "<table>"
        << "<tr style=\"font-weight:bold;\">"
        << "<td>Username</td><td>Msg Queue Size</td><td>Address</td></tr>";
    
    os  << "<tr><td colspan='3'><pre>"
        << m_router->connections_str()
        << "</pre></td></tr>";
    /*
    BOOST_FOREACH(pair_t p, connections())
    {
        connection_ptr conn(p.second);
        boost::asio::ip::tcp::endpoint remote_ep = conn->socket().remote_endpoint();
        os  << "<tr>"
            << "<td>" << p.first << "</td>"
            << "<td>" << conn->writeq_size() << "</td>"
            << "<td>" << remote_ep.address().to_string() 
            <<           ":" << remote_ep.port() << "</td>"
            << "</tr>";
    }
    */
    os  << "</table>" 
        << endl; 
        
    resp = playdar_response( os.str() );
    return true;
}









EXPORT_DYNAMIC_CLASS( greynet )

} }

