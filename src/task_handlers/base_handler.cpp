#include "base_handler.h"

bool base_handler::prepare(const std::string& params)
{
    BGN_TRY
    {
        m_duration.start();

        CHK_PRM(m_reader.parse(params), "Parse error");

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

        LOGDEBUG << "Prepared json (" << complete << "," << pending << "):" << m_writer.stringify();

        return complete;
    }
    END_TRY_RET(false)
}
