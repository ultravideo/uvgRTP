set(CPACK_COMPONENTS_ALL ${PROJECT_NAME}_Runtime ${PROJECT_NAME}_Develop)
set(CPACK_PACKAGE_NAME   ${PROJECT_NAME})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${uvgrtp_DESCR})
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_PACKAGE_CONTACT https://github.com/polusto)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER polusto)
set(CPACK_RPM_COMPONENT_INSTALL TRUE)
set(CPACK_DEB_COMPONENT_INSTALL TRUE)
set(CPACK_COMPONENTS_GROUPING ONE_PER_GROUP)
set(CPACK_DEBIAN_UVGRTP_DEVELOP_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-dev.deb")
set(CPACK_DEBIAN_UVGRTP_RUNTIME_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${CMAKE_HOST_SYSTEM_PROCESSOR}.deb")

set(CPACK_PACKAGE_DESCRIPTION_FILE
        ${CMAKE_CURRENT_LIST_DIR}/Description.txt
        )
set(CPACK_RESOURCE_FILE_WELCOME
        ${CMAKE_CURRENT_LIST_DIR}/Welcome.txt
        )
set(CPACK_RESOURCE_FILE_LICENSE
        ${CMAKE_CURRENT_LIST_DIR}/License.txt
        )
set(CPACK_RESOURCE_FILE_README
        ${CMAKE_CURRENT_LIST_DIR}/Readme.txt
        )
set(CPACK_SOURCE_IGNORE_FILES
        /\\.git/
        /\\.circleci/
        /\\.idea/
        \\.swp
        \\.orig
        /CMakeLists\\.txt\\.user
        /privateDir/
        /cmake\\-build.*/
        )
include(CPack)

if(WIN32)
    set(CPACK_GENERATOR ZIP WIX)
elseif(APPLE)
    set(CPACK_GENERATOR TGZ productbuild)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR TGZ RPM)
else()
    set(CPACK_GENERATOR TGZ)
endif()

cpack_add_component(${PROJECT_NAME}_Runtime
        DISPLAY_NAME Runtime
        Description "Shared libraries"
        REQUIRED
        INSTALL_TYPES Full Developer Minimal)

cpack_add_component(${PROJECT_NAME}_Development
        DISPLAY_NAME "Developer pre-requisites"
        DESCRIPTION "Headers needed for development"
        DEPENDS  ${PROJECT_NAME}_Runtime
        INSTALL_TYPES Full Developer)

cpack_add_component(${PROJECT_NAME}_Samples
        DISPLAY_NAME "Code Samples"
        INSTALL_TYPES Full Developer
        DISABLED)

cpack_add_install_type(Full)
cpack_add_install_type(Minimal)
cpack_add_install_type(Developer
        DISPLAY_NAME "SDK Development")
