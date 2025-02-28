/*
 * ion/ioncore/conf.c
 *
 * Copyright (c) Tuomo Valkonen 1999-2009.
 *
 * See the included file LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>

#include <libtu/map.h>
#include <libtu/minmax.h>
#include <libtu/objp.h>
#include <libtu/map.h>
#include <libextl/readconfig.h>

#include "common.h"
#include "global.h"
#include "modules.h"
#include "rootwin.h"
#include "bindmaps.h"
#include "kbresize.h"
#include "reginfo.h"
#include "group-ws.h"
#include "llist.h"


StringIntMap frame_idxs[]={
    {"last", LLIST_INDEX_LAST},
    {"next",  LLIST_INDEX_AFTER_CURRENT},
    {"next-act",  LLIST_INDEX_AFTER_CURRENT_ACT},
    END_STRINGINTMAP
};

StringIntMap win_stackrq[]={
    {"ignore", IONCORE_WINDOWSTACKINGREQUEST_IGNORE},
    {"activate",  IONCORE_WINDOWSTACKINGREQUEST_ACTIVATE},
    END_STRINGINTMAP
};

static bool get_winprop_fn_set=FALSE;
static ExtlFn get_winprop_fn;

static bool get_layout_fn_set=FALSE;
static ExtlFn get_layout_fn;


/*EXTL_DOC
 * Set ioncore basic settings. The table \var{tab} may contain the
 * following fields.
 *
 * \begin{tabularx}{\linewidth}{lX}
 *  \tabhead{Field & Description}
 *  \var{opaque_resize} & (boolean) Controls whether interactive move and
 *                        resize operations simply draw a rubberband during
 *                        the operation (false) or immediately affect the
 *                        object in question at every step (true). \\
 *  \var{warp} &          (boolean) Should focusing operations move the
 *                        pointer to the object to be focused? \\
 *  \var{warp_margin} &   (integer) Border offset in pixels to apply
 *                        to the cursor when warping. \\
 *  \var{warp_factor} &   (double[2]) X and Y factor to offset the cursor.
 *                        between 0 and 1, where 0.5 is the center. \\
 *  \var{switchto} &      (boolean) Should a managing \type{WMPlex} switch
 *                        to a newly mapped client window? \\
 *  \var{screen_notify} & (boolean) Should notification tooltips be displayed
 *                        for hidden workspaces with activity? \\
 *  \var{frame_default_index} & (string) Specifies where to add new regions
 *                        on the mutually exclusive list of a frame. One of
 *                        \codestr{last}, \codestr{next}, (for after current),
 *                        or \codestr{next-act}
 *                        (for after current and anything with activity right
 *                        after it). \\
 *  \var{dblclick_delay} & (integer) Delay between clicks of a double click.\\
 *  \var{kbresize_delay} & (integer) Delay in milliseconds for ending keyboard
 *                         resize mode after inactivity. \\
 *  \var{kbresize_t_max} & (integer) Controls keyboard resize acceleration.
 *                         See description below for details. \\
 *  \var{kbresize_t_min} & (integer) See below. \\
 *  \var{kbresize_step} & (floating point) See below. \\
 *  \var{kbresize_maxacc} & (floating point) See below. \\
 *  \var{edge_resistance} & (integer) Resize edge resistance in pixels. \\
 *  \var{framed_transients} & (boolean) Put transients in nested frames. \\
 *  \var{float_placement_method} & (string) How to place floating frames.
 *                          One of \codestr{udlr} (up-down, then left-right),
 *                          \codestr{lrud} (left-right, then up-down),
 *                          \codestr{pointer} to place under the pointer or
 *                          \codestr{random}. \\
 *  \var{float_placement_padding} & (integer) Pixels between frames when
 *                          \var{float_placement_method} is \codestr{udlr} or
 *                          \codestr{lrud}. \\
 *  \var{mousefocus} & (string) Mouse focus mode:
 *                     \codestr{disabled} or \codestr{sloppy}. \\
 *  \var{unsqueeze} & (boolean) Auto-unsqueeze transients/menus/queries/etc. \\
 *  \var{window_dialog_float} & (boolean) Float dialog type windows. \\
 *  \var{autoraise} & (boolean) Autoraise regions in groups on goto. \\
 *  \var{usertime_diff_current} & (integer) Controls switchto timeout. \\
 *  \var{usertime_diff_new} & (integer) Controls switchto timeout. \\
 *  \var{autosave_layout} & (boolean) Automatically save layout on restart and exit. \\
 *  \var{window_stacking_request} & (string) How to respond to window-stacking
 *                          requests. \codestr{ignore} to do nothing,
 *                          \codestr{activate} to set the activity flag on a
 *                          window requesting to be stacked Above. \\
 *  \var{focuslist_insert_delay} & (integer) Time (in ms) that a window must
 *                          stay focused in order to be added to the focus list.
 *                          If this value is set <=0, this logic is disabled:
 *                          the focus list is updated immediately \\
 *  \var{activity_notification_on_all_screens} & (boolean) If enabled, activity
 *                          notifiers are displayed on ALL the screens, not just
 *                          the screen that contains the window producing the
 *                          notification. This is only relevant on multi-head
 *                          setups. By default this is disabled \\
 *  \var{workspace_indicator_timeout} & (integer) If enabled, a workspace
 *                          indicator comes up at the bottom-left of the screen
 *                          when a new workspace is selected. This indicator
 *                          stays active for only as long as indicated by this
 *                          variable (in ms). Timeout values <=0 disable the
 *                          indicator altogether. This is disabled by default \\
 * \end{tabularx}
 *
 * When a keyboard resize function is called, and at most \var{kbresize_t_max}
 * milliseconds has passed from a previous call, acceleration factor is reset
 * to 1.0. Otherwise, if at least \var{kbresize_t_min} milliseconds have
 * passed from the from previous acceleration update or reset the squere root
 * of the acceleration factor is incremented by \var{kbresize_step}. The
 * maximum acceleration factor (pixels/call modulo size hints) is given by
 * \var{kbresize_maxacc}. The default values are (200, 50, 30, 100).
 */
EXTL_EXPORT
void ioncore_set(ExtlTab tab)
{
    int dd;
    char *tmp;
    ExtlFn fn;

    extl_table_gets_b(tab, "opaque_resize", &(ioncore_g.opaque_resize));
    extl_table_gets_b(tab, "warp", &(ioncore_g.warp_enabled));
    extl_table_gets_i(tab, "warp_margin", &(ioncore_g.warp_margin));
    extl_table_gets_d(tab, "warp_factor_x", &(ioncore_g.warp_factor[0]));
    extl_table_gets_d(tab, "warp_factor_y", &(ioncore_g.warp_factor[1]));
    extl_table_gets_b(tab, "switchto", &(ioncore_g.switchto_new));
    extl_table_gets_b(tab, "screen_notify", &(ioncore_g.screen_notify));
    extl_table_gets_b(tab, "framed_transients", &(ioncore_g.framed_transients));
    extl_table_gets_b(tab, "unsqueeze", &(ioncore_g.unsqueeze_enabled));
    extl_table_gets_b(tab, "window_dialog_float", &(ioncore_g.window_dialog_float));
    extl_table_gets_b(tab, "autoraise", &(ioncore_g.autoraise));
    extl_table_gets_b(tab, "autosave_layout", &(ioncore_g.autosave_layout));

    if(extl_table_gets_s(tab, "window_stacking_request", &tmp)){
        ioncore_g.window_stacking_request=stringintmap_value(win_stackrq,
                                                         tmp,
                                                         ioncore_g.window_stacking_request);
        free(tmp);
    }

    if(extl_table_gets_s(tab, "frame_default_index", &tmp)){
        ioncore_g.frame_default_index=stringintmap_value(frame_idxs,
                                                         tmp,
                                                         ioncore_g.frame_default_index);
        free(tmp);
    }

    if(extl_table_gets_s(tab, "mousefocus", &tmp)){
        if(strcmp(tmp, "disabled")==0)
            ioncore_g.no_mousefocus=TRUE;
        else if(strcmp(tmp, "sloppy")==0)
            ioncore_g.no_mousefocus=FALSE;
        free(tmp);
    }

    if(extl_table_gets_i(tab, "dblclick_delay", &dd))
        ioncore_g.dblclick_delay=MAXOF(0, dd);

    if(extl_table_gets_i(tab, "usertime_diff_current", &dd))
        ioncore_g.usertime_diff_current=MAXOF(0, dd);

    if(extl_table_gets_i(tab, "usertime_diff_new", &dd))
        ioncore_g.usertime_diff_new=MAXOF(0, dd);

    if(extl_table_gets_i(tab, "focuslist_insert_delay", &dd))
        ioncore_g.focuslist_insert_delay=MAXOF(0, dd);

    if(extl_table_gets_i(tab, "workspace_indicator_timeout", &dd))
        ioncore_g.workspace_indicator_timeout=MAXOF(0, dd);

    extl_table_gets_b(tab, "activity_notification_on_all_screens",
                      &(ioncore_g.activity_notification_on_all_screens));

    ioncore_set_moveres_accel(tab);

    ioncore_groupws_set(tab);

    /* Internal -- therefore undocumented above */
    if(extl_table_gets_f(tab, "_get_winprop", &fn)){
        if(get_winprop_fn_set)
            extl_unref_fn(get_winprop_fn);
        get_winprop_fn=fn;
        get_winprop_fn_set=TRUE;
    }

    if(extl_table_gets_f(tab, "_get_layout", &fn)){
        if(get_layout_fn_set)
            extl_unref_fn(get_layout_fn);
        get_layout_fn=fn;
        get_layout_fn_set=TRUE;
    }

}


/*EXTL_DOC
 * Get ioncore basic settings. For details see \fnref{ioncore.set}.
 */
EXTL_SAFE
EXTL_EXPORT
ExtlTab ioncore_get()
{
    ExtlTab tab=extl_create_table();

    extl_table_sets_b(tab, "opaque_resize", ioncore_g.opaque_resize);
    extl_table_sets_b(tab, "warp", ioncore_g.warp_enabled);
    extl_table_sets_b(tab, "switchto", ioncore_g.switchto_new);
    extl_table_sets_i(tab, "dblclick_delay", ioncore_g.dblclick_delay);
    extl_table_sets_b(tab, "screen_notify", ioncore_g.screen_notify);
    extl_table_sets_b(tab, "framed_transients", ioncore_g.framed_transients);
    extl_table_sets_b(tab, "unsqueeze", ioncore_g.unsqueeze_enabled);
    extl_table_sets_b(tab, "window_dialog_float", ioncore_g.window_dialog_float);
    extl_table_sets_b(tab, "autoraise", ioncore_g.autoraise);
    extl_table_sets_b(tab, "autosave_layout", ioncore_g.autosave_layout);
    extl_table_sets_i(tab, "focuslist_insert_delay", ioncore_g.focuslist_insert_delay);
    extl_table_sets_i(tab, "workspace_indicator_timeout", ioncore_g.workspace_indicator_timeout);
    extl_table_sets_b(tab, "activity_notification_on_all_screens",
                      ioncore_g.activity_notification_on_all_screens);

    extl_table_sets_s(tab, "window_stacking_request",
                      stringintmap_key(win_stackrq,
                                       ioncore_g.window_stacking_request,
                                       NULL));

    extl_table_sets_s(tab, "frame_default_index",
                      stringintmap_key(frame_idxs,
                                       ioncore_g.frame_default_index,
                                       NULL));

    extl_table_sets_s(tab, "mousefocus", (ioncore_g.no_mousefocus
                                          ? "disabled"
                                          : "sloppy"));

    ioncore_get_moveres_accel(tab);

    ioncore_groupws_get(tab);

    return tab;
}


ExtlTab ioncore_get_winprop(WClientWin *cwin)
{
    ExtlTab tab=extl_table_none();

    if(get_winprop_fn_set){
        extl_protect(NULL);
        extl_call(get_winprop_fn, "o", "t", cwin, &tab);
        extl_unprotect(NULL);
    }

    return tab;
}


ExtlTab ioncore_get_layout(const char *layout)
{
    ExtlTab tab=extl_table_none();

    if(get_layout_fn_set){
        extl_protect(NULL);
        extl_call(get_layout_fn, "s", "t", layout, &tab);
        extl_unprotect(NULL);
    }

    return tab;
}


/*EXTL_DOC
 * Get important directories (the fields \var{userdir},
 * \var{sessiondir}, \var{searchpath} in the returned table).
 */
EXTL_SAFE
EXTL_EXPORT
ExtlTab ioncore_get_paths(ExtlTab tab)
{
    tab=extl_create_table();
    extl_table_sets_s(tab, "userdir", extl_userdir());
    extl_table_sets_s(tab, "sessiondir", extl_sessiondir());
    extl_table_sets_s(tab, "searchpath", extl_searchpath());
    return tab;
}


/*EXTL_DOC
 * Set important directories (the fields \var{sessiondir}, \var{searchpath}
 * of \var{tab}).
 */
EXTL_EXPORT
bool ioncore_set_paths(ExtlTab tab)
{
    char *s;

    if(extl_table_gets_s(tab, "userdir", &s)){
        warn(TR("User directory can not be set."));
        free(s);
        return FALSE;
    }

    if(extl_table_gets_s(tab, "sessiondir", &s)){
        extl_set_sessiondir(s);
        free(s);
        return FALSE;
    }

    if(extl_table_gets_s(tab, "searchpath", &s)){
        extl_set_searchpath(s);
        free(s);
        return FALSE;
    }

    return TRUE;
}


/* Exports these in ioncore. */

/*EXTL_DOC
 * Lookup script \var{file}. If \var{try_in_dir} is set, it is tried
 * before the standard search path.
 */
EXTL_SAFE
EXTL_EXPORT_AS(ioncore, lookup_script)
char *extl_lookup_script(const char *file, const char *sp);


/*EXTL_DOC
 * Get a file name to save (session) data in. The string \var{basename}
 * should contain no path or extension components.
 */
EXTL_SAFE
EXTL_EXPORT_AS(ioncore, get_savefile)
char *extl_get_savefile(const char *basename);


/*EXTL_DOC
 * Write \var{tab} in file with basename \var{basename} in the
 * session directory.
 */
EXTL_SAFE
EXTL_EXPORT_AS(ioncore, write_savefile)
bool extl_write_savefile(const char *basename, ExtlTab tab);


/*EXTL_DOC
 * Read a savefile.
 */
EXTL_SAFE
EXTL_EXPORT_AS(ioncore, read_savefile)
ExtlTab extl_extl_read_savefile(const char *basename);



bool ioncore_read_main_config(const char *cfgfile)
{
    bool ret;
    int unset=0;

    if(cfgfile==NULL)
        cfgfile="cfg_notion";

    ret=extl_read_config(cfgfile, ".", TRUE);

    unset+=(ioncore_screen_bindmap->nbindings==0);
    unset+=(ioncore_mplex_bindmap->nbindings==0);
    unset+=(ioncore_frame_bindmap->nbindings==0);

    if(unset>0){
        warn(TR("Some bindmaps were empty, loading ioncore_efbb."));
        extl_read_config("ioncore_efbb", NULL, TRUE);
    }

    return (ret && unset==0);
}
