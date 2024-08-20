#ifndef INCLUDE_GUARD_VARNODE_HPP
#define INCLUDE_GUARD_VARNODE_HPP

#include <utility>
#include <memory>
#include <stdexcept>

namespace vsock {

    //////////////////////////////////////////////////////////////////////////////////
    // VarNode class declaration
    ////////////////////////////////////////////////////////////////////////////////

    class VarNode {
    public:

        VarNode(const VarNode&) = delete;
        VarNode& operator=(const VarNode&) = delete;

    public:

        VarNode();
        VarNode(VarNode&& other);
        VarNode& operator=(VarNode&& rhs);
        ~VarNode();

        template<typename T>
        VarNode(T&& data);

        template<typename T>
        void Put(T&& data);

        template<typename T>
        T& Emplace(T&& data);

        template<typename T>
        const T& Get() const;

        template<typename T>
        T& Get();

        void Drop();
        bool Empty() const noexcept;

    private:

        template<typename T>
        void* Put_(T&& data);

        template<typename T>
        static void Delete_(void* ptr);

        void Swap_(VarNode& other) noexcept;

    private:

        void* data_;
        void (*delete_fnc_)(void*);
        std::size_t hash_code_;

    };

    //////////////////////////////////////////////////////////////////////////////////
    // VarNode class defenition (template methods)
    ////////////////////////////////////////////////////////////////////////////////

    template<typename T>
    VarNode::VarNode(T&& data) :
        data_{ Put_(std::move(data)) }
    {}

    template<typename T>
    inline void VarNode::Put(T&& data) {
        data_ = Put_(std::move(data));
    }

    template<typename T>
    inline T& VarNode::Emplace(T&& data) {
        Put(std::move(data));
        return *(reinterpret_cast<T*>(data_));
    }

    template<typename T>
    inline const T& VarNode::Get() const {
        if (typeid(T).hash_code() != hash_code_) {
            throw std::runtime_error("bad type");
        }
        return *(reinterpret_cast<T*>(data_));
    }

    template<typename T>
    inline T& VarNode::Get() {
        if (typeid(T).hash_code() != hash_code_) {
            throw std::runtime_error("bad type");
        }      
        return *(reinterpret_cast<T*>(data_));
    }

    template<typename T>
    inline void* VarNode::Put_(T&& data) {
        void* result = reinterpret_cast<void*>(new T(std::move(data)));
        Drop();
        hash_code_ = typeid(T).hash_code();
        delete_fnc_ = VarNode::Delete_<T>;
        return result;
    }

    template<typename T>
    inline void VarNode::Delete_(void* ptr) {
        delete reinterpret_cast<T*>(ptr);
        ptr = nullptr;
    }

}

#endif // INCLUDE_GUARD_VARNODE_HPP