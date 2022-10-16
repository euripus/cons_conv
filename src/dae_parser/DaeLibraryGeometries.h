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
    std::string                      _pos_source_id;
    std::vector<DaeSource>::iterator _pos_source_it;   // position source array

    void Parse(pugi::xml_node const & vertices_node);
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
        std::string                              source_id;
        std::vector<DaeSource>::iterator         source_it;
        std::vector<DaeVerticesSource>::iterator pos_source_it;   // for VERTEX semantic only
        int                                      set;   // for TEXCOORD, TEXTANGENT, TEXBINORMAL semantic
        int                                      offset;
    };

    struct TriangleGroup
    {
        using VertexAttributes = std::vector<unsigned int>;   // size() == _attributes.size()

        std::string                   _material_name;
        std::vector<Input>            _attributes;
        std::vector<VertexAttributes> _triangles;   // every 3 form triangle in CCW mode

        Input const & GetInput(Semantic sem) const
        {
            auto it = std::find_if(_attributes.begin(), _attributes.end(),
                                   [&](Input const & ip) -> bool { return ip.semantic == sem; });

            if(it != _attributes.end())
                return *it;
            else
                throw std::runtime_error("Desired input semantic not found");
        }
    };

    std::vector<TriangleGroup> _meshes;

    void            Parse(pugi::xml_node const & polylist_node);
    static Semantic GetSemanticType(char const * str);
};

// mesh node abstraction
struct DaeMeshNode
{
    std::string                    _id;
    std::string                    _name;
    std::vector<DaeSource>         _sources;
    std::vector<DaeVerticesSource> _pos_sources;
    std::vector<DaeGeometry>       _tri_groups;

    void Parse(pugi::xml_node const & geo);
    void CheckInputConsistency() const;
};

class DaeLibraryGeometries
{
public:
    void                Parse(pugi::xml_node const & geo);
    DaeMeshNode const * Find(std::string const & id) const;

    std::vector<DaeMeshNode> _lib;
};

#endif   // DAELIBRARYGEOMETRIES_H
