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
#include "xxMaxReader.h"
#include "MaxReader.h"

#if defined(__APPLE__)
#define HAVE_FILEDIALOG 1
#elif defined(_WIN32) && !defined(__clang__)
#define HAVE_FILEDIALOG 1
#else
#define HAVE_FILEDIALOG 0
#endif

static std::string path;
static std::string info;
static ImGuiFileDialog* fileDialog;
//------------------------------------------------------------------------------
static xxMaxNode* root;
//------------------------------------------------------------------------------
static bool BlockFinder(xxMaxNode::Block& block, std::function<void(uint16_t type, std::vector<char> const& property)> select)
{
    static void* selected;
    bool updated = false;

    // Children
    if (selected)
    {
        ssize_t delta = 0;
        if (ImGui::IsKeyReleased(ImGuiKey_UpArrow)) delta = -1;
        if (ImGui::IsKeyReleased(ImGuiKey_DownArrow)) delta = 1;
        if (delta != 0)
        {
            size_t index = std::distance(&block.front(), (xxMaxNode::Block::value_type*)selected) + delta;
            if (index < block.size())
            {
                auto& [type, _2, flags, child] = block[index];
                selected = &type;
                updated = true;
            }
        }
    }
    for (size_t i = 0; i < block.size(); ++i)
    {
        auto& [type, name, child, flags] = block[i];

        char text[128];
        snprintf(text, 128, "%s%zd:%s", (flags & 1) ? ICON_FA_CIRCLE_O : ICON_FA_CIRCLE, i, name.c_str());

        ImGui::PushID(&type);
        ImGui::Selectable(text, selected == &type);
        ImGui::PopID();
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Index:%zd\n"
                              "Type:%04X\n"
                              "Name:%s",
                              i, type, name.c_str());

            if (ImGui::IsItemClicked() && selected != &type)
            {
                selected = &type;
            }
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                flags ^= 1;
            }
        }
        if (flags & 1)
        {
            ImGui::Indent();
            updated |= BlockFinder(child, select);
            ImGui::Unindent();
        }
    }

    // Property
    if (selected)
    {
        ssize_t delta = 0;
        if (ImGui::IsKeyReleased(ImGuiKey_UpArrow)) delta = -1;
        if (ImGui::IsKeyReleased(ImGuiKey_DownArrow)) delta = 1;
        if (delta != 0)
        {
            size_t index = std::distance(&block.properties.front(), (xxMaxNode::Block::Properties::value_type*)selected) + delta;
            if (index < block.properties.size())
            {
                auto& [type, _2, child] = block.properties[index];
                selected = &type;
                select(type, child);
                updated = true;
            }
        }
    }
    for (size_t i = 0; i < block.properties.size(); ++i)
    {
        auto& [type, name, property] = block.properties[i];

        char text[128];
        snprintf(text, 128, "%s%zd:%s", ICON_FA_FILE_TEXT, i, name.c_str());

        ImGui::PushID(&type);
        ImGui::Selectable(text, selected == &type);
        ImGui::PopID();
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Index:%zd\n"
                              "Type:%04X\n"
                              "Name:%s\n"
                              "Size:%zd",
                              i, type, name.c_str(), property.size());

            if (ImGui::IsItemClicked() && selected != &type)
            {
                selected = &type;
                select(type, property);
            }
        }
    }

    return updated;
}
//------------------------------------------------------------------------------
static bool NodeFinder(xxMaxNode& node)
{
    static void* selected;
    bool updated = false;

    // Children
    if (selected)
    {
        //ssize_t delta = 0;
        //if (ImGui::IsKeyReleased(ImGuiKey_UpArrow)) delta = -1;
        //if (ImGui::IsKeyReleased(ImGuiKey_DownArrow)) delta = 1;
        //if (delta != 0)
        //{
        //    size_t index = std::distance(&node.front(), (xxMaxNode::value_type*)selected) + delta;
        //    if (index < node.size())
        //    {
        //        selected = &(node.begin() + index);
        //        updated = true;
        //    }
        //}
    }
    for (auto& child : node)
    {
        char text[128];
        snprintf(text, 128, "%s%s", child.dummy ? ICON_FA_CIRCLE_O : ICON_FA_CIRCLE, child.name.c_str());

        ImGui::PushID(&child);
        ImGui::Selectable(text, selected == &child);
        ImGui::PopID();
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Name:%s\n"
                              "Position:%g, %g, %g\n"
                              "Rotation:%g, %g, %g\n"
                              "Scale:%g, %g, %g\n",
                              child.name.c_str(),
                              child.position[0], child.position[1], child.position[2],
                              child.rotation[0], child.rotation[1], child.rotation[2],
                              child.scale[0], child.scale[1], child.scale[2]);

            if (ImGui::IsItemClicked() && selected != &child)
            {
                selected = &child;
            }
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                child.dummy = !child.dummy;
            }
        }
        if (child.dummy)
        {
            ImGui::Indent();
            updated |= NodeFinder(child);
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
#if HAVE_FILEDIALOG
    fileDialog = new ImGuiFileDialog;
#endif
    root = nullptr;
}
//------------------------------------------------------------------------------
void MaxReader::Shutdown()
{
#if HAVE_FILEDIALOG
    delete fileDialog;
#endif
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
#if HAVE_FILEDIALOG
            IGFD::FileDialogConfig config = { path };
#if defined(_WIN32)
            if (config.path.size() && config.path.back() != '\\')
                config.path.resize(config.path.rfind('\\') + 1);
#else
            if (config.path.size() && config.path.back() != '/')
                config.path.resize(config.path.rfind('/') + 1);
#endif
            fileDialog->OpenDialog("Reader", "Choose File", "All Files(*.*){.*}", config);
#endif
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
        if (ImGui::BeginChild("BlockFinder", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 16)))
        {
            if (root)
            {
                xxMaxNode::Block* block = nullptr;
                switch (tabIndex)
                {
                    case 0: block = root->classData;        break;
                    case 1: block = root->classDirectory;   break;
                    case 2: block = root->config;           break;
                    case 3: block = root->dllDirectory;     break;
                    case 4: block = root->scene;            break;
                    case 5: block = root->videoPostQueue;   break;
                }
                if (block)
                {
                    updated |= BlockFinder(*block, [](uint16_t type, std::vector<char> const& data)
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
                snprintf(line + i * 3, 64 - i * 3, i < count ? "%02x " : "   ", pitch[i]);
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
        if (ImGui::BeginChild("NodeFinder", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 16)))
        {
            if (root)
            {
                updated |= NodeFinder(*root);
            }
            ImGui::EndChild();
        }

        ImGui::Separator();
        ImGui::InputTextMultiline("INFO", info, ImVec2(0, 0), ImGuiInputTextFlags_ReadOnly);
    }
    ImGui::End();

#if HAVE_FILEDIALOG
    if (fileDialog->Display("Reader", 0, ImVec2(512, 384)))
    {
        if (fileDialog->IsOk())
        {
            delete root;
            root = nullptr;

            path = fileDialog->GetFilePathName();
            root = xxMaxReader(path.c_str());
        }
        fileDialog->Close();
    }
#endif

    return updated;
}
//------------------------------------------------------------------------------
