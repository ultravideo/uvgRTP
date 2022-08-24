project(uvgrtp_examples)

add_executable(binding)
target_sources(binding
        PRIVATE
            binding.cc
        )
target_link_libraries(binding
        PRIVATE
            uvgrtp
        )
        
add_executable(configuration)
target_sources(configuration
        PRIVATE
            configuration.cc
        )
target_link_libraries(configuration
    PRIVATE
        uvgrtp
    )

add_executable(custom_timestamps)
target_sources(custom_timestamps
        PRIVATE
            custom_timestamps.cc
        )
target_link_libraries(custom_timestamps
        PRIVATE
            uvgrtp
        )

add_executable(receiving_hook)
target_sources(receiving_hook
        PRIVATE
            receiving_hook.cc
        )
target_link_libraries(receiving_hook
        PRIVATE
            uvgrtp
        )

add_executable(receiving_poll)
target_sources(receiving_poll
        PRIVATE
            receiving_poll.cc
        )
target_link_libraries(receiving_poll
        PRIVATE
            uvgrtp
        )

add_executable(rtcp_hook)
target_sources(rtcp_hook
        PRIVATE
            rtcp_hook.cc
        )
target_link_libraries(rtcp_hook
        PRIVATE
            uvgrtp
        )

add_executable(sending)
target_sources(sending
        PRIVATE
            sending.cc
        )
target_link_libraries(sending
        PRIVATE
            uvgrtp
        )

add_executable(sending_generic)
target_sources(sending_generic
        PRIVATE
            sending_generic.cc
        )
target_link_libraries(sending_generic
        PRIVATE
            uvgrtp
        )


# SRTP user keys example
add_executable(srtp_user)
target_sources(srtp_user
        PRIVATE
            srtp_user.cc
        )

if(MSVC)
    target_link_libraries(srtp_user
            PRIVATE
                uvgrtp
                cryptlib
            )
else()
    target_link_libraries(srtp_user
            PRIVATE
                uvgrtp
                cryptopp
            )
endif()


# SRTP + ZRTP example
add_executable(srtp_zrtp)
target_sources(srtp_zrtp
        PRIVATE
            srtp_zrtp.cc
        )

if(MSVC)
    target_link_libraries(srtp_zrtp
            PRIVATE
                uvgrtp
                cryptlib
            )
else()
    target_link_libraries(srtp_zrtp
            PRIVATE
                uvgrtp
                cryptopp
            )
endif()



# ZRTP Multistream example
add_executable(zrtp_multistream)
target_sources(zrtp_multistream
        PRIVATE
            zrtp_multistream.cc
        )

if(MSVC)
    target_link_libraries(zrtp_multistream
            PRIVATE
                uvgrtp
                cryptlib
            )
else()
    target_link_libraries(zrtp_multistream
            PRIVATE
                uvgrtp
                cryptopp
            )
endif()