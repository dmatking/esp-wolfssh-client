cmake_minimum_required(VERSION 3.16)

# wolfssh sources are compiled directly into this component.
# misc.c is intentionally excluded: only needed when NO_INLINE is defined.
set(WOLFSSH_SRCS
    wolfssh/src/agent.c
    wolfssh/src/certman.c
    wolfssh/src/internal.c
    wolfssh/src/io.c
    wolfssh/src/keygen.c
    wolfssh/src/log.c
    wolfssh/src/port.c
    wolfssh/src/ssh.c
    wolfssh/src/wolfscp.c
    wolfssh/src/wolfsftp.c
    wolfssh/src/wolfterm.c
)

idf_component_register(
    SRCS "src/ssh_client.c" ${WOLFSSH_SRCS}
    INCLUDE_DIRS "include" "wolfssh" "wolfssl_config"
    REQUIRES wolfssl lwip freertos
)

# wolfssl_config/user_settings.h is the pre-tuned ESP32 wolfSSL config.
# WOLFSSL_USER_SETTINGS tells wolfSSL to use it instead of options.h.
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    WOLFSSL_USER_SETTINGS
    WOLFSSH_USER_SETTINGS
)
