#ifndef TXTEXPORTER_H
#define TXTEXPORTER_H

#include "../Exporter.h"

class TxtExporter : public Exporter
{
    bool geometry;
    bool animation;
    bool material;

public:
    TxtExporter(const CmdLineOptions & cmd);

    void WriteFile(const std::string & basic_fname, const InternalData & rep) const override;
};

#endif   // TXTEXPORTER_H
