@echo off
:: Build HTML Help documentation

:: Copy XML source and images to "repository" folder
set REPO=c:\docbook\repository\UserGuide_en
del /q %REPO%\*.xml
md %REPO% 2> nul:
md %REPO%\figure 2> nul:
xcopy /r/y/q UserGuide\*.xml %REPO%
xcopy /r/y/q UserGuide\figure\* %REPO%\figure

:: Copy our custom stylesheet
xcopy /r/y/q document_UserGuide_en_htmlhelp.xsl c:\docbook\stylesheet

:: Process
pushd .
call docbook_htmlhelp UserGuide_en
popd

if not %errorlevel%==0 goto error_docbook

:: Copy generated file
xcopy /r/y/q c:\docbook\output\UserGuide_en\htmlhelp\UserGuide_en.chm .

:error_docbook