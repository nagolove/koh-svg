// vim: set colorcolumn=85
// vim: fdm=marker
#include "koh_imgui.h"

#include "cimgui.h"

static const char *map_TreeNodeFlags[] = {
    // Imgui tree flags selector {{{

    [ImGuiTreeNodeFlags_None] = "ImGuiTreeNodeFlags_None",
    [ImGuiTreeNodeFlags_Selected] = "ImGuiTreeNodeFlags_Selected",
    [ImGuiTreeNodeFlags_Framed] = "ImGuiTreeNodeFlags_Framed",
    [ImGuiTreeNodeFlags_AllowOverlap] = "ImGuiTreeNodeFlags_AllowOverlap",
    [ImGuiTreeNodeFlags_NoTreePushOnOpen] = "ImGuiTreeNodeFlags_NoTreePushOnOpen",
    [ImGuiTreeNodeFlags_NoAutoOpenOnLog] = "ImGuiTreeNodeFlags_NoAutoOpenOnLog",
    [ImGuiTreeNodeFlags_DefaultOpen] = "ImGuiTreeNodeFlags_DefaultOpen",
    [ImGuiTreeNodeFlags_OpenOnDoubleClick] = "ImGuiTreeNodeFlags_OpenOnDoubleClick",
    [ImGuiTreeNodeFlags_OpenOnArrow] = "ImGuiTreeNodeFlags_OpenOnArrow",
    [ImGuiTreeNodeFlags_Leaf] = "ImGuiTreeNodeFlags_Leaf",
    [ImGuiTreeNodeFlags_Bullet] = "ImGuiTreeNodeFlags_Bullet",
    [ImGuiTreeNodeFlags_FramePadding] = "ImGuiTreeNodeFlags_FramePadding",
    [ImGuiTreeNodeFlags_SpanAvailWidth] = "ImGuiTreeNodeFlags_SpanAvailWidth",
    [ImGuiTreeNodeFlags_SpanFullWidth] = "ImGuiTreeNodeFlags_SpanFullWidth",
    [ImGuiTreeNodeFlags_SpanAllColumns] = "ImGuiTreeNodeFlags_SpanAllColumns",
    [ImGuiTreeNodeFlags_NavLeftJumpsBackHere] = "ImGuiTreeNodeFlags_NavLeftJumpsBackHere",
    [ImGuiTreeNodeFlags_CollapsingHeader] = "ImGuiTreeNodeFlags_CollapsingHeader",

    // }}}
};

static int num_TreeNodeFlags = sizeof(map_TreeNodeFlags) / sizeof(map_TreeNodeFlags[0]);

const char *koh_getter_TreeNodeFlags(void *data, int n) {
    return map_TreeNodeFlags[n];
}

