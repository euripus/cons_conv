#ifndef _daeLibMaterials_H_
#define _daeLibMaterials_H_

#include "DaeLibraryEffects.h"
#include "DaeParser.h"
#include <string>
#include <vector>

struct DaeMaterial
{
    std::string id;
    std::string name;
    std::string effectId;
    DaeEffect * effect;
    bool        used;

    DaeMaterial()
    {
        used   = false;
        effect = nullptr;
    }

    bool Parse(pugi::xml_node const & matNode)
    {
        id   = matNode.attribute("id").value();
        name = matNode.attribute("name").value();
        if(name.empty())
            name = id;

        pugi::xml_node node1 = matNode.child("instance_effect");
        if(!node1.empty())
        {
            effectId = node1.attribute("url").value();
            RemoveGate(effectId);
        }

        return true;
    }
};

struct DaeLibraryMaterials
{
    std::vector<DaeMaterial> materials;

    DaeMaterial * FindMaterial(std::string const & id)
    {
        if(id == "")
            return nullptr;

        for(unsigned int i = 0; i < materials.size(); ++i)
        {
            if(materials[i].id == id)
                return &materials[i];
        }

        return nullptr;
    }

    void Parse(pugi::xml_node const & rootNode)
    {
        pugi::xml_node node1 = rootNode.child("library_materials");
        if(node1.empty())
            return;

        for(pugi::xml_node node2 = node1.child("material"); node2; node2 = node2.next_sibling("material"))
        {
            materials.emplace_back();
            if(!materials.back().Parse(node2))
                materials.pop_back();
        }
    }
};

#endif   // _daeLibMaterials_H_
