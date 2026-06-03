self.description = "Track and remove a wrapped installer package in the Windows backend"

self.windows_backend = True

p = pmpkg("wrapped-app", "3.1-2")
p.package_format = "pwpkg"
p.source_type = "wrapped-installer"
p.trace_roots = ["opt/wrapped-app"]
p.trace_registry_keys = ["HKCU\\\\Software\\\\PacmanWin\\\\WrappedApp"]
p.registry = [{
	"hive": "HKCU",
	"key": "Software\\\\PacmanWin\\\\WrappedApp",
	"name": "Installed",
	"type": "REG_SZ",
	"value": "yes",
}]
p.installer = {
	"executable": "installer.sh",
	"arguments": [],
	"working_directory": "",
	"uninstall": {
		"command": "/bin/sh",
		"working_directory": "",
		"arguments": ["var/tmp/wrapped-uninstall.sh"],
	},
}
p.archive_entries["installer.sh"] = "#!/bin/sh\nmkdir -p \"$PACMAN_ROOT/opt/wrapped-app/bin\"\nprintf 'wrapped\\n' > \"$PACMAN_ROOT/opt/wrapped-app/bin/app.exe\"\nprintf '#!/bin/sh\\nrm -rf \"$PACMAN_ROOT/opt/wrapped-app\"\\n' > \"$PACMAN_ROOT/var/tmp/wrapped-uninstall.sh\"\nchmod +x \"$PACMAN_ROOT/var/tmp/wrapped-uninstall.sh\"\n"
self.addpkg(p)
self.env["PACMAN_ROOT"] = self.rootdir().rstrip("/")

self.args = "-U %s" % p.filename()

self.addrule("PACMAN_RETCODE=0")
self.addrule("FILE_EXIST=opt/wrapped-app/bin/app.exe")
self.addrule("STATE_EXIST=windows-local/wrapped-app.json")
self.addrule("STATE_CONTENTS=windows-local/wrapped-app.json|\"has_explicit_uninstall\": true")
self.addrule("STATE_CONTENTS=windows-local/wrapped-app.json|\"trace_roots\": [")
self.addrule("STATE_CONTENTS=windows-local/wrapped-app.json|\"install_status\": \"installed\"")
self.addrule("STATE_CONTENTS=windows-local/wrapped-app.json|\"phase\": \"install\"")
