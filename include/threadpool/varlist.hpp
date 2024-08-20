#ifndef INCLUDE_GUARD_VARLIST_HPP
#define INCLUDE_GUARD_VARLIST_HPP

#include <threadpool/varnode.hpp>

namespace vsock {

    //////////////////////////////////////////////////////////////////////////////////
    // VarList class declaration
    ////////////////////////////////////////////////////////////////////////////////

    class VarList {
    public:

        VarList() = default;
        VarList(VarList&&) = default;
        VarList& operator=(VarList&&) = default;

        template<typename T>
        void Add(T&& var);

        template<typename T>
        T& Emplace(T&& var);

        template<typename T>
        T& Get(std::size_t index);

        template<typename T>
        const T& Get(std::size_t index) const;

        void Remove(std::size_t index);

        void Clear() noexcept;
        bool Empty() const noexcept;
        std::size_t Size() const noexcept;

    private:

        std::unique_ptr<VarNode[]> nodes_{ nullptr };
        std::size_t size_{ 0 };
        std::size_t capacity_{ 0 };

        void Resize_(std::size_t new_capacity);

    };

    //////////////////////////////////////////////////////////////////////////////////
    // VarList class defenition (template methods)
    ////////////////////////////////////////////////////////////////////////////////    

    template<typename T>
    inline void VarList::Add(T&& var) {
        Resize_(size_ + 1);
        (nodes_.get() + (size_++))->Put(std::forward<T>(var));
    }

    template<typename T>
    inline T& VarList::Emplace(T&& var) {
        Resize_(size_ + 1);
        return (nodes_.get() + (size_++))->Emplace(std::forward<T>(var));
    }

    template<typename T>
    inline T& VarList::Get(std::size_t index) {
        if (index >= size_) {
            throw std::out_of_range("Out of range");
        }
        return ((nodes_).get() + index)->Get<T>();
    }

    template<typename T>
    inline  const T& VarList::Get(std::size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("Out of range");
        }
        return ((nodes_).get() + index)->Get<T>();
    }

}

#endif // INCLUDE_GUARD_VARLIST_HPP