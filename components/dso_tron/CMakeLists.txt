idf_component_register(
    SRCS
        src/sched.c
        src/task.c
        src/timer.c
        src/dual_core.c
        src/ctx_switch.S
    INCLUDE_DIRS include
)
