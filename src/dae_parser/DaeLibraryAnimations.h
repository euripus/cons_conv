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
    std::string  _nodeId;             // Target node
    std::string  _transSid;           // Target transformation channel
    int          _transValuesIndex;   // Index in values of node transformation (-1 for no index)
};

struct DaeAnimation
{
    std::string               _id;
    std::vector<DaeSource>    _sources;
    std::vector<DaeSampler>   _samplers;
    std::vector<DaeChannel>   _channels;
    std::vector<DaeAnimation> _children;

    void         Parse(const pugi::xml_node & animNode, unsigned int & maxFrameCount, float & maxAnimTime);
    DaeSampler * FindAnimForTarget(const std::string & nodeId, int * transValuesIndex) const;

protected:
    DaeSource * FindSource(const std::string & id) const;
};

struct DaeLibraryAnimations
{
    std::vector<DaeAnimation> _animations;
    unsigned int              _maxFrameCount;
    float                     _maxAnimTime;

    void         Parse(const pugi::xml_node & rootNode);
    DaeSampler * FindAnimForTarget(const std::string & nodeId, int * index) const;
};

#endif   // DAELIBRARYANIMATIONS_H
