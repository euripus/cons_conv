#include "DaeConverter.h"
#include "../utils.h"
#include "DaeLibraryAnimations.h"
#include "DaeLibraryControllers.h"
#include "DaeLibraryGeometries.h"
#include "DaeLibraryMaterials.h"
#include "DaeLibraryVisualScenes.h"
#include <algorithm>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>

// glm::to_string
#include <glm/gtx/string_cast.hpp>

/******************************************************************************
 *
 ******************************************************************************/
glm::mat4 CreateTransformMatrix(std::vector<DaeTransformation> const & trans_stack)
{
    glm::mat4 mat(1.0), dae_mat(1.0);
    glm::mat4 scale(1.0), rotate(1.0), trans(1.0);

    for(auto & tr : trans_stack)
    {
        switch(tr._type)
        {
            case DaeTransformation::Type::SCALE:
                {
                    scale = glm::scale(scale, glm::vec3(tr._values[0], tr._values[1], tr._values[2]));
                    break;
                }
            case DaeTransformation::Type::ROTATION:
                {
                    rotate = glm::rotate(rotate, tr._values[3],
                                         glm::vec3(tr._values[0], tr._values[1], tr._values[2]));
                    break;
                }
            case DaeTransformation::Type::TRANSLATION:
                {
                    trans = glm::translate(trans, glm::vec3(tr._values[0], tr._values[1], tr._values[2]));
                    break;
                }
            case DaeTransformation::Type::MATRIX:
                {
                    dae_mat = CreateDAEMatrix(tr._values);
                    break;
                }
        }
    }

    mat = dae_mat * trans * rotate * scale;   /// MATRIX transform order
    return mat;
}

/******************************************************************************
 *
 ******************************************************************************/
DaeConverter::DaeConverter(DaeParser const & parser) :
    m_parser(parser), m_frame_count(0), m_max_anim_time(0.0f)
{}

void DaeConverter::Convert()
{
    if(m_parser._v_scenes->_scenes.empty())
    {
        std::stringstream ss;
        ss << "Error: No scenes to export"
           << "\n";

        throw std::runtime_error(ss.str());
    }

    for(auto & sc : m_parser._v_scenes->_scenes)
    {
        ConvertScene(sc);
    }
}

void DaeConverter::ConvertScene(DaeVisualScene const & sc)
{
    if(sc._nodes.empty())
    {
        std::stringstream ss;
        ss << "Error: No nodes in scene"
           << "\n";

        throw std::runtime_error(ss.str());
    }

    if(!m_parser._anim->_animations.empty())
    {
        m_frame_count   = m_parser._anim->_max_frame_count;
        m_max_anim_time = m_parser._anim->_max_anim_time;
    }

    glm::mat4 rt = GetConvertMatrix(m_parser._up_axis);
    // For Z_UP equivalent:
    // glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    std::vector<glm::mat4> anim_trans_accum;
    for(unsigned int i = 0; i < m_frame_count; ++i)
        anim_trans_accum.push_back(rt);

    for(size_t i = 0; i < sc._nodes.size(); i++)
    {
        ProcessNode(sc._nodes[i], nullptr, rt, sc, anim_trans_accum);
    }

    if(!m_joints.empty())
        ProcessJoints();

    if(!m_meshes.empty())
        ProcessMeshes();
}

SceneNode * DaeConverter::ProcessNode(DaeNode const & node, SceneNode * parent, glm::mat4 trans_accum,
                                      DaeVisualScene const & sc, std::vector<glm::mat4> anim_trans_accum)
{
    // Note: animTransAccum is used for pure transformation nodes of Collada that are no joints or meshes
    if(node._reference)
    {
        DaeNode const * nd = sc.FindNode(node._name);
        if(nd)
            return ProcessNode(*nd, parent, trans_accum, sc, anim_trans_accum);
        else
        {
            std::cout << "Warning: undefined reference '" + node._name + "' in instance_node" << std::endl;
            return nullptr;
        }
    }

    glm::mat4 rel_mat = CreateTransformMatrix(node._trans_stack);

    SceneNode * scene_node = nullptr;

    if(node._joint)
    {
        auto jnt        = std::make_unique<JointNode>();
        jnt->m_dae_node = &node;
        jnt->m_parent   = parent;

        scene_node = jnt.get();
        m_joints.push_back(std::move(jnt));
    }
    else
    {
        if(!node._instances.empty())
        {
            auto mesh        = std::make_unique<MeshNode>();
            mesh->m_dae_node = &node;
            mesh->m_parent   = parent;

            scene_node = mesh.get();
            m_meshes.push_back(std::move(mesh));
        }
    }

    if(scene_node != nullptr)
    {
        scene_node->m_rel_transf = rel_mat;

        if(parent != nullptr)
        {
            scene_node->m_abs_transf = parent->m_abs_transf * scene_node->m_rel_transf;
        }
        else
        {
            scene_node->m_abs_transf = trans_accum * scene_node->m_rel_transf;
        }

        trans_accum = glm::mat4(1.0f);
    }
    else
    {
        trans_accum = trans_accum * rel_mat;
    }

    // Animation
    for(uint32_t i = 0; i < m_frame_count; ++i)
    {
        glm::mat4 mat = GetNodeTransform(node, scene_node, i);
        if(scene_node != nullptr)
        {
            if(scene_node->m_parent == nullptr)
                mat = anim_trans_accum[i] * mat;

            scene_node->m_r_frames.push_back(mat);
            anim_trans_accum[i] = glm::mat4(1.0f);
        }
        else
            anim_trans_accum[i] = anim_trans_accum[i] * mat;   // Pure transformation node
    }

    for(auto & chd : node._children)
    {
        SceneNode * par_node = scene_node != nullptr ? scene_node : parent;
        SceneNode * snd      = ProcessNode(*chd, par_node, trans_accum, sc, anim_trans_accum);
        if(snd != nullptr && par_node != nullptr)
            par_node->m_child.push_back(snd);
    }

    return scene_node;
}

glm::mat4 DaeConverter::GetNodeTransform(DaeNode const & node, SceneNode const * scene_node,
                                         uint32_t frame) const
{
    int          tr_index = -1;
    glm::mat4    tr       = glm::mat4(1.0);
    DaeSampler * sampler  = m_parser._anim->FindAnimForTarget(node._id, &tr_index);

    if(sampler != nullptr)
    {
        if(sampler->_output->_floatArray.size() == m_frame_count * 16)
        {
            tr = CreateDAEMatrix(&sampler->_output->_floatArray[frame * 16]);
        }
        else
        {
            std::stringstream ss;
            ss << "Error: Animation data not sampled"
               << "\n";

            throw std::runtime_error(ss.str());
        }
    }
    else
    {
        // If no animation data is found, use standard transformation
        if(scene_node != nullptr)
        {
            tr = scene_node->m_rel_transf;
        }
        else
            tr = CreateTransformMatrix(node._trans_stack);
    }

    return tr;
}

void CalcJointFrame(JointNode * nd, JointNode * parent)
{
    for(uint32_t i = 0; i < nd->m_r_frames.size(); i++)
    {
        glm::mat4 par_tr = glm::mat4(1.0);
        if(parent != nullptr)
            par_tr = parent->m_a_frames[i];
        nd->m_a_frames.push_back(par_tr * nd->m_r_frames[i]);
    }

    for(uint32_t i = 0; i < nd->m_child.size(); i++)
    {
        auto chd = dynamic_cast<JointNode *>(nd->m_child[i]);
        if(chd == nullptr)
        {
            std::stringstream ss;
            ss << "Error: Armature has mesh node" << std::endl;
            throw std::runtime_error(ss.str());
        }
        CalcJointFrame(chd, nd);
    }
}

void DaeConverter::CalcAbsTransfMatrices()
{
    auto it =
        std::find_if(m_joints.begin(), m_joints.end(),
                     [](std::unique_ptr<JointNode> const & nd) -> bool { return nd->m_parent == nullptr; });
    if(it != m_joints.end())
    {
        JointNode * root_joint = (*it).get();
        CalcJointFrame(root_joint, nullptr);
    }
}

void DaeConverter::ProcessJoints()
{
    unsigned int i = 1;
    for(auto & jnt : m_joints)
    {
        jnt->m_index = i;
        i++;
    }

    if(m_frame_count > 0)
        CalcAbsTransfMatrices();
}

VertexData::Semantic ConvertSemantic(DaeGeometry::Semantic sem)
{
    VertexData::Semantic s = VertexData::Semantic::UNKNOWN;

    switch(sem)
    {
        case DaeGeometry::Semantic::VERTEX:
            {
                s = VertexData::Semantic::POSITION;
                break;
            }
        case DaeGeometry::Semantic::BINORMAL:
            {
                s = VertexData::Semantic::BINORMAL;
                break;
            }
        case DaeGeometry::Semantic::COLOR:
            {
                s = VertexData::Semantic::COLOR;
                break;
            }
        case DaeGeometry::Semantic::NORMAL:
            {
                s = VertexData::Semantic::NORMAL;
                break;
            }
        case DaeGeometry::Semantic::TANGENT:
            {
                s = VertexData::Semantic::TANGENT;
                break;
            }
        case DaeGeometry::Semantic::TEXBINORMAL:
            {
                s = VertexData::Semantic::TEXBINORMAL;
                break;
            }
        case DaeGeometry::Semantic::TEXCOORD:
            {
                s = VertexData::Semantic::TEXCOORD;
                break;
            }
        case DaeGeometry::Semantic::TEXTANGENT:
            {
                s = VertexData::Semantic::TEXTANGENT;
                break;
            }
    }
    return s;
}

unsigned int DaeConverter::FindJointIndex(std::string const & name) const
{
    unsigned int indx = 0;

    auto it = std::find_if(m_joints.begin(), m_joints.end(),
                           [&name](auto & nd) -> bool { return nd->m_dae_node->_sid == name; });
    if(it != m_joints.end())
    {
        indx = (*it)->m_index;
    }

    return indx;
}

void DaeConverter::ProcessMeshes()
{
    for(auto & msh : m_meshes)
    {
        glm::mat4 trans = msh->m_abs_transf;

        MeshNode *  cur_mesh = msh.get();
        DaeSkin *   skn      = nullptr;
        std::string geo_src;

        for(size_t ins = 0; ins < cur_mesh->m_dae_node->_instances.size(); ins++)
        {
            if(!m_parser._controllers->_skinControllers.empty())
            {
                std::string skin_id = cur_mesh->m_dae_node->_instances[ins]._url;
                auto        it =
                    std::find_if(m_parser._controllers->_skinControllers.begin(),
                                 m_parser._controllers->_skinControllers.end(),
                                 [&skin_id](DaeSkin const & skn) -> bool { return skn._id == skin_id; });

                if(it != m_parser._controllers->_skinControllers.end())
                {
                    skn     = &(*it);
                    geo_src = skn->_owner_id;
                }
                else
                {
                    std::stringstream ss;
                    ss << "ERROR! Controller node not found. Skin id: " << skin_id << "\n";

                    throw std::runtime_error(ss.str());
                }
            }
            else
            {
                geo_src = cur_mesh->m_dae_node->_instances[ins]._url;
            }

            // Check that skin has all required arrays
            if(skn != nullptr)
            {
                if(skn->_joint_array == nullptr || skn->_bind_mat_array == nullptr
                   || skn->_weight_array == nullptr)
                {
                    std::cout << "Skin controller '" << skn->_id
                              << "' is missing information and is ignored\n";
                    skn = nullptr;
                }
            }

            DaeMeshNode const *      geometry = m_parser._geom->Find(geo_src);
            std::vector<JointNode *> joint_lookup;

            if(!geometry)
            {
                std::stringstream ss;
                ss << "ERROR! Geometry data not found. Geometry name: " << geo_src << "\n";

                throw std::runtime_error(ss.str());
            }

            if(skn != nullptr)
            {
                // Build lookup table
                for(unsigned int j = 0; j < skn->_joint_array->_stringArray.size(); ++j)
                {
                    std::string sid = skn->_joint_array->_stringArray[j];

                    joint_lookup.push_back(nullptr);

                    bool found = false;
                    for(auto & jnt : m_joints)
                    {
                        if(jnt->m_dae_node->_sid == sid)
                        {
                            joint_lookup[j] = jnt.get();
                            found           = true;
                            break;
                        }
                    }

                    if(!found)
                    {
                        std::cout << "Warning: joint '" << sid << "' used in skin controller not found\n";
                    }
                }

                // Find bind matrices
                for(unsigned int j = 0; j < skn->_joint_array->_stringArray.size(); ++j)
                {
                    if(joint_lookup[j] != nullptr)
                    {
                        // joint_lookup[j]->m_inv_bind_mat =
                        //     CreateDAEMatrix(&skn->_bind_mat_array->_floatArray[j * 16]);
                        joint_lookup[j]->m_inv_bind_mat = glm::inverse(joint_lookup[j]->m_abs_transf);
                    }
                }
            }

            // copy vertex data
            for(auto & attr : geometry->_tri_groups[0]._meshes[0]._attributes)
            {
                if(attr.pos_source_it != geometry->_pos_sources.end())
                {
                    // copy vertex
                    VertexData::ChunkVec3 pos;
                    pos.m_semantic = VertexData::Semantic::POSITION;

                    auto     pos_src    = (*(*attr.pos_source_it)._pos_source_it);
                    uint32_t num_vertex = pos_src._floatArray.size() / pos_src._paramsPerItem;

                    for(uint32_t i = 0; i < num_vertex; ++i)
                    {
                        glm::vec3 vertex(0.0f);

                        vertex.x = pos_src._floatArray[i * pos_src._paramsPerItem + 0];
                        vertex.y = pos_src._floatArray[i * pos_src._paramsPerItem + 1];
                        vertex.z = pos_src._floatArray[i * pos_src._paramsPerItem + 2];

                        pos.m_data.push_back(vertex);
                    }

                    cur_mesh->m_vertices.m_sources_vec3.push_back(std::move(pos));

                    if(skn != nullptr)
                    {
                        // copy weights
                        for(uint32_t i = 0; i < num_vertex; i++)
                        {
                            VertexData::WeightsVec vert_weight_vect;

                            for(auto skn_w : skn->_vert_weights[i])
                            {
                                VertexData::WeightsVec::Weight tw{0, 1.0f};

                                tw.m_joint_index =
                                    FindJointIndex(skn->_joint_array->_stringArray[skn_w._joint]);
                                tw.m_w = skn->_weight_array->_floatArray[skn_w._weight];

                                vert_weight_vect.m_weights.push_back(tw);
                            }

                            cur_mesh->m_vertices.m_wght.push_back(vert_weight_vect);
                        }
                    }
                }
                else
                {
                    auto src = *attr.source_it;

                    if(src._paramsPerItem == 2)
                    {
                        VertexData::ChunkVec2 chunk;
                        chunk.m_semantic    = ConvertSemantic(attr.semantic);
                        uint32_t num_vertex = src._floatArray.size() / 2;

                        for(uint32_t i = 0; i < num_vertex; ++i)
                        {
                            glm::vec2 vertex(0.0f);

                            vertex.x = src._floatArray[i * 2 + 0];
                            vertex.y = src._floatArray[i * 2 + 1];

                            chunk.m_data.push_back(vertex);
                        }

                        cur_mesh->m_vertices.m_sources_vec2.push_back(std::move(chunk));
                    }
                    else
                    {
                        VertexData::ChunkVec3 chunk;
                        chunk.m_semantic    = ConvertSemantic(attr.semantic);
                        uint32_t num_vertex = src._floatArray.size() / 3;

                        for(uint32_t i = 0; i < num_vertex; ++i)
                        {
                            glm::vec3 vertex(0.0f);

                            vertex.x = src._floatArray[i * 3 + 0];
                            vertex.y = src._floatArray[i * 3 + 1];
                            vertex.z = src._floatArray[i * 3 + 2];

                            chunk.m_data.push_back(vertex);
                        }

                        cur_mesh->m_vertices.m_sources_vec3.push_back(std::move(chunk));
                    }
                }
            }

            // copy index
            for(auto & mesh : geometry->_tri_groups)
            {
                auto & inp = geometry->_tri_groups[0]._meshes[0]._attributes;
                for(auto & poly : mesh._meshes)
                {
                    assert(poly._triangles[0].size() == inp.size());

                    TriGroup      tg;
                    DaeMaterial * mat = m_parser._material->FindMaterial(poly._material_name);
                    if(mat != nullptr)
                    {
                        tg.m_mat_id = mat->name;
                        mat->used   = true;
                    }
                    else
                    {
                        std::cout << "Warning: Material '" << poly._material_name << "' not found"
                                  << std::endl;
                        tg.m_mat_id = poly._material_name;
                    }

                    // copy index
                    for(auto & tri : poly._triangles)
                    {
                        TriGroup::VertexAttribut index;

                        for(unsigned int i = 0; i < tri.size(); ++i)
                        {
                            index.push_back(tri[i]);
                        }

                        tg.m_indices.push_back(std::move(index));
                    }
                    cur_mesh->m_polylists.push_back(std::move(tg));
                }
            }

            // Apply bind_shape matrix
            if(skn != nullptr)
            {
                trans = trans * skn->_bind_shape_mat;
            }

            for(auto & vec3_array : cur_mesh->m_vertices.m_sources_vec3)
            {
                std::for_each(vec3_array.m_data.begin(), vec3_array.m_data.end(),
                              [&trans, sem = vec3_array.m_semantic](glm::vec3 & vt) {
                                  glm::vec4 tmp(0.0f);
                                  if(sem == VertexData::Semantic::POSITION)
                                      tmp = glm::vec4(vt, 1.0f);
                                  else
                                      tmp = glm::vec4(vt, 0.0f);

                                  tmp = trans * tmp;

                                  if(sem == VertexData::Semantic::POSITION && tmp.w != 1.0f)
                                  {
                                      std::cout << "Warning. Transform matrix not affine. "
                                                << "Result may be incorrect.\n";
                                  }

                                  vt = glm::vec3(tmp);
                              });
            }
        }
    }
}

struct CurrVertexData
{
    glm::vec3              pos          = glm::vec3{0.0f};   // Required
    bool                   is_normal    = {false};
    glm::vec3              normal       = glm::vec3{0.0f};
    bool                   is_tangent   = {false};
    glm::vec3              tangent      = glm::vec3{0.0f};
    bool                   is_bitangent = {false};
    glm::vec3              bitangent    = glm::vec3{0.0f};
    bool                   is_color     = {false};
    glm::vec3              color        = glm::vec3{0.0f};
    std::vector<glm::vec2> tex_coords   = {};   // 0 Required
};

void PushVertexData(InternalData::SubMesh & msh, CurrVertexData const & vd)
{
    msh.pos.push_back(vd.pos);
    if(vd.is_normal)
        msh.normal.push_back(vd.normal);
    if(vd.is_tangent)
        msh.tangent.push_back(vd.tangent);
    if(vd.is_bitangent)
        msh.bitangent.push_back(vd.bitangent);
    if(vd.is_color)
        msh.color.push_back(vd.color);

    // first
    if(vd.tex_coords.size() != msh.tex_coords.size())
    {
        msh.tex_coords.clear();
        msh.tex_coords.resize(vd.tex_coords.size());
    }

    for(unsigned int i = 0; i < vd.tex_coords.size(); ++i)
        msh.tex_coords[i].push_back(vd.tex_coords[i]);
}

bool FindSimilarVertex(CurrVertexData const & vd, InternalData::SubMesh const & msh, unsigned int * found_ind)
{
    for(unsigned int i = 0; i < msh.pos.size(); i++)
    {
        bool is_equal = false;

        is_equal = VecEqual(vd.pos, msh.pos[i]);
        if(vd.is_normal)
            is_equal = is_equal && VecEqual(vd.normal, msh.normal[i]);
        if(vd.is_tangent)
            is_equal = is_equal && VecEqual(vd.tangent, msh.tangent[i]);
        if(vd.is_bitangent)
            is_equal = is_equal && VecEqual(vd.bitangent, msh.bitangent[i]);
        if(vd.is_color)
            is_equal = is_equal && VecEqual(vd.color, msh.color[i]);

        for(unsigned int j = 0; j < vd.tex_coords.size(); j++)
        {
            is_equal = is_equal && VecEqual(vd.tex_coords[j], msh.tex_coords[j][i]);
        }

        if(is_equal)
        {
            *found_ind = i;
            return true;
        }
    }

    return false;
}

InternalData::WeightsVec GetWeightsForVertex(VertexData::WeightsVec const & wvec)
{
    InternalData::WeightsVec wv;
    InternalData::Weight     temp_weight{0, 0.0f};

    for(auto const & w : wvec.m_weights)
    {
        temp_weight.joint_index = w.m_joint_index;
        temp_weight.w           = w.m_w;
        wv.push_back(temp_weight);
    }

    return wv;
}

void DaeConverter::ExportToInternal(InternalData & rep, CmdLineOptions const & cmd) const
{
    assert(rep.meshes.empty());

    if(m_meshes.empty())
    {
        std::cout << "WARNING! Nothing to export, no gometry data\n";
    }

    for(auto & mesh : m_meshes)
    {
        for(auto & poly : mesh->m_polylists)
        {
            InternalData::SubMesh sub_poly;
            sub_poly.material = poly.m_mat_id;

            for(auto & indx : poly.m_indices)
            {
                CurrVertexData vd;
                bool           found;
                unsigned int   founded_index = 0;
                unsigned int   pos_index     = 0;

                for(unsigned int j = 0; j < indx.size(); ++j)
                {
                    if(j < mesh->m_vertices.m_sources_vec3.size())
                    {
                        switch(mesh->m_vertices.m_sources_vec3[j].m_semantic)
                        {
                            case VertexData::Semantic::POSITION:
                                {
                                    vd.pos    = mesh->m_vertices.m_sources_vec3[j].m_data[indx[j]];
                                    pos_index = indx[j];
                                    break;
                                }
                            case VertexData::Semantic::NORMAL:
                                {
                                    vd.is_normal = true;
                                    vd.normal    = mesh->m_vertices.m_sources_vec3[j].m_data[indx[j]];
                                    break;
                                }
                            case VertexData::Semantic::COLOR:
                                {
                                    vd.is_color = true;
                                    vd.color    = mesh->m_vertices.m_sources_vec3[j].m_data[indx[j]];
                                    break;
                                }
                            case VertexData::Semantic::TANGENT:
                            case VertexData::Semantic::TEXTANGENT:
                                {
                                    vd.is_tangent = true;
                                    vd.tangent    = mesh->m_vertices.m_sources_vec3[j].m_data[indx[j]];
                                    break;
                                }
                            case VertexData::Semantic::BINORMAL:
                            case VertexData::Semantic::TEXBINORMAL:
                                {
                                    vd.is_bitangent = true;
                                    vd.bitangent    = mesh->m_vertices.m_sources_vec3[j].m_data[indx[j]];
                                    break;
                                }
                        }
                    }
                    else
                    {
                        unsigned int t   = j - mesh->m_vertices.m_sources_vec3.size();
                        glm::vec2    tex = mesh->m_vertices.m_sources_vec2[t].m_data[indx[j]];
                        vd.tex_coords.push_back(tex);
                    }
                }

                found = FindSimilarVertex(vd, sub_poly, &founded_index);
                if(found)
                {
                    sub_poly.indexes.push_back(founded_index);
                }
                else
                {
                    PushVertexData(sub_poly, vd);
                    if(!mesh->m_vertices.m_wght.empty())   // for skinned mesh
                        sub_poly.weights.push_back(GetWeightsForVertex(mesh->m_vertices.m_wght[pos_index]));

                    unsigned int new_index = sub_poly.pos.size() - 1;
                    sub_poly.indexes.push_back(new_index);
                }
            }

            rep.meshes.push_back(std::move(sub_poly));
        }
    }

    if(!m_joints.empty())
    {
        rep.num_frames        = m_frame_count;
        rep.frame_rate        = m_frame_count / m_max_anim_time;
        uint32_t parent_check = 0;

        for(auto const & joint : m_joints)
        {
            InternalData::JointNode ex_joint;

            ex_joint.index        = joint->m_index;
            ex_joint.name         = joint->m_dae_node->_name;
            ex_joint.inverse_bind = joint->m_inv_bind_mat;

            if(joint->m_parent != nullptr)
            {
                JointNode const * parent = dynamic_cast<JointNode const *>(joint->m_parent);
                ex_joint.parent          = parent->m_index;
            }
            else
            {
                ex_joint.parent = 0;   // root joint

                ++parent_check;
                if(parent_check > 1)
                {
                    std::stringstream ss;
                    ss << "Error: Supported only one mesh with animation per file"
                       << "\n";

                    throw std::runtime_error(ss.str());
                }
            }

            if(m_frame_count > 0)
            {
                for(uint16_t i = 0; i < m_frame_count; i++)
                {
                    // relative skinning matrices
                    glm::mat4 rel = joint->m_r_frames[i];
                    glm::quat rot = glm::quat_cast(rel);
                    rot           = glm::normalize(rot);
                    ex_joint.r_rot.push_back(rot);

                    glm::vec4 transf = glm::column(rel, 3);
                    ex_joint.r_trans.push_back(glm::vec3(transf));

                    // absolute matrices
                    glm::mat4 abs = joint->m_a_frames[i] * joint->m_inv_bind_mat;
                    rot           = glm::quat_cast(abs);
                    rot           = glm::normalize(rot);
                    ex_joint.a_rot.push_back(rot);

                    transf = glm::column(abs, 3);
                    ex_joint.a_trans.push_back(glm::vec3(transf));
                }
            }

            rep.joints.push_back(std::move(ex_joint));

            // std::cout << glm::to_string(glm::transpose(joint->_inv_bind)) << std::endl;
            // std::cout << glm::to_string(glm::transpose(joint->_transf)) << std::endl << std::endl;
        }
    }

    rep.CalculateBBoxes();

    if(!m_parser._material->materials.empty())
    {
        for(auto const & mat : m_parser._material->materials)
        {
            if(!mat.used)
                continue;

            InternalData::Material material;
            material.name = mat.name;
            if(mat.effect != nullptr)
            {
                if(mat.effect->diffuseMap != nullptr)
                {
                    material.tex_name = mat.effect->diffuseMap->fileName;
                }
                else if(!mat.effect->diffuseColor.empty())
                {
                    std::istringstream s(mat.effect->diffuseColor);
                    float              r(0.0f), g(0.0f), b(0.0f), a(0.0f);
                    s >> r >> g >> b >> a;
                    material.diffusecolor = glm::vec4(r, g, b, a);
                }

                if(!mat.effect->specularColor.empty())
                {
                    std::istringstream s(mat.effect->specularColor);
                    float              r(0.0f), g(0.0f), b(0.0f), a(0.0f);
                    s >> r >> g >> b >> a;
                    material.specularcolor = glm::vec4(r, g, b, a);
                }

                material.shininess = mat.effect->shininess;
            }
            rep.materials.push_back(std::move(material));
        }
    }
}
