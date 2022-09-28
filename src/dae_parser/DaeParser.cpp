#include "DaeParser.h"
#include "DaeConverter.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "DaeLibraryAnimations.h"
#include "DaeLibraryControllers.h"
#include "DaeLibraryEffects.h"
#include "DaeLibraryGeometries.h"
#include "DaeLibraryImages.h"
#include "DaeLibraryMaterials.h"
#include "DaeLibraryVisualScenes.h"

DaeParser::DaeParser() :
    _up_axis(UpAxis::Unknown),
    _images(std::make_unique<DaeLibraryImages>()),
    _effects(std::make_unique<DaeLibraryEffects>()),
    _material(std::make_unique<DaeLibraryMaterials>()),
    _geom(std::make_unique<DaeLibraryGeometries>()),
    _v_scenes(std::make_unique<DaeLibraryVisualScenes>()),
    _controllers(std::make_unique<DaeLibraryControllers>()),
    _anim(std::make_unique<DaeLibraryAnimations>())
{}

DaeParser::~DaeParser() {}

void DaeParser::Parse(std::string const & fname, CmdLineOptions const & cmd)
{
    pugi::xml_document     doc;
    pugi::xml_node         root, asset;
    pugi::xml_parse_result result = doc.load_file(fname.c_str());

    if(!result)
    {
        std::stringstream ss;
        ss << "Error while parse file:\"" << fname << "\". "
           << "Error description:\"" << result.description() << "\".\n";

        throw std::runtime_error(ss.str());
    }

    root = doc.first_child();
    // File content check
    if(strcmp(root.name(), "COLLADA") != 0)
    {
        std::stringstream ss;
        ss << "Error while parse file:\"" << fname << "\". "
           << "Error: not a COLLADA document"
           << "\n";

        throw std::runtime_error(ss.str());
    }
    if(strcmp(root.attribute("version").value(), "1.4.0") != 0
       && strcmp(root.attribute("version").value(), "1.4.1") != 0)
    {
        std::stringstream ss;
        ss << "Error while parse file:\"" << fname << "\". "
           << "Error: Unsupported Collada version"
           << "\n";

        throw std::runtime_error(ss.str());
    }

    asset = root.child("asset");
    if(!asset.empty())
    {
        pugi::xml_node axis = asset.child("up_axis");
        if(!axis.empty() && axis.text().get() != nullptr)
        {
            if(strcmp(axis.text().get(), "Y_UP") == 0)
                _up_axis = UpAxis::Y_UP;
            else if(strcmp(axis.text().get(), "Z_UP") == 0)
                _up_axis = UpAxis::Z_UP;
            else if(strcmp(axis.text().get(), "X_UP") == 0)
                _up_axis = UpAxis::X_UP;
            else
                std::cout << "While parse file:\"" << fname << "\". "
                          << "Warning: Unsupported up-axis" << std::endl;
        }
    }

    // Parse libraries
    _images->Parse(root);
    _effects->Parse(root);
    // Assign images
    for(auto & effect : _effects->effects)
    {
        if(!effect.diffuseMapId.empty())
        {
            effect.diffuseMap = _images->FindImage(effect.diffuseMapId);

            if(effect.diffuseMap == nullptr)
                std::cout << "Warning: Image '" << effect.diffuseMapId << "' not found" << std::endl;
        }
    }

    _material->Parse(root);
    // Assign effects
    for(auto & material : _material->materials)
    {
        material.effect = _effects->FindEffect(material.effectId);

        if(material.effect == nullptr)
            std::cout << "Warning: Effect '" << material.effectId << "' not found" << std::endl;
    }

    _geom->Parse(root);
    _v_scenes->Parse(root);
    _controllers->Parse(root);
    _anim->Parse(root);
}

std::unique_ptr<Converter> DaeParser::GetConverter() const
{
    return std::make_unique<DaeConverter>(*this);
}

/*******************************************************************************
 * Global functions
 *******************************************************************************/
glm::mat4 GetConvertMatrix(DaeParser::UpAxis up)
{
    // COLLADA: All coordinates are right-handed by definition.
    // default(openGL): Y_UP: Right(Positive x)  Up(Positive y) In(Positive z)
    glm::mat4 mt(1.0f);

    // glm::mat4 constructor assigned elements in column major order
    if(up == DaeParser::UpAxis::Z_UP)
    {
        // Z_UP: Right(Positive x)  Up(Positive z) In(Negative y)
        mt = glm::mat4(glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 0.0f, -1.0f, 0.0f),
                       glm::vec4(0.0f, 1.0f, 0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    }
    else if(up == DaeParser::UpAxis::X_UP)
    {
        // X-UP: Right(Negative y)  Up(Positive x) In(Positive z)
        mt = glm::mat4(glm::vec4(0.0f, 1.0f, 0.0f, 0.0f), glm::vec4(-1.0f, 0.0f, 0.0f, 0.0f),
                       glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    return mt;
}

glm::mat4 CreateDAEMatrix(float const * m)
{
    // glm::mat4 constructor assigned elements in column major order
    // matrices in COLLADA are written in row-major order to aid the human reader

    if(m == nullptr)
    {
        std::cerr << "Error: null pointer for matrix generation" << std::endl;
        return glm::mat4(1.0);
    }

    glm::mat4 mt(glm::vec4(m[0], m[4], m[8], m[12]), glm::vec4(m[1], m[5], m[9], m[13]),
                 glm::vec4(m[2], m[6], m[10], m[14]), glm::vec4(m[3], m[7], m[11], m[15]));

    return mt;
}

void RemoveGate(std::string & s)
{
    if(s.length() == 0)
        return;

    if(s[0] == '#')
        s = s.substr(1, s.length() - 1);
}
