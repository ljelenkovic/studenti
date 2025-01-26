file(GLOB_RECURSE SOURCES "src/**.cpp" "src/**.c")

if(CONFIG_BUILD_FIRMWARE)
    set(ENTRY "main.cpp")
    set(LIBS_INCL "lib/opencv")
elseif(CONFIG_EXAMPLE_RECORDER)
    set(ENTRY "../examples/recorder.c")
endif()

file(GLOB_RECURSE LIBS "lib/*/src/**.cpp" "lib/*/src/**.c")



idf_component_register(SRCS ${ENTRY} ${SOURCES} ${LIBS} INCLUDE_DIRS "src" ${LIBS_INCL})

add_prebuilt_library(opencv_imgproc "lib/opencv/libopencv_imgproc.a" PRIV_REQUIRES cxx)
add_prebuilt_library(opencv_core "lib/opencv/libopencv_core.a" PRIV_REQUIRES cxx)
target_link_libraries(${COMPONENT_LIB} opencv_imgproc opencv_core)
