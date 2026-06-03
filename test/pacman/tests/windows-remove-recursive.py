self.description = "Recursively remove Windows backend packages and orphan dependencies"

self.windows_backend = True

dep = pmpkg("dep-pkg", "1.0-1")
dep.package_format = "pwpkg"
dep.files = ["opt/apps/dep/bin/dep.dll"]
self.addwinlocalpkg(dep, reason="dependency")

app = pmpkg("hello-app", "1.0-1")
app.package_format = "pwpkg"
app.depends = ["dep-pkg"]
app.files = ["opt/apps/hello/bin/hello.exe"]
self.addwinlocalpkg(app, reason="explicit")

self.args = "-Rns hello-app"

self.addrule("PACMAN_RETCODE=0")
self.addrule("!FILE_EXIST=opt/apps/hello/bin/hello.exe")
self.addrule("!FILE_EXIST=opt/apps/dep/bin/dep.dll")
self.addrule("!STATE_EXIST=windows-local/hello-app.json")
self.addrule("!STATE_EXIST=windows-local/dep-pkg.json")
