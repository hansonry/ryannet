settings = NewSettings()
settings.debug = 1
source = Collect("*.c")

objects = Compile(settings, source)
exe = Link(settings, "ryannet_test", objects)

