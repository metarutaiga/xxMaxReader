//==============================================================================
// xxMaxReader : xxMaxReader Source
//
// Copyright (c) 2025 TAiGA
// https://github.com/metarutaiga/xxmaxreader
//==============================================================================
#include <stdio.h>
#include <format>
#include <functional>
#include <map>
#include "compoundfilereader/src/include/compoundfilereader.h"
#include "compoundfilereader/src/include/utf.h"
#include "xxMaxReader.h"

struct GUID
{
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
};

static void parse(xxMaxNode::Block& block, char const* begin, char const* end)
{
    for (;;)
    {
        bool children = false;
        char const* header = begin;
        if (begin + 6 > end)
            break;
        uint16_t type = 0;
        uint64_t length = 0;
        memcpy(&type, begin, 2); begin += 2;
        memcpy(&length, begin, 4); begin += 4;
        if (length == 0)
        {
            if (begin + 8 > end)
                break;
            memcpy(&length, begin, 8); begin += 8;
            if (length == 0)
                break;
            if (length & 0x8000000000000000ull)
            {
                length &= 0x7FFFFFFFFFFFFFFFull;
                children = true;
            }
        }
        else if (length & 0x80000000ull)
        {
            length &= 0x7FFFFFFFull;
            children = true;
        }
        char const* next = (header + length);
        if (next > end)
            break;
        if (children)
        {
            xxMaxNode::Block child;
            parse(child, begin, next);
            block.emplace_back(type, std::format("{:04X}", type), std::move(child), 0);
        }
        else
        {
            std::vector<char> property;
            property.assign(begin, next);
            block.properties.emplace_back(type, std::format("{:04X}", type), std::move(property));
        }
        begin = next;
    }
}

static std::vector<char> getProperty(xxMaxNode::Block const& block, uint16_t typeID)
{
    for (auto const& [type, _2, property] : block.properties)
        if (type == typeID)
            return property;
    return std::vector<char>();
}

static std::vector<char> getClass(xxMaxNode::Block const& classDirectory, uint16_t classID, uint16_t typeID)
{
    if (classDirectory.size() <= classID)
        return std::vector<char>();
    auto const& [type, _2, child, _4] = classDirectory[classID];
    return getProperty(child, typeID);
}

xxMaxNode* xxMaxReader(char const* name)
{
    FILE* file = fopen(name, "rb");
    if (file == nullptr)
        return nullptr;

    std::vector<char> dataClassData;
    std::vector<char> dataClassDirectory;
    std::vector<char> dataConfig;
    std::vector<char> dataDllDirectory;
    std::vector<char> dataScene;
    std::vector<char> dataVideoPostQueue;

    try
    {
        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        fseek(file, 0, SEEK_SET);
        std::vector<char> buffer(size);
        fread(buffer.data(), 1, size, file);
        fclose(file);

        CFB::CompoundFileReader cfbReader(buffer.data(), buffer.size());
        cfbReader.EnumFiles(cfbReader.GetRootEntry(), -1, [&](CFB::COMPOUND_FILE_ENTRY const* entry, CFB::utf16string const& dir, int level)
        {
            std::string name = UTF16ToUTF8(entry->name);
            if (name == "ClassData")
            {
                dataClassData.resize(entry->size);
                cfbReader.ReadFile(entry, 0, dataClassData.data(), dataClassData.size());
            }
            else if (name == "ClassDirectory" || name == "ClassDirectory3")
            {
                dataClassDirectory.resize(entry->size);
                cfbReader.ReadFile(entry, 0, dataClassDirectory.data(), dataClassDirectory.size());
            }
            else if (name == "Config")
            {
                dataConfig.resize(entry->size);
                cfbReader.ReadFile(entry, 0, dataConfig.data(), dataConfig.size());
            }
            else if (name == "DllDirectory")
            {
                dataDllDirectory.resize(entry->size);
                cfbReader.ReadFile(entry, 0, dataDllDirectory.data(), dataDllDirectory.size());
            }
            else if (name == "Scene")
            {
                dataScene.resize(entry->size);
                cfbReader.ReadFile(entry, 0, dataScene.data(), dataScene.size());
            }
            else if (name == "VideoPostQueue")
            {
                dataVideoPostQueue.resize(entry->size);
                cfbReader.ReadFile(entry, 0, dataVideoPostQueue.data(), dataVideoPostQueue.size());
            }
        });
    }
    catch (...)
    {
        return nullptr;
    }

    xxMaxNode* root = new xxMaxNode;
    root->classData = new xxMaxNode::Block;
    root->classDirectory = new xxMaxNode::Block;
    root->config = new xxMaxNode::Block;
    root->dllDirectory = new xxMaxNode::Block;
    root->scene = new xxMaxNode::Block;
    root->videoPostQueue = new xxMaxNode::Block;

    parse(*root->classData, dataClassData.data(), dataClassData.data() + dataClassData.size());
    parse(*root->classDirectory, dataClassDirectory.data(), dataClassDirectory.data() + dataClassDirectory.size());
    parse(*root->config, dataConfig.data(), dataConfig.data() + dataConfig.size());
    parse(*root->dllDirectory, dataDllDirectory.data(), dataDllDirectory.data() + dataDllDirectory.size());
    parse(*root->scene, dataScene.data(), dataScene.data() + dataScene.size());
    parse(*root->videoPostQueue, dataVideoPostQueue.data(), dataVideoPostQueue.data() + dataVideoPostQueue.size());

    // ClassName
    for (auto& [type, _2, block, _4] : (*root->scene))
    {
        if (type != 0x2020)
            continue;
        std::map<uint32_t, xxMaxNode*> nodes;
        for (uint32_t i = 0; i < block.size(); ++i)
        {
            auto& [classID, className, child, _4] = block[i];

            // DllIndex
            // 00000000-0000-0000-0000-000000000000
            std::vector<char> propertyClassName = getClass(*root->classDirectory, classID, 0x2042);
            std::vector<char> propertyClassGUID = getClass(*root->classDirectory, classID, 0x2060);
            if (propertyClassName.empty() || propertyClassGUID.size() != 16)
                continue;
            className = UTF16ToUTF8((uint16_t*)propertyClassName.data(), propertyClassName.size() / sizeof(uint16_t));
            GUID* classGUID = (GUID*)propertyClassGUID.data();
#if 0
            printf("%08X-%04X-%04X-%02X%02X%02X%02X%02X%02X\n",
                   classGUID->Data1, classGUID->Data2, classGUID->Data3,
                   classGUID->Data4[0], classGUID->Data4[1], classGUID->Data4[2], classGUID->Data4[3],
                   classGUID->Data4[4], classGUID->Data4[5], classGUID->Data4[6], classGUID->Data4[7]);
#endif

            // Node
            // FFFFFFFF-0001-0000-0000-000001000000
            static GUID const guidNode = { 0xFFFFFFFF, 0x0001, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 } };

            // RootNode
            // FFFFFFFF-0002-0000-0000-000001000000
            static GUID const guidRootNode = { 0xFFFFFFFF, 0x0002, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 } };
            if (memcmp(classGUID, &guidNode, 16) != 0 && memcmp(classGUID, &guidRootNode, 16) != 0)
                continue;

            xxMaxNode node;

            // Parent
            std::vector<char> propertyParent = getProperty(child, 0x0960);
            xxMaxNode* parent = root;
            if (propertyParent.size() >= 4)
            {
                uint32_t index = *(uint32_t*)propertyParent.data();
                xxMaxNode* found = nodes[index];
                if (found)
                {
                    parent = found;
                }
                else
                {
                    printf("Parent %d is not found! (Node:%d)\n", index, i);
                }
            }

            // Name
            std::vector<char> propertyName = getProperty(child, 0x0962);
            if (propertyName.empty() == false)
            {
                node.name = UTF16ToUTF8((uint16_t*)propertyName.data(), propertyName.size() / sizeof(uint16_t));
            }
            else
            {
                node.name = className;
            }

            parent->emplace_back(std::move(node));
            nodes[i] = &parent->back();
        }
        break;
    }

    return root;
}
