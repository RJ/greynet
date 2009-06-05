#ifndef __GREYNET_STREAMING_STRAT_H__
#define __GREYNET_STREAMING_STRAT_H__

#include "playdar/streaming_strategy.h"

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <deque>

#include "libf2f/router.h"
#include "libf2f/protocol.h"

#include "greynet.h"
#include "greynet_messages.hpp"

#include "greynet_asyncadaptor.hpp"

namespace playdar {
namespace resolvers {

class greynet; 

class ss_greynet 
:   public StreamingStrategy, 
    public boost::enable_shared_from_this<ss_greynet>
{
public:

    ss_greynet(greynet * g, libf2f::connection_ptr conn, const std::string &sid);
    ss_greynet(const ss_greynet& other);
    ~ss_greynet() ;
    
    static boost::shared_ptr<ss_greynet> factory(std::string url, greynet* g);
    
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
    void sidheaders_handler(libf2f::message_ptr msgp);
    void cancel_handler(); //called if connection terminates
    
    void timeout_cb(const boost::system::error_code& e);
    
    libf2f::connection_ptr conn() const { return m_conn; }
    greynet * protocol() const { return m_greynet; }
    const std::string& sid() const { return m_sid; }
    
private:
    boost::shared_ptr<boost::asio::deadline_timer> m_timer;
    AsyncAdaptor_ptr m_aa;
    greynet* m_greynet;
    libf2f::connection_ptr m_conn;
    std::string m_sid;
    bool m_finished;        //EOS reached?
};

}}

#endif
