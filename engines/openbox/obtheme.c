#include "obengine.h"
#include "kernel/openbox.h"
#include "kernel/config.h"

#include <glib.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif
#ifdef HAVE_CTYPE_H
#  include <ctype.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif

static XrmDatabase loaddb(char *theme)
{
    XrmDatabase db;

    db = XrmGetFileDatabase(theme);
    if (db == NULL) {
	char *s = g_build_filename(g_get_home_dir(), ".openbox", "themes",
				   "openbox", theme, NULL);
	db = XrmGetFileDatabase(s);
	g_free(s);
    }
    if (db == NULL) {
	char *s = g_build_filename(THEMEDIR, theme, NULL);
	db = XrmGetFileDatabase(s);
	g_free(s);
    }
    return db;
}

static char *create_class_name(char *rname)
{
    char *rclass = g_strdup(rname);
    char *p = rclass;

    while (TRUE) {
	*p = toupper(*p);
	p = strchr(p+1, '.');
	if (p == NULL) break;
	++p;
	if (*p == '\0') break;
    }
    return rclass;
}

static gboolean read_int(XrmDatabase db, char *rname, int *value)
{
    gboolean ret = FALSE;
    char *rclass = create_class_name(rname);
    char *rettype, *end;
    XrmValue retvalue;
  
    if (XrmGetResource(db, rname, rclass, &rettype, &retvalue) &&
	retvalue.addr != NULL) {
	*value = (int)strtol(retvalue.addr, &end, 10);
	if (end != retvalue.addr)
	    ret = TRUE;
    }

    g_free(rclass);
    return ret;
}

static gboolean read_string(XrmDatabase db, char *rname, char **value)
{
    gboolean ret = FALSE;
    char *rclass = create_class_name(rname);
    char *rettype;
    XrmValue retvalue;
  
    if (XrmGetResource(db, rname, rclass, &rettype, &retvalue) &&
	retvalue.addr != NULL) {
	*value = g_strdup(retvalue.addr);
	ret = TRUE;
    }

    g_free(rclass);
    return ret;
}

static gboolean read_color(XrmDatabase db, char *rname, color_rgb **value)
{
    gboolean ret = FALSE;
    char *rclass = create_class_name(rname);
    char *rettype;
    XrmValue retvalue;
  
    if (XrmGetResource(db, rname, rclass, &rettype, &retvalue) &&
	retvalue.addr != NULL) {
	color_rgb *c = color_parse(retvalue.addr);
	if (c != NULL) {
	    *value = c;
	    ret = TRUE;
	}
    }

    g_free(rclass);
    return ret;
}

static gboolean read_mask(XrmDatabase db, char *rname, pixmap_mask **value)
{
    gboolean ret = FALSE;
    char *rclass = create_class_name(rname);
    char *rettype;
    char *s;
    char *button_dir;
    XrmValue retvalue;
    int hx, hy; /* ignored */
    unsigned int w, h;
    unsigned char *b;
  
    if (XrmGetResource(db, rname, rclass, &rettype, &retvalue) &&
        retvalue.addr != NULL) {

	button_dir = g_strdup_printf("%s_buttons", config_engine_theme);

        s = g_build_filename(g_get_home_dir(), ".openbox", "themes",
                             "openbox", button_dir, retvalue.addr, NULL);

        if (XReadBitmapFileData(s, &w, &h, &b, &hx, &hy) == BitmapSuccess)
            ret = TRUE;
        else {
            g_free(s);
            s = g_build_filename(THEMEDIR, button_dir, retvalue.addr, NULL);
	
            if (XReadBitmapFileData(s, &w, &h, &b, &hx, &hy) == BitmapSuccess) 
                ret = TRUE;
            else {
                char *themename;

                g_free(s);
                themename = g_path_get_basename(config_engine_theme);
                s = g_strdup_printf("%s/%s_buttons/%s", config_engine_theme,
                                    themename, retvalue.addr);
                g_free(themename);
                if (XReadBitmapFileData(s, &w, &h, &b, &hx, &hy) ==
                    BitmapSuccess) 
                    ret = TRUE;
                else
                    g_message("Unable to find bitmap '%s'", retvalue.addr);
            }
        }

        if (ret) {
            *value = pixmap_mask_new(w, h, (char*)b);
            XFree(b);
        }
      
        g_free(s);
        g_free(button_dir);
    }

    g_free(rclass);
    return ret;
}

static void parse_appearance(char *tex, SurfaceColorType *grad,
                             ReliefType *relief, BevelType *bevel,
                             gboolean *interlaced, gboolean *border)
{
    char *t;

    /* convert to all lowercase */
    for (t = tex; *t != '\0'; ++t)
	*t = g_ascii_tolower(*t);

    if (strstr(tex, "parentrelative") != NULL) {
	*grad = Background_ParentRelative;
    } else {
	if (strstr(tex, "gradient") != NULL) {
	    if (strstr(tex, "crossdiagonal") != NULL)
		*grad = Background_CrossDiagonal;
	    else if (strstr(tex, "rectangle") != NULL)
		*grad = Background_Rectangle;
	    else if (strstr(tex, "pyramid") != NULL)
		*grad = Background_Pyramid;
	    else if (strstr(tex, "pipecross") != NULL)
		*grad = Background_PipeCross;
	    else if (strstr(tex, "elliptic") != NULL)
		*grad = Background_Elliptic;
	    else if (strstr(tex, "horizontal") != NULL)
		*grad = Background_Horizontal;
	    else if (strstr(tex, "vertical") != NULL)
		*grad = Background_Vertical;
	    else
		*grad = Background_Diagonal;
	} else {
	    *grad = Background_Solid;
	}

	if (strstr(tex, "sunken") != NULL)
	    *relief = Sunken;
	else if (strstr(tex, "flat") != NULL)
	    *relief = Flat;
	else
	    *relief = Raised;
	
	*border = FALSE;
	if (*relief == Flat) {
	    if (strstr(tex, "border") != NULL)
		*border = TRUE;
	} else {
	    if (strstr(tex, "bevel2") != NULL)
		*bevel = Bevel2;
	    else
		*bevel = Bevel1;
	}

	if (strstr(tex, "interlaced") != NULL)
	    *interlaced = TRUE;
	else
	    *interlaced = FALSE;
    }
}


static gboolean read_appearance(XrmDatabase db, char *rname, Appearance *value)
{
    gboolean ret = FALSE;
    char *rclass = create_class_name(rname), *cname, *ctoname, *bcname;
    char *rettype;
    XrmValue retvalue;

    cname = g_strconcat(rname, ".color", NULL);
    ctoname = g_strconcat(rname, ".colorTo", NULL);
    bcname = g_strconcat(rname, ".borderColor", NULL);

    if (XrmGetResource(db, rname, rclass, &rettype, &retvalue) &&
	retvalue.addr != NULL) {
	parse_appearance(retvalue.addr,
			 &value->surface.data.planar.grad,
			 &value->surface.data.planar.relief,
			 &value->surface.data.planar.bevel,
			 &value->surface.data.planar.interlaced,
			 &value->surface.data.planar.border);
	if (!read_color(db, cname, &value->surface.data.planar.primary))
	    value->surface.data.planar.primary = color_new(0, 0, 0);
	if (!read_color(db, ctoname, &value->surface.data.planar.secondary))
	    value->surface.data.planar.secondary = color_new(0, 0, 0);
	if (value->surface.data.planar.border)
	    if (!read_color(db, bcname,
			    &value->surface.data.planar.border_color))
		value->surface.data.planar.border_color = color_new(0, 0, 0);
	ret = TRUE;
    }

    g_free(bcname);
    g_free(ctoname);
    g_free(cname);
    g_free(rclass);
    return ret;
}

static void set_default_appearance(Appearance *a)
{
    a->surface.data.planar.grad = Background_Solid;
    a->surface.data.planar.relief = Flat;
    a->surface.data.planar.bevel = Bevel1;
    a->surface.data.planar.interlaced = FALSE;
    a->surface.data.planar.border = FALSE;
    a->surface.data.planar.primary = color_new(0, 0, 0);
    a->surface.data.planar.secondary = color_new(0, 0, 0);
}

gboolean obtheme_load()
{
    XrmDatabase db = NULL;
    Justify winjust;
    char *winjuststr;

    if (config_engine_theme) {
	db = loaddb(config_engine_theme);
        if (db == NULL) {
	    g_warning("Failed to load the theme '%s'", config_engine_theme);
	    g_message("Falling back to the default: '%s'", DEFAULT_THEME);
	}
    }
    if (db == NULL) {
	db = loaddb(DEFAULT_THEME);
	if (db == NULL) {
	    g_warning("Failed to load the theme '%s'.", DEFAULT_THEME);
	    return FALSE;
	}
        /* set it to what was loaded */
        g_free(config_engine_theme);
        config_engine_theme = g_strdup(DEFAULT_THEME);
    }

    /* load the font, not from the theme file tho, its in the config */

    ob_s_winfont = font_open(config_engine_font);
    ob_s_winfont_height = font_height(ob_s_winfont, config_engine_shadow,
                                      config_engine_shadow_offset);

    winjust = Justify_Left;
    if (read_string(db, "window.justify", &winjuststr)) {
        if (!g_ascii_strcasecmp(winjuststr, "right"))
            winjust = Justify_Right;
        else if (!g_ascii_strcasecmp(winjuststr, "center"))
            winjust = Justify_Center;
        g_free(winjuststr);
    }

    if (!read_int(db, "handleWidth", &ob_s_handle_height) ||
	ob_s_handle_height < 0 || ob_s_handle_height > 100)
        ob_s_handle_height = 6;
    if (!read_int(db, "bevelWidth", &ob_s_bevel) ||
	ob_s_bevel <= 0 || ob_s_bevel > 100) ob_s_bevel = 3;
    if (!read_int(db, "borderWidth", &ob_s_bwidth) ||
	ob_s_bwidth < 0 || ob_s_bwidth > 100) ob_s_bwidth = 1;
    if (!read_int(db, "frameWidth", &ob_s_cbwidth) ||
	ob_s_cbwidth < 0 || ob_s_cbwidth > 100) ob_s_cbwidth = ob_s_bevel;

    if (!read_color(db, "borderColor", &ob_s_b_color))
	ob_s_b_color = color_new(0, 0, 0);
    if (!read_color(db, "window.frame.focusColor", &ob_s_cb_focused_color))
	ob_s_cb_focused_color = color_new(0xff, 0xff, 0xff);
    if (!read_color(db, "window.frame.unfocusColor", &ob_s_cb_unfocused_color))
	ob_s_cb_unfocused_color = color_new(0xff, 0xff, 0xff);
    if (!read_color(db, "window.label.focus.textColor",
                    &ob_s_title_focused_color))
	ob_s_title_focused_color = color_new(0x0, 0x0, 0x0);
    if (!read_color(db, "window.label.unfocus.textColor",
                    &ob_s_title_unfocused_color))
	ob_s_title_unfocused_color = color_new(0xff, 0xff, 0xff);
    if (!read_color(db, "window.button.focus.picColor",
                    &ob_s_titlebut_focused_color))
	ob_s_titlebut_focused_color = color_new(0, 0, 0);
    if (!read_color(db, "window.button.unfocus.picColor",
                    &ob_s_titlebut_unfocused_color))
	ob_s_titlebut_unfocused_color = color_new(0xff, 0xff, 0xff);

    if (read_mask(db, "window.button.max.mask", &ob_s_max_unset_mask)) {
        if (!read_mask(db, "window.button.max.toggled.mask",
                       &ob_s_max_set_mask)) {
            ob_s_max_set_mask = pixmap_mask_copy(ob_s_max_unset_mask);
        }
    } else {
        {
            char data[] = { 0x7f, 0x7f, 0x7f, 0x41, 0x41, 0x41, 0x7f };
            ob_s_max_unset_mask = pixmap_mask_new(7, 7, data);
        }
        {
            char data[] = { 0x7c, 0x44, 0x47, 0x47, 0x7f, 0x1f, 0x1f };
            ob_s_max_set_mask = pixmap_mask_new(7, 7, data);
        }
    }

    if (!read_mask(db, "window.button.icon.mask",
                   &ob_s_iconify_mask)) {
        char data[] = { 0x00, 0x00, 0x00, 0x00, 0x7f, 0x7f, 0x7f };
        ob_s_iconify_mask = pixmap_mask_new(7, 7, data);
    }

    if (read_mask(db, "window.button.stick.mask",
                   &ob_s_desk_unset_mask)) {
        if (!read_mask(db, "window.button.stick.toggled.mask",
                       &ob_s_desk_set_mask)) {
            ob_s_desk_set_mask =
                pixmap_mask_copy(ob_s_desk_unset_mask);
        }
    } else {
        {
            char data[] = { 0x63, 0x63, 0x00, 0x00, 0x00, 0x63, 0x63 };
            ob_s_desk_unset_mask = pixmap_mask_new(7, 7, data);
        }
        {
            char data[] = { 0x00, 0x36, 0x36, 0x08, 0x36, 0x36, 0x00 };
            ob_s_desk_set_mask = pixmap_mask_new(7, 7, data);
        }
    }

    if (read_mask(db, "window.button.shade.mask",
                   &ob_s_shade_unset_mask)) {
        if (!read_mask(db, "window.button.shade.toggled.mask",
                       &ob_s_shade_set_mask)) {
            ob_s_shade_set_mask =
                pixmap_mask_copy(ob_s_shade_unset_mask);
        }
    } else {
        {
            char data[] = { 0x7f, 0x7f, 0x7f, 0x00, 0x00, 0x00, 0x00 };
            ob_s_shade_unset_mask = pixmap_mask_new(7, 7, data);
        }
        {
            char data[] = { 0x7f, 0x7f, 0x7f, 0x00, 0x00, 0x00, 0x7f };
            ob_s_shade_set_mask = pixmap_mask_new(7, 7, data);
        }
    }

    if (!read_mask(db, "window.button.close.mask",
                   &ob_s_close_mask)) {
        char data[] = { 0x63, 0x77, 0x3e, 0x1c, 0x3e, 0x77, 0x63 };
        ob_s_close_mask = pixmap_mask_new(7, 7, data);
    }        

    /* read the decoration textures */
    if (!read_appearance(db, "window.title.focus", ob_a_focused_title))
	set_default_appearance(ob_a_focused_title);
    if (!read_appearance(db, "window.title.unfocus", ob_a_unfocused_title))
	set_default_appearance(ob_a_unfocused_title);
    if (!read_appearance(db, "window.label.focus", ob_a_focused_label))
	set_default_appearance(ob_a_focused_label);
    if (!read_appearance(db, "window.label.unfocus", ob_a_unfocused_label))
	set_default_appearance(ob_a_unfocused_label);
    if (!read_appearance(db, "window.handle.focus", ob_a_focused_handle))
	set_default_appearance(ob_a_focused_handle);
    if (!read_appearance(db, "window.handle.unfocus", ob_a_unfocused_handle))
	set_default_appearance(ob_a_unfocused_handle);
    if (!read_appearance(db, "window.grip.focus", ob_a_focused_grip))
	set_default_appearance(ob_a_focused_grip);
    if (!read_appearance(db, "window.grip.unfocus", ob_a_unfocused_grip))
	set_default_appearance(ob_a_unfocused_grip);

    /* read the appearances for rendering non-decorations. these cannot be
       parent-relative */
    if (ob_a_focused_label->surface.data.planar.grad !=
        Background_ParentRelative) {
        if (!read_appearance(db, "window.label.focus", ob_app_hilite_label))
            set_default_appearance(ob_app_hilite_label);
    } else {
        if (!read_appearance(db, "window.title.focus", ob_app_hilite_label))
            set_default_appearance(ob_app_hilite_label);
    }
    if (ob_a_unfocused_label->surface.data.planar.grad !=
        Background_ParentRelative) {
        if (!read_appearance(db, "window.label.unfocus",ob_app_unhilite_label))
            set_default_appearance(ob_app_unhilite_label);
    } else {
        if (!read_appearance(db, "window.title.unfocus",ob_app_unhilite_label))
            set_default_appearance(ob_app_unhilite_label);
    }

    /* read buttons textures */
    if (!read_appearance(db, "window.button.pressed.focus",
			 ob_a_focused_pressed_max))
	if (!read_appearance(db, "window.button.pressed",
                             ob_a_focused_pressed_max))
	    set_default_appearance(ob_a_focused_pressed_max);
    if (!read_appearance(db, "window.button.pressed.unfocus",
			 ob_a_unfocused_pressed_max))
	if (!read_appearance(db, "window.button.pressed",
			     ob_a_unfocused_pressed_max))
	    set_default_appearance(ob_a_unfocused_pressed_max);
    if (!read_appearance(db, "window.button.focus",
			 ob_a_focused_unpressed_max))
	set_default_appearance(ob_a_focused_unpressed_max);
    if (!read_appearance(db, "window.button.unfocus",
			 ob_a_unfocused_unpressed_max))
	set_default_appearance(ob_a_unfocused_unpressed_max);

    ob_a_unfocused_unpressed_close =
        appearance_copy(ob_a_unfocused_unpressed_max);
    ob_a_unfocused_pressed_close = appearance_copy(ob_a_unfocused_pressed_max);
    ob_a_focused_unpressed_close = appearance_copy(ob_a_focused_unpressed_max);
    ob_a_focused_pressed_close = appearance_copy(ob_a_focused_pressed_max);
    ob_a_unfocused_unpressed_desk =
        appearance_copy(ob_a_unfocused_unpressed_max);
    ob_a_unfocused_pressed_desk = appearance_copy(ob_a_unfocused_pressed_max);
    ob_a_unfocused_pressed_set_desk =
        appearance_copy(ob_a_unfocused_pressed_max);
    ob_a_focused_unpressed_desk = appearance_copy(ob_a_focused_unpressed_max);
    ob_a_focused_pressed_desk = appearance_copy(ob_a_focused_pressed_max);
    ob_a_focused_pressed_set_desk = appearance_copy(ob_a_focused_pressed_max);
    ob_a_unfocused_unpressed_shade =
        appearance_copy(ob_a_unfocused_unpressed_max);
    ob_a_unfocused_pressed_shade = appearance_copy(ob_a_unfocused_pressed_max);
    ob_a_unfocused_pressed_set_shade =
        appearance_copy(ob_a_unfocused_pressed_max);
    ob_a_focused_unpressed_shade = appearance_copy(ob_a_focused_unpressed_max);
    ob_a_focused_pressed_shade = appearance_copy(ob_a_focused_pressed_max);
    ob_a_focused_pressed_set_shade = appearance_copy(ob_a_focused_pressed_max);
    ob_a_unfocused_unpressed_iconify =
        appearance_copy(ob_a_unfocused_unpressed_max);
    ob_a_unfocused_pressed_iconify =
        appearance_copy(ob_a_unfocused_pressed_max);
    ob_a_focused_unpressed_iconify =
        appearance_copy(ob_a_focused_unpressed_max);
    ob_a_focused_pressed_iconify = appearance_copy(ob_a_focused_pressed_max);
    ob_a_unfocused_pressed_set_max =
        appearance_copy(ob_a_unfocused_pressed_max);
    ob_a_focused_pressed_set_max = appearance_copy(ob_a_focused_pressed_max);

    ob_a_icon->surface.data.planar.grad = Background_ParentRelative;

    /* set up the textures */
    ob_a_focused_label->texture[0].type = Text;
    ob_a_focused_label->texture[0].data.text.justify = winjust;
    ob_a_focused_label->texture[0].data.text.font = ob_s_winfont;
    ob_a_focused_label->texture[0].data.text.shadow = config_engine_shadow;
    ob_a_focused_label->texture[0].data.text.offset =
        config_engine_shadow_offset;
    ob_a_focused_label->texture[0].data.text.tint = config_engine_shadow_tint;
    ob_a_focused_label->texture[0].data.text.color = ob_s_title_focused_color;
    ob_app_hilite_label->texture[0].type = Text;
    ob_app_hilite_label->texture[0].data.text.justify = winjust;
    ob_app_hilite_label->texture[0].data.text.font = ob_s_winfont;
    ob_app_hilite_label->texture[0].data.text.shadow = config_engine_shadow;
    ob_app_hilite_label->texture[0].data.text.offset =
        config_engine_shadow_offset;
    ob_app_hilite_label->texture[0].data.text.tint = config_engine_shadow_tint;
    ob_app_hilite_label->texture[0].data.text.color = ob_s_title_focused_color;

    ob_a_unfocused_label->texture[0].type = Text;
    ob_a_unfocused_label->texture[0].data.text.justify = winjust;
    ob_a_unfocused_label->texture[0].data.text.font = ob_s_winfont;
    ob_a_unfocused_label->texture[0].data.text.shadow = config_engine_shadow;
    ob_a_unfocused_label->texture[0].data.text.offset =
        config_engine_shadow_offset;
    ob_a_unfocused_label->texture[0].data.text.tint =config_engine_shadow_tint;
    ob_a_unfocused_label->texture[0].data.text.color =
        ob_s_title_unfocused_color;
    ob_app_unhilite_label->texture[0].type = Text;
    ob_app_unhilite_label->texture[0].data.text.justify = winjust;
    ob_app_unhilite_label->texture[0].data.text.font = ob_s_winfont;
    ob_app_unhilite_label->texture[0].data.text.shadow = config_engine_shadow;
    ob_app_unhilite_label->texture[0].data.text.offset =
        config_engine_shadow_offset;
    ob_app_unhilite_label->texture[0].data.text.tint =
        config_engine_shadow_tint;
    ob_app_unhilite_label->texture[0].data.text.color =
        ob_s_title_unfocused_color;

    ob_a_focused_unpressed_max->texture[0].type = 
        ob_a_focused_pressed_max->texture[0].type = 
        ob_a_focused_pressed_set_max->texture[0].type =  
        ob_a_unfocused_unpressed_max->texture[0].type = 
        ob_a_unfocused_pressed_max->texture[0].type = 
        ob_a_unfocused_pressed_set_max->texture[0].type = 
        ob_a_focused_unpressed_close->texture[0].type = 
        ob_a_focused_pressed_close->texture[0].type = 
        ob_a_unfocused_unpressed_close->texture[0].type = 
        ob_a_unfocused_pressed_close->texture[0].type = 
        ob_a_focused_unpressed_desk->texture[0].type = 
        ob_a_focused_pressed_desk->texture[0].type = 
        ob_a_focused_pressed_set_desk->texture[0].type = 
        ob_a_unfocused_unpressed_desk->texture[0].type = 
        ob_a_unfocused_pressed_desk->texture[0].type = 
        ob_a_unfocused_pressed_set_desk->texture[0].type = 
        ob_a_focused_unpressed_shade->texture[0].type = 
        ob_a_focused_pressed_shade->texture[0].type = 
        ob_a_focused_pressed_set_shade->texture[0].type = 
        ob_a_unfocused_unpressed_shade->texture[0].type = 
        ob_a_unfocused_pressed_shade->texture[0].type = 
        ob_a_unfocused_pressed_set_shade->texture[0].type = 
        ob_a_focused_unpressed_iconify->texture[0].type = 
        ob_a_focused_pressed_iconify->texture[0].type = 
        ob_a_unfocused_unpressed_iconify->texture[0].type = 
        ob_a_unfocused_pressed_iconify->texture[0].type = Bitmask;
    ob_a_focused_unpressed_max->texture[0].data.mask.mask = 
        ob_a_unfocused_unpressed_max->texture[0].data.mask.mask = 
        ob_a_focused_pressed_max->texture[0].data.mask.mask = 
        ob_a_unfocused_pressed_max->texture[0].data.mask.mask =
        ob_s_max_unset_mask;
    ob_a_focused_pressed_set_max->texture[0].data.mask.mask = 
        ob_a_unfocused_pressed_set_max->texture[0].data.mask.mask =
        ob_s_max_set_mask;
    ob_a_focused_pressed_close->texture[0].data.mask.mask = 
        ob_a_unfocused_pressed_close->texture[0].data.mask.mask =
        ob_a_focused_unpressed_close->texture[0].data.mask.mask = 
        ob_a_unfocused_unpressed_close->texture[0].data.mask.mask =
        ob_s_close_mask;
    ob_a_focused_unpressed_desk->texture[0].data.mask.mask = 
        ob_a_unfocused_unpressed_desk->texture[0].data.mask.mask = 
        ob_a_focused_pressed_desk->texture[0].data.mask.mask = 
        ob_a_unfocused_pressed_desk->texture[0].data.mask.mask =
        ob_s_desk_unset_mask;
    ob_a_focused_pressed_set_desk->texture[0].data.mask.mask = 
        ob_a_unfocused_pressed_set_desk->texture[0].data.mask.mask =
        ob_s_desk_set_mask;
    ob_a_focused_unpressed_shade->texture[0].data.mask.mask = 
        ob_a_unfocused_unpressed_shade->texture[0].data.mask.mask = 
        ob_a_focused_pressed_shade->texture[0].data.mask.mask = 
        ob_a_unfocused_pressed_shade->texture[0].data.mask.mask =
        ob_s_shade_unset_mask;
    ob_a_focused_pressed_set_shade->texture[0].data.mask.mask = 
        ob_a_unfocused_pressed_set_shade->texture[0].data.mask.mask =
        ob_s_shade_set_mask;
    ob_a_focused_unpressed_iconify->texture[0].data.mask.mask = 
        ob_a_unfocused_unpressed_iconify->texture[0].data.mask.mask = 
        ob_a_focused_pressed_iconify->texture[0].data.mask.mask = 
        ob_a_unfocused_pressed_iconify->texture[0].data.mask.mask =
        ob_s_iconify_mask;
    ob_a_focused_unpressed_max->texture[0].data.mask.color = 
        ob_a_focused_pressed_max->texture[0].data.mask.color = 
        ob_a_focused_pressed_set_max->texture[0].data.mask.color = 
        ob_a_focused_unpressed_close->texture[0].data.mask.color = 
        ob_a_focused_pressed_close->texture[0].data.mask.color = 
        ob_a_focused_unpressed_desk->texture[0].data.mask.color = 
        ob_a_focused_pressed_desk->texture[0].data.mask.color = 
        ob_a_focused_pressed_set_desk->texture[0].data.mask.color = 
        ob_a_focused_unpressed_shade->texture[0].data.mask.color = 
        ob_a_focused_pressed_shade->texture[0].data.mask.color = 
        ob_a_focused_pressed_set_shade->texture[0].data.mask.color = 
        ob_a_focused_unpressed_iconify->texture[0].data.mask.color = 
        ob_a_focused_pressed_iconify->texture[0].data.mask.color =
        ob_s_titlebut_focused_color;
    ob_a_unfocused_unpressed_max->texture[0].data.mask.color = 
        ob_a_unfocused_pressed_max->texture[0].data.mask.color = 
        ob_a_unfocused_pressed_set_max->texture[0].data.mask.color = 
        ob_a_unfocused_unpressed_close->texture[0].data.mask.color = 
        ob_a_unfocused_pressed_close->texture[0].data.mask.color = 
        ob_a_unfocused_unpressed_desk->texture[0].data.mask.color = 
        ob_a_unfocused_pressed_desk->texture[0].data.mask.color = 
        ob_a_unfocused_pressed_set_desk->texture[0].data.mask.color = 
        ob_a_unfocused_unpressed_shade->texture[0].data.mask.color = 
        ob_a_unfocused_pressed_shade->texture[0].data.mask.color = 
        ob_a_unfocused_pressed_set_shade->texture[0].data.mask.color = 
        ob_a_unfocused_unpressed_iconify->texture[0].data.mask.color = 
        ob_a_unfocused_pressed_iconify->texture[0].data.mask.color =
        ob_s_titlebut_unfocused_color;

    XrmDestroyDatabase(db);
    return TRUE;
}


