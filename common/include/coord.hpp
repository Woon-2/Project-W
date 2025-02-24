#ifndef __COORD_HPP
#define __COORD_HPP

#include "mathUtil.hpp"

#include <utility>
#include <vector>
#include <memory>
#include <type_traits>
#include <stack>

#include <cassert>

#include "gfxExcept.hpp"

#include "config.hpp"

namespace gfx {

/**
 * @brief A namespace for coordinate system management.    
 * A coordinate system determines how to transform the coordinates    
 * with respect to the relativeness to other coordinate systems.
 */
namespace coord {

/**
 * @brief A class representing a coordinate system.    
 * Multiple coordinate systems can be nested as a tree structure.     
 * Each coordinate system has its own local transformation matrix, applied leaf to the root to get the total transformation matrix.
 * @note The total transformation which is obtained by applying the local transformation from the leaf to the root,     
 * is only calculated when the System is traversed with System::traverse.     
 * System::xform is expected to return the total transformation matrix, but it is just cached result of the calculation of System::traverse.
 * @note It is important to know how the special member functions of System works.    
 * 
 * Copying just copies the local and cached total transformation matrix and the pointer to the parent, ignoring the children,     
 * with a side effect for the parent to add the copied System's pointer to its children.    
 * 
 * Moving copies the local and cached total transformation matrix, the pointer to the parent, and moves the children to the moved system,     
 * with three side effects one for the parent to remove the moved-from System's pointer and add the moved-to System's pointer to its children,   
 * and another for the children to change the parent pointer to the moved-to System,     
 * and the last for the moved-from System's members are cleared as default constructed values.
 * 
 * Destruction removes the being-destructed System's pointer from the parent's children and changes the parent pointer of the children to nullptr,     
 * making the children's local transform to be the same as the being-destructed System's cached total transform multiplied by their local transform.
 */
class System {
public:
    /**
     * @brief Creates a coordinate system with an identity local transformation matrix, no children, and no parent.
     */
    System()
        : localXform_(), cachedTotalXform_(), children_(), parent_(nullptr) {}

    /**
     * @brief Copys other coordinate system with the same local and cached total transformation matrices.     
     * It doesn't copy the children from the source coordinate system.
     * @param sys The other coordinate system to copy.
     * @note The effects of copying can be described as follows:     
     * - The local transformation matrix is copied from the source System.
     * - The cached total transformation matrix is copied from the source System.
     * - The parent pointer is copied from the source System.
     * - The children are not copied from the source System.
     * - The created System's pointer is added to the children of the parent.
     */
    System(const System& sys);
    /**
     * @brief Copies the other coordinate system with the same local and cached total transformation matrices.     
     * It doesn't copy the children from the source coordinate system.
     * @param sys The other coordinate system to copy.
     * @return The reference of the copied coordinate system.
     * @note The effects of copying can be described as follows:     
     * - The local transformation matrix is copied from the source System.
     * - The cached total transformation matrix is copied from the source System.
     * - The parent pointer is copied from the source System.
     * - The children are not copied from the source System.
     * - The created System's pointer is added to the children of the parent.
     */
    System& operator=(const System& sys);
    /**
     * @brief Moves the other coordinate system with the same local and cached total transformation matrices.     
     * It moves the children from the source coordinate system.
     * @param sys The other coordinate system to move.
     * @note The effects of moving can be described as follows:     
     * - The local transformation matrix is copied from the source System.
     * - The cached total transformation matrix is copied from the source System.
     * - The parent pointer is copied from the source System.
     * - The children are moved from the source System.
     * - The created System's pointer is added to the children of the parent     
     *   and the source System's pointer is removed from the children of the parent.
     * - The source System's members are cleared as default constructed values.
     */
    System(System&& sys) noexcept;
    /**
     * @brief Moves the other coordinate system with the same local and cached total transformation matrices.     
     * It moves the children from the source coordinate system.
     * @param sys The other coordinate system to move.
     * @return The reference of the moved coordinate system.
     * @note The effects of moving can be described as follows:     
     * - The local transformation matrix is copied from the source System.
     * - The cached total transformation matrix is copied from the source System.
     * - The parent pointer is copied from the source System.
     * - The children are moved from the source System.
     * - The created System's pointer is added to the children of the parent    
     *   and the source System's pointer is removed from the children of the parent.
     * - The source System's members are cleared as default constructed values.
     */
    System& operator=(System&& sys) noexcept;
    /**
     * @brief Destructs the coordinate system.     
     * It removes the pointer to the parent from the parent's children and changes the parent pointer of the children to nullptr.
     * @note The effects of destruction can be described as follows:
     * - The being-destructed System's pointer is removed from the parent's children.
     * - The parent pointer of the children is changed to nullptr.
     * - The children's local transform is set to the being-destructed System's cached total transform multiplied by their local transform.
     */
    ~System();

    /**
     * @brief Applies a transformation to the coordinate system.    
     * It accumulates the transformation to the local transformation matrix via matrix multiplication.
     * @param xform The transformation matrix to apply.
     * @see System::operator<< System::localXform System::xform
     */
    void accXform(const mu::Mat4x4& xform) NOEXCEPT {
        localXform_ *= xform;
    }
    /**
     * @brief Calculates the transposed local transformation matrix and gets it.
     * @return `mu::Mat4x4` The transposed local transformation matrix.
     * @see System::localXform System::xform System::accXform System::operator<<
     */
    mu::Mat4x4 MU_CALLCONV localXformTransposed() const NOEXCEPT {
        return mu::transpose(localXform_);
    }

    /**
     * @brief Gets the local transformation matrix.
     * @return `const mu::Mat4x4&` The reference of the local transformation matrix.
     * @see System::localXformTransposed System::xform System::accXform System::operator<<
     */
    const mu::Mat4x4& localXform() const NOEXCEPT {
        return localXform_;
    }
    void MU_CALLCONV setLocalXform(mu::Mat4x4 xform) NOEXCEPT {
        localXform_ = xform;
    }
    /**
     * @brief Gets the total transformation matrix.
     * @return `const mu::Mat4x4&` The reference of the total transformation matrix.
     * @details The total transformation matrix is the cached result of the calculation of System::traverse.
     * @see System::traverse System::localXform System::localXformTransposed System::accXform System::operator<<
     */
    const mu::Mat4x4& xform() const NOEXCEPT {
        return cachedTotalXform_;
    }
    /**
     * @brief Traverses the coordinate system to calculate the total transformation matrix.     
     * As every child's total transformation is calculated during the traversal,    
     * calling this function from the root is enough to calculate the total transformation matrix of the whole tree.
     * @param parentXform The parent's total transformation matrix, if it is not the root.
     * @details the parentXform is defaulted to the identity matrix, as the root doesn't have a parent.     
     * If you want traverse from a node which is not the root, you should pass the parent's total transformation matrix.     
     * 
     * The total transformation matrix is calculated by applying the local transformation matrix from the leaf to the root.
     * @note It doesn't utilizes the stored parent pointer,     
     * since calculating the parent's total transformation from a child is inversion of dependency    
     * and it loses constness of the parent pointer.    
     * So, it is required to pass the parent's total transformation matrix explicitly.
     * @see System::xform System::localXform System::localXformTransposed System::accXform System::operator<<
     */
    void traverse(const mu::Mat4x4& parentXform = mu::Mat4x4()) NOEXCEPT;
    /**
     * @brief Sets the parent of the coordinate system.
     * @details It sets the parent pointer to given argument and adds the coordinate system's pointer to the parent's children.
     * @param parent The parent coordinate system to set.
     * @see System::parent System::popChild
     */
    void setParent(System* parent);
    /**
     * @brief Gets the parent of the coordinate system.     
     * It returns nullptr if the coordinate system doesn't have a parent.
     * @return `const System*` The pointer to the parent coordinate system.
     * @see System::setParent System::popChild
     */
    const System* parent() const NOEXCEPT {
        return parent_;
    }
    /**
     * @brief Pops a child from the coordinate system.     
     * If the child doesn't exist in the children, it does nothing.
     * @param child The child to pop.
     * @see System::setParent System::parent
     */
    void popChild(const System* child) {
        std::erase_if( children_, [child](const auto& c) {
            return c == child;
        } );
    }
    /**
     * @brief Applies a transformation to the coordinate system.    
     * It accumulates the transformation to the local transformation matrix via matrix multiplication.
     * @param xform The transformation matrix to apply.
     * @see accXform System::localXform System::xform
     */
    friend System& operator<<(System& cs, const mu::Mat4x4& xform) {
        cs.accXform(xform);
        return cs;
    }

private:
    mu::Mat4x4 localXform_;
    mu::Mat4x4 cachedTotalXform_;
    std::vector<System*> children_;
    mutable const System* parent_;
};

}   // namespace coord

}   // namespace gfx

#endif