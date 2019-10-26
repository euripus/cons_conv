#include "DaeLibraryAnimations.h"
#include "DaeParser.h"
#include <cstring>
#include <iostream>

/*******************************************************************************
 * DaeAnimation
 *******************************************************************************/
DaeSource * DaeAnimation::FindSource(const std::string & id) const
{
    if(id.empty())
        return nullptr;

    for(auto & src : _sources)
    {
        if(src._id == id)
            return const_cast<DaeSource *>(&src);
    }

    return nullptr;
}

DaeSampler * DaeAnimation::FindAnimForTarget(const std::string & nodeId, int * transValuesIndex) const
{
    if(nodeId.empty())
        return nullptr;

    for(auto & chn : _channels)
    {
        if(chn._nodeId == nodeId)
        {
            if(transValuesIndex != nullptr)
                *transValuesIndex = chn._transValuesIndex;
            return chn._source;
        }
    }

    // Parse children
    for(auto & chd : _children)
    {
        DaeSampler * sampler = chd.FindAnimForTarget(nodeId, transValuesIndex);
        if(sampler != nullptr)
            return sampler;
    }

    return nullptr;
}

void DaeAnimation::Parse(const pugi::xml_node & animNode, unsigned int & maxFrameCount, float & maxAnimTime)
{
    _id = animNode.attribute("id").value();

    // Sources
    for(pugi::xml_node node1 = animNode.child("source"); node1; node1 = node1.next_sibling("source"))
    {
        _sources.emplace_back();
        _sources.back().Parse(node1);
    }

    // Samplers
    for(pugi::xml_node node1 = animNode.child("sampler"); node1; node1 = node1.next_sibling("sampler"))
    {
        _samplers.emplace_back();
        DaeSampler & sampler = _samplers.back();

        sampler._id = node1.attribute("id").value();

        for(pugi::xml_node node2 = node1.child("input"); node2; node2 = node2.next_sibling("input"))
        {
            if(strcmp(node2.attribute("semantic").value(), "INPUT") == 0)
            {
                std::string id = node2.attribute("source").value();
                RemoveGate(id);
                sampler._input = FindSource(id);
            }
            else if(strcmp(node2.attribute("semantic").value(), "OUTPUT") == 0)
            {
                std::string id = node2.attribute("source").value();
                RemoveGate(id);
                sampler._output = FindSource(id);
            }
        }

        if(sampler._input == nullptr || sampler._output == nullptr)
            _samplers.pop_back();
        else
        {
            unsigned int frameCount = (unsigned int)sampler._input->_floatArray.size();
            maxFrameCount           = std::max(maxFrameCount, frameCount);

            for(unsigned int i = 0; i < frameCount; ++i)
                maxAnimTime = std::max(maxAnimTime, sampler._input->_floatArray[i]);
        }
    }

    // Channels
    for(pugi::xml_node node1 = animNode.child("channel"); node1; node1 = node1.next_sibling("channel"))
    {
        _channels.emplace_back();
        DaeChannel & channel      = _channels.back();
        channel._source           = nullptr;
        channel._transValuesIndex = -1;

        // Parse target
        std::string s   = node1.attribute("target").value();
        size_t      pos = s.find("/");
        if(pos != std::string::npos && pos != s.length() - 1)
        {
            channel._nodeId   = s.substr(0, pos);
            channel._transSid = s.substr(pos + 1, s.length() - pos);
            if(channel._transSid.find(".X") != std::string::npos)
            {
                channel._transValuesIndex = 0;
                channel._transSid         = channel._transSid.substr(0, channel._transSid.find("."));
            }
            else if(channel._transSid.find(".Y") != std::string::npos)
            {
                channel._transValuesIndex = 1;
                channel._transSid         = channel._transSid.substr(0, channel._transSid.find("."));
            }
            else if(channel._transSid.find(".Z") != std::string::npos)
            {
                channel._transValuesIndex = 2;
                channel._transSid         = channel._transSid.substr(0, channel._transSid.find("."));
            }
            else if(channel._transSid.find(".ANGLE") != std::string::npos)
            {
                channel._transValuesIndex = 3;
                channel._transSid         = channel._transSid.substr(0, channel._transSid.find("."));
            }
            else if(channel._transSid.find('(') != std::string::npos)
            {
                size_t index1 = channel._transSid.find('(');
                size_t index2 = channel._transSid.find('(', index1 + 1);
                if(index2 == std::string::npos)   // We got a vector index
                {
                    channel._transValuesIndex =
                        atoi(channel._transSid
                                 .substr(index1 + 1, channel._transSid.find(')', index1) - (index1 + 1))
                                 .c_str());
                }
                else   // We got an array index
                {
                    int x = atoi(channel._transSid
                                     .substr(index1 + 1, channel._transSid.find(')', index1) - (index1 + 1))
                                     .c_str());
                    int y = atoi(channel._transSid
                                     .substr(index2 + 1, channel._transSid.find(')', index2) - (index2 + 1))
                                     .c_str());
                    // TODO: Is this the correct access order? Maybe collada defines it transposed
                    channel._transValuesIndex = y * 4 + x;
                }
                channel._transSid = channel._transSid.substr(0, index1);
            }
        }

        // Find source
        s = node1.attribute("source").value();
        RemoveGate(s);
        if(!s.empty())
        {
            for(auto & smp : _samplers)
            {
                if(smp._id == s)
                {
                    channel._source = &smp;
                    break;
                }
            }
        }

        if(channel._nodeId.empty() || channel._transSid.empty() || channel._source == nullptr)
        {
            std::cerr << "Warning: Missing channel attributes or sampler not found" << std::endl;
            _channels.pop_back();
        }
    }

    // Parse children
    for(pugi::xml_node node1 = animNode.child("animation"); node1; node1 = node1.next_sibling("animation"))
    {
        _children.emplace_back();
        _children.back().Parse(node1, maxFrameCount, maxAnimTime);
    }
}

/*******************************************************************************
 * DaeLibraryAnimations
 *******************************************************************************/
void DaeLibraryAnimations::Parse(const pugi::xml_node & rootNode)
{
    _maxFrameCount = 0;

    pugi::xml_node node1 = rootNode.child("library_animations");
    if(node1.empty())
        return;

    for(pugi::xml_node node2 = node1.child("animation"); node2; node2 = node2.next_sibling("animation"))
    {
        _animations.emplace_back();
        _animations.back().Parse(node2, _maxFrameCount, _maxAnimTime);
    }
}

DaeSampler * DaeLibraryAnimations::FindAnimForTarget(const std::string & nodeId, int * index) const
{
    for(auto & anm : _animations)
    {
        DaeSampler * sampler = anm.FindAnimForTarget(nodeId, index);
        if(sampler != nullptr)
            return sampler;
    }

    return nullptr;
}
