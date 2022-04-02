#include "ObjParser.h"
#include "ObjConverter.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

std::unique_ptr<Converter> ObjParser::GetConverter() const
{
    return std::make_unique<ObjConverter>(*this);
}

void ObjParser::Parse(const std::string & fname, const CmdLineOptions & cmd)
{
    std::ifstream in(fname, std::ios::in);
    if(!in)
    {
        std::stringstream ss;
        ss << "Error file: " << fname << " not open!" << std::endl;

        throw std::runtime_error(ss.str());
    }

    std::string line;
    SubMesh     sm;
    bool        first = true;

    while(std::getline(in, line))
    {
        if(line.substr(0, 2) == "v ")
        {
            std::istringstream s(line.substr(2));
            glm::vec3          v;
            s >> v.x;
            s >> v.y;
            s >> v.z;
            sm._pos.push_back(std::move(v));
        }
        else if(line.substr(0, 2) == "vt")
        {
            std::istringstream s(line.substr(2));
            glm::vec2          v;
            s >> v.x;
            s >> v.y;
            sm._tex.push_back(std::move(v));
        }
        else if(line.substr(0, 2) == "vn")
        {
            std::istringstream s(line.substr(2));
            glm::vec3          v;
            s >> v.x;
            s >> v.y;
            s >> v.z;
            sm._nrm.push_back(glm::normalize(v));
        }
        else if(line.substr(0, 6) == "usemtl")
        {
            std::string mat_name = line.substr(7);
            if(first)
            {
                first        = false;
                sm._mat_name = mat_name;
            }
            else
            {
                _meshes.push_back(std::move(sm));
                sm._mat_name = mat_name;
            }
        }
        else if(line.substr(0, 6) == "mtllib")
        {
            _matlib = line.substr(7);
        }
        else if(line.substr(0, 2) == "f ")
        {
            // split string with space delim
            std::vector<std::string> tokens;
            std::istringstream       iss(line.substr(2));
            std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(),
                      std::back_inserter(tokens));

            uint32_t              cur_offset = 0;
            std::vector<uint32_t> first_ind, last_ind, cur_ind;
            for(std::string & st : tokens)
            {
                // split string with delim '/'
                // http://stackoverflow.com/a/236803/863977
                std::vector<std::string> elems;
                std::stringstream        ss;
                ss.str(st);
                std::string item;
                while(std::getline(ss, item, '/'))
                {
                    elems.push_back(std::move(item));
                }

                for(std::string & el : elems)
                {
                    if(el.empty())
                    {
                        std::stringstream ss;
                        ss << "Obj file: " << fname << " doesnt have tex coord!" << std::endl;

                        throw std::runtime_error(ss.str());
                    }

                    int i = std::stoi(el);
                    if(i < 0)
                    {
                        // negative reference numbers
                        i = sm._pos.size() + 1 + i;
                    }
                    else
                        i--;

                    cur_ind.push_back(i);
                }

                // Do simple triangulation (assumes convex polygons)
                if(cur_offset == 0)
                {
                    first_ind = cur_ind;
                }
                if(++cur_offset > 3)
                {
                    sm._index.push_back(first_ind);
                    sm._index.push_back(last_ind);
                }

                sm._index.push_back(cur_ind);
                last_ind = cur_ind;
                cur_ind.clear();
            }
        }
    }
    _meshes.push_back(std::move(sm));

    if(_meshes[0]._tex.empty())
    {
        std::stringstream ss;
        ss << "Obj file: " << fname << " doesnt have tex coord!" << std::endl;

        throw std::runtime_error(ss.str());
    }

    if(!_meshes.empty() && _meshes[1]._pos.empty())
    {
        for(uint32_t i = 1; i < _meshes.size(); ++i)
        {
            _meshes[i]._pos = _meshes[0]._pos;
            _meshes[i]._nrm = _meshes[0]._nrm;
            _meshes[i]._tex = _meshes[0]._tex;
        }
    }
}
