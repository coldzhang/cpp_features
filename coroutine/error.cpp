#include "error.h"
#include "scheduler.h"

namespace co
{

const char* co_error_category::name() const noexcept
{
    return "coroutine_error";
}

std::string co_error_category::message(int v) const
{
    switch (v) {
        case (int)eCoErrorCode::ec_ok:
            return "ok";

        case (int)eCoErrorCode::ec_mutex_double_unlock:
            return "co_mutex double unlock";

        case (int)eCoErrorCode::ec_block_object_locked:
            return "block object locked when destructor";

        case (int)eCoErrorCode::ec_block_object_waiting:
            return "block object was waiting when destructor";

        case (int)eCoErrorCode::ec_yield_failed:
            return "yield failed";

        case (int)eCoErrorCode::ec_swapcontext_failed:
            return "swapcontext failed";
    }

    return "";
}

const std::error_category& GetCoErrorCategory()
{
    static co_error_category obj;
    return obj;
}
std::error_code MakeCoErrorCode(eCoErrorCode code)
{
    return std::error_code((int)code, GetCoErrorCategory());
}
void ThrowError(eCoErrorCode code)
{
    DebugPrint(dbg_exception, "throw exception %d:%s",
            (int)code, GetCoErrorCategory().message((int)code).c_str());
    if (std::uncaught_exception()) return ;
    throw std::system_error(MakeCoErrorCode(code));
}

} //namespace co
