#ifndef DAEVISUALSCENES_H
#define DAEVISUALSCENES_H

#include <map>
#include <memory>
#include <pugixml.hpp>
#include <string>
#include <vector>

struct DaeTransformation
{
    enum class Type
    {
        MATRIX,
        TRANSLATION,
        ROTATION,
        SCALE
    };

    std::string _sid;
    Type        _type;
    float       _values[16];
};

struct DaeInstance
{
    std::string                        _url;
    std::map<std::string, std::string> _material_bindings;
};

struct DaeNode
{
    std::string _id, _sid;
    std::string _name;
    std::string _root_joint_name;   // for skin instance only
    bool        _joint;
    bool        _reference;

    DaeNode *                             _parent;
    std::vector<DaeTransformation>        _trans_stack;
    std::vector<std::unique_ptr<DaeNode>> _children;
    std::vector<DaeInstance>              _instances;

    void Parse(pugi::xml_node const & node, DaeNode * parent = nullptr);
};

struct DaeVisualScene
{
    std::string          _id;
    std::string          _name;
    std::vector<DaeNode> _nodes;

    void            Parse(pugi::xml_node const & visScene);
    DaeNode const * FindNode(std::string const & id) const;
};

class DaeLibraryVisualScenes
{
public:
    std::vector<DaeVisualScene> _scenes;

    void                   Parse(pugi::xml_node const & root);
    DaeVisualScene const * Find(std::string const & id) const;
};

#endif   // DAEVISUALSCENES_H
