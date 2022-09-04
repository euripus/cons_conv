// http://www.radmangames.com/programming/how-to-use-boost-program_options

#include "CmdLineOptions.h"

#include <boost/program_options.hpp>
#include <iostream>

#define CONV_VERSION "Console converter version 0.2"

bool ParseCmdLine(int argc, char ** argv, CmdLineOptions & cmd) noexcept
{
    boost::program_options::variables_map vm;

    try
    {
        boost::program_options::options_description generic("Generic options");
        generic.add_options()("help", "produce help message")("version", "Print program version");

        boost::program_options::options_description config("Export flags");
        config.add_options()("cache-optimize",
                             boost::program_options::value<bool>(&cmd.geometry_optimize)->default_value(true),
                             "Geometry optimization\n\t0|1")(
            "export-type,E", boost::program_options::value<std::string>()->default_value("geo"),
            "Specify data for export\n\tgeo+anim")(
            "convert-type,C", boost::program_options::value<std::string>()->default_value("txt"),
            "Type of export file: either plain text or binary format\n\tbin|txt")(
            "matrix-type,M", boost::program_options::value<bool>(&cmd.relative)->default_value(false),
            "Export relative matrices for animation\n\t0|1")(
            "material-export",
            boost::program_options::value<bool>(&cmd.material_export)->default_value(false),
            "Flag for material export\n\t0|1")(
            "tex-channel", boost::program_options::value<uint32_t>(&cmd.chan)->default_value(0),
            "texture channel for TBN calculating\n\t0 - 3");

        boost::program_options::options_description hiden("Hidden options");
        hiden.add_options()("input-file", boost::program_options::value<std::vector<std::string>>(),
                            "input file");

        boost::program_options::options_description visible(CONV_VERSION);
        visible.add(generic).add(config);

        boost::program_options::options_description cmdline_options;
        cmdline_options.add(generic).add(config).add(hiden);

        boost::program_options::positional_options_description p;
        p.add("input-file", -1);

        try
        {
            store(boost::program_options::command_line_parser(argc, argv)
                      .options(cmdline_options)
                      .positional(p)
                      .run(),
                  vm);
            notify(vm);
        }
        catch(boost::program_options::error const & e)
        {
            std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
            std::cerr << cmdline_options << std::endl;
            return false;
        }

        if(vm.count("help"))
        {
            std::cout << visible << std::endl;
            return true;
        }
    }
    catch(std::exception const & e)
    {
        std::cerr << "ERROR! Failed parsing commandline parameters" << std::endl;
        std::cerr << "\t" << e.what() << std::endl;
        return false;
    }

    if(vm.count("version"))
    {
        std::cout << CONV_VERSION << std::endl;
        return true;
    }

    if(vm.count("cache-optimize"))
    {
        cmd.geometry_optimize = vm["cache-optimize"].as<bool>();
    }

    if(vm.count("material-export"))
    {
        cmd.material_export = vm["material-export"].as<bool>();
    }

    if(vm.count("tex-channel"))
    {
        cmd.chan = vm["tex-channel"].as<uint32_t>();
    }

    if(vm.count("convert-type"))
    {
        if(vm["convert-type"].as<std::string>().size() == 3)
        {
            if(vm["convert-type"].as<std::string>() == std::string("bin"))
                cmd.plain_text_export = false;
            else if(vm["convert-type"].as<std::string>() == std::string("txt"))
                cmd.plain_text_export = true;
            else
            {
                std::cerr << "ERROR! Invalid --convert-type parameter" << std::endl;
                return false;
            }
        }
        else
        {
            std::cerr << "ERROR! Invalid --convert-type parameter" << std::endl;
            return false;
        }
    }

    if(vm.count("export-type"))
    {
        if(vm["export-type"].as<std::string>().find("geo") != std::string::npos)
            cmd.geometry = true;
        if(vm["export-type"].as<std::string>().find("anim") != std::string::npos)
            cmd.animation = true;
    }

    if(vm.count("input-file"))
    {
        cmd.file_list = vm["input-file"].as<std::vector<std::string>>();
    }
    else
    {
        std::cerr << "ERROR! No filename argument(s)." << std::endl;
        return false;
    }

    return true;
}
