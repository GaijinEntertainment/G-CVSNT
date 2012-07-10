#!/usr/bin/env python
import os
import sys
import fileinput
import shutil
import filecmp
import getopt

def usage():
  print 'testcvs [options]'
  print '  -v  --verbose    verbose output'
  
def cvs(command):
  global count, verbose
  #cmd = 'valgrind -q --logfile-fd=9 cvs --allow-root='+base_dir+'/repos,/repos -d'+current_cvsroot+' '+command+' >'+outfile+' 2>'+errfile+' 9>/dev/stderr'
  cmd = 'cvs --allow-root="'+current_physroot+','+current_cvsroot+'" -d'+current_cvsroot+' '+command+' >"'+outfile+'" 2>"'+errfile+'"'
  if(verbose): print count,': ',cmd
  count = count + 1
  result = os.system(cmd)
  if(verbose):
    cat(errfile)
    cat(outfile)
  return result

def cat(file):
  for line in fileinput.input(file):
    if line and line[-1] == '\n':
      line = line[:-1]
    print line

def fail(command, result):
  print 'Test \''+current_test+'\' failed ('+command+') (result='+str(result)+')'
  cat (errfile)
  raise SystemExit

def chdir(directoryname):
  print 'chdir '+directoryname
  os.chdir(directoryname)

def cvs_pass(command):
  result = cvs(command)
  if(result != 0): fail(command,result)

def cvs_fail(command):
  result = cvs(command)
  if(result == 0): fail(command,result)

def start_test(name):
  global current_test
  current_test = name
  print current_test

def dir_exists(dirname):
  if(not os.path.isdir(dirname)):
    print 'Directory '+dirname+' Which should exist, doesn\'t.  Terminating.'
    raise SystemExit

def file_exists(filename):
  if(not os.path.isfile(filename)):
    print 'File '+filename+' Which should exist, doesn\'t.  Terminating.'
    raise SystemExit

def file_not_exists(filename):
  if(os.path.isfile(filename)):
    print 'File '+filename+' Which shouldn\'t exist, does.  Terminating.'
    raise SystemExit

def file_copy(srcfile,destfile):
  if(verbose): print "Copy "+srcfile+" -> "+destfile
  shutil.copyfile(srcfile,destfile) 

def file_compare(file1,file2):
  if(verbose): print "Compare "+file1+" -> "+file2
  if(filecmp.cmp(file1,file2) == 0):
    print 'File '+file1+' Should be identical to File '+file2+'. Terminating.'
    raise SystemExit

def file_delete(filename):
  if(verbose): print "Delete "+filename
  os.unlink(filename)

def main():
  global verbose, base_dir, current_cvsroot, current_physroot, outfile, errfile, count
  try:
    opts, args = getopt.getopt(sys.argv[1:], "vai:", ["verbose"])
  except getopt.GetoptError:
    usage()
    sys.exit(2)
  verbose = 0
  instance = '0'
  for o,a in opts:
    if o in ("-v", "--verbose"):
      verbose = 1
    if o in ("-i", "--instance"):
       instance = a

  base_dir = os.getcwd()
  outfile = base_dir+'/testcvs_'+instance+'.out'
  errfile = base_dir+'/testcvs_'+instance+'.err'
  current_cvsroot = '/repos'
  current_physroot = base_dir+current_cvsroot+'_'+instance
  current_tree = base_dir+'/tree_'+instance
  current_test = '(none)'
  test_data = base_dir+'/test_data'
  try:
    shutil.rmtree(current_tree) 
  except OSError:
    0
  try:
    shutil.rmtree(current_physroot) 
  except OSError:
    0
  os.mkdir(current_physroot)
  os.mkdir(current_tree)

  count = 1

  start_test('Basic functionality, Init, Import, Checkout')
  cvs_pass('-v')
#  cvs_fail('version')
  cvs_pass('init -n')
  dir_exists(current_physroot+'/CVSROOT')
  file_exists(current_physroot+'/CVSROOT/checkoutlist')
  file_exists(current_physroot+'/CVSROOT/checkoutlist,v')
  file_exists(current_physroot+'/CVSROOT/commitinfo')
  file_exists(current_physroot+'/CVSROOT/commitinfo,v')
  file_exists(current_physroot+'/CVSROOT/config')
  file_exists(current_physroot+'/CVSROOT/config,v')
  file_exists(current_physroot+'/CVSROOT/cvswrappers')
  file_exists(current_physroot+'/CVSROOT/cvswrappers,v')
#  file_exists(current_physroot+'/CVSROOT/history')
  file_exists(current_physroot+'/CVSROOT/loginfo')
  file_exists(current_physroot+'/CVSROOT/loginfo,v')
  file_exists(current_physroot+'/CVSROOT/modules')
  file_exists(current_physroot+'/CVSROOT/modules,v')
  file_exists(current_physroot+'/CVSROOT/notify')
  file_exists(current_physroot+'/CVSROOT/notify,v')
  file_exists(current_physroot+'/CVSROOT/rcsinfo')
  file_exists(current_physroot+'/CVSROOT/rcsinfo,v')
  file_exists(current_physroot+'/CVSROOT/taginfo')
  file_exists(current_physroot+'/CVSROOT/taginfo,v')
  file_exists(current_physroot+'/CVSROOT/verifymsg')
  file_exists(current_physroot+'/CVSROOT/verifymsg,v')
  chdir(test_data + '/import_test')
  cvs_pass('import -n -m "Initial import" testcvs')
  dir_exists(current_physroot+'/testcvs')
  dir_exists(current_physroot+'/testcvs/sub')
  dir_exists(current_physroot+'/testcvs/sub2')
  file_exists(current_physroot+'/testcvs/test1.txt,v')
  file_exists(current_physroot+'/testcvs/test2.txt,v')
  file_exists(current_physroot+'/testcvs/sub/test3.txt,v')
  file_exists(current_physroot+'/testcvs/sub/test4.txt,v')
  file_exists(current_physroot+'/testcvs/sub2/test5.txt,v')
  file_exists(current_physroot+'/testcvs/sub2/test6.txt,v') 
  cvs_pass('version')
  chdir(current_tree)
  cvs_fail('co cvsfailtest')
  cvs_pass('co testcvs')
  dir_exists(current_tree+'/testcvs')
  dir_exists(current_tree+'/testcvs/CVS')
  dir_exists(current_tree+'/testcvs/sub')
  dir_exists(current_tree+'/testcvs/sub2')
  file_exists(current_tree+'/testcvs/test1.txt')
  file_exists(current_tree+'/testcvs/test2.txt')
  file_exists(current_tree+'/testcvs/sub/test3.txt')
  file_exists(current_tree+'/testcvs/sub/test4.txt')
  file_exists(current_tree+'/testcvs/sub2/test5.txt')
  file_exists(current_tree+'/testcvs/sub2/test6.txt')
  file_compare(test_data+'/import_test/test1.txt',current_tree+'/testcvs/test1.txt')
  file_compare(test_data+'/import_test/test2.txt',current_tree+'/testcvs/test2.txt')
  file_compare(test_data+'/import_test/sub/test3.txt',current_tree+'/testcvs/sub/test3.txt')
  file_compare(test_data+'/import_test/sub/test4.txt',current_tree+'/testcvs/sub/test4.txt')
  file_compare(test_data+'/import_test/sub2/test5.txt',current_tree+'/testcvs/sub2/test5.txt')
  file_compare(test_data+'/import_test/sub2/test6.txt',current_tree+'/testcvs/sub2/test6.txt')

###
#  chdir(current_tree+'/testcvs')
#  fname = 'test1.txt'
#  module = 'testcvs'
#  for fcnt in range(20000):
#    cvs_pass('ci -m change1 '+fname)
#    cvs_pass('-z7 log '+fname)
#    cvs_pass('stat '+fname)
#    cvs_pass('-z7 up '+fname)
#    cvs_pass('diff -r1 '+fname)
#    cvs_pass('tag -F aa'+str(fcnt)+' '+fname)
#    cvs_pass('stat -v '+fname)
#    cvs_pass('editors '+fname)
##    cvs_pass('history '+fname)
#    cvs_pass('watchers '+fname)
#    file_copy(test_data+'/diff_test.txt.2',current_tree+'/testcvs/'+fname)
#    cvs_pass('ci -m change2 '+fname)
#    cvs_pass('-z6 diff -r1 '+fname)
#    cvs_pass('rtag -b branch_'+str(fcnt)+' '+module)
#    cvs_pass('rlog '+module)
#    file_copy(test_data+'/maastrict.txt',current_tree+'/testcvs/'+fname)
#    cvs_pass('-z7 ci -m grow '+fname)
#    os.unlink(fname)
#    cvs_pass('up '+fname)
#    file_copy(test_data+'/add_test.txt',current_tree+'/testcvs/'+fname)
#    cvs_pass('ci -m shrink '+fname)
#  exit(1)
###

  start_test('Basic Add, Remove, Resurrect, Commit')

  chdir(current_tree+'/testcvs')
  file_copy(test_data+'/add_test.txt','add_test.txt')
  cvs_pass('add add_test.txt')
  file_exists(current_tree+'/testcvs/add_test.txt')
  file_not_exists(current_physroot+'/testcvs/add_test.txt,v')
  cvs_pass('commit -m "" add_test.txt')
  file_exists(current_tree+'/testcvs/add_test.txt')
  file_exists(current_physroot+'/testcvs/add_test.txt,v')
  cvs_pass('remove -f add_test.txt')
  file_not_exists(current_tree+'/testcvs/add_test.txt')
  file_exists(current_physroot+'/testcvs/add_test.txt,v')
  cvs_pass('add add_test.txt')
  file_exists(current_tree+'/testcvs/add_test.txt')
  file_exists(current_physroot+'/testcvs/add_test.txt,v')
  cvs_pass('commit -m "" add_test.txt')
  cvs_pass('remove -f add_test.txt')
  file_not_exists(current_tree+'/testcvs/add_test.txt')
  file_exists(current_physroot+'/testcvs/add_test.txt,v')
  cvs_pass('commit -m "" add_test.txt')
  file_not_exists(current_tree+'/testcvs/add_test.txt')
#  file_not_exists(current_physroot+'/testcvs/add_test.txt,v')
#  dir_exists(current_physroot+'/testcvs/Attic')
#  file_exists(current_physroot+'/testcvs/Attic/add_test.txt,v')
  file_copy(test_data+'/add_test.txt','add_test.txt')
  cvs_pass('add add_test.txt')
  cvs_pass('commit -m "" add_test.txt')
  file_exists(current_tree+'/testcvs/add_test.txt')
  file_exists(current_physroot+'/testcvs/add_test.txt,v')
  cvs_pass('remove -f add_test.txt')
  file_not_exists(current_tree+'/testcvs/add_test.txt')
  cvs_pass('commit -m "" add_test.txt')
  file_not_exists(current_tree+'/testcvs/add_test.txt')
#  file_not_exists(current_physroot+'/testcvs/add_test.txt,v')
#  dir_exists(current_physroot+'/testcvs/Attic')
#  file_exists(current_physroot+'/testcvs/Attic/add_test.txt,v')
  cvs_pass('remove -f add_test.txt') # Should fail IMHO but standard cvs doesn't
  cvs_fail('commit -m "" fail_test.txt')

  start_test('Basic binary Add/Checkout')
  
  chdir(current_tree+'/testcvs')
  file_copy(test_data+'/binary_test.gif',current_tree+'/testcvs/binary_test.gif')
  cvs_pass('add -kb binary_test.gif')
  cvs_pass('commit -m "" binary_test.gif')
  file_exists(current_tree+'/testcvs/binary_test.gif')
  file_exists(current_physroot+'/testcvs/binary_test.gif,v')
  file_compare(test_data+'/binary_test.gif',current_tree+'/testcvs/binary_test.gif')
  file_delete(current_tree+'/testcvs/binary_test.gif')
  cvs_pass('update binary_test.gif')
  file_compare(test_data+'/binary_test.gif',current_tree+'/testcvs/binary_test.gif')

  start_test('Binary remove and revert')
  
  file_copy(test_data+'/binary_test.gif',current_tree+'/testcvs/binary_rm_test.gif')
  cvs_pass('add -kb binary_rm_test.gif')
  cvs_pass('commit -m "" binary_rm_test.gif')
  file_exists(current_tree+'/testcvs/binary_rm_test.gif')
  cvs_pass('remove -f binary_rm_test.gif')
  file_not_exists(current_tree+'/testcvs/binary_rm_test.gif')
  cvs_pass('commit -m "" binary_rm_test.gif')
  cvs_pass('update -r 1.1 binary_rm_test.gif')
  file_exists(current_tree+'/testcvs/binary_rm_test.gif')
  file_compare(test_data+'/binary_test.gif',current_tree+'/testcvs/binary_rm_test.gif')

  start_test('Binary delta Add/Checkout')
  
  chdir(current_tree+'/testcvs')
  file_copy(test_data+'/binary_test.gif',current_tree+'/testcvs/binary_delta_test.gif')
  cvs_pass('add -kB binary_delta_test.gif')
  cvs_pass('commit -m "" binary_delta_test.gif')
  file_exists(current_tree+'/testcvs/binary_delta_test.gif')
  file_exists(current_physroot+'/testcvs/binary_delta_test.gif,v')
  file_compare(test_data+'/binary_test.gif',current_tree+'/testcvs/binary_delta_test.gif')
  file_delete(current_tree+'/testcvs/binary_delta_test.gif')
  cvs_pass('update binary_delta_test.gif')
  file_compare(test_data+'/binary_test.gif',current_tree+'/testcvs/binary_delta_test.gif')

  start_test('Add/Checkout large file')

  file_copy(test_data+'/maastrict.txt',current_tree+'/testcvs/maastrict.txt')
  cvs_pass('add maastrict.txt')
  cvs_pass('commit -m "" maastrict.txt')
  file_exists(current_tree+'/testcvs/maastrict.txt')
  file_exists(current_physroot+'/testcvs/maastrict.txt,v')
  file_compare(test_data+'/maastrict.txt',current_tree+'/testcvs/maastrict.txt')
  file_delete(current_tree+'/testcvs/maastrict.txt')
  cvs_pass('update maastrict.txt')
  file_compare(test_data+'/maastrict.txt',current_tree+'/testcvs/maastrict.txt')

  start_test('Commit 50 revisions of a small file')
  for i in range(25):
#  start_test('Infinite commits of small file')
#  while(1):
    file_copy(test_data+'/diff_test.txt.1',current_tree+'/testcvs/test1.txt')
    cvs_pass('commit -f -m "" test1.txt')
    file_copy(test_data+'/diff_test.txt.2',current_tree+'/testcvs/test1.txt')
    cvs_pass('commit -f -m "" test1.txt')

  start_test('Commit 50 revisions of a large file')
  for i in range(25):
    file_copy(test_data+'/diff_test.txt.1',current_tree+'/testcvs/maastrict.txt')
    cvs_pass('commit -f -m "" maastrict.txt')
    file_copy(test_data+'/maastrict.txt',current_tree+'/testcvs/maastrict.txt')
    cvs_pass('commit -f -m "" maastrict.txt') 

  start_test('Checkout/Diff different versions of a text file')

  chdir(current_tree+'/testcvs')
  file_copy(test_data+'/diff_test.txt.1',current_tree+'/testcvs/diff_test.txt')
  cvs_pass('add diff_test.txt')
  cvs_pass('commit -m "" diff_test.txt')
  file_copy(test_data+'/diff_test.txt.2',current_tree+'/testcvs/diff_test.txt')
  cvs_pass('commit -f -m "" diff_test.txt')
  cvs_pass('tag sticky_tag_test_symbolic_tag diff_test.txt')
  file_copy(test_data+'/diff_test.txt.3',current_tree+'/testcvs/diff_test.txt')
  cvs_pass('commit -f -m "" diff_test.txt')
  file_copy(test_data+'/diff_test.txt.4',current_tree+'/testcvs/diff_test.txt')
  cvs_pass('commit -f -m "" diff_test.txt')
  cvs_pass('update -r 1.1 diff_test.txt')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  file_compare(test_data+'/diff_test.txt.1',current_tree+'/testcvs/diff_test.txt')
  cvs_pass('update -r 1.2 diff_test.txt')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  file_compare(test_data+'/diff_test.txt.2',current_tree+'/testcvs/diff_test.txt')
  cvs_pass('update -r 1.3 diff_test.txt')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  file_compare(test_data+'/diff_test.txt.3',current_tree+'/testcvs/diff_test.txt')
  cvs_pass('update -r 1.4 diff_test.txt')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  file_compare(test_data+'/diff_test.txt.4',current_tree+'/testcvs/diff_test.txt')
  cvs_pass('update -r 1.1 diff_test.txt')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  file_compare(test_data+'/diff_test.txt.1',current_tree+'/testcvs/diff_test.txt')
  cvs_pass('update -A diff_test.txt')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  file_compare(test_data+'/diff_test.txt.4',current_tree+'/testcvs/diff_test.txt')
  cvs_fail('-q diff -r 1.1 -r 1.3 diff_test.txt') # CVS always returns 1
  file_compare(outfile,test_data+'/diff_test.diff.1')
  cvs_fail('-q diff -r 1.3 -r 1.1 diff_test.txt') # CVS always returns 1
  file_compare(outfile,test_data+'/diff_test.diff.2')
  cvs_fail('-q diff -r 1.2 diff_test.txt') # CVS always returns 1
  file_compare(outfile,test_data+'/diff_test.diff.3')

  start_test('Checkout different versions of a binary file')

  file_copy(test_data+'/binary_test3.gif',current_tree+'/testcvs/binary_test.gif')
  cvs_pass('commit -m "" binary_test.gif')
  cvs_pass('tag sticky_tag_test_symbolic_tag binary_test.gif')
  file_copy(test_data+'/binary_test4.gif',current_tree+'/testcvs/binary_test.gif')
  cvs_pass('commit -m "" binary_test.gif')
  cvs_pass('tag sticky_tag_test_symbolic_tag')
  cvs_pass('update -r 1.1 binary_test.gif')
  file_exists(current_tree+'/testcvs/binary_test.gif')
  file_compare(test_data+'/binary_test.gif',current_tree+'/testcvs/binary_test.gif')
  cvs_pass('update -r 1.2 binary_test.gif')
  file_exists(current_tree+'/testcvs/binary_test.gif')
  file_compare(test_data+'/binary_test3.gif',current_tree+'/testcvs/binary_test.gif')
  cvs_pass('update -r 1.3 binary_test.gif')
  file_exists(current_tree+'/testcvs/binary_test.gif')
  file_compare(test_data+'/binary_test4.gif',current_tree+'/testcvs/binary_test.gif')

  start_test('Branching')

  cvs_pass('update -r 1.3 diff_test.txt')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  cvs_pass('tag -b branch_test diff_test.txt')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  cvs_pass('update -r branch_test diff_test.txt')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  file_copy(test_data+'/branch_test.txt.1',current_tree+'/testcvs/diff_test.txt')
  cvs_pass('commit -m "" diff_test.txt')
  file_copy(test_data+'/branch_test.txt.2',current_tree+'/testcvs/diff_test.txt')
  cvs_pass('commit -m "" diff_test.txt')
  cvs_pass('log -t diff_test.txt')
  file_compare(outfile,test_data+'/branch_test.txt.3')

  start_test('Sticky tag test (symbolic)')

  cvs_pass('update -r sticky_tag_test_symbolic_tag')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  file_compare(test_data+'/diff_test.txt.2',current_tree+'/testcvs/diff_test.txt')
  file_exists(current_tree+'/testcvs/binary_test.gif')
  file_compare(test_data+'/binary_test3.gif',current_tree+'/testcvs/binary_test.gif')

  start_test('Sticky tag test (symbolic+update)')
  cvs_pass('update')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  file_compare(test_data+'/diff_test.txt.2',current_tree+'/testcvs/diff_test.txt')
  file_exists(current_tree+'/testcvs/binary_test.gif')
  file_compare(test_data+'/binary_test3.gif',current_tree+'/testcvs/binary_test.gif')
  cvs_pass('update -A')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  file_copy(test_data+'/diff_test.txt.4',current_tree+'/testcvs/diff_test.txt')
  file_exists(current_tree+'/testcvs/binary_test.gif')
  file_compare(test_data+'/binary_test4.gif',current_tree+'/testcvs/binary_test.gif')
  
  start_test('Sticky tag test (revision)')
  cvs_pass('update -r 1.2 diff_test.txt')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  file_compare(test_data+'/diff_test.txt.2',current_tree+'/testcvs/diff_test.txt')
  cvs_pass('update -r 1.2 binary_test.gif')
  file_exists(current_tree+'/testcvs/binary_test.gif')
  file_compare(test_data+'/binary_test3.gif',current_tree+'/testcvs/binary_test.gif')

  start_test('Sticky tag test (revision+update)')
  cvs_pass('update')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  file_compare(test_data+'/diff_test.txt.2',current_tree+'/testcvs/diff_test.txt')
  file_exists(current_tree+'/testcvs/binary_test.gif')
  file_compare(test_data+'/binary_test3.gif',current_tree+'/testcvs/binary_test.gif')
  cvs_pass('update -A')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  file_copy(test_data+'/diff_test.txt.4',current_tree+'/testcvs/diff_test.txt')
  file_exists(current_tree+'/testcvs/binary_test.gif')
  file_compare(test_data+'/binary_test4.gif',current_tree+'/testcvs/binary_test.gif')

  start_test('Merging')

  cvs_pass('update -A diff_test.txt')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  cvs_fail('diff -r branch_test diff_test.txt')
  file_compare(outfile,test_data+'/merge_test.txt.1')
  cvs_pass('update -j branch_test diff_test.txt')
  file_compare(outfile,test_data+'/merge_test.txt.2')
  file_compare('diff_test.txt',test_data+'/merge_test.txt.3')
  file_copy(test_data+'/merge_test.txt.4',current_tree+'/testcvs/diff_test.txt')
  cvs_pass('commit -m "" diff_test.txt')
  cvs_pass('update -r branch_test diff_test.txt')
  file_copy(test_data+'/merge_test.txt.5',current_tree+'/testcvs/diff_test.txt')
  cvs_pass('commit -m "" diff_test.txt')
  cvs_pass('update -A diff_test.txt')
  cvs_pass('update -m -j branch_test diff_test.txt')
  file_compare('diff_test.txt',test_data+'/merge_test.txt.6')
  os.unlink('diff_test.txt')
  cvs_pass('update diff_test.txt')
  cvs_pass('update -b -j branch_test diff_test.txt')
  file_compare('diff_test.txt',test_data+'/merge_test.txt.7')

  start_test('*info')

  os.chmod(current_physroot+'/CVSROOT/commitinfo',0644)
  os.chmod(current_physroot+'/CVSROOT/loginfo',0644)
  os.chmod(current_physroot+'/CVSROOT/postcommand',0644)
  file_copy(test_data + '/commitinfo_test', current_physroot+'/CVSROOT/commitinfo')
  file_copy(test_data + '/loginfo_test', current_physroot+'/CVSROOT/loginfo')
  file_copy(test_data + '/postcommand_test', current_physroot+'/CVSROOT/postcommand')
  cvs_pass('commit -f -m "info test" diff_test.txt')
  file_compare(outfile, test_data+'/info_test_output.txt')

if __name__ == "__main__":
  main()

