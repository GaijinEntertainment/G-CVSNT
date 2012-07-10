[Code]

{  TortoiseCVS - a Windows shell extension for easy version control             }
{                                                                               }
{  Inno Setup install script Pascal code                                        }
{										}
{ Copyright (C) 2011 - Torsten Martinsen					}
{ <torsten@bullestock.net>                                                      }
{                                                                               }
{  This program is free software; you can redistribute it and/or                }
{  modify it under the terms of the GNU General Public License                  }
{  as published by the Free Software Foundation; either version 2               }
{  of the License, or (at your option) any later version.                       }
{                                                                               }
{  This program is distributed in the hope that it will be useful,              }
{  but WITHOUT ANY WARRANTY; without even the implied warranty of               }
{  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                }
{  GNU General Public License for more details.                                 }
{                                                                               }
{  You should have received a copy of the GNU General Public License            }
{  along with this program; if not, write to the Free Software                  }
{  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.  }

var
   bNeedRestart        : Boolean;
   
{ External functions }
   
procedure Debug(msg : String);
begin
   MsgBox(msg, mbInformation, MB_OK);
end;
   
{  Delete old MSVC DLL's installed by older TortoiseCVS versions }
procedure DeleteMsvcDlls;
var
   slFiles : TStringList;
   i       : Integer;
begin
   slFiles := TStringList.Create;
   slFiles.Add('{app}\msvcrt.dll');
   slFiles.Add('{app}\msvcp60.dll');
   slFiles.Add('{app}\msvcr70.dll');
   slFiles.Add('{app}\msvcp70.dll');

   for i := 0 to slFiles.Count - 1 do
   begin
      slFiles.Strings[i] := ExpandConstant(slFiles.Strings[i]);
      if FileExists(slFiles.Strings[i]) then
      begin
         if not DeleteFile(slFiles.Strings[i]) then
         begin
            RestartReplace(slFiles.Strings[i], '');
            bNeedRestart := True;
         end;
      end;
   end;
   
   slFiles.Free;
end;


{ This is the first function called }
function InitializeSetup: Boolean;
begin
    bNeedRestart := False;
    Result := True;
end;


function NeedRestart: Boolean;
begin
   Log('# NeedRestart: Return ' + IntToStr(Integer(bNeedRestart)));
   Result := bNeedRestart;
end;


function UninstallNeedRestart: Boolean;
begin
   Log('# UninstallNeedRestart: Return ' + IntToStr(Integer(bNeedRestart)));
   Result := bNeedRestart;
end;



Procedure CurStepChanged(CurStep: TSetupStep);
Var
    fileName : String;
    exitCode : Integer;
    args     : String;
    src      : String;
    dst      : String;
Begin
   If CurStep = ssPostInstall Then
   Begin
      Log('# Finishing install');
      DeleteMsvcDlls;
   end;
End; { CurStepChanged }
