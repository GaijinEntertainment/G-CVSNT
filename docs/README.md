# G-CVSNT documentation

G-CVSNT is Gaijin Entertainment's fork of [CVSNT](https://en.wikipedia.org/wiki/CVSNT)
2.5.05, re-versioned as **CVSNT 3.5.x**, modified to handle very large amounts of
binary data (game development assets). The headline change is that binary file
content is moved out of the RCS `,v` files into a **content-addressed blob
store** keyed by BLAKE3 hashes, served by a dedicated, very simple TCP
key-value server with optional caching proxies — while remaining
protocol-compatible with standard CVS/CVSNT clients for everything else.

| Document | Contents |
|----------|----------|
| [architecture.md](architecture.md) | Components, client/server protocol, how a command flows through the system |
| [gaijin-modifications.md](gaijin-modifications.md) | What this fork changes compared to stock CVSNT 2.5.05 |
| [blob-storage.md](blob-storage.md) | The content-addressed store, blob server, proxies, configuration |
| [source-layout.md](source-layout.md) | Map of the source tree |
| [performance.md](performance.md) | Why update/tag/branch scale with file count, and what can be improved |
| [../HOWTOBUILD.md](../HOWTOBUILD.md) | Building on Windows / Linux / macOS |
| [adr/ADR.md](adr/ADR.md) | Architecture decision records |
| [../_reports/README.md](../_reports/README.md) | Code-audit findings index (62 bugs) + performance analysis |

Related upstream docs inside the source tree:

* `cvsnt/cvsnt-2.5.05.3744/keyValueServer/readme.md` — blob server library internals
* `cvsnt/cvsnt-2.5.05.3744/doc/` — original CVS/CVSNT manuals (texinfo)
* `cvsnt/cvsnt-2.5.05.3744/README`, `INSTALL`, `NEWS` — original upstream files
