import pmfile

self.description = "Upgrade a wrapped installer package by running the previous uninstall first"

self.windows_backend = True

old = pmpkg("wrapped-upgrade", "1.0-1")
old.package_format = "pwpkg"
old.source_type = "wrapped-installer"
old.trace_roots = ["opt/wrapped-upgrade"]
old.installer = {
	"uninstall": {
		"command": "/bin/sh",
		"working_directory": "var/tmp",
		"arguments": ["wrapped-upgrade-uninstall.sh"],
	},
}
old.files = ["opt/wrapped-upgrade/old.exe"]
self.filesystem = [
	"var/tmp/",
]
self.filesystem.append(pmfile.pmfile(
	"var/tmp/wrapped-upgrade-uninstall.sh",
	"#!/bin/sh\nrm -rf \"$PACMAN_ROOT/opt/wrapped-upgrade\"\n",
	mode=0o755))
self.env["PACMAN_ROOT"] = self.rootdir().rstrip("/")
self.addwinlocalpkg(old, reason="explicit")

new = pmpkg("wrapped-upgrade", "2.0-1")
new.package_format = "pwpkg"
new.source_type = "wrapped-installer"
new.trace_roots = ["opt/wrapped-upgrade"]
new.installer = {
	"executable": "installer.sh",
	"arguments": [],
	"working_directory": "",
}
new.archive_entries["installer.sh"] = "#!/bin/sh\nmkdir -p \"$PACMAN_ROOT/opt/wrapped-upgrade\"\nprintf 'new\\n' > \"$PACMAN_ROOT/opt/wrapped-upgrade/new.exe\"\n"
self.addpkg(new)

self.args = "-U %s" % new.filename()

self.addrule("PACMAN_RETCODE=0")
self.addrule("!FILE_EXIST=opt/wrapped-upgrade/old.exe")
self.addrule("FILE_EXIST=opt/wrapped-upgrade/new.exe")
self.addrule("STATE_EXIST=windows-local/wrapped-upgrade.json")
self.addrule("STATE_CONTENTS=windows-local/wrapped-upgrade.json|\"version\": \"2.0\"")
self.addrule("!STATE_CONTENTS=windows-local/wrapped-upgrade.json|opt/wrapped-upgrade/old.exe")
