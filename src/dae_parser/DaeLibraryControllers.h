#ifndef DAELIBRARYCONTROLLERS_H
#define DAELIBRARYCONTROLLERS_H

#include "DaeSource.h"
#include <glm/glm.hpp>
#include <string>

struct DaeWeight
{
    int _joint;
    int _weight;
};

using WeightsVec = std::vector<DaeWeight>;

struct DaeSkin
{
    std::string             _id;
    std::string             _owner_id;
    glm::mat4               _bind_shape_mat;
    std::vector<DaeSource>  _sources;
    DaeSource *             _joint_array;
    DaeSource *             _weight_array;
    DaeSource *             _bind_mat_array;
    std::vector<WeightsVec> _vert_weights;

    void Parse(pugi::xml_node const & skin);
};

struct DaeLibraryControllers
{
    std::vector<DaeSkin> _skinControllers;

    void Parse(pugi::xml_node const & root);
};

#endif   // DAELIBRARYCONTROLLERS_H
