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
#include "playdar/auth.hpp"

#include "greynet_messages.hpp"

#define GREYNET_PORT 60211

//#include "servent.h"
//#include "ss_greynet.h"

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
    
    m_router = new Router( accp, this );
    
    // start io_services:
    cout << "Greynet router coming online on port " <<  port <<endl;
    cout << "Greynet build date: " << __DATE__ << " " << __TIME__ << endl;
    
    for (std::size_t i = 0; i < 1; ++i)
    {
        m_threads.create_thread( boost::bind(
            &boost::asio::io_service::run, m_io_service.get()));
    }
 
    // get peers: TODO support multiple/list from config
    string remote_ip = pap->get<string>("plugins.greynet.peerip","");
    if(remote_ip!="")
    {
        unsigned short remote_port = pap->get<int>
                                     ("plugins.greynet.peerport",GREYNET_PORT);

        cout << "Attempting peer connect: " 
             << remote_ip << ":" << remote_port << endl;
        boost::asio::ip::address_v4 ipaddr =
            boost::asio::ip::address_v4::from_string(remote_ip);
        boost::asio::ip::tcp::endpoint ep(ipaddr, remote_port);
        m_router->connect_to_remote(ep);
    }
    return true;
}

greynet::~greynet() throw()
{
    cout << "DTOR greynet" << endl;
    m_router->stop();
    m_threads.join_all();
}

void
greynet::start_resolving(boost::shared_ptr<ResolverQuery> rq)
{
    cout << "greynet::start_resolving..." << endl;
    message_ptr msg( new QueryMessage(rq) );
    m_router->send_all( msg );
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
    cout << "greynet::new_outgoing_connection " << conn->str() << endl;
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
    
    if( false ) // TODO validate the IDENT msg / auth code / whatever
    {
        cout << "Invalid ident/auth from incoming connection. closing." << endl;
        conn->fin();
        return;
    }
    
    // if the other end initiated the connection, and IDENTed, we should now
    // send them our IDENT message
    if( incoming ) send_ident( conn );
    
    // now the connection is considered ready to handle normal messages:
    conn->set_ready( true );
    conn->set_name( msgp->payload_str() );
    
    // remove our custom msg rcvd callback:
    conn->pop_message_received_cb();
    
    cout << "Connection ready to rock: " << conn->str() << endl;
}

void
greynet::send_ident( connection_ptr conn )
{
    cout << "Sending our IDENT.." << endl;
    message_ptr msg( new IdentMessage( m_pap->hostname()) );
    conn->async_write( msg );
}

void
greynet::connection_terminated(connection_ptr conn)
{
    cout << "Connection terminated: " << conn->str() << endl;
}

/// Only called once auth/ident has completed
void
greynet::message_received( message_ptr msgp, connection_ptr conn )
{
    switch(msgp->type())
    {
        case QUERY:
            handle_query(conn,msgp);
            break;
            
        case QUERYRESULT:
            handle_queryresult(conn,msgp);
            break;
        
        case QUERYCANCEL:
            //handle_querycancel(conn,msgp);
            break;
            
        /*
        case SEARCHQUERY:
            return handle_searchquery(conn,msg);
        case SEARCHRESULT:
            return handle_searchresult(conn, msg);
        case SIDREQUEST:
            m_io_service->post( boost::bind( 
                &greynet::handle_sidrequest, this,
                conn, msg ));
            return true;
        case SIDDATA:
            //m_io_service->post( boost::bind( 
            //    &greynet::handle_siddata, this, conn, msg));
            handle_siddata(conn,msg);
            return true;
        */
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
        message_ptr resp(new QueryResultMessage(qid, rip));
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
    rbs << "greynet://" << conn->str() << "/sid/" << rip->id();
    cout << "created new greynet url:" << rbs.str() << endl;
    rip->set_url( rbs.str() );
    vector< json_spirit::Object> res;
    res.push_back(rip->get_json());
    m_pap->report_results( qid, res );
}
/*
// asks remote host to start streaming us data for this sid
void
greynet::start_sidrequest(connection_ptr conn, source_uid sid, 
                             boost::function<bool (const char * payload, size_t len)> handler)
{
    m_sidhandlers[sid] = handler;
    msg_ptr msg(new LameMsg(sid, SIDREQUEST));
    send_msg(conn, msg);
}

// a peer has asked us to start streaming something:
bool
greynet::handle_sidrequest(connection_ptr conn, msg_ptr msg)
{
    source_uid sid = msg->payload();
    cout << "Greynet request for sid: " << sid << endl;
    
    ss_ptr ss = m_pap->get_ss( sid );
    ri_ptr ri = m_pap->get_ri( sid );
    cout << "-> " << ss->debug() << endl;
    
    
    
    // We send SIDDATA msgs, where the payload is a sid_header followed
    // by the audio data.
    char buf[8194]; // this is the lamemsg payload.
    int len, total=0;
    sid_header sheader;
    memcpy((char*)&sheader.sid, sid.c_str(), 36);
    // put sheader at the start of our buffer:
    memcpy((char*)&buf, (char*)&sheader, sizeof(sid_header));
    
    if(ri) // send data:
    {
       cout << "-> PlayableItem: " << ri->url();
 //           << " - " << pip->track() << endl;
       cout << "-> " << ss->debug() << endl;
       cout << "-> source: '"<< ri->source() <<"'" << endl;
       cout << "Sending siddata packets: header.sid:'" 
           << sid << "'" << endl;
       // this will be the offset where we write audio data,
       // to leave the sid_header intact at the start:
       char * const buf_datapos = ((char*)&buf) + sizeof(sid_header);
       // read audio data into buffer at the data offset:
       while ((len = ss->read_bytes( buf_datapos,
                                     sizeof(buf)-sizeof(sid_header))
              )>0)
       {
           total+=len;
           string payload((const char*)&buf, sizeof(sid_header)+len);
           msg_ptr msgp(new LameMsg(payload, SIDDATA));
           send_msg(conn, msgp);
       }
    }
    else
    {
       cout << "No ri for sid '"<<sid<<"'" << endl;
       // send empty packet anyway, to signify EOS
       // TODO possibly send an msgtype=error msg
    }
    
    // send empty siddata to signify end of stream
    cout << "Sending end part. Transferred " << total << " bytes" << endl;
    string eostream((char*)&buf, sizeof(sid_header));
    msg_ptr msge(new LameMsg(eostream, SIDDATA));
    send_msg(conn, msge);
    cout << "Greynet: done streaming sid" << endl; 
    return true;
}

bool
greynet::handle_siddata(connection_ptr conn, msg_ptr msg)
{
    // first part of payload is sid_header:
    sid_header sheader;
    memcpy(&sheader, msg->payload().c_str(), sizeof(sid_header));
    source_uid sid = string((char *)&sheader.sid, 36);
    //cout << "Rcvd part for " << sid << endl; 
    if(m_sidhandlers.find(sid) == m_sidhandlers.end())
    {
        cout << "Data transfer received for invalid sid("<<sid<<"), discarding" << endl;
        // TODO send cancel message
        return true;
    } 
    // pass payload to appropriate handler
    const size_t toread = (msg->payload_len()-sizeof(sid_header));
    assert(toread >= 0);
    const char * payload = msg->payload().data() + sizeof(sid_header);
    if( m_sidhandlers[sid]( payload, toread ) ) // takes a copy of this memory
    {
        return true;
    }
    else
    {
        // TODO send cancel message, our client gave up or data no longer needed
        // for some reason. sidhandler is rejecting it.
        return false;
    }
}
*/

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
    
    string formtoken = m_pap->gen_uuid();
    pauth.add_formtoken( formtoken );
    typedef pair<string, connection_ptr_weak> pair_t;
    ostringstream os;
    os  << "<h2>Greynet Settings</h2>"
        "<form method=\"post\" action=\"\">"
        "Connect "
        "IP: <input type=\"text\" name=\"newaddr\" />"
        "Port: <input type=\"text\" name=\"newport\" value=\"9999\"/>"
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

