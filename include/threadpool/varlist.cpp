#include <threadpool/varlist.hpp>

namespace vsock {

    //////////////////////////////////////////////////////////////////////////////////
    // VarList class defenition
    ////////////////////////////////////////////////////////////////////////////////

    VarList::VarList(VarList&& other) :
        nodes_{ std::exchange(other.nodes_,nullptr) },
        size_{ std::exchange(other.size_,0) },
        capacity_{ std::exchange(other.capacity_,0) }
    {}

    VarList& VarList::operator=(VarList&& rhs) {
        if (&rhs != this) {
            auto old = VarList(std::move(rhs));
            Swap_(old);
        }
        return *this;
    }

    void VarList::Remove(std::size_t index) {
        if (index >= size_) {
            throw std::out_of_range("Out of range");
        }
        bool removed{ false };
        for (auto it = ((nodes_).get() + index); it != ((nodes_).get() + size_); ++it) {
            *it = std::move(*(it + 1));
            removed = true;
        }
        if (removed) {
            --size_;
        }
    }

    void VarList::Clear() noexcept {
        if (Empty()) {
            return;
        }
        nodes_.reset();
        capacity_ = 0;
        size_ = 0;
    }

    bool VarList::Empty() const noexcept {
        return size_ == 0;
    }

    std::size_t VarList::Size() const noexcept {
        return size_;
    }

    void VarList::Resize_(std::size_t new_capacity) {
        if (new_capacity <= capacity_) {
            return;
        }

        std::size_t future_capacity = std::max(new_capacity, capacity_ * 2);
        std::unique_ptr<VarNode[]> new_nodes = std::make_unique<VarNode[]>(future_capacity);

        auto from = new_nodes.get();
        for (auto it = nodes_.get(); it != (nodes_.get() + size_); ++it, ++from) {
            *from = std::move(*it);
        }

        capacity_ = future_capacity;
        nodes_.reset();
        nodes_ = std::move(new_nodes);
    }

    void VarList::Swap_(VarList& other) noexcept {
        std::swap(nodes_, other.nodes_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

}