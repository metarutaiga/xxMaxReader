/*
    2012 Kaetemi https://blog.kaetemi.be
    2025 TAiGA   https://github.com/metarutaiga/miMAX
*/
#pragma once

#include <array>
#include <list>
#include <string>
#include <vector>

struct miMaxNode : public std::list<miMaxNode>
{
public:
    typedef std::array<float, 3> Point3;
    typedef std::array<float, 4> Point4;

public:
    std::string name;
    std::string text;

    Point3 position = { 0, 0, 0 };
    Point4 rotation = { 0, 0, 0, 1 };
    Point3 scale = { 1, 1, 1 };

    std::vector<Point3> vertex;
    std::vector<Point3> texture;

    std::vector<Point3> normal;
    std::vector<Point3> vertexColor;
    std::vector<Point3> vertexIllum;
    std::vector<Point3> vertexAlpha;

    std::vector<std::vector<uint32_t>> vertexArray;
    std::vector<std::vector<uint32_t>> textureArray;

    int padding = 0;

public:
    typedef std::pair<uint32_t, uint32_t> ClassID;
    typedef uint32_t SuperClassID;

    struct ClassData
    {
        uint32_t dllIndex;
        ClassID classID;
        SuperClassID superClassID;
    };

public:
    struct Chunk : public std::vector<Chunk>
    {
        std::vector<char> property;

        uint16_t type = 0;
        uint16_t padding = 0;
        ClassData classData = {};
        std::string classDllFile;
        std::string classDllName;
        std::string name;
    };
    Chunk* classData = nullptr;
    Chunk* classDirectory = nullptr;
    Chunk* config = nullptr;
    Chunk* dllDirectory = nullptr;
    Chunk* scene = nullptr;
    Chunk* videoPostQueue = nullptr;

    ~miMaxNode()
    {
        delete classData;
        delete classDirectory;
        delete config;
        delete dllDirectory;
        delete scene;
        delete videoPostQueue;
    }
};

miMaxNode* miMAXOpenFile(char const* name, int(*log)(char const*, ...));

#if defined(__MIMAX_INTERNAL__)
typedef miMaxNode::ClassID ClassID;
typedef miMaxNode::SuperClassID SuperClassID;
typedef miMaxNode::ClassData ClassData;
typedef miMaxNode::Point3 Point3;
typedef miMaxNode::Point4 Point4;
typedef miMaxNode::Chunk Chunk;

#define BASENODE_SUPERCLASS_ID          0x00000001
#define PARAMETER_BLOCK_SUPERCLASS_ID   0x00000008
#define PARAMETER_BLOCK2_SUPERCLASS_ID  0x00000082
#define GEOMOBJECT_SUPERCLASS_ID        0x00000010
#define OSM_SUPERCLASS_ID               0x00000810
#define FLOAT_SUPERCLASS_ID             0x00009003
#define MATRIX3_SUPERCLASS_ID           0x00009008
#define POSITION_SUPERCLASS_ID          0x0000900b
#define ROTATION_SUPERCLASS_ID          0x0000900c
#define SCALE_SUPERCLASS_ID             0x0000900d

#define LININTERP_POSITION_CLASS_ID     ClassID{0x00002002, 0x00000000}
#define LININTERP_ROTATION_CLASS_ID     ClassID{0x00002003, 0x00000000}
#define LININTERP_SCALE_CLASS_ID        ClassID{0x00002004, 0x00000000}
#define PRS_CONTROL_CLASS_ID            ClassID{0x00002005, 0x00000000}
#define HYBRIDINTERP_FLOAT_CLASS_ID     ClassID{0x00002007, 0x00000000}
#define HYBRIDINTERP_POSITION_CLASS_ID  ClassID{0x00002008, 0x00000000}
#define HYBRIDINTERP_SCALE_CLASS_ID     ClassID{0x00002010, 0x00000000}
#define HYBRIDINTERP_POINT4_CLASS_ID    ClassID{0x00002012, 0x00000000}
#define TCBINTERP_POSITION_CLASS_ID     ClassID{0x00442312, 0x00000000}
#define TCBINTERP_ROTATION_CLASS_ID     ClassID{0x00442313, 0x00000000}
#define TCBINTERP_SCALE_CLASS_ID        ClassID{0x00442315, 0x00000000}
#define IPOS_CONTROL_CLASS_ID           ClassID{0x118f7e02, 0xffee238a}

#define BOXOBJ_CLASS_ID                 ClassID{0x00000010, 0x00000000}
#define SPHERE_CLASS_ID                 ClassID{0x00000011, 0x00000000}
#define CYLINDER_CLASS_ID               ClassID{0x00000012, 0x00000000}
#define TORUS_CLASS_ID                  ClassID{0x00000020, 0x00000000}
#define CONE_CLASS_ID                   ClassID{0xa86c23dd, 0x00000000}
#define GSPHERE_CLASS_ID                ClassID{0x00000000, 0x00007f9e}
#define TUBE_CLASS_ID                   ClassID{0x00007b21, 0x00000000}
#define PYRAMID_CLASS_ID                ClassID{0x76bf318a, 0x4bf37b10}
#define PLANE_CLASS_ID                  ClassID{0x081f1dfc, 0x77566f65}
#define EDITTRIOBJ_CLASS_ID             ClassID{0xe44f10b3, 0x00000000}
#define EPOLYOBJ_CLASS_ID               ClassID{0x1bf8338d, 0x192f6098}

#define EDIT_NORMALS_CLASS_ID           ClassID{0x4aa52ae3, 0x35ca1cde}
#define PAINTLAYERMOD_CLASS_ID          ClassID{0x7ebb4645, 0x7be2044b}
#endif
