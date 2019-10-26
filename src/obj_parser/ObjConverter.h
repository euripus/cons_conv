#ifndef OBJCONVERTER_H
#define OBJCONVERTER_H

#include "../Converter.h"
#include "ObjParser.h"

class ObjConverter : public Converter
{
    const ObjParser & _parser;

public:
    ObjConverter(const ObjParser & ps) : _parser(ps) {}
    virtual ~ObjConverter() {}

    void Convert() override;
    void ExportToInternal(InternalData & rep, const CmdLineOptions & cmd) const override;

    void ReadMaterials(const std::string & fname, InternalData & rep) const;
};

#endif   // OBJCONVERTER_H
