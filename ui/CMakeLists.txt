
add_executable(CompositeCefSubprocess MACOSX_BUNDLE ${CEF_SUBPROCESS_SRC_FILES})

source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${CEF_SUBPROCESS_SRC_FILES})

target_link_libraries(CompositeCefSubprocess PRIVATE CEF)
target_include_directories(CompositeCefSubprocess PRIVATE ${CEF_SUBPROCESS_SRC_DIR})

install(
	TARGETS CompositeCefSubprocess
	RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}"
	BUNDLE DESTINATION "${CMAKE_INSTALL_PREFIX}/CompositeEngine.app/Contents/Frameworks"
)

if(OS_WINDOWS)
	target_compile_options(CompositeCefSubprocess PRIVATE /W3 /WX)

	# TODO: ONLY FOR DEBUG BUILDS
	install(
		FILES $<TARGET_PDB_FILE:CompositeCefSubprocess>
		DESTINATION "${CMAKE_INSTALL_PREFIX}"
		OPTIONAL
	)
endif()
