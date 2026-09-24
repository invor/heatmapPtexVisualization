#ifndef PtexMeshRenderPass_hpp
#define PtexMeshRenderPass_hpp

#include "Frame.hpp"
#include "WorldState.hpp"
#include "OpenGL/ResourceManager.hpp"

namespace EngineCore {
    namespace Graphics {
        namespace OpenGL {

            void addPtexMeshRenderPass(Common::Frame& frame,
                WorldState& world_state,
                ResourceManager& resource_mngr);

        }
    }
}

#endif //!PtexMeshRenderPass_hpp