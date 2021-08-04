#!/usr/bin/env python
#-*- encoding: utf-8 -*-

# Generate CMakeLists.txt from *.vcxproj

import xml.dom.minidom

includeDirs = []
sourceFiles = []
linkDirs = []
libs = []
projectName = ""
link_mysql = ""


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

    fileLines += 'set(EXECUTABLE_OUTPUT_PATH ../bin)\n'
    fileLines += 'set(LIBRARY_OUTPUT_PATH ../bin)\n'
    fileLines += ("project(%s)\n\n" % projectName)

    # add define
    fileLines += "add_definitions(-D__LINUX__)\n\n"
    fileLines += "add_definitions(-D__linux__)\n\n"

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
    fileLines += ")\n\n"

    # source file
    fileLines += "set(SOURCE_FILE "
    for sourceUnit in sourceFiles:
        fileLines += (sourceUnit + " ")
    fileLines += ")\n\n"

    # set flags
    fileLines += "set(CMAKE_VERBOSE_MAKEFILE on)\n"
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -o0")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-Bstatic")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/usr/local/include -L/usr/local/lib -lprotobuf ")\n'
    fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")\n\n'
    global link_mysql
    if link_mysql == "mysqlclient" :
          fileLines += 'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L/usr/lib/x86_64-linux-gnu -lmysqlclient -lpthread")\n\n'

    if target_type == "lib":
        # add exec
        fileLines += ("add_library(%s ${SOURCE_FILE})\n\n" % projectName)
	global libs
	libname = (("%s.a") %projectName)
        libs.append((("%s.a") %projectName))
    else:
        # add exec
        fileLines += ("add_executable(%s ${SOURCE_FILE})\n\n" % projectName)

    # link lib
    fileLines += ("target_link_libraries(%s " % projectName)
    for lib in libs:
        fileLines += (lib + " ")
    fileLines += ("libprotobuf.a libprotobuf-lite.a libmysqlclient.a)")
    if link_mysql == "mysqlclient" :
        #fileLines += ("target_link_libraries(%s libbmysqlclient.a libssl.a libcrypto.a libdl.a)" % projectName )

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

generate("./protopb/protopb.vcxproj", "./protopb/", "lib")
link_mysql = "mysqlclient"
generate("./common/common.vcxproj", "./common/", "lib")
generate("./third_party/third_party.vcxproj", "./third_party/", "lib")
generate("./deploy_server/deploy_server.vcxproj", "./deploy_server/", "")
generate("./client/client.vcxproj", "./client/", "")
