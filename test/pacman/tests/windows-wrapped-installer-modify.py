self.description = "Track modified files for wrapped installer packages"

self.windows_backend = True

self.filesystem = ["opt/modify-app/config.ini"]

p = pmpkg("modify-app", "1.0-1")
p.package_format = "pwpkg"
p.source_type = "wrapped-installer"
p.trace_roots = ["opt/modify-app"]
p.installer = {
	"executable": "installer.sh",
	"arguments": [],
	"working_directory": "",
}
p.archive_entries["installer.sh"] = "#!/bin/sh\nprintf 'updated\\n' > \"$PACMAN_ROOT/opt/modify-app/config.ini\"\n"
self.addpkg(p)
self.env["PACMAN_ROOT"] = self.rootdir().rstrip("/")

self.args = "-U %s" % p.filename()

self.addrule("PACMAN_RETCODE=0")
self.addrule("FILE_CONTENTS=opt/modify-app/config.ini|updated\n")
self.addrule("STATE_EXIST=windows-local/modify-app.json")
self.addrule("STATE_CONTENTS=windows-local/modify-app.json|\"change_type\": \"modified\"")
