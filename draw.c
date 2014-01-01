/*
 * Drawing functions.
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

struct BEAM {			/* packages info on one beam */
	struct SYMBOL *s1, *s2;
	float a, b;
	short nflags;
};

static char *acc_tb[] = { "", "sh", "nt", "ft", "dsh", "dft" };

/* scaling stuff */
static int scale_voice;		/* staff (0) or voice(1) scaling */
static float cur_scale = 1;	/* voice or staff scale */
static float cur_trans = 0;	/* != 0 when scaled staff */
static float cur_staff = 1;	/* current scaled staff */

static void draw_note(float x,
		      struct SYMBOL *s,
		      int fl);
static void set_tie_room(void);

/* output debug annotations */
static void anno_out(struct SYMBOL *s, char type)
{
	if (s->as.linenum == 0)
		return;
	if (mbf[-1] != '\n')
		*mbf++ = '\n';
	a2b("%%A %c %d %d ", type, s->as.linenum, s->as.colnum);
	putxy(s->x - s->wl - 2, staff_tb[s->staff].y + s->ymn - 2);
	if (type != 'b' && type != 'e')		/* if not beam */
		a2b("%.1f %d", s->wl + s->wr + 4, s->ymx - s->ymn + 4);
	a2b("\n");
}

/* -- up/down shift needed to get k*6 -- */
static float rnd6(float y)
{
	int iy;

	iy = ((int) (y + 2.999) + 12) / 6 * 6 - 12;
	return iy - y;
}

/* -- compute the best vertical offset for the beams -- */
static float b_pos(int grace,
		   int stem,
		   int flags,
		   float b)
{
	float d1, d2, shift, depth;
	float top, bot;

	shift = !grace ? BEAM_SHIFT : 3;
	depth = !grace ? BEAM_DEPTH : 1.7;
	if (stem > 0) {
		bot = b - (flags - 1) * shift - depth;
		if (bot > 26)
			return 0;
		top = b;
	} else {
		top = b + (flags - 1) * shift + depth;
		if (top < -2)
			return 0;
		bot = b;
	}

	d1 = rnd6(top - BEAM_OFFSET);
	d2 = rnd6(bot + BEAM_OFFSET);
	if (d1 * d1 > d2 * d2)
		return d2;
	return d1;
}

/* duplicate a note for beaming continuation */
static struct SYMBOL *sym_dup(struct SYMBOL *s_orig)
{
	struct SYMBOL *s;

	s = (struct SYMBOL *) getarena(sizeof *s);
	memcpy(s, s_orig, sizeof *s);
	s->as.flags |= ABC_F_INVIS;
	s->as.text = NULL;
	memset(s->as.u.note.sl1, 0, sizeof s->as.u.note.sl1);
	memset(s->as.u.note.decs, 0, sizeof s->as.u.note.decs);
	memset(&s->as.u.note.dc, 0, sizeof s->as.u.note.dc);
	s->gch = NULL;
	s->ly = NULL;
	return s;
}

/* -- calculate a beam -- */
/* (the staves may be defined or not) */
static int calculate_beam(struct BEAM *bm,
			  struct SYMBOL *s1)
{
	struct SYMBOL *s, *s2;
	int notes, nflags, staff, voice, two_staves, two_dir;
	float x, y, ys, a, b, max_stem_err;
	float sx, sy, sxx, sxy, syy, a0, stem_xoff, scale;
	static float min_tb[2][6] = {
		{STEM_MIN, STEM_MIN,
			STEM_MIN2, STEM_MIN3, STEM_MIN4, STEM_MIN4},
		{STEM_CH_MIN, STEM_CH_MIN,
			STEM_CH_MIN2, STEM_CH_MIN3, STEM_CH_MIN4, STEM_CH_MIN4}
	};

	/* must have one printed note head */
	if (s1->as.flags & ABC_F_INVIS) {
		if (!s1->next
		 || (s1->next->as.flags & ABC_F_INVIS))
			return 0;
	}

	if (!(s1->sflags & S_BEAM_ST)) {	/* beam from previous music line */
		s = sym_dup(s1);
		s1->prev->next = s;
		s->prev = s1->prev;
		s1->prev = s;
		s->next = s1;
		s1->ts_prev->ts_next = s;
		s->ts_prev = s1->ts_prev;
		s1->ts_prev = s;
		s->ts_next = s1;
		s->x -= 12;
		if (s->x > s1->prev->x + 12)
			s->x = s1->prev->x + 12;
		s->sflags &= S_SEQST;
		s->sflags |= S_BEAM_ST | S_TEMP;
		s->as.u.note.slur_st = 0;
		s->as.u.note.slur_end = 0;
		s1 = s;
	}

	/* search last note in beam */
	notes = nflags = 0;	/* set x positions, count notes and flags */
	two_staves = two_dir = 0;
	staff = s1->staff;
	voice = s1->voice;
	stem_xoff = (s1->as.flags & ABC_F_GRACE) ? GSTEM_XOFF : STEM_XOFF;
	for (s2 = s1; ; s2 = s2->next) {
		if (s2->as.type == ABC_T_NOTE) {
			if (s2->nflags > nflags)
				nflags = s2->nflags;
			notes++;
			if (s2->staff != staff)
				two_staves = 1;
			if (s2->stem != s1->stem)
				two_dir = 1;
			if (s2->sflags & S_BEAM_END)
				break;
		}
		if (!s2->next) {		/* beam towards next music line */
			for (; ; s2 = s2->prev) {
				if (s2->as.type == ABC_T_NOTE)
					break;
			}
			s = sym_dup(s2);
			s->next = s2->next;
			if (s->next)
				s->next->prev = s;;
			s2->next = s;
			s->prev = s2;
			s->ts_next = s2->ts_next;
			if (s->ts_next)
				s->ts_next->ts_prev = s;
			s2->ts_next = s;
			s->ts_prev = s2;
			s->sflags &= S_SEQST;
			s->sflags |= S_BEAM_END | S_TEMP;
			s->as.u.note.slur_st = 0;
			s->as.u.note.slur_end = 0;
			s->x += 12;
			if (s->x < realwidth - 12)
				s->x = realwidth - 12;
			s2 = s;
			notes++;
			break;
		}
	}
	bm->s2 = s2;			/* (don't display the flags) */
	if (staff_tb[staff].y == 0) {	/* staves not defined */
		if (two_staves)
			return 0;
	} else {			/* staves defined */
		if (!two_staves && !(s1->as.flags & ABC_F_GRACE)) {
			bm->s1 = s1;	/* beam already calculated */
			bm->a = (s1->ys- s2->ys) / (s1->xs - s2->xs);
			bm->b = s1->ys - s1->xs * bm->a
				+ staff_tb[staff].y;
			bm->nflags = nflags;
			return 1;
		}
	}

	sx = sy = sxx = sxy = syy = 0;	/* linear fit through stem ends */
	for (s = s1; ; s = s->next) {
		if (s->as.type != ABC_T_NOTE)
			continue;
		if ((scale = voice_tb[s->voice].scale) == 1)
			scale = staff_tb[s->staff].clef.staffscale;
		if (s->stem >= 0)
			x = stem_xoff + s->shhd[0];
		else
			x = -stem_xoff + s->shhd[s->nhd];
		x *= scale;
		x += s->x;
		s->xs = x;
		y = s->ys + staff_tb[s->staff].y;
		sx += x; sy += y;
		sxx += x * x; sxy += x * y; syy += y * y;
		if (s == s2)
			break;
	}

	/* beam fct: y=ax+b */
	a = (sxy * notes - sx * sy) / (sxx * notes - sx * sx);
	b = (sy - a * sx) / notes;

	/* the next few lines modify the slope of the beam */
	if (!(s1->as.flags & ABC_F_GRACE)) {
		if (notes >= 3) {
			float hh;

			hh = syy - a * sxy - b * sy;	/* flatten if notes not in line */
			if (hh > 0
			 && hh / (notes - 2) > .5)
				a *= BEAM_FLATFAC;
		}
		if (a >= 0)
			a = BEAM_SLOPE * a / (BEAM_SLOPE + a);	/* max steepness for beam */
		else
			a = BEAM_SLOPE * a / (BEAM_SLOPE - a);
	} else {
		if (a > BEAM_SLOPE)
			a = BEAM_SLOPE;
		else if (a < -BEAM_SLOPE)
			a = -BEAM_SLOPE;
	}

	/* to decide if to draw flat etc. use normalized slope a0 */
	a0 = a * (s2->xs - s1->xs) / (20 * (notes - 1));

	if (a0 * a0 < BEAM_THRESH * BEAM_THRESH)
		a = 0;			/* flat below threshhold */

	b = (sy - a * sx) / notes;	/* recalculate b for new slope */

/*  if (nflags>1) b=b+2*stem;*/	/* leave a bit more room if several beams */

	/* have flat beams when asked */
	if (cfmt.flatbeams) {
		if (!(s1->as.flags & ABC_F_GRACE))
			b = -11 + staff_tb[staff].y;
		else
			b = 35 + staff_tb[staff].y;
		a = 0;
	}

/*fixme: have a look again*/
	/* have room for the symbols in the staff */
	max_stem_err = 0;		/* check stem lengths */
	s = s1;
	if (two_dir) {				/* 2 directions */
/*fixme: more to do*/
		if (!(s1->as.flags & ABC_F_GRACE))
			ys = BEAM_SHIFT;
		else
			ys = 3;
		ys *= (nflags - 1);
		ys += BEAM_DEPTH;
		ys *= .5;
		if (s1->stem != s2->stem && s1->nflags < s2->nflags)
			ys *= s2->stem;
		else
			ys *= s1->stem;
		b += ys;
	} else if (!(s1->as.flags & ABC_F_GRACE)) {	/* normal notes */
		float stem_err, beam_h;

		beam_h = BEAM_DEPTH + BEAM_SHIFT * (nflags - 1);
		while (s->ts_prev->as.type == ABC_T_NOTE
		    && s->ts_prev->time == s->time
		    && s->ts_prev->x > s1->xs)
			s = s->ts_prev;

		for (; s && s->time <= s2->time; s = s->ts_next) {
			if (s->as.type != ABC_T_NOTE
			 || (s->as.flags & ABC_F_INVIS)
			 || (s->staff != staff
			  && s->voice != voice)) {
				continue;
			}
			x = s->voice == voice ? s->xs : s->x;
			ys = a * x + b - staff_tb[s->staff].y;
			if (s->voice == voice) {
				if (s->nhd == 0)
					stem_err = min_tb[0][(unsigned) s->nflags];
				else
					stem_err = min_tb[1][(unsigned) s->nflags];
				if (s->stem > 0) {
					if (s->pits[s->nhd] > 26) {
						stem_err -= 2;
						if (s->pits[s->nhd] > 28)
							stem_err -= 2;
					}
					stem_err -= ys - (float) (3 * (s->pits[s->nhd] - 18));
				} else {
					if (s->pits[0] < 18) {
						stem_err -= 2;
						if (s->pits[0] < 16)
							stem_err -= 2;
					}
					stem_err -= (float) (3 * (s->pits[0] - 18)) - ys;
				}
				stem_err += BEAM_DEPTH + BEAM_SHIFT * (s->nflags - 1);
			} else {
/*fixme: KO when two_staves*/
				if (s1->stem > 0) {
					if (s->stem > 0) {
/*fixme: KO when the voice numbers are inverted*/
						if (s->ymn > ys + 4
						 || s->ymx < ys - beam_h - 2)
							continue;
						if (s->voice > voice)
							stem_err = s->ymx - ys;
						else
							stem_err = s->ymn + 8 - ys;
					} else {
						stem_err = s->ymx - ys;
					}
				} else {
					if (s->stem < 0) {
						if (s->ymx < ys - 4
						 || s->ymn > ys - beam_h - 2)
							continue;
						if (s->voice < voice)
							stem_err = ys - s->ymn;
						else
							stem_err = ys - s->ymx + 8;
					} else {
						stem_err = ys - s->ymn;
					}
				}
				stem_err += 2 + beam_h;
			}
			if (stem_err > max_stem_err)
				max_stem_err = stem_err;
		}
	} else {				/* grace notes */
		for ( ; ; s = s->next) {
			float stem_err;

			ys = a * s->xs + b - staff_tb[s->staff].y;
			stem_err = GSTEM - 2;
			if (s->stem > 0)
				stem_err -= ys - (float) (3 * (s->pits[s->nhd] - 18));
			else
				stem_err += ys - (float) (3 * (s->pits[0] - 18));
			stem_err += 3 * (s->nflags - 1);
			if (stem_err > max_stem_err)
				max_stem_err = stem_err;
			if (s == s2)
				break;
		}
	}

	if (max_stem_err > 0)		/* shift beam if stems too short */
		b += s1->stem * max_stem_err;

	/* have room for the gracenotes, bars and clefs */
/*fixme: test*/
    if (!two_staves && !two_dir)
	for (s = s1->next; ; s = s->next) {
		struct SYMBOL *g;

		switch (s->type) {
		case NOTEREST:		/* cannot move rests in multi-voices */
			if (s->as.type != ABC_T_REST)
				break;
			g = s->ts_next;
			if (g->staff != staff
			 || g->type != NOTEREST)
				break;
//fixme:too much vertical shift if some space above the note
//fixme:this does not fix rest under beam in second voice (ts_prev)
			/*fall thru*/
		case BAR:
#if 1
			if (s->as.flags & ABC_F_INVIS)
#else
//??
			if (!(s->as.flags & ABC_F_INVIS))
#endif
				break;
			/*fall thru*/
		case CLEF:
			y = a * s->x + b;
			if (s1->stem > 0) {
				y = s->ymx - y
					+ BEAM_DEPTH + BEAM_SHIFT * (nflags - 1)
					+ 2;
				if (y > 0)
					b += y;
			} else {
				y = s->ymn - y
					- BEAM_DEPTH - BEAM_SHIFT * (nflags - 1)
					- 2;
				if (y < 0)
					b += y;
			}
			break;
		case GRACE:
			g = s->extra;
			for ( ; g; g = g->next) {
				if (g->type != NOTEREST)
					continue;
				y = a * g->x + b;
				if (s1->stem > 0) {
					y = g->ymx - y
						+ BEAM_DEPTH + BEAM_SHIFT * (nflags - 1)
						+ 2;
					if (y > 0)
						b += y;
				} else {
					y = g->ymn - y
						- BEAM_DEPTH - BEAM_SHIFT * (nflags - 1)
						- 2;
					if (y < 0)
						b += y;
				}
			}
			break;
		}
		if (s == s2)
			break;
	}

	if (a == 0)		/* shift flat beams onto staff lines */
		b += b_pos(s1->as.flags & ABC_F_GRACE, s1->stem, nflags,
				b - staff_tb[staff].y);

	/* adjust final stems and rests under beam */
	for (s = s1; ; s = s->next) {
		float dy;

		switch (s->as.type) {
		case ABC_T_NOTE:
			s->ys = a * s->xs + b - staff_tb[s->staff].y;
			if (s->stem > 0) {
				s->ymx = s->ys + 2.5;
//fixme: hack
				if (s->ts_prev
				 && s->ts_prev->stem > 0
				 && s->ts_prev->staff == s->staff
				 && s->ts_prev->ymn < s->ymx
				 && s->ts_prev->x == s->x
				 && s->shhd[0] == 0) {
					s->ts_prev->x -= 5;	/* fix stem clash */
					s->ts_prev->xs -= 5;
				}
			} else {
				s->ymn = s->ys - 2.5;
			}
			break;
		case ABC_T_REST:
			y = a * s->x + b - staff_tb[s->staff].y;
			dy = BEAM_DEPTH + BEAM_SHIFT * (nflags - 1)
				+ (s->head != H_FULL ? 4 : 9);
			if (s1->stem > 0) {
				y -= dy;
				if (s1->multi == 0 && y > 12)
					y = 12;
				if (s->y <= y)
					break;
			} else {
				y += dy;
				if (s1->multi == 0 && y < 12)
					y = 12;
				if (s->y >= y)
					break;
			}
			if (s->head != H_FULL) {
				int iy;

				iy = ((int) y + 3 + 12) / 6 * 6 - 12;
				y = iy;
			}
			s->y = y;
			break;
		}
		if (s == s2)
			break;
	}

	/* save beam parameters */
	if (staff_tb[staff].y == 0)	/* if staves not defined */
		return 0;
	bm->s1 = s1;
	bm->a = a;
	bm->b = b;
	bm->nflags = nflags;
	return 1;
}

/* -- draw a single beam -- */
/* (the staves are defined) */
static void draw_beam(float x1,
		      float x2,
		      float dy,
		      float h,
		      struct BEAM *bm,
		      int n)			/* beam number (1..n) */
{
	struct SYMBOL *s;
	float y1, dy2;

	s = bm->s1;
	if (n > s->nflags - s->u
	 && (s->sflags & S_TREM2) && s->head != H_EMPTY) {
		if (s->head >= H_OVAL) {
			x1 = s->x + 6;
			x2 = bm->s2->x - 6;
		} else {
			x1 += 5;
			x2 -= 6;
		}
	}

	y1 = bm->a * x1 + bm->b - dy;
	x2 -= x1;
	dy2 = bm->a * x2;

	putf(h);
	putx(x2);
	putf(dy2);
	putxy(x1, y1);
	a2b("bm\n");
}

/* -- draw the beams for one word -- */
/* (the staves are defined) */
static void draw_beams(struct BEAM *bm)
{
	struct SYMBOL *s, *s1, *s2;
	int i, beam_dir;
	float shift, bshift, bstub, bh, da;

	s1 = bm->s1;
/*fixme: KO if many staves with different scales*/
	set_scale(s1);
	s2 = bm->s2;
	if (!(s1->as.flags & ABC_F_GRACE)) {
		bshift = BEAM_SHIFT;
		bstub = BEAM_STUB;
		shift = .34;		/* (half width of the stem) */
		bh = BEAM_DEPTH;
	} else {
		bshift = 3;
		bstub = 3.2;
		shift = .29;
		bh = 1.6;
	}

/*fixme: quick hack for stubs at end of beam and different stem directions*/
	beam_dir = s1->stem;
	if (s1->stem != s2->stem
	 && s1->nflags < s2->nflags)
		beam_dir = s2->stem;
	if (beam_dir < 0)
		bh = -bh;
	if (cur_trans == 0 && cur_scale != 1) {
		bm->a /= cur_scale;
		bm->b = s1->ys - s1->xs * bm->a
			+ staff_tb[s1->staff].y;
		bshift *= cur_scale;
	}

	/* make first beam over whole word and adjust the stem lengths */
	draw_beam(s1->xs - shift, s2->xs + shift, 0., bh, bm, 1);
	da = 0;
	for (s = s1; ; s = s->next) {
		if (s->as.type == ABC_T_NOTE
		 && s->stem != beam_dir)
			s->ys = bm->a * s->xs + bm->b
				- staff_tb[s->staff].y
				+ bshift * (s->nflags - 1) * s->stem
				- bh;
		if (s == s2)
			break;
	}

	if (s1->sflags & S_FEATHERED_BEAM) {
		da = bshift / (s2->xs - s1->xs);
		if (s1->dur > s2->dur) {
			da = -da;
			bshift = da * s1->xs;
		} else {
			bshift = da * s2->xs;
		}
		da = da * beam_dir;
	}

	/* other beams with two or more flags */
	shift = 0;
	for (i = 2; i <= bm->nflags; i++) {
		shift += bshift;
		if (da != 0)
			bm->a += da;
		for (s = s1; ; s = s->next) {
			struct SYMBOL *k1, *k2;
			float x1;

			if (s->as.type != ABC_T_NOTE
			 || s->nflags < i) {
				if (s == s2)
					break;
				continue;
			}
			if ((s->sflags & S_TREM1)
			 && i > s->nflags - s->u) {
				if (s->head >= H_OVAL)
					x1 = s->x;
				else
					x1 = s->xs;
				draw_beam(x1 - 5, x1 + 5,
					  (shift + 2.5) * beam_dir,
					  bh, bm, i);
				if (s == s2)
					break;
				continue;
			}
			k1 = s;
			for (;;) {
				if (s == s2)
					break;
				if ((s->next->type == NOTEREST
				 && s->next->nflags < i)
				  || (s->next->sflags & S_BEAM_BR1)
				  || ((s->next->sflags & S_BEAM_BR2)
					 && i > 2))
					break;
				s = s->next;
			}
			k2 = s;
			while (k2->as.type != ABC_T_NOTE)
				k2 = k2->prev;
			x1 = k1->xs;
			if (k1 == k2) {
				if (k1 == s1
				 || (k1->sflags & S_BEAM_BR1)
				 || ((k1->sflags & S_BEAM_BR2)
					&& i > 2)) {
					x1 += bstub;
				} else if (k1 == s2) {
					x1 -= bstub;
				} else {
					struct SYMBOL *k;

					k = k1->next;
					while (k->as.type != ABC_T_NOTE)
						k = k->next;
					if ((k->sflags & S_BEAM_BR1)
					 || ((k->sflags & S_BEAM_BR2)
						&& i > 2)) {
						x1 -= bstub;
					} else {
						k1 = k1->prev;
						while (k1->as.type != ABC_T_NOTE)
							k1 = k1->prev;
						if (k1->nflags < k->nflags
						 || (k1->nflags == k->nflags
							&& k1->dots < k->dots))
							x1 += bstub;
						else
						x1 -= bstub;
					}
				}
			}
			draw_beam(x1, k2->xs,
#if 1
				  shift * beam_dir,
#else
				  shift * k1->stem,	/*fixme: more complicated */
#endif
				  bh, bm, i);
			if (s == s2)
				break;
		}
	}
	if (s1->sflags & S_TEMP)
		unlksym(s1);
	else if (s2->sflags & S_TEMP)
		unlksym(s2);
}

/* -- draw a system brace or bracket -- */
static void draw_sysbra(float x, int staff, int flag)
{
	int i, end;
	float yt, yb;

	while (cursys->staff[staff].empty
	    || staff_tb[staff].clef.stafflines == 0) {
		if (cursys->staff[staff].flags & flag)
			return;
		staff++;
	}
	i = end = staff;
	for (;;) {
		if (!cursys->staff[i].empty
		 && staff_tb[i].clef.stafflines != 0)
			end = i;
		if (cursys->staff[i].flags & flag)
			break;
		i++;
	}
	yt = staff_tb[staff].y + staff_tb[staff].topbar
				* staff_tb[staff].clef.staffscale;
	yb = staff_tb[end].y + staff_tb[end].botbar
				* staff_tb[end].clef.staffscale;
	a2b("%.1f %.1f %.1f %s\n",
	     yt - yb, x, yt,
	     (flag & (CLOSE_BRACE | CLOSE_BRACE2)) ? "brace" : "bracket");
}

/* -- draw the left side of the staves -- */
static void draw_lstaff(float x)
{
	int i, j, l, nst;
	float yb;

	if (cfmt.alignbars)
		return;
	nst = cursys->nstaff;
	l = 0;
	for (i = 0; i < nst; i++) {
		if (cursys->staff[i].flags & (OPEN_BRACE | OPEN_BRACKET))
			l++;
		if (!cursys->staff[i].empty
		 && staff_tb[i].clef.stafflines != 0)
			break;
		if (cursys->staff[i].flags & (CLOSE_BRACE | CLOSE_BRACKET))
			l--;
	}
	for (j = nst; j > i; j--) {
		if (!cursys->staff[j].empty
		 && staff_tb[j].clef.stafflines != 0)
			break;
	}
	if (i == j && l == 0)
		return;
	set_sscale(-1);
	yb = staff_tb[j].y + staff_tb[j].botbar
				* staff_tb[j].clef.staffscale;
	a2b("%.1f %.1f %.1f bar\n",
	     staff_tb[i].y
		+ staff_tb[i].topbar * staff_tb[i].clef.staffscale
		- yb,
	     x, yb);
	for (i = 0; i <= nst; i++) {
		if (cursys->staff[i].flags & OPEN_BRACE)
			draw_sysbra(x, i, CLOSE_BRACE);
		if (cursys->staff[i].flags & OPEN_BRACKET)
			draw_sysbra(x, i, CLOSE_BRACKET);
		if (cursys->staff[i].flags & OPEN_BRACE2)
			draw_sysbra(x - 6, i, CLOSE_BRACE2);
		if (cursys->staff[i].flags & OPEN_BRACKET2)
			draw_sysbra(x - 6, i, CLOSE_BRACKET2);
	}
}

/* -- draw a staff -- */
static void draw_staff(int staff,
			float x1, float x2)
{
	int nlines;
	float y;

	/* draw the staff */
	set_sscale(staff);
	y = staff_tb[staff].y;
	nlines = cursys->staff[staff].clef.stafflines;
	switch (nlines) {
	case 0:
		return;
	case 1:
		y += 12;
		break;
	case 2:
	case 3:
		y += 6;
		break;
	}
	putx(x2 - x1);
	a2b("%d ", nlines);
	putxy(x1, y);
	a2b("staff\n");
}

/* -- draw the time signature -- */
static void draw_timesig(float x,
			 struct SYMBOL *s)
{
	unsigned i, staff, l, l2;
	char *f, meter[64];
	float dx;

	if (s->as.u.meter.nmeter == 0)
		return;
	staff = s->staff;
	x -= s->wl;
	for (i = 0; i < s->as.u.meter.nmeter; i++) {
		l = strlen(s->as.u.meter.meter[i].top);
		if (l > sizeof s->as.u.meter.meter[i].top)
			l = sizeof s->as.u.meter.meter[i].top;
		if (s->as.u.meter.meter[i].bot[0] != '\0') {
			sprintf(meter, "(%.8s)(%.2s)",
				s->as.u.meter.meter[i].top,
				s->as.u.meter.meter[i].bot);
			f = "tsig";
			l2 = strlen(s->as.u.meter.meter[i].bot);
			if (l2 > sizeof s->as.u.meter.meter[i].bot)
				l2 = sizeof s->as.u.meter.meter[i].bot;
			if (l2 > l)
				l = l2;
		} else switch (s->as.u.meter.meter[i].top[0]) {
			case 'C':
				if (s->as.u.meter.meter[i].top[1] != '|')
					f = "csig";
				else {
					f = "ctsig";
					l--;
				}
				meter[0] = '\0';
				break;
			case 'c':
				if (s->as.u.meter.meter[i].top[1] != '.')
					f = "imsig";
				else {
					f = "iMsig";
					l--;
				}
				meter[0] = '\0';
				break;
			case 'o':
				if (s->as.u.meter.meter[i].top[1] != '.')
					f = "pmsig";
				else {
					f = "pMsig";
					l--;
				}
				meter[0] = '\0';
				break;
			case '(':
			case ')':
				sprintf(meter, "(\\%s)",
					s->as.u.meter.meter[i].top);
				f = "stsig";
				break;
			default:
				sprintf(meter, "(%.8s)",
					s->as.u.meter.meter[i].top);
				f = "stsig";
				break;
		}
		if (meter[0] != '\0')
			a2b("%s ", meter);
		dx = (float) (13 * l);
		putxy(x + dx * .5, staff_tb[staff].y);
		a2b("%s\n", f);
		x += dx;
	}
}

/* -- draw a key signature -- */
static void draw_keysig(struct VOICE_S *p_voice,
			float x,
			struct SYMBOL *s)
{
	int old_sf = s->u;
	int staff = p_voice->staff;
	float staffb = staff_tb[staff].y;
	int i, clef_ix, shift;
	const signed char *p_seq;

	static const char sharp_cl[7] = {24, 9, 15, 21, 6, 12, 18};
	static const char flat_cl[7] = {12, 18, 24, 9, 15, 21, 6};
	static const signed char sharp1[6] = {-9, 12, -9, -9, 12, -9};
	static const signed char sharp2[6] = {12, -9, 12, -9, 12, -9};
	static const signed char flat1[6] = {9, -12, 9, -12, 9, -12};
	static const signed char flat2[6] = {-12, 9, -12, 9, -12, 9};

	clef_ix = s->pits[0];
	if (clef_ix & 1)
		clef_ix += 7;
	clef_ix /= 2;
	while (clef_ix < 0)
		clef_ix += 7;
	clef_ix %= 7;

	/* normal accidentals */
	if (s->as.u.key.nacc == 0 && !s->as.u.key.empty) {

		/* put neutrals if not 'accidental cancel' */
		if (cfmt.cancelkey || s->as.u.key.sf == 0) {

			/* when flats to sharps, or sharps to flats, */
			if (s->as.u.key.sf == 0
			 || old_sf * s->as.u.key.sf < 0) {

				/* old sharps */
				shift = sharp_cl[clef_ix];
				p_seq = shift > 9 ? sharp1 : sharp2;
				for (i = 0; i < old_sf; i++) {
					putxy(x, staffb + shift);
					a2b("nt0 ");
					shift += *p_seq++;
					x += 5.5;
				}

				/* old flats */
				shift = flat_cl[clef_ix];
				p_seq = shift < 18 ? flat1 : flat2;
				for (i = 0; i > old_sf; i--) {
					putxy(x, staffb + shift);
					a2b("nt0 ");
					shift += *p_seq++;
					x += 5.5;
				}
				if (s->as.u.key.sf != 0)
					x += 3;		/* extra space */

			/* or less sharps or flats */
			} else if (s->as.u.key.sf > 0) {	/* sharps */
				if (s->as.u.key.sf < old_sf) {
					shift = sharp_cl[clef_ix];
					p_seq = shift > 9 ? sharp1 : sharp2;
					for (i = 0; i < s->as.u.key.sf; i++)
						shift += *p_seq++;
					for (; i < old_sf; i++) {
						putxy(x, staffb + shift);
						a2b("nt0 ");
						shift += *p_seq++;
						x += 5.5;
					}
					x += 3;			/* extra space */
				}
			} else /*if (s->as.u.key.sf < 0)*/ {	/* flats */
				if (s->as.u.key.sf > old_sf) {
					shift = flat_cl[clef_ix];
					p_seq = shift < 18 ? flat1 : flat2;
					for (i = 0; i > s->as.u.key.sf; i--)
						shift += *p_seq++;
					for (; i > old_sf; i--) {
						putxy(x, staffb + shift);
						a2b("nt0 ");
						shift += *p_seq++;
						x += 5.5;
					}
					x += 3;			/* extra space */
				}
			}
		}

		/* new sharps */
		shift = sharp_cl[clef_ix];
		p_seq = shift > 9 ? sharp1 : sharp2;
		for (i = 0; i < s->as.u.key.sf; i++) {
			putxy(x, staffb + shift);
			a2b("sh0 ");
			shift += *p_seq++;
			x += 5.5;
		}

		/* new flats */
		shift = flat_cl[clef_ix];
		p_seq = shift < 18 ? flat1 : flat2;
		for (i = 0; i > s->as.u.key.sf; i--) {
			putxy(x, staffb + shift);
			a2b("ft0 ");
			shift += *p_seq++;
			x += 5.5;
		}
	} else {
		int last_acc, last_shift, n, d;

		/* explicit accidentals */
		last_acc = s->as.u.key.accs[0];
		last_shift = 100;
		for (i = 0; i < s->as.u.key.nacc; i++) {
			if (s->as.u.key.accs[i] != last_acc) {
				last_acc = s->as.u.key.accs[i];
				x += 3;
			}
			shift = s->pits[0] * 3
				+ 3 * (s->as.u.key.pits[i] - 18);
			while (shift < -3)
				shift += 21;
			while (shift > 24 + 3)
				shift -= 21;
			if (shift == last_shift + 21
			 || shift == last_shift - 21)
				x -= 5.5;		/* octave */
			last_shift = shift;
			putxy(x, staffb + shift);
			n = micro_tb[i >> 3];
			if (n != 0 && cfmt.micronewps) {
				d = (n & 0xff) + 1;
				n = (n >> 8) + 1;
				a2b("%d %s%d", n, acc_tb[i & 0x07], d);
			} else {
				a2b("%s%d ", acc_tb[last_acc & 0x07],
					micro_tb[last_acc >> 3]);
			}
			x += 5.5;
		}
	}
	if (old_sf != 0 || s->as.u.key.sf != 0 || s->as.u.key.nacc >= 0)
		a2b("\n");
}

/* -- convert the standard measure bars -- */
static int bar_cnv(int bar_type)
{
	switch (bar_type) {
	case B_OBRA:
/*	case B_CBRA: */
	case (B_OBRA << 4) + B_CBRA:
		return 0;			/* invisible */
	case B_COL:
		return B_BAR;			/* dotted */
#if 0
	case (B_CBRA << 4) + B_BAR:
		return B_BAR;
#endif
	case (B_BAR << 4) + B_COL:
		bar_type |= (B_OBRA << 8);		/* |: -> [|: */
		break;
	case (B_BAR << 8) + (B_COL << 4) + B_COL:
		bar_type |= (B_OBRA << 12);		/* |:: -> [|:: */
		break;
	case (B_BAR << 12) + (B_COL << 8) + (B_COL << 4) + B_COL:
		bar_type |= (B_OBRA << 16);		/* |::: -> [|::: */
		break;
	case (B_COL << 4) + B_BAR:
	case (B_COL << 8) + (B_COL << 4) + B_BAR:
	case (B_COL << 12) + (B_COL << 8) + (B_COL << 4) + B_BAR:
		bar_type <<= 4;
		bar_type |= B_CBRA;			/* :..| -> :..|] */
		break;
	case (B_COL << 4) + B_COL:
		bar_type = cfmt.dblrepbar;		/* :: -> dble repeat bar */
		break;
	}
	return bar_type;
}

/* -- draw a measure bar -- */
static void draw_bar(struct SYMBOL *s, float bot, float h)
{
	int staff, bar_type, dotted;
	float x, yb;
	char *psf;

	staff = s->staff;
	yb = staff_tb[staff].y;
	x = s->x;

	/* if measure repeat, draw the '%' like glyphs */
	if (s->as.u.bar.len != 0) {
		struct SYMBOL *s2;

		set_scale(s);
		if (s->as.u.bar.len == 1) {
			for (s2 = s->prev; s2->as.type != ABC_T_REST; s2 = s2->prev)
				;
			putxy(s2->x, yb);
			a2b("mrep\n");
		} else {
			putxy(x, yb);
			a2b("mrep2\n");
			if (s->voice == cursys->top_voice) {
/*fixme				set_font(s->gcf); */
				set_font(cfmt.anf);
				putxy(x, yb + staff_tb[staff].topbar + 4);
				a2b("M(%d)showc\n", s->as.u.bar.len);
			}
		}
	}
	dotted = s->as.u.bar.dotted || s->as.u.bar.type == B_COL;
	bar_type = bar_cnv(s->as.u.bar.type);
	if (bar_type == 0)
		return;				/* invisible */
	for (;;) {
		psf = "bar";
		switch (bar_type & 0x07) {
		case B_BAR:
			if (dotted)
				psf = "dotbar";
			break;
		case B_OBRA:
		case B_CBRA:
			psf = "thbar";
			x -= 3;
			break;
		case B_COL:
			x -= 2;
			break;
		}
		switch (bar_type & 0x07) {
		default:
			set_sscale(-1);
			a2b("%.1f %.1f %.1f %s ", h, x, bot, psf);
			break;
		case B_COL:
			set_sscale(staff);
			putxy(x + 1, staff_tb[staff].y);
			a2b("rdots ");
			break;
		}
		bar_type >>= 4;
		if (bar_type == 0)
			break;
		x -= 3;
	}
	a2b("\n");
}

/* -- draw a rest -- */
/* (the staves are defined) */
static void draw_rest(struct SYMBOL *s)
{
	int i, y;
	float x, dotx, staffb;

static char *rest_tb[NFLAGS_SZ] = {
	"r128", "r64", "r32", "r16", "r8",
	"r4",
	"r2", "r1", "r0", "r00"
};

	/* if rest alone in the measure, center */
	x = s->x + s->shhd[0] * cur_scale;
	if (s->dur == voice_tb[s->voice].meter.wmeasure) {
		struct SYMBOL *prev;

		if (s->next)
			x = s->next->x;
		else
			x = realwidth;
		prev = s->prev;
		if (!prev) {
			prev = s;
		} else if (prev->type != BAR && !(s->sflags & S_SECOND)) {
			for (prev = prev->ts_next; ; prev = prev->ts_next) {
				switch (prev->type) {
				case CLEF:
				case KEYSIG:
				case TIMESIG:
				case FMTCHG:
					continue;
				default:
					break;
				}
				prev = prev->ts_prev;
				break;
			}
		}
		x = (x + prev->x) * .5;

		/* center the associated decorations */
		if (s->as.u.note.dc.n > 0)
			deco_update(s, x - s->x);
		s->x = x;
	}
	if ((s->as.flags & ABC_F_INVIS)
	 && !(s->sflags & S_OTHER_HEAD))
		return;

	staffb = staff_tb[s->staff].y;		/* bottom of staff */

	if (s->sflags & S_REPEAT) {
		putxy(x, staffb);
		if (s->doty < 0) {
			a2b("srep\n");
		} else {
			a2b("mrep\n");
			if (s->doty > 2
			 && s->voice == cursys->top_voice) {
/*fixme				set_font(s->gcf); */
				set_font(cfmt.anf);
				putxy(x, staffb + 24 + 4);
				a2b("M(%d)showc\n", s->doty);
			}
		}
		return;
	}

	y = s->y;

	if (s->sflags & S_OTHER_HEAD) {
		draw_all_deco_head(s, x, y + staffb);
		return;
	}

	i = C_XFLAGS - s->nflags;		/* rest_tb index */
	if (i == 7 && y == 12
	 && staff_tb[s->staff].clef.stafflines <= 2)
		y -= 6;				/* semibreve a bit lower */

	putxy(x, y + staffb);				/* rest */
	a2b("%s ", rest_tb[i]);

	/* output ledger line(s) when greater than minim */
	if (i >= 6) {
		int yb, yt;

		switch (staff_tb[s->staff].clef.stafflines) {
		case 0:
			yb = 12;
			yt = 12;
			break;
		case 1:
			yb = 6;
			yt = 18;
			break;
		case 2:
			yb = 0;
			yt = 18;
			break;
		case 3:
			yb = 0;
			yt = 24;
			break;
		default:
			yb = -6;
			yt = staff_tb[s->staff].clef.stafflines * 6;
			break;
		}
		switch (i) {
		case 6:					/* minim */
			if (y <= yb || y >= yt) {
				putxy(x, y + staffb);
				a2b("hl ");
			}
			break;
		case 7:					/* semibreve */
			if (y < yb || y >= yt - 6) {
				putxy(x, y + 6 + staffb);
				a2b("hl ");
			}
			break;
		default:
			if (y < yb || y >= yt - 6) {
				putxy(x,y + 6 + staffb);
				a2b("hl ");
			}
			if (i == 9)			/* longa */
				y -= 6;
			if (y <= yb || y >= yt) {
				putxy(x, y + staffb);
				a2b("hl ");
			}
			break;
		}
	}

	dotx = 8;
	for (i = 0; i < s->dots; i++) {
		a2b("%.1f 3 dt ", dotx);
		dotx += 3.5;
	}
	a2b("\n");
}

/* -- draw grace notes -- */
/* (the staves are defined) */
static void draw_gracenotes(struct SYMBOL *s)
{
	int yy;
	float x0, y0, x1, y1, x2, y2, x3, y3, bet1, bet2, dy1, dy2;
	struct SYMBOL *g, *last;
	struct BEAM bm;

	/* draw the notes */
	bm.s2 = 0;				/* (draw flags) */
	for (g = s->extra; g; g = g->next) {
		if (g->type != NOTEREST)
			continue;
		if ((g->sflags & (S_BEAM_ST | S_BEAM_END)) == S_BEAM_ST) {
			if (annotate)
				anno_out(g, 'b');
			if (calculate_beam(&bm, g))
				draw_beams(&bm);
		}
		draw_note(g->x, g, bm.s2 == 0);
		if (annotate)
			anno_out(s, 'g');
		if (g == bm.s2)
			bm.s2 = 0;			/* (draw flags again) */

		if (g->as.flags & ABC_F_SAPPO) {	/* (on 1st note only) */
			if (!g->next) {			/* if one note */
				x1 = 9;
				y1 = g->stem > 0 ? 5 : -5;
			} else {			/* many notes */
				x1 = (g->next->x - g->x) * .5 + 4;
				y1 = (g->ys + g->next->ys) * .5 - g->y;
				if (g->stem > 0)
					y1 -= 1;
				else
					y1 += 1;
			}
			putxy(x1, y1);
			a2b("g%ca\n", g->stem > 0 ? 'u' : 'd');
		}
		if (annotate
		 && (g->sflags & (S_BEAM_ST | S_BEAM_END)) == S_BEAM_END)
			anno_out(g, 'e');
		if (!g->next)
			break;			/* (keep the last note) */
	}

	/* slur */
	if (voice_tb[s->voice].key.mode >= BAGPIPE /* no slur when bagpipe */
	 || !cfmt.graceslurs
	 || s->as.u.note.slur_st		/* explicit slur */
	 || !s->next
	 || s->next->as.type != ABC_T_NOTE)
		return;
	last = g;
	if (last->stem >= 0) {
		yy = 127;
		for (g = s->extra; g; g = g->next) {
			if (g->type != NOTEREST)
				continue;
			if (g->y < yy) {
				yy = g->y;
				last = g;
			}
		}
		x0 = last->x;
		y0 = last->y - 5;
		if (s->extra != last) {
			x0 -= 4;
			y0 += 1;
		}
		s = s->next;
		x3 = s->x - 1;
		if (s->stem < 0)
			x3 -= 4;
		y3 = 3 * (s->pits[0] - 18) - 5;
		dy1 = (x3 - x0) * .4;
		if (dy1 > 3)
			dy1 = 3;
			dy2 = dy1;
		bet1 = .2;
		bet2 = .8;
		if (y0 > y3 + 7) {
			x0 = last->x - 1;
			y0 += .5;
			y3 += 6.5;
			x3 = s->x - 5.5;
			dy1 = (y0 - y3) * .8;
			dy2 = (y0 - y3) * .2;
			bet1 = 0;
		} else if (y3 > y0 + 4) {
			y3 = y0 + 4;
			x0 = last->x + 2;
			y0 = last->y - 4;
		}
	} else {
		yy = -127;
		for (g = s->extra; g; g = g->next) {
			if (g->type != NOTEREST)
				continue;
			if (g->y > yy) {
				yy = g->y;
				last = g;
			}
		}
		x0 = last->x;
		y0 = last->y + 5;
		if (s->extra != last) {
			x0 -= 4;
			y0 -= 1;
		}
		s = s->next;
		x3 = s->x - 1;
		if (s->stem >= 0)
			x3 -= 2;
		y3 = 3 * (s->pits[s->nhd] - 18) + 5;
		dy1 = (x0 - x3) * .4;
		if (dy1 < -3)
			dy1 = -3;
		dy2 = dy1;
		bet1 = .2;
		bet2 = .8;
		if (y0 < y3 - 7) {
			x0 = last->x - 1;
			y0 -= .5;
			y3 -= 6.5;
			x3 = s->x - 5.5;
			dy1 = (y0 - y3) * .8;
			dy2 = (y0 - y3) * .2;
			bet1 = 0;
		} else if (y3 < y0 - 4) {
			y3 = y0 - 4;
			x0 = last->x + 2;
			y0 = last->y + 4;
		}
	}

	x1 = bet1 * x3 + (1 - bet1) * x0;
	y1 = bet1 * y3 + (1 - bet1) * y0 - dy1;
	x2 = bet2 * x3 + (1 - bet2) * x0;
	y2 = bet2 * y3 + (1 - bet2) * y0 - dy2;

	a2b("%.2f %.2f %.2f %.2f %.2f %.2f ",
		x1 - x0, y1 - y0,
		x2 - x0, y2 - y0,
		x3 - x0, y3 - y0);
	putxy(x0, y0 + staff_tb[s->staff].y);
	a2b("gsl\n");
}

/* -- set the y offset of the dots -- */
static void setdoty(struct SYMBOL *s,
		    signed char *y_tb)
{
	int m, m1, y, doty;

	/* set the normal offsets */
	doty = s->doty;
	for (m = 0; m <= s->nhd; m++) {
		y = 3 * (s->pits[m] - 18);	/* note height on staff */
		if ((y % 6) == 0) {
			if (doty != 0)
				y -= 3;
			else
				y += 3;
		}
		y_tb[m] = y;
	}

	/* dispatch and recenter the dots in the staff spaces */
	for (m = 0; m < s->nhd; m++) {
		if (y_tb[m + 1] > y_tb[m])
			continue;
		m1 = m;
		while (m1 > 0) {
			if (y_tb[m1] > y_tb[m1 - 1] + 6)
				break;
			m1--;
		}
		if (3 * (s->pits[m1] - 18) - y_tb[m1]
				< y_tb[m + 1] - 3 * (s->pits[m + 1] - 18)) {
			while (m1 <= m)
				y_tb[m1++] -= 6;
		} else {
			y_tb[m + 1] = y_tb[m] + 6;
		}
	}
}

/* -- draw m-th head with accidentals and dots -- */
/* (the staves are defined) */
static void draw_basic_note(float x,
			    struct SYMBOL *s,
			    int m,
			    signed char *y_tb)
{
	int i, y, no_head, head, dots, nflags;
	float staffb, shhd;
	char *p;
	char perc_hd[8];

	staffb = staff_tb[s->staff].y;		/* bottom of staff */
	y = 3 * (s->pits[m] - 18);		/* note height on staff */
	shhd = s->shhd[m] * cur_scale;

	/* draw the note decorations */
	no_head = (s->sflags & S_OTHER_HEAD);
	if (no_head)
		draw_all_deco_head(s, x + shhd, y + staffb);
	if (s->as.u.note.decs[m] != 0) {
		int n;

		i = s->as.u.note.decs[m] >> 3;		/* index */
		n = i + (s->as.u.note.decs[m] & 0x07);	/* # deco */
		for ( ; i < n; i++)
			no_head |= draw_deco_head(s->as.u.note.dc.t[i],
						  x + shhd,
						  y + staffb,
						  s->stem);
	}
	if (s->as.flags & ABC_F_INVIS)
		return;

	/* special case when no head */
	if (s->nohdix >= 0) {
		if ((s->stem > 0 && m <= s->nohdix)
		 || (s->stem < 0 && m >= s->nohdix)) {
			a2b("/x ");			/* set x y */
			putx(x + shhd);
			a2b("def/y ");
			puty(y + staffb);
			a2b("def");
			return;
		}
	}

	identify_note(s, s->as.u.note.lens[m],
		      &head, &dots, &nflags);

	/* output a ledger line if horizontal shift / chord
	 * and note on a line */
	if (y % 6 == 0
	 && shhd != (s->stem > 0 ? s->shhd[0] : s->shhd[s->nhd])) {
		int yy;

		yy = 0;
		if (y >= 30) {
			yy = y;
			if (yy % 6)
				yy -= 3;
		} else if (y <= -6) {
			yy = y;
			if (yy % 6)
				yy += 3;
		}
		if (yy) {
			putxy(x + shhd, yy + staffb);
			a2b("hl ");
		}
	}

	/* draw the head */
	putxy(x + shhd, y + staffb);
	if (no_head) {
		p = "/y exch def/x exch def";
	} else if (s->as.flags & ABC_F_GRACE) {
		p = "ghd";
	} else if (s->type == CUSTOS) {
		p = "custos";
	} else if ((s->sflags & S_PERC)
	        && (i = s->as.u.note.accs[m]) != 0) {
		i &= 0x07;
		sprintf(perc_hd, "p%shd", acc_tb[i]);
		p = perc_hd;
	} else {
		switch (head) {
		case H_OVAL:
			if (s->as.u.note.lens[m] < BREVE) {
				p = "HD";
				break;
			}
			if (s->head != H_SQUARE) {
				p = "HDD";
				break;
			}
			/* fall thru */
		case H_SQUARE:
			if (s->as.u.note.lens[m] < BREVE * 2)
				p = "breve";
			else
				p = "longa";

			/* don't display dots on last note of the tune */
			if (!tsnext && s->next
			 && s->next->type == BAR && !s->next->next)
				dots = 0;
			break;
		case H_EMPTY:
			p = "Hd"; break;
		default:
			p = "hd"; break;
		}
	}
	a2b("%s", p);

	/* draw the dots */
/*fixme: to see for grace notes*/
	if (dots) {
		float dotx;
		int doty;

		dotx = (int) (8. + s->xmx);
		doty = y_tb[m] - y;
		while (--dots >= 0) {
			a2b(" %.1f %d dt", dotx - shhd, doty);
			dotx += 3.5;
		}
	}

	/* draw the accidental */
	if ((i = s->as.u.note.accs[m]) != 0
	 && !(s->sflags & S_PERC)) {
		int n, d;

		x -= s->shac[m] * cur_scale;
		a2b(" ");
		putx(x);
		n = micro_tb[i >> 3];
		if (n != 0 && cfmt.micronewps) {
			d = (n & 0xff) + 1;
			n = (n >> 8) + 1;
			a2b((s->as.flags & ABC_F_GRACE)
					? "gsc %d %s%d grestore" : "y %d %s%d",
				n, acc_tb[i & 0x07], d);
		} else {
			a2b((s->as.flags & ABC_F_GRACE)
					? "gsc %s%d grestore" : "y %s%d",
				acc_tb[i & 0x07], n);
		}
	}
}

/* -- draw a note or a chord -- */
/* (the staves are defined) */
static void draw_note(float x,
		      struct SYMBOL *s,
		      int fl)
{
	int i, m, ma, y;
	float staffb, slen, shhd;
	char c, *hltype;
	signed char y_tb[MAXHD];

	if (s->dots)
		setdoty(s, y_tb);
	if (s->head >= H_OVAL)
		x += 1;
	staffb = staff_tb[s->staff].y;

	/* output the ledger lines */
	if (!(s->as.flags & ABC_F_INVIS)) {
		if (s->as.flags & ABC_F_GRACE) {
			hltype = "ghl";
		} else {
			switch (s->head) {
			default:
				hltype = "hl";
				break;
			case H_OVAL:
				hltype = "hl1";
				break;
			case H_SQUARE:
				hltype = "hl2";
				break;
			}
		}
		shhd = s->stem > 0 ? s->shhd[0] : s->shhd[s->nhd]
			* cur_scale;
		y = 3 * (s->pits[0] - 18);	/* lower ledger lines */
		switch (staff_tb[s->staff].clef.stafflines) {
		case 0:
		case 1: i = 6; break;
		case 2:
		case 3: i = 0; break;
		default: i = -6; break;
		}
		for ( ; i >= y; i -= 6) {
			putxy(x + shhd, i + staffb);
			a2b("%s ", hltype);
		}
		y = 3 * (s->pits[s->nhd] - 18);	/* upper ledger lines */
		switch (staff_tb[s->staff].clef.stafflines) {
		case 0:
		case 1:
		case 2: i = 18; break;
		case 3: i = 24; break;
		default: i = staff_tb[s->staff].clef.stafflines * 6; break;
		}
		for ( ; i <= y; i += 6) {
			putxy(x + shhd, i + staffb);
			a2b("%s ", hltype);
		}
	}

	/* draw the master note, first or last one */
	if (cfmt.setdefl)
		set_defl(s->stem >= 0 ? DEF_STEMUP : 0);
	ma = s->stem >= 0 ? 0 : s->nhd;

	draw_basic_note(x, s, ma, y_tb);

	/* add stem and flags */
	if (!(s->as.flags & (ABC_F_INVIS | ABC_F_STEMLESS))) {
		char c2;

		c = s->stem >= 0 ? 'u' : 'd';
		slen = (s->ys - s->y) / voice_tb[s->voice].scale;
		if (!fl || s->nflags - s->u <= 0) {	/* stem only */
			c2 = (s->as.flags & ABC_F_GRACE) ? 'g' : 's';
			if (s->nflags > 0) {	/* (fix for PS low resolution) */
				if (s->stem >= 0)
					slen -= 1;
				else
					slen += 1;
			}
			a2b(" %.1f %c%c", slen, c2, c);
		} else {				/* stem and flags */
			if (cfmt.straightflags)
				c = 's';		/* straight flag */
			c2 = (s->as.flags & ABC_F_GRACE) ? 'g' : 'f';
			a2b(" %d %.1f s%c%c", s->nflags - s->u, slen, c2, c);
		}
	} else if (s->sflags & S_XSTEM) {	/* cross-staff stem */
		struct SYMBOL *s2;

		s2 = s->ts_prev;
		if (s2->stem > 0)
			slen = s2->y - s->y;
		else
			slen = s2->ys - s->y;
		slen += staff_tb[s2->staff].y - staffb;
/*fixme:KO when different scales*/
		slen /= voice_tb[s->voice].scale;
		a2b(" %.1f su", slen);
	}

	/* draw the tremolo bars */
	if (!(s->as.flags & ABC_F_INVIS)
	 && fl
	 && (s->sflags & S_TREM1)) {
		float x1;

		x1 = x;
		if (s->stem > 0)
			slen = 3 * (s->pits[s->nhd] - 18);
		else
			slen = 3 * (s->pits[0] - 18);
		if (s->head >= H_OVAL) {
			if (s->stem > 0)
				slen = slen + 5 + 5.4 * s->u;
			else
				slen = slen - 5 - 5.4;
		} else {
			x1 += ((s->as.flags & ABC_F_GRACE)
					? GSTEM_XOFF : STEM_XOFF)
							* s->stem;
			if (s->stem > 0)
				slen = slen + 6 + 5.4 * s->u;
			else
				slen = slen - 6 - 5.4;
		}
		slen /= voice_tb[s->voice].scale;
		a2b(" %d ", s->u);
		putxy(x1, staffb + slen);
		a2b("trem");
	}

	/* draw the other note heads */
	for (m = 0; m <= s->nhd; m++) {
		if (m == ma)
			continue;
		a2b(" ");
		draw_basic_note(x, s, m, y_tb);
	}
	a2b("\n");
}

/* -- find where to terminate/start a slur -- */
static struct SYMBOL *next_scut(struct SYMBOL *s)
{
	struct SYMBOL *prev;

	prev = s;
	for (s = s->next; s; s = s->next) {
		if (s->type == BAR
		 && ((s->sflags & S_RRBAR)
			|| s->as.u.bar.type == B_THIN_THICK
			|| s->as.u.bar.type == B_THICK_THIN
			|| (s->as.u.bar.repeat_bar
			 && s->as.text
			 && s->as.text[0] != '1')))
			return s;
		prev = s;
	}
	/*fixme: KO when no note for this voice at end of staff */
	return prev;
}

static struct SYMBOL *prev_scut(struct SYMBOL *s)
{
	struct SYMBOL *sym;
	int voice;
	float x;

	voice = s->voice;
	for (s = s->prev ; s; s = s->prev) {
		if (s->type == BAR
		 && ((s->sflags & S_RRBAR)
		  || s->as.u.bar.type == B_THIN_THICK
		  || s->as.u.bar.type == B_THICK_THIN
		  || (s->as.u.bar.repeat_bar
		   && s->as.text
		   && s->as.text[0] != '1')))
			return s;
	}

	/* return sym before first note/rest/bar */
	sym = voice_tb[voice].sym;
	for (s = sym->next; s; s = s->next) {
		switch (s->as.type) {
		case ABC_T_NOTE:
		case ABC_T_REST:
		case ABC_T_BAR:
			x = s->x;
			do {
				s = s->prev;
			} while (s->x == x);
			return s;
		}
	}
	return sym;
}

/* -- decide whether a slur goes up or down -- */
static int slur_direction(struct SYMBOL *k1,
			  struct SYMBOL *k2)
{
	struct SYMBOL *s;
	int some_upstem, low;

	some_upstem = low = 0;
	for (s = k1; ; s = s->next) {
		if (s->as.type == ABC_T_NOTE) {
			if (!(s->as.flags & ABC_F_STEMLESS)) {
				if (s->stem < 0)
					return 1;
				some_upstem = 1;
			}
			if (s->pits[0] < 22)	/* if under middle staff */
				low = 1;
		}
		if (s == k2)
			break;
	}
	if (!some_upstem && !low)
		return 1;
	return -1;
}

/* -- output a slur / tie -- */
static void slur_out(float x1,
		     float y1,
		     float x2,
		     float y2,
		     int s,
		     float height,
		     int dotted,
		     int staff)	/* if < 0, the staves are defined */
{
	float alfa, beta, mx, my, xx1, yy1, xx2, yy2, dx, dy, dz;
	float scale_y;

	alfa = .3;
	beta = .45;

	/* for wide flat slurs, make shape more square */
	dy = y2 - y1;
	if (dy < 0)
		dy = -dy;
	dx = x2 - x1;
	if (dx > 40. && dy / dx < .7) {
		alfa = .3 + .002 * (dx - 40.);
		if (alfa > .7)
			alfa = .7;
	}

	/* alfa, beta, and height determine Bezier control points pp1,pp2
	 *
	 *           X====alfa===|===alfa=====X
	 *	    /		 |	       \
	 *	  pp1		 |	        pp2
	 *	  /	       height		 \
	 *	beta		 |		 beta
	 *      /		 |		   \
	 *    p1		 m		     p2
	 *
	 */

	mx = .5 * (x1 + x2);
	my = .5 * (y1 + y2);

	xx1 = mx + alfa * (x1 - mx);
	yy1 = my + alfa * (y1 - my) + height;
	xx1 = x1 + beta * (xx1 - x1);
	yy1 = y1 + beta * (yy1 - y1);

	xx2 = mx + alfa * (x2 - mx);
	yy2 = my + alfa * (y2 - my) + height;
	xx2 = x2 + beta * (xx2 - x2);
	yy2 = y2 + beta * (yy2 - y2);

	dx = .03 * (x2 - x1);
//	if (dx > 10.)
//		dx = 10.;
//	dy = 1.6 * s;
	dy = 2 * s;
	dz = .2 + .001 * (x2 - x1);
	if (dz > .6)
		dz = .6;
	dz *= s;
	
	scale_y = scale_voice ? cur_scale : 1;
	if (!dotted)
		a2b("%.2f %.2f %.2f %.2f %.2f %.2f 0 %.2f ",
			(xx2 - dx - x2) / cur_scale,
				(yy2 + dy - y2 - dz) / scale_y,
			(xx1 + dx - x2) / cur_scale,
				(yy1 + dy - y2 - dz) / scale_y,
			(x1 - x2) / cur_scale,
				(y1 - y2 - dz) / scale_y,
				dz);
	a2b("%.2f %.2f %.2f %.2f %.2f %.2f ",
		(xx1 - x1) / cur_scale, (yy1 - y1) / scale_y,
		(xx2 - x1) / cur_scale, (yy2 - y1) / scale_y,
		(x2 - x1) / cur_scale, (y2 - y1) / scale_y);
	putxy(x1, y1);
	if (staff >= 0)
		a2b("y%d ", staff);
	a2b(dotted ? "dSL\n" : "SL\n");
}

/* -- check if slur sequence in a multi-voice staff -- */
static int slur_multi(struct SYMBOL *k1,
		      struct SYMBOL *k2)
{
	for (;;) {
		if (k1->multi != 0)	/* if multi voice */
			/*fixme: may change*/
			return k1->multi;
		if (k1 == k2)
			break;
		k1 = k1->next;
	}
	return 0;
}

/* -- draw a phrasing slur between two symbols -- */
/* (the staves are not yet defined) */
/* (not a pretty routine, this) */
static int draw_slur(struct SYMBOL *k1,
		     struct SYMBOL *k2,
		     int m1,
		     int m2,
		     int slur_type)
{
	struct SYMBOL *k;
	float x1, y1, x2, y2, height, addy;
	float a, y, z, h, dx, dy;
	int s, nn, upstaff, two_staves;

/*fixme: if two staves, may have upper or lower slur*/
	switch (slur_type & 0x03) {	/* (ignore dot bit) */
	case SL_ABOVE: s = 1; break;
	case SL_BELOW: s = -1; break;
	default:
		if ((s = slur_multi(k1, k2)) == 0)
			s = slur_direction(k1, k2);
		break;
	}

	nn = 1;
	upstaff = k1->staff;
	two_staves = 0;
	if (k1 != k2)
	    for (k = k1->next; k; k = k->next) {
		if (k->type == NOTEREST) {
			nn++;
			if (k->staff != upstaff) {
				two_staves = 1;
				if (k->staff < upstaff)
					upstaff = k->staff;
			}
		}
		if (k == k2)
			break;
	}
/*fixme: KO when two staves*/
if (two_staves) error(0, k1, "*** multi-staves slurs not treated yet");

	/* fix endpoints */
	x1 = k1->x + k1->xmx;		/* take the max right side */
	if (k1 != k2) {
		x2 = k2->x;
	} else {		/* (the slur starts on last note of the line) */
		for (k = k2->ts_next; k; k = k->ts_next)
			if (k->type == STAVES)
				break;
		if (!k)
			x2 = realwidth;
		else
			x2 = k->x;
	}
	y1 = (float) (s > 0 ? k1->ymx + 2 : k1->ymn - 2);
	y2 = (float) (s > 0 ? k2->ymx + 2 : k2->ymn - 2);

	if (k1->as.type == ABC_T_NOTE) {
		if (s > 0) {
			if (k1->stem > 0) {
				x1 += 5;
				if ((k1->sflags & S_BEAM_END)
				 && k1->nflags >= -1	/* if with a stem */
//fixme: check if at end of tuplet
				 && (!(k1->sflags & S_IN_TUPLET))) {
//				  || k1->ys > y1 - 3)) {
					if (k1->nflags > 0) {
						x1 += 2;
						y1 = k1->ys - 3;
					} else {
						y1 = k1->ys - 6;
					}
				} else {
					y1 = k1->ys + 3;
				}
			} else {
				y1 = k1->y + 8;
			}
		} else {
			if (k1->stem < 0) {
				x1 -= 1;
				if ((k1->sflags & S_BEAM_END)
				 && k1->nflags >= -1
				 && (!(k1->sflags & S_IN_TUPLET)
				  || k1->ys < y1 + 3)) {
					if (k1->nflags > 0) {
						x1 += 2;
						y1 = k1->ys + 3;
					} else {
						y1 = k1->ys + 6;
					}
				} else {
					y1 = k1->ys - 3;
				}
			} else {
				y1 = k1->y - 8;
			}
		}
	}

	if (k2->as.type == ABC_T_NOTE) {
		if (s > 0) {
			if (k2->stem > 0) {
				x2 += 1;
				if ((k2->sflags & S_BEAM_ST)
				 && k2->nflags >= -1
				 && (!(k2->sflags & S_IN_TUPLET)))
//					|| k2->ys > y2 - 3))
					y2 = k2->ys - 6;
				else
					y2 = k2->ys + 3;
			} else {
				y2 = k2->y + 8;
			}
		} else {
			if (k2->stem < 0) {
				x2 -= 5;
				if ((k2->sflags & S_BEAM_ST)
				 && k2->nflags >= -1
				 && (!(k2->sflags & S_IN_TUPLET)))
//					|| k2->ys < y2 + 3))
					y2 = k2->ys + 6;
				else
					y2 = k2->ys - 3;
			} else {
				y2 = k2->y - 8;
			}
		}
	}

	if (k1->as.type != ABC_T_NOTE) {
		y1 = y2 + 1.2 * s;
		x1 = k1->x + k1->wr * .5;
		if (x1 > x2 - 12)
			x1 = x2 - 12;
	}

	if (k2->as.type != ABC_T_NOTE) {
		if (k1->as.type == ABC_T_NOTE)
			y2 = y1 + 1.2 * s;
		else
			y2 = y1;
		if (k1 != k2)
			x2 = k2->x - k2->wl * .3;
	}

	if (nn >= 3) {
		if (k1->next->type != BAR
		 && k1->next->x < x1 + 48) {
			if (s > 0) {
				y = k1->next->ymx - 2;
				if (y1 < y)
					y1 = y;
			} else {
				y = k1->next->ymn + 2;
				if (y1 > y)
					y1 = y;
			}
		}
		if (k2->prev->type != BAR
		 && k2->prev->x > x2 - 48) {
			if (s > 0) {
				y = k2->prev->ymx - 2;
				if (y2 < y)
					y2 = y;
			} else {
				y = k2->prev->ymn + 2;
				if (y2 > y)
					y2 = y;
			}
		}
	}

#if 0
	/* shift endpoints */
	addx = .04 * (x2 - x1);
	if (addx > 3.0)
		addx = 3.0;
	addy = .01 * (x2 - x1);
	if (addy > 3.0)
		addy = 3.0;
	x1 += addx;
	x2 -= addx;

/*fixme: to simplify*/
	if (k1->staff == upstaff)
		y1 += s * addy;
	else
		y1 = -6;
	if (k2->staff == upstaff)
		y2 += s * addy;
	else
		y2 = -6;
#endif

	a = (y2 - y1) / (x2 - x1);		/* slur steepness */
	if (a > SLUR_SLOPE || a < -SLUR_SLOPE) {
		if (a > SLUR_SLOPE)
			a = SLUR_SLOPE;
		else
			a = -SLUR_SLOPE;
		if (a * s > 0)
			y1 = y2 - a * (x2 - x1);
		else
			y2 = y1 + a * (x2 - x1);
	}

	/* for big vertical jump, shift endpoints */
	y = y2 - y1;
	if (y > 8)
		y = 8;
	else if (y < -8)
		y = -8;
	z = y;
	if (z < 0)
		z = -z;
	dx = .5 * z;
	dy = .3 * y;
	if (y * s > 0) {
		x2 -= dx;
		y2 -= dy;
	} else {
		x1 += dx;
		y1 += dy;
	}

	/* special case for grace notes */
	if (k1->as.flags & ABC_F_GRACE)
		x1 = k1->x - GSTEM_XOFF * .5;
	if (k2->as.flags & ABC_F_GRACE)
		x2 = k2->x + GSTEM_XOFF * 1.5;

	h = 0;
	a = (y2 - y1) / (x2 - x1);
	if (k1 != k2) {
	    addy = y1 - a * x1;
	    for (k = k1->next; k != k2 ; k = k->next) {
		if (k->staff != upstaff)
			continue;
		switch (k->type) {
		case NOTEREST:
			if (s > 0) {
				y = 3 * (k->pits[k->nhd] - 18) + 6;
				if (y < k->ymx)
					y = k->ymx;
				y -= a * k->x + addy;
				if (y > h)
					h = y;
			} else {
				y = 3 * (k->pits[0] - 18) - 6;
				if (y > k->ymn)
					y = k->ymn;
				y -= a * k->x + addy;
				if (y < h)
					h = y;
			}
			break;
		case GRACE: {
			struct SYMBOL *g;

			for (g = k->extra; g; g = g->next) {
#if 1
				if (s > 0) {
					y = 3 * (g->pits[g->nhd] - 18) + 6;
					if (y < g->ymx)
						y = g->ymx;
					y -= a * g->x + addy;
					if (y > h)
						h = y;
				} else {
					y = 3 * (g->pits[0] - 18) - 6;
					if (y > g->ymn)
						y = g->ymn;
					y -= a * g->x + addy;
					if (y < h)
						h = y;
				}
#else
				y = g->y - a * k->x - addy;
				if (s > 0) {
					y += GSTEM + 2;
					if (y > h)
						h = y;
				} else {
					y -= 2;
					if (y < h)
						h = y;
				}
#endif
			}
			break;
		    }
		}
	    }
	    y1 += .45 * h;
	    y2 += .45 * h;
	    h *= .65;
	}

	if (nn > 3)
		height = (.08 * (x2 - x1) + 12) * s;
	else
		height = (.03 * (x2 - x1) + 8) * s;
	if (s > 0) {
		if (height < 3 * h)
			height = 3 * h;
		if (height > 40)
			height = 40;
	} else {
		if (height > 3 * h)
			height = 3 * h;
		if (height < -40)
			height = -40;
	}

	y = y2 - y1;
	if (y < 0)
		y = -y;
	if (s > 0) {
		if (height < .8 * y)
			height = .8 * y;
	} else {
		if (height > -.8 * y)
			height = -.8 * y;
	}
	height *= cfmt.slurheight;

/*fixme: ugly!*/
	if (m1 >= 0)
		y1 = (float) (3 * (k1->pits[m1] - 18) + 5 * s);
	if (m2 >= 0)
		y2 = (float) (3 * (k2->pits[m2] - 18) + 5 * s);

	slur_out(x1, y1, x2, y2, s, height, slur_type & SL_DOTTED, upstaff);

	/* have room for other symbols */
	dx = x2 - x1;
	a = (y2 - y1) / dx;
/*fixme: it seems to work with .4, but why?*/
	addy = y1 - a * x1 + .4 * height;
	for (k = k1; k != k2; k = k->next) {
		if (k->staff != upstaff)
			continue;
		y = a * k->x + addy;
		if (k->ymx < y)
			k->ymx = y;
		else if (k->ymn > y)
			k->ymn = y;
		if (k->next == k2) {
			dx = x2;
			if (k2->sflags & S_SL1)
				dx -= 5;
		} else {
			dx = k->next->x;
		}
		if (k != k1)
			x1 = k->x;
		dx -= x1;
		y_set(upstaff, s > 0, x1, dx, y);
	}
	return (s > 0 ? SL_ABOVE : SL_BELOW) | (slur_type & SL_DOTTED);
}

/* -- draw the slurs between 2 symbols --*/
static void draw_slurs(struct SYMBOL *first,
		       struct SYMBOL *last)
{
	struct SYMBOL *s, *s1, *k, *gr1, *gr2;
	int i, m1, m2, gr1_out, slur_type, cont;

	gr1 = gr2 = NULL;
	s = first;
	for (;;) {
		if (!s || s == last) {
			if (!gr1
			 || !(s = gr1->next)
			 || s == last)
				break;
			gr1 = NULL;
		}
		if (s->type == GRACE) {
			gr1 = s;
			s = s->extra;
			continue;
		}
		if ((s->type != NOTEREST && s->type != SPACE)
		 || (s->as.u.note.slur_st == 0
			&& !(s->sflags & S_SL1))) {
			s = s->next;
			continue;
		}
		k = NULL;		/* find matching slur end */
		s1 = s->next;
		gr1_out = 0;
		for (;;) {
			if (!s1) {
				if (gr2) {
					s1 = gr2->next;
					gr2 = NULL;
					continue;
				}
				if (!gr1 || gr1_out)
					break;
				s1 = gr1->next;
				gr1_out = 1;
				continue;
			}
			if (s1->type == GRACE) {
				gr2 = s1;
				s1 = s1->extra;
				continue;
			}
			if (s1->type == BAR
			 && ((s1->sflags & S_RRBAR)
			  || s1->as.u.bar.type == B_THIN_THICK
			  || s1->as.u.bar.type == B_THICK_THIN
			  || (s1->as.u.bar.repeat_bar
			   && s1->as.text
			   && s1->as.text[0] != '1'))) {
				k = s1;
				break;
			}
			if (s1->type != NOTEREST && s1->type != SPACE) {
				s1 = s1->next;
				continue;
			}
			if (s1->as.u.note.slur_end
			 || (s1->sflags & S_SL2)) {
				k = s1;
				break;
			}
			if (s1->as.u.note.slur_st
			 || (s1->sflags & S_SL1)) {
				if (gr2) {	/* if in grace note sequence */
					for (k = s1; k->next; k = k->next)
						;
					k->next = gr2->next;
					if (gr2->next)
						gr2->next->prev = k;
//					gr2->as.u.note.slur_st = SL_AUTO;
					k = NULL;
				}
				draw_slurs(s1, last);
				if (gr2
				 && gr2->next) {
					gr2->next->prev->next = NULL;
					gr2->next->prev = gr2;
				}
			}
			if (s1 == last)
				break;
			s1 = s1->next;
		}
		if (!s1) {
			k = next_scut(s);
		} else if (!k) {
			s = s1;
			if (s == last)
				break;
			continue;
		}

		/* if slur in grace note sequence, change the linkages */
		if (gr1) {
			for (s1 = s; s1->next; s1 = s1->next)
				;
			s1->next = gr1->next;
			if (gr1->next)
				gr1->next->prev = s1;
			gr1->as.u.note.slur_st = SL_AUTO;
		}
		if (gr2) {
			gr2->prev->next = gr2->extra;
			gr2->extra->prev = gr2->prev;
			gr2->as.u.note.slur_st = SL_AUTO;
		}
		if (s->as.u.note.slur_st) {
			slur_type = s->as.u.note.slur_st & 0x07;
			s->as.u.note.slur_st >>= 3;
			m1 = -1;
		} else {
			for (m1 = 0; m1 <= s->nhd; m1++)
				if (s->as.u.note.sl1[m1])
					break;
			slur_type = s->as.u.note.sl1[m1] & 0x07;
			s->as.u.note.sl1[m1] >>= 3;
			if (s->as.u.note.sl1[m1] == 0) {
				for (i = m1 + 1; i <= s->nhd; i++)
					if (s->as.u.note.sl1[i])
						break;
				if (i > s->nhd)
					s->sflags &= ~S_SL1;
			}
		}
		m2 = -1;
		cont = 0;
		if ((k->type == NOTEREST || k->type == SPACE)
		  && (k->as.u.note.slur_end
		   || (k->sflags & S_SL2))) {
			if (k->as.u.note.slur_end) {
				k->as.u.note.slur_end--;
			} else {
				for (m2 = 0; m2 <= k->nhd; m2++)
					if (k->as.u.note.sl2[m2])
						break;
				k->as.u.note.sl2[m2]--;
				if (k->as.u.note.sl2[m2] == 0) {
					for (i = m2 + 1; i <= k->nhd; i++)
						if (k->as.u.note.sl2[i])
							break;
					if (i > k->nhd)
						k->sflags &= ~S_SL2;
				}
			}
		} else {
			if (k->type != BAR
			 || (!(k->sflags & S_RRBAR)
			  && k->as.u.bar.type != B_THIN_THICK
			  && k->as.u.bar.type != B_THICK_THIN
			  && (!k->as.u.bar.repeat_bar
			   || !k->as.text
			   || k->as.text[0] == '1')))
				cont = 1;
		}
		slur_type = draw_slur(s, k, m1, m2, slur_type);
		if (cont) {
/*fixme: the slur types are inverted*/
			voice_tb[k->voice].slur_st <<= 3;
			voice_tb[k->voice].slur_st += slur_type;
		}

		/* if slur in grace note sequence, restore the linkages */
		if (gr1
		 && gr1->next) {
			gr1->next->prev->next = NULL;
			gr1->next->prev = gr1;
		}
		if (gr2) {
			gr2->prev->next = gr2;
			gr2->extra->prev = NULL;
		}

		if (s->as.u.note.slur_st
		 || (s->sflags & S_SL1))
			continue;
		if (s == last)
			break;
		s = s->next;
	}
}

/* -- draw a tuplet -- */
/* (the staves are not yet defined) */
/* See 'tuplets' in format.txt about the value of 'u' */
static struct SYMBOL *draw_tuplet(struct SYMBOL *t,	/* tuplet in extra */
				  struct SYMBOL *s)	/* main note */
{
	struct SYMBOL *s1, *s2, *sy, *next, *g;
	int r, upstaff, nb_only, some_slur;
	float x1, x2, y1, y2, xm, ym, a, s0, yy, yx, dy;

	next = s;
	if ((t->u & 0x0f00) == 0x100)		/* if 'when' == never */
		return next;

	/* treat the nested tuplets starting on this symbol */
	for (g = t->next; g; g = g->next) {
		if (g->type == TUPLET) {
			sy = draw_tuplet(g, s);
			if (sy->time > next->time)
				next = sy;
		}
	}

	/* search the first and last notes/rests of the tuplet */
	r = t->as.u.tuplet.r_plet;
	s1 = NULL;
	some_slur = 0;
	upstaff = s->staff;
	for (s2 = s; s2; s2 = s2->next) {
		if (s2 != s) {
			for (g = s2->extra; g; g = g->next) {
				if (g->type == TUPLET) {
					sy = draw_tuplet(g, s2);
					if (sy->time > next->time)
						next = sy;
				}
			}
		}
		if (s2->type != NOTEREST) {
			if (s2->type == GRACE) {
				for (g = s2->extra; g; g = g->next) {
					if (g->type != NOTEREST)
						continue;
					if (g->as.u.note.slur_st
					 || (g->sflags & S_SL1))
						some_slur = 1;
				}
			}
			continue;
		}
		if (s2->as.u.note.slur_st	/* if slur start/end */
		 || s2->as.u.note.slur_end
		 || (s2->sflags & (S_SL1 | S_SL2)))
			some_slur = 1;
		if (s2->staff < upstaff)
			upstaff = s2->staff;
		if (!s1)
			s1 = s2;
		if (--r <= 0)
			break;
	}
	if (!s2)
		return next;			/* no solution... */
	if (s2->time > next->time)
		next = s2;

//-- ici test
#if 0
	/* draw the slurs when inside the tuplet */
	if (some_slur) {
		draw_slurs(s1, s2);
		if (s1->as.u.note.slur_st
		 || (s1->sflags & S_SL1))
			return next;
		for (sy = s1->next; sy != s2; sy = sy->next) {
			if (sy->as.u.note.slur_st	/* if slur start/end */
			 || sy->as.u.note.slur_end
			 || (sy->sflags & (S_SL1 | S_SL2)))
				return next;		/* don't draw now */
		}
		if (s2->as.u.note.slur_end
		 || (s2->sflags & S_SL2))
			return next;
	}
#endif

	if (s1 == s2) {				/* tuplet with 1 note (!) */
		nb_only = 1;
	} else if ((t->u & 0x0f0) == 0x10) {	/* 'what' == slur */
		nb_only = 1;
		draw_slur(s1, s2, -1, -1, 
			  s1->stem > 0 ? SL_ABOVE : SL_BELOW);
	} else {

		/* search if a bracket is needed */
		if ((t->u & 0x0f00) == 0x200	/* if 'when' == always */
		 || s1->as.type != ABC_T_NOTE || s2->as.type != ABC_T_NOTE) {
			nb_only = 0;
		} else {
			nb_only = 1;
			for (sy = s1; ; sy = sy->next) {
				if (sy->type != NOTEREST) {
					if (sy->type == GRACE
					 || sy->type == SPACE)
						continue;
					nb_only = 0;
					break;
				}
				if (sy == s2)
					break;
				if (sy->sflags & S_BEAM_END) {
					nb_only = 0;
					break;
				}
			}
			if (nb_only
			 && !(s1->sflags & (S_BEAM_ST | S_BEAM_BR1 | S_BEAM_BR2))) {
				for (sy = s1->prev; sy; sy = sy->prev) {
					if (sy->type == NOTEREST) {
						if (sy->nflags >= s1->nflags)
							nb_only = 0;
						break;
					}
				}
			}
			if (nb_only && !(s2->sflags & S_BEAM_END)) {
				for (sy = s2->next; sy; sy = sy->next) {
					if (sy->type == NOTEREST) {
						if (!(sy->sflags & (S_BEAM_BR1 | S_BEAM_BR2))
						 && sy->nflags >= s2->nflags)
							nb_only = 0;
						break;
					}
				}
			}
		}
	}

	/* if number only, draw it */
	if (nb_only) {
		float a, b;

		if ((t->u & 0x0f) == 1)		/* if 'value' == none */
			return next;
		xm = (s2->x + s1->x) * .5;
		if (s1 == s2)			/* tuplet with 1 note */
			a = 0;
		else
			a = (s2->ys - s1->ys) / (s2->x - s1->x);
		b = s1->ys - a * s1->x;
		yy = a * xm + b;
		if (s1->stem > 0) {
			ym = y_get(s1->staff, 1, xm - 3, 6);
			if (ym > yy)
				b += ym - yy;
			b += 4;
		} else {
			ym = y_get(s1->staff, 0, xm - 3, 6);
			if (ym < yy)
				b += ym - yy;
			b -= 12;
		}
		for (sy = s1; ; sy = sy->next) {
			if (sy->x >= xm)
				break;
		}
		if (s1->stem * s2->stem > 0) {
			if (s1->stem > 0)
				xm += GSTEM_XOFF;
			else
				xm -= GSTEM_XOFF;
		}
		ym = a * xm + b;
		if ((t->u & 0x0f) == 0)		/* if 'value' == number */
			a2b("(%d)", t->as.u.tuplet.p_plet);
		else
			a2b("(%d:%d)", t->as.u.tuplet.p_plet,
			     t->as.u.tuplet.q_plet);
		putxy(xm, ym);
		a2b("y%d bnum\n", s1->staff);

		if (s1->stem > 0) {
			ym += 8;
			if (sy->ymx < ym)
				sy->ymx = (short) ym;
			y_set(s1->staff, 1, xm - 3, 6, ym);
		} else {
			if (sy->ymn > ym)
				sy->ymn = (short) ym;
			y_set(s1->staff, 0, xm - 3, 6, ym);
		}
		s->sflags &= ~S_IN_TUPLET;		/* the tuplet is drawn */
		return next;
	}

//-- ici test
#if 1
	/* draw the slurs when inside the tuplet */
	if (some_slur) {
		draw_slurs(s1, s2);
		if (s1->as.u.note.slur_st
		 || (s1->sflags & S_SL1))
			return next;
		for (sy = s1->next; sy != s2; sy = sy->next) {
			if (sy->as.u.note.slur_st	/* if slur start/end */
			 || sy->as.u.note.slur_end
			 || (sy->sflags & (S_SL1 | S_SL2)))
				return next;		/* don't draw now */
		}
//-- ici
		/* don't draw the tuplet when a slur ends on the last note */
		if (s2->as.u.note.slur_end
		 || (s2->sflags & S_SL2))
			return next;
	}
#endif
	if ((t->u & 0x0f0) != 0)	/* if 'what' != square */
		fprintf(stderr, "'what' value of %%%%tuplets not yet coded\n");

/*fixme: two staves not treated*/
/*fixme: to optimize*/
    if (s1->multi >= 0) {

	/* sole or upper voice: the bracket is above the staff */
	x1 = s1->x - 4;
	y1 = 24;
	if (s1->staff == upstaff) {
		sy = s1;
		if (sy->as.type != ABC_T_NOTE) {
			for (sy = sy->next; sy != s2; sy = sy->next)
				if (sy->as.type == ABC_T_NOTE)
					break;
		}
		ym = y_get(upstaff, 1, sy->x, 0);
		if (ym > y1)
			y1 = ym;
		if (s1->stem > 0)
			x1 += 3;
	}
	y2 = 24;
	if (s2->staff == upstaff) {
		sy = s2;
		if (sy->as.type != ABC_T_NOTE) {
			for (sy = sy->prev; sy != s1; sy = sy->prev)
				if (sy->as.type == ABC_T_NOTE)
					break;
		}
		ym = y_get(upstaff, 1, sy->x, 0);
		if (ym > y2)
			y2 = ym;
	}

	/* end the backet according to the last note duration */
#if 1
	if (s2->dur > s2->prev->dur) {
		if (s2->next)
			x2 = s2->next->x - s2->next->wl - 5;
		else
			x2 = realwidth - 6;
	} else {
		x2 = s2->x + 4;
		r = s2->stem >= 0 ? 0 : s2->nhd;
		if (s2->shhd[r] > 0)
			x2 += s2->shhd[r];
		if (s2->staff == upstaff
		 && s2->stem > 0)
			x2 += 3.5;
	}
#else
	if (s2->next)
		x2 += (s2->next->x - s2->next->wl - s2->x - s2->wr) * 0.5;
	else
		x2 += (realwidth - s2->x) * 0.5;
#endif

	xm = .5 * (x1 + x2);
	ym = .5 * (y1 + y2);

	a = (y2 - y1) / (x2 - x1);
	s0 = 3 * (s2->pits[s2->nhd] - s1->pits[s1->nhd]) / (x2 - x1);
	if (s0 > 0) {
		if (a < 0)
			a = 0;
		else if (a > s0)
			a = s0;
	} else {
		if (a > 0)
			a = 0;
		else if (a < s0)
			a = s0;
	}
	if (a * a < .1 * .1)
		a = 0;

	/* shift up bracket if needed */
	dy = 0;
	for (sy = s1; ; sy = sy->next) {
		if (sy->dur == 0	/* not a note or a rest */
		 || sy->staff != upstaff) {
			if (sy == s2)
				break;
			continue;
		}
		yy = ym + (sy->x - xm) * a;
		yx = y_get(upstaff, 1, sy->x, 0);
		if (yx - yy > dy)
			dy = yx - yy;
		if (sy == s2)
			break;
	}

	ym += dy + 4;
	y1 = ym + a * (x1 - xm);
	y2 = ym + a * (x2 - xm);
	putxy(x2 - x1, y2 - y1);
	putxy(x1, y1 + 4);
	a2b("y%d tubr", upstaff);

	/* shift the slurs / decorations */
	ym += 8;
	for (sy = s1; ; sy = sy->next) {
		if (sy->staff == upstaff) {
			yy = ym + (sy->x - xm) * a;
			if (sy->ymx < yy)
				sy->ymx = yy;
			if (sy == s2)
				break;
			y_set(upstaff, 1, sy->x, sy->next->x - sy->x, yy);
		} else if (sy == s2) {
			break;
		}
	}

    } else {	/* lower voice of the staff: the bracket is below the staff */
/*fixme: think to all that again..*/
	x1 = s1->x - 7;

#if 1
	if (s2->dur > s2->prev->dur) {
		if (s2->next)
			x2 = s2->next->x - s2->next->wl - 8;
		else
			x2 = realwidth - 6;
	} else {
		x2 = s2->x + 2;
		if (s2->shhd[s2->nhd] > 0)
			x2 += s2->shhd[s2->nhd];
	}
#else
	if (s2->next)
		x2 += (s2->next->x - s2->next->wl - s2->x - s2->wr) * 0.5;
	else
		x2 += (realwidth - s2->x) * 0.5;
#endif

	if (s1->staff == upstaff) {
		sy = s1;
		if (sy->as.type != ABC_T_NOTE) {
			for (sy = sy->next; sy != s2; sy = sy->next)
				if (sy->as.type == ABC_T_NOTE)
					break;
		}
		y1 = y_get(upstaff, 0, sy->x, 0);
	} else {
		y1 = 0;
	}
	if (s2->staff == upstaff) {
		sy = s2;
		if (sy->as.type != ABC_T_NOTE) {
			for (sy = sy->prev; sy != s1; sy = sy->prev)
				if (sy->as.type == ABC_T_NOTE)
					break;
		}
		y2 = y_get(upstaff, 0, sy->x, 0);
	} else {
		y2 = 0;
	}

	xm = .5 * (x1 + x2);
	ym = .5 * (y1 + y2);

	a = (y2 - y1) / (x2 - x1);
	s0 = 3 * (s2->pits[0] - s1->pits[0]) / (x2 - x1);
	if (s0 > 0) {
		if (a < 0)
			a = 0;
		else if (a > s0)
			a = s0;
	} else {
		if (a > 0)
			a = 0;
		else if (a < s0)
			a = s0;
	}
	if (a * a < .1 * .1)
		a = 0;

	/* shift down bracket if needed */
	dy = 0;
	for (sy = s1; ; sy = sy->next) {
		if (sy->dur == 0	/* not a note nor a rest */
		 || sy->staff != upstaff) {
			if (sy == s2)
				break;
			continue;
		}
		yy = ym + (sy->x - xm) * a;
		yx = y_get(upstaff, 0, sy->x, 0);
		if (yx - yy < dy)
			dy = yx - yy;
		if (sy == s2)
			break;
	}

	ym += dy - 12;
	y1 = ym + a * (x1 - xm);
	y2 = ym + a * (x2 - xm);
	putxy(x2 - x1, y2 - y1);
	putxy(x1, y1 + 4);
	a2b("y%d tubrl",upstaff);

	/* shift the slurs / decorations */
	ym -= 8;
	for (sy = s1; ; sy = sy->next) {
		if (sy->staff == upstaff) {
			if (sy == s2)
				break;
			yy = ym + (sy->x - xm) * a;
			if (sy->ymn > yy)
				sy->ymn = (short) yy;
			y_set(upstaff, 0, sy->x, sy->next->x - sy->x, yy);
		}
		if (sy == s2)
			break;
	}
    } /* lower voice */

	if ((t->u & 0x0f) == 1) {	/* if 'value' == none */
		a2b("%%tuplet\n");
		s->sflags &= ~S_IN_TUPLET;
		return next;
	}
	yy = .5 * (y1 + y2);
	if ((t->u & 0x0f) == 0)		/* if 'value' == number */
		a2b("(%d)", t->as.u.tuplet.p_plet);
	else
		a2b("(%d:%d)", t->as.u.tuplet.p_plet,
		     t->as.u.tuplet.q_plet);
	putxy(xm, yy);
	a2b("y%d bnumb\n", upstaff);
	s->sflags &= ~S_IN_TUPLET;
	return next;
}

/* -- draw the ties between two notes/chords -- */
static void draw_note_ties(struct SYMBOL *k1,
			   struct SYMBOL *k2,
			   int ntie,
			   int *mhead1,
			   int *mhead2,
			   int job)
{
	int i, s, m1, m2, p, p1, p2, y, staff;
	float x1, x2, h, sh;

	for (i = 0; i < ntie; i++) {
		m1 = mhead1[i];
		p1 = k1->pits[m1];
		m2 = mhead2[i];
		p2 = k2->pits[m2];
		if ((k1->as.u.note.ti1[m1] & 0x03) == SL_ABOVE)
			s = 1;
		else
			s = -1;

		x1 = k1->x;
		sh = k1->shhd[m1];		/* head shift */
		if (s > 0) {
			if (m1 < k1->nhd && k1->pits[m1] + 1 == k1->pits[m1 + 1])
				if (k1->shhd[m1 + 1] > sh)
					sh = k1->shhd[m1 + 1];
		} else {
			if (m1 > 0 && k1->pits[m1] == k1->pits[m1 - 1] + 1)
				if (k1->shhd[m1 - 1] > sh)
					sh = k1->shhd[m1 - 1];
		}
		x1 += sh;

		x2 = k2->x;
		sh = k2->shhd[m2];
		if (s > 0) {
			if (m2 < k2->nhd && k2->pits[m2] + 1 == k2->pits[m2 + 1])
				if (k2->shhd[m2 + 1] < sh)
					sh = k2->shhd[m2 + 1];
		} else {
			if (m2 > 0 && k2->pits[m2] == k2->pits[m2 - 1] + 1)
				if (k2->shhd[m2 - 1] < sh)
					sh = k2->shhd[m2 - 1];
		}
		x2 += sh;

		staff = k1->staff;
		switch (job) {
		case 0:
			if (p1 == p2 || (p1 & 1))
				p = p1;
			else
				p = p2;
			break;
		case 1:				/* no starting note */
		case 3:				/* clef or staff change */
			x1 = k1->x;
			if (x1 > x2 - 20)
				x1 = x2 - 20;
			p = p2;
			staff = k2->staff;
			if (job == 3)
				s = -s;
			break;
/*		case 2:				 * no ending note */
		default:
			if (k1 != k2) {
				x2 -= k2->wl;
			} else {
				struct SYMBOL *k;

				for (k = k2->ts_next; k; k = k->ts_next)
					if (k->type == STAVES)
						break;
				if (!k)
					x2 = realwidth;
				else
					x2 = k->x;
			}
			if (x2 < x1 + 16)
				x2 = x1 + 16;
			p = p1;
			break;
		}
		if (x2 - x1 > 20) {
			x1 += 2;
			x2 -= 2;
		}

		y = 3 * (p - 18);
		if (job != 1 && job != 3) {
			if (k1->nhd != 0) {
				x1 += 4.5;
				y += ((p & 1) ? 2 : 0) * s;
			} else {
				y += ((p & 1) ? 6 : 4) * s;
			}
			if (s > 0) {
				if (k1->nflags > -2 && k1->stem > 0
				 && k1->nhd == 0)
					x1 += 4.5;
				if (!(p & 1) && k1->dots > 0)
					y = 3 * (p - 18) + 6;
			}
		}
//		if (job != 2) {
		 else {
			if (k2->nhd != 0) {
				x2 -= 4.5;
				y += ((p & 1) ? 2 : 0) * s;
			} else {
				y += ((p2 & 1) ? 7 : 4) * s;
			}
			if (s < 0) {
				if (k2->nflags > -2 && k2->stem < 0
				 && k2->nhd == 0)
					x2 -= 4.5;
			}
//			if (job != 0)
//				y1 = y2;
//		} else {
//			if (k1 == k2) {		/* if continuation on next line */
//				k1->as.u.note.ti1[m1] &= SL_DOTTED;
//				k1->as.u.note.ti1[m1] +=
//					s > 0 ? SL_ABOVE : SL_BELOW;
//			}
		}

		h = (.04 * (x2 - x1) + 10) * s;
		slur_out(x1, staff_tb[staff].y + y,
			 x2, staff_tb[staff].y + y,
			 s, h, k1->as.u.note.ti1[m1] & SL_DOTTED, -1);
	}
}

/* -- draw ties between neighboring notes/chords -- */
static void draw_ties(struct SYMBOL *k1,
		      struct SYMBOL *k2,
		      int job)		/* 0: normal
					 * 1: no starting note
					 * 2: no ending note
					 * 3: no start for clef or staff change */
{
	struct SYMBOL *k3;
	int i, m1, nh1, pit, ntie, tie2, ntie3, time;
	int mhead1[MAXHD], mhead2[MAXHD], mhead3[MAXHD];

	/* special cases for ties to/from a grace note */
	if (k1->type == GRACE) {
		k3 = k1->extra;
		while (k3) {
			if (k3->type == NOTEREST)
				k1 = k3;	/* last grace note */
			k3 = k3->next;
		}
	}
	if (k2->type == GRACE) {
		k3 = k2->extra;
		while (k3) {
			if (k3->type == NOTEREST) {
				k2 = k3;	/* first grace note */
				break;
			}
			k3 = k3->next;
		}
	}

	ntie = ntie3 = 0;
	nh1 = k1->nhd;
	time = k1->time + k1->dur;

	/* half ties from last note in line or before new repeat */
	if (job == 2) {
		for (i = 0; i <= nh1; i++) {
			if (k1->as.u.note.ti1[i])
				mhead3[ntie3++] = i;
		}
		draw_note_ties(k1, k2, ntie3, mhead3, mhead3, job);
		return;
	}

	/* set up list of ties to draw */
	for (i = 0; i <= nh1; i++) {
		if (k1->as.u.note.ti1[i] == 0)
			continue;
		tie2 = -1;
		pit = k1->as.u.note.pits[i];
		for (m1 = k2->nhd; m1 >= 0; m1--) {
			switch (k2->as.u.note.pits[m1] - pit) {
			case 1:			/* maybe ^c - _d */
			case -1:		/* _d - ^c */
				if (k1->as.u.note.accs[i] != k2->as.u.note.accs[m1])
					tie2 = m1;
				break;
			case 0:
				mhead1[ntie] = i;
				mhead2[ntie++] = m1;
				goto found;
			}
		}
		if (tie2 >= 0) {		/* second choice */
			mhead1[ntie] = i;
			mhead2[ntie++] = tie2;
		} else {
			mhead3[ntie3++] = i;	/* no match */
		}
found:
		;
	}

	/* draw the ties */
	draw_note_ties(k1, k2, ntie, mhead1, mhead2, job);

	/* if any bad tie, try an other voice of the same staff */
	if (ntie3 == 0)
		return;				/* no bad tie */
	k3 = k1->ts_next;
	while (k3 && k3->time < time)
		k3 = k3->ts_next;
	while (k3 && k3->time == time) {
		if (k3->as.type != ABC_T_NOTE
		 || k3->staff != k1->staff) {
			k3 = k3->ts_next;
			continue;
		}
		ntie = 0;
		for (i = ntie3; --i >= 0; ) {
			pit = k1->as.u.note.pits[mhead3[i]];
			for (m1 = k3->nhd; m1 >= 0; m1--) {
				if (k3->as.u.note.pits[m1] == pit) {
					mhead1[ntie] = mhead3[i];
					mhead2[ntie++] = m1;
					ntie3--;
					mhead3[i] = mhead3[ntie3];
					break;
				}
			}
		}
		if (ntie > 0) {
			draw_note_ties(k1, k3,
					ntie, mhead1, mhead2,
					job == 1 ? 1 : 0);
			if (ntie3 == 0)
				return;
		}
		k3 = k3->ts_next;
	}

	if (ntie3 != 0)
		error(1, k1, "Bad tie");
}

/* -- draw all ties between neighboring notes -- */
static void draw_all_ties(struct VOICE_S *p_voice)
{
	struct SYMBOL *s1, *s2, *rtie;
	struct SYMBOL tie;
	int clef_chg;

	for (s1 = p_voice->sym->next; s1; s1 = s1->next)
		if (s1->type != KEYSIG && s1->type != TIMESIG)
			break;
	rtie = p_voice->rtie;			/* tie from 1st repeat bar */
	for (s2 = s1; s2; s2 = s2->next) {
		if (s2->as.type == ABC_T_NOTE
		 || s2->type == GRACE)
			break;
		if (s2->type != BAR
		 || !s2->as.u.bar.repeat_bar
		 || !s2->as.text)
			continue;
		if (s2->as.text[0] == '1')	/* 1st repeat bar */
			rtie = p_voice->tie;
		else
			p_voice->tie = rtie;
	}
	if (!s2)
		return;
	if (p_voice->tie) {			/* tie from previous line */
		p_voice->tie->x = s1->x + s1->wr;
		s1 = p_voice->tie;
		p_voice->tie = 0;
		s1->staff = s2->staff;
		s1->ts_next = tsfirst->next;	/* (for tie to other voice) */
		s1->time = s2->time - s1->dur;	/* (if after repeat sequence) */
		draw_ties(s1, s2, 1);		/* tie to 1st note */
	}

	/* search the start of ties */
	clef_chg = 0;
	for (;;) {
		for (s1 = s2; s1; s1 = s1->next) {
			if (s1->sflags & S_TI1)
				break;
			if (rtie == 0)
				continue;
			if (s1->type != BAR
			 || !s1->as.u.bar.repeat_bar
			 || !s1->as.text)
				continue;
			if (s1->as.text[0] == '1') {	/* 1st repeat bar */
				rtie = 0;
				continue;
			}
			for (s2 = s1->next; s2; s2 = s2->next)
				if (s2->as.type == ABC_T_NOTE)
					break;
			if (!s2) {
				s1 = NULL;
				break;
			}
			memcpy(&tie, rtie, sizeof tie);
			tie.x = s1->x + s1->wr;
			tie.next = s2;
			tie.staff = s2->staff;
			tie.time = s2->time - tie.dur;
			draw_ties(&tie, s2, 1);
		}
		if (!s1)
			break;

		/* search the end of the tie
		 * and notice the clef changes */
		for (s2 = s1->ts_next; s2; s2 = s2->ts_next) {
			if (s2->staff != s1->staff
			 && s2->voice != s1->voice)
				continue;
			if (s2->type == CLEF) {
				clef_chg = 1;
				continue;
			}
			if (s2->voice != s1->voice)
				continue;
			if (s2->as.type == ABC_T_NOTE) {
				if (s2->time != s1->time + s1->dur)
					s2 = NULL;	/* %%combinevoices */
				break;
			}
			if (s2->type == GRACE)
				break;
			if (s2->type == BAR) {
				if ((s2->sflags & S_RRBAR)
				 || s2->as.u.bar.type == B_THIN_THICK
				 || s2->as.u.bar.type == B_THICK_THIN)
					break;
				if (!s2->as.u.bar.repeat_bar
				 || !s2->as.text)
					continue;
				if (s2->as.text[0] != '1')
					break;
				rtie = s1;		/* 1st repeat bar */
			}
		}
		if (!s2) {

			/* special case: tie to a combined chord */
			if (s1->ts_prev && s1->ts_prev->next
			 && s1->ts_prev->next->type == ABC_T_NOTE
			 && s1->ts_prev->next->time == s1->time + s1->dur) {
				draw_ties(s1, s1->ts_prev->next, 1);
				break;
			}
			draw_ties(s1, s1, 2);
			p_voice->tie = s1;
			break;
		}

		/* ties with clef or staff change */
		if (clef_chg || s1->staff != s2->staff) {
			float x, dx;

			clef_chg = 0;
			dx = (s2->x - s1->x) * 0.4;
			x = s2->x;
			s2->x -= dx;
			if (s2->x > s1->x + 32.)
				s2->x = s1->x + 32.;
			draw_ties(s1, s2, 2);
			s2->x = x;
			x = s1->x;
			s1->x += dx;
			if (s1->x < s2->x - 24.)
				s1->x = s2->x - 24.;
			draw_ties(s1, s2, 3);
			s1->x = x;
			continue;
		}
		draw_ties(s1, s2, s2->as.type == ABC_T_NOTE ? 0 : 2);
	}
	p_voice->rtie = rtie;
}

/* -- draw all phrasing slurs for one staff -- */
/* (the staves are not yet defined) */
static void draw_all_slurs(struct VOICE_S *p_voice)
{
	struct SYMBOL *s, *k;
	int i, m2, slur_type;
	unsigned char slur_st;

	s = p_voice->sym->next;
	if (!s)
		return;
	slur_type = p_voice->slur_st;
	p_voice->slur_st = 0;

	/* the starting slur types are inverted */
	slur_st = 0;
	while (slur_type != 0) {
		slur_st <<= 3;
		slur_st |= (slur_type & 0x07);
		slur_type >>= 3;
	}

	/* draw the slurs inside the music line */
	draw_slurs(s, NULL);

	/* do unbalanced slurs still left over */
	for ( ; s; s = s->next) {
		if (s->type != NOTEREST && s->type != SPACE)
			continue;
		while (s->as.u.note.slur_end
		    || (s->sflags & S_SL2)) {
			if (s->as.u.note.slur_end) {
				s->as.u.note.slur_end--;
				m2 = -1;
			} else {
				for (m2 = 0; m2 <= s->nhd; m2++)
					if (s->as.u.note.sl2[m2])
						break;
				s->as.u.note.sl2[m2]--;
				if (s->as.u.note.sl2[m2] == 0) {
					for (i = m2 + 1; i <= s->nhd; i++)
						if (s->as.u.note.sl2[i])
							break;
					if (i > s->nhd)
						s->sflags &= ~S_SL2;
				}
			}
			slur_type = slur_st & 0x07;
			k = prev_scut(s);
			draw_slur(k, s, -1, m2, slur_type);
			if (k->type != BAR
			 || (!(k->sflags & S_RRBAR)
			  && k->as.u.bar.type != B_THIN_THICK
			  && k->as.u.bar.type != B_THICK_THIN
			  && (!k->as.u.bar.repeat_bar
			   || !k->as.text
			   || k->as.text[0] == '1')))
				slur_st >>= 3;
		}
	}
	s = p_voice->sym->next;
	while (slur_st != 0) {
		slur_type = slur_st & 0x07;
		slur_st >>= 3;
		k = next_scut(s);
		draw_slur(s, k, -1, -1, slur_type);
		if (k->type != BAR
		 || (!(k->sflags & S_RRBAR)
		  && k->as.u.bar.type != B_THIN_THICK
		  && k->as.u.bar.type != B_THICK_THIN
		  && (!k->as.u.bar.repeat_bar
		   || !k->as.text
		   || k->as.text[0] == '1'))) {
/*fixme: the slur types are inverted again*/
			p_voice->slur_st <<= 3;
			p_voice->slur_st += slur_type;
		}
	}
}

/* -- work out accidentals to be applied to each note -- */
static void setmap(int sf,	/* number of sharps/flats in key sig (-7 to +7) */
		   unsigned char *map)	/* for 7 notes only */
{
	int j;

	for (j = 7; --j >= 0; )
		map[j] = A_NULL;
	switch (sf) {
	case 7: map[6] = A_SH;
	case 6: map[2] = A_SH;
	case 5: map[5] = A_SH;
	case 4: map[1] = A_SH;
	case 3: map[4] = A_SH;
	case 2: map[0] = A_SH;
	case 1: map[3] = A_SH;
		break;
	case -7: map[3] = A_FT;
	case -6: map[0] = A_FT;
	case -5: map[4] = A_FT;
	case -4: map[1] = A_FT;
	case -3: map[5] = A_FT;
	case -2: map[2] = A_FT;
	case -1: map[6] = A_FT;
		break;
	}
}

/* output a tablature string escaping the parenthesis */
static void tbl_out(char *s, float x, int j, char *f)
{
	char *p;

	a2b("(");
	p = s;
	for (;;) {
		while (*p != '\0' && *p != '(' && *p != ')' )
			p++;
		if (p != s) {
			a2b("%.*s", (int) (p - s), s);
			s = p;
		}
		if (*p == '\0')
			break;
		a2b("\\");
		p++;
	}
	a2b(")%.1f y %d %s ", x, j, f);
}

/* -- draw the tablature with w: -- */
static void draw_tblt_w(struct VOICE_S *p_voice,
			int nly,
			float y,
			struct tblt_s *tblt)
{
	struct SYMBOL *s;
	struct lyrics *ly;
	struct lyl *lyl;
	char *p;
	int j, l;

	a2b("/y{%.1f y%d}def ", y, p_voice->staff);
	set_font(VOCALFONT);
	a2b("%.1f 0 y %d %s\n", realwidth, nly, tblt->head);
	for (j = 0; j < nly ; j++) {
		for (s = p_voice->sym->next; s; s = s->next) {
			ly = s->ly;
			if (!ly
			 || (lyl = ly->lyl[j]) == NULL) {
				if (s->type == BAR) {
					if (tblt->bar == 0)
						continue;
					p = &tex_buf[16];
					*p-- = '\0';
					l = bar_cnv(s->as.u.bar.type);
					while (l != 0) {
						*p-- = "?|[]:???"[l & 0x07];
						l >>= 4;
					}
					p++;
					tbl_out(p, s->x, j, tblt->bar);
				}
				continue;
			}
			tbl_out(lyl->t, s->x, j, tblt->note);
		}
		a2b("\n");
	}
}

/* -- draw the tablature with automatic pitch -- */
static void draw_tblt_p(struct VOICE_S *p_voice,
			float y,
			struct tblt_s *tblt)
{
	struct SYMBOL *s;
	int j, pitch, octave, sf, tied, acc;
	unsigned char workmap[70];	/* sharps/flats - base: lowest 'C' */
	unsigned char basemap[7];
	static int scale[7] = {0, 2, 4, 5, 7, 9, 11};	/* index = natural note */
	static int acc_pitch[6] = {0, 1, 0, -1, 2, -2};	/* index = enum accidentals */

	sf = p_voice->key.sf;
	setmap(sf, basemap);
	for (j = 0; j < 10; j++)
		memcpy(&workmap[7 * j], basemap, 7);
	a2b("gsave 0 %.1f y%d T(%.2s)%s\n",
		y, p_voice->staff,
		tblt->instr, tblt->head);
	tied = 0;
	for (s = p_voice->sym; s; s = s->next) {
		switch (s->type) {
		case NOTEREST:
			if (s->as.type == ABC_T_REST)
				continue;
			if (tied) {
				tied = s->as.u.note.ti1[0];
				continue;
			}
			break;
		case KEYSIG:
			sf = s->as.u.key.sf;
			setmap(sf, basemap);
			for (j = 0; j < 10; j++)
				memcpy(&workmap[7 * j], basemap, 7);
			continue;
		case BAR:
			if (s->as.flags & ABC_F_INVIS)
				continue;
			for (j = 0; j < 10; j++)
				memcpy(&workmap[7 * j], basemap, 7);
			continue;
		default:
			continue;
		}
		pitch = s->as.u.note.pits[0] + 19;
		acc = s->as.u.note.accs[0];
		if (acc != 0) {
			workmap[pitch] = acc == A_NT
				? A_NULL
				: (acc & 0x07);
		}
		pitch = scale[pitch % 7]
			+ acc_pitch[workmap[pitch]]
			+ 12 * (pitch / 7)
			- tblt->pitch;
		octave = 0;
		while (pitch < 0) {
			pitch += 12;
			octave--;
		}
		while (pitch >= 36) {
			pitch -= 12;
			octave++;
		}
		if ((acc & 0xf8) == 0) {
			a2b("%d %d %.2f %s\n", octave, pitch, s->x, tblt->note);
		} else {
			int n, d;
			float micro_p;

			n = micro_tb[acc >> 3];
			d = (n & 0xff) + 1;
			n = (n >> 8) + 1;
			switch (acc & 0x07) {
			case A_FT:
			case A_DF:
				n = -n;
				break;
			}
			micro_p = (float) pitch + (float) n / d;
			a2b("%d %.3f %.2f %s\n", octave, micro_p, s->x, tblt->note);
		}
		tied = s->as.u.note.ti1[0];
	}
	a2b("grestore\n");
}

/* -- draw the lyrics under (or above) notes -- */
/* !! this routine is tied to set_width() !! */
static float draw_lyrics(struct VOICE_S *p_voice,
			 int nly,
			 float y,
			 int incr)	/* 1: below, -1: above */
{
	int hyflag, l, j, lflag;
	char *p;
	float lastx, w, lskip, desc;
	struct SYMBOL *s;
	struct FONTSPEC *f;
	struct lyrics *ly;
	struct lyl *lyl;

	/* check if the lyrics contain tablatures */
	if (p_voice->tblts[0] != 0) {
		if (p_voice->tblts[0]->pitch == 0)
			return y;		/* yes */
		if (p_voice->tblts[1] != 0
		 && p_voice->tblts[1]->pitch == 0)
			return y;		/* yes */
	}

	outft = -1;				/* force font output */
	lskip = 0;				/* (compiler warning) */
	f = 0;					/* (force new font) */
	if (incr > 0) {				/* under the staff */
		j = 0;
/*fixme: may not be the current font*/
		y -= cfmt.font_tb[VOCALFONT].size;
		if (y > -cfmt.vocalspace)
			y = -cfmt.vocalspace;
	} else {
		j = nly - 1;
		nly = -1;
		if (y < 24 + cfmt.vocalspace - cfmt.font_tb[VOCALFONT].size)
			y = 24 + cfmt.vocalspace - cfmt.font_tb[VOCALFONT].size;
	}
/*fixme: may not be the current font*/
	desc = cfmt.font_tb[VOCALFONT].size * .25;	/* descent */
	for (; j != nly ; j += incr) {
		float x0, shift;

		a2b("/y{%.1f y%d}! ", y + desc, p_voice->staff);
		hyflag = lflag = 0;
		if (p_voice->hy_st & (1 << j)) {
			hyflag = 1;
			p_voice->hy_st &= ~(1 << j);
		}
		for (s = p_voice->sym; /*s*/; s = s->next)
			if (s->type != CLEF
			 && s->type != KEYSIG && s->type != TIMESIG)
				break;
		if (s->prev)
			lastx = s->prev->x;
		else
			lastx = 0;
		x0 = 0;
		if (f != 0)
			lskip = f->size * 1.1;
		for ( ; s; s = s->next) {
			ly = s->ly;
			if (!ly
			 || (lyl = ly->lyl[j]) == NULL) {
				switch (s->type) {
				case NOTEREST:
					if (s->as.type == ABC_T_NOTE)
						break;
					/* fall thru */
				case MREST:
					if (lflag) {
						putx(x0 - lastx);
						putx(lastx + 3);
						a2b("y wln ");
						lflag = 0;
						lastx = s->x + s->wr;
					}
				}
				continue;
			}
			if (lyl->f != f) {		/* font change */
				f = lyl->f;
				str_font(f - cfmt.font_tb);
				if (lskip < f->size * 1.1)
					lskip = f->size * 1.1;
			}
			p = lyl->t;
			w = lyl->w;
			shift = lyl->s;
			if (hyflag) {
				if (*p == LY_UNDER) {		/* '_' */
					*p = LY_HYPH;
				} else if (*p != LY_HYPH) {	/* not '-' */
					putx(s->x - shift - lastx);
					putx(lastx);
					a2b("y hyph ");
					hyflag = 0;
					lastx = s->x + s->wr;
				}
			}
			if (lflag
			 && *p != LY_UNDER) {		/* not '_' */
				putx(x0 - lastx + 3);
				putx(lastx + 3);
				a2b("y wln ");
				lflag = 0;
				lastx = s->x + s->wr;
			}
			if (*p == LY_HYPH			/* '-' */
			 || *p == LY_UNDER) {		/* '_' */
				if (x0 == 0 && lastx > s->x - 18)
					lastx = s->x - 18;
				if (*p == LY_HYPH)
					hyflag = 1;
				else
					lflag = 1;
				x0 = s->x - shift;
				continue;
			}
			x0 = s->x - shift;
			l = strlen(p) - 1;
			if (p[l] == LY_HYPH) {		/* '-' at end */
				p[l] = '\0';
				hyflag = 1;
			}
			putx(x0);
			a2b("y M ");
			put_str(p, A_LYRIC);
			lastx = x0 + w;
		}
		if (hyflag) {
			x0 = realwidth - 10;
			if (x0 < lastx + 10)
				x0 = lastx + 10;
			putx(x0 - lastx);
			putx(lastx);
			a2b("y hyph ");
			if (cfmt.hyphencont)
				p_voice->hy_st |= (1 << j);
		}

		/* see if any underscore in the next line */
		for (s = tsnext; s; s = s->ts_next)
			if (s->voice == p_voice - voice_tb)
				break;
		for ( ; s; s = s->next) {
			if (s->as.type == ABC_T_NOTE) {
				if (s->ly && s->ly->lyl[j] != 0
				 && s->ly->lyl[j]->t[0] == LY_UNDER) {
					lflag = 1;
					x0 = realwidth - 15;
					if (x0 < lastx + 12)
						x0 = lastx + 12;
				}
				break;
			}
		}
		if (lflag) {
			putx(x0 - lastx + 3);
			putx(lastx + 3);
			a2b("y wln");
		}
		a2b("\n");
		if (incr > 0)
			y -= lskip;
		else
			y += lskip;
	}
	if (incr > 0)
		y += lskip;
	return y;
}

/* -- draw all the lyrics and the tablatures -- */
/* (the staves are not yet defined) */
static void draw_all_lyrics(void)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s;
	int staff, voice, nly, i;
	struct {
		short a, b;
		float top, bot;
	} lyst_tb[MAXSTAFF];
	char nly_tb[MAXVOICE];
	char above_tb[MAXVOICE];
	char rv_tb[MAXVOICE];
	float top, bot, y;

	/* check if any lyric */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		if (p_voice->have_ly
		 || p_voice->tblts[0] != 0)
			break;
	}
	if (!p_voice)
		return;

	/* compute the number of lyrics per voice - staff
	 * and their y offset on the staff */
	memset(above_tb, 0, sizeof above_tb);
	memset(nly_tb, 0, sizeof nly_tb);
	memset(lyst_tb, 0, sizeof lyst_tb);
	staff = -1;
	top = bot = 0;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		if (!p_voice->sym)
			continue;
		voice = p_voice - voice_tb;
		if (p_voice->staff != staff) {
			top = 0;
			bot = 0;
			staff = p_voice->staff;
		}
		nly = 0;
		if (p_voice->have_ly) {
			for (s = p_voice->sym; s; s = s->next) {
				struct lyrics *ly;
				float x, w;

				ly = s->ly;
				if (!ly)
					continue;
/*fixme:should get the real width*/
				x = s->x;
				if (ly->lyl[0] != 0) {
					x -= ly->lyl[0]->s;
					w = ly->lyl[0]->w;
				} else {
					w = 10;
				}
				y = y_get(p_voice->staff, 1, x, w);
				if (top < y)
					top = y;
				y = y_get(p_voice->staff, 0, x, w);
				if (bot > y)
					bot = y;
				for (i = MAXLY; --i >= 0; )
					if (ly->lyl[i] != 0)
						break;
				i++;
				if (i > nly)
					nly = i;
			}
		} else {
			y = y_get(p_voice->staff, 1, 0, realwidth);
			if (top < y)
				top = y;
			y = y_get(p_voice->staff, 0, 0, realwidth);
			if (bot > y)
				bot = y;
		}
		lyst_tb[staff].top = top;
		lyst_tb[staff].bot = bot;
		if (nly == 0)
			continue;
		nly_tb[voice] = nly;
		if (p_voice->posit.voc != 0)
			above_tb[voice] = p_voice->posit.voc == SL_ABOVE;
		else if (p_voice->next
/*fixme:%%staves:KO - find an other way..*/
		      && p_voice->next->staff == staff
		      && p_voice->next->have_ly)
			above_tb[voice] = 1;
		if (above_tb[voice])
			lyst_tb[staff].a = 1;
		else
			lyst_tb[staff].b = 1;
	}

	/* draw the lyrics under the staves */
	i = 0;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		struct tblt_s *tblt;

		if (!p_voice->sym)
			continue;
		if (!p_voice->have_ly
		 && p_voice->tblts[0] == 0)
			continue;
		voice = p_voice - voice_tb;
		if (above_tb[voice]) {
			rv_tb[i++] = voice;
			continue;
		}
		staff = p_voice->staff;
		set_sscale(staff);
		if (nly_tb[voice] > 0)
			lyst_tb[staff].bot = draw_lyrics(p_voice, nly_tb[voice],
							 lyst_tb[staff].bot, 1);
		for (nly = 0; nly < 2; nly++) {
			if ((tblt = p_voice->tblts[nly]) == 0)
				continue;
			if (tblt->hu > 0) {
				lyst_tb[staff].bot -= tblt->hu;
				lyst_tb[staff].b = 1;
			}
			if (tblt->pitch == 0)
				draw_tblt_w(p_voice, nly_tb[voice],
					lyst_tb[staff].bot, tblt);
			else
				draw_tblt_p(p_voice, lyst_tb[staff].bot,
					tblt);
			if (tblt->ha != 0) {
				lyst_tb[staff].top += tblt->ha;
				lyst_tb[staff].a = 1;
			}
		}
	}

	/* draw the lyrics above the staff */
	while (--i >= 0) {
		voice = rv_tb[i];
		p_voice = &voice_tb[voice];
		staff = p_voice->staff;
		set_sscale(staff);
		lyst_tb[staff].top = draw_lyrics(p_voice, nly_tb[voice],
						 lyst_tb[staff].top, -1);
	}

	/* set the max y offsets of all symbols */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		if (!p_voice->sym)
			continue;
		staff = p_voice->staff;
		set_sscale(staff);
		if (lyst_tb[staff].a) {
			top = lyst_tb[staff].top + 2;
			for (s = p_voice->sym->next; s; s = s->next) {
/*fixme: may have lyrics crossing a next symbol*/
				if (s->ly) {
/*fixme:should set the real width*/
					y_set(staff, 1, s->x - 2, 10, top);
				}
			}
		}
		if (lyst_tb[staff].b) {
			bot = lyst_tb[staff].bot - 2;
			if (nly_tb[p_voice - voice_tb] > 0) {
				for (s = p_voice->sym->next; s; s = s->next) {
					if (s->ly) {
/*fixme:should set the real width*/
						y_set(staff, 0, s->x - 2, 10, bot);
					}
				}
			} else {
				y_set(staff, 0, 0, realwidth, bot);
			}
		}
	}
}

/* -- draw the symbols near the notes -- */
/* (the staves are not yet defined) */
/* order:
 * - beams
 * - decorations near the notes
 * - measure bar numbers
 * - n-plets
 * - decorations tied to the notes
 * - slurs
 * - guitar chords
 * - then remaining decorations
 * The buffer output is delayed until the definition of the staff system,
 * so, global variables must be saved (see music.c delayed_output()).
 */
void draw_sym_near(void)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s;

	/* calculate the beams but don't draw them (the staves are not yet defined) */
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		struct BEAM bm;
		int first_note = 1;

		for (s = p_voice->sym; s; s = s->next) {
			if (s->as.type != ABC_T_NOTE)
				continue;
			if (((s->sflags & S_BEAM_ST) && !(s->sflags & S_BEAM_END))
			 || (first_note && !(s->sflags & S_BEAM_ST))) {
				first_note = 0;
				calculate_beam(&bm, s);
			}
		}
	}

	/* initialize the y offsets */
	{
		int i, staff;

		for (staff = 0; staff <= nstaff; staff++) {
			for (i = 0; i < YSTEP; i++) {
				staff_tb[staff].top[i] = 0;
				staff_tb[staff].bot[i] = 24;
			}
		}
	}

	set_tie_room();
	draw_deco_near();

	/* set the min/max vertical offsets */
	for (s = tsfirst; s; s = s->ts_next) {
		int y;
		struct SYMBOL *g;

//		if (!s->prev)
//			continue;		/* skip the clefs */
		if (s->as.flags & ABC_F_INVIS)
			continue;
		if (s->type == GRACE) {
			g = s->extra;
			for ( ; g; g = g->next) {
				y_set(s->staff, 1, g->x - g->wl, g->wl + g->wr,
						g->ymx + 1);
				y_set(s->staff, 0, g->x - g->wl, g->wl + g->wr,
						g->ymn - 1);
			}
			continue;
		}
		if (s->type != MREST) {
			y_set(s->staff, 1, s->x - s->wl, s->wl + s->wr, s->ymx + 2);
			y_set(s->staff, 0, s->x - s->wl, s->wl + s->wr, s->ymn - 2);
		} else {
			y_set(s->staff, 1, s->x - 16, 32, s->ymx + 2);
		}
		if (s->as.type != ABC_T_NOTE)
			continue;

		/* have room for the accidentals */
		if (s->as.u.note.accs[s->nhd]) {
			y = s->y + 8;
			if (s->ymx < y)
				s->ymx = y;
			y_set(s->staff, 1, s->x, 0., y);
		}
		if (s->as.u.note.accs[0]) {
			y = s->y;
			if ((s->as.u.note.accs[0] & 0x07) == A_SH
			 || s->as.u.note.accs[0] == A_NT)
				y -= 7;
			else
				y -= 5;
			if (s->ymn > y)
				s->ymn = y;
			y_set(s->staff, 0, s->x, 0., y);
		}
	}

	if (cfmt.measurenb >= 0)
		draw_measnb();

	draw_deco_note();

	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		s = p_voice->sym;
		if (!s)
			continue;
		set_sscale(s->staff);

		/* draw the tuplets near the notes */
		for (s = s->next; s; s = s->next) {
			struct SYMBOL *g;

			if ((s->sflags & S_IN_TUPLET)
			 && (g = s->extra) != NULL) {
				for ( ; g; g = g->next) {
					if (g->type == TUPLET) {
						s = draw_tuplet(g, s);
						break;
					}
				}
			}
		}
		draw_all_slurs(p_voice);

		/* draw the tuplets over the slurs */
		for (s = p_voice->sym->next; s; s = s->next) {
			struct SYMBOL *g;

			if ((s->sflags & S_IN_TUPLET)
			 && (g = s->extra) != NULL) {
				for ( ; g; g = g->next) {
					if (g->type == TUPLET) {
						s = draw_tuplet(g, s);
						break;
					}
				}
			}
		}
	}

	/* set the top and bottom for all symbols to be out of the staves */
	{
		int top, bot, i, staff;

		for (staff = 0; staff <= nstaff; staff++) {
			top = staff_tb[staff].topbar + 2;
			bot = staff_tb[staff].botbar - 2;
/*fixme:should handle stafflines changes*/
			for (i = 0; i < YSTEP; i++) {
				if (top > staff_tb[staff].top[i])
					staff_tb[staff].top[i] = (float) top;
				if (bot < staff_tb[staff].bot[i])
					staff_tb[staff].bot[i] = (float) bot;
			}
		}
	}
	draw_all_lyrics();
	draw_deco_staff();
	set_sscale(-1);		/* restore the scale parameters */
}

/* -- draw the name/subname of the voices -- */
static void draw_vname(float indent)
{
	struct VOICE_S *p_voice;
	int n, staff;
	struct {
		int nl;
		char *v[8];
	} staff_d[MAXSTAFF], *staff_p;
	char *p, *q;
	float y;

	for (staff = cursys->nstaff; staff >= 0; staff--) {
		if (!cursys->staff[staff].empty)
			break;
	}
	if (staff < 0)
		return;

	memset(staff_d, 0, sizeof staff_d);
	n = 0;
	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		if (!p_voice->sym)
			continue;
		staff = p_voice->staff;
		if (cursys->staff[staff].empty)
			continue;
		if (p_voice->new_name) {
			p_voice->new_name = 0;
			p = p_voice->nm;
		} else {
			p = p_voice->snm;
		}
		if (!p)
			continue;
		if (cursys->staff[staff].flags & CLOSE_BRACE2) {
			while (!(cursys->staff[staff].flags & OPEN_BRACE2))
				staff--;
		} else if (cursys->staff[staff].flags & CLOSE_BRACE) {
			while (!(cursys->staff[staff].flags & OPEN_BRACE))
				staff--;
		}
		staff_p = &staff_d[staff];
		for (;;) {
			staff_p->v[staff_p->nl++] = p;
			p = strstr(p, "\\n");
			if (!p
			 || staff_p->nl >= MAXSTAFF)
				break;
			p += 2;
		}
		n++;
	}
	if (n == 0)
		return;
	str_font(VOICEFONT);
	indent = -indent * .5;			/* center */
	for (staff = nstaff; staff >= 0; staff--) {
		staff_p = &staff_d[staff];
		if (staff_p->nl == 0)
			continue;
		y = staff_tb[staff].y
			+ staff_tb[staff].topbar * .5
				* staff_tb[staff].clef.staffscale
			+ 9 * (staff_p->nl - 1)
			- cfmt.font_tb[VOICEFONT].size * .3;
		n = staff;
		if (cursys->staff[staff].flags & OPEN_BRACE2) {
			while (!(cursys->staff[n].flags & CLOSE_BRACE2))
				n++;
		} else if (cursys->staff[staff].flags & OPEN_BRACE) {
			while (!(cursys->staff[n].flags & CLOSE_BRACE))
				n++;
		}
		if (n != staff)
			y -= (staff_tb[staff].y - staff_tb[n].y) * .5;
		for (n = 0; n < staff_p->nl; n++) {
			p = staff_p->v[n];
			q = strstr(p, "\\n");
			if (q)
				*q = '\0';
			a2b("%.1f %.1f M ", indent, y);
			put_str(p, A_CENTER);
			y -= 18.;
			if (q)
				*q = '\\';
		}
	}
}

/* -- adjust the empty flag in a staff system -- */
static void set_empty(struct SYSTEM *sy)
{
	int staff;

	/* if a system brace has empty and non empty staves, keep all staves */
	for (staff = 0; staff <= nstaff; staff++) {
		int i, empty_fl;

		if (!(sy->staff[staff].flags & (OPEN_BRACE | OPEN_BRACE2)))
			continue;
		empty_fl = 0;
		i = staff;
		while (staff <= nstaff) {
			if (sy->staff[staff].empty)
				empty_fl |= 1;
			else
				empty_fl |= 2;
			if (cursys->staff[staff].flags & (CLOSE_BRACE | CLOSE_BRACE2))
				break;
			staff++;
		}
		if (empty_fl == 3) {	/* if empty and not empty staves */
/*fixme: add measure bars on empty main voices*/
			while (i <= staff)
				sy->staff[i++].empty = 0;
		}
	}
}

/* -- set the y offset of the staves and return the whole height -- */
static float set_staff(void)
{
	struct SYSTEM *sy;
	struct SYMBOL *s;
	int staff;
	float y, staffsep, dy, maxsep, mbot;
	struct {
		float mtop;
		int empty;
	} delta_tb[MAXSTAFF], *p_delta;

	/* search the empty staves in each parts */
	memset(delta_tb, 0, sizeof delta_tb);
	for (staff = 0; staff <= nstaff; staff++) {
		delta_tb[staff].empty = 1;
		staff_tb[staff].empty = 0;
	}
	sy = cursys;
	set_empty(sy);
	for (staff = 0; staff <= nstaff; staff++) {
		if (!sy->staff[staff].empty)
			delta_tb[staff].empty = 0;
	}
	for (s = tsfirst; s; s = s->ts_next) {
		if (s->type != STAVES)
			continue;
		sy = sy->next;
		set_empty(sy);
		for (staff = 0; staff <= nstaff; staff++) {
			if (!sy->staff[staff].empty)
				delta_tb[staff].empty = 0;
		}
	}

	{
		int i, prev_staff;
		float v;

		prev_staff = -1;
		for (staff = 0, p_delta = delta_tb;
		     staff <= nstaff;
		     staff++, p_delta++) {
			if (p_delta->empty) {
				staff_tb[staff].empty = 1;
				continue;
			}
			if (prev_staff >= 0) {
				if (staff_tb[staff].clef.staffscale
						== staff_tb[prev_staff].clef.staffscale) {
					float mtop;

					mtop = 0;
					for (i = 0; i < YSTEP; i++) {
						v = staff_tb[staff].top[i]
						  - staff_tb[prev_staff].bot[i];
						if (mtop < v)
							mtop = v;
					}
					p_delta->mtop = mtop
							* staff_tb[staff].clef.staffscale;
				} else {
					for (i = 0; i < YSTEP; i++) {
						v = staff_tb[staff].top[i]
							* staff_tb[staff].clef.staffscale
						  - staff_tb[prev_staff].bot[i]
							* staff_tb[prev_staff].clef.staffscale;
						if (p_delta->mtop < v)
							p_delta->mtop = v;
					}
				}
			} else {
				float mtop;

				mtop = 0;
				for (i = 0; i < YSTEP; i++) {
					v = staff_tb[staff].top[i];
					if (mtop < v)
						mtop = v;
				}
				p_delta->mtop = mtop
						* staff_tb[staff].clef.staffscale;
			}
			prev_staff = staff;
		}
		mbot = 0;
		for (i = 0; i < YSTEP; i++) {
			v = staff_tb[prev_staff].bot[i];
			if (mbot > v)
				mbot = v;
		}
		mbot *= staff_tb[prev_staff].clef.staffscale;
	}

	/* handle the empty staves and their tablatures
	 * and output the scale of the voices */
	{
		struct VOICE_S *p_voice;
		int i;
		float ha, hu;

		for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
			if (p_voice->scale != 1)
				a2b("/scvo%d{gsave %.2f dup scale}!\n",
				     (int) (p_voice - voice_tb),
				     p_voice->scale);
			staff = p_voice->staff;
			if (!staff_tb[staff].empty)
				continue;
			ha = hu = 0;
			for (i = 0; i < 2; i++) {
				if (p_voice->tblts[i] != 0
				 && p_voice->tblts[i]->pitch == 0) {
					ha += p_voice->tblts[i]->ha
						* staff_tb[staff].clef.staffscale;
					hu += p_voice->tblts[i]->hu
						* staff_tb[staff].clef.staffscale;
				}
			}
			if (ha == 0 && hu == 0) {
				staff_tb[staff].topbar = 0;
				continue;
			}
			delta_tb[staff].mtop += ha;
			if (staff < nstaff)
				delta_tb[staff + 1].mtop += hu;
			else
				mbot -= hu;
			delta_tb[staff].empty = 0;
		}
	}

	/* draw the parts and tempo indications if any */
	dy = 0;
	for (staff = 0; staff <= nstaff; staff++) {
		dy = delta_tb[staff].mtop;
		if (dy != 0)
			break;
	}
	dy = draw_partempo(staff, dy);

	/* set the staff offsets */
	staffsep = cfmt.staffsep * 0.5;
	maxsep = cfmt.maxstaffsep * 0.5;
	y = 0;
	for (staff = 0, p_delta = delta_tb;
	     staff <= nstaff;
	     staff++, p_delta++) {
		dy += p_delta->mtop;
		if (!staff_tb[staff].empty) {
			staffsep += staff_tb[staff].topbar
				* staff_tb[staff].clef.staffscale;
			if (dy < staffsep)
				dy = staffsep;
			maxsep += staff_tb[staff].topbar
				* staff_tb[staff].clef.staffscale;
			if (dy > maxsep)
				dy = maxsep;
		}
		y += dy;
		staff_tb[staff].y = -y;
/*fixme: handle tablature?*/
		if (staff_tb[staff].empty)
			staffsep = 0;
		else if (sy->staff[staff].sep != 0)
			staffsep = sy->staff[staff].sep;
		else
			staffsep = cfmt.sysstaffsep;
		if (sy->staff[staff].maxsep != 0)
			maxsep = sy->staff[staff].maxsep;
		else
			maxsep = cfmt.maxsysstaffsep;
		dy = 0;
	}

	/* output the staff offsets */
	dy = staff_tb[nstaff].y;
	for (staff = nstaff; staff >= 0; staff--) {
		if (staff_tb[staff].y == 0)
			staff_tb[staff].y = dy;
		else
			dy = staff_tb[staff].y;
		if (staff_tb[staff].clef.staffscale != 1
		 && staff_tb[staff].clef.staffscale != 0) {
			a2b("/scst%d{gsave 0 %.2f T %.2f dup scale}!\n",
			     staff, dy, staff_tb[staff].clef.staffscale);
			a2b("/y%d{}!\n", staff);
		} else {
			a2b("/y%d{%.1f add}!\n", staff, dy);
		}
	}

	if (mbot == 0) {
		for (staff = nstaff; staff >= 0; staff--) {
			if (!delta_tb[staff].empty)
				break;
		}
		if (staff < 0)		/* no symbol in this system ! */
			return y;
	}
	dy = -mbot;
	staffsep = cfmt.staffsep * 0.5;
	if (dy < staffsep)
		dy = staffsep;
	maxsep = cfmt.maxstaffsep * 0.5;
	if (dy > maxsep)
		dy = maxsep;
	y += dy;
	if (y > cfmt.maxstaffsep)
		y = cfmt.maxstaffsep;

	/* return the whole staff system height */
	return y;
}

/* -- set the bottom and height of the measure bars -- */
static void bar_set(float *bar_bot, float *bar_height)
{
	int staff, nlines;
	float dy, staffscale;
			/* !! max number of staff lines !! */
	char top[10] = {18, 18, 12, 18, 18, 24, 30, 36, 42, 48};
	char bot[10] = { 6,  6,  6,  6,  0,  0,  0,  0,  0,  0};

	dy = 0;
	for (staff = 0; staff <= nstaff; staff++) {
		nlines = cursys->staff[staff].clef.stafflines;
		staffscale = cursys->staff[staff].clef.staffscale;
		if (cursys->staff[staff].empty) {
			bar_bot[staff] = bar_height[staff] = 0;
			if (dy == 0)
				continue;
		} else {
			if (dy == 0)
				dy = staff_tb[staff].y + top[nlines]
								* staffscale;
			bar_height[staff] = dy
				- staff_tb[staff].y - bot[nlines]
								* staffscale;
		}
		bar_bot[staff] = staff_tb[staff].y + bot[nlines]
						* staffscale;

		if (cursys->staff[staff].flags & STOP_BAR)
			dy = 0;
		else
			dy = bar_bot[staff];
	}
}

/* -- draw the staff systems and the measure bars -- */
float draw_systems(float indent)
{
	struct SYSTEM *next_sy;
	struct SYMBOL *s, *s2;
	int staff;
	float xstaff[MAXSTAFF], bar_bot[MAXSTAFF], bar_height[MAXSTAFF];
	float x, x2;
	float line_height;

	line_height = set_staff();
	draw_vname(indent);

	/* draw the staff, skipping the staff breaks */
	for (staff = 0; staff <= nstaff; staff++)
		xstaff[staff] = cursys->staff[staff].empty ? -1 : 0;
	bar_set(bar_bot, bar_height);
	draw_lstaff(0);
	for (s = tsfirst; s; s = s->ts_next) {
		staff = s->staff;
		switch (s->type) {
		case STAVES:
			next_sy = cursys->next;
			for (staff = 0; staff <= nstaff; staff++) {
				if (next_sy->staff[staff].empty
						== cursys->staff[staff].empty
				 && next_sy->staff[staff].clef.stafflines
						== cursys->staff[staff].clef.stafflines)
					continue;
				x2 = s->x;
				if ((x = xstaff[staff]) >= 0) {
					if (s->ts_prev->type == BAR)
						x2 = s->ts_prev->x;
					draw_staff(staff, x, x2);
				}
				if (next_sy->staff[staff].empty) {
					xstaff[staff] = -1;
				} else if (xstaff[staff] < 0) {
					if (s->ts_next->type != BAR)
						xstaff[staff] = x2;
					else
						xstaff[staff] = s->ts_next->x;
				} else {
					xstaff[staff] = x2;
				}
			}
			cursys = next_sy;
			bar_set(bar_bot, bar_height);
			break;
		case BAR:
			if ((s->sflags & S_SECOND)
			 || cursys->staff[staff].empty)
				s->as.flags |= ABC_F_INVIS;
			if (s->as.flags & ABC_F_INVIS)
				break;
			draw_bar(s, bar_bot[staff], bar_height[staff]);
			if (annotate)
				anno_out(s, 'B');
			break;
		case STBRK:
			if (cursys->voice[s->voice].range == 0) {
				if (s->next
				 && s->next->type == STAVES)
					s->next->x = s->x;
				if ( s->xmx > .5 CM) {
					int i, nvoice;

					/* draw the left system if stbrk in all voices */
					nvoice = 0;
					for (i = 0; i < MAXVOICE; i++) {
						if (cursys->voice[i].range > 0)
							nvoice++;
					}
					for (s2 = s->ts_next; s2; s2 = s2->ts_next) {
						if (s2->type != STBRK)
							break;
						nvoice--;
					}
					if (nvoice == 0)
						draw_lstaff(s->x);
				}
			}
			s2 = s->prev;
			if (!s2)
				break;
			if (s2->type == STAVES)
				s2 = s2->prev;
			x2 = s2->x;
			if (s2->type != BAR)
				x2 += s2->wr;
			staff = s->staff;
			x = xstaff[staff];
			if (x >= 0) {
				if (x >= x2)
					continue;
				draw_staff(staff, x, x2);
			}
			xstaff[staff] = s->x;
			break;
		default:
//fixme:does not work for "%%staves K: M: $" */
			if (cursys->staff[staff].empty)
				s->as.flags |= ABC_F_INVIS;
			break;
		}
	}
	for (staff = 0; staff <= nstaff; staff++) {
		if ((x = xstaff[staff]) < 0
		 || x >= realwidth - 8)
			continue;
		draw_staff(staff, x, realwidth);
	}
	set_sscale(-1);
	return line_height;
}

/* -- output PostScript sequences -- */
void output_ps(struct SYMBOL *s, int state)
{
	struct SYMBOL *g, *g2;

	g = s->extra;
	g2 = NULL;
	for (;;) {
		if (g->type == FMTCHG
		 && (g->u == PSSEQ || g->u == SVGSEQ)
		 && g->as.state <= state) {
			if (g->u == SVGSEQ)
				a2b("%%svg %s\n", g->as.text);
			else
				a2b("%s\n", g->as.text);
			if (!g2)
				s->extra = g->next;
			else
				g2->next = g->next;
		} else {
			g2 = g;
		}
		g = g->next;
		if (!g)
			break;
	}
}

/* -- draw remaining symbols when the staves are defined -- */
static void draw_symbols(struct VOICE_S *p_voice)
{
	struct BEAM bm;
	struct SYMBOL *s;
	float x, y;
	int staff, first_note;

	/* output the PostScript code at start of line */
	for (s = p_voice->sym; s; s = s->next) {
		if (s->extra)
			output_ps(s, 127);
		switch (s->type) {
		case CLEF:
		case KEYSIG:
		case TIMESIG:
		case BAR:
			continue;	/* skip the symbols added by init_music_line() */
		}
		break;
	}

	bm.s2 = 0;
	first_note = 1;
	for (s = p_voice->sym; s; s = s->next) {
		if (s->extra)
			output_ps(s, 127);
		if ((s->as.flags & ABC_F_INVIS)
		 && s->type != NOTEREST && s->type != GRACE)
			continue;
		x = s->x;
		switch (s->type) {
		case NOTEREST:
			set_scale(s);
			if (s->as.type == ABC_T_NOTE) {
				if ((s->sflags & (S_BEAM_ST | S_BEAM_END)) == S_BEAM_ST
				 || (first_note && !(s->sflags & S_BEAM_ST))) {
					first_note = 0;
					if (calculate_beam(&bm, s)) {
						if (annotate)
							anno_out(s, 'b');
						draw_beams(&bm);
					}
				}
				draw_note(x, s, bm.s2 == 0);
				if (annotate)
					anno_out(s, 'N');
				if (s == bm.s2)
					bm.s2 = NULL;
				if (annotate
				 && (s->sflags & (S_BEAM_ST | S_BEAM_END))
							== S_BEAM_END)
					anno_out(s, 'e');
				break;
			}
			draw_rest(s);
			if (annotate)
				anno_out(s, 'R');
			break;
		case BAR:
			break;			/* drawn in draw_systems */
		case CLEF:
			staff = s->staff;
			if (s->sflags & S_SECOND)
/*			 || p_voice->staff != staff)	*/
				break;		/* only one clef per staff */
			if ((s->as.flags & ABC_F_INVIS)
			 || staff_tb[staff].empty)
				break;
			set_sscale(staff);
			y = staff_tb[staff].y;
			x -= 10;	/* clef shift - see set_width() */
			putxy(x, y + s->y);
			if (s->as.u.clef.name)
				a2b("%s\n", s->as.u.clef.name);
			else
				a2b("%c%cclef\n",
				     s->u ? 's' : ' ',
				     "tcbp"[(unsigned) s->as.u.clef.type]);
			if (s->as.u.clef.octave != 0) {
/*fixme:break the compatibility and avoid strange numbers*/
				if (s->as.u.clef.octave > 0)
					y += s->ymx - 12;
				else
					y += s->ymn + 2;
				putxy(x, y);
				a2b("oct%c\n",
				     s->as.u.clef.octave > 0 ? 'u' : 'l');
			}
			if (annotate)
				anno_out(s, 'c');
			break;
		case TIMESIG:
			memcpy(&p_voice->meter, &s->as.u.meter,
			       sizeof p_voice->meter);
			if ((s->sflags & S_SECOND)
			 || staff_tb[s->staff].empty)
				break;
			if (cfmt.alignbars && s->staff != 0)
				break;
			set_sscale(s->staff);
			draw_timesig(x, s);
			if (annotate)
				anno_out(s, 'M');
			break;
		case KEYSIG:
			memcpy(&p_voice->key, &s->as.u.key,
			       sizeof p_voice->key);
			if ((s->sflags & S_SECOND)
			 || staff_tb[s->staff].empty)
				break;
			set_sscale(s->staff);
			draw_keysig(p_voice, x, s);
			if (annotate)
				anno_out(s, 'K');
			break;
		case MREST:
			set_scale(s);
			a2b("(%d)", s->as.u.bar.len);
			putxy(x, staff_tb[s->staff].y);
			a2b("mrest\n");
			break;
		case GRACE:
			set_scale(s);
			draw_gracenotes(s);
			break;
		case SPACE:
		case STAVES:
		case STBRK:
		case FMTCHG:
			break;			/* nothing */
		case CUSTOS:
			set_scale(s);
			s->sflags |= ABC_F_STEMLESS;
			draw_note(x, s, 0);
			break;
		default:
			bug("Symbol not drawn", 1);
		}
	}
	set_scale(p_voice->sym);
	draw_all_ties(p_voice);
}

/* -- draw all symbols -- */
void draw_all_symb(void)
{
	struct VOICE_S *p_voice;

	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
#if 1 /*fixme:test*/
		if (!p_voice->sym)
#else
		if (staff_tb[p_voice->staff].empty || !p_voice->sym)
#endif
			continue;
		draw_symbols(p_voice);
	}
}

/* -- output a floating value, and x and y according to the current scale -- */
void putf(float v)
{
	a2b("%.1f ", v);
}

void putx(float x)
{
	putf(x / cur_scale);
}

void puty(float y)
{
	putf(scale_voice ?
		y / cur_scale :		/* scaled voice */
		y - cur_trans);		/* scaled staff */
}

void putxy(float x, float y)
{
	if (scale_voice)
		a2b("%.1f %.1f ",
		     x / cur_scale, y / cur_scale);	/* scaled voice */
	else
		a2b("%.1f %.1f ",
		     x / cur_scale, y - cur_trans);	/* scaled staff */
}

/* -- set the voice or staff scale -- */
void set_scale(struct SYMBOL *s)
{
	int staff;
	float scale, trans;

	staff = -1;
	scale = voice_tb[s->voice].scale;
	if (scale == 1) {
		staff = s->staff;
		scale = staff_tb[staff].clef.staffscale;
	}
/*fixme: KO when both staff and voice are scaled */
	if (staff >= 0 && scale != 1) {
		trans = staff_tb[staff].y;
		scale_voice = 0;
		if (staff != cur_staff && cur_scale != 1)
			cur_scale = 0;
	} else {
		trans = 0;
		scale_voice = 1;
	}
	if (scale == cur_scale && trans == cur_trans)
		return;
	if (cur_scale != 1)
		a2b("grestore ");
	cur_scale = scale;
	cur_trans = trans;
	if (scale != 1) {
		if (scale_voice) {
			a2b("scvo%d ", s->voice);
		} else {
			a2b("scst%d ", staff);
			cur_staff = staff;
		}
	}
}

/* -- set the staff scale (only) -- */
void set_sscale(int staff)
{
	float scale, trans;

	scale_voice = 0;
	if (staff != cur_staff && cur_scale != 1)
		cur_scale = 0;
	if (staff >= 0)
		scale = staff_tb[staff].clef.staffscale;
	else
		scale = 1;
	if (staff >= 0 && scale != 1)
		trans = staff_tb[staff].y;
	else
		trans = 0;
	if (scale == cur_scale && trans == cur_trans)
		return;
	if (cur_scale != 1)
		a2b("grestore ");
	cur_scale = scale;
	cur_trans = trans;
	if (scale != 1) {
		a2b("scst%d ", staff);
		cur_staff = staff;
	}
}

/* -- set the tie directions for one voice -- */
static void set_tie_dir(struct SYMBOL *sym)
{
	struct SYMBOL *s;
	int i, ntie, dir, sec, pit, ti;

	for (s = sym; s; s = s->next) {
		if (!(s->sflags & S_TI1))
			continue;

		/* if other voice, set the ties in opposite direction */
		if (s->multi != 0) {
/*			struct SYMBOL *s2;

			s2 = s->ts_next;
			if (s2->time == s->time && s2->staff == s->staff) { */
				dir = s->multi > 0 ? SL_ABOVE : SL_BELOW;
				for (i = 0; i <= s->nhd; i++) {
					ti = s->as.u.note.ti1[i];
					if (!((ti & 0x03) == SL_AUTO))
						continue;
					s->as.u.note.ti1[i] = (ti & SL_DOTTED) | dir;
				}
				continue;
/*			} */
		}

		/* if one note, set the direction according to the stem */
		sec = ntie = 0;
		pit = 128;
		for (i = 0; i <= s->nhd; i++) {
			if (s->as.u.note.ti1[i]) {
				ntie++;
				if (pit < 128
				 && s->as.u.note.pits[i] <= pit + 1)
					sec++;
				pit = s->as.u.note.pits[i];
			}
		}
		if (ntie <= 1) {
			dir = s->stem < 0 ? SL_ABOVE : SL_BELOW;
			for (i = 0; i <= s->nhd; i++) {
				ti = s->as.u.note.ti1[i];
				if (ti != 0) {
					if ((ti & 0x03) == SL_AUTO)
						s->as.u.note.ti1[i] =
							(ti & SL_DOTTED) | dir;
					break;
				}
			}
			continue;
		}
		if (sec == 0) {
			if (ntie & 1) {
/* in chords with an odd number of notes, the outer noteheads are paired off
 * center notes are tied according to their position in relation to the
 * center line */
				ntie = ntie / 2 + 1;
				dir = SL_BELOW;
				for (i = 0; i <= s->nhd; i++) {
					ti = s->as.u.note.ti1[i];
					if (ti == 0)
						continue;
					if (--ntie == 0) {	/* central tie */
						if (s->as.u.note.pits[i] >= 22)
							dir = SL_ABOVE;
					}
					if ((ti & 0x03) == SL_AUTO)
						s->as.u.note.ti1[i] =
							(ti & SL_DOTTED) | dir;
					if (ntie == 0)
						dir = SL_ABOVE;
				}
				continue;
			} else {
/* even number of notes, ties divided in opposite directions */
				ntie /= 2;
				dir = SL_BELOW;
				for (i = 0; i <= s->nhd; i++) {
					ti = s->as.u.note.ti1[i];
					if (ti == 0)
						continue;
					if ((ti & 0x03) == SL_AUTO)
						s->as.u.note.ti1[i] =
							(ti & SL_DOTTED) | dir;
					if (--ntie == 0)
						dir = SL_ABOVE;
				}
				continue;
			}
		}
/*fixme: treat more than one second */
/*		if (nsec == 1) {	*/
/* When a chord contains the interval of a second, tie those two notes in
 * opposition; then fill in the remaining notes of the chord accordingly */
			pit = 128;
			for (i = 0; i <= s->nhd; i++) {
				if (s->as.u.note.ti1[i]) {
					if (pit < 128
					 && s->as.u.note.pits[i] <= pit + 1) {
						ntie = i;
						break;
					}
					pit = s->as.u.note.pits[i];
				}
			}
			dir = SL_BELOW;
			for (i = 0; i <= s->nhd; i++) {
				ti = s->as.u.note.ti1[i];
				if (ti == 0)
					continue;
				if (ntie == i)
					dir = SL_ABOVE;
				if ((ti & 0x03) == SL_AUTO)
					s->as.u.note.ti1[i] =
							(ti & SL_DOTTED) | dir;
			}
/*fixme..
			continue;
		}
..*/
/* if a chord contains more than one pair of seconds, the pair farthest
 * from the center line receives the ties drawn in opposition */
	}
}

/* -- have room for the ties out of the staves -- */
static void set_tie_room(void)
{
	struct VOICE_S *p_voice;
	struct SYMBOL *s, *s2;

	for (p_voice = first_voice; p_voice; p_voice = p_voice->next) {
		s = p_voice->sym;
		if (!s)
			continue;
		s = s->next;
		if (!s)
			continue;
		set_tie_dir(s);
		for ( ; s; s = s->next) {
			float dx, y, dy;

			if (!(s->sflags & S_TI1))
				continue;
			if (s->pits[0] < 20 && s->as.u.note.ti1[0] == SL_BELOW)
				;
			else if (s->pits[s->nhd] > 24
			      && s->as.u.note.ti1[s->nhd] == SL_ABOVE)
				;
			else
				continue;
			s2 = s->next;
			while (s2 && s2->as.type != ABC_T_NOTE)
				s2 = s2->next;
			if (s2) {
				if (s2->staff != s->staff)
					continue;
				dx = s2->x - s->x - 10;
			} else {
				dx = realwidth - s->x - 10;
			}
			if (dx < 100)
				dy = 9;
			else if (dx < 300)
				dy = 12;
			else
				dy = 16;
			if (s->pits[s->nhd] > 24) {
				y = 3 * (s->pits[s->nhd] - 18) + dy;
				if (s->ymx < y)
					s->ymx = y;
				if (s2 && s2->ymx < y)
					s2->ymx = y;
				y_set(s->staff, 1, s->x + 5, dx, y);
			}
			if (s->pits[0] < 20) {
				y = 3 * (s->pits[0] - 18) - dy;
				if (s->ymn > y)
					s->ymn = y;
				if (s2 && s2->ymn > y)
					s2->ymn = y;
				y_set(s->staff, 0, s->x + 5, dx, y);
			}
		}
	}
}
