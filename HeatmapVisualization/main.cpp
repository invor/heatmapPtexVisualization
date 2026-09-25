#include <chrono>
#include <thread>
#include <future>

#include "InputEvent.hpp"

#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "OpenGLEngineFrontend.hpp"
#include "OpenGL/ResourceManager.hpp"

#include "CameraComponent.hpp"
#include "MaterialComponentManager.hpp"
#include "RenderTaskComponentManager.hpp"
#include "TransformComponentManager.hpp"

#include "../Editor/CameraController.hpp"

#include "DynamicPtexMeshSystems.hpp"


void createDemoScene(EngineCore::WorldState& world_state, EngineCore::Graphics::OpenGL::ResourceManager& resource_manager)
{
    auto& entity_mngr = world_state.accessEntityManager();
    auto& camera_mngr = world_state.get<EngineCore::Graphics::CameraComponentManager>();
    auto& mtl_mngr = world_state.get<EngineCore::Graphics::MaterialComponentManager>();
    auto& ptexMesh_mngr = world_state.get<EngineCore::Graphics::DynamicPtexMeshComponentManager>();
    auto& rsrc_mngr = resource_manager;
    auto& renderTask_mngr = world_state.get<EngineCore::Graphics::RenderTaskComponentManager<EngineCore::Graphics::RenderTaskTags::PtexMesh>>();
    auto& transform_mngr = world_state.get<EngineCore::Common::TransformComponentManager>();


    auto camera = entity_mngr.create();
    transform_mngr.addComponent(camera, Vec3(0.0, 0.0, 0.0));
    camera_mngr.addComponent(camera,0.01,1000.0,0.7);
    camera_mngr.setActiveCamera(camera);

    Entity ptex_mesh_entity = createPtexMesh(
        entity_mngr,
        rsrc_mngr,
        camera_mngr,
        mtl_mngr,
        ptexMesh_mngr,
        renderTask_mngr,
        transform_mngr,
        "../bin/Keller.ply");


    // add system that compute patch distances once per simulation frame
    world_state.add([](EngineCore::WorldState& world_state, double dt, EngineCore::Utility::TaskScheduler& task_schedueler) {
        auto& transform_mngr = world_state.get<EngineCore::Common::TransformComponentManager>();
        auto& camera_mngr = world_state.get<EngineCore::Graphics::CameraComponentManager>();
        auto& ptexMesh_mngr = world_state.get<EngineCore::Graphics::DynamicPtexMeshComponentManager>();

        for (size_t ptex_mesh_idx = 0; ptex_mesh_idx < ptexMesh_mngr.getComponentCount(); ++ptex_mesh_idx)
        {
            EngineCore::Graphics::computePatchDistances(
                transform_mngr,
                camera_mngr,
                ptexMesh_mngr,
                task_schedueler,
                ptexMesh_mngr.getComponent(ptex_mesh_idx).entity);

            EngineCore::Graphics::computeTextureTileUpdateLists(ptexMesh_mngr, ptexMesh_mngr.getComponent(ptex_mesh_idx).entity);
        }
        }
    );
}


struct App {
    App() : m_active_window(nullptr), m_window_width(0), m_window_height(0), m_engine_frontend(std::make_unique<OpenGLEngineFrontend>())
    {
        std::lock_guard<std::mutex> lk(m_window_mutex);

        // Initialize GLFW
        if (!glfwInit())
        {
            std::cout << "-----\n"
                << "The time is out of joint - O cursed spite,\n"
                << "That ever I was born to set it right!\n"
                << "-----\n"
                << "Error: Couldn't initialize glfw.";
        }
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#if EDITOR_MODE
        m_active_window = glfwCreateWindow(1600, 900, "Space-Lion", NULL, NULL);
        m_window_width = 1600;
        m_window_height = 900;
        //m_active_window = glfwCreateWindow(1920, 1080, "Space-Lion", glfwGetPrimaryMonitor(), NULL);
#else
        //m_active_window = glfwCreateWindow(1920, 1080, "Space-Lion", glfwGetPrimaryMonitor('), NULL);
        m_active_window = glfwCreateWindow(1280, 720, "Space-Lion", NULL, NULL);
        m_window_width = 1280;
        m_window_height = 720;
#endif

        if (!m_active_window)
        {
            std::cout << "-----\n"
                << "The time is out of joint - O cursed spite,\n"
                << "That ever I was born to set it right!\n"
                << "-----\n"
                << "Error: Couldn't open glfw window";

            glfwTerminate();
        }

        glfwMakeContextCurrent(m_active_window);

        //glfwSwapInterval(1);

        // Get context version information
        int major = glfwGetWindowAttrib(m_active_window, GLFW_CONTEXT_VERSION_MAJOR);
        int minor = glfwGetWindowAttrib(m_active_window, GLFW_CONTEXT_VERSION_MINOR);

        std::cout << "OpenGL context " << major << "." << minor << std::endl;

        // Register callback functions
        glfwSetWindowSizeCallback(m_active_window, windowSizeCallback);
        glfwSetWindowCloseCallback(m_active_window, windowCloseCallback);
        glfwSetKeyCallback(m_active_window, keyCallback);

        glfwSetWindowUserPointer(m_active_window, this);

        // Initialize glad
        if (!gladLoadGL()) {
            std::cout << "-----\n"
                << "The time is out of joint - O cursed spite,\n"
                << "That ever I was born to set it right!\n"
                << "-----\n"
                << "Error during gladLoadGL.\n";
            exit(-1);
        }
        std::cout << "OpenGL %d.%d\n" << GLVersion.major << " " << GLVersion.minor;

        assert((glGetError() == GL_NO_ERROR));

        //glDebugMessageCallback(opengl_debug_message_callback, NULL);
    }

    void run()
    {
        auto game_update_loop = [this](std::shared_future<void> render_exec)
        {
            std::cout << "  thread_id : " << std::this_thread::get_id() << "\n";

            // inplace construct an input action context to test the new concept
            auto evt_func = [](EngineCore::Common::Input::Event const& evt, EngineCore::Common::Input::HardwareState const& state) {
                std::cout << "Paying respect to new input system" << "\n";
            };
            EngineCore::Common::Input::EventDrivenAction evt_action = { {EngineCore::Common::Input::Device::KEYBOARD,EngineCore::Common::Input::KeyboardKeys::KEY_F,EngineCore::Common::Input::EventTrigger::PRESS}, evt_func };
            EngineCore::Common::Input::InputActionContext input_context = { "test_input_context", true, {evt_action}, {} };
            m_input_action_contexts.push_back(input_context);

            auto& world_state = m_engine_frontend->accessWorldState();
            auto& resource_manager = m_engine_frontend->accessResourceManager();
            auto& frame_manager = m_engine_frontend->accessFrameManager();

            Editor::Controls::CameraController cam_ctrl(world_state);
            m_input_action_contexts.push_back(cam_ctrl.getKeyboardInputActionContext());
            m_input_action_contexts.push_back(cam_ctrl.getGamepadInputActionContext());
            //m_engine_frontend->addInputActionContext(cam_ctrl.getInputActionContext());
            createDemoScene(world_state, resource_manager);

            while (render_exec.wait_for(std::chrono::seconds(1)) != std::future_status::ready) {
                // do nothing so far...
            }
        };

        auto engine_update_loop = [this](std::shared_future<void> render_exec)
        {
            std::cout << "  thread_id : " << std::this_thread::get_id() << "\n";

            size_t update_frameID = 0;
            auto t_0 = std::chrono::high_resolution_clock::now();
            auto t_1 = std::chrono::high_resolution_clock::now();

            auto render_exec_status = render_exec.wait_for(std::chrono::microseconds(0));
            while (render_exec_status != std::future_status::ready)
            {
                double dt = std::chrono::duration_cast<std::chrono::duration<double>>(t_1 - t_0).count();
                t_0 = std::chrono::high_resolution_clock::now();

                auto window_resolution = this->getWindowResolution();

                m_frame_ready_to_render.wait(true);

                m_engine_frontend->update(update_frameID++, dt, std::get<0>(window_resolution), std::get<1>(window_resolution));

                m_frame_ready_to_render.test_and_set();
                m_frame_ready_to_render.notify_all();

                render_exec_status = render_exec.wait_for(std::chrono::microseconds(0));

                t_1 = std::chrono::high_resolution_clock::now();
            }
        };

        auto engine_render_loop = [this]()
        {
            std::cout << "  thread_id : " << std::this_thread::get_id() << "\n";

            // Init ImGui, set install_callbacks to false cause I call them myself
                //IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            if (!ImGui_ImplGlfw_InitForOpenGL(m_active_window, false))
                std::cerr << "Error during imgui init " << std::endl;
            ImGui_ImplOpenGL3_Init("#version 450");

            size_t render_frameID = 0;
            double t0, t1 = 0.0;

            while (!glfwWindowShouldClose(m_active_window))
            {
                auto orig_imgui_ctx = ImGui::GetCurrentContext();

                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();

                int width, height;
                glfwGetFramebufferSize(m_active_window, &width, &height);

                t0 = t1;
                t1 = glfwGetTime();
                double dt = t1 - t0;

                m_frame_ready_to_render.wait(false);
                m_frame_ready_to_render.clear();
                m_frame_ready_to_render.notify_all();

                // Get current frame for rendering
                auto& frame = m_engine_frontend->accessFrameManager().getRenderFrame();
                frame.m_render_frameID = render_frameID;
                frame.m_render_dt = dt;

                processInputActions(dt);

                m_engine_frontend->render(render_frameID++, dt, width, height);

                ImGui::SetNextWindowPos(ImVec2(width - 375.0f, height - 100.0f));
                bool p_open = true;
                if (!ImGui::Begin("FPS", &p_open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
                {
                    ImGui::End();
                    return;
                }
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                ImGui::End();
                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


                //  // create temporary imgui context
                //  //auto orig_imgui_ctx = ImGui::GetCurrentContext();
                //  auto imgui_ctx = ImGui::CreateContext(ImGui::GetFont()->ContainerAtlas);
                //  ImGui::SetCurrentContext(imgui_ctx);
                //  ImGuiIO& io = ImGui::GetIO(); (void)io;
                //  if (!ImGui_ImplGlfw_InitForOpenGL(m_active_window, false))
                //      std::cerr << "Error during imgui init " << std::endl;
                //  ImGui_ImplOpenGL3_Init("#version 450");
                //  
                //  ImGui_ImplOpenGL3_NewFrame();
                //  ImGui_ImplGlfw_NewFrame();
                //  ImGui::NewFrame();
                //  
                //  // create imgui/implot elements
                //  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
                //  bool window_status = ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.
                //  ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
                //  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                //  ImGui::End();
                //  
                //  ImGui::Render();
                //  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                //  
                //  // delete temporary imgui context, reset to original imgui context
                //  ImGui::DestroyContext(imgui_ctx);
                //  ImGui::SetCurrentContext(orig_imgui_ctx);

                glfwSwapBuffers(m_active_window);
                glfwPollEvents();
            }

            // TODO some sort of exit call to frontend, e.g. to delete GPU resources
        };

        // Deferred starting and running of rendering on current thread (window and context usually live here)
        auto engine_render_exec = std::async(std::launch::deferred, engine_render_loop).share();

        // Start and run engine update in seperate thread
        auto engine_update_exec = std::async(std::launch::async, engine_update_loop, engine_render_exec);

        // Start "game"
        auto game_update_exec = std::async(std::launch::async, game_update_loop, engine_render_exec);

        // trigger deferred start of render loop
        engine_render_exec.get();
    }

    std::pair<int, int> getWindowResolution() {
        std::lock_guard<std::mutex> lk(m_window_mutex);
        return { m_window_width,m_window_height };
    }

    void processInputActions(float dt)
    {
        for (auto& input_context : m_input_action_contexts)
        {
            if (input_context.m_is_active)
            {
                for (auto& state_action : input_context.m_state_actions)
                {
                    std::vector<EngineCore::Common::Input::HardwareState> states;

                    for (auto& part : state_action.m_state_query)
                    {
                        if (std::get<0>(part) == EngineCore::Common::Input::Device::KEYBOARD)
                        {
                            states.emplace_back(glfwGetKey(m_active_window, std::get<1>(part)) == GLFW_PRESS ? 1.0f : 0.0);
                        }
                        else if (std::get<0>(part) == EngineCore::Common::Input::Device::MOUSE_AXES)
                        {
                            double x, y;
                            glfwGetCursorPos(m_active_window, &x, &y);

                            if (std::get<1>(part) == EngineCore::Common::Input::MouseAxes::MOUSE_CURSOR_X)
                            {
                                states.emplace_back(x);
                            }
                            else if (std::get<1>(part) == EngineCore::Common::Input::MouseAxes::MOUSE_CURSOR_Y)
                            {
                                states.emplace_back(y);
                            }
                        }
                        else if (std::get<0>(part) == EngineCore::Common::Input::Device::MOUSE_BUTTON)
                        {
                            if (std::get<1>(part) == EngineCore::Common::Input::MouseButtons::MOUSE_BUTTON_RIGHT)
                            {
                                states.emplace_back(glfwGetMouseButton(m_active_window, std::get<1>(part)) == GLFW_PRESS ? 1.0f : 0.0f);
                            }
                        }
                        else if (std::get<0>(part) == EngineCore::Common::Input::Device::GAMEPAD_AXES)
                        {
                            if (glfwJoystickIsGamepad(GLFW_JOYSTICK_1))
                            {
                                GLFWgamepadstate state;

                                if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state))
                                {
                                    if (std::get<1>(part) == EngineCore::Common::Input::GamepadAxes::GAMEPAD_AXIS_LEFT_X) {
                                        states.emplace_back(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
                                    }
                                    else if (std::get<1>(part) == EngineCore::Common::Input::GamepadAxes::GAMEPAD_AXIS_LEFT_Y) {
                                        states.emplace_back(state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
                                    }
                                    else if (std::get<1>(part) == EngineCore::Common::Input::GamepadAxes::GAMEPAD_AXIS_RIGHT_X) {
                                        states.emplace_back(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);
                                    }
                                    else if (std::get<1>(part) == EngineCore::Common::Input::GamepadAxes::GAMEPAD_AXIS_RIGHT_Y) {
                                        states.emplace_back(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
                                    }
                                }
                            }
                        }
                    }

                    if (!states.empty()) {
                        state_action.m_action(state_action.m_state_query, states, dt);
                    }
                }
            }
        }
    }

    void addInputActionContext(EngineCore::Common::Input::InputActionContext const& context);

    void removeInputActionContext(std::string const& context_name);

    void setInputActionContextActive(std::string const& context_name);

    void setInputActionContextInactive(std::string const& context_name);

    /** Pointer to active window */
    GLFWwindow* m_active_window;
    int m_window_width;
    int m_window_height;

    std::mutex m_window_mutex;
    //std::condition_variable m_winodw_cVar;

    std::atomic_flag m_frame_ready_to_render = ATOMIC_FLAG_INIT;

    /** List of input contexts */
    std::list<EngineCore::Common::Input::InputActionContext> m_input_action_contexts;

    /** Frontend for space lion engine */
    std::unique_ptr<OpenGLEngineFrontend> m_engine_frontend;

    /**************************************************************************
     * (Static) callbacks functions
     *************************************************************************/

    static void windowSizeCallback(GLFWwindow* window, int width, int height)
    {
        auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));

        std::lock_guard<std::mutex> lk(app->m_window_mutex);
        app->m_window_width = width;
        app->m_window_height = height;
    }

    static void windowCloseCallback(GLFWwindow* window) {

    }

    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
    {

    }

    static void mouseCursorCallback(GLFWwindow* window, double xpos, double ypos)
    {

    }

    static void mouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
    {

    }

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));

        for (auto& input_context : app->m_input_action_contexts)
        {
            if (input_context.m_is_active)
            {
                for (auto& event_action : input_context.m_event_actions)
                {
                    if (std::get<0>(event_action.m_event) == EngineCore::Common::Input::Device::KEYBOARD &&
                        std::get<1>(event_action.m_event) == key &&
                        std::get<2>(event_action.m_event) == action
                        )
                    {
                        event_action.m_action(event_action.m_event,/*TODO map action to meanigful state?*/1.0);
                    }
                }
            }
        }
    }
};


int main() {

    App app;
    app.run();

    return 0;
}