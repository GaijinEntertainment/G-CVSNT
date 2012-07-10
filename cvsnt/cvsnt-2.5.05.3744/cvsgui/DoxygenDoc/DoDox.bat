 @REM --- COMMAND LINE PARAMETERS ---

@REM --- SELECT THE TARGET ---

@set DOC_TARGET=Help

@if "%1%"=="" set		DOC_TARGET=Win32
@if "%1%"=="Win32" set	DOC_TARGET=Win32
@if "%1%"=="Win" set	DOC_TARGET=Win32
@if "%1%"=="W" set		DOC_TARGET=Win32
@if "%1%"=="w" set		DOC_TARGET=Win32

@if "%1%"=="Mac" set	DOC_TARGET=Mac
@if "%1%"=="M" set		DOC_TARGET=Mac
@if "%1%"=="m" set		DOC_TARGET=Mac

@if "%1%"=="Unix" set	DOC_TARGET=Unix
@if "%1%"=="U" set		DOC_TARGET=Unix
@if "%1%"=="u" set		DOC_TARGET=Unix

@REM --- DISPLAY HELP ---
@if %DOC_TARGET%==Help goto HELP


@REM --- SET THE ENVIRONMENT ---

@REM *Platform-specific variables*

@echo Building target: %DOC_TARGET%

@if %DOC_TARGET%==Win32 (
	@REM *Project*
	@set DOX_OUTPUT_DIRECTORY=cvsgui_protocol_WinDoc
	@set DOX_PREDEFINED=WIN32 __AFX_H__
)

@if %DOC_TARGET%==Mac (
	@REM *Project*
	@set DOX_OUTPUT_DIRECTORY=cvsgui_protocol_MacDoc
	@set DOX_PREDEFINED=__cplusplus=1
)

@if %DOC_TARGET%==Unix (
	@REM *Project*
	@set DOX_OUTPUT_DIRECTORY=cvsgui_protocol_UniDoc
	@set DOX_PREDEFINED=
)

@REM *Variable to strip from files path (STRIP_FROM_PATH)*
@REM You may need to hardcode the value of DOX_STRIP_FROM_PATH on NT on Win9x that doesn't support PWD for CD command
@set PWD_ORG=%CD%
@cd ..
@set DOX_STRIP_FROM_PATH=%CD:\=/%
@cd %PWD_ORG%

@REM *Project*
@set DOX_PROJECT_NAME=cvsgui protocol (%DOC_TARGET%)
@set DOX_PROJECT_VERSION=1.0

@REM *Input*
@set DOX_INPUT=..
@set DOX_INCLUDE_PATH=
@set DOX_FILE_PATTERNS=*.c *.cpp *.h *.txt
@set DOX_EXCLUDE_PATTERNS=*/CVS/* */SourceDoc/*

@REM *Output settings*
@set DOX_CASE_SENSE_NAMES=NO
@set DOX_SHORT_NAMES=NO
@set DOX_OUTPUT_LANG=English
@set DOX_USE_WINDOWS_ENCODING=YES

@REM *Output types*
@set DOX_GENERATE_HTML=YES
@set DOX_DOT_IMAGE_FORMAT=png
@set DOX_GENERATE_HTMLHELP=YES
@set DOX_GENERATE_TREEVIEW=NO
@set DOX_GENERATE_LATEX=NO
@set DOX_GENERATE_RTF=NO
@set DOX_GENERATE_MAN=NO
@set DOX_GENERATE_XML=NO
@set DOX_GENERATE_AUTOGEN_DEF=NO
@set DOX_SEARCHENGINE=NO


@REM --- SET THE FILES ---

@rem Execution log file
@set DOX_LOG=DoDox.log

@rem HHC project file
@set DOX_HHP=%DOX_OUTPUT_DIRECTORY%\html\index.hhp


@REM --- EXECUTE ---

@echo Generating the documentation
@echo Start: %DATE% %TIME% > %DOX_LOG%
@doxygen.exe >> %DOX_LOG% 2>&1 

@echo Creating the compressed html file
@if EXIST %DOX_HHP% hhc.exe %DOX_HHP% >> %DOX_LOG%
@echo Finished: %DATE% %TIME% >> %DOX_LOG%

goto EXIT

@REM --- DISPLAY HELP ---

:HELP
@REM DISPLAY THE HELP INFO
@type Readme.txt | more
@PAUSE


@REM --- EXIT ---
:EXIT
