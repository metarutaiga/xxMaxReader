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
#include <tuple>
#include "compoundfilereader/src/include/compoundfilereader.h"
#include "compoundfilereader/src/include/utf.h"
#include "xxMaxReader.h"

#define BASENODE_SUPERCLASS_ID          0x00000001
#define FLOAT_SUPERCLASS_ID             0x00009003
#define MATRIX3_SUPERCLASS_ID           0x00009008
#define POSITION_SUPERCLASS_ID          0x0000900B
#define ROTATION_SUPERCLASS_ID          0x0000900C
#define SCALE_SUPERCLASS_ID             0x0000900D

#define PRS_CONTROL_CLASS_ID            xxMaxNode::ClassID{0x00002005, 0x00000000}
#define HYBRIDINTERP_FLOAT_CLASS_ID     xxMaxNode::ClassID{0x00002007, 0x00000000}
#define HYBRIDINTERP_POINT4_CLASS_ID    xxMaxNode::ClassID{0x00002012, 0x00000000}

#define IPOS_CONTROL_CLASS_ID           xxMaxNode::ClassID{0x118f7e02, 0xffee238a}

#define TRY         try {
#define CATCH(x)    } catch(...)
#define THROW       throw

static void parseChunk(xxMaxNode::Chunk& chunk, char const* begin, char const* end)
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
            xxMaxNode::Chunk child;
            child.type = type;
            child.name = std::format("{:04X}", type);
            parseChunk(child, begin, next);
            chunk.emplace_back(std::move(child));
        }
        else
        {
            xxMaxNode::Chunk::Property property;
            property.type = type;
            property.name = std::format("{:04X}", type);
            property.assign(begin, next);
            chunk.properties.emplace_back(std::move(property));
        }
        begin = next;
    }
}

static xxMaxNode::Chunk const* getTypeChunk(xxMaxNode::Chunk const& chunk, std::vector<uint16_t> typeArray)
{
    xxMaxNode::Chunk const* output = &chunk;
    for (uint16_t type : typeArray)
    {
        auto it = std::find_if(output->begin(), output->end(), [type](auto const& chunk) { return chunk.type == type; });
        if (it == output->end())
            return nullptr;
        output = &(*it);
    }
    return output;
}

template <typename T = char, typename... Args>
static std::vector<T> getProperty(xxMaxNode::Chunk const& chunk, Args&&... args)
{
    std::vector<uint16_t> typeArray;
    (typeArray.emplace_back(args), ...);

    std::vector<uint16_t> chunkArray = typeArray;
    if (chunkArray.empty() == false)
        chunkArray.pop_back();

    xxMaxNode::Chunk const* found = getTypeChunk(chunk, chunkArray);
    if (found)
        for (auto const& property : (*found).properties)
            if (property.type == typeArray.back())
                return std::vector<T>((T*)property.data(), (T*)property.data() + property.size() / sizeof(T));

    return std::vector<T>();
}

static std::tuple<std::string, xxMaxNode::ClassData> getClass(xxMaxNode::Chunk const& classDirectory, uint16_t classIndex)
{
    if (classDirectory.size() <= classIndex)
        return {};
    auto const& chunk = classDirectory[classIndex];
    std::vector<uint16_t> propertyClassName = getProperty<uint16_t>(chunk, 0x2042);
    std::vector<xxMaxNode::ClassData> propertyClassChunk = getProperty<xxMaxNode::ClassData>(chunk, 0x2060);
    if (propertyClassName.empty() || propertyClassChunk.empty())
        return {};
    return { UTF16ToUTF8(propertyClassName.data(), propertyClassName.size()), *propertyClassChunk.data() };
}

static std::tuple<std::string, std::string> getDll(xxMaxNode::Chunk const& dllDirectory, uint32_t dllIndex)
{
    if (dllIndex == UINT32_MAX)
        return { "(Internal)", "(Internal)" };
    if (dllDirectory.size() <= dllIndex)
        return { "(Unknown)", "(Unknown)" };
    auto const& chunk = dllDirectory[dllIndex];
    std::vector<uint16_t> propertyDllFile = getProperty<uint16_t>(chunk, 0x2037);
    std::vector<uint16_t> propertyDllName = getProperty<uint16_t>(chunk, 0x2039);
    if (propertyDllFile.empty() || propertyDllName.empty())
        return { "(Unknown)", "(Unknown)" };
    return { UTF16ToUTF8(propertyDllFile.data(), propertyDllFile.size()), UTF16ToUTF8(propertyDllName.data(), propertyDllName.size()) };
}

static std::map<uint32_t, uint32_t> getLink(xxMaxNode::Chunk const& chunk)
{
    std::map<uint32_t, uint32_t> link;
    std::vector<uint32_t> propertyLink2034 = getProperty<uint32_t>(chunk, 0x2034);
    for (uint32_t i = 0; i < propertyLink2034.size(); ++i)
    {
        link[i] = propertyLink2034[i];
    }
    std::vector<uint32_t> propertyLink2035 = getProperty<uint32_t>(chunk, 0x2035);
    for (uint32_t i = 1; i + 1 < propertyLink2035.size(); i += 2)
    {
        uint32_t index = propertyLink2035[i + 0];
        link[index] = propertyLink2035[i + 1];
    }
    return link;
}

template <typename... Args>
static xxMaxNode::Chunk const* getLinkChunk(xxMaxNode::Chunk const& scene, xxMaxNode::Chunk const& chunk, Args&&... args)
{
    std::vector<uint32_t> indexedLink;
    (indexedLink.emplace_back(args), ...);

    xxMaxNode::Chunk const* output = &chunk;
    std::map<uint32_t, uint32_t> link = getLink(*output);
    for (uint32_t index : indexedLink)
    {
        auto it = link.find(index);
        if (it == link.end())
            return nullptr;
        if (scene.size() <= (*it).second)
            return nullptr;
        auto const& chunk = scene[(*it).second];
        output = &chunk;
        link = getLink(*output);
    }
    return output;
}

xxMaxNode* xxMaxReader(char const* name)
{
    FILE* file = fopen(name, "rb");
    if (file == nullptr)
        return nullptr;

    xxMaxNode* root = nullptr;

    TRY

    root = new xxMaxNode;
    if (root == nullptr)
        THROW;

    std::vector<char> dataClassData;
    std::vector<char> dataClassDirectory;
    std::vector<char> dataConfig;
    std::vector<char> dataDllDirectory;
    std::vector<char> dataScene;
    std::vector<char> dataVideoPostQueue;

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    std::vector<char> buffer(size);
    fread(buffer.data(), 1, size, file);
    fclose(file);
    file = nullptr;

    if (buffer.empty() == false)
    {
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

    // Parse
    root->classData = new xxMaxNode::Chunk;
    root->classDirectory = new xxMaxNode::Chunk;
    root->config = new xxMaxNode::Chunk;
    root->dllDirectory = new xxMaxNode::Chunk;
    root->scene = new xxMaxNode::Chunk;
    root->videoPostQueue = new xxMaxNode::Chunk;
    parseChunk(*root->classData, dataClassData.data(), dataClassData.data() + dataClassData.size());
    parseChunk(*root->classDirectory, dataClassDirectory.data(), dataClassDirectory.data() + dataClassDirectory.size());
    parseChunk(*root->config, dataConfig.data(), dataConfig.data() + dataConfig.size());
    parseChunk(*root->dllDirectory, dataDllDirectory.data(), dataDllDirectory.data() + dataDllDirectory.size());
    parseChunk(*root->scene, dataScene.data(), dataScene.data() + dataScene.size());
    parseChunk(*root->videoPostQueue, dataVideoPostQueue.data(), dataVideoPostQueue.data() + dataVideoPostQueue.size());

    // Root
    if (root->scene->empty())
        THROW;
    auto& scene = root->scene->front();
    if (scene.type != 0x2020)
        THROW;

    // First Pass
    for (uint32_t i = 0; i < scene.size(); ++i)
    {
        auto& chunk = scene[i];
        auto [className, classData] = getClass(*root->classDirectory, chunk.type);
        if (className.empty())
        {
            printf("Class %d is not found! (Chunk:%d)\n", chunk.type, i);
            continue;
        }
        auto [dllFile, dllName] = getDll(*root->dllDirectory, classData.dllIndex);
        chunk.name = className;
        chunk.classData = classData;
        chunk.classDllFile = dllFile;
        chunk.classDllName = dllName;
    }
    for (uint32_t i = 0; i < scene.properties.size(); ++i)
    {
        auto& property = scene.properties[i];
        auto [className, classData] = getClass(*root->classDirectory, property.type);
        if (className.empty())
        {
            printf("Class %d is not found! (Property:%d)\n", property.type, i);
            continue;
        }
        auto [dllFile, dllName] = getDll(*root->dllDirectory, classData.dllIndex);
        property.name = className;
        property.classData = classData;
        property.classDllFile = dllFile;
        property.classDllName = dllName;
    }

    // Second Pass
    std::map<uint32_t, xxMaxNode*> nodes;
    for (uint32_t i = 0; i < scene.size(); ++i)
    {
        auto& chunk = scene[i];
        auto& className = chunk.name;
        auto& classData = chunk.classData;

        // FFFFFFFF-00000001-00000000-00000001 - Node
        // FFFFFFFF-00000002-00000000-00000001 - RootNode
        // BASENODE_SUPERCLASS_ID
        if (classData.superClassID != BASENODE_SUPERCLASS_ID)
            continue;

        xxMaxNode node;

        // Parent
        std::vector<uint32_t> propertyParent = getProperty<uint32_t>(chunk, 0x0960);
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
                printf("Parent %d is not found! (Chunk:%d)\n", index, i);
            }
        }

        // Name
        std::vector<uint16_t> propertyName = getProperty<uint16_t>(chunk, 0x0962);
        if (propertyName.size() > 0)
        {
            node.name = UTF16ToUTF8(propertyName.data(), propertyName.size());
        }
        else
        {
            node.name = className;
        }

        // Link
        for (uint32_t i = 0; i < 4; ++i)
        {
            xxMaxNode::Chunk const* linkChunk = getLinkChunk(scene, chunk, i);
            if (linkChunk == nullptr)
                continue;
            switch (i)
            {
            case 0:
            {
                // Position/Rotation/Scale
                // FFFFFFFF-00002005-00000000-00009008
                // PRS_CONTROL_CLASS_ID + MATRIX3_SUPERCLASS_ID
                if (linkChunk->classData.classID != PRS_CONTROL_CLASS_ID ||
                    linkChunk->classData.superClassID != MATRIX3_SUPERCLASS_ID)
                    break;

                // Position XYZ
                // 00000005-118F7E02-FFEE238A-0000900B
                // IPOS_CONTROL_CLASS_ID + POSITION_SUPERCLASS_ID
                for (uint32_t i = 0; i < 3; ++i)
                {
                    xxMaxNode::Chunk const* position = getLinkChunk(scene, *linkChunk, 0, i);
                    if (position == nullptr)
                        continue;
                    std::vector<float> propertyFloat = getProperty<float>(*position, 0x7127, 0x2501);
                    if (propertyFloat.empty())
                        continue;
                    node.position[i] = propertyFloat.front();
                }
                // Euler XYZ
                // 00000005-00002012-00000000-0000900C
                // HYBRIDINTERP_POINT4_CLASS_ID + ROTATION_SUPERCLASS_ID
                for (uint32_t i = 0; i < 3; ++i)
                {
                    xxMaxNode::Chunk const* rotation = getLinkChunk(scene, *linkChunk, 1, i);
                    if (rotation == nullptr)
                        continue;
                    std::vector<float> propertyFloat = getProperty<float>(*rotation, 0x7127, 0x2501);
                    if (propertyFloat.empty())
                        continue;
                    node.rotation[i] = propertyFloat.front();
                }
                // Bezier Scale
                // FFFFFFFF-00002007-00000000-00009003
                // HYBRIDINTERP_FLOAT_CLASS_ID + FLOAT_SUPERCLASS_ID
                if (true)
                {
                    xxMaxNode::Chunk const* scale = getLinkChunk(scene, *linkChunk, 2);
                    if (scale == nullptr)
                        continue;
                    std::vector<float> propertyFloat = getProperty<float>(*scale, 0x7127, 0x2501);
                    if (propertyFloat.empty())
                        continue;
                    node.scale[0] = node.scale[1] = node.scale[2] = propertyFloat.front();
                }
                break;
            }
            default:
                break;
            }
        }

        parent->emplace_back(std::move(node));
        nodes[i] = &parent->back();
    }

    CATCH (...)
    {
        if (file)
        {
            fclose(file);
        }
        delete root;
        return nullptr;
    }

    return root;
}
