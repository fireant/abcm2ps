/*
 * Formatting functions.
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "abc2ps.h"

struct FORMAT cfmt;		/* current format for output */

char *fontnames[MAXFONTS];		/* list of font names */
static char font_enc[MAXFONTS];		/* font encoding */
static char def_font_enc[MAXFONTS];	/* default font encoding */
static char used_font[MAXFONTS];	/* used fonts */
static float swfac_font[MAXFONTS];	/* width scale */
static int nfontnames;
static float staffwidth;

/* format table */
static struct format {
	char *name;
	void *v;
	char type;
#define FORMAT_I 0	/* int */
#define FORMAT_R 1	/* float */
#define FORMAT_F 2	/* font spec */
#define FORMAT_U 3	/* float with unit */
#define FORMAT_B 4	/* boolean */
#define FORMAT_S 5	/* string */
	char subtype;		/* special cases - see code */
	short lock;
} format_tb[] = {
	{"abc2pscompat", &cfmt.abc2pscompat, FORMAT_B, 0},
	{"alignbars", &cfmt.alignbars, FORMAT_I, 0},
	{"aligncomposer", &cfmt.aligncomposer, FORMAT_I, 0},
	{"autoclef", &cfmt.autoclef, FORMAT_B, 0},
	{"annotationfont", &cfmt.font_tb[ANNOTATIONFONT], FORMAT_F, 0},
	{"barsperstaff", &cfmt.barsperstaff, FORMAT_I, 0},
	{"bgcolor", &cfmt.bgcolor, FORMAT_S, 0},
	{"botmargin", &cfmt.botmargin, FORMAT_U, 0},
	{"breaklimit", &cfmt.breaklimit, FORMAT_R, 3},
	{"breakoneoln", &cfmt.breakoneoln, FORMAT_B, 0},
	{"bstemdown", &cfmt.bstemdown, FORMAT_B, 0},
	{"cancelkey", &cfmt.cancelkey, FORMAT_B, 0},
	{"combinevoices", &cfmt.combinevoices, FORMAT_I, 0},
	{"composerfont", &cfmt.font_tb[COMPOSERFONT], FORMAT_F, 0},
	{"composerspace", &cfmt.composerspace, FORMAT_U, 0},
	{"contbarnb", &cfmt.contbarnb, FORMAT_B, 0},
	{"continueall", &cfmt.continueall, FORMAT_B, 0},
	{"custos", &cfmt.custos, FORMAT_B, 0},
	{"dateformat", &cfmt.dateformat, FORMAT_S, 0},
	{"dblrepbar", &cfmt.dblrepbar, FORMAT_I, 2},
	{"dynalign", &cfmt.dynalign, FORMAT_B, 0},
	{"footer", &cfmt.footer, FORMAT_S, 0},
	{"footerfont", &cfmt.font_tb[FOOTERFONT], FORMAT_F, 0},
	{"flatbeams", &cfmt.flatbeams, FORMAT_B, 0},
	{"gchordbox", &cfmt.gchordbox, FORMAT_B, 0},
	{"gchordfont", &cfmt.font_tb[GCHORDFONT], FORMAT_F, 3},
	{"graceslurs", &cfmt.graceslurs, FORMAT_B, 0},
	{"gracespace", &cfmt.gracespace, FORMAT_I, 5},
	{"header", &cfmt.header, FORMAT_S, 0},
	{"headerfont", &cfmt.font_tb[HEADERFONT], FORMAT_F, 0},
	{"historyfont", &cfmt.font_tb[HISTORYFONT], FORMAT_F, 0},
	{"hyphencont", &cfmt.hyphencont, FORMAT_B, 0},
	{"indent", &cfmt.indent, FORMAT_U, 0},
	{"infofont", &cfmt.font_tb[INFOFONT], FORMAT_F, 0},
	{"infoline", &cfmt.infoline, FORMAT_B, 0},
	{"infospace", &cfmt.infospace, FORMAT_U, 0},
	{"keywarn", &cfmt.keywarn, FORMAT_B, 0},
	{"landscape", &cfmt.landscape, FORMAT_B, 0},
	{"leftmargin", &cfmt.leftmargin, FORMAT_U, 0},
	{"lineskipfac", &cfmt.lineskipfac, FORMAT_R, 0},
	{"linewarn", &cfmt.linewarn, FORMAT_B, 0},
	{"maxshrink", &cfmt.maxshrink, FORMAT_R, 2},
	{"maxstaffsep", &cfmt.maxstaffsep, FORMAT_U, 0},
	{"maxsysstaffsep", &cfmt.maxsysstaffsep, FORMAT_U, 0},
	{"measurebox", &cfmt.measurebox, FORMAT_B, 0},
	{"measurefirst", &cfmt.measurefirst, FORMAT_I, 0},
	{"measurefont", &cfmt.font_tb[MEASUREFONT], FORMAT_F, 2},
	{"measurenb", &cfmt.measurenb, FORMAT_I, 0},
	{"micronewps", &cfmt.micronewps, FORMAT_B, 0},
	{"microscale", &cfmt.microscale, FORMAT_I, 0},
	{"musicspace", &cfmt.musicspace, FORMAT_U, 0},
	{"notespacingfactor", &cfmt.notespacingfactor, FORMAT_R, 1},
	{"oneperpage", &cfmt.oneperpage, FORMAT_B, 0},
	{"pageheight", &cfmt.pageheight, FORMAT_U, 0},
	{"pagewidth", &cfmt.pagewidth, FORMAT_U, 0},
#ifdef HAVE_PANGO
	{"pango", &cfmt.pango, FORMAT_B, 2},
#endif
	{"parskipfac", &cfmt.parskipfac, FORMAT_R, 0},
	{"partsbox", &cfmt.partsbox, FORMAT_B, 0},
	{"partsfont", &cfmt.font_tb[PARTSFONT], FORMAT_F, 1},
	{"partsspace", &cfmt.partsspace, FORMAT_U, 0},
	{"pdfmark", &cfmt.pdfmark, FORMAT_I, 0},
	{"repeatfont", &cfmt.font_tb[REPEATFONT], FORMAT_F, 0},
	{"rightmargin", &cfmt.rightmargin, FORMAT_U, 0},
	{"scale", &cfmt.scale, FORMAT_R, 0},
	{"setdefl", &cfmt.setdefl, FORMAT_B, 0},
	{"setfont-1", &cfmt.font_tb[1], FORMAT_F, 0},
	{"setfont-2", &cfmt.font_tb[2], FORMAT_F, 0},
	{"setfont-3", &cfmt.font_tb[3], FORMAT_F, 0},
	{"setfont-4", &cfmt.font_tb[4], FORMAT_F, 0},
#if FONT_UMAX!=5
#	error Bad number of user fonts
#endif
//	{"shifthnote", &cfmt.shiftunison, FORMAT_B, 0},	/*to remove*/
	{"shiftunison", &cfmt.shiftunison, FORMAT_I, 0},
	{"shiftunisson", &cfmt.shiftunison, FORMAT_I, 0}, /*to remove*/
	{"slurheight", &cfmt.slurheight, FORMAT_R, 0},
	{"splittune", &cfmt.splittune, FORMAT_B, 0},
	{"squarebreve", &cfmt.squarebreve, FORMAT_B, 0},
	{"staffnonote", &cfmt.staffnonote, FORMAT_B, 0},
	{"staffsep", &cfmt.staffsep, FORMAT_U, 0},
	{"staffwidth", &staffwidth, FORMAT_U, 1},
	{"stemheight", &cfmt.stemheight, FORMAT_R, 0},
	{"straightflags", &cfmt.straightflags, FORMAT_B, 0},
	{"stretchlast", &cfmt.stretchlast, FORMAT_R, 2},
	{"stretchstaff", &cfmt.stretchstaff, FORMAT_B, 0},
	{"subtitlefont", &cfmt.font_tb[SUBTITLEFONT], FORMAT_F, 0},
	{"subtitlespace", &cfmt.subtitlespace, FORMAT_U, 0},
	{"sysstaffsep", &cfmt.sysstaffsep, FORMAT_U, 0},
	{"tempofont", &cfmt.font_tb[TEMPOFONT], FORMAT_F, 0},
	{"textfont", &cfmt.font_tb[TEXTFONT], FORMAT_F, 0},
	{"textoption", &cfmt.textoption, FORMAT_I, 4},
	{"textspace", &cfmt.textspace, FORMAT_U, 0},
	{"titlecaps", &cfmt.titlecaps, FORMAT_B, 0},
	{"titlefont", &cfmt.font_tb[TITLEFONT], FORMAT_F, 0},
	{"titleformat", &cfmt.titleformat, FORMAT_S, 0},
	{"titleleft", &cfmt.titleleft, FORMAT_B, 0},
	{"titlespace", &cfmt.titlespace, FORMAT_U, 0},
	{"titletrim", &cfmt.titletrim, FORMAT_B, 0},
	{"timewarn", &cfmt.timewarn, FORMAT_B, 0},
	{"topmargin", &cfmt.topmargin, FORMAT_U, 0},
	{"topspace", &cfmt.topspace, FORMAT_U, 0},
	{"transpose", &cfmt.transpose, FORMAT_I, 1},
	{"tuplets", &cfmt.tuplets, FORMAT_I, 3},
	{"vocalfont", &cfmt.font_tb[VOCALFONT], FORMAT_F, 0},
	{"vocalspace", &cfmt.vocalspace, FORMAT_U, 0},
	{"voicefont", &cfmt.font_tb[VOICEFONT], FORMAT_F, 0},
	{"wordsfont", &cfmt.font_tb[WORDSFONT], FORMAT_F, 0},
	{"wordsspace", &cfmt.wordsspace, FORMAT_U, 0},
	{"writefields", &cfmt.fields, FORMAT_B, 1},
	{0, 0, 0, 0}		/* end of table */
};

/* -- search a font and add it if not yet defined -- */
static int get_font(char *fname, int encoding)
{
	int fnum;

	/* get or set the default encoding */
	for (fnum = nfontnames; --fnum >= 0; )
		if (strcmp(fname, fontnames[fnum]) == 0) {
			if (encoding < 0)
				encoding = def_font_enc[fnum];
			if (encoding == font_enc[fnum])
				return fnum;		/* font found */
			break;
		}
	while (--fnum >= 0) {
		if (strcmp(fname, fontnames[fnum]) == 0
		 && encoding == font_enc[fnum])
			return fnum;
	}

	/* add the font */
	if (nfontnames >= MAXFONTS) {
		error(1, 0, "Too many fonts");
		return 0;
	}
	if (file_initialized
	 && (epsf != 2 && !svg))
		error(1, 0,
		      "Cannot have a new font when the output file is opened");
	fnum = nfontnames++;
	fontnames[fnum] = strdup(fname);
	if (encoding < 0)
		encoding = 0;
	font_enc[fnum] = encoding;
	return fnum;
}

/* -- set a dynamic font -- */
static int dfont_set(struct FONTSPEC *f)
{
	int i;

	for (i = FONT_DYN; i < cfmt.ndfont; i++) {
		if (cfmt.font_tb[i].fnum == f->fnum
		    && cfmt.font_tb[i].size == f->size)
			return i;
	}
	if (i >= FONT_MAX - 1) {
		error(1, 0, "Too many dynamic fonts");
		return FONT_MAX - 1;
	}
	memcpy(&cfmt.font_tb[i], f, sizeof cfmt.font_tb[0]);
	cfmt.ndfont = i + 1;
	return i;
}

/* -- define a font -- */
static void fontspec(struct FONTSPEC *f,
		     char *name,
		     int encoding,
		     float size)
{
	if (name)
		f->fnum = get_font(name, encoding);
	else
		name = fontnames[f->fnum];
	f->size = size;
	f->swfac = size;
	if (swfac_font[f->fnum] != 0) {
		f->swfac *= swfac_font[f->fnum];
	} else if (strncmp(name, "Times", 5) == 0) {
		if (strcmp(name, "Times-Bold") == 0)
			f->swfac *= 1.05;
	} else if (strcmp(name, "Helvetica-Bold") == 0) {
		f->swfac *= 1.15;
	} else if (strncmp(name, "Helvetica", 9) == 0
		|| strncmp(name, "Palatino", 8) == 0) {
		f->swfac *= 1.10;
	} else if (strncmp(name, "Courier", 7) == 0) {
		f->swfac *= 1.35;
	} else {
		f->swfac *= 1.2;		/* unknown font */
	}
	if (f == &cfmt.font_tb[GCHORDFONT])
		cfmt.gcf = dfont_set(f);
	else if (f == &cfmt.font_tb[ANNOTATIONFONT])
		cfmt.anf = dfont_set(f);
	else if (f == &cfmt.font_tb[VOCALFONT])
		cfmt.vof = dfont_set(f);
}

/* -- output the font definitions with their encodings -- */
/* This output must occurs after user PostScript definitions because
 * these ones may change the default behaviour */
void define_fonts(void)
{
	int i;
	static char *mkfont =
	"/mkfont{findfont dup length 1 add dict begin\n"
	"	{1 index/FID ne{def}{pop pop}ifelse}forall\n"
	"	CharStrings/double_sharp known not{\n"
	"		/CharStrings CharStrings dup length dict copy def\n"
	"		CharStrings/sharp{pop 460 0 setcharwidth usharp ufill}bind put\n"
	"		CharStrings/flat{pop 460 0 setcharwidth uflat ufill}bind put\n"
	"		CharStrings/natural{pop 400 0 setcharwidth unat ufill}bind put\n"
	"		CharStrings/double_sharp{pop 460 0 setcharwidth udblesharp ufill}bind put\n"
	"		CharStrings/double_flat{pop 500 0 setcharwidth udbleflat ufill}bind put\n"
	"	}if currentdict definefont pop end}!\n";
	
	fputs(mkfont, fout);
	make_font_list();
	for (i = 0; i < nfontnames; i++) {
		if (used_font[i])
			define_font(fontnames[i], i, font_enc[i]);
	}
}

/* -- mark the used fonts -- */
void make_font_list(void)
{
	struct FORMAT *f;
	int i;

	f = &cfmt;
	for (i = FONT_UMAX; i < FONT_DYN; i++)
		used_font[f->font_tb[i].fnum] = 1;
}

/* -- set the name of an information header type -- */
/* the argument is
 *	<letter> [ <possibly quoted string> ]
 * this information is kept in the 'I' information */
static void set_infoname(char *p)
{
	struct SYMBOL *s, *prev;

	if (*p == 'I')
		return;
	s = info['I' - 'A'];
	prev = NULL;
	while (s) {
		if (s->as.text[0] == *p)
			break;
		prev = s;
		s = s->next;
	}
	if (p[1] == '\0') {		/* if delete */
		if (s) {
			if (!prev)
				info['I' - 'A'] = s->next;
			else if ((prev->next = s->next) != 0)
				prev->next->prev = prev;
		}
		return;
	}
	if (!s) {
		s = (struct SYMBOL *) getarena(sizeof *s);
		memset(s, 0, sizeof *s);
		if (!prev)
			info['I' - 'A'] = s;
		else {
			prev->next = s;
			s->prev = prev;
		}
	}
	s->as.text = (char *) getarena(strlen(p) + 1);
	strcpy(s->as.text, p);
}

/* -- set the default format -- */
/* this function is called only once, at abcm2ps startup time */
void set_format(void)
{
	struct FORMAT *f;

	f = &cfmt;
	memset(f, 0, sizeof *f);
	f->pageheight = PAGEHEIGHT;
	f->pagewidth = PAGEWIDTH;
	f->leftmargin = MARGIN;
	f->rightmargin = MARGIN;
	f->topmargin = 1.0 CM;
	f->botmargin = 1.0 CM;
	f->topspace = 0.8 CM;
	f->titlespace = 0.2 CM;
	f->subtitlespace = 0.1 CM;
	f->composerspace = 0.2 CM;
	f->musicspace = 0.2 CM;
	f->partsspace = 0.3 CM;
	f->staffsep = 46.0 PT;
	f->sysstaffsep = 34.0 PT;
	f->maxstaffsep = 2000.0 PT;
	f->maxsysstaffsep = 2000.0 PT;
	f->vocalspace = 23.0 PT;
	f->textspace = 0.5 CM;
	f->scale = 0.75;
	f->slurheight = 1.0;
	f->maxshrink = 0.65;
	f->breaklimit = 0.7;
	f->stretchlast = 0.2;
	f->stretchstaff = 1;
	f->graceslurs = 1;
	f->hyphencont = 1;
	f->lineskipfac = 1.1;
	f->parskipfac = 0.4;
	f->measurenb = -1;
	f->measurefirst = 1;
	f->autoclef = 1;
	f->breakoneoln = 1;
	f->dblrepbar = (B_COL << 12) + (B_CBRA << 8) + (B_OBRA << 4) + B_COL;
	f->dynalign = 1;
	f->keywarn = 1;
	f->linewarn = 1;
#ifdef HAVE_PANGO
	if (!svg && epsf != 2)
		f->pango = 1;
	else
		lock_fmt(&cfmt.pango);	/* SVG output does not use panga */
#endif
	f->staffnonote = 1;
	f->titletrim = 1;
	f->aligncomposer = A_RIGHT;
	f->notespacingfactor = 1.414;
	f->stemheight = STEM;
#ifndef WIN32
	f->dateformat = strdup("%b %e, %Y %H:%M");
#else
	f->dateformat = strdup("%b %#d, %Y %H:%M");
#endif
	f->gracespace = (65 << 16) | (80 << 8) | 120;	/* left-inside-right - unit 1/10 pt */
	f->textoption = T_LEFT;
	f->ndfont = FONT_DYN;
	fontspec(&f->font_tb[ANNOTATIONFONT], "Helvetica", 0, 12.0);
	fontspec(&f->font_tb[COMPOSERFONT], "Times-Italic", 0, 14.0);
	fontspec(&f->font_tb[FOOTERFONT], "Times-Roman", 0, 12.0); /* not scaled */
	fontspec(&f->font_tb[GCHORDFONT], "Helvetica", 0, 12.0);
	fontspec(&f->font_tb[HEADERFONT], "Times-Roman", 0, 12.0); /* not scaled */
	fontspec(&f->font_tb[HISTORYFONT], "Times-Roman", 0, 16.0);
	fontspec(&f->font_tb[INFOFONT],	"Times-Italic", 0, 14.0); /* same as composer by default */
	fontspec(&f->font_tb[MEASUREFONT], "Times-Italic", 0, 14.0);
	fontspec(&f->font_tb[PARTSFONT], "Times-Roman", 0, 15.0);
	fontspec(&f->font_tb[REPEATFONT], "Times-Roman", 0, 13.0);
	fontspec(&f->font_tb[SUBTITLEFONT], "Times-Roman", 0, 16.0);
	fontspec(&f->font_tb[TEMPOFONT], "Times-Bold", 0, 15.0);
	fontspec(&f->font_tb[TEXTFONT],	"Times-Roman", 0, 16.0);
	fontspec(&f->font_tb[TITLEFONT], "Times-Roman", 0, 20.0);
	fontspec(&f->font_tb[VOCALFONT], "Times-Bold", 0, 13.0);
	fontspec(&f->font_tb[VOICEFONT], "Times-Bold", 0, 13.0);
	fontspec(&f->font_tb[WORDSFONT], "Times-Roman", 0, 16.0);
	f->fields[0] = (1 << ('C' - 'A'))
		| (1 << ('M' - 'A'))
		| (1 << ('O' - 'A'))
		| (1 << ('P' - 'A'))
		| (1 << ('Q' - 'A'))
		| (1 << ('T' - 'A'))
		| (1 << ('W' - 'A'));
	f->fields[1] = (1 << ('w' - 'a'));
	set_infoname("R \"Rhythm: \"");
	set_infoname("B \"Book: \"");
	set_infoname("S \"Source: \"");
	set_infoname("D \"Discography: \"");
	set_infoname("N \"Notes: \"");
	set_infoname("Z \"Transcription: \"");
	set_infoname("H \"History: \"");
}

/* -- print the current format -- */
void print_format(void)
{
	struct format *fd;
static char *yn[2] = {"no","yes"};

	for (fd = format_tb; fd->name; fd++) {
		printf("%-15s ", fd->name);
		switch (fd->type) {
		case FORMAT_B:
			switch (fd->subtype) {
#ifdef HAVE_PANGO
			case 2:				/* pango = 0, 1 or 2 */
				if (cfmt.pango == 2) {
					printf("2\n");
					break;
				}
				/* fall thru */
#endif
			case 0:
				printf("%s\n", yn[*((int *) fd->v)]);
				break;
			case 1: {			/* writefields */
				int i;

				for (i = 0; i < 32; i++) {
					if (cfmt.fields[0] & (1 << i))
						printf("%c", (char) ('A' + i));
					if (cfmt.fields[1] & (1 << i))
						printf("%c", (char) ('a' + i));
				}
				printf("\n");
				break;
			    }
			}
			break;
		case FORMAT_I:
			switch (fd->subtype) {
			default:
				printf("%d\n", *((int *) fd->v));
				break;
			case 1: {		/* transpose */
				int t;

				t = *((int *) fd->v);
				if (t >= 0)
					putchar('+');
				printf("%d", t / 3);
				switch ((t + 240) % 3) {
				case 1:
					putchar('#');
					break;
				case 2:
					putchar('b');
					break;
				}
				putchar('\n');
				break;
			    }
			case 2: {		/* dblrepbar */
				int v;
				char tmp[16], *p;

				p = &tmp[sizeof tmp - 1];
				*p = '\0';
				for (v = cfmt.dblrepbar; v != 0; v >>= 4) {
					switch (v & 0x0f) {
					case B_BAR:
						*--p = '|';
						break;
					case B_OBRA:
						*--p = '[';
						break;
					case B_CBRA:
						*--p = ']';
						break;
					default:
//					case B_COL:
						*--p = ':';
						break;
					}
				}
				printf("%s\n", p);
				break;
			    }
			case 3:			/* tuplets */
				printf("%d %d %d\n",
					cfmt.tuplets >> 8,
					(cfmt.tuplets >> 4) & 0x0f,
					cfmt.tuplets & 0x0f);
				break;
//			case 4:			/* textoption */
//				break;
			case 5:			/* gracespace */
				printf("%d.%d %d.%d %d.%d\n",
					(cfmt.gracespace >> 16) / 10,
					(cfmt.gracespace >> 16) % 10,
					((cfmt.gracespace >> 8) & 0xff) / 10,
					((cfmt.gracespace >> 8) & 0xff) % 10,
					(cfmt.gracespace & 0xff) / 10,
					(cfmt.gracespace & 0xff) % 10);
				break;
			}
			break;
		case FORMAT_R:
			printf("%.2f\n", *((float *) fd->v));
			break;
		case FORMAT_F: {
			struct FONTSPEC *s;

			s = (struct FONTSPEC *) fd->v;
			printf("%s", fontnames[s->fnum]);
			printf(" %s", font_enc[s->fnum] ? "native" : "utf-8");
			printf(" %.1f", s->size);
			if ((fd->subtype == 1 && cfmt.partsbox)
			 || (fd->subtype == 2 && cfmt.measurebox)
			 || (fd->subtype == 3 && cfmt.gchordbox))
				printf(" box");
			printf("\n");
			break;
		}
		case FORMAT_U:
			if (fd->subtype == 0)
				printf("%.2fcm\n", *((float *) fd->v) / (1 CM));
			else
				printf("%.2fcm\n",
					(cfmt.pagewidth
						- cfmt.leftmargin
						- cfmt.rightmargin)
					/ (1 CM));
			break;
		case FORMAT_S:
			printf("\"%s\"\n",
				*((char **) fd->v) != 0 ? *((char **) fd->v) : "");
			break;
		}
	}
}

/* -- get an encoding -- */
static int parse_encoding(char *p)
{
	return strncasecmp(p, "native", 6) == 0;
}

/* -- get a position -- */
static int get_posit(char *p)
{
	if (strcmp(p, "up") == 0
	 || strcmp(p, "above") == 0)
		return SL_ABOVE;
	if (strcmp(p, "down") == 0
	 || strcmp(p, "below") == 0)
		return SL_BELOW;
	if (strcmp(p, "hidden") == 0)
		return SL_HIDDEN;
	return 0;			/* auto (!= SL_AUTO) */
}

/* -- get the option for text -- */
int get_textopt(char *p)
{
	if (*p == '\0'
	 || strncmp(p, "obeylines", 9) == 0)
		return T_LEFT;
	if (strncmp(p, "align", 5) == 0
	 || strncmp(p, "justify", 7) == 0)
		return T_JUSTIFY;
	if (strncmp(p, "ragged", 6) == 0
	 || strncmp(p, "fill", 4) == 0)
		return T_FILL;
	if (strncmp(p, "center", 6) == 0)
		return T_CENTER;
	if (strncmp(p, "skip", 4) == 0)
		return T_SKIP;
	if (strncmp(p, "right", 5) == 0)
		return T_RIGHT;
	return -1;
}

/* -- get the double repeat bar -- */
static int get_dblrepbar(char *p)
{
	int bar_type;

	bar_type = 0;
	for (;;) {
		switch (*p++) {
		case '|':
			bar_type <<= 4;
			bar_type |= B_BAR;
			continue;
		case '[':
			bar_type <<= 4;
			bar_type |= B_OBRA;
			continue;
		case ']':
			bar_type <<= 4;
			bar_type |= B_CBRA;
			continue;
		case ':':
			bar_type <<= 4;
			bar_type |= B_COL;
			continue;
		default:
			break;
		}
		break;
	}
	return bar_type;
}

/* -- get a boolean value -- */
static int g_logv(char *p)
{
	switch (*p) {
	case '\0':
	case '1':
	case 'y':
	case 'Y':
	case 't':
	case 'T':
		return 1;
	case '0':
	case 'n':
	case 'N':
	case 'f':
	case 'F':
		break;
	default:
		error(0, 0, "Unknown logical '%s' - false assumed", p);
		break;
	}
	return 0;
}

/* -- get a font specifier -- */
static void g_fspc(char *p,
		   struct FONTSPEC *f)
{
	char fname[80];
	int encoding;
	float fsize;

	p = get_str(fname, p, sizeof fname);
	if (isalpha((unsigned char) *p) || *p == '*') {
		if (*p == '*')
			encoding = font_enc[f->fnum];
		else
			encoding = parse_encoding(p);
		while (*p != '\0' && !isspace((unsigned char) *p))
			p++;
		while (isspace((unsigned char) *p))
			p++;
	} else {
		encoding = -1;
	}
	fsize = f->size;
	if (*p != '\0' && *p != '*') {
		char *q;
		float v;

		v = strtod(p, &q);
		if (v <= 0 || (*q != '\0' && *q != ' '))
			error(1, 0, "Bad font size '%s'", p);
		else
			fsize = v;
	}
	fontspec(f,
		 strcmp(fname, "*") != 0 ? fname : 0,
		 encoding,
		 fsize);
	if (!file_initialized)
		used_font[f->fnum] = 1;
	if (f - cfmt.font_tb == outft)
		outft = -1;
#ifdef HAVE_PANGO
	pg_reset_font();
#endif
}

/* -- parse a 'tablature' definition -- */
/* %%tablature
 *	[#<nunmber (1..MAXTBLT)>]
 *	[pitch=<instrument pitch (<note> # | b)>]
 *	[[<head width>]
 *	 <height above>]
 *	<height under>
 *	<head function>
 *	<note function>
 *	[<bar function>]
 */
struct tblt_s *tblt_parse(char *p)
{
	struct tblt_s *tblt;
	int n;
	char *q;
	static char notes_tb[14] = "CDEFGABcdefgab";
	static char pitch_tb[14] = {60, 62, 64, 65, 67, 69, 71,
				    72, 74, 76, 77, 79, 81, 83};

	/* number */
	if (*p == '#') {
		p++;
		n = *p++ - '0' - 1;
		if ((unsigned) n >= MAXTBLT
		 || (*p != '\0' && *p != ' ')) {
			error(1, 0, "Invalid number in %%%%tablature");
			return 0;
		}
		if (*p == '\0')
			return tblts[n];
		while (isspace((unsigned char) *p))
			p++;
	} else	n = -1;

	/* pitch */
	tblt = malloc(sizeof *tblt);
	memset(tblt, 0, sizeof *tblt);
	if (strncmp(p, "pitch=", 6) == 0) {
		p += 6;
		if (*p == '^' || *p == '_') {
			if (*p == '^') {
				tblt->pitch++;
				tblt->instr[1] = '#';
			} else {
				tblt->pitch--;
				tblt->instr[1] = 'b';
			}
			p++;
		}
		if (*p == '\0' || (q = strchr(notes_tb, *p)) == 0) {
			error(1, 0, "Invalid pitch in %%%%tablature");
			return 0;
		}
		tblt->pitch += pitch_tb[q - notes_tb];
		tblt->instr[0] = toupper(*p++);
		while (*p == '\'' || *p == ',') {
			if (*p++ == '\'')
				tblt->pitch += 12;
			else
				tblt->pitch -= 12;
		}
		if (*p == '#' || *p == 'b') {
			if (*p == '#')
				tblt->pitch++;
			else
				tblt->pitch--;
			tblt->instr[1] = *p++;
		}
		while (*p == '\'' || *p == ',') {
			if (*p++ == '\'')
				tblt->pitch += 12;
			else
				tblt->pitch -= 12;
		}
		while (isspace((unsigned char) *p))
			p++;
	}

	/* width and heights */
	if (!isdigit(*p)) {
		error(1, 0, "Invalid width/height in %%%%tablature");
		return 0;
	}
	tblt->hu = scan_u(p);
	while (*p != '\0' && !isspace((unsigned char) *p))
		p++;
	while (isspace((unsigned char) *p))
		p++;
	if (isdigit(*p)) {
		tblt->ha = tblt->hu;
		tblt->hu = scan_u(p);
		while (*p != '\0' && !isspace((unsigned char) *p))
			p++;
		while (isspace((unsigned char) *p))
			p++;
		if (isdigit(*p)) {
			tblt->wh = tblt->ha;
			tblt->ha = tblt->hu;
			tblt->hu = scan_u(p);
			while (*p != '\0' && !isspace((unsigned char) *p))
				p++;
			while (isspace((unsigned char) *p))
				p++;
		}
	}
	if (*p == '\0')
		goto err;

	/* PS functions */
	p = strdup(p);
	tblt->head = p;
	while (*p != '\0' && !isspace((unsigned char) *p))
		p++;
	if (*p == '\0')
		goto err;
	*p++ = '\0';
	while (isspace((unsigned char) *p))
		p++;
	tblt->note = p;
	while (*p != '\0' && !isspace((unsigned char) *p))
		p++;
	if (*p != '\0') {
		*p++ = '\0';
		while (isspace((unsigned char) *p))
			p++;
		tblt->bar = p;
		while (*p != '\0' && !isspace((unsigned char) *p))
			p++;
		if (*p != '\0')
			goto err;
	}

	/* memorize the definition */
	if (n >= 0)
		tblts[n] = tblt;
	return tblt;
err:
	error(1, 0, "Wrong values in %%%%tablature");
	return 0;
}

/* functions to set a voice parameter */
#define F_SET_PAR(param) \
static void set_ ## param(struct VOICE_S *p_voice, int val)\
{\
	p_voice->posit.param = val;\
}
F_SET_PAR(dyn)
F_SET_PAR(gch)
F_SET_PAR(orn)
F_SET_PAR(voc)
F_SET_PAR(vol)
F_SET_PAR(std)
F_SET_PAR(gsd)

struct vpar {
	char *name;
	void (*f)(struct VOICE_S *p_voice, int val);
	int max;
};
static struct vpar vpar_tb[] = {
	{"dynamic", set_dyn, 3},	/* 0 */
	{"gchord", set_gch, 3},		/* 1 */
	{"gstemdir", set_gsd, 2},	/* 2 */
	{"ornament", set_orn, 3},	/* 3 */
	{"stemdir", set_std, 2},	/* 4 */
	{"vocal", set_voc, 3},		/* 5 */
	{"volume", set_vol, 3},		/* 6 */
	{}
};
/* -- set a voice parameter -- */
void set_voice_param(struct VOICE_S *p_voice,	/* current voice */
			int state,		/* tune state */
			char *w,		/* keyword */
			char *p)		/* argument */
{
	struct vpar *vpar, *vpar2 = NULL;
	int i, val;

	for (vpar = vpar_tb; vpar->name; vpar++) {
		if (strcmp(w, vpar->name))
			continue;
		if (!isdigit(*p))
			val = get_posit(p);
		else
			val = strtod(p, 0);
		if ((unsigned) val > vpar->max)
			goto err;
		vpar->f(p_voice, val);
		break;
	}
	if (!vpar->name) {	/* compatibility with previous versions */
		val = -1;
		switch (*w) {
		case 'e':
			if (strcmp(w, "exprabove") == 0) {
				vpar = &vpar[0];	/* dyn */
				vpar = &vpar[6];	/* vol */
				if (g_logv(p))
					val = SL_ABOVE;
				else
					val = SL_BELOW;
				break;
			}
			if (strcmp(w, "exprbelow") == 0) {
				vpar = &vpar[0];	/* dyn */
				vpar = &vpar[6];	/* vol */
				if (g_logv(p))
					val = SL_BELOW;
				else
					val = SL_ABOVE;
				break;
			}
			break;
		case 'v':
			if (strcmp(w, "vocalabove") == 0) {	/* compatibility */
				vpar = &vpar[5];	/* voc */
				if (g_logv(p))
					val = SL_ABOVE;
				else
					val = SL_BELOW;
				break;
			}
			break;
		}
		if (val < 0)
			goto err;
	}
	if (state == ABC_S_TUNE) {
		vpar->f(p_voice, val);
		if (vpar2)
			vpar2->f(p_voice, val);
		return;
	}
	for (i = MAXVOICE, p_voice = voice_tb;	/* global */
	     --i >= 0;
	     p_voice++) {
		vpar->f(p_voice, val);
		if (vpar2)
			vpar2->f(p_voice, val);
	}
	return;
err:
	error(1, 0, "Bad value %%%%%s %s", w, p);
}

/* -- parse a format line -- */
void interpret_fmt_line(char *w,		/* keyword */
			char *p,		/* argument */
			int lock)
{
	struct format *fd;

	switch (*w) {
	case 'b':
		if (strcmp(w, "barnumbers") == 0)	/* compatibility */
			w = "measurenb";
		break;
	case 'c':
		if (strcmp(w, "comball") == 0) {	/* compatibility */
			cfmt.combinevoices = 2;
			return;
		}
		break;
	case 'f':
		if (strcmp(w, "font") == 0) {
			int i, fnum, encoding;
			float swfac;
			char fname[80];

			if (file_initialized) {
				error(1, 0,
				      "Cannot define a font when the output file is opened");
				return;
			}
			p = get_str(fname, p, sizeof fname);
			swfac = 0;			/* defaults to 1.2 */
			encoding = 0;
			if (*p != '\0') {
				if (isalpha((unsigned char) *p)) {
					encoding = parse_encoding(p);
					while (*p != '\0'
					    && !isspace((unsigned char) *p))
						p++;
					while (isspace((unsigned char) *p))
						p++;
				}
				if (isdigit((unsigned char) *p)) {
					char *q;
					float v;

					v = strtod(p, &q);
					if (v > 2 || (*q != '\0' && *q != '\0'))
						goto bad;
					swfac = v;
				}
			}
			fnum = get_font(fname, encoding);
			def_font_enc[fnum] = encoding;
			swfac_font[fnum] = swfac;
			used_font[fnum] = 1;
			for (i = FONT_UMAX; i < FONT_MAX; i++) {
				if (cfmt.font_tb[i].fnum == fnum)
					cfmt.font_tb[i].swfac = cfmt.font_tb[i].size
									* swfac;
			}
			return;
		}
		break;
	case 'i':
		if (strcmp(w, "infoname") == 0) {
			if (*p < 'A' || *p > 'Z')
				goto bad;
			set_infoname(p);
			return;
		}
		break;
	case 'm':
		if (strcmp(w, "musiconly") == 0) {	/* compatibility */
			if (g_logv(p))
				cfmt.fields[1] &= ~(1 << ('w' - 'a'));
			else
				cfmt.fields[1] |= (1 << ('w' - 'a'));
			return;
		}
		break;
	case 'p':
		if (strcmp(w, "printparts") == 0) {	/* compatibility */
			if (g_logv(p))
				cfmt.fields[0] |= (1 << ('P' - 'A'));
			else
				cfmt.fields[0] &= ~(1 << ('P' - 'A'));
			return;
		}
		if (strcmp(w, "printtempo") == 0) {	/* compatibility */
			if (g_logv(p))
				cfmt.fields[0] |= (1 << ('Q' - 'A'));
			else
				cfmt.fields[0] &= ~(1 << ('Q' - 'A'));
			return;
		}
		break;
	case 'w':
		if (strcmp(w, "withxrefs") == 0) {	/* compatibility */
			if (g_logv(p))
				cfmt.fields[0] |= (1 << ('X' - 'A'));
			else
				cfmt.fields[0] &= ~(1 << ('X' - 'A'));
			return;
		}
		if (strcmp(w, "writehistory") == 0) {	/* compatibility */
			struct SYMBOL *s;
			int bool;
			unsigned u;

			bool = g_logv(p);
			for (s = info['I' - 'A']; s != 0; s = s->next) {
				u = s->as.text[0] - 'A';
				if (bool)
					cfmt.fields[0] |= (1 << u);
				else
					cfmt.fields[0] &= ~(1 << u);
			}
			return;
		}
		break;
	}
	for (fd = format_tb; fd->name; fd++)
		if (strcmp(w, fd->name) == 0)
			break;
	if (fd->name == 0)
		return;

	{
		int l;

		l = strlen(p);
		if (strcmp(p + l - 5, " lock") == 0) {
			p[l - 5] = '\0';
			lock = 1;
		}
	}
	if (lock)
		fd->lock = 1;
	else if (fd->lock)
		return;

	switch (fd->type) {
	case FORMAT_B:
		switch (fd->subtype) {
#ifdef HAVE_PANGO
		case 2:				/* %%pango = 0, 1 or 2 */
			if (*p == '2') {
				cfmt.pango = 2;
				break;
			}
			/* fall thru */
#endif
		case 0:
			*((int *) fd->v) = g_logv(p);
			break;
		case 1:	{			/* %%writefields */
			char *q;
			int bool, i;
			unsigned u;

			q = p;
			while (*p != '\0' && !isspace((unsigned char) *p))
				p++;
			while (isspace((unsigned char) *p))
				p++;
			bool = g_logv(p);
			while (*q != '\0' && !isspace((unsigned char) *q)) {
				u = *q - 'A';
				if (u < 26) {
					i = 0;
				} else {
					u = *q - 'a';
					if (u < 26)
						i = 1;
					else
						break;	/*fixme: error */
				}
				if (bool)
					cfmt.fields[i] |= (1 << u);
				else
					cfmt.fields[i] &= ~(1 << u);
				q++;
			}
			break;
		    }
		}
		break;
	case FORMAT_I:
		if (fd->subtype == 3) {		/* tuplets */
			unsigned i1, i2, i3;

			if (sscanf(p, "%d %d %d", &i1, &i2, &i3) != 3
			 || i1 > 2 || i2 > 2 || i3 > 2)
				goto bad;
			cfmt.tuplets = (i1 << 8) | (i2 << 4) | i3;
			break;
		}
		if (fd->subtype == 5) {		/* gracespace */
			unsigned i1, i2, i3;
			float f1, f2, f3;

			if (sscanf(p, "%f %f %f", &f1, &f2, &f3) != 3
			 || f1 > 256 || f2 > 256 || f3 > 256)
				goto bad;
			i1 = f2 * 10;
			i2 = f2 * 10;
			i3 = f3 * 10;
			cfmt.gracespace = (i1 << 16) | (i2 << 8) | i3;
			break;
		}
		if (fd->subtype == 4 && !isdigit(*p))	/* textoption */
			cfmt.textoption = get_textopt(p);
		else if (fd->subtype == 2)		/* dblrepbar */
			cfmt.dblrepbar = get_dblrepbar(p);
		else if (isdigit(*p) || *p == '-' || *p == '+')
			sscanf(p, "%d", (int *) fd->v);
		else
			*((int *) fd->v) = g_logv(p);
		switch (fd->subtype) {
		case 1:					/* transpose */
			cfmt.transpose *= 3;
			if (p[strlen(p) - 1] == '#') {
				if (cfmt.transpose > 0)
					cfmt.transpose++;
				else
					cfmt.transpose -= 2;
			} else if (p[strlen(p) - 1] == 'b') {
				if (cfmt.transpose > 0)
					cfmt.transpose += 2;
				else
					cfmt.transpose--;
			}
			break;
//		case 2:				/* (free) */
//			break;
		case 4:				/* textoption */
			if (cfmt.textoption < 0)
				goto bad;
			break;
		}
		break;
	case FORMAT_R: {
		char *q;
		int i;
		float v;

		v = strtod(p, &q);
		if (*q != '\0' && *q != ' ')
			goto bad;
		switch (fd->subtype) {
		default:
			if (v <= 0)
				goto bad;
			break;
		case 1: {			/* note spacing factor */
			float v2;

			if (v < 1 || v > 2)
				goto bad;
			i = C_XFLAGS;		/* crotchet index */
			v2 = space_tb[i];
			for ( ; --i >= 0; ) {
				v2 /= v;
				space_tb[i] = v2;
			}
			i = C_XFLAGS;
			v2 = space_tb[i];
			for ( ; ++i < NFLAGS_SZ; ) {
				v2 *= v;
				space_tb[i] = v2;
			}
			break;
		    }
		case 2:				/* maxshrink / stretchlast */
			if (v < 0 || v > 1)
				goto bad;
			break;
		case 3:				/* breaklimit */
			if (v < 0.5 || v > 1)
				goto bad;
			break;
		}
		*((float *) fd->v) = v;
		break;
	    }
	case FORMAT_F: {
		int b;

		g_fspc(p, (struct FONTSPEC *) fd->v);
		b = strstr(p, "box") != 0;
		switch (fd->subtype) {
		case 1:
			cfmt.partsbox = b;
			break;
		case 2:
			cfmt.measurebox = b;
			break;
		case 3:
			cfmt.gchordbox = b;
			break;
		}
		break;
	    }
	case FORMAT_U:
		*((float *) fd->v) = scan_u(p);
		if (fd->subtype == 1) {
			float rmargin;

			rmargin = (cfmt.landscape ? cfmt.pageheight : cfmt.pagewidth)
					- staffwidth - cfmt.leftmargin;
			if (rmargin < 0)
				error(1, 0, "'staffwidth' too big\n");
			cfmt.rightmargin = rmargin;
		}
		break;
	case FORMAT_S: {
		int l;

		l = strlen(p) + 1;
		*((char **) fd->v) = getarena(l);
		if (*p == '"')
			get_str(*((char **) fd->v), p, l);
		else
			strcpy(*((char **) fd->v), p);
		break;
	    }
	}
	return;
bad:
	error(1, 0, "Bad value '%s' for '%s' - ignored", p, w);
}

/* -- lock a format -- */
void lock_fmt(void *fmt)
{
	struct format *fd;

	for (fd = format_tb; fd->name; fd++)
		if (fd->v == fmt)
			break;
	if (fd->name == 0)
		return;
	fd->lock = 1;
}

/* -- start a new font -- */
void set_font(int ft)
{
	int fnum;
	struct FONTSPEC *f, *f2;

	if (ft == outft)
		return;
	f = &cfmt.font_tb[ft];
	if (outft >= 0) {
		f2 = &cfmt.font_tb[outft];
		outft = ft;
		fnum = f->fnum;
		if (fnum == f2->fnum && f->size == f2->size)
			return;
	} else {
		outft = ft;
		fnum = f->fnum;
	}
	if (!used_font[fnum]
	 && epsf != 2 && !svg) {	/* (not usefull for svg output) */
		if (!file_initialized) {
			used_font[fnum] = 1;
		} else {
			error(1, 0,
			      "Font '%s' not predefined; using first in list",
			      fontnames[fnum]);
			fnum = 0;
		}
	}
	if (f->size == 0) {
		error(0, 0, "Font '%s' with a null size - set to 8",
		      fontnames[fnum]);
		f->size = 8;
	}
	a2b("%.1f F%d ", f->size, fnum);
}

/* -- get the encoding of a font -- */
int get_font_encoding(int ft)
{
	return font_enc[ft];
}
