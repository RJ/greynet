#ifndef __GREYNET_ASYNCADAPTOR_HPP__
#define __GREYNET_ASYNCADAPTOR_HPP__

#include "playdar/streaming_strategy.h"
#include "libf2f/router.h"
#include "libf2f/protocol.h"
#include "greynet.h"
#include "greynet_messages.hpp"

namespace playdar {
namespace resolvers {

class greynet; 

class greynet_asyncadaptor 
:   public playdar::AsyncAdaptor,
    public boost::enable_shared_from_this<greynet_asyncadaptor>
{
public:
    greynet_asyncadaptor(libf2f::connection_ptr conn, source_uid sid)
        :  m_sid(sid), m_conn(conn), m_sentfirst(false),
            m_status(200), m_mimetype("application/binary"), m_contentlength(0)
    {
        //ggHack=0; // hack to make sender die mid-stream
    }

    virtual void write_content(const char *buffer, int size)
    {
        if( !m_sentfirst )
        {
            send_headers();
            m_sentfirst = true;
        }
        //if(++ggHack == 100) throw; // hack to make sender die mid-stream
        libf2f::message_ptr msg( new libf2f::GeneralMessage(SIDDATA, std::string(buffer, size), m_sid) );
        m_conn->async_write( msg );
    }
    
    virtual void write_finish()
    {
        std::cout << "greynet_asyncadaptor write_finish" << std::endl;
        // send empty siddata to indicate finished, for now.
        write_content((const char*)0, 0 );
    }

    virtual void write_cancel()
    {
        std::cout << "greynet_asyncadaptor write_cancel" << std::endl;
        libf2f::message_ptr msg( new libf2f::GeneralMessage(SIDFAIL, "", m_sid) );
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
        libf2f::message_ptr msgp(new libf2f::GeneralMessage(SIDHEADERS, os.str(), m_sid));
        m_conn->async_write( msgp );
    }
    
    source_uid m_sid;
    libf2f::connection_ptr m_conn;
    bool m_sentfirst; // did we already send first data message?
    int m_status;
    std::string m_mimetype;
    int m_contentlength;
};

}} //ns

#endif
