#ifndef __DARKNET_STREAMING_STRAT_H__
#define __DARKNET_STREAMING_STRAT_H__

#include "playdar/streaming_strategy.h"

#include "msgs.h"
#include "servent.h"
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <deque>

namespace playdar {
namespace resolvers {
    
class DarknetStreamingStrategy : public StreamingStrategy
{
public:

    DarknetStreamingStrategy(darknet * darknet, 
                             connection_ptr conn, 
                             std::string sid);
    
    ~DarknetStreamingStrategy() ;
    
    /// copy constructor, used by get_instance()
    DarknetStreamingStrategy(const DarknetStreamingStrategy& other);
    
    static boost::shared_ptr<DarknetStreamingStrategy> factory(std::string url, darknet * darknet);
    
    
    size_t read_bytes(char * buf, size_t size);
    
    string debug()
    { 
        ostringstream s;
        s<< "DarknetStreamingStrategy(from:" << m_conn->username() << " sid:" << m_sid << ")";
        return s.str();
    }
    
    void reset() 
    {
        m_active = false;
        m_finished = false;
        m_numrcvd=0;
        m_data.clear();
    }
    
     // tell peer to start sending us data.
    void start();
    
    bool siddata_handler(const char * payload, size_t len);

    std::string mime_type() { return "dont/know"; }
    
    bool async_delegate(WriteFunc writefunc)
    {
        if (!writefunc) {
            // aborted by the client side.
            m_abort = true;
            m_wf = 0;
            return false;
        }

        if (m_abort) {
            m_wf = 0;
            return false;
        }

        m_wf = writefunc;

        if(!m_connected) connect();
        if(!m_connected)
        {
            std::cout << "ERROR: connect failed in httpss." << std::endl;
            if( m_curl ) curl_easy_cleanup( m_curl );
            reset();
            m_wf = 0;
            return false;
        }

        {
            boost::lock_guard<boost::mutex> lock(m_mutex);

            if (m_writing && m_buffers.size()) {
                // previous write has completed:
                m_buffers.pop_front();
                m_writing = false;
            }

            if (!m_writing && m_buffers.size()) {
                // write something new
                m_writing = true;
                m_wf(boost::asio::const_buffer(m_buffers.front().data(), m_buffers.front().length()));
            }
        }

        bool result =  m_writing || !m_curl_finished;
        if (result == false) {
            m_wf = 0;
        }
        return result;
    }
    
    
private:
    darknet * m_darknet;
    connection_ptr m_conn;
    std::string m_sid;

    boost::mutex m_mutex;
    boost::condition m_cond;

    deque< char > m_data;   //output buffer
    unsigned int m_numrcvd; //bytes recvd so far
    bool m_finished;        //EOS reached?
    bool m_active;
    
    
    /////
    void enqueue(const std::string& s)
    {
        boost::lock_guard<boost::mutex> lock(m_mutex);
        m_buffers.push_back(s);
        if (!m_writing) {
            m_writing = true;
            m_wf(boost::asio::const_buffer(m_buffers.front().data(), m_buffers.front().length()));
        }
    }

    WriteFunc m_wf;     // keep a hold of the write func to keep the connection alive.
    boost::mutex m_mutex;
    bool m_writing;
    bool m_abort;
    std::list<std::string> m_buffers;
    
};

}}

#endif
