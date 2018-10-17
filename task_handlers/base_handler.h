#pragma once

#include <memory>
#include "../http_session.h"
#include "../json_rpc.h"
#include "../cpplib_open_ssl_decor/crypto.h"
#include "../settings/settings.h"
#include "../log/log.h"
#include "utils.h"

using mh_count_t = uint64_t;

struct handler_result
{
    operator bool() const                 {return !pending;}
    operator const std::string() const    {return message;}
    bool pending = { false };
    std::string message;
};

template <class T>
class base_handler
{
    friend struct handler_result;

public:
    base_handler(http_session_ptr session): m_session(session) {}
    virtual ~base_handler()	{}

    static handler_result perform(http_session_ptr session, const std::string& params)
    {
        try
        {
            utils::time_duration dur(true, "execute handler");
            T obj(session);
            if (obj.prepare(params))
                obj.execute();
            return obj.result();
        }
        catch (std::exception& ex)
        {
            STREAM_LOG_ERR("Perform handler exception: " << ex.what())
            return handler_result();
        }
        catch (...)
        {
            STREAM_LOG_ERR("Perform handler unknown exception")
            return handler_result();
        }
    }

protected:
    bool prepare(const std::string& params);

    virtual bool prepare_params() = 0;
    virtual void execute() = 0;

    handler_result result()
    {
        this->m_result.message = this->m_writer.stringify();
        return this->m_result;
    }

protected:
    json_rpc_id         m_id = { 0 };
    handler_result      m_result;
    json_rpc_reader     m_reader;
    json_rpc_writer     m_writer;
    http_session_ptr    m_session;
};

#define DECL_BASE_HANDLER(cl)\
    class cl : public base_handler<cl>\
    {\
    public:\
        typedef base_handler<cl> base;\
        cl(http_session_ptr session): base(session) {}\
        virtual ~cl() override {}\
        virtual bool prepare_params() override;\
        virtual void execute() override;\
    };
