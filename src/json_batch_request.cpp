#include "json_batch_request.h"
#include "json_rpc.h"
#include "task_handlers/task_handlers.h"
#include "task_handlers/base_handler.h"
#include "settings/settings.h"
#include "common/string_utils.h"
#include <boost/exception/all.hpp>

#define BGN try

#define END(ret) \
    catch (boost::exception& ex) {\
        LOGERR << __PRETTY_FUNCTION__ << " boost exception: " << boost::diagnostic_information(ex);\
        ret;\
    } catch (std::exception& ex) {\
        LOGERR << __PRETTY_FUNCTION__ << " std exception: " << ex.what();\
        ret;\
    } catch (...) {\
        LOGERR << __PRETTY_FUNCTION__ << " unhandled exception";\
        ret;\
    }

batch_json_request::batch_json_request(session_context_ptr ctx)
    : m_ctx(ctx)
    , m_size(0)
{
}

batch_json_request::~batch_json_request()
{
}

boost::asio::io_context& batch_json_request::get_io_context()
{
    return m_ctx->get_io_context();
}

const std::string& batch_json_request::get_remote_ep() const
{
    return m_ctx->get_remote_ep();
}

void batch_json_request::send_json(const char* data, size_t size)
{
    BGN
    {
        std::lock_guard<std::mutex> lock(m_lock);

        std::string_view json(data, size);

        json_rpc_writer writer;
        if (!writer.parse(data, size)) {
            writer.reset();
            writer.set_id(0);
            writer.set_error(-32603, "Could not parse json response");
            json = writer.stringify();
            LOGERR << "[" << get_remote_ep() << "] Could not parse json response: \n" << data;
        }

//        std::cout << "size: " << size << " data: " << data << std::endl;

//        m_result.PushBack(writer.get_doc().Move(), m_result.GetAllocator());

//        rapidjson::StringBuffer buf;
//        rapidjson::Writer<rapidjson::StringBuffer> w(buf);
//        m_result.Accept(w);

//        std::cout << "size: " << buf.GetLength() << " data: " << buf.GetString() << std::endl;

        m_res.emplace_back(json.data(), json.size());

        LOGINFO << "[" << get_remote_ep() << "] Completed request " << m_res.size() << "/" << m_size;
//        LOGINFO << "[" << get_remote_ep() << "] Completed request " << m_result.Size() << "/" << m_size;

        if (m_size == m_res.size()) {
//        if (m_size == m_result.Size()) {
            send_response();
        }
    }
    END()
}

void batch_json_request::process(const json_rpc_reader& reader)
{
    BGN
    {
        std::lock_guard<std::mutex> lock(m_lock);

        m_size = reader.get_doc().GetArray().Size();

        LOGINFO << "[" << get_remote_ep() << "] Process batch requests: " << m_size;

        std::string_view json, method;
        json_rpc_writer writer;
        if (m_size == 0) {
            writer.set_id(0);
            writer.set_error(-32600, "Invalid batch request");
            json = writer.stringify();
            m_ctx->send_json(json.data(), json.size());
            return;
        }

//        m_result.SetArray();
//        m_result.Reserve(m_size, m_result.GetAllocator());

        json_rpc_reader tmp;

        for (const auto&v : reader.get_doc().GetArray()) {
            json = reader.stringify(&v);
            tmp.parse(json.data(), json.size());

            if (!tmp.get_doc().IsObject()) {
                writer.reset();
                writer.set_id(0);
                writer.set_error(-32600, "Invalid request");
//                m_result.PushBack(writer.get_doc().Move(), m_result.GetAllocator());
                json = writer.stringify();
                m_res.emplace_back(json.data(), json.size());
                continue;
            }

            method = tmp.get_method();
            if (method.data() == nullptr) {
                writer.reset();
                writer.set_id(0);
                writer.set_error(-32600, "JSON method is not provided");
//                m_result.PushBack(writer.get_doc().Move(), m_result.GetAllocator());
                json = writer.stringify();
                m_res.emplace_back(json.data(), json.size());
                continue;
            }
            if (!tmp.has_id()) {
                LOGWARN << "[" << get_remote_ep() << "] Notification has recieved";
                m_size--;
                continue;
            }

            auto it = post_handlers.find(std::make_pair(method.data(), settings::system::useLocalDatabase));
            if (it == post_handlers.end()) {
                writer.reset();
                writer.set_id(tmp.get_id());
                writer.set_error(-32601, string_utils::str_concat("Method '", method, "' does not exist").c_str());
//                m_result.PushBack(writer.get_doc().Move(), m_result.GetAllocator());
                json = writer.stringify();
                m_res.emplace_back(json.data(), json.size());
            } else {
                auto res = it->second(shared_from(this), json.data());
                if (res) {
                    // sync complete
                    writer.parse(res.message.c_str(), res.message.size());
//                    m_result.PushBack(writer.get_doc().Move(), m_result.GetAllocator());
                    json = writer.stringify();
                    m_res.emplace_back(json.data(), json.size());
                }
            }
        }

        LOGINFO << "[" << get_remote_ep() << "] Completed requests " << m_res.size() << "/" << m_size;
//        LOGINFO << "[" << get_remote_ep() << "] Completed requests " << m_result.Size() << "/" << m_size;

        if (m_size == m_res.size()) {
//        if (m_size == m_result.Size()) {
            send_response();
        }
    }
    END()
}

void batch_json_request::send_response()
{
    BGN
    {
        size_t size = 0;
        for (const auto& v: m_res){
            size += v.size();
        }
        std::string result;
        result.reserve(size + m_res.size() + 2);
        result.append("[");
        for (auto& v: m_res){
            if (result.size() > 1) {
                result.append(",");
            }
            result.append(v);
        }
        result.append("]");
//        rapidjson::StringBuffer buf;
//        rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
//        m_result.Accept(writer);
//        m_ctx->send_json(buf.GetString(), buf.GetLength());
        m_ctx->send_json(result.c_str(), result.size());

#ifdef _DEBUG_
//        LOGDEBUG << result;
#endif
    }
    END()
}
