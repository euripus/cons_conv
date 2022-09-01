#ifndef DAESOURCE_H
#define DAESOURCE_H

#include <pugixml.hpp>
#include <string>
#include <vector>

struct DaeSource
{
    std::string              _id;
    std::vector<float>       _floatArray;
    std::vector<std::string> _stringArray;
    unsigned int             _paramsPerItem;

    void Parse(pugi::xml_node const & src);
};

#endif   // DAESOURCE_H
