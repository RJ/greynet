#ifndef __DARKNET_STREAMING_STRAT_H__
#define __DARKNET_STREAMING_STRAT_H__

#include "playdar/streaming_strategy.h"

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <deque>

#include "libf2f/router.h"
#include "libf2f/protocol.h"

#include "greynet.h"
#include "greynet_messages.hpp"

using namespace libf2f;
using namespace std;

namespace playdar {
namespace resolvers {

class greynet; 

typedef boost::function< void(boost::asio::const_buffer)> WriteFunc;

class greynet_asyncadaptor 
:   public playdar::AsyncAdaptor,
    public boost::enable_shared_from_this<greynet_asyncadaptor>
{
public:
    greynet_asyncadaptor(connection_ptr conn, source_uid sid)
        :  m_sid(sid), m_conn(conn), m_sentfirst(false),
            m_status(200), m_mimetype("application/binary"), m_contentlength(0)
    {
    ggHack=0;
    }

    virtual void write_content(const char *buffer, int size)
    {
        if( !m_sentfirst )
        {
            send_headers();
            m_sentfirst = true;
        }
        if(++ggHack == 100) throw;
        message_ptr msg( new GeneralMessage(SIDDATA, std::string(buffer, size), m_sid) );
        m_conn->async_write( msg );
    }
    
    virtual void write_finish()
    {
        std::cout << "greynet_asyncadaptor write_finish" << endl;
        // send empty siddata to indicate finished, for now.
        write_content((const char*)0, 0 );
    }

    virtual void write_cancel()
    {
        std::cout << "greynet_asyncadaptor write_cancel" << endl;
        message_ptr msg( new GeneralMessage(SIDFAIL, "", m_sid) );
        m_conn->async_write( msg );
    }

    virtual void set_content_length(int contentLength)
    {
        m_contentlength = contentLength;
    }

    virtual void set_mime_type(const std::string& mimetype)
    {
        m_mimetype = mimetype;
    }

    virtual void set_status_code(int status)
    {
        m_status = status;
    }

    virtual void set_finished_cb(boost::function<void(void)> cb)
    {
        
    }
    
private:
    int ggHack;

    void send_headers()
    {
        std::ostringstream os;
        os  << "status\t" << m_status << "\n"
            << "mime-type\t" << m_mimetype << "\n"
            << "content-length\t" << m_contentlength << "\n";
        std::cout << "Sending headers:\n" << os.str();
        message_ptr msgp(new GeneralMessage(SIDHEADERS, os.str(), m_sid));
        m_conn->async_write( msgp );
    }
    
    source_uid m_sid;
    libf2f::connection_ptr m_conn;
    bool m_sentfirst; // did we already send first data message?
    int m_status;
    std::string m_mimetype;
    int m_contentlength;
};



///


class ss_greynet 
:   public StreamingStrategy, 
    public boost::enable_shared_from_this<ss_greynet>
{
public:

    ss_greynet(greynet * g, connection_ptr conn, const std::string &sid);
    ss_greynet(const ss_greynet& other);
    ~ss_greynet() ;
    
    static boost::shared_ptr<ss_greynet> factory(std::string url, playdar::resolvers::greynet* g);
    
    std::string debug()
    { 
        std::ostringstream s;
        s<< "ss_greynet(from:" << m_conn->name() << " sid:" << m_sid << ")";
        return s.str();
    }
    
    void reset()
    {
        m_finished = false;
    }
    
    void start_reply(AsyncAdaptor_ptr aa);
    bool siddata_handler(const char * payload, size_t len);
    void sidheaders_handler(message_ptr msgp);
    
    connection_ptr conn() const { return m_conn; }
    greynet * protocol() const { return m_greynet; }
    const std::string& sid() const { return m_sid; }
    
private:
    AsyncAdaptor_ptr m_aa;
    greynet* m_greynet;
    connection_ptr m_conn;
    std::string m_sid;
    bool m_finished;        //EOS reached?
};

}}

#endif
