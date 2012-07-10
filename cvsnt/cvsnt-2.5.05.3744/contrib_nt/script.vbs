'
' example script.  Call this 'script.vbs' and add it to CVSROOT.  
'
' also supported: Javascript (.js), Perlscript (.pl), Pythonscript (.py), Rubyscript (.rb).
'
'
' Output is via the server object:
'
' Server.Trace(tracelevel, message)
' Server.Warning(message)
' Server.Error(message)
'
'
function init(command, current_date, hostname, username, virtual_repository, physical_repository, session_id, editor, user_variable_array, client_version, character_set)
Server.Warning("vbs init")
init = 0
end function

function close
Server.Warning("vbs close")
end function

function taginfo(message,directory, file_array, action, tag)
Server.Warning("vbs taginfo")
taginfo = 0
end function

function verifymsg(directory, filename)
Server.Warning("vbs verifymsg")
verifymsg = 0
end function

function loginfo(message, status, directory, change_array)
' change_array is array of:
'   filename
'   rev_old
'   rev_new

Server.Warning("vbs loginfo")
loginfo = 0
end function

function history(history_type, workdir, revs, name, bugid, message, dummy)
Server.Warning("vbs history")
history = 0
end function

function notify(message, bugid, directory, notify_user, tag, notify_type, file)
Server.Warning("vbs notify")
notify = 0
end function

function precommit(name_list, message, directory)
Server.Warning("vbs precommit")
precommit = 0
end function

function postcommit(directory)
Server.Warning("vbs postcommit")
postcommit = 0
end function

function precommand(argument_list)
Server.Warning("vbs precommand")
precommand = 0
end function

function postcommand(directory)
Server.Warning("vbs postcommand")
postcommand = 0
end function

function premodule(module)
Server.Warning("vbs premodule")
premodule = 0
end function

function postmodule(module)
Server.Warning("vbs postmodule")
postmodule = 0
end function

function get_template(directory)
Server.Warning("vbs get_template")
' return name of template file, or null
get_template = null
end function

function parse_keyword(keyword, directory, file, branch, author, printable_date, rcs_date, locker, state, version, name, bugid, commitid, global_properties, local_properties)
Server.Warning("vbs parse_keyword")
' return value of keyword, or null
parse_keyword = null
end function

function prercsdiff(file, directory, oldfile, newfile, diff_type, options, oldversion, newversion)
Server.Warning("vbs prercsdiff")
' return 1 if you want the rcsdiff function to be called
prercsdiff = 0
end function

function rcsdiff(file, directory, oldfile, newfile, diff, diff_type, options, oldversion, newversion, added, removed)
Server.Warning("vbs rcsdiff")
rcsdiff = 0
end function
