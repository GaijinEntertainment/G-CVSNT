@echo off
set path=c:\docbook\bat;c:\docbook\bin;%path%
:: Build HTML Help documentation

:: Copy XML source and images to "repository" folder
set REPO=c:\docbook\repository\UserGuide_ru
del /q %REPO%\*.xml
md %REPO% 2> nul:
md %REPO%\figure 2> nul:
xcopy /r/y/q UserGuide_ru\*_ru.xml %REPO%
xcopy /r/y/q UserGuide_Ru\figure\* %REPO%\figure

:: Copy our custom stylesheet
xcopy /r/y/q document_UserGuide_ru_htmlhelp.xsl c:\docbook\stylesheet

:: Process
pushd .
call docbook_htmlhelp UserGuide_ru
popd

if not %errorlevel%==0 goto error_docbook

:: Copy generated file
xcopy /r/y/q c:\docbook\output\UserGuide_ru\htmlhelp\UserGuide_ru.chm .

:error_docbook