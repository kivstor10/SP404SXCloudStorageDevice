from SCons.Script import Environment, DefaultEnvironment

def set_idf_options(env):
    env.Append(
        CPPDEFINES=[
            ("CONFIG_HEAP_CORRUPTION_DETECTION", "1"),
            ("CONFIG_HEAP_TRACING_ENABLED", "1"),
            ("CONFIG_HEAP_TRACING_DEST", "0"),
        ]
    )
    #This is the crucial part, though.
    env.Replace(
       #This is the path to the sdkconfig.h file.
        CPPDEFINES=[
            ("CONFIG_IDF_PATH", "\\\${IDF_PATH}".replace("\\","/")),
            ]
    )
    env.Append(
        #These are the menuconfig options.
        #Important:  These are the CONFIG options as they appear in sdkconfig.h
        #NOT as they appear in the menuconfig UI.
        #You can also set other menuconfig options here.
        #CONFIG_FREERTOS_UNICORE=1
        #CONFIG_BT_ENABLED=1
        #CONFIG_BT_CLASSIC_MODE=1
        MENUCONFIGOPTS=[
            "CONFIG_HEAP_TRACING_ENABLED=y",
            "CONFIG_HEAP_CORRUPTION_DETECTION=y",
            "CONFIG_HEAP_TRACING_DEST_RAM=y",
        ]
    )

env = DefaultEnvironment()
set_idf_options(env)