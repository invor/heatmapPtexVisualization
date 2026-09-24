#ifndef EngineFrontend_hpp
#define EngineFrontend_hpp

#include "Frame.hpp"

#include "OpenGL/GraphicsBackend.hpp"
#include "OpenGL/ResourceManager.hpp"
#include "TaskScheduler.hpp"
#include "WorldState.hpp"


class OpenGLEngineFrontend
{
public:
    OpenGLEngineFrontend();
    ~OpenGLEngineFrontend();

    void update(size_t update_frameID, double dt, int window_width, int window_height);

    void render(size_t render_frameID, double dt, int window_width, int window_height);

    /**
     * Grant access to world for the "driving" part of the application, e.g. editor, game, simulation etc.
     */
    EngineCore::WorldState& accessWorldState();

    /**
     * Grant access to frame data for the "driving" part of the application, e.g. editor, game, simulation etc.
     */
    EngineCore::Common::FrameManager<EngineCore::Common::Frame>& accessFrameManager();

    /**
     * Grant access to gpu resources for the "driving" part of the application, e.g. editor, game, simulation etc.
     */
    EngineCore::Graphics::OpenGL::ResourceManager& accessResourceManager();

    /**
     * Pass on an input action context to window handling in graphics backend
     */
    void addInputActionContext(EngineCore::Common::Input::InputActionContext const& input_action_context);

private:
    bool                    m_engine_started;
    std::mutex              m_engine_started_mutex;
    std::condition_variable m_engine_started_cVar;

    /**
     * Simple multi-thread task schedueler.
     */
    std::unique_ptr<EngineCore::Utility::TaskScheduler> m_task_scheduler;

    /**
     * The frame manager used to carry over snapshots of the world simulation to rendering.
     */
    std::unique_ptr<EngineCore::Common::FrameManager<EngineCore::Common::Frame>> m_frame_manager;

    /**
     * Graphics backend used for rendering.
     */
    std::unique_ptr<EngineCore::Graphics::OpenGL::GraphicsBackend> m_graphics_backend;

    /**
     * GPU resource manager.
     */
    std::unique_ptr<EngineCore::Graphics::OpenGL::ResourceManager> m_resource_manager;

    /**
     * Collection of all component manager that make up the world (state).
     */
    std::unique_ptr<EngineCore::WorldState> m_world_state;

};

#endif // !EngineFrontend_hpp
