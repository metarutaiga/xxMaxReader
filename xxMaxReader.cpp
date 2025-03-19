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

#define BASENODE_CLASS_ID   1

struct Class_ID
{
    uint32_t a;
    uint32_t b;
};

struct ClassChunk
{
    uint32_t DllIndex;
    Class_ID ClassID;
    uint32_t SuperClassID;
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

template<typename T = char>
static std::vector<T> getProperty(xxMaxNode::Block const& block, uint16_t typeID)
{
    for (auto const& [type, _2, property] : block.properties)
        if (type == typeID)
            return std::vector<T>((T*)property.data(), (T*)property.data() + property.size() / sizeof(T));
    return std::vector<T>();
}

template<typename T = char>
static std::vector<T> getClass(xxMaxNode::Block const& classDirectory, uint16_t classID, uint16_t typeID)
{
    if (classDirectory.size() <= classID)
        return std::vector<T>();
    auto const& [type, _2, child, _4] = classDirectory[classID];
    return getProperty<T>(child, typeID);
}

static std::map<uint32_t, uint32_t> getLink(xxMaxNode::Block const& block)
{
    std::map<uint32_t, uint32_t> link;
    std::vector<uint32_t> propertyLink2034 = getProperty<uint32_t>(block, 0x2034);
    for (uint32_t i = 0; i < propertyLink2034.size(); ++i)
    {
        link[i] = propertyLink2034[i];
    }
    std::vector<uint32_t> propertyLink2035 = getProperty<uint32_t>(block, 0x2035);
    for (uint32_t i = 1; i + 1 < propertyLink2035.size(); i += 2)
    {
        uint32_t index = propertyLink2035[i + 0];
        link[index] = propertyLink2035[i + 1];
    }
    return link;
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

            // ClassChunk
            // FFFFFFFF-00000001-00000000-00000001 - Node
            // FFFFFFFF-00000002-00000000-00000001 - RootNode
            std::vector<uint16_t> propertyClassName = getClass<uint16_t>(*root->classDirectory, classID, 0x2042);
            std::vector<ClassChunk> propertyClassChunk = getClass<ClassChunk>(*root->classDirectory, classID, 0x2060);
            if (propertyClassName.empty() || propertyClassChunk.empty())
                continue;
            className = UTF16ToUTF8(propertyClassName.data(), propertyClassName.size());
            ClassChunk* classChunk = propertyClassChunk.data();
#if 0
            printf("%08X-%08X-%08X-%08X\n", classChunk->DllIndex, classChunk->ClassID.a, classChunk->ClassID.b, classChunk->SuperClassID);
#endif

            switch (classChunk->SuperClassID)
            {
            case BASENODE_CLASS_ID:
            {
                xxMaxNode node;

                // Parent
                std::vector<uint32_t> propertyParent = getProperty<uint32_t>(child, 0x0960);
                xxMaxNode* parent = root;
                if (propertyParent.empty() == false)
                {
                    uint32_t index = *propertyParent.data();
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
                std::vector<uint16_t> propertyName = getProperty<uint16_t>(child, 0x0962);
                if (propertyName.size() > 0)
                {
                    node.name = UTF16ToUTF8(propertyName.data(), propertyName.size());
                }
                else
                {
                    node.name = className;
                }

                // Reference
                std::map<uint32_t, uint32_t> link = getLink(child);
                for (auto [index, chunkID] : link)
                {
                    if (block.size() <= chunkID)
                        continue;
                    auto& [_1, _2, child, _4] = block[chunkID];
                    switch (index)
                    {
                    case 0: // Position/Rotation/Scale
                    {
                        std::map<uint32_t, uint32_t> link = getLink(child);
                        for (auto [index, chunkID] : link)
                        {
                            if (block.size() <= chunkID)
                                continue;
                            auto& [_1, _2, child, _4] = block[chunkID];
                            switch (index)
                            {
                            case 0: // Position XYZ
                            {
                                std::map<uint32_t, uint32_t> link = getLink(child);
                                for (auto [index, chunkID] : link)
                                {
                                    if (block.size() <= chunkID)
                                        continue;
                                    if (index >= 3)
                                        continue;
                                    for (auto& [type, _2, child, _4] : std::get<2>(block[chunkID]))
                                    {
                                        if (type != 0x7127)
                                            continue;
                                        for (auto& [type, _2, property] : child.properties)
                                        {
                                            if (type != 0x2501)
                                                continue;
                                            if (property.size() < sizeof(float))
                                                continue;
                                            memcpy(&node.position[index], property.data(), sizeof(float));
                                        }
                                    }
                                }
                                break;
                            }
                            default:
                                break;
                            }
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }

                parent->emplace_back(std::move(node));
                nodes[i] = &parent->back();
                break;
            }
            default:
                break;
            }
        }
        break;
    }

    return root;
}
