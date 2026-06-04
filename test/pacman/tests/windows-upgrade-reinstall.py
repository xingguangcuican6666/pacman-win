self.description = "Reinstall a Windows payload package and replace its tracked state"

self.windows_backend = True

old = pmpkg("reinstall-app", "1.0-1")
old.package_format = "pwpkg"
old.files = ["opt/reinstall-app/old.exe"]
old.installer_hash = "old-hash"
self.addwinlocalpkg(old, reason="explicit")

new = pmpkg("reinstall-app", "1.0-1")
new.package_format = "pwpkg"
new.files = ["opt/reinstall-app/new.exe"]
new.installer_hash = "new-hash"
self.addpkg(new)

self.args = "-U %s" % new.filename()

self.addrule("PACMAN_RETCODE=0")
self.addrule("!FILE_EXIST=opt/reinstall-app/old.exe")
self.addrule("FILE_EXIST=opt/reinstall-app/new.exe")
self.addrule("STATE_EXIST=windows-local/reinstall-app.json")
self.addrule("STATE_CONTENTS=windows-local/reinstall-app.json|\"installer_hash\": \"new-hash\"")
self.addrule("!STATE_CONTENTS=windows-local/reinstall-app.json|opt/reinstall-app/old.exe")
