#include "InternalRep.h"
#include "utils.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <list>
#include <set>
#include <sstream>
#include <stdexcept>

InternalData::InternalData() : num_frames(0), frame_rate(0.0f) {}

unsigned int InternalData::RemoveDegeneratedTriangles()
{
    unsigned int num_deg_tris = 0;

    for(auto & mesh : meshes)
    {
        for(unsigned int i = 0; i < mesh.indexes.size(); i += 3)
        {
            glm::vec3 v1 = mesh.pos[mesh.indexes[i + 0]];
            glm::vec3 v2 = mesh.pos[mesh.indexes[i + 1]];
            glm::vec3 v3 = mesh.pos[mesh.indexes[i + 2]];

            if(glm::length(glm::cross((v2 - v1), (v3 - v1))) < Epsilon<float>::epsilon())
            {
                num_deg_tris++;
                // Remove triangle indices
                mesh.indexes.erase(mesh.indexes.begin() + i + 2);
                mesh.indexes.erase(mesh.indexes.begin() + i + 1);
                mesh.indexes.erase(mesh.indexes.begin() + i + 0);
                i -= 3;
            }
        }
    }

    return num_deg_tris;
}

//===========================================================================//

struct OptFace;
struct OptVertex
{
    unsigned int        index = {0};   // Index in vertex array
    float               score = {0.0f};
    std::set<OptFace *> faces = {};   // Faces that are using this vertex

    void UpdateScore(int cacheIndex);
};

struct OptFace
{
    unsigned int id;
    OptVertex *  verts[3];

    OptFace(unsigned int cur_id) : id(cur_id), verts{nullptr, nullptr, nullptr} {}

    float GetScore() { return verts[0]->score + verts[1]->score + verts[2]->score; }
};

void OptVertex::UpdateScore(int cache_index)
{
    if(faces.empty())
    {
        score = 0;
        return;
    }

    // The constants used here are coming from the paper
    if(cache_index < 0)
        score = 0;   // Not in cache
    else if(cache_index < 3)
        score = 0.75f;   // Among three most recent vertices
    else
        score = std::pow(1.0f - ((cache_index - 3.0f) / InternalData::maxCacheSize), 1.5f);

    score += 2.0f * std::pow((float)faces.size(), -0.5f);
}

// Implementation of Linear-Speed Vertex Cache Optimization by Tom Forsyth
// https://tomforsyth1000.github.io/papers/fast_vert_cache_opt.html
void InternalData::OptimizeIndexOrder()
{
    for(auto & mesh : meshes)
    {
        if(mesh.indexes.empty())
            continue;

        std::vector<OptVertex>    verts(mesh.pos.size());
        std::vector<unsigned int> new_index;
        std::list<OptFace>        faces;
        std::list<OptVertex *>    cache;

        // Build vertex and triangle structures
        for(unsigned int i = 0; i < mesh.indexes.size(); i += 3)
        {
            faces.emplace_back(i / 3);
            OptFace & face = faces.back();

            face.verts[0] = &verts[mesh.indexes[i + 0]];
            face.verts[1] = &verts[mesh.indexes[i + 1]];
            face.verts[2] = &verts[mesh.indexes[i + 2]];
            face.verts[0]->faces.insert(&face);
            face.verts[1]->faces.insert(&face);
            face.verts[2]->faces.insert(&face);
        }

        for(unsigned int i = 0; i < verts.size(); ++i)
        {
            verts[i].index = i;
            verts[i].UpdateScore(-1);
        }

        // Main loop of algorithm
        while(!faces.empty())
        {
            OptFace * best_face  = nullptr;
            float     best_score = -1.0f;

            // Try to find best scoring face in cache
            auto itr1 = cache.begin();
            while(itr1 != cache.end())
            {
                auto itr2 = (*itr1)->faces.begin();
                while(itr2 != (*itr1)->faces.end())
                {
                    if((*itr2)->GetScore() > best_score)
                    {
                        best_face  = *itr2;
                        best_score = best_face->GetScore();
                    }
                    ++itr2;
                }
                ++itr1;
            }

            // If that didn't work find it in the complete list of triangles
            if(best_face == nullptr)
            {
                auto itr2 = faces.begin();
                while(itr2 != faces.end())
                {
                    if((*itr2).GetScore() > best_score)
                    {
                        best_face  = &(*itr2);
                        best_score = best_face->GetScore();
                    }
                    ++itr2;
                }
            }

            // Process vertices of best face
            for(unsigned int i = 0; i < 3; ++i)
            {
                // Add vertex to draw list
                new_index.push_back(best_face->verts[i]->index);

                // Move vertex to head of cache
                itr1 = std::find(cache.begin(), cache.end(), best_face->verts[i]);
                if(itr1 != cache.end())
                    cache.erase(itr1);
                cache.push_front(best_face->verts[i]);

                // Remove face from vertex lists
                best_face->verts[i]->faces.erase(best_face);
            }

            // Remove best face
            faces.erase(std::find_if(faces.begin(), faces.end(), [&best_face](OptFace const & fc) -> bool {
                return best_face->id == fc.id;
            }));

            // Update scores of vertices in cache
            unsigned int cacheIndex = 0;
            for(itr1 = cache.begin(); itr1 != cache.end(); ++itr1)
            {
                (*itr1)->UpdateScore(cacheIndex++);
            }

            // Trim cache
            for(auto i = cache.size(); i > maxCacheSize; --i)
            {
                cache.pop_back();
            }
        }

        std::copy(new_index.begin(), new_index.end(), mesh.indexes.begin());
    }
}

float InternalData::CalcCacheEfficiency() const
{
    float        atvr = 0.0f;
    unsigned int i    = 0;
    for(auto & mesh : meshes)
    {
        unsigned int            misses = 0;
        std::list<unsigned int> test_cache;
        for(auto index : mesh.indexes)
        {
            if(std::find(test_cache.begin(), test_cache.end(), index) == test_cache.end())
            {
                test_cache.push_back(index);
                if(test_cache.size() > maxCacheSize)
                    test_cache.erase(test_cache.begin());
                ++misses;
            }
        }

        // Average transform to vertex ratio (ATVR)
        // 1.0 is theoretical optimum, meaning that each vertex is just transformed exactly one time
        atvr += (float)(mesh.indexes.size() + misses) / mesh.indexes.size();
        i++;
    }

    return atvr / i;
}

//===========================================================================//

void InternalData::CalculateNormals()
{
    for(auto & mesh : meshes)
    {
        mesh.normal.clear();
        mesh.normal.resize(mesh.pos.size(), glm::vec3(0.0f));

        for(unsigned int i = 0; i < mesh.indexes.size() / 3; i++)
        {
            // default CCW order
            glm::vec3 const & a          = mesh.pos[mesh.indexes[i * 3 + 0]];
            glm::vec3 const & b          = mesh.pos[mesh.indexes[i * 3 + 1]];
            glm::vec3 const & c          = mesh.pos[mesh.indexes[i * 3 + 2]];
            glm::vec3         faceNormal = glm::normalize(glm::cross(b - a, c - a));

            mesh.normal[mesh.indexes[i * 3 + 0]] += faceNormal;
            mesh.normal[mesh.indexes[i * 3 + 1]] += faceNormal;
            mesh.normal[mesh.indexes[i * 3 + 2]] += faceNormal;
        }

        std::for_each(mesh.normal.begin(), mesh.normal.end(),
                      [](glm::vec3 & nm) { nm = glm::normalize(nm); });
    }
}

// Basic algorithm: Eric Lengyel, Mathematics for 3D Game Programming & Computer Graphics
// section 7.8.3
void InternalData::CalculateTangentSpace(uint32_t tex_channnel)
{
    if(tex_channnel > meshes[0].tex_coords.size())
    {
        std::stringstream ss;
        ss << "Error: No necessary texture channel"
           << "\n";

        throw std::runtime_error(ss.str());
    }

    unsigned int num_invalid_basis = 0;

    for(auto & mesh : meshes)
    {
        mesh.tangent.clear();
        mesh.bitangent.clear();
        mesh.tangent.resize(mesh.pos.size(), glm::vec3(0.0));
        mesh.bitangent.resize(mesh.pos.size(), glm::vec3(0.0));

        for(unsigned int i = 0; i < mesh.indexes.size(); i += 3)
        {
            glm::vec3 const & v1 = mesh.pos[mesh.indexes[i + 0]];
            glm::vec3 const & v2 = mesh.pos[mesh.indexes[i + 1]];
            glm::vec3 const & v3 = mesh.pos[mesh.indexes[i + 2]];

            glm::vec2 const & w1 = mesh.tex_coords[tex_channnel][mesh.indexes[i + 0]];
            glm::vec2 const & w2 = mesh.tex_coords[tex_channnel][mesh.indexes[i + 1]];
            glm::vec2 const & w3 = mesh.tex_coords[tex_channnel][mesh.indexes[i + 2]];

            float x1 = v2.x - v1.x;
            float x2 = v3.x - v1.x;
            float y1 = v2.y - v1.y;
            float y2 = v3.y - v1.y;
            float z1 = v2.z - v1.z;
            float z2 = v3.z - v1.z;
            float s1 = w2.x - w1.x;
            float s2 = w3.x - w1.x;
            float t1 = w2.y - w1.y;
            float t2 = w3.y - w1.y;

            float r = 1.0f / (s1 * t2 - s2 * t1);

            glm::vec3 sDir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r);
            glm::vec3 tDir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r, (s1 * z2 - s2 * z1) * r);

            mesh.tangent[mesh.indexes[i + 0]] += sDir;
            mesh.tangent[mesh.indexes[i + 1]] += sDir;
            mesh.tangent[mesh.indexes[i + 2]] += sDir;

            mesh.bitangent[mesh.indexes[i + 0]] += tDir;
            mesh.bitangent[mesh.indexes[i + 1]] += tDir;
            mesh.bitangent[mesh.indexes[i + 2]] += tDir;
        }

        // Normalize tangent space basis
        for(unsigned int i = 0; i < mesh.pos.size(); i++)
        {
            glm::vec3 const & n = mesh.normal[i];
            glm::vec3 const & t = mesh.tangent[i];

            mesh.tangent[i] = glm::normalize(t - n * glm::dot(n, t));

            float handedness  = glm::dot(glm::cross(n, t), mesh.bitangent[i]) < 0 ? -1.0f : 1.0f;
            mesh.bitangent[i] = glm::cross(n, t) * handedness;

            // Check if tangent space basis is invalid
            if(mesh.normal[i].length() == 0 || mesh.tangent[i].length() == 0
               || mesh.bitangent[i].length() == 0)
                ++num_invalid_basis;
        }
    }

    if(num_invalid_basis > 0)
    {
        std::cout << "Warning: Geometry has zero-length basis vectors\n";
        std::cout << "   Maybe two faces point in opposite directions and share same vertices\n";
    }
}

void InternalData::CompleteVertexData(uint32_t tex_channnel)
{
    if(meshes.empty() || meshes[0].tex_coords[0].empty() ||   // Required data
       meshes[0].pos.empty())
    {
        std::stringstream ss;
        ss << "Error: No position or/and texture coord data"
           << "\n";

        throw std::runtime_error(ss.str());
    }

    if(meshes[0].normal.empty())
        CalculateNormals();

    if(meshes[0].tangent.empty() || meshes[0].bitangent.empty())
        CalculateTangentSpace(tex_channnel);
}

void InternalData::CalculateBBoxes()
{
    for(auto & msh : meshes)
    {
        if(!msh.pos.empty())
            msh.bbox.buildBoundBox(msh.pos);
    }

    bboxes.clear();
    for(unsigned int i = 0; i < num_frames; ++i)
    {
        std::vector<glm::vec3> new_pos_vec;

        for(auto & msh : meshes)
        {
            for(unsigned int j = 0; j < msh.pos.size(); ++j)
            {
                glm::mat4 mat_tr(0.0f);
                for(auto & weight : msh.weights[j])
                {
                    glm::mat4 mt = glm::mat4_cast(joints[weight.joint_index - 1].a_rot[i]);
                    mt = glm::column(mt, 3, glm::vec4(joints[weight.joint_index - 1].a_trans[i], 1.0f));

                    mat_tr += weight.w * mt;
                }

                glm::vec4 cpos = mat_tr * glm::vec4(msh.pos[j], 1.0);
                new_pos_vec.push_back(glm::vec3(cpos));
            }
        }
        AABB temp;
        temp.buildBoundBox(new_pos_vec);
        bboxes.push_back(temp);
    }
}
