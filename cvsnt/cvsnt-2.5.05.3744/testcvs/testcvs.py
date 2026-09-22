#!/usr/bin/env python3
"""Scenario suite for CVSNT, run against local repositories.

The scenarios are arranged in groups.  Every group builds its own repository
and working copy and runs its scenarios in a fixed order, because the golden
outputs in test_data pin revision and branch numbers that only come out right
after the whole sequence.  A scenario that fails blocks the rest of its own
group and nothing else, so one broken area no longer hides every other area.

Usage:
    python testcvs.py [--cvs <path>] [--libdir <dir>] [-i N] [-v]

--cvs defaults to the cvs found on PATH.  --libdir holds the protocol and
trigger plugins and becomes the global -L option; it is needed when cvs is run
from a build tree rather than an installation.

Exit status is 0 only if every scenario passed.
"""

import argparse
import filecmp
import os
import shlex
import shutil
import subprocess
import sys

CVS = 'cvs'
LIBDIR = None
TIMEOUT = 300
VERBOSE = False

base_dir = None
test_data = None
current_cvsroot = '/repos'
current_physroot = None
current_tree = None
outfile = None
errfile = None
count = 1
current_test = '(none)'

PASSED = []
FAILED = []
BLOCKED = []
XFAILED = []


class ScenarioFailed(Exception):
  """A check inside a scenario did not hold.

  reason is phrased so the report line reads Test '<scenario>' failed
  (<reason>), which is the wording the CI log gate looks for.  Extra context,
  if there is any, goes in detail."""

  def __init__(self, reason, detail=None):
    Exception.__init__(self, reason)
    self.reason = reason
    self.detail = detail


def cvs(command):
  global count
  args = [CVS]
  if LIBDIR:
    args += ['-L', LIBDIR]
  args += ['--allow-root=' + current_physroot + ',' + current_cvsroot,
           '-d' + current_cvsroot]
  args += shlex.split(command, posix=True)
  if(VERBOSE): print(count, ': ', ' '.join(args))
  count = count + 1
  with open(outfile, 'wb') as out, open(errfile, 'wb') as err:
    try:
      p = subprocess.run(args, stdout=out, stderr=err, timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
      raise ScenarioFailed('%s) (no exit within %d s' % (command, TIMEOUT))
    except OSError as e:
      raise ScenarioFailed('%s) (cannot run %s: %s' % (command, CVS, e))
  if(VERBOSE):
    cat(errfile)
    cat(outfile)
  return p.returncode


def read_text(file):
  with open(file, 'rb') as f:
    data = f.read()
  # A log may quote repository content in any encoding; reading it must never
  # crash the run.
  return data.decode('utf-8', 'backslashreplace')


def cat(file):
  text = read_text(file).rstrip('\n')
  if text:
    print(text)


def tail(file, lines=20):
  text = read_text(file).rstrip('\n')
  if not text:
    return None
  split = text.split('\n')
  if len(split) > lines:
    split = ['...'] + split[-lines:]
  return '\n'.join(split)


def outcome(rc):
  """The exit code as text; a death by signal must not read as an error code."""
  if rc < 0:
    return 'killed by signal %d' % -rc
  return 'result=%d' % rc


def chdir(directoryname):
  if(VERBOSE): print('chdir ' + directoryname)
  os.chdir(directoryname)


def cvs_pass(command):
  rc = cvs(command)
  if(rc != 0):
    raise ScenarioFailed('%s) (%s' % (command, outcome(rc)), tail(errfile))


def cvs_fail(command):
  rc = cvs(command)
  if(rc == 0):
    raise ScenarioFailed('%s) (expected a non-zero exit, got 0' % command,
                         tail(errfile))
  if(rc < 0):
    raise ScenarioFailed('%s) (%s, and a crash is not the expected failure'
                         % (command, outcome(rc)), tail(errfile))


def start_test(name):
  global current_test
  current_test = name
  print(current_test)


def dir_exists(dirname):
  if(not os.path.isdir(dirname)):
    raise ScenarioFailed('directory ' + dirname + ' which should exist, does not')


def file_exists(filename):
  if(not os.path.isfile(filename)):
    raise ScenarioFailed('file ' + filename + ' which should exist, does not')


def file_not_exists(filename):
  if(os.path.isfile(filename)):
    raise ScenarioFailed('file ' + filename + ' which should not exist, does')


def file_copy(srcfile, destfile):
  if(VERBOSE): print("Copy " + srcfile + " -> " + destfile)
  shutil.copyfile(srcfile, destfile)
  # CVS decides modified-ness by comparing the Entries timestamp with the file
  # mtime at whole-second granularity, so a copy landing in the same second as
  # the checkout or the previous commit reads as unmodified and the next commit
  # becomes a silent no-op.
  st = os.stat(destfile)
  os.utime(destfile, (st.st_atime + 2, st.st_mtime + 2))


def first_difference(file1, file2):
  with open(file1, 'rb') as f:
    a = f.read()
  with open(file2, 'rb') as f:
    b = f.read()
  for i in range(min(len(a), len(b))):
    if a[i] != b[i]:
      return ('sizes %d and %d, first difference at offset %d (%r against %r)'
              % (len(a), len(b), i, a[i:i + 16], b[i:i + 16]))
  return ('sizes %d and %d, the shorter one is a prefix of the other'
          % (len(a), len(b)))


def file_compare(file1, file2):
  """Require the two files to be identical byte for byte."""
  if(VERBOSE): print("Compare " + file1 + " -> " + file2)
  # shallow=False: the default compares the stat signature only and would call
  # two files of the same size and mtime identical without reading either.
  if(not filecmp.cmp(file1, file2, shallow=False)):
    raise ScenarioFailed('file ' + file1 + ' should be identical to file ' + file2,
                         first_difference(file1, file2))


def file_delete(filename):
  if(VERBOSE): print("Delete " + filename)
  os.unlink(filename)


def rmtree_force(path):
  # CVS creates the ,v files read-only, and on Windows a read-only file stops
  # rmtree outright, so clear the bit across the tree before removing it.
  for root, dirs, files in os.walk(path):
    for name in dirs + files:
      try:
        os.chmod(os.path.join(root, name), 0o700)
      except OSError:
        pass
  shutil.rmtree(path, ignore_errors=True)


def scenario(name):
  def deco(fn):
    fn.scenario_name = name
    return fn
  return deco


def xfail(name, reason):
  """Pin a known-open defect: the scenario is expected to fail today.

  Reported xfail when it fails, and XPASS when it unexpectedly passes - which
  means the defect is fixed and the marker has to come off, so an XPASS counts
  as a suite failure and cannot be missed."""
  def deco(fn):
    fn.scenario_name = name
    fn.xfail_reason = reason
    return fn
  return deco


# --------------------------------------------------------------------- scenarios

@scenario('Basic functionality, Init, Import, Checkout')
def s_init_import_checkout():
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


@scenario('Basic Add, Remove, Resurrect, Commit')
def s_add_remove_resurrect():
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


@scenario('Basic binary Add/Checkout')
def s_binary_add_checkout():
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


@scenario('Binary remove and revert')
def s_binary_remove_revert():
  chdir(current_tree+'/testcvs')
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


@scenario('Binary delta Add/Checkout')
def s_binary_delta_add_checkout():
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


@scenario('Add/Checkout large file')
def s_large_file():
  chdir(current_tree+'/testcvs')
  file_copy(test_data+'/maastrict.txt',current_tree+'/testcvs/maastrict.txt')
  cvs_pass('add maastrict.txt')
  cvs_pass('commit -m "" maastrict.txt')
  file_exists(current_tree+'/testcvs/maastrict.txt')
  file_exists(current_physroot+'/testcvs/maastrict.txt,v')
  file_compare(test_data+'/maastrict.txt',current_tree+'/testcvs/maastrict.txt')
  file_delete(current_tree+'/testcvs/maastrict.txt')
  cvs_pass('update maastrict.txt')
  file_compare(test_data+'/maastrict.txt',current_tree+'/testcvs/maastrict.txt')


@scenario('Commit 50 revisions of a small file')
def s_commit_50_small():
  chdir(current_tree+'/testcvs')
  for i in range(25):
    file_copy(test_data+'/diff_test.txt.1',current_tree+'/testcvs/test1.txt')
    cvs_pass('commit -f -m "" test1.txt')
    file_copy(test_data+'/diff_test.txt.2',current_tree+'/testcvs/test1.txt')
    cvs_pass('commit -f -m "" test1.txt')


@scenario('Commit 50 revisions of a large file')
def s_commit_50_large():
  chdir(current_tree+'/testcvs')
  for i in range(25):
    file_copy(test_data+'/diff_test.txt.1',current_tree+'/testcvs/maastrict.txt')
    cvs_pass('commit -f -m "" maastrict.txt')
    file_copy(test_data+'/maastrict.txt',current_tree+'/testcvs/maastrict.txt')
    cvs_pass('commit -f -m "" maastrict.txt')


@scenario('Checkout/Diff different versions of a text file')
def s_text_versions():
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


@scenario('Checkout different versions of a binary file')
def s_binary_versions():
  chdir(current_tree+'/testcvs')
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


@scenario('Branching')
def s_branching():
  chdir(current_tree+'/testcvs')
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


@scenario('Sticky tag test (symbolic)')
def s_sticky_symbolic():
  chdir(current_tree+'/testcvs')
  cvs_pass('update -r sticky_tag_test_symbolic_tag')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  file_compare(test_data+'/diff_test.txt.2',current_tree+'/testcvs/diff_test.txt')
  file_exists(current_tree+'/testcvs/binary_test.gif')
  file_compare(test_data+'/binary_test3.gif',current_tree+'/testcvs/binary_test.gif')


@scenario('Sticky tag test (symbolic+update)')
def s_sticky_symbolic_update():
  chdir(current_tree+'/testcvs')
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


@scenario('Sticky tag test (revision)')
def s_sticky_revision():
  chdir(current_tree+'/testcvs')
  cvs_pass('update -r 1.2 diff_test.txt')
  file_exists(current_tree+'/testcvs/diff_test.txt')
  file_compare(test_data+'/diff_test.txt.2',current_tree+'/testcvs/diff_test.txt')
  cvs_pass('update -r 1.2 binary_test.gif')
  file_exists(current_tree+'/testcvs/binary_test.gif')
  file_compare(test_data+'/binary_test3.gif',current_tree+'/testcvs/binary_test.gif')


@scenario('Sticky tag test (revision+update)')
def s_sticky_revision_update():
  chdir(current_tree+'/testcvs')
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


@scenario('Merging')
def s_merging():
  chdir(current_tree+'/testcvs')
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


@scenario('*info')
def s_info():
  chdir(current_tree+'/testcvs')
  os.chmod(current_physroot+'/CVSROOT/commitinfo',0o644)
  os.chmod(current_physroot+'/CVSROOT/loginfo',0o644)
  os.chmod(current_physroot+'/CVSROOT/postcommand',0o644)
  file_copy(test_data + '/commitinfo_test', current_physroot+'/CVSROOT/commitinfo')
  file_copy(test_data + '/loginfo_test', current_physroot+'/CVSROOT/loginfo')
  file_copy(test_data + '/postcommand_test', current_physroot+'/CVSROOT/postcommand')
  cvs_pass('commit -f -m "info test" diff_test.txt')
  file_compare(outfile, test_data+'/info_test_output.txt')


# ------------------------------------------------------------------------ groups
#
# A group is a repository of its own.  Its scenarios run in the listed order and
# that order matters: the golden outputs in test_data pin revision numbers,
# branch numbers and tag sets that only the whole sequence produces.  A scenario
# two groups both need is simply listed in both.

GROUPS = [
  ('G1 basics', [
    s_init_import_checkout,
    s_add_remove_resurrect,
  ]),
  ('G2 binary', [
    s_init_import_checkout,
    s_binary_add_checkout,
    s_binary_remove_revert,
    s_binary_delta_add_checkout,
  ]),
  ('G3 large file and revision churn', [
    s_init_import_checkout,
    s_large_file,
    s_commit_50_small,
    s_commit_50_large,       # commits over maastrict.txt, added by s_large_file
  ]),
  ('G4 revisions and tags', [
    s_init_import_checkout,
    s_text_versions,         # diff_test.txt 1.1-1.4, symbolic tag on 1.2
    s_binary_add_checkout,   # binary_test.gif 1.1
    s_binary_versions,       # binary_test.gif 1.2-1.3, same tag, then tree-wide
    s_sticky_symbolic,
    s_sticky_symbolic_update,
    s_sticky_revision,
    s_sticky_revision_update,
  ]),
  ('G5 branch and merge', [
    s_init_import_checkout,
    s_text_versions,         # diff_test.txt 1.1-1.4, branched from 1.3 below
    s_branching,
    s_merging,
    s_info,                  # its golden output pins "new revision: 1.6"
  ]),
]


def run_group(work, gid, name, scenarios):
  global current_physroot, current_tree, count
  print('=== ' + name)
  current_physroot = os.path.join(work, gid, 'repos')
  current_tree = os.path.join(work, gid, 'tree')
  os.makedirs(current_physroot)
  os.makedirs(current_tree)
  count = 1
  blocked_by = None
  for fn in scenarios:
    label = fn.scenario_name
    if blocked_by:
      BLOCKED.append((name, label))
      print("  blocked  " + label + "   (after '" + blocked_by + "')")
      continue
    start_test(label)
    reason = detail = None
    try:
      chdir(current_tree)
      fn()
    except ScenarioFailed as e:
      reason, detail = e.reason, e.detail
    except Exception as e:  # a scenario that crashes the driver is a failed scenario
      reason = '%s: %s' % (type(e).__name__, e)
    xreason = getattr(fn, 'xfail_reason', None)
    if xreason and reason:
      XFAILED.append((name, label))
      print('  xfail ' + label)
      continue
    if xreason and not reason:
      reason = 'expected to fail but passed - remove the xfail marker (%s)' % xreason
    if not reason:
      PASSED.append((name, label))
      print('  ok    ' + label)
      continue
    FAILED.append((name, label))
    print('  FAIL  ' + label)
    # The historical wording is kept on purpose: the CI log gate greps "failed (".
    print("        Test '" + label + "' failed (" + reason + ")")
    if detail:
      print('        ' + detail.replace('\n', '\n        '))
    blocked_by = label


def main():
  global CVS, LIBDIR, TIMEOUT, VERBOSE, base_dir, test_data, outfile, errfile

  ap = argparse.ArgumentParser(description=__doc__,
                               formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument('--cvs', default='cvs',
                  help='cvs executable under test (default: the one on PATH)')
  ap.add_argument('--libdir', help='plugin directory, passed as the global -L option')
  ap.add_argument('--timeout', type=int, default=TIMEOUT,
                  help='seconds one cvs command may take (default: %d)' % TIMEOUT)
  ap.add_argument('-i', '--instance', default='0',
                  help='suffix of the scratch directory, so runs can go in parallel')
  ap.add_argument('--keep', action='store_true',
                  help='do not delete the scratch directory')
  ap.add_argument('-v', '--verbose', action='store_true')
  args = ap.parse_args()

  # The scenarios chdir around, so a path has to be absolute; a bare command
  # name has to stay a name, or the PATH lookup stops working.
  CVS = os.path.abspath(args.cvs) if os.path.dirname(args.cvs) else args.cvs
  LIBDIR = os.path.abspath(args.libdir) if args.libdir else None
  TIMEOUT = args.timeout
  VERBOSE = args.verbose

  base_dir = os.getcwd()
  test_data = os.path.join(base_dir, 'test_data')
  if not os.path.isdir(test_data):
    print('no test_data directory in ' + base_dir)
    return 2

  work = os.path.join(base_dir, 'work_' + args.instance)
  outfile = os.path.join(base_dir, 'testcvs_' + args.instance + '.out')
  errfile = os.path.join(base_dir, 'testcvs_' + args.instance + '.err')
  if os.path.isdir(work):
    rmtree_force(work)
    if os.path.isdir(work):
      print('cannot clear the scratch directory ' + work)
      return 2
  os.makedirs(work)

  for i, (name, scenarios) in enumerate(GROUPS):
    run_group(work, 'g%d' % (i + 1), name, scenarios)
    chdir(base_dir)

  print()
  print('%d passed, %d failed, %d blocked%s'
        % (len(PASSED), len(FAILED), len(BLOCKED),
           (', %d xfail' % len(XFAILED)) if XFAILED else ''))
  for group, label in FAILED:
    print('  failing: %s / %s' % (group, label))

  if not args.keep:
    rmtree_force(work)
  else:
    print('kept: ' + work)

  return 0 if not FAILED else 1


if __name__ == "__main__":
  sys.exit(main())
