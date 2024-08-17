#ifndef __D3D12MESH_HPP
#define __D3D12MESH_HPP

#include "mesh.hpp"

#include "d3d12core.hpp"

#include "gfxPrimitive.hpp"

#include <ranges>

namespace gfx {

namespace d3d12 {

/**
 * @brief A class representing a mesh in D3D12.     
 * It contains a vertex buffer and an index buffer, and it can bind them to the graphics pipeline's input assembler stage.    
 * Mesh::draw call count should be considered as draw call count, as unlike shader buffers, multiple input assembler settings cannot be bound at once.
 * @details It creates D3D12 resources for the vertex buffer and the index buffer in the constructors,     
 * and to upload the data to the GPU, auxiliary upload buffers are used.    
 * Since the upload buffers is not more needed after the data is uploaded,    
 * it is recommended to release them after the data is uploaded to the GPU.     
 * The release of the upload buffers is done by calling Mesh::completeInit.    
 * 
 * @note The constructors adds graphics command list a command to copy the data from the upload buffers to the default buffers,    
 * so you need to prepare the graphics command list to open and the data is uploaded to the GPU when the command list is executed.    
 * In consequence, waiting for the command list to be executed is required before using the mesh or releasing the upload buffers.
 * 
 * sample code:    
 * @code
 * core.preRender();    // to open the graphics command list
 * 
 * auto mesh = gfx::d3d12::Mesh( core, ctx, gfx::loadMesh("path/to/mesh.obj"),
 *     "myMesh1TmpUpBuf_vb", "myMesh1TmpUpBuf_ib"
 * );
 * 
 * core.postRender();    // to close the graphics command list and execute it
 * core.waitForGpu();    // to wait for the command list to be executed
 * 
 * mesh.completeInit(core);    // to release the upload buffers
 * @endcode
 * @note It is recommended to add Mesh object on DrawInfo object when rendering things with IScene implementation and IRenderer implementation,    
 * as binding and drawing logic of meshes is a lot easier than directly using the d3d12 resources.    
 * @see gfx::Mesh VertexBuffer Core Core::preRender Core::postRender Core::waitForGpu
 */
class Mesh {
public:
    using IndexCont = gfx::Mesh::IndexCont;

    /**
     * @brief Constructs a d3d12 mesh with a vertex buffer and an index buffer.     
     * It uploads the data to the GPU in same memory layout as the input buffers.
     * @param core The D3D12 core object.
     * @param ctx The D3D12 render context object.
     * @param vbuf The vertex buffer.
     * @param ibuf The index buffer.
     * @param vbUpIdx The upload buffer index for the vertex buffer, which is used to register and to pop the upload buffer from the core.
     * @param ibUpIdx The upload buffer index for the index buffer, which is used to register and to pop the upload buffer from the core.
     * @see VertexBuffer Core Core::addTmpUpBuf Core::popTmpUpBuf Core::popTmpUpBufs
     * @details It creates d3d12 resources internally and isn't in valid state until the gpu actually uploads the data.     
     * Look at the class details to be acknowledged about proper construction of the mesh.
     */
    Mesh( d3d12::Core& core, d3d12::D3D12RenderContext& ctx, const VertexBuffer& vbuf, const IndexCont& ibuf,
        Core::UpBufIdx vbUpIdx, Core::UpBufIdx ibUpIdx
    ) : vbView_(), ibView_(), vb_(), ib_(),
        vbUpIdx_(vbUpIdx), ibUpIdx_(ibUpIdx) {
        buildRes(core, ctx, vbuf, ibuf);
    }
    /**
     * @brief Constructs a d3d12 mesh with a vertex buffer and an index buffer.    
     * It uploads the data to the GPU in same memory layout as the input buffers.
     * @tparam R A range of 32-bit unsigned integers.
     * @param core The D3D12 core object.
     * @param ctx The D3D12 render context object.
     * @param vb The vertex buffer.
     * @param ib The index buffer.
     * @param vbUpIdx The upload buffer index for the vertex buffer, which is used to register and to pop the upload buffer from the core.
     * @param ibUpIdx The upload buffer index for the index buffer, which is used to register and to pop the upload buffer from the core.
     * @details It creates d3d12 resources internally and isn't in valid state until the gpu actually uploads the data.     
     * Look at the class details to be acknowledged about proper construction of the mesh.
     * @see VertexBuffer Core Core::addTmpUpBuf Core::popTmpUpBuf Core::popTmpUpBufs
     */
    template <std::ranges::range R>
    Mesh( d3d12::Core& core, d3d12::D3D12RenderContext& ctx, const VertexBuffer& vb, R&& ib,
        Core::UpBufIdx vbUpIdx, Core::UpBufIdx ibUpIdx
    ) : vbView_(), ibView_(), vb_(), ib_(),
        vbUpIdx_(vbUpIdx), ibUpIdx_(ibUpIdx) {
        buildRes(core, ctx, vb, IndexCont(std::begin(ib), std::end(ib)));
    }
    /**
     * @brief Constructs a d3d12 mesh with gfx::Mesh object.    
     * It uploads the data to the GPU in same memory layout as the vertex buffer and the index buffer of the input mesh.
     * @param core The D3D12 core object.
     * @param ctx The D3D12 render context object.
     * @param mesh The mesh object to copy the vertex buffer and the index buffer.
     * @param vbUpIdx The upload buffer index for the vertex buffer, which is used to register and to pop the upload buffer from the core.
     * @param ibUpIdx The upload buffer index for the index buffer, which is used to register and to pop the upload buffer from the core.
     * @details It creates d3d12 resources internally and isn't in valid state until the gpu actually uploads the data.     
     * Look at the class details to be acknowledged about proper construction of the mesh.
     * @see VertexBuffer Core Core::addTmpUpBuf Core::popTmpUpBuf Core::popTmpUpBufs
     */
    Mesh( d3d12::Core& core, d3d12::D3D12RenderContext& ctx, const gfx::Mesh& mesh,
        Core::UpBufIdx vbUpIdx, Core::UpBufIdx ibUpIdx
    ) : vbView_(), ibView_(), vb_(), ib_(),
        vbUpIdx_(vbUpIdx), ibUpIdx_(ibUpIdx) {
        buildRes(core, ctx, mesh.vb(), mesh.ib());
    }
    /**
     * @brief Pops the temporary upload buffers used to upload the data to the GPU from the core.
     * @param core The D3D12 core object.
     * @see Core Core::addTmpUpBuf Core::popTmpUpBuf Core::popTmpUpBufs
     */
    void completeInit(d3d12::Core& core) const;

    /**
     * @brief Binds the mesh to the graphics pipeline's input assembler stage.
     * @param pCmdList The graphics command list to bind the mesh.
     * @details It binds the internal vertex buffer and the index buffer to the graphics pipeline's input assembler stage.
     * @note It is recommended to add Mesh object on DrawInfo object when rendering things with IScene implementation and IRenderer implementation,    
     * as binding and drawing logic of meshes is a lot easier than directly using the d3d12 resources.  
     */
    void bind(ID3D12GraphicsCommandList* pCmdList) const;
    /**
     * @brief Draws the mesh.
     * @param pCmdList The graphics command list to draw the mesh.
     * @details The draw call is performed by calling `DrawIndexedInstanced` with the index buffer's index count.
     * @note It is recommended to add Mesh object on DrawInfo object when rendering things with IScene implementation and IRenderer implementation,    
     * as binding and drawing logic of meshes is a lot easier than directly using the d3d12 resources.  
     */
    void draw(ID3D12GraphicsCommandList* pCmdList) const;

private:
    void buildRes(d3d12::Core& core, d3d12::D3D12RenderContext& ctx, const VertexBuffer& vb, const IndexCont& ib);

    std::array<D3D12_VERTEX_BUFFER_VIEW, 1> vbView_;
    D3D12_INDEX_BUFFER_VIEW ibView_;
    wrl::ComPtr<ID3D12Resource> vb_;
    wrl::ComPtr<ID3D12Resource> ib_;
    Core::UpBufIdx vbUpIdx_;
    Core::UpBufIdx ibUpIdx_;
};

}   // namespace d3d12

}   // namespace gfx

#endif // __D3D12MESH_HPP