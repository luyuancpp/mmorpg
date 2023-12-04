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

abseil_libs = " absl::strings "

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
    fileLines = "cmake_minimum_required(VERSION 3.0)\n"

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
    fileLines += "add_definitions(-DGOOGLE_LOG=ABSL_LOG)\n"
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
    fileLines += " /usr/lib/x86_64-linux-gnu /usr/local/lib/)\n\n"

    # source file
    fileLines += "set(SOURCE_FILE "
    for sourceUnit in sourceFiles:
        fileLines += (sourceUnit + " ")
    fileLines += ")\n\n"

    # set flags
    fileLines += "set(CMAKE_VERBOSE_MAKEFILE on)\n"
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread ")\n\n'
    if link_mysql:
        fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MYSQL_INCLUDE}")\n\n'

    if target_type == "lib":
        # add exec
        fileLines += ("add_library(%s ${SOURCE_FILE})\n\n" % projectName)
    else:
        # add exec
        fileLines += ("add_executable(%s ${SOURCE_FILE})\n\n" % projectName)

    # link lib
    fileLines += ("target_link_libraries(%s " % projectName)
    for lib in libs:
        fileLines += ("%s " % lib)
    fileLines += abseil_libs
    fileLines += (" ${MYSQL_LIBS} libprotobuf.a libprotobuf-lite.a hiredis lua liblua.a libmuduo_base.a  libmuduo_net.a libz.a)")

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
