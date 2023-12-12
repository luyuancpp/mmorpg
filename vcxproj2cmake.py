#!/usr/bin python
#-*- encoding: utf-8 -*-

# Generate CMakeLists.txt from *.vcxproj

import xml.dom.minidom

includeDirs = []
sourceFiles = []
linkDirs = []
libs = []
projectName = ""
link_mysql = ""

abseil_libs = " \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/base/libabsl_base.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/strings/libabsl_cord.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/log/libabsl_die_if_null.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/flags/libabsl_flags.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/hash/libabsl_hash.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/log/libabsl_log_initialize.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/base/libabsl_log_severity.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/status/libabsl_status.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/status/libabsl_statusor.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/strings/libabsl_strings.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/synchronization/libabsl_synchronization.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/time/libabsl_time.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/utf8_range/libutf8_validity.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/log/libabsl_log_internal_check_op.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/debugging/libabsl_leak_check.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/log/libabsl_log_internal_conditions.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/log/libabsl_log_internal_message.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/log/libabsl_log_internal_nullguard.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/debugging/libabsl_examine_stack.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/log/libabsl_log_internal_format.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/log/libabsl_log_internal_proto.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/log/libabsl_log_internal_log_sink_set.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/log/libabsl_log_sink.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/log/libabsl_log_entry.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/flags/libabsl_flags_internal.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/flags/libabsl_flags_marshalling.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/flags/libabsl_flags_reflection.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/flags/libabsl_flags_config.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/flags/libabsl_flags_program_name.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/flags/libabsl_flags_private_handle_accessor.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/flags/libabsl_flags_commandlineflag.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/flags/libabsl_flags_commandlineflag_internal.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/log/libabsl_log_globals.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/log/libabsl_log_internal_globals.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/container/libabsl_raw_hash_set.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/hash/libabsl_city.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/hash/libabsl_low_level_hash.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/container/libabsl_hashtablez_sampler.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/strings/libabsl_cordz_info.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/strings/libabsl_cord_internal.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/strings/libabsl_cordz_functions.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/profiling/libabsl_exponential_biased.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/strings/libabsl_cordz_handle.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/crc/libabsl_crc_cord_state.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/crc/libabsl_crc32c.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/crc/libabsl_crc_internal.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/crc/libabsl_crc_cpu_detect.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/types/libabsl_bad_optional_access.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/strings/libabsl_str_format_internal.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/base/libabsl_strerror.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/debugging/libabsl_stacktrace.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/debugging/libabsl_symbolize.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/debugging/libabsl_debugging_internal.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/debugging/libabsl_demangle_internal.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/synchronization/libabsl_graphcycles_internal.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/synchronization/libabsl_kernel_timeout_internal.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/base/libabsl_malloc_internal.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/strings/libabsl_string_view.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/base/libabsl_throw_delegate.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/strings/libabsl_strings_internal.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/base/libabsl_spinlock_wait.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/numeric/libabsl_int128.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/time/libabsl_civil_time.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/time/libabsl_time_zone.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/types/libabsl_bad_variant_access.a \
/usr/src/turn-based-game/third_party/protobuf/third_party/abseil-cpp/absl/base/libabsl_raw_logging_internal.a "

# parse vcxproj file
def parseVCProjFile(vcxprojFile):
    xmldoc= xml.dom.minidom.parse(vcxprojFile)
    for rootNode in xmldoc.childNodes:
        for subNode in rootNode.childNodes:
            if subNode.nodeName == "ItemGroup":
                for itemNode in subNode.childNodes:
                    if itemNode.nodeName == "ClCompile":
                        path = itemNode._attrs["Include"].value
                        sourceFiles.append(path.replace("\\", "/"))
                    if itemNode.nodeName == "ClInclude":
                        path = itemNode._attrs["Include"].value
                        index = path.rfind("\\")
                        includeDirs.append(path[:index+1].replace("\\", "/"))
            if subNode.nodeName == "PropertyGroup":
                for propertyNode in subNode.childNodes:
                    if propertyNode.nodeName == "RootNamespace":
                        global projectName
                        projectName = propertyNode.firstChild.data
            if subNode.nodeName == "ItemDefinitionGroup":
                for defineGroup in subNode.childNodes:
                    for defineNode in defineGroup.childNodes:
                        if defineNode.nodeName == "AdditionalIncludeDirectories":
                            for additionInc in defineNode.firstChild.data.split(";"):
                                if not additionInc.startswith("%"):
                                    includeDirs.append(additionInc)
                        if defineNode.nodeName == "AdditionalLibraryDirectories":
                            for additionLib in defineNode.firstChild.data.split(";"):
                                if not additionLib.startswith("%"):
                                    linkDirs.append(additionLib)

# write cmake file
def writeCMakeLists(vcxprojDir, target_type):
    # mini version
    fileLines = "cmake_minimum_required(VERSION 3.28)\n"

    fileLines += 'set(EXECUTABLE_OUTPUT_PATH ../../bin)\n'
    fileLines += 'set(LIBRARY_OUTPUT_PATH ../../lib)\n'
    if link_mysql:
        fileLines += 'execute_process(COMMAND mysql_config --cflags OUTPUT_VARIABLE MYSQL_CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)\n'
        fileLines += 'execute_process(COMMAND mysql_config --libs OUTPUT_VARIABLE MYSQL_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)\n'
        fileLines += 'execute_process(COMMAND mysql_config --include OUTPUT_VARIABLE MYSQL_INCLUDE OUTPUT_STRIP_TRAILING_WHITESPACE)\n'

    fileLines += ("project(%s)\n\n" % projectName)

    # add define
    fileLines += 'add_definitions(-D__LINUX__)\n'
    fileLines += 'add_definitions(-D__linux__)\n'
    fileLines += "add_definitions(-DNOMINMAX)\n"
    fileLines += "add_definitions(-DGOOGLE_CHECK_EQ=ABSL_CHECK_EQ)\n"
    fileLines += "add_definitions(-DGOOGLE_LOG=ABSL_DLOG)\n"
    fileLines += "add_definitions(-DGOOGLE_DCHECK=ABSL_DCHECK)\n"
    fileLines += "add_definitions(-DENTT_ID_TYPE=uint64_t)\n\n"

    
    # include directory
    fileLines += "include_directories("
    uniqIncludeDir = list(set(includeDirs))
    for incUnit in uniqIncludeDir:
        fileLines += (incUnit + " ")
    fileLines += ")\n\n"

    # link directory
    fileLines += "link_directories("
    uniqLinkDir = list(set(linkDirs))
    for linkUnit in uniqLinkDir:
        fileLines += (linkUnit + " ")
    fileLines += " /usr/lib/x86_64-linux-gnu /usr/local/lib/ )\n\n"

    # source file
    fileLines += "set(SOURCE_FILE "
    for sourceUnit in sourceFiles:
        fileLines += (sourceUnit + " ")
    fileLines += ")\n\n"

    # set flags
    fileLines += "set(CMAKE_VERBOSE_MAKEFILE on)\n"
    fileLines += "set(CMAKE_CXX_STANDARD  20)\n"
    fileLines += "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread ")\n\n'
    if link_mysql:
        fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MYSQL_INCLUDE}")\n\n'

    # add exec or lib
    if target_type == "lib":
        fileLines += ("add_library(%s ${SOURCE_FILE})\n\n" % projectName)
    else:
        fileLines += ("add_executable(%s ${SOURCE_FILE})\n\n" % projectName)

    # link lib
    fileLines += ("target_link_libraries(%s " % projectName)
    for lib in libs:
        fileLines += ("%s " % lib)
    fileLines += " muduo_base  muduo_net muduo_protobuf_codec muduo_protorpc_wire "
    fileLines += " ${MYSQL_LIBS} protobuf hiredis lua z "
    fileLines += abseil_libs
    fileLines += " )"

    if target_type == "lib":
        libs.append((("%s") %projectName))

    # write file
    file = open(vcxprojDir + "CMakeLists.txt", "w")
    file.writelines(fileLines)
    file.close()

# CMakeList.txt
def generate(vcxprojfile, vcxprojDir, target_type):
    global includeDirs
    global sourceFiles
    global linkDirs
    includeDirs = []
    sourceFiles = []
    linkDirs = []
    if not vcxprojfile.endswith(".vcxproj"):
        raise NameError("vcxproj file name error")
    parseVCProjFile(vcxprojfile)
    writeCMakeLists(vcxprojDir, target_type)
    return projectName

link_mysql = "mysqlclient"
generate("./pkg/pbc/pbc.vcxproj", "./pkg/pbc/", "lib")
generate("./pkg/config/config.vcxproj", "./pkg/config/", "lib")
generate("./pkg/common/common.vcxproj", "./pkg/common/", "lib")
generate("./server/database_server/database_server.vcxproj", "./server/database_server/", "")
generate("./server/controller_server/controller_server.vcxproj", "./server/controller_server/", "")
generate("./server/login_server/login_server.vcxproj", "./server/login_server/", "")
generate("./server/gate_server/gate_server.vcxproj", "./server/gate_server/", "")
generate("./server/game_server/game_server.vcxproj", "./server/game_server/", "")
generate("./client/client.vcxproj", "./client/", "")
