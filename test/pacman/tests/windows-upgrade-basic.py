self.description = "Install a Windows payload package via -U"

self.windows_backend = True

p = pmpkg("foo", "1.0-1")
p.package_format = "pwpkg"
p.desc = "payload package"
p.arch = "any"
p.files = ["opt/foo/bin/foo.exe", "opt/foo/share/"]
p.registry = [{
	"hive": "HKCU",
	"key": "Software\\\\PacmanWin\\\\Foo",
	"name": "InstallDir",
	"type": "REG_SZ",
	"value": "opt/foo",
}]
p.installer_hash = "hash-foo"
self.addpkg(p)

self.args = "-U %s" % p.filename()

self.addrule("PACMAN_RETCODE=0")
self.addrule("FILE_EXIST=opt/foo/bin/foo.exe")
self.addrule("DIR_EXIST=opt/foo/share/")
self.addrule("STATE_EXIST=windows-local/foo.json")
self.addrule("STATE_CONTENTS=windows-local/foo.json|\"installer_hash\": \"hash-foo\"")
self.addrule("STATE_CONTENTS=windows-local/foo.json|\"reason\": \"explicit\"")
self.addrule("STATE_EXIST=windows-registry/foo.json")
