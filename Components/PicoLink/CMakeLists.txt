register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/PicoLink.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/PicoLink.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../common/protocol/PicoProtocol.cpp"
    DEPENDS
        Drv_Interfaces
        Svc_Sched
)

target_include_directories("${FPRIME_CURRENT_MODULE}" PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../common/protocol"
)
