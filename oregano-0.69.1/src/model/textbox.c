/*
 * textbox.c
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gnome.h>
#include <math.h>
#include "item-data.h"
#include "textbox.h"
#include "clipboard.h"
#include "node-store.h"
#include "schematic-print-context.h"

/*
#define TEXTBOX_DEFAULT_FONT "-*-helvetica-medium-r-*-*-*-100-*-*-*-*-*-*"
*/
#define TEXTBOX_DEFAULT_FONT "Arial 10"

/*
 * Private declarations
 */

static void textbox_class_init (TextboxClass *klass);
static void textbox_init (Textbox *textbox);
static void textbox_copy (ItemData *dest, ItemData *src);
static ItemData *textbox_clone (ItemData *src);
static void textbox_rotate (ItemData *data, int angle, SheetPos *center);
static void textbox_print (ItemData *data, cairo_t *cr, SchematicPrintContext *ctx);
static int textbox_register (ItemData *data);
static void textbox_unregister (ItemData *data);
static gboolean textbox_has_properties (ItemData *data);

static void textbox_flip (ItemData *data, gboolean horizontal,
	SheetPos *center);


enum {
	TEXT_CHANGED,
	FONT_CHANGED,
	LAST_SIGNAL
};

static ItemDataClass *parent_class = NULL;
static guint textbox_signals[LAST_SIGNAL] = { 0 };

struct _TextboxPriv {
	char *text;
	char *font;
};

GType
textbox_get_type (void)
{
	static GType textbox_type = 0;

	if (!textbox_type) {
		static const GTypeInfo textbox_info = {
			sizeof (TextboxClass),
			NULL,
			NULL,
			(GClassInitFunc) textbox_class_init,
			NULL,
			NULL,
			sizeof (Textbox),
			0,
			(GInstanceInitFunc) textbox_init,
			NULL
		};

		textbox_type = g_type_register_static(TYPE_ITEM_DATA,
			"Textbox", &textbox_info, 0);
	}

	return textbox_type;
}

static void
textbox_finalize(GObject *object)
{
	Textbox *textbox = TEXTBOX (object);
	TextboxPriv *priv = textbox->priv;

	g_free (priv);

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
textbox_dispose(GObject *object)
{
	G_OBJECT_CLASS(parent_class)->dispose(object);
}

static void
textbox_class_init (TextboxClass *klass)
{
	GObjectClass *object_class;
	ItemDataClass *item_data_class;

	parent_class = g_type_class_peek(TYPE_ITEM_DATA);
	item_data_class = ITEM_DATA_CLASS(klass);
	object_class = G_OBJECT_CLASS(klass);

	textbox_signals[TEXT_CHANGED] =
		g_signal_new ("text_changed",
			G_TYPE_FROM_CLASS(object_class),
			G_SIGNAL_RUN_FIRST,
			0,
			NULL,
			NULL,
			g_cclosure_marshal_VOID__STRING,
			G_TYPE_NONE, 1, G_TYPE_STRING);

	textbox_signals[FONT_CHANGED] =
		g_signal_new ("font_changed",
			G_TYPE_FROM_CLASS(object_class),
			G_SIGNAL_RUN_FIRST,
			0,
			NULL,
			NULL,
			g_cclosure_marshal_VOID__STRING,
			G_TYPE_NONE, 1, G_TYPE_STRING);

	object_class->finalize= textbox_finalize;
	object_class->dispose = textbox_dispose;

	item_data_class->clone = textbox_clone;
	item_data_class->copy = textbox_copy;
	item_data_class->rotate = textbox_rotate;
	item_data_class->flip = textbox_flip;
	item_data_class->unreg = textbox_unregister;
	item_data_class->reg = textbox_register;
	item_data_class->has_properties = textbox_has_properties;
	item_data_class->print = textbox_print;
}

static void
textbox_init (Textbox *textbox)
{
	TextboxPriv *priv = g_new0 (TextboxPriv, 1);
	textbox->priv = priv;
}

Textbox *
textbox_new (char *font)
{
	Textbox *textbox;

	textbox = TEXTBOX(g_object_new(TYPE_TEXTBOX, NULL));

	if (font == NULL)
		textbox->priv->font = g_strdup (TEXTBOX_DEFAULT_FONT);
	else
		textbox->priv->font = g_strdup (font);

	return textbox;
}

static ItemData *
textbox_clone (ItemData *src)
{
	Textbox *src_textbox, *new_textbox;
	ItemDataClass *id_class;

	g_return_val_if_fail (src != NULL, NULL);
	g_return_val_if_fail (IS_TEXTBOX (src), NULL);

	id_class = ITEM_DATA_CLASS(G_OBJECT_GET_CLASS(src));
	if (id_class->copy == NULL)
		return NULL;

	src_textbox = TEXTBOX(src);
	new_textbox = TEXTBOX(g_object_new(TYPE_TEXTBOX, NULL));
	id_class->copy (ITEM_DATA (new_textbox), src);

	return ITEM_DATA (new_textbox);
}

static void
textbox_copy (ItemData *dest, ItemData *src)
{
	Textbox *dest_textbox, *src_textbox;

	g_return_if_fail (dest != NULL);
	g_return_if_fail (IS_TEXTBOX (dest));
	g_return_if_fail (src != NULL);
	g_return_if_fail (IS_TEXTBOX (src));

	if (parent_class->copy != NULL)
		parent_class->copy (dest, src);

	dest_textbox = TEXTBOX (dest);
	src_textbox = TEXTBOX (src);

	dest_textbox->priv->text = src_textbox->priv->text;
	dest_textbox->priv->font = src_textbox->priv->font;
}

static void
textbox_rotate (ItemData *data, int angle, SheetPos *center)
{
	double affine[6];
	ArtPoint src, dst;
	Textbox *textbox;
	TextboxPriv *priv;
	SheetPos b1, b2;
	SheetPos textbox_center, delta;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_TEXTBOX (data));

	if (angle == 0)
		return;

	textbox = TEXTBOX (data);

	if (center) {
		item_data_get_absolute_bbox (ITEM_DATA (textbox), &b1, &b2);
		textbox_center.x = b1.x + (b2.x - b1.x) / 2;
		textbox_center.y = b1.y + (b2.y - b1.y) / 2;
	}

	priv = textbox->priv;

	art_affine_rotate (affine, angle);

	/*
	 * Let the views (canvas items) know about the rotation.
	 */
	g_signal_emit_by_name(G_OBJECT(textbox), "rotated", angle);

	if (center) {
		SheetPos textbox_pos;

		item_data_get_pos (ITEM_DATA (textbox), &textbox_pos);

		src.x = textbox_center.x - center->x;
		src.y = textbox_center.y - center->y;
		art_affine_point (&dst, &src, affine);

		delta.x = -src.x + dst.x;
		delta.y = -src.y + dst.y;

		item_data_move (ITEM_DATA (textbox), &delta);
	}
}

static void
textbox_flip (ItemData *data, gboolean horizontal, SheetPos *center)
{
	double affine[6];
	ArtPoint src, dst;
	Textbox *textbox;
	TextboxPriv *priv;
	SheetPos b1, b2;
	SheetPos textbox_center, delta;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_TEXTBOX (data));

	textbox = TEXTBOX (data);

	if (center) {
		item_data_get_absolute_bbox (ITEM_DATA (textbox), &b1, &b2);
		textbox_center.x = b1.x + (b2.x - b1.x) / 2;
		textbox_center.y = b1.y + (b2.y - b1.y) / 2;
	}

	priv = textbox->priv;

	if (horizontal)
		art_affine_scale (affine, -1, 1);
	else
		art_affine_scale (affine, 1, -1);

	/*
	 * Let the views (canvas items) know about the rotation.
	 */
	g_signal_emit_by_name(G_OBJECT (textbox), "flipped", horizontal);

	if (center) {
		SheetPos textbox_pos;

		item_data_get_pos (ITEM_DATA (textbox), &textbox_pos);

		src.x = textbox_center.x - center->x;
		src.y = textbox_center.y - center->y;
		art_affine_point (&dst, &src, affine);

		delta.x = -src.x + dst.x;
		delta.y = -src.y + dst.y;

		item_data_move (ITEM_DATA (textbox), &delta);
	}
}

/* static */
void
textbox_update_bbox (Textbox *textbox)
{
	PangoFontDescription *font;
	/*
	Unused variables
	int width;
	int rbearing;
	int lbearing;
	int ascent, descent;
	*/
	SheetPos b1, b2;
	TextboxPriv *priv;

	priv = textbox->priv;

	font = pango_font_description_from_string(priv->font);
	/* TODO : Find out how to do this with Pango. */
	/* gdk_string_extents (font,
		priv->text,
		&lbearing,
		&rbearing,
		&width,
		&ascent,
		&descent);
		gdk_font_unref (font);
	*/
	b1.x = 0.0;
	b1.y = 0.0-5; // - font->ascent;
	b2.x = 0.0+5; // + rbearing;
	b2.y = 0.0+5; // + font->descent;

	item_data_set_relative_bbox (ITEM_DATA (textbox), &b1, &b2);
	pango_font_description_free(font);
}

static gboolean
textbox_has_properties (ItemData *data)
{
	return TRUE;
}

void
textbox_set_text (Textbox *textbox, const char *text)
{
	g_return_if_fail (textbox != NULL);
	g_return_if_fail (IS_TEXTBOX (textbox));

	g_free (textbox->priv->text);
	textbox->priv->text = g_strdup (text);

	textbox_update_bbox (textbox);

	g_signal_emit_by_name (G_OBJECT(textbox), "text_changed", text);
}

char *
textbox_get_text (Textbox *textbox)
{
	g_return_val_if_fail (textbox != NULL, NULL);
	g_return_val_if_fail (IS_TEXTBOX (textbox), NULL);

	return textbox->priv->text;
}

void
textbox_set_font (Textbox *textbox, char *font)
{
	g_return_if_fail (textbox != NULL);
	g_return_if_fail (IS_TEXTBOX (textbox));

	g_free (textbox->priv->font);
	if (font == NULL)
		textbox->priv->font = g_strdup (TEXTBOX_DEFAULT_FONT);
	else
		textbox->priv->font = g_strdup (font);

	textbox_update_bbox (textbox);

	g_signal_emit_by_name(G_OBJECT (textbox),
		"font_changed", textbox->priv->font);
}

char *
textbox_get_font (Textbox *textbox)
{
	g_return_val_if_fail (textbox != NULL, NULL);
	g_return_val_if_fail (IS_TEXTBOX (textbox), NULL);

	return textbox->priv->font;
}

static void
textbox_print (ItemData *data, cairo_t *cr, SchematicPrintContext *ctx)
{
/*	GnomeCanvasPoints *line;
	double x0, y0;
	ArtPoint dst, src;
	double affine[6];
	int i;
	Textbox *textbox;
	TextboxPriv *priv;
	SheetPos pos;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_TEXTBOX (data));

	textbox = TEXTBOX (data);
	priv = textbox->priv;

	item_data_get_pos (ITEM_DATA (textbox), &pos);
	src.x = pos.x;
	src.y = pos.y;

	art_affine_identity (affine);

	gnome_print_setfont(opc->ctx,
		gnome_font_face_get_font_default(
		opc->label_font, 6)
	);
	print_draw_text (opc->ctx, priv->text, &src);
	*/
}

static void
textbox_unregister (ItemData *data)
{
	NodeStore *store;

	g_return_if_fail (IS_TEXTBOX (data));

	store = item_data_get_store (data);
	node_store_remove_textbox (store, TEXTBOX (data));
}

static int 
textbox_register (ItemData *data)
{
	NodeStore *store;

	g_return_val_if_fail (IS_TEXTBOX (data), 0);

	store = item_data_get_store (data);
	node_store_add_textbox (store, TEXTBOX (data));
	return 0;
}
