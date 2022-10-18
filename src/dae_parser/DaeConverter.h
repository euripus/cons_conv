#ifndef DAECONVERTER_H
#define DAECONVERTER_H

#include "../Converter.h"
#include "DaeParser.h"
#include <glm/glm.hpp>
#include <vector>

class DaeNode;
class DaeSkin;
class DaeVisualScene;

struct VertexData
{
    enum class Semantic
    {
        UNKNOWN,
        BINORMAL,
        COLOR,
        NORMAL,
        TANGENT,
        TEXBINORMAL,
        TEXCOORD,
        TEXTANGENT,
        POSITION
    };

    template<typename T>
    struct Chunk
    {
        Semantic       _semantic;
        std::vector<T> _data;
    };

    using ChunkVec3 = Chunk<glm::vec3>;
    using ChunkVec2 = Chunk<glm::vec2>;
    struct WeightsVec
    {
        struct Weight
        {
            unsigned int _joint_index;
            float        _w;
        };

        std::vector<Weight> _weights;
    };

    std::vector<ChunkVec2>  _sources_vec2;
    std::vector<ChunkVec3>  _sources_vec3;
    std::vector<WeightsVec> _wght;
};

struct TriGroup
{
    using VertexAttribut = std::vector<unsigned int>;

    std::vector<VertexAttribut> _indices;   // _indices.size() == _sources_vec3.size() + _sources_vec2.size()
    std::string                 _mat_id;
};

struct SceneNode
{
    bool _joint;
    bool _is_joint_root;

    glm::mat4 _transf;       // absolute transform
    glm::mat4 _rel_transf;   // relative transform

    std::vector<glm::mat4> _r_frames;   // relative transformation for every frame
    std::vector<glm::mat4> _a_frames;   // absolute transformation for every frame

    SceneNode *              _parent;
    std::vector<SceneNode *> _child;

    DaeNode const * _dae_node;

    SceneNode() : _joint(false), _is_joint_root(false), _parent(nullptr), _dae_node(nullptr)
    {
        _transf     = glm::mat4(1.0f);
        _rel_transf = glm::mat4(1.0f);
    }

    virtual ~SceneNode() = default;
};

struct MeshNode : public SceneNode
{
    VertexData            _vertices;
    std::vector<TriGroup> _polylists;
};

struct JointNode : public SceneNode
{
    unsigned int _index;
    glm::mat4    _inv_bind_mat;   // inverse bind matrix

    JointNode() : _index(0)
    {
        _joint        = true;
        _inv_bind_mat = glm::mat4(1.0f);
    }
};

class DaeConverter : public Converter
{
    DaeParser const & m_parser;
    uint32_t          m_frame_count;
    float             m_max_anim_time;
    bool              m_first_joint_enter;

    std::vector<std::unique_ptr<MeshNode>>  m_meshes;
    std::vector<std::unique_ptr<JointNode>> m_joints;

protected:
    void         ConvertScene(DaeVisualScene const & sc);
    SceneNode *  ProcessNode(DaeNode const & node, SceneNode * parent, glm::mat4 trans_accum,
                             DaeVisualScene const & sc, std::vector<glm::mat4> anim_trans_accum);
    glm::mat4    GetNodeTransform(DaeNode const & node, SceneNode const * scene_node, uint32_t frame) const;
    void         CalcAbsTransfMatrices();
    unsigned int FindJointIndex(std::string const & name) const;

    void ProcessMeshes();
    void ProcessJoints();

public:
    DaeConverter(DaeParser const & parser);
    virtual ~DaeConverter() override = default;

    void Convert() override;
    void ExportToInternal(InternalData & rep, CmdLineOptions const & cmd) const override;
};

#endif   // DAECONVERTER_H
