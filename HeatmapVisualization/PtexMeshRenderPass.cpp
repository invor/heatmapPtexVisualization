#include "PtexMeshRenderPass.hpp"

#include "CameraComponent.hpp"
#include "FloatTableComponent.hpp"
#include "MaterialComponentManager.hpp"
#include "DynamicPtexMeshComponent.hpp"
#include "RenderTaskComponentManager.hpp"
#include "TransformComponentManager.hpp"

#include "DynamicPtexMeshSystems.hpp"

namespace EngineCore
{
    namespace Graphics
    {
        namespace OpenGL
        {
            void addPtexMeshRenderPass(Common::Frame& frame, WorldState& world_state, ResourceManager& resource_mngr)
            {
                struct PtexMeshPassData
                {
                    struct ModelData
                    {
                        Mat4x4 transform;

                        std::vector<DynamicPtexMeshComponentData::PtexParameters> ptex_params;
                        size_t                                                    lod_lvls;
                        std::vector<size_t>                                       lod_bin_sizes;
                        std::vector<uint32_t>                                     updatePatches_tgt;
                        std::vector<size_t>                                       update_bin_sizes;
                        std::vector<DynamicPtexMeshComponentData::TextureSlot>    availableTiles;
                        std::vector<size_t>                                       availableTiles_indexOffsets;

                        size_t                                         gaze_data_column_cnt;
                        size_t                                         gaze_data_row_cnt;
                        std::vector<float>                             gaze_data;
                    };

                    std::vector<ModelData> per_model_data;

                    Mat4x4 view_matrix;
                    Mat4x4 proj_matrix;
                };

                struct PtexMeshPassResources
                {
                    struct ModelResources {
                        WeakResource<glowl::GLSLProgram>  shader_prgm;
                        WeakResource<glowl::Mesh>         geometry;
                        WeakResource<glowl::BufferObject> ptex_parameters;

                        WeakResource<glowl::BufferObject> bindless_texture_handles;
                        WeakResource<glowl::BufferObject> bindless_image_handles;
                        WeakResource<glowl::BufferObject> bindless_mipmap_image_handles;

                        std::vector<ResourceID> textures;
                    };

                    std::vector<ModelResources> per_model_resources;
                };

                frame.addRenderPass<PtexMeshPassData, PtexMeshPassResources>("PtexMeshPass",
                    // data setup phase
                    [&world_state, &resource_mngr, &frame](PtexMeshPassData& data, PtexMeshPassResources& resources) {
                        auto& cam_mngr = world_state.get<CameraComponentManager>();
                        auto& mtl_mngr = world_state.get<MaterialComponentManager>();
                        auto& ptex_mngr = world_state.get<DynamicPtexMeshComponentManager>();
                        auto& transform_mngr = world_state.get<EngineCore::Common::TransformComponentManager>();
                        auto const& renderTask_mngr = world_state.get<RenderTaskComponentManager<Graphics::RenderTaskTags::PtexMesh>>();
                        auto& floatTable_mngr = world_state.get<EngineCore::Common::FloatTableComponentManager>();

                        // set camera matrices
                        Entity camera_entity = cam_mngr.getActiveCamera();
                        auto camera_idx = cam_mngr.getIndex(camera_entity).front();
                        auto camera_transform_idx = transform_mngr.getIndex(camera_entity);

                        data.view_matrix = glm::inverse(transform_mngr.getWorldTransformation(camera_transform_idx));

                        if (frame.m_window_width != 0 && frame.m_window_height != 0) {
                            cam_mngr.setAspectRatio(camera_idx, static_cast<float>(frame.m_window_width) / static_cast<float>(frame.m_window_height));
                            cam_mngr.updateProjectionMatrix(camera_idx);
                            data.proj_matrix = cam_mngr.getProjectionMatrix(camera_idx);
                        }

                        auto ptex_rts = renderTask_mngr.getComponentDataCopy();

                        for (auto& rt : ptex_rts)
                        {
                            size_t ptex_cmp_idx = ptex_mngr.getIndex(rt.entity);
                            auto const& ptex_cmp = ptex_mngr.getComponent(ptex_cmp_idx);

                            size_t floatTable_cmp_idx = floatTable_mngr.getIndex(rt.entity);
                            auto const& floatTable_cmp = floatTable_mngr.getComponent(floatTable_cmp_idx);

                            PtexMeshPassData::ModelData model_data;
                            model_data.transform = transform_mngr.getWorldTransformation(rt.cached_transform_idx);
                            model_data.ptex_params = *ptex_cmp.ptex_params_; //TODO this is an actual copy, should make it thread safe wrt rendering, but also expensive in update loop
                            model_data.lod_lvls = ptex_cmp.lod_lvls_;
                            model_data.lod_bin_sizes = ptex_cmp.lod_bin_sizes_;
                            model_data.updatePatches_tgt = ptex_cmp.updatePatches_tgt_;
                            model_data.update_bin_sizes = ptex_cmp.update_bin_sizes_;
                            model_data.availableTiles = ptex_cmp.availableTiles_uploadBuffer_;
                            model_data.availableTiles_indexOffsets = ptex_cmp.availableTiles_indexOffsets_;
                            model_data.gaze_data = floatTable_cmp.data;
                            model_data.gaze_data_column_cnt = floatTable_cmp.column_cnt;
                            model_data.gaze_data_row_cnt = floatTable_cmp.row_cnt;
                            data.per_model_data.emplace_back(std::move(model_data));

                            PtexMeshPassResources::ModelResources model_resources;
                            model_resources.geometry = resource_mngr.getMeshResource(rt.mesh);
                            model_resources.shader_prgm = resource_mngr.getShaderProgramResource(rt.shader_prgm);
                            model_resources.ptex_parameters = resource_mngr.getBufferResource(ptex_cmp.ptex_params_buffer_);
                            model_resources.bindless_texture_handles = resource_mngr.getBufferResource(ptex_cmp.bindless_texture_handles_);
                            model_resources.bindless_image_handles = resource_mngr.getBufferResource(ptex_cmp.bindless_image_handles_);
                            model_resources.bindless_mipmap_image_handles = resource_mngr.getBufferResource(ptex_cmp.bindless_mipmap_image_handles_);
                            model_resources.textures = mtl_mngr.getTextures(mtl_mngr.getIndex(rt.entity).front(), MaterialComponentData::TextureSemantic::ALBEDO);
                            resources.per_model_resources.push_back(model_resources);
                        }

                    },
                    // resource setup phase
                    [&world_state, &resource_mngr, &frame](PtexMeshPassData& data, PtexMeshPassResources& resources) {

                        try
                        {
                            auto& mtl_mngr = world_state.get<MaterialComponentManager>();
                            auto& renderTask_mngr = world_state.get<RenderTaskComponentManager<RenderTaskTags::PtexMesh>>();

                            for (size_t idx = 0; idx < resources.per_model_resources.size(); ++idx)
                            {
                                //resources.per_model_resources[idx].ptex_parameters = resource_mngr.updateBufferObject(resources.per_model_resources[idx].ptex_parameters.id, data.per_model_data[idx].ptex_params);
                                glMemoryBarrier(GL_ALL_BARRIER_BITS);

                                // check if texture arrays with ptex tiles are ready
                                bool textures_ready = true;
                                for (auto const& texture : resources.per_model_resources[idx].textures)
                                {
                                    auto tx_rsrc = resource_mngr.getTexture2DArray(texture);
                                    textures_ready &= (tx_rsrc.state == READY);
                                }

                                bool handle_buffers_ready =
                                    resources.per_model_resources[idx].bindless_texture_handles.state == READY &&
                                    resources.per_model_resources[idx].bindless_image_handles.state == READY &&
                                    resources.per_model_resources[idx].bindless_mipmap_image_handles.state == READY;

                                if (textures_ready && handle_buffers_ready)
                                {
                                    if (resources.per_model_resources[idx].bindless_texture_handles.resource->getByteSize() == 0 ||
                                        resources.per_model_resources[idx].bindless_image_handles.resource->getByteSize() == 0 ||
                                        resources.per_model_resources[idx].bindless_mipmap_image_handles.resource->getByteSize() == 0)
                                    {
                                        std::vector<GLuint64> texture_handles;
                                        std::vector<GLuint64> image_handles;
                                        std::vector<GLuint64> mipmap_image_handles;

                                        for (auto const& texture : resources.per_model_resources[idx].textures)
                                        {
                                            auto tx_rsrc = resource_mngr.getTexture2DArray(texture);
                                            texture_handles.push_back(tx_rsrc.resource->getTextureHandle());
                                            image_handles.push_back(tx_rsrc.resource->getImageHandle(0, GL_TRUE, 0));
                                            mipmap_image_handles.push_back(tx_rsrc.resource->getImageHandle(1, GL_TRUE, 0)); // we use two mipmap levels for ptex tiles so query both levels

                                            tx_rsrc.resource->makeResident();
                                            glMakeImageHandleResidentARB(image_handles.back(), GL_WRITE_ONLY);
                                            glMakeImageHandleResidentARB(mipmap_image_handles.back(), GL_WRITE_ONLY);
                                        }
                                        glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

                                        resources.per_model_resources[idx].bindless_texture_handles.resource->rebuffer(texture_handles);
                                        resources.per_model_resources[idx].bindless_image_handles.resource->rebuffer(image_handles);
                                        resources.per_model_resources[idx].bindless_mipmap_image_handles.resource->rebuffer(mipmap_image_handles);


                                        {
                                            //TODO check if vista textures need to be baked
                                            //TODO temporarily combine with texture handle buffer init to onyl execute once

                                            auto bakePtexVistaTiles_prgm_resource = resource_mngr.getShaderProgramResource("bakePtexVistaTiles_prgm");

                                            if (bakePtexVistaTiles_prgm_resource.state != READY)
                                            {
                                                // create shader for rendering the ptex surface
                                                std::string shader_root = "../HeatmapVisualization/shaders/";
                                                std::vector<EngineCore::Graphics::OpenGL::ResourceManager::ShaderFilename> shader_names
                                                    = std::initializer_list<EngineCore::Graphics::OpenGL::ResourceManager::ShaderFilename>{
                                                        { shader_root + "bakePtexVistaTiles_c.glsl", glowl::GLSLProgram::ShaderType::Compute }
                                                };
                                                bakePtexVistaTiles_prgm_resource = resource_mngr.createShaderProgram(
                                                    "bakePtexVistaTiles_prgm",
                                                    shader_names
                                                );
                                            }

                                            // get gaze point data buffer
                                            auto gaze_point_data_buffer = resource_mngr.getBufferResource("gaze_point_data_buffer");

                                            if (gaze_point_data_buffer.state != READY)
                                            {
                                                gaze_point_data_buffer = resource_mngr.createBufferObject(
                                                    "gaze_point_data_buffer",
                                                    GL_SHADER_STORAGE_BUFFER,
                                                    data.per_model_data[idx].gaze_data);
                                            }

                                            // Bake surface textures
                                            bakePtexVistaTiles_prgm_resource.resource->use();
                                            
                                            // Bind vertex and index buffer as storage buffer
                                            resources.per_model_resources[idx].geometry.resource->getVbos().front()->bindAs(GL_SHADER_STORAGE_BUFFER, 0);
                                            resources.per_model_resources[idx].geometry.resource->getIbo().bindAs(GL_SHADER_STORAGE_BUFFER, 1);
                                            
                                            resources.per_model_resources[idx].bindless_image_handles.resource->bind(2);

                                            gaze_point_data_buffer.resource->bind(4);

                                            bakePtexVistaTiles_prgm_resource.resource->setUniform(
                                                "gaze_data_column_cnt", static_cast<int>(data.per_model_data[idx].gaze_data_column_cnt));
                                            bakePtexVistaTiles_prgm_resource.resource->setUniform(
                                                "gaze_data_row_cnt", static_cast<int>(data.per_model_data[idx].gaze_data_row_cnt));

                                            bakePtexVistaTiles_prgm_resource.resource->setUniform("layers", 2048);
                                            
                                            int texture_base_idx = image_handles.size() - ((data.per_model_data[idx].lod_bin_sizes[data.per_model_data[idx].lod_lvls - 1]) / 2048);
                                            bakePtexVistaTiles_prgm_resource.resource->setUniform("texture_base_idx", texture_base_idx);
                                            
                                            GLint primitive_base_idx = 0;
                                            GLint remaining_dispatches = static_cast<GLint>(data.per_model_data[idx].ptex_params.size());
                                            GLint max_work_groups_z = 0;
                                            glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &max_work_groups_z);
                                            
                                            while (remaining_dispatches > 0)
                                            {
                                                GLuint dispatchs_cnt = (remaining_dispatches > max_work_groups_z) ? max_work_groups_z : remaining_dispatches;
                                            
                                                bakePtexVistaTiles_prgm_resource.resource->setUniform("primitive_base_idx", primitive_base_idx);
                                                glDispatchCompute(1, 1, dispatchs_cnt);
                                            
                                                if (remaining_dispatches > max_work_groups_z)
                                                {
                                                    remaining_dispatches -= max_work_groups_z;
                                                    primitive_base_idx += max_work_groups_z;
                                                }
                                                else
                                                {
                                                    remaining_dispatches = 0;
                                                }
                                            }
                                            
                                            // Build vista tiles mipmaps
                                            for (int i = texture_base_idx; i < resources.per_model_resources[idx].textures.size(); ++i)
                                            {
                                                auto ptex_texture_resource = resource_mngr.getTexture2DArray(resources.per_model_resources[idx].textures[i]);
                                            
                                                ptex_texture_resource.resource->bindTexture();
                                                glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
                                            }
                                        }
                                    }
                                }


                                // check if non-vista-level updates are queued
                                uint update_patches = 0;
                                for (auto const& bin_size : data.per_model_data[idx].update_bin_sizes) { update_patches += bin_size; }

                                auto updatePtexTiles_prgm_resource = resource_mngr.getShaderProgramResource("updatePtexTiles_prgm");

                                if (updatePtexTiles_prgm_resource.state != READY)
                                {
                                    // create shader for rendering the ptex surface
                                    std::string shader_root = "../HeatmapVisualization/shaders/";
                                    std::vector<EngineCore::Graphics::OpenGL::ResourceManager::ShaderFilename> shader_names
                                        = std::initializer_list<EngineCore::Graphics::OpenGL::ResourceManager::ShaderFilename>{
                                            { shader_root + "updatePtexTiles_c.glsl", glowl::GLSLProgram::ShaderType::Compute }
                                    };
                                    updatePtexTiles_prgm_resource = resource_mngr.createShaderProgram(
                                        "updatePtexTiles_prgm",
                                        shader_names
                                    );
                                }

                                auto updatePtexTilesMipmaps_prgm_resource = resource_mngr.getShaderProgramResource("updatePtexTilesMipmaps_prgm");

                                if (updatePtexTilesMipmaps_prgm_resource.state != READY)
                                {
                                    // create shader for rendering the ptex surface
                                    std::string shader_root = "../HeatmapVisualization/shaders/";
                                    std::vector<EngineCore::Graphics::OpenGL::ResourceManager::ShaderFilename> shader_names
                                        = std::initializer_list<EngineCore::Graphics::OpenGL::ResourceManager::ShaderFilename>{
                                            { shader_root + "updatePtexTilesMipmaps_c.glsl", glowl::GLSLProgram::ShaderType::Compute }
                                    };
                                    updatePtexTilesMipmaps_prgm_resource = resource_mngr.createShaderProgram(
                                        "updatePtexTilesMipmaps_prgm",
                                        shader_names
                                    );
                                }

                                auto setPtexVistaTiles_prgm_resource = resource_mngr.getShaderProgramResource("setPtexVistaTiles_prgm");

                                if (setPtexVistaTiles_prgm_resource.state != READY)
                                {
                                    // create shader for rendering the ptex surface
                                    std::string shader_root = "../HeatmapVisualization/shaders/";
                                    std::vector<EngineCore::Graphics::OpenGL::ResourceManager::ShaderFilename> shader_names
                                        = std::initializer_list<EngineCore::Graphics::OpenGL::ResourceManager::ShaderFilename>{
                                            { shader_root + "setPtexVistaTiles_c.glsl", glowl::GLSLProgram::ShaderType::Compute }
                                    };
                                    setPtexVistaTiles_prgm_resource = resource_mngr.createShaderProgram(
                                        "setPtexVistaTiles_prgm",
                                        shader_names
                                    );
                                }

                                // get gaze point data buffer
                                auto gaze_point_data_buffer = resource_mngr.getBufferResource("gaze_point_data_buffer");
                                
                                if (gaze_point_data_buffer.state != READY)
                                {
                                    gaze_point_data_buffer = resource_mngr.createBufferObject(
                                        "gaze_point_data_buffer",
                                        GL_SHADER_STORAGE_BUFFER,
                                        data.per_model_data[idx].gaze_data);
                                }

                                // check availabilty of resources and abort update if any resource is not ready
                                bool ptex_update_ready =
                                    resources.per_model_resources[idx].bindless_image_handles.state == ResourceState::READY &&
                                    resources.per_model_resources[idx].ptex_parameters.state == ResourceState::READY;

                                if (ptex_update_ready && (update_patches > 0))
                                {
                                    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

                                    // upload update information to GPU

                                    auto updatePatches_buffer = resource_mngr.getBufferResource("updatePatches_buffer");
                                    if (updatePatches_buffer.id == resource_mngr.invalidResourceID())
                                    {
                                        updatePatches_buffer = resource_mngr.createBufferObject(
                                            "updatePatches_buffer",
                                            GL_SHADER_STORAGE_BUFFER,
                                            data.per_model_data[idx].updatePatches_tgt
                                        );
                                    }
                                    else
                                    {
                                        updatePatches_buffer.resource->rebuffer(data.per_model_data[idx].updatePatches_tgt);
                                    }

                                    auto availableTiles_buffer = resource_mngr.getBufferResource("availableTiles_buffer");
                                    if (availableTiles_buffer.id == resource_mngr.invalidResourceID())
                                    {
                                        availableTiles_buffer = resource_mngr.createBufferObject(
                                            "availableTiles_buffer",
                                            GL_SHADER_STORAGE_BUFFER,
                                            data.per_model_data[idx].availableTiles
                                        );
                                    }
                                    else
                                    {
                                        availableTiles_buffer.resource->rebuffer(data.per_model_data[idx].availableTiles);
                                    }

                                    glMemoryBarrier(GL_ALL_BARRIER_BITS);
                                    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                                    // TODO per LOD level dispatch computes
                                    int update_patch_offset = 0;
                                    int tile_size_multiplier = std::pow(2, data.per_model_data[idx].lod_lvls - 1);

                                    for (int i = 0; i < static_cast<int>(data.per_model_data[idx].update_bin_sizes.size()) - 1; ++i)
                                    {
                                        // available textures should always be >= update patches per LOD
                                        //assert(m_bricks[index].m_ptex_availableTiles_bin_sizes[i] >= m_bricks[index].m_ptex_update_bin_sizes[i]);

                                        uint32_t bin_size = data.per_model_data[idx].update_bin_sizes[i];
                                        if (bin_size > 0)
                                        {
                                            float texture_lod = static_cast<float>(i);
                                            int texture_slot_offset = static_cast<int>(data.per_model_data[idx].availableTiles_indexOffsets[i]);

                                            // set GLSL program
                                            updatePtexTiles_prgm_resource.resource->use();

                                            // Bind vertex and index buffer as storage buffer
                                            resources.per_model_resources[idx].geometry.resource->getVbos().front()->bindAs(GL_SHADER_STORAGE_BUFFER, 0);
                                            resources.per_model_resources[idx].geometry.resource->getIbo().bindAs(GL_SHADER_STORAGE_BUFFER, 1);

                                            resources.per_model_resources[idx].bindless_image_handles.resource->bind(2);
                                            resources.per_model_resources[idx].ptex_parameters.resource->bind(3);

                                            gaze_point_data_buffer.resource->bind(4);

                                            updatePatches_buffer.resource->bind(6);
                                            availableTiles_buffer.resource->bind(7);

                                            updatePtexTiles_prgm_resource.resource->setUniform("texture_lod", texture_lod + 1.0f); //TODO more accurate computation of fitting mipmap level for source textures
                                            updatePtexTiles_prgm_resource.resource->setUniform("update_patch_offset", update_patch_offset);
                                            updatePtexTiles_prgm_resource.resource->setUniform("texture_slot_offset", texture_slot_offset);

                                            updatePtexTiles_prgm_resource.resource->setUniform(
                                                "gaze_data_column_cnt", static_cast<int>(data.per_model_data[idx].gaze_data_column_cnt));
                                            updatePtexTiles_prgm_resource.resource->setUniform(
                                                "gaze_data_row_cnt", static_cast<int>(data.per_model_data[idx].gaze_data_row_cnt));

                                            {
                                                auto gl_err = glGetError();
                                                if (gl_err != GL_NO_ERROR)
                                                    std::cerr << "GL error before dispatch: " << gl_err << std::endl;
                                            }

                                            //GLint data;
                                            //glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &data);
                                            //std::cout << "GL_MAX_COMPUTE_WORK_GROUP_COUNT: " << data<<std::endl;

                                            glDispatchCompute(tile_size_multiplier, tile_size_multiplier, std::min(65535u, bin_size));
                                            //glDispatchCompute(tile_size_multiplier, tile_size_multiplier, bin_size);
                                            //updatePtexTiles_prgm_resource.resource->dispatchCompute(tile_size_multiplier, tile_size_multiplier, bin_size);

                                            {
                                                auto gl_err = glGetError();
                                                if (gl_err != GL_NO_ERROR)
                                                    std::cerr << "GL error after dispatch: " << gl_err << std::endl;
                                            }

                                            glMemoryBarrier(GL_ALL_BARRIER_BITS);

                                        }

                                        update_patch_offset += bin_size;

                                        tile_size_multiplier /= 2;
                                    }

                                    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

                                    {
                                        // Assign vista tiles (no recomutation necessary)
                                        if (data.per_model_data[idx].update_bin_sizes.back() > 0)
                                        {
                                            setPtexVistaTiles_prgm_resource.resource->use();
                                        
                                            resources.per_model_resources[idx].ptex_parameters.resource->bind(3);
                                            updatePatches_buffer.resource->bind(6);
                                        
                                            setPtexVistaTiles_prgm_resource.resource->setUniform("update_patch_offset", update_patch_offset);
                                        
                                            int texture_base_idx = resources.per_model_resources[idx].textures.size() - (data.per_model_data[idx].lod_bin_sizes.back() / 2048);
                                            unsigned int vista_patch_cnt = static_cast<unsigned int>(data.per_model_data[idx].update_bin_sizes.back());
                                            setPtexVistaTiles_prgm_resource.resource->setUniform("texture_base_idx", texture_base_idx);
                                            setPtexVistaTiles_prgm_resource.resource->setUniform("vista_patch_cnt", vista_patch_cnt);
                                        
                                            glDispatchCompute(1, 1, (vista_patch_cnt / 32) + 1);
                                        
                                            //	for (auto texture : m_bricks[index].m_ptex_textures)
                                                //	{
                                                //		auto tex_rsrc = GEngineCore::resourceManager().getTexture2DArray(texture);
                                                //		tex_rsrc.resource->updateMipmaps();
                                                //	}
                                        }
                                        
                                        glMemoryBarrier(GL_ALL_BARRIER_BITS);
                                        
                                        //  // copy updated ptex params to cpu
                                        //  size_t byte_size = ptex_parameters_resource.resource->getSize();
                                        //  //m_bricks[index].m_mesh_ptex_params.resize(quad_cnt); // 1 set of ptex params per quad
                                        //  ptex_parameters_resource.resource->bind();
                                        //  GLvoid* ptex_params = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, byte_size, GL_MAP_READ_BIT);
                                        //  memcpy(m_bricks[index].m_mesh_ptex_params.data(), ptex_params, byte_size);
                                        //  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
                                    }

                                    {
                                        resources.per_model_resources[idx].bindless_texture_handles.resource->bind(0);
                                        resources.per_model_resources[idx].bindless_mipmap_image_handles.resource->bind(1);
                                        resources.per_model_resources[idx].ptex_parameters.resource->bind(2);
                                        updatePatches_buffer.resource->bind(3);
                                    
                                        int update_patch_offset = 0;
                                        int tile_size_multiplier = std::pow(2, data.per_model_data[idx].lod_lvls - 1);
                                    
                                        updatePtexTilesMipmaps_prgm_resource.resource->use();
                                    
                                        // only update mipmaps of non-vista level tiles
                                        for (int i = 0; i < static_cast<int>(data.per_model_data[idx].update_bin_sizes.size()) - 1; ++i)
                                        {
                                            uint32_t bin_size = data.per_model_data[idx].update_bin_sizes[i];
                                            if (bin_size > 0)
                                            {
                                                updatePtexTilesMipmaps_prgm_resource.resource->setUniform("update_patch_offset", update_patch_offset);

                                                glDispatchCompute(tile_size_multiplier, tile_size_multiplier, bin_size);
                                            }
                                    
                                            tile_size_multiplier /= 2;
                                            update_patch_offset += bin_size;
                                        }
                                    }

                                }
                            }

                        }
                        catch (const glowl::BaseException& e)
                        {
                            std::cerr << e.what();
                        }
                    },
                    // execute phase
                    [&frame](PtexMeshPassData const& data, PtexMeshPassResources const& resources) {

                        glMemoryBarrier(GL_ALL_BARRIER_BITS);

                        glEnable(GL_CULL_FACE);
                        glFrontFace(GL_CCW);
                        glEnable(GL_DEPTH_TEST);

                        //glDisable(GL_BLEND);
                        //glDisable(GL_CULL_FACE);

                        //resources.m_render_target.resource->bind();
                        glBindFramebuffer(GL_FRAMEBUFFER, 0);
                        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                        int width = frame.m_window_width;
                        int height = frame.m_window_height;
                        glViewport(0, 0, width, height);

                        // bind global resources?

                        auto gl_err = glGetError();
                        if (gl_err != GL_NO_ERROR)
                            std::cerr << "GL error in geometry pass execution: " << gl_err << std::endl;

                        size_t model_idx = 0;
                        for (auto& model_resources : resources.per_model_resources)
                        {
                            if (model_resources.shader_prgm.state != READY
                                || model_resources.geometry.state != READY
                                || model_resources.ptex_parameters.state != READY
                                || model_resources.bindless_texture_handles.state != READY)
                            {
                                continue;
                            }


                            // TODO use program
                            model_resources.shader_prgm.resource->use();

                            //glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                            //glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                            glMemoryBarrier(GL_ALL_BARRIER_BITS);

                            // TODO upload per program data
                            model_resources.shader_prgm.resource->setUniform("projection_matrix", data.proj_matrix);

                            // TODO bind per object ptex resources
                            model_resources.bindless_texture_handles.resource->bind(2);
                            model_resources.ptex_parameters.resource->bind(3);
                            // TODO upload index of transform matrix?
                            //prgm->setUniform("transform_idx", batch_data.transform_idx);
                            Mat4x4 model_matrix = data.per_model_data[model_idx].transform;
                            //model_matrix = Mat4x4(1.0f);
                            Mat4x4 model_view_matrix = data.view_matrix * model_matrix;
                            model_resources.shader_prgm.resource->setUniform("model_view_matrix", model_view_matrix);

                            if (model_resources.geometry.resource->getPrimitiveType() == GL_PATCHES)
                                glPatchParameteri(GL_PATCH_VERTICES, 4);
                            // TODO submit draw call
                            model_resources.geometry.resource->draw();
                        }
                    }
                );
            }

        }
    }
}
