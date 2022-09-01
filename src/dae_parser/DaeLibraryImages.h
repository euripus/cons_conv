#ifndef _daeLibImages_H_
#define _daeLibImages_H_

#include <pugixml.hpp>
#include <string>
#include <vector>

struct DaeImage
{
    std::string id;
    std::string name;
    std::string fileName;

    bool Parse(pugi::xml_node const & imageNode)
    {
        id = imageNode.attribute("id").value();
        if(id == "")
            return false;
        name = imageNode.attribute("name").value();
        if(name.empty())
            name = id;

        if(!imageNode.child("init_from").empty() && imageNode.child("init_from").text().get() != 0x0)
        {
            fileName = imageNode.child("init_from").text().get();
        }
        else
        {
            return false;
        }

        return true;
    }
};

struct DaeLibraryImages
{
    std::vector<DaeImage> images;

    DaeImage * FindImage(std::string const & id)
    {
        if(id == "")
            return nullptr;

        for(unsigned int i = 0; i < images.size(); ++i)
        {
            if(images[i].id == id)
                return &images[i];
        }

        return nullptr;
    }

    void Parse(pugi::xml_node const & rootNode)
    {
        pugi::xml_node node1 = rootNode.child("library_images");
        if(node1.empty())
            return;

        for(pugi::xml_node node2 = node1.child("image"); node2; node2 = node2.next_sibling("image"))
        {
            images.emplace_back();
            if(!images.back().Parse(node2))
                images.pop_back();
        }
    }
};

#endif   // _daeLibImages_H_
