set(SRC_FILES
    queue.cpp
    main.cpp
)

add_executable(hw2 ${SRC_FILES})

if(NOT MSVC)
    target_link_libraries(hw2 PRIVATE pthread)
endif()

include(GNUInstallDirs)

install(TARGETS hw2
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
