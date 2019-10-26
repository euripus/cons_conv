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
    std::string             _ownerId;
    glm::mat4               _bindShapeMat;
    std::vector<DaeSource>  _sources;
    DaeSource *             _jointArray;
    DaeSource *             _weightArray;
    DaeSource *             _bindMatArray;
    std::vector<WeightsVec> _vertWeights;

    void Parse(const pugi::xml_node & skin);
};

struct DaeLibraryControllers
{
    std::vector<DaeSkin> _skinControllers;

    void Parse(const pugi::xml_node & root);
};

#endif   // DAELIBRARYCONTROLLERS_H
