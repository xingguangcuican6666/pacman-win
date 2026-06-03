self.description = "Keep wrapped installer state on install command failure"

self.windows_backend = True

p = pmpkg("broken-wrapped", "1.2-1")
p.package_format = "pwpkg"
p.source_type = "wrapped-installer"
p.trace_roots = ["opt/broken-wrapped"]
p.installer = {
	"executable": "installer.sh",
	"arguments": [],
	"working_directory": "",
}
p.archive_entries["installer.sh"] = "#!/bin/sh\nmkdir -p \"$PACMAN_ROOT/opt/broken-wrapped/bin\"\nprintf 'broken\\n' > \"$PACMAN_ROOT/opt/broken-wrapped/bin/app.exe\"\nexit 23\n"
self.addpkg(p)
self.env["PACMAN_ROOT"] = self.rootdir().rstrip("/")

self.args = "-U %s" % p.filename()

self.addrule("PACMAN_RETCODE=1")
self.addrule("STATE_EXIST=windows-local/broken-wrapped.json")
self.addrule("STATE_CONTENTS=windows-local/broken-wrapped.json|\"install_status\": \"install-command-failed\"")
self.addrule("STATE_CONTENTS=windows-local/broken-wrapped.json|\"last_error\": \"wrapped installer returned non-zero status\"")
