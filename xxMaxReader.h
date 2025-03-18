//==============================================================================
// xxMaxReader : xxMaxReader Header
//
// Copyright (c) 2025 TAiGA
// https://github.com/metarutaiga/xxmaxreader
//==============================================================================
#pragma once

#include <list>
#include <string>
#include <tuple>
#include <vector>

struct xxMaxNode : public std::list<xxMaxNode>
{
    std::string name;

    bool dummy = false;

public:
    struct Block : public std::vector<std::tuple<uint16_t, std::string, Block, uint16_t>>
    {
        typedef std::vector<std::tuple<uint16_t, std::string, std::vector<char>>> Properties;
        Properties properties;
    };
    Block* classData = nullptr;
    Block* classDirectory = nullptr;
    Block* config = nullptr;
    Block* dllDirectory = nullptr;
    Block* scene = nullptr;
    Block* videoPostQueue = nullptr;

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

xxMaxNode* xxMaxReader(char const* name);
