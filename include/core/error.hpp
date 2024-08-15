#ifndef INCLUDE_GUARD_VSOCK_CORE_ERROR_HPP
#define INCLUDE_GUARD_VSOCK_CORE_ERROR_HPP

#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#endif

namespace vsock {
    
    ////////////////////////////////////
    // Helpers
    //////////////////////////////////

    using namespace std::string_literals;

    template<typename ...Args>
    std::string ConcatErrors(const Args & ...args) {
        std::string result;
        ([&]() {result.append("\n");result.append(args);}(), ...);
        return result;
    }

    int GetLastErrorCode(bool socket_error);

    //////////////////////////////////////////////////////////////////////////////////
    // ErrorBase class declaration
    ////////////////////////////////////////////////////////////////////////////////

    class ErrorBase : public std::runtime_error {
    protected:
        ErrorBase(const std::string& what, bool socket_error);
    };

    //////////////////////////////////////////////////////////////////////////////////
    // RuntimeError class declaration
    ////////////////////////////////////////////////////////////////////////////////    

    class RuntimeError : public ErrorBase {
    public:
        template <typename... Args>
        RuntimeError(const Args &...args);
    };
    
    template <typename... Args>
    inline RuntimeError::RuntimeError(const Args &...args) :
        ErrorBase(ConcatErrors(args...),false)
    {
        // Merged error constructor
    }    

}

#endif // INCLUDE_GUARD_VSOCK_CORE_ERROR_HPP