include_directories (BEFORE "${PROJECT_SOURCE_DIR}/mathplot" ${HDF5_INCLUDE_DIR})
include_directories (BEFORE "${PROJECT_SOURCE_DIR}/maths" ${HDF5_INCLUDE_DIR})
add_executable (make_poly_eye make_poly_eye.cpp)

find_package(OpenGL REQUIRED)
find_package(glfw3 3.2...3.4 REQUIRED)
set(MPLOT_LIBS_GL_EXTRA OpenGL::GL glfw)
add_executable (make_hexy_eye make_hexy_eye.cpp)
target_link_libraries(make_hexy_eye ${MPLOT_LIBS_CORE} ${MPLOT_LIBS_GL} ${MPLOT_LIBS_GL_EXTRA})
