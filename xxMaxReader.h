//==============================================================================
// xxMaxReader : xxMaxReader Header
//
// Copyright (c) 2025 TAiGA
// https://github.com/metarutaiga/xxmaxreader
//==============================================================================
#pragma once

#include <array>
#include <list>
#include <string>
#include <vector>

struct xxMaxNode : public std::list<xxMaxNode>
{
    std::string name;

    std::array<float, 3> position = { 0, 0, 0 };
    std::array<float, 4> rotation = { 0, 0, 0, 1 };
    std::array<float, 3> scale = { 1, 1, 1 };

    std::vector<uint32_t> indices;
    std::vector<std::array<float, 3>> vertices;

    std::string text;

    uint16_t padding = 0;

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

    ~xxMaxNode()
    {
        delete classData;
        delete classDirectory;
        delete config;
        delete dllDirectory;
        delete scene;
        delete videoPostQueue;
    }
};

xxMaxNode* xxMaxReader(char const* name, int(*log)(char const*, ...));
