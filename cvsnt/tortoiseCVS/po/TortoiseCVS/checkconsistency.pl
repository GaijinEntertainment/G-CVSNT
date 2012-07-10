#!/usr/bin/perl
# $Header: /cvs/cvsnt/tortoiseCVS/po/TortoiseCVS/checkconsistency.pl,v 1.1 2012/03/04 01:09:09 aliot Exp $
# 
# Summary:
#   This perl script reads the english po file and retrieves all the msgid's.
#   After this it loads all the other po files and checks that there exist
#   the same msgids in each.
#   If any msgid is missing, the script will print out the msgid's missing 
#   or empty.
# 
# $Log: checkconsistency.pl,v $
# Revision 1.1  2012/03/04 01:09:09  aliot
# initial revision
#
# Revision 1.2  2004/11/22 21:21:12  bullestock
# Updated from source.
#
# Revision 1.1  2003/11/07 19:37:08  hhonisch
# Multilingual setup
#
# Revision 1.1  2003/09/11 11:41:46  linuxuser42
# Added this file. It can be used to do a consistency check to see whether all translations have been done appropiately.
#

# open the english po file


# look for po files in current directory
@pofiles = glob("*.po");

# run msgfmt for each pofile
for $pofile (@pofiles)
{
    $_ = $pofile;
    print("Checking $pofile\n");
    if (($aname)=/([\w\W_]+)\.po/ and $pofile ne "en_GB.po" and $pofile ne "")
    {
	system("msgfmt --statistics $pofile");
    }
}


