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
#include <iostream>
#include <sstream>
#include <stdexcept>

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
DaeConverter::DaeConverter(DaeParser const & parser) : _parser(parser), _frameCount(0), _maxAnimTime(0.0f) {}

void DaeConverter::Convert()
{
    if(_parser._v_scenes->_scenes.empty())
    {
        std::stringstream ss;
        ss << "Error: No scenes to export"
           << "\n";

        throw std::runtime_error(ss.str());
    }

    for(auto & sc : _parser._v_scenes->_scenes)
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

    if(!_parser._anim->_animations.empty())
    {
        _frameCount  = _parser._anim->_maxFrameCount;
        _maxAnimTime = _parser._anim->_maxAnimTime;
    }

    glm::mat4              up_mat = GetConvertMatrix(_parser._up_axis);
    std::vector<glm::mat4> animTransAccum;
    for(unsigned int i = 0; i < _frameCount; ++i)
        animTransAccum.push_back(glm::mat4(1.0f));

    for(size_t i = 0; i < sc._nodes.size(); i++)
    {
        ProcessNode(sc._nodes[i], nullptr, up_mat, sc, animTransAccum);
    }

    if(!_joints.empty())
        ProcessJoints();

    if(!_meshes.empty())
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

    // glm::mat4 up_mat = GetConvertMatrix(_parser._up_axis);
    glm::mat4 rel_mat = CreateTransformMatrix(node._transStack);
    // up_mat * CreateTransformMatrix(node._transStack) * glm::transpose(up_mat) * trans_accum;

    SceneNode * sceneNode = nullptr;

    if(node._joint)
    {
        auto jnt      = std::make_unique<JointNode>();
        jnt->_daeNode = &node;
        jnt->_parent  = parent;

        sceneNode = jnt.get();
        _joints.push_back(std::move(jnt));
    }
    else
    {
        if(!node._instances.empty())
        {
            auto mesh      = std::make_unique<MeshNode>();
            mesh->_daeNode = &node;
            mesh->_parent  = parent;

            sceneNode = mesh.get();
            _meshes.push_back(std::move(mesh));
        }
    }

    if(sceneNode != nullptr)
    {
        sceneNode->_relTransf = rel_mat;

        if(parent != nullptr)
            sceneNode->_transf = sceneNode->_relTransf * parent->_transf;
        else
            sceneNode->_transf = sceneNode->_relTransf * trans_accum;

        trans_accum = glm::mat4(1.0f);
    }
    else
    {
        trans_accum = rel_mat * trans_accum;
    }

    // Animation
    for(uint32_t i = 0; i < _frameCount; ++i)
    {
        glm::mat4 mat = GetNodeTransform(node, sceneNode, i) * anim_trans_accum[i];
        if(sceneNode != nullptr)
        {
            sceneNode->_frames.push_back(mat);
            anim_trans_accum[i] = glm::mat4(1.0);
        }
        else
            anim_trans_accum[i] = mat * anim_trans_accum[i];   // Pure transformation node
    }

    for(auto & chd : node._children)
    {
        SceneNode * parNode = sceneNode != nullptr ? sceneNode : parent;
        SceneNode * snd     = ProcessNode(*chd, parNode, trans_accum, sc, anim_trans_accum);
        if(snd != nullptr && parNode != nullptr)
            parNode->_child.push_back(snd);
    }

    return sceneNode;
}

glm::mat4 DaeConverter::GetNodeTransform(DaeNode const & node, SceneNode const * scene_node,
                                         uint32_t frame) const
{
    int       tr_index = -1;
    glm::mat4 tr       = glm::mat4(1.0);
    // glm::mat4    up_mat   = GetConvertMatrix(_parser._up_axis);
    DaeSampler * sampler = _parser._anim->FindAnimForTarget(node._id, &tr_index);

    if(sampler != nullptr)
    {
        if(sampler->_output->_floatArray.size() == _frameCount * 16)
        {
            tr = CreateDAEMatrix(&sampler->_output->_floatArray[frame * 16]);
            // up_mat * CreateDAEMatrix(&sampler->_output->_floatArray[frame * 16]) * glm::transpose(up_mat);
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
            tr = scene_node->_transf;
        else
            tr = CreateTransformMatrix(node._transStack);
    }

    return tr;
}

void CalcJointFrame(JointNode * nd, JointNode * parent)
{
    for(uint32_t i = 0; i < nd->_frames.size(); i++)
    {
        glm::mat4 parTr = glm::mat4(1.0);
        if(parent != nullptr)
            parTr = parent->_aFrames[i];
        nd->_aFrames.push_back(nd->_frames[i] * parTr);
    }

    for(uint32_t i = 0; i < nd->_child.size(); i++)
    {
        auto chd = dynamic_cast<JointNode *>(nd->_child[i]);
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
    auto it = std::find_if(_joints.begin(), _joints.end(), [](std::unique_ptr<JointNode> const & nd) -> bool {
        return nd->_parent == nullptr;
    });
    if(it != _joints.end())
    {
        JointNode * rootJoint = (*it).get();
        CalcJointFrame(rootJoint, nullptr);
    }
}

void DaeConverter::ProcessJoints()
{
    unsigned int i = 1;
    for(auto & jnt : _joints)
    {
        jnt->_index = i;
        i++;
    }

    if(_frameCount > 0)
        CalcAbsTransfMatrices();
}

VertexData::Semantic ConverSemantic(DaeGeometry::Semantic sem)
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

    auto it = std::find_if(_joints.begin(), _joints.end(),
                           [&name](auto & nd) -> bool { return nd->_daeNode->_sid == name; });
    if(it != _joints.end())
    {
        indx = (*it)->_index;
    }

    return indx;
}

void DaeConverter::ProcessMeshes()
{
    for(auto & msh : _meshes)
    {
        glm::mat4 trans = msh->_transf;   // CreateTransformMatrix(msh->_daeNode->_transStack);
        // trans           = GetConvertMatrix(_parser._up_axis) * trans;

        MeshNode *  cur_mesh = msh.get();
        DaeSkin *   skn      = nullptr;
        std::string geo_src;

        for(size_t ins = 0; ins < cur_mesh->_daeNode->_instances.size(); ins++)
        {
            if(!_parser._controllers->_skinControllers.empty())
            {
                std::string skin_id = cur_mesh->_daeNode->_instances[ins]._url;
                auto        it =
                    std::find_if(_parser._controllers->_skinControllers.begin(),
                                 _parser._controllers->_skinControllers.end(),
                                 [&skin_id](DaeSkin const & skn) -> bool { return skn._id == skin_id; });

                if(it != _parser._controllers->_skinControllers.end())
                {
                    skn     = &(*it);
                    geo_src = skn->_ownerId;
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
                geo_src = cur_mesh->_daeNode->_instances[ins]._url;
            }

            // Check that skin has all required arrays
            if(skn != nullptr)
            {
                if(skn->_jointArray == nullptr || skn->_bindMatArray == nullptr
                   || skn->_weightArray == nullptr)
                {
                    std::cout << "Skin controller '" << skn->_id
                              << "' is missing information and is ignored\n";
                    skn = nullptr;
                }
            }

            DaeMeshNode const *      geometry = _parser._geom->Find(geo_src);
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
                for(unsigned int j = 0; j < skn->_jointArray->_stringArray.size(); ++j)
                {
                    std::string sid = skn->_jointArray->_stringArray[j];

                    joint_lookup.push_back(nullptr);

                    bool found = false;
                    for(auto & jnt : _joints)
                    {
                        if(jnt->_daeNode->_sid == sid)
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
                glm::mat4 up_mat = GetConvertMatrix(_parser._up_axis);
                for(unsigned int j = 0; j < skn->_jointArray->_stringArray.size(); ++j)
                {
                    if(joint_lookup[j] != nullptr)
                    {
                        joint_lookup[j]->_daeInvBindMat =
                            CreateDAEMatrix(&skn->_bindMatArray->_floatArray[j * 16]);

                        // Multiply bind matrices with up matrix
                        joint_lookup[j]->_invBindMat =
                            joint_lookup[j]->_daeInvBindMat * glm::transpose(up_mat);
                    }
                }
            }

            // copy vertex data
            for(auto & attr : geometry->_triGroups[0]._meshes[0]._attributes)
            {
                if(attr.posSourceIt != geometry->_posSources.end())
                {
                    // copy vertex
                    VertexData::ChunkVec3 pos;
                    pos._semantic = VertexData::Semantic::POSITION;

                    auto     pos_src    = (*(*attr.posSourceIt)._posSourceIt);
                    uint32_t num_vertex = pos_src._floatArray.size() / pos_src._paramsPerItem;

                    for(uint32_t i = 0; i < num_vertex; ++i)
                    {
                        glm::vec3 vertex(0.0f);

                        vertex.x = pos_src._floatArray[i * pos_src._paramsPerItem + 0];
                        vertex.y = pos_src._floatArray[i * pos_src._paramsPerItem + 1];
                        vertex.z = pos_src._floatArray[i * pos_src._paramsPerItem + 2];

                        pos._data.push_back(vertex);
                    }

                    cur_mesh->_vertices._sources_vec3.push_back(std::move(pos));

                    if(skn != nullptr)
                    {
                        // copy weights
                        for(uint32_t i = 0; i < num_vertex; i++)
                        {
                            VertexData::WeightsVec vert_weight_vect;

                            for(auto skn_w : skn->_vertWeights[i])
                            {
                                VertexData::WeightsVec::Weight tw{0, 1.0f};

                                tw._jointIndex = FindJointIndex(skn->_jointArray->_stringArray[skn_w._joint]);
                                tw._w          = skn->_weightArray->_floatArray[skn_w._weight];

                                vert_weight_vect._weights.push_back(tw);
                            }

                            cur_mesh->_vertices._wght.push_back(vert_weight_vect);
                        }
                    }
                }
                else
                {
                    auto src = *attr.sourceIt;

                    if(src._paramsPerItem == 2)
                    {
                        VertexData::ChunkVec2 chunk;
                        chunk._semantic     = ConverSemantic(attr.semantic);
                        uint32_t num_vertex = src._floatArray.size() / 2;

                        for(uint32_t i = 0; i < num_vertex; ++i)
                        {
                            glm::vec2 vertex(0.0f);

                            vertex.x = src._floatArray[i * 2 + 0];
                            vertex.y = src._floatArray[i * 2 + 1];

                            chunk._data.push_back(vertex);
                        }

                        cur_mesh->_vertices._sources_vec2.push_back(std::move(chunk));
                    }
                    else
                    {
                        VertexData::ChunkVec3 chunk;
                        chunk._semantic     = ConverSemantic(attr.semantic);
                        uint32_t num_vertex = src._floatArray.size() / 3;

                        for(uint32_t i = 0; i < num_vertex; ++i)
                        {
                            glm::vec3 vertex(0.0f);

                            vertex.x = src._floatArray[i * 3 + 0];
                            vertex.y = src._floatArray[i * 3 + 1];
                            vertex.z = src._floatArray[i * 3 + 2];

                            chunk._data.push_back(vertex);
                        }

                        cur_mesh->_vertices._sources_vec3.push_back(std::move(chunk));
                    }
                }
            }

            // copy index
            for(auto & mesh : geometry->_triGroups)
            {
                auto & inp = geometry->_triGroups[0]._meshes[0]._attributes;
                for(auto & poly : mesh._meshes)
                {
                    assert(poly._triangles[0].size() == inp.size());

                    TriGroup      tg;
                    DaeMaterial * mat = _parser._material->FindMaterial(poly._material_name);
                    if(mat != nullptr)
                    {
                        tg._matId = mat->name;
                        mat->used = true;
                    }
                    else
                    {
                        std::cout << "Warning: Material '" << poly._material_name << "' not found"
                                  << std::endl;
                        tg._matId = poly._material_name;
                    }

                    // copy index
                    for(auto & tri : poly._triangles)
                    {
                        TriGroup::VertexAttribut index;

                        for(unsigned int i = 0; i < tri.size(); ++i)
                        {
                            index.push_back(tri[i]);
                        }

                        tg._indices.push_back(std::move(index));
                    }
                    cur_mesh->_polylists.push_back(std::move(tg));
                }
            }

            // Apply bind_shape matrix
            if(skn != nullptr)
            {
                trans = trans * skn->_bindShapeMat;
            }

            for(auto & vec3_array : cur_mesh->_vertices._sources_vec3)
            {
                std::for_each(vec3_array._data.begin(), vec3_array._data.end(),
                              [&trans, sem = vec3_array._semantic](glm::vec3 & vt) {
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

                                  vt = glm::vec3(tmp.x, tmp.y, tmp.z);
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

    for(auto const & w : wvec._weights)
    {
        temp_weight.jointIndex = w._jointIndex;
        temp_weight.w          = w._w;
        wv.push_back(temp_weight);
    }
    return wv;
}

void DaeConverter::ExportToInternal(InternalData & rep, CmdLineOptions const & cmd) const
{
    assert(rep.meshes.empty());

    if(_meshes.empty())
    {
        std::cout << "WARNING! Nothing to export, no gometry data\n";
    }

    for(auto & mesh : _meshes)
    {
        for(auto & poly : mesh->_polylists)
        {
            InternalData::SubMesh sub_poly;
            sub_poly.material = poly._matId;

            for(auto & indx : poly._indices)
            {
                CurrVertexData vd;
                bool           found;
                unsigned int   founded_index = 0;
                unsigned int   pos_index     = 0;

                for(unsigned int j = 0; j < indx.size(); ++j)
                {
                    if(j < mesh->_vertices._sources_vec3.size())
                    {
                        switch(mesh->_vertices._sources_vec3[j]._semantic)
                        {
                            case VertexData::Semantic::POSITION:
                                {
                                    vd.pos    = mesh->_vertices._sources_vec3[j]._data[indx[j]];
                                    pos_index = indx[j];
                                    break;
                                }
                            case VertexData::Semantic::NORMAL:
                                {
                                    vd.is_normal = true;
                                    vd.normal    = mesh->_vertices._sources_vec3[j]._data[indx[j]];
                                    break;
                                }
                            case VertexData::Semantic::COLOR:
                                {
                                    vd.is_color = true;
                                    vd.color    = mesh->_vertices._sources_vec3[j]._data[indx[j]];
                                    break;
                                }
                            case VertexData::Semantic::TANGENT:
                            case VertexData::Semantic::TEXTANGENT:
                                {
                                    vd.is_tangent = true;
                                    vd.tangent    = mesh->_vertices._sources_vec3[j]._data[indx[j]];
                                    break;
                                }
                            case VertexData::Semantic::BINORMAL:
                            case VertexData::Semantic::TEXBINORMAL:
                                {
                                    vd.is_bitangent = true;
                                    vd.bitangent    = mesh->_vertices._sources_vec3[j]._data[indx[j]];
                                    break;
                                }
                        }
                    }
                    else
                    {
                        unsigned int t   = j - mesh->_vertices._sources_vec3.size();
                        glm::vec2    tex = mesh->_vertices._sources_vec2[t]._data[indx[j]];
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
                    if(!mesh->_vertices._wght.empty())   // for skinned mesh
                        sub_poly.weights.push_back(GetWeightsForVertex(mesh->_vertices._wght[pos_index]));

                    unsigned int new_index = sub_poly.pos.size() - 1;
                    sub_poly.indexes.push_back(new_index);
                }
            }

            rep.meshes.push_back(std::move(sub_poly));
        }
    }

    if(!_joints.empty())
    {
        rep.numFrames             = _frameCount;
        rep.frameRate             = _frameCount / _maxAnimTime;
        unsigned int parent_check = 0;

        for(auto & joint : _joints)
        {
            InternalData::JointNode ex_joint;

            ex_joint.index        = joint->_index;
            ex_joint.name         = joint->_daeNode->_name;
            ex_joint.inverse_bind = joint->_invBindMat;

            if(joint->_parent != nullptr)
            {
                JointNode * parent = dynamic_cast<JointNode *>(joint->_parent);
                ex_joint.parent    = parent->_index;
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

            if(_frameCount > 0)
            {
                for(uint16_t i = 0; i < _frameCount; i++)
                {
                    // relative matrices
                    glm::quat rot = glm::quat_cast(joint->_frames[i]);
                    rot           = glm::normalize(rot);
                    ex_joint.r_rot.push_back(rot);

                    glm::vec4 transf = glm::column(joint->_frames[i], 3);
                    ex_joint.r_trans.push_back(glm::vec3(transf));

                    // absolute matrices
                    glm::mat4 jointTransf = joint->_aFrames[i] * ex_joint.inverse_bind;
                    rot                   = glm::quat_cast(jointTransf);
                    rot                   = glm::normalize(rot);
                    ex_joint.a_rot.push_back(rot);

                    transf = glm::column(jointTransf, 3);
                    ex_joint.a_trans.push_back(glm::vec3(transf));
                }
            }

            rep.joints.push_back(std::move(ex_joint));
        }
    }

    rep.CalculateBBoxes();

    if(!_parser._material->materials.empty())
    {
        for(auto & mat : _parser._material->materials)
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
