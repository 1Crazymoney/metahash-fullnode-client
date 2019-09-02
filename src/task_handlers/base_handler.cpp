#include "base_handler.h"
#include "string_utils.h"

bool base_handler::prepare(const std::string& params)
{
    BGN_TRY
    {
        m_duration.start();

        CHK_PARSE(m_reader.parse(params.c_str()),
                string_utils::str_concat("Parse json error ", std::to_string(m_reader.get_parse_error()),
                                         ": ", m_reader.get_parse_error_str()).c_str());

        m_id = m_reader.get_id();
        m_writer.set_id(m_id);

        const bool complete = prepare_params();
        const bool pending = m_result.pending;
        if (!complete && !pending)
        {
            // prepare_params must set an error

            if (!m_writer.is_error())
            {
                m_writer.reset();
                m_writer.set_error(-32602, "Invalid params");
            }
        }
        LOGINFO << "[" << get_name() << "] Prepared json (complete = " << complete << ", async = " << pending << ")";

#ifdef _DEBUG_
        LOGDEBUG << "Prepared json: \n" << m_writer.stringify();
#endif
        return complete;
    }
    END_TRY(return false)
}
