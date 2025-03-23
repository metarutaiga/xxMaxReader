//==============================================================================
// xxMaxReader : xxMaxReader Source
//
// Copyright (c) 2025 TAiGA
// https://github.com/metarutaiga/xxmaxreader
//==============================================================================
#include <stdio.h>
#include <functional>
#include <map>
#include <tuple>
#include "xxMaxReader.h"

#if _CPPUNWIND || __cpp_exceptions
#include <exception>
#define TRY         try {
#define CATCH(x)    } catch(x)
#define THROW       throw std::runtime_error(__FILE_NAME__ ":" _LIBCPP_TOSTRING(__LINE__))
#else
#include <setjmp.h>
thread_local jmp_buf compoundfilereader_jmp_buf = {};
#define TRY         if (setjmp(compoundfilereader_jmp_buf) == 0) {
#define CATCH(x)    } else
#define THROW       longjmp(compoundfilereader_jmp_buf, 1);
#define throw       longjmp(compoundfilereader_jmp_buf, 1);
#endif
#include "compoundfilereader/src/include/compoundfilereader.h"
#include "compoundfilereader/src/include/utf.h"

#if defined(__APPLE__)
#include <zlib.h>
#endif

#define BASENODE_SUPERCLASS_ID          0x00000001
#define GEOMOBJECT_SUPERCLASS_ID        0x00000010
#define FLOAT_SUPERCLASS_ID             0x00009003
#define MATRIX3_SUPERCLASS_ID           0x00009008
#define POSITION_SUPERCLASS_ID          0x0000900b
#define ROTATION_SUPERCLASS_ID          0x0000900c
#define SCALE_SUPERCLASS_ID             0x0000900d

#define LININTERP_POSITION_CLASS_ID     xxMaxNode::ClassID{0x00002002, 0x00000000}
#define LININTERP_ROTATION_CLASS_ID     xxMaxNode::ClassID{0x00002003, 0x00000000}
#define LININTERP_SCALE_CLASS_ID        xxMaxNode::ClassID{0x00002004, 0x00000000}
#define PRS_CONTROL_CLASS_ID            xxMaxNode::ClassID{0x00002005, 0x00000000}
#define HYBRIDINTERP_FLOAT_CLASS_ID     xxMaxNode::ClassID{0x00002007, 0x00000000}
#define HYBRIDINTERP_SCALE_CLASS_ID     xxMaxNode::ClassID{0x00002010, 0x00000000}
#define HYBRIDINTERP_POINT4_CLASS_ID    xxMaxNode::ClassID{0x00002012, 0x00000000}
#define TCBINTERP_POSITION_CLASS_ID     xxMaxNode::ClassID{0x00442312, 0x00000000}
#define TCBINTERP_ROTATION_CLASS_ID     xxMaxNode::ClassID{0x00442313, 0x00000000}
#define TCBINTERP_SCALE_CLASS_ID        xxMaxNode::ClassID{0x00442315, 0x00000000}
#define IPOS_CONTROL_CLASS_ID           xxMaxNode::ClassID{0x118f7e02, 0xffee238a}

#define BOXOBJ_CLASS_ID                 xxMaxNode::ClassID{0x00000010, 0x00000000}
#define SPHERE_CLASS_ID                 xxMaxNode::ClassID{0x00000011, 0x00000000}
#define CYLINDER_CLASS_ID               xxMaxNode::ClassID{0x00000012, 0x00000000}
#define TORUS_CLASS_ID                  xxMaxNode::ClassID{0x00000020, 0x00000000}
#define CONE_CLASS_ID                   xxMaxNode::ClassID{0xa86c23dd, 0x00000000}
#define GSPHERE_CLASS_ID                xxMaxNode::ClassID{0x00000000, 0x00007f9e}
#define TUBE_CLASS_ID                   xxMaxNode::ClassID{0x00007b21, 0x00000000}
#define PYRAMID_CLASS_ID                xxMaxNode::ClassID{0x76bf318a, 0x4bf37b10}
#define PLANE_CLASS_ID                  xxMaxNode::ClassID{0x081f1dfc, 0x77566f65}
#define EDITTRIOBJ_CLASS_ID             xxMaxNode::ClassID{0xe44f10b3, 0x00000000}
#define EPOLYOBJ_CLASS_ID               xxMaxNode::ClassID{0x1bf8338d, 0x192f6098}

#define FLOAT_TYPE                      0x2501, 0x2503, 0x2504, 0x2505

template <typename... Args>
static std::string format(char const* format, Args&&... args)
{
    std::string output;
    size_t length = snprintf(nullptr, 0, format, args...) + 1;
    output.resize(length);
    snprintf(output.data(), length, format, args...);
    output.pop_back();
    return output;
}

static std::vector<char> uncompress(std::vector<char> const& input)
{
#if defined(__APPLE__)
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
#else
    return input;
#endif
}

static constexpr uint64_t class64(xxMaxNode::ClassID classID)
{
    return (uint64_t)classID.first | ((uint64_t)classID.second << 32);
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
        child.name = format("%04X", type);
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

template <typename... Args>
static xxMaxNode::Chunk const* getChunk(xxMaxNode::Chunk const& chunk, Args&&... args)
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
    for (uint16_t type : { args... })
    {
        xxMaxNode::Chunk const* found = getChunk(chunk, type);
        if (found == nullptr)
            continue;
        T* data = (T*)found->property.data();
        size_t size = found->property.size() / sizeof(T);
        return std::vector<T>(data, data + size);
    }
    return std::vector<T>();
}

static std::tuple<std::string, xxMaxNode::ClassData> getClass(xxMaxNode::Chunk const& classDirectory, uint16_t classIndex)
{
    if (classDirectory.size() <= classIndex)
        return {};
    auto const& chunk = classDirectory[classIndex];
    std::vector<uint16_t> propertyClassName = getProperty<uint16_t>(chunk, 0x2042);
    std::vector<xxMaxNode::ClassData> propertyClassChunk = getProperty<xxMaxNode::ClassData>(chunk, 0x2060);
    if (propertyClassChunk.empty())
        return {};
    if (propertyClassName.empty())
        return { "(Unnamed)", *propertyClassChunk.data() };
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

static bool checkClass(int(*log)(char const*, ...), xxMaxNode::Chunk const& chunk, xxMaxNode::ClassID classID, xxMaxNode::SuperClassID superClassID)
{
    if (chunk.classData.classID == classID && chunk.classData.superClassID == superClassID)
        return true;
    auto& classData = chunk.classData;
    log("Unknown (%08X-%08X-%08X-%08X) %s", classData.dllIndex, classData.classID.first, classData.classID.second, classData.superClassID, chunk.name.c_str());
    log("\n");
    return false;
}

static void eulerToQuaternion(float quaternion[4], float euler[3])
{
    float cx = cosf(euler[0] * 0.5f);
    float cy = cosf(euler[1] * 0.5f);
    float cz = cosf(euler[2] * 0.5f);
    float sx = sinf(euler[0] * 0.5f);
    float sy = sinf(euler[1] * 0.5f);
    float sz = sinf(euler[2] * 0.5f);
    quaternion[0] = (sx * cy * cz - cx * sy * sz);
    quaternion[1] = (cx * sy * cz + sx * cy * sz);
    quaternion[2] = (cx * cy * sz - sx * sy * cz);
    quaternion[3] = (cx * cy * cz + sx * sy * sz);
}

static void getPositionRotationScale(int(*log)(char const*, ...), xxMaxNode::Chunk const& scene, xxMaxNode::Chunk const& chunk, xxMaxNode& node)
{
    // ????????-00002007-00000000-00009003 Bezier Float             HYBRIDINTERP_FLOAT_CLASS_ID + FLOAT_SUPERCLASS_ID

    // FFFFFFFF-00002005-00000000-00009008 Position/Rotation/Scale  PRS_CONTROL_CLASS_ID + MATRIX3_SUPERCLASS_ID
    if (checkClass(log, chunk, PRS_CONTROL_CLASS_ID, MATRIX3_SUPERCLASS_ID) == false)
        return;

    // FFFFFFFF-00002002-00000000-0000900B Linear Position  LININTERP_POSITION_CLASS_ID + POSITION_SUPERCLASS_ID
    // ????????-118F7E02-FFEE238A-0000900B Position XYZ     IPOS_CONTROL_CLASS_ID + POSITION_SUPERCLASS_ID
    // FFFFFFFF-00442312-00000000-0000900B TCB Position     TCBINTERP_POSITION_CLASS_ID + POSITION_SUPERCLASS_ID
    for (uint32_t i = 0; i < 1; ++i)
    {
        xxMaxNode::Chunk const* positionXYZ = getLinkChunk(scene, chunk, 0);
        if (positionXYZ == nullptr)
            continue;
        auto& classData = positionXYZ->classData;
        if (classData.superClassID == POSITION_SUPERCLASS_ID)
        {
            if (classData.classID == IPOS_CONTROL_CLASS_ID)
            {
                for (uint32_t i = 0; i < 3; ++i)
                {
                    xxMaxNode::Chunk const* position = getLinkChunk(scene, *positionXYZ, i);
                    if (position == nullptr)
                        continue;
                    if (checkClass(log, *position, HYBRIDINTERP_FLOAT_CLASS_ID, FLOAT_SUPERCLASS_ID) == false)
                        continue;
                    xxMaxNode::Chunk const* chunk7127 = getChunk(*position, 0x7127);
                    if (chunk7127)
                        position = chunk7127;
                    std::vector<float> propertyFloat = getProperty<float>(*position, FLOAT_TYPE);
                    if (propertyFloat.size() >= 1)
                    {
                        node.position[i] = propertyFloat[0];
                        continue;
                    }
                    log("Value is not found (%s)", position->name.c_str());
                    log("\n");
                }
                continue;
            }
            if (classData.classID == LININTERP_POSITION_CLASS_ID ||
                classData.classID == TCBINTERP_POSITION_CLASS_ID)
            {
                xxMaxNode::Chunk const* chunk7127 = getChunk(*positionXYZ, 0x7127);
                if (chunk7127)
                    positionXYZ = chunk7127;
                std::vector<float> propertyFloat = getProperty<float>(*positionXYZ, FLOAT_TYPE);
                if (propertyFloat.size() >= 3)
                {
                    node.position[0] = propertyFloat[0];
                    node.position[1] = propertyFloat[1];
                    node.position[2] = propertyFloat[2];
                    continue;
                }
                log("Value is not found (%s)", positionXYZ->name.c_str());
                log("\n");
                continue;
            }
        }
        checkClass(log, *positionXYZ, {}, 0);
    }

    // FFFFFFFF-00002003-00000000-0000900C Linear Rotation  LININTERP_ROTATION_CLASS_ID + ROTATION_SUPERCLASS_ID
    // ????????-00002012-00000000-0000900C Euler XYZ        HYBRIDINTERP_POINT4_CLASS_ID + ROTATION_SUPERCLASS_ID
    // FFFFFFFF-00442313-00000000-0000900C TCB Rotation     TCBINTERP_ROTATION_CLASS_ID + ROTATION_SUPERCLASS_ID
    for (uint32_t i = 0; i < 1; ++i)
    {
        xxMaxNode::Chunk const* rotationXYZ = getLinkChunk(scene, chunk, 1);
        if (rotationXYZ == nullptr)
            continue;
        auto& classData = rotationXYZ->classData;
        if (classData.superClassID == ROTATION_SUPERCLASS_ID)
        {
            if (classData.classID == HYBRIDINTERP_POINT4_CLASS_ID)
            {
                for (uint32_t i = 0; i < 3; ++i)
                {
                    xxMaxNode::Chunk const* rotation = getLinkChunk(scene, *rotationXYZ, i);
                    if (rotation == nullptr)
                        continue;
                    if (checkClass(log, *rotation, HYBRIDINTERP_FLOAT_CLASS_ID, FLOAT_SUPERCLASS_ID) == false)
                        continue;
                    xxMaxNode::Chunk const* chunk7127 = getChunk(*rotation, 0x7127);
                    if (chunk7127)
                        rotation = chunk7127;
                    std::vector<float> propertyFloat = getProperty<float>(*rotation, FLOAT_TYPE);
                    if (propertyFloat.size() >= 1)
                    {
                        node.rotation[i] = propertyFloat[0];
                        continue;
                    }
                    log("Value is not found (%s)", rotation->name.c_str());
                    log("\n");
                }
                eulerToQuaternion(node.rotation.data(), node.rotation.data());
                continue;
            }
            if (classData.classID == LININTERP_ROTATION_CLASS_ID ||
                classData.classID == TCBINTERP_ROTATION_CLASS_ID)
            {
                xxMaxNode::Chunk const* chunk7127 = getChunk(*rotationXYZ, 0x7127);
                if (chunk7127)
                    rotationXYZ = chunk7127;
                std::vector<float> propertyFloat = getProperty<float>(*rotationXYZ, FLOAT_TYPE);
                if (propertyFloat.size() >= 4)
                {
                    node.rotation[0] = propertyFloat[0];
                    node.rotation[1] = propertyFloat[1];
                    node.rotation[2] = propertyFloat[2];
                    node.rotation[3] = propertyFloat[3];
                    continue;
                }
                if (propertyFloat.size() >= 3)
                {
                    eulerToQuaternion(node.rotation.data(), propertyFloat.data());
                    continue;
                }
                log("Value is not found (%s)", rotationXYZ->name.c_str());
                log("\n");
                continue;
            }
        }
        checkClass(log, *rotationXYZ, {}, 0);
    }

    // FFFFFFFF-00002004-00000000-0000900D Linear Scale LININTERP_SCALE_CLASS_ID + SCALE_SUPERCLASS_ID
    // FFFFFFFF-00002010-00000000-0000900D Bezier Scale HYBRIDINTERP_SCALE_CLASS_ID + SCALE_SUPERCLASS_ID
    // FFFFFFFF-00442315-00000000-0000900D TCB Scale    TCBINTERP_SCALE_CLASS_ID + SCALE_SUPERCLASS_ID
    for (uint32_t i = 0; i < 1; ++i)
    {
        xxMaxNode::Chunk const* scale = getLinkChunk(scene, chunk, 2);
        if (scale == nullptr)
            continue;
        auto& classData = scale->classData;
        if (classData.superClassID == SCALE_SUPERCLASS_ID)
        {
            if (classData.classID == LININTERP_SCALE_CLASS_ID ||
                classData.classID == HYBRIDINTERP_SCALE_CLASS_ID ||
                classData.classID == TCBINTERP_SCALE_CLASS_ID)
            {
                xxMaxNode::Chunk const* chunk7127 = getChunk(*scale, 0x7127);
                if (chunk7127)
                    scale = chunk7127;
                std::vector<float> propertyFloat = getProperty<float>(*scale, FLOAT_TYPE);
                if (propertyFloat.size() >= 3)
                {
                    node.scale[0] = propertyFloat[0];
                    node.scale[1] = propertyFloat[1];
                    node.scale[2] = propertyFloat[2];
                    continue;
                }
                if (propertyFloat.size() >= 1)
                {
                    node.scale[0] = node.scale[1] = node.scale[2] = propertyFloat[0];
                    continue;
                }
                log("Value is not found (%s)", scale->name.c_str());
                log("\n");
                continue;
            }
        }
        checkClass(log, *scale, {}, 0);
    }
}

static void getPrimitive(int(*log)(char const*, ...), xxMaxNode::Chunk const& scene, xxMaxNode::Chunk const& chunk, xxMaxNode& node)
{
    if (chunk.classData.superClassID != GEOMOBJECT_SUPERCLASS_ID)
        return;
    xxMaxNode::Chunk const* pParamBlock = getLinkChunk(scene, chunk, 0);
    if (pParamBlock == nullptr)
        return;
    xxMaxNode::Chunk const& paramBlock = (*pParamBlock);

    // ????????-00000010-00000000-00000010 Box              BOXOBJ_CLASS_ID + GEOMOBJECT_SUPERCLASS_ID
    // ????????-00000011-00000000-00000010 Sphere           SPHERE_CLASS_ID + GEOMOBJECT_SUPERCLASS_ID
    // ????????-00000012-00000000-00000010 Cylinder         CYLINDER_CLASS_ID + GEOMOBJECT_SUPERCLASS_ID
    // ????????-00000020-00000000-00000010 Torus            TORUS_CLASS_ID + GEOMOBJECT_SUPERCLASS_ID
    // ????????-a86c23dd-00000000-00000010 Cone             CONE_CLASS_ID + GEOMOBJECT_SUPERCLASS_ID
    // ????????-00000000-00007f9e-00000010 GeoSphere        GSPHERE_CLASS_ID + GEOMOBJECT_SUPERCLASS_ID
    // ????????-00007b21-00000000-00000010 Tube             TUBE_CLASS_ID + GEOMOBJECT_SUPERCLASS_ID
    // ????????-76bf318a-4bf37b10-00000010 Pyramid          PYRAMID_CLASS_ID + GEOMOBJECT_SUPERCLASS_ID
    // ????????-081f1dfc-77566f65-00000010 Plane            PLANE_CLASS_ID + GEOMOBJECT_SUPERCLASS_ID
    // ????????-e44f10b3-00000000-00000010 Editable Mesh    EDITTRIOBJ_CLASS_ID + GEOMOBJECT_SUPERCLASS_ID
    // ????????-1bf8338d-192f6098-00000010 Editable Poly    EPOLYOBJ_CLASS_ID + GEOMOBJECT_SUPERCLASS_ID
    switch (class64(chunk.classData.classID))
    {
    case class64(BOXOBJ_CLASS_ID):
        if (paramBlock.size() > 4)
        {
            std::vector<float> propertyLength = getProperty<float>(paramBlock[2], 0x0100);
            std::vector<float> propertyWidth = getProperty<float>(paramBlock[3], 0x0100);
            std::vector<float> propertyHeight = getProperty<float>(paramBlock[4], 0x0100);
            if (propertyLength.empty() == false && propertyWidth.empty() == false && propertyHeight.empty() == false)
            {
                float length = propertyLength.front();
                float width = propertyWidth.front();
                float height = propertyHeight.front();

                node.vertices =
                {
                    { -length, -width, -height },
                    {  length, -width, -height },
                    { -length,  width, -height },
                    {  length,  width, -height },
                    { -length, -width,  height },
                    {  length, -width,  height },
                    { -length,  width,  height },
                    {  length,  width,  height },
                };

                node.text += format("Primitive : %s", "Box") + '\n';
                node.text += format("Length : %f", length) + '\n';
                node.text += format("Width : %f", width) + '\n';
                node.text += format("Height : %f", height) + '\n';
                return;
            }
        }
        checkClass(log, paramBlock, {}, 0);
        break;
    case class64(SPHERE_CLASS_ID):
        if (paramBlock.size() > 2)
        {
            std::vector<float> propertyRadius = getProperty<float>(paramBlock[2], 0x0100);
            if (propertyRadius.empty() == false)
            {
                float radius = propertyRadius.front();

                node.text += format("Primitive : %s", "Sphere") + '\n';
                node.text += format("Radius : %f", radius) + '\n';
                return;
            }
        }
        checkClass(log, paramBlock, {}, 0);
        break;
    case class64(CYLINDER_CLASS_ID):
        if (paramBlock.size() > 3)
        {
            std::vector<float> propertyRadius = getProperty<float>(paramBlock[2], 0x0100);
            std::vector<float> propertyHeight = getProperty<float>(paramBlock[3], 0x0100);
            if (propertyRadius.empty() == false && propertyHeight.empty() == false)
            {
                float radius = propertyRadius.front();
                float height = propertyHeight.front();

                node.text += format("Primitive : %s", "Cylinder") + '\n';
                node.text += format("Radius : %f", radius) + '\n';
                node.text += format("Height : %f", height) + '\n';
                return;
            }
        }
        checkClass(log, paramBlock, {}, 0);
        break;
    case class64(TORUS_CLASS_ID):
        if (paramBlock.size() > 3)
        {
            std::vector<float> propertyRadius1 = getProperty<float>(paramBlock[2], 0x0100);
            std::vector<float> propertyRadius2 = getProperty<float>(paramBlock[3], 0x0100);
            if (propertyRadius1.empty() == false && propertyRadius2.empty() == false)
            {
                float radius1 = propertyRadius1.front();
                float radius2 = propertyRadius2.front();

                node.text += format("Primitive : %s", "Torus") + '\n';
                node.text += format("Radius1 : %f", radius1) + '\n';
                node.text += format("Radius2 : %f", radius2) + '\n';
                return;
            }
        }
        checkClass(log, paramBlock, {}, 0);
        break;
    case class64(CONE_CLASS_ID):
        if (paramBlock.size() > 4)
        {
            std::vector<float> propertyRadius1 = getProperty<float>(paramBlock[2], 0x0100);
            std::vector<float> propertyRadius2 = getProperty<float>(paramBlock[3], 0x0100);
            std::vector<float> propertyHeight = getProperty<float>(paramBlock[4], 0x0100);
            if (propertyRadius1.empty() == false && propertyRadius2.empty() == false && propertyHeight.empty() == false)
            {
                float radius1 = propertyRadius1.front();
                float radius2 = propertyRadius2.front();
                float height = propertyHeight.front();

                node.text += format("Primitive : %s", "Cone") + '\n';
                node.text += format("Radius1 : %f", radius1) + '\n';
                node.text += format("Radius2 : %f", radius2) + '\n';
                node.text += format("Height : %f", height) + '\n';
                return;
            }
        }
        checkClass(log, paramBlock, {}, 0);
        break;
    case class64(GSPHERE_CLASS_ID):
        if (paramBlock.size() > 2)
        {
            std::vector<float> propertyRadius = getProperty<float>(paramBlock[2], 0x0100);
            if (propertyRadius.empty() == false)
            {
                float radius = propertyRadius.front();

                node.text += format("Primitive : %s", "GeoSphere") + '\n';
                node.text += format("Radius : %f", radius) + '\n';
                return;
            }
        }
        checkClass(log, paramBlock, {}, 0);
        break;
    case class64(TUBE_CLASS_ID):
        if (paramBlock.size() > 4)
        {
            std::vector<float> propertyRadius1 = getProperty<float>(paramBlock[2], 0x0100);
            std::vector<float> propertyRadius2 = getProperty<float>(paramBlock[3], 0x0100);
            std::vector<float> propertyHeight = getProperty<float>(paramBlock[4], 0x0100);
            if (propertyRadius1.empty() == false && propertyRadius2.empty() == false && propertyHeight.empty() == false)
            {
                float radius1 = propertyRadius1.front();
                float radius2 = propertyRadius2.front();
                float height = propertyHeight.front();

                node.text += format("Primitive : %s", "Tube") + '\n';
                node.text += format("Radius1 : %f", radius1) + '\n';
                node.text += format("Radius2 : %f", radius2) + '\n';
                node.text += format("Height : %f", height) + '\n';
                return;
            }
        }
        checkClass(log, paramBlock, {}, 0);
        break;
    case class64(PYRAMID_CLASS_ID):
        if (paramBlock.size() > 4)
        {
            std::vector<float> propertyWidth = getProperty<float>(paramBlock[2], 0x0100);
            std::vector<float> propertyDepth = getProperty<float>(paramBlock[3], 0x0100);
            std::vector<float> propertyHeight = getProperty<float>(paramBlock[4], 0x0100);
            if (propertyWidth.empty() == false && propertyDepth.empty() == false && propertyHeight.empty() == false)
            {
                float width = propertyWidth.front();
                float depth = propertyDepth.front();
                float height = propertyHeight.front();

                node.text += format("Primitive : %s", "Pyramid") + '\n';
                node.text += format("Width : %f", width) + '\n';
                node.text += format("Depth : %f", depth) + '\n';
                node.text += format("Height : %f", height) + '\n';
                return;
            }
        }
        checkClass(log, paramBlock, {}, 0);
        break;
    case class64(PLANE_CLASS_ID):
        if (paramBlock.size() > 3)
        {
            std::vector<float> propertyLength = getProperty<float>(paramBlock[2], 0x0100);
            std::vector<float> propertyWidth = getProperty<float>(paramBlock[3], 0x0100);
            if (propertyLength.empty() == false && propertyWidth.empty() == false)
            {
                float length = propertyLength.front();
                float width = propertyWidth.front();

                node.vertices =
                {
                    { -length, -width, 0 },
                    {  length, -width, 0 },
                    { -length,  width, 0 },
                    {  length,  width, 0 },
                };

                node.text += format("Primitive : %s", "Plane") + '\n';
                node.text += format("Length : %f", length) + '\n';
                node.text += format("Width : %f", width) + '\n';
                return;
            }
        }
        checkClass(log, paramBlock, {}, 0);
        break;
    case class64(EDITTRIOBJ_CLASS_ID):
        break;
    case class64(EPOLYOBJ_CLASS_ID):
        break;
    default:
        break;
    }
    checkClass(log, chunk, {}, 0);
}

xxMaxNode* xxMaxReader(char const* name, int(*log)(char const*, ...))
{
    FILE* file = fopen(name, "rb");
    if (file == nullptr)
    {
        log("File is not found", name);
        log("\n");
        return nullptr;
    }

    xxMaxNode* root = nullptr;

    TRY

    root = new xxMaxNode;
    if (root == nullptr)
    {
        log("Out of memory");
        log("\n");
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
        log("Scene is empty");
        log("\n");
        THROW;
    }
    auto& scene = root->scene->front();
    switch (scene.type)
    {
    case 0x200E:    // [x] 3ds Max 9
    case 0x200F:    // [x] 3ds Max 2008
    case 0x2020:    // [x] 3ds Max 2015
    case 0x2023:    // [x] 3ds Max 2018
        break;
    default:
        if (scene.type >= 0x2000)
            break;
        log("Scene type %04X is not supported", scene.type);
        log("\n");
        THROW;
    }

    // First Pass
    for (uint32_t i = 0; i < scene.size(); ++i)
    {
        auto& chunk = scene[i];
        auto [className, classData] = getClass(*root->classDirectory, chunk.type);
        if (className.empty())
        {
            log("Class %04X is not found! (Chunk:%d)", chunk.type, i);
            log("\n");
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
                log("Parent %d is not found! (Chunk:%d)", index, i);
                log("\n");
            }
        }

        // Name
        std::vector<uint16_t> propertyName = getProperty<uint16_t>(chunk, 0x0962);
        if (propertyName.empty() == false)
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
            case 0: getPositionRotationScale(log, scene, *linkChunk, node); break;
            case 1: getPrimitive(log, scene, *linkChunk, node);             break;
            }
        }

        // Text
        std::vector<uint16_t> propertyText = getProperty<uint16_t>(chunk, 0x0120);
        if (propertyText.empty() == false)
        {
            node.text = UTF16ToUTF8(propertyText.data(), propertyText.size());
        }

        // Attach
        parent->emplace_back(std::move(node));
        nodes[i] = &parent->back();
    }

    CATCH (std::exception const& e)
    {
#if _CPPUNWIND || __cpp_exceptions
        log("Exception : %s", e.what());
        log("\n");
#endif
        if (file)
        {
            fclose(file);
        }
        delete root;
        return nullptr;
    }

    return root;
}
