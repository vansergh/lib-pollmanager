#include <core/error.hpp>

namespace vsock {

    ////////////////////////////////////
    // Helpers
    //////////////////////////////////

    int GetLastErrorCode(bool socket_error) {
        #ifdef _WIN32
        int res = GetLastError();
        #else
        int res = static_cast<int>(errno);
        #endif
        return res;
    }

    ////////////////////////////////////
    // ErrorBase class defenition
    //////////////////////////////////

    ErrorBase::ErrorBase(const std::string& what, bool socket_error) :
        std::runtime_error(what + "\nLast Error Code: "s + std::to_string(GetLastErrorCode(socket_error)))
    {
        // Default runtime error
    }



}