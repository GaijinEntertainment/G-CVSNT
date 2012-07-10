#ifndef SID__H
#define SID__H

typedef struct  _SID5
{
   UCHAR Revision;
   UCHAR SubAuthorityCount;
   SID_IDENTIFIER_AUTHORITY IdentifierAuthority;
   ULONG SubAuthority[ 5 ];
} SID5;

#define MAKE_SID0(_name, _sa ) \
	SID5 _name = { 1, 1, { 0, 0, 0, 0, 0, _sa } }
#define MAKE_SID1(_name, _sa, _rid ) \
	SID5 _name = { 1, 1, { 0, 0, 0, 0, 0, _sa }, _rid }
#define MAKE_SID2(_name, _sa, _rid , _rid2 ) \
	SID5 _name = { 1, 2, { 0, 0, 0, 0, 0, _sa }, _rid, _rid2 }
#define MAKE_SID3(_name, _sa, _rid , _rid2, _rid3 ) \
	SID5 _name = { 1, 2, { 0, 0, 0, 0, 0, _sa }, _rid, _rid2, _rid3 }
#define MAKE_SID4(_name, _sa, _rid , _rid2, _rid3, _rid4 ) \
	SID5 _name = { 1, 2, { 0, 0, 0, 0, 0, _sa }, _rid, _rid2, _rid3, _rid4 }
#define MAKE_SID5(_name, _sa, _rid , _rid2, _rid3, _rid4, _rid5 ) \
	SID5 _name = { 1, 2, { 0, 0, 0, 0, 0, _sa }, _rid, _rid2, _rid3, _rid4, _rid5 }

#endif