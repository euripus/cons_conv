TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

#QMAKE_CXXFLAGS += -Weffc++

DESTDIR = $$PWD/bin

INCLUDEPATH += ./include
LIBS += -L$$PWD/lib -lpugixml -lboost_program_options

SOURCES += \
    src/dae_parser/DaeConverter.cpp \
    src/dae_parser/DaeLibraryAnimations.cpp \
    src/dae_parser/DaeLibraryControllers.cpp \
    src/dae_parser/DaeLibraryGeometries.cpp \
    src/dae_parser/DaeLibraryVisualScenes.cpp \
    src/dae_parser/DaeParser.cpp \
    src/dae_parser/DaeSource.cpp \
    src/obj_parser/ObjConverter.cpp \
    src/obj_parser/ObjParser.cpp \
    src/txt_export/TxtExporter.cpp \
    src/CmdLineOptions.cpp \
    src/Exporter.cpp \
    src/InternalRep.cpp \
    src/main.cpp \
    src/Parser.cpp \
    src/utils.cpp

HEADERS += \
    src/dae_parser/DaeConverter.h \
    src/dae_parser/DaeLibraryAnimations.h \
    src/dae_parser/DaeLibraryControllers.h \
    src/dae_parser/DaeLibraryEffects.h \
    src/dae_parser/DaeLibraryGeometries.h \
    src/dae_parser/DaeLibraryImages.h \
    src/dae_parser/DaeLibraryMaterials.h \
    src/dae_parser/DaeLibraryVisualScenes.h \
    src/dae_parser/DaeParser.h \
    src/dae_parser/DaeSource.h \
    src/obj_parser/ObjConverter.h \
    src/obj_parser/ObjParser.h \
    src/txt_export/TxtExporter.h \
    src/AABB.h \
    src/CmdLineOptions.h \
    src/Converter.h \
    src/Exporter.h \
    src/InternalRep.h \
    src/Parser.h \
    src/utils.h

DISTFILES += \
    doc/msh.txt

