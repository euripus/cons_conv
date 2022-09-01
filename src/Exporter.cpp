#include "Exporter.h"
#include "./txt_export/TxtExporter.h"

std::unique_ptr<Exporter> Exporter::GetExporter(CmdLineOptions const & cmd)
{
    if(cmd.plain_text_export)
        return std::make_unique<TxtExporter>(cmd);

    return std::unique_ptr<Exporter>(nullptr);
}
