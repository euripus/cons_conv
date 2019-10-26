#ifndef INTERNALREP_H
#define INTERNALREP_H

#include "AABB.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

// Internal representation of parsed data
// Default internal coordinate system(OpenGL):
//      Y_UP: Right(Positive x)  Up(Positive y) In(Positive z)
// Required data: position, texture coord channel

struct InternalData
{
    struct Weight
    {
        uint32_t jointIndex;
        float    w;
    };
    using WeightsVec = std::vector<Weight>;

    struct JointNode
    {
        uint32_t    parent;
        uint32_t    index;
        std::string name;

        std::vector<glm::quat> rot;     // absolute transform matrix for animation
        std::vector<glm::vec3> trans;   // vec.size() == numFrames
    };

    struct SubMesh
    {
        std::vector<glm::vec3>              pos;
        std::vector<WeightsVec>             weights;
        std::vector<glm::vec3>              normal;
        std::vector<glm::vec3>              tangent;
        std::vector<glm::vec3>              bitangent;
        std::vector<glm::vec3>              color;
        std::vector<std::vector<glm::vec2>> tex_coords;
        AABB                                bbox;

        std::string           material;
        std::vector<uint32_t> indexes;
    };

    struct Material
    {
        std::string name;
        float       shininess;
        glm::vec4   specularcolor;
        glm::vec4   diffusecolor;
        std::string tex_name;
    };

    static const int maxCacheSize = 16;

    std::vector<JointNode> joints;
    std::vector<AABB>      bboxes;
    uint32_t               numFrames;
    float                  frameRate;

    std::vector<SubMesh>  meshes;
    std::vector<Material> materials;

    InternalData();
    virtual ~InternalData() = default;

    // Optimizations
    unsigned int RemoveDegeneratedTriangles();
    void         OptimizeIndexOrder();
    float        CalcCacheEfficiency() const;

    // Additional calculations
    void CalculateNormals();
    void CalculateTangentSpace(uint32_t tex_channnel = 0);
    void CompleteVertexData(uint32_t tex_channnel = 0);
    void CalculateBBoxes();
};

#endif   // INTERNALREP_H
