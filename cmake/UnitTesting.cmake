# This file defines the two macros below for easily adding groups of unit tests and scripts,
# as well as sets up unit testing and defines several cache options used to control how
# tests and scripts are built and run(Adapted from GtsamTesting.cmake).



###############################################################################
# Macro:
#
# gtsamAddTestsGlob(groupName globPatterns excludedFiles linkLibraries)
#
# Add a group of unit tests.  A list of unit test .cpp files or glob patterns specifies the
# tests to create.  Tests are assigned into a group name so they can easily by run
# independently with a make target.  Running 'make check' builds and runs all tests.
#
# Usage example:
#   gtsamAddTestsGlob(basic "test*.cpp" "testBroken.cpp" "gtsam;GeographicLib")
#
# Arguments:
#   groupName:     A name that will allow this group of tests to be run independently, e.g.
#                  'basic' causes a 'check.basic' target to be created to run this test
#                  group.
#   globPatterns:  The list of files or glob patterns from which to create unit tests, with
#                  one test created for each cpp file.  e.g. "test*.cpp", or
#                  "testA*.cpp;testB*.cpp;testOneThing.cpp".
#   excludedFiles: A list of files or globs to exclude, e.g. "testC*.cpp;testBroken.cpp".
#                  Pass an empty string "" if nothing needs to be excluded.
#   linkLibraries: The list of libraries to link to in addition to CppUnitLite.
macro(gtsamAddTestsGlob groupName globPatterns excludedFiles linkLibraries)
	gtsamAddTestsGlob_impl("${groupName}" "${globPatterns}" "${excludedFiles}" "${linkLibraries}" "")
endmacro()

macro(gtsamAddTestsGlobAll groupName globPatterns excludedFiles linkLibraries otherCppFiles)
	gtsamAddTestsGlob_impl("${groupName}" "${globPatterns}" "${excludedFiles}" "${linkLibraries}" "${otherCppFiles}")
endmacro()


# Implementation follows:

# Build macros for using tests
enable_testing()

option(GTSAM_BUILD_TESTS                 "Enable/Disable building of tests"          ON)

# Add option for combining unit tests
if(MSVC OR XCODE_VERSION)
	option(GTSAM_SINGLE_TEST_EXE "Combine unit tests into single executable (faster compile)" ON)
else()
	option(GTSAM_SINGLE_TEST_EXE "Combine unit tests into single executable (faster compile)" OFF)
endif()
mark_as_advanced(GTSAM_SINGLE_TEST_EXE)

# Enable make check (http://www.cmake.org/Wiki/CMakeEmulateMakeCheck)
if(GTSAM_BUILD_TESTS)
    add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIGURATION> --output-on-failure)
endif()

# Add examples target
add_custom_target(examples)

# Add timing target
add_custom_target(timing)

# Add target to build tests without running
add_custom_target(all.tests)

# Implementations of this file's macros:

macro(gtsamAddTestsGlob_impl groupName globPatterns excludedFiles linkLibraries otherCppFiles)
	if(GTSAM_BUILD_TESTS)
		# Add group target if it doesn't already exist
	    if(NOT TARGET check.${groupName})
			add_custom_target(check.${groupName} COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIGURATION> --output-on-failure)
		endif()

	    # Get all script files
        file(GLOB script_files ${globPatterns})

	    # Remove excluded scripts from the list
	    if(NOT "${excludedFiles}" STREQUAL "")
			file(GLOB excludedFilePaths ${excludedFiles})
			if("${excludedFilePaths}" STREQUAL "")
				message(WARNING "The pattern '${excludedFiles}' for excluding tests from group ${groupName} did not match any files")
			else()
		    	list(REMOVE_ITEM script_files ${excludedFilePaths})
			endif()
	    endif()

		# Separate into source files and headers (allows for adding headers to show up in
		# MSVC and Xcode projects).
		set(script_srcs "")
		set(script_headers "")
		foreach(script_file IN ITEMS ${script_files})
			get_filename_component(script_ext ${script_file} EXT)
			if(script_ext MATCHES "(h|H)")
				list(APPEND script_headers ${script_file})
			else()
				list(APPEND script_srcs ${script_file})
			endif()
		endforeach()

		# Don't put test files in folders in MSVC and Xcode because they're already grouped
		source_group("" FILES ${script_srcs} ${script_headers})

		if(NOT GTSAM_SINGLE_TEST_EXE)
			# Default for Makefiles - each test in its own executable
			foreach(script_src IN ITEMS ${script_srcs})
				# Get test base name
				get_filename_component(script_name ${script_src} NAME_WE)

				# Add executable
        set(script_src_all "${script_src}" "${otherCppFiles}")
				add_executable(${script_name} ${script_src_all} ${script_headers})
				target_link_libraries(${script_name} CppUnitLite ${linkLibraries})
        #-- message(STATUS "ZHENGDONG: target_link_libraries(${script_name} CppUnitLite ${linkLibraries})")

				# Add target dependencies
				add_test(NAME ${script_name} COMMAND ${script_name})
				add_dependencies(check.${groupName} ${script_name})
				add_dependencies(check ${script_name})
                add_dependencies(all.tests ${script_name})
				if(NOT MSVC AND NOT XCODE_VERSION)
				  add_custom_target(${script_name}.run ${EXECUTABLE_OUTPUT_PATH}${script_name} DEPENDS ${script_name})
				endif()

				# Add TOPSRCDIR
				set_property(SOURCE ${script_src} APPEND PROPERTY COMPILE_DEFINITIONS "TOPSRCDIR=\"${PROJECT_SOURCE_DIR}\"")

				# Exclude from 'make all' and 'make install'
				set_target_properties(${script_name} PROPERTIES EXCLUDE_FROM_ALL ON)

				# Configure target folder (for MSVC and Xcode)
				set_property(TARGET ${script_name} PROPERTY FOLDER "Unit tests/${groupName}")
			endforeach()
		else()

			#skip folders which don't have any tests
			if(NOT script_srcs)
				return()
			endif()

			# Default on MSVC and XCode - combine test group into a single exectuable
			set(target_name check_${groupName}_program)

			set(script_src_all "${script_srcs}" "${otherCppFiles}")
			# Add executable
			add_executable(${target_name} "${script_src_all}" ${script_headers})
			target_link_libraries(${target_name} CppUnitLite ${linkLibraries})

			# Only have a main function in one script - use preprocessor
			set(rest_script_srcs ${script_srcs})
			list(REMOVE_AT rest_script_srcs 0)
			set_property(SOURCE ${rest_script_srcs} APPEND PROPERTY COMPILE_DEFINITIONS "main=inline no_main")

			# Add target dependencies
			add_test(NAME ${target_name} COMMAND ${target_name})
			add_dependencies(check.${groupName} ${target_name})
			add_dependencies(check ${target_name})
			if(NOT XCODE_VERSION)
				add_dependencies(all.tests ${target_name})
			endif()

			# Add TOPSRCDIR
			set_property(SOURCE ${script_srcs} APPEND PROPERTY COMPILE_DEFINITIONS "TOPSRCDIR=\"${PROJECT_SOURCE_DIR}\"")

			# Exclude from 'make all' and 'make install'
			set_target_properties(${target_name} PROPERTIES EXCLUDE_FROM_ALL ON)

			# Configure target folder (for MSVC and Xcode)
			set_property(TARGET ${script_name} PROPERTY FOLDER "Unit tests")
		endif()
	endif()
endmacro()