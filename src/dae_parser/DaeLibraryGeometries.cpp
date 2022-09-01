#include "DaeLibraryGeometries.h"
#include "DaeParser.h"
#include <cassert>
#include <cstring>
#include <sstream>
#include <stdexcept>

DaeMeshNode const * DaeLibraryGeometries::Find(std::string const & id) const
{
    auto it = find_if(_lib.begin(), _lib.end(), [&](DaeMeshNode const & dg) { return dg._id == id; });

    if(it != _lib.end())
        return &(*it);
    else
        return nullptr;
}

void DaeLibraryGeometries::Parse(pugi::xml_node const & geo)
{
    pugi::xml_node libgeo = geo.child("library_geometries");
    if(libgeo.empty())
    {
        std::stringstream ss;
        ss << "Error: geometry node not found"
           << "\n";

        throw std::runtime_error(ss.str());
    }

    for(pugi::xml_node geom = libgeo.child("geometry"); geom; geom = geom.next_sibling("geometry"))
    {
        _lib.emplace_back();
        _lib.back().Parse(geom);
        _lib.back().CheckInputConsistency();
    }
}

void DaeMeshNode::Parse(pugi::xml_node const & geo)
{
    pugi::xml_node node1 = geo.child("mesh");
    if(node1.empty())
    {
        std::stringstream ss;
        ss << "Mesh child not found '" << std::string(geo.attribute("id").value()) << "'\n";

        throw std::runtime_error(ss.str());
    }

    _id = geo.attribute("id").value();
    if(_id.empty())
    {
        std::stringstream ss;
        ss << "geometry node doesnt have id '" << std::string(geo.name()) << "'\n";

        throw std::runtime_error(ss.str());
    }

    _name = geo.attribute("name").value();
    pugi::xml_node node2;

    // Parse sources
    for(node2 = node1.child("source"); node2; node2 = node2.next_sibling("source"))
    {
        _sources.emplace_back();
        _sources.back().Parse(node2);
    }

    // Parse vertex data
    for(node2 = node1.child("vertices"); node2; node2 = node2.next_sibling("vertices"))
    {
        _posSources.emplace_back();
        _posSources.back().Parse(node2);
        std::string posSourceId = _posSources.back()._posSourceId;
        _posSources.back()._posSourceIt =
            std::find_if(_sources.begin(), _sources.end(),
                         [&posSourceId](DaeSource const & src) -> bool { return posSourceId == src._id; });
        if(_posSources.back()._posSourceIt == _sources.end())
        {
            std::stringstream ss;
            ss << "For vertices node not found source node '" << std::string(node2.name()) << "'\n";

            throw std::runtime_error(ss.str());
        }
    }

    // Parse primitives
    for(node2 = node1.first_child(); node2; node2 = node2.next_sibling())
    {
        if(strcmp(node2.name(), "triangles") == 0 || strcmp(node2.name(), "polygons") == 0
           || strcmp(node2.name(), "polylist") == 0)
        {
            _triGroups.emplace_back();
            _triGroups.back().Parse(node2);

            for(auto & tg : _triGroups.back()._meshes)
            {
                for(auto & attr : tg._attributes)
                {
                    if(attr.semantic == DaeGeometry::Semantic::VERTEX)
                    {
                        attr.sourceIt = _sources.end();
                        attr.posSourceIt =
                            std::find_if(_posSources.begin(), _posSources.end(),
                                         [sourceId = attr.sourceId](DaeVerticesSource const & vs) -> bool {
                                             return sourceId == vs._id;
                                         });

                        if(attr.posSourceIt == _posSources.end())
                        {
                            std::stringstream ss;
                            ss << "Source node for '" << attr.sourceId << "' not found\n";

                            throw std::runtime_error(ss.str());
                        }
                    }
                    else
                    {
                        attr.posSourceIt = _posSources.end();
                        attr.sourceIt =
                            std::find_if(_sources.begin(), _sources.end(),
                                         [sourceId = attr.sourceId](DaeSource const & vs) -> bool {
                                             return sourceId == vs._id;
                                         });

                        if(attr.sourceIt == _sources.end())
                        {
                            std::stringstream ss;
                            ss << "Source node for '" << attr.sourceId << "' not found\n";

                            throw std::runtime_error(ss.str());
                        }
                    }
                }
            }
        }
    }
}

void DaeMeshNode::CheckInputConsistency() const
{
    if(_triGroups.size() == 1)
        return;

    auto fst_attr_list = _triGroups[0]._meshes[0];

    for(unsigned int i = 1; i < _triGroups.size(); i++)
    {
        for(auto & tmp : _triGroups[i]._meshes)
        {
            uint32_t i = 0;
            for(auto & inp : tmp._attributes)
            {
                // Warning: Order of occurrence depended
                auto tmp = fst_attr_list._attributes[i];

                if(tmp.sourceId != inp.sourceId || tmp.set != inp.set || tmp.offset != inp.offset)
                    throw std::runtime_error("Not consistent vertex stream inputs");

                ++i;
            }
        }
    }
}

DaeGeometry::Semantic DaeGeometry::GetSemanticType(char const * str)
{
    assert(str != nullptr);

    if(strcmp(str, "VERTEX") == 0)
        return Semantic::VERTEX;
    if(strcmp(str, "BINORMAL") == 0)
        return Semantic::BINORMAL;
    if(strcmp(str, "COLOR") == 0)
        return Semantic::COLOR;
    if(strcmp(str, "NORMAL") == 0)
        return Semantic::NORMAL;
    if(strcmp(str, "TANGENT") == 0)
        return Semantic::TANGENT;
    if(strcmp(str, "TEXBINORMAL") == 0)
        return Semantic::TEXBINORMAL;
    if(strcmp(str, "TEXCOORD") == 0)
        return Semantic::TEXCOORD;
    if(strcmp(str, "TEXTANGENT") == 0)
        return Semantic::TEXTANGENT;

    std::stringstream ss;
    ss << "Unknown semantic type:'" << str << "'\n";

    throw std::runtime_error(ss.str());
}

void DaeGeometry::Parse(pugi::xml_node const & polylistNode)
{
    enum class PrimType
    {
        Unknown,
        Triangles,
        Polygons,
        Polylist
    };
    PrimType primType = PrimType::Unknown;

    if(strcmp(polylistNode.name(), "triangles") == 0)
        primType = PrimType::Triangles;
    else if(strcmp(polylistNode.name(), "polygons") == 0)
        primType = PrimType::Polygons;
    else if(strcmp(polylistNode.name(), "polylist") == 0)
        primType = PrimType::Polylist;
    else
    {
        std::stringstream ss;
        ss << "Unsupported geometry primitive '" << std::string(polylistNode.name()) << "'\n";

        throw std::runtime_error(ss.str());
    }

    // Find the base mapping channel with the lowest set-id
    // and max input offset
    int           baseChannel = 999999;
    int           offset      = 0;
    TriangleGroup sub_mesh;

    pugi::xml_node node1;
    for(node1 = polylistNode.child("input"); node1; node1 = node1.next_sibling("input"))
    {
        if(strcmp(node1.attribute("semantic").value(), "TEXCOORD") == 0)
        {
            if(node1.attribute("set") != nullptr)
            {
                if(std::atoi(node1.attribute("set").value()) < baseChannel)
                    baseChannel = std::atoi(node1.attribute("set").value());
            }
            else
            {
                baseChannel = 0;
            }
        }
        if(std::atoi(node1.attribute("offset").value()) > offset)
            offset = std::atoi(node1.attribute("offset").value());
    }

    sub_mesh._material_name = polylistNode.attribute("material").value();

    // Parse input mapping
    for(node1 = polylistNode.child("input"); node1; node1 = node1.next_sibling("input"))
    {
        Input inp;

        inp.offset = std::atoi(node1.attribute("offset").value());
        assert(inp.offset < offset + 1);
        inp.semantic = GetSemanticType(node1.attribute("semantic").value());
        inp.sourceId = node1.attribute("source").value();
        RemoveGate(inp.sourceId);

        inp.set = 0;
        if(inp.semantic == Semantic::TEXCOORD || inp.semantic == Semantic::TEXTANGENT
           || inp.semantic == Semantic::TEXBINORMAL)
        {
            inp.set = std::atoi(node1.attribute("set").value());
            inp.set -= baseChannel;
        }

        sub_mesh._attributes.push_back(std::move(inp));
    }

    // Form index list for submesh

    // Get vertex counts for polylists
    char const * vcountStr = nullptr;
    unsigned int num_verts = 0;
    if(primType == PrimType::Polylist)
    {
        if(polylistNode.child("vcount").empty())
        {
            std::stringstream ss;
            ss << "Polylist node doesnt has 'vcount' '" << std::string(polylistNode.name()) << "'\n";

            throw std::runtime_error(ss.str());
        }
        vcountStr = polylistNode.child("vcount").text().get();
    }

    // Parse actual primitive data
    //      The winding order of vertices produced is counter-clockwise
    //      and describes the front side of each polygon
    for(node1 = polylistNode.child("p"); node1; node1 = node1.next_sibling("p"))
    {
        char const * str = node1.text().get();
        if(str == nullptr)
        {
            std::stringstream ss;
            ss << "Polylist node doesnt has \"p\" '" << std::string(polylistNode.name()) << "'\n";

            throw std::runtime_error(ss.str());
        }

        char *       end        = nullptr;
        unsigned int cur_offset = 0, vert_cnt = 0;
        int          si;
        unsigned int num_attrib        = sub_mesh._attributes.size();
        unsigned int inputs_per_vertex = offset + 1;

        TriangleGroup::VertexAttributes cur_ind;
        TriangleGroup::VertexAttributes first_ind, last_ind;
        cur_ind.resize(num_attrib);

        while(true)
        {
            si = std::strtol(str, &end, 10);
            if(str == end)
                break;
            str = end;

            unsigned int num_att_for_offset = 0, num_sets = 0;
            for(auto & attr : sub_mesh._attributes)
            {
                if((int)cur_offset == attr.offset)
                {
                    cur_ind[attr.offset + attr.set + num_sets] = si;
                    num_att_for_offset++;
                }
            }
            if(num_att_for_offset > 1)
                num_sets++;

            if(++cur_offset == inputs_per_vertex)
            {
                if(primType == PrimType::Polylist && vert_cnt == num_verts)
                {
                    vert_cnt  = 0;
                    num_verts = std::strtol(vcountStr, &end, 10);
                    vcountStr = end;
                }

                if(primType == PrimType::Polygons || primType == PrimType::Polylist)
                {
                    // Do simple triangulation (assumes convex polygons)
                    if(vert_cnt == 0)
                    {
                        first_ind = cur_ind;
                    }
                    else if(vert_cnt > 2)
                    {
                        // Create new triangle
                        sub_mesh._triangles.push_back(first_ind);
                        sub_mesh._triangles.push_back(last_ind);
                    }
                }

                sub_mesh._triangles.push_back(cur_ind);
                last_ind   = cur_ind;
                cur_offset = 0;
                ++vert_cnt;
            }
        }
    }

    _meshes.push_back(std::move(sub_mesh));
}

void DaeVerticesSource::Parse(pugi::xml_node const & verticesNode)
{
    _id = verticesNode.attribute("id").value();
    if(_id.empty())
    {
        std::stringstream ss;
        ss << "vertices node doesnt have id '" << std::string(verticesNode.name()) << "'\n";

        throw std::runtime_error(ss.str());
    }

    for(pugi::xml_node node1 = verticesNode.child("input"); node1; node1 = node1.next_sibling("input"))
    {
        if(strcmp(node1.attribute("semantic").value(), "POSITION") == 0)
        {
            _posSourceId = node1.attribute("source").value();
            RemoveGate(_posSourceId);
        }
    }

    if(_posSourceId.empty())
    {
        std::stringstream ss;
        ss << "In vertices node doesnt found POSITION semantic '" << std::string(verticesNode.name())
           << "'\n";

        throw std::runtime_error(ss.str());
    }
}
