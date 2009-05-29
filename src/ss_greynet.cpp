#include "ss_darknet.h"

#include "playdar/types.h"
#include "darknet.h"

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <deque>

namespace playdar {
namespace resolvers {
        
        
DarknetStreamingStrategy::DarknetStreamingStrategy(darknet* darknet, connection_ptr conn, std::string sid)
    : m_conn(conn), m_sid(sid), m_abort(false)
{
    m_darknet = darknet;
    reset();
}


DarknetStreamingStrategy::~DarknetStreamingStrategy()
{
    m_abort = true;
    cout << "DTOR " << debug() << endl;
}

DarknetStreamingStrategy(const DarknetStreamingStrategy& other);
        : m_curl(0)
        , m_url(other.url())
        , m_thread(0)
        , m_abort(false)
{
//        reset();
}
    
boost::shared_ptr<DarknetStreamingStrategy> 
DarknetStreamingStrategy::factory(std::string url, playdar::resolvers::darknet* darknet)
{
    cout << "in DSS::factory with url:" << url << endl;
    size_t offset = 10; // skip 'darknet://'
    size_t offset2 = url.find( "/", offset );
    if( offset2 == string::npos ) return boost::shared_ptr<DarknetStreamingStrategy>();
    string username = url.substr(offset, offset2 - offset ); // username/sid/<sid>
    string sid = url.substr( offset2 + 5, url.length() - (offset2 + 5) );
    cout << "got username in darknet_ss::factory: " << username << " and sid: " << sid << endl;
    std::map<std::string, connection_ptr_weak> map = darknet->connections();
    std::map<std::string, connection_ptr_weak>::iterator iter = map.begin();
    for(;iter!=map.end();iter++)
        cout << "got possible username in connection map:" << iter->first << endl;
    connection_ptr con = darknet->connections()[username].lock();
    
    return boost::shared_ptr<DarknetStreamingStrategy>(new DarknetStreamingStrategy(darknet, con, sid ) );
}

size_t 
DarknetStreamingStrategy::read_bytes(char* buf, size_t size)
{
    if(!m_active) start();
    boost::mutex::scoped_lock lk(m_mutex);
    // Wait until data available:
    while( (m_data.empty() || m_data.size()==0) && !m_finished )
    {
        //cout << "Waiting for SIDDATA message..." << endl;
        m_cond.wait(lk);
    }
    //cout << "Got data to send, max available: "<< m_data.size() << endl;
    if(!m_data.empty())
    {
        int sent = 0;
        for(; !m_data.empty() && (sent < size); sent++)
        {
            *(buf+sent) = m_data.front();
            m_data.pop_front();
        }
        if(sent) return sent;
        assert(0);
        return 0;
    }
    else if(m_finished)
    {
        cout << "End of stream marker reached. "
        << m_numrcvd << " bytes rcvd total" << endl;
        cout << "Current size of output buffer: " << m_data.size() << endl;
        assert(m_data.size()==0);
        reset();
        return 0;
    }
    else
    {
        assert(0); // wtf.
        return 0;
    }
}

void 
DarknetStreamingStrategy::start()
{
    reset();
    if(!m_conn->alive())
    {
        cout << "Darknet connection went away :(" << endl;
        throw;
    }
    m_active = true;
    // setup a timer to abort if we don't get data quick enough?
    m_darknet->start_sidrequest( m_conn, m_sid, 
                                 boost::bind(&DarknetStreamingStrategy::siddata_handler, 
                                             this, _1)
                                             ); 
}

bool 
DarknetStreamingStrategy::siddata_handler(const char * payload, size_t len)
{
    boost::mutex::scoped_lock lk(m_mutex);
    enqueue( string( payload, len ) );
    m_numrcvd+=toread;
    if(toread == 0)
    {
        cout << "last SIDDATA msg has arrived ("
             << m_numrcvd <<" bytes total)." << endl;
        m_finished = true;
    }
    return true;
}

}
}