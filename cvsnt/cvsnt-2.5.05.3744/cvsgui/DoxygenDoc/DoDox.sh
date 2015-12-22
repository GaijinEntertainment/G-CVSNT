#!/bin/sh

# DISPLAY THE HELP INFO
help() {
	more Readme.txt | more
}

# --- COMMAND LINE PARAMETERS ---
case "$1" in
  "")
    # try to guess what's the right thing to build by default:
    #  If we're running on Mac OS X, we'll build the Mac documentation, else we'll build the Unix documentation
    #  To find out whether we're running on Mac OS X, check whether the Cocoa framework folder exists
    #
    if [ -d /System/Library/Frameworks/Cocoa.framework ]; then
      whichDox="Mac"
    else
      whichDox="Unix"
    fi
    ;;
  
  "M"|"m"|"Mac")
    whichDox="Mac"
    ;;
  
  "U"|"u"|"Unix")
    whichDox="Unix"
    ;;
  
  "W"|"w"|"Win"|"Win32")
    whichDox="Win32"
    ;;
  
  *)
    help
    exit
    ;;
esac

# --- SET THE ENVIRONMENT ---

# *Platform-specific variables*
case "$whichDox" in
  "Mac")
    export DOX_OUTPUT_DIRECTORY=cvsgui_protocol_MacDoc
    export DOX_PREDEFINED="__cplusplus=1"
  ;;
  
  "Unix")
    export DOX_OUTPUT_DIRECTORY=cvsgui_protocol_UniDoc
    export DOX_PREDEFINED=
  ;;
  
  "Win32")
    export DOX_OUTPUT_DIRECTORY=cvsgui_protocol_WinDoc
    export DOX_PREDEFINED=WIN32 __AFX_H__
  ;;
esac

# *Input*
export DOX_INPUT=".."
export DOX_INCLUDE_PATH=
export DOX_FILE_PATTERNS="*.c *.cpp *.h *.txt"
export DOX_EXCLUDE_PATTERNS="*/CVS/* */SourceDoc/*"

# *Variable to strip from files path (STRIP_FROM_PATH)*
PWD_ORG=`pwd`
cd ..
export DOX_STRIP_FROM_PATH=`pwd`
cd "$PWD_ORG"

# *Project*
export DOX_PROJECT_NAME="cvsgui protocol ($whichDox)"
export DOX_PROJECT_VERSION="1.0"

# *Output settings*
export DOX_CASE_SENSE_NAMES=NO
export DOX_SHORT_NAMES=YES
export DOX_MAN_EXT=4
export DOX_OUTPUT_LANG=English
export DOX_USE_WINDOWS_ENCODING=NO

# *Output types*
export DOX_GENERATE_HTML=YES
export DOX_DOT_IMAGE_FORMAT=gif
export DOX_GENERATE_HTMLHELP=YES
export DOX_GENERATE_TREEVIEW=NO
export DOX_GENERATE_LATEX=NO
export DOX_GENERATE_RTF=NO
export DOX_GENERATE_MAN=NO
export DOX_GENERATE_XML=NO
export DOX_GENERATE_AUTOGEN_DEF=NO
export DOX_SEARCHENGINE=NO


# --- SET THE FILES ---

#@rem Execution log file
DOX_LOG=DoDox.log

#@rem HHC project file
#export DOX_HHP=$DOX_OUTPUT_DIRECTORY\html\index.hhp


# --- EXECUTE ---

echo Generating the documentation for $whichDox
echo Start: `date +"%d.%m.%y %H:%M:%S"`> $DOX_LOG
doxygen >> $DOX_LOG 2>&1 

#echo Creating the compressed html file
#if EXIST %DOX_HHP% hhc.exe %DOX_HHP% >> %DOX_LOG%

echo Finished: `date +"%d.%m.%y %H:%M:%S"` >> $DOX_LOG

