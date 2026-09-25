#include "OpenGLEngineFrontend.hpp"

#include <thread>
#include <vector>
#include <future>
#include <chrono>

#include "AnimationSystems.hpp"
#include "CameraComponent.hpp"
#include "FloatTableComponent.hpp"
#include "GeometryBakery.hpp"
#include "gltfAssetComponentManager.hpp"
#include "MeshComponentManager.hpp"
#include "NameComponentManager.hpp"
#include "PointlightComponent.hpp"
#include "DynamicPtexMeshComponent.hpp"
#include "PtexMeshRenderPass.hpp"
#include "RenderTaskComponentManager.hpp"
#include "SunlightComponentManager.hpp"
#include "TaskScheduler.hpp"

#include "InputEvent.hpp"

#include <GLFW/glfw3.h>


        OpenGLEngineFrontend::OpenGLEngineFrontend()
            : m_engine_started(false),
            m_task_scheduler(std::make_unique<EngineCore::Utility::TaskScheduler>()),
            m_frame_manager(std::make_unique<EngineCore::Common::FrameManager<EngineCore::Common::Frame>>()),
            m_graphics_backend(std::make_unique<EngineCore::Graphics::OpenGL::GraphicsBackend>()),
            m_resource_manager(std::make_unique<EngineCore::Graphics::OpenGL::ResourceManager>()),
            m_world_state(std::make_unique<EngineCore::WorldState>())
        {
            m_world_state->add<EngineCore::Graphics::CameraComponentManager>(std::make_unique<EngineCore::Graphics::CameraComponentManager>());
            m_world_state->add<EngineCore::Common::FloatTableComponentManager>(std::make_unique<EngineCore::Common::FloatTableComponentManager>());
            m_world_state->add<EngineCore::Graphics::GltfAssetComponentManager>(std::make_unique<EngineCore::Graphics::GltfAssetComponentManager>());
            m_world_state->add<EngineCore::Graphics::MaterialComponentManager>(std::make_unique<EngineCore::Graphics::MaterialComponentManager>());
            m_world_state->add<EngineCore::Graphics::MeshComponentManager<EngineCore::Graphics::OpenGL::ResourceManager>>(std::make_unique<EngineCore::Graphics::MeshComponentManager<EngineCore::Graphics::OpenGL::ResourceManager>>(m_resource_manager.get()));
            m_world_state->add<EngineCore::Common::NameComponentManager>(std::make_unique<EngineCore::Common::NameComponentManager>());
            m_world_state->add<EngineCore::Graphics::PointlightComponentManager>(std::make_unique<EngineCore::Graphics::PointlightComponentManager>());
            m_world_state->add<EngineCore::Graphics::DynamicPtexMeshComponentManager>(std::make_unique < EngineCore::Graphics::DynamicPtexMeshComponentManager>());
            m_world_state->add<EngineCore::Graphics::SunlightComponentManager>(std::make_unique<EngineCore::Graphics::SunlightComponentManager>());
            m_world_state->add<EngineCore::Graphics::RenderTaskComponentManager<EngineCore::Graphics::RenderTaskTags::StaticMesh>>(std::make_unique<EngineCore::Graphics::RenderTaskComponentManager<EngineCore::Graphics::RenderTaskTags::StaticMesh>>());
            m_world_state->add<EngineCore::Graphics::RenderTaskComponentManager<EngineCore::Graphics::RenderTaskTags::PtexMesh>>(std::make_unique<EngineCore::Graphics::RenderTaskComponentManager<EngineCore::Graphics::RenderTaskTags::PtexMesh>>());
            m_world_state->add<EngineCore::Common::TransformComponentManager>(std::make_unique<EngineCore::Common::TransformComponentManager>());
            
            // start task schedueler with 1 thread
            m_task_scheduler->run(12);
        }

        OpenGLEngineFrontend::~OpenGLEngineFrontend()
        {
            m_task_scheduler->stop();
        }

        void OpenGLEngineFrontend::update(size_t udpate_frameID, double dt, int window_width, int window_height)
        {
            // update world
            auto active_systems = m_world_state->getSystems();
            for (auto& system : active_systems)
            {
                auto& world_state = *m_world_state.get();
                system(world_state, dt, *m_task_scheduler);
                //task_scheduler->submitTask(
                //    [&world_state, dt, system]() {
                //        system(world_state, dt);
                //    }
                //);
            }

            // finalize engine update by creating a new frame
            EngineCore::Common::Frame new_frame;

            new_frame.m_frameID = udpate_frameID;
            new_frame.m_simulation_dt = dt;
            new_frame.m_window_width = window_width;
            new_frame.m_window_height = window_height;

            auto& camera_mngr = m_world_state->get<EngineCore::Graphics::CameraComponentManager>();
            auto& entity_mngr = m_world_state->accessEntityManager();
            auto& transform_mngr = m_world_state->get<EngineCore::Common::TransformComponentManager>();

            Entity camera_entity = camera_mngr.getActiveCamera();

            if (camera_entity != entity_mngr.invalidEntity())
            {
                auto camera_idx = camera_mngr.getIndex(camera_entity).front();

                size_t camera_transform_idx = transform_mngr.getIndex(camera_entity);
                new_frame.m_view_matrix = glm::inverse(transform_mngr.getWorldTransformation(camera_transform_idx));
                new_frame.m_projection_matrix = camera_mngr.getProjectionMatrix(camera_idx);
                new_frame.m_fovy = camera_mngr.getFovy(camera_idx);
                new_frame.m_aspect_ratio = camera_mngr.getAspectRatio(camera_idx);
                new_frame.m_exposure = camera_mngr.getExposure(camera_idx);

                EngineCore::Common::Frame& update_frame = m_frame_manager->setUpdateFrame(std::move(new_frame));

                //Graphics::OpenGL::setupBasicForwardRenderingPipeline(update_frame, *m_world_state, *m_resource_manager);
                //EngineCore::Graphics::OpenGL::setupBasicDeferredRenderingPipeline(update_frame, *m_world_state, *m_resource_manager);
                EngineCore::Graphics::OpenGL::addPtexMeshRenderPass(update_frame, *m_world_state, *m_resource_manager);
                
                m_frame_manager->swapUpdateFrame();
            }
        }

        void OpenGLEngineFrontend::render(size_t render_frameID, double dt, int window_width, int window_height)
        {
            // Perform single execution tasks
            //processSingleExecutionTasks();

            auto gl_err = glGetError();
            if (gl_err != GL_NO_ERROR)
                std::cerr << "GL error after single exection tasks: " << gl_err << std::endl;

            // Perform resource manager async tasks
            m_resource_manager->executeRenderThreadTasks();

            gl_err = glGetError();
            if (gl_err != GL_NO_ERROR)
                std::cerr << "GL error after resource manager tasks: " << gl_err << std::endl;

            // TODO try getting update for render frame ?
            
            // Get current frame for rendering
            auto& frame = m_frame_manager->getRenderFrame();

            std::vector<double> setup_resource_timings;
            setup_resource_timings.reserve(frame.m_render_passes.size());
            // Call buffer phase for each render pass
            for (auto& render_pass : frame.m_render_passes)
            {
                auto t0 = glfwGetTime();

                render_pass.setupResources();

                auto t1 = glfwGetTime();
                setup_resource_timings.emplace_back(t1 - t0);
            }

            gl_err = glGetError();
            if (gl_err != GL_NO_ERROR)
                std::cerr << "GL error after resource setup of frame " << frame.m_frameID << " : " << gl_err << std::endl;

            std::vector<double> render_timings;
            render_timings.reserve(frame.m_render_passes.size());
            // Call execution phase for each render pass
            for (auto& render_pass : frame.m_render_passes)
            {
                auto t0 = glfwGetTime();

                render_pass.execute();

                auto t1 = glfwGetTime();
                render_timings.emplace_back(t1 - t0);
            }

            gl_err = glGetError();
            if (gl_err != GL_NO_ERROR)
                std::cerr << "GL error after execution of frame " << frame.m_frameID << " : " << gl_err << std::endl;

            m_frame_manager->swapRenderFrame();
        }

        EngineCore::WorldState & OpenGLEngineFrontend::accessWorldState()
        {
            return (*m_world_state.get());
        }

        EngineCore::Common::FrameManager<EngineCore::Common::Frame>& OpenGLEngineFrontend::accessFrameManager()
        {
            return (*m_frame_manager.get());
        }

        EngineCore::Graphics::OpenGL::ResourceManager& OpenGLEngineFrontend::accessResourceManager()
        {
            return (*m_resource_manager.get());
        }

        void OpenGLEngineFrontend::addInputActionContext(EngineCore::Common::Input::InputActionContext const & input_action_context)
        {
            m_graphics_backend->addInputActionContext(input_action_context);
        }