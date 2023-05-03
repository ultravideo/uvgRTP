project(uvgrtp_examples)

add_executable(binding)
add_executable(configuration)
add_executable(custom_timestamps)
add_executable(receiving_hook)
add_executable(receiving_poll)
add_executable(rtcp_hook)
add_executable(sending)
add_executable(sending_generic)
add_executable(srtp_user)        # SRTP user keys example
add_executable(srtp_zrtp)        # SRTP + ZRTP example
add_executable(zrtp_multistream) # ZRTP Multistream example
add_executable(sync_sender)      # Syncing streams example, sender
add_executable(sync_receiver)    # Syncing streams example, receiver

# Sources
target_sources(binding           PRIVATE binding.cc)
target_sources(configuration     PRIVATE configuration.cc)
target_sources(custom_timestamps PRIVATE custom_timestamps.cc)
target_sources(receiving_hook    PRIVATE receiving_hook.cc)
target_sources(receiving_poll    PRIVATE receiving_poll.cc)
target_sources(rtcp_hook         PRIVATE rtcp_hook.cc)
target_sources(sending           PRIVATE sending.cc)
target_sources(sending_generic   PRIVATE sending_generic.cc)
target_sources(srtp_user         PRIVATE srtp_user.cc)
target_sources(srtp_zrtp         PRIVATE srtp_zrtp.cc)
target_sources(zrtp_multistream  PRIVATE zrtp_multistream.cc)
target_sources(sync_sender       PRIVATE sync_sender.cc)
target_sources(sync_receiver     PRIVATE sync_receiver.cc)

# set crypto++ to be linked in examples if available
if (NOT UVGRTP_DISABLE_CRYPTO AND CRYPTOPP_FOUND)
    if(MSVC)
        set(CRYPTOPP_LIB_NAME "cryptlib")
    else()
        set(CRYPTOPP_LIB_NAME "cryptopp")
    endif()
else()
    set(CRYPTOPP_LIB_NAME "")
endif()

target_link_libraries(binding           PRIVATE uvgrtp ${CRYPTOPP_LIB_NAME})
target_link_libraries(configuration     PRIVATE uvgrtp ${CRYPTOPP_LIB_NAME})
target_link_libraries(custom_timestamps PRIVATE uvgrtp ${CRYPTOPP_LIB_NAME})
target_link_libraries(receiving_hook    PRIVATE uvgrtp ${CRYPTOPP_LIB_NAME})
target_link_libraries(receiving_poll    PRIVATE uvgrtp ${CRYPTOPP_LIB_NAME})
target_link_libraries(rtcp_hook         PRIVATE uvgrtp ${CRYPTOPP_LIB_NAME})
target_link_libraries(sending           PRIVATE uvgrtp ${CRYPTOPP_LIB_NAME})
target_link_libraries(sending_generic   PRIVATE uvgrtp ${CRYPTOPP_LIB_NAME})
target_link_libraries(srtp_user         PRIVATE uvgrtp ${CRYPTOPP_LIB_NAME})
target_link_libraries(srtp_zrtp         PRIVATE uvgrtp ${CRYPTOPP_LIB_NAME})
target_link_libraries(zrtp_multistream  PRIVATE uvgrtp ${CRYPTOPP_LIB_NAME})
target_link_libraries(sync_sender       PRIVATE uvgrtp ${CRYPTOPP_LIB_NAME})
target_link_libraries(sync_receiver     PRIVATE uvgrtp ${CRYPTOPP_LIB_NAME})