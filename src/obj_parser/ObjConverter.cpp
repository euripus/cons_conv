#include "ObjConverter.h"
#include "../utils.h"
#include <fstream>
#include <iostream>
#include <sstream>

void ObjConverter::Convert() {}

void ObjConverter::ReadMaterials(std::string const & fname, InternalData & rep) const
{
    std::ifstream in(fname, std::ios::in);
    if(!in)
    {
        std::cout << "Warning! Material file:" << fname << " for obj file not found" << std::endl;
        return;
    }

    std::string            line;
    InternalData::Material mat;
    bool                   first = true;
    while(std::getline(in, line))
    {
        if(line.substr(0, 2) == "Kd")
        {
            std::istringstream s(line.substr(2));
            glm::vec3          v;
            s >> v.x;
            s >> v.y;
            s >> v.z;
            mat.diffusecolor = glm::vec4(v, 1.0);
        }
        else if(line.substr(0, 2) == "Ks")
        {
            std::istringstream s(line.substr(2));
            glm::vec3          v;
            s >> v.x;
            s >> v.y;
            s >> v.z;
            mat.specularcolor = glm::vec4(v, 1.0);
        }
        else if(line.substr(0, 2) == "Ns")
        {
            std::istringstream s(line.substr(2));
            float              v;
            s >> v;
            mat.shininess = v / 1000.0;
        }
        else if(line.substr(0, 2) == "d ")
        {
            std::istringstream s(line.substr(2));
            float              v;
            s >> v;
            mat.diffusecolor.a  = v;
            mat.specularcolor.a = v;
        }
        else if(line.substr(0, 6) == "map_Kd")
        {
            std::string name = line.substr(7);
            if(name.size() > 4)   // extension + . + name of file
                mat.tex_name = name;
        }
        else if(line.substr(0, 6) == "newmtl")
        {
            std::string mat_name = line.substr(7);
            if(first)
            {
                first    = false;
                mat.name = mat_name;
            }
            else
            {
                rep.materials.push_back(std::move(mat));
                mat.name = mat_name;
            }
        }
    }
    rep.materials.push_back(std::move(mat));
}

struct CurrObjVertexData
{
    glm::vec3 pos       = glm::vec3{0.0f};
    bool      is_normal = {false};
    glm::vec3 nor       = glm::vec3{0.0f};
    glm::vec2 tex       = glm::vec2{0.0f};
};

void PushVertexData(InternalData::SubMesh & msh, CurrObjVertexData const & vd)
{
    if(msh.tex_coords.empty())
        msh.tex_coords.resize(1);

    msh.pos.push_back(vd.pos);
    if(vd.is_normal)
        msh.normal.push_back(vd.nor);
    msh.tex_coords[0].push_back(vd.tex);
}

bool FindSimilarVertex(CurrObjVertexData const & vd, InternalData::SubMesh const & msh,
                       unsigned int * foundInd)
{
    for(unsigned int i = 0; i < msh.pos.size(); i++)
    {
        bool isEqual = false;

        isEqual = VecEqual(vd.pos, msh.pos[i]);
        if(vd.is_normal)
            isEqual = isEqual && VecEqual(vd.nor, msh.normal[i]);
        isEqual = isEqual && VecEqual(vd.tex, msh.tex_coords[0][i]);

        if(isEqual)
        {
            *foundInd = i;
            return true;
        }
    }

    return false;
}

void ObjConverter::ExportToInternal(InternalData & rep, CmdLineOptions const & cmd) const
{
    if(cmd.material_export && !_parser._matlib.empty())
        ReadMaterials(_parser._matlib, rep);

    for(auto const & msh : _parser._meshes)
    {
        InternalData::SubMesh sm;
        sm.material = msh._mat_name;

        for(uint32_t i = 0; i < msh._index.size(); ++i)
        {
            CurrObjVertexData vd;
            bool              found         = false;
            uint32_t          founded_index = 0;

            vd.pos = msh._pos[msh._index[i][0]];
            if(msh._index[0].size() > 2)
            {
                vd.is_normal = true;
                vd.nor       = msh._nrm[msh._index[i][2]];
                vd.tex       = msh._tex[msh._index[i][1]];
            }
            else
                vd.tex = msh._tex[msh._index[i][1]];

            found = FindSimilarVertex(vd, sm, &founded_index);
            if(found)
            {
                sm.indexes.push_back(founded_index);
            }
            else
            {
                PushVertexData(sm, vd);

                unsigned int new_index = sm.pos.size() - 1;
                sm.indexes.push_back(new_index);
            }
        }
        rep.meshes.push_back(std::move(sm));
    }

    rep.CalculateBBoxes();
}
