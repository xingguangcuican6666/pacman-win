self.description = "Install a Windows payload package from a sync db"

self.windows_backend = True

sp = pmpkg("dummy", "2.0-3")
sp.package_format = "pwpkg"
sp.desc = "dummy sync package"
sp.arch = "any"
sp.files = ["opt/dummy/bin/dummy.exe", "opt/dummy/share/doc.txt"]
sp.registry = [{
	"hive": "HKLM",
	"key": "Software\\\\PacmanWin\\\\Dummy",
	"name": "Version",
	"type": "REG_SZ",
	"value": "2.0",
}]
sp.installer_hash = "sync-hash"
self.addpkg2db("sync", sp)

self.args = "-S dummy"

self.addrule("PACMAN_RETCODE=0")
self.addrule("CACHE_EXISTS=dummy|2.0-3")
self.addrule("FILE_EXIST=opt/dummy/bin/dummy.exe")
self.addrule("STATE_EXIST=windows-local/dummy.json")
self.addrule("STATE_CONTENTS=windows-local/dummy.json|\"filename\": \"dummy-2.0-3.pwpkg\"")
self.addrule("STATE_EXIST=windows-registry/dummy.json")
