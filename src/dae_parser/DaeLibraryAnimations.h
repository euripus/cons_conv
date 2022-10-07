#ifndef DAELIBRARYANIMATIONS_H
#define DAELIBRARYANIMATIONS_H

#include "DaeSource.h"

struct DaeSampler
{
    std::string _id;
    DaeSource * _input;    // Time values
    DaeSource * _output;   // Transformation data
};

struct DaeChannel
{
    DaeSampler * _source;
    std::string  _node_id;              // Target node
    std::string  _trans_sid;            // Target transformation channel
    int          _trans_values_index;   // Index in values of node transformation (-1 for no index)
};

struct DaeAnimation
{
    std::string               _id;
    std::vector<DaeSource>    _sources;
    std::vector<DaeSampler>   _samplers;
    std::vector<DaeChannel>   _channels;
    std::vector<DaeAnimation> _children;

    void         Parse(pugi::xml_node const & animNode, unsigned int & maxFrameCount, float & maxAnimTime);
    DaeSampler * FindAnimForTarget(std::string const & nodeId, int * transValuesIndex) const;

protected:
    DaeSource * FindSource(std::string const & id) const;
};

struct DaeLibraryAnimations
{
    std::vector<DaeAnimation> _animations;
    unsigned int              _max_frame_count;
    float                     _max_anim_time;

    void         Parse(pugi::xml_node const & rootNode);
    DaeSampler * FindAnimForTarget(std::string const & nodeId, int * index) const;
};

#endif   // DAELIBRARYANIMATIONS_H
