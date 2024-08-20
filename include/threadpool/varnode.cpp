#include <threadpool/varnode.hpp>

namespace vsock {

    //////////////////////////////////////////////////////////////////////////////////
    // VarNode class defenition
    ////////////////////////////////////////////////////////////////////////////////

    VarNode::VarNode() :
        data_{ nullptr },
        delete_fnc_{ nullptr },
        hash_code_{ 0 }
    {}

    VarNode::VarNode(VarNode&& other) :
        data_{ std::exchange(other.data_,nullptr) },
        delete_fnc_{ std::exchange(other.delete_fnc_,nullptr) },
        hash_code_{ std::exchange(other.hash_code_,0) }
    {}

    VarNode& VarNode::operator=(VarNode&& rhs) {
        if (&rhs != this) {
            auto old = VarNode(std::move(rhs));
            Swap_(old);
        }
        return *this;
    }

    VarNode::~VarNode() {
        Drop();
    }

    void VarNode::Drop() {
        if (Empty()) {
            return;
        }
        delete_fnc_(data_);
        data_ = nullptr;
        delete_fnc_ = nullptr;
        hash_code_ = 0;
    }

    bool VarNode::Empty() const noexcept {
        return !data_;
    }

    void VarNode::Swap_(VarNode& other) noexcept {
        std::swap(other.data_, data_);
        std::swap(other.delete_fnc_, delete_fnc_);
        std::swap(other.hash_code_, hash_code_);
    }

}