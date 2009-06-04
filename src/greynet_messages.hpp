#ifndef __LIBF2F_GREYNET_MESSAGE_H__
#define __LIBF2F_GREYNET_MESSAGE_H__

#include "playdar/resolver_query.hpp"
#include "playdar/playdar_plugin_include.h"
#include "libf2f/message.h"

#define PING            0
#define PONG            1
#define IDENT           2
#define QUERY           3
#define QUERYRESULT     4
#define QUERYCANCEL     5
#define SIDREQUEST      6
#define SIDDATA         7
#define SIDFAIL         8
#define SIDHEADERS      9

class PongMessage : public libf2f::Message
{
public:
    PongMessage(const std::string& uuid)
    {
        libf2f::message_header h;
        memcpy( &h.guid, uuid.data(), 36 );
        h.type = PONG;
        h.ttl  = 1;
        h.hops = 0;
        h.length = 0;
        m_header = h;
        m_payload = 0;
    }
};

class PingMessage : public libf2f::Message
{
public:
    PingMessage(const std::string& uuid)
    {
        libf2f::message_header h;
        memcpy( &h.guid, uuid.data(), 36 );
        h.type = PING;
        h.ttl  = 1;
        h.hops = 0;
        h.length = 0;
        m_header = h;
        m_payload = 0;
    }
};

class IdentMessage : public libf2f::Message
{
public:
    IdentMessage(const std::string &name, const std::string& uuid)
    {
        libf2f::message_header h;
        memcpy( &h.guid, uuid.data(), 36 );
        h.type = IDENT;
        h.ttl  = 1;
        h.hops = 0;
        h.length = htonl(name.length());
        m_header = h;
        malloc_payload();
        memcpy( m_payload, name.data(), name.length() );
    }
};

class QueryMessage : public libf2f::Message
{
public:
    QueryMessage(playdar::rq_ptr rq, const std::string& uuid)
    {
        libf2f::message_header h;
        memcpy( &h.guid, uuid.data(), 36 );
        h.type = QUERY;
        h.ttl  = 1;
        h.hops = 0;
        using namespace json_spirit;
        Object jq = rq->get_json();
        std::ostringstream querystr;
        write( jq, querystr );
        std::string pl = querystr.str();
        h.length = htonl( pl.length() );
        m_header = h;
        malloc_payload();
        memcpy( m_payload, pl.c_str(), pl.length() );
    }

};

class QueryResultMessage : public libf2f::Message
{
public:
    QueryResultMessage(playdar::query_uid qid, playdar::ri_ptr rip, const std::string& uuid)
    {
        libf2f::message_header h;
        memcpy( &h.guid, uuid.data(), 36 );
        h.type = QUERYRESULT;
        h.ttl  = 1;
        h.hops = 0;
        using namespace json_spirit;
        Object response;
        response.push_back( Pair("qid", qid) );
        response.push_back( Pair("result", rip->get_json()) );
        std::ostringstream ss;
        write( response, ss );
        std::string pl = ss.str();
        h.length = htonl( pl.length() );
        m_header = h;
        malloc_payload();
        memcpy( m_payload, pl.c_str(), pl.length() );
    }

};
        
        
#endif

