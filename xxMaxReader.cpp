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
#define HYBRIDINTERP_SCALE_CLASS_ID     xxMaxNode::ClassID{0x00002010, 0x00000000}
#define HYBRIDINTERP_POINT4_CLASS_ID    xxMaxNode::ClassID{0x00002012, 0x00000000}

#define IPOS_CONTROL_CLASS_ID           xxMaxNode::ClassID{0x118f7e02, 0xffee238a}

#define TRY         try {
#define CATCH(x)    } catch(x)
#define THROW       throw std::runtime_error(__FILE_NAME__ ":" _LIBCPP_TOSTRING(__LINE__))

#include <zlib.h>

static std::vector<char> uncompress(std::vector<char> const& input)
{
    if (input.size() < 10 || input[0] != char(0x1F) || input[1] != char(0x8B))
        return input;

    z_stream stream = {};
    stream.next_in = (Bytef*)input.data();
    stream.avail_in = (uInt)input.size();
    inflateInit2(&stream, MAX_WBITS | 32);

    std::vector<char> output(input.size());
    stream.next_out = (Bytef*)output.data();
    stream.avail_out = (uint)output.size();
    for (;;)
    {
        int result = inflate(&stream, Z_NO_FLUSH);
        if (result == Z_BUF_ERROR)
        {
            output.push_back(0);
            output.resize(output.capacity());
            stream.next_out = (Bytef*)(output.data() + stream.total_out);
            stream.avail_out = (uint)(output.size() - stream.total_out);
            continue;
        }
        if (result == Z_STREAM_END)
        {
            output.resize(stream.total_out);
            inflateEnd(&stream);
            return output;
        }
        if (result == Z_OK)
        {
            continue;
        }
        break;
    }

    inflateEnd(&stream);
    return input;
}

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
        xxMaxNode::Chunk child;
        child.type = type;
        child.name = std::format("{:04X}", type);
        if (children)
        {
            parseChunk(child, begin, next);
        }
        else
        {
            child.property.assign(begin, next);
        }
        chunk.emplace_back(std::move(child));
        begin = next;
    }
}

static bool checkClass(int(*log)(char const*, ...), xxMaxNode::Chunk const& chunk, xxMaxNode::ClassID classID, xxMaxNode::SuperClassID superClassID)
{
    if (chunk.classData.classID == classID && chunk.classData.superClassID == superClassID)
        return true;
    auto& classData = chunk.classData;
    log("Unknown (%08X-%08X-%08X-%08X) %s", classData.dllIndex, classData.classID.first, classData.classID.second, classData.superClassID, chunk.name.c_str());
    return false;
}

template <typename... Args>
static xxMaxNode::Chunk const* getTypeChunk(xxMaxNode::Chunk const& chunk, Args&&... args)
{
    xxMaxNode::Chunk const* output = &chunk;
    for (uint16_t type : { args... })
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
    xxMaxNode::Chunk const* found = getTypeChunk(chunk, args...);
    T* data = found ? (T*)found->property.data() : nullptr;
    size_t size = found ? found->property.size() / sizeof(T) : 0;
    return std::vector<T>(data, data + size);
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
        link[i] = propertyLink2034[i];
    std::vector<uint32_t> propertyLink2035 = getProperty<uint32_t>(chunk, 0x2035);
    for (uint32_t i = 1; i + 1 < propertyLink2035.size(); i += 2)
        link[propertyLink2035[i + 0]] = propertyLink2035[i + 1];
    return link;
}

template <typename... Args>
static xxMaxNode::Chunk const* getLinkChunk(xxMaxNode::Chunk const& scene, xxMaxNode::Chunk const& chunk, Args&&... args)
{
    xxMaxNode::Chunk const* output = &chunk;
    std::map<uint32_t, uint32_t> link = getLink(*output);
    for (uint32_t index : { args... })
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

xxMaxNode* xxMaxReader(char const* name, int(*log)(char const*, ...))
{
    FILE* file = fopen(name, "rb");
    if (file == nullptr)
    {
        log("File is not found\n", name);
        return nullptr;
    }

    xxMaxNode* root = nullptr;

    TRY

    root = new xxMaxNode;
    if (root == nullptr)
    {
        log("Out of memory\n");
        THROW;
    }

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
            std::vector<char>* data = nullptr;
            if (name == "ClassData")            data = &dataClassData;
            else if (name == "ClassDirectory")  data = &dataClassDirectory;
            else if (name == "ClassDirectory3") data = &dataClassDirectory;
            else if (name == "Config")          data = &dataConfig;
            else if (name == "DllDirectory")    data = &dataDllDirectory;
            else if (name == "Scene")           data = &dataScene;
            else if (name == "VideoPostQueue")  data = &dataVideoPostQueue;
            if (data)
            {
                data->resize(entry->size);
                cfbReader.ReadFile(entry, 0, data->data(), data->size());
                (*data) = uncompress(*data);
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
    {
        log("Scene is empty\n");
        THROW;
    }
    auto& scene = root->scene->front();
    if (scene.type != 0x2020 && scene.type != 0x2023)
    {
        log("Scene type %04X is not supported\n", scene.type);
        THROW;
    }

    // First Pass
    for (uint32_t i = 0; i < scene.size(); ++i)
    {
        auto& chunk = scene[i];
        auto [className, classData] = getClass(*root->classDirectory, chunk.type);
        if (className.empty())
        {
            log("Class %04X is not found! (Chunk:%d)\n", chunk.type, i);
            continue;
        }
        auto [dllFile, dllName] = getDll(*root->dllDirectory, classData.dllIndex);
        chunk.name = className;
        chunk.classData = classData;
        chunk.classDllFile = dllFile;
        chunk.classDllName = dllName;
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
                log("Parent %d is not found! (Chunk:%d)\n", index, i);
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
                if (checkClass(log, *linkChunk, PRS_CONTROL_CLASS_ID, MATRIX3_SUPERCLASS_ID) == false)
                    continue;

                // Bezier Float
                // ????????-00002007-00000000-00009003
                // HYBRIDINTERP_FLOAT_CLASS_ID + FLOAT_SUPERCLASS_ID

                // Position XYZ
                // ????????-118F7E02-FFEE238A-0000900B
                // IPOS_CONTROL_CLASS_ID + POSITION_SUPERCLASS_ID
                for (uint32_t i = 0; i < 1; ++i)
                {
                    xxMaxNode::Chunk const* positionXYZ = getLinkChunk(scene, *linkChunk, 0);
                    if (positionXYZ == nullptr)
                        continue;
                    if (checkClass(log, *positionXYZ, IPOS_CONTROL_CLASS_ID, POSITION_SUPERCLASS_ID) == false)
                        continue;
                    for (uint32_t i = 0; i < 3; ++i)
                    {
                        xxMaxNode::Chunk const* position = getLinkChunk(scene, *positionXYZ, i);
                        if (position == nullptr)
                            continue;
                        if (checkClass(log, *position, HYBRIDINTERP_FLOAT_CLASS_ID, FLOAT_SUPERCLASS_ID) == false)
                            continue;
                        std::vector<float> propertyFloat = getProperty<float>(*position, 0x7127, 0x2501);
                        if (propertyFloat.empty())
                            continue;
                        node.position[i] = propertyFloat.front();
                    }
                }
                // Euler XYZ
                // ????????-00002012-00000000-0000900C
                // HYBRIDINTERP_POINT4_CLASS_ID + ROTATION_SUPERCLASS_ID
                for (uint32_t i = 0; i < 1; ++i)
                {
                    xxMaxNode::Chunk const* eulerXYZ = getLinkChunk(scene, *linkChunk, 1);
                    if (eulerXYZ == nullptr)
                        continue;
                    if (checkClass(log, *eulerXYZ, HYBRIDINTERP_POINT4_CLASS_ID, ROTATION_SUPERCLASS_ID) == false)
                        continue;
                    for (uint32_t i = 0; i < 3; ++i)
                    {
                        xxMaxNode::Chunk const* euler = getLinkChunk(scene, *eulerXYZ, i);
                        if (euler == nullptr)
                            continue;
                        if (checkClass(log, *euler, HYBRIDINTERP_FLOAT_CLASS_ID, FLOAT_SUPERCLASS_ID) == false)
                            continue;
                        std::vector<float> propertyFloat = getProperty<float>(*euler, 0x7127, 0x2501);
                        if (propertyFloat.empty())
                            continue;
                        node.rotation[i] = propertyFloat.front();
                    }
                }
                // Bezier Scale
                // FFFFFFFF-00002010-00000000-0000900D
                // HYBRIDINTERP_SCALE_CLASS_ID + SCALE_SUPERCLASS_ID
                if (true)
                {
                    xxMaxNode::Chunk const* scale = getLinkChunk(scene, *linkChunk, 2);
                    if (scale == nullptr)
                        continue;
                    if (checkClass(log, *scale, HYBRIDINTERP_SCALE_CLASS_ID, SCALE_SUPERCLASS_ID) == false)
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

    CATCH (std::exception const& e)
    {
        log("Exception : %s\n", e.what());
        if (file)
        {
            fclose(file);
        }
        delete root;
        return nullptr;
    }

    return root;
}
