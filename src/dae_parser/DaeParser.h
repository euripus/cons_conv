#ifndef DAEPARSER_H
#define DAEPARSER_H

#include "../Parser.h"
#include <glm/glm.hpp>

class DaeLibraryImages;
class DaeLibraryEffects;
class DaeLibraryMaterials;
class DaeLibraryGeometries;
class DaeLibraryVisualScenes;
class DaeLibraryControllers;
class DaeLibraryAnimations;

class DaeParser : public Parser
{
public:
    enum class UpAxis
    {
        Unknown,
        X_UP,
        Y_UP,
        Z_UP,
    };

    DaeParser();
    virtual ~DaeParser() override;

    void                       Parse(std::string const & fname, CmdLineOptions const & cmd) override;
    std::unique_ptr<Converter> GetConverter() const override;

private:
    UpAxis _up_axis;

    std::unique_ptr<DaeLibraryImages>       _images;
    std::unique_ptr<DaeLibraryEffects>      _effects;
    std::unique_ptr<DaeLibraryMaterials>    _material;
    std::unique_ptr<DaeLibraryGeometries>   _geom;
    std::unique_ptr<DaeLibraryVisualScenes> _v_scenes;
    std::unique_ptr<DaeLibraryControllers>  _controllers;
    std::unique_ptr<DaeLibraryAnimations>   _anim;

    friend class DaeConverter;
};

glm::mat4 GetConvertMatrix(DaeParser::UpAxis up);
glm::mat4 CreateDAEMatrix(float const * mat);

void RemoveGate(std::string & s);

#endif   // DAEPARSER_H
