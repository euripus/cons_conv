#ifndef TXTEXPORTER_H
#define TXTEXPORTER_H

#include "../Exporter.h"

class TxtExporter : public Exporter
{
    bool geometry;
    bool animation;
    bool material;
    bool rel_matrices;

public:
    TxtExporter(CmdLineOptions const & cmd);

    void WriteFile(std::string const & basic_fname, InternalData const & rep) const override;
};

#endif   // TXTEXPORTER_H
