/*
 * SVG definitions.
 *
 * This file is part of abcm2ps.
 *
 * Modified by Mosalam Ebrahimi <m.ebrahimi@ieee.org>
 * Copyright (C) 1998-2013 Jean-François Moine
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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "abc2ps.h"

enum elt_t {			/* element types */
	VAL,
	STR,
	SEQ,			/* {..} */
	BRK,			/* [..] */
};
struct elt_s {
	struct elt_s *next;
	char type;
	union {
		float v;
		char *s;
		struct elt_s *e;
	} u;
};
struct ps_sym_s {
	char *n;		/* name */
	struct elt_s *e;	/* value */
	int exec;		/* current number of execution */
};
/* -- PostScript tiny interpreter -- */
//jfm test
#define NELTS 2048	/* number of elements per block */
//#define NSYMS 128	/* max number of symbols */
#define NSYMS 512	/* max number of symbols */
static struct elt_s *elts;
static struct elt_s *stack, *free_elt;
static struct ps_sym_s ps_sym[NSYMS];
static int n_sym;
static int ps_error;
static int in_cnt;			/* in [..] or {..} */
static float cx, cy;			/* current point */
static char *path;
static char path_buf[256];

static float xoffs, yoffs;

static int id = 0;

/* graphical context */
#define DLW 0.7			/* default line width */
static struct gc {
	float xscale, yscale;
	char font_n[64];
	float font_s;
	float linewidth, rotate;
	int rgb;
	char dash[64];
} gcur, gold;
static float x_rot, y_rot;	/* save x and y offset when rotate != 0 */
static struct {
	struct gc gc;
	float cx, cy;
	float xoffs, yoffs;
	float x_rot, y_rot;
} gsave[8];
static int nsave;
static int g;			/* current container */
static int boxend;

/* abcm2ps definitions */
static struct {
	char *def;
	char use;
	char defined;
} def_tb[] = {
#define D_brace 0
{	"<path id=\"brace\" fill=\"currentColor\" d=\"m-2.5 101\n"
	"	c-4.5 -4.6 -7.5 -12.2 -4.4 -26.8\n"
	"	c3.5 -14.3 3.2 -21.7 -2.1 -24.2\n"
	"	c7.4 2.4 7.3 14.2 3.5 29.5\n"
	"	c-2.7 9.5 -1.5 16.2 3 21.5\n"
	"	M-2.5 1c-4.5 4.6 -7.5 12.2 -4.4 26.8\n"
	"	c3.5 14.3 3.2 21.7 -2.1 24.2\n"
	"	c7.4 -2.4 7.3 -14.2 3.5 -29.5\n"
	"	c-2.7 -9.5 -1.5 -16.2 3 -21.5\"/>\n"},
#define D_utclef 1
{	"<path id=\"utclef\" fill=\"currentColor\" d=\"m-50 -90\n"
	"	c-72 -41 -72 -158 52 -188\n"
	"	c150 -10 220 188 90 256\n"
	"	c-114 52 -275 0 -293 -136\n"
	"	c-15 -181 93 -229 220 -334\n"
	"	c88 -87 79 -133 62 -210\n"
	"	c-51 33 -94 105 -89 186\n"
	"	c17 267 36 374 49 574\n"
	"	c6 96 -19 134 -77 135\n"
	"	c-80 1 -126 -93 -61 -133\n"
	"	c85 -41 133 101 31 105\n"
	"	c23 17 92 37 90 -92\n"
	"	c-10 -223 -39 -342 -50 -617\n"
	"	c0 -90 0 -162 96 -232\n"
	"	c56 72 63 230 22 289\n"
	"	c-74 106 -257 168 -255 316\n"
	"	c9 153 148 185 252 133\n"
	"	c86 -65 29 -192 -80 -176\n"
	"	c-71 12 -105 67 -59 124\"/>\n"},
#define D_tclef 2
{	"<use id=\"tclef\" transform=\"scale(0.045)\"\n"
	"	xlink:href=\"#utclef\"/>\n", D_utclef},
#define D_stclef 3
{	"<use id=\"stclef\" transform=\"translate(0,-0.6) scale(0.037)\"\n"
	"	xlink:href=\"#utclef\"/>\n", D_utclef},
#define D_ubclef 4
{	"<path id=\"ubclef\" fill=\"currentColor\" d=\"m-200 -87\n"
	"	c124 -35 222 -78 254 -236\n"
	"	c43 -228 -167 -246 -192 -103\n"
	"	c59 -80 157 22 92 78\n"
	"	c-62 47 -115 -22 -106 -88\n"
	"	c21 -141 270 -136 274 52\n"
	"	c-1 175 -106 264 -322 297\n"
	"	m357 -250\n"
	"	c0 -36 51 -34 51 0\n"
	"	c0 37 -51 36 -51 0\n"
	"	m-2 -129\n"
	"	c0 -36 51 -34 51 0\n"
	"	c0 38 -51 37 -51 0\"/>\n"},
#define D_bclef 5
{	"<use id=\"bclef\" transform=\"scale(0.045)\"\n"
	"	xlink:href=\"#ubclef\"/>\n", D_ubclef},
#define D_sbclef 6
{	"<use id=\"sbclef\" transform=\"translate(0,-3.5) scale(0.037)\"\n"
	"	xlink:href=\"#ubclef\"/>\n", D_ubclef},
#define D_ucclef 7
{	"<path id=\"ucclef\" fill=\"currentColor\" d=\"\n"
	"	m-51 -264\n"
	"	v262\n"
	"	h-13\n"
	"	v-529\n"
	"	h13\n"
	"	v256\n"
	"	c25 -20 41 -36 63 -109\n"
	"	c14 31 13 51 56 70\n"
	"	c90 34 96 -266 -41 -185\n"
	"	c52 19 27 80 -11 77\n"
	"	c-90 -38 33 -176 139 -69\n"
	"	c72 79 1 241 -134 186\n"
	"	l-16 39 16 38\n"
	"	c135 -55 206 107 134 186\n"
	"	c-106 108 -229 -31 -139 -69\n"
	"	c38 -3 63 58 11 77\n"
	"	c137 81 131 -219 41 -185\n"
	"	c-43 19 -45 30 -56 64\n"
	"	c-22 -73 -38 -89 -63 -109\n"
	"	m-99 -267\n"
	"	h57\n"
	"	v529\n"
	"	h-57\n"
	"	v-529\"/>\n"},
#define D_cclef 8
{	"<use id=\"cclef\" transform=\"scale(0.045)\"\n"
	"	xlink:href=\"#ucclef\"/>\n", D_ucclef},
#define D_scclef 9
{	"<use id=\"scclef\" transform=\"translate(0,-2) scale(0.037)\"\n"
	"	xlink:href=\"#ucclef\"/>\n", D_ucclef},
#define D_pclef 10
{	"<path id=\"pclef\" d=\"m-2.7 -2h5.4v-20h-5.4v20\" stroke=\"currentColor\" fill=\"none\" stroke-width=\"1.4\"/>\n"},
#define D_hd 11
{	"<ellipse id=\"hd\" rx=\"4.1\" ry=\"2.9\"\n"
	"	transform=\"rotate(-20)\" fill=\"currentColor\"/>\n"},
#define D_Hd 12
{	"<path id=\"Hd\" fill=\"currentColor\" d=\"m3 -1.6\n"
	"	c-1 -1.8 -7 1.4 -6 3.2\n"
	"	c1 1.8 7 -1.4 6 -3.2\n"
	"	m0.5 -0.3\n"
	"	c2 3.8 -5 7.6 -7 3.8\n"
	"	c-2 -3.8 5 -7.6 7 -3.8\"/>\n"},
#define D_HD 13
{	"<path id=\"HD\" fill=\"currentColor\" d=\"m-2.7 -1.4\n"
	"	c1.5 -2.8 6.9 0 5.3 2.7\n"
	"	c-1.5 2.8 -6.9 0 -5.3 -2.7\n"
	"	m8.3 1.4\n"
	"	c0 -1.5 -2.2 -3 -5.6 -3\n"
	"	c-3.4 0 -5.6 1.5 -5.6 3\n"
	"	c0 1.5 2.2 3 5.6 3\n"
	"	c3.4 0 5.6 -1.5 5.6 -3\"/>\n"},
#define D_HDD 14
{	"<g id=\"HDD\">\n"
	"	<use xlink:href=\"#HD\"/>\n"
	"	<path d=\"m-6 -4v8m12 0v-8\" stroke=\"currentColor\" fill=\"none\"/>\n"
	"</g>\n", D_HD},
#define D_breve 15
{	"<g id=\"breve\" stroke=\"currentColor\" fill=\"none\">\n"
	"	<path d=\"m-6 -2.7h12m0 5.4h-12\" stroke-width=\"2.5\"/>\n"
	"	<path d=\"m-6 -5v10m12 0v-10\"/>\n"
	"</g>\n"},
#define D_longa 16
{	"<g id=\"longa\" stroke=\"currentColor\" fill=\"none\">\n"
	"	<path d=\"m-6 2.7h12m0 -5.4h-12\" stroke-width=\"2.5\"/>\n"
	"	<path d=\"m-6 5v-10m12 0v16\"/>\n"
	"</g>\n"},
#define D_ghd 17
{	"<path id=\"ghd\" fill=\"currentColor\" d=\"m1.7 -1\n"
	"	c-1 -1.7 -4.5 0.2 -3.4 2\n"
	"	c1 1.7 4.5 -0.2 3.4 -2\"/>\n"},
#define D_r00 18
{	"<rect id=\"r00\" fill=\"currentColor\"\n"
	"	x=\"-1.6\" y=\"-6\" width=\"3\" height=\"12\"/>\n"},
#define D_r0 19
{	"<rect id=\"r0\" fill=\"currentColor\"\n"
	"	x=\"-1.6\" y=\"-6\" width=\"3\" height=\"6\"/>\n"},
#define D_r1 20
{	"<rect id=\"r1\" fill=\"currentColor\"\n"
	"	x=\"-3.5\" y=\"-6\" width=\"7\" height=\"3\"/>\n"},
#define D_r2 21
{	"<rect id=\"r2\" fill=\"currentColor\"\n"
	"	x=\"-3.5\" y=\"-3\" width=\"7\" height=\"3\"/>\n"},
#define D_r4 22
{	"<path id=\"r4\" fill=\"currentColor\" d=\"m-1 -8.5\n"
	"	l3.6 5.1 -2.1 5.2 2.2 4.3\n"
	"	c-2.6 -2.3 -5.1 0 -2.4 2.6\n"
	"	c-4.8 -3 -1.5 -6.9 1.4 -4.1\n"
	"	l-3.1 -4.5 1.9 -5.1 -1.5 -3.5\"/>\n"},
#define D_r8e 23
{	"<path id=\"r8e\" fill=\"currentColor\" d=\"m 0 0\n"
	"	c-1.5 1.5 -2.4 2 -3.6 2\n"
	"	c2.4 -2.8 -2.8 -4 -2.8 -1.2\n"
	"	c0 2.7 4.3 2.4 5.9 0.6\"/>\n"},
#define D_r8 24
{	"<g id=\"r8\">\n"
	"	<path d=\"m3.3 -4l-3.4 9.6\" stroke=\"currentColor\" fill=\"none\"/>\n"
	"	<use x=\"3.4\" y=\"-4\" xlink:href=\"#r8e\"/>\n"
	"</g>\n", D_r8e},
#define D_r16 25
{	"<g id=\"r16\">\n"
	"	<path d=\"m3.3 -4l-4 15.6\" stroke=\"currentColor\" fill=\"none\"/>\n"
	"	<use x=\"3.4\" y=\"-4\" xlink:href=\"#r8e\"/>\n"
	"	<use x=\"1.9\" y=\"2\" xlink:href=\"#r8e\"/>\n"
	"</g>\n", D_r8e},
#define D_r32 26
{	"<g id=\"r32\">\n"
	"	<path d=\"m4.8 -10l-5.5 21.6\" stroke=\"currentColor\" fill=\"none\"/>\n"
	"	<use x=\"4.9\" y=\"-10\" xlink:href=\"#r8e\"/>\n"
	"	<use x=\"3.4\" y=\"-4\" xlink:href=\"#r8e\"/>\n"
	"	<use x=\"1.9\" y=\"2\" xlink:href=\"#r8e\"/>\n"
	"</g>\n", D_r8e},
#define D_r64 27
{	"<g id=\"r64\">\n"
	"	<path d=\"m4.8 -10 l-7 27.6\" stroke=\"currentColor\" fill=\"none\"/>\n"
	"	<use x=\"4.9\" y=\"-10\" xlink:href=\"#r8e\"/>\n"
	"	<use x=\"3.4\" y=\"-4\" xlink:href=\"#r8e\"/>\n"
	"	<use x=\"1.9\" y=\"2\" xlink:href=\"#r8e\"/>\n"
	"	<use x=\"0.4\" y=\"8\" xlink:href=\"#r8e\"/>\n"
	"</g>\n", D_r8e},
#define D_r128 28
{	"<g id=\"r128\">\n"
	"	<path d=\"m5.8 -16 l-8.5 33.6\" stroke=\"currentColor\" fill=\"none\"/>\n"
	"	<use x=\"5.9\" y=\"-16\" xlink:href=\"#r8e\"/>\n"
	"	<use x=\"4.4\" y=\"-10\" xlink:href=\"#r8e\"/>\n"
	"	<use x=\"2.9\" y=\"-4\" xlink:href=\"#r8e\"/>\n"
	"	<use x=\"1.4\" y=\"2\" xlink:href=\"#r8e\"/>\n"
	"	<use x=\"0.1\" y=\"8\" xlink:href=\"#r8e\"/>\n"
	"</g>\n", D_r8e},
#define D_mrest 29
{	"<g id=\"mrest\">\n"
	"	<path d=\"m-20 -6v-12m40 0v12\" stroke=\"currentColor\" fill=\"none\"/>\n"
	"	<path d=\"m-20 -12h40\" stroke=\"currentColor\" stroke-width=\"5\" fill=\"none\"/>\n"
	"</g>\n"},
#define D_usharp 30
{	"<path id=\"usharp\" fill=\"currentColor\" d=\"\n"
	"	m136 -702\n"
	"	v890\n"
	"	h32\n"
	"	v-890\n"
	"	m128 840\n"
	"	h32\n"
	"	v-888\n"
	"	h-32\n"
	"	m-232 286\n"
	"	v116\n"
	"	l338 -96\n"
	"	v-116\n"
	"	m-338 442\n"
	"	v116\n"
	"	l338 -98\n"
	"	v-114\"/>\n"},
#define D_uflat 31
{	"<path id=\"uflat\" fill=\"currentColor\" d=\"\n"
	"	m100 -746\n"
	"	h32\n"
	"	v734\n"
	"	l-32 4\n"
	"	m32 -332\n"
	"	c46 -72 152 -90 208 -20\n"
	"	c100 110 -120 326 -208 348\n"
	"	m0 -28\n"
	"	c54 0 200 -206 130 -290\n"
	"	c-50 -60 -130 -4 -130 34\"/>\n"},
#define D_unat 32
{	"<path id=\"unat\" fill=\"currentColor\" d=\"\n"
	"	m96 -750\n"
	"	h-32\n"
	"	v716\n"
	"	l32 -8\n"
	"	l182 -54\n"
	"	v282\n"
	"	h32\n"
	"	v-706\n"
	"	l-34 10\n"
	"	l-180 50\n"
	"	v-290\n"
	"	m0 592\n"
	"	v-190\n"
	"	l182 -52\n"
	"	v188\"/>\n"},
#define D_udblesharp 33
{	"<path id=\"udblesharp\" fill=\"currentColor\" d=\"\n"
	"	m240 -282\n"
	"	c40 -38 74 -68 158 -68\n"
	"	v-96\n"
	"	h-96\n"
	"	c0 84 -30 118 -68 156\n"
	"	c-40 -38 -70 -72 -70 -156\n"
	"	h-96\n"
	"	v96\n"
	"	c86 0 120 30 158 68\n"
	"	c-38 38 -72 68 -158 68\n"
	"	v96\n"
	"	h96\n"
	"	c0 -84 30 -118 70 -156\n"
	"	c38 38 68 72 68 156\n"
	"	h96\n"
	"	v-96\n"
	"	c-84 0 -118 -30 -158 -68\"/>\n"},
#define D_udbleflat 34
{	"<path id=\"udbleflat\" fill=\"currentColor\" d=\"\n"
	"	m20 -746\n"
	"	h24\n"
	"	v734\n"
	"	l-24 4\n"
	"	m24 -332\n"
	"	c34 -72 114 -90 156 -20\n"
	"	c75 110 -98 326 -156 348\n"
	"	m0 -28\n"
	"	c40 0 150 -206 97 -290\n"
	"	c-37 -60 -97 -4 -97 34\n"
	"	m226 -450\n"
	"	h24\n"
	"	v734\n"
	"	l-24 4\n"
	"	m24 -332\n"
	"	c34 -72 114 -90 156 -20\n"
	"	c75 110 -98 326 -156 348\n"
	"	m0 -28\n"
	"	c40 0 150 -206 97 -290\n"
	"	c-37 -60 -97 -4 -97 34\"/>\n"},
#define D_sh0 35
{	"<use id=\"sh0\" transform=\"translate(-4,5) scale(0.018)\"\n"
	"	xlink:href=\"#usharp\"/>\n", D_usharp},
#define D_ft0 36
{	"<use id=\"ft0\" transform=\"translate(-3.5,3.5) scale(0.018)\"\n"
	"	xlink:href=\"#uflat\"/>\n", D_uflat},
#define D_nt0 37
{	"<use id=\"nt0\" transform=\"translate(-3,5) scale(0.018)\"\n"
	"	xlink:href=\"#unat\"/>\n", D_unat},
#define D_dsh0 38
{	"<use id=\"dsh0\" transform=\"translate(-4,5) scale(0.018)\"\n"
	"	xlink:href=\"#udblesharp\"/>\n", D_udblesharp},
#define D_dft0 39
{	"<use id=\"dft0\" transform=\"translate(-4,3.5) scale(0.018)\"\n"
	"	xlink:href=\"#udbleflat\"/>\n", D_udbleflat},
#define D_sh1 40
{	"<g id=\"sh1\">\n"
	"    <path d=\"M0 7.8v-15.4\" stroke=\"currentColor\" fill=\"none\"/>\n"
	"    <path fill=\"currentColor\" d=\"M-1.8 2.7l3.6 -1.1v2.2l-3.6 1.1v-2.2z\n"
	"	M-1.8 -3.7l3.6 -1.1v2.2l-3.6 1.1v-2.2\"/>\n"
	"</g>\n"},
#define D_sh513 41
{	"<g id=\"sh513\">\n"
	"    <path d=\"M-2.5 8.7v-15.4M0 7.8v-15.4M2.5 6.9v-15.4\" stroke=\"currentColor\" fill=\"none\"/>\n"
	"    <path fill=\"currentColor\" d=\"M-3.7 3.1l7.4 -2.2v2.2l-7.4 2.2v-2.2z\n"
	"	M-3.7 -3.2l7.4 -2.2v2.2l-7.4 2.2v-2.2\"/>\n"
	"</g>\n"},
#define D_ft1 42
{	"<g id=\"ft1\" transform=\"scale(-1,1)\">\n"
	"	<use xlink:href=\"#ft0\"/>\n"
	"</g>\n", D_ft0},
#define D_ft513 43
{	"<g id=\"ft513\">\n"
	"    <path fill=\"currentColor\" d=\"M0.6 -2.7\n"
	"	c-5.7 -3.1 -5.7 3.6 0 6.7c-3.9 -4 -4 -7.6 0 -5.8\n"
	"	M1 -2.7c5.7 -3.1 5.7 3.6 0 6.7c3.9 -4 4 -7.6 0 -5.8\"/>\n"
	"    <path d=\"M1.6 3.5v-13M0 3.5v-13\" stroke=\"currentColor\" fill=\"none\" stroke-width=\".6\"/>\n"
	"</g>\n"},
#define D_pshhd 44
{	"<g id=\"pshhd\">\n"
	"	<use xlink:href=\"#dsh0\"/>\n"
	"</g>\n", D_dsh0},
#define D_pfthd 45
{	"<g id=\"pfthd\">\n"
	"	<use xlink:href=\"#dsh0\"/>\n"
	"	<circle r=\"4\" stroke=\"currentColor\" fill=\"none\"/>\n"
	"</g>\n", D_dsh0},
#define D_csig 46
{	"<path id=\"csig\" fill=\"currentColor\" d=\"\n"
	"	m1 -17.3\n"
	"	c0.9 0 2.3 0.7 2.4 2.2\n"
	"	c-1.2 -2 -3.6 0.1 -1.6 1.7\n"
	"	c2 1 3.8 -3.5 -0.8 -4.7\n"
	"	c-2 -0.4 -6.4 1.3 -5.8 7\n"
	"	c0.4 6.4 7.9 6.8 9.1 0.7\n"
	"	c-2.3 5.6 -6.7 5.1 -6.8 0\n"
	"	c-0.5 -4.4 0.7 -7.5 3.5 -6.9\"/>\n"},
#define D_ctsig 47
{	"<g id=\"ctsig\">\n"
	"	<use xlink:href=\"#csig\"/>\n"
	"	<path d=\"m0 -4v-16\" stroke=\"currentColor\"/>\n"
	"</g>\n", D_csig},
#define D_pmsig 48
{	"<path id=\"pmsig\" stroke=\"currentColor\" stroke-width=\"0.8\" fill=\"none\"\n"
	"	d=\"M0 -7a5 5 0 0 1 0 -10a5 5 0 0 1 0 10\"/>\n"},
#define D_pMsig 49
{	"<g id=\"pMsig\">\n"
	"	<use xlink:href=\"#pmsig\"/>\n"
	"	<path fill=\"currentColor\" d=\"M0 -10a2 2 0 0 1 0 -4a2 2 0 0 1 0 4\"/>\n"
	"</g>\n", D_pmsig},
#define D_imsig 50
{	"<path id=\"imsig\" stroke=\"currentColor\" stroke-width=\"0.8\" fill=\"none\"\n"
	"	d=\"M0 -7a5 5 0 1 1 0 -10\"/>\n"},
#define D_iMsig 51
{	"<g id=\"iMsig\">\n"
	"	<use xlink:href=\"#imsig\"/>\n"
	"	<path fill=\"currentColor\" d=\"M0 -10a2 2 0 0 1 0 -4a2 2 0 0 1 0 4\"/>\n"
	"</g>\n", D_imsig},
#define D_hl 52
{	"<path id=\"hl\" stroke=\"currentColor\" d=\"m-6 0h12\"/>\n"},
#define D_hl1 53
{	"<path id=\"hl1\" stroke=\"currentColor\" d=\"m-7 0h14\"/>\n"},
#define D_hl2 54
{	"<path id=\"hl2\" stroke=\"currentColor\" d=\"m-9 0h18\"/>\n"},
#define D_ghl 55
{	"<path id=\"ghl\" stroke=\"currentColor\" fill=\"none\" d=\"m-3 0h6\"/>\n"},
#define D_rdots 56
{	"<g id=\"rdots\" fill=\"currentColor\">\n"
	"	<circle cx=\"0\" cy=\"-9\" r=\"1.2\"/>\n"
	"	<circle cx=\"0\" cy=\"-15\" r=\"1.2\"/>\n"
	"</g>\n"},
#define D_srep 57
{	"<path id=\"srep\" fill=\"currentColor\" d=\"M-1 -6l11 -12h3l-11 12h-3\"/>\n"},
#define D_mrep 58
{	"<path id=\"mrep\" fill=\"currentColor\"\n"
	"    d=\"M-5 -16.5a1.5 1.5 0 0 1 0 3a1.5 1.5 0 0 1 0 -3\n"
	"	M4.5 -10a1.5 1.5 0 0 1 0 3a1.5 1.5 0 0 1 0 -3\n"
	"	M-7 -6l11 -12h3l-11 12h-3\"/>\n"},
#define D_mrep2 59
{	"<g id=\"mrep2\" fill=\"currentColor\">\n"
	"    <path d=\"M-5.5 -19.5a1.5 1.5 0 0 1 0 3a1.5 1.5 0 0 1 0 -3\n"
	"	M5 -7.5a1.5 1.5 0 0 1 0 3a1.5 1.5 0 0 1 0 -3\"/>\n"
	"    <path d=\"M-7 -4l14 -10m-14 4l14 -10\" stroke=\"currentColor\" stroke-width=\"1.8\" fill=\"none\"/>\n"
	"</g>\n"},
#define D_accent 60
{	"<g id=\"accent\" stroke=\"currentColor\" fill=\"none\">\n"
	"	<path d=\"m-4 0l8 -2l-8 -2\" stroke-width=\"1.2\"/>\n"
	"</g>\n"},
#define D_umrd 61
{	"<path id=\"umrd\" fill=\"currentColor\" d=\"m0 -4\n"
	"	l2.2 -2.2 2.1 2.9 0.7 -0.7 0.2 0.2\n"
	"	-2.2 2.2 -2.1 -2.9 -0.7 0.7\n"
	"	-2.2 2.2 -2.1 -2.9 -0.7 0.7 -0.2 -0.2\n"
	"	2.2 -2.2 2.1 2.9 0.7 -0.7\"/>\n"},
#define D_lmrd 62
{	"<g id=\"lmrd\">\n"
	"	<use xlink:href=\"#umrd\"/>\n"
	"	<line x1=\"0\" y1=\"0\" x2=\"0\" y2=\"-8\" stroke=\"currentColor\" stroke-width=\".6\"/>\n"
	"</g>\n", D_umrd},
#define D_grm 63
{	"<path id=\"grm\" fill=\"currentColor\" d=\"\n"
	"	m-5 -2.5\n"
	"	c5 -8.5 5.5 4.5 10 -2\n"
	"	c-5 8.5 -5.5 -4.5 -10 2\"/>\n"},
#define D_stc 64
{	"<circle id=\"stc\" fill=\"currentColor\" cx=\"1.2\" cy=\"-3\" r=\"1.2\"/>\n"},
#define D_sld 65
{	"<path id=\"sld\" fill=\"currentColor\" d=\"\n"
	"	m-7.2 4.8\n"
	"	c1.8 0.7 4.5 -0.2 7.2 -4.8\n"
	"	c-2.1 5 -5.4 6.8 -7.6 6\"/>\n"},
#define D_emb 66
{	"<path id=\"emb\" d=\"m-2.5 -3h5\" stroke-width=\"1.2\" stroke-linecap=\"round\" stroke=\"currentColor\"/>\n"},
#define D_hld 67
{	"<g id=\"hld\" fill=\"currentColor\">\n"
	"    <circle cx=\"0\" cy=\"-3\" r=\"1.3\"/>\n"
	"    <path d=\"m-7.5 -1.5\n"
	"	c0 -11.5 15 -11.5 15 0\n"
	"	h-0.25\n"
	"	c-1.25 -9 -13.25 -9 -14.5 0\"/>\n"
	"</g>\n"},
#define D_cpu 68
{	"<path id=\"cpu\" fill=\"currentColor\" d=\"\n"
	"	m-6 0\n"
	"	c0.4 -7.3 11.3 -7.3 11.7 0\n"
	"	c-1.3 -6 -10.4 -6 -11.7 0\"/>\n"},
#define D_upb 69
{	"<path id=\"upb\" stroke=\"currentColor\" fill=\"none\" d=\"\n"
	"	m-2.6 -9.4\n"
	"	l2.6 8.8\n"
	"	l2.6 -8.8\"/>\n"},
#define D_dnb 70
{	"<g id=\"dnb\">\n"
	"	<path d=\"M-3.2 -2v-7.2m6.4 0v7.2\" stroke=\"currentColor\" fill=\"none\"/>\n"
	"	<path d=\"M-3.2 -6.8v-2.4l6.4 0v2.4\" fill=\"currentColor\"/>\n"
	"</g>\n"},
#define D_sgno 71
{	"<g id=\"sgno\">\n"
	"    <path fill=\"currentColor\" d=\"m0 -3\n"
	"	c1.5 1.7 6.4 -0.3 3 -3.7\n"
	"	c-10.4 -7.8 -8 -10.6 -6.5 -11.9\n"
	"	c4 -1.9 5.9 1.7 4.2 2.6\n"
	"	c-1.3 0.7 -2.9 -1.3 -0.7 -2\n"
	"	c-1.5 -1.7 -6.4 0.3 -3 3.7\n"
	"	c10.4 7.8 8 10.6 6.5 11.9\n"
	"	c-4 1.9 -5.9 -1.7 -4.2 -2.6\n"
	"	c1.3 -0.7 2.9 1.3 0.7 2\"/>\n"
	"    <line x1=\"-6\" y1=\"-4.2\" x2=\"6.6\" y2=\"-16.8\" stroke=\"currentColor\"/>\n"
	"    <circle cx=\"-6\" cy=\"-10\" r=\"1.2\"/>\n"
	"    <circle cx=\"6\" cy=\"-11\" r=\"1.2\"/>\n"
	"</g>\n"},
#define D_coda 72
{	"<g id=\"coda\" stroke=\"currentColor\" fill=\"none\">\n"
	"	<path d=\"m0 -2v-20m-10 10h20\"/>\n"
	"	<circle cx=\"0\" cy=\"-12\" r=\"6\" stroke-width=\"1.7\"/>\n"
	"</g>\n"},
#define D_dplus 73
{	"<path id=\"dplus\" stroke=\"currentColor\" stroke-width=\"1.7\"\n"
	"	d=\"m0 -0.5v-6m-3 3h6\"/>\n"},
#define D_lphr 74
{	"<path id=\"lphr\" stroke=\"currentColor\" stroke-width=\"1.2\"\n"
	"	d=\"m0 0v18\"/>\n"},
#define D_mphr 75
{	"<path id=\"mphr\" stroke=\"currentColor\" stroke-width=\"1.2\"\n"
	"	d=\"m0 0v12\"/>\n"},
#define D_sphr 76
{	"<path id=\"sphr\" stroke=\"currentColor\" stroke-width=\"1.2\"\n"
	"	d=\"m0 0v6\"/>\n"},
#define D_opend 77
{	"<circle id=\"opend\" stroke=\"currentColor\" fill=\"none\"\n"
	"	cx=\"0\" cy=\"-3\" r=\"2.5\"/>\n"},
#define D_snap 78
{	"<path id=\"snap\" stroke=\"currentColor\" fill=\"none\"\n"
	"    d=\"M-3 -6\n"
	"	c0 -5 6 -5 6 0\n"
	"	c0 5 -6 5 -6 0\n"
	"	M0 -5v6\"/>\n"},
#define D_thumb 79
{	"<path id=\"thumb\" stroke=\"currentColor\" fill=\"none\"\n"
	"    d=\"M-2.5 -7\n"
	"	c0 -6 5 -6 5 0\n"
	"	c0 6 -5 6 -5 0\n"
	"	M-2.5 -9v4\"/>\n"},
#define D_turn 80
{	"<path id=\"turn\" fill=\"currentColor\" d=\"\n"
	"	m5.2 -8\n"
	"	c1.4 0.5 0.9 4.8 -2.2 2.8\n"
	"	l-4.8 -3.5\n"
	"	c-3 -2 -5.8 1.8 -3.6 4.4\n"
	"	c1 1.1 2 0.8 2.1 -0.1\n"
	"	c0.1 -0.9 -0.7 -1.2 -1.9 -0.6\n"
	"	c-1.4 -0.5 -0.9 -4.8 2.2 -2.8\n"
	"	l4.8 3.5\n"
	"	c3 2 5.8 -1.8 3.6 -4.4\n"
	"	c-1 -1.1 -2 -0.8 -2.1 0.1\n"
	"	c-0.1 0.9 0.7 1.2 1.9 0.6\"/>\n"},
#define D_turnx 81
{	"<g id=\"turnx\">\n"
	"	<use xlink:href=\"#turn\"/>\n"
	"	<path d=\"M0 -1.5v-9\" stroke=\"currentColor\" fill=\"none\"/>\n"
	"</g>\n", D_turn},
#define D_wedge 82
{	"<path id=\"wedge\" fill=\"currentColor\" d=\"M0 -1l-1.5 -5h3l-1.5 5\"/>\n"},
#define D_ltr 83
{	"<path id=\"ltr\" fill=\"currentColor\"\n"
	"    d=\"m0 -0.4c2 -1.5 3.4 -1.9 3.9 0.4\n"
	"	c0.2 0.8 0.7 0.7 2.1 -0.4\n"
	"	v0.8c-2 1.5 -3.4 1.9 -3.9 -0.4\n"
	"	c-0.2 -0.8 -0.7 -0.7 -2.1 0.4z\"/>\n"},
#define D_custos 84
{	"<g id=\"custos\">\n"
	"	<path d=\"M-4 0l2 2.5l2 -2.5l2 2.5l2 -2.5\n"
	"		l-2 -2.5l-2 2.5l-2 -2.5l-2 2.5\" fill=\"currentColor\"/>\n"
	"	<path d=\"M3.5 0l5 -7\" stroke=\"currentColor\" fill=\"none\"/>\n"
	"</g>\n"},
#define D_showerror 85
{	"<circle id=\"showerror\" r=\"30\" stroke=\"#ffc0c0\" stroke-width=\"2.5\" fill=\"none\"/>\n"},
};

/* PS functions */
static void elts_link(struct elt_s *e)
{
	int i;

	/* set the linkages - the first element is the link to the next block */
	for (i = 1; i < NELTS - 1; i++) {
		e[i].next = &e[i + 1];
		if (e[i].type == STR)
			free(e[i].u.s);
		e[i].type = VAL;
	}
	e[NELTS - 1].next = NULL;
}

/* (re)initialize all PS elements */
static void elts_reset(void)
{
	struct elt_s *e;

	if (!elts)
		elts = calloc(sizeof *elts, NELTS);
	elts_link(elts);
	free_elt = elts + 1;

	/* link all blocks */
	for (e = elts; e->u.e; e = e->u.e) {
		elts_link(e->u.e);
		e[NELTS - 1].next = e->u.e;
	}
}

static struct elt_s *elt_new(void)
{
	struct elt_s *e;

	e = free_elt;
	if (!e) {
		e = calloc(sizeof *e, NELTS);
		if (!e) {
			fprintf(stderr, "svg: elt_new out of memory\n");
			ps_error = 1;
			return e;
		}
		elts_link(e);
		e->u.e = elts;
		elts = e;
		e++;
	}
	free_elt = e->next;
	e->next = NULL;
	e->type = VAL;
	return e;
}

static void elt_free(struct elt_s *e)
{
	struct elt_s *e2;

	e->next = free_elt;
	free_elt = e;
	switch (e->type) {
	case STR:
		free(e->u.s);
		e->type = VAL;
		e->u.v = 0;
		break;
	case SEQ:
	case BRK:
		e2 = e->u.e;
		e->type = VAL;
		e->u.v = 0;
		while (e2) {
			e = e2->next;
			elt_free(e2);
			e2 = e;
		}
		break;
	}
}

static struct elt_s *elt_dup(struct elt_s *e)
{
	struct elt_s *e2, *e3, *e4;

	e2 = elt_new();
	if (!e2)
		return e2;
	e2->type = e->type;
	switch (e->type) {
	case VAL:
		e2->u.v = e->u.v;
		break;
	case STR:
		e2->u.s = strdup(e->u.s);
		break;
	case SEQ:
	case BRK:
		e = e->u.e;
		if (!e) {
			e2->u.e = NULL;
			break;
		}
		e3 = e2->u.e = elt_dup(e);
		if (!e3)
			break;
		for (;;) {
			e = e->next;
			if (!e)
				break;
			e4 = elt_dup(e);
			if (!e4)
				break;
			e3->next = e4;
			e3 = e4;
		}
		e3->next = NULL;
		break;
	}
	return e2;
}

static void elt_dump(struct elt_s *e)
{
	int type;

	type = e->type;
	switch (type) {
	case VAL:
		fprintf(stderr, " %.2f", e->u.v);
		break;
	case STR:
		fprintf(stderr, " %s", e->u.s);
		if (e->u.s[0] == '(')
			fprintf(stderr, ")");
		break;
	case SEQ:
	case BRK:
		fprintf(stderr, type == SEQ ? " {" : " [");
		e = e->u.e;
		while (e) {
			elt_dump(e);
			e = e->next;
		}
		fprintf(stderr, type == SEQ ? " }" : " ]");
	}
}

static void elt_lst_dump(struct elt_s *e)
{
	do {
		elt_dump(e);
		e = e->next;
	} while (e);
}

static struct ps_sym_s *ps_sym_lookup(char *name)
{
	struct ps_sym_s *ps;

	if (n_sym == 0)
		return NULL;
	ps = &ps_sym[n_sym];
	for (;;) {
		ps--;
		if (strcmp(ps->n, name) == 0)
			break;
		if (ps == ps_sym)
			return NULL;
	}
	return ps;
}

static struct ps_sym_s *ps_sym_def(char *name, struct elt_s *e)
{
	struct ps_sym_s *ps;

	ps = ps_sym_lookup(name);
	if (ps) {
		elt_free(ps->e);
	} else {
		if (n_sym >= NSYMS) {
			fprintf(stderr, "svg: Too many PS symbols\n");
			ps_error = 1;
			return NULL;
		}
		ps = &ps_sym[n_sym++];
		ps->n = strdup(name);
	}
	ps->e = e;
	ps->exec = 0;
	return ps;
}

static void push(struct elt_s *e)
{
	e->next = stack;
	stack = e;
}

static void stack_dump(void)
{
	fprintf(stderr, "stack:");
	if (stack)
		elt_lst_dump(stack);
	else
		fprintf(stderr, "(empty)");
	fprintf(stderr, "\n");
}

static struct elt_s *pop(int type)
{
	struct elt_s *e;

	e = stack;
	if (!e) {
		fprintf(stderr, "svg pop: Stack empty\n");
		ps_error = 1;
		return NULL;
	}
	if (e->type != type) {
		fprintf(stderr, "svg pop: Bad element type %d != %d\n",
			e->type, type);
		stack_dump();
		ps_error = 1;
		return NULL;
	}
	stack = e->next;
	return e;
}

static float pop_free_val(void)
{
	struct elt_s *e;

	e = pop(VAL);
	if (!e)
		return 0;
	e->next = free_elt;
	free_elt = e;
	return e->u.v;
}

static char *pop_free_str(void)
{
	struct elt_s *e;
	char *s;

	e = pop(STR);
	if (!e)
		return NULL;
	s = e->u.s;
	e->type = VAL;
	e->next = free_elt;
	free_elt = e;
	return s;
}

/* PS condition code */
#define C_EQ 0
#define C_NE 1
#define C_GT 2
#define C_GE 3
#define C_LT 4
#define C_LE 5
static void cond(int type)
{
	float v;
	char *s;

	/* special case when 1 character strings */
	if (stack && stack->type == STR) {
		s = stack->u.s;
		stack->u.v = s[1];
		free(s);
		stack->type = VAL;
	}
	if (stack && stack->next != 0 && stack->next->type == STR) {
		s = stack->next->u.s;
		stack->next->u.v = s[1];
		free(s);
		stack->next->type = VAL;
	}
	v = pop_free_val();
	if (!stack || stack->type != VAL) {
		fprintf(stderr, "svg: Bad value for condition\n");
		ps_error = 1;
		return;
	}
	switch (type) {
	case C_EQ:
		stack->u.v = stack->u.v == v;
		break;
	case C_NE:
		stack->u.v = stack->u.v != v;
		break;
	case C_GT:
		stack->u.v = stack->u.v > v;
		break;
	case C_GE:
		stack->u.v = stack->u.v >= v;
		break;
	case C_LT:
		stack->u.v = stack->u.v < v;
		break;
	case C_LE:
		stack->u.v = stack->u.v <= v;
		break;
	}
}

/* -- output information about the generation in the XHTML/SVG headers -- */
static void gen_info(void)
{
	unsigned i;
	time_t ltime;

	time(&ltime);
#ifndef WIN32
	strftime(tex_buf, TEX_BUF_SZ, "%b %e, %Y %H:%M", localtime(&ltime));
#else
	strftime(tex_buf, TEX_BUF_SZ, "%b %#d, %Y %H:%M", localtime(&ltime));
#endif
	fprintf(fout, "<!-- CreationDate: %s -->\n"
			"<!-- CommandLine:",
			tex_buf);

	for (i = 1; i < (unsigned) s_argc; i++) {
		char *p;
		int space;

		p = s_argv[i];
		space = strchr(p, ' ') != NULL || strchr(p, '\n') != NULL;
		fputc(' ', fout);
		if (space)
			fputc('\'', fout);

		/* cannot have '--' inside comment ! */
		if (*p == '-' && p[1] == '-') {
			fputs("-\\", fout);
			p++;
		}
		fputs(p, fout);
		if (space)
			fputc('\'', fout);
	}
	fputs(" -->\n", fout);
}

/* -- output the symbol definitions -- */
void define_svg_symbols(char *title, int num, float w, float h)
{
	char *s;
	unsigned i;
	static const char *svg_head =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\"\n"
		"\txmlns:xlink=\"http://www.w3.org/1999/xlink\"\n"
		"\txml:space='preserve' color=\"black\"\n"
		"\twidth=\"%.2fin\" height=\"%.2fin\" viewBox=\"0 0 %.0f %.0f\">\n"
		"<title>%s %s %d</title>\n";

	xoffs = yoffs = x_rot = y_rot = 0;
	memset(&gcur, 0, sizeof gcur);
	gcur.xscale = gcur.yscale = 1;
	gcur.linewidth = DLW;
	memcpy(&gold, &gcur, sizeof gold);
	nsave = 0;
	for (i = 0; i < sizeof def_tb / sizeof def_tb[0]; i++)
		def_tb[i].defined = 0;

	if (svg == 2) {			/* if XHTML */
		if (!file_initialized) {
			if ((s = strrchr(in_fname, DIRSEP)) == NULL)
				s = in_fname;
			else
				s++;
			fputs("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\"\n"
				"\"http://www.w3.org/TR/xhtml1/DTD/xhtml1.dtd\">\n"
				"<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
				"<head>\n"
				"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"/>\n"
				"<meta name=\"generator\" content=\"abcm2ps-" VERSION "\"/>\n",
				fout);
			gen_info();
			fprintf(fout,
				"<style type=\"text/css\">\n"
				"\tbody {margin:0; padding:0; border:0;");
			if (cfmt.bgcolor != 0 && cfmt.bgcolor[0] != '\0')
				fprintf(fout, " background-color:%s",
						cfmt.bgcolor);
			fprintf(fout,
				"}\n"
				"\t@page {margin:0;}\n"
				"</style>\n"
				"<title>%s</title>\n"
				"</head>\n"
				"<body>\n",
				
				s);
		}
		fputs("<p>\n", fout);
		fprintf(fout, svg_head, w / 72, h / 72, w, h, title, "page", num);
		if (cfmt.bgcolor != 0 && cfmt.bgcolor[0] != '\0')
			fprintf(fout,
				"<rect width=\"100%%\" height=\"100%%\" fill=\"%s\"/>\n",
				cfmt.bgcolor);
	} else {				/* -g or -v */
		if (fout != stdout)
			fputs("<?xml version=\"1.0\" standalone=\"no\"?>\n"
				"<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n"
				"\t\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n",
				fout);
		else if (svg)
			fputs("<p>\n", fout);
		fprintf(fout, svg_head, w / 72, h / 72, w, h, title,
			epsf ? "tune" : "page", num);
		fputs("<!-- Creator: abcm2ps-" VERSION " -->\n", fout);
		gen_info();
		if (cfmt.bgcolor != 0 && cfmt.bgcolor[0] != '\0')
			fprintf(fout,
				"<rect width=\"100%%\" height=\"100%%\" fill=\"%s\"/>\n",
				cfmt.bgcolor);
	}

	/* if new page, done */
	if (file_initialized)
		return;

	elts_reset();
	n_sym = 0;

	in_cnt = 0;
	path = NULL;
	ps_error = 0;

	s = strdup("/defl 0 def\n"
		   "/gsc{gsave y T .7 dup scale 0 0}def\n");
	svg_write(s, strlen(s));
	free(s);
}

static void selfont(int back)
{
	char *p;
	int flags;

	if (gcur.font_n[0] == '\0')
		return;
	flags = 0;
	p = strchr(gcur.font_n, '-');
	if (!p) {
		fprintf(fout, " font-family=\"%s\" font-size=\"%.2f\"",
			gcur.font_n, gcur.font_s);
	} else {
		fprintf(fout, " font-family=\"%.*s\" font-size=\"%.2f\"",
			(int) (p - gcur.font_n), gcur.font_n, gcur.font_s);
		if (strstr(gcur.font_n, "Bold") != 0) {
			fprintf(fout, " font-weight=\"bold\"");
			flags = 1;
		}
		if (strstr(gcur.font_n, "Italic") != 0) {
			fprintf(fout, " font-style=\"italic\"");
			flags |= 2;
		} else if (strstr(gcur.font_n, "Oblique") != 0) {
			fprintf(fout, " font-style=\"oblique\"");
			flags |= 2;
		}
	}
	if (!back)
		return;
	if (!(flags & 1)
	 && strstr(gold.font_n, "Bold") != 0)
		fprintf(fout, " font-weight=\"normal\"");
	if (!(flags & 2)
	 && (strstr(gold.font_n, "Italic") != 0
	  || strstr(gold.font_n, "Oblique") != 0))
		fprintf(fout, " font-style=\"normal\"");
}

static float strw(char *s)
{
	unsigned short c;
	float w;

	w = 0;
	for (;;) {
		c = *s++;
		if (c == '\0')
			break;
		w += cwid(c) * 1.1;
	}
	return w * gcur.font_s;
}

/* define the global container */
static void setg(int newg);
static void defg1(void)
{
	setg(0);
	fputs("<g", fout);
	if (gcur.xscale != 1 || gcur.yscale != 1 || gcur.rotate != 0) {
		fprintf(fout, " transform=\"");
		if (gcur.xscale != 1 || gcur.yscale != 1) {
			if (gcur.xscale == gcur.yscale)
				fprintf(fout, "scale(%.3f)", gcur.xscale);
			else
				fprintf(fout, "scale(%.3f,%.3f)",
						gcur.xscale, gcur.yscale);
		}
		if (gcur.rotate != 0) {
			if (xoffs != 0 || yoffs != 0) {
				fprintf(fout, " translate(%.2f, %.2f)",
						xoffs, yoffs);
				x_rot = xoffs;
				y_rot = yoffs;
				xoffs = 0;
				yoffs = 0;
			}
			fprintf(fout, " rotate(%.2f)",
					gcur.rotate);
		}
		fputs("\"", fout);
	}
	if (gcur.linewidth != 1)
		fprintf(fout, " stroke-width=\"%.2f\"", gcur.linewidth);
	selfont(0);
	if (gcur.rgb != 0)
		fprintf(fout, " style=\"color:#%06x;fill:#%06x\"",
				gcur.rgb, gcur.rgb);
//jfm test
//	fprintf(fout, "%s>\n", gcur.dash);
	fprintf(fout, ">\n");
	g = 1;
	memcpy(&gold, &gcur, sizeof gold);
}

/*
 * set the state of the containers
 * state:
 *	0: no container
 *	1: graphical container
 *	2: graphical container and text
 * newg:
 *	0: close both the text and the graphical container
 *	1: close only the text and reset the graphical container
 */
static void setg(int newg)
{
#if 0 //path change
	if (in_path) {
		fputs("\"/>\n", fout);
		fprintf(stderr, "svg setg: No stroke nor fill\n");
//		ps_error = 1;
		in_path = 0;
	}
#endif
	if (g == 2) {
		fputs("</text>\n", fout);
		g = 1;
	}
	if (newg == 0) {
		if (g != 0) {
			fputs("</g>\n", fout);
			if (gcur.rotate != 0) {
				xoffs = x_rot;
				yoffs = y_rot;
				x_rot = 0;
				y_rot = 0;
			}
			g = 0;
		}
	} else if (memcmp(&gcur, &gold, sizeof gcur) != 0) {
		defg1();
	}
}

/* graphic path */
static void path_print(char *fmt, ...)
{
	va_list args;
	char *p;

	va_start(args, fmt);
	vsnprintf(path_buf, sizeof path_buf, fmt, args);
	va_end(args);
	if (!path) {
		path = malloc(strlen(path_buf) + 1);
		p = path;
	} else {
		path = realloc(path, strlen(path) + strlen(path_buf) + 1);
		p = path + strlen(path);
	}
	if (!path) {
		fprintf(stderr, "Out of memory.\n");
		exit(EXIT_FAILURE);
	}
	strcpy(p, path_buf);
}

static void path_def(void)
{
	if (path)
		return;
	setg(1);
	path_print("<path d=\"m%.2f %.2f\n", xoffs + cx, yoffs - cy);
}

static void path_end(void)
{
	setg(1);
	fputs(path, fout);
	free(path);
	path = NULL;
}

static void def_use(int def)
{
	int i;

	gcur.linewidth = DLW;
	setg(1);
	if (def_tb[def].defined)
		return;
	def_tb[def].defined = 1;
	fputs("<defs>\n", fout);
	i = def_tb[def].use;
	while (i != 0 && !def_tb[i].defined) {
		def_tb[i].defined = 1;
		fputs(def_tb[i].def, fout);
		i = def_tb[i].use;
	}
	fputs(def_tb[def].def, fout);
	fputs("</defs>\n", fout);
}

static void xysym(char *op, int use)
{
	float x, y;

	def_use(use);
	y = yoffs - pop_free_val();
	x = xoffs + pop_free_val();
	fprintf(fout, "<use x=\"%.2f\" y=\"%.2f\" xlink:href=\"#%s\"/>\n",
		x, y, op);
}

static void setxory(char *s, float v)
{
	struct elt_s *e;
	struct ps_sym_s *sym;

	sym = ps_sym_lookup(s);
	if (!sym || sym->e->type != VAL) {
		e = elt_new();
		if (!e)
			return;
		e->type = VAL;
		sym = ps_sym_def(s, e);
		if (!sym)
			return;
	}
	sym->e->u.v = v;
}

static void setxysym(char *op, int use)
{
	float x, y;

	y = pop_free_val();
	x = pop_free_val();
	setxory("x", x);
	setxory("y", y);
	def_use(use);
	fprintf(fout, "<use id=\"sym%d\" x=\"%.2f\" y=\"%.2f\" xlink:href=\"#%s\"/>\n",
		id++, xoffs + x, yoffs - y, op);
}

/*  gua gda (acciaccatura) */
static void acciac(char *op)
{
	struct ps_sym_s *sym;
	float x, y, dx, dy;

	setg(1);
	dy = pop_free_val();
	dx = pop_free_val();
	sym = ps_sym_lookup("x");
	x = xoffs + sym->e->u.v;
	sym = ps_sym_lookup("y");
	y = yoffs - sym->e->u.v;
	if (op[1] == 'u') {
		x -= 1;
		y -= 4;
	} else {
		x -= 5;
		y += 4;
	}
	fprintf(fout,
		"<path d=\"M%.2f %.2fl%.2f %.2f\" stroke=\"currentColor\" fill=\"none\"/>\n",
		x, y, dx, -dy);
}

/* arp - ltr */
static void arp_ltr(char type)
{
	float x, y, t, w;
	int n;

	def_use(D_ltr);
	y = yoffs - pop_free_val();
	x = xoffs + pop_free_val();
	w = pop_free_val();
	n = (w + 5) / 6;
	if (type == 'a') {
		fprintf(fout, "<g transform=\"rotate(270)\">\n");
		t = x;
		x = -y;
		y = t;
	}
	y -= 4;
	while (--n >= 0) {
		fprintf(fout, "<use x=\"%.2f\" y=\"%.2f\" xlink:href=\"#ltr\"/>\n",
			x, y);
			x += 6;
	}
	if (type == 'a')
		fprintf(fout, "</g>\n");
}

/* sd su gd gu */
static void stem(char *op)
{
	struct ps_sym_s *sym;
	float x, y, dx, h;

	gcur.linewidth = DLW;
	setg(1);
	h = pop_free_val();
	if (op[0] == 's')
		dx = 3.5;
	else
		dx = 1.6;
	if (op[1] == 'd')
		dx = -dx;
	sym = ps_sym_lookup("x");
	x = xoffs + sym->e->u.v + dx;
	sym = ps_sym_lookup("y");
	y = yoffs - sym->e->u.v;

	fprintf(fout,
		"<path id=\"stem%d\" d=\"M%.2f %.2fv%.2f\" stroke=\"currentColor\" fill=\"none\"/>\n",
		id, x, y, -h);
}

/* output a xml string */
static void xml_str_out(char *p)
{
	char *q, *r;

	q = p;
	for (q = p; *p != 0;) {
		switch (*p++) {
		case '<': r = "&lt;"; break;
		case '>': r = "&gt;"; break;
		case '\'': r = "&apos;"; break;
		case '"': r = "&quot;"; break;
		case '&':
			if (*p == '#')
				continue;
			r = "&amp;";
			break;
		default:
			continue;
		}
		if (p - 1 != q)
			fwrite(q, 1, p - 1 - q, fout);
		q = p;
		fputs(r, fout);
	}
	if (p != q)
		fputs(q, fout);
}

/*
 * types:
 *	s show / c showc / r showr / j jshow / b showb /x gxshow
 */
static void show(char type)
{
	float x, y, w;
	char tmp[4], *s, *p, *q;
	int span;

	span = 0;
	if (memcmp(&gcur, &gold, sizeof gcur) != 0) {
		if (g == 2)
			span = 1;
		else
			defg1();
	}
	switch (type) {
	case 'j':
		x = cx;
		y = cy;
		w = pop_free_val();
		p = tmp;
		tmp[0] = '\0';
		s = NULL;
		break;
	default:
		x = cx;
		y = cy;
		if (stack->type == STR) {
			s = pop_free_str();
			if (!s || s[0] != '(') {
				fprintf(stderr, "svg: No string\n");
				ps_error = 1;
				return;
			}
			p = s + 1;			/* remove '(' */
		} else {
			p = tmp;
			tmp[0] = pop_free_val();
			tmp[1] = '\0';
			s = NULL;
		}
		w = strw(p);
		if (type == 'x') {		/* gxshow */
			w = pop_free_val();	/* inter TAB width */
			q = strchr(p, '\t');
			*q = '\0';		/* string after the 1st one */
		}
		break;
	}
	if (span) {
		fprintf(fout,"<tspan\n\t");
		selfont(1);
		fprintf(fout,">");
	} else if (g != 2) {
		fprintf(fout, "<text x=\"%.2f\" y=\"%.2f\"", x + xoffs, yoffs - y);
		switch (type) {
		case 'c':
			fprintf(fout, " text-anchor=\"middle\"");
			w /= 2;
			break;
		case 'r':
			fprintf(fout, " text-anchor=\"end\"");
			w = 0;
			break;
		case 'j':
			fprintf(fout, " textLength=\"%.2f\"", w);
			break;
		}

//		if (gcur.rgb != 0)
//			fprintf(fout, " fill=\"currentColor\"");
		fputs(">", fout);
		g = 2;
	}

back:
	xml_str_out(p);
	if (span)
		fprintf(fout, "</tspan>");

	if (type == 'x') {
		p = p + strlen(p) + 1;		/* next string of gxshow */
		q = strchr(p, '\t');
		if (q) {
			*q = '\0';
		} else {

			/* restore the string width (!! tied to elt_free() !!) */
			w = free_elt->u.v;
			type = 's';
		}
		fprintf(fout, "<tspan dx=\"%.2f\">", w);
		span = 1;
		goto back;
	}
	if (type == 'b') {
		setg(1);
		fprintf(fout,
			"<rect stroke=\"currentColor\" fill=\"none\" stroke-width=\"0.6\"\n"
			"	x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\"/>\n",
			xoffs + cx - 2, yoffs - y - gcur.font_s + 2, w + 4, gcur.font_s + 1);
	}
	cx = x + w;
	if (s)
		free(s);
}

static void ps_exec(char *op);

/* execute a sequence
 * returns 1 on 'exit' or error */
static int seq_exec(struct elt_s *e)
{
	struct elt_s *e2;

	switch (e->type) {
	case STR:
		if (e->u.s[0] != '/'
		 && e->u.s[0] != '(') {
			if (strcmp(e->u.s, "exit") == 0)
				return 1;
			ps_exec(e->u.s);
			return 0;
		}
		/* fall thru */
	case VAL:
	case BRK:
		e = elt_dup(e);
		if (!e)
			return 1;
		push(e);
		return 0;
	}
	/* (e->type == SEQ) */
	e = e->u.e;
	while (e) {
		switch (e->type) {
		case STR:
			if (strcmp(e->u.s, "exit") == 0)
				return 1;
			if (e->u.s[0] != '(' && e->u.s[0] != '/') {
				ps_exec(e->u.s);
				break;
			}
			/* fall thru */
		default:
			e2 = elt_dup(e);
			if (!e2)
				return 1;
			push(e2);
			break;
		}
		e = e->next;
	}
	return 0;
}

/* execute a command */
/* (in case of error, a string may be not freed, but it is not important!) */
static void ps_exec(char *op)
{
	struct ps_sym_s *sym;
	struct elt_s *e, *e2;
	float x, y, w, h;
	int n;
	char *s;

	if (ps_error)
		return;
#if 0
fprintf(stderr, "%s ", op);
stack_dump();
#endif
	sym = ps_sym_lookup(op);
	if (sym) {
		sym->exec++;
		if (sym->exec > 2) {
			fprintf(stderr, "svg: Too many recursions\n");
			ps_error = 1;
			return;
		}
		seq_exec(sym->e);
		sym->exec--;
		return;
	}

	if (*op == ' ')				/* load */
		op++;

	switch (*op) {
	case '!':				/* def */
		if (op[1] == '\0') {
			if (!stack) {
				fprintf(stderr, "svg def: Stack empty\n");
				ps_error = 1;
				return;
			}
			e = pop(stack->type);	/* value */
			s = pop_free_str();	/* symbol */
			if (!s || *s != '/') {
				fprintf(stderr, "svg def: No / bad symbol\n");
				ps_error = 1;
				return;
			}
			ps_sym_def(&s[1], e);
			free(s);
			return;
		}
		break;
	case 'a':
		if (strcmp(op, "accent") == 0) {
			xysym(op, D_accent);
			return;
		}
		if (strcmp(op, "abs") == 0) {
			if (!stack || stack->type != VAL) {
				fprintf(stderr, "svg abs: Bad value\n");
				ps_error = 1;
				return;
			}
			if (stack->u.v < 0)
				stack->u.v = -stack->u.v;
			return;
		}
		if (strcmp(op, "add") == 0) {
			x = pop_free_val();
			if (!stack || stack->type != VAL) {
				fprintf(stderr, "svg add: Bad value\n");
				ps_error = 1;
				return;
			}
			stack->u.v += x;
			return;
		}
		if (strcmp(op, "and") == 0) {
			x = pop_free_val();
			if (!stack || stack->type != VAL) {
				fprintf(stderr, "svg and: Bad value\n");
				ps_error = 1;
				return;
			}
			stack->u.v = (int) x & (int) stack->u.v;
			return;
		}
		if (strcmp(op, "anshow") == 0) {
			show('s');
			return;
		}
		if (strcmp(op, "arc") == 0
		 || strcmp(op, "arcn") == 0) {
			float r, a1, a2, x1, y1, x2, y2;

			path_def();
			a2 = pop_free_val();
			a1 = pop_free_val();
			r = pop_free_val();
			if (r < 0) {
				fprintf(stderr, "svg arc: Bad value\n");
				ps_error = 1;
				return;
			}
			y = pop_free_val();
			x = pop_free_val();
			x1 = x + r * cos(a1 * M_PI / 180);
			y1 = y + r * sinf(a1 * M_PI / 180);
			if (a1 >= 360)
				a1 -= 360;
			if (a2 >= 360)
				a2 -= 360;
			path_print("\t", fout);
			if (x1 != cx || y1 != cy)
				path_print("m%.2f %.2f", 
					x1 - cx, -(y1 - cy));
			if (a1 == a2) {			/* circle */
				a2 = 180 - a1;
				x2 = x + r * cosf(a2 * M_PI / 180);
				y2 = y + r * sinf(a2 * M_PI / 180);
				path_print("a%.2f %.2f 0 0 %d %.2f %.2f"
						"a%.2f %.2f 0 0 %d %.2f %.2f\n",
					r, r, op[3] == 'n', x2 - x1, -(y2 - y1),
					r, r, op[3] == 'n', x1 - x2, -(y1 - y2));
				cx = x1;
				cy = y1;
			} else {
				x2 = x + r * cosf(a2 * M_PI / 180);
				y2 = y + r * sinf(a2 * M_PI / 180);
				path_print("a%.2f %.2f 0 0 %d %.2f %.2f\n",
					r, r, op[3] == 'n', x2 - x1, -(y2 - y1));
				cx = x2;
				cy = y2;
			}
			return;
		}
		if (strcmp(op, "arp") == 0) {
			arp_ltr('a');
			return;
		}
		if (strcmp(op, "atan") == 0) {
			y = pop_free_val();	/* den */
			if (!stack || stack->type != VAL) {
				fprintf(stderr, "svg atan: Bad value\n");
				ps_error = 1;
				return;
			}
			x = stack->u.v;		/* num */
			if (x == 0 && y == 0) {
				fprintf(stderr, "svg atan: Bad value\n");
				ps_error = 1;
				return;
			}
			stack->u.v = atan(x / y) / M_PI * 180;
			return;
		}
		break;
	case 'b':
		if (strcmp(op, "bar") == 0) {
			setg(1);
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val();
			h = pop_free_val();
			fprintf(fout, "<path stroke=\"currentColor\" fill=\"none\"\n"
				"	d=\"M%.2f %.2fv%.2f\"/>\n",
				x, y, -h);
			return;
		}
		if (strcmp(op, "bclef") == 0) {
			xysym(op, D_bclef);
			return;
		}
		if (strcmp(op, "bdef") == 0) {
			ps_exec("!");
			return;
		}
		if (strcmp(op, "bind") == 0) {
			return;
		}
		if (strcmp(op, "bitshift") == 0) {
			int shift;

			shift = pop_free_val();
			if (!stack || stack->type != VAL
			 || shift >= 32  || shift < -32) {
				fprintf(stderr, "svg: Bad value for bitshift\n");
				ps_error = 1;
				return;
			}
			if (shift > 0)
				n = (int) stack->u.v << shift;
			else
				n = (int) stack->u.v >> -shift;
			stack->u.v = n;
			return;
		}
		if (strcmp(op, "bm") == 0) {
			float dx, dy;

			setg(1);
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val();
			dy = pop_free_val();
			dx = pop_free_val();
			h = pop_free_val();
			fprintf(fout,
				"<path fill=\"currentColor\"\n"
				"	d=\"M%.2f %.2fl%.2f %.2fv%.2fl%.2f %.2f\"/>\n",
				x, y, dx, -dy, h,-dx, dy);
			return;
		}
		if (strcmp(op, "bnum") == 0
		 || strcmp(op, "bnumb") == 0) {
			setg(1);
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val();
			s = pop_free_str();
			if (!s) {
				fprintf(stderr, "svg: No string\n");
				ps_error = 1;
				return;
			}
			if (op[4] == 'b') {
				w = 7 * strlen(s);
				fprintf(fout,
					"<rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"12\" fill=\"white\"/>\n",
					x - w / 2, y - 10, w);
			}
			fprintf(fout,
				"<text font-family=\"Times\" font-size=\"12\" font-style=\"italic\" font-weight=\"normal\"\n"
				"	x=\"%.2f\" y=\"%.2f\" text-anchor=\"middle\">%s</text>\n",
				x, y, s + 1);
			free(s);
			return;
		}
		if (strcmp(op, "box") == 0) {
			setg(1);
			h = pop_free_val();
			w = pop_free_val();
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val();
			fprintf(fout,
				"<rect stroke=\"currentColor\" fill=\"none\"\n"
				"	x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\"/>\n",
				x, y - h, w, h);
			return;
		}
		if (strcmp(op, "boxdraw") == 0) {
			setg(1);
			h = pop_free_val();
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val();
			fprintf(fout,
				"<rect stroke=\"currentColor\" fill=\"none\"\n"
				"	x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\"/>\n",
				x, y - h, boxend - (x - xoffs) + 6, h);
			return;
		}
		if (strcmp(op, "boxmark") == 0) {
			if (cx > boxend)
				boxend = cx;
			return;
		}
		if (strcmp(op, "boxstart") == 0) {
			boxend = cx;
			return;
		}
		if (strcmp(op, "brace") == 0) {
			def_use(D_brace);
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val();
			h = pop_free_val() * 0.01;
			fprintf(fout,
				"<g transform=\"translate(%.2f,%.2f) scale(1,%.2f)\">\n"
				"	<use xlink:href=\"#brace\"/>\n"
				"</g>\n",
				x, y, h);
			return;
		}
		if (strcmp(op, "bracket") == 0) {
			setg(1);
			y = yoffs - pop_free_val() - 3;
			x = xoffs + pop_free_val() - 5;
			h = pop_free_val() + 2;
			fprintf(fout,
				"<path fill=\"currentColor\"\n"
				"	d=\"M%.2f %.2f\n"
				"	c10.5 1 12 -4.5 12 -3.5c0 1 -3.5 5.5 -8.5 5.5\n"
				"	v%.2f\n"
				"	c5 0 8.5 4.5 8.5 5.5c0 1 -1.5 -4.5 -12 -3.5\"/>\n",
				x, y, h);
			return;
		}
		if (strcmp(op, "breve") == 0) {
			setxysym(op, D_breve);
			return;
		}
		if (strcmp(op, "brth") == 0) {
			setg(1);
			y = yoffs - pop_free_val() - 6;
			x = xoffs + pop_free_val();
			fprintf(fout, "<text x=\"%.2f\" y=\"%.2f\" font-family=\"Times\" font-size=\"30\"\n"
				"	font-weight=\"bold\" font-style=\"italic\">,</text>\n",
				x, y);
			return;
		}
		break;
	case 'C':
		if (strcmp(op, "C") == 0) {
			float c1, c2, c3, c4;

curveto:
			path_def();
			y = pop_free_val();
			x = pop_free_val();
			c4 = yoffs - pop_free_val();
			c3 = xoffs + pop_free_val();
			c2 = yoffs - pop_free_val();
			c1 = xoffs + pop_free_val();
			path_print("\tC%.2f %.2f %.2f %.2f %.2f %.2f\n",
				c1, c2, c3, c4, xoffs + x, yoffs - y);
			cx = x;
			cy = y;
			return;
		}
		break;
	case 'c':
		if (strcmp(op, "cclef") == 0) {
			xysym(op, D_cclef);
			return;
		}
		if (strcmp(op, "csig") == 0) {
			xysym(op, D_csig);
			return;
		}
		if (strcmp(op, "ctsig") == 0) {
			xysym(op, D_ctsig);
			return;
		}
		if (strcmp(op, "coda") == 0) {
			xysym(op, D_coda);
			return;
		}
		if (strcmp(op, "closepath") == 0) {
			path_def();
			path_print("\tz");
			return;
		}
		if (strcmp(op, "composefont") == 0) {
			pop(BRK);
			pop(STR);
			return;
		}
		if (strcmp(op, "copy") == 0) {
			struct elt_s *e3;

			n = pop_free_val();
			if ((unsigned) n > 10) {
				fprintf(stderr, "svg copy: Too wide\n");
				ps_error = 1;
				return;
			}
			e = stack;
			e2 = NULL;
			while (--n >= 0) {
				if (!e)
					break;
				e3 = elt_dup(e);
				if (!e3)
					return;
				e3->next = e2;
				e2 = e3;
				e = e->next;
			}
			if (n >= 0) {
				fprintf(stderr, "svg copy: Stack empty\n");
				ps_error = 1;
				return;
			}
			while (e2 != 0) {
				e3 = e2->next;
				push(e2);
				e2 = e3;
			}
			return;
		}
		if (strcmp(op, "cos") == 0) {
			if (!stack || stack->type != VAL) {
				fprintf(stderr, "svg cos: Bad value\n");
				ps_error = 1;
				return;
			}
			stack->u.v = cos(stack->u.v * M_PI / 180);
			return;
		}
		if (strcmp(op, "cpu") == 0) {
			xysym(op, D_cpu);
			return;
		}
		if (strcmp(op, "cresc") == 0) {
			setg(1);
			y = yoffs - pop_free_val() - 5;
			x = xoffs + pop_free_val();
			w = pop_free_val();
			sym = ps_sym_lookup("defl");
			x += w;
			if ((int) sym->e->u.v & 1)
				fprintf(fout, "<path stroke=\"currentColor\" fill=\"none\"\n"
					"d=\"M%.2f %.2fl%.2f -2.2m0 -3.6l%.2f -2.2\"/>\n",
					x, y, -w, w);
			else
				fprintf(fout, "<path stroke=\"currentColor\" fill=\"none\"\n"
					"d=\"M%.2f %.2fl%.2f -4l%.2f -4\"/>\n",
					x, y, -w, w);
			return;
		}
		if (strcmp(op, "custos") == 0) {
			xysym(op, D_custos);
			return;
		}
		if (strcmp(op, "currentgray") == 0) {
			e = elt_new();
			if (!e)
				return;
			e->type = VAL;
			e->u.v = (float) gcur.rgb / 0xffffff;
			push(e);
			return;
		}
		if (strcmp(op, "currentpoint") == 0) {
			e = elt_new();
			if (!e)
				return;
			e->type = VAL;
			e->u.v = cx;
			push(e);
			e = elt_new();
			if (!e)
				return;
			e->type = VAL;
			e->u.v = cy;
			push(e);
			return;
		}
		if (strcmp(op, "curveto") == 0)
			goto curveto;
		if (strcmp(op, "cvi") == 0) {
			if (!stack || stack->type != VAL) {
				fprintf(stderr, "svg cvi: Bad value\n");
				ps_error = 1;
				return;
			}
			n = stack->u.v;
			stack->u.v = n;
			return;
		}
		if (strcmp(op, "cvx") == 0) {
			s = pop_free_str();
			if (!s || ((*s != '/') && (*s != '('))) {
				fprintf(stderr, "svg cvx: No / bad string\n");
				ps_error = 1;
				return;
			}
			*s = '{';
			svg_write(s, strlen(s));
			svg_write("}", 1);
			free(s);
			return;
		}
		break;
	case 'd':
		if (strcmp(op, "dacs") == 0) {
			setg(1);
			y = yoffs - pop_free_val() - 3;
			x = xoffs + pop_free_val();
			s = pop_free_str();
			if (!s) {
				fprintf(stderr, "svg dacs: No string\n");
				ps_error = 1;
				return;
			}
			fprintf(fout, "<text font-family=\"Times\" font-size=\"16\" font-weight=\"normal\" font-style=\"normal\"\n"
				"	x=\"%.2f\" y=\"%.2f\" text-anchor=\"middle\">%s</text>\n",
				x, y, s + 1);
			free(s);
			return;
		}
		if (strcmp(op, "def") == 0) {
			ps_exec("!");
			return;
		}
		if (strcmp(op, "dim") == 0) {
			setg(1);
			y = yoffs - pop_free_val() - 5;
			x = xoffs + pop_free_val();
			w = pop_free_val();
			sym = ps_sym_lookup("defl");
			if ((int) sym->e->u.v & 2)
				fprintf(fout, "<path stroke=\"currentColor\" fill=\"none\"\n"
					"d=\"M%.2f %.2fl%.2f -2.2m0 -3.6l%.2f -2.2\"/>\n",
					x, y, w, -w);
			else
				fprintf(fout, "<path stroke=\"currentColor\" fill=\"none\"\n"
					"d=\"M%.2f %.2fl%.2f -4l%.2f -4\"/>\n",
					x, y, w, -w);
			return;
		}
		if (strcmp(op, "div") == 0) {
			x = pop_free_val();
			if (!stack || stack->type != VAL || x == 0) {
				fprintf(stderr, "svg: Bad value for div\n");
				ps_error = 1;
				return;
			}
			stack->u.v /= x;
			return;
		}
		if (strcmp(op, "dnb") == 0) {
			xysym(op, D_dnb);
			return;
		}
		if (strcmp(op, "dplus") == 0) {
			xysym(op, D_dplus);
			return;
		}
		if (strcmp(op, "dSL") == 0) {
			float a1, a2, a3, a4, a5, a6, m1, m2;

			setg(1);
			m2 = yoffs - pop_free_val();
			m1 = xoffs + pop_free_val();
			a6 = pop_free_val();
			a5 = pop_free_val();
			a4 = pop_free_val();
			a3 = pop_free_val();
			a2 = pop_free_val();
			a1 = pop_free_val();
			fprintf(fout,
				"<path stroke=\"currentColor\" fill=\"none\" stroke-dasharray=\"5,5\"\n"
				"	d=\"M%.2f %.2fc%.2f %.2f %.2f %.2f %.2f %.2f\"/>\n",
					m1, m2, a1, -a2, a3, -a4, a5, -a6);
			return;
		}
		if (strcmp(op, "dt") == 0) {
			setg(1);
			sym = ps_sym_lookup("x");
			x = xoffs + sym->e->u.v;
			sym = ps_sym_lookup("y");
			y = yoffs - sym->e->u.v;
			y -= pop_free_val();
			x += pop_free_val();
			fprintf(fout,
				"<circle fill=\"currentColor\" cx=\"%.2f\" cy=\"%.2f\" r=\"1.2\"/>\n",
				x, y);
			return;
		}
		if (strcmp(op, "dlw") == 0) {
			gcur.linewidth = DLW;
			return;
		}
		if (strcmp(op, "dotbar") == 0) {
			setg(1);
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val();
			h = pop_free_val();
			fprintf(fout,
				"<path stroke=\"currentColor\" fill=\"none\" stroke-dasharray=\"5,5\"\n"
				"	d=\"M%.2f %.2fv%.2f\"/>\n",
				x, y, -h);
			return;
		}
		if (strcmp(op, "dup") == 0) {
			if (!stack) {
				fprintf(stderr, "svg dup: Stack empty\n");
				ps_error = 1;
				return;
			}
			e = elt_dup(stack);
			if (e != 0)
				push(e);
			return;
		}
		if (strcmp(op, "dft0") == 0) {
			xysym(op, D_dft0);
			return;
		}
		if (strcmp(op, "dsh0") == 0) {
			xysym(op, D_dsh0);
			return;
		}
		break;
	case 'e':
		if (strcmp(op, "emb") == 0) {
			xysym(op, D_emb);
			return;
		}
		if (strcmp(op, "eofill") == 0) {
			if (!path) {
				fprintf(stderr, "svg eofill: No path\n");
				ps_error = 1;
				return;
			}
			path_end();
			fprintf(fout, "\t\" fill-rule=\"evenodd\" fill=\"currentColor\"/>\n");
			return;
		}
		if (strcmp(op, "eq") == 0) {
			cond(C_EQ);
			return;
		}
		if (strcmp(op, "exch") == 0) {
			if (!stack || !stack->next) {
				fprintf(stderr, "svg exch: Stack empty\n");
				ps_error = 1;
				return;
			}
			e = stack->next;
			stack->next = e->next;
			e->next = stack;
			stack = e;
			return;
		}
		if (strcmp(op, "exec") == 0) {
			e = pop(SEQ);
			if (!e)
				return;
			seq_exec(e);
			elt_free(e);
			return;
		}
		break;
	case 'F':
		if (sscanf(op, "F%d", &n) == 1) {
			if (strlen(fontnames[n]) >= sizeof gcur.font_n - 1) {
				fprintf(stderr, "svg %s: Font name too long\n",
						op);
			}
			strncpy(gcur.font_n, fontnames[n], sizeof gcur.font_n);
			gcur.font_n[sizeof gcur.font_n - 1] = '\0';
			gcur.font_s = pop_free_val();
			return;
		}
		break;
	case 'f':
		if (strcmp(op, "false") == 0) {
			e = elt_new();
			if (!e)
				return;
			e->type = VAL;
			e->u.v = 0;
			push(e);
			return;
		}
		if (strcmp(op, "fill") == 0) {
			if (!path) {
				fprintf(stderr, "svg fill: No path\n");
//				ps_error = 1;
				return;
			}
			path_end();
			fprintf(fout, "\t\" fill=\"currentColor\"/>\n");
			return;
		}
		if (strcmp(op, "findfont") == 0) {
			s = pop_free_str();
			if (!s
			 || *s != '/'
			 || strlen(s) >= sizeof gcur.font_n) {
				fprintf(stderr, "svg selectfont: No / bad font\n");
				ps_error = 1;
				return;
			}
			strcpy(gcur.font_n, s + 1);
			free(s);
			return;
		}
		if (strcmp(op, "fng") == 0) {
			setg(1);
			y = yoffs - pop_free_val() - 1;
			x = xoffs + pop_free_val() - 3;
			s = pop_free_str();
			if (!s) {
				fprintf(stderr, "svg fng: No string\n");
				ps_error = 1;
				return;
			}
			fprintf(fout, "<text font-family=\"Bookman\" font-size=\"8\" font-weight=\"normal\" font-style=\"normal\"\n"
				"	x=\"%.2f\" y=\"%.2f\">%s</text>\n",
				x, y, s + 1);
			free(s);
			return;
		}
		if (strcmp(op, "for") == 0) {
			float init, incr, limit;

			e = pop(SEQ);			/* proc */
			if (!e)
				return;
			limit = pop_free_val();
			incr = pop_free_val();
			init = pop_free_val();
			if (incr == 0
			 || (limit - init) / incr > 100) {
				fprintf(stderr, "svg for: Bad values\n");
				ps_error = 1;
				return;
			}
			if (incr > 0) {
				while (init <= limit) {
					e2 = elt_new();
					if (!e2)
						break;
					e2->type = VAL;
					e2->u.v = init;
					push(e2);
					if (seq_exec(e) != 0)
						break;
					init += incr;
				}
			} else {
				while (init >= limit) {
					e2 = elt_new();
					if (!e2)
						break;
					e2->type = VAL;
					e2->u.v = init;
					push(e2);
					if (seq_exec(e) != 0)
						break;
					init += incr;
				}
			}
			elt_free(e);
			return;
		}
		if (strcmp(op, "ft0") == 0) {
			xysym(op, D_ft0);
			return;
		}
		if (strcmp(op, "ft1") == 0) {
			xysym(op, D_ft1);
			return;
		}
		if (strcmp(op, "ft513") == 0) {
			xysym(op, D_ft513);
			return;
		}
		break;
	case 'g':
		if (strcmp(op, "gcshow") == 0) {
			show('s');
			return;
		}
		if (strcmp(op, "ge") == 0) {
			cond(C_GE);
			return;
		}
		if (strcmp(op, "get") == 0) {
			n = pop_free_val();
			if (!stack) {
				fprintf(stderr, "svg get: Stack empty\n");
				ps_error = 1;
				return;
			}
			switch (stack->type) {
			case VAL:
				if (n != 0) {
					fprintf(stderr, "svg get: Out of bounds\n");
					ps_error = 1;
					return;
				}
				return;
			case STR:
				s = stack->u.s;
				if (*s != '(') {
					fprintf(stderr, "svg get: Not a string\n");
					ps_error = 1;
					return;
				}
				if ((unsigned) n >= strlen(s) - 1) {
					fprintf(stderr, "svg get: Out of bounds\n");
					ps_error = 1;
					return;
				}
				stack->type = VAL;
				stack->u.v = s[n + 1];
				free(s);
				return;
			}
			e = stack->u.e;
			e2 = 0;
			while (--n >= 0) {
				if (!e)
					break;
				e2 = e;
				e = e->next;
			}
			if (!e) {
				fprintf(stderr, "svg get: Out of bounds\n");
				ps_error = 1;
				return;
			}
			if (!e2)
				stack->u.e = e->next;
			else
				e2->next = e->next;
			e->next = stack->next;
			elt_free(stack);
			stack = e;
			return;
		}
		if (strcmp(op, "getinterval") == 0) {
			int count;

			count = pop_free_val();
			n = pop_free_val();
			s = pop_free_str();
			if (!s || *s != '(') {
				fprintf(stderr, "svg getinterval: No string\n");
				ps_error = 1;
				return;
			}
			if ((unsigned) n >= strlen(s)
			 || (unsigned) count >= strlen(s) - n) {
				fprintf(stderr, "svg getinterval: Out of bounds\n");
				ps_error = 1;
				return;
			}
			e = elt_new();
			if (!e)
				return;
			e->type = STR;
			e->u.s = malloc(count + 2);
			e->u.s[0] = '(';
			memcpy(&e->u.s[1], &s[n + 1], count);
			e->u.s[count + 1] = '\0';
			push(e);
			free(s);
			return;
		}
		if (strcmp(op, "ghd") == 0) {
			setxysym(op, D_ghd);
			return;
		}
		if (strcmp(op, "ghl") == 0) {
			xysym(op, D_ghl);
			return;
		}
		if (strcmp(op, "gt") == 0) {
			cond(C_GT);
			return;
		}
		if (strcmp(op, "gu") == 0
		 || strcmp(op, "gd") == 0) {
			stem(op);
			return;
		}
		if (strcmp(op, "gua") == 0
		 || strcmp(op, "gda") == 0) {
			acciac(op);
			return;
		}
		if (strcmp(op, "grestore") == 0) {
			if (nsave <= 0) {
				fprintf(stderr, "svg grestore: No gsave\n");
				ps_error = 1;
				return;
			}
			setg(1);
			nsave--;
			cx = gsave[nsave].cx;
			cy = gsave[nsave].cy;
			xoffs = gsave[nsave].xoffs;
			yoffs = gsave[nsave].yoffs;
			x_rot = gsave[nsave].x_rot;
			y_rot = gsave[nsave].y_rot;
			memcpy(&gcur, &gsave[nsave].gc, sizeof gcur);
			return;
		}
		if (strcmp(op, "grm") == 0) {
			xysym(op, D_grm);
			return;
		}
		if (strcmp(op, "gsave") == 0) {
			if (nsave >= (int) (sizeof gsave / sizeof gsave[0])) {
				fprintf(stderr, "svg grestore: Too many gsave's\n");
				ps_error = 1;
				return;
			}
			setg(1);
			memcpy(&gsave[nsave].gc, &gcur, sizeof gsave[0].gc);
			gsave[nsave].cx = cx;
			gsave[nsave].cy = cy;
			gsave[nsave].xoffs = xoffs;
			gsave[nsave].yoffs = yoffs;
			gsave[nsave].x_rot = x_rot;
			gsave[nsave].y_rot = y_rot;
			nsave++;
			return;
		}
		if (strcmp(op, "gsl") == 0) {
			float a1, a2, a3, a4, a5, a6, m1, m2;

			setg(1);
			m2 = yoffs - pop_free_val();
			m1 = xoffs + pop_free_val();
			a6 = pop_free_val();
			a5 = pop_free_val();
			a4 = pop_free_val();
			a3 = pop_free_val();
			a2 = pop_free_val();
			a1 = pop_free_val();
			fprintf(fout,
				"<path stroke=\"currentColor\" fill=\"none\"\n"
				"	d=\"M%.2f %.2fc%.2f %.2f %.2f %.2f %.2f %.2f\"/>\n",
					m1, m2, a1, -a2, a3, -a4, a5, -a6);
			return;
		}
		if (strcmp(op, "gxshow") == 0) {
			show('x');
			return;
		}
		break;
	case 'H':
		if (strcmp(op, "Hd") == 0) {
			setxysym(op, D_Hd);
			return;
		}
		if (strcmp(op, "HD") == 0) {
			setxysym(op, D_HD);
			return;
		}
		if (strcmp(op, "HDD") == 0) {
			setxysym(op, D_HDD);
			return;
		}
		break;
	case 'h':
		if (strcmp(op, "hd") == 0) {
			setxysym(op, D_hd);
			return;
		}
		if (strcmp(op, "hl") == 0) {
			xysym(op, D_hl);
			return;
		}
		if (strcmp(op, "hl1") == 0) {
			xysym(op, D_hl1);
			return;
		}
		if (strcmp(op, "hl2") == 0) {
			xysym(op, D_hl2);
			return;
		}
		if (strcmp(op, "hld") == 0) {
			xysym(op, D_hld);
			return;
		}
		if (strcmp(op, "hyph") == 0) {
			int d;

			setg(1);
			y = pop_free_val();
			x = pop_free_val();
			w = pop_free_val();
			d = 25 + (int) w / 20 * 3;
			n = (w - 15.) / d;
			x += (w - d * n - 5) / 2;
			fprintf(fout, "<path stroke=\"currentColor\" fill=\"none\" stroke-width=\"1.2\"\n"
				"	stroke-dasharray=\"5,%d\"\n"
				"	d=\"M%.2f %.2fh%d\"/>\n",
				d - 5,
				xoffs + x, yoffs - y - gcur.font_s * 0.3, d * n + 5);
			return;
		}
		break;
	case 'i':
		if (strcmp(op, "idiv") == 0) {
			n = pop_free_val();
			if (!stack || stack->type != VAL || n == 0) {
				fprintf(stderr, "svg idiv: Bad value\n");
				ps_error = 1;
				return;
			}
			n = (int) stack->u.v / n;
			stack->u.v = n;
			return;
		}
		if (strcmp(op, "if") == 0) {
			e = pop(SEQ);		/* sequence */
			if (!e)
				return;
			n = pop_free_val();	/* condition */
			if (n != 0)
				seq_exec(e);
			elt_free(e);
			return;
		}
		if (strcmp(op, "ifelse") == 0) {
			e2 = pop(SEQ);		/* sequence 2 */
			e = pop(SEQ);		/* sequence 1 */
			if (!e || !e2)
				return;
			n = pop_free_val();	/* condition */
			if (n != 0)
				seq_exec(e);
			else
				seq_exec(e2);
			elt_free(e);
			elt_free(e2);
			return;
		}
		if (strcmp(op, "imsig") == 0) {
			xysym(op, D_imsig);
			return;
		}
		if (strcmp(op, "iMsig") == 0) {
			xysym(op, D_iMsig);
			return;
		}
		if (strcmp(op, "index") == 0) {
			n = pop_free_val();
			e = stack;
			while (--n >= 0) {
				if (!e)
					break;
				e = e->next;
			}
			if (!e) {
				fprintf(stderr, "svg index: Stack empty\n");
				ps_error = 1;
				return;
			}
			e = elt_dup(e);
			if (!e)
				return;
			push(e);
			return;
		}
		break;
	case 'j':
		if (strcmp(op, "jshow") == 0) {
			show('j');
			return;
		}
		break;
	case 'L':
		if (strcmp(op, "L") == 0) {
lineto:
			path_def();
			y = pop_free_val();
			x = pop_free_val();
			if (x == cx)
				path_print("\tv%.2f\n", cy - y);
			else if (y == cy)
				path_print("\th%.2f\n", x - cx);
			else
				path_print("\tl%.2f %.2f\n",
					x - cx, cy - y);
			cx = x;
			cy = y;
			return;
		}
	case 'l':
		if (strcmp(op, "le") == 0) {
			cond(C_LE);
			return;
		}
		if (strcmp(op, "lt") == 0) {
			cond(C_LT);
			return;
		}
		if (strcmp(op, "length") == 0) {
			s = pop_free_str();
			if (!s || *s != '(') {
				fprintf(stderr, "svg length: No string\n");
				ps_error = 1;
				return;
			}
			e = elt_new();
			if (!e)
				return;
			e->type = VAL;
			e->u.v = strlen(s + 1);
			push(e);
			free(s);
			return;
		}
		if (strcmp(op, "lineto") == 0)
			goto lineto;
		if (strcmp(op, "lmrd") == 0) {
			xysym(op, D_lmrd);
			return;
		}
		if (strcmp(op, "load") == 0) {
			s = pop_free_str();
			if (!s || *s != '/') {
				fprintf(stderr, "svg load: No / bad symbol\n");
				ps_error = 1;
				return;
			}
			sym = ps_sym_lookup(s + 1);
			if (!sym) {
				e = elt_new();
				if (!e)
					return;
				e->type = STR;
				e->u.s = strdup(s);
				e->u.s[0] = ' ';	/* internal */
			} else {
				e = elt_dup(sym->e);
				if (!e)
					return;
			}
			free(s);
			push(e);
			return;
		}
		if (strcmp(op, "longa") == 0) {
			setxysym(op, D_longa);
			return;
		}
		if (strcmp(op, "lphr") == 0) {
			xysym(op, D_lphr);
			return;
		}
		if (strcmp(op, "ltr") == 0) {
			arp_ltr('l');
			return;
		}
		if (strcmp(op, "lyshow") == 0) {
			show('s');
			return;
		}
		break;
	case 'M':
		if (strcmp(op, "M") == 0) {
moveto:
			cy = pop_free_val();
			cx = pop_free_val();
			if (path) {
				path_print("\tM%.2f %.2f\n", xoffs + cx, yoffs - cy);
			} else if (g == 2) {
				fputs("</text>\n", fout);
				g = 1;
			}
			return;
		}
		break;
	case 'm':
		if (strcmp(op, "moveto") == 0)
			goto moveto;
		if (strcmp(op, "mphr") == 0) {
			xysym(op, D_mphr);
			return;
		}
		if (strcmp(op, "mod") == 0) {
			x = pop_free_val();
			if (!stack || stack->type != VAL || x == 0) {
				fprintf(stderr, "svg: Bad value for mod\n");
				ps_error = 1;
				return;
			}
			n = (int) stack->u.v % (int) x;
			stack->u.v = n;
			return;
		}
		if (strcmp(op, "mrep") == 0) {
			xysym(op, D_mrep);
			return;
		}
		if (strcmp(op, "mrep2") == 0) {
			xysym(op, D_mrep2);
			return;
		}
		if (strcmp(op, "mrest") == 0) {
			def_use(D_mrest);
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val();
			s = pop_free_str();
			if (!s) {
				fprintf(stderr, "svg: No string\n");
				ps_error = 1;
				return;
			}
			fprintf(fout, "<use x=\"%.2f\" y=\"%.2f\" xlink:href=\"#mrest\"/>\n"
				"<text font-family=\"Times\" font-size=\"15\" font-weight=\"bold\" font-style=\"normal\"\n"
				"	x=\"%.2f\" y=\"%.2f\" text-anchor=\"middle\">%s</text>\n",
				x, y, x, y - 28, s + 1);
			free(s);
			return;
		}
		if (strcmp(op, "mul") == 0) {
			x = pop_free_val();
			if (!stack || stack->type != VAL) {
				fprintf(stderr, "svg: Bad value for mul\n");
				ps_error = 1;
				return;
			}
			stack->u.v *= x;
			return;
		}
		break;
	case 'n':
		if (strcmp(op, "ne") == 0) {
			cond(C_NE);
			return;
		}
		if (strcmp(op, "neg") == 0) {
			if (!stack || stack->type != VAL) {
				fprintf(stderr, "svg: Bad value for neg\n");
				ps_error = 1;
				return;
			}
			stack->u.v = -stack->u.v;
			return;
		}
		if (strcmp(op, "newpath") == 0) {
			path_def();
			return;
		}
		if (strcmp(op, "nt0") == 0) {
			xysym(op, D_nt0);
			return;
		}
		break;
	case 'o':
		if (strcmp(op, "octl") == 0
		 || strcmp(op, "octu") == 0) {
			setg(1);
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val();
			if (op[3] == 'l')
				x -= 3.5;
			else
				x -= 2.5;
			fprintf(fout, "<text font-family=\"Times\" font-size=\"12\" font-weight=\"normal\" font-style=\"normal\"\n"
				"	x=\"%.2f\" y=\"%.2f\">8</text>\n",
				x, y);
			return;
		}
		if (strcmp(op, "opend") == 0) {
			xysym(op, D_opend);
			return;
		}
		if (strcmp(op, "or") == 0) {
			x = pop_free_val();
			if (!stack || stack->type != VAL) {
				fprintf(stderr, "svg or: Bad value\n");
				ps_error = 1;
				return;
			}
			stack->u.v = (int) x & (int) stack->u.v;
			return;
		}
		break;
	case 'p':
		if (strcmp(op, "pclef") == 0) {
			def_use(D_pclef);
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val();
			fprintf(fout, "<use x=\"%.2f\" y=\"%.2f\" xlink:href=\"#%s\"/>\n",
				x, y, op);
			return;
		}
		if (strcmp(op, "pf") == 0) {
			setg(1);
			y = yoffs - pop_free_val() - 5;
			x = xoffs + pop_free_val();
			s = pop_free_str();
			if (!s) {
				fprintf(stderr, "svg pf: No string\n");
				ps_error = 1;
				return;
			}
			fprintf(fout, "<text font-family=\"Times\" font-size=\"16\" font-weight=\"bold\" font-style=\"italic\"\n"
				"	x=\"%.2f\" y=\"%.2f\">%s</text>\n",
				x, y, s + 1);
			free(s);
			return;
		}
		if (strcmp(op, "pmsig") == 0) {
			xysym(op, D_pmsig);
			return;
		}
		if (strcmp(op, "pMsig") == 0) {
			xysym(op, D_pMsig);
			return;
		}
		if (strcmp(op, "pop") == 0) {
			if (!stack) {
				fprintf(stderr, "svg pop: Stack empty\n");
				ps_error = 1;
				return;
			}
			e = pop(stack->type);
			elt_free(e);
			return;
		}
		if (strcmp(op, "pshhd") == 0) {
			setxysym(op, D_pshhd);
			return;
		}
		if (strcmp(op, "pdshhd") == 0) {
			setxysym("pshhd", D_pshhd);
			return;
		}
		if (strcmp(op, "pfthd") == 0) {
			setxysym(op, D_pfthd);
			return;
		}
		if (strcmp(op, "pdfthd") == 0) {
			setxysym("pfthd", D_pfthd);
			return;
		}
#if 0
//fixme: cannot work because duplication...
		if (strcmp(op, "put") == 0) {
			int v;

			v = pop_free_val();
			n = pop_free_val();
			if (!stack) {
				fprintf(stderr, "svg put: Stack empty\n");
				ps_error = 1;
				return;
			}
			s = pop_free_str();
			if (!s || *s != '(') {
				fprintf(stderr, "svg put: No string\n");
				ps_error = 1;
				return;
			}
			if ((unsigned) n >= strlen(s) - 1) {
				fprintf(stderr, "svg put: Out of bounds\n");
				ps_error = 1;
				return;
			}
//fixme: should keep the original string...
			s[n + 1] = v;
			free(s);
			return;
		}
#endif
		break;
	case 'R':
		if (strcmp(op, "RC") == 0) {
			float c1, c2, c3, c4;

rcurveto:
			path_def();
			y = pop_free_val();
			x = pop_free_val();
			c4 = pop_free_val();
			c3 = pop_free_val();
			c2 = pop_free_val();
			c1 = pop_free_val();
			path_print("\tc%.2f %.2f %.2f %.2f %.2f %.2f\n",
				c1, -c2, c3, -c4, x, -y);
			cx += x;
			cy += y;
			return;
		}
		if (strcmp(op, "RL") == 0) {
rlineto:
			path_def();
			y = pop_free_val();
			x = pop_free_val();
			if (x == 0)
				path_print("\tv%.2f\n", -y);
			else if (y == 0)
				path_print("\th%.2f\n", x);
			else
				path_print("\tl%.2f %.2f\n", x, -y);
			cx += x;
			cy += y;
			return;
		}
		if (strcmp(op, "RM") == 0) {
rmoveto:
			y = pop_free_val();
			x = pop_free_val();
			if (path) {
				path_print("\tm%.2f %.2f\n", x, -y);
			} else if (g == 2) {
				fputs("</text>\n", fout);
				g = 1;
			}
			cx += x;
			cy += y;
			return;
		}
		break;
	case 'r':
		if (strcmp(op, "r00") == 0) {
			setxysym(op, D_r00);
			return;
		}
		if (strcmp(op, "r0") == 0) {
			setxysym(op, D_r0);
			return;
		}
		if (strcmp(op, "r1") == 0) {
			setxysym(op, D_r1);
			return;
		}
		if (strcmp(op, "r2") == 0) {
			setxysym(op, D_r2);
			return;
		}
		if (strcmp(op, "r4") == 0) {
			setxysym(op, D_r4);
			return;
		}
		if (strcmp(op, "r8") == 0) {
			setxysym(op, D_r8);
			return;
		}
		if (strcmp(op, "r16") == 0) {
			setxysym(op, D_r16);
			return;
		}
		if (strcmp(op, "r32") == 0) {
			setxysym(op, D_r32);
			return;
		}
		if (strcmp(op, "r64") == 0) {
			setxysym(op, D_r64);
			return;
		}
		if (strcmp(op, "r128") == 0) {
			setxysym(op, D_r128);
			return;
		}
		if (strcmp(op, "rdots") == 0) {
			xysym(op, D_rdots);
			return;
		}
		if (strcmp(op, "rcurveto") == 0)
			goto rcurveto;
		if (strcmp(op, "rlineto") == 0)
			goto rlineto;
		if (strcmp(op, "rmoveto") == 0)
			goto rmoveto;
		if (strcmp(op, "roll") == 0) {
			int i, j;

			j = pop_free_val();
			n = pop_free_val();
			if (n <= 0) {
				fprintf(stderr, "svg roll: Invalid value\n");
				ps_error = 1;
				return;
			}
			if (j > 0) {
				j = j % n;
				if (j > n / 2)
					j -= n;
			} else if (j < 0) {
				j = -(-j % n);
				if (j < -n / 2)
					j += n;
			}
			if (j == 0)
				return;
			e2 = stack;		/* check the stack */
			i = n;
			for (;;) {
				if (!e2) {
					fprintf(stderr, "svg roll: Stack empty\n");
					ps_error = 1;
					return;
				}
				if (--i <= 0)
					break;
				e2 = e2->next;
			}
			if (j > 0) {
				while (j-- > 0) {
					e = stack;
					stack = e->next;
					e->next = e2->next;
					e2->next = e;
					e2 = e;
				}
				return;
			}
			while (j++ < 0) {
				e = stack;
				for (i = 0; i < n - 2; i++)
					e = e->next;
				e2 = e->next;
				e->next = e2->next;
				e2->next = stack;
				stack = e2;
			}
			return;
		}
		if (strcmp(op, "repbra") == 0) {
			int i;

			setg(1);
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val();
			w = pop_free_val();
			i = pop_free_val();
			h = pop_free_val();
			s = pop_free_str();
			if (!s) {
				fprintf(stderr, "svg repbra: No string\n");
				ps_error = 1;
				return;
			}
			fprintf(fout,
				"<text x=\"%.2f\" y=\"%.2f\">",
				x + 4, y - h);
			xml_str_out(s + 1);
			fprintf(fout,
				"</text>\n"
				"<path stroke=\"currentColor\" fill=\"none\"\n"
				"	d=\"M%.2f %.2f",
				x, y);
			if (i != 1)
				fprintf(fout, "v20M%.2f %.2f", x, y);
			fprintf(fout, "h%.2f", w);
			if (i != 0)
				fprintf(fout, "v20");
			fprintf(fout, "\"/>\n");
			free(s);
			return;
		}
		if (strcmp(op, "repeat") == 0) {
			e = pop(SEQ);		/* sequence */
			if (!e)
				return;
			n = pop_free_val();	/* n times */
			if ((unsigned) n >= 100) {
				fprintf(stderr, "svg repeat: Too high value\n");
				ps_error = 1;
			}
			while (--n >= 0) {
				if (seq_exec(e))
					break;		/* exit */
				if (ps_error)
					break;
			}
			elt_free(e);
			return;
		}
		if (strcmp(op, "rotate") == 0) {
			setg(0);
#if 1
			h = 360 - pop_free_val();
			gcur.rotate += h;
#else
			h = pop_free_val();
			gcur.rotate -= h;
#endif
			h = h * M_PI / 180;
			x = cx;
			cx = x * cos(h) + cy * sin(h);
			cy = -x * sin(h) + cy * cos(h);
			return;
		}
		break;
	case 'S':
		if (strcmp(op, "SL") == 0) {
			float c1, c2, c3, c4, c5, c6, l1, l2;
			float a1, a2, a3, a4, a5, a6, m1, m2;

			setg(1);
			m2 = yoffs - pop_free_val();
			m1 = xoffs + pop_free_val();
			a6 = pop_free_val();
			a5 = pop_free_val();
			a4 = pop_free_val();
			a3 = pop_free_val();
			a2 = pop_free_val();
			a1 = pop_free_val();
			l2 = pop_free_val();
			l1 = pop_free_val();
			c6 = pop_free_val();
			c5 = pop_free_val();
			c4 = pop_free_val();
			c3 = pop_free_val();
			c2 = pop_free_val();
			c1 = pop_free_val();
			fprintf(fout,
				"<path fill=\"currentColor\"\n"
				"	d=\"M%.2f %.2fc%.2f %.2f %.2f %.2f %.2f %.2f\n"
				"	l%.2f %.2fc%.2f %.2f %.2f %.2f %.2f %.2f\"/>\n",
				m1, m2, a1, -a2, a3, -a4, a5, -a6,
				l1, -l2, c1, -c2, c3, -c4, c5, -c6);
			return;
		}
		if (strcmp(op, "SLW") == 0) {
			gcur.linewidth = pop_free_val();
			return;
		}
		break;
	case 's':
		if (strcmp(op, "scale") == 0) {
			y = pop_free_val();
			x = pop_free_val();
			xoffs /= x;
			yoffs /= y;
			cx /= x;
			cy /= y;
			gcur.xscale *= x;
			gcur.yscale *= y;
			return;
		}
		if (strcmp(op, "scalefont") == 0) {
			w = pop_free_val();
			gcur.font_s = w;
			return;
		}
		if (strcmp(op, "selectfont") == 0) {
			w = pop_free_val();
			s = pop_free_str();
			if (!s
			 || *s != '/'
			 || strlen(s) >= sizeof gcur.font_n) {
				fprintf(stderr, "svg selectfont: No / bad font\n");
				ps_error = 1;
				return;
			}
			strcpy(gcur.font_n, s + 1);
			gcur.font_s = w;
			free(s);
			return;
		}
		if (strcmp(op, "sep0") == 0) {
			x = pop_free_val();
			w = pop_free_val();
			fprintf(fout,
				"<path stroke=\"currentColor\" fill=\"none\"\n"
				"	d=\"M%.2f %.2fh%.2f\"/>\n",
					xoffs + x, yoffs, w);
			return;
		}
		if (strcmp(op, "setdash") == 0) {
			char *p;

			n = pop_free_val();
			e = pop(BRK);
			if (!e) {
				fprintf(stderr, "svg setdash: Bad pattern\n");
				ps_error = 1;
				return;
			}
			e = e->u.e;
			if (!e) {
				gcur.dash[0] = '\0';
				return;
			}
			p = gcur.dash;
			if (n != 0)
				p += sprintf(p, " stroke-dashoffset=\"%d\"", n);
			p += sprintf(p, " stroke-dasharray=\"");
			do {
				if (e->type != VAL) {
					fprintf(stderr, "svg setdash: Bad pattern type\n");
					ps_error = 1;
					return;
				}
				if (p >= &gcur.dash[sizeof gcur.dash] - 10) {
					fprintf(stderr, "svg setdash: Pattern too wide\n");
					ps_error = 1;
					return;
				}
				p += sprintf(p, "%d,", (int) e->u.v);
				e = e->next;
			} while (e != 0);
			p--;
			sprintf(p, "\"");
			return;
		}
		if (strcmp(op, "setfont") == 0) {
			return;
		}
		if (strcmp(op, "setgray") == 0) {
			gcur.rgb = pop_free_val() * 0xffffff;
			return;
		}
		if (strcmp(op, "setlinewidth") == 0) {
			gcur.linewidth = pop_free_val();
			return;
		}
//fixme: use 'use' for flags
		if (strcmp(op, "sfu") == 0) {
			setg(1);
			h = pop_free_val();
			n = pop_free_val();
			sym = ps_sym_lookup("x");
			x = xoffs + sym->e->u.v + 3.5;
			sym = ps_sym_lookup("y");
			y = yoffs - sym->e->u.v;
			fprintf(fout,
				"<path d=\"M%.2f %.2fv%.2f\" stroke=\"currentColor\" fill=\"none\"/>\n"
				"<path fill=\"currentColor\"\n"
				"	d=\"",
				x, y, -h);
			if (n == 1) {
				fprintf(fout,
					"	M%.2f %.2fc0.6 5.6 9.6 9 5.6 18.4\n"
					"	c1.6 -6 -1.3 -11.6 -5.6 -12.8\n",
					x, y - h);
			} else {
				y -= h;
				while (--n >= 0) {
					fprintf(fout,
						"M%.2f %.2fc0.9 3.7 9.1 6.4 6 12.4\n"
						"	c1 -5.4 -4.2 -8.4 -6 -8.4\n",
						x, y);
					y += 5.4;
				}
			}
			fprintf(fout, "\"/>\n");
			return;
		}
		if (strcmp(op, "sfd") == 0) {
			setg(1);
			h = pop_free_val();
			n = pop_free_val();
			sym = ps_sym_lookup("x");
			x = xoffs + sym->e->u.v - 3.5;
			sym = ps_sym_lookup("y");
			y = yoffs - sym->e->u.v;
			fprintf(fout,
				"<path d=\"M%.2f %.2fv%.2f\" stroke=\"currentColor\" fill=\"none\"/>\n"
				"<path fill=\"currentColor\"\n"
				"	d=\"",
				x, y, -h);
			if (n == 1) {
				fprintf(fout,
					"M%.2f %.2fc0.6 -5.6 9.6 -9 5.6 -18.4\n"
					"	c1.6 6 -1.3 11.6 -5.6 12.8\n",
					x, y - h);
			} else {
				y -= h;
				while (--n >= 0) {
					fprintf(fout,
					"M%.2f %.2fc0.9 -3.7 9.1 -6.4 6 -12.4\n"
					"	c1 5.4 -4.2 8.4 -6 8.4\n",
					x, y);
					y -= 5.4;
				}
			}
			fprintf(fout, "\"/>\n");
			return;
		}
		if (strcmp(op, "sfs") == 0) {
			setg(1);
			h = pop_free_val();
			n = pop_free_val();
			sym = ps_sym_lookup("x");
			x = xoffs + sym->e->u.v;
			sym = ps_sym_lookup("y");
			y = yoffs - sym->e->u.v - 1;
			if (h > 0) {
				x += 3.5;
				y -= 1;
				fprintf(fout,
					"<path d=\"M%.2f %.2fv%.2f\" stroke=\"currentColor\" fill=\"none\"/>\n"
					"<path fill=\"currentColor\"\n"
					"	d=\"",
					x, y, -h + 1);
				y -= h - 1;
				while (--n >= 0) {
					fprintf(fout,
						"M%.2f %.2fl7 3.2 0 3.2 -7 -3.2z\n",
						x, y);
					y += 5.4;
				}
			} else {
				x -= 3.5;
				y += 1;
				fprintf(fout,
					"<path d=\"M%.2f %.2fv%.2f\" stroke=\"currentColor\" fill=\"none\"/>\n"
					"<path fill=\"currentColor\"\n"
					"	d=\"",
					x, y, -h - 1);
				y -= h + 1;
				while (--n >= 0) {
					fprintf(fout,
						"M%.2f %.2fl7 -3.2 0 -3.2 -7 3.2z\n",
						x, y);
					y -= 5.4;
				}
			}
			fprintf(fout, "\"/>\n");
			return;
		}
		if (strcmp(op, "sgu") == 0) {
			setg(1);
			h = pop_free_val();
			n = pop_free_val();
			sym = ps_sym_lookup("x");
			x = xoffs + sym->e->u.v + 1.6;
			sym = ps_sym_lookup("y");
			y = yoffs - sym->e->u.v;
			fprintf(fout,
				"<path d=\"M%.2f %.2fv%.2f\" stroke=\"currentColor\" fill=\"none\"/>\n"
				"<path fill=\"currentColor\"\n"
				"	d=\"",
				x, y, -h);
			if (n == 1) {
				fprintf(fout,
					"M%.2f %.2fc0.6 3.4 5.6 3.8 3 10\n"
					"	c1.2 -4.4 -1.4 -7 -3 -7\n",
					x, y - h);
			} else {
				y -= h;
				while (--n >= 0) {
					fprintf(fout,
						"M%.2f %.2fc1 3.2 5.6 2.8 3.2 8\n"
						"	c1.4 -4.8 -2.4 -5.4 -3.2 -5.2\n",
					x, y);
					y += 3.5;
				}
			}
			fprintf(fout, "\"/>\n");
			return;
		}
		if (strcmp(op, "sgd") == 0) {
			setg(1);
			h = pop_free_val();
			n = pop_free_val();
			sym = ps_sym_lookup("x");
			x = xoffs + sym->e->u.v - 1.6;
			sym = ps_sym_lookup("y");
			y = yoffs - sym->e->u.v;
			fprintf(fout,
				"<path d=\"M%.2f %.2fv%.2f\" stroke=\"currentColor\" fill=\"none\"/>\n"
				"<path fill=\"currentColor\"\n"
				"	d=\"",
				x, y, -h);
			if (n == 1) {
				fprintf(fout,
					"M%.2f %.2fc0.6 -3.4 5.6 -3.8 3 -10\n"
					"	c1.2 4.4 -1.4 7 -3 7\n",
					x, y - h);
			} else {
				y -= h;
				while (--n >= 0) {
					fprintf(fout,
						"M%.2f %.2fc1 -3.2 5.6 -2.8 3.2 -8\n"
						"	c1.4 4.8 -2.4 5.4 -3.2 5.2\n",
						x, y);
						y -= 3.5;
				}
			}
			fprintf(fout, "\"/>\n");
			return;
		}
		if (strcmp(op, "sgs") == 0) {
			setg(1);
			h = pop_free_val();
			n = pop_free_val();
			sym = ps_sym_lookup("x");
			x = xoffs + sym->e->u.v + 1.6;
			sym = ps_sym_lookup("y");
			y = yoffs - sym->e->u.v;
			fprintf(fout,
				"<path d=\"M%.2f %.2fv%.2f\" stroke=\"currentColor\" fill=\"none\"/>\n"
				"<path fill=\"currentColor\"\n"
				"	d=\"",
				x, y, -h);
			y -= h;
			while (--n >= 0) {
				fprintf(fout,
					"M%.2f %.2fl3 1.5 0 2 -3 -1.5z\n",
					x, y);
				y += 3;
			}
			fprintf(fout, "\"/>\n");
			return;
		}
		if (strcmp(op, "sfz") == 0) {
			setg(1);
			y = yoffs - pop_free_val() - 5;
			x = xoffs + pop_free_val() - 7;
			s = pop_free_str();
			if (s != 0)
				free(s);
			fprintf(fout, "<text font-family=\"Times\" font-size=\"14\" font-style=\"italic\" font-weight=\"normal\"\n"
				"	x=\"%.2f\" y=\"%.2f\">s<tspan\n"
				"	font-size=\"16\" font-weight=\"bold\">f</tspan>z</text>\n",
				x, y);
			return;
		}
		if (strcmp(op, "sgno") == 0) {
			xysym(op, D_sgno);
			return;
		}
		if (strcmp(op, "show") == 0) {
			show('s');
			return;
		}
		if (strcmp(op, "showb") == 0) {
			show('b');
			return;
		}
		if (strcmp(op, "showc") == 0) {
			show('c');
			return;
		}
		if (strcmp(op, "showr") == 0) {
			show('r');
			return;
		}
		if (strcmp(op, "showerror") == 0) {
			def_use(D_showerror);
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val();
			fprintf(fout, "<use x=\"%.2f\" y=\"%.2f\" xlink:href=\"#%s\"/>\n",
				x, y, op);
			return;
		}
		if (strcmp(op, "sld") == 0) {
			xysym(op, D_sld);
			return;
		}
		if (strcmp(op, "snap") == 0) {
			xysym(op, D_snap);
			return;
		}
		if (strcmp(op, "sphr") == 0) {
			xysym(op, D_sphr);
			return;
		}
		if (strcmp(op, "spclef") == 0) {
			def_use(D_pclef);
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val();
			fprintf(fout, "<use x=\"%.2f\" y=\"%.2f\" xlink:href=\"#pclef\"/>\n",
				x, y);
			return;
		}
		if (strcmp(op, "setrgbcolor") == 0) {
			int r, g, b;

			b = pop_free_val() * 255;
			g = pop_free_val() * 255;
			r = pop_free_val() * 255;
			gcur.rgb = (r << 16) | (g << 8) | b;
			return;
		}
		if (strcmp(op, "staff") == 0) {
			gcur.linewidth = DLW;
			setg(1);
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val();
			n = pop_free_val();
			w = pop_free_val();
			fprintf(fout,
				"<path stroke=\"currentColor\" fill=\"none\"\n"
				"	d=\"M%.2f %.2f", x, y);
			for (;;) {
				fprintf(fout, "h%.2f", w);
				if (--n <= 0)
					break;
				fprintf(fout, "m%.2f -6", -w);
			}
			fprintf(fout, "\"/>\n");
			return;
		}
		if (strcmp(op, "stc") == 0) {
			xysym(op, D_stc);
			return;
		}
		if (strcmp(op, "stroke") == 0) {
			if (!path) {
				fprintf(stderr, "svg: 'stroke' with no path\n");
//				ps_error = 1;
				return;
			}
			path_end();
			fprintf(fout, "\t\" stroke=\"currentColor\" fill=\"none\"%s/>\n",
					gcur.dash);
			return;
		}
		if (strcmp(op, "su") == 0
		 || strcmp(op, "sd") == 0) {
			stem(op);
			return;
		}
		if (strcmp(op, "stsig") == 0) {
			setg(1);
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val();
			s = pop_free_str();
			if (!s) {
				fprintf(stderr, "svg: No string\n");
				ps_error = 1;
				return;
			}
			fprintf(fout, "<g font-family=\"Times\" font-size=\"18\" font-weight=\"bold\" font-style=\"normal\"\n"
				"	transform=\"translate(%.2f,%.2f) scale(1.2,1)\">\n"
				"	<text x=\"0\" y=\"-7\" text-anchor=\"middle\">%s</text>\n"
				"</g>\n",
				x, y, s + 1);
			free(s);
			return;
		}
		if (strcmp(op, "sub") == 0) {
			x = pop_free_val();
			if (!stack || stack->type != VAL) {
				fprintf(stderr, "svg: Bad value for sub\n");
				ps_error = 1;
				return;
			}
			stack->u.v -= x;
			return;
		}
		if (strcmp(op, "sbclef") == 0) {
			xysym(op, D_sbclef);
			return;
		}
		if (strcmp(op, "scclef") == 0) {
			xysym(op, D_scclef);
			return;
		}
		if (strcmp(op, "sh0") == 0) {
			xysym(op, D_sh0);
			return;
		}
		if (strcmp(op, "sh1") == 0) {
			xysym(op, D_sh1);
			return;
		}
		if (strcmp(op, "sh513") == 0) {
			xysym(op, D_sh513);
			return;
		}
		if (strcmp(op, "srep") == 0) {
			xysym(op, D_srep);
			return;
		}
		if (strcmp(op, "stclef") == 0) {
			xysym(op, D_stclef);
			return;
		}
		if (strcmp(op, "stringwidth") == 0) {
			s = pop_free_str();
			if (!s || *s != '(') {
				fprintf(stderr, "svg stringwidth: No string\n");
				ps_error = 1;
				return;
			}
			e = elt_new();
			if (!e)
				return;
			e->type = VAL;
			e->u.v = strw(s + 1);
			push(e);
			e = elt_new();
			if (!e)
				return;
			e->type = VAL;
			e->u.v = gcur.font_s;
			push(e);
			return;
		}
		if (strcmp(op, "svg") == 0) {
			e = elt_new();
			if (!e)
				return;
			e->type = VAL;
			e->u.v = 1;
			push(e);
			return;
		}
		break;
	case 'T':
		if (strcmp(op, "T") == 0) {
translate:
//fixme:test
//			setg(1);
			y = pop_free_val();
			x = pop_free_val();
			xoffs += x;
			yoffs -= y;
			cx -= x;
			cy -= y;
//fprintf(stderr, "T %.2f %.2f -> %.2f %.2f\n", x, y, xoffs, yoffs);
			return;
		}
		break;
	case 't':
		if (strcmp(op, "tclef") == 0) {
			xysym(op, D_tclef);
			return;
		}
		if (strcmp(op, "thbar") == 0) {
			setg(1);
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val() + 1.5;
			h = pop_free_val();
			fprintf(fout,
				"<path stroke=\"currentColor\" fill=\"none\" stroke-width=\"3\"\n"
				"	d=\"M%.2f %.2fv%.2f\"/>\n",
				x, y, -h);
			return;
		}
		if (strcmp(op, "thumb") == 0) {
			xysym(op, D_thumb);
			return;
		}
		if (strcmp(op, "translate") == 0)
			goto translate;
		if (strcmp(op, "trem") == 0) {
			setg(1);
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val() - 4.5;
			n = pop_free_val();
			fprintf(fout, "<path fill=\"currentColor\" d=\"m%.2f %.2f\n\t",
				x, y);
			for (;;) {
				fputs("l9 -3v3l-9 3z", fout);
				if (--n <= 0)
					break;
				fputs("m0 5.4", fout);
			}
			fputs("\"/>", fout);
			return;
		}
		if (strcmp(op, "trl") == 0) {
			setg(1);
			y = yoffs - pop_free_val() - 2;
			x = xoffs + pop_free_val() - 4;
			fprintf(fout, "<text font-family=\"Times\" font-size=\"16\" font-weight=\"bold\" font-style=\"italic\"\n"
				"	x=\"%.2f\" y=\"%.2f\">tr</text>\n",
				x, y);
			return;
		}
		if (strcmp(op, "true") == 0) {
			e = elt_new();
			if (!e)
				return;
			e->type = VAL;
			e->u.v = 1;
			push(e);
			return;
		}
		if (strcmp(op, "tsig") == 0) {
			char *d;

			setg(1);
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val();
			d = pop_free_str();
			s = pop_free_str();
			if (!d || !s) {
				fprintf(stderr, "svg: No string\n");
				ps_error = 1;
				return;
			}
			fprintf(fout, "<g font-family=\"Times\" font-size=\"16\" font-weight=\"bold\" font-style=\"normal\"\n"
				"	transform=\"translate(%.2f,%.2f) scale(1.2,1)\">\n"
				"	<text y=\"-1\" text-anchor=\"middle\">%s</text>\n"
				"	<text y=\"-13\" text-anchor=\"middle\">%s</text>\n"
				"</g>\n",
				x, y, d + 1, s + 1);
			free(d);
			free(s);
			return;
		}
		if (strcmp(op, "tubr") == 0
		 || strcmp(op, "tubrl") == 0) {
			float dx, dy;
			int h;

			setg(1);
			y = yoffs - pop_free_val();
			x = xoffs + pop_free_val();
			dy = pop_free_val();
			dx = pop_free_val();
			if (op[4] == 'l') {
				h = 3;
				y -= 3;
			} else {
				h = -3;
				y += 3;
			}
			fprintf(fout,
				"<path stroke=\"currentColor\" fill=\"none\"\n"
				"	d=\"M%.2f %.2fv%dl%.2f %.2fv%d\"/>\n",
				x, y, h, dx, -dy, -h);
			return;
		}
		if (strcmp(op, "turn") == 0) {
			xysym(op, D_turn);
			return;
		}
		if (strcmp(op, "turnx") == 0) {
			xysym(op, D_turnx);
			return;
		}
		break;
	case 'u':
		if (strcmp(op, "upb") == 0) {
			xysym(op, D_upb);
			return;
		}
		if (strcmp(op, "umrd") == 0) {
			xysym(op, D_umrd);
			return;
		}
		break;
	case 'w':
		if (strcmp(op, "wedge") == 0) {
			xysym(op, D_wedge);
			return;
		}
		if (strcmp(op, "wln") == 0) {
			setg(1);
			y = pop_free_val();
			x = pop_free_val();
			w = pop_free_val();
			fprintf(fout, "<path stroke=\"currentColor\" fill=\"none\" stroke-width=\"0.8\"\n"
				"	d=\"M%.2f %.2fh%.2f\"/>\n",
				xoffs + x, yoffs - y, w);
			return;
		}
		if (strcmp(op, "where") == 0) {
			s = pop_free_str();		/* symbol */
			if (!s || *s != '/') {
				fprintf(stderr, "svg where: No / bad symbol\n");
				ps_error = 1;
				return;
			}
			e = elt_new();
			if (!e)
				return;
			e->type = VAL;
			sym = ps_sym_lookup(&s[1]);
			if (!sym) {
				e->u.v = 0;
			} else {
				e->u.v = 1;
				e2 = elt_new();		/* dictionnary */
				if (!e2)
					return;
				e2->type = VAL;
				e2->u.v = 0;
				push(e2);
			}
			free(s);
			push(e);
			return;
		}
		break;
	case 'x':
		if (strcmp(op, "xymove") == 0) {
			cy = y = pop_free_val();
			cx = x = pop_free_val();
			setxory("x", x);
			setxory("y", y);
			return;
		}
		break;
	}
	fprintf(stderr, "svg: Symbol '%s' not defined\n", op);
	ps_error = 1;
}

void svg_write(char *buf, int len)
{
	int l;
	struct elt_s *e, *e2;
	unsigned char c, *p, *q, *r;

	if (ps_error)
		return;

	p = (unsigned char *) buf;
#if 0
	if (strncmp((char *) p, "%svg ", 5) == 0) {	/* %%beginsvg */
		fwrite(p + 5, 1, len - 5, fout);
		fputs("\n", fout);
		return;
	}
#endif

	/* scan the string */
	while (--len >= 0) {
		c = *p++;
		switch (c) {
		case ' ':
		case '\t':
		case '\n':
			continue;
		case '{':
		case '[':		/* treat '[' as '{' */
			e = elt_new();
			if (!e)
				return;
			in_cnt++;
			e->type = STR;
			e->u.s = strdup(c == '{' ? "{" : "[");
			push(e);
			break;
		case '}':
		case ']':
			in_cnt--;
			if (in_cnt < 0) {
				if (c == '}')
					fprintf(stderr, "svg: '}' without '{'\n");
				else
					fprintf(stderr, "svg: ']' without '['\n");
				ps_error = 1;
				return;
			}
			e = elt_new();
			if (!e)
				return;

			/* create a container with elements in direct order */
			e->u.e = 0;
			if (c == '}') {
				e->type = SEQ;
				c = '{';
			} else {
				e->type = BRK;
				c = '[';
			}
			for (;;) {
				e2 = stack;
				stack = stack->next;
				if (e2->type == STR
				 && (e2->u.s[0] == '['
				  || e2->u.s[0] == '{'))
					break;
				e2->next = e->u.e;
				e->u.e = e2;
			}
			if (e2->u.s[0] != c) {
				fprintf(stderr, "svg: '%c' found before '%c'\n",
					e2->u.s[0], c);
				ps_error = 1;
				return;
			}
			elt_free(e2);
			push(e);
			break;
		case '%':
			q = p;
			while (--len >= 0) {
				c = *p++;
				if (c == '\n')
					break;
			}
			if ((char *) q != &buf[1] && q[-2] != '\n')
				break;
			if (strncmp((char *) q, "A ", 2) == 0) {	/* annotation */
				char type;
				int row , col, h;
				float x, y, w;

				q += 2;
				type = *q++;
				if (type != 'b' && type != 'e') {	/* if not beam */
					sscanf((char *) q + 1, "%d %d %f %f %f %d",
						&row, &col, &x, &y, &w, &h);
				} else {
					sscanf((char *) q + 1, "%d %d %f %f",
						&row, &col, &x, &y);
					w = h = 6;
				}
					fprintf(fout, "<abc type=\"%c\" row=\"%d\" col=\"%d\" x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%d\"/>\n",
						type, row, col, xoffs + x, yoffs - y - h, w, h);
				break;
			}
			if (strncmp((char *) q, " --- ", 5) == 0	/* title info */
			 && strncmp((char *) p - 6, ") ---", 5) == 0) {
				setg(1);
				if (q[5] == '+') {		/* subtitle */
					q += 8;		/* % --- + (%s) ... */
					r = (unsigned char *) strchr((char *) q, ')');
//					if (!r)
//						break;
					fprintf(fout, "<!-- subtitle: %.*s -->\n",
							r - q, q);
					break;
				}
				q = (unsigned char *) strchr((char *) q, '(');
//				if (!q)
//					break;
				q++;
				r = (unsigned char *) strchr((char *) q, ')');
//				if (!r)
//					break;
				fprintf(fout, "<!-- title: %.*s -->\n",
						r - q, q);
				break;
			}
			break;
		case '(':
			q = p - 1;
			l = 1;
			for (;;) {
				switch (*p++) {
				case '\\':
					p++;
					l--;
					continue;
				default:
					continue;
				case ')':
					break;
				}
				break;
			}
			len -= p - q - 1;
			l += p - q - 1;
			p = q;
			e = elt_new();
			if (!e)
				return;
			e->type = STR;
			r = malloc(l);
			e->u.s = (char *) r;
			for (;;) {
				c = *p++;
				switch (c) {
				case '\\':
					*r++ = *p++;
					continue;
				default:
					*r++ = c;
					continue;
				case ')':
					break;
				}
				break;
			}
			*r = '\0';
			push(e);
			break;
		default:
			q = p - 1;
			while (--len >= 0) {
				c = *p++;
				switch (c) {
				case '(':
				case ' ':
				case '\t':
				case '\n':
				case '{':
				case '}':
				case '[':
				case ']':
				case '%':
				case '/':
					break;
				default:
					continue;
				}
				break;
			}
			if (len >= 0) {
				p--;
				len++;
			}
			if (isdigit((unsigned) *q) || *q == '-' || *q == '.') {
				int i;
				float v;

				e = elt_new();
				if (!e)
					return;
				e->type = VAL;
				c = *p;
				*p = '\0';
				if (q[1] == '#') {
					i = strtol((char *) q + 2, 0, 8);
					e->u.v = i;
				} else if (q[2] == '#') {
					i = strtol((char *) q + 3, 0, 16);
					e->u.v = i;
				} else {
					if (sscanf((char *) q, "%f", &v) != 1) {
						fprintf(stderr, "svg: Bad numeric value in '%s'",
							buf);
						v = 0;
					}
					e->u.v = v;
				}
				*p = c;
			} else {
				if (!in_cnt) {
					if (*q != '/') {	/* operator */
						c = *p;
						*p = '\0';
						ps_exec((char *) q);
						if (ps_error)
							return;
						*p = c;
						break;
					}
				} else if (strncmp((char *) q, "pdfmark", 7) == 0) {
					in_cnt--;
					for (;;) {
						e = pop(stack->type);
						if (e->type == STR
						 && (e->u.s[0] == '['
						  || e->u.s[0] == '{'))
							break;
						elt_free(e);
					}
					elt_free(e);
					break;
				}
				l = p - q;
				r = malloc(l + 1);
				memcpy(r, q, l);
				r[l] = '\0';
				e = elt_new();
				if (!e)
					return;
				e->type = STR;
				e->u.s = (char *) r;
			}
			push(e);
			break;
		}
	}
}

int svg_output(FILE *out, const char *fmt, ...)
{
	va_list args;
	char tmp[128];

	va_start(args, fmt);
	vsnprintf(tmp, sizeof tmp, fmt, args);
	va_end(args);
	svg_write(tmp, strlen(tmp));
	return 0;
}

void svg_close(void)
{
	struct elt_s *e, *e2;

	setg(0);
	fputs("</svg>\n", fout);
	e = stack;
	if (e != 0) {
		stack = 0;
		fprintf(stderr, "svg close: stack not empty ");
		elt_lst_dump(e);
		fprintf(stderr, "\n");
		do {
			e2 = e->next;
			elt_free(e);
			e = e2;
		} while (e != 0);
	}
}
