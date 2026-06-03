self.description = "Keep wrapped installer state when explicit uninstall fails"

import pmfile

self.windows_backend = True

p = pmpkg("wrapped-app-fail", "3.1-2")
p.package_format = "pwpkg"
p.source_type = "wrapped-installer"
p.trace_roots = ["opt/wrapped-app-fail"]
p.registry = [{
	"hive": "HKCU",
	"key": "Software\\\\PacmanWin\\\\WrappedAppFail",
	"name": "Installed",
	"type": "REG_SZ",
	"value": "yes",
}]
p.installer = {
	"uninstall": {
		"command": "/bin/sh",
		"working_directory": "var/tmp",
		"arguments": ["wrapped-uninstall-fail.sh"],
	},
}
self.filesystem = [
	"opt/wrapped-app-fail/bin/app.exe",
	"var/tmp/",
]
self.filesystem.append(pmfile.pmfile("var/tmp/wrapped-uninstall-fail.sh", "#!/bin/sh\nexit 9\n", mode=0o755))
self.env["PACMAN_ROOT"] = self.rootdir().rstrip("/")
self.addwinlocalpkg(p, reason="explicit")

self.args = "-R wrapped-app-fail"

self.addrule("PACMAN_RETCODE=1")
self.addrule("FILE_EXIST=opt/wrapped-app-fail/bin/app.exe")
self.addrule("STATE_EXIST=windows-local/wrapped-app-fail.json")
self.addrule("STATE_CONTENTS=windows-local/wrapped-app-fail.json|\"uninstall_status\": \"command-failed\"")
self.addrule("STATE_CONTENTS=windows-local/wrapped-app-fail.json|\"last_error\": \"explicit uninstall command failed\"")
