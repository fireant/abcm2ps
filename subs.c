/*
 * Low-level utilities.
 *
 * This file is part of abcm2ps.
 *
 * Copyright (C) 1998-2013 Jean-François Moine
 * Adapted from abc2ps, Copyright (C) 1996,1997 Michael Methfessel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#ifdef HAVE_PANGO
#include <pango/pangocairo.h>
#include <pango/pangofc-font.h>
#endif

#include "abc2ps.h" 

char tex_buf[TEX_BUF_SZ];	/* result of tex_str() */
int outft = -1;			/* last font in the output file */

static char *strop;		/* current string output operation */
static float strlw;		/* line width */
static int curft;		/* current (wanted) font */
static int defft;		/* default font */
static char strtx;		/* PostScript text outputing (bits) */
#define TX_STR 1			/* string started */
#define TX_ARR 2			/* glyph/string array started */
#define TX_EXT 4			/* glyph/string array needed */

/* width of characters according to the encoding */
/* these are the widths for Times-Roman, extracted from the 'a2ps' package */
/*fixme-hack: set 500 to control characters for utf-8*/
static short cw_tb[] = {
	500,500,500,500,500,500,500,500,
	500,500,500,500,500,500,500,500,
	500,500,500,500,500,500,500,500,
	500,500,500,500,500,500,500,500,
	250,333,408,500,500,833,778,333,
	333,333,500,564,250,564,250,278,
	500,500,500,500,500,500,500,500,
	500,500,278,278,564,564,564,444,
	921,722,667,667,722,611,556,722,
	722,333,389,722,611,889,722,722,
	556,722,667,556,611,722,722,944,
	722,722,611,333,278,333,469,500,
	333,444,500,444,500,444,333,500,
	500,278,278,500,278,778,500,500,
	500,500,333,389,278,500,500,722,
	500,500,444,480,200,480,541,500,
	500,500,500,500,500,500,500,500,
	500,500,500,500,500,500,500,500,
	500,500,500,500,500,500,500,500,
	500,500,500,500,500,500,500,500,
	250,333,500,500,500,500,200,500,
	333,760,276,500,564,333,760,333,
	400,564,300,300,333,500,453,350,
	333,278,310,500,750,750,750,444,
	722,722,722,722,722,722,889,667,
	611,611,611,611,333,333,333,333,
	722,722,722,722,722,722,722,564,
	722,722,722,722,722,722,556,500,
	444,444,444,444,444,444,667,444,
	444,444,444,444,278,278,278,278,
	500,500,500,500,500,500,500,564,
	500,500,500,500,500,500,500,500,
};

static struct u_ps {
	struct u_ps *next;
	char text[2];
} *user_ps;

/* -- print message for internal error and maybe stop -- */
void bug(char *msg, int fatal)
{
	error(1, 0, "Internal error: %s.", msg);
	if (fatal) {
		fprintf(stderr, "Emergency stop.\n\n");
		exit(EXIT_FAILURE);
	}
	fprintf(stderr, "Trying to continue...\n");
}

/* -- print an error message -- */
void error(int sev,	/* 0: warning, 1: error */
	   struct SYMBOL *s,
	   char *fmt, ...)
{
	va_list args;
static struct SYMBOL *t;

	if (t != info['T' - 'A']) {
		char *p;

		t = info['T' - 'A'];
		p = &t->as.text[2];
		while (isspace((unsigned char) *p))
			p++;
		fprintf(stderr, "   - In tune '%s':\n", p);
	}
	fprintf(stderr, sev == 0 ? "Warning " : "Error ");
	if (s) {
		fprintf(stderr, "in line %d.%d",
			s->as.linenum, s->as.colnum);
		s->as.flags |= ABC_F_ERROR;
	}
	fprintf(stderr, ": ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	if (sev > severity)
		severity = sev;
}

/* -- read a number with a unit -- */
float scan_u(char *str)
{
	float a;
	int nch;

	if (sscanf(str, "%f%n", &a, &nch) == 1) {
		if (str[nch] == '\0' || str[nch] == ' ')
			return a PT;
		if (!strncasecmp(str + nch, "cm", 2))
			return a CM;
		if (!strncasecmp(str + nch, "in", 2))
			return a IN;
		if (!strncasecmp(str + nch, "pt", 2))
			return a PT;
	}
	error(1, 0, "Unknown unit value \"%s\"", str);
	return 20 PT;
}

/* -- capitalize a string -- */
static void cap_str(char *p)
{
	while (*p != '\0') {
#if 1
/* pb with toupper - works with ASCII and some latin characters only */
		unsigned char c;

		c = (unsigned char) *p;
		if (c >= 'a' && c <= 'z') {
			*p = c & ~0x20;
		} else if (c == 0xc3) {
			p++;
			c = *p;
			if (c >= 0xa0 && c <= 0xbe && c != 0xb7)
				*p = c & ~0x20;
		} else if (c == 0xc4) {
			p++;
			c = *p;
			if (c >= 0x81 && c <= 0xb7 && (c & 0x01))
				(*p)--;
		}
#else
		*p = toupper((unsigned char) *p);
#endif
		p++;
	}
}

/* -- return the character width -- */
float cwid(unsigned short c)
{
	if (c > sizeof cw_tb / sizeof cw_tb[0])
		c = 'a';
	return (float) cw_tb[c] / 1000.;
}

/* -- change string taking care of some tex-style codes -- */
/* Return an estimated width of the string. */
float tex_str(char *s)
{
	char *d;
	signed char c1;
	unsigned maxlen, i;
	float w, swfac;

	w = 0;
	d = tex_buf;
	maxlen = sizeof tex_buf - 1;		/* have room for EOS */
	if ((i = curft) <= 0)
		i = defft;
	swfac = cfmt.font_tb[i].swfac;
	while ((c1 = *s++) != '\0') {
		switch (c1) {
		case '\\':
			c1 = *s++;
			if (c1 == '\0') {
				*d = '\0';
				return w;
			}
			switch (c1) {
			case 'n':
				c1 = '\n';
				break;
			case 't':
				c1 = '\t';
				break;
			}
			break;
		case '$':
			if (isdigit((unsigned char) *s)
			 && (unsigned) (*s - '0') < FONT_UMAX) {
				i = *s - '0';
				if (i == 0)
					i = defft;
				swfac = cfmt.font_tb[i].swfac;
				if (--maxlen <= 0)
					break;
				*d++ = c1;
				c1 = *s++;
				goto addchar_nowidth;
			}
			if (*s == '$') {
				if (--maxlen <= 0)
					break;
				*d++ = c1;
				s++;
			}
			break;
		case '&':
			if (*s == '#' && !svg && epsf != 2) {	/* XML char ref */
				int j;
				long v;

				if (s[1] == 'x')
					i = sscanf(s, "#x%lx;%n", &v, &j);
				else
					i = sscanf(s, "#%ld;%n", &v, &j);
				if (i != 1) {
					error(0, 0, "Bad XML char reference");
					break;
				}
				if (v < 0x80) {	/* convert to UTF-8 */
					*d++ = v;
				} else if (v < 0x800) {
					*d++ = 0xc0 | (v >> 6);
					*d++ = 0x80 | (v & 0x3f);
				} else if (v < 0x10000) {
					*d++ = 0xe0 | (v >> 12);
					*d++ = 0x80 | ((v >> 6) & 0x3f);
					*d++ = 0x80 | (v & 0x3f);
				} else {
					*d++ = 0xf0 | (v >> 18);
					*d++ = 0x80 | ((v >> 12) & 0x3f);
					*d++ = 0x80 | ((v >> 6) & 0x3f);
					*d++ = 0x80 | (v & 0x3f);
				}
				s += j;
				continue;
			}
			break;
		}
		if (c1 < 0) {
			if ((c1 & 0xc0) == 0x80) {
				unsigned short unicode;

/*fixme: does not work with utf-8 on 3 characters*/
				unicode = ((d[-1] & 0x0f) << 6) | (c1 & 0x3f);
				w += cwid(unicode) * swfac;
			}
		} else if (c1 <= 5) {		/* accidentals from gchord */
			if (--maxlen < 4)
				break;
			switch (c1) {
			case 1:
				*d++ = 0xe2;
				*d++ = 0x99;
				*d++ = 0xaf;
				break;
			case 2:
				*d++ = 0xe2;
				*d++ = 0x99;
				*d++ = 0xad;
				break;
			case 3:
				*d++ = 0xe2;
				*d++ = 0x99;
				*d++ = 0xae;
				break;
			case 4:
				*d++ = 0xf0;
				*d++ = 0x9d;
				*d++ = 0x84;
				*d++ = 0xaa;
				break;
			case 5:
				*d++ = 0xf0;
				*d++ = 0x9d;
				*d++ = 0x84;
				*d++ = 0xab;
				break;
			}
			w += cwid('a') * swfac;
			continue;
		} else {
			w += cwid((unsigned short) c1) * swfac;
		}
	addchar_nowidth:
		if (--maxlen <= 0)
			break;
		*d++ = c1;
	}
	*d = '\0';
	if (maxlen <= 0)
		error(0, 0, "Text too large - ignored part: '%s'", s);
	return w;
}

#ifdef HAVE_PANGO
#define PG_SCALE (PANGO_SCALE * 72 / 96)	/* 96 DPI */

static PangoFontDescription *desc_tb[MAXFONTS];
static PangoLayout *layout = (PangoLayout *) -1;
static PangoAttrList *attrs;
static int out_pg_ft = -1;		/* current pango font */
static GString *pg_str;

/* -- initialize the pango mechanism -- */
void pg_init(void)
{
	static PangoContext *context;

	/* initialize glib */
	g_type_init();

	context = pango_font_map_create_context(
			pango_cairo_font_map_get_default());
	if (context != NULL)
		layout = pango_layout_new(context);
	if (layout == NULL) {
		error(0, 0, "pango disabled\n");
		cfmt.pango = 0;
	} else {
		pango_layout_set_wrap(layout, PANGO_WRAP_WORD);
//		pango_layout_set_spacing(layout, 0);
		pg_str = g_string_sized_new(256);
	}
}
void pg_reset_font(void)
{
	out_pg_ft = -1;
}

static void desc_font(int fnum)
{
	char font_name[128], *p;

	if (desc_tb[fnum] == 0) {
		p = font_name;
		sprintf(p, "%s 10", fontnames[fnum]);
		while (*p != '\0') {
			if (*p == '-')
				*p = ' ';
			p++;
		}
		desc_tb[fnum] = pango_font_description_from_string(font_name);
	}
}

/* output a line */
static void pg_line_output(PangoLayoutLine *line)
{
	GSList *runs_list;
	PangoGlyphInfo *glyph_info;
	char tmp[256];
	const char *fontname = NULL;
	int ret, glypharray;

	outft = -1;
	glypharray = 0;
	for (runs_list = line->runs; runs_list; runs_list = runs_list->next) {
		PangoLayoutRun *run = runs_list->data;
		PangoItem *item = run->item;
		PangoGlyphString *glyphs = run->glyphs;
		PangoAnalysis *analysis = &item->analysis;
		PangoFont *font = analysis->font;
		PangoFcFont *fc_font = PANGO_FC_FONT(font);
		FT_Face face = pango_fc_font_lock_face(fc_font);
		PangoFontDescription *ftdesc =
				pango_font_describe(font);
		int wi = pango_font_description_get_size(ftdesc);
		int i, c;

		if (pango_font_description_get_size(ftdesc) != wi) {
			wi = pango_font_description_get_size(ftdesc);
			fontname = NULL;
		}
		for (i = 0; i < glyphs->num_glyphs; i++) {
			glyph_info = &glyphs->glyphs[i];
			c = glyph_info->glyph;
			if (c == PANGO_GLYPH_EMPTY)
				continue;
			if (c & PANGO_GLYPH_UNKNOWN_FLAG) {
				c &= ~PANGO_GLYPH_UNKNOWN_FLAG;
				error(0, 0, "char %04x not treated\n", c);
				continue;
			}

			ret = FT_Load_Glyph(face,
					c,		// PangoGlyph = index
					FT_LOAD_NO_SCALE);
			if (ret != 0) {
				error(0, 0, "freetype error %d\n", ret);
			} else if (FT_HAS_GLYPH_NAMES(face)) {
				if (FT_Get_Postscript_Name(face) != fontname) {
					fontname = FT_Get_Postscript_Name(face);
					if (glypharray)
						a2b("]glypharray");
						a2b("\n");
					a2b("/%s %.1f selectfont[",
						fontname,
						(float) wi / PG_SCALE);
					glypharray = 1;
				}
				FT_Get_Glyph_Name((FT_FaceRec *) face, c,
						tmp, sizeof tmp);
				a2b("/%s", tmp);
			} else {
				a2b("%% glyph: %s %d\n",
					FT_Get_Postscript_Name(face), c);
			}
		}
		pango_fc_font_unlock_face(fc_font);
	}
	if (glypharray)
		a2b("]glypharray");
}

static void str_font_change(int start,
			int end)
{
	struct FONTSPEC *f;
	int fnum;
	PangoAttribute *attr1, *attr2;;

	f = &cfmt.font_tb[curft];
	fnum = f->fnum;
	if (f->size == 0) {
		error(0, 0, "Font \"%s\" with a null size - set to 8",
			fontnames[fnum]);
		f->size = 8;
	}
	desc_font(fnum);
	
	attr1 = pango_attr_font_desc_new(desc_tb[fnum]);
	attr1->start_index = start;
	attr1->end_index = end;
	pango_attr_list_insert(attrs, attr1);
	attr2 = pango_attr_size_new((int) (f->size * PG_SCALE));
	attr2->start_index = start;
	attr2->end_index = end;
	pango_attr_list_insert(attrs, attr2);
}

static void str_set_font(char *p)
{
	GString *str;
	char *q;
	int start;

	str = pg_str;
	start = str->len;
	q = p;
	while (*p != '\0') {
		switch (*p) {
		case '$':
			if (isdigit((unsigned char) p[1])
			 && (unsigned) (p[1] - '0') < FONT_UMAX) {
				if (p > q)
					str = g_string_append_len(str, q, p - q);
				if (curft != p[1] - '0') {
					str_font_change(start, str->len);
					start = str->len;
					curft = p[1] - '0';
					if (curft == 0)
						curft = defft;
				}
				p += 2;
				q = p;
				continue;
			}
			if (p[1] == '$') {
				str = g_string_append_len(str, q, p - q);
				q = ++p;
			}
			break;
		}
		p++;
	}
	if (p > q) {
		str = g_string_append_len(str, q, p - q);
		str_font_change(start, str->len);
	}
	pg_str = str;
}

/* -- output a string using the pango and freetype libraries -- */
static void str_pg_out(char *p, int action)
{
	PangoLayoutLine *line;
	int wi;
	float w;

//fixme: test
//a2b("\n%% t: '%s'\n", p);
	if (out_pg_ft != curft)
		out_pg_ft = -1;

	/* guitar chord with TABs */
	if (action == A_GCHEXP) {
		char *q;

		/* get the inter TAB width (see draw_gchord) */
		q = mbf - 1;
		while (q[-1] != ' ')
			q--;
		mbf = q;
		w = atof(q);
		for (;;) {
			q = strchr(p, '\t');
			if (!q)
				break;
			*q = '\0';
			str_pg_out(p, A_LEFT);
			a2b(" %.1f 0 RM ", w);
			p = q + 1;
		}
	}

	attrs = pango_attr_list_new();
	str_set_font(p);

	pango_layout_set_text(layout, pg_str->str, pg_str->len);
	pango_layout_set_attributes(layout, attrs);

	/* only one line */
	line = pango_layout_get_line_readonly(layout, 0);
	switch (action) {
	case A_CENTER:
	case A_RIGHT:
		pango_layout_get_size(layout, &wi, NULL);
		if (action == A_CENTER)
			wi /= 2;
//		w = (float) wi / PG_SCALE;
		w = (float) wi / PANGO_SCALE;
		a2b("-%.1f 0 RM ", w);
		break;
	}
	pg_line_output(line);
	pango_layout_set_attributes(layout, NULL);
	pg_str = g_string_truncate(pg_str, 0);
	pango_attr_list_unref(attrs);
}

/* output a justified or filled paragraph */
static void pg_para_output(int job)
{
	GSList *lines, *runs_list;
	PangoLayoutLine *line;
	PangoGlyphInfo *glyph_info;
	char tmp[256];
	const char *fontname = NULL;
	int ret, glypharray;
	int wi;
	float y;

	pango_layout_set_text(layout, pg_str->str,
			pg_str->len - 1);	/* remove the last space */
	pango_layout_set_attributes(layout, attrs);
	outft = -1;
	glypharray = 0;
	wi = 0;
	y = 0;
	lines = pango_layout_get_lines_readonly(layout);

	for (; lines; lines = lines->next) {
		PangoRectangle pos;

		line = lines->data;
		pango_layout_line_get_extents(line, NULL, &pos);
		y += (float) pos.height
				* .87		/* magic! */
				/ PANGO_SCALE;

		for (runs_list = line->runs; runs_list; runs_list = runs_list->next) {
			PangoLayoutRun *run = runs_list->data;
			PangoItem *item = run->item;
			PangoGlyphString *glyphs = run->glyphs;
			PangoAnalysis *analysis = &item->analysis;
			PangoFont *font = analysis->font;
			PangoFcFont *fc_font = PANGO_FC_FONT(font);
			FT_Face face = pango_fc_font_lock_face(fc_font);
			PangoFontDescription *ftdesc =
					pango_font_describe(font);
			int i, g, set_move, x;

			if (pango_font_description_get_size(ftdesc) != wi) {
				wi = pango_font_description_get_size(ftdesc);
				fontname = NULL;
			}
//printf("font size: %.2f\n", (float) wi / PG_SCALE);

			pango_layout_index_to_pos(layout, item->offset, &pos);
			x = pos.x;
			set_move = 1;
			for (i = 0; i < glyphs->num_glyphs; i++) {
				glyph_info = &glyphs->glyphs[i];
				g = glyph_info->glyph;
				if (g == PANGO_GLYPH_EMPTY)
					continue;
				if (set_move) {
					set_move = 0;
					if (glypharray) {
						a2b("]glypharray");
						glypharray = 0;
					}
					a2b("\n");
					a2b("%.2f %.2f M ",
						(float) x / PANGO_SCALE, -y);
				}
				x += glyph_info->geometry.width;
				if (g & PANGO_GLYPH_UNKNOWN_FLAG) {
					g &= ~PANGO_GLYPH_UNKNOWN_FLAG;
					error(0, 0, "char %04x not treated\n", g);
					continue;
				}

				ret = FT_Load_Glyph(face,
						g,		// PangoGlyph = index
						FT_LOAD_NO_SCALE);
				if (ret != 0) {
					fprintf(stdout, "%%%% freetype error %d\n", ret);
				} else if (FT_HAS_GLYPH_NAMES(face)) {
					if (FT_Get_Postscript_Name(face) != fontname) {
						fontname = FT_Get_Postscript_Name(face);
						if (glypharray)
							a2b("]glypharray");
						a2b("\n/%s %.1f selectfont[",
							fontname,
							(float) wi / PG_SCALE);
						glypharray = 1;
					}
					FT_Get_Glyph_Name((FT_FaceRec *) face, g,
							tmp, sizeof tmp);
					if (job == T_JUSTIFY
					 && strcmp(tmp, "space") == 0) {
						set_move = 1;
						continue;
					}
					if (!glypharray) {
						a2b("[");
						glypharray = 1;
					}
					a2b("/%s", tmp);
				} else {
					a2b("%% glyph: %s %d\n",
						FT_Get_Postscript_Name(face), g);
				}
			}
			pango_fc_font_unlock_face(fc_font);
			if (glypharray) {
				a2b("]glypharray\n");
				glypharray = 0;
			}
		}
		if (glypharray) {
			a2b("]glypharray\n");
			glypharray = 0;
		}
	}
	bskip(y);
	pango_layout_set_attributes(layout, NULL);
	pg_str = g_string_truncate(pg_str, 0);
}

/* output of filled / justified text */
static void pg_write_text(char *s, int job, float baseskip)
{
	char *p;

	curft = defft;
	pango_layout_set_width(layout, strlw * PANGO_SCALE);
	pango_layout_set_justify(layout, job == T_JUSTIFY);
	attrs = pango_attr_list_new();

	p = s;
	while (*p != '\0') {
		if (*p++ != '\n')
			continue;
		if (*p == '\n') {		/* if empty line */
			p[-1] = '\0';
			tex_str(s);
			str_set_font(tex_buf);
			if (pg_str->len > 0)
				pg_para_output(job);
			bskip(baseskip * 0.5);
			buffer_eob();
			s = ++p;
			continue;
		}
//fixme: maybe not useful
		p [-1] = ' ';
	}
	tex_str(s);
	str_set_font(tex_buf);
	if (pg_str->len)
		pg_para_output(job);
	pango_attr_list_unref(attrs);
}

/* check if pango is needed */
static int is_latin(unsigned char *p)
{
	while (*p != '\0') {
		if (*p >= 0xc6) {
			if (*p == 0xe2) {
				if (p[1] != 0x99
				 || p[2] < 0xad || p[2] > 0xaf)
					return 0;
				p += 2;
			} else if (*p == 0xf0) {
				if (p[1] != 0x9d
				 || p[2] != 0x84
				 || p[3] < 0xaa || p[3] > 0xab)
					return 0;
			} else {
				return 0;
			}
		}
		p++;
	}
	return 1;
}
#endif /* HAVE_PANGO */

/* -- set the default font of a string -- */
void str_font(int ft)
{
	curft = defft = ft;
}

/* -- get the current and default fonts -- */
void get_str_font(int *cft, int *dft)
{
	*cft = curft;
	*dft = defft;
}

/* -- set the current and default fonts -- */
void set_str_font(int cft, int dft)
{
	curft = cft;
	defft = dft;
}

/* close a string */
static void str_end(int end)
{
	if (strtx & TX_STR) {
		a2b(")");
		strtx &= ~TX_STR;
		if (!(strtx & TX_ARR))
			a2b("%s", strop);
	}
	if (!end || !(strtx & TX_ARR))
		return;
	strtx &= ~TX_ARR;
	a2b("]arrayshow");
}

/* -- output one string -- */
static void str_ft_out1(char *p, int l)
{
	if (curft != outft) {
		str_end(1);
		a2b(" ");
		set_font(curft);
	}
	if (!(strtx & TX_STR)) {
		a2b("(");
		strtx |= TX_STR;
	}
	a2b("%.*s", l, p);
}

static char *strop_tb[] = {	/* index = action (A_xxxx) */
	"show",
	"showc",
	"showr",
	"lyshow",
	"gcshow",
	"anshow",
	"gxshow",
};

/* -- output a string and the font changes -- */
static void str_ft_out(char *p, int end)
{
	int use_glyph;
	char *q;

	use_glyph = !svg && epsf != 2 &&		/* not SVG */
		 get_font_encoding(curft) == 0;		/* utf-8 */
	q = p;
	while (*p != '\0') {
		if ((unsigned char) *p >= 0x80
		 && use_glyph) {
			if (p > q) {
				str_ft_out1(q, p - q);
			} else if (curft != outft) {
				str_end(1);
				a2b(" ");
				set_font(curft);
			}
			str_end(0);
			if (!(strtx & TX_ARR)) {
				a2b("[");
				strtx |= TX_ARR;
			}
			q = p = glyph_out(p);
			continue;
		}
		switch ((unsigned char) *p) {
		case '$':
			if (isdigit((unsigned char) p[1])
			 && (unsigned) (p[1] - '0') < FONT_UMAX) {
				if (p > q)
					str_ft_out1(q, p - q);
				if (curft != p[1] - '0') {
					curft = p[1] - '0';
					if (curft == 0)
						curft = defft;
					use_glyph = !svg && epsf != 2 &&
						 get_font_encoding(curft) == 0;
				}
				p += 2;
				q = p;
				continue;
			}
			if (p[1] == '$') {
				str_ft_out1(q, p - q);
				q = ++p;
			}
			break;
		case '(':
		case ')':
		case '\\':
			if (p > q)
				str_ft_out1(q, p - q);
			str_ft_out1("\\", 1);
			q = p;
			break;
		}
		p++;
	}
	if (p > q)
		str_ft_out1(q, p - q);
	if (end && strtx)
		str_end(1);
}

/* check if some non ASCII characters */
static int non_ascii_p(char *p)
{
	while (*p != '\0') {
		if ((signed char) *p++ < 0)
			return 1;
	}
	return 0;
}

/* -- output a string, handling the font changes -- */
void str_out(char *p, int action)
{
	if (curft <= 0)		/* first call */
		curft = defft;

	/* special case when font change at start of text */
	if (*p == '$' && isdigit((unsigned char) p[1])
	 && (unsigned) (p[1] - '0') < FONT_UMAX) {
		if (curft != p[1] - '0') {
			curft = p[1] - '0';
			if (curft == 0)
				curft = defft;
		}
		p += 2;
	}

#ifdef HAVE_PANGO
//fixme: pango KO if user modification of ly/gc/an/gxshow
	/* use pango if some characters are out of the utf-array (in syms.c) */
	if (cfmt.pango) {
		if (cfmt.pango == 2 || !is_latin((unsigned char *) p)) {
			str_pg_out(p, action);	/* output the string */
			return;
		}
	}
#endif

	/* direct output if no font change
	 * nor non ASCII characters */
	if (strchr(p, '$') == 0
	 && !non_ascii_p(p)) {
		strop = strop_tb[action];
		str_ft_out(p, 1);		/* output the string */
		return;
	}

	/* if not left aligned, build a PS function */
	switch (action) {
	case A_CENTER:
	case A_RIGHT:
		if (!svg && epsf != 2) {
			a2b("/str{");
			outft = -1;
			strop = "strop";
			break;
		}
		/* fall thru */
	default:
		strop = strop_tb[action];
		break;
	}

	str_ft_out(p, 1);		/* output the string */

	/* if not left aligned, call the PS function */
	if (svg || epsf == 2)
		return;
	if (action == A_CENTER || action == A_RIGHT) {
		a2b("}def\n"
			"strw w");
		if (action == A_CENTER)
			a2b(" 0.5 mul");
		a2b(" neg 0 RM str");
	}
}

/* -- output a string with TeX translation -- */
void put_str(char *str, int action)
{
	tex_str(str);
	str_out(tex_buf, action);
	a2b("\n");
}

/* -- output a header information -- */
static void put_inf(struct SYMBOL *s)
{
	char *p;

	p = s->as.text;
	if (p[1] == ':')
		p += 2;
	while (isspace((unsigned char) *p))
		p++;
	put_str(p, A_LEFT);
}

/* -- output a header format '111 (222)' -- */
static void put_inf2r(struct SYMBOL *s1,
			struct SYMBOL *s2,
			int action)
{
	char buf[256], *p, *q;

	if (!s1) {
		s1 = s2;
		s2 = NULL;
	}
	p = &s1->as.text[2];
	if (s1->as.text[0] == 'T')
		p = trim_title(p, s1);
	if (s2) {
		buf[sizeof buf - 1] = '\0';
		strncpy(buf, p, sizeof buf - 1);
		q = buf + strlen(buf);
		if (q < buf + sizeof buf - 4) {
			*q++ = ' ';
			*q++ = '(';
			p = &s2->as.text[2];
			strncpy(q, p, buf + sizeof buf - 2 - q);
			q += strlen(q);
			*q++ = ')';
			*q = '\0';
		}
		p = buf;
	}
	put_str(p, action);
}

/* -- write a text block (%%begintext / %%text / %%center) -- */
void write_text(char *cmd, char *s, int job)
{
	int nw;
#ifdef HAVE_PANGO
	int do_pango;
#endif
	float baseskip, strw;
	char *p;
	struct FONTSPEC *f;

	str_font(TEXTFONT);
	strlw = ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
		- cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale;

	f = &cfmt.font_tb[defft];
	baseskip = f->size * cfmt.lineskipfac;

	/* follow lines */
	switch (job) {
	case T_LEFT:
	case T_CENTER:
	case T_RIGHT:
		switch (job) {
		case T_LEFT:
#if T_LEFT != A_LEFT
			job = A_LEFT;
#endif
			break;
		case T_CENTER:
#if T_CENTER != A_CENTER
			job = A_CENTER;
#endif
			break;
		default:
#if T_RIGHT != A_RIGHT
			job = A_RIGHT;
#endif
			break;
		}
		while (*s != '\0') {
			p = s;
			while (*p != '\0' && *p != '\n')
				p++;
			if (*p != '\0')
				*p++ = '\0';
			if (*s == '\0') {
				bskip(baseskip * 0.5);
				buffer_eob();
			}
			bskip(baseskip);
			switch (job) {
			case A_LEFT:
				a2b("0 0 M");
				break;
			case A_CENTER:
				a2b("%.1f 0 M", strlw * 0.5);
				break;
			default:
				a2b("%.1f 0 M", strlw);
				break;
			}
			put_str(s, job);
			s = p;
		}
		bskip(baseskip * 0.5);
		buffer_eob();
		return;
	}

	/* fill or justify lines */
#ifdef HAVE_PANGO
	do_pango = cfmt.pango;
	if (do_pango == 1)
		do_pango = !is_latin((unsigned char *) s);
	if (do_pango) {
		pg_write_text(s, job, baseskip);
		bskip(cfmt.font_tb[TEXTFONT].size * cfmt.parskipfac);
		buffer_eob();
		return;
	}
#endif
	curft = defft;
	nw = 0;					/* number of words */
	strw = 0;				/* have gcc happy */
	while (*s != '\0') {
		float lw;

		if (nw == 0) {			/* if new paragraph */
			bskip(baseskip);
			a2b("0 0 M");
			if (job == T_FILL) {
				strop = "show";
			} else {
				a2b("/str{");
				outft = -1;
				strop = "strop";
			}
			strw = 0;		/* current line width */
		}
		if (*s == '\n') {		/* empty line */
			if (strtx) {
				str_end(1);
				if (job == T_JUSTIFY)
					a2b("}def\n"
					    "/strop/show load def str");
			}
			a2b("\n");
			bskip(baseskip * 0.5);
			buffer_eob();
			nw = 0;
			while (isspace((unsigned char) *s))
				s++;
			continue;
		}

		/* get a word */
		p = s;
		while (*p != '\0' && !isspace((unsigned char) *p))
			p++;
		if (*p != '\0') {
			char *q;

			q = p;
			if (*p != '\n') {
				do {
					p++;
				} while (*p != '\n' && isspace((unsigned char) *p));
			}
			if (*p == '\n')
				p++;
			*q = '\0';
		}

		lw = tex_str(s);
		if (strw + lw > strlw) {
			str_end(1);
			if (job == T_JUSTIFY) {
				if (svg || epsf == 2)
					a2b("}def\n"
						"%.1f jshow"
						" str",
						strlw);
				else
					a2b("}def\n"
						"strw"
						"/w %.1f w sub %d div def"
						"/strop/jshow load def str",
						strlw, nw);
			}
			a2b("\n");
			bskip(cfmt.font_tb[curft].size * cfmt.lineskipfac);
			a2b("0 0 M");
			if (job == T_JUSTIFY) {
				a2b("/str{");
				outft = -1;
			}
			nw = 0;
			strw = 0;
		}

		if (nw != 0) {
			str_ft_out1(" ", 1);
			strw += cwid(' ') * cfmt.font_tb[curft].swfac;
		}
		str_ft_out(tex_buf, 0);
		strw += lw;
		nw++;

		s = p;
	}
	if (strtx) {
		str_end(1);
		if (job == T_JUSTIFY)
			a2b("}def\n"
				"/strop/show load def str");
	}
//	if (mbf[-1] != '\n')
		a2b("\n");
	bskip(cfmt.font_tb[TEXTFONT].size * cfmt.parskipfac);
	buffer_eob();
}

/* -- output a line of words after tune -- */
static int put_wline(char *p,
			float x,
			int right)
{
	char *q, *r, sep;

	while (isspace((unsigned char) *p))
		p++;
	if (*p == '$' && isdigit((unsigned char) p[1])
	 && (unsigned) (p[1] - '0') < FONT_UMAX) {
		if (curft != p[1] - '0') {
			curft = p[1] - '0';
			if (curft == 0)
				curft = defft;
		}
		p += 2;
	}
	r = 0;
	q = p;
	if (isdigit((unsigned char) *p) || p[1] == '.') {
		while (*p != '\0') {
			p++;
			if (*p == ' '
			 || p[-1] == ':'
			 || p[-1] == '.')
				break;
		}
		r = p;
		while (*p == ' ')
			p++;
	}

	if (r != 0) {
		sep = *r;
		*r = '\0';
		a2b("%.1f 0 M", x);
		put_str(q,  A_RIGHT);
		*r = sep;
	}
	if (*p != '\0') {
		a2b("%.1f 0 M", x + 5);
		put_str(p, A_LEFT);
	}
	return *p == '\0' && r == 0;
}

/* -- output the words after tune -- */
void put_words(struct SYMBOL *words)
{
	struct SYMBOL *s, *s_end, *s2;
	char *p;
	int i, n, have_text, max2col;
	float middle;

	str_font(WORDSFONT);

	/* see if we may have 2 columns */
	middle = 0.5 * ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
		- cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale;
	max2col = (int) ((middle - 45.) / (cwid('a') * cfmt.font_tb[WORDSFONT].swfac));
	n = 0;
	have_text = 0;
	for (s = words; s != 0; s = s->next) {
		p = &s->as.text[2];
/*fixme:utf8*/
		if ((int) strlen(p) > max2col) {
			n = 0;
			break;
		}
		if (*p == '\0') {
			if (have_text) {
				n++;
				have_text = 0;
			}
		} else {
			have_text = 1;
		}
	}
	if (n > 0) {
		n++;
		n /= 2;
		i = n;
		have_text = 0;
		s_end = words;
		for (;;) {
			p = &s_end->as.text[2];
			while (isspace((unsigned char) *p))
				p++;
			if (*p == '\0') {
				if (have_text && --i <= 0)
					break;
				have_text = 0;
			} else {
				have_text = 1;
			}
			s_end = s_end->next;
		}
		s2 = s_end->next;
	} else {
		s_end = 0;
		s2 = 0;
	}

	/* output the text */
	bskip(cfmt.wordsspace);
	for (s = words; s != 0 || s2 != 0; ) {
//fixme:should also permit page break on stanza start
		if (s && s->as.text[2] == '\0')
			buffer_eob();
		bskip(cfmt.lineskipfac * cfmt.font_tb[WORDSFONT].size);
		if (s) {
			put_wline(&s->as.text[2], 45., 0);
			s = s->next;
			if (s == s_end)
				s = NULL;
		}
		if (s2) {
			if (put_wline(&s2->as.text[2], 20. + middle, 1)) {
				if (--n == 0) {
					if (s) {
						n++;
					} else if (s2->next) {

						/* center the last words */
/*fixme: should compute the width average.. */
						middle *= 0.6;
					}
				}
			}
			s2 = s2->next;
		}
	}
	buffer_eob();
}

/* -- output history -- */
void put_history(void)
{
	struct SYMBOL *s, *s2;
	int font;
	unsigned u;
	float w, h;
	char tmp[265];

	font = 0;
	for (s = info['I' - 'A']; s; s = s->next) {
		u = s->as.text[0] - 'A';
		if (!(cfmt.fields[0] & (1 << u))
		 || (s2 = info[u]) == NULL)
			continue;
		if (!font) {
			bskip(cfmt.textspace);
			str_font(HISTORYFONT);
			font = 1;
		}
		get_str(tmp, &s->as.text[1], sizeof tmp);
		w = tex_str(tmp);
		h = cfmt.font_tb[HISTORYFONT].size * cfmt.lineskipfac;
		set_font(HISTORYFONT);
		a2b("0 0 M(%s)show ", tex_buf);
		for (;;) {
			put_inf(s2);
			if ((s2 = s2->next) == NULL)
				break;
			bskip(h);
			a2b("%.2f 0 M ", w);
		}
		bskip(h * 1.2);
		buffer_eob();
	}
}

/* -- move trailing "The" to front, set to uppercase letters or add xref -- */
char *trim_title(char *p, struct SYMBOL *title)
{
	char *b, *q, *r;
static char buf[STRL1];

	q = NULL;
	if (cfmt.titletrim) {
		q = strrchr(p, ',');
		if (q) {
			if (q[1] != ' ' || !isupper((unsigned char) q[2])
			 || strlen(q) > 7	/* word no more than 4 characters */
			 || strchr(q + 2, ' '))
				q = NULL;
		}
	}
	if (title != info['T' - 'A']
	 || !(cfmt.fields[0] & (1 << ('X' - 'A'))))
		title = 0;
	if (!q
	 && !title
	 && !cfmt.titlecaps)
		return p;		/* keep the title as it is */
	b = buf;
	r = &info['X' - 'A']->as.text[2];
	if (title
	 && *r != '\0') {
		if (strlen(p) + strlen(r) + 3 >= sizeof buf) {
			error(1, 0, "Title or X: too long");
			return p;
		}
		b += sprintf(b, "%s.  ", r);
	} else {
		if (strlen(p) >= sizeof buf) {
			error(1, 0, "Title too long");
			return p;
		}
	}
	if (q)
		sprintf(b, "%s %.*s", q + 2, (int) (q - p), p);
	else
		strcpy(b, p);
	if (cfmt.titlecaps)
		cap_str(buf);
	return buf;
}

/* -- write a title -- */
void write_title(struct SYMBOL *s)
{
	char *p;
	float sz;

	p = &s->as.text[2];
	if (*p == '\0')
		return;
	p = trim_title(p, s);
	if (s == info['T' - 'A']) {
		sz = cfmt.font_tb[TITLEFONT].size;
		bskip(cfmt.titlespace + sz);
		str_font(TITLEFONT);
	} else {
		sz = cfmt.font_tb[SUBTITLEFONT].size;
		bskip(cfmt.subtitlespace + sz);
		str_font(SUBTITLEFONT);
	}
	if (cfmt.titleleft)
		a2b("0");
	else
		a2b("%.1f",
		     0.5 * ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
			- cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale);
	a2b(" %.1f M ", sz * 0.2);
	put_str(p, cfmt.titleleft ? A_LEFT : A_CENTER);
}

/* -- write heading with format -- */
static void write_headform(float lwidth)
{
	char *p, *q;
	struct SYMBOL *s;
	struct FONTSPEC *f;
	int align, i;
	unsigned j;
	float x, y, xa[3], ya[3], sz, yb[3];	/* !! see action A_xxx */
	char inf_nb[26];
	INFO inf_s;
	char inf_ft[26];
	float inf_sz[26];
	char fmt[64];

	memset(inf_nb, 0, sizeof inf_nb);
	memset(inf_ft, HISTORYFONT, sizeof inf_ft);
	inf_ft['A' - 'A'] = INFOFONT;
	inf_ft['C' - 'A'] = COMPOSERFONT;
	inf_ft['O' - 'A'] = COMPOSERFONT;
	inf_ft['P' - 'A'] = PARTSFONT;
	inf_ft['Q' - 'A'] = TEMPOFONT;
	inf_ft['R' - 'A'] = INFOFONT;
	inf_ft['T' - 'A'] = TITLEFONT;
	inf_ft['X' - 'A'] = TITLEFONT;
	memcpy(inf_s, info, sizeof inf_s);
	memset(inf_sz, 0, sizeof inf_sz);
	inf_sz['A' - 'A'] = cfmt.infospace;
	inf_sz['C' - 'A'] = cfmt.composerspace;
	inf_sz['O' - 'A'] = cfmt.composerspace;
	inf_sz['R' - 'A'] = cfmt.infospace;
	p = cfmt.titleformat;
	j = 0;
	for (;;) {
		while (isspace((unsigned char) *p))
			p++;
		if (*p == '\0')
			break;
		i = *p - 'A';
		if ((unsigned) i < 26) {
			inf_nb[i]++;
			switch (p[1]) {
			default:
				align = A_CENTER;
				break;
			case '1':
				align = A_RIGHT;
				p++;
				break;
			case '-':
				align = A_LEFT;
				p++;
				break;
			}
			if (j < sizeof fmt - 4) {
				fmt[j++] = i;
				fmt[j++] = align;
			}
		} else if (*p == ',') {
			if (j < sizeof fmt - 3)
				fmt[j++] = 126;		/* next line */
		} else if (*p == '+') {
			if (j > 0 && fmt[j - 1] < 125
			 && j < sizeof fmt - 4) {
				fmt[j++] = 125;		/* concatenate */
				fmt[j++] = 0;
			}
/*new fixme: add free text "..." ?*/
		}
		p++;
	}
	fmt[j++] = 126;			/* newline */
	fmt[j] = 127;			/* end of format */

	ya[0] = ya[1] = ya[2] = cfmt.titlespace;;
	xa[0] = 0;
	xa[1] = lwidth * 0.5;
	xa[2] = lwidth;

	p = fmt;
	for (;;) {
		yb[0] = yb[1] = yb[2] = y = 0;
		q = p;
		for (;;) {
			i = *q++;
			if (i >= 126)		/* if newline */
				break;
			align = *q++;
			if (yb[align] != 0
			 || i == 125)
				continue;
			s = inf_s[i];
			if (s == 0 || inf_nb[i] == 0)
				continue;
			j = inf_ft[i];
			f = &cfmt.font_tb[j];
			sz = f->size * 1.1 + inf_sz[i];
			if (y < sz)
				y = sz;
			yb[align] = sz;
/*fixme:should count the height of the concatenated field*/
		}
		for (i = 0; i < 3; i++)
			ya[i] += y - yb[i];
		for (;;) {
			i = *p++;
			if (i >= 126)		/* if newline */
				break;
			align = *p++;
			if (i == 125)
				continue;
			s = inf_s[i];
			if (s == 0 || inf_nb[i] == 0)
				continue;
			j = inf_ft[i];
			str_font(j);
			x = xa[align];
			f = &cfmt.font_tb[j];
			sz = f->size * 1.1 + inf_sz[i];
			y = ya[align] + sz;
			a2b("%.1f %.1f M ", x, -y);
			if (*p == 125) {	/* concatenate */
			    p += 2;
/*fixme: do it work with different fields*/
			    if (*p == i && p[1] == align
			     && s->next != 0) {
				char buf[256], *r;

				q = s->as.text;
				if (q[1] == ':')
					q += 2;
				while (isspace((unsigned char) *q))
					q++;
				if (i == 'T' - 'A')
					q = trim_title(q, s);
				strncpy(buf, q, sizeof buf - 1);
				buf[sizeof buf - 1] = '\0';
				j = strlen(buf);
				if (j < sizeof buf - 1) {
					buf[j] = ' ';
					buf[j + 1] = '\0';
				}
				s = s->next;
				q = s->as.text;
				if (q[1] == ':')
					q += 2;
				while (isspace((unsigned char) *q))
					q++;
				if (s->as.text[0] == 'T' && s->as.text[1] == ':')
					q = trim_title(q, s);
				r = buf + strlen(buf);
				strncpy(r, q, buf + sizeof buf - r - 1);
				tex_str(buf);
				str_out(tex_buf, align);
				a2b("\n");
				inf_nb[i]--;
				p += 2;
			    } else {
				put_inf2r(s, 0, align);
			    }
			} else if (i == 'Q' - 'A') {	/* special case for tempo */
				if (align != A_LEFT) {
					float w;

					w = -tempo_width(s);
					if (align == A_CENTER)
						w *= 0.5;
					a2b("%.1f 0 RM ", w);
				}
				write_tempo(s, 0, 0.75);
				info['Q' - 'A'] = 0;	/* don't display in tune */
			} else {
				put_inf2r(s, 0, align);
			}
			if (inf_s[i] == info['T' - 'A']) {
				inf_ft[i] = SUBTITLEFONT;
				str_font(SUBTITLEFONT);
				f = &cfmt.font_tb[SUBTITLEFONT];
				inf_sz[i] = cfmt.subtitlespace;
				sz = f->size * 1.1 + inf_sz[i];
			}
			s = s->next;
			if (inf_nb[i] == 1) {
				while (s) {
					y += sz;
					a2b("%.1f %.1f M ", x, -y);
					put_inf2r(s, 0, align);
					s = s->next;
				}
			}
			inf_s[i] = s;
			inf_nb[i]--;
			ya[align] = y;
		}
		if (ya[1] > ya[0])
			ya[0] = ya[1];
		if (ya[2] > ya[0])
			ya[0] = ya[2];
		if (*p == 127) {
			bskip(ya[0]);
			break;
		}
		ya[1] = ya[2] = ya[0];
	}
}

/* -- output the tune heading -- */
void write_heading(struct abctune *t)
{
	struct SYMBOL *s, *rhythm, *area, *author, *composer, *origin;
	float lwidth, down1, down2;

	lwidth = ((cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
		- cfmt.leftmargin - cfmt.rightmargin) / cfmt.scale;

	if (cfmt.titleformat != 0 && cfmt.titleformat[0] != '\0') {
		write_headform(lwidth);
		bskip(cfmt.musicspace);
		return;
	}

	/* titles */
	if (cfmt.fields[0] & (1 << ('T' - 'A'))) {
		for (s = info['T' - 'A']; s; s = s->next)
			write_title(s);
	}

	/* rhythm, composer, origin */
	down1 = cfmt.composerspace + cfmt.font_tb[COMPOSERFONT].size;
	rhythm = (first_voice->key.mode >= BAGPIPE
			&& !cfmt.infoline
			&& (cfmt.fields[0] & (1 << ('R' - 'A'))))
					? info['R' - 'A'] : 0;
	if (rhythm) {
		str_font(COMPOSERFONT);
		a2b("0 %.1f M ",
		     -(cfmt.composerspace + cfmt.font_tb[COMPOSERFONT].size));
		put_inf(rhythm);
		down1 -= cfmt.font_tb[COMPOSERFONT].size;
	}
	area = author = 0;
	if (cfmt.fields[0] & (1 << ('A' - 'A'))) {
		if (t->abc_vers != (2 << 16))
			area = info['A' - 'A'];
		else
			author = info['A' - 'A'];
	}
	composer = (cfmt.fields[0] & (1 << ('C' - 'A'))) ? info['C' - 'A'] : NULL;
	origin = (cfmt.fields[0] & (1 << ('O' - 'A'))) ? info['O' - 'A'] : NULL;
	if (composer || origin || author) {
		float xcomp;
		int align;

		str_font(COMPOSERFONT);
		bskip(cfmt.composerspace);
		if (cfmt.aligncomposer < 0) {
			xcomp = 0;
			align = A_LEFT;
		} else if (cfmt.aligncomposer == 0) {
			xcomp = lwidth * 0.5;
			align = A_CENTER;
		} else {
			xcomp = lwidth;
			align = A_RIGHT;
		}
		down2 = down1;
		if (author) {
			for (;;) {
				bskip(cfmt.font_tb[COMPOSERFONT].size);
				down2 += cfmt.font_tb[COMPOSERFONT].size;
				a2b("0 0 M ");
				put_inf(author);
				if ((author = author->next) == NULL)
					break;
			}
		}
		if (composer || origin) {
			if (cfmt.aligncomposer >= 0
			 && down1 != down2)
				bskip(down1 - down2);
			s = composer;
			for (;;) {
				bskip(cfmt.font_tb[COMPOSERFONT].size);
				a2b("%.1f 0 M ", xcomp);
				put_inf2r(s,
					  (!s || !s->next) ? origin : NULL,
					  align);
				if (!s)
					break;
				if ((s = s->next) == NULL)
					break;
				down1 += cfmt.font_tb[COMPOSERFONT].size;
			}
			if (down2 > down1)
				bskip(down2 - down1);
		}

		if (cfmt.fields[0] & (1 << ('R' - 'A')))
			rhythm = rhythm ? 0 : info['R' - 'A'];
		if ((rhythm || area) && cfmt.infoline) {

			/* if only one of rhythm or area then do not use ()'s
			 * otherwise output 'rhythm (area)' */
			str_font(INFOFONT);
			bskip(cfmt.font_tb[INFOFONT].size + cfmt.infospace);
			a2b("%.1f 0 M ", lwidth);
			put_inf2r(rhythm, area, A_RIGHT);
			down1 += cfmt.font_tb[INFOFONT].size + cfmt.infospace;
		}
		down2 = 0;
	} else {
		down2 = cfmt.composerspace + cfmt.font_tb[COMPOSERFONT].size;
	}

	/* parts */
	if (info['P' - 'A'] != 0
	 && (cfmt.fields[0] & (1 << ('P' - 'A')))) {
		down1 = cfmt.partsspace + cfmt.font_tb[PARTSFONT].size - down1;
		if (down1 > 0)
			down2 += down1;
		if (down2 > 0.01)
			bskip(down2);
		str_font(PARTSFONT);
		a2b("0 0 M ");
		put_inf(info['P' - 'A']);
		down2 = 0;
	}
	bskip(down2 + cfmt.musicspace);
}

/* -- memorize a PS / SVG line -- */
/* 'use' may be:
 *	'g': SVG code
 *	'p': PS code for PS output only
 *	's': PS code for SVG output only
 *	'b': PS code for PS or SVG output
 */
void user_ps_add(char *s, char use)
{
	struct u_ps *t, *r;
	int l;

	if (*s == '\0' || *s == '%')
		return;
	l = strlen(s);
	if (use == 'g') {
		t = malloc(sizeof *user_ps - sizeof user_ps->text + l + 6);
		sprintf(t->text, "%%svg %s", s);
	} else {
		t = malloc(sizeof *user_ps - sizeof user_ps->text + l + 2);
		sprintf(t->text, "%c%s", use, s);
	}
	t->next = NULL;
	if ((r = user_ps) == NULL) {
		user_ps = t;
	} else {
		while (r->next)
			r = r->next;
		r->next = t;
	}
}

/* -- output the user defined postscript sequences -- */
void user_ps_write(void)
{
	struct u_ps *t;
	char *p;

	for (t = user_ps; t; t = t->next) {
		p = t->text;
		switch (*p) {
		case '\001': {		/* PS file */
			FILE *f;
			char line[BSIZE];

			if ((f = fopen(p + 1, "r")) == NULL) {
				error(1, 0, "Cannot open PS file '%s'",
					&t->text[1]);
			} else {
				while (fgets(line, sizeof line, f))	/* copy the file */
					fputs(line, fout);
				fclose(f);
			}
			continue;
		    }
		case '%':		/* "%svg " = SVG code */
//			if (svg || epsf == 2)
//				svg_write(t->text, strlen(t->text));
			fputs(p + 5, fout);
			fputc('\n', fout);
			continue;
		case 'p':		/* PS code for PS output only */
//			if (secure || svg || epsf == 2)
//				continue;
			break;
		case 'b':		/* PS code for both PS and SVG */
			if (svg || epsf == 2) {
				svg_write(p + 1, strlen(p + 1));
				continue;
			}
//			if (secure)
//				continue;
			break;
		case 's':		/* PS code for SVG output only */
//			if (!svg && epsf != 2)
//				continue;
			svg_write(p + 1, strlen(&t->text[1]));
			continue;
		}
		fputs(p + 1, fout);
		fputc('\n', fout);
	}
}
