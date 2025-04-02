//==============================================================================
// xxMaxReader : MaxReader Source
//
// Copyright (c) 2025 TAiGA
// https://github.com/metarutaiga/xxmaxreader
//==============================================================================
#include "MaxReaderPCH.h"
#include <xxGraphicPlus/xxFile.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <IconFontCppHeaders/IconsFontAwesome4.h>
#include "miMAX.h"
#include "MaxReader.h"

static std::string path;
static std::string info;
static ImGuiFileDialog* fileDialog;
//------------------------------------------------------------------------------
static miMaxNode* root;
//------------------------------------------------------------------------------
static int MaxReaderLog(bool breakline, char const* format, ...)
{
    va_list args;

    va_start(args, format);
    size_t length = vsnprintf(nullptr, 0, format, args) + 1;
    va_end(args);

    size_t pos = info.size();
    info.resize(info.size() + length);

    va_start(args, format);
    int result = vsnprintf(info.data() + pos, length, format, args);
    info.pop_back();
    va_end(args);

    if (breakline)
    {
        info += '\n';
    }

    return result;
}
//------------------------------------------------------------------------------
static bool ChunkFinder(miMaxNode::Chunk& chunk, std::function<void(uint16_t type, std::vector<char> const& property)> select)
{
    static void* selected;
    bool updated = false;

    // Chunk
    if (selected)
    {
        intptr_t delta = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) delta = -1;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) delta = 1;
        if (delta != 0)
        {
            size_t index = std::distance(chunk.data(), (miMaxNode::Chunk::value_type*)selected) + delta;
            if (index < chunk.size())
            {
                auto& child = chunk[index];
                select(child.type, child.property);
                selected = &child;
                updated = true;
            }
        }
    }
    for (size_t i = 0; i < chunk.size(); ++i)
    {
        auto& child = chunk[i];
        auto& flags = child.padding;

        char text[128];
        if (child.empty())
        {
            snprintf(text, 128, "%s%zX:%s", ICON_FA_FILE_TEXT, i, child.name.c_str());
        }
        else
        {
            snprintf(text, 128, "%s%zX:%s", (flags & 1) ? ICON_FA_CIRCLE_O : ICON_FA_CIRCLE, i, child.name.c_str());
        }

        ImGui::PushID(&child);
        ImGui::Selectable(text, selected == &child);
        ImGui::PopID();
        if (ImGui::IsItemHovered())
        {
            if (child.classDllName.empty() == false)
            {
                ImGui::BeginTooltip();
                ImGui::Text("Index:%zX", i);
                ImGui::Text("Type:%04X", child.type);
                ImGui::Text("Class:%08X-%08X-%08X-%08X", child.classData.dllIndex, child.classData.classID.first, child.classData.classID.second, child.classData.superClassID);
                ImGui::Text("DllFile:%s", child.classDllFile.c_str());
                ImGui::Text("DllName:%s", child.classDllName.c_str());
                ImGui::Text("Name:%s", child.name.c_str());
                if (child.empty())
                {
                    ImGui::Text("Size:%zd", child.property.size());
                }
                else
                {
                    size_t size = 0;
                    std::function<void(miMaxNode::Chunk const&)> traversal = [&](miMaxNode::Chunk const& chunk)
                    {
                        size += chunk.property.size();
                        for (auto const& child : chunk)
                            traversal(child);
                    };
                    traversal(child);
                    ImGui::Text("Size:%zd", size);
                }
                ImGui::EndTooltip();
            }
            else
            {
                ImGui::BeginTooltip();
                if (child.empty())
                {
                    ImGui::Text("Size:%zd", child.property.size());
                }
                else
                {
                    size_t size = 0;
                    std::function<void(miMaxNode::Chunk const&)> traversal = [&](miMaxNode::Chunk const& chunk)
                    {
                        size += chunk.property.size();
                        for (auto const& child : chunk)
                            traversal(child);
                    };
                    traversal(child);
                    ImGui::Text("Size:%zd", size);
                }
                ImGui::EndTooltip();
            }
            if (ImGui::IsItemClicked() && selected != &child)
            {
                select(child.type, child.property);
                selected = &child;
                updated = true;
            }
            if (child.empty() == false && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                flags ^= 1;
            }
        }
        if (child.empty() == false && (flags & 1))
        {
            ImGui::Indent();
            updated |= ChunkFinder(child, select);
            ImGui::Unindent();
        }
    }

    return updated;
}
//------------------------------------------------------------------------------
static bool NodeFinder(miMaxNode& node, std::function<void(std::string& text)> select)
{
    static void* selected;
    bool updated = false;

    if (selected)
    {
        intptr_t delta = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) delta = -1;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) delta = 1;
        if (delta != 0)
        {
            for (auto it = node.begin(); it != node.end(); ++it)
            {
                if (&(*it) != selected)
                    continue;
                auto previous = it;
                if (previous != node.begin())
                    previous--;
                if (delta < 0)
                {
                    select(previous->text);
                    selected = &(*previous);
                    updated = true;
                    break;
                }
                auto next = it;
                if (next != node.end())
                    next++;
                if (next != node.end())
                {
                    select(next->text);
                    selected = &(*next);
                    updated = true;
                    break;
                }
            }
        }
    }
    for (auto& child : node)
    {
        auto& flags = child.padding;

        char text[128];
        snprintf(text, 128, "%s%s", (flags & 1) ? ICON_FA_CIRCLE_O : ICON_FA_CIRCLE, child.name.c_str());

        ImGui::PushID(&child);
        ImGui::Selectable(text, selected == &child);
        ImGui::PopID();
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("Name:%s", child.name.c_str());
            ImGui::Text("Position:%g, %g, %g", child.position[0], child.position[1], child.position[2]);
            ImGui::Text("Rotation:%g, %g, %g, %g", child.rotation[0], child.rotation[1], child.rotation[2], child.rotation[3]);
            ImGui::Text("Scale:%g, %g, %g", child.scale[0], child.scale[1], child.scale[2]);
            if (child.vertex.empty() == false)
            {
                ImGui::Separator();
                ImGui::Text("Vertex : %zd", child.vertex.size());
                ImGui::Text("Texture : %zd", child.texture.size());
                ImGui::Text("Normal : %zd", child.normal.size());
                ImGui::Text("Vertex Color : %zd", child.vertexColor.size());
//              ImGui::Text("Vertex Illum : %zd", child.vertexIllum.size());
                ImGui::Text("Vertex Alpha : %zd", child.vertexAlpha.size());
                ImGui::Text("Vertex Array : %zd", child.vertexArray.size());
                ImGui::Text("Texture Array : %zd", child.textureArray.size());
//              ImGui::Text("Polygon Array : %zd", child.polygonArray.size());
            }
            ImGui::EndTooltip();

            if (ImGui::IsItemClicked() && selected != &child)
            {
                select(child.text);
                selected = &child;
            }
            if (child.empty() == false && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                flags ^= 1;
            }
        }
        if (child.empty() == false && (flags & 1))
        {
            ImGui::Indent();
            updated |= NodeFinder(child, select);
            ImGui::Unindent();
        }
    }

    return updated;
}
//------------------------------------------------------------------------------
void MaxReader::Initialize()
{
#if defined(_WIN32)
    path = std::string(xxGetDocumentPath()) + "\\Project\\";
#else
    path = std::string(xxGetDocumentPath()) + "/Project/";
#endif
    fileDialog = new ImGuiFileDialog;
    root = nullptr;
}
//------------------------------------------------------------------------------
void MaxReader::Shutdown()
{
    delete fileDialog;
    delete root;
}
//------------------------------------------------------------------------------
bool MaxReader::Update(const UpdateData& updateData, bool& show)
{
    if (show == false)
        return false;

    bool updated = false;
    ImGui::SetNextWindowSize(ImVec2(1280.0f, 768.0f), ImGuiCond_Appearing);
    if (ImGui::Begin("Max Reader", &show, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    {
        ImGui::InputTextEx("PATH", nullptr, path);
        ImGui::SameLine();
        if (ImGui::Button("..."))
        {
            IGFD::FileDialogConfig config = { path };
#if defined(_WIN32)
            if (config.path.size() && config.path.back() != '\\')
                config.path.resize(config.path.rfind('\\') + 1);
#else
            if (config.path.size() && config.path.back() != '/')
                config.path.resize(config.path.rfind('/') + 1);
#endif
            fileDialog->OpenDialog("MaxReader", "Choose File", "All Files(*.*){.*}", config);
        }

        static int tabIndex;
        static std::vector<char> fileContent;
        static size_t fileContentIndex;

        ImGui::BeginTabBar("Type");
        if (ImGui::BeginTabItem("ClassData",      nullptr, 0)) { tabIndex = 0; ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("ClassDirectory", nullptr, 0)) { tabIndex = 1; ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Config",         nullptr, 0)) { tabIndex = 2; ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("DllDirectory",   nullptr, 0)) { tabIndex = 3; ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Scene",          nullptr, 0)) { tabIndex = 4; ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("VideoPostQueue", nullptr, 0)) { tabIndex = 5; ImGui::EndTabItem(); }
        ImGui::EndTabBar();

        ImGui::Columns(2);
        if (ImGui::BeginChild("ChunkFinder", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 16)))
        {
            if (root)
            {
                miMaxNode::Chunk* chunk = nullptr;
                switch (tabIndex)
                {
                    case 0: chunk = root->classData;        break;
                    case 1: chunk = root->classDirectory;   break;
                    case 2: chunk = root->config;           break;
                    case 3: chunk = root->dllDirectory;     break;
                    case 4: chunk = root->scene;            break;
                    case 5: chunk = root->videoPostQueue;   break;
                }
                if (chunk)
                {
                    updated |= ChunkFinder(*chunk, [](uint16_t type, std::vector<char> const& data)
                    {
                        fileContent = data;
                        fileContentIndex = 0;
                    });
                }
            }
            ImGui::EndChild();
        }
        ImGui::NextColumn();
        for (size_t i = fileContentIndex, size = fileContent.size(); i < fileContentIndex + 256; i += 16)
        {
            size_t count;
            if (i >= size)
                count = 0;
            else if ((i + 16) > size)
                count = fileContent.size() % 16;
            else
                count = 16;

            uint8_t pitch[16] = {};
            memcpy(pitch, fileContent.data() + i, count);

            char line[64];
            for (size_t i = 0; i < 16; ++i)
            {
                snprintf(line, 64, "%s%s", line, i < count && i && i % 4 == 0 ? "-" : " ");
                snprintf(line + i * 3, 64 - i * 3, i < count ? "%02x" : "  ", pitch[i]);
            }
            ImGui::TextUnformatted(line);
            ImGui::SameLine();
            for (uint8_t& c : pitch)
            {
                if (c == 0 || c == '\t' || c == '\n')
                    c = ' ';
            }
            ImGui::Text("\t%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
                        pitch[0], pitch[1], pitch[2], pitch[3],
                        pitch[4], pitch[5], pitch[6], pitch[7],
                        pitch[8], pitch[9], pitch[10], pitch[11],
                        pitch[12], pitch[13], pitch[14], pitch[15]);
        }
        if (ImGui::IsWindowHovered())
        {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel > 0.0f && fileContentIndex - 16 < fileContentIndex)
                fileContentIndex -= 16;
            if (wheel < 0.0f && fileContentIndex + 16 < fileContent.size())
                fileContentIndex += 16;
            if (ImGui::IsKeyReleased(ImGuiKey_PageUp) && fileContentIndex - 256 < fileContentIndex)
                fileContentIndex -= 256;
            if (ImGui::IsKeyReleased(ImGuiKey_PageDown) && fileContentIndex + 256 < fileContent.size())
                fileContentIndex += 256;
        }
        ImGui::Columns(1);

        ImGui::Separator();

        static std::string nodeText;

        ImGui::Columns(2);
        if (ImGui::BeginChild("NodeFinder", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 16)))
        {
            if (root)
            {
                updated |= NodeFinder(*root, [](std::string const& text)
                {
                    nodeText = text;
                });
            }
            ImGui::EndChild();
        }
        ImGui::NextColumn();
        if (ImGui::BeginChild("NodeText", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 16)))
        {
            ImGui::TextUnformatted(nodeText.c_str());
            ImGui::EndChild();
        }
        ImGui::Columns(1);

        ImGui::Separator();
        ImGui::InputTextMultiline("INFO", info, ImVec2(0, 0), ImGuiInputTextFlags_ReadOnly);
    }
    ImGui::End();

    if (fileDialog->Display("MaxReader", 0, ImVec2(512, 384)))
    {
        if (fileDialog->IsOk())
        {
            delete root;
            root = nullptr;
            info.clear();

            path = fileDialog->GetFilePathName();
            root = miMAXOpenFile(path.c_str(), MaxReaderLog);
        }
        fileDialog->Close();
    }

    return updated;
}
//------------------------------------------------------------------------------
