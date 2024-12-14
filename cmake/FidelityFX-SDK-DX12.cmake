set(FFX_API_VK OFF)
set(FFX_API_DX12 ON)
set(FFX_BRIXELIZER ON)
set(FFX_ALL OFF)
set(FFX_FSR3 ON)
set(FFX_FSR ON)
set(FFX_AUTO_COMPILE_SHADERS 1)

add_subdirectory(${CMAKE_SOURCE_DIR}/extern/FidelityFX-SDK-DX12/sdk)

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE
  ffx_frameinterpolation_x64
  ffx_fsr3_x64
  ffx_fsr3upscaler_x64
  ffx_opticalflow_x64
  ffx_backend_dx12_x64
  ffx_brixelizer_x64
)
