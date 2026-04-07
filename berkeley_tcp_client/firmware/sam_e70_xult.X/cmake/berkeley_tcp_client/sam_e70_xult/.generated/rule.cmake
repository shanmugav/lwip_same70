# The following functions contains all the flags passed to the different build stages.

set(PACK_REPO_PATH "C:/Users/I41802/.mchp_packs" CACHE PATH "Path to the root of a pack repository.")

function(berkeley_tcp_client_sam_e70_xult_sam_e70_xult_XC32_assemble_rule target)
    set(options
        "-g"
        "${ASSEMBLER_PRE}"
        "-mprocessor=ATSAME70Q21B"
        "-Wa,--defsym=__MPLAB_BUILD=1${MP_EXTRA_AS_POST},--defsym=__MPLAB_DEBUG=1,--defsym=__DEBUG=1"
        "-g,-I${CMAKE_CURRENT_SOURCE_DIR}/../../.."
        "-mdfp=${PACK_REPO_PATH}/Microchip/SAME70_DFP/4.10.248/same70b")
    list(REMOVE_ITEM options "")
    target_compile_options(${target} PRIVATE "${options}")
    target_compile_definitions(${target} PRIVATE "__DEBUG=1")
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
endfunction()
function(berkeley_tcp_client_sam_e70_xult_sam_e70_xult_XC32_assembleWithPreprocess_rule target)
    set(options
        "-x"
        "assembler-with-cpp"
        "-g"
        "${MP_EXTRA_AS_PRE}"
        "${DEBUGGER_NAME_AS_MACRO}"
        "-mdfp=${PACK_REPO_PATH}/Microchip/SAME70_DFP/4.10.248/same70b"
        "-mprocessor=ATSAME70Q21B"
        "-Wa,--defsym=__MPLAB_BUILD=1${MP_EXTRA_AS_POST},--defsym=__MPLAB_DEBUG=1,--defsym=__DEBUG=1,-I${CMAKE_CURRENT_SOURCE_DIR}/../../..")
    list(REMOVE_ITEM options "")
    target_compile_options(${target} PRIVATE "${options}")
    target_compile_definitions(${target}
        PRIVATE "__DEBUG"
        PRIVATE "XPRJ_sam_e70_xult=sam_e70_xult")
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
endfunction()
function(berkeley_tcp_client_sam_e70_xult_sam_e70_xult_XC32_compile_rule target)
    set(options
        "-g"
        "${CC_PRE}"
        "-x"
        "c"
        "-c"
        "-mprocessor=ATSAME70Q21B"
        "-ffunction-sections"
        "-fdata-sections"
        "-O1"
        "-mdfp=${PACK_REPO_PATH}/Microchip/SAME70_DFP/4.10.248/same70b")
    list(REMOVE_ITEM options "")
    target_compile_options(${target} PRIVATE "${options}")
    target_compile_definitions(${target}
        PRIVATE "__DEBUG"
        PRIVATE "HAVE_CONFIG_H"
        PRIVATE "WOLFSSL_IGNORE_FILE_WARN"
        PRIVATE "XPRJ_sam_e70_xult=sam_e70_xult")
    target_include_directories(${target}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/config/default"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/config/default/library"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/config/default/library/tcpip/src"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/config/default/library/tcpip/src/common"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/config/sam_e70_xult"
        PRIVATE "../src/config/sam_e70_xult/library"
        PRIVATE "../src/config/sam_e70_xult/library/tcpip/src"
        PRIVATE "../src/config/sam_e70_xult/library/tcpip/src/common"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/packs/ATSAME70Q21B_DFP"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/packs/CMSIS"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/packs/CMSIS/CMSIS/Core/Include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/third_party/rtos/FreeRTOS/Source/include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/third_party/rtos/FreeRTOS/Source/portable/GCC/SAM/CM7"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/third_party/wolfssl"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/third_party/wolfssl/wolfssl"
        PRIVATE "${PACK_REPO_PATH}/ARM/CMSIS/5.8.0/CMSIS/Core/Include")
endfunction()
function(berkeley_tcp_client_sam_e70_xult_sam_e70_xult_XC32_compile_cpp_rule target)
    set(options
        "-g"
        "${CC_PRE}"
        "${DEBUGGER_NAME_AS_MACRO}"
        "-mprocessor=ATSAME70Q21B"
        "-frtti"
        "-fexceptions"
        "-fno-check-new"
        "-fenforce-eh-specs"
        "-ffunction-sections"
        "-O1"
        "-fno-common"
        "-mdfp=${PACK_REPO_PATH}/Microchip/SAME70_DFP/4.10.248/same70b")
    list(REMOVE_ITEM options "")
    target_compile_options(${target} PRIVATE "${options}")
    target_compile_definitions(${target}
        PRIVATE "__DEBUG"
        PRIVATE "XPRJ_sam_e70_xult=sam_e70_xult")
    target_include_directories(${target}
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/config/default"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/config/sam_e70_xult"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/packs/ATSAME70Q21B_DFP"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/packs/CMSIS"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/packs/CMSIS/CMSIS/Core/Include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/third_party/rtos/FreeRTOS/Source/include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/third_party/rtos/FreeRTOS/Source/portable/GCC/SAM/CM7"
        PRIVATE "${PACK_REPO_PATH}/ARM/CMSIS/5.8.0/CMSIS/Core/Include")
endfunction()
function(berkeley_tcp_client_sam_e70_xult_dependentObject_rule target)
    set(options
        "-mprocessor=ATSAME70Q21B"
        "-mdfp=${PACK_REPO_PATH}/Microchip/SAME70_DFP/4.10.248/same70b")
    list(REMOVE_ITEM options "")
    target_compile_options(${target} PRIVATE "${options}")
endfunction()
function(berkeley_tcp_client_sam_e70_xult_link_rule target)
    set(options
        "-g"
        "${MP_EXTRA_LD_PRE}"
        "${DEBUGGER_OPTION_TO_LINKER}"
        "${DEBUGGER_NAME_AS_MACRO}"
        "-mprocessor=ATSAME70Q21B"
        "-mno-device-startup-code"
        "-Wl,--defsym=__MPLAB_BUILD=1${MP_EXTRA_LD_POST},--script=${berkeley_tcp_client_sam_e70_xult_LINKER_SCRIPT},--defsym=__MPLAB_DEBUG=1,--defsym=__DEBUG=1,--defsym=_min_heap_size=64960,--gc-sections,-L${CMAKE_CURRENT_SOURCE_DIR}/../../..,-Map=mem.map,--memorysummary,memoryfile.xml"
        "-mdfp=${PACK_REPO_PATH}/Microchip/SAME70_DFP/4.10.248/same70b")
    list(REMOVE_ITEM options "")
    target_link_options(${target} PRIVATE "${options}")
    target_compile_definitions(${target} PRIVATE "XPRJ_sam_e70_xult=sam_e70_xult")
endfunction()
