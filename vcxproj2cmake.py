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
absl_bad_any_cast_impl \
absl_bad_optional_access \
absl_bad_variant_access \
absl_base \
absl_city \
absl_civil_time \
absl_cord \
absl_cord_internal \
absl_cordz_functions \
absl_cordz_handle \
absl_cordz_info \
absl_cordz_sample_token \
absl_crc32c \
absl_crc_cord_state \
absl_crc_cpu_detect \
absl_crc_internal \
absl_debugging_internal \
absl_demangle_internal \
absl_die_if_null \
absl_examine_stack \
absl_exponential_biased \
absl_failure_signal_handler \
absl_flags \
absl_flags_commandlineflag \
absl_flags_commandlineflag_internal \
absl_flags_config \
absl_flags_internal \
absl_flags_marshalling \
absl_flags_parse \
absl_flags_private_handle_accessor \
absl_flags_program_name \
absl_flags_reflection \
absl_flags_usage \
absl_flags_usage_internal \
absl_graphcycles_internal \
absl_hash \
absl_hashtablez_sampler \
absl_int128 \
absl_kernel_timeout_internal \
absl_leak_check \
absl_log_entry \
absl_log_flags \
absl_log_globals \
absl_log_initialize \
absl_log_internal_check_op \
absl_log_internal_conditions \
absl_log_internal_format \
absl_log_internal_globals \
absl_log_internal_log_sink_set \
absl_log_internal_message \
absl_log_internal_nullguard \
absl_log_internal_proto \
absl_log_severity \
absl_log_sink \
absl_low_level_hash \
absl_malloc_internal \
absl_periodic_sampler \
absl_random_distributions \
absl_random_internal_distribution_test_util \
absl_random_internal_platform \
absl_random_internal_pool_urbg \
absl_random_internal_randen \
absl_random_internal_randen_hwaes \
absl_random_internal_randen_hwaes_impl \
absl_random_internal_randen_slow \
absl_random_internal_seed_material \
absl_random_seed_gen_exception \
absl_random_seed_sequences \
absl_raw_hash_set \
absl_raw_logging_internal \
absl_scoped_set_env \
absl_spinlock_wait \
absl_stacktrace \
absl_status \
absl_statusor \
absl_strerror \
absl_strings \
absl_strings_internal \
absl_string_view \
absl_str_format_internal \
absl_symbolize \
absl_synchronization \
absl_throw_delegate \
absl_time \
absl_time_zone \
utf8_range \
utf8_validity "

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
    fileLines += " /usr/lib/x86_64-linux-gnu /usr/local/lib/ /usr/lib/asbel)\n\n"

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
        fileLines += "add_subdirectory(../../third_party/abseil-cpp)\n"
        fileLines += ("add_executable(%s ${SOURCE_FILE})\n\n" % projectName)

    # link lib
    fileLines += ("target_link_libraries(%s " % projectName)
    for lib in libs:
        fileLines += ("%s " % lib)
    fileLines += abseil_libs
    fileLines += " muduo_base  muduo_net muduo_protobuf_codec muduo_protorpc_wire "
    fileLines += (" ${MYSQL_LIBS} protobuf hiredis lua z )")

    if target_type == "lib":
        libs.append((("%s") %projectName))

    #check
    fileLines += "\nif(CMAKE_CXX_STANDARD LESS 20)"
    fileLines += '\n message(FATAL_ERROR '
    fileLines += '"my_lib_project requires CMAKE_CXX_STANDARD >= 20 (got: ${CMAKE_CXX_STANDARD})")'
    fileLines += "\nendif()" + '\n'

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
