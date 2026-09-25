#include "FloatTableComponent.hpp"

#include "rapidcsv.h"

namespace
{
    inline void loadFloatTableFromCSV(std::string const& path, size_t& column_cnt, size_t& row_cnt, std::vector<std::string>& column_headers, std::vector<float>& data)
    {
        rapidcsv::Document doc(path);

        column_cnt = doc.GetColumnCount();
        row_cnt = doc.GetRowCount();

        column_headers = doc.GetColumnNames();

        data = std::vector<float>(column_cnt * row_cnt);

        for (size_t i = 0; i < row_cnt; ++i)
        {
            auto row = doc.GetRow<float>(i);
            std::copy(row.begin(), row.end(), data.begin() + (i * column_cnt));
        }
    }
}

size_t EngineCore::Common::FloatTableComponentManager::addComponent(Entity entity, std::string const& csv_filepath)
{
    FloatTableComponentData component;
    component.entity = entity;
    loadFloatTableFromCSV(csv_filepath, component.column_cnt, component.row_cnt, component.column_headers, component.data);

    auto index = data_.addComponent(
        std::move(component)
    );

    addIndex(entity.id(), index);

    return index;
}
