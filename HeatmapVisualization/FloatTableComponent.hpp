#ifndef FloatTableComponent_hpp
#define FloatTableComponent_hpp

#include "BaseSingleInstanceComponentManager2.hpp"
#include "BaseResourceManager.hpp"

namespace EngineCore
{
    namespace Common
    {
        struct FloatTableComponentData
        {
            Entity                   entity;

            size_t                   column_cnt;
            size_t                   row_cnt;

            std::vector<std::string> column_headers;
            std::vector<float>       data;

            //Graphics::ResourceID     gpu_data_buffer; // future work
        };

        class FloatTableComponentManager : public BaseSingleInstanceComponentManager2<FloatTableComponentData,100,5>
        {
        public:
            size_t addComponent(Entity entity, std::string const& csv_filepath);
        };

    }
}

#endif // !FloatTableComponent_hpp
