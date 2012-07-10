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
#include <config.h>
#include "lib/api_system.h"
#include <assert.h>
#include <sys/types.h>
#include <stdlib.h>
#include <ctype.h>
#include "lib/getdate.h"
#include "TagDate.h"

CTagDate::CTagDate(bool range)
{
	m_range = range;
}

CTagDate::~CTagDate()
{
}

bool CTagDate::AddTag(const char *tag, bool isDate /* = false */)
{
	return AddGenericTag(tag,isDate);
}

/* Add a tag or date 
 * Supports ranges as:
 *
 * tag (just tag)
 * tag.n (revision on branch)
 * tag@date (date on branch)
 * tag1:tag2  (inclusive)
 * tag1::tag2 (exclusive)
 * tag1:::tag2 (exclude tag1, include tag2)
 * <tag (before tag)
 * <=tag (tag and before tag)
 * >tag (after tag)
 * >=tag (tag and after tag)
 */
bool CTagDate::AddGenericTag(const char *tag,bool isDate)
{
	CTagDateItem item,item2;

	assert(tag && *tag);

	if(strchr(tag,':'))
	{
		if(!m_range)
			return false; /* Attempted to specify range on non-ranged tag */

		const char *p = strchr(tag,':');
		int type=0;
		std::string tag1,tag2;
		
		tag1.assign(tag,tag-p);
		while(*p==':')
		{
			p++;
			type++;
		}
		tag2.assign(p);

		if(!BreakdownTag(isDate,tag1.c_str(),item.m_tag,item.m_tagVer,item.m_date))
			return false;

		if(!BreakdownTag(isDate,tag2.c_str(),item2.m_tag,item2.m_tagVer,item2.m_date))
			return false;

		if(type==1)
			item.m_tagRangeType = trtRangeStartIncluded;
		else
			item.m_tagRangeType = trtRangeStartExcluded;
		m_list.push_back(item);

		if(type==1 || type==3)
			item2.m_tagRangeType = trtRangeEndIncluded;
		else
			item2.m_tagRangeType = trtRangeEndExcluded;
		m_list.push_back(item2);
	}
	else
	{
		if(m_range)
		{
			if(tag[0]=='<' && tag[1]=='=')
			{ 
				item.m_tagRangeType=trtLessThanOrEqual;
				tag+=2;
			}
			else if(tag[0]=='<')
			{
				item.m_tagRangeType=trtLessThan;
				tag++;
			}
			else if(tag[0]=='>' && tag[1]=='=')
			{
				item.m_tagRangeType=trtGreaterThanOrEqual;
				tag+=2;
			}
			else if(tag[0]=='>')
			{
				item.m_tagRangeType=trtGreaterThan;
				tag++;
			}
			else
				item.m_tagRangeType=trtSingle;
		}
		else
			item.m_tagRangeType=trtSingle;

		if(!BreakdownTag(isDate,tag,item.m_tag,item.m_tagVer,item.m_date))
			return false;

		m_list.push_back(item);
	}
	return true;
}

/* Deterimine if a tag is syntactically valid
 * supports (for non-dates):
 *
 * 1.2.3.4 (numeric)
 * foo (text)
 * foo.3 (specific revision on branch)
 * foo@date (specific date at branch)
 * @43346363AA234 (commit identifier)
 * 
 * for dates the generic date parser is used.
 */
bool CTagDate::BreakdownTag(bool isDate, const char *tag, cvs::string& tagName, int& tagVer, time_t& time)
{
	const char *p = tag;
	if(!isDate)
	{
		// Numeric
		if(isdigit(*p))
		{
			for(;*p;p++)
				if(!isdigit(*p) && *p!='.')
					break;
			if(*p)
				return false;
			tagName=tag;
			tagVer-=1;
			time=-1;
			return true;
		}
		// Commit identifier
		if(*p=='@')
		{
			tagName=tag;
			tagVer=-1;
			time=-1;
			return true;
		}

		// Real tag
		for(;*p;p++)
			if(!isalnum(*p) && *p!='_')
				break;
		// We allow tag.n or tag@date
		if(*p && *p!='.' && *p!='@')
			return false;
		tagName=tag;
		tagName.resize(p-tag);
		if(*p=='.')
		{
			const char *q=++p;
			if(*p)
			{
				for(;*p;p++)
					if(!isdigit(*p))
						break;
				if(*p)
					return false;
			}
			tagVer=atoi(q);
			time=-1;
		}
		else if(*p=='@')
		{
			time = get_date((char*)++p,NULL);
			if(time==(time_t)-1)
				return false;
			tagVer=-1;
		}
		else
		{
			time=-1;
			tagVer=-1;
		}
		return true;
	}
	else
	{
		time = get_date((char*)tag,NULL);
		if(time==(time_t)-1)
			return false;
		tagName="";
		tagVer=-1;
		return true;
	}
}

void CTagDateItem::GenerateDateText()
{
	if(m_date==-1)
	{
		m_dateText.resize(0);
		return;
	}
	m_dateText = ctime(&m_date);
}
