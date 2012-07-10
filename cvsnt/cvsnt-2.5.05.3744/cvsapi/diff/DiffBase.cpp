/* diff - compute a shortest edit script (SES) given two sequences
 * Copyright (c) 2004 Michael B. Allen <mba2000 ioplex.com>
 *
 * The MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/* This algorithm is basically Myers' solution to SES/LCS with
 * the Hirschberg linear space refinement as described in the
 * following publication:
 *
 *   E. Myers, ``An O(ND) Difference Algorithm and Its Variations,''
 *   Algorithmica 1, 2 (1986), 251-266.
 *   http://www.cs.arizona.edu/people/gene/PAPERS/diff.ps
 *
 * This is the same algorithm used by GNU diff(1).
 */

#include <config.h>
#include "../lib/api_system.h"
#include "DiffBase.h"

#ifndef INT_MAX
#define INT_MAX 2147483647
#endif

#define FV(k) v((k), 0)
#define RV(k) v((k), 1)

CDiffBase::CDiffBase()
{
}

CDiffBase::~CDiffBase()
{
}

struct CDiffBase::middle_snake
{
	int x, y, u, v;
};

void CDiffBase::setv(int k, int r, int val)
{
	int j;
                /* Pack -N to N into 0 to N * 2
                 */
	j = k <= 0 ? -k * 4 + r : k * 4 + (r - 2);

	m_buf[j]=val;
}

int CDiffBase::v(int k, int r)
{
	int j;

	j = k <= 0 ? -k * 4 + r : k * 4 + (r - 2);

	return m_buf[j];
}

int CDiffBase::find_middle_snake(const void *a, int aoff, int n,
		const void *b, int boff, int m,	struct middle_snake *ms)
{
	int delta, odd, mid, d;

	delta = n - m;
	odd = delta & 1;
	mid = (n + m) / 2;
	mid += odd;

	setv(1, 0, 0);
	setv(delta - 1, 1, n);

	for (d = 0; d <= mid; d++) {
		int k, x, y;

		if ((2 * d - 1) >= m_dmax) {
			return m_dmax;
		}

		for (k = d; k >= -d; k -= 2) {
			if (k == -d || (k != d && FV(k - 1) < FV(k + 1))) {
				x = FV(k + 1);
			} else {
				x = FV(k - 1) + 1;
			}
			y = x - k;

			ms->x = x;
			ms->y = y;
			while (x < n && y < m && CompareFn(IndexFn(a, aoff + x),IndexFn(b, boff + y)) == 0) {
				x++; y++;
			}
			setv(k, 0, x);

			if (odd && k >= (delta - (d - 1)) && k <= (delta + (d - 1))) {
				if (x >= RV(k)) {
					ms->u = x;
					ms->v = y;
					return 2 * d - 1;
				}
			}
		}
		for (k = d; k >= -d; k -= 2) {
			int kr = (n - m) + k;

			if (k == d || (k != -d && RV(kr - 1) < RV(kr + 1))) {
				x = RV(kr - 1);
			} else {
				x = RV(kr + 1) - 1;
			}
			y = x - kr;

			ms->u = x;
			ms->v = y;
			while (x > 0 && y > 0 && CompareFn(IndexFn(a, aoff + (x - 1)), IndexFn(b, boff + (y - 1))) == 0) {
					x--; y--;
			}
			setv(kr, 1, x);

			if (!odd && kr >= -d && kr <= d) {
				if (x <= FV(kr)) {
					ms->x = x;
					ms->y = y;
					return 2 * d;
				}
			}
		}
	}

	return -1;
}

void CDiffBase::edit(diff_op op, int off, int len)
{
	struct diff_edit e;

	if (len == 0) {
		return;
	}               /* Add an edit to the SES (or
                     * coalesce if the op is the same)
                     */
	if(m_ses.empty() || m_ses[m_ses.size()-1].op!=op)
	{
		e.op = op;
		e.off = off;
		e.len = len;
		m_ses.push_back(e);
	}
	else
	{
		m_ses[m_ses.size()-1].len+=len;
	}
}

int CDiffBase::ses(const void *a, int aoff, int n,
		const void *b, int boff, int m)
{
	struct middle_snake ms;
	int d;

	if (n == 0) {
		edit(DIFF_INSERT, boff, m);
		d = m;
	} else if (m == 0) {
		edit(DIFF_DELETE, aoff, n);
		d = n;
	} else {
                    /* Find the middle "snake" around which we
                     * recursively solve the sub-problems.
                     */
		d = find_middle_snake(a, aoff, n, b, boff, m, &ms);
		if (d == -1) {
			return -1;
		} else if (d >= m_dmax) {
			return m_dmax;
		} else if (d > 1) {
			if (ses(a, aoff, ms.x, b, boff, ms.y) == -1) {
				return -1;
			}

			edit(DIFF_MATCH, aoff + ms.x, ms.u - ms.x);

			aoff += ms.u;
			boff += ms.v;
			n -= ms.u;
			m -= ms.v;
			if (ses(a, aoff, n, b, boff, m) == -1) {
				return -1;
			}
		} else {
			int x = ms.x;
			int u = ms.u;

                 /* There are only 4 base cases when the
                  * edit distance is 1.
                  *
                  * n > m   m > n
                  *
                  *   -       |
                  *    \       \    x != u
                  *     \       \
                  *
                  *   \       \
                  *    \       \    x == u
                  *     -       |
                  */

			if (m > n) {
				if (x == u) {
					edit(DIFF_MATCH, aoff, n);
					edit(DIFF_INSERT, boff + (m - 1), 1);
				} else {
					edit(DIFF_INSERT, boff, 1);
					edit(DIFF_MATCH, aoff, n);
				}
			} else {
				if (x == u) {
					edit(DIFF_MATCH, aoff, m);
					edit(DIFF_DELETE, aoff + (n - 1), 1);
				} else {
					edit(DIFF_DELETE, aoff, 1);
					edit(DIFF_MATCH, aoff + 1, m);
				}
			}
		}
	}

	return d;
}

int CDiffBase::ExecuteDiff(const void *a, int aoff, int n,
		const void *b, int boff, int m, int dmax)
{
	int d, x, y;
	struct diff_edit *e = NULL;

	m_bufa = a;
	m_bufb = b;
	m_aoff = aoff;
	m_boff = boff;
	m_alen = n;
	m_blen = m;

	m_dmax = dmax ? dmax : INT_MAX;

	      /* The _ses function assumes the SES will begin or end with a delete
          * or insert. The following will insure this is true by eating any
          * beginning matches. This is also a quick to process sequences
          * that match entirely.
          */
	x = y = 0;
	while (x < n && y < m && CompareFn(IndexFn(a, aoff + x), IndexFn(b, boff + y)) == 0) {
			x++; y++;
	}

	edit(DIFF_MATCH, aoff, x);

	if ((d = ses(a, aoff + x, n - x, b, boff + y, m - y)) == -1) {
		return -1;
	}

	return d;
}
