cmake_minimum_required(VERSION 2.6)

project(faceRecognition)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/${TF_SRC}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/${TF_FLATBUF_HEADER}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/${OPENCV_INSTALL}/include/opencv4")

link_directories("${CMAKE_CURRENT_SOURCE_DIR}/${TF_BUILD}")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/${TF_BUILD}/_deps/fft2d-build")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/${TF_BUILD}/_deps/ruy-build/ruy")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/${TF_BUILD}/_deps/farmhash-build")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/${TF_BUILD}/_deps/flatbuffers-build")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/${TF_BUILD}/_deps/xnnpack-build")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/${TF_BUILD}/pthreadpool")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/${TF_BUILD}/_deps/cpuinfo-build")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/${TF_BUILD}/_deps/clog-build")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/${OPENCV_INSTALL}/lib")


add_executable(faceRecognition main.cc RecognitionModel.cc)

target_link_libraries(faceRecognition tensorflow-lite)

target_link_libraries(faceRecognition ruy_context)
target_link_libraries(faceRecognition ruy_context_get_ctx)
if(AARCH64)
target_link_libraries(faceRecognition ruy_pack_arm)
else()
target_link_libraries(faceRecognition ruy_pack_avx2_fma)
target_link_libraries(faceRecognition ruy_pack_avx512)
target_link_libraries(faceRecognition ruy_pack_avx)
endif(AARCH64)
target_link_libraries(faceRecognition ruy_allocator)
target_link_libraries(faceRecognition ruy_apply_multiplier)
target_link_libraries(faceRecognition ruy_denormal)
target_link_libraries(faceRecognition ruy_frontend)
target_link_libraries(faceRecognition ruy_prepare_packed_matrices)
target_link_libraries(faceRecognition ruy_system_aligned_alloc)
if(AARCH64)
target_link_libraries(faceRecognition ruy_kernel_arm)
else()
target_link_libraries(faceRecognition ruy_kernel_avx2_fma)
target_link_libraries(faceRecognition ruy_kernel_avx512)
target_link_libraries(faceRecognition ruy_kernel_avx)
target_link_libraries(faceRecognition ruy_pack_avx)
endif(AARCH64)
target_link_libraries(faceRecognition ruy_trmul)
target_link_libraries(faceRecognition ruy_tune)
target_link_libraries(faceRecognition ruy_block_map)
target_link_libraries(faceRecognition ruy_cpuinfo)
target_link_libraries(faceRecognition ruy_thread_pool)
target_link_libraries(faceRecognition ruy_blocking_counter)
target_link_libraries(faceRecognition ruy_wait)
target_link_libraries(faceRecognition ruy_ctx)
target_link_libraries(faceRecognition ruy_prepacked_cache)
if(AARCH64)
else()
target_link_libraries(faceRecognition ruy_have_built_path_for_avx)
target_link_libraries(faceRecognition ruy_have_built_path_for_avx2_fma)
target_link_libraries(faceRecognition ruy_have_built_path_for_avx512)
endif(AARCH64)

target_link_libraries(faceRecognition fft2d_fftsg2d)
target_link_libraries(faceRecognition fft2d_fftsg)
target_link_libraries(faceRecognition pthread)
target_link_libraries(faceRecognition dl)
target_link_libraries(faceRecognition farmhash)
target_link_libraries(faceRecognition flatbuffers)
target_link_libraries(faceRecognition XNNPACK)
target_link_libraries(faceRecognition pthreadpool)
target_link_libraries(faceRecognition cpuinfo)
target_link_libraries(faceRecognition clog)

target_link_libraries(faceRecognition opencv_video)
target_link_libraries(faceRecognition opencv_core)
target_link_libraries(faceRecognition opencv_imgproc)
target_link_libraries(faceRecognition opencv_gapi)
target_link_libraries(faceRecognition opencv_features2d)
target_link_libraries(faceRecognition opencv_calib3d)
target_link_libraries(faceRecognition opencv_objdetect)
target_link_libraries(faceRecognition opencv_highgui)
target_link_libraries(faceRecognition opencv_imgcodecs)
target_link_libraries(faceRecognition opencv_videoio)
target_link_libraries(faceRecognition opencv_flann)
