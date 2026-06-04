self.description = "Upgrade a Windows payload package and remove files from the previous version"

self.windows_backend = True

old = pmpkg("upgrade-app", "1.0-1")
old.package_format = "pwpkg"
old.files = ["opt/upgrade-app/v1.exe"]
old.installer_hash = "v1-hash"
self.addwinlocalpkg(old, reason="explicit")

new = pmpkg("upgrade-app", "2.0-1")
new.package_format = "pwpkg"
new.files = ["opt/upgrade-app/v2.exe"]
new.installer_hash = "v2-hash"
self.addpkg(new)

self.args = "-U %s" % new.filename()

self.addrule("PACMAN_RETCODE=0")
self.addrule("!FILE_EXIST=opt/upgrade-app/v1.exe")
self.addrule("FILE_EXIST=opt/upgrade-app/v2.exe")
self.addrule("STATE_EXIST=windows-local/upgrade-app.json")
self.addrule("STATE_CONTENTS=windows-local/upgrade-app.json|\"version\": \"2.0\"")
self.addrule("STATE_CONTENTS=windows-local/upgrade-app.json|\"installer_hash\": \"v2-hash\"")
self.addrule("!STATE_CONTENTS=windows-local/upgrade-app.json|opt/upgrade-app/v1.exe")
