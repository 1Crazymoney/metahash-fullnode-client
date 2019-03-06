#include "send_tx_handler.h"

#include "../SyncSingleton.h"
#include "../sync/BlockInfo.h"
#include "settings/settings.h"
#include "http_session.h"
#include "http_json_rpc_request.h"

bool send_tx_handler::prepare_params()
{
    BGN_TRY
    {
        if (!check_params())
            return false;

        auto params = m_reader.get_params();
        auto jValue = m_reader.get("nonce", *params);
        if (jValue)
        {
            std::string tmp;
            CHK_PRM(json_utils::val2str(jValue, tmp), "nonce field incorrect format")
            m_nonce = std::stoull(tmp);
        }
        else if (settings::system::useLocalDatabase) {
            CHK_PRM(syncSingleton() != nullptr, "Sync not set");
            const torrent_node_lib::Sync &sync = *syncSingleton();
            const torrent_node_lib::BalanceInfo balance = sync.getBalance(torrent_node_lib::Address(m_address));
            m_nonce = balance.countSpent + 1;
        } else {
            m_result.pending = true;

            json_rpc_id id = 1;
            json_rpc_writer writer;
            writer.set_id(id);
            writer.add_param("address", m_address);

            auto request = std::make_shared<http_json_rpc_request>(settings::server::tor, m_session->get_io_context());
            request->set_path("fetch-balance");
            request->set_body(writer.stringify());
            request->execute_async(boost::bind(&send_tx_handler::on_get_balance, shared_from(this), request, id));
            return false;
        }

        if (!build_request())
            return false;

        return true;
    }
    END_TRY_RET(false)
}

void send_tx_handler::on_get_balance(http_json_rpc_request_ptr request, json_rpc_id id)
{
    json_rpc_writer writer;
    BGN_TRY
    {
        json_rpc_reader reader;
        writer.set_id(id);

        CHK_PRM(reader.parse(request->get_result()), "Invalid response json")

        std::cout << reader.stringify();

        //json_rpc_id _id = reader.get_id();
        //CHK_PRM(_id != 0 && _id == id, "Returned id doesn't match")

        auto err = reader.get_error();
        auto res = reader.get_result();

        CHK_PRM(err || res, "No occur result or error")

        bool success = false;
        if (err) {
            writer.set_error(*err);
        } else if (res) {
            int64_t count_spent(0);
            if (reader.get_value(*res, "count_spent", count_spent)) {
                success = true;
                m_nonce = count_spent + 1;
            } else {
                writer.set_error(-32605, "field spent count not found");
            }
        }

        if (success) {
            success = build_request();
        }

        if (success) {
            send_tx_handler::execute();
//            boost::asio::post(boost::bind(&send_tx_handler::execute, shared_from(this)));
        } else {
            m_session->send_json(writer.stringify());
//            boost::asio::post(boost::bind(&http_session::send_json, m_session, writer.stringify()));
        }
    }
//    END_TRY_PARAM(boost::asio::post(boost::bind(&http_session::send_json, m_session, writer.stringify())))
    END_TRY_PARAM(m_session->send_json(writer.stringify()))
}

void send_tx_handler::processResponse(json_rpc_reader &reader)
{
    //json_rpc_id _id = reader.get_id();
    //CHK_PRM(_id != 0 && _id == id, "Returned id doesn't match")
    
    auto err = reader.get_error();
    auto res = reader.get_result();
    auto params = reader.get_params();
    
    CHK_PRM(err || res, "No occur result or error")
    
    if (err) {
        m_writer.set_error(*err);
    } else {
        CHK_PRM(params->IsString(), "params field not found");
        m_writer.set_result(*params);
    }
}
