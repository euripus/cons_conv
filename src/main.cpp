#include "Exporter.h"
#include "Parser.h"
#include <iostream>

int main(int argc, char ** argv)
{
    if(argc < 2)
    {
        std::cout << "--help for help" << std::endl;
        std::cout << std::endl;
        return 0;
    }

    CmdLineOptions cmd;

    // Command line parse
    if(!ParseCmdLine(argc, argv, cmd))
    {
        return 1;
    }

    // Parse files
    for(auto & str : cmd.file_list)
    {
        try
        {
            // Parse file
            auto parser = Parser::GetParser(CheckFileExtension(str));
            parser->Parse(str, cmd);

            // Convert file
            auto converter = parser->GetConverter();
            converter->Convert();

            // Export to internal format
            InternalData rep;
            converter->ExportToInternal(rep, cmd);

            // Optimize & additional calculation
            rep.RemoveDegeneratedTriangles();
            if(cmd.geometry_optimize)
                rep.OptimizeIndexOrder();
            rep.CompleteVertexData(cmd.chan);

            // Export to designated format
            auto exporter = Exporter::GetExporter(cmd);
            exporter->WriteFile(str, rep);
        }
        catch(std::exception const & e)
        {
            std::cerr << "ERROR! Fail export file:\"" << str << "\"" << std::endl;
            std::cerr << "\tError msg: " << e.what() << std::endl;
            std::cerr << std::endl;
        }
    }

    return 0;
}
