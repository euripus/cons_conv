#ifndef AABB_H
#define AABB_H

#include <cassert>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

class AABB
{
    glm::vec3 _min;
    glm::vec3 _max;

public:
    /** Creates an uninitialized bounding box. */
    inline AABB() : _min(FLT_MAX, FLT_MAX, FLT_MAX), _max(FLT_MIN, FLT_MIN, FLT_MIN) {}

    /** Creates a bounding box initialized to the given extents. */
    inline AABB(float xmin, float ymin, float zmin, float xmax, float ymax, float zmax) :
        _min(xmin, ymin, zmin), _max(xmax, ymax, zmax)
    {}

    inline AABB(const AABB & bb) : _min(bb._min), _max(bb._max) {}

    inline AABB(AABB && bb) : _min(bb._min), _max(bb._max) {}

    inline AABB & operator=(const AABB & bb)
    {
        if(this != &bb)
        {
            _min = bb._min;
            _max = bb._max;
        }

        return *this;
    }

    inline AABB & operator=(AABB && bb)
    {
        if(this != &bb)
        {
            _min = bb._min;
            _max = bb._max;
        }

        return *this;
    }

    ~AABB() {}

    inline bool operator==(const AABB & rhs) const { return _min == rhs._min && _max == rhs._max; }
    inline bool operator!=(const AABB & rhs) const { return _min != rhs._min || _max != rhs._max; }

    const glm::vec3 & min() const { return _min; }
    const glm::vec3 & max() const { return _max; }

    /** Build the bounding box to include the given coordinates. */
    void buildBoundBox(const std::vector<glm::vec3> & positions)
    {
        assert(positions.size() > 0);

        _min = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
        _max = glm::vec3(FLT_MIN, FLT_MIN, FLT_MIN);

        for(unsigned int i = 0; i < positions.size(); i++)
        {
            glm::vec3 pos = positions[i];

            // Min and max X
            if(pos.x < _min.x)
                _min.x = pos.x;
            if(pos.x > _max.x)
                _max.x = pos.x;

            // Min and max Y
            if(pos.y < _min.y)
                _min.y = pos.y;
            if(pos.y > _max.y)
                _max.y = pos.y;

            // Min and max Z
            if(pos.z < _min.z)
                _min.z = pos.z;
            if(pos.z > _max.z)
                _max.z = pos.z;
        }
    }

    /** Transform this bounding box */
    inline void transform(const glm::mat4 & matrix)
    {
        // http://dev.theomader.com/transform-bounding-boxes/
        glm::vec3 xa = glm::vec3(glm::column(matrix, 0)) * _min.x;
        glm::vec3 xb = glm::vec3(glm::column(matrix, 0)) * _max.x;

        glm::vec3 ya = glm::vec3(glm::column(matrix, 1)) * _min.y;
        glm::vec3 yb = glm::vec3(glm::column(matrix, 1)) * _max.y;

        glm::vec3 za = glm::vec3(glm::column(matrix, 2)) * _min.z;
        glm::vec3 zb = glm::vec3(glm::column(matrix, 2)) * _max.z;

        _min = glm::min(xa, xb) + glm::min(ya, yb) + glm::min(za, zb) + glm::vec3(glm::column(matrix, 3));
        _max = glm::max(xa, xb) + glm::max(ya, yb) + glm::max(za, zb) + glm::vec3(glm::column(matrix, 3));
    }

    /** Expands the bounding box to include the given coordinate.
     * If the box is uninitialized, set its min and max extents to v. */
    inline void expandBy(const glm::vec3 & v)
    {
        if(v.x < _min.x)
            _min.x = v.x;
        if(v.x > _max.x)
            _max.x = v.x;

        if(v.y < _min.y)
            _min.y = v.y;
        if(v.y > _max.y)
            _max.y = v.y;

        if(v.z < _min.z)
            _min.z = v.z;
        if(v.z > _max.z)
            _max.z = v.z;
    }

    /** Expands this bounding box to include the given bounding box.
     * If this box is uninitialized, set it equal to bb. */
    void expandBy(const AABB & bb)
    {
        if(bb._min.x < _min.x)
            _min.x = bb._min.x;
        if(bb._max.x > _max.x)
            _max.x = bb._max.x;

        if(bb._min.y < _min.y)
            _min.y = bb._min.y;
        if(bb._max.y > _max.y)
            _max.y = bb._max.y;

        if(bb._min.z < _min.z)
            _min.z = bb._min.z;
        if(bb._max.z > _max.z)
            _max.z = bb._max.z;
    }

    /** Returns the intersection of this bounding box and the specified bounding box. */
    inline AABB intersect(const AABB & bb) const
    {
        return AABB(std::max(_min.x, bb._min.x), std::max(_min.y, bb._min.y), std::max(_min.z, bb._min.z),
                    std::min(_max.x, bb._max.x), std::min(_max.y, bb._max.y), std::min(_max.z, bb._max.z));
    }

    /** Return true if this bounding box intersects the specified bounding box. */
    inline bool intersects(const AABB & bb) const
    {
        return std::max(bb._min.x, _min.x) <= std::min(bb._max.x, _max.x)
               && std::max(bb._min.y, _min.y) <= std::min(bb._max.y, _max.y)
               && std::max(bb._min.z, _min.z) <= std::min(bb._max.z, _max.z);
    }

    /** Returns true if this bounding box contains the specified coordinate. */
    inline bool contains(const glm::vec3 & v) const
    {
        return (v.x >= _min.x && v.x <= _max.x) && (v.y >= _min.y && v.y <= _max.y)
               && (v.z >= _min.z && v.z <= _max.z);
    }
};

#endif   // AABB_H
