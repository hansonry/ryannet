settings = NewSettings()
settings.debug = 0
source = Collect("*.c")

objects = Compile(settings, source)
exe = Link(settings, "ryannet_test", objects)

