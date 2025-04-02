//==============================================================================
// miMAX : MaxReaderEntry Source
//
// Copyright (c) 2025 TAiGA
// https://github.com/metarutaiga/miMAX
//==============================================================================
#include "MaxReaderPCH.h"
#include "MaxReader.h"
#include "CFBReader.h"

#if _CPPUNWIND == 0 && __cpp_exceptions == 0
#if defined(xxWINDOWS)
#include <windows.h>
#endif
#define try         if (1) {
#define catch(x)    } else { std::exception e; std::string msg;
#define throw       }
#endif
#include <ImGuiFileDialog/ImGuiFileDialog.cpp>

#define PLUGIN_NAME     "MaxReader"
#define PLUGIN_MAJOR    1
#define PLUGIN_MINOR    0

//------------------------------------------------------------------------------
moduleAPI const char* Create(const CreateData& createData)
{
    MaxReader::Initialize();
    CFBReader::Initialize();
    return PLUGIN_NAME;
}
//------------------------------------------------------------------------------
moduleAPI void Shutdown(const ShutdownData& shutdownData)
{
    MaxReader::Shutdown();
    CFBReader::Shutdown();
}
//------------------------------------------------------------------------------
moduleAPI void Message(const MessageData& messageData)
{
    if (messageData.length == 1)
    {
        switch (xxHash(messageData.data[0]))
        {
        case xxHash("INIT"):
            break;
        case xxHash("SHUTDOWN"):
            break;
        default:
            break;
        }
    }
}
//------------------------------------------------------------------------------
moduleAPI bool Update(const UpdateData& updateData)
{
    static bool showAbout = false;
    static bool showMaxReader = false;
    static bool showCFBFinder = false;

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu(PLUGIN_NAME))
        {
            ImGui::MenuItem("Max Reader", nullptr, &showMaxReader);
            ImGui::MenuItem("CFB Finder", nullptr, &showCFBFinder);
            ImGui::Separator();
            ImGui::MenuItem("About " PLUGIN_NAME, nullptr, &showAbout);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (showAbout)
    {
        if (ImGui::Begin("About " PLUGIN_NAME, &showAbout, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
        {
            ImGui::Text("%s Plugin Version %d.%d", PLUGIN_NAME, PLUGIN_MAJOR, PLUGIN_MINOR);
            ImGui::Text("Build Date : %s %s", __DATE__, __TIME__);
            ImGui::Separator();
            ImGui::DumpBuildInformation();
        }
        ImGui::End();
    }

    bool updated = false;
    updated |= MaxReader::Update(updateData, showMaxReader);
    updated |= CFBReader::Update(updateData, showCFBFinder);

    return updated;
}
//------------------------------------------------------------------------------
moduleAPI void Render(const RenderData& renderData)
{

}
//------------------------------------------------------------------------------
