Title : Mac OS X enhancements for cvsnt
Authors : Jens Miltner <jum@mac.com> --- October 2004

(adapted from original cvsgui documentation of Alexandre Parenteau <aubonbeurre@hotmail.com> --- January 2001)

/* $Id: README.txt,v 1.1.2.2 2005/04/02 22:28:40 jmiltner Exp $ */
----------------------------------------------------------------------------
1) Why Mac OS X enhancements for cvsnt ?
  1.1 - Resource fork encoding
  1.2 - The line feed problem
  1.3 - Signature for files
  1.4 - The MacCvs support for Mac OS X

2) How does-it work (for developers) ?
  2.1 - Client-side changes to cvsnt
  2.2 - the convert scheme

3) How to install & use ?
  3.1 - replacing cvs
  3.2 - preparing a client session
  3.3 - using the environment variables


1) Why Mac OS X enhancements for cvsnt?

1.1 - Resource fork encoding

The 'standard' cvs command has several problems when used
with repositories created with MacCvs or MacCvsPro : MacCvs and
MacCvsPro are using an encoding for the macintosh files with
a resource fork (either using the HQX or AppleSingle file format).
These files get encoded on the server and are decoded on the client.

The 'regular' cvs command will not interpret
correctly this encoding and it will result in a corrupted checked out
file.

cvsnt with the Mac OS X enhancements will interpret correctly
the format when checking out and commiting a resource file.

1.2 - The line feed problem

The 'standard' cvsnt command will check-out text file with a Unix line feed, and
it will corrupt a repository if you commit a file with a Mac line
feed.

cvsnt with Mac OS X enhancements will optionally check-out text files with the Mac line
feed and allows any text file to be stored with a Mac or Unix
line feed, making it impossible to corrupt the repository.

Note: it is *highly* recommended to not use anymore the old line
ending, and use the Unix line feed instead.

1.3 - Signature for files

The 'standard' cvs does not deal with Mac-signature for text and binaries files.

The Mac OS X enhanced version of cvsnt does deal with Mac-signature, it asks InternetConfig 
or uses a user-defined extension map to determine what is
the correct Mac-signature for a newly created file (optional, see below).

It also provides several optional goodies like an ISO8859 conversion
on the text files.

1.4 - The MacCvs support for Mac OS X

The Mac OS X enhanced version of cvsnt can be used as the underlying cvs tool
for MacCvsX, replacing the built-in cvsgui tool.


2) How does-it work (for developers) ?

2.1 - Client-side changes to cvsnt

The enhancements done to cvsnt only apply to the client side - the server is agnostic of the 
additional resource fork support and conversions done on the Mac client.
The main changes are calls to a function that will en-/decode resource forks into a flat binary file
or apply the text conversions (line endings, ISO8859 conversion) at the point where data is received
from the server or about to be sent to the server.

2.2 - the convert scheme

The folder 'osx' in the cvsnt source distribution contains the convert 
scheme used in client mode for maintaining the macintosh informations 
on the resource and text files.

This part converts local files to send to the server, and convert
back the server files to the local format.

Local and server file formats may be very different, for example
you may have :

Local:                             Server:

Text file with Mac line feed       Text file with Unix line feed
and Mac-encoding                   and ISO-encoding

Text file with Unix line feed      Text file with Unix line feed
and Mac-encoding                   and ISO-encoding

Text file with Unix line feed      Text file with Unix line feed
and Unix-encoding                  and ISO-encoding

Resource file                      HQX file

JPEG file with the JPEG            JPEG file
signature


3) How to install & use ?

3.1 - replacing cvs

It's not recommended to use the regular cvs with cvsnt because of the line
feed problems and missing resource fork support.

I personnaly recommend to disable the existing cvs and to replace it
by the new one:

> cd /usr/bin
> mv cvs cvs.orig
> ln -s /usr/local/bin/cvsnt cvs

(assuming your cvsnt is installed in the default location /usr/local/bin/cvsnt)

Now try :

> cvs --version

It should say :

> Concurrent Versions System (CVSNT) 2.0.58b (client/server)
> ...

(or a higher version number)

3.2 - preparing a client session

The ability of cvsnt to manage Mac formats work *only* in client
mode. IT WON'T WORK IN LOCAL MODE.

This means that you should have your CVSROOT to use either :pserver:,
:rsh:, :ext: (ssh), :kserver: (Kerberos).

See the cvsnt doc for more details on how to set-up a client session.

3.3 - using the environment variables

Controlling the Mac informations for the cvs files is done using
environment variables. Environment variables can be set and unset using
the shell command 'setenv'.

Here is an example of my ~/.tcshrc :

> setenv CVSROOT :pserver:alexp@pelforth:/cvsroot
> setenv IC_ON_TXT yes
> setenv MAC_DEFAULT_RESOURCE_ENCODING AppleSingle

BTW: Files with HFS filetype 'TEXT' will never have their resource
forks encoded, regardless of any of the environment settings.

* MAC_DEFAULT_RESOURCE_ENCODING (HQX, AppleSingle, PlainBinary)

Set which format to use in order to encode files with a resource fork
for sending to the server.
Only binary files (-kb) will have their resource forks encoded, unless specified otherwise (see MAC_ENCODE_ONLY_BINARY_FILES below).
The encoding types are:
	- HQX: 		Use BinHex encoding to encode both the resource and data fork
	- AppleSingle: 	Use AppleSingle or AppleDouble encoding to encode resource and data forks
	- PlainBinary:	Discard the resource fork, send only data fork unencoded. This format 
			forces the files to be sent in the 'regular' binary format; it's convenient
			for e.g. Word documents that may have resource forks, but they don't
			contain vital data and must be discarded in order to keep the documents
			compatible with the Windows world.

Decoding is not relevant, cvsnt will pick the appropriate one.

The default encoding is HQX.

* ISO8859 (1)

Text files will be converted from the Mac-encoding to a ISO8859 format.
Usefull if you use accentuation accross platforms (French, German...)

The default makes no conversion for text files.
- Value of 1 is using the 8859 table of Fetch.
- Value of 2 is using the 8859 table of MacCvsPro. Usefull for checking out
MacCvsPro sandbox

* IC_ON_TXT (yes)

If defined, it will use InterConfig to set the Mac signature for any
newly created file (except the files using a HQX or AppleSingle encoding,
because the file type/creator is already enclosed in these formats).

By default the created file do not have a file type/creator.

* CVS_MACLF (yes)

If defined, any newly created text file will have the Mac line feed (0x0d).

By default, the file have a Unix line feed.

AVOID THIS OPTION WHEN POSSIBLE !!! Mac line feed will disappear and most
of the good editors already handle Unix line feed.

* MAC_BINARY_TYPES_PLAIN, MAC_BINARY_TYPES_HQX, MAC_BINARY_TYPES_SINGLE

It contains a semi-colon separated set of types (ex: JPEG;TIFF) or of
types and extensions (ex: JPEG/jpg;????/jpeg;TIFF;????/tiff) and help
cvs to decide which encoding to use on the server for these files.
The format for each type specification is a 4-letter HFS file type constant 
(use '????' as a wildcard match when only matching extension), followed by an optional filename
extension, which, if it's provided must be separated by a '/'.
If both type _and_ extension are given, both must match to consider the
file a match.

MAC_BINARY_TYPES_PLAIN will ignore the resource fork of the file
which matches the type and/or extension.

MAC_BINARY_TYPES_HQX will encode the file which matches the type and/or extension
in the HQX format.

MAC_BINARY_TYPES_SINGLE will encode the file which matches the type and/or extension
in the AppleSingle format.

* MAC_ENCODE_ONLY_BINARY_FILES (1)

Unless this environment variable exists and is set to "0", only binary files (-kb) will
ever have their resource forks encoded (if at all), resource forks
of text files will always be discarded, no matter what the file type is.

* CVSREAD (y)

This is part of the regular cvs, it let you check-out read-only if
defined.


</jum>
