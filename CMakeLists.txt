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

# The registry wolfssl's default user_settings.h disables RSA and DH on ESP32-P4,
# and omits WOLFSSL_ED25519_STREAMING_VERIFY — both required by wolfssh.
# Prepend our wolfssl_config/ to wolfssl's include path so it finds our
# user_settings.h (which enables RSA+ECC+DH+streaming Ed25519) before its own.
# Managed components use namespace__name format. Try both forms.
idf_component_get_property(wolfssl_lib wolfssl__wolfssl COMPONENT_LIB)
if(NOT wolfssl_lib)
    idf_component_get_property(wolfssl_lib wolfssl COMPONENT_LIB)
endif()
if(wolfssl_lib)
    target_include_directories(${wolfssl_lib} BEFORE PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/wolfssl_config"
    )
else()
    message(WARNING "esp-wolfssh-client: could not locate wolfssl target; HAVE_DH may be missing")
endif()
