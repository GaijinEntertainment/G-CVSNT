#!/usr/bin/perl
# This script tries to patch the PUTTY sources to get working
# TortoisePlink sources.


####################################################
# Patch console.c

$WINDLG = `cat windlg.c`;
$CONSOLE = `cat console.c`;

# Replace verify_ssh_host_key
$WINDLG =~ m/^(int verify_ssh_host_key.*?^\})/ms 
	|| die "Error retrieving \"verify_ssh_host_key\" from windlg.c"; 
$s = $1;
$CONSOLE =~ s/^void verify_ssh_host_key.*?^\}/$s/ms
	|| die "Error substituting \"verify_ssh_host_key\" in console.c"; 

# Replace askcipher
$WINDLG =~ m/^(void askcipher.*?^\})/ms 
	|| die "Error retrieving \"askcipher\" from windlg.c"; 
$s = $1;
$CONSOLE =~ s/^void askcipher.*?^\}/$s/ms
	|| die "Error substituting \"askcipher\" in console.c"; 

# Replace askappend
$WINDLG =~ m/^(int askappend.*?^\})/ms 
	|| die "Error retrieving \"askappend\" from windlg.c"; 
$s = $1;
$CONSOLE =~ s/^int askappend.*?^\}/$s/ms
	|| die "Error substituting \"askappend\" in console.c"; 

# Replace old_keyfile_warning
$WINDLG =~ m/^(void old_keyfile_warning.*?^\})/ms 
	|| die "Error retrieving \"old_keyfile_warning\" from windlg.c"; 
$s = $1;
$CONSOLE =~ s/^void old_keyfile_warning.*?^\}/$s/ms
	|| die "Error substituting \"old_keyfile_warning\" in console.c"; 

# Include LoginDialog.h
if (!($CONSOLE =~ m/#include "LoginDialog.h"/))
{
	$CONSOLE =~ s/(#include "ssh.h")/$1\r\n#include "LoginDialog.h"/ms
 		|| die "Error including \"LoginDialog.h\" in console.c";
}
 
# Include call to password dialog
$s = <<END_OF_TEXT;
int console_get_line(const char *prompt, char *str,
			    int maxlen, int is_pw)
{
    if (console_batch_mode) {
	if (maxlen > 0)
	    str[0] = '\\0';
    } else {
    if (DoLoginDialog(str, maxlen -1, prompt, is_pw))
	return 1;
    else
	cleanup_exit(1);

    }
    return 1;
}
END_OF_TEXT

chomp $s;
$s =~ s/\n/\r\n/g;

$CONSOLE =~ s/^int console_get_line.*?^\}/$s/ms
	|| die "Error substituting \"console_get_line\" in console.c"; 

# Fix MessageBox calls
$CONSOLE =~ s/(MessageBox\().*?,/$1GetParentHwnd\(\),/msg
	|| die "Error fixing MessageBox calls in console.c"; 

# Store file
open(OUTHANDLE, "> console.c") 
 	|| die "Error opening console.c";
print OUTHANDLE $CONSOLE;
close OUTHANDLE;

undef $WINDLG;
undef $CONSOLE;


####################################################
# Patch plink.c

$WINDOW = `cat window.c`;
$PLINK = `cat plink.c`;

# Replace fatalbox
$WINDOW =~ m/^(void fatalbox.*?^\})/ms 
	|| die "Error retrieving \"fatalbox\" from window.c"; 
$s = $1;
$PLINK =~ s/^void fatalbox.*?^\}/$s/ms
	|| die "Error substituting \"fatalbox\" in plink.c"; 

# Replace modalfatalbox
$WINDOW =~ m/^(void modalfatalbox.*?^\})/ms 
	|| die "Error retrieving \"modalfatalbox\" from window.c"; 
$s = $1;
$PLINK =~ s/^void modalfatalbox.*?^\}/$s/ms
	|| die "Error substituting \"modalfatalbox\" in plink.c"; 

# Replace connection_fatal
$WINDOW =~ m/^(void connection_fatal.*?^)\{/ms 
	|| die "Error retrieving \"connection_fatal\" from window.c"; 
$s = $1;
$WINDOW =~ m/^void modalfatalbox.*?^(\{.*?\})/ms 
	|| die "Error retrieving \"modalfatalbox\" from window.c"; 
$s = $s . $1;
$PLINK =~ s/^void connection_fatal.*?^\{.*?\}/$s/ms
	|| die "Error substituting \"connection_fatal\" in plink.c"; 

# Replace cmdline_error
$WINDOW =~ m/^(void cmdline_error.*?^\})/ms 
	|| die "Error retrieving \"cmdline_error\" from window.c"; 
$s = $1;
$PLINK =~ s/^void cmdline_error.*?^\}/$s/ms
	|| die "Error substituting \"cmdline_error\" in plink.c"; 

# Include LoginDialog.h
if (!($PLINK =~ m/#include "LoginDialog.h"/))
{
	$PLINK =~ s/(#include "tree234.h")/$1\r\n#include "LoginDialog.h"/ms
		|| die "Error including \"LoginDialog.h\" in plink.c";
}

# Change usage info
if (!($PLINK =~ m/printf\("TortoisePlink /))
{
	$PLINK =~ s/"PuTTY Link:.*?printf\("/"TortoisePlink /ms
		|| die "Error changing usage info plink.c";
	$PLINK =~ s/"Usage: plink/"Usage: TortoisePlink/ms
		|| die "Error changing usage info in plink.c";
	$PLINK =~ s/("Options:.*?"\)\;)/$1\r\n    printf\("  -V        show version\\n"\)\;/ms
		|| die "Error changing usage info in plink.c";
	$PLINK =~ s/"plink:/"TortoisePlink:/msg
		|| die "Error changing usage info in plink.c";
		
}

# Change replace stderr output with message boxes
if ($PLINK =~ m/fprintf\(stderr,\s*[^\r]*\r\n(?!\s*cleanup_exit\(0\))/)
{
	$PLINK =~ s/fprintf\(stderr,\s*([^\r]*\r\n)(?!\s*cleanup_exit\(0\))/mboxprintf\(\1/msg
		|| die "Error replacing stderr output plink.c";
}


# Add mboxprintf
if (!($PLINK =~ m/^void mboxprintf/))
{
	$s = <<END_OF_TEXT;
void mboxprintf(char *fmt, ...)
{
    va_list ap;
    char *stuff;

    va_start(ap, fmt);
    stuff = dupvprintf(fmt, ap);
    va_end(ap);
    MessageBox(GetParentHwnd(), stuff, "TortoisePlink",
	       MB_ICONERROR | MB_OK);
    sfree(stuff);
}
END_OF_TEXT
	
	chomp $s;
	$s =~ s/\n/\r\n/g;
	
	$PLINK =~ s/(void connection_fatal)/$s\r\n$1/ms
		|| die "Error adding function \"mboxprintf\" inplink.c";
}

# Fix MessageBox calls
$PLINK =~ s/(MessageBox\().*?,/$1GetParentHwnd\(\),/msg
	|| die "Error fixing MessageBox calls in plink.c"; 

# Store file
open(OUTHANDLE, "> plink.c") 
	|| die "Error opening plink.c";
print OUTHANDLE $PLINK;
close OUTHANDLE;

undef $WINDOW;
undef $PLINK;


####################################################
# Patch cmdline.c

$CMDLINE = `cat cmdline.c`;

# Add flag -V for version
# Include LoginDialog.h
if (!($CMDLINE =~ m/FLAG_VERSION/))
{
	$s = <<END_OF_TEXT;
    if (!strcmp(p, "-V")) {
        RETURN(1);
        flags |= FLAG_VERSION;
    }
END_OF_TEXT

	chomp $s;
	$s =~ s/\n/\r\n/g;
	$CMDLINE =~ s/(FLAG_VERBOSE;.*?\}).*?$/$1\r\n$s/ms
		|| die "Error adding version flag to cmdline.c";
}

# Store file
open(OUTHANDLE, "> cmdline.c") 
	|| die "Error opening cmdline.c";
print OUTHANDLE $CMDLINE;
close OUTHANDLE;

undef $WINDOW;
undef $PLINK;


####################################################
# Patch putty.h

$PUTTY = `cat putty.h`;

# Add FLAG_VERSION
if (!($PUTTY =~ m/FLAG_VERSION/))
{
	$PUTTY =~ s/(#define.*?FLAG_INTERACTIVE.*?0x0004)/$1\r\n#define FLAG_VERSION     0x0008/ms
		|| die "Error adding \"FLAG_VERSION\"to putty.h";
}

# Store file
open(OUTHANDLE, "> putty.h") 
	|| die "Error opening putty.h";
print OUTHANDLE $PUTTY;
close OUTHANDLE;

undef $PLINK;




####################################################
# Patch plink.rc

$PLINK = `cat plink.rc`;

# Include TortoisePlink.rc
if (!($PLINK =~ m/#include "TortoisePlink.rc"/))
{
	$PLINK = $PLINK . "\r\n#include \"TortoisePlink.rc\"\r\n";
}

# Store file
open(OUTHANDLE, "> plink.rc") 
	|| die "Error opening plink.rc";
print OUTHANDLE $PLINK;
close OUTHANDLE;

undef $PLINK;

