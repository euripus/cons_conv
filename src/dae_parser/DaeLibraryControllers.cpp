#include "DaeLibraryControllers.h"
#include "DaeParser.h"
#include <cstring>
#include <iostream>

/*******************************************************************************
 * DaeSkin
 *******************************************************************************/
void DaeSkin::Parse(pugi::xml_node const & skin)
{
    auto Find = [&](std::string const & str) -> DaeSource * {
        if(str.empty())
            return nullptr;

        for(auto & src : _sources)
        {
            if(src._id == str)
                return &src;
        }

        return nullptr;
    };

    pugi::xml_node node1 = skin.child("skin");

    _id = skin.attribute("id").value();
    if(_id.empty())
        return;
    _ownerId = node1.attribute("source").value();
    RemoveGate(_ownerId);

    // Bind shape matrix
    pugi::xml_node node2 = node1.child("bind_shape_matrix");
    if(node2.empty())
        return;

    char const * str = node2.text().get();
    if(str == nullptr)
        return;
    float mat4[16] = {0};
    for(int i = 0; i < 16; ++i)
    {
        char * end;
        float  f = std::strtof(str, &end);
        str      = end;
        mat4[i]  = f;
    }
    _bindShapeMat = CreateDAEMatrix(mat4);

    // Sources
    for(node2 = node1.child("source"); node2; node2 = node2.next_sibling("source"))
    {
        DaeSource source;
        source.Parse(node2);
        _sources.push_back(std::move(source));
    }

    // Joints
    node2 = node1.child("joints");
    if(!node2.empty())
    {
        for(pugi::xml_node node3 = node2.child("input"); node3; node3 = node3.next_sibling("input"))
        {
            if(strcmp(node3.attribute("semantic").value(), "JOINT") == 0)
            {
                std::string id = node3.attribute("source").value();
                RemoveGate(id);
                _jointArray = Find(id);
            }
            else if(strcmp(node3.attribute("semantic").value(), "INV_BIND_MATRIX") == 0)
            {
                std::string sourceId = node3.attribute("source").value();
                RemoveGate(sourceId);
                _bindMatArray = Find(sourceId);
            }
        }
    }

    // Vertex weights
    unsigned int jointOffset = 0, weightOffset = 0;
    unsigned int numInputs = 0;

    node2                = node1.child("vertex_weights");
    int            count = atoi(node2.attribute("count").value());
    pugi::xml_node node3 = node2.child("input");
    for(; node3; node3 = node3.next_sibling("input"))
    {
        ++numInputs;

        if(strcmp(node3.attribute("semantic").value(), "JOINT") == 0)
        {
            jointOffset = atoi(node3.attribute("offset").value());

            std::string id = node3.attribute("source").value();
            RemoveGate(id);
            DaeSource * vertJointArray = Find(id);
            if(jointOffset != 0 || vertJointArray->_stringArray != _jointArray->_stringArray)
            {
                std::cout << "Warning: Vertex weight joint array doesn't match skin joint array\n";
            }
        }
        else if(strcmp(node3.attribute("semantic").value(), "WEIGHT") == 0)
        {
            weightOffset   = atoi(node3.attribute("offset").value());
            std::string id = node3.attribute("source").value();
            RemoveGate(id);
            _weightArray = Find(id);
        }
    }

    node3 = node2.child("vcount");
    str   = node3.text().get();
    if(str == nullptr)
        return;
    for(int i = 0; i < count; ++i)
    {
        char * end;
        int    si = std::strtol(str, &end, 10);
        str       = end;

        WeightsVec vertWeight;
        for(unsigned int j = 0; j < (unsigned)si; ++j)
        {
            vertWeight.push_back(DaeWeight());
        }

        _vertWeights.push_back(vertWeight);
    }

    node3 = node2.child("v");
    str   = node3.text().get();
    if(str == nullptr)
        return;
    for(int i = 0; i < count; ++i)
    {
        for(auto & vrtWeight : _vertWeights[i])
        {
            for(unsigned int k = 0; k < numInputs; ++k)
            {
                char * end;
                int    si = std::strtol(str, &end, 10);
                str       = end;

                if(k == jointOffset)
                    vrtWeight._joint = si;
                if(k == weightOffset)
                    vrtWeight._weight = si;
            }
        }
    }
}

/*******************************************************************************
 * DaeLibraryControllers
 *******************************************************************************/
void DaeLibraryControllers::Parse(pugi::xml_node const & root)
{
    pugi::xml_node node1 = root.child("library_controllers");
    if(node1.empty())
        return;

    pugi::xml_node node2 = node1.child("controller");
    for(; node2; node2 = node2.next_sibling("controller"))
    {
        // Skin
        pugi::xml_node node3 = node2.child("skin");
        if(!node3.empty())
        {
            _skinControllers.emplace_back();
            _skinControllers.back().Parse(node2);
        }
    }
}
