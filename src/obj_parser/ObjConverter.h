#ifndef OBJCONVERTER_H
#define OBJCONVERTER_H

#include "../Converter.h"
#include "ObjParser.h"

class ObjConverter : public Converter
{
    ObjParser const & _parser;

public:
    ObjConverter(ObjParser const & ps) : _parser(ps) {}
    virtual ~ObjConverter() {}

    void Convert() override;
    void ExportToInternal(InternalData & rep, CmdLineOptions const & cmd) const override;

    void ReadMaterials(std::string const & fname, InternalData & rep) const;
};

#endif   // OBJCONVERTER_H
