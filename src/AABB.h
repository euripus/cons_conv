#ifndef AABB_H
#define AABB_H

#include <algorithm>
#include <cassert>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <vector>

constexpr float min_float = -std::numeric_limits<float>::max();
constexpr float max_float = std::numeric_limits<float>::max();

//! Axis-aligned minimum bounding box class
/*!
    This class holds AABB for a given point set,
    is its minimum bounding box subject to the constraint
    that the edges of the box are parallel to the (Cartesian) coordinate axes..
*/
class AABB
{
    glm::vec3 m_min; /*!< The corner with the smallest values for each coordinate of the AABB */
    glm::vec3 m_max; /*!< The corner with the largest values for each coordinate of the AABB */
public:
    //! Construct to invalid values to represent an unset bounding box
    inline AABB() : m_min(max_float), m_max(min_float) {}

    //! Construct to with specified min and max values
    inline AABB(float xmin, float ymin, float zmin, float xmax, float ymax, float zmax) :
        m_min(xmin, ymin, zmin), m_max(xmax, ymax, zmax)
    {}

    //! Construct to with specified min and max values
    inline AABB(glm::vec3 min, glm::vec3 max) : m_min(min), m_max(max) {}

    inline AABB(AABB const & bb) : m_min(bb.m_min), m_max(bb.m_max) {}

    inline AABB(AABB && bb) : m_min(bb.m_min), m_max(bb.m_max) {}

    inline AABB & operator=(AABB const & bb)
    {
        if(this != &bb)
        {
            m_min = bb.m_min;
            m_max = bb.m_max;
        }

        return *this;
    }

    inline AABB & operator=(AABB && bb)
    {
        if(this != &bb)
        {
            std::swap(m_min, bb.m_min);
            std::swap(m_max, bb.m_max);
        }

        return *this;
    }

    ~AABB() {}

    inline bool operator==(AABB const & rhs) const { return m_min == rhs.m_min && m_max == rhs.m_max; }

    inline bool operator!=(AABB const & rhs) const { return m_min != rhs.m_min || m_max != rhs.m_max; }

    inline glm::vec3 min() const { return m_min; }

    inline glm::vec3 max() const { return m_max; }

    /*! Expands the bounding box to include the given coordinate.
        If this box is uninitialized, set its min and max extents to v.
        \param[in] v given coordinate
    */
    inline void expandBy(glm::vec3 const & v)
    {
        if(v.x < m_min.x)
            m_min.x = v.x;
        if(v.x > m_max.x)
            m_max.x = v.x;

        if(v.y < m_min.y)
            m_min.y = v.y;
        if(v.y > m_max.y)
            m_max.y = v.y;

        if(v.z < m_min.z)
            m_min.z = v.z;
        if(v.z > m_max.z)
            m_max.z = v.z;
    }

    /*! Expands this bounding box to include the given bounding box.
        If this box is uninitialized, set it equal to bb.
        \param[in] bb given bounding box
    */
    void expandBy(AABB const & bb)
    {
        if(bb.m_min.x < m_min.x)
            m_min.x = bb.m_min.x;
        if(bb.m_max.x > m_max.x)
            m_max.x = bb.m_max.x;

        if(bb.m_min.y < m_min.y)
            m_min.y = bb.m_min.y;
        if(bb.m_max.y > m_max.y)
            m_max.y = bb.m_max.y;

        if(bb.m_min.z < m_min.z)
            m_min.z = bb.m_min.z;
        if(bb.m_max.z > m_max.z)
            m_max.z = bb.m_max.z;
    }

    /*! Calculate intersection of bounding boxes
        \param[in] bb specified bounding box
        \return AABB the intersection of this bounding box and the specified bounding box.
    */
    inline AABB intersect(AABB const & bb) const
    {
        return AABB(glm::max(m_min.x, bb.m_min.x), glm::max(m_min.y, bb.m_min.y),
                    glm::max(m_min.z, bb.m_min.z), glm::min(m_max.x, bb.m_max.x),
                    glm::min(m_max.y, bb.m_max.y), glm::min(m_max.z, bb.m_max.z));
    }

    /*! Check for intersection of bounding boxes
        \param[in] bb AABB that will be checked
        \return True if this bounding box intersects the specified bounding box.
    */
    inline bool intersects(AABB const & bb) const
    {
        return glm::max(bb.m_min.x, m_min.x) <= glm::min(bb.m_max.x, m_max.x)
               && glm::max(bb.m_min.y, m_min.y) <= glm::min(bb.m_max.y, m_max.y)
               && glm::max(bb.m_min.z, m_min.z) <= glm::min(bb.m_max.z, m_max.z);
    }

    /*! Check for the containing of the coordinate in the AABB
        \param[in] v coordinate that will be checked
        \return True if this AABB contains the specified coordinate.
    */
    inline bool contains(glm::vec3 const & v) const
    {
        return (v.x >= m_min.x && v.x <= m_max.x) && (v.y >= m_min.y && v.y <= m_max.y)
               && (v.z >= m_min.z && v.z <= m_max.z);
    }

    //! Transform this bounding box
    /*! Transform a given axis aligned bounding box by some matrix and then
        convert it into an axis aligned bounding box in the resulting
        coordinate space again
        \param[in] matrix transformation matrix
    */
    inline void transform(glm::mat4 const & matrix)
    {
        // https://stackoverflow.com/questions/6053522/how-to-recalculate-axis-aligned-bounding-box-after-translate-rotate/
        glm::vec3 new_min(max_float), new_max(min_float);
        for(int i = 0; i < 3; i++)
        {
            new_min[i] = new_max[i] = matrix[3][i];
            for(int j = 0; j < 3; j++)
            {
                float e = matrix[j][i] * m_min[j];
                float f = matrix[j][i] * m_max[j];

                new_min[i] += glm::min(e, f);
                new_max[i] += glm::max(e, f);
            }
        }

        m_min = new_min;
        m_max = new_max;

        // slow variant
        //        std::vector<glm::vec4> corner_points{
        //            glm::vec4(m_min.x, m_max.y, m_min.z, 1.0f), glm::vec4(m_max.x, m_max.y, m_min.z, 1.0f),
        //            glm::vec4(m_max.x, m_min.y, m_min.z, 1.0f), glm::vec4(m_min.x, m_min.y, m_min.z, 1.0f),
        //            glm::vec4(m_min.x, m_max.y, m_max.z, 1.0f), glm::vec4(m_max.x, m_max.y, m_max.z, 1.0f),
        //            glm::vec4(m_max.x, m_min.y, m_max.z, 1.0f), glm::vec4(m_min.x, m_min.y, m_max.z, 1.0f),
        //        };

        //        std::for_each(begin(corner_points), end(corner_points),
        //                      [&matrix](glm::vec4 & pnt) { pnt = matrix * pnt; });

        //        buildBoundBox(corner_points);
    }

    /*! Build the bounding box to include the given coordinates.
        \param[in] positions point set for building AABB
    */
    void buildBoundBox(std::vector<glm::vec3> const & positions)
    {
        m_min = glm::vec3(max_float);
        m_max = glm::vec3(min_float);

        std::for_each(begin(positions), end(positions), [this](glm::vec3 const & pos) { expandBy(pos); });
    }

    /*! Build the bounding box to include the given coordinates.
        \param[in] positions point set for building AABB
    */
    void buildBoundBox(std::vector<glm::vec4> const & positions)
    {
        m_min = glm::vec3(max_float);
        m_max = glm::vec3(min_float);

        std::for_each(begin(positions), end(positions),
                      [this](glm::vec4 const & pos) { expandBy(glm::vec3(pos)); });
    }
};

#endif   // AABB_H
