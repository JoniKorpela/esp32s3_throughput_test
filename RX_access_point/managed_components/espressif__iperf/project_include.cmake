# Newer glibc defines SIOCATMARK from <sys/ioctl.h>. lwIP's sockets.h defines
# it again. Put a wrapper on the include path before component targets exist,
# so every linux build that pulls in this component sees the host macro dropped.
if(CONFIG_IDF_TARGET_LINUX)
    include_directories("${COMPONENT_DIR}/linux_compat")
endif()
