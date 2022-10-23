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
        Semantic       m_semantic;
        std::vector<T> m_data;
    };

    using ChunkVec3 = Chunk<glm::vec3>;
    using ChunkVec2 = Chunk<glm::vec2>;
    struct WeightsVec
    {
        struct Weight
        {
            unsigned int m_joint_index;
            float        m_w;
        };

        std::vector<Weight> m_weights;
    };

    std::vector<ChunkVec2>  m_sources_vec2;
    std::vector<ChunkVec3>  m_sources_vec3;
    std::vector<WeightsVec> m_wght;
};

struct TriGroup
{
    using VertexAttribut = std::vector<unsigned int>;

    std::vector<VertexAttribut> m_indices;   // _indices.size() == _sources_vec3.size() + _sources_vec2.size()
    std::string                 m_mat_id;
};

struct SceneNode
{
    bool m_joint;

    glm::mat4 m_abs_transf;   // absolute transform
    glm::mat4 m_rel_transf;   // relative transform

    std::vector<glm::mat4> m_r_frames;   // relative transformation for every frame
    std::vector<glm::mat4> m_a_frames;   // absolute transformation for every frame

    SceneNode *              m_parent;
    std::vector<SceneNode *> m_child;

    DaeNode const * m_dae_node;

    SceneNode() : m_joint(false), m_parent(nullptr), m_dae_node(nullptr)
    {
        m_abs_transf = glm::mat4(1.0f);
        m_rel_transf = glm::mat4(1.0f);
    }

    virtual ~SceneNode() = default;
};

struct MeshNode : public SceneNode
{
    VertexData            m_vertices;
    std::vector<TriGroup> m_polylists;
};

struct JointNode : public SceneNode
{
    unsigned int m_index;
    glm::mat4    m_inv_bind_mat;   // inverse bind matrix

    JointNode() : m_index(0)
    {
        m_joint        = true;
        m_inv_bind_mat = glm::mat4(1.0f);
    }
};

class DaeConverter : public Converter
{
    DaeParser const & m_parser;
    uint32_t          m_frame_count;
    float             m_max_anim_time;

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
