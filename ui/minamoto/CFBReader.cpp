//==============================================================================
// xxMaxReader : CFBReader Source
//
// Copyright (c) 2025 TAiGA
// https://github.com/metarutaiga/xxmaxreader
//==============================================================================
#include "MaxReaderPCH.h"
#include <xxGraphicPlus/xxFile.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <IconFontCppHeaders/IconsFontAwesome4.h>
#include "compoundfilereader/src/include/compoundfilereader.h"
#include "utf.h"
#include "CFBReader.h"

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
static std::vector<char> fileContent;
static size_t fileContentIndex;
//------------------------------------------------------------------------------
struct Node : public std::vector<std::tuple<size_t, std::string, size_t, Node>> {};
static Node root;
static Node* selected;
//------------------------------------------------------------------------------
static void Finder(Node& node, std::function<void(size_t size, std::string const& name, size_t entryID)> select)
{
    static std::vector<std::string> stacked;
    auto SubFolder = [](std::string const& name)
    {
        std::string path;
        for (auto const& name : stacked)
        {
            path += name.c_str() + sizeof(ICON_FA_FOLDER) - 1;
            path += '/';
        }
        path += name.c_str() + sizeof(ICON_FA_FOLDER) - 1;
        path += '/';
        return path;
    };

    for (auto& [entryID, name, size, object] : node)
    {
        bool open = name.compare(0, sizeof(ICON_FA_FOLDER_OPEN) - 1, ICON_FA_FOLDER_OPEN) == 0;
        ImGui::Selectable(name.c_str(), selected == &object);
        if (ImGui::IsItemHovered())
        {
            bool folder = open;
            if (folder == false && name.compare(0, sizeof(ICON_FA_FOLDER) - 1, ICON_FA_FOLDER) == 0)
            {
                folder = true;
            }
            if (ImGui::IsItemClicked() && selected != &object)
            {
                selected = &object;
                if (folder == false)
                {
                    select(entryID, SubFolder(name), size);
                }
            }
            if (folder && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                if (open)
                {
                    name.replace(0, sizeof(ICON_FA_FOLDER) - 1, ICON_FA_FOLDER);
                }
                else
                {
                    name.replace(0, sizeof(ICON_FA_FOLDER_OPEN) - 1, ICON_FA_FOLDER_OPEN);
                }
            }
        }
        if (open)
        {
            ImGui::Indent();
            stacked.push_back(name);
            Finder(object, select);
            stacked.pop_back();
            ImGui::Unindent();
        }
    }
}
//------------------------------------------------------------------------------
static std::pair<CFB::CompoundFileReader, std::vector<char>>* getCFB(std::string const& name)
{
    xxFile* file = xxFile::Load(name.c_str());
    if (file)
    {
        std::vector<char> buffer(file->Size());
        file->Read(buffer.data(), buffer.size());
        delete file;

        try { return new std::pair{ CFB::CompoundFileReader(buffer.data(), buffer.size()), std::move(buffer) }; } catch (...) {}
    }

    return nullptr;
}
//------------------------------------------------------------------------------
void CFBReader::Initialize()
{
#if defined(_WIN32)
    path = std::string(xxGetDocumentPath()) + "\\Project\\";
#else
    path = std::string(xxGetDocumentPath()) + "/Project/";
#endif
#if HAVE_FILEDIALOG
    fileDialog = new ImGuiFileDialog;
#endif
}
//------------------------------------------------------------------------------
void CFBReader::Shutdown()
{
#if HAVE_FILEDIALOG
    delete fileDialog;
#endif
    root.clear();
}
//------------------------------------------------------------------------------
bool CFBReader::Update(const UpdateData& updateData, bool& show)
{
    if (show == false)
        return false;

    ImGui::SetNextWindowSize(ImVec2(1200.0f, 720.0f), ImGuiCond_Appearing);
    if (ImGui::Begin("CFB Finder", &show, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
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

        ImGui::Columns(2);
        Finder(root, [](size_t entryID, std::string const& name, size_t size)
        {
            std::pair<CFB::CompoundFileReader, std::vector<char>>* cfb = getCFB(fileDialog->GetFilePathName());
            if (cfb)
            {
                auto& reader = cfb->first;
                auto* entry = reader.GetEntry(entryID);
                if (entry)
                {
                    fileContent.resize(size);
                    try { reader.ReadFile(entry, 0, fileContent.data(), fileContent.size()); } catch (...) {}
                }
                delete cfb;
            }
            fileContentIndex = 0;
        });
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

        ImGui::InputTextMultiline("INFO", info, ImVec2(0, 0), ImGuiInputTextFlags_ReadOnly);
    }
    ImGui::End();

#if HAVE_FILEDIALOG
    if (fileDialog->Display("Reader", 0, ImVec2(512, 384)))
    {
        if (fileDialog->IsOk())
        {
            path = fileDialog->GetFilePathName();

            std::pair<CFB::CompoundFileReader, std::vector<char>>* cfb = getCFB(path);
            if (cfb)
            {
                auto& reader = cfb->first;
                const CFB::COMPOUND_FILE_HDR* hdr = reader.GetFileInfo();

                info.clear();
                info += std::format("file version: {}.{}\n", hdr->majorVersion, hdr->minorVersion);
                info += std::format("difat sector: {}\n", hdr->numDIFATSector);
                info += std::format("directory sector: {}\n", hdr->numDirectorySector);
                info += std::format("fat sector: {}\n", hdr->numFATSector);
                info += std::format("mini fat sector: {}\n", hdr->numMiniFATSector);
                info += '\n';

                auto rootEntry = reader.GetRootEntry();
                std::map<CFB::COMPOUND_FILE_ENTRY const*, size_t> mappedEntry;
                mappedEntry[reader.GetEntry(rootEntry->leftSiblingID)] = rootEntry->leftSiblingID;
                mappedEntry[reader.GetEntry(rootEntry->rightSiblingID)] = rootEntry->rightSiblingID;
                mappedEntry[reader.GetEntry(rootEntry->childID)] = rootEntry->childID;

                root.clear();
                reader.EnumFiles(rootEntry, -1, [&](CFB::COMPOUND_FILE_ENTRY const* entry, CFB::utf16string const& dir, int level)
                {
                    size_t entryID = mappedEntry[entry];
                    char const* type;
                    switch (entry->type) {
                    case 0x01:
                        type = ICON_FA_FOLDER;
                        break;
                    case 0x02:
                        type = ICON_FA_FILE;
                        break;
                    default:
                        type = ICON_FA_QUESTION;
                        break;
                    }
                    root.emplace_back(entryID, type + UTF16ToUTF8(entry->name), entry->size, Node());
                    mappedEntry[reader.GetEntry(entry->leftSiblingID)] = entry->leftSiblingID;
                    mappedEntry[reader.GetEntry(entry->rightSiblingID)] = entry->rightSiblingID;
                    mappedEntry[reader.GetEntry(entry->childID)] = entry->childID;
                });

                delete cfb;
            }
        }
        fileDialog->Close();
    }
#endif

    return false;
}
//------------------------------------------------------------------------------
