#include "TxtExporter.h"
#include "../utils.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

// glm::to_string
#include <glm/gtx/string_cast.hpp>

TxtExporter::TxtExporter(CmdLineOptions const & cmd)
{
    geometry     = cmd.geometry;
    animation    = cmd.animation;
    material     = cmd.material_export;
    rel_matrices = cmd.relative;
}

void TxtExporter::WriteFile(std::string const & basic_fname, InternalData const & rep) const
{
    std::string new_geom_fname = basic_fname.substr(0, basic_fname.find('.')) + ".txt.msh";
    std::string new_anim_fname = basic_fname.substr(0, basic_fname.find('.')) + ".txt.anm";
    std::string new_matl_fname = basic_fname.substr(0, basic_fname.find('.')) + ".txt.mtl";

    if(geometry)
    {
        std::vector<InternalData::Weight> linear_weight;
        std::vector<unsigned int>         end_wieghts_vec;

        std::ofstream out(new_geom_fname, std::ofstream::out | std::ofstream::trunc);
        if(!out)
        {
            std::stringstream ss;
            ss << "Cannot open: " << new_geom_fname << std::endl;

            throw std::runtime_error(ss.str());
        }

        out << "meshes " << rep.meshes.size() << std::endl;
        out << std::endl;

        for(unsigned int j = 0; j < rep.meshes.size(); ++j)
        {
            auto const & msh = rep.meshes[j];

            if(!msh.weights.empty())
            {
                unsigned int count = 0;
                end_wieghts_vec.resize(msh.pos.size());

                for(unsigned int i = 0; i < msh.weights.size(); ++i)
                {
                    auto wv = msh.weights[i];
                    for(auto & w : wv)
                    {
                        linear_weight.push_back(w);
                        ++count;
                    }
                    end_wieghts_vec[i] = count;
                }
            }

            out << "mesh " << j << std::endl;
            out << "vertices " << msh.pos.size() << std::endl;
            out << "weights " << linear_weight.size() << std::endl;
            out << "bbox " << RoundEps(msh.bbox.min().x) << " " << RoundEps(msh.bbox.min().y) << " "
                << RoundEps(msh.bbox.min().z) << " " << RoundEps(msh.bbox.max().x) << " "
                << RoundEps(msh.bbox.max().y) << " " << RoundEps(msh.bbox.max().z) << std::endl;
            out << "material " << msh.material << std::endl;

            // Write positions
            std::for_each(msh.pos.begin(), msh.pos.end(), [&out](glm::vec3 const & v) {
                out << "vps " << RoundEps(v.x) << " " << RoundEps(v.y) << " " << RoundEps(v.z) << std::endl;
            });

            // Write normals
            std::for_each(msh.normal.begin(), msh.normal.end(), [&out](glm::vec3 const & v) {
                out << "vnr " << RoundEps(v.x) << " " << RoundEps(v.y) << " " << RoundEps(v.z) << std::endl;
            });

            // Write tangent
            std::for_each(msh.tangent.begin(), msh.tangent.end(), [&out](glm::vec3 const & v) {
                out << "vtg " << RoundEps(v.x) << " " << RoundEps(v.y) << " " << RoundEps(v.z) << std::endl;
            });

            // Write bitangent
            std::for_each(msh.bitangent.begin(), msh.bitangent.end(), [&out](glm::vec3 const & v) {
                out << "vbt " << RoundEps(v.x) << " " << RoundEps(v.y) << " " << RoundEps(v.z) << std::endl;
            });

            // Write tex coords
            out << "tex_channels " << msh.tex_coords.size() << std::endl;
            for(unsigned int i = 0; i < msh.tex_coords.size(); i++)
            {
                std::stringstream ss;
                ss << "tx" << i;
                std::for_each(
                    msh.tex_coords[i].begin(), msh.tex_coords[i].end(), [&out, &ss](auto const & v) {
                        out << ss.str() << " " << RoundEps(v.x) << " " << RoundEps(v.y) << std::endl;
                    });
            }

            out << "triangles " << msh.indexes.size() / 3 << std::endl;
            for(unsigned int i = 0; i < msh.indexes.size(); i += 3)
            {
                out << "fcx " << msh.indexes[i + 0] << " " << msh.indexes[i + 1] << " " << msh.indexes[i + 2]
                    << std::endl;
            }

            // Write weights
            if(!msh.weights.empty())
            {
                std::for_each(end_wieghts_vec.begin(), end_wieghts_vec.end(),
                              [&out](unsigned int i) { out << "wgi " << i << std::endl; });
                std::for_each(linear_weight.begin(), linear_weight.end(), [&out](auto & w) {
                    out << "wgh " << w.joint_index << " " << RoundEps(w.w) << std::endl;
                });
                out << std::endl;

                linear_weight.clear();
                end_wieghts_vec.clear();
            }
        }

        // Write joints
        if(!rep.joints.empty())
        {
            out << "bones " << rep.joints.size() << std::endl;
            for(auto const & jnt : rep.joints)
            {
                glm::quat rot    = glm::quat_cast(jnt.inverse_bind);
                rot              = glm::normalize(rot);
                glm::vec4 transf = glm::column(jnt.inverse_bind, 3);

                out << "jnt " << jnt.index << " " << jnt.parent << " " << jnt.name << " " << RoundEps(rot.x)
                    << " " << RoundEps(rot.y) << " " << RoundEps(rot.z) << " " << RoundEps(rot.w) << " "
                    << RoundEps(transf.x) << " " << RoundEps(transf.y) << " " << RoundEps(transf.z)
                    << std::endl;
            }
            out << std::endl;
        }
    }

    if(material && !rep.materials.empty())
    {
        std::ofstream out(new_matl_fname, std::ofstream::out | std::ofstream::trunc);
        if(!out)
        {
            std::stringstream ss;
            ss << "Cannot open: " << new_matl_fname << std::endl;

            throw std::runtime_error(ss.str());
        }

        for(auto const & mtl : rep.materials)
        {
            out << "Material name: " << mtl.name << std::endl;

            if(mtl.tex_name.empty())
            {
                out << "Diffuse: " << mtl.diffusecolor.x << " " << mtl.diffusecolor.y << " "
                    << mtl.diffusecolor.z << " " << mtl.diffusecolor.w << std::endl;
            }
            else
            {
                out << "Texture: " << mtl.tex_name << std::endl;
            }

            out << "Specular: " << mtl.specularcolor.x << " " << mtl.specularcolor.y << " "
                << mtl.specularcolor.z << " " << mtl.specularcolor.w << std::endl;

            out << "Shininess: " << mtl.shininess << std::endl;
        }
    }

    if(animation)
    {
        if(rep.num_frames == 0)
            return;

        std::ofstream out(new_anim_fname, std::ofstream::out | std::ofstream::trunc);
        if(!out)
        {
            std::stringstream ss;
            ss << "Cannot open: " << new_anim_fname << std::endl;

            throw std::runtime_error(ss.str());
        }

        // Write joints
        out << "bones " << rep.joints.size() << std::endl;
        for(auto const & jnt : rep.joints)
        {
            glm::quat rot    = glm::quat_cast(jnt.inverse_bind);
            rot              = glm::normalize(rot);
            glm::vec4 transf = glm::column(jnt.inverse_bind, 3);

            out << "jnt " << jnt.index << " " << jnt.parent << " " << jnt.name << " " << RoundEps(rot.x)
                << " " << RoundEps(rot.y) << " " << RoundEps(rot.z) << " " << RoundEps(rot.w) << " "
                << RoundEps(transf.x) << " " << RoundEps(transf.y) << " " << RoundEps(transf.z) << std::endl;
            //out << glm::to_string(glm::transpose(jnt.inverse_bind)) << std::endl;
        }
        out << std::endl;

        // Write animations
        assert(rep.joints[0].a_rot.size() == rep.num_frames);
        assert(rep.joints[0].a_trans.size() == rep.num_frames);

        out << "frames " << rep.num_frames << std::endl;
        out << "framerate " << rep.frame_rate << std::endl;
        out << std::endl;

        for(uint32_t i = 0; i < rep.num_frames; i++)
        {
            out << "frame " << i << std::endl;
            out << "bbox " << RoundEps(rep.bboxes[i].min().x) << " " << RoundEps(rep.bboxes[i].min().y) << " "
                << RoundEps(rep.bboxes[i].min().z) << " " << RoundEps(rep.bboxes[i].max().x) << " "
                << RoundEps(rep.bboxes[i].max().y) << " " << RoundEps(rep.bboxes[i].max().z) << std::endl;

            for(auto const & jnt : rep.joints)
            {
                if(rel_matrices)
                {
                    out << "jtr " << RoundEps(jnt.r_rot[i].x) << " " << RoundEps(jnt.r_rot[i].y) << " "
                        << RoundEps(jnt.r_rot[i].z) << " " << RoundEps(jnt.r_rot[i].w) << " "
                        << RoundEps(jnt.r_trans[i].x) << " " << RoundEps(jnt.r_trans[i].y) << " "
                        << RoundEps(jnt.r_trans[i].z) << std::endl;

                    //glm::mat4 mt = glm::mat4_cast(jnt.r_rot[i]);
                    //mt           = glm::column(mt, 3, glm::vec4(jnt.r_trans[i], 1.0f));
                    //out << glm::to_string(glm::transpose(mt)) << std::endl;
                }
                else
                    out << "jtr " << RoundEps(jnt.a_rot[i].x) << " " << RoundEps(jnt.a_rot[i].y) << " "
                        << RoundEps(jnt.a_rot[i].z) << " " << RoundEps(jnt.a_rot[i].w) << " "
                        << RoundEps(jnt.a_trans[i].x) << " " << RoundEps(jnt.a_trans[i].y) << " "
                        << RoundEps(jnt.a_trans[i].z) << std::endl;
            }
            out << std::endl;
        }
    }
}
