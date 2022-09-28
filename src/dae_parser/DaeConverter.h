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
            unsigned int _jointIndex;
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
    std::string                 _matId;
};

struct SceneNode
{
    bool      _joint;
    glm::mat4 _transf;   // absolute transform - for animation and skin export ONLY. Mesh export not affect.
    glm::mat4 _relTransf;   // relative transform -

    std::vector<glm::mat4> _frames;   // Relative transformation for every frame
    std::vector<glm::mat4> _aFrames;

    SceneNode *              _parent;
    std::vector<SceneNode *> _child;

    DaeNode const * _daeNode;

    SceneNode() : _joint(false), _parent(nullptr), _daeNode(nullptr)
    {
        _transf    = glm::mat4(1.0f);
        _relTransf = glm::mat4(1.0f);
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
    glm::mat4    _daeInvBindMat;
    glm::mat4    _invBindMat;   // inverse bind matrix

    JointNode() : _index(0)
    {
        _joint         = true;
        _invBindMat    = glm::mat4(1.0f);
        _daeInvBindMat = glm::mat4(1.0f);
    }
};

class DaeConverter : public Converter
{
    DaeParser const & _parser;
    uint32_t          _frameCount;
    float             _maxAnimTime;

    std::vector<std::unique_ptr<MeshNode>>  _meshes;
    std::vector<std::unique_ptr<JointNode>> _joints;

protected:
    void         ConvertScene(DaeVisualScene const & sc);
    SceneNode *  ProcessNode(DaeNode const & node, SceneNode * parent, glm::mat4 transAccum,
                             DaeVisualScene const & sc, std::vector<glm::mat4> animTransAccum);
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
