// vim: set colorcolumn=85
// vim: fdm=marker
#include "stage_shot.h"

// includes {{{
#include "cimgui.h"
#include "koh_common.h"
#include "koh_resource.h"
#include "koh_hotkey.h"
#include "koh_stages.h"
#include "raylib.h"
#include <assert.h>
#include <stdlib.h>
#include "koh_common.h"
// }}}

#define MAX_SHAPES_UNDER_POINT  100

typedef struct Stage_shot {
    Stage             parent;
    Camera2D          cam;
    FilesSearchSetup  fssetup;
    FilesSearchResult fsr;
    Resource          res_list;
    ResourceAsyncLoader *loader;
} Stage_shot;

static void stage_shot_init(struct Stage_shot *st) {
    trace("stage_shot_init:\n");
    st->cam.zoom = 1.;

    st->fssetup.deep = -1;
    st->fssetup.path = "/home/nagolove/shots/";
    st->fssetup.regex_pattern = ".*\\.png$";
    st->fsr = koh_search_files(&st->fssetup);

    st->loader = res_async_loader_new(NULL);
}

static void stage_shot_update(struct Stage_shot *st) {
    res_async_loader_pump(st->loader, &st->res_list);

    koh_camera_process_mouse_drag(&(struct CameraProcessDrag) {
            .mouse_btn = MOUSE_BUTTON_RIGHT,
            .cam = &st->cam
    });
    koh_camera_process_mouse_scale_wheel(&(struct CameraProcessScale) {
        //.mouse_btn = MOUSE_BUTTON_LEFT,
        .cam = &st->cam,
        .dscale_value = 0.05,
        .modifier_key_down = KEY_LEFT_SHIFT,
    });

}

static void stage_shot_gui(struct Stage_shot *st) {
    assert(st);

    ImGuiTableFlags table_flags = 
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV |
        ImGuiTableFlags_ContextMenuInBody |
        ImGuiTableFlags_ScrollY;

    ImVec2 outer_size = {0., 0.};

    /*
    if (igBeginTable("assets", 2, table_flags, outer_size, 0.)) {

        igTableSetupColumn("file name", 0, 0, 0);
        igTableSetupColumn("preview", 0, 0, 1);
        igTableHeadersRow();

        for (int i = 0; i < st->fsr_images.num; i++) {
            ImGuiTableFlags row_flags = 0;
            igTableNextRow(row_flags, 0);

            igTableSetColumnIndex(0);
            if (igSelectable_BoolPtr(
                st->fsr_images.names[i], &st->images_selected[i], 
                ImGuiSelectableFlags_SpanAllColumns, (ImVec2){0, 0}
            )) {
                for (int j = 0; j < st->fsr_images.num; j++)
                    if (j != i)
                        st->images_selected[j] = false;

                st->last_selected = LAST_SELECTED_IMAGE;
                trace("gui_images_table: trash_menu\n");
                //if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                    //igOpenPopup_Str("trash_menu", ImGuiPopupFlags_MouseButtonRight);
                    igOpenPopup_Str("trash_menu", 0);
                //}

                if (igBeginPopup("trash_menu", 0)) {
                    trace("gui_images_table: popup stuff here\n");
                    static bool move2trash = true;
                    if (igSelectable_Bool(
                        "move file to trash", move2trash, 
                        0,
                        (ImVec2){})) {
                        trace(
                            "gui_images_table: move file to trash\n"
                        );
                    }
                    igEndPopup();
                }

            }

            igTableSetColumnIndex(1);
            rlImGuiImage(&st->textures[i]);
        }

        igEndTable();
    }
    */
}

static void stage_shot_enter(struct Stage_shot *st) {
    trace("stage_shot_enter:\n");
}

static void stage_shot_leave(struct Stage_shot *st) {
    trace("stage_shot_leave:\n");
}

static void stage_shot_draw(struct Stage_shot *st) {
}

static void stage_shot_shutdown(struct Stage_shot *st) {
    trace("stage_shot_shutdown:\n");
    koh_search_files_shutdown(&st->fsr);
    res_async_loader_free(st->loader);
}

Stage *stage_shot_new(HotkeyStorage *hk_store) {
    assert(hk_store);
    struct Stage_shot *st = calloc(1, sizeof(*st));
    st->parent.data = hk_store;

    st->parent.init = (Stage_callback)stage_shot_init;
    st->parent.enter = (Stage_callback)stage_shot_enter;
    st->parent.leave = (Stage_callback)stage_shot_leave;

    st->parent.gui = (Stage_callback)stage_shot_gui;
    st->parent.update = (Stage_callback)stage_shot_update;
    st->parent.draw = (Stage_callback)stage_shot_draw;
    st->parent.shutdown = (Stage_callback)stage_shot_shutdown;

    return (Stage*)st;
}
