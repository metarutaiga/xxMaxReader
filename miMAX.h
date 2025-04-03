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
