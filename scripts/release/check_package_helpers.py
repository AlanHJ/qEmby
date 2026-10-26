"""Regression check for dependency parsing without executing macOS tools."""
import package


package.output = lambda *args: '''/tmp/QtConcurrent (architecture arm64):
    @rpath/QtConcurrent.framework/Versions/A/QtConcurrent (compatibility version 6.0.0, current version 6.9.2)
    /usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1351.0.0)
'''
assert package.mac_dependencies("unused") == [
    "@rpath/QtConcurrent.framework/Versions/A/QtConcurrent",
    "/usr/lib/libSystem.B.dylib",
]
print("Mach-O dependency parser OK")
