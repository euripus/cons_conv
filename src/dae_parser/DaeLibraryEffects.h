#ifndef _daeLibEffects_H_
#define _daeLibEffects_H_

#include "DaeLibraryImages.h"
#include <cstring>
#include <string>
#include <vector>

struct DaeEffect
{
    std::string id;
    std::string name;
    std::string diffuseMapId;
    std::string diffuseColor;
    std::string specularColor;
    float       shininess;
    DaeImage *  diffuseMap;

    DaeEffect()
    {
        diffuseMap = nullptr;
        shininess  = 0.5f;
    }

    bool Parse(pugi::xml_node const & effectNode)
    {
        id = effectNode.attribute("id").value();
        if(id == "")
            return false;
        name = effectNode.attribute("name").value();
        if(name.empty())
            name = id;

        pugi::xml_node node1 = effectNode.child("profile_COMMON");
        if(node1.empty())
            return true;

        pugi::xml_node node2 = node1.child("technique");
        if(node2.empty())
            return true;

        pugi::xml_node node3 = node2.child("phong");
        if(node3.empty())
            node3 = node2.child("blinn");
        if(node3.empty())
            node3 = node2.child("lambert");
        if(node3.empty())
            return true;

        pugi::xml_node node6 = node3.child("specular");
        if(!node6.empty())
        {
            node6 = node6.child("color");
            if(!node6.empty())
                specularColor = node6.text().get();
        }

        node6 = node3.child("shininess");
        if(!node6.empty())
        {
            node6 = node6.child("float");
            if(!node6.empty())
            {
                shininess = (float)std::atof(node6.text().get());
                if(shininess > 1.0)
                    shininess /= 128.0f;
            }
        }

        pugi::xml_node node4 = node3.child("diffuse");
        if(node4.empty())
            return true;

        pugi::xml_node node5 = node4.child("texture");
        if(node5.empty())
        {
            pugi::xml_node node6 = node4.child("color");
            if(node6.empty())
                return true;
            diffuseColor = node6.text().get();
            return true;
        }

        std::string samplerId = node5.attribute("texture").value();
        if(samplerId == "")
            return true;

        // This is a hack to support files that don't completely respect the COLLADA standard
        // and use the texture image directly instead of sampler2D
        if(node1.child("newparam").empty())
        {
            diffuseMapId = samplerId;
            return true;
        }

        // Find sampler
        std::string surfaceId;
        for(node2 = node1.child("newparam"); node2; node2 = node2.next_sibling("newparam"))
        {
            if(node2.attribute("sid").value() != nullptr
               && std::strcmp(node2.attribute("sid").value(), samplerId.c_str()) == 0)
            {
                node3 = node2.child("sampler2D");
                if(node3.empty())
                    return true;

                node4 = node3.child("source");
                if(node4.empty())
                    return true;

                if(node4.text().get() != nullptr)
                    surfaceId = node4.text().get();

                break;
            }
        }

        // Find surface
        for(node2 = node1.child("newparam"); node2; node2 = node2.next_sibling("newparam"))
        {
            if(node2.attribute("sid").value() != nullptr
               && std::strcmp(node2.attribute("sid").value(), surfaceId.c_str()) == 0)
            {
                node3 = node2.child("surface");
                if(node3.empty() || node3.attribute("type").value() == nullptr)
                    return true;

                if(strcmp(node3.attribute("type").value(), "2D") == 0)
                {
                    node4 = node3.child("init_from");
                    if(node4.empty())
                        return true;

                    if(node4.text().get() != nullptr)
                        diffuseMapId = node4.text().get();
                }

                break;
            }
        }

        return true;
    }
};

struct DaeLibraryEffects
{
    std::vector<DaeEffect> effects;

    DaeEffect * FindEffect(std::string const & id)
    {
        if(id == "")
            return nullptr;

        for(unsigned int i = 0; i < effects.size(); ++i)
        {
            if(effects[i].id == id)
                return &effects[i];
        }

        return nullptr;
    }

    void Parse(pugi::xml_node const & rootNode)
    {
        pugi::xml_node node1 = rootNode.child("library_effects");
        if(node1.empty())
            return;

        for(pugi::xml_node node2 = node1.child("effect"); node2; node2 = node2.next_sibling("effect"))
        {
            effects.emplace_back();
            if(!effects.back().Parse(node2))
                effects.pop_back();
        }
    }
};

#endif   // _daeLibEffects_H_
