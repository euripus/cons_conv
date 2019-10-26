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
    std::map<std::string, std::string> _materialBindings;
};

struct DaeNode
{
    std::string _id, _sid;
    std::string _name;
    std::string _rootJointName;   // for skin instance only
    bool        _joint;
    bool        _reference;

    DaeNode *                             _parent;
    std::vector<DaeTransformation>        _transStack;
    std::vector<std::unique_ptr<DaeNode>> _children;
    std::vector<DaeInstance>              _instances;

    void Parse(const pugi::xml_node & node, DaeNode * parent = nullptr);
};

struct DaeVisualScene
{
    std::string          _id;
    std::string          _name;
    std::vector<DaeNode> _nodes;

    void            Parse(const pugi::xml_node & visScene);
    const DaeNode * FindNode(const std::string & id) const;
};

class DaeLibraryVisualScenes
{
public:
    std::vector<DaeVisualScene> _scenes;

    void                   Parse(const pugi::xml_node & root);
    const DaeVisualScene * Find(const std::string & id) const;
};

#endif   // DAEVISUALSCENES_H
