set(SRC_FILES
    counters.cpp
)

add_executable(hw1 ${SRC_FILES})

if(NOT MSVC)
    target_link_libraries(hw1 PRIVATE pthread)
endif()

include(GNUInstallDirs)

install(TARGETS hw1
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
