#include "ss_greynet.h"

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/algorithm/string.hpp>
#include <deque>

using namespace std;

namespace playdar {
namespace resolvers {
        
        
ss_greynet::ss_greynet(greynet* g, connection_ptr conn, const std::string &sid)
    : m_greynet(g), m_conn(conn), m_sid(sid)
    /* m_abort(false),*/ 
{
    cout << "CTOR ss_greynet" << endl;
    reset();
    
}

ss_greynet::~ss_greynet()
{
    //m_abort = true;
    cout << "DTOR ss_greynet " << debug() << endl;
}

ss_greynet::ss_greynet(const ss_greynet& other)
        :   m_greynet(other.protocol()),
            m_conn(other.conn()),
            m_sid(other.sid())
            //m_abort(false),
            
{
        reset();
}
    
boost::shared_ptr<ss_greynet> 
ss_greynet::factory(std::string url, playdar::resolvers::greynet* g)
{
    cout << "in ss_greynet::factory with url:" << url << endl;
    size_t offset = 10; // skip 'greynet://'
    size_t offset2 = url.find( "/", offset );
    if( offset2 == string::npos ) return boost::shared_ptr<ss_greynet>();
    string username = url.substr(offset, offset2 - offset ); // username/sid/<sid>
    string sid = url.substr( offset2 + 5, url.length() - (offset2 + 5) );
    cout << "got username in greynet_ss::factory: " << username << " and sid: " << sid << endl;
    connection_ptr conn = g->get_conn( username );
    if( conn )
        return boost::shared_ptr<ss_greynet>(new ss_greynet(g, conn, sid ));
    else
        return boost::shared_ptr<ss_greynet>();
}



void 
ss_greynet::start_reply(AsyncAdaptor_ptr aa)
{
    reset();
    if(!m_conn->ready())
    {
        cout << "Greynet connection went away :(" << endl;
        throw;
    }
    m_aa = aa;
    //m_aa->set_finished_cb( boost::bind(&ss_greynet::write_ending, shared_from_this()) );
    // request stream start:
    m_greynet->start_sidrequest(m_conn, m_sid, shared_from_this());
}

bool 
ss_greynet::siddata_handler(const char * payload, size_t len)
{
    
    if( len == 0 )
    {
        cout << "last SIDDATA msg has arrived" << endl;
        m_finished = true;
        m_aa->write_finish();
    }
    else
    {
        // our async delegate knows what to do with the data...
        m_aa->write_content(payload, len);
    }
    return true;
}

void
ss_greynet::sidheaders_handler(message_ptr msgp)
{
    vector<string> lines;
    vector<string> parts;
    const string p = msgp->payload_str();
    boost::split(lines, p, boost::is_any_of("\n"));
    BOOST_FOREACH(string line, lines)
    {
        parts.clear();
        boost::split(parts, line, boost::is_any_of("\t"));
        if(parts.size() == 2)
        {
            cout << "Header line: " << line << endl;
            if( parts[0] == "status" )
            {
                int code = atoi(parts[1].c_str());
                m_aa->set_status_code( code );
                if( code != 200 )
                {
                    cout << "Remote end gave status code: " << code << " aborting." << endl;
                    m_aa->write_cancel();
                    return;
                }
                continue;
            }
            if( parts[0] == "mime-type" )
            {
                m_aa->set_mime_type( parts[1] );
                continue;
            }
            if( parts[0] == "content-length" )
            {
                int len = atoi(parts[1].c_str());
                m_aa->set_content_length( len );
                continue;
            }
        }
        cout << "UNHANDLED line" << endl;
    }
}


}
}