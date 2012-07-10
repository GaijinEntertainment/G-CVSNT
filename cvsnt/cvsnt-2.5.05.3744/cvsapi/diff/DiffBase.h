/*
	CVSNT Generic API
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/* Note:  The diff algorithm itself is licensed under the MIT license.  For the
   original version see http://www.ioplex.com/~miallen/libmba/dl/src/diff.c */

#ifndef DIFFBASE__H
#define DIFFBASE__H

#include <map>
#include <vector>

class CDiffBase
{
protected:
	CDiffBase();
	virtual ~CDiffBase();

	virtual const void *IndexFn(const void *s, int idx) =0;
	virtual int CompareFn(const void *a, const void *b) =0;
	
	int ExecuteDiff(const void *a, int aoff, int n,
		const void *b, int boff, int m, int dmax);

protected:
	enum diff_op
	{
		DIFF_MATCH = 1,
		DIFF_DELETE,
		DIFF_INSERT
	};

	struct diff_edit
	{
		diff_op op;
		int off; /* off into s1 if MATCH or DELETE but s2 if INSERT */
		int len;
	};

	std::map<int,int> m_buf;
	std::vector<diff_edit> m_ses;
	int m_dmax;

	const void *m_bufa,*m_bufb;
	int m_aoff,m_boff,m_alen,m_blen;

private:
	struct middle_snake;

	void setv(int k, int r, int val);
	int v(int k, int r);
	int find_middle_snake(const void *a, int aoff, int n,
		const void *b, int boff, int m,	struct middle_snake *ms);
	void edit(diff_op op, int off, int len);
	int ses(const void *a, int aoff, int n,
		const void *b, int boff, int m);
};

#endif 
