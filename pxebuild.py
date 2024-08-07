#!/usr/bin/env python

import sys
import os
import time
from optparse import OptionParser
import signal
import logging

VERBOSE_MODE=" >/dev/null "
MERGEROM = "./util/mergerom.pl"
ROM_PREFIX_FILE = "arch/i386/prefix/romprefix.S"
OLD_PREFIX_FILE = "arch/i386/prefix/romprefix.S.old"
TEMP_OUTPUT_FILE = "arch/i386/prefix/romprefix.S.new"
GENERAL_FILE = "config/general.h"

DEVICES = {
        "4099": "ConnectX3",
        "4103": "ConnectX3-Pro",
        "4113": "ConnectIB",
        "4115": "ConnectX4"
        }

def openFile(fileName):
    try:
        sourceFile = open(fileName,'r')
    except:
        print "ERROR: Can't open file: %s" % fileName
        cleanup()
        exit(1)
    lines = sourceFile.readlines()
    sourceFile.close()
    return lines

def writeOutput(chunk, fileName):
    try:
        outputFile = open(fileName, 'w')
    except:
        print "ERROR: Can't open file: %s" % BIST_CONF_FILENAME
        cleanup()
        exit(1)
    outputFile.write(chunk)
    outputFile.close()
    return

def configureParser(optParser):
    optParser.add_option("-d", "--device", default="ALL",  help="Device ID. e.g. 4099. (default=ALL)")
    optParser.add_option("-v", "--version", default="3.4.400", help="FlexBoot version")
    optParser.add_option("--release", action="store_true", default=False, help="Build release" )
    optParser.add_option("--debug", default="", help="Add debug flags (<file1>:<verbosity>,<file2>:<verbosity>,...)")
    optParser.add_option("--j_cpus", default="`getconf _NPROCESSORS_ONLN`", help="Concurrent compilation (default: according to cpu count)")

def checkOptions(options):
    if options.release and options.debug != "":
        print "Can't build release version with debug flags"
        return 1

    if options.device:
        if (not options.device == "ALL") and (not options.device in DEVICES):
            print "Device not found. Please enter a valid device ID or \"ALL\""
            print "Valid devices:"
            for dev in DEVICES.keys():
                print "ID: " + dev + " - Device: " + DEVICES[dev]
            return 1
    else:
        return 1

    if len(options.version.split(".")) != 3:
        print "Invalid version format. Must be A.B.XYZ (e.g. 3.4.160)"
        return 1

    return 0

def updateRomPrefix(deviceId, version):
    os.system("\cp -f " + ROM_PREFIX_FILE + "_ " + ROM_PREFIX_FILE)
    os.system("rm -f " + OLD_PREFIX_FILE)
    os.system("rm -f " + TEMP_OUTPUT_FILE)

    versionList = version.split(".")
    for subVersion in versionList:
        if len ( subVersion ) > 4:
            print "Error romversion %s" % version
            cleanup()
            exit(1)

    prefix = "driver_version:\n .align 16\n .long 0x73786c6d\n .long 0x3a6e6769\n .long 0x0010"

    # Preparing rest of ROM extension
    prefix = prefix + "%04x" % int ( versionList[0] )
    prefix = prefix + "\n .long 0x" +  "%04x" % int ( versionList[1] )
    prefix = prefix + "%04x" % int ( versionList[2] )
    prefix = prefix + "\n .long " + str ( hex ( int ( deviceId ) ) )
    # Adding port number
    prefix = prefix + "00"
    # Adding protocol type
    prefix = prefix + "ff"
    prefix = prefix + "\n\n"

    #open file
    fileLines = openFile(ROM_PREFIX_FILE)

    #create new file
    chunk = ""
    foundRomVersion = False
    for line in fileLines:
        if foundRomVersion == True:
            if line.find("/* ROM version added */") != -1:
                foundRomVersion = False
            else:
                continue
        if line.find("driver_version:") != -1:
            foundRomVersion = True
            chunk = "".join([chunk, prefix])
            continue

        chunk = "".join([chunk, line])

    #write file into temporary file
    writeOutput(chunk, TEMP_OUTPUT_FILE)

    #replace original file
    print "Replacing ROM file"
    rc = os.system("mv -f " + ROM_PREFIX_FILE + " " + OLD_PREFIX_FILE)
    if rc != 0:
        print "Error replacing file 1"
        cleanup()
        exit(rc)

    rc = os.system("mv -f " + TEMP_OUTPUT_FILE + " " + ROM_PREFIX_FILE)
    if rc != 0:
        print "Error replacing file 2"
        cleanup()
        exit(rc)

    return

def build(options, version, requestedDevices, outputDir, debugString):
    for dev in requestedDevices:
        os.system("make clean");
        isFC = (dev == "4099" or dev == "4103")
        print "Current build configuration: Device - " + dev + " ; FC - " + str(isFC)
        updateRomPrefix(dev, version)

        flags = "-Imlx_common/include --include mlx_debug.h --include mlx_bullseye.h "

        if not options.release:
            flags = flags + "-DMLX_DEBUG "

        if isFC:
            flags = flags + "-DFLASH_CONFIGURATION "

        # Add debug flag
        debugFlags = ""
        if debugString != "":
            debugFlags = "DEBUG='" + debugString + "'"

        #-j `getconf _NPROCESSORS_ONLN`

        fileName = "FlexBoot-" + version + "_"
        if not options.release:
            fileName += "DEBUG_"

        fileName += dev
        fileNamePrefix = fileName
        fileName += ROM_SUFFIX

        eflags='EXTRA_CFLAGS="'+flags+'"'

        cmd = "make -j "+options.j_cpus+"   bin/" + requestedDevices[dev] + ROM_SUFFIX+' ' + eflags + ' ' + debugFlags
        rc = 0

	os.system("echo Executing build:%s"%(cmd))
        rc = os.system(cmd)
        if rc != 0:
            print "Build failed!"
            return rc

        # Copy output file to build directory
        os.system("\cp -f bin/" + requestedDevices[dev] + ROM_SUFFIX+" " + " " + outputDir + "/" + fileName)
        os.system("\cp -f bin/" + requestedDevices[dev] + ROM_SUFFIX+".tmp" + " " + outputDir + "/"+fileName+".tmp")
        os.system("\cp -f bin/" + requestedDevices[dev] + ROM_SUFFIX+".tmp.map" + " " + outputDir + "/"+fileName+".tmp.map")

    return 0

def merge(srcDir, destDir):
    if (os.path.isdir(srcDir)):
        rc = os.system("mkdir -p " + destDir)
        if rc != 0:
            print "Failed to create directory " + dir
            return rc

        files = []
        for root, d, f in os.walk(srcDir):
            files.extend(f)
            break
        for file in files:
            if not file.endswith('mrom'):
                continue

            if  file.find('4099') != -1:
                # Merge DELL CLP
                clpFileName = '.'.join(os.path.realpath(CLP_DIRS["dell"] + "latest_4099.bin").split("/")[-1].split(".")[:-1])
                outputMrom = '.'.join(file.split(".")[:-1]) + "_" + clpFileName + ROM_SUFFIX
                cmd = MERGEROM + " " + root + "/" + file + " " + CLP_DIRS["dell"] + "latest_4099.bin > " + destDir + "/" + outputMrom
                print cmd
                rc = os.system(cmd)
                if rc != 0:
                    print "Failed to create file"

                # Merge HP CLP
                clpFileName = '.'.join(os.path.realpath(CLP_DIRS["hp"] + "latest_4099.bin").split("/")[-1].split(".")[:-1])
                outputMrom = '.'.join(file.split(".")[:-1]) + "_" + clpFileName + ROM_SUFFIX
                cmd = MERGEROM + " " + root + "/" + file + " " + CLP_DIRS["hp"] + "latest_4099.bin > " + destDir + "/" + outputMrom
                print cmd
                rc = os.system(cmd)
                if rc != 0:
                    print "Failed to create file"

                # Merge Huawei CLP
                clpFileName = '.'.join(os.path.realpath(CLP_DIRS["huawei"] + "latest_4099.bin").split("/")[-1].split(".")[:-1])
                outputMrom = '.'.join(file.split(".")[:-1]) + "_" + clpFileName + ROM_SUFFIX
                cmd = MERGEROM + " " + root + "/" + file + " " + CLP_DIRS["huawei"] + "latest_4099.bin > " + destDir + "/" + outputMrom
                print cmd
                rc = os.system(cmd)
                if rc != 0:
                    print "Failed to create file"

            if file.find('4103') != -1:
                # Merge HP CLP
                clpFileName = '.'.join(os.path.realpath(CLP_DIRS["hp"] + "latest_4103.bin").split("/")[-1].split(".")[:-1])
                outputMrom = '.'.join(file.split(".")[:-1]) + "_" + clpFileName + ROM_SUFFIX
                cmd = MERGEROM + " " + root + "/" + file + " " + CLP_DIRS["hp"] + "latest_4103.bin > " + destDir + "/" + outputMrom
                print cmd
                rc = os.system(cmd)
                if rc != 0:
                    print "Failed to create file"

                # Merge Huawei CLP
                clpFileName = '.'.join(os.path.realpath(CLP_DIRS["huawei"] + "latest_4103.bin").split("/")[-1].split(".")[:-1])
                outputMrom = '.'.join(file.split(".")[:-1]) + "_" + clpFileName + ROM_SUFFIX
                cmd = MERGEROM + " " + root + "/" + file + " " + CLP_DIRS["huawei"] + "latest_4103.bin > " + destDir + "/" + outputMrom
                print cmd
                rc = os.system(cmd)
                if rc != 0:
                    print "Failed to create file"

            if file.find('4113') != -1:
                # Merge HP CLP
                clpFileName = '.'.join(os.path.realpath(CLP_DIRS["hp"] + "latest_4113.bin").split("/")[-1].split(".")[:-1])
                outputMrom = '.'.join(file.split(".")[:-1]) + "_" + clpFileName + ROM_SUFFIX
                cmd = MERGEROM + " " + root + "/" + file + " " + CLP_DIRS["hp"] + "latest_4113.bin > " + destDir + "/" + outputMrom
                print cmd
                rc = os.system(cmd)
                if rc != 0:
                    print "Failed to create file"

    return 0

def mergeCLP(srcDir, destDir):
    print "Creating CLP Merged ROMs..."
    merge(srcDir + "/", destDir + "/")

######################################################################
# MAIN
######################################################################
def cleanup():
    # Move back the original files
    os.system("mv -f " + GENERAL_FILE + "_ " + GENERAL_FILE+" 2>/dev/null")
    os.system("mv -f " + ROM_PREFIX_FILE + "_ " + ROM_PREFIX_FILE+" 2>/dev/null")
    os.system("rm -f " + OLD_PREFIX_FILE+" 2>/dev/null")
    os.system("rm -f " + TEMP_OUTPUT_FILE+" 2>/dev/null")

def signal_handler(signal, frame):
    print "You pressed Ctrl+C!"
    cleanup()
    exit(1)

def checkpatch():
    logging.info("Running checkpatch on workspace")
    ret=exprom_process.SystemExec("cpatch_raw.sh")
    logging.info("returncode=%d output:\n%s"%(ret['returncode'],ret['output']))
    return ret['returncode']

if __name__ == "__main__":
    try:
        # Make sure we start from clean environment
        cleanup()
        signal.signal(signal.SIGINT, signal_handler)
        optParser = OptionParser();
        configureParser(optParser)
        (options, args) = optParser.parse_args()

        if (len(args) > 0):
            optParser.print_help()
            exit(1)

        VERBOSE_MODE=""
        ROM_SUFFIX=".mrom"

        rc = checkOptions(options)
        if rc != 0:
            optParser.print_help()
            exit(1)

        ##################################################################
        ## From here we work on the current directory (local or cloned) ##
        ##################################################################

        # Create a backup files
        os.system("\cp -f " + GENERAL_FILE + " " + GENERAL_FILE + "_")
        os.system("\cp -f " + ROM_PREFIX_FILE + " " + ROM_PREFIX_FILE + "_")


        # Set general.h with version
        newlines = []
        oldlines = openFile(GENERAL_FILE)
        for line in oldlines:
            newlines.append(line.replace('__BLD_VERSION__', '__BUILD_VERSION__ \"' + options.version + '\"'))
        writeOutput("".join(newlines), GENERAL_FILE)

        if (options.device == "ALL"):
            requestedDevices = DEVICES
        else:
            requestedDevices = {options.device: DEVICES[options.device]}

        # Create destination directory
        buildDir = "./build_" + options.version
        if (os.path.isdir(buildDir)):
            print "Removing " + buildDir
            rc = os.system("rm -rf " + buildDir)
            if rc != 0:
                print "Failed to remove directory " + buildDir + ". Exiting..."
                cleanup()
                exit(rc);
        print "Creating output directory: " + buildDir
        rc = os.system("mkdir -p " + buildDir);
        if rc != 0:
            print "Failed to create directory " + buildDir + ". Exiting..."
            cleanup()
            exit(rc);

        # Build versions
        outputDir = buildDir
        outputDir += "/"
        if (not os.path.isdir(outputDir)):
            rc = os.system("mkdir -p " + outputDir);
            if rc != 0:
                print "Failed to create directory " + outputDir + ". Exiting..."
                cleanup()
                exit(rc);
        rc = build(options, options.version, requestedDevices, outputDir, options.debug)
        if rc != 0:
            cleanup()
            exit(rc);

    except Exception, e:
        os.system("echo Exception during build: "+str(e))
    finally:
        cleanup()

    print "Done."
    exit(0)
