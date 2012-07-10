#ifndef SID__H
#define SID__H

typedef struct  _SID2
{
   UCHAR Revision;
   UCHAR SubAuthorityCount;
   SID_IDENTIFIER_AUTHORITY IdentifierAuthority;
   ULONG SubAuthority[ 2 ];
} SID2;

#define DEFINE_SID1(_name, _sa, _rid ) \
	SID2 _name = { 1, 1, { 0, 0, 0, 0, 0, _sa }, _rid }
#define DEFINE_SID2(_name, _sa, _rid , _rid2 ) \
	SID2 _name = { 1, 2, { 0, 0, 0, 0, 0, _sa }, _rid, _rid2 }

#endif