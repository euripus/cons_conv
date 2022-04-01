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
class _daeNode;
glm::mat4 CreateTransformMatrix(const std::vector<DaeTransformation> & transStack)
{
    glm::mat4 mat(1.0), daeMat(1.0);
    glm::mat4 scale(1.0), rotate(1.0), trans(1.0);

    for(auto & tr : transStack)
    {
        switch(tr._type)
        {
            case DaeTransformation::Type::SCALE: {
                scale = glm::scale(scale, glm::vec3(tr._values[0], tr._values[1], tr._values[2]));
                break;
            }
            case DaeTransformation::Type::ROTATION: {
                rotate = glm::rotate(rotate, tr._values[3],
                                     glm::vec3(tr._values[0], tr._values[1], tr._values[2]));
                break;
            }
            case DaeTransformation::Type::TRANSLATION: {
                trans = glm::translate(trans, glm::vec3(tr._values[0], tr._values[1], tr._values[2]));
                break;
            }
            case DaeTransformation::Type::MATRIX: {
                daeMat = CreateDAEMatrix(tr._values);
                break;
            }
        }
    }

    mat = daeMat * trans * rotate * scale;   /// MATRIX transform order
    return mat;
}

/******************************************************************************
 *
 ******************************************************************************/
DaeConverter::DaeConverter(const DaeParser & parser) : _parser(parser), _frameCount(0), _maxAnimTime(0.0f) {}

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

void DaeConverter::ConvertScene(const DaeVisualScene & sc)
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

    std::vector<glm::mat4> animTransAccum;
    for(unsigned int i = 0; i < _frameCount; ++i)
        animTransAccum.push_back(glm::mat4(1.0f));

    for(size_t i = 0; i < sc._nodes.size(); i++)
    {
        ProcessNode(sc._nodes[i], nullptr, glm::mat4(1.0f), sc, animTransAccum);
    }

    if(!_joints.empty())
        ProcessJoints();

    if(!_meshes.empty())
        ProcessMeshes();
}

SceneNode * DaeConverter::ProcessNode(const DaeNode & node, SceneNode * parent, glm::mat4 transAccum,
                                      const DaeVisualScene & sc, std::vector<glm::mat4> animTransAccum)
{
    // Note: animTransAccum is used for pure transformation nodes of Collada that are no joints or meshes

    if(node._reference)
    {
        const DaeNode * nd = sc.FindNode(node._name);
        if(nd)
            return ProcessNode(*nd, parent, transAccum, sc, animTransAccum);
        else
        {
            std::cout << "Warning: undefined reference '" + node._name + "' in instance_node" << std::endl;
            return nullptr;
        }
    }

    glm::mat4 upMat  = GetConvertMatrix(_parser._up_axis);
    glm::mat4 relMat = transAccum * upMat * CreateTransformMatrix(node._transStack) * glm::transpose(upMat);

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
        sceneNode->_relTransf = relMat;

        if(parent != nullptr)
            sceneNode->_transf = parent->_transf * sceneNode->_relTransf;
        else
            sceneNode->_transf = sceneNode->_relTransf;

        transAccum = glm::mat4(1.0);
    }
    else
    {
        transAccum = relMat;
    }

    // Animation
    for(uint32_t i = 0; i < _frameCount; ++i)
    {
        glm::mat4 mat = animTransAccum[i] * GetNodeTransform(node, i);
        if(sceneNode != nullptr)
        {
            sceneNode->_frames.push_back(mat);
            animTransAccum[i] = glm::mat4(1.0);
        }
        else
            animTransAccum[i] = mat;   // Pure transformation node
    }

    for(auto & chd : node._children)
    {
        SceneNode * parNode = sceneNode != nullptr ? sceneNode : parent;
        SceneNode * snd     = ProcessNode(*chd, parNode, transAccum, sc, animTransAccum);
        if(snd != nullptr && parNode != nullptr)
            parNode->_child.push_back(snd);
    }

    return sceneNode;
}

glm::mat4 DaeConverter::GetNodeTransform(const DaeNode & node, uint32_t frame) const
{
    int          trIndex = -1;
    glm::mat4    tr      = glm::mat4(1.0);
    glm::mat4    upMat   = GetConvertMatrix(_parser._up_axis);
    DaeSampler * sampler = _parser._anim->FindAnimForTarget(node._id, &trIndex);

    if(sampler != nullptr)
    {
        if(sampler->_output->_floatArray.size() == _frameCount * 16)
        {
            tr = upMat * CreateDAEMatrix(&sampler->_output->_floatArray[frame * 16]) * glm::transpose(upMat);
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
        tr = upMat * CreateTransformMatrix(node._transStack) * glm::transpose(upMat);
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
        nd->_aFrames.push_back(parTr * nd->_frames[i]);
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
    auto it = std::find_if(_joints.begin(), _joints.end(), [](const std::unique_ptr<JointNode> & nd) -> bool {
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
        case DaeGeometry::Semantic::VERTEX: {
            s = VertexData::Semantic::POSITION;
            break;
        }
        case DaeGeometry::Semantic::BINORMAL: {
            s = VertexData::Semantic::BINORMAL;
            break;
        }
        case DaeGeometry::Semantic::COLOR: {
            s = VertexData::Semantic::COLOR;
            break;
        }
        case DaeGeometry::Semantic::NORMAL: {
            s = VertexData::Semantic::NORMAL;
            break;
        }
        case DaeGeometry::Semantic::TANGENT: {
            s = VertexData::Semantic::TANGENT;
            break;
        }
        case DaeGeometry::Semantic::TEXBINORMAL: {
            s = VertexData::Semantic::TEXBINORMAL;
            break;
        }
        case DaeGeometry::Semantic::TEXCOORD: {
            s = VertexData::Semantic::TEXCOORD;
            break;
        }
        case DaeGeometry::Semantic::TEXTANGENT: {
            s = VertexData::Semantic::TEXTANGENT;
            break;
        }
    }
    return s;
}

unsigned int DaeConverter::FindJointIndex(const std::string & name) const
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
        glm::mat4 trans = CreateTransformMatrix(msh->_daeNode->_transStack);
        trans           = GetConvertMatrix(_parser._up_axis) * trans;

        MeshNode *  curMesh = msh.get();
        DaeSkin *   skn     = nullptr;
        std::string geoSrc;

        for(size_t ins = 0; ins < curMesh->_daeNode->_instances.size(); ins++)
        {
            if(!_parser._controllers->_skinControllers.empty())
            {
                std::string skinId = curMesh->_daeNode->_instances[ins]._url;
                auto        it     = std::find_if(_parser._controllers->_skinControllers.begin(),
                                       _parser._controllers->_skinControllers.end(),
                                       [&skinId](const DaeSkin & skn) -> bool { return skn._id == skinId; });

                if(it != _parser._controllers->_skinControllers.end())
                {
                    skn    = &(*it);
                    geoSrc = skn->_ownerId;
                }
                else
                {
                    std::stringstream ss;
                    ss << "ERROR! Controller node not found. Skin id: " << skinId << "\n";

                    throw std::runtime_error(ss.str());
                }
            }
            else
            {
                geoSrc = curMesh->_daeNode->_instances[ins]._url;
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

            const DaeMeshNode *      geometry = _parser._geom->Find(geoSrc);
            std::vector<JointNode *> jointLookup;

            if(!geometry)
            {
                std::stringstream ss;
                ss << "ERROR! Geometry data not found. Geometry name: " << geoSrc << "\n";

                throw std::runtime_error(ss.str());
            }

            if(skn != nullptr)
            {
                // Build lookup table
                for(unsigned int j = 0; j < skn->_jointArray->_stringArray.size(); ++j)
                {
                    std::string sid = skn->_jointArray->_stringArray[j];

                    jointLookup.push_back(nullptr);

                    bool found = false;
                    for(auto & jnt : _joints)
                    {
                        if(jnt->_daeNode->_sid == sid)
                        {
                            jointLookup[j] = jnt.get();
                            found          = true;
                            break;
                        }
                    }

                    if(!found)
                    {
                        std::cout << "Warning: joint '" << sid << "' used in skin controller not found\n";
                    }
                }

                // Find bind matrices
                glm::mat4 upMat        = GetConvertMatrix(_parser._up_axis);
                glm::mat4 bindShapeMat = upMat * skn->_bindShapeMat * glm::transpose(upMat);
                for(unsigned int j = 0; j < skn->_jointArray->_stringArray.size(); ++j)
                {
                    if(jointLookup[j] != nullptr)
                    {
                        jointLookup[j]->_daeInvBindMat =
                            upMat * CreateDAEMatrix(&skn->_bindMatArray->_floatArray[j * 16])
                            * glm::transpose(upMat);

                        // Multiply bind matrices with bind shape matrix
                        jointLookup[j]->_invBindMat = jointLookup[j]->_daeInvBindMat * bindShapeMat;
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

                    curMesh->_vertices._sources_vec3.push_back(std::move(pos));

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

                            curMesh->_vertices._wght.push_back(vert_weight_vect);
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

                        curMesh->_vertices._sources_vec2.push_back(std::move(chunk));
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

                        curMesh->_vertices._sources_vec3.push_back(std::move(chunk));
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
                    curMesh->_polylists.push_back(std::move(tg));
                }
            }

            // Apply transform matrix
            // if( skn != nullptr )
            //{
            // trans = trans * skn->_bindShapeMat;
            //}
            for(auto & vec3_array : curMesh->_vertices._sources_vec3)
            {
                std::for_each(vec3_array._data.begin(), vec3_array._data.end(), [&trans](glm::vec3 & vt) {
                    glm::vec4 tmp(vt, 1.0);
                    tmp = trans * tmp;
                    if(tmp.w != 1.0f)
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
    std::vector<glm::vec2> tex_coords   = {};   // Required
};

void PushVertexData(InternalData::SubMesh & msh, const CurrVertexData & vd)
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
		msh.tex_coords.clean();
		msh.tex_coords.resize(vd.tex_coords.size());
		//for(unsigned int i = 0; i < vd.tex_coords.size(); ++i)
		//	msh.tex_coords.push_back({});
	}	
	for(unsigned int i = 0; i < vd.tex_coords.size(); ++i)
			msh.tex_coords[i].push_back(vd.tex_coords[i]);
}

bool FindSimilarVertex(const CurrVertexData & vd, const InternalData::SubMesh & msh, unsigned int * foundInd)
{
    for(unsigned int i = 0; i < msh.pos.size(); i++)
    {
        bool isEqual = false;

        isEqual = VecEqual(vd.pos, msh.pos[i]);
        if(vd.is_normal)
            isEqual = isEqual && VecEqual(vd.normal, msh.normal[i]);
        if(vd.is_tangent)
            isEqual = isEqual && VecEqual(vd.tangent, msh.tangent[i]);
        if(vd.is_bitangent)
            isEqual = isEqual && VecEqual(vd.bitangent, msh.bitangent[i]);
        if(vd.is_color)
            isEqual = isEqual && VecEqual(vd.color, msh.color[i]);

        for(unsigned int j = 0; j < vd.tex_coords.size(); j++)
        {
            isEqual = isEqual && VecEqual(vd.tex_coords[j], msh.tex_coords[i][j]);
        }

        if(isEqual)
        {
            *foundInd = i;
            return true;
        }
    }

    return false;
}

InternalData::WeightsVec GetWeightsForVertex(VertexData::WeightsVec wvec)
{
    InternalData::WeightsVec wv;
    InternalData::Weight     temp_weight{0, 0.0f};

    for(auto w : wvec._weights)
    {
        temp_weight.jointIndex = w._jointIndex;
        temp_weight.w          = w._w;
        wv.push_back(temp_weight);
    }
    return wv;
}

void DaeConverter::ExportToInternal(InternalData & rep, const CmdLineOptions & cmd) const
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
                            case VertexData::Semantic::POSITION: {
                                vd.pos    = mesh->_vertices._sources_vec3[j]._data[indx[j]];
                                pos_index = indx[j];
                                break;
                            }
                            case VertexData::Semantic::NORMAL: {
                                vd.is_normal = true;
                                vd.normal    = mesh->_vertices._sources_vec3[j]._data[indx[j]];
                                break;
                            }
                            case VertexData::Semantic::COLOR: {
                                vd.is_color = true;
                                vd.color    = mesh->_vertices._sources_vec3[j]._data[indx[j]];
                                break;
                            }
                            case VertexData::Semantic::TANGENT:
                            case VertexData::Semantic::TEXTANGENT: {
                                vd.is_tangent = true;
                                vd.tangent    = mesh->_vertices._sources_vec3[j]._data[indx[j]];
                                break;
                            }
                            case VertexData::Semantic::BINORMAL:
                            case VertexData::Semantic::TEXBINORMAL: {
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
            InternalData::JointNode exJoint;

            exJoint.index = joint->_index;
            exJoint.name  = joint->_daeNode->_name;

            if(joint->_parent != nullptr)
            {
                JointNode * parent = dynamic_cast<JointNode *>(joint->_parent);
                exJoint.parent     = parent->_index;
            }
            else
            {
                exJoint.parent = 0;   // root joint

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
                    glm::mat4 jointTransf = joint->_aFrames[i] * joint->_invBindMat;
                    glm::quat rot         = glm::quat_cast(jointTransf);
                    rot                   = glm::normalize(rot);
                    exJoint.rot.push_back(rot);

                    glm::vec4 transf = glm::column(jointTransf, 3);
                    exJoint.trans.push_back(glm::vec3(transf));
                }
            }

            rep.joints.push_back(std::move(exJoint));
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
