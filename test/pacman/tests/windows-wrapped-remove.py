self.description = "Remove a wrapped installer package and replay uninstall cleanup"

import pmfile

self.windows_backend = True

p = pmpkg("wrapped-app", "3.1-2")
p.package_format = "pwpkg"
p.source_type = "wrapped-installer"
p.trace_roots = ["opt/wrapped-app"]
p.installer = {
	"uninstall": {
		"command": "/bin/sh",
		"working_directory": "var/tmp",
		"arguments": ["wrapped-uninstall.sh"],
	},
}
self.filesystem = [
	"opt/wrapped-app/bin/app.exe",
	"var/tmp/",
]
self.filesystem.append(pmfile.pmfile("var/tmp/wrapped-uninstall.sh", "#!/bin/sh\nrm -rf \"$PACMAN_ROOT/opt/wrapped-app\"\nrm -f \"$PACMAN_ROOT/var/lib/pacman/windows-registry/wrapped-app.json\"\n", mode=0o755))
self.env["PACMAN_ROOT"] = self.rootdir().rstrip("/")
self.addwinlocalpkg(p, reason="explicit")

self.args = "-R wrapped-app"

self.addrule("PACMAN_RETCODE=0")
self.addrule("!DIR_EXIST=opt/wrapped-app/")
self.addrule("!STATE_EXIST=windows-local/wrapped-app.json")
