import pmfile

self.description = "Preserve wrapped installer state when tracked registry values remain after uninstall"

self.windows_backend = True

p = pmpkg("wrapped-registry-residual", "3.1-2")
p.package_format = "pwpkg"
p.source_type = "wrapped-installer"
p.trace_roots = ["opt/wrapped-registry-residual"]
p.registry = [{
	"hive": "HKCU",
	"key": "Software\\\\PacmanWin\\\\WrappedRegistryResidual",
	"name": "Installed",
	"type": "REG_SZ",
	"value": "yes",
}]
p.installer = {
	"uninstall": {
		"command": "/bin/sh",
		"working_directory": "var/tmp",
		"arguments": ["wrapped-registry-residual-uninstall.sh"],
	},
}
self.filesystem = [
	"opt/wrapped-registry-residual/bin/app.exe",
	"var/tmp/",
]
self.filesystem.append(pmfile.pmfile(
	"var/tmp/wrapped-registry-residual-uninstall.sh",
	"#!/bin/sh\nrm -rf \"$PACMAN_ROOT/opt/wrapped-registry-residual\"\n",
	mode=0o755))
self.env["PACMAN_ROOT"] = self.rootdir().rstrip("/")
self.addwinlocalpkg(p, reason="explicit")

self.args = "-R wrapped-registry-residual"

self.addrule("PACMAN_RETCODE=1")
self.addrule("!DIR_EXIST=opt/wrapped-registry-residual/")
self.addrule("STATE_EXIST=windows-local/wrapped-registry-residual.json")
self.addrule("STATE_EXIST=windows-registry/wrapped-registry-residual.json")
self.addrule("STATE_CONTENTS=windows-local/wrapped-registry-residual.json|\"uninstall_status\": \"registry-residuals-remain\"")
self.addrule("STATE_CONTENTS=windows-local/wrapped-registry-residual.json|\"last_error\": \"tracked registry residuals remain after uninstall\"")
