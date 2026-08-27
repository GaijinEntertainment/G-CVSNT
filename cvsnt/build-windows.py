#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build-windows.py -- build G-CVSNT on Windows without requiring a Visual Studio
                    installation (works with a bare MSVC toolchain + Windows SDK,
                    e.g. Gaijin devtools packages), or from a VS developer prompt.

It is a small, self-contained driver that parses the .vcxproj files of the
cvsnt.sln solution and invokes cl.exe / ml64.exe / rc.exe / lib.exe / link.exe
directly, so no MSBuild is needed.

Usage (bare toolchain, Gaijin devtools):
    python build-windows.py --vc D:\\devtools\\vc2019_16.11.34 --sdk D:\\devtools\\win.sdk.100

Usage (from a "x64 Native Tools Command Prompt for VS"):
    python build-windows.py

Options:
    --config Release|Debug   (default Release)
    --platform x64|Win32     (default x64)
    --projects a,b,c         build only the named projects (see PROJECTS below)
    --with-optional          also build optional projects (control panel, plink, ...)
    --jobs N                 parallel compilations (default: cpu count)

Output goes to <srcroot>\\Release<platform>\\ exactly like the Visual Studio build.
"""

import argparse
import hashlib
import multiprocessing.dummy
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET


def _ver_key(d):
    """Sort key for Windows SDK version dir names (10.0.22621.0) by numeric
    components, so 10.0.10240.0 sorts after 10.0.9xxx rather than before."""
    return [int(x) if x.isdigit() else -1 for x in d.split(".")]

NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
SRCROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cvsnt-2.5.05.3744")

# Core projects, in dependency order: (name, vcxproj path relative to SRCROOT)
PROJECTS = [
    # static libraries
    ("zlib",            r"zlib\win32\zlib.vcxproj"),
    ("pcre",            r"pcre\pcre.vcxproj"),
    ("libxml2",         r"libxml\win32\libxml2.vcxproj"),
    ("zstd",            r"zstd\zstd.vcxproj"),
    ("blake3",          r"blake3\blake3.vcxproj"),
    ("ca_blobs_fs",     r"ca_blobs_fs\ca_blobs_fs.vcxproj"),
    ("blob_sockets",    r"keyValueServer\blob_sockets\blob_sockets.vcxproj"),
    ("clientLib",       r"keyValueServer\clientLib\clientLib.vcxproj"),
    ("gnulib",          r"lib\gnulib.vcxproj"),
    ("libdiff",         r"diff\libdiff.vcxproj"),
    ("cvsdelta",        r"cvsdelta\cvsdelta.vcxproj"),
    ("cvsgui",          r"cvsgui\cvsgui.vcxproj"),
    ("crypt",           r"ufc-crypt\crypt.vcxproj"),
    ("mdnsclient",      r"mdnsclient\mdnsclient.vcxproj"),
    # core DLLs
    ("cvsapi",          r"cvsapi\cvsapi.vcxproj"),
    ("cvstools",        r"cvstools\cvstools.vcxproj"),
    ("xml_xdiff",       r"xdiff\xml_xdiff.vcxproj"),
    ("sqlite_database", r"cvsapi\db\sqlite\sqlite_database.vcxproj"),
    ("odbc_database",   r"cvsapi\db\odbc\odbc_database.vcxproj"),
    ("mdns_mini",       r"cvsapi\mdns\mini\mdns_mini.vcxproj"),
    # protocols
    ("plink",           r"plink\plink.vcxproj"),
    ("server_protocol", r"protocols\server_protocol.vcxproj"),
    ("pserver",         r"protocols\pserver_protocol.vcxproj"),
    ("sserver",         r"protocols\sserver_protocol.vcxproj"),
    ("sspi",            r"protocols\sspi_protocol.vcxproj"),
    ("ext",             r"protocols\ext_protocol.vcxproj"),
    ("ssh",             r"protocols\ssh_protocol.vcxproj"),
    ("enum",            r"protocols\enum_protocol.vcxproj"),
    ("fork",            r"protocols\fork_protocol.vcxproj"),
    # triggers (plugins)
    ("info_triggers",   r"triggers\info_triggers.vcxproj"),
    ("audit_trigger",   r"triggers\audit_trigger.vcxproj"),
    ("email_trigger",   r"triggers\email_trigger.vcxproj"),
    ("script_trigger",  r"triggers\script_trigger.vcxproj"),
    ("checkout_trigger",r"triggers\checkout_trigger.vcxproj"),
    # executables
    ("genbuild",        r"genbuild\genbuild.vcxproj"),
    ("libsuid",         r"windows-NT\setuid\libsuid\libsuid.vcxproj"),
    ("setuid",          r"windows-NT\setuid\setuid\setuid.vcxproj"),
    ("cvsservice",      r"cvsservice\cvsservice.vcxproj"),
    ("cvslock",         r"lockservice\lockservice.vcxproj"),
    ("cvsnt",           r"cvsnt.vcxproj"),
]

OPTIONAL_PROJECTS = [
    ("gserver",         r"protocols\gserver_protocol_ad.vcxproj"),
    ("mdns_apple",      r"cvsapi\mdns\apple\mdns_apple.vcxproj"),
    ("co",              r"rcs\co.vcxproj"),
    ("rcsdiff",         r"rcs\rcsdiff.vcxproj"),
    ("rlog",            r"rcs\rlog.vcxproj"),
    ("cvsdiag",         r"windows-NT\cvsdiag\cvsdiag.vcxproj"),
    ("extnt",           r"extnt\extnt.vcxproj"),
    ("su",              r"su\su.vcxproj"),
    ("genkey",          r"genkey\genkey.vcxproj"),
    ("postinst",        r"postinst\postinst.vcxproj"),
    ("uninsthlp",       r"uninsthlp\uninsthlp.vcxproj"),
]


def find_sdk_ver(sdk):
    best = None
    for d in sorted(os.listdir(os.path.join(sdk, "include")), key=_ver_key):
        if os.path.isfile(os.path.join(sdk, "include", d, "um", "windows.h")) and \
           os.path.isdir(os.path.join(sdk, "lib", d, "um")):
            best = d
    return best


TOOLS = {"cl": "cl.exe", "ml64": "ml64.exe", "rc": "rc.exe",
         "lib": "lib.exe", "link": "link.exe", "mc": "mc.exe", "midl": "midl.exe"}

# Per-project fixups the .vcxproj files don't express cleanly
OVERRIDES = {
    "genbuild": {"subsystem": "Windows"},   # uses WinMain
    # clientLib calls into blob_sockets, but only the .sln (not the .vcxproj)
    # expresses the dependency
    "cvsnt": {"extra_ref": ["blob_sockets"]},
    "cvsservice": {"extra_ref": ["blob_sockets"]},
}

# Code generation steps (message compiler / MIDL) that MSBuild runs as custom
# build tools. (produced-file, cwd-relative-to-SRCROOT, command builder)
def _mc_servicemsg(env, plat):
    d = os.path.join(SRCROOT, "cvsapi", "win32")
    run_tool([TOOLS["mc"], "ServiceMsg.mc"], d, env, "mc ServiceMsg.mc")
    # cvsservice includes it relative to its own dir as well
    import shutil
    for f in ("ServiceMsg.h", "ServiceMsg.rc"):
        src = os.path.join(d, f)
        if os.path.isfile(src):
            shutil.copy2(src, os.path.join(SRCROOT, "cvsservice", f))

def _midl_trigger(env, plat):
    d = os.path.join(SRCROOT, "cvstools")
    run_tool([TOOLS["midl"], "/nologo", "/env", "x64" if plat == "x64" else "win32",
              "/h", "trigger_h.h", "/iid", "trigger_i.c",
              "/tlb", os.path.join("win32", "trigger.tlb"),
              os.path.join("win32", "trigger.idl")], d, env, "midl trigger.idl")

def _midl_server(env, plat):
    d = os.path.join(SRCROOT, "triggers")
    run_tool([TOOLS["midl"], "/nologo", "/env", "x64" if plat == "x64" else "win32",
              "/h", "Server_h.h", "/iid", "Server_i.c", "/tlb", "script_trigger.tlb",
              "server.idl"], d, env, "midl server.idl")

PRE_STEPS = {
    "cvsapi":     [(r"cvsapi\win32\ServiceMsg.h", _mc_servicemsg)],
    "cvsservice": [(r"cvsservice\ServiceMsg.h", _mc_servicemsg)],
    "cvstools":   [(r"cvstools\trigger_h.h", _midl_trigger),
                   (r"cvstools\win32\trigger.tlb", _midl_trigger)],
    "info_triggers":  [(r"triggers\Server_h.h", _midl_server)],
    "audit_trigger":  [(r"triggers\Server_h.h", _midl_server)],
    "email_trigger":  [(r"triggers\Server_h.h", _midl_server)],
    "script_trigger": [(r"triggers\Server_h.h", _midl_server),
                       (r"triggers\script_trigger.tlb", _midl_server)],
    "checkout_trigger": [(r"triggers\Server_h.h", _midl_server)],
}


BUILT = {}          # normalized vcxproj path -> (type, output file, import lib)
BUILT_BY_NAME = {}  # project name -> (type, output file, import lib)


def run_tool(cmd, cwd, env, what):
    r = subprocess.run(cmd, capture_output=True, text=True, env=env, cwd=cwd)
    if r.returncode != 0:
        sys.stderr.write("FAILED(%s):\n%s\n%s\n" % (what, r.stdout, r.stderr))
        raise RuntimeError(what)


def setup_env(args):
    """Compose PATH/INCLUDE/LIB for a bare toolchain, unless cl.exe is already usable.
    Resolves TOOLS to absolute paths (Windows CreateProcess ignores child-env PATH)."""
    env = os.environ.copy()
    if args.vc:
        vc = os.path.abspath(args.vc)
        sdk = os.path.abspath(args.sdk)
        sdkver = args.sdk_ver or find_sdk_ver(sdk)
        if not sdkver:
            sys.exit("error: no usable Windows SDK version under %s" % sdk)
        host = "Hostx64"
        tgt = "x64" if args.platform == "x64" else "x86"
        vcbin = os.path.join(vc, "bin", host, tgt)
        sdkbin_ver = sorted((d for d in os.listdir(os.path.join(sdk, "bin"))
                             if os.path.isfile(os.path.join(sdk, "bin", d, tgt, "rc.exe"))),
                            key=_ver_key)
        if not sdkbin_ver:
            sys.exit("error: rc.exe not found under %s\\bin" % sdk)
        sdkbin = os.path.join(sdk, "bin", sdkbin_ver[-1], tgt)
        env["PATH"] = os.pathsep.join([vcbin, sdkbin, env.get("PATH", "")])
        env["INCLUDE"] = os.pathsep.join([
            os.path.join(vc, "include"),
            os.path.join(vc, "atlmfc", "include"),
            os.path.join(sdk, "include", sdkver, "ucrt"),
            os.path.join(sdk, "include", sdkver, "um"),
            os.path.join(sdk, "include", sdkver, "shared"),
            os.path.join(sdk, "include", sdkver, "winrt"),
        ])
        env["LIB"] = os.pathsep.join([
            os.path.join(vc, "lib", tgt),
            os.path.join(vc, "atlmfc", "lib", tgt),
            os.path.join(sdk, "lib", sdkver, "ucrt", tgt),
            os.path.join(sdk, "lib", sdkver, "um", tgt),
        ])
        for k, exe in TOOLS.items():
            p = os.path.join(vcbin, exe)
            if not os.path.isfile(p):
                p = os.path.join(sdkbin, exe)
            TOOLS[k] = p
        print("using VC: %s" % vc)
        print("using SDK: %s (%s, rc from %s)" % (sdk, sdkver, sdkbin_ver[-1]))
    else:
        # expect a developer prompt
        import shutil
        for k, exe in TOOLS.items():
            p = shutil.which(exe)
            if not p:
                sys.exit("error: %s not on PATH. Run from a VS developer prompt "
                         "or pass --vc/--sdk (see --help)." % exe)
            TOOLS[k] = p
    return env


def cond_matches(node, cfg):
    c = node.get("Condition")
    if not c:
        return True
    m = re.search(r"==\s*'([^']*)'", c)
    return bool(m) and m.group(1) == cfg


def text(node, default=""):
    return (node.text or default).strip() if node is not None else default


class Project(object):
    def __init__(self, name, path, args):
        self.name = name
        self.vcxproj = os.path.join(SRCROOT, path)
        self.dir = os.path.dirname(self.vcxproj)
        self.cfg = "%s|%s" % (args.config, args.platform)
        self.args = args
        self.macros = {
            "SolutionDir": SRCROOT + "\\",
            "ProjectDir": self.dir + "\\",
            "ProjectName": os.path.splitext(os.path.basename(path))[0],
            "Configuration": args.config,
            "Platform": args.platform,
        }
        self.parse()

    def expand(self, s):
        s = re.sub(r"%\(\w+\)", "", s or "")
        for k, v in self.macros.items():
            s = s.replace("$(%s)" % k, v)
        s = re.sub(r"\$\(\w+\)", "", s)  # unknown macros -> empty
        return s

    def parse(self):
        root = ET.parse(self.vcxproj).getroot()
        self.type = "Application"
        self.charset = ""
        for pg in root.findall("m:PropertyGroup", NS):
            if pg.get("Label") == "Configuration" and cond_matches(pg, self.cfg):
                self.type = text(pg.find("m:ConfigurationType", NS), self.type)
                self.charset = text(pg.find("m:CharacterSet", NS), self.charset)
        self.outdir = os.path.join(SRCROOT, self.args.config + self.args.platform) + "\\"
        self.intdir = os.path.join(SRCROOT, "tmp", self.args.config + self.args.platform,
                                   self.macros["ProjectName"]) + "\\"
        def absdir(p):
            return p if os.path.isabs(p) else os.path.join(self.dir, p)
        for pg in root.findall("m:PropertyGroup", NS):
            if cond_matches(pg, self.cfg):
                od = text(pg.find("m:OutDir", NS))
                if od:
                    self.outdir = absdir(self.expand(od))
                nd = text(pg.find("m:IntDir", NS))
                if nd:
                    self.intdir = absdir(self.expand(nd))
                tn = text(pg.find("m:TargetName", NS))
                if tn:
                    self.macros["TargetName"] = self.expand(tn)
        self.macros.setdefault("TargetName", self.macros["ProjectName"])
        self.macros["OutDir"] = self.outdir
        self.macros["IntDir"] = self.intdir
        self.macros["TargetDir"] = self.outdir

        self.includes, self.defines, self.cl_extra = [], [], []
        self.libs, self.libdirs, self.link_extra = [], [], []
        self.deffile = self.outfile = self.implib = ""
        self.runtime = "MultiThreadedDLL" if self.args.config == "Release" else "MultiThreadedDebugDLL"
        self.eh = "Sync"
        self.subsystem = "Console"
        for idg in root.findall("m:ItemDefinitionGroup", NS):
            if not cond_matches(idg, self.cfg):
                continue
            clc = idg.find("m:ClCompile", NS)
            if clc is not None:
                inc = self.expand(text(clc.find("m:AdditionalIncludeDirectories", NS)))
                self.includes += [i for i in inc.split(";") if i.strip()]
                de = self.expand(text(clc.find("m:PreprocessorDefinitions", NS)))
                self.defines += [d for d in de.split(";") if d.strip()]
                rt = text(clc.find("m:RuntimeLibrary", NS))
                if rt:
                    self.runtime = rt
                eh = text(clc.find("m:ExceptionHandling", NS))
                if eh:
                    self.eh = eh
                if text(clc.find("m:TreatWChar_tAsBuiltInType", NS)) == "false":
                    self.cl_extra.append("/Zc:wchar_t-")
                if text(clc.find("m:CompileAs", NS)) == "CompileAsC":
                    self.cl_extra.append("/TC")
                if text(clc.find("m:CompileAs", NS)) == "CompileAsCpp":
                    self.cl_extra.append("/TP")
            lnk = idg.find("m:Link", NS)
            if lnk is not None:
                ad = self.expand(text(lnk.find("m:AdditionalDependencies", NS)))
                self.libs += [l for l in ad.split(";") if l.strip()]
                ld = self.expand(text(lnk.find("m:AdditionalLibraryDirectories", NS)))
                self.libdirs += [d for d in ld.split(";") if d.strip()]
                df = self.expand(text(lnk.find("m:ModuleDefinitionFile", NS)))
                if df:
                    self.deffile = df
                of = self.expand(text(lnk.find("m:OutputFile", NS)))
                if of:
                    self.outfile = of
                il = self.expand(text(lnk.find("m:ImportLibrary", NS)))
                if il:
                    self.implib = il
                ss = text(lnk.find("m:SubSystem", NS))
                if ss and ss != "NotSet":
                    self.subsystem = ss
            lib = idg.find("m:Lib", NS)
            if lib is not None:
                of = self.expand(text(lib.find("m:OutputFile", NS)))
                if of:
                    self.outfile = of

        self.refs = []
        for ig in root.findall("m:ItemGroup", NS):
            for pr in ig.findall("m:ProjectReference", NS):
                f = pr.get("Include")
                if f:
                    self.refs.append(os.path.normcase(os.path.normpath(
                        os.path.join(self.dir, f))))

        self.sources, self.masm, self.rc = [], [], []
        for ig in root.findall("m:ItemGroup", NS):
            for it in ig.findall("m:ClCompile", NS):
                f = it.get("Include")
                if not f:
                    continue
                excluded = False
                for ex in it.findall("m:ExcludedFromBuild", NS):
                    if cond_matches(ex, self.cfg) and text(ex) == "true":
                        excluded = True
                if not excluded:
                    self.sources.append(os.path.join(self.dir, f))
            for it in ig.findall("m:MASM", NS):
                f = it.get("Include")
                excluded = False
                for ex in it.findall("m:ExcludedFromBuild", NS):
                    if cond_matches(ex, self.cfg) and text(ex) == "true":
                        excluded = True
                if f and not excluded:
                    self.masm.append(os.path.join(self.dir, f))
            for it in ig.findall("m:ResourceCompile", NS):
                f = it.get("Include")
                if f:
                    self.rc.append(os.path.join(self.dir, f))

        if not self.outfile:
            ext = {"Application": ".exe", "DynamicLibrary": ".dll", "StaticLibrary": ".lib"}[self.type]
            self.outfile = os.path.join(self.outdir, self.macros["TargetName"] + ext)

    def cl_flags(self):
        f = ["/nologo", "/c", "/W3", "/Z7", "/bigobj",
             {"MultiThreadedDLL": "/MD", "MultiThreadedDebugDLL": "/MDd",
              "MultiThreaded": "/MT", "MultiThreadedDebug": "/MTd"}[self.runtime]]
        if self.args.config == "Release":
            f += ["/O2", "/Oy-", "/GF", "/Gy"]
        else:
            f += ["/Od", "/RTC1"]
        if self.eh in ("Sync", "SyncCThrow"):
            f.append("/EHsc")
        elif self.eh == "Async":
            f.append("/EHa")
        if self.charset == "Unicode":
            f += ["/DUNICODE", "/D_UNICODE"]
        elif self.charset == "MultiByte":
            f += ["/D_MBCS"]
        f += ["/D" + d for d in self.defines]
        f += ["/D_CRT_SECURE_NO_WARNINGS", "/D_CRT_NONSTDC_NO_WARNINGS",
              "/D_WINSOCK_DEPRECATED_NO_WARNINGS"]
        for i in self.includes:
            p = i if os.path.isabs(i) else os.path.join(self.dir, i)
            f.append("/I" + os.path.normpath(p))
        f += self.cl_extra
        return f

    def _obj_name(self, src):
        # Disambiguate sources that share a basename across directories so their
        # .obj files (C and MASM alike) do not overwrite each other.
        stem = os.path.splitext(os.path.basename(src))[0]
        tag = hashlib.md5(os.path.normcase(os.path.abspath(src)).encode()).hexdigest()[:8]
        return os.path.join(self.intdir, "%s_%s.obj" % (stem, tag))

    def build(self, env, jobs):
        os.makedirs(self.intdir, exist_ok=True)
        os.makedirs(self.outdir, exist_ok=True)
        os.makedirs(os.path.dirname(os.path.join(self.dir, self.outfile)), exist_ok=True)
        for produced, step in PRE_STEPS.get(self.name, []):
            if not os.path.isfile(os.path.join(SRCROOT, produced)):
                step(env, self.args.platform)
        sso = OVERRIDES.get(self.name, {}).get("subsystem")
        if sso:
            self.subsystem = sso
        self.libs += OVERRIDES.get(self.name, {}).get("extra_libs", [])
        objs = []
        flags = self.cl_flags()

        def compile_one(src):
            obj = self._obj_name(src)
            cmd = [TOOLS["cl"]] + flags + ["/Fo" + obj, src]
            r = subprocess.run(cmd, capture_output=True, text=True, env=env, cwd=self.dir)
            return src, obj, r

        pool = multiprocessing.dummy.Pool(jobs)
        failed = False
        for src, obj, r in pool.map(compile_one, self.sources):
            if r.returncode != 0:
                sys.stderr.write("FAILED: %s\n%s\n%s\n" % (src, r.stdout, r.stderr))
                failed = True
            else:
                objs.append(obj)
        pool.close()
        pool.join()
        if failed:
            return False

        for src in self.masm:
            obj = self._obj_name(src)
            cmd = [TOOLS["ml64"], "/nologo", "/c", "/Fo" + obj, src]
            r = subprocess.run(cmd, capture_output=True, text=True, env=env, cwd=self.dir)
            if r.returncode != 0:
                sys.stderr.write("FAILED(masm): %s\n%s\n%s\n" % (src, r.stdout, r.stderr))
                return False
            objs.append(obj)

        res_files = []
        for src in self.rc:
            res = os.path.join(self.intdir,
                               os.path.splitext(os.path.basename(src))[0] + ".res")
            cmd = [TOOLS["rc"], "/nologo",
                   "/d", "NDEBUG" if self.args.config == "Release" else "_DEBUG",
                   "/i", os.path.dirname(src), "/i", SRCROOT,
                   "/fo", res, src]
            r = subprocess.run(cmd, capture_output=True, text=True, env=env, cwd=self.dir)
            if r.returncode != 0:
                sys.stderr.write("FAILED(rc): %s\n%s\n%s\n" % (src, r.stdout, r.stderr))
                return False
            res_files.append(res)

        machine = "/MACHINE:X64" if self.args.platform == "x64" else "/MACHINE:X86"
        if self.type == "StaticLibrary":
            cmd = [TOOLS["lib"], "/nologo", machine, "/OUT:" + self.outfile] + objs
        else:
            cmd = [TOOLS["link"], "/nologo", machine, "/DEBUG", "/INCREMENTAL:NO",
                   "/OUT:" + self.outfile]
            if self.type == "DynamicLibrary":
                cmd.append("/DLL")
                implib = self.implib or os.path.join(self.outdir, self.macros["TargetName"] + ".lib")
                if not os.path.isabs(implib):
                    implib = os.path.join(self.dir, implib)
                os.makedirs(os.path.dirname(implib), exist_ok=True)
                cmd.append("/IMPLIB:" + implib)
            else:
                cmd.append("/SUBSYSTEM:" + self.subsystem.upper())
            if self.deffile:
                cmd.append("/DEF:" + os.path.join(self.dir, self.deffile))
            for d in self.libdirs:
                p = d if os.path.isabs(d) else os.path.join(self.dir, d)
                cmd.append("/LIBPATH:" + os.path.normpath(p))
            cmd.append("/LIBPATH:" + self.outdir)  # import libs of already-built DLLs/static libs
            # emulate MSBuild's LinkLibraryDependencies: link outputs of referenced projects
            reflibs = []
            entries = [BUILT.get(r) for r in self.refs]
            entries += [BUILT_BY_NAME.get(n) for n in
                        OVERRIDES.get(self.name, {}).get("extra_ref", [])]
            for built in entries:
                if not built:
                    continue
                btype, boutfile, bimplib = built
                if btype == "StaticLibrary":
                    reflibs.append(boutfile)
                elif btype == "DynamicLibrary" and bimplib and os.path.isfile(bimplib):
                    reflibs.append(bimplib)
            cmd += objs + res_files + self.libs + reflibs
            cmd += ["kernel32.lib", "user32.lib", "gdi32.lib", "advapi32.lib",
                    "shell32.lib", "ole32.lib", "oleaut32.lib", "uuid.lib",
                    "ws2_32.lib", "version.lib", "shlwapi.lib", "secur32.lib",
                    "comdlg32.lib", "winspool.lib"]
            cmd += self.link_extra
        r = subprocess.run(cmd, capture_output=True, text=True, env=env, cwd=self.dir)
        if r.returncode != 0:
            sys.stderr.write("FAILED(link) %s:\n%s\n%s\n" % (self.name, r.stdout, r.stderr))
            return False
        implib = self.implib or os.path.join(self.outdir, self.macros["TargetName"] + ".lib")
        if not os.path.isabs(implib):
            implib = os.path.join(self.dir, implib)
        out_abs = self.outfile if os.path.isabs(self.outfile) else \
            os.path.normpath(os.path.join(self.dir, self.outfile))
        BUILT[os.path.normcase(os.path.normpath(self.vcxproj))] = (self.type, out_abs, implib)
        BUILT_BY_NAME[self.name] = (self.type, out_abs, implib)
        print("  -> %s" % self.outfile)
        return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--vc", help=r"MSVC root (contains bin\Hostx64, include, lib), e.g. D:\devtools\vc2019_16.11.34")
    ap.add_argument("--sdk", help=r"Windows 10 SDK root (contains include\<ver>, lib\<ver>), e.g. D:\devtools\win.sdk.100")
    ap.add_argument("--sdk-ver", help="SDK version to use (default: newest complete)")
    ap.add_argument("--config", default="Release", choices=["Release", "Debug"])
    ap.add_argument("--platform", default="x64", choices=["x64", "Win32"])
    ap.add_argument("--projects", help="comma separated subset of project names")
    ap.add_argument("--with-optional", action="store_true")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    args = ap.parse_args()
    if bool(args.vc) != bool(args.sdk):
        sys.exit("error: --vc and --sdk must be used together")

    env = setup_env(args)
    everything = PROJECTS + OPTIONAL_PROJECTS
    todo = PROJECTS + (OPTIONAL_PROJECTS if args.with_optional else [])
    if args.projects:
        names = [n.strip() for n in args.projects.split(",")]
        todo = [p for p in todo if p[0] in names]
    # pre-register outputs of projects built in earlier runs so ProjectReference
    # linking works for partial --projects builds
    todo_names = {n for n, _ in todo}
    for name, relpath in everything:
        if name in todo_names:
            continue
        try:
            p = Project(name, relpath, args)
        except Exception:
            continue
        out_abs = p.outfile if os.path.isabs(p.outfile) else \
            os.path.normpath(os.path.join(p.dir, p.outfile))
        if os.path.isfile(out_abs):
            implib = p.implib or os.path.join(p.outdir, p.macros["TargetName"] + ".lib")
            if not os.path.isabs(implib):
                implib = os.path.join(p.dir, implib)
            BUILT[os.path.normcase(os.path.normpath(p.vcxproj))] = (p.type, out_abs, implib)
            BUILT_BY_NAME[name] = (p.type, out_abs, implib)

    results = {}
    for name, relpath in todo:
        print("=== %s (%s) ===" % (name, relpath))
        try:
            ok = Project(name, relpath, args).build(env, args.jobs)
        except Exception as e:
            sys.stderr.write("EXCEPTION in %s: %r\n" % (name, e))
            ok = False
        results[name] = ok

    print("\n==== summary ====")
    for name, _ in todo:
        print("%-18s %s" % (name, "OK" if results.get(name) else "FAILED"))
    sys.exit(0 if all(results.values()) else 1)


if __name__ == "__main__":
    main()
