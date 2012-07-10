/*
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 1, or (at your option)
** any later version.

** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.

** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef Serialize_H
#define Serialize_H

#include <string>

class Serializable;
class Serialized;

class Serializable
{
public:
	virtual ~Serializable() {}
	virtual bool Serialize(Serialized &) const = 0;
	virtual bool Deserialize(Serialized &) = 0;
};

class Serialized
{
public:
	virtual ~Serialized() {}
	// serialization operators
	virtual bool Put(const int x) { return Put(&x, sizeof(int)); }
	virtual bool Put(const std::string &x) { return Put(static_cast<int>(x.size())) && Put(x.data(), static_cast<int>(x.size())); }
	virtual bool Put(const Serializable &x) { return x.Serialize(*this); }
	virtual bool Put(const void *, int size) = 0;
	// deserialization operators
	virtual bool Get(int &x) { return Get(&x, sizeof(int)); }
	virtual bool Get(std::string &x);
	virtual bool Get(Serializable &x) { return x.Deserialize(*this); }
	virtual bool Get(void *, int size) = 0;
};

#endif
