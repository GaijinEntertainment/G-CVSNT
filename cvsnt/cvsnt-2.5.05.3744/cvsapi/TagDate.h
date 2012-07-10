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
#ifndef TAGDATE__H
#define TAGDATE__H

#include <vector>
#include "cvs_string.h"

enum tagRangeType_t
{
	trtSingle,
	trtRangeStartIncluded,
	trtRangeStartExcluded,
	trtRangeEndIncluded,
	trtRangeEndExcluded,
	trtLessThan,
	trtLessThanOrEqual,
	trtGreaterThan,
	trtGreaterThanOrEqual
};

class CTagDateItem
{
	friend class CTagDate;
public:
	CVSAPI_EXPORT virtual ~CTagDateItem() { }
	CVSAPI_EXPORT CTagDateItem(const CTagDateItem& other)
	{
		m_tagRangeType = other.m_tagRangeType;
		m_tag = other.m_tag;
		m_tagVer = other.m_tagVer;
		m_date = other.m_date;
	}

	CVSAPI_EXPORT bool hasDate() const { return m_date!=-1; };
	CVSAPI_EXPORT bool hasTag() const { return m_tag.size()>0; }
	CVSAPI_EXPORT tagRangeType_t getType() const { return m_tagRangeType; }
	CVSAPI_EXPORT const char *tag() const { return m_tag.c_str(); }
	CVSAPI_EXPORT int ver() const { return m_tagVer; }
	CVSAPI_EXPORT const time_t date() const { return m_date; }
	CVSAPI_EXPORT const char *dateText() { if(m_date!=-1 && !m_dateText.size()) GenerateDateText(); return m_dateText.c_str(); };

protected:
	CTagDateItem() { }

	tagRangeType_t m_tagRangeType;
	cvs::string m_tag;
	int m_tagVer;
	time_t m_date;
	cvs::string m_dateText;

	CVSAPI_EXPORT void GenerateDateText();
};

class CTagDate
{
public:
	CVSAPI_EXPORT CTagDate(bool range = false);
	CVSAPI_EXPORT CTagDate(const CTagDate& other)
	{
		m_range = other.m_range;
		m_list = other.m_list;
	}
	CVSAPI_EXPORT virtual ~CTagDate();

	CVSAPI_EXPORT size_t size() { return m_list.size(); }
	CVSAPI_EXPORT CTagDateItem& operator[](size_t item) { return m_list[item]; }

	CVSAPI_EXPORT bool AddTag(const char *tag, bool isDate = false);

	// Bounded date-based revisions for this tag list, calculated once to save overhead.
	unsigned long startRev;
	unsigned long endRev;

protected:
	bool m_range;
	std::vector<CTagDateItem> m_list;

	bool AddGenericTag(const char *tag, bool isDate);
	bool BreakdownTag(bool isDate, const char *tag, cvs::string& tagName, int& tagVer, time_t& time);
};

#endif
