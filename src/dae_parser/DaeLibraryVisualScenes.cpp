#include "DaeLibraryVisualScenes.h"
#include "DaeParser.h"
#include <cstring>
#include <iostream>

/*******************************************************************************
 * DaeNode
 *******************************************************************************/
void DaeNode::Parse(pugi::xml_node const & node, DaeNode * parent)
{
    _parent = parent;

    _reference = false;
    _id        = node.attribute("id").value();
    _name      = node.attribute("name").value();
    _sid       = node.attribute("sid").value();

    if(strcmp(node.attribute("type").value(), "JOINT") == 0)
        _joint = true;
    else
        _joint = false;

    // Parse transformations
    pugi::xml_node node1 = node.first_child();
    for(; node1; node1 = node1.next_sibling())
    {
        if(node1.name() == nullptr)
            continue;

        DaeTransformation trans;
        trans._sid = node1.attribute("sid").value();

        if(strcmp(node1.name(), "matrix") == 0)
        {
            trans._type = DaeTransformation::Type::MATRIX;

            char const * str = node1.text().get();
            if(str == nullptr)
                continue;
            for(int i = 0; i < 16; ++i)
            {
                char * end;
                float  f         = std::strtof(str, &end);
                trans._values[i] = f;
                str              = end;
            }

            _transStack.push_back(trans);
        }
        else if(strcmp(node1.name(), "translate") == 0)
        {
            trans._type = DaeTransformation::Type::TRANSLATION;

            char const * str = node1.text().get();
            char *       end;
            if(str == nullptr)
                continue;
            trans._values[0] = std::strtof(str, &end);
            str              = end;
            trans._values[1] = std::strtof(str, &end);
            str              = end;
            trans._values[2] = std::strtof(str, &end);

            _transStack.push_back(trans);
        }
        else if(strcmp(node1.name(), "rotate") == 0)
        {
            trans._type = DaeTransformation::Type::ROTATION;

            char const * str = node1.text().get();
            char *       end;
            if(str == nullptr)
                continue;
            trans._values[0] = std::strtof(str, &end);
            str              = end;
            trans._values[1] = std::strtof(str, &end);
            str              = end;
            trans._values[2] = std::strtof(str, &end);
            str              = end;
            trans._values[3] = std::strtof(str, &end);

            _transStack.push_back(trans);
        }
        else if(strcmp(node1.name(), "scale") == 0)
        {
            trans._type = DaeTransformation::Type::SCALE;

            char const * str = node1.text().get();
            char *       end;
            if(str == nullptr)
                continue;
            trans._values[0] = std::strtof(str, &end);
            str              = end;
            trans._values[1] = std::strtof(str, &end);
            str              = end;
            trans._values[2] = std::strtof(str, &end);

            _transStack.push_back(trans);
        }
        else if(strcmp(node1.name(), "skew") == 0)
        {
            std::cerr << "Warning: Unsupported transformation type 'skew'" << std::endl;
        }
    }

    // Parse instances
    for(node1 = node.first_child(); !node1.empty() && node1.name() != nullptr; node1 = node1.next_sibling())
    {
        if(strcmp(node1.name(), "instance_node") == 0)
        {
            std::string url = node1.attribute("url").value();
            RemoveGate(url);
            if(!url.empty())
            {
                auto nd = std::make_unique<DaeNode>();

                nd->_name      = url;
                nd->_reference = true;
                _children.push_back(std::move(nd));
            }
        }
        else if(strcmp(node1.name(), "instance_geometry") == 0
                || strcmp(node1.name(), "instance_controller") == 0)
        {
            std::string url = node1.attribute("url").value();
            RemoveGate(url);

            if(strcmp(node1.name(), "instance_controller") == 0)
            {
                pugi::xml_node skl     = node1.child("skeleton");
                std::string    rootSkl = skl.text().get();
                RemoveGate(rootSkl);
                _rootJointName = rootSkl;
            }

            if(!url.empty())
            {
                _instances.emplace_back();
                DaeInstance & inst = _instances.back();

                inst._url = url;

                // Find material bindings
                pugi::xml_node node2 = node1.child("bind_material");
                if(!node2.empty())
                {
                    pugi::xml_node node3 = node2.child("technique_common");
                    if(!node3.empty())
                    {
                        pugi::xml_node node4 = node3.child("instance_material");
                        for(; !node4.empty(); node4 = node4.next_sibling("instance_material"))
                        {
                            std::string s = node4.attribute("target").value();
                            RemoveGate(s);
                            inst._materialBindings[node4.attribute("symbol").value()] = s;
                        }
                    }
                }
            }
        }
    }

    // Parse children
    for(node1 = node.child("node"); node1; node1 = node1.next_sibling("node"))
    {
        auto nd = std::make_unique<DaeNode>();
        nd->Parse(node1, nd.get());

        _children.push_back(std::move(nd));
    }
}

/*******************************************************************************
 * DaeVisualScene
 *******************************************************************************/
DaeNode const * DaeVisualScene::FindNode(std::string const & id) const
{
    for(auto & ptr : _nodes)
    {
        if(ptr._id == id)
            return &ptr;
    }

    return nullptr;
}

void DaeVisualScene::Parse(pugi::xml_node const & visScene)
{
    _id = visScene.attribute("id").value();
    if(_id.empty())
        return;
    _name = visScene.attribute("name").value();

    for(pugi::xml_node node1 = visScene.child("node"); node1; node1 = node1.next_sibling("node"))
    {
        _nodes.emplace_back();
        _nodes.back().Parse(node1);
    }
}

/*******************************************************************************
 * DaeVisualScenes
 *******************************************************************************/
DaeVisualScene const * DaeLibraryVisualScenes::Find(std::string const & id) const
{
    for(auto & ptr : _scenes)
    {
        if(ptr._id == id)
            return &ptr;
    }

    return nullptr;
}

void DaeLibraryVisualScenes::Parse(pugi::xml_node const & root)
{
    pugi::xml_node node1 = root.child("library_visual_scenes");
    if(node1.empty())
        return;

    for(pugi::xml_node node2 = node1.child("visual_scene"); node2; node2 = node2.next_sibling("visual_scene"))
    {
        _scenes.emplace_back();
        _scenes.back().Parse(node2);
    }
}
