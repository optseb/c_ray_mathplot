# The OptiX way to create an executable
OPTIX_add_sample_executable (c_ray_mathplot target_name c_ray_mathplot.cpp OPTIONS -rdc true)
# Link CUDA, mathplot dependencies and libEyeRenderer3.so omit: ${CUDA_LIBRARIES}
target_link_libraries(${target_name} ${MPLOT_LIBS_CORE} ${MPLOT_LIBS_GL} compound-ray::EyeRenderer3)
