self.description = "Query Windows backend dependency-installed packages"

self.windows_backend = True

dep = pmpkg("dep-pkg", "1.0-1")
dep.package_format = "pwpkg"
dep.files = ["opt/dep/bin/dep.dll"]
self.addwinlocalpkg(dep, reason="dependency")

app = pmpkg("hello-app", "1.0-1")
app.package_format = "pwpkg"
app.files = ["opt/hello/bin/hello.exe"]
self.addwinlocalpkg(app, reason="explicit")

self.args = "-Qd"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PACMAN_OUTPUT=dep-pkg 1.0")
self.addrule("!PACMAN_OUTPUT=hello-app 1.0")
