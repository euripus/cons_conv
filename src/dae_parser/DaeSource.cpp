#include "DaeSource.h"
#include "../utils.h"
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

void DaeSource::Parse(pugi::xml_node const & src)
{
    bool isFloatArray = true;
    _paramsPerItem    = 1;

    _id = src.attribute("id").value();
    if(_id.empty())
    {
        std::stringstream ss;
        ss << "source node doesnt have id '" << std::string(src.name()) << "'\n";

        throw std::runtime_error(ss.str());
    }

    pugi::xml_node arrayNode = src.child("float_array");
    if(arrayNode.empty())
    {
        arrayNode = src.child("Name_array");
        if(arrayNode.empty())
            arrayNode = src.child("IDREF_array");
        if(arrayNode.empty())
        {
            std::stringstream ss;
            ss << "source node array type undefined '" << std::string(src.attribute("id").value()) << "'\n";

            throw std::runtime_error(ss.str());
        }

        isFloatArray = false;
    }

    int count = std::atoi(arrayNode.attribute("count").value());

    if(count > 0)
    {
        // Check accessor
        int            numItems = count;
        pugi::xml_node node1    = src.child("technique_common");
        if(!node1.empty())
        {
            pugi::xml_node node2 = node1.child("accessor");
            if(!node2.empty())
                numItems = std::atoi(node2.attribute("count").value());
        }

        _paramsPerItem = count / numItems;   //  equivalent accessor.stride

        // Parse data
        char const * str = arrayNode.text().get();
        if(str == nullptr)
        {
            std::stringstream ss;
            ss << "source node doesnt have data '" << std::string(src.attribute("id").value()) << "'\n";

            throw std::runtime_error(ss.str());
        }

        std::stringstream ss;
        if(isFloatArray)
            _floatArray.reserve(count);
        else
        {
            _stringArray.reserve(count);
            std::string temp(str);
            std::for_each(temp.begin(), temp.end(), [](std::string::value_type & c) {
                if(c == '\t' || c == '\n' || c == '\r')
                    c = ' ';
            });
            // Skip whitespace
            while(temp[0] == ' ')
                temp = temp.substr(1, temp.length() - 1);

            ss.str(std::move(temp));
        }

        for(int i = 0; i < count; ++i)
        {
            if(isFloatArray)
            {
                char * end;
                float  f = std::strtof(str, &end);
                _floatArray.push_back(RoundEps(f));
                str = end;
            }
            else
            {
                std::string name;
                std::getline(ss, name, ' ');
                _stringArray.push_back(std::move(name));
            }
        }
    }
}
