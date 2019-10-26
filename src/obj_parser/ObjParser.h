#ifndef OBJPARSER_H
#define OBJPARSER_H

#include "../Parser.h"
#include <glm/glm.hpp>
#include <vector>

class ObjConverter;

class ObjParser : public Parser
{
protected:
    struct SubMesh
    {
        std::vector<glm::vec3> _pos;
        std::vector<glm::vec3> _nrm;
        std::vector<glm::vec2> _tex;

        std::string                        _mat_name;
        std::vector<std::vector<uint32_t>> _index;
    };

    std::vector<SubMesh> _meshes;
    std::string          _matlib;

    friend ObjConverter;

public:
    ObjParser()                   = default;
    virtual ~ObjParser() override = default;

    void                       Parse(const std::string & fname, const CmdLineOptions & cmd) override;
    std::unique_ptr<Converter> GetConverter() const override;
};

#endif   // OBJPARSER_H
