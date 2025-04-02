//==============================================================================
// miMAX : MaxReader Header
//
// Copyright (c) 2025 TAiGA
// https://github.com/metarutaiga/miMAX
//==============================================================================
#pragma once

struct MaxReader
{
    static void Initialize();
    static void Shutdown();
    static bool Update(const UpdateData& updateData, bool& show);
};
