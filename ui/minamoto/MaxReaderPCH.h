//==============================================================================
// xxMaxReader : MaxReaderPCH Header
//
// Copyright (c) 2025 TAiGA
// https://github.com/metarutaiga/minamoto
//==============================================================================
#pragma once

#include <xxGraphic/xxGraphic.h>
#include <Runtime/Runtime.h>

#ifdef _WIN32
#undef IMGUI_API
#define IMGUI_API __declspec(dllimport)
#endif

#include <Interface.h>
#include <Editor/ImGuiHelper.h>

#ifdef _WIN32
#undef IMGUI_API
#define IMGUI_API
#endif
