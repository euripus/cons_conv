#ifndef DAELIBRARYGEOMETRIES_H
#define DAELIBRARYGEOMETRIES_H

#include <algorithm>
#include <pugixml.hpp>
#include <vector>

#include "DaeSource.h"

// vertices node
// describes mesh-vertices in a mesh
struct DaeVerticesSource
{
    std::string                      _id;
    std::string                      _posSourceId;
    std::vector<DaeSource>::iterator _posSourceIt;   // position source array

    void Parse(const pugi::xml_node & verticesNode);
};

struct DaeGeometry
{
    enum class Semantic
    {
        UNKNOWN,
        BINORMAL,
        COLOR,
        NORMAL,
        TANGENT,
        TEXBINORMAL,
        TEXCOORD,
        TEXTANGENT,
        VERTEX,
    };

    struct Input
    {
        Semantic                                 semantic;
        std::string                              sourceId;
        std::vector<DaeSource>::iterator         sourceIt;
        std::vector<DaeVerticesSource>::iterator posSourceIt;   // for VERTEX semantic only
        int                                      set;   // for TEXCOORD, TEXTANGENT, TEXBINORMAL semantic
        int                                      offset;
    };

    struct TriangleGroup
    {
        using VertexAttributes = std::vector<unsigned int>;   // size() == _attributes.size()

        std::string                   _material_name;
        std::vector<Input>            _attributes;
        std::vector<VertexAttributes> _triangles;   // every 3 form triangle in CCW mode

        const Input & GetInput(Semantic sem) const
        {
            auto it = std::find_if(_attributes.begin(), _attributes.end(),
                                   [&](const Input & ip) -> bool { return ip.semantic == sem; });

            if(it != _attributes.end())
                return *it;
            else
                throw std::runtime_error("Desired input semantic not found");
        }
    };

    std::vector<TriangleGroup> _meshes;

    void            Parse(const pugi::xml_node & polylistNode);
    static Semantic GetSemanticType(const char * str);
};

// mesh node abstraction
struct DaeMeshNode
{
    std::string                    _id;
    std::string                    _name;
    std::vector<DaeSource>         _sources;
    std::vector<DaeVerticesSource> _posSources;
    std::vector<DaeGeometry>       _triGroups;

    void Parse(const pugi::xml_node & geo);
    void CheckInputConsistency() const;
};

class DaeLibraryGeometries
{
public:
    void                Parse(const pugi::xml_node & geo);
    const DaeMeshNode * Find(const std::string & id) const;

    std::vector<DaeMeshNode> _lib;
};

#endif   // DAELIBRARYGEOMETRIES_H
